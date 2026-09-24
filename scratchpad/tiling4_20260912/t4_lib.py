"""TILING4 -- the WARP-SIGNATURE instrument (the "swirl"), and nothing else.

bungo's complaint about TILING3's proposal is that the fix is VISIBLE as swirls.
TILING2 and TILING3 between them own the repeat, the edges, the grain and the
band table; none of those instruments reads a swirl, because a smooth domain
warp changes neither the amplitude spectrum, nor the histogram, nor the local
variance of the texture it resamples -- it only bends WHERE things are.  What a
warp does leave is an ORIENTATION field: the texture's own fine detail is
stretched along the warp's local principal direction, and that direction is the
same over the whole lattice cell, so the picture grows smears that are coherent
over tens of texels and that curve.

THE STATISTIC.  Energy-weighted mean coherence of the structure tensor of the
band-passed luminance, measured over a LARGE window:

    a       the sheet's luminance, optionally with the repeat's own frequency
            family notched out (t1_lib.notch_repeat, period 10.6667 texels)
    b       band pass: box(a, BAND_LO) - box(a, BAND_HI) -- the 9..33 texel
            band, which is where the warp's smears live (the warp lattice is
            1,024 world units = 32 texels)
    gx,gy   the gradient of b
    J       the structure tensor, each term box-smoothed over WIN texels
    coh     (l1 - l2) / (l1 + l2) per texel, in [0,1]
    SWIRL   sum(coh * trace(J)) / sum(trace(J))

WHY THE LARGE WINDOW IS THE WHOLE POINT.  At a 1-texel window every texture is
locally anisotropic and coherence reads near 1 for everything; the statistic
would measure nothing.  Over a 33-texel window a natural texture's orientations
average away and the reading falls toward the isotropic floor, while a warp's
smear holds ONE direction across the window and stays high.  The two known
answers below pin both ends of that.

WHY THE NOTCH.  A texture repeat is also coherent and also axis-aligned, so a
grid would inflate the same statistic.  The repeat has a fixed lattice period
and the swirl does not, so the repeat can be removed exactly -- its own
frequency family, and only that -- before the orientation is read.  Control C4
shows the notch moving the rung's reading down to its no-repeat control's and
leaving a warped sheet's reading alone: that is the separation the brief asks
for, measured rather than asserted.

FLOOR: the sheet's own phase twin (same amplitude spectrum, random phase).  A
phase twin is worthless as a floor for a periodicity or a spectrum -- TILING2
wrote that rule -- but orientation coherence is exactly the kind of structure
statistic it IS the floor for: the twin has the same energy at the same scales
and no orientation structure at all.

CEILING: vanilla's own sheets, the same population TILING2 froze its repeat
ceilings on.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T2, SP):
    if p not in sys.path:
        sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402

REPEAT_TEXELS = 341.3333 / 32.0        # 10.6667 -- one land-texture repeat
# FROZEN 00:2x by s1b_design.py, on the KNOWN defect only (a synthetic warp of
# known strain, and TILING3's proposal against its own rung), before any
# candidate of this lane existed.  The first values -- 4 / 16 / 16, the 9..33
# texel band -- FAILED two of the registered answers and are kept in
# logs/s1_controls.txt as the refused design.
BAND_LO = 0                            # box radius: 0 = keep the grain itself
BAND_HI = 2                            # box radius: removes coarser than 5 texels
WIN = 8                                # structure-tensor window radius (17 texels)


def _grad(a):
    gx = np.zeros_like(a)
    gy = np.zeros_like(a)
    gx[:, 1:-1] = (a[:, 2:] - a[:, :-2]) * 0.5
    gy[1:-1, :] = (a[2:, :] - a[:-2, :]) * 0.5
    return gx, gy


def bandpass(L, lo=BAND_LO, hi=BAND_HI):
    a = np.asarray(L, np.float64)
    return S._box(a, lo) - S._box(a, hi)


def swirl(L, notch=True, lo=BAND_LO, hi=BAND_HI, win=WIN, full=False):
    """The warp signature. Returns a scalar in [0,1] (or the maps with full)."""
    a = np.asarray(L, np.float64)
    if notch:
        a = T.notch_repeat(a, REPEAT_TEXELS)
    b = bandpass(a, lo, hi)
    gx, gy = _grad(b)
    jxx = S._box(gx * gx, win)
    jyy = S._box(gy * gy, win)
    jxy = S._box(gx * gy, win)
    tr = jxx + jyy
    dd = np.sqrt(np.maximum((jxx - jyy) ** 2 + 4.0 * jxy * jxy, 0.0))
    coh = dd / np.maximum(tr, 1e-12)
    # drop the border, where the box filters are edge-clamped and the tensor is
    # an artefact of the padding rather than of the sheet
    m = np.zeros(a.shape, bool)
    m[win + hi:-(win + hi), win + hi:-(win + hi)] = True
    w = tr * m
    val = float((coh * w).sum() / max(w.sum(), 1e-12))
    if not full:
        return val
    theta = 0.5 * np.arctan2(2.0 * jxy, jxx - jyy)
    return val, dict(coh=coh, theta=theta, energy=tr, mask=m)


def swirl_floor(L, seed=1, **kw):
    """The sheet's own phase twin: same spectrum, no orientation structure."""
    return swirl(S.phase_twin(np.asarray(L, np.float64), seed), **kw)


# ------------------------------------------------------- known-answer helpers

def _hash01(i, j, k):
    h = (i.astype(np.uint32) * np.uint32(374761393)
         + j.astype(np.uint32) * np.uint32(668265263)
         + np.uint32(k) * np.uint32(2246822519))
    h ^= h >> np.uint32(13)
    h = h * np.uint32(1274126177)
    h ^= h >> np.uint32(16)
    return h.astype(np.float64) / 4294967296.0


def warp_offsets(wx, wy, lattice):
    """TILING3's warp field, verbatim (a4_warp.warp_offsets), so the synthetic
    swirl this instrument is calibrated on is the SAME field the proposal
    applies -- not a look-alike."""
    gx = wx / lattice
    gy = wy / lattice
    i = np.floor(gx).astype(np.int64)
    j = np.floor(gy).astype(np.int64)
    fx = gx - i
    fy = gy - j
    sx = fx * fx * (3.0 - 2.0 * fx)
    sy = fy * fy * (3.0 - 2.0 * fy)
    out = []
    for k in (0, 1):
        a = _hash01(i, j, k)
        b = _hash01(i + 1, j, k)
        c = _hash01(i, j + 1, k)
        d = _hash01(i + 1, j + 1, k)
        out.append(((a * (1 - sx) + b * sx) * (1 - sy)
                    + (c * (1 - sx) + d * sx) * sy) * 2.0 - 1.0)
    return out[0], out[1]


def warp_strain(amp, lattice, n=4096, seed=3):
    """RMS local stretch of that field, in the same units a5_tune.strain used."""
    rng = np.random.default_rng(seed)
    wx = rng.uniform(-50000, 50000, n)
    wy = rng.uniform(-50000, 50000, n)
    h = 1.0
    ox0, oy0 = warp_offsets(wx, wy, lattice)
    ox1, _ = warp_offsets(wx + h, wy, lattice)
    _, oy1 = warp_offsets(wx, wy + h, lattice)
    dxdx = amp * (ox1 - ox0) / h
    dydy = amp * (oy1 - oy0) / h
    return float(np.sqrt((dxdx ** 2 + dydy ** 2).mean()))


def apply_swirl(L, amp_texels, lattice_texels, seed_shift=0.0):
    """Resample a FINISHED sheet through the same warp, in texel units.

    This is the positive known answer: the input is a real sheet whose swirl
    reading is already known, and the output is that same sheet with a warp of
    a known RMS strain applied to it and nothing else changed."""
    a = np.asarray(L, np.float64)
    h, w = a.shape
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    ox, oy = warp_offsets(xx + seed_shift, yy + seed_shift, lattice_texels)
    sx = xx + amp_texels * ox
    sy = yy + amp_texels * oy
    x0 = np.floor(sx).astype(np.int64)
    y0 = np.floor(sy).astype(np.int64)
    tx = sx - x0
    ty = sy - y0
    x0 = np.clip(x0, 0, w - 2)
    y0 = np.clip(y0, 0, h - 2)
    return ((a[y0, x0] * (1 - tx) + a[y0, x0 + 1] * tx) * (1 - ty)
            + (a[y0 + 1, x0] * (1 - tx) + a[y0 + 1, x0 + 1] * tx) * ty)


def grating(h, w, period=16.0, amp=8.0, angle=0.0, mean=100.0):
    """A pure 1-D sinusoid: perfectly coherent, so the instrument must read
    near 1.0 on it. The positive control that fails on broken code."""
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    t = xx * np.cos(angle) + yy * np.sin(angle)
    return mean + amp * np.cos(2.0 * np.pi * t / period)


def isotropic(h, w, seed=5, amp=8.0, mean=100.0, lo=8.0, hi=32.0):
    """Band-limited ISOTROPIC noise in the same band: the other end. The
    instrument must read near its floor here."""
    rng = np.random.default_rng(seed)
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    rad = np.sqrt(fy ** 2 + fx ** 2)
    m = (rad >= 1.0 / hi) & (rad <= 1.0 / lo)
    F = np.fft.fft2(rng.standard_normal((h, w))) * m
    a = np.real(np.fft.ifft2(F))
    a = a / max(a.std(), 1e-9) * amp
    return a + mean
