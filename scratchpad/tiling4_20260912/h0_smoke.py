"""TILING4 -- a smoke test of the two candidate samplers on ONE sheet, before
any sweep: does the bake come out at all, is it not flat, and do the three
instruments move in the direction the construction predicts?

Registered before running: H1 and H2 should cut the repeat hard (the offsets are
a full random phase within the repeat, so the periodic term cannot survive), keep
the grain near the rung's (the blend is variance-preserving), and leave the swirl
at or below the rung's (no strain anywhere).  If the swirl comes out ABOVE the
rung's, the blend itself is making orientation structure and the idea is in
trouble -- which is worth knowing in one bake rather than after a sweep.

    python h0_smoke.py  ->  logs/h0_smoke.txt
"""
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
import t4_lib as W                                            # noqa: E402
import h_cand as H                                            # noqa: E402
import s1e_law as LAW                                         # noqa: E402
from s1b_design import rung, prop, vanilla                    # noqa: E402

CX, CY = -20, 24
ABS_CEIL = 0.264       # TILING2's absolute repeat ceiling
RAT_CEIL = 0.448       # TILING2's ratio ceiling


def row(name, sh, secs=None):
    sh = np.asarray(sh, np.float64)
    L_ = S.lum(sh) if sh.ndim == 3 else sh
    vis, fl = T.tiling_visibility(L_)
    r, sw, tw = LAW.ratio(L_)
    return dict(name=name, sd=float(L_.std()), vis=vis, floor=fl,
                ratio=vis / max(fl, 1e-9), hpsd=float(T.hp_residual(L_, r=2).std()),
                lv=float(S.local_var(L_).mean()), swirl=sw, swirl_r=r, secs=secs)


def main():
    van = vanilla(CX, CY)
    rows = [row('vanilla sheet', van), row('rung (footprint)', rung(CX, CY)),
            row('TILING3 proposal', prop(CX, CY))]
    cands = [
        ('H1 hex size=341', H.make_tap_h1(341.3333)),
        ('H1 hex size=683', H.make_tap_h1(682.6667)),
        ('H1 hex size=1024', H.make_tap_h1(1024.0)),
        ('H1 hex 683 no varnorm', H.make_tap_h1(682.6667, varnorm=False)),
        ('H2 cell=341 b=1 rot none', H.make_tap_h2(341.3333, 1.0, 'none')),
        ('H2 cell=683 b=1 rot none', H.make_tap_h2(682.6667, 1.0, 'none')),
        ('H2 cell=683 b=.25 rot none', H.make_tap_h2(682.6667, 0.25, 'none')),
        ('H2 cell=683 b=1 rot quad', H.make_tap_h2(682.6667, 1.0, 'quad')),
        ('H2 cell=683 b=1 rot free', H.make_tap_h2(682.6667, 1.0, 'free')),
    ]
    for name, tap in cands:
        t0 = time.time()
        sh = H.bake(CX, CY, tap)
        rows.append(row(name, sh, time.time() - t0))

    ceil = min(LAW.MARGIN * LAW.van_ratio(CX, CY), LAW.ABS_CEIL)
    L = ['TILING4 -- smoke test of the candidate samplers, chunk (%d,%d)' % (CX, CY), '',
         'repeat: amplitude at 10.667 texels, 8-bit luminance; TILING2 ceilings',
         '        %.3f absolute and %.3f over the sheet`s own null floor.'
         % (ABS_CEIL, RAT_CEIL),
         'grain : hp SD (everything finer than 5 texels) and mean local variance;',
         '        vanilla`s values are the target, not zero.',
         'swirl : this lane`s frozen law; this sheet`s ceiling is %.4f.' % ceil, '',
         '%-28s %6s %7s %7s %7s | %6s %7s | %7s %7s %s'
         % ('sampler', 'SD', 'repeat', 'floor', 'ratio', 'grain', 'localvar',
            'swirl', 'ratio', 'verdict')]
    for d in rows:
        v = []
        if d['name'].startswith(('H1', 'H2', 'TILING3')):
            v.append('repeat ok' if (d['ratio'] <= RAT_CEIL and d['vis'] <= ABS_CEIL)
                     else 'REPEAT RED')
            v.append('swirl ok' if d['swirl_r'] <= ceil else 'SWIRL RED')
        L.append('%-28s %6.2f %7.3f %7.3f %7.3f | %6.3f %7.2f | %7.4f %7.4f %s'
                 % (d['name'], d['sd'], d['vis'], d['floor'], d['ratio'],
                    d['hpsd'], d['lv'], d['swirl'], d['swirl_r'], ', '.join(v)))
    L.append('')
    L.append('bake seconds per sheet: ' + ', '.join(
        '%s %.1f' % (d['name'].split()[0] + d['name'].split()[1], d['secs'])
        for d in rows if d['secs']))
    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'h0_smoke.txt'), 'w', newline='\n') as f:
        f.write(txt)


if __name__ == '__main__':
    main()
