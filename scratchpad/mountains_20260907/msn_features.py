"""Per-quadrant features from the vanilla terrain LOD NORMAL maps (_msn).

Why the normal map at all.  Mean colour was MEASURED to be badly ambiguous for
recovering which LTEX an unpainted cell should carry (28.4% top-1 on an
outward-extrapolation holdout): gravel, sand and rubble are all mid grey-brown.
Their NORMAL maps are not alike at all -- gravel is a field of pebble-sized
bumps, sand is nearly flat, rubble is chunky and directional -- so if any of
that survived the LOD bake, the _msn discriminates exactly where colour cannot.

Why a high-pass is the whole trick.  The GEOMETRIC part of these normals comes
from the 33x33 VHGT vertex grid, which is 4 texels per vertex at mip 0.  Its
detail is therefore band-limited to about 4 texels.  Anything at 2-3 texels
cannot be terrain shape; it can only be baked material detail.  So a 3x3
laplacian, and the residual after subtracting a 5x5 or 9x9 box mean, are close
to pure material signal without having to model the heightfield at all.  Two
band widths are kept deliberately: the laplacian is the narrowest (material
only), the 9x9 residual is wider (material plus the finest geometry), and the
covariate-shift test in msn_shift.py decides which of the two transfers.

Channel convention (settled upstream, re-checked in msn_decode_check.py and in
the green sanity numbers this script prints): R = X east, G = up, B = Y north,
each 0.5+0.5 encoded.  Up is in GREEN.  Alpha is a constant 255 and carries
nothing, so the RGB-only vectorised decoder is enough.

Neighbourhood operators run over the WHOLE 512x512 tile before the quadrants
are cut out, not inside each 64x64 quadrant.  A quadrant's edge texels then see
their real neighbours in the adjacent cell instead of a truncated window.  Only
the outermost 4-texel ring of a whole TILE is affected by padding, which is one
quadrant edge in sixteen.

Feature block, 31 numbers per cell per quadrant -- see NAMES.

Writes msn_features.npz: M (192,192,4,31) float32, have (192,192) bool,
names (31,) object.
"""
import os
import re
import sys
import time

import numpy as np

from dds import DDS
from bcnp import decode_rgb
from rec_common import HERE, MINX, MINY, N

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
QP = 64           # mip 0: a quadrant is 64x64 texels, a tile is 8x8 quadrants
QG = 8

NAMES = [
    'mean_nx', 'mean_ny', 'mean_nz',
    'std_nx', 'std_ny', 'std_nz',
    'lap_nx', 'lap_ny', 'lap_nz',
    'hp9_nx', 'hp9_ny', 'hp9_nz',
    'hp5_nx', 'hp5_ny', 'hp5_nz',
    'dh_nx', 'dh_ny', 'dh_nz',
    'dv_nx', 'dv_ny', 'dv_nz',
    'rough', 'aniso', 'lapVec',
    'tiltMean', 'tiltStd', 'hpTilt',
    'detRat_nx', 'detRat_ny', 'detRat_nz',
    'hpCorr_xz',
]
NM = len(NAMES)

# quadrant -> (quadrant-row, quadrant-col) offset inside a cell's 2x2 block,
# in the tile's north-up frame (row 0 = north).  0 BL, 1 BR, 2 TL, 3 TR.
QOFF = {0: (1, 0), 1: (1, 1), 2: (0, 0), 3: (0, 1)}


def boxmean(a, k):
    """Mean over a k x k window, edge-padded, same shape out.  Integral image."""
    r = k // 2
    pad = [(r, r), (r, r)] + [(0, 0)] * (a.ndim - 2)
    p = np.pad(a.astype(np.float64), pad, mode='edge')
    cs = np.cumsum(p, axis=0)
    cs = np.concatenate([np.zeros((1,) + cs.shape[1:]), cs], axis=0)
    s = cs[k:] - cs[:-k]
    cs = np.cumsum(s, axis=1)
    cs = np.concatenate([np.zeros((cs.shape[0], 1) + cs.shape[2:]), cs], axis=1)
    s = cs[:, k:] - cs[:, :-k]
    return s / float(k * k)


def laplace(a):
    """4c - N - S - E - W, edge-padded, same shape out."""
    p = np.pad(a, [(1, 1), (1, 1)] + [(0, 0)] * (a.ndim - 2), mode='edge')
    return 4.0 * a - p[:-2, 1:-1] - p[2:, 1:-1] - p[1:-1, :-2] - p[1:-1, 2:]


def fwd_diff(a, axis):
    """a[i+1] - a[i] along axis, edge-padded so the last row/col is 0."""
    pad = [(0, 0)] * a.ndim
    pad[axis] = (0, 1)
    p = np.pad(a, pad, mode='edge')
    if axis == 0:
        return p[1:] - p[:-1]
    return p[:, 1:] - p[:, :-1]


def quad_reduce(t):
    """(512,512,K) per-texel maps -> (8,8,K) per-quadrant means."""
    h, w, k = t.shape
    return t.reshape(QG, QP, QG, QP, k).mean(axis=(1, 3), dtype=np.float64)


def tile_features(img):
    """img (512,512,3) uint8 -> (8,8,NM) float32 per-quadrant features."""
    n = (img.astype(np.float32) / 255.0 - 0.5) * 2.0     # signed nx, ny_up, nz

    lap = np.stack([laplace(n[:, :, c]) for c in range(3)], axis=2)
    hp9 = n - boxmean(n, 9)
    hp5 = n - boxmean(n, 5)
    dh = fwd_diff(n, 1)
    dv = fwd_diff(n, 0)
    tilt = np.hypot(n[:, :, 0], n[:, :, 2])
    hpt = tilt - boxmean(tilt, 9)

    t = np.concatenate([
        n,                                  # 0..2   mean
        n * n,                              # 3..5   -> std
        np.abs(lap),                        # 6..8
        hp9 * hp9,                          # 9..11  -> hp9 std
        hp9,                                # 12..14
        hp5 * hp5,                          # 15..17
        hp5,                                # 18..20
        np.abs(dh),                         # 21..23
        np.abs(dv),                         # 24..26
        (dh * dh).sum(2)[:, :, None],       # 27  horizontal gradient energy
        (dv * dv).sum(2)[:, :, None],       # 28  vertical
        np.sqrt((lap * lap).sum(2))[:, :, None],          # 29 lapVec
        tilt[:, :, None], (tilt * tilt)[:, :, None],      # 30,31
        hpt[:, :, None], (hpt * hpt)[:, :, None],         # 32,33
        (hp9[:, :, 0] * hp9[:, :, 2])[:, :, None],        # 34
    ], axis=2).astype(np.float32)

    m = quad_reduce(t)                                     # (8,8,35)

    # roughness needs each quadrant's own mean normal, so it is a second pass
    mn = m[:, :, 0:3]
    mn = mn / np.maximum(np.linalg.norm(mn, axis=2, keepdims=True), 1e-9)
    mnfull = np.repeat(np.repeat(mn, QP, axis=0), QP, axis=1)
    unit = n / np.maximum(np.linalg.norm(n, axis=2, keepdims=True), 1e-9)
    dot = np.clip((unit * mnfull).sum(2), -1.0, 1.0)
    ang = quad_reduce(np.arccos(dot)[:, :, None].astype(np.float32))[:, :, 0]

    def sd(i2, i1):
        return np.sqrt(np.maximum(m[:, :, i2] - m[:, :, i1] ** 2, 0.0))

    f = np.empty((QG, QG, NM), dtype=np.float64)
    f[:, :, 0:3] = m[:, :, 0:3]
    for c in range(3):
        f[:, :, 3 + c] = sd(3 + c, 0 + c)
        f[:, :, 6 + c] = m[:, :, 6 + c]
        f[:, :, 9 + c] = sd(9 + c, 12 + c)
        f[:, :, 12 + c] = sd(15 + c, 18 + c)
        f[:, :, 15 + c] = m[:, :, 21 + c]
        f[:, :, 18 + c] = m[:, :, 24 + c]
    f[:, :, 21] = ang
    f[:, :, 22] = m[:, :, 27] / np.maximum(m[:, :, 27] + m[:, :, 28], 1e-12)
    f[:, :, 23] = m[:, :, 29]
    f[:, :, 24] = m[:, :, 30]
    f[:, :, 25] = sd(31, 30)
    f[:, :, 26] = sd(33, 32)
    # scale-free speckliness: detail divided by the quadrant's own contrast, so
    # a flat-but-gritty quadrant and a steep-and-gritty one score alike
    for c in range(3):
        f[:, :, 27 + c] = m[:, :, 6 + c] / np.maximum(f[:, :, 3 + c], 1e-4)
    sx, sz = f[:, :, 9], f[:, :, 11]
    cov = m[:, :, 34] - m[:, :, 12] * m[:, :, 14]
    f[:, :, 30] = cov / np.maximum(sx * sz, 1e-9)
    return f.astype(np.float32)


def main():
    pat = re.compile(r'^Commonwealth\.4\.(-?\d+)\.(-?\d+)_msn\.DDS$', re.I)
    tiles = {}
    for nm in os.listdir(VAN):
        mo = pat.match(nm)
        if mo:
            tiles[(int(mo.group(1)), int(mo.group(2)))] = os.path.join(VAN, nm)
    print('tiles found: %d' % len(tiles))

    M = np.zeros((N, N, 4, NM), dtype=np.float32)
    have = np.zeros((N, N), dtype=bool)

    # green-channel sanity, accumulated over the whole sweep
    gsum = 0.0
    gpos = 0
    gcount = 0

    t0 = time.time()
    for k, ((tx, ty), path) in enumerate(sorted(tiles.items())):
        img = decode_rgb(DDS(path), 0)                     # (512,512,3) north-up
        g = (img[:, :, 1].astype(np.float32) / 255.0 - 0.5) * 2.0
        gsum += float(g.sum())
        gpos += int((g > 0).sum())
        gcount += g.size
        f = tile_features(img)
        for dy in range(4):
            for dx in range(4):
                cx, cy = tx + dx, ty + dy
                r, c = cy - MINY, cx - MINX
                if not (0 <= r < N and 0 <= c < N):
                    continue
                qr0, qc0 = 2 * (3 - dy), 2 * dx
                for q, (ar, ac) in QOFF.items():
                    M[r, c, q] = f[qr0 + ar, qc0 + ac]
                have[r, c] = True
        if k % 300 == 0:
            print('  %4d/%d  %.1fs' % (k, len(tiles), time.time() - t0))
    print('sweep done in %.1fs' % (time.time() - t0))

    print('GREEN SANITY: mean(ny_up) = %.4f   frac(ny_up > 0) = %.6f  over %d texels'
          % (gsum / gcount, gpos / float(gcount), gcount))
    print('cells with features: %d of %d' % (have.sum(), N * N))

    out = os.path.join(HERE, 'msn_features.npz')
    np.savez(out, M=M, have=have, names=np.array(NAMES, dtype=object))
    print('wrote msn_features.npz  %s  (%.1f MB)'
          % (M.shape, os.path.getsize(out) / 1e6))


if __name__ == '__main__':
    sys.exit(main())
