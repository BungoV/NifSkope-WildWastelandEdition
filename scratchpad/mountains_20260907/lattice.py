"""Lane LATTICE -- is there a periodic square lattice in OUR terrain `_msn`
that vanilla's does not have, and at what period?

bungo, on `images/mountain_peak_closeup.png`: "You can see the square pattern on
the right in the terrain, which is not good."

THE MEASUREMENT.  A square lattice of period p texels, axis aligned, puts energy
into a COMB of spatial frequencies: k = RES/p and its harmonics, on both axes.
Everything else in a terrain slope field is broadband.  So:

  * take the slope field the sheet encodes (P = dh/du, R = dh/dv, FO4 order
    R=east G=up B=north, row 0 = north edge),
  * subtract its own 9x9 box mean, the same local blur every earlier `_msn`
    measurement on this thread used,
  * 1-D power spectrum along x (averaged over rows) and along y,
  * PROMINENCE at bin k = P[k] / median(P over a local annulus, k excluded).

Prominence is 1.0 for a broadband field whatever its amplitude, so it does not
reward or punish our sheet for having less high-frequency energy than vanilla's
-- which matters, because ours HAS 6.4x less and a raw energy comparison at one
frequency would say nothing about a lattice.

CONTROLS (ww-control-calibration):
  known-answer   a fractal slope field reads ~1 everywhere; the same field with
                 a period-4 square lattice added at 5% of its rms must fire.
  positive       the PRE-FIX nearest-sampled sheet, recomputed from the same
                 VHGT heights through the same encoder and the same block codec.
                 That sheet is KNOWN to be piecewise constant on 4x4 blocks, so
                 whatever number the metric gives it is what "a lattice" reads.
  floor          vanilla's own shipped sheet, same tile, same size, same mip.
  ceiling/floor  positive / vanilla, pre-registered gate 5x.

Usage:  python lattice.py [--mip N] [tile ...]
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bcnp import open_decode

HERE = os.path.dirname(os.path.abspath(__file__))
VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
OURS_DIRS = [
    os.path.join(HERE, 'images', 'ours4', 'textures', 'terrain', 'Commonwealth'),
    r'C:/Users/bungo/AppData/Local/Temp/claude/laneb/gen/peak/tex',
]
LAND = os.path.join(HERE, 'land_all.bin')

BLUR = 9
MARGIN = 8
UP_FLOOR = 0.15


# ------------------------------------------------------------------ basics

def boxmean(a, k):
    r = k // 2
    p = np.pad(np.asarray(a, dtype=np.float64), [(r, r), (r, r)], mode='edge')
    cs = np.cumsum(p, axis=0)
    cs = np.concatenate([np.zeros((1,) + cs.shape[1:]), cs], axis=0)
    s = cs[k:] - cs[:-k]
    cs = np.cumsum(s, axis=1)
    cs = np.concatenate([np.zeros((cs.shape[0], 1)), cs], axis=1)
    return (cs[:, k:] - cs[:, :-k]) / float(k * k)


def slopes(rgb):
    """(h,w,3) uint8 _msn -> (P, R).  FO4: R east, G up, B north."""
    n = np.asarray(rgb, dtype=np.float64) / 255.0 * 2.0 - 1.0
    up = np.maximum(n[:, :, 1], UP_FLOOR)
    return -n[:, :, 0] / up, n[:, :, 2] / up


def residual(P, R, k=BLUR, margin=MARGIN):
    a = P - boxmean(P, k)
    b = R - boxmean(R, k)
    if margin:
        a = a[margin:-margin, margin:-margin]
        b = b[margin:-margin, margin:-margin]
    return a, b


# ------------------------------------------------------------------ metric

def spec1d(f, axis):
    """Mean power spectrum along one axis, Hann-windowed line by line.

    Windowing matters: a non-periodic 512-wide field leaks a 1/k^2 skirt onto
    every bin, and that skirt is smooth, so it inflates the BACKGROUND of the
    prominence ratio rather than a comb bin -- but only if it is the same for
    both sides.  Hann makes it small for both.
    """
    f = np.asarray(f, dtype=np.float64)
    if axis == 1:
        f = f.T                                   # rows along the axis of interest
    n = f.shape[1]
    w = np.hanning(n)[None, :]
    f = (f - f.mean(axis=1, keepdims=True)) * w
    F = np.fft.rfft(f, axis=1)
    return (np.abs(F) ** 2).mean(axis=0)


def prominence(P, k, half=10, guard=2):
    """P[k] over the median of a local background, k and its immediate
    neighbours excluded.  1.0 = no spike."""
    n = len(P)
    lo, hi = max(1, k - half), min(n, k + half + 1)
    idx = [i for i in range(lo, hi) if abs(i - k) > guard]
    if not idx:
        return float('nan')
    bg = np.median(P[idx])
    if bg <= 0:
        return float('nan')
    return float(P[k] / bg)


def comb(P, period, n, harmonics=3):
    """Mean prominence over the first `harmonics` comb bins of a square lattice
    of this period, i.e. k = n/period, 2n/period, ...  A lattice puts energy in
    all of them; a single sinusoid only in the first."""
    vals = []
    for h in range(1, harmonics + 1):
        k = int(round(n * h / float(period)))
        if 1 <= k <= n // 2:
            vals.append(prominence(P, k))
    return float(np.mean(vals)) if vals else float('nan')


def lattice_report(rgb, periods, label='', out=sys.stdout):
    P, R = residual(*slopes(rgb))
    n = P.shape[0]
    px = spec1d(P, 0) + spec1d(R, 0)
    py = spec1d(P, 1) + spec1d(R, 1)
    row = {}
    for p in periods:
        row[p] = (comb(px, p, n), comb(py, p, n))
    row['rms'] = float(np.sqrt((P ** 2 + R ** 2).mean()))
    if label:
        cells = '  '.join('p%-3d %5.2f/%-5.2f' % (p, row[p][0], row[p][1])
                          for p in periods)
        print('%-34s rms %6.4f   %s' % (label, row['rms'], cells), file=out)
    return row


# ------------------------------------------------------------------ the land

def load_land(path=LAND):
    b = np.fromfile(path, dtype=np.uint8)
    minx, miny, cw, ch = np.frombuffer(b[:16].tobytes(), dtype='<i4')
    off = 16
    present = b[off:off + cw * ch].astype(bool).reshape(ch, cw)
    off += cw * ch
    grid = np.frombuffer(b[off:].tobytes(), dtype='<i2').reshape(ch, cw, 33, 33)
    return int(minx), int(miny), present, grid


def chunk_hgt(land, chunkX, chunkY, dim):
    """The generator's own `hgt`: hn x hn floats, row 0 SOUTH, col 0 WEST, the
    later cell winning on a shared edge (lodgen.cpp:4844)."""
    minx, miny, present, grid = land
    hn = dim * 32 + 1
    out = np.zeros((hn, hn), dtype=np.float64)
    for cy in range(dim):
        for cx in range(dim):
            j, i = chunkY + cy - miny, chunkX + cx - minx
            if not present[j, i]:
                continue
            out[cy * 32:cy * 32 + 33, cx * 32:cx * 32 + 33] = \
                grid[j, i].astype(np.float64) * 8.0
    return out


# --------------------------------------------------- the generator, replicated

def sample_bilinear(hgt, gx, gy):
    hn = hgt.shape[0]
    cx = np.clip(gx, 0.0, hn - 1 - 0.001)
    cy = np.clip(gy, 0.0, hn - 1 - 0.001)
    ix = cx.astype(np.int64)
    iy = cy.astype(np.int64)
    tx = cx - ix
    ty = cy - iy
    h00 = hgt[iy, ix]
    h10 = hgt[iy, ix + 1]
    h01 = hgt[iy + 1, ix]
    h11 = hgt[iy + 1, ix + 1]
    return (h00 * (1 - tx) + h10 * tx) * (1 - ty) + (h01 * (1 - tx) + h11 * tx) * ty


def sample_nearest(hgt, gx, gy):
    """The PRE-FIX reconstruction: int() truncation of the grid coordinate."""
    hn = hgt.shape[0]
    ix = np.clip(gx, 0.0, hn - 1 - 0.001).astype(np.int64)
    iy = np.clip(gy, 0.0, hn - 1 - 0.001).astype(np.int64)
    return hgt[iy, ix]


def _bspline_w(t):
    """Cubic B-spline weights for the four taps at -1,0,1,2."""
    t2, t3 = t * t, t * t * t
    return (
        (1 - 3 * t + 3 * t2 - t3) / 6.0,
        (4 - 6 * t2 + 3 * t3) / 6.0,
        (1 + 3 * t + 3 * t2 - 3 * t3) / 6.0,
        t3 / 6.0,
    )


def sample_bspline(hgt, gx, gy):
    """Cubic B-spline (APPROXIMATING, C2, no overshoot -- unlike Catmull-Rom,
    which interpolates and rings on a staircase)."""
    hn = hgt.shape[0]
    cx = np.clip(gx, 0.0, hn - 1 - 0.001)
    cy = np.clip(gy, 0.0, hn - 1 - 0.001)
    ix = cx.astype(np.int64)
    iy = cy.astype(np.int64)
    wx = _bspline_w(cx - ix)
    wy = _bspline_w(cy - iy)
    acc = np.zeros(cx.shape, dtype=np.float64)
    for dy in range(-1, 3):
        jy = np.clip(iy + dy, 0, hn - 1)
        rowacc = np.zeros(cx.shape, dtype=np.float64)
        for dx in range(-1, 3):
            jx = np.clip(ix + dx, 0, hn - 1)
            rowacc += wx[dx + 1] * hgt[jy, jx]
        acc += wy[dy + 1] * rowacc
    return acc


def sample_catmull(hgt, gx, gy):
    """Catmull-Rom, the basis WW_CHANGES 2026-09-07 tried and REVERTED.  Kept as
    the ringing control, not as a candidate."""
    hn = hgt.shape[0]
    cx = np.clip(gx, 0.0, hn - 1 - 0.001)
    cy = np.clip(gy, 0.0, hn - 1 - 0.001)
    ix = cx.astype(np.int64)
    iy = cy.astype(np.int64)

    def w(t):
        t2, t3 = t * t, t * t * t
        return (-0.5 * t + t2 - 0.5 * t3, 1 - 2.5 * t2 + 1.5 * t3,
                0.5 * t + 2 * t2 - 1.5 * t3, -0.5 * t2 + 0.5 * t3)

    wx, wy = w(cx - ix), w(cy - iy)
    acc = np.zeros(cx.shape, dtype=np.float64)
    for dy in range(-1, 3):
        jy = np.clip(iy + dy, 0, hn - 1)
        rowacc = np.zeros(cx.shape, dtype=np.float64)
        for dx in range(-1, 3):
            jx = np.clip(ix + dx, 0, hn - 1)
            rowacc += wx[dx + 1] * hgt[jy, jx]
        acc += wy[dy + 1] * rowacc
    return acc


def _blend_bilinear(hgt, gx, gy, ease):
    """Bilinear with the INTERPOLATION PARAMETER eased.

    The central difference over a FULL grid step turns any such blend into the
    same blend of the GRID-POINT gradients:

        H(g+1) - H(g-1) = (1-w) (H[i+1]-H[i-1]) + w (H[i+2]-H[i])

    -- algebra, not approximation, and it is why easing t is enough.  With
    w = t the gradient is linearly blended, so it is CONTINUOUS BUT KINKED at
    every grid line (a crease every 4 texels).  An ease whose derivatives
    vanish at 0 and 1 removes the kink WITHOUT moving a single grid-point
    value, so nothing is blurred and nothing overshoots: the reconstruction
    still passes through every VHGT sample and stays inside the four it sits
    between."""
    hn = hgt.shape[0]
    cx = np.clip(gx, 0.0, hn - 1 - 0.001)
    cy = np.clip(gy, 0.0, hn - 1 - 0.001)
    ix = cx.astype(np.int64)
    iy = cy.astype(np.int64)
    tx = ease(cx - ix)
    ty = ease(cy - iy)
    h00 = hgt[iy, ix]
    h10 = hgt[iy, ix + 1]
    h01 = hgt[iy + 1, ix]
    h11 = hgt[iy + 1, ix + 1]
    return (h00 * (1 - tx) + h10 * tx) * (1 - ty) + (h01 * (1 - tx) + h11 * tx) * ty


def sample_smoothstep(hgt, gx, gy):
    """Cubic ease 3t^2-2t^3: C1 gradient (the kink goes, a curvature step stays)."""
    return _blend_bilinear(hgt, gx, gy, lambda t: t * t * (3.0 - 2.0 * t))


def sample_quintic(hgt, gx, gy):
    """Quintic ease 6t^5-15t^4+10t^3: C2 gradient.  Perlin replaced the cubic
    with exactly this in 2002 because the cubic's second-derivative step is
    visible IN A NORMAL MAP, which is what this sheet is."""
    return _blend_bilinear(hgt, gx, gy,
                           lambda t: t * t * t * (t * (t * 6.0 - 15.0) + 10.0))


SAMPLERS = {
    'bilinear': sample_bilinear,
    'nearest': sample_nearest,
    'bspline': sample_bspline,
    'catmull': sample_catmull,
    'smoothstep': sample_smoothstep,
    'quintic': sample_quintic,
}


def gauss_blur_grid(hgt, sigma):
    """Separable Gaussian over the GRID (not the texels): a dequantiser for the
    8-unit VHGT staircase, applied before any differencing."""
    if sigma <= 0:
        return hgt
    r = max(1, int(np.ceil(3 * sigma)))
    x = np.arange(-r, r + 1, dtype=np.float64)
    k = np.exp(-0.5 * (x / sigma) ** 2)
    k /= k.sum()
    p = np.pad(hgt, r, mode='edge')
    tmp = np.apply_along_axis(lambda v: np.convolve(v, k, mode='valid'), 1, p)
    return np.apply_along_axis(lambda v: np.convolve(v, k, mode='valid'), 0, tmp)


def make_msn(hgt, dim, res=512, mode='bilinear', gsigma=0.0, phase=0.0):
    """Byte-for-byte the generator's msn loop (lodgen.cpp:4998-5091), vectorised.

    `phase` shifts the texel->grid mapping by that many GRID STEPS, which is the
    x-mod-4 discriminator: if the lattice follows the sample lines it moves with
    the phase; if it is an artefact of the texel grid it does not.
    """
    hn = hgt.shape[0]
    span = float(dim) * 4096.0
    spacing = span / float(hn - 1)
    if gsigma > 0:
        hgt = gauss_blur_grid(hgt, gsigma)
    px = np.arange(res, dtype=np.float64)
    py = np.arange(res, dtype=np.float64)
    ngx = ((px + 0.5) / res) * (hn - 1) + phase
    ngy = (1.0 - (py + 0.5) / res) * (hn - 1) + phase
    GX, GY = np.meshgrid(ngx, ngy)                # GY row 0 = north = high grid y
    f = SAMPLERS[mode]
    dzdx = (f(hgt, GX + 1.0, GY) - f(hgt, GX - 1.0, GY)) / (2.0 * spacing)
    dzdy = (f(hgt, GX, GY + 1.0) - f(hgt, GX, GY - 1.0)) / (2.0 * spacing)
    n = np.stack([-dzdx, -dzdy, np.ones_like(dzdx)], axis=2)
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    b = np.clip(((n * 0.5 + 0.5) * 255.0 + 0.5).astype(np.int64), 0, 255)
    # R = east (n0), G = up (n2), B = north (n1)
    return np.stack([b[:, :, 0], b[:, :, 2], b[:, :, 1]], axis=2).astype(np.uint8)


# --------------------------------------------------------------- block codec

def bc_roundtrip(rgb, seed=0):
    """4x4 principal-axis / RGB565 / 4-entry-palette emulation of BC1, so a
    control passes through the same lossy stage the shipped sheet did
    (msn_curl.py's, kept identical)."""
    a = np.asarray(rgb, dtype=np.float64)
    h, w, _ = a.shape
    out = a.copy()
    for by in range(0, h - 3, 4):
        for bx in range(0, w - 3, 4):
            blk = a[by:by + 4, bx:bx + 4].reshape(16, 3)
            m = blk.mean(axis=0)
            d = blk - m
            u, s, vt = np.linalg.svd(d, full_matrices=False)
            ax = vt[0]
            t = d @ ax
            e0 = m + ax * t.min()
            e1 = m + ax * t.max()
            q = lambda c: np.array([
                round(np.clip(c[0], 0, 255) / 255.0 * 31) / 31.0 * 255.0,
                round(np.clip(c[1], 0, 255) / 255.0 * 63) / 63.0 * 255.0,
                round(np.clip(c[2], 0, 255) / 255.0 * 31) / 31.0 * 255.0])
            e0, e1 = q(e0), q(e1)
            pal = np.stack([e0, e1, (2 * e0 + e1) / 3.0, (e0 + 2 * e1) / 3.0])
            dd = ((blk[:, None, :] - pal[None, :, :]) ** 2).sum(axis=2)
            out[by:by + 4, bx:bx + 4] = pal[dd.argmin(axis=1)].reshape(4, 4, 3)
    return np.clip(np.rint(out), 0, 255).astype(np.uint8)


# ----------------------------------------------------------------- utilities

def find_ours(tile, suffix='_msn'):
    for d in OURS_DIRS:
        p = os.path.join(d, 'Commonwealth.%s%s.DDS' % (tile, suffix))
        if os.path.exists(p):
            return p
    return None


def load(path, mip=0):
    return open_decode(path, mip)


def tile_dim_chunk(tile):
    dim, cx, cy = tile.split('.')
    return int(dim), int(cx), int(cy)
