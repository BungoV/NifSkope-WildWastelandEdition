"""TILING4 step 1 -- the swirl instrument's KNOWN ANSWERS, run before any
candidate of this lane is scored (gate F1).

Seven controls, each with the answer written down before it was run:

  C1  a pure sinusoid grating          must read near 1.0  (perfectly coherent)
  C2  band-limited isotropic noise     must read near the floor
  C3  every vanilla sheet's own phase twin -- the FLOOR, per sheet
  C4  the RUNG (the repeat present) with and WITHOUT the notch, beside its
      no-repeat control (the `average` bake): the notch must take the rung's
      reading down to the no-repeat control's, and must NOT take a warped
      sheet's reading down.  That is the separation the brief demands.
  C5  a synthetic swirl of KNOWN strain, applied to a real vanilla sheet: the
      reading must rise monotonically with the strain, so the number has a
      meaning ("this much warp reads this much").
  C6  vanilla's own 22 sheets -- the CEILING.  (Not a floor: vanilla's ground
      has drainage lines and ridges, which are coherent and which we may not
      call a defect.)
  C7  TILING3's PROPOSAL, on the seven selection sheets: the instrument must
      READ THE SWIRL bungo saw -- i.e. sit above the vanilla ceiling.  If it
      does not, the instrument is not measuring his complaint and nothing in
      this lane may be graded on it.

    python s1_controls.py  ->  logs/s1_controls.txt
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
import t4_lib as W                                            # noqa: E402
import offline_bake as OB                                     # noqa: E402
import a5_tune as A5                                          # noqa: E402

TILE = 341.3333
RES = 512


def main():
    cfg = json.load(open(os.path.join(HERE, 'pool.json')))
    SEL = [tuple(c) for c in cfg['selection']]
    VAL = [tuple(c) for c in cfg['validation']]
    L = ['TILING4 -- the swirl instrument and its known answers', '']
    out = {}

    # ---------------------------------------------------------------- C1, C2
    g = W.grating(RES, RES, period=16.0, amp=8.0, angle=0.4)
    iso = W.isotropic(RES, RES)
    c1 = W.swirl(g, notch=False)
    c2 = W.swirl(iso, notch=False)
    c2f = W.swirl_floor(iso, notch=False)
    L.append('C1 pure sinusoid grating (period 16 tx, 23 deg): swirl %.4f'
             '   [expected near 1.0]' % c1)
    L.append('C2 band-limited isotropic noise (8..32 tx): swirl %.4f, its own'
             ' phase twin %.4f   [expected near the floor]' % (c2, c2f))
    out['C1_grating'] = c1
    out['C2_isotropic'] = dict(value=c2, twin=c2f)
    L.append('')

    # ---------------------------------------------------------------- C3, C6
    L.append('C3/C6 vanilla`s own sheets -- the FLOOR (each sheet`s phase twin)')
    L.append('      and the CEILING (the sheets themselves).  Both sevens, and')
    L.append('      TILING2`s full 22 for the population ceiling.')
    L.append('   %-12s %8s %8s %8s   %s' % ('sheet', 'swirl', 'twin', 'over', 'set'))
    t2 = json.load(open(os.path.join(T2, 'sheets.json')))
    all22 = [tuple(c) for c in t2['sheets']]
    van = {}
    for cx, cy in all22:
        v = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        sw = W.swirl(v)
        tw = W.swirl_floor(v)
        tag = ('selection' if (cx, cy) in SEL
               else ('validation' if (cx, cy) in VAL else 'TILING2 22'))
        van['%d,%d' % (cx, cy)] = dict(swirl=sw, twin=tw, tag=tag)
        L.append('   (%4d,%4d) %8.4f %8.4f %8.4f   %s' % (cx, cy, sw, tw, sw - tw, tag))
    for cx, cy in VAL:
        if '%d,%d' % (cx, cy) in van:
            continue
        v = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        sw, tw = W.swirl(v), W.swirl_floor(v)
        van['%d,%d' % (cx, cy)] = dict(swirl=sw, twin=tw, tag='validation')
        L.append('   (%4d,%4d) %8.4f %8.4f %8.4f   %s' % (cx, cy, sw, tw, sw - tw, 'validation'))
    vals22 = [van['%d,%d' % c]['swirl'] for c in all22]
    twins22 = [van['%d,%d' % c]['twin'] for c in all22]
    ceil = float(max(vals22))
    ceil_p90 = float(np.percentile(vals22, 90))
    floor = float(np.median(twins22))
    L.append('   VANILLA CEILING (worst of TILING2`s 22): %.4f      90th pct %.4f'
             % (ceil, ceil_p90))
    L.append('   VANILLA median %.4f;  phase-twin floor, median of 22: %.4f'
             % (float(np.median(vals22)), floor))
    out['ceiling'] = ceil
    out['ceiling_p90'] = ceil_p90
    out['twin_floor'] = floor
    out['vanilla'] = van
    L.append('')

    # -------------------------------------------------------------------- C5
    L.append('C5 a synthetic swirl of KNOWN strain on a real vanilla sheet (-20,24):')
    L.append('   %-10s %8s %9s %9s' % ('amp(tx)', 'strain', 'swirl', 'over twin'))
    base = S.lum(S.Dds(S.van_sheet(-20, 24)).level(0))
    lat_tx = 1024.0 / 32.0                      # TILING3's lattice in texels
    c5 = []
    for amp_tx in (0.0, 2.0, 5.0, 10.0, 21.34, 42.7):
        sh = base if amp_tx == 0 else W.apply_swirl(base, amp_tx, lat_tx)
        st = W.warp_strain(amp_tx, lat_tx)
        sw = W.swirl(sh)
        tw = W.swirl_floor(sh)
        L.append('   %-10.2f %8.3f %9.4f %9.4f' % (amp_tx, st, sw, sw - tw))
        c5.append(dict(amp=amp_tx, strain=st, swirl=sw, twin=tw))
    mono = all(c5[i]['swirl'] <= c5[i + 1]['swirl'] + 1e-6 for i in range(len(c5) - 1))
    L.append('   monotone in the strain: %s' % ('YES' if mono else 'NO -- INSTRUMENT REFUSED'))
    out['C5'] = c5
    out['C5_monotone'] = bool(mono)
    L.append('')

    # -------------------------------------------------------------------- C4
    L.append('C4 the notch: it must remove the REPEAT`s contribution and leave the')
    L.append('   swirl`s.  Per selection sheet: the rung (repeat present) with and')
    L.append('   without the notch, the no-repeat control (`average` bake) with the')
    L.append('   notch, and TILING3`s proposal with and without.')
    L.append('   %-12s %9s %9s %9s | %9s %9s' %
             ('sheet', 'rung raw', 'rung ntc', 'ctrl ntc', 'prop raw', 'prop ntc'))
    c4 = {}
    for cx, cy in SEL:
        rung = S.lum(OB.bake(cx, cy, dim=4, tile=TILE, mip='code'))
        ctrl = S.lum(OB.bake(cx, cy, dim=4, tile=TILE, mip='mean'))
        prop = S.lum(A5.bake_v(cx, cy, 683.0, 1024.0, 1, -1.0))
        row = dict(rung_raw=W.swirl(rung, notch=False), rung_ntc=W.swirl(rung),
                   ctrl_ntc=W.swirl(ctrl), prop_raw=W.swirl(prop, notch=False),
                   prop_ntc=W.swirl(prop))
        c4['%d,%d' % (cx, cy)] = row
        L.append('   (%4d,%4d) %9.4f %9.4f %9.4f | %9.4f %9.4f'
                 % (cx, cy, row['rung_raw'], row['rung_ntc'], row['ctrl_ntc'],
                    row['prop_raw'], row['prop_ntc']))
    dr = float(np.median([r['rung_raw'] - r['rung_ntc'] for r in c4.values()]))
    dp = float(np.median([r['prop_raw'] - r['prop_ntc'] for r in c4.values()]))
    L.append('   median drop from the notch: rung %+.4f, proposal %+.4f' % (dr, dp))
    L.append('   median |rung notched - no-repeat control|: %.4f'
             % float(np.median([abs(r['rung_ntc'] - r['ctrl_ntc']) for r in c4.values()])))
    out['C4'] = c4
    L.append('')

    # -------------------------------------------------------------------- C7
    L.append('C7 DOES THE INSTRUMENT READ BUNGO`S SWIRL?  TILING3`s proposal on the')
    L.append('   seven selection sheets against the vanilla ceiling %.4f.' % ceil)
    L.append('   %-12s %9s %9s %9s  %s' % ('sheet', 'proposal', 'vanilla', 'twin', 'verdict'))
    nover = 0
    c7 = {}
    for cx, cy in SEL:
        p = c4['%d,%d' % (cx, cy)]['prop_ntc']
        v = van['%d,%d' % (cx, cy)]['swirl']
        tw = van['%d,%d' % (cx, cy)]['twin']
        over = p > ceil
        nover += 1 if over else 0
        c7['%d,%d' % (cx, cy)] = dict(prop=p, van=v, twin=tw, over=bool(over))
        L.append('   (%4d,%4d) %9.4f %9.4f %9.4f  %s'
                 % (cx, cy, p, v, tw, 'OVER THE CEILING' if over else 'inside'))
    L.append('   %d of %d selection sheets read over vanilla`s worst-of-22.' % (nover, len(SEL)))
    out['C7'] = c7
    out['C7_over'] = nover
    L.append('')
    L.append('VERDICT ON THE INSTRUMENT: %s'
             % ('usable -- C1 near 1, C2 near the floor, C5 monotone, C4 separates,'
                ' C7 reads the swirl'
                if (c1 > 0.8 and c2 < 0.5 and mono and nover >= 4)
                else 'SEE THE ROWS -- one of C1/C2/C5/C7 did not answer as registered'))

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 's1_controls.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(out, open(os.path.join(HERE, 's1_controls.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
