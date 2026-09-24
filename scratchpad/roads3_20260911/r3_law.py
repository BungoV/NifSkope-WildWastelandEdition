"""ROADS3 gate F1, part 2 -- what vanilla's road ACTUALLY tracks.

`r3_fit.py` refused the brief's per-texel opacity law with its own floor (the
unexplained fraction equals the shuffled-ground floor on (-20,20) and is WORSE
than it on (-8,8)).  So the question is asked the other way round, on vanilla
ALONE, with no reference to our bake at all:

    does a vanilla road texel's luminance follow the LOCAL GROUND around it,
    or is it a fixed paint colour that ignores the ground?

The discriminator is the slope of  road_L  on  localGround_L  :

    slope ~ 1   the road is the ground plus a rise -- a wash
    slope ~ 0   the road is a fixed colour -- a paint

`localGround_L` is the mean luminance of the NON-road texels inside a radius-r
disc around the texel, computed on the SAME sheet.  Ours is measured the same
way on our own sheet, so the two slopes are comparable.

FLOORS, both sides:
  * the same regression with the local-ground field TRANSLATED by a large
    random shift (keeps its amplitude and its spectrum, breaks its
    registration) -- a slope near 0 there is what "no relationship" reads as;
  * a KNOWN-ANSWER pair built from the sheets themselves: a synthetic road of
    a fixed colour (slope must come back ~0) and a synthetic road of
    ground + 4 levels (slope must come back ~1), both measured by this same
    code.

    python r3_law.py   ->  logs/f1_law.txt, f1_law.json
"""
import numpy as np

import r3lib as R3
import splatlib as S

RAD = 8                    # the "local ground" disc, 8 texels = 256 world units

lines = ['ROADS3 gate F1 part 2 -- does vanilla`s road follow the local ground?',
         '']
out = {}


def local_ground(Lf, mask, r=RAD):
    """Mean luminance of the NON-road texels within a box of radius r.

    Returned as NaN where the box holds fewer than 20 non-road texels, so a
    road texel deep inside a wide junction never invents a ground."""
    w = (~mask).astype(np.float64)
    num = S._box(Lf * w, r)
    den = S._box(w, r)
    n = den * (2 * r + 1) ** 2
    g = np.full(Lf.shape, np.nan)
    ok = n >= 20
    g[ok] = (num / den)[ok]
    return g, ok


def slope(y, x):
    """Ordinary least squares slope and intercept, plus r."""
    x = x - x.mean()
    yy = y - y.mean()
    s = float((x * yy).sum() / max((x * x).sum(), 1e-12))
    r = float(np.corrcoef(y, x + x.mean())[0, 1])
    return s, float(y.mean() - s * (x.mean() + x.mean() * 0)), r


for tile in ('t2020', 't0808'):
    cx, cy = R3.TILES[tile]
    A = R3.ours('rung_roads', tile)
    G = R3.ours('rung_noroads', tile)
    V = R3.vanilla(tile)
    mask = R3.road_mask(A, G)
    sd = R3.signed_dist(mask)
    Lv, La, Lg = R3.L(V), R3.L(A), R3.L(G)
    t = {}
    lines += ['=' * 78, 'TILE (%d,%d)' % (cx, cy), '']

    rows = []
    for name, Lf in (('vanilla', Lv), ('ours (rung)', La), ('our ground', Lg)):
        g, ok = local_ground(Lf, mask)
        m = mask & ok
        s, b, r = slope(Lf[m], g[m])
        rows.append(dict(field=name, n=int(m.sum()), slope=s, intercept=b,
                         corr=r, road_mean=float(Lf[m].mean()),
                         ground_mean=float(g[m].mean()),
                         rise=float((Lf[m] - g[m]).mean()),
                         rise_sd=float((Lf[m] - g[m]).std())))
    # the translated floor, on vanilla
    rng = np.random.default_rng(3)
    fl = []
    gv, okv = local_ground(Lv, mask)
    for _ in range(5):
        gs = np.roll(np.roll(gv, int(rng.integers(80, 430)), 0),
                     int(rng.integers(80, 430)), 1)
        oks = np.roll(np.roll(okv, int(rng.integers(80, 430)), 0),
                      int(rng.integers(80, 430)), 1)
        m = mask & okv & oks & np.isfinite(gs)
        fl.append(slope(Lv[m], gs[m])[0])
    t['floor_translated_slope'] = dict(mean=float(np.mean(fl)),
                                       max=float(np.max(np.abs(fl))))

    # the two known answers, built from these very sheets
    for nm, synth in (('KNOWN fixed paint 100', np.where(mask, 100.0, Lv)),
                      ('KNOWN ground + 4', np.where(mask, gv + 4.0, Lv))):
        g2, ok2 = local_ground(synth, mask)
        m = mask & ok2 & np.isfinite(g2)
        s, b, r = slope(synth[m], g2[m])
        rows.append(dict(field=nm, n=int(m.sum()), slope=s, intercept=b,
                         corr=r, road_mean=float(synth[m].mean()),
                         ground_mean=float(g2[m].mean()),
                         rise=float((synth[m] - g2[m]).mean()),
                         rise_sd=float((synth[m] - g2[m]).std())))
    t['rows'] = rows
    lines += ['  road luminance regressed on the LOCAL GROUND (disc r=%d texels):' % RAD,
              '    field                  n      slope   corr    road L   ground L   rise']
    for r in rows:
        lines.append('    %-20s %6d   %+6.3f  %+.3f   %6.2f   %6.2f    %+6.2f (sd %.2f)'
                     % (r['field'], r['n'], r['slope'], r['corr'],
                        r['road_mean'], r['ground_mean'], r['rise'], r['rise_sd']))
    lines += ['    FLOOR (local ground translated, 5 draws): slope mean %+.3f, '
              'worst |slope| %.3f' % (t['floor_translated_slope']['mean'],
                                      t['floor_translated_slope']['max']), '']

    # the surround contrast, the number TILING2's addendum quoted
    band = mask & False
    sur = (sd <= -1) & (sd >= -8)
    t['contrast'] = {}
    for name, Lf in (('vanilla', Lv), ('ours', La), ('our ground', Lg)):
        t['contrast'][name] = dict(road=float(Lf[mask].mean()),
                                   surround=float(Lf[sur].mean()),
                                   rise=float(Lf[mask].mean() - Lf[sur].mean()))
    lines += ['  ROAD minus SURROUND (surround = 1..8 texels outside the mask):']
    for k, v in t['contrast'].items():
        lines.append('    %-12s road %6.2f  surround %6.2f  rise %+6.2f'
                     % (k, v['road'], v['surround'], v['rise']))
    lines.append('')

    # what a GLOBAL opacity would have to be, three targets, both readable
    Rl, Gl = float(La[mask].mean()), float(Lg[mask].mean())
    tgt_abs = float(Lv[mask].mean())
    tgt_rise = float(Lg[sur].mean()) + t['contrast']['vanilla']['rise']
    t['opacity_needed'] = dict(
        paint_L=Rl, ground_L=Gl,
        for_absolute=dict(target=tgt_abs,
                          a=(tgt_abs - Gl) / (Rl - Gl) if abs(Rl - Gl) > 1e-9 else None),
        for_rise=dict(target=tgt_rise,
                      a=(tgt_rise - Gl) / (Rl - Gl) if abs(Rl - Gl) > 1e-9 else None))
    lines += ['  WHAT A GLOBAL OPACITY WOULD HAVE TO BE',
              '    our paint on the road %.2f, our ground under it %.2f '
              '(the whole interval a can reach)' % (Rl, Gl),
              '    to hit vanilla`s ABSOLUTE road %.2f:  a = %s'
              % (tgt_abs, '%.3f' % t['opacity_needed']['for_absolute']['a']),
              '    to hit vanilla`s RISE over OUR surround (%.2f):  a = %s'
              % (tgt_rise, '%.3f' % t['opacity_needed']['for_rise']['a']), '']
    out[tile] = t

R3.log(lines, 'f1_law.txt')
R3.dump('f1_law.json', out)
