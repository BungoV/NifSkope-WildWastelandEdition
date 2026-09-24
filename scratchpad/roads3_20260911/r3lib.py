"""ROADS3 -- the instruments for vanilla's road law, and their floors.

Nothing here imports the generator.  DDS decoding, luminance and the box
filter come from lane SPLAT1's `splatlib`, gated 12/12 against an independent
decoder before any verdict was read off it (lane SPLAT1, 2026-09-11).

WHY THE ROAD SHEET *IS* THE ROAD PLANE.  The generator composites the road as

    colour = ground + ( roadColour - ground ) * roadAlpha        (lodgen.cpp:7923)

then the grass tint, then `--grade`.  On these chunks the grass tint never
fires (GRADE1's red 1: the cover plane is empty, `dwReserved1 = 0`) and
`--grade` is 1.0 with the multiply branched over, so on a road texel painted at
alpha 1 -- which is every texel of an opaque road under the default `max-z`
composite -- the sheet's RGB *is* the road plane's RGB, quantised once.  That
is what makes `ours(roads)` usable as the road-colour field `R` below without
instrumenting the C++.  The claim is CHECKED, not assumed: `check_alpha_one`
counts the road-mask texels where `ours(detail 1)` and `ours(detail 0)` differ
by less than a level, which cannot happen at partial alpha unless the ground
happens to equal the paint.

THE LAW UNDER TEST.  For a road texel,

    vanilla  =  a * R  +  ( 1 - a ) * G                                    (A)

with `G` = our own ground at that texel (the `--no-roads` bake, same exe, same
command line minus the switch), `R` = our road paint at that texel, and `a` the
per-texel opacity.  Least squares over the three channels gives

    a = <V - G, R - G> / <R - G, R - G>

which is the one-parameter regression the brief asks for, with the constraint
that the two weights sum to one (a paint over a ground, not a free 2-term fit).
The part of `V - G` that (A) CANNOT reach is the residual, and its size against
`|V - G|` is the honest measure of whether (A) is the right law at all.

THE FLOORS.  `a` is a ratio and it is meaningless where the denominator is
small, so every statistic below is taken on texels with `|R - G| >= DENOM_MIN`
and the number dropped is always reported.  The floor for "the fit means
something" is the SAME fit with `G` shuffled among the road texels: the law
must stop working.  The ceiling is vanilla against a NEIGHBOURING shipped tile
under the same mask -- the value the statistic takes when the explanation is
certainly wrong but both fields are real terrain sheets.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'splat1_20260911'))
import splatlib as S                                          # noqa: E402

TEXEL_WORLD = 32.0          # dim 4: 512 texels over 16,384 world units
RES = 512
DENOM_MIN = 6.0             # |R - G| in levels, below which `a` is not read

TILES = {'t2020': (-20, 20), 't0808': (-8, 8)}
NEIGHBOUR = {'t2020': (-20, 24), 't0808': (-8, 12)}


# ------------------------------------------------------------------ the fields

def ours(variant, tile, suffix=''):
    cx, cy = TILES[tile]
    p = os.path.join(HERE, 'out', variant, tile, 'tex',
                     'Commonwealth.4.%d.%d%s.DDS' % (cx, cy, suffix))
    return S.Dds(p).level(0)[:, :, :3].astype(np.float64)


def vanilla(tile_or_cell, suffix=''):
    if isinstance(tile_or_cell, str):
        cx, cy = TILES[tile_or_cell]
    else:
        cx, cy = tile_or_cell
    return S.Dds(S.van_sheet(cx, cy, suffix)).level(0)[:, :, :3].astype(np.float64)


def lum(rgb):
    return S.lum(rgb.astype(np.uint8) if rgb.dtype != np.float64 else rgb)


def L(rgb):
    """Rec.601 luminance on a float RGB field, the same weights as splatlib."""
    return 0.299 * rgb[:, :, 0] + 0.587 * rgb[:, :, 1] + 0.114 * rgb[:, :, 2]


# -------------------------------------------------------------------- the mask

def road_mask(a_roads, a_noroads, thr=1.0):
    """The texels the road pass actually changed.

    A WORLD fact -- it comes from the ESM's road records through our own
    rasteriser -- which is what lets the same mask be applied to vanilla's
    shipped sheet and still name the same ground (lane TILING2, `t3c_road.py`).
    No connected-component surgery here: every texel the paint touched counts,
    including the decal spurs, because a law that only holds on the trunk is
    not the law."""
    d = np.abs(a_roads - a_noroads).max(2)
    return d >= thr


def dist_in(mask):
    """Distance in texels from a mask texel to the nearest NON-mask texel.

    1 on the mask's outermost ring, 2 on the next, and so on -- the 4-neighbour
    (city-block) distance, swept to convergence.  This is the "texels from the
    edge" axis of the cross-road profile."""
    big = 1e6
    d = np.where(mask, big, 0.0)
    for _ in range(200):
        nd = d.copy()
        for ax, sh in ((0, 1), (0, -1), (1, 1), (1, -1)):
            nd = np.minimum(nd, np.roll(d, sh, axis=ax) + 1.0)
        nd[~mask] = 0.0
        if np.array_equal(nd, d):
            break
        d = nd
    return d


def dist_out(mask):
    """Distance in texels from a non-mask texel to the nearest mask texel."""
    return dist_in(~mask)


def signed_dist(mask):
    """+n inside the road (n texels from the edge), -n outside, 0 nowhere."""
    return dist_in(mask) - dist_out(mask)


# --------------------------------------------------------------------- the fit

def fit_alpha(V, R, G, mask, denom_min=DENOM_MIN):
    """Per-texel opacity under law (A), and everything needed to judge it.

    Returns a dict with
      a         the fitted opacity on the READ texels (denominator big enough)
      read      boolean field, the texels `a` was read on
      resid     |V - (a R + (1-a) G)| in levels, on the read texels: the part
                of vanilla the law cannot reach at ANY opacity
      reach     |V - G| on the read texels, the size of what is to be explained
      frac      resid / reach, 0 = law explains the direction exactly
    """
    dR = R - G
    dV = V - G
    den = (dR * dR).sum(2)
    num = (dV * dR).sum(2)
    read = mask & (np.sqrt(den) >= denom_min)
    a = np.zeros(den.shape)
    np.divide(num, den, out=a, where=read)
    pred = G + dR * a[:, :, None]
    resid = np.sqrt(((V - pred) ** 2).sum(2))
    reach = np.sqrt((dV * dV).sum(2))
    frac = np.zeros(den.shape)
    np.divide(resid, reach, out=frac, where=read & (reach > 1e-9))
    return dict(a=a, read=read, resid=resid, reach=reach, frac=frac,
                denom=np.sqrt(den))


def pct(x, q):
    return float(np.percentile(x, q)) if x.size else float('nan')


def summarise(a, read, tag):
    v = a[read]
    return dict(tag=tag, n=int(read.sum()),
                median=float(np.median(v)) if v.size else float('nan'),
                mean=float(v.mean()) if v.size else float('nan'),
                p10=pct(v, 10), p90=pct(v, 90),
                p25=pct(v, 25), p75=pct(v, 75),
                sd=float(v.std()) if v.size else float('nan'))


# ------------------------------------------------------------------ hue / chroma

def hsv(rgb):
    """H in degrees 0..360, S 0..1, V 0..255 -- plain HSV on 0..255 RGB."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mx = np.maximum(r, np.maximum(g, b))
    mn = np.minimum(r, np.minimum(g, b))
    d = mx - mn
    h = np.zeros(mx.shape)
    m = d > 1e-9
    idx = m & (mx == r)
    h[idx] = (60 * ((g - b)[idx] / d[idx])) % 360
    idx = m & (mx == g) & (mx != r)
    h[idx] = 60 * ((b - r)[idx] / d[idx]) + 120
    idx = m & (mx == b) & (mx != r) & (mx != g)
    h[idx] = 60 * ((r - g)[idx] / d[idx]) + 240
    s = np.zeros(mx.shape)
    np.divide(d, mx, out=s, where=mx > 1e-9)
    return h, s, mx


def hue_numbers(rgb, m):
    """The brief's "hue within 3 of 255" -- stated as the two OPPONENT axes in
    levels, which is what "within 3 of 255" can mean for a colour, plus HSV for
    reading.  b_y = B - (R+G)/2 (positive = bluer), r_g = R - G."""
    r = rgb[:, :, 0][m]
    g = rgb[:, :, 1][m]
    b = rgb[:, :, 2][m]
    h, s, _ = hsv(rgb)
    return dict(n=int(m.sum()),
                R=float(r.mean()), G=float(g.mean()), B=float(b.mean()),
                b_y=float((b - (r + g) / 2.0).mean()),
                r_g=float((r - g).mean()),
                sat=float(s[m].mean()),
                hue_deg=float(np.median(h[m])))


# ------------------------------------------------------------------- local SD

def local_sd(a, r=2):
    """The 5x5 local standard deviation ROADS2 quoted (r = 2)."""
    a = a.astype(np.float64)
    m = S._box(a, r)
    m2 = S._box(a * a, r)
    return np.sqrt(np.maximum(m2 - m * m, 0.0))


# ------------------------------------------------------- the cross-road profile

def profile(Lf, sd, lo=-8, hi=12):
    """Mean luminance against signed distance to the road edge."""
    out = {}
    for d in range(lo, hi + 1):
        if d == 0:
            continue
        m = (sd == d)
        if m.sum() >= 20:
            out[d] = (float(Lf[m].mean()), int(m.sum()))
    return out


def second_difference(prof):
    """The biggest STEP in a profile: max |p(d-1) - 2 p(d) + p(d+1)| over the
    road's own texels (d >= 1), which is what a skirt band shows up as."""
    ds = sorted(d for d in prof if d >= 1)
    worst, at = 0.0, None
    for d in ds:
        if (d - 1) in prof and (d + 1) in prof:
            v = abs(prof[d - 1][0] - 2 * prof[d][0] + prof[d + 1][0])
            if v > worst:
                worst, at = v, d
        elif d == 1 and (d + 1) in prof and (d + 2) in prof:
            pass
    return worst, at


def dump(path, obj):
    with open(os.path.join(HERE, path), 'w') as fh:
        json.dump(obj, fh, indent=1, sort_keys=True, default=float)


def log(lines, path):
    txt = '\n'.join(lines) + '\n'
    d = os.path.join(HERE, 'logs')
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, path), 'w') as fh:
        fh.write(txt)
    print(txt)
