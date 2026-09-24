"""TILING4 step 1d -- the READOUT: how the coherence field is summarised into
one number, chosen on the known defect, and the C4 notch control done properly.

WHY THIS STEP EXISTS.  s1c_freeze.py fixed the band and the window and proved
(C8) that the statistic responds to the warp and not to the mip sharpening that
rode with it: warp alone moves the reading +3.19 vanilla SDs, the -1.00 mip bias
alone -1.29.  But the SUMMARY was an energy-weighted MEAN over the whole 512
sheet, and on that summary TILING3's proposal lands inside vanilla's population
on 7 of 7 sheets -- while the swirls are plainly visible in the PROPOSAL panel of
cmp_tiling3.png.  Looking at that panel says why: the smears are long curved
strokes filling PART of the sheet, where the warp's gradient is large, and the
mean over the whole sheet dilutes them with the parts the warp barely touched.
The eye does not average a picture; it finds the worst patch in it.

So the summary is swept here -- mean, three quantiles of the coherence over the
texels that carry the energy, and the area fraction over a fixed coherence --
and scored on the same known answers as before and on nothing else:

  the warp-only bake (the defect, isolated from the sharpening) must clear
  vanilla's ceiling, and the rung must not.  That is the whole requirement:
  an instrument that calls the rung guilty is useless, and one that calls the
  warp innocent is the instrument this lane already threw away once.

C4 IS ALSO REDONE HERE.  s1c used the `average` bake as the no-repeat control.
That is a degenerate reference for THIS statistic: `average` replaces every land
texture by one flat colour, so the sheet's fine band is nothing but alpha-blend
edges and VCLR ramps, which are few and highly oriented -- it read 0.28..0.70,
far ABOVE the rung.  The proper control is a synthetic one: inject a repeat of
known amplitude into a real sheet and check that the notched reading does not
move.  That is a known answer; the `average` bake was an assumption.

    python s1d_readout.py  ->  logs/s1d_readout.txt, s1d_readout.json
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
from s1b_design import rung, prop, vanilla, cached            # noqa: E402


def field(L, notch=True):
    """The coherence field and its energy, at the frozen band and window."""
    _, m = W.swirl(L, notch=notch, full=True)
    return m['coh'], m['energy'], m['mask']


def readouts(L, notch=True):
    coh, en, msk = field(L, notch)
    c = coh[msk]
    e = en[msk]
    keep = e >= np.median(e)               # the texels that carry the grain
    cc = c[keep]
    ee = e[keep]
    order = np.argsort(cc)
    cs = cc[order]
    ws = ee[order]
    cw = np.cumsum(ws) / ws.sum()
    out = {'mean': float((c * e).sum() / e.sum())}
    for q in (0.75, 0.90, 0.95):
        out['p%d' % (q * 100)] = float(np.interp(q, cw, cs))
    out['area50'] = float((ee[cc > 0.5].sum()) / ee.sum())
    return out


KEYS = ['mean', 'p75', 'p90', 'p95', 'area50']


def main():
    cfg = json.load(open(os.path.join(HERE, 'pool.json')))
    SEL = [tuple(c) for c in cfg['selection']]
    all22 = [tuple(c) for c in json.load(open(os.path.join(T2, 'sheets.json')))['sheets']]
    L = ['TILING4 -- choosing the readout, and the C4 notch control done properly', '']

    # ------------------------------------------------------------------ sweep
    van = {}
    for c in all22:
        v = vanilla(*c)
        van['%d,%d' % c] = dict(val=readouts(v), twin=readouts(S.phase_twin(v, 1)))
    ru = {}
    pr = {}
    wo = {}
    for c in SEL:
        ru['%d,%d' % c] = dict(val=readouts(rung(*c)), twin=readouts(S.phase_twin(rung(*c), 1)))
        pr['%d,%d' % c] = dict(val=readouts(prop(*c)), twin=readouts(S.phase_twin(prop(*c), 1)))
        w = np.load(os.path.join(HERE, 'cache', 'warp_%d_%d.npy' % c))
        wo['%d,%d' % c] = dict(val=readouts(w), twin=readouts(S.phase_twin(w, 1)))

    L.append('Each readout scored on the SAME known answers.  "abs ceil" and "rat ceil"')
    L.append('are the worst of vanilla`s 22 on that readout, absolute and over the')
    L.append('sheet`s own phase twin.  "warp over" = how many of the seven selection')
    L.append('sheets the WARP-ONLY bake (the defect) puts over the ratio ceiling;')
    L.append('"rung over" = how many the rung does, which must be 0 or near it.')
    L.append('')
    L.append('%-9s %9s %9s | %9s %9s | %9s %9s'
             % ('readout', 'abs ceil', 'rat ceil', 'warp over', 'rung over', 'prop over', 'sep'))
    rows = {}
    for k in KEYS:
        v22 = np.array([van[ '%d,%d' % c]['val'][k] for c in all22])
        r22 = np.array([van['%d,%d' % c]['val'][k] / max(van['%d,%d' % c]['twin'][k], 1e-9)
                        for c in all22])
        ac, rc = float(v22.max()), float(r22.max())

        def rat(d, c):
            return d['%d,%d' % c]['val'][k] / max(d['%d,%d' % c]['twin'][k], 1e-9)
        nw = sum(1 for c in SEL if rat(wo, c) > rc)
        nr = sum(1 for c in SEL if rat(ru, c) > rc)
        npp = sum(1 for c in SEL if rat(pr, c) > rc)
        sep = float(np.median([rat(wo, c) for c in SEL]) - np.median([rat(ru, c) for c in SEL]))
        rows[k] = dict(abs_ceil=ac, rat_ceil=rc, warp_over=nw, rung_over=nr,
                       prop_over=npp, sep=sep,
                       rung=[rat(ru, c) for c in SEL], prop=[rat(pr, c) for c in SEL],
                       warp=[rat(wo, c) for c in SEL],
                       van_rat=[float(x) for x in r22], van_abs=[float(x) for x in v22])
        L.append('%-9s %9.4f %9.4f | %6d of 7 %6d of 7 | %6d of 7 %9.3f'
                 % (k, ac, rc, nw, nr, npp, sep))

    good = [k for k in KEYS if rows[k]['rung_over'] == 0 and rows[k]['warp_over'] >= 5]
    L.append('')
    if not good:
        L.append('REFUSED: no readout both clears the warp and acquits the rung.')
        pick = max(KEYS, key=lambda k: rows[k]['warp_over'] - 2 * rows[k]['rung_over'])
        L.append('Closest: %s (warp %d of 7 over, rung %d of 7 over).  This is a LIMIT'
                 % (pick, rows[pick]['warp_over'], rows[pick]['rung_over']))
        L.append('of the instrument and it goes in the report as one.')
    else:
        pick = max(good, key=lambda k: rows[k]['warp_over'])
        L.append('CHOSEN READOUT: %s -- it puts the isolated warp over vanilla`s ratio'
                 % pick)
        L.append('ceiling on %d of 7 and the rung on %d of 7.'
                 % (rows[pick]['warp_over'], rows[pick]['rung_over']))
    L.append('')
    L.append('   %-12s %9s %9s %9s %9s   (ratio over the sheet`s own twin)'
             % ('sheet', 'vanilla', 'rung', 'warp only', 'proposal'))
    for i, c in enumerate(SEL):
        k = pick
        vr = (van['%d,%d' % c]['val'][k] / van['%d,%d' % c]['twin'][k]
              if '%d,%d' % c in van else float('nan'))
        L.append('   (%4d,%4d) %9.4f %9.4f %9.4f %9.4f'
                 % (c[0], c[1], vr, rows[k]['rung'][i], rows[k]['warp'][i],
                    rows[k]['prop'][i]))
    L.append('   ratio ceiling %.4f (worst of vanilla`s 22)' % rows[pick]['rat_ceil'])
    L.append('')

    # --------------------------------------------------------------- C4 again
    L.append('C4 (redone) the notch, against an INJECTED repeat of known amplitude.')
    L.append('   A repeat is coherent and axis-aligned, so it would inflate any')
    L.append('   orientation statistic.  Injected into a real vanilla sheet at')
    L.append('   amplitudes spanning TILING2`s whole range (its ceiling is 0.264 and')
    L.append('   the rung reads about 2.0), the NOTCHED reading must not move.')
    L.append('   %-10s %11s %11s' % ('amp (8-bit)', 'un-notched', 'notched'))
    base = vanilla(-20, 24)
    c4 = []
    b0 = readouts(base)[pick]
    for amp in (0.0, 0.5, 2.0, 8.0):
        sh = T.inject_repeat(base, W.REPEAT_TEXELS, amp, axis='x')
        raw = readouts(sh, notch=False)[pick]
        ntc = readouts(sh, notch=True)[pick]
        c4.append(dict(amp=amp, raw=raw, ntc=ntc))
        L.append('   %-10.1f %11.4f %11.4f' % (amp, raw, ntc))
    drift = max(abs(r['ntc'] - c4[0]['ntc']) for r in c4)
    L.append('   the notched reading drifts %.4f over a 16x range of injected repeat'
             % drift)
    L.append('   (the un-notched one moves %.4f).  %s'
             % (max(abs(r['raw'] - c4[0]['raw']) for r in c4),
                'The notch does separate the grid from the swirl.'
                if drift < 0.25 * max(abs(r['raw'] - c4[0]['raw']) for r in c4)
                else 'REFUSED -- the notch does not hold.'))
    L.append('   (s1c`s C4 used the `average` bake as the no-repeat control and read')
    L.append('   it ABOVE the rung, 0.28..0.70 against 0.15..0.32.  That bake replaces')
    L.append('   every land texture by one flat colour, so its fine band is only')
    L.append('   alpha-blend edges and VCLR ramps -- highly oriented and almost')
    L.append('   energy-free.  It is a valid no-repeat control for a PERIODICITY, which')
    L.append('   is what TILING3 used it for, and a useless one for an ORIENTATION.)')

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 's1d_readout.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(dict(rows=rows, pick=pick, c4=c4, base=b0),
              open(os.path.join(HERE, 's1d_readout.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
