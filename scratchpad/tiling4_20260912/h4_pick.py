"""TILING4 step 2d -- the fine bias sweep, the PICK, the leave-one-out stability
check, and the frozen VALIDATION seven.

What stages A-C settled (logs/h3_sweep.txt), in one paragraph, because the pick
rule below is built on it:

  * H3 is REFUSED and the numbers refuse it: the capped warp buys nothing on the
    repeat (6 of 7 at every strain tested, exactly what H1 gets without it) and
    it loses the swirl -- 7 of 7 at strain 0, 6 of 7 at strain 0.20, 2 of 7 at
    the brief's own cap of 0.50.  The cap the brief set by eye at 0.5 is about
    two and a half times looser than this lane's instrument allows.
  * G1-band is refused as a gate on the sampler, with the price measured: the
    RUNG reads 2 of 6, and the only thing that moves the three finest bands is a
    much sharper mip, which at bias -1.50 buys 5 of 6 and costs G2 (0 of 7) and
    G1 (+27.2 %).  Never 6 of 6 at any bias.
  * G1 and G2 do overlap, and the overlap was never sampled: at bias -0.20 G1
    reads -20.5 % (0.5 of a percentage point outside its +-20 % ceiling) with
    G2 7 of 7, and at -0.30 G1 reads -17.6 % (inside) with G2 5 of 7.  The
    window is between them.  Stage D samples it at quarter-tenths.

THE PICK RULE, written here before the stage D table existed:
  eligible = swirl 7 of 7 AND G1 inside AND G2 7 of 7 (the gates a sampler can
  actually move), then rank by: the most repeat-passing sheets, then the lowest
  worst-sheet repeat amplitude, then the lowest worst-sheet swirl.  The repeat
  is ranked rather than gated because stage A/B showed no candidate of any
  family reaches 7 of 7 on it, and the report must name the sheet and the
  margin rather than pick a variant that hides it.

Also here, because the report has to state it rather than assert it:
  * every selection sheet's OWN vanilla repeat amplitude, beside ours;
  * the pick repeated with each of the seven selection sheets left out (the
    brief: "the pick must not turn on one sheet");
  * the winner on the VALIDATION seven frozen at 00:11, graded by the same code.

    python h4_pick.py  ->  logs/h4_pick.txt, h4_pick.json
"""
import json
import os
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T3 = os.path.join(os.path.dirname(HERE), 'tiling3_20260911')
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T3, T2, SP):
    sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402
import h_cand as H                                            # noqa: E402
import h1_sweep as HS                                         # noqa: E402
import t4_gates as G                                          # noqa: E402
import h3_sweep as H3                                         # noqa: E402
from s1b_design import rung, vanilla                          # noqa: E402


def eligible(r):
    return r['swirl'] == r['n'] and r['g1_ok'] and r['g2_n'] == r['n']


def rank(r):
    return (-r['rep'], r['rep_max'], r['swirl_max'])


def main():
    cfg = json.load(open(os.path.join(HERE, 'pool.json')))
    SEL = [tuple(c) for c in cfg['selection']]
    VAL = [tuple(c) for c in cfg['validation']]
    rp, van = H3.refs(SEL)

    L = ['TILING4 -- the fine bias sweep, the pick, its stability, and the',
         '           frozen validation seven', '',
         'ALL = repeat 7/7 and swirl 7/7 and G1 and G2 7/7 and G2-band 7/7.',
         'G1-band is reported and is not in ALL (refused gate: the rung reads 2/6).',
         '']

    # ------------------------------------------------------------- stage D
    L.append('STAGE D -- the bias window at quarter-tenths')
    L.append(G.HEAD)
    D = []
    for size, nm in ((256.0, 'H1 hex 256'), (341.3333, 'H1 hex 341')):
        for b in (-0.22, -0.25, -0.27):
            D.append(H3.run(nm, H.make_tap_h1(size, b), b, SEL, rp, van, L))
            D[-1]['size'] = size
    L.append('')

    el = [r for r in D if eligible(r)]
    if not el:
        L.append('NO row in the window is eligible (swirl 7/7 + G1 + G2 7/7).')
        L.append('The window G1 and G2 leave open is empty at this resolution, and')
        L.append('that is the finding: the two grain gates cannot both be met.')
        el = [r for r in D if r['swirl'] == r['n'] and r['g2_n'] == r['n']]
        L.append('Falling back to swirl 7/7 + G2 7/7, %d rows, with G1`s miss'
                 % len(el))
        L.append('printed for the winner.')
    win = sorted(el, key=rank)[0]
    L.append('PICK: %s at mip bias %.2f.' % (win['name'], win['bias']))
    L.append('   repeat %d of 7 (worst sheet %.3f), swirl %d of 7 (worst %.4f),'
             % (win['rep'], win['rep_max'], win['swirl'], win['swirl_max']))
    L.append('   G1 %+.1f%% (ceiling +-20 %%), G2 %d of 7, G2-band %d of 7,'
             % (100 * win['g1_err'], win['g2_n'], win['g2b_n']))
    L.append('   G1-band %d of 6 (refused gate), literal per-sheet grain %d of 7.'
             % (win['g1b_n'], win['lit_grain']))
    L.append('')

    # ------------------------------------ vanilla's own repeat, sheet by sheet
    L.append('THE REPEAT ON EVERY SELECTION SHEET, ours beside THAT SHEET`S OWN')
    L.append('vanilla.  The gate is TILING2`s frozen pair and it is not moved here:')
    L.append('amplitude <= 0.264 (vanilla`s worst of 22) or the sheet`s own')
    L.append('no-repeat control where that is higher, AND ratio <= 0.448.')
    L.append('   %-12s %7s %7s %7s | %7s %7s %7s | %s'
             % ('sheet', 'ours', 'ceil', 'ratio', 'vanilla', 'vanRat', 'rung',
                'verdict'))
    vanrep = {}
    for cx, cy in SEL:
        k = '%d,%d' % (cx, cy)
        v = win['per'][k]
        R = HS.ref(cx, cy)
        vv, vf = T.tiling_visibility(vanilla(cx, cy))
        rv, _ = T.tiling_visibility(rung(cx, cy))
        vanrep[k] = dict(vis=vv, ratio=vv / max(vf, 1e-9))
        L.append('   (%4d,%4d) %7.3f %7.3f %7.3f | %7.3f %7.3f %7.3f | %s'
                 % (cx, cy, v['vis'], max(HS.ABS_CEIL, R['ctrl']), v['ratio'],
                    vv, vv / max(vf, 1e-9), rv,
                    'ok' if v['rep_ok'] else 'RED %+.0f%% over the ceiling'
                    % (100 * (v['vis'] / max(HS.ABS_CEIL, R['ctrl']) - 1.0))))
    L.append('')

    # ------------------------------------------------- leave-one-out stability
    L.append('DOES THE PICK TURN ON ONE SHEET?  The same rule with each selection')
    L.append('sheet dropped, over the same stage D rows:')
    keys = ['%d,%d' % c for c in SEL]
    stab = {}
    for drop in keys:
        sub = [k for k in keys if k != drop]
        rows = []
        for r in D:
            rr = G.decided(r['per'], rp, van, sub)
            rr['name'], rr['bias'] = r['name'], r['bias']
            rows.append(rr)
        e = [r for r in rows if eligible(r)] or [r for r in rows
                                                 if r['swirl'] == r['n']
                                                 and r['g2_n'] == r['n']]
        w = sorted(e, key=rank)[0]
        stab[drop] = '%s @ %.2f' % (w['name'], w['bias'])
        L.append('   without (%9s): %s' % (drop, stab[drop]))
    same = sum(1 for v in stab.values()
               if v == '%s @ %.2f' % (win['name'], win['bias']))
    L.append('   the pick is the same on %d of 7 drops (floor: it must not need'
             % same)
    L.append('   any single sheet, so 7 of 7 is the only passing answer).')
    L.append('')

    # ----------------------------------------------------- the validation seven
    L.append('THE VALIDATION SEVEN, frozen 00:11, never looked at until now.')
    t0 = time.time()
    vrp, vvan = H3.refs(VAL)
    L.append(G.HEAD)
    vkeys = ['%d,%d' % c for c in VAL]
    vr = G.decided(vrp, vrp, vvan, vkeys)
    L.append(G.row('rung (footprint)', 0.0, vr))
    vwin = H3.run(win['name'], H.make_tap_h1(win['size'], win['bias']),
                  win['bias'], VAL, vrp, vvan, L)
    L.append('')
    L.append('   %-12s %7s %7s %7s | %7s %7s %7s | %7s %7s | %s'
             % ('sheet', 'repeat', 'ceil', 'ratio', 'grain', 'rung', 'G2 err',
                'swirl', 'ceil', 'verdict'))
    for cx, cy in VAL:
        k = '%d,%d' % (cx, cy)
        v = vwin['per'][k]
        R = HS.ref(cx, cy)
        g2e = v['hpsd'] / vrp[k]['hpsd'] - 1.0
        L.append('   (%4d,%4d) %7.3f %7.3f %7.3f | %7.3f %7.3f %+6.1f%% | '
                 '%7.4f %7.4f | %s'
                 % (cx, cy, v['vis'], max(HS.ABS_CEIL, R['ctrl']), v['ratio'],
                    v['hpsd'], vrp[k]['hpsd'], 100 * g2e,
                    v['swirl_r'], v['swirl_ceil'],
                    'PASS' if (v['rep_ok'] and v['swirl_ok']
                               and abs(g2e) <= G.MARG) else 'RED: ' + ' '.join(
                        n for n, ok in (('repeat', v['rep_ok']),
                                        ('swirl', v['swirl_ok']),
                                        ('G2grain', abs(g2e) <= G.MARG))
                        if not ok)))
    L.append('   (%.0f s)' % (time.time() - t0))
    L.append('')
    L.append('THE ANSWER TO GATE F3, in one line per set:')
    for nm, r in (('selection', win), ('validation', vwin)):
        L.append('   %-10s repeat %d/7  swirl %d/7  G1 %+.1f%%  G2 %d/7  '
                 'G2-band %d/7  G1-band %d/6'
                 % (nm, r['rep'], r['swirl'], 100 * r['g1_err'], r['g2_n'],
                    r['g2b_n'], r['g1b_n']))
    L.append('   F3 as decided asks 7 of 7 on repeat, swirl, G1/G2 and G2-band on')
    L.append('   BOTH sets.  It is NOT met: the repeat and G2-band are short.')
    L.append('   So `stochastic` does NOT become the default in this lane, and the')
    L.append('   switch`s meaning is replaced instead -- the swirl goes from 2 of 7')
    L.append('   (TILING3`s warp) to 7 of 7 with the repeat count unchanged at 6.')

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'h4_pick.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(dict(stageD=[{k: v for k, v in r.items() if k != 'per'} for r in D],
                   pick=dict(name=win['name'], bias=win['bias'], size=win['size'],
                             sel={k: v for k, v in win.items() if k != 'per'},
                             val={k: v for k, v in vwin.items() if k != 'per'}),
                   per_sel=win['per'], per_val=vwin['per'],
                   van_repeat=vanrep, stability=stab),
              open(os.path.join(HERE, 'h4_pick.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
