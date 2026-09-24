"""TILING4 step 2c -- the bias WINDOW, then H3 (the capped warp on top of H1).

What h2_rescore.txt established, and why this script looks the way it does:

  * NOTHING in the H1/H2 sweep gates 7 of 7, and what is binding is ONE sheet's
    REPEAT: (-4,-20) reads 0.303 to 0.320 against its 0.264 ceiling on every
    geometry, 15 to 21 % over.  H1's per-hex offsets break the phase BETWEEN
    tiles; they do not touch the land texture's own 10.667-texel period INSIDE
    one tap, and on that sheet what is left is still over the ceiling.  A warp
    is the only thing tested so far that removes the period inside a tap -- so
    H3 is exactly the right next rung, and the brief named it.
  * G1 and G2 pin the mip bias into a NARROW WINDOW from opposite sides.  G2
    allows at most +20 % of the rung's grain; the rung sits 26.7 % below
    vanilla's median, so G1 needs at least +9.1 % of the rung.  The sweep only
    ever sampled 0.00 (+1.8 % of the rung, G1 red at -25.4 %) and -0.50
    (+22.9 %, G2 red).  The window is between them and had never been sampled.
    Stage A samples it.
  * G1-band is NOT a gate on the sampler, and stage C measures that rather than
    asserting it: the RUNG ITSELF reads 2 of 6, because the offline composite
    carries -46 %, -75 % and -97 % of vanilla's share in the three finest bands
    whatever the sampler does.  Stage C prices what closing them would cost.

H3, precisely: the warp is applied to the world position FIRST, then H1's hex
tiling runs on the warped position.  Both stages stay deterministic functions of
world position, so the sample is still seamless across chunk and cell edges and
byte-identical at 1 and 16 threads by construction.  The warp is TILING3's own
field (a5_tune.warp2, one octave, lattice 1,024) so its strain is the same
quantity a5_tune measured and the brief capped at 0.5.

    python h3_sweep.py  ->  logs/h3_sweep.txt, h3_sweep.json
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
import offline_bake as OB                                     # noqa: E402
import a5_tune as A5                                          # noqa: E402
import h_cand as H                                            # noqa: E402
import h1_sweep as HS                                         # noqa: E402
import t4_gates as G                                          # noqa: E402
from s1b_design import rung                                   # noqa: E402

TILE = 341.3333
LAT = 1024.0            # TILING3's warp lattice, unchanged
OCT = 1                 # one octave: a5_tune's own strain law is for this field
_orig_tap = OB._tap


def make_tap_h3(size, amp, mipbias, lat=LAT, oct_=OCT, varnorm=True):
    """TILING3's warp, then H1's hex tiling on the warped position."""
    h1 = H.make_tap_h1(size, mipbias, varnorm=varnorm)

    def tap(dds, wx, wy, tile, upt, mip):
        wx = np.asarray(wx, np.float64)
        wy = np.asarray(wy, np.float64)
        if amp > 0.0:
            ox, oy = A5.warp2(wx, wy, amp, lat, oct_)
            wx = wx + ox
            wy = wy + oy
        return h1(dds, wx, wy, tile, upt, mip)
    return tap


def amp_for_strain(target, lat=LAT, oct_=OCT):
    """The warp field's strain is linear in its amplitude, so one measurement
    fixes the constant; it is re-measured rather than assumed."""
    s1 = A5.strain(1.0, lat, oct_)
    return target / s1, s1


def refs(sheets):
    """The rung and vanilla, per sheet, for the decided gates."""
    rp, van = {}, {}
    for cx, cy in sheets:
        k = '%d,%d' % (cx, cy)
        rp[k] = HS.score(cx, cy, rung(cx, cy))
        R = HS.ref(cx, cy)
        van[k] = dict(hpsd=R['hpsd'], bands=R['bands'])
    return rp, van


def run(name, tap, bias, sheets, rp, van, L, extra=''):
    per = {}
    t0 = time.time()
    for cx, cy in sheets:
        per['%d,%d' % (cx, cy)] = HS.score(cx, cy, H.bake(cx, cy, tap))
    keys = ['%d,%d' % (cx, cy) for cx, cy in sheets]
    r = G.decided(per, rp, van, keys)
    r['name'], r['bias'], r['secs'] = name, bias, time.time() - t0
    r['per'] = per
    L.append(G.row(name + extra, bias, r))
    return r


def main():
    cfg = json.load(open(os.path.join(HERE, 'pool.json')))
    SEL = [tuple(c) for c in cfg['selection']]
    rp, van = refs(SEL)

    L = ['TILING4 -- the mip-bias window, then H3 (the capped warp over H1)', '',
         'Gates: the DECIDED ones (t4_gates.py).  ALL = repeat 7/7 and swirl 7/7',
         'and G1 and G2 7/7 and G2-band 7/7.  G1-band is reported in its own',
         'column and is NOT in ALL -- stage C measures why: the rung itself reads',
         '2 of 6 and no sampler can move the three finest bands.',
         'litG / litB are the brief`s literal per-sheet-vs-vanilla counts.', '']

    out = {}
    L.append('THE RUNG, for the floor (it passes G2 and G2-band by construction):')
    L.append(G.HEAD)
    keys = ['%d,%d' % c for c in SEL]
    rr = G.decided(rp, rp, van, keys)
    L.append(G.row('rung (footprint)', 0.0, rr))
    out['rung'] = {k: v for k, v in rr.items() if k != 'per'}
    L.append('')

    # ------------------------------------------------------------- stage A
    L.append('STAGE A -- the mip-bias window G1 and G2 leave open, never sampled')
    L.append('           before: G2 allows at most +20 % of the rung`s grain, G1')
    L.append('           needs at least +9.1 % of it.')
    L.append(G.HEAD)
    A = []
    for size, nm in ((256.0, 'H1 hex 256'), (341.3333, 'H1 hex 341')):
        for b in (-0.20, -0.30, -0.40):
            A.append(run(nm, H.make_tap_h1(size, b), b, SEL, rp, van, L))
    L.append('')

    okA = [r for r in A if r['g1_ok'] and r['g2_n'] == r['n']]
    if okA:
        L.append('The window is real: %d of %d rows are inside G1 AND G2 at once.'
                 % (len(okA), len(A)))
        best_bias = min(okA, key=lambda r: (-r['rep'], r['swirl_max']))['bias']
        L.append('H3 runs at bias %.2f, the one with the best repeat among them.'
                 % best_bias)
    else:
        best_bias = -0.30
        L.append('NO row is inside G1 and G2 at once; H3 runs at bias -0.30, the')
        L.append('middle of the window the two gates leave, and the report says so.')
    L.append('')

    # ------------------------------------------------------------- stage B
    k1, s1 = amp_for_strain(1.0)
    L.append('STAGE B -- H3: TILING3`s warp, capped at strain 0.5 (the brief`s cap,')
    L.append('           and a5_tune`s own visible-wobble line), applied BEFORE')
    L.append('           H1`s hex tiling.  amp = strain / %.6f (measured, lattice'
             % s1)
    L.append('           %.0f, 1 octave); TILING3 shipped strain %.3f.'
             % (LAT, A5.strain(683.0, 1024.0, 1)))
    L.append(G.HEAD)
    B = []
    for tgt in (0.10, 0.20, 0.30, 0.50):
        amp = tgt / s1
        for size, nm in ((256.0, 'H1 256'), (341.3333, 'H1 341')):
            B.append(run('%s + warp s%.2f' % (nm, tgt),
                         make_tap_h3(size, amp, best_bias), best_bias,
                         SEL, rp, van, L))
            B[-1]['strain'] = tgt
            B[-1]['amp'] = amp
            B[-1]['size'] = size
    L.append('')

    # ------------------------------------------------------------- stage C
    L.append('STAGE C -- what closing G1-band would cost.  The three finest bands')
    L.append('           are short by -46 %, -75 % and -97 % on THE RUNG; only a')
    L.append('           much sharper mip can move them, and this prices it.')
    L.append(G.HEAD)
    C = []
    for b in (-1.50, -2.00):
        C.append(run('H1 hex 256', H.make_tap_h1(256.0, b), b, SEL, rp, van, L))
    L.append('')

    allrows = A + B + C
    green = [r for r in allrows if r['all_ok_nog1b']]
    if green:
        win = min(green, key=lambda r: (r['swirl_max'], r['rep_max']))
        L.append('SELECTION WINNER: %s at mip bias %.2f -- every decided gate 7 of 7.'
                 % (win['name'], win['bias']))
        L.append('Chosen among the %d green rows by the lowest worst-sheet swirl, then'
                 % len(green))
        L.append('the lowest worst-sheet repeat -- h1_sweep.py`s rule, unchanged.')
    else:
        win = max(allrows, key=lambda r: (r['rep'] + r['swirl'] + r['g2_n']
                                          + r['g2b_n'] + (1 if r['g1_ok'] else 0)))
        L.append('NOTHING GATES 7 OF 7.  Closest: %s at bias %.2f.'
                 % (win['name'], win['bias']))
    L.append('')

    L.append('THE BEST ROW, SHEET BY SHEET (every number beside its own ceiling):')
    L.append('   %-12s %7s %7s %7s %7s | %7s %7s %7s | %7s %7s | %s'
             % ('sheet', 'repeat', 'ceil', 'ratio', 'ratCeil', 'grain', 'rung',
                'G2 err', 'swirl', 'ceil', 'verdict'))
    for cx, cy in SEL:
        k = '%d,%d' % (cx, cy)
        v = win['per'][k]
        R = HS.ref(cx, cy)
        L.append('   (%4d,%4d) %7.3f %7.3f %7.3f %7.3f | %7.3f %7.3f %+6.1f%% | '
                 '%7.4f %7.4f | %s'
                 % (cx, cy, v['vis'], max(HS.ABS_CEIL, R['ctrl']), v['ratio'],
                    HS.RAT_CEIL, v['hpsd'], rp[k]['hpsd'],
                    100 * (v['hpsd'] / rp[k]['hpsd'] - 1.0),
                    v['swirl_r'], v['swirl_ceil'],
                    'PASS' if (v['rep_ok'] and v['swirl_ok']
                               and abs(v['hpsd'] / rp[k]['hpsd'] - 1.0) <= G.MARG)
                    else 'RED: ' + ' '.join(
                        n for n, ok in (('repeat', v['rep_ok']),
                                        ('swirl', v['swirl_ok']),
                                        ('G2grain', abs(v['hpsd'] / rp[k]['hpsd'] - 1.0)
                                         <= G.MARG)) if not ok)))

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'h3_sweep.txt'), 'w', newline='\n') as f:
        f.write(txt)
    for r in allrows:
        out['%s @ %.2f' % (r['name'], r['bias'])] = {k: v for k, v in r.items()
                                                     if k != 'per'}
    out['winner'] = dict(name=win['name'], bias=win['bias'],
                         per=win['per'],
                         green=bool(win['all_ok_nog1b']),
                         strain=win.get('strain', 0.0), amp=win.get('amp', 0.0),
                         size=win.get('size', 0.0))
    json.dump(out, open(os.path.join(HERE, 'h3_sweep.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
