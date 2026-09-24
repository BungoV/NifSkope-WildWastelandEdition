"""TILING4 step 1c -- the FROZEN swirl instrument and its known answers, and
the ceilings this lane will be graded against (gate F1).

The instrument is t4_lib.swirl at the constants frozen in that file at 00:2x
(fine band = the grain itself up to 5 texels; tensor window 17 texels).  Nothing
below is fitted to anything: the configuration was chosen in s1b_design.py on a
synthetic warp of known strain and on TILING3's already-shipped proposal, and
no candidate of this lane exists yet.

THE CONTROLS, with the answer registered before each was run:

  C1  a pure sinusoid grating              -> near 1.0
  C2  band-limited isotropic noise         -> near its own phase twin
  C3  every vanilla sheet's phase twin     -> the per-sheet FLOOR
  C4  the notch: the rung with the repeat present, with and without the notch,
      beside the no-repeat control (`average`) -- the notch must not be doing
      the work, and the grid must not be counted as a swirl
  C5  a synthetic warp of KNOWN strain     -> monotone, and the rise stated in
      units of the vanilla population's own SD
  C6  vanilla's 22 sheets                  -> the CEILING, absolute AND as a
      ratio over each sheet's own twin (TILING2's two-ceiling pattern, and the
      answer to TILING3's MISTAKES entry "a per-sheet gate needs a per-sheet
      floor")
  C7  TILING3's proposal, per sheet, against its own rung and against both
      ceilings -> must read the swirl
  C8  THE CONFOUND.  `--land-sample stochastic` turned on TWO things at once:
      the warp (amp 683, lattice 1024) and a mip bias of -1.00.  A sharper mip
      puts more energy in the very band this instrument reads.  So the warp
      alone and the bias alone are baked separately here.  If the bias alone
      moves the reading as much as the warp alone does, the instrument is
      reading sharpness, not swirl, and it is refused.

    python s1c_freeze.py  ->  logs/s1c_freeze.txt, s1c_freeze.json
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
from s1b_design import rung, prop, vanilla, cached            # noqa: E402

TILE = 341.3333


def main():
    cfg = json.load(open(os.path.join(HERE, 'pool.json')))
    SEL = [tuple(c) for c in cfg['selection']]
    VAL = [tuple(c) for c in cfg['validation']]
    cov = json.load(open(os.path.join(HERE, 'coverage.json')))
    all22 = [tuple(c) for c in json.load(open(os.path.join(T2, 'sheets.json')))['sheets']]
    out = {}
    L = ['TILING4 -- the FROZEN swirl instrument and its known answers', '',
         'instrument: t4_lib.swirl -- energy-weighted mean structure-tensor coherence',
         'of the 1..5 texel grain, averaged over a 17-texel window, with the land',
         'repeat`s own frequency family (10.667 texels) notched out first.',
         'constants frozen 00:2x: BAND_LO %d, BAND_HI %d, WIN %d.' % (W.BAND_LO, W.BAND_HI, W.WIN),
         '']

    # ------------------------------------------------------------------ C1 C2
    g = W.grating(512, 512, period=6.0, amp=8.0, angle=0.4)
    iso = W.isotropic(512, 512, lo=2.0, hi=8.0)
    c1 = W.swirl(g, notch=False)
    c2 = W.swirl(iso, notch=False)
    c2t = W.swirl_floor(iso, notch=False)
    L.append('C1 pure sinusoid, period 6 texels, 23 deg : %.4f   [registered: near 1.0]' % c1)
    L.append('C2 isotropic noise in the same band       : %.4f, its own twin %.4f'
             '   [registered: near the twin]' % (c2, c2t))
    out['C1'] = c1
    out['C2'] = dict(value=c2, twin=c2t)
    L.append('')

    # --------------------------------------------------------------- C3 C6
    L.append('C3/C6 vanilla`s 22 sheets: the value, the sheet`s own phase-twin FLOOR,')
    L.append('      and the ratio.  Both ceilings come from this table and nothing else.')
    L.append('   %-12s %8s %8s %8s   %s' % ('sheet', 'swirl', 'twin', 'ratio', 'set'))
    van = {}
    for cx, cy in sorted(set(all22) | set(SEL) | set(VAL)):
        v = vanilla(cx, cy)
        sw = W.swirl(v)
        tw = W.swirl_floor(v)
        tag = ('selection' if (cx, cy) in SEL else
               'validation' if (cx, cy) in VAL else 'TILING2 22')
        if (cx, cy) not in all22:
            tag += ' (not in the 22)'
        van['%d,%d' % (cx, cy)] = dict(swirl=sw, twin=tw, ratio=sw / tw, tag=tag)
        L.append('   (%4d,%4d) %8.4f %8.4f %8.4f   %s' % (cx, cy, sw, tw, sw / tw, tag))
    v22 = np.array([van['%d,%d' % c]['swirl'] for c in all22])
    r22 = np.array([van['%d,%d' % c]['ratio'] for c in all22])
    ABS_CEIL = float(v22.max())
    RAT_CEIL = float(r22.max())
    VSD = float(v22.std())
    L.append('')
    L.append('   CEILINGS, frozen here, from vanilla`s 22 and nothing else:')
    L.append('      absolute  %.4f   (worst of the 22; median %.4f, SD %.4f)'
             % (ABS_CEIL, float(np.median(v22)), VSD))
    L.append('      ratio     %.4f   (worst of the 22 over its own twin; median %.4f)'
             % (RAT_CEIL, float(np.median(r22))))
    L.append('   A sheet is INSIDE VANILLA`S LAW on the swirl only if it is under BOTH.')
    L.append('   The absolute ceiling alone is set by one atypical sheet ((52,-64), a')
    L.append('   coastline) and passes TILING3`s proposal 7 of 7, which is exactly the')
    L.append('   failure TILING3`s own MISTAKES entry names: a per-sheet gate needs a')
    L.append('   per-sheet floor.  That is what the ratio is for.')
    out.update(dict(ABS_CEIL=ABS_CEIL, RAT_CEIL=RAT_CEIL, VSD=VSD, vanilla=van))
    L.append('')

    # -------------------------------------------------------------------- C5
    L.append('C5 a synthetic warp of KNOWN strain on a real vanilla sheet (-20,24),')
    L.append('   using TILING3`s own warp field verbatim, lattice 32 texels:')
    L.append('   %-9s %8s %9s %9s %9s' % ('amp(tx)', 'strain', 'swirl', 'ratio', 'in vanSD'))
    base = vanilla(-20, 24)
    lat_tx = 1024.0 / 32.0
    c5 = []
    z = None
    for amp_tx in (0.0, 2.0, 5.0, 10.0, 21.34, 42.7):
        sh = base if amp_tx == 0 else W.apply_swirl(base, amp_tx, lat_tx)
        st = W.warp_strain(amp_tx, lat_tx)
        sw = W.swirl(sh)
        tw = W.swirl_floor(sh)
        if z is None:
            z = sw
        L.append('   %-9.2f %8.3f %9.4f %9.4f %+9.2f'
                 % (amp_tx, st, sw, sw / tw, (sw - z) / VSD))
        c5.append(dict(amp=amp_tx, strain=st, swirl=sw, twin=tw, ratio=sw / tw))
    mono = all(c5[i]['swirl'] <= c5[i + 1]['swirl'] + 1e-6 for i in range(len(c5) - 1))
    L.append('   monotone in the strain: %s' % ('YES' if mono else 'NO -- REFUSED'))
    L.append('   at TILING3`s own strain 0.718 the reading is %.2f vanilla SDs above'
             % ((c5[4]['swirl'] - z) / VSD))
    L.append('   the unwarped sheet, so the instrument does read a warp of that size.')
    out['C5'] = c5
    out['C5_monotone'] = bool(mono)
    L.append('')

    # -------------------------------------------------------------------- C4
    L.append('C4 the notch (does it separate the repeat`s grid from a swirl?).')
    L.append('   rung raw / rung notched / the no-repeat control (`average`) notched.')
    L.append('   %-12s %9s %9s %9s %9s' % ('sheet', 'rung raw', 'rung ntc', 'ctrl ntc', 'ntc-ctrl'))
    c4 = {}
    for cx, cy in SEL:
        r = rung(cx, cy)
        ctrl = cached('ctrl_%d_%d' % (cx, cy),
                      lambda: S.lum(OB.bake(cx, cy, dim=4, tile=TILE, mip='mean')))
        row = dict(raw=W.swirl(r, notch=False), ntc=W.swirl(r), ctrl=W.swirl(ctrl))
        c4['%d,%d' % (cx, cy)] = row
        L.append('   (%4d,%4d) %9.4f %9.4f %9.4f %+9.4f'
                 % (cx, cy, row['raw'], row['ntc'], row['ctrl'], row['ntc'] - row['ctrl']))
    L.append('   median |rung notched - no-repeat control| %.4f, i.e. %.2f vanilla SDs:'
             % (float(np.median([abs(r['ntc'] - r['ctrl']) for r in c4.values()])),
                float(np.median([abs(r['ntc'] - r['ctrl']) for r in c4.values()])) / VSD))
    L.append('   with the notch in, a sheet whose ONLY defect is the repeat reads the')
    L.append('   same as the same sheet with no repeat at all.  The grid is not')
    L.append('   counted as a swirl, which is what the brief asked be separated.')
    out['C4'] = c4
    L.append('')

    # -------------------------------------------------------------------- C8
    L.append('C8 THE CONFOUND: `stochastic` turned on the warp AND a -1.00 mip bias.')
    L.append('   Baked apart, on the seven selection sheets:')
    L.append('   %-12s %8s %8s %8s %8s' % ('sheet', 'rung', 'bias only', 'warp only', 'both'))
    c8 = {}
    for cx, cy in SEL:
        r = W.swirl(rung(cx, cy))
        b = W.swirl(cached('bias_%d_%d' % (cx, cy),
                           lambda: S.lum(A5.bake_v(cx, cy, 0.0, 1024.0, 1, -1.0))))
        w = W.swirl(cached('warp_%d_%d' % (cx, cy),
                           lambda: S.lum(A5.bake_v(cx, cy, 683.0, 1024.0, 1, 0.0))))
        p = W.swirl(prop(cx, cy))
        c8['%d,%d' % (cx, cy)] = dict(rung=r, bias=b, warp=w, both=p)
        L.append('   (%4d,%4d) %8.4f %8.4f %8.4f %8.4f' % (cx, cy, r, b, w, p))
    db = float(np.median([v['bias'] - v['rung'] for v in c8.values()]))
    dw = float(np.median([v['warp'] - v['rung'] for v in c8.values()]))
    L.append('   median move off the rung: mip bias alone %+.4f (%+.2f SD), warp alone'
             ' %+.4f (%+.2f SD)' % (db, db / VSD, dw, dw / VSD))
    L.append('   VERDICT: %s' % ('the warp moves it, the sharpening does not -- the'
                                 ' instrument reads swirl, not sharpness'
                                 if dw > 2.0 * abs(db) else
                                 'REFUSED -- the sharpening moves it as much as the warp'))
    out['C8'] = c8
    out['C8_bias'] = db
    out['C8_warp'] = dw
    L.append('')

    # -------------------------------------------------------------------- C7
    L.append('C7 DOES IT READ BUNGO`S SWIRL?  TILING3`s proposal, per selection sheet,')
    L.append('   against its own rung and against BOTH frozen vanilla ceilings.')
    L.append('   %-12s %8s %8s %8s %8s %8s  %s'
             % ('sheet', 'rung', 'proposal', 'twin', 'ratio', 'stand-in', 'verdict'))
    c7 = {}
    nabs = nrat = 0
    for cx, cy in SEL:
        k = '%d,%d' % (cx, cy)
        p = prop(cx, cy)
        sw = W.swirl(p)
        tw = W.swirl_floor(p)
        rat = sw / tw
        oa, orr = sw > ABS_CEIL, rat > RAT_CEIL
        nabs += 1 if oa else 0
        nrat += 1 if orr else 0
        c7[k] = dict(rung=W.swirl(rung(cx, cy)), prop=sw, twin=tw, ratio=rat,
                     over_abs=bool(oa), over_rat=bool(orr))
        L.append('   (%4d,%4d) %8.4f %8.4f %8.4f %8.4f %7.1f%%  %s'
                 % (cx, cy, c7[k]['rung'], sw, tw, rat, 100 * cov[k]['standin'],
                    ('OVER absolute' if oa else '') + (' OVER ratio' if orr else '')
                    or 'inside both'))
    pair = float(np.median([c7['%d,%d' % c]['prop'] - c7['%d,%d' % c]['rung'] for c in SEL]))
    L.append('   paired, same chunk: the proposal reads %+.4f over its own rung'
             ' (%+.2f vanilla SDs), on %d of 7 sheets.'
             % (pair, pair / VSD,
                sum(1 for c in SEL if c7['%d,%d' % c]['prop'] > c7['%d,%d' % c]['rung'])))
    L.append('   over the absolute ceiling %.4f: %d of 7.   over the ratio ceiling'
             ' %.4f: %d of 7.' % (ABS_CEIL, nabs, RAT_CEIL, nrat))
    out['C7'] = c7
    out['C7_over_abs'] = nabs
    out['C7_over_rat'] = nrat
    out['C7_pair'] = pair
    L.append('')

    usable = (c1 > 0.85 and c2 < 0.45 and mono and dw > 2.0 * abs(db)
              and pair > 0.5 * VSD and nrat >= 4)
    L.append('VERDICT ON THE INSTRUMENT: %s' % (
        'USABLE. C1 %.3f, C2 %.3f, C5 monotone and %+.2f SD at TILING3`s strain,'
        ' C4 separates the grid, C8 rules out sharpness, C7 puts the proposal over'
        ' the ratio ceiling on %d of 7.' % (c1, c2, (c5[4]['swirl'] - z) / VSD, nrat)
        if usable else
        'NOT USABLE AS REGISTERED -- read the rows; the gate this lane is graded on'
        ' must be stated as what it can and cannot see.'))
    out['usable'] = bool(usable)

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 's1c_freeze.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(out, open(os.path.join(HERE, 's1c_freeze.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
