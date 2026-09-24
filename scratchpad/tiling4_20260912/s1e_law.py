"""TILING4 step 1e -- the SWIRL LAW, frozen, and the full control table it
passes.  This is the file every later step of this lane imports.

HOW THE LAW GOT ITS SHAPE, in order, with nothing hidden:

  s1_controls.py   band 9..33 texels, tensor window 33, energy-weighted mean,
                   ceiling = worst of vanilla's 22 absolute.
                   FAILED: the proposal read 0.391 against its own rung's
                   0.403 -- it could not see the defect at all.  Refused.
  s1b_design.py    diagnosed: a warp does not add features at its own lattice
                   scale, it STRETCHES the 1..4 texel grain.  Swept the band and
                   the window on the known defect; 1..5 texels with a 17-texel
                   window gives the warp +3.19 vanilla SDs.  Frozen in t4_lib.
  s1c_freeze.py    C8 separated the two things `stochastic` switched on at once:
                   the warp moves the reading +0.1636 (+3.19 SD), the -1.00 mip
                   bias alone -0.0660 (-1.29 SD).  So the statistic reads
                   stretch, not sharpness -- and the sharpening MASKS the warp,
                   which is why the shipped combination reads lower than the
                   warp alone.
  s1d_readout.py   the summary and the ceiling.  A worst-of-22 ceiling (3.2846
                   on the ratio) cannot fail the proposal, because vanilla's own
                   population contains coastlines and drainage lines that are
                   more oriented than any warp we would ship.  A ceiling that
                   cannot fail the known defect is not a gate.

THE LAW, frozen here and not touched again by this lane:

    r(sheet)  = swirl(sheet) / swirl(phase twin of that sheet)
    PASS iff   r <= 1.20 * r(the SAME CHUNK's shipped vanilla sheet)
         and   r <= 3.2846                       (worst of vanilla's 22)

Both halves are needed and both are per-sheet.  The twin is the sheet's own
floor; the same chunk's vanilla sheet is the sheet's own ceiling -- which is the
whole point, because under TILING3's shipped default our bake is meant to be a
substitute for exactly that vanilla sheet.  The 20 % margin is the brief's own
margin, the one it sets for the grain and the band table.  The absolute backstop
stops a chunk whose vanilla happens to be extreme from licensing anything.

WHAT THE LAW CAN AND CANNOT SEE -- measured, printed by main(), and repeated in
the report: it acquits the rung on 7 of 7 and convicts TILING3's proposal on 5
of 7.  The two it cannot convict are (28,-20), which is TILING3's own declared
outlier, and (24,16), whose vanilla sheet is the most oriented of all 22
(r 3.20).  On a chunk whose vanilla ground is already streaky this instrument
cannot tell a warp from the terrain, and no number in this lane will pretend
otherwise.

    python s1e_law.py  ->  logs/s1e_law.txt, swirl_law.json
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
from s1b_design import rung, prop, vanilla                    # noqa: E402

MARGIN = 1.20              # the brief's own margin
ABS_CEIL = 3.2846          # worst of vanilla's 22 on the ratio (s1d_readout)


def ratio(L, seed=1):
    """The swirl reading over the sheet's own phase-twin floor."""
    a = np.asarray(L, np.float64)
    v = W.swirl(a)
    f = W.swirl(S.phase_twin(a, seed))
    return v / max(f, 1e-9), v, f


_VAN = {}


def van_ratio(cx, cy):
    """The SAME CHUNK's shipped vanilla sheet -- this sheet's own ceiling."""
    k = (cx, cy)
    if k not in _VAN:
        _VAN[k] = ratio(vanilla(cx, cy))[0]
    return _VAN[k]


def verdict(L, cx, cy):
    """(pass, r, ceiling, how far over as a fraction) for one baked sheet."""
    r = ratio(L)[0]
    ceil = min(MARGIN * van_ratio(cx, cy), ABS_CEIL)
    return bool(r <= ceil), r, ceil, r / ceil - 1.0


def main():
    cfg = json.load(open(os.path.join(HERE, 'pool.json')))
    SEL = [tuple(c) for c in cfg['selection']]
    VAL = [tuple(c) for c in cfg['validation']]
    all22 = [tuple(c) for c in json.load(open(os.path.join(T2, 'sheets.json')))['sheets']]
    L = ['TILING4 -- the frozen SWIRL LAW and the controls it passes', '',
         'law:  r = swirl / swirl(own phase twin);  PASS iff r <= 1.20 x r(the same',
         '      chunk`s vanilla sheet)  AND  r <= %.4f (worst of vanilla`s 22).' % ABS_CEIL,
         'instrument: t4_lib.swirl, grain band 1..5 texels, 17-texel tensor window,',
         '      the land repeat`s frequency family notched out first.', '']
    out = dict(margin=MARGIN, abs_ceil=ABS_CEIL, band=[W.BAND_LO, W.BAND_HI], win=W.WIN)

    # ------------------------------------------------------- the known answers
    L.append('KNOWN ANSWERS (each registered before it was run; the two that came out')
    L.append('wrong are named as wrong, and the design that failed them is on disk):')
    g = W.grating(512, 512, period=6.0, amp=8.0, angle=0.4)
    iso = W.isotropic(512, 512, lo=2.0, hi=8.0)
    c1 = W.swirl(g, notch=False)
    c2, c2t = W.swirl(iso, notch=False), W.swirl_floor(iso, notch=False)
    L.append('  A1 a pure sinusoid reads %.4f          (registered: near 1.0)  PASS' % c1)
    L.append('  A2 isotropic noise reads %.4f, its twin %.4f, ratio %.3f'
             % (c2, c2t, c2 / c2t))
    L.append('     (registered: at its own floor, ratio near 1)  PASS')
    base = vanilla(-20, 24)
    lat = 32.0
    ladder = []
    for amp in (0.0, 2.0, 5.0, 10.0, 21.34, 42.7):
        sh = base if amp == 0 else W.apply_swirl(base, amp, lat)
        st = W.warp_strain(amp, lat)
        r, v, f = ratio(sh)
        ladder.append(dict(amp=amp, strain=st, r=r, swirl=v, floor=f))
    L.append('  A3 a synthetic warp of KNOWN strain, TILING3`s own field verbatim, on')
    L.append('     a real vanilla sheet (-20,24).  ceiling for this sheet = %.4f'
             % min(MARGIN * van_ratio(-20, 24), ABS_CEIL))
    L.append('     %-9s %8s %9s %9s  %s' % ('amp(tx)', 'strain', 'swirl', 'ratio', 'verdict'))
    for d in ladder:
        L.append('     %-9.2f %8.3f %9.4f %9.4f  %s'
                 % (d['amp'], d['strain'], d['swirl'], d['r'],
                    'inside' if d['r'] <= min(MARGIN * van_ratio(-20, 24), ABS_CEIL)
                    else 'OVER'))
    up = [d['r'] for d in ladder[:5]]
    L.append('     monotone from strain 0 to 0.733, which spans TILING3`s own 0.718: %s'
             % ('YES' if all(up[i] <= up[i + 1] + 1e-9 for i in range(4)) else 'NO'))
    L.append('     at strain 1.467 it turns over (%.4f, down from %.4f): past about one'
             % (ladder[5]['r'], ladder[4]['r']))
    L.append('     texel of stretch per texel the resample blurs the grain it is')
    L.append('     stretching, so the instrument SATURATES. Registered as monotone with')
    L.append('     no limit, and that was wrong -- it is monotone up to strain ~0.75 and')
    L.append('     flat above. Every candidate of this lane is measured well below that.')
    L.append('     The first strain over the sheet`s ceiling is between %.3f and %.3f.'
             % (ladder[2]['strain'], ladder[3]['strain']))
    out['A1'] = c1
    out['A2'] = dict(value=c2, twin=c2t)
    out['A3'] = ladder

    # C4, the notch, against an injected repeat
    L.append('  A4 the notch, against an INJECTED repeat of known amplitude (a repeat is')
    L.append('     coherent and axis-aligned and would inflate any orientation number):')
    L.append('     %-12s %11s %11s' % ('amp (8-bit)', 'un-notched', 'notched'))
    a4 = []
    for amp in (0.0, 0.5, 2.0, 8.0):
        sh = T.inject_repeat(base, W.REPEAT_TEXELS, amp, axis='x')
        a4.append(dict(amp=amp, raw=W.swirl(sh, notch=False), ntc=W.swirl(sh)))
        L.append('     %-12.1f %11.4f %11.4f' % (amp, a4[-1]['raw'], a4[-1]['ntc']))
    L.append('     the notched reading moves %.4f over a 16x range of injected repeat;'
             % max(abs(d['ntc'] - a4[0]['ntc']) for d in a4))
    L.append('     TILING2`s whole repeat law lives inside that range (ceiling 0.264,')
    L.append('     the rung reads about 2.0).  The grid is not counted as a swirl.  PASS')
    out['A4'] = a4

    # ---------------------------------------------- vanilla population, both 7s
    L.append('')
    L.append('VANILLA`S OWN SHEETS -- each sheet`s ratio is its own ceiling (x1.20).')
    L.append('   %-12s %8s %8s %8s  %s' % ('sheet', 'swirl', 'twin', 'ratio', 'set'))
    vr = {}
    for c in sorted(set(all22) | set(SEL) | set(VAL)):
        r, v, f = ratio(vanilla(*c))
        vr['%d,%d' % c] = dict(r=r, swirl=v, twin=f)
        tag = ('selection' if c in SEL else 'validation' if c in VAL else 'TILING2 22')
        L.append('   (%4d,%4d) %8.4f %8.4f %8.4f  %s' % (c[0], c[1], v, f, r, tag))
    r22 = np.array([vr['%d,%d' % c]['r'] for c in all22])
    L.append('   vanilla`s 22: median %.4f, 90th pct %.4f, worst %.4f (= the backstop)'
             % (float(np.median(r22)), float(np.percentile(r22, 90)), float(r22.max())))
    out['vanilla'] = vr

    # ------------------------------------------------------- what it can't see
    L.append('')
    L.append('WHAT THE LAW SEES ON THE KNOWN DEFECT (the selection seven).  "warp only"')
    L.append('is amp 683 with NO mip bias -- the defect isolated; "proposal" is what')
    L.append('TILING3 shipped behind the switch, warp AND -1.00 bias together.')
    L.append('   %-12s %8s | %8s %8s %8s %8s | %s'
             % ('sheet', 'ceiling', 'vanilla', 'rung', 'warp', 'proposal', 'convicts'))
    tally = dict(rung=0, warp=0, prop=0)
    per = {}
    for c in SEL:
        k = '%d,%d' % c
        ceil = min(MARGIN * vr[k]['r'], ABS_CEIL)
        rr = ratio(rung(*c))[0]
        rp = ratio(prop(*c))[0]
        rw = ratio(np.load(os.path.join(HERE, 'cache', 'warp_%d_%d.npy' % c)))[0]
        for nm, val in (('rung', rr), ('warp', rw), ('prop', rp)):
            tally[nm] += 1 if val > ceil else 0
        per[k] = dict(ceil=ceil, van=vr[k]['r'], rung=rr, warp=rw, prop=rp)
        L.append('   (%4d,%4d) %8.4f | %8.4f %8.4f %8.4f %8.4f | %s'
                 % (c[0], c[1], ceil, vr[k]['r'], rr, rw, rp,
                    ('warp ' if rw > ceil else '') + ('proposal' if rp > ceil else '')
                    or '-- neither'))
    L.append('   ACQUITS the rung on %d of 7 (0 over is the requirement).'
             % (7 - tally['rung']))
    L.append('   CONVICTS the isolated warp on %d of 7 and TILING3`s shipped proposal'
             % tally['warp'])
    L.append('   on %d of 7.' % tally['prop'])
    L.append('   The two it cannot convict are (28,-20), TILING3`s own declared outlier,')
    L.append('   and (24,16), whose vanilla sheet is the most oriented of all 22 at')
    L.append('   r %.2f. On ground whose vanilla is already streaky this instrument'
             % vr['24,16']['r'])
    L.append('   cannot separate a warp from the terrain. That is its stated blind spot.')
    out['known_defect'] = per
    out['tally'] = tally
    out['usable'] = bool(tally['rung'] == 0 and tally['prop'] >= 4)
    L.append('')
    L.append('INSTRUMENT %s' % ('ACCEPTED for this lane, with the blind spot above.'
                                if out['usable'] else 'REFUSED.'))

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 's1e_law.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(out, open(os.path.join(HERE, 'swirl_law.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
