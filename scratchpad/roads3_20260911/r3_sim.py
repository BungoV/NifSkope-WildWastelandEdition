"""ROADS3 -- every candidate opacity rule simulated on the rung's own sheets,
with the whole F3 gate table read off each, BEFORE the build is spent.

The composite the generator runs is exactly

    colour = ground + ( paint - ground ) * a                 (lodgen.cpp:7923)

and on these chunks the grass tint never fires and `--grade` is 1.0, so the
simulation is the arithmetic the C++ will do, not an approximation of it.  The
one thing it cannot simulate is a change to the road plane's COVERAGE (the
skirt's vertex alpha), because the sheet does not carry coverage separately --
that limitation is stated, not hidden.

Candidates:
  flat a            a constant multiplier on the road plane's alpha
  ground-relative   a per texel, chosen so the result's luminance sits `rise`
                    levels above the mean luminance of the NON-road ground in a
                    disc of radius r -- the quantity vanilla's own sheets were
                    measured with in `r3_law.py` (+1.02 and +1.91 levels)

    python r3_sim.py   ->  logs/f2_sim.txt, f2_sim.json
"""
import numpy as np

import r3lib as R3
import splatlib as S

RAD = 8
lines = ['ROADS3 -- candidate opacity rules, simulated, full gate table', '']
out = {}


def disc_ground_L(Lg, mask, r=RAD):
    w = (~mask).astype(np.float64)
    den = S._box(w, r)
    num = S._box(Lg * w, r)
    return num / np.maximum(den, 1e-9)


for tile in ('t2020', 't0808'):
    cx, cy = R3.TILES[tile]
    A = R3.ours('rung_roads', tile)
    G = R3.ours('rung_noroads', tile)
    V = R3.vanilla(tile)
    mask = R3.road_mask(A, G)
    sd = R3.signed_dist(mask)
    sur = (sd <= -1) & (sd >= -8)
    Lv, La, Lg = R3.L(V), R3.L(A), R3.L(G)
    dg = disc_ground_L(Lg, mask)
    van = dict(L=float(Lv[mask].mean()),
               rise=float(Lv[mask].mean() - Lv[sur].mean()),
               sd=float(R3.local_sd(Lv)[mask].mean()),
               step=R3.second_difference(R3.profile(Lv, sd))[0])
    vo = dict(b_y=float((V[:, :, 2] - (V[:, :, 0] + V[:, :, 1]) / 2)[mask].mean()),
              r_g=float((V[:, :, 0] - V[:, :, 1])[mask].mean()))

    def row(sheet, tag, a_field=None):
        Ls = R3.L(sheet)
        b_y = float((sheet[:, :, 2] - (sheet[:, :, 0] + sheet[:, :, 1]) / 2)[mask].mean())
        r_g = float((sheet[:, :, 0] - sheet[:, :, 1])[mask].mean())
        d = dict(tag=tag,
                 road_L=float(Ls[mask].mean()),
                 dL=float(Ls[mask].mean() - van['L']),
                 rise=float(Ls[mask].mean() - Ls[sur].mean()),
                 d_rise=float(Ls[mask].mean() - Ls[sur].mean() - van['rise']),
                 sd=float(R3.local_sd(Ls)[mask].mean()),
                 sd_ratio=float(R3.local_sd(Ls)[mask].mean() / van['sd']),
                 step=R3.second_difference(R3.profile(Ls, sd))[0],
                 d_by=b_y - vo['b_y'], d_rg=r_g - vo['r_g'])
        if a_field is not None:
            d['a_median'] = float(np.median(a_field[mask]))
            d['a_p10'] = R3.pct(a_field[mask], 10)
            d['a_p90'] = R3.pct(a_field[mask], 90)
        return d

    rows = [row(A, 'rung  (a = 1, opaque)')]
    for a in (0.75, 0.5, 0.326, 0.25):
        rows.append(row(G + (A - G) * a, 'flat a = %.3f' % a))
    den = La - Lg
    for rise in (1.02, 1.50, 1.91, 3.0):
        af = np.zeros(den.shape)
        np.divide(dg + rise - Lg, den, out=af, where=np.abs(den) > 1e-6)
        af = np.clip(af, 0.0, 1.0)
        rows.append(row(G + (A - G) * af[:, :, None],
                        'ground-relative rise %.2f' % rise, af))

    lines += ['=' * 96,
              'TILE (%d,%d)   vanilla: road L %.2f  rise %+.2f  SD %.2f  step %.2f'
              % (cx, cy, van['L'], van['rise'], van['sd'], van['step']),
              '',
              '  rule                        road L    dL     rise   d_rise    SD   SD/van  step   d(b_y)  d(r_g)   a med']
    for r in rows:
        lines.append('  %-26s %7.2f %+7.2f %+7.2f %+7.2f  %5.2f  %5.2f  %5.2f  %+6.2f  %+6.2f   %s'
                     % (r['tag'], r['road_L'], r['dL'], r['rise'], r['d_rise'],
                        r['sd'], r['sd_ratio'], r['step'], r['d_by'], r['d_rg'],
                        ('%.3f' % r['a_median']) if 'a_median' in r else '  -  '))
    lines.append('')
    out[tile] = dict(vanilla=van, rows=rows)

lines += ['GATE READING (F3): dL must be within +-3; SD/van within 0.8..1.2;',
          'step must not exceed vanilla`s; d(b_y) and d(r_g) within +-3.']
R3.log(lines, 'f2_sim.txt')
R3.dump('f2_sim.json', out)
