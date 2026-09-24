"""TILING4 -- the candidate samplers, offline, as prototypes.

Both candidates replace TILING3's single smooth domain warp with a sample whose
PHASE is broken WITHOUT continuous strain.  That is the whole idea: TILING3
showed that a smooth warp removes the repeat only by stretching the texture
enough to be seen, and this lane's own instrument puts numbers on the stretch
(the isolated warp is over vanilla's swirl ceiling on 7 of 7 sheets).  A sample
made of PIECEWISE-CONSTANT offsets has a strain of exactly zero everywhere
except on a set of measure zero, so it cannot have that defect by construction --
what it can have instead is a seam where the pieces meet, and that is what the
band table, the local variance and the edge instruments are for.

H1 -- HISTOGRAM-PRESERVING HEX TILING (Heitz & Neyret 2018, "High-Performance
By-Example Noise using a Histogram-Preserving Blending Operator").  The plane is
covered by a triangle lattice; each lattice VERTEX carries one random offset into
the texture; every position takes the three offsets of the triangle it falls in
and blends them with its barycentric weights.  The blend is variance-preserving:

    result = mean + sum_k w_k (s_k - mean) / sqrt(sum_k w_k^2)

Blending three decorrelated samples of the same texture with weights summing to
one would otherwise drop the contrast by up to sqrt(1/3) -- a visible soft
mottling exactly where the paper's operator is needed.  The paper's full method
Gaussianises the texture, blends, and inverts the histogram; the brief allows the
cheaper variance-normalised blend, which preserves the variance exactly and the
rest of the histogram only approximately.  Both are measured here; the
approximation shows up, if it shows up, in the band table and the local variance.

H2 -- PER-CELL ROTATION + OFFSET (Wang-tile style).  A square cell lattice; each
cell carries a random offset and optionally a random rotation; the four cells
whose centres bracket a position are blended with smoothstep weights, again
variance-preserving.  `border` sets how wide the blend band is as a fraction of
a cell: 1.0 is a full bilinear blend everywhere, 0.25 a narrow seam band with
single-tap interiors.

A NOTE ON THE ROTATION.  A rotation is rigid, so it adds no strain -- but it
does give every cell's grain one coherent direction, which is precisely what
this lane's swirl instrument was built to see.  So H2 is measured with the
rotation off, with 90-degree multiples (which map axis-aligned features to
axis-aligned features), and with arbitrary angles, and the instrument is allowed
to convict any of them.

EVERYTHING IS DETERMINISTIC IN WORLD POSITION.  The offsets come from the lattice
index through TILING3's own uint32 hash, so the sample is the same function of
world position no matter which chunk, cell or thread reaches it: seamless across
chunk and cell boundaries and byte-identical at 1 and 16 threads by construction,
not by a lock.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T3 = os.path.join(os.path.dirname(HERE), 'tiling3_20260911')
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T3, T2, SP):
    if p not in sys.path:
        sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import offline_bake as OB                                     # noqa: E402

TILE = 341.3333
_orig_tap = OB._tap

# the skew that turns a square lattice into an equilateral triangle one
_SKEW = 0.57735026918962576       # 1/sqrt(3)
_SCALE = 1.15470053837925152      # 2/sqrt(3)


def hash01(i, j, k):
    """TILING3's uint32 value hash, verbatim, so the C++ side can be the same
    function to the bit.  i, j are integer lattice indices; k picks the channel."""
    kterm = np.uint32((int(k) * 2246822519) & 0xFFFFFFFF)   # wrapped in python
    h = (np.asarray(i, np.int64).astype(np.uint32) * np.uint32(374761393)
         + np.asarray(j, np.int64).astype(np.uint32) * np.uint32(668265263)
         + kterm)
    h ^= h >> np.uint32(13)
    h = h * np.uint32(1274126177)
    h ^= h >> np.uint32(16)
    return h.astype(np.float64) / 4294967296.0


def tri_grid(px, py):
    """Heitz & Neyret's triangle grid: the three lattice vertices of the
    triangle containing (px,py) in lattice units, and the barycentric weights."""
    sx = px - _SKEW * py
    sy = _SCALE * py
    bi = np.floor(sx)
    bj = np.floor(sy)
    tx = sx - bi
    ty = sy - bj
    tz = 1.0 - tx - ty
    up = tz > 0.0
    w = (np.where(up, tz, -tz), np.where(up, ty, 1.0 - ty), np.where(up, tx, 1.0 - tx))
    o = np.where(up, 0.0, 1.0)
    verts = (((bi + o).astype(np.int64), (bj + o).astype(np.int64)),
             ((bi + o).astype(np.int64), (bj + 1.0 - o).astype(np.int64)),
             ((bi + 1.0 - o).astype(np.int64), (bj + o).astype(np.int64)))
    return w, verts


def _mean_colour(dds):
    """The texture's own mean, the same value the C++ `average` path already
    takes: getPixelT(0.5, 0.5, maxMip)."""
    z = np.zeros(1)
    return S.sample_trilinear(dds, z + 0.5, z + 0.5, float(dds.maxMip))[0, :3]


def _blend(taps, ws, mean, varnorm):
    acc = 0.0
    wsq = 0.0
    for s, w in zip(taps, ws):
        acc = acc + w[..., None] * (s - mean)
        wsq = wsq + w * w
    if varnorm:
        acc = acc / np.sqrt(np.maximum(wsq, 1e-12))[..., None]
    return mean + acc


def make_tap_h1(size, mipbias=0.0, varnorm=True):
    """H1: three per-vertex offsets on a triangle lattice of `size` world units."""
    def tap(dds, wx, wy, tile, upt, mip):
        m = mipbias if mip == 'code' else float(mip) + mipbias
        ws, verts = tri_grid(np.asarray(wx, np.float64) / size,
                             np.asarray(wy, np.float64) / size)
        mean = _mean_colour(dds)
        taps = []
        for (i, j) in verts:
            ox = hash01(i, j, 0) * tile
            oy = hash01(i, j, 1) * tile
            taps.append(_orig_tap(dds, wx + ox, wy + oy, tile, upt, m))
        return _blend(taps, ws, mean, varnorm)
    return tap


def _smoothstep(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def make_tap_h2(cell, border=1.0, rot='none', mipbias=0.0, varnorm=True):
    """H2: per-cell offset (+ optional rotation) on a square lattice of `cell`
    world units, the four bracketing cells blended over a band `border` cells
    wide.  rot: 'none' | 'quad' (multiples of 90 degrees) | 'free'."""
    def tap(dds, wx, wy, tile, upt, mip):
        m = mipbias if mip == 'code' else float(mip) + mipbias
        wx = np.asarray(wx, np.float64)
        wy = np.asarray(wy, np.float64)
        gx = wx / cell - 0.5
        gy = wy / cell - 0.5
        i0 = np.floor(gx)
        j0 = np.floor(gy)
        fx = gx - i0
        fy = gy - j0
        b = max(float(border), 1e-6)
        sx = _smoothstep((fx - 0.5) / b + 0.5)
        sy = _smoothstep((fy - 0.5) / b + 0.5)
        mean = _mean_colour(dds)
        taps = []
        ws = []
        for di, dj in ((0, 0), (1, 0), (0, 1), (1, 1)):
            w = (sx if di else 1.0 - sx) * (sy if dj else 1.0 - sy)
            ii = (i0 + di).astype(np.int64)
            jj = (j0 + dj).astype(np.int64)
            cxw = (ii + 0.5) * cell          # this cell's centre in world units
            cyw = (jj + 0.5) * cell
            lx = wx - cxw
            ly = wy - cyw
            if rot == 'none':
                rx, ry = lx, ly
            else:
                if rot == 'quad':
                    q = np.floor(hash01(ii, jj, 2) * 4.0)
                    ang = q * (np.pi * 0.5)
                else:
                    ang = hash01(ii, jj, 2) * (2.0 * np.pi)
                ca = np.cos(ang)
                sa = np.sin(ang)
                rx = ca * lx - sa * ly
                ry = sa * lx + ca * ly
            ox = hash01(ii, jj, 0) * tile
            oy = hash01(ii, jj, 1) * tile
            taps.append(_orig_tap(dds, cxw + rx + ox, cyw + ry + oy, tile, upt, m))
            ws.append(w)
        return _blend(taps, ws, mean, varnorm)
    return tap


def bake(cx, cy, tap):
    """One offline sheet through a candidate tap, at the shipped footprint mip."""
    OB._tap = tap
    try:
        return OB.bake(cx, cy, dim=4, tile=TILE, mip='code')
    finally:
        OB._tap = _orig_tap


def strain_of(tap_kind):
    """Both candidates are piecewise constant in their offsets, so the RMS local
    stretch of the coordinate map is exactly 0 away from the lattice edges.  H2
    with a rotation is a rigid motion per cell, which is also strain-free.  This
    is a property of the construction, not a measurement, and it is the reason
    the candidates exist -- but the BLEND still mixes two differently-placed
    copies of the texture near a lattice edge, and that mixing is what the band
    table and the local variance are asked about."""
    return 0.0
