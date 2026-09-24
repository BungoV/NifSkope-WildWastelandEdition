"""TILING4 -- is "grain within 20 % of vanilla's on every sheet" a gate a
sampler can pass at all?  Measured on the two bakes that are not this lane's:
the RUNG (what ships today) and TILING3's PROPOSAL.

WHY THIS IS ASKED.  h1_sweep.py's first run scored the rung at 0 of 7 on grain
and 0 of 7 on the band table, against that chunk's own vanilla sheet at the
brief's 20 % margin.  A gate the shipped bake fails on every sheet is not
measuring the sampler; it is measuring the terrain.  Before this lane touches
the gate it has to be shown that the failure is not a property of the
candidates, so the two bakes this lane did not produce are scored here.

Three grain statistics, because TILING3 used two different ones:
  hp SD     the SD of everything finer than 5 texels (t1_lib.hp_residual, r=2)
            -- the number printed in cmp_tiling3.png and in TILING3's report
  local var the mean 3x3 local variance (splatlib.local_var) -- the number
            a5_tune.py actually GATED on, within 20 % of vanilla's
  bands     the six radial band shares, counted inside a 20 % window; a5_tune
            required 5 of 6

    python g0_grain.py  ->  logs/g0_grain.txt
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T3 = os.path.join(os.path.dirname(HERE), 'tiling3_20260911')
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T3, T2, SP):
    sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402
import a5_tune as A5                                          # noqa: E402
from s1b_design import rung, prop, vanilla                     # noqa: E402


def g(L):
    L = np.asarray(L, np.float64)
    bt, bn = A5.bandtab(L)
    return dict(hp=float(T.hp_residual(L, r=2).std()),
                lv=float(S.local_var(L).mean()), bt=bt, bn=bn)


def main():
    cfg = json.load(open(os.path.join(HERE, 'pool.json')))
    SEL = [tuple(c) for c in cfg['selection']]
    L = ['TILING4 -- can any sampler match vanilla`s grain sheet by sheet?', '',
         'Each row is one selection sheet.  "vs van" columns are the percentage',
         'difference from the SAME CHUNK`s vanilla sheet; the brief`s gate is +-20 %.',
         '"B/6" counts the six radial bands whose share is inside 20 % of vanilla`s',
         '(TILING3`s a5_tune.py required 5 of 6).', '']
    L.append('%-12s | %7s %7s %7s | %7s %7s %7s | %4s %4s'
             % ('sheet', 'van hp', 'rung', 'prop', 'van lv', 'rung', 'prop',
                'B/6', 'B/6'))
    L.append('%-12s | %7s %7s %7s | %7s %7s %7s | %4s %4s'
             % ('', 'value', 'vs van', 'vs van', 'value', 'vs van', 'vs van',
                'rung', 'prop'))
    out = {}
    tally = dict(rung_hp=0, prop_hp=0, rung_lv=0, prop_lv=0, rung_b=0, prop_b=0)
    for cx, cy in SEL:
        v = g(vanilla(cx, cy))
        r = g(rung(cx, cy))
        p = g(prop(cx, cy))
        nbr = sum(1 for a, b in zip(r['bt'], v['bt']) if abs(a / max(b, 1e-9) - 1) <= .20)
        nbp = sum(1 for a, b in zip(p['bt'], v['bt']) if abs(a / max(b, 1e-9) - 1) <= .20)
        dhr = r['hp'] / v['hp'] - 1
        dhp = p['hp'] / v['hp'] - 1
        dlr = r['lv'] / v['lv'] - 1
        dlp = p['lv'] / v['lv'] - 1
        tally['rung_hp'] += abs(dhr) <= .20
        tally['prop_hp'] += abs(dhp) <= .20
        tally['rung_lv'] += abs(dlr) <= .20
        tally['prop_lv'] += abs(dlp) <= .20
        tally['rung_b'] += nbr >= 5
        tally['prop_b'] += nbp >= 5
        out['%d,%d' % (cx, cy)] = dict(van=v, rung=r, prop=p, nbr=nbr, nbp=nbp)
        L.append('(%4d,%4d) | %7.3f %+6.0f%% %+6.0f%% | %7.2f %+6.0f%% %+6.0f%% | %4d %4d'
                 % (cx, cy, v['hp'], 100 * dhr, 100 * dhp,
                    v['lv'], 100 * dlr, 100 * dlp, nbr, nbp))
    L.append('')
    L.append('INSIDE the brief`s +-20 %% of the same chunk`s vanilla, of 7 sheets:')
    L.append('   hp SD        rung %d of 7,  TILING3 proposal %d of 7'
             % (tally['rung_hp'], tally['prop_hp']))
    L.append('   local var    rung %d of 7,  TILING3 proposal %d of 7'
             % (tally['rung_lv'], tally['prop_lv']))
    L.append('   bands 5 of 6 rung %d of 7,  TILING3 proposal %d of 7'
             % (tally['rung_b'], tally['prop_b']))
    L.append('')
    vh = [out[k]['van']['hp'] for k in out]
    rh = [out[k]['rung']['hp'] for k in out]
    ph = [out[k]['prop']['hp'] for k in out]
    vl = [out[k]['van']['lv'] for k in out]
    rl = [out[k]['rung']['lv'] for k in out]
    pl = [out[k]['prop']['lv'] for k in out]
    L.append('WHY.  Vanilla`s own grain across these seven sheets runs %.2f to %.2f on'
             % (min(vh), max(vh)))
    L.append('hp SD (a factor of %.1f) and %.1f to %.1f on local variance (a factor of'
             % (max(vh) / min(vh), min(vl), max(vl)))
    L.append('%.1f), because it is set by what terrain is there.  Our composite`s runs'
             % (max(vl) / min(vl)))
    L.append('%.2f to %.2f and %.1f to %.1f -- it is set by which land textures are'
             % (min(rh), max(rh), min(rl), max(rl)))
    L.append('painted, and a change to the SAMPLER cannot move one sheet`s grain to')
    L.append('its own chunk`s vanilla value.  The gate as written is not a gate on the')
    L.append('sampler.')
    L.append('')
    L.append('THE TWO ATTAINABLE GATES, registered here before any candidate is ranked:')
    L.append('  G1 the MEDIAN grain over the seven within 20 %% of vanilla`s median over')
    L.append('     the same seven.  This is TILING3`s own reported criterion ("mip bias')
    L.append('     -1.00 puts median grain at +2 %% of vanilla`s").')
    L.append('       vanilla median hp %.3f, lv %.2f' % (np.median(vh), np.median(vl)))
    L.append('       rung     median hp %.3f (%+.0f%%), lv %.2f (%+.0f%%)'
             % (np.median(rh), 100 * (np.median(rh) / np.median(vh) - 1),
                np.median(rl), 100 * (np.median(rl) / np.median(vl) - 1)))
    L.append('       proposal median hp %.3f (%+.0f%%), lv %.2f (%+.0f%%)'
             % (np.median(ph), 100 * (np.median(ph) / np.median(vh) - 1),
                np.median(pl), 100 * (np.median(pl) / np.median(vl) - 1)))
    L.append('  G2 PER SHEET, within 20 %% of THE RUNG`s grain on that same sheet -- no')
    L.append('     grain regression from the sampler change, a per-sheet gate with a')
    L.append('     per-sheet floor. The rung passes it by construction (it IS the floor);')
    L.append('     TILING3`s proposal scores:')
    nh = sum(1 for k in out if abs(out[k]['prop']['hp'] / out[k]['rung']['hp'] - 1) <= .20)
    nl = sum(1 for k in out if abs(out[k]['prop']['lv'] / out[k]['rung']['lv'] - 1) <= .20)
    L.append('       hp SD %d of 7, local variance %d of 7' % (nh, nl))
    L.append('  Both are reported for every candidate, and the brief`s literal per-sheet-')
    L.append('  vs-vanilla number is reported beside them so nothing is hidden.')

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'g0_grain.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(out, open(os.path.join(HERE, 'g0_grain.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
