"""Lane GROUND1 Part B, gate F1: what vanilla's own LOD normal sheets say about
erosion, measured BEFORE the pass that has to reproduce it exists.

The subject is vanilla's `_msn` at dim 4: 512 texels over 16,384 world units, so
one texel is 32 u and our LAND height grid (128 u) is FOUR texels. Everything at
or below four texels is relief our heightfield cannot represent at all, which is
precisely what bungo says we lost:

    "We lose all the fluvial, erosion features and other topographical features"

TILING3 established the channel order and this script re-checks it rather than
trusting it: `src/lodgen.cpp:5566`, R = east, G = up, B = north.

A NORMAL MAP IS A GRADIENT FIELD, which is why no height integration happens
anywhere below. For a height h over the world plane, the surface normal is
proportional to (-dh/dx, 1, -dh/dy), so

    gx = -east/up ,  gy = -north/up

is the height gradient itself, in height units per world unit. Every statistic
here is computed on that gradient field.

The four statistics, each with its floor, fixed before any of them was read:

  S1  WHERE THE RELIEF LIVES.  The band table of the gradient magnitude, and the
      share of its variance finer than 4 texels (finer than our height grid).
      Floor: none needed, it is a decomposition, not a detection. It is the
      target amplitude the pass has to hit.

  S2  IS IT FLUVIAL OR IS IT NOISE.  In each texel's OWN coarse frame -- down =
      steepest descent of the 4-texel-blurred gradient, across = perpendicular
      to it -- split the fine gradient into its across-slope and along-slope
      parts and take

          A = var(fine . across) / var(fine . along)

      A rill is a channel that runs DOWNHILL and has walls facing ACROSS, so its
      gradient swings across-slope and stays put along-slope: A > 1. Isotropic
      noise reads A = 1 whatever its spectrum.
      Floors: (a) the PHASE TWIN of each fine component -- identical power
      spectrum, random phase, so it keeps any GLOBAL anisotropy and destroys the
      LOCAL alignment, which is the thing being claimed; (b) white noise of the
      same SD.

  S3  HOW BIG ARE THE CHANNELS.  Per 64x64 block (2,048 u), the autocorrelation
      of (fine . across) sampled ALONG the across direction at fractional lags
      by the band-limited form (no resampling): the first minimum is half the
      rill spacing, the half-width of the central peak is the rill width.
      Floor: the same statistic on the phase twin.

  S4  DOES IT SCALE WITH SLOPE.  Real drainage cuts hardest where the water runs
      fastest. Regress the local SD of the fine gradient (r = 4 texels) on the
      coarse slope magnitude and report Pearson r and the fitted slope.
      Floor: the phase twin, whose local SD is by construction unrelated to the
      coarse slope.

CEILING for all four: the same statistic on the EAST NEIGHBOUR sheet of each of
the 22. Two adjacent vanilla sheets are the same terrain type, the same tools
and the same codec, so the spread between them is how close anything can be
asked to get.

The 22 sheets are TILING2's, taken from its own `sheets.json`, not re-picked.
"""
import io
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = r'E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, os.path.join(REPO, 'scratchpad', 'splat1_20260911'))
import splatlib as S                                              # noqa: E402

TEXEL_WORLD = 32.0          # dim 4: 512 texels over 16,384 u
COARSE_R = 2                # box radius 2 = 5 texels = 160 u ~ the 128-u grid
LOCAL_R = 4


# --------------------------------------------------------------- the field

def gradient_field(path):
    """(gx, gy) in height units per world unit, from a vanilla `_msn`."""
    a = S.Dds(path).level(0)
    v = a[:, :, :3].astype(np.float64) / 127.5 - 1.0
    east, up, north = v[:, :, 0], v[:, :, 1], v[:, :, 2]
    up = np.maximum(up, 1.0e-3)                  # never divide by a flat-on face
    return -east / up, -north / up, (east, up, north)


def split(gx, gy, r=COARSE_R):
    cx, cy = S._box(gx, r), S._box(gy, r)
    return cx, cy, gx - cx, gy - cy


def frame(cx, cy):
    """down = steepest descent of the coarse surface; across = perpendicular."""
    m = np.sqrt(cx * cx + cy * cy)
    safe = np.maximum(m, 1.0e-9)
    dx, dy = cx / safe, cy / safe            # the gradient points UPHILL; the
    return dx, dy, -dy, dx, m                # axis is what matters, not the sign


# --------------------------------------------------------------- statistics

def s1_bands(gm):
    ctr, pw = S.radial_power(gm)
    tab = S.band_table(ctr, pw)
    tot = sum(v for _, v in tab) or 1.0
    fine = sum(v for name, v in tab if name.startswith(('4..8', '2..4', '<=2')))
    return tab, fine / tot


def s2_anisotropy(fx, fy, ax, ay, dx, dy):
    acr = fx * ax + fy * ay
    aln = fx * dx + fy * dy
    va, vl = float(acr.var()), float(aln.var())
    return (va / vl if vl > 0 else float('nan')), va, vl


def _rho_line(P, ux, uy, lags):
    """Autocorrelation at fractional lags along (ux,uy), band-limited."""
    h, w = P.shape
    ky = np.fft.fftfreq(h)[:, None]
    kx = np.fft.fftfreq(w)[None, :]
    tot = float(P.sum())
    out = []
    for L in lags:
        c = np.cos(2.0 * np.pi * (ky * (uy * L) + kx * (ux * L)))
        out.append(float((P * c).sum()) / tot if tot > 0 else 0.0)
    return np.array(out)


def s3_channels(acr, ax, ay, block=64):
    """First minimum (half the spacing) and half-width, per block, in texels."""
    h, w = acr.shape
    spac, wide = [], []
    lags = np.arange(0.0, 24.25, 0.25)
    for by in range(0, h - block + 1, block):
        for bx in range(0, w - block + 1, block):
            f = acr[by:by + block, bx:bx + block]
            ux = float(ax[by:by + block, bx:bx + block].mean())
            uy = float(ay[by:by + block, bx:bx + block].mean())
            n = (ux * ux + uy * uy) ** 0.5
            if n < 0.35:              # the block has no agreed across direction
                continue
            ux, uy = ux / n, uy / n
            g = f - f.mean()
            win = np.outer(np.hanning(block), np.hanning(block))
            F = np.fft.fft2(g * win)
            r = _rho_line(np.abs(F) ** 2, ux, uy, lags)
            if r[0] <= 0:
                continue
            r = r / r[0]
            i = 1
            while i < len(r) - 1 and not (r[i] < r[i - 1] and r[i] <= r[i + 1]):
                i += 1
            if i >= len(r) - 1:
                continue
            spac.append(2.0 * lags[i])
            j = 0
            while j < len(r) and r[j] > 0.5:
                j += 1
            wide.append(lags[j] if j < len(r) else float('nan'))
    if not spac:
        return float('nan'), float('nan'), 0
    return float(np.median(spac)), float(np.nanmedian(wide)), len(spac)


DRIVER_R = 8                # 17 texels, 544 u -- above every rill this looks at


def s4_slope(fx, fy, gx, gy, r=LOCAL_R, dr=DRIVER_R):
    """Does the fine relief get stronger where the ground is steeper?

    The driver is the slope magnitude at a blur radius of 8 texels, NOT the
    5-texel coarse field the frame uses. Control C1 is why: a box of width 5
    does not null a period-8 corrugation, so the 5-texel "coarse" slope still
    wobbles in phase with the rills, and correlating the rills' own amplitude
    against it read r = +0.468 on a plane built with a CONSTANT rill amplitude.
    Both sides are then read at the same 8-texel scale so neither carries a
    band the other has."""
    fm = np.sqrt(fx * fx + fy * fy)
    loc = np.sqrt(np.maximum(S._box(fm * fm, r) - S._box(fm, r) ** 2, 0.0))
    m = np.sqrt(S._box(gx, dr) ** 2 + S._box(gy, dr) ** 2)
    a, b = S._box(loc, dr).ravel(), m.ravel()
    ok = np.isfinite(a) & np.isfinite(b)
    a, b = a[ok], b[ok]
    if a.size < 100 or a.std() == 0 or b.std() == 0:
        return float('nan'), float('nan')
    r_p = float(np.corrcoef(a, b)[0, 1])
    k = float(np.polyfit(b, a, 1)[0])
    return r_p, k


# --------------------------------------------------------------- one sheet

def measure(cx, cy, seed=1):
    path = S.van_sheet(cx, cy, '_msn')
    if not os.path.exists(path):
        return None
    gx, gy, raw = gradient_field(path)
    ccx, ccy, fx, fy = split(gx, gy)
    dx, dy, ax, ay, m = frame(ccx, ccy)
    gm = np.sqrt(gx * gx + gy * gy)

    tab, fineShare = s1_bands(gm)
    A, va, vl = s2_anisotropy(fx, fy, ax, ay, dx, dy)
    spac, wide, nb = s3_channels(fx * ax + fy * ay, ax, ay)
    rp, k = s4_slope(fx, fy, gx, gy)

    # floor 1: the phase twin of each fine component, same spectrum, no phase
    tx = S.phase_twin(fx, seed) - fx.mean()
    ty = S.phase_twin(fy, seed + 77) - fy.mean()
    A_t, _, _ = s2_anisotropy(tx, ty, ax, ay, dx, dy)
    spac_t, wide_t, _ = s3_channels(tx * ax + ty * ay, ax, ay)
    rp_t, k_t = s4_slope(tx, ty, ccx + tx, ccy + ty)

    # floor 2: white noise of the same SD
    rng = np.random.default_rng(seed + 999)
    nx = rng.standard_normal(fx.shape) * fx.std()
    ny = rng.standard_normal(fy.shape) * fy.std()
    A_n, _, _ = s2_anisotropy(nx, ny, ax, ay, dx, dy)

    return dict(
        cell=[cx, cy], bands=tab, fineShare=fineShare,
        gradMean=float(gm.mean()), gradSd=float(gm.std()),
        fineSd=float(np.sqrt(fx.var() + fy.var())),
        coarseSlopeMean=float(m.mean()),
        A=A, A_twin=A_t, A_white=A_n,
        spacingTx=spac, widthTx=wide, blocks=nb,
        spacing_twin=spac_t, width_twin=wide_t,
        slopeR=rp, slopeK=k, slopeR_twin=rp_t, slopeK_twin=k_t,
        meanRGB=[float(raw[0].mean() * 127.5 + 127.5),
                 float(raw[1].mean() * 127.5 + 127.5),
                 float(raw[2].mean() * 127.5 + 127.5)])


def main():
    sj = json.load(io.open(os.path.join(REPO, 'scratchpad', 'tiling2_20260911',
                                        'sheets.json'), encoding='utf-8'))
    cells = [tuple(c) for c in sj['sheets']]
    out, ceil = [], []
    for cx, cy in cells:
        r = measure(cx, cy)
        if r is None:
            print('MISSING %d.%d' % (cx, cy))
            continue
        out.append(r)
        nb = measure(cx + 4, cy)           # the east neighbour = the ceiling
        if nb is not None:
            ceil.append(dict(a=[cx, cy], b=[cx + 4, cy],
                             dA=abs(r['A'] - nb['A']) / max(r['A'], 1e-9),
                             dFine=abs(r['fineShare'] - nb['fineShare'])
                             / max(r['fineShare'], 1e-9),
                             dSpacing=abs(r['spacingTx'] - nb['spacingTx'])
                             / max(r['spacingTx'], 1e-9),
                             dSd=abs(r['fineSd'] - nb['fineSd'])
                             / max(r['fineSd'], 1e-9)))
        print('%4d,%4d  fine %.4f  A %.3f (twin %.3f white %.3f)  '
              'spacing %.2f tx  width %.2f  slopeR %+0.3f (twin %+0.3f)'
              % (cx, cy, r['fineShare'], r['A'], r['A_twin'], r['A_white'],
                 r['spacingTx'], r['widthTx'], r['slopeR'], r['slopeR_twin']))
        sys.stdout.flush()

    def med(key, src=out):
        v = [s[key] for s in src if np.isfinite(s[key])]
        return float(np.median(v)) if v else float('nan')

    summary = dict(
        n=len(out),
        fineShare=med('fineShare'), fineSd=med('fineSd'),
        gradMean=med('gradMean'),
        A=med('A'), A_twin=med('A_twin'), A_white=med('A_white'),
        spacingTx=med('spacingTx'), spacing_twin=med('spacing_twin'),
        widthTx=med('widthTx'), width_twin=med('width_twin'),
        slopeR=med('slopeR'), slopeR_twin=med('slopeR_twin'),
        slopeK=med('slopeK'),
        ceilingA=float(np.median([c['dA'] for c in ceil])) if ceil else None,
        ceilingFine=float(np.median([c['dFine'] for c in ceil])) if ceil else None,
        ceilingSpacing=(float(np.median([c['dSpacing'] for c in ceil]))
                        if ceil else None),
        ceilingSd=float(np.median([c['dSd'] for c in ceil])) if ceil else None,
        ceilingPairs=len(ceil))
    print('\n--- median of %d vanilla sheets ---' % len(out))
    for k in sorted(summary):
        print('  %-16s %s' % (k, summary[k]))
    json.dump(dict(sheets=out, ceiling=ceil, summary=summary),
              io.open(os.path.join(HERE, 'b1_vanilla.json'), 'w',
                      encoding='utf-8'), indent=1)


if __name__ == '__main__':
    main()
