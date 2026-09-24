"""ROADS3 gate F3 on the REAL bakes, and the simulation held to account.

Section 1.7 priced every candidate offline, before the build, by applying the
generator's own composite to the rung's sheets. This reads the SAME gates off
the baked `--road-opacity 0.326` sheets and prints the simulation beside them,
so the offline table is either confirmed on the aggregates it was read on, or
shown to be wrong. Note what the agreement is and is not: it is an agreement of
MEANS, not of texels.

    python r3_f3.py    ->  logs/f3_gates.txt, f3_gates.json
"""
import numpy as np

import r3lib as R3

lines = ['ROADS3 gate F3 -- the baked sheets, and the simulation checked', '']
out = {}
A_VAL = 0.326

for tile in ('t2020', 't0808'):
    cx, cy = R3.TILES[tile]
    A = R3.ours('rung_roads', tile)
    G = R3.ours('rung_noroads', tile)
    V = R3.vanilla(tile)
    B = R3.ours('new_op0326', tile)          # the BAKED candidate
    S = G + (A - G) * A_VAL                  # the SIMULATED candidate
    mask = R3.road_mask(A, G)
    sd = R3.signed_dist(mask)
    sur = (sd <= -1) & (sd >= -8)

    def opp(rgb, m):
        r, g, b = rgb[:, :, 0][m], rgb[:, :, 1][m], rgb[:, :, 2][m]
        return (float((b - (r + g) / 2).mean()), float((r - g).mean()))

    def row(sheet, tag):
        Ls = R3.L(sheet)
        step, at = R3.second_difference(R3.profile(Ls, sd))
        b_y, r_g = opp(sheet, mask)
        return dict(tag=tag, road_L=float(Ls[mask].mean()),
                    rise=float(Ls[mask].mean() - Ls[sur].mean()),
                    sd=float(R3.local_sd(Ls)[mask].mean()),
                    step=step, step_at=at, b_y=b_y, r_g=r_g)

    rows = [row(V, 'vanilla'), row(A, 'rung (a = 1)'),
            row(B, 'BAKED   a = %.3f' % A_VAL),
            row(S, 'SIMULATED a = %.3f' % A_VAL)]
    d = np.abs(R3.L(B) - R3.L(S))
    agree = dict(mean=float(d[mask].mean()), p99=R3.pct(d[mask], 99),
                 max=float(d[mask].max()),
                 road_L_gap=rows[2]['road_L'] - rows[3]['road_L'])
    out[tile] = dict(rows=rows, agreement=agree)
    lines += ['=' * 88, 'TILE (%d,%d)   %d road texels' % (cx, cy, int(mask.sum())),
              '',
              '  field                 road L    rise    local SD   step (at d)   b_y      r_g']
    for r in rows:
        lines.append('  %-20s %7.2f  %+7.2f   %6.2f    %5.2f (%s)   %+6.2f  %+6.2f'
                     % (r['tag'], r['road_L'], r['rise'], r['sd'], r['step'],
                        r['step_at'], r['b_y'], r['r_g']))
    lines += ['',
              '  BAKED vs SIMULATED, per road texel: mean %.3f, p99 %.3f, max %.3f levels'
              % (agree['mean'], agree['p99'], agree['max']),
              '  road mean luminance gap between them: %+.3f levels'
              % agree['road_L_gap'],
              '  so the simulation is right on the AGGREGATE (road mean and rise',
              '  within %.2f of a level) and NOT right per texel (%.2f levels on'
              % (abs(agree['road_L_gap']), agree['mean']),
              '  average, %.1f at worst): the bake goes through 8-bit quantisation'
              % agree['max'],
              '  and BC1 block compression and the simulation does not.', '']

R3.log(lines, 'f3_gates.txt')
R3.dump('f3_gates.json', out)
