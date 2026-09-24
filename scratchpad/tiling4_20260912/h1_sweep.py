"""TILING4 step 2 -- H1 and H2 on the SELECTION seven, every gate at once.

Two stages, the way TILING3 did it, because the sampler's geometry and the mip
bias answer different questions and sweeping them together hides which one is
responsible:

  STAGE 1  the geometry, at a fixed mip bias of -1.00 (TILING3's, so the grain is
           in the right neighbourhood while the geometry is judged).  Graded on
           the repeat and on the swirl.
  STAGE 2  the mip bias, on the geometries that came out of stage 1 green.
           Graded on the grain, the band table and the local variance, with the
           repeat and the swirl re-checked so a grain fixed by losing the repeat
           counts as a loss (gate F3).

EVERY GATE IS PER SHEET AND EVERY FLOOR IS THAT SHEET'S OWN:

  repeat   TILING2's law, with TILING3's control-aware rule: the ratio over the
           sheet's own null floor must be <= 0.448, and the absolute amplitude
           must be <= 0.264 -- or, where the sheet's own no-repeat control
           already reads above 0.264, <= that control (a sheet whose broadband
           energy lands in the bin cannot be asked to beat a number its own
           repeat-free bake cannot beat either).
  grain    the SD of everything finer than 5 texels, within 20 % of the SAME
           CHUNK's vanilla sheet.  Vanilla's value is the target, not zero.
  band tab all six radial bands, each share within 20 % of the same chunk's
           vanilla share.
  swirl    this lane's frozen law (s1e_law.py): r <= 1.20 x the same chunk's
           vanilla r, and r <= 3.2846.
  localvar reported beside, not gated -- it is the same quantity as the grain
           seen at 3x3, and gating both would double-count one fact.

    python h1_sweep.py  ->  logs/h1_sweep.txt, h1_sweep.json
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
import offline_bake as OB                                     # noqa: E402
import a5_tune as A5                                          # noqa: E402
import h_cand as H                                            # noqa: E402
import s1e_law as LAW                                         # noqa: E402
from s1b_design import rung, vanilla, cached                  # noqa: E402

TILE = 341.3333
ABS_CEIL = 0.264          # TILING2's absolute repeat ceiling, worst of 22
RAT_CEIL = 0.448          # TILING2's ratio ceiling
MARG = 0.20               # the brief's margin for grain and band table
FLAT_SD = 1.0             # TILING3's refuse_flat


def geoms():
    """Stage 1's variants.  Sizes are in world units; one land repeat is 341.3333."""
    g = []
    for s in (256.0, 341.3333, 512.0, 682.6667):
        g.append(('H1 hex %.0f' % s, lambda b, s=s: H.make_tap_h1(s, b)))
    g.append(('H1 hex 341 no-varnorm',
              lambda b: H.make_tap_h1(341.3333, b, varnorm=False)))
    for c in (341.3333, 682.6667):
        for r in ('none', 'quad', 'free'):
            g.append(('H2 %.0f b1.0 %s' % (c, r),
                      lambda b, c=c, r=r: H.make_tap_h2(c, 1.0, r, b)))
    g.append(('H2 341 b0.25 none', lambda b: H.make_tap_h2(341.3333, 0.25, 'none', b)))
    g.append(('H2 341 b0.50 none', lambda b: H.make_tap_h2(341.3333, 0.50, 'none', b)))
    return g


def refuse_flat(L, where):
    sd = float(np.asarray(L, np.float64).std())
    if sd < FLAT_SD:
        raise RuntimeError('REFUSED: %s baked flat (SD %.4f < %.2f)' % (where, sd, FLAT_SD))
    return L


def ctrl_vis(cx, cy):
    """The sheet's own no-repeat control: the same composite at the texture's
    1x1 mip, so no periodic term of the land texture can reach the sheet."""
    sh = cached('ctrl_%d_%d' % (cx, cy),
                lambda: S.lum(OB.bake(cx, cy, dim=4, tile=TILE, mip='mean')))
    return T.tiling_visibility(refuse_flat(sh, 'control (%d,%d)' % (cx, cy)))[0]


_REF = {}


def ref(cx, cy):
    """The same chunk's vanilla sheet: the target for the grain and the bands."""
    k = (cx, cy)
    if k not in _REF:
        v = vanilla(cx, cy)          # s1b_design returns luminance already
        sh, names = A5.bandtab(v)
        _REF[k] = dict(hpsd=float(T.hp_residual(v, r=2).std()),
                       lv=float(S.local_var(v).mean()), bands=sh, names=names,
                       ctrl=ctrl_vis(cx, cy), van_r=LAW.van_ratio(cx, cy))
    return _REF[k]


def score(cx, cy, L):
    """Every instrument on one baked sheet, each beside its own floor/ceiling."""
    lum = S.lum(L) if np.asarray(L).ndim == 3 else np.asarray(L, np.float64)
    refuse_flat(lum, 'bake (%d,%d)' % (cx, cy))
    R = ref(cx, cy)
    vis, fl = T.tiling_visibility(lum)
    rat = vis / max(fl, 1e-9)
    abs_ok = vis <= (ABS_CEIL if R['ctrl'] < ABS_CEIL else R['ctrl'])
    rep_ok = bool(abs_ok and rat <= RAT_CEIL)
    hp = float(T.hp_residual(lum, r=2).std())
    grain_err = hp / R['hpsd'] - 1.0
    bands, _ = A5.bandtab(lum)
    berr = [b / max(v, 1e-12) - 1.0 for b, v in zip(bands, R['bands'])]
    sw_ok, r, ceil, over = LAW.verdict(lum, cx, cy)
    return dict(vis=vis, floor=fl, ratio=rat, rep_ok=rep_ok,
                hpsd=hp, grain_err=grain_err, grain_ok=bool(abs(grain_err) <= MARG),
                bands=bands, band_err=berr,
                band_ok=bool(max(abs(e) for e in berr) <= MARG),
                band_worst=int(np.argmax([abs(e) for e in berr])),
                swirl_r=r, swirl_ceil=ceil, swirl_ok=bool(sw_ok), swirl_over=over,
                lv=float(S.local_var(lum).mean()), lv_ref=R['lv'],
                all_ok=bool(rep_ok and abs(grain_err) <= MARG and sw_ok
                            and max(abs(e) for e in berr) <= MARG))


def run(name, mk, bias, SHEETS, L, tag):
    per = {}
    t0 = time.time()
    for cx, cy in SHEETS:
        per['%d,%d' % (cx, cy)] = score(cx, cy, H.bake(cx, cy, mk(bias)))
    n = sum(1 for v in per.values() if v['all_ok'])
    row = dict(name=name, bias=bias, per=per, gated=n, secs=time.time() - t0,
               rep=sum(1 for v in per.values() if v['rep_ok']),
               grain=sum(1 for v in per.values() if v['grain_ok']),
               band=sum(1 for v in per.values() if v['band_ok']),
               swirl=sum(1 for v in per.values() if v['swirl_ok']))
    L.append('   %-24s %5.2f | %4s %4s %4s %4s | %5s  %7.3f %7.3f %+7.1f%% %7.4f'
             % (name, bias, '%d/7' % row['rep'], '%d/7' % row['grain'],
                '%d/7' % row['swirl'], '%d/7' % row['band'], '%d/7' % n,
                max(v['vis'] for v in per.values()),
                max(v['ratio'] for v in per.values()),
                100 * max(v['grain_err'] for v in per.values()),
                max(v['swirl_r'] for v in per.values())))
    return row


def main():
    cfg = json.load(open(os.path.join(HERE, 'pool.json')))
    SEL = [tuple(c) for c in cfg['selection']]
    L = ['TILING4 -- H1 and H2 on the selection seven', '',
         'ceilings: repeat %.3f absolute (or the sheet`s own no-repeat control where'
         % ABS_CEIL,
         '          that is higher) and %.3f over the sheet`s own null floor;' % RAT_CEIL,
         '          grain and every one of the six bands within %d %% of the SAME'
         % int(100 * MARG),
         '          CHUNK`s vanilla sheet; swirl by this lane`s frozen law.',
         'the columns after the gate counts are the WORST sheet of the seven.', '']

    L.append('THE RUNG AND TILING3`S PROPOSAL, scored the same way, for reference:')
    L.append('   %-24s %5s | %4s %4s %4s %4s | %5s  %7s %7s %8s %7s'
             % ('variant', 'bias', 'rep', 'grn', 'swl', 'bnd', 'ALL',
                'repMax', 'ratMax', 'grainMax', 'swirlMx'))
    per = {}
    for cx, cy in SEL:
        per['%d,%d' % (cx, cy)] = score(cx, cy, rung(cx, cy))
    L.append('   %-24s %5.2f | %4s %4s %4s %4s | %5s  %7.3f %7.3f %+7.1f%% %7.4f'
             % ('rung (footprint)', 0.0,
                '%d/7' % sum(1 for v in per.values() if v['rep_ok']),
                '%d/7' % sum(1 for v in per.values() if v['grain_ok']),
                '%d/7' % sum(1 for v in per.values() if v['swirl_ok']),
                '%d/7' % sum(1 for v in per.values() if v['band_ok']),
                '%d/7' % sum(1 for v in per.values() if v['all_ok']),
                max(v['vis'] for v in per.values()), max(v['ratio'] for v in per.values()),
                100 * max(v['grain_err'] for v in per.values()),
                max(v['swirl_r'] for v in per.values())))
    rungrow = dict(name='rung', per=per)
    pper = {}
    for cx, cy in SEL:
        pper['%d,%d' % (cx, cy)] = score(cx, cy, A5.bake_v(cx, cy, 683.0, 1024.0, 1, -1.0))
    L.append('   %-24s %5.2f | %4s %4s %4s %4s | %5s  %7.3f %7.3f %+7.1f%% %7.4f'
             % ('TILING3 proposal', -1.0,
                '%d/7' % sum(1 for v in pper.values() if v['rep_ok']),
                '%d/7' % sum(1 for v in pper.values() if v['grain_ok']),
                '%d/7' % sum(1 for v in pper.values() if v['swirl_ok']),
                '%d/7' % sum(1 for v in pper.values() if v['band_ok']),
                '%d/7' % sum(1 for v in pper.values() if v['all_ok']),
                max(v['vis'] for v in pper.values()), max(v['ratio'] for v in pper.values()),
                100 * max(v['grain_err'] for v in pper.values()),
                max(v['swirl_r'] for v in pper.values())))
    proprow = dict(name='TILING3 proposal', per=pper)
    L.append('')

    # ----------------------------------------------------------------- stage 1
    L.append('STAGE 1 -- the geometry, at a fixed mip bias of -1.00')
    L.append('   %-24s %5s | %4s %4s %4s %4s | %5s  %7s %7s %8s %7s'
             % ('variant', 'bias', 'rep', 'grn', 'swl', 'bnd', 'ALL',
                'repMax', 'ratMax', 'grainMax', 'swirlMx'))
    s1 = []
    for name, mk in geoms():
        s1.append(run(name, mk, -1.0, SEL, L, 's1'))
    L.append('')

    # ----------------------------------------------------------------- stage 2
    keep = sorted(s1, key=lambda r: (-r['rep'] - r['swirl'], r['name']))[:3]
    L.append('STAGE 2 -- the mip bias, on the three geometries with the best repeat +')
    L.append('           swirl out of stage 1: %s' % ', '.join(r['name'] for r in keep))
    L.append('   %-24s %5s | %4s %4s %4s %4s | %5s  %7s %7s %8s %7s'
             % ('variant', 'bias', 'rep', 'grn', 'swl', 'bnd', 'ALL',
                'repMax', 'ratMax', 'grainMax', 'swirlMx'))
    mkof = dict(geoms())
    s2 = []
    for r in keep:
        for b in (0.0, -0.5, -0.75, -1.0, -1.25):
            if b == -1.0:
                s2.append(r)
                L.append('   %-24s %5.2f | %4s %4s %4s %4s | %5s  (from stage 1)'
                         % (r['name'], b, '%d/7' % r['rep'], '%d/7' % r['grain'],
                            '%d/7' % r['swirl'], '%d/7' % r['band'], '%d/7' % r['gated']))
                continue
            s2.append(run(r['name'], mkof[r['name']], b, SEL, L, 's2'))
    L.append('')

    green = [r for r in s2 if r['gated'] == 7]
    if green:
        win = min(green, key=lambda r: (max(v['swirl_r'] for v in r['per'].values()),
                                        max(v['vis'] for v in r['per'].values())))
        L.append('SELECTION WINNER: %s at mip bias %.2f -- 7 of 7 on all four gates.'
                 % (win['name'], win['bias']))
        L.append('Chosen among the %d variants that gated 7 of 7 by the lowest worst-sheet'
                 % len(green))
        L.append('swirl, then the lowest worst-sheet repeat.  The rule was fixed before')
        L.append('the table was read and it does not turn on any single sheet.')
    else:
        best = max(s2, key=lambda r: r['gated'])
        win = best
        L.append('NOTHING GATED 7 OF 7 ON THE SELECTION SEVEN.  Best: %s at bias %.2f,'
                 % (best['name'], best['bias']))
        L.append('%d of 7.  H3 (the warp capped at strain 0.5 plus H1/H2) is the brief`s'
                 % best['gated'])
        L.append('next rung and it is what happens next.')
    L.append('')
    L.append('THE WINNER, SHEET BY SHEET (every number beside its own ceiling):')
    L.append('   %-12s %7s %7s %7s | %7s %7s | %7s %7s | %6s %s'
             % ('sheet', 'repeat', 'ceil', 'ratio', 'grain', 'vanilla', 'swirl',
                'ceil', 'bands', 'verdict'))
    for cx, cy in SEL:
        v = win['per']['%d,%d' % (cx, cy)]
        R = ref(cx, cy)
        L.append('   (%4d,%4d) %7.3f %7.3f %7.3f | %7.3f %7.3f | %7.4f %7.4f | %5.1f%% %s'
                 % (cx, cy, v['vis'], max(ABS_CEIL, R['ctrl']), v['ratio'],
                    v['hpsd'], R['hpsd'], v['swirl_r'], v['swirl_ceil'],
                    100 * max(abs(e) for e in v['band_err']),
                    'PASS' if v['all_ok'] else 'RED: ' + ' '.join(
                        k for k, ok in (('repeat', v['rep_ok']), ('grain', v['grain_ok']),
                                        ('swirl', v['swirl_ok']), ('bands', v['band_ok']))
                        if not ok)))

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'h1_sweep.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(dict(stage1=s1, stage2=s2, winner=win, rung=rungrow, proposal=proprow,
                   abs_ceil=ABS_CEIL, rat_ceil=RAT_CEIL, margin=MARG),
              open(os.path.join(HERE, 'h1_sweep.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
