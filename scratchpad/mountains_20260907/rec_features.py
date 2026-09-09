"""Per-quadrant features for every cell in the worldspace, at FULL resolution.

Colour alone was measured to be badly ambiguous (rec_ambiguity.py: 27.3%
far-weighted top-1).  A quadrant is 64x64 texels at mip 0 of a level-4 tile, so
there is real texture detail to use: gravel is speckled where sand is smooth,
even when their means match.  This sweeps mip 0 once and reduces each quadrant
to a fixed feature vector, so nothing downstream has to touch the DDS files
again.

Feature block (per cell, per quadrant), 22 numbers:
   0..2   mean R,G,B                       (VCLR divided out)
   3..5   std  R,G,B
   6..8   chroma: R-G, G-B, R-B of the mean
   9..10  normalised chroma r/(r+g+b), g/(r+g+b)
  11..13  luminance p10, p50, p90
  14..15  mean |horizontal d| , mean |vertical d| of luminance   <- texture detail
  16      mean |laplacian| of luminance                          <- speckle
  17..19  terrain: mean height, std height, mean slope
  20      terrain: max slope
  21      mean height gradient magnitude of the CELL (context)

Terrain features come from lens2/land3C.npz's 33x33 VHGT grids.  They are
included so their transferability can be MEASURED (rec_shift.py); the model
only keeps them if they survive that test.

Writes features.npz: F (192,192,4,22) float32, have (192,192) bool.
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
CP = 128          # mip 0: a cell is 128x128 texels
QP = 64           # a quadrant is 64x64
NF = 22

# quadrant -> (rows, cols) inside the cell's north-up 128x128 block
QS = {0: (slice(64, 128), slice(0, 64)),
      1: (slice(64, 128), slice(64, 128)),
      2: (slice(0, 64), slice(0, 64)),
      3: (slice(0, 64), slice(64, 128))}


def quad_features(q, vc):
    """q: (64,64,3) float32 already VCLR-divided.  -> 17 colour features."""
    flat = q.reshape(-1, 3)
    m = flat.mean(0)
    s = flat.std(0)
    lum = 0.2126 * q[:, :, 0] + 0.7152 * q[:, :, 1] + 0.0722 * q[:, :, 2]
    p10, p50, p90 = np.percentile(lum, [10, 50, 90])
    dh = np.abs(np.diff(lum, axis=1)).mean()
    dv = np.abs(np.diff(lum, axis=0)).mean()
    lap = np.abs(4 * lum[1:-1, 1:-1] - lum[:-2, 1:-1] - lum[2:, 1:-1]
                 - lum[1:-1, :-2] - lum[1:-1, 2:]).mean()
    tot = max(m.sum(), 1e-6)
    return np.array([m[0], m[1], m[2], s[0], s[1], s[2],
                     m[0] - m[1], m[1] - m[2], m[0] - m[2],
                     m[0] / tot, m[1] / tot,
                     p10, p50, p90, dh, dv, lap], dtype=np.float32)


def main():
    pat = re.compile(r'^Commonwealth\.4\.(-?\d+)\.(-?\d+)\.DDS$', re.I)
    tiles = {}
    for n in os.listdir(VAN):
        m = pat.match(n)
        if m:
            tiles[(int(m.group(1)), int(m.group(2)))] = os.path.join(VAN, n)

    # VCLR multiplier at mip-0 resolution: nearest-sample the 33x33 vertex grid
    z = np.load(os.path.join(HERE, 'lens2', 'land3C.npz'))
    col = z['colors'].astype(np.float32) / 255.0     # (192,192,33,33,3)
    pv = z['present_vclr'].astype(bool)
    heights = z['heights'].astype(np.float32)        # (192,192,33,33)

    F = np.zeros((N, N, 4, NF), dtype=np.float32)
    have = np.zeros((N, N), dtype=bool)
    t0 = time.time()
    for k, ((tx, ty), path) in enumerate(sorted(tiles.items())):
        img = decode_rgb(DDS(path), 0).astype(np.float32)   # (512,512,3) north-up
        for dy in range(4):
            for dx in range(4):
                cx, cy = tx + dx, ty + dy
                r, c = cy - MINY, cx - MINX
                if not (0 <= r < N and 0 <= c < N):
                    continue
                block = img[(3 - dy) * CP:(4 - dy) * CP, dx * CP:(dx + 1) * CP]
                if pv[r, c]:
                    # 33x33 vertex grid, row 0 SOUTH -> flip to north-up, then
                    # nearest-sample onto 128 texels (texel i sits at vertex
                    # (i+0.5)*32/128 = (i+0.5)/4)
                    g = col[r, c][::-1, :, :]
                    idx = np.clip(((np.arange(CP) + 0.5) / 4.0).astype(int), 0, 32)
                    vm = g[np.ix_(idx, idx)]
                    block = block / np.maximum(vm, 1.0 / 255.0)
                for q, (rs, cs) in QS.items():
                    F[r, c, q, :17] = quad_features(block[rs, cs], None)
                have[r, c] = True
        if k % 300 == 0:
            print('  %4d/%d  %.1fs' % (k, len(tiles), time.time() - t0))
    print('colour features done in %.1fs' % (time.time() - t0))

    # terrain features; VHGT grid row 0 SOUTH col 0 WEST, units of 8
    # quadrant 0 BL = rows 0:17 cols 0:17 in that (south-up) frame
    TQ = {0: (slice(0, 17), slice(0, 17)), 1: (slice(0, 17), slice(16, 33)),
          2: (slice(16, 33), slice(0, 17)), 3: (slice(16, 33), slice(16, 33))}
    gy, gx = np.gradient(heights, axis=(2, 3))
    slope = np.hypot(gx, gy)
    cellslope = slope.mean(axis=(2, 3))
    for q, (rs, cs) in TQ.items():
        h = heights[:, :, rs, cs]
        sl = slope[:, :, rs, cs]
        F[:, :, q, 17] = h.mean(axis=(2, 3))
        F[:, :, q, 18] = h.std(axis=(2, 3))
        F[:, :, q, 19] = sl.mean(axis=(2, 3))
        F[:, :, q, 20] = sl.max(axis=(2, 3))
        F[:, :, q, 21] = cellslope
    print('cells with features: %d' % have.sum())

    np.savez(os.path.join(HERE, 'features.npz'), F=F, have=have)
    print('wrote features.npz  %s  (%.1f MB)'
          % (F.shape, os.path.getsize(os.path.join(HERE, 'features.npz')) / 1e6))


if __name__ == '__main__':
    sys.exit(main())
