"""TILING2 -- the three instruments, and their floors.

bungo's three complaints, one instrument each:

  a. "you can see the tiling pattern of each texture"  -> `tiling_visibility`
  b. "hard blend edges also appear in some places"     -> `edge_widths`
  c. "it's muddy or blurry looking"                    -> `radial` / `bands`

Nothing here imports the generator. DDS decoding, local variance, the radial
power spectrum and the phase twin come from lane SPLAT1's `splatlib`, which was
gated 12/12 against an independent decoder before any verdict was read off it.

WHY THE PHASE TWIN IS NOT THE FLOOR FOR (a) or (c).  `phase_twin` keeps the
amplitude spectrum and randomises the phase, so its power spectrum -- and
therefore its autocorrelation -- is IDENTICAL to the subject's, bin for bin.
Any second-order statistic reads exactly the same on it. It is a floor for a
STRUCTURE statistic (a correlation against another field, which is what SPLAT1
used it for) and it is worth nothing as a floor for periodicity or for a
spectrum. The floor used here instead is a NULL SWEEP: the same prominence
statistic evaluated at periods that are not the repeat and not a codec or grid
period, on the same sheet, which is the distribution of the statistic under "no
repeat at this lag".
"""
import numpy as np
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'splat1_20260911'))
import splatlib as S                                          # noqa: E402

TEXEL_WORLD = 32.0          # dim 4, 512 texels over 16,384 world units
RES = 512
QUAD_TEXELS = 64            # 2,048 world units = one landscape quadrant


# --------------------------------------------------------------- (a) the repeat

def _hipass(L, r=16):
    """Remove everything coarser than (2r+1) texels -- the terrain's own shape.

    The repeat is 10.67 texels, so a 33x33 high pass keeps every candidate
    period this lane looks at (6..32) and removes the hillside gradients that
    otherwise dominate the autocorrelation at EVERY small lag."""
    return L.astype(np.float64) - S._box(L.astype(np.float64), r)


def _power(f):
    h, w = f.shape
    win = np.outer(np.hanning(h), np.hanning(w))
    g = (f - f.mean()) * win
    F = np.fft.fft2(g)
    return (np.abs(F) ** 2)


def _rho(P, dx, dy):
    """Normalised autocorrelation at a FRACTIONAL lag, exactly.

    r(d) = sum_k P(k) exp(2i.pi k.d/N) -- band-limited interpolation of the
    autocorrelation, not a resampling of it."""
    h, w = P.shape
    ky = np.fft.fftfreq(h)[:, None]
    kx = np.fft.fftfreq(w)[None, :]
    r = float((P * np.cos(2.0 * np.pi * (ky * dy + kx * dx))).sum())
    return r / float(P.sum())


NULL_PERIODS = (6.1, 7.3, 8.5, 9.1, 12.4, 13.7, 15.2, 17.3, 19.1, 23.4, 27.6, 31.5)


def _peak_ratio(P, period, direction, halo=1):
    """The repeat as the eye sees it is a NARROW SPIKE in the spectrum at the
    repeat's own frequency. Read it there, not in the autocorrelation:

      peak        = the largest power in the 3x3 bin neighbourhood of the
                    target frequency (spectral leakage of a non-integer repeat
                    lands in the neighbours);
      background  = the MEDIAN power over an annulus of the same radius,
                    +-25 percent, with a disc of 3 bins around every member of
                    the repeat's own frequency family removed;
      ratio       = peak / background, so 1.0 means "no spike here".

    The autocorrelation was tried first and refused (see the report, section 1):
    at a lag of ten texels a terrain sheet's autocorrelation is dominated by
    its own hillside decay, and an off-peak baseline cannot separate the decay
    from a peak -- the null sweep read 0.20 where an injected repeat of
    amplitude 8/255 read 0.06."""
    h, w = P.shape
    ky = np.fft.fftfreq(h) * h
    kx = np.fft.fftfreq(w) * w
    f0x = (w / period) * direction[0]
    f0y = (h / period) * direction[1]
    iy = int(round(f0y)) % h
    ix = int(round(f0x)) % w
    peak = 0.0
    for dy in range(-halo, halo + 1):
        for dx in range(-halo, halo + 1):
            peak = max(peak, float(P[(iy + dy) % h, (ix + dx) % w]))
    rad = np.sqrt(f0x ** 2 + f0y ** 2)
    KY = ky[:, None]
    KX = kx[None, :]
    R = np.sqrt(KX ** 2 + KY ** 2)
    ann = (R >= 0.75 * rad) & (R <= 1.25 * rad)
    # remove the whole repeat family (all four signed target directions and the
    # two axes) so the background is never the thing under test
    for (a, b) in ((f0x, f0y), (-f0x, f0y), (f0x, -f0y), (-f0x, -f0y),
                   (w / period, 0.0), (0.0, h / period),
                   (-w / period, 0.0), (0.0, -h / period),
                   (w / period, h / period), (-w / period, h / period),
                   (w / period, -h / period), (-w / period, -h / period)):
        ann &= ~((np.abs(KX - a) <= 3.0) & (np.abs(KY - b) <= 3.0))
    bg = float(np.median(P[ann])) if ann.any() else 0.0
    return (peak / bg if bg > 0 else float('inf')), peak, bg


def _peak_amp(P, period, direction, halo=1):
    """THE VISIBILITY ITSELF: the amplitude, in 8-bit luminance units, of the
    periodic component at this period and direction.

    A cosine of amplitude A under a 2-D Hann window has |F|^2 = (A*N^2/8)^2 at
    its own bin, so A = 8*sqrt(power)/N^2. The power is summed over the 3x3
    neighbourhood (the Hann window's leakage is confined to +-1 bin) with the
    annulus background subtracted from each of the nine, so a sheet with no
    repeat reads its own broadband noise at that scale and not more.

    Amplitude, not a peak-to-background RATIO: the ratio explodes to 1e9 on a
    band-limited synthetic whose background at bin 48 is numerically zero, and
    it is not comparable between sheets of different contrast. Amplitude is in
    the units of bungo's complaint -- how many levels of 255 the repeat moves
    the sheet by."""
    h, w = P.shape
    ratio, peak, bg = _peak_ratio(P, period, direction, halo)
    f0x = (w / period) * direction[0]
    f0y = (h / period) * direction[1]
    iy = int(round(f0y)) % h
    ix = int(round(f0x)) % w
    ex = 0.0
    for dy in range(-halo, halo + 1):
        for dx in range(-halo, halo + 1):
            ex += max(float(P[(iy + dy) % h, (ix + dx) % w]) - bg, 0.0)
    # The Hann window's own frequency kernel is [-1/4, 1/2, -1/4], so a single
    # sinusoid puts a quarter of its power into each neighbour bin on each axis:
    # a 3x3 sum holds 1.5 * 1.5 = 2.25x the centre's power. Checked against a
    # cosine of known amplitude in t1_selftest A0.
    return 8.0 * np.sqrt(ex) / float(h * w) / 1.5


def tiling_visibility(L, period=341.3333 / TEXEL_WORLD, full=False):
    """ONE NUMBER per sheet: the strongest of the three directions.

    Returns (visibility, floor, detail). Visibility is the repeat's AMPLITUDE
    in 8-bit luminance units. `floor` is the null sweep's MAXIMUM -- the same
    statistic at twelve periods that are not the repeat, not the BC block (4)
    and not the quadrant grid (64) -- on this same sheet, so it carries the
    sheet's own spectrum."""
    P = _power(_hipass(L))
    dirs = (('x', (1.0, 0.0)), ('y', (0.0, 1.0)), ('xy', (1.0, 1.0)))
    per = {}
    for name, d in dirs:
        r, pk, bg = _peak_ratio(P, period, d)
        per[name] = (_peak_amp(P, period, d), r)
    vis = max(v[0] for v in per.values())
    amp = vis
    nulls = [max(_peak_amp(P, q, d) for _n, d in dirs) for q in NULL_PERIODS]
    floor = float(max(nulls))
    if not full:
        return vis, floor
    return vis, floor, dict(per=per, amp=amp, nulls=nulls,
                            p90=float(np.percentile(nulls, 90)))


def T_amp(P, period, d):
    return _peak_amp(P, period, d)


def inject_repeat(L, period, amp, axis='x'):
    """Known answer, positive: a cosine of exactly this amplitude, so the
    instrument's own reading can be checked against the number injected."""
    yy, xx = np.mgrid[0:L.shape[0], 0:L.shape[1]].astype(np.float64)
    if axis == 'x':
        pat = np.cos(2 * np.pi * xx / period)
    elif axis == 'y':
        pat = np.cos(2 * np.pi * yy / period)
    else:
        pat = np.cos(2 * np.pi * (xx + yy) / period)
    return L.astype(np.float64) + amp * pat


def notch_repeat(L, period):
    """Known answer, negative: the same field with the repeat's own frequency
    family zeroed. Visibility must fall into the null floor."""
    h, w = L.shape
    F = np.fft.fft2(L.astype(np.float64))
    ky = np.fft.fftfreq(h)[:, None] * h      # cycles per tile
    kx = np.fft.fftfreq(w)[None, :] * w
    f0 = float(w) / period                   # 48.0 at 10.6667 over 512
    m = np.zeros_like(F, bool)
    for n in (1, 2, 3):
        for (a, b) in ((n * f0, 0.0), (0.0, n * f0), (n * f0, n * f0),
                       (n * f0, -n * f0)):
            m |= (np.abs(np.abs(kx) - abs(a)) < 1.5) & (np.abs(np.abs(ky) - abs(b)) < 1.5)
    F[m] = 0.0
    return np.real(np.fft.ifft2(F))


# ----------------------------------------------------------------- (b) the edges

def _grad(L):
    a = L.astype(np.float64)
    gx = np.zeros_like(a)
    gy = np.zeros_like(a)
    gx[:, 1:-1] = (a[:, 2:] - a[:, :-2]) * 0.5
    gy[1:-1, :] = (a[2:, :] - a[:-2, :]) * 0.5
    return gx, gy


def _bilinear(a, x, y):
    h, w = a.shape
    x = np.clip(x, 0, w - 1.001)
    y = np.clip(y, 0, h - 1.001)
    x0 = np.floor(x).astype(np.int64)
    y0 = np.floor(y).astype(np.int64)
    tx, ty = x - x0, y - y0
    return ((a[y0, x0] * (1 - tx) + a[y0, x0 + 1] * tx) * (1 - ty)
            + (a[y0 + 1, x0] * (1 - tx) + a[y0 + 1, x0 + 1] * tx) * ty)


HALF = 8.0
STEP = 0.25


def edge_widths(L, decile=0.90, maxPts=6000, seed=7):
    """The 10-90 percent transition width, in texels, of the strongest edges.

    For every texel in the top decile of gradient magnitude, the luminance is
    sampled along its own gradient direction over +-8 texels at quarter-texel
    steps; the profile's low and high plateaus are its min and max in that
    window, and the width is the distance between the last crossing of
    lo+0.1*(hi-lo) before the centre and the first crossing of lo+0.9*(hi-lo)
    after it (and symmetrically for a falling edge).

    Returns (widths, idx, grad) -- widths in texels, idx the flat indices of
    the texels they belong to, grad their gradient magnitudes."""
    a = L.astype(np.float64)
    gx, gy = _grad(a)
    g = np.sqrt(gx * gx + gy * gy)
    inner = np.zeros_like(g, bool)
    inner[10:-10, 10:-10] = True
    thr = np.quantile(g[inner], decile)
    sel = np.flatnonzero((g >= thr) & inner)
    rng = np.random.default_rng(seed)
    if sel.size > maxPts:
        sel = np.sort(rng.choice(sel, maxPts, replace=False))
    yy, xx = np.unravel_index(sel, a.shape)
    ux = gx[yy, xx] / np.maximum(g[yy, xx], 1e-9)
    uy = gy[yy, xx] / np.maximum(g[yy, xx], 1e-9)
    ts = np.arange(-HALF, HALF + STEP * 0.5, STEP)
    prof = np.stack([_bilinear(a, xx + t * ux, yy + t * uy) for t in ts], 1)
    lo = prof.min(1)
    hi = prof.max(1)
    rngv = hi - lo
    ok = rngv > 1.0
    t10 = lo + 0.1 * rngv
    t90 = lo + 0.9 * rngv
    mid = len(ts) // 2
    # the profile rises with the gradient direction by construction
    widths = np.full(prof.shape[0], np.nan)
    for k in range(prof.shape[0]):
        if not ok[k]:
            continue
        p = prof[k]
        # last index at or below t10 before the mid crossing, first at or above t90 after
        below = np.flatnonzero(p <= t10[k])
        above = np.flatnonzero(p >= t90[k])
        if below.size == 0 or above.size == 0:
            continue
        b = below[below <= mid]
        A = above[above >= mid]
        if b.size == 0 or A.size == 0:
            # edge whose plateaus both sit on one side of the centre
            b = below
            A = above
        i0 = b[-1]
        i1 = A[0]
        if i1 <= i0:
            i0 = below[0]
            i1 = above[-1]
        widths[k] = abs(ts[i1] - ts[i0])
    m = np.isfinite(widths)
    return widths[m], sel[m], g[yy, xx][m]


def quadrant_hits(idx, res=RES, quad=QUAD_TEXELS, tol=1):
    """How many of these texels lie within `tol` texels of a quadrant border,
    and how many would by chance (the fraction of the sheet's area that is)."""
    yy, xx = np.unravel_index(idx, (res, res))
    dx = np.minimum(xx % quad, quad - (xx % quad))
    dy = np.minimum(yy % quad, quad - (yy % quad))
    on = (dx <= tol) | (dy <= tol)
    band = (2 * tol + 1) / float(quad)
    chance = 1.0 - (1.0 - band) ** 2
    return int(on.sum()), int(len(idx)), float(chance)


# ---------------------------------------------------------------- (c) the detail

def bands(L):
    ctr, pw = S.radial_power(L.astype(np.float64))
    return ctr, pw, S.band_table(ctr, pw)


def spectrum_distance(La, Lb):
    """One number for "how different are these two sheets' detail": the mean
    absolute log2 ratio of band power over the four bands finer than 32 texels,
    which is where a blur or a repeat lives."""
    _c, _p, ba = bands(La)
    _c2, _p2, bb = bands(Lb)
    d = []
    for (na, va), (nb, vb) in list(zip(ba, bb))[2:]:
        d.append(abs(np.log2(max(va, 1e-9) / max(vb, 1e-9))))
    return float(np.mean(d))


def hp_residual(L, r=2):
    """The fine detail: everything finer than 5 texels (160 world units)."""
    a = L.astype(np.float64)
    return a - S._box(a, r)


def corr(a, b):
    x = np.asarray(a, np.float64).ravel()
    y = np.asarray(b, np.float64).ravel()
    x = x - x.mean()
    y = y - y.mean()
    d = np.sqrt((x * x).sum() * (y * y).sum())
    return float((x * y).sum() / d) if d > 0 else 0.0
