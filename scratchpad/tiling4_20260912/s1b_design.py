"""TILING4 step 1b -- REDESIGN of the swirl instrument, against the same known
answers, because the first design FAILED two of them.

What the first design got wrong (s1_controls.txt, 00:1x, kept on disk):
  C1 grating 1.0000 and C2 isotropic 0.2015 passed, but
  C5 a synthetic warp of RMS strain 0.733 moved the reading only 0.306 -> 0.385,
     inside the vanilla population's own spread (0.288 .. 0.633), and
  C7 TILING3's proposal read 0.391 where its own rung read 0.403 -- the
     instrument could not tell the warped sheet from the unwarped one.
An instrument that cannot see the defect bungo pointed at cannot be used to
grade a fix for it.  So it is redesigned here, BEFORE any candidate of this
lane is scored, and the reason is on the record.

THE DIAGNOSIS.  The first design band-passed to 9..33 texels "because the warp
lattice is 32 texels".  That is where the warp's own shape lives, but it is not
where its SIGNATURE lives.  A warp does not add features at its own lattice
scale; it STRETCHES the texture's existing grain, and the land texture's grain
is 1..4 texels.  A local stretch of factor (1+e) turns that grain from isotropic
into elongated, and the elongation direction is constant over a whole lattice
cell.  So the signature is: FINE detail, LARGE averaging window.  The first
design filtered the fine detail out and then measured the orientation of what
was left, which is the terrain's own shape.

THE SWEEP.  Box radii lo (the fine band's lower cut) and hi (its upper cut),
and the tensor window win.  Scored on the answers that must come out right:
  C1  grating           -> near 1.0
  C2  isotropic noise   -> near its own twin
  C5  synthetic warp    -> rises with the strain; the rise is reported in units
                           of the vanilla population's own SD, which is the
                           only honest way to say "big"
  C7  proposal vs rung  -> paired, per sheet, same chunk: must be positive and
                           large; and the proposal must clear vanilla's ceiling
The configuration is chosen on C5 and C7 only -- on the KNOWN defect, never on
any candidate of this lane, none of which exists yet.

    python s1b_design.py  ->  logs/s1b_design.txt
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
CACHE = os.path.join(HERE, 'cache')


def cached(tag, fn):
    os.makedirs(CACHE, exist_ok=True)
    p = os.path.join(CACHE, tag + '.npy')
    if os.path.exists(p):
        return np.load(p)
    a = fn()
    np.save(p, a)
    return a


def rung(cx, cy):
    return cached('rung_%d_%d' % (cx, cy),
                  lambda: S.lum(OB.bake(cx, cy, dim=4, tile=TILE, mip='code')))


def prop(cx, cy):
    return cached('prop_%d_%d' % (cx, cy),
                  lambda: S.lum(A5.bake_v(cx, cy, 683.0, 1024.0, 1, -1.0)))


def vanilla(cx, cy):
    return cached('van_%d_%d' % (cx, cy),
                  lambda: S.lum(S.Dds(S.van_sheet(cx, cy)).level(0)))


def main():
    cfg = json.load(open(os.path.join(HERE, 'pool.json')))
    SEL = [tuple(c) for c in cfg['selection']]
    all22 = [tuple(c) for c in json.load(open(os.path.join(T2, 'sheets.json')))['sheets']]

    L = ['TILING4 -- redesigning the swirl instrument against its known answers', '',
         'The first design (s1_controls.txt) passed C1/C2 and FAILED C5/C7: it could',
         'not tell TILING3`s warped sheet from its own unwarped rung.  Diagnosis: it',
         'band-passed away the 1..4 texel grain that a warp actually stretches.',
         'Swept here: the fine band (box radii lo..hi) and the tensor window.', '']

    base = vanilla(-20, 24)
    lat_tx = 1024.0 / 32.0
    c5amps = [0.0, 5.0, 10.0, 21.34]
    c5sheets = [base if a == 0 else W.apply_swirl(base, a, lat_tx) for a in c5amps]
    g = W.grating(512, 512, period=6.0, amp=8.0, angle=0.4)
    iso = W.isotropic(512, 512, lo=2.0, hi=8.0)

    rows = []
    L.append('%-16s %6s %6s | %7s %7s | %8s %8s %8s | %s'
             % ('lo..hi  win', 'C1', 'C2', 'vanSD', 'ceil', 'C5 rise', 'C7 pair', 'C7 sig',
                'prop over ceil'))
    for lo, hi in ((0, 2), (0, 4), (1, 4), (1, 6), (0, 8), (2, 8)):
        for win in (8, 16, 24, 32):
            kw = dict(lo=lo, hi=hi, win=win)
            c1 = W.swirl(g, notch=False, **kw)
            c2 = W.swirl(iso, notch=False, **kw)
            van = np.array([W.swirl(vanilla(*c), **kw) for c in all22])
            vsd = float(van.std())
            ceil = float(van.max())
            c5 = [W.swirl(s, **kw) for s in c5sheets]
            rise = (c5[-1] - c5[0]) / max(vsd, 1e-9)
            pr = np.array([W.swirl(prop(*c), **kw) for c in SEL])
            ru = np.array([W.swirl(rung(*c), **kw) for c in SEL])
            pair = float(np.median(pr - ru))
            sig = pair / max(vsd, 1e-9)
            nover = int((pr > ceil).sum())
            rows.append(dict(lo=lo, hi=hi, win=win, c1=c1, c2=c2, vsd=vsd, ceil=ceil,
                             rise=float(rise), pair=pair, sig=float(sig), nover=nover,
                             c5=[float(x) for x in c5],
                             prop=[float(x) for x in pr], rung=[float(x) for x in ru]))
            L.append('%-16s %6.3f %6.3f | %7.4f %7.4f | %8.2f %8.4f %8.2f | %d of 7'
                     % ('%d..%d  w%d' % (lo, hi, win), c1, c2, vsd, ceil,
                        rise, pair, sig, nover))

    L.append('')
    L.append('C5 rise  = (swirl at strain 0.733) - (swirl at strain 0) in vanilla SDs')
    L.append('C7 pair  = median over the seven selection sheets of (proposal - its own rung)')
    L.append('C7 sig   = that paired difference in vanilla SDs')
    L.append('')
    ok = [r for r in rows if r['c1'] > 0.85 and r['c2'] < 0.45]
    if not ok:
        L.append('REFUSED: no configuration kept C1 > 0.85 and C2 < 0.45.')
        best = None
    else:
        best = max(ok, key=lambda r: min(r['rise'], r['sig']))
        L.append('CHOSEN: lo=%d hi=%d win=%d -- it maximises the SMALLER of the two'
                 % (best['lo'], best['hi'], best['win']))
        L.append('   sensitivities (C5 rise %.2f SD, C7 paired %.2f SD), so it is not'
                 % (best['rise'], best['sig']))
        L.append('   chosen by one answer at the other`s expense.  C1 %.3f, C2 %.3f.'
                 % (best['c1'], best['c2']))
        L.append('   vanilla ceiling %.4f, population SD %.4f, proposal over the'
                 % (best['ceil'], best['vsd']))
        L.append('   ceiling on %d of 7 selection sheets.' % best['nover'])
        L.append('')
        L.append('   %-12s %9s %9s %9s' % ('sheet', 'rung', 'proposal', 'vanilla'))
        for i, c in enumerate(SEL):
            L.append('   (%4d,%4d) %9.4f %9.4f %9.4f'
                     % (c[0], c[1], best['rung'][i], best['prop'][i],
                        W.swirl(vanilla(*c), lo=best['lo'], hi=best['hi'], win=best['win'])))

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 's1b_design.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(dict(rows=rows, best=best), open(os.path.join(HERE, 's1b_design.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
