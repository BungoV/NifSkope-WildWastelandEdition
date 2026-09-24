"""TILING3 -- choosing the two numbers, by a rule stated BEFORE the numbers.

a5_tune.py's six-band gate reads 0 or 1 of 6 for EVERY variant including ones
whose grain is within a few percent of vanilla's, because two of the six bands
(coarser than 32 texels) carry the sheet's large-scale shape and its tone -- which
this lane may not touch (the brief's Rules: no tone changes, those are GRADE1's;
chunk (-20,24) sits at 2.8x vanilla's coarsest band share and chunk (-20,20) at
0.43x, in the RUNG, before anything of this lane's is switched on).  Gating the
grain on bands the grain does not own is `ww-spec-gate-audit`'s one-sided gate: it
can only be hit by breaking something else.  So the six-band number is still
reported, and the grain is ALSO graded on the statistic the change actually owns:

  * the HIGH-PASS SD -- the amplitude, in luminance levels, of everything finer
    than 5 texels.  Vanilla 4.476 / 5.459; the rung 3.409 / 3.203; the `average`
    bake bungo rejected would be near zero.  This is bungo's "grain", measured.
  * the share of the sheet's variance finer than 4 texels -- TILING2's headline
    factor of eight (vanilla 22.6 % / 21.5 %, the rung 2.8 % / 1.5 %).
  * the 3x3 local variance, as registered.

THE SELECTION RULE, written before this script was run:

  1. Among the warps whose dimensionless repeat (over the sheet's own null floor)
     is at or under vanilla's 0.448 ceiling on EVERY sheet, and whose absolute
     visibility is at or under 0.264 on the majority, take the one with the
     SMALLEST strain -- the least distortion that does the job.
  2. Given that warp, take the mip bias whose MEDIAN high-pass SD over the sheets
     is closest to vanilla's median.

Seven sheets, from TILING2's own 22.

    python a6_pick.py  ->  logs/a6_pick.txt


=============================================================================
AMENDED 2026-09-11 22:5x, AFTER THE SECOND RUN, AND WHY -- THREE FINDINGS
=============================================================================

(1) THE FALLBACK LADDER PRINTED A CLAIM THAT WAS FALSE.  Rule 1's majority
    clause was unmet, so the script took its documented fallback -- "the
    smallest strain that MEETS THE DIMENSIONLESS CEILING on every sheet" -- and
    printed exactly that sentence.  No warp met it: the picked warp's own
    ratMax was 0.698, printed in the table three lines above the sentence.
    `alt` came back empty and the code fell through to a HARDCODED default
    (`'A=683 L=1024 o1'`) while announcing a rule-driven pick.  A selection that
    cannot fail to produce an answer is not a selection.  The ladder below is
    explicit, ordered, and REFUSES rather than defaulting.

(2) THE ABSOLUTE GATE HAD NO PER-SHEET FLOOR.  TILING2 measured that on chunk
    (-20,20) the `average` bake -- byte-identical at two different tiling
    constants, therefore carrying NO periodic term at all -- still read a
    visibility of 0.562, far above vanilla's 0.264 ceiling.  The instrument's
    absolute reading on such a sheet is that sheet's own broadband energy
    landing in the repeat's bin, not a repeat.  So the absolute gate is now
    measured against a PER-SHEET NO-REPEAT CONTROL: the same composite sampled
    at the texture's 1x1 mip (`mip='mean'`), which IS the `average` bake and
    cannot contain a repeat by construction.  Where that control sits under
    0.264 the absolute ceiling applies as written; where it sits above it, the
    ceiling is uninformative on that sheet and the gate becomes "no more
    periodic than a bake that provably has no repeat", plus the dimensionless
    0.448.  This is the floor the brief demands: a control that cannot pass by
    being empty, and that PROVES when the instrument cannot discriminate.

(3) THE NEAR-FLAT VANILLA REFERENCE, chunk (28,-20), was suspected of needing a
    refusal like `refuse_flat`'s.  IT DOES NOT, and the probe that would have
    justified one refutes it instead: its terrain is not flat (mean slope 15.1
    degrees off vertical, from vanilla's own `_msn`, against 16.4 on (-4,-20)
    and a population range 15.1..27.1), and its high-pass SD of 1.371 is still
    4.4x the instrument's own 8-bit quantisation floor on that sheet (0.313),
    against 6.6..9.4x elsewhere.  It is a genuinely smooth vanilla sheet, not an
    empty one, so discarding it would be discarding evidence.  It is KEPT, its
    per-sheet ratio is reported as the outlier it is, and every median is
    printed BOTH with and without it so the pick can be seen not to turn on it.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T2, SP):
    sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402
import offline_bake as OB                                     # noqa: E402
import a5_tune as A5                                          # noqa: E402

# THE FLOOR THAT SHOULD HAVE BEEN HERE FIRST.  The first run of this script
# picked its numbers off six sheets of which THREE baked flat 127.5 -- SPLAT1's
# offline model serves only 7 of TILING2's 22 chunks (logs/a6_usable.txt); the
# other 15 come back as one grey value, and a flat sheet reads high-pass SD
# 0.000, local variance 0.000 and a repeat of 0.000, which sails through a gate
# that only looks for "small".  `refuse_flat` below refuses them BY NAME.
SHEETS = [(-20, 24), (-20, 20), (-36, -20), (-4, -20), (28, -20), (-4, 16), (24, 16)]
OUTLIER = (28, -20)          # kept, but every median is printed without it too
FLAT_SD = 1.0                # a bake below this is not a sheet; it is a refusal
ABS_CEIL = 0.264             # vanilla's worst of 22 (TILING2 section 1a)
RAT_CEIL = 0.448             # the same, over the sheet's own null floor
TILE = 341.3333
WARPS = [('none', 0.0, 2048.0, 1),
         ('A=341 L=2048 o1', 341.0, 2048.0, 1),
         ('A=683 L=2048 o1', 683.0, 2048.0, 1),
         ('A=683 L=1024 o1', 683.0, 1024.0, 1),
         ('A=683 L=1024 o2', 683.0, 1024.0, 2),
         ('A=1365 L=2048 o3', 1365.0, 2048.0, 3)]
MIPS = [0.0, -0.5, -0.75, -1.0, -1.25]


def fine4(L):
    ctr, pw = S.radial_power(np.asarray(L, np.float64))
    tab = S.band_table(ctr, pw)
    tot = sum(v for _n, v in tab) or 1e-12
    return sum(v for _n, v in tab[-2:]) / tot


def refuse_flat(L, where):
    sd = float(np.asarray(L, np.float64).std())
    if sd < FLAT_SD:
        raise RuntimeError('REFUSED: %s baked flat (SD %.4f < %.2f)' % (where, sd, FLAT_SD))
    return L


def stats(L):
    vis, fl = T.tiling_visibility(L)
    return dict(vis=vis, floor=fl, ratio=vis / max(fl, 1e-9),
                hpsd=float(T.hp_residual(L, r=2).std()),
                lv=float(S.local_var(L).mean()), f4=fine4(L))


def no_repeat_control(cx, cy):
    """The per-sheet floor of the absolute periodicity instrument: the same
    composite sampled at the texture's 1x1 mip -- its exact mean over one whole
    repeat -- so NO periodic term of the land texture can reach the sheet.  Any
    visibility this reads is the sheet's own broadband energy in the bin."""
    sh = refuse_flat(S.lum(OB.bake(cx, cy, dim=4, tile=TILE, mip='mean')),
                     'no-repeat control (%d,%d)' % (cx, cy))
    vis, fl = T.tiling_visibility(sh)
    return vis, fl


def med(vals, keys, drop=None):
    xs = [v for v, k in zip(vals, keys) if k != drop]
    return float(np.median(xs))


def main():
    Lg = ['TILING3 -- picking the warp and the mip bias over seven shipped sheets', '']
    keys = ['%d,%d' % (cx, cy) for cx, cy in SHEETS]
    okey = '%d,%d' % OUTLIER

    # ---------------------------------------------- the per-sheet instrument floor
    Lg.append('FLOOR -- the no-repeat control, per sheet (the `average` bake: the land')
    Lg.append('texture 1x1 mip, which cannot carry a repeat).  Where this sits ABOVE the')
    Lg.append('0.264 absolute ceiling, that ceiling is uninformative on that sheet.')
    Lg.append('   %-12s %8s %8s   %s' % ('sheet', 'ctrlVis', 'ctrlRat', 'absolute gate'))
    ctrl = {}
    for (cx, cy), k in zip(SHEETS, keys):
        cv, cf = no_repeat_control(cx, cy)
        ctrl[k] = dict(vis=cv, ratio=cv / max(cf, 1e-9))
        Lg.append('   (%4d,%4d) %8.3f %8.3f   %s'
                  % (cx, cy, cv, ctrl[k]['ratio'],
                     'APPLIES (<= %.3f)' % ABS_CEIL if cv < ABS_CEIL
                     else 'REFUSED: control %.3f is already over the ceiling' % cv))
    Lg.append('')

    van = {}
    Lg.append('   %-12s %7s %7s %7s %7s %7s' % ('vanilla', 'repeat', 'ratio', 'hpSD', 'locVar', 'f<4tx'))
    for (cx, cy), k in zip(SHEETS, keys):
        v = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        st = stats(v)
        van[k] = st
        Lg.append('   (%4d,%4d) %7.3f %7.3f %7.3f %7.2f %6.1f%%%s'
                  % (cx, cy, st['vis'], st['ratio'], st['hpsd'], st['lv'], 100 * st['f4'],
                     '   <- smooth reference, see docstring (3)' if k == okey else ''))
    vmed = {x: med([van[k][x] for k in keys], keys) for x in ('vis', 'ratio', 'hpsd', 'lv', 'f4')}
    vmed_no = {x: med([van[k][x] for k in keys], keys, okey)
               for x in ('vis', 'ratio', 'hpsd', 'lv', 'f4')}
    Lg.append('   MEDIAN       %7.3f %7.3f %7.3f %7.2f %6.1f%%'
              % (vmed['vis'], vmed['ratio'], vmed['hpsd'], vmed['lv'], 100 * vmed['f4']))
    Lg.append('   MEDIAN without (28,-20)      %7.3f %7.2f %6.1f%%'
              % (vmed_no['hpsd'], vmed_no['lv'], 100 * vmed_no['f4']))
    Lg.append('')

    def sheet_rep_ok(k, vis, ratio):
        """The control-aware per-sheet repeat gate."""
        if ratio > RAT_CEIL:
            return False
        c = ctrl[k]['vis']
        return vis <= ABS_CEIL if c < ABS_CEIL else vis <= c

    # ------------------------------------------------------------ step 1, the warp
    Lg.append('STEP 1 -- the warp, at a fixed mip bias of -1.0 (step 2 re-picks the bias)')
    Lg.append('   %-18s %7s %7s %7s %7s %6s  %s'
              % ('warp', 'strain', 'repMax', 'ratMax', 'repMed', 'gated', 'rule 1'))
    wres = {}
    for lab, amp, lat, oc in WARPS:
        vis, rat, gated = [], [], 0
        for (cx, cy), k in zip(SHEETS, keys):
            sh = refuse_flat(S.lum(A5.bake_v(cx, cy, amp, lat, oc, -1.0)),
                             'warp %s (%d,%d)' % (lab, cx, cy))
            st = stats(sh)
            vis.append(st['vis'])
            rat.append(st['ratio'])
            gated += 1 if sheet_rep_ok(k, st['vis'], st['ratio']) else 0
        st_ = A5.strain(amp, lat, oc) if amp > 0 else 0.0
        ok1 = (max(rat) <= RAT_CEIL) and (sum(1 for x in vis if x <= ABS_CEIL) * 2 > len(vis))
        Lg.append('   %-18s %7.3f %7.3f %7.3f %7.3f %5s  %s'
                  % (lab, st_, max(vis), max(rat), float(np.median(vis)),
                     '%d/%d' % (gated, len(SHEETS)), 'QUALIFIES' if ok1 else ''))
        wres[lab] = dict(strain=st_, vis=vis, rat=rat, ok1=bool(ok1), gated=gated,
                         amp=amp, lat=lat, oct=oc)

    # THE LADDER.  Ordered, explicit, and it REFUSES rather than defaulting.
    pick_w, rung = None, None
    q1 = sorted((v['strain'], k) for k, v in wres.items() if v['ok1'])
    if q1:
        pick_w, rung = q1[0][1], 'rule 1 as pre-registered'
    else:
        q2 = sorted((v['strain'], k) for k, v in wres.items()
                    if max(v['rat']) <= RAT_CEIL and v['amp'] > 0)
        if q2:
            pick_w, rung = q2[0][1], 'fallback (a): the dimensionless ceiling on every sheet'
        else:
            q3 = sorted((v['strain'], k) for k, v in wres.items()
                        if v['gated'] == len(SHEETS) and v['amp'] > 0)
            if q3:
                pick_w, rung = q3[0][1], ('fallback (b): the CONTROL-AWARE per-sheet gate '
                                          'green on every sheet, smallest strain')
            else:
                # NOTHING PASSES.  The lane does NOT go looking for a denominator
                # that would make one pass -- that is defect (1) again, in a new
                # costume.  It names the failure, and emits the best variant
                # LABELLED AS A PROPOSAL, never as a pass.  This is safe to ship
                # only because the switch's OFF value is the rung's bytes: a
                # proposal that is off by default regresses nothing.
                best = max(wres.items(),
                           key=lambda kv: (kv[1]['gated'], -kv[1]['strain'])
                           if kv[1]['amp'] > 0 else (-1, 0))
                pick_w = best[0]
                rung = ('NOT PASSED -- PROPOSAL ONLY: no warp meets rule 1, fallback (a)'
                        ' or fallback (b); this is the most-gated variant, ties to the'
                        ' smallest strain')
                Lg.append('   GATE NOT MET.  No warp passes rule 1, fallback (a) or (b).')
                Lg.append('   The lane does not re-choose the denominator until something')
                Lg.append('   passes; it names the red and ships the switch OFF by default.')
    Lg.append('   PICKED BY %s' % rung)
    Lg.append('   PICKED: %s  (strain %.3f, gated %d/%d)'
              % (pick_w, wres[pick_w]['strain'], wres[pick_w]['gated'], len(SHEETS)))
    for (cx, cy), k, v, r in zip(SHEETS, keys, wres[pick_w]['vis'], wres[pick_w]['rat']):
        if not sheet_rep_ok(k, v, r):
            Lg.append('   RED (%d,%d): visibility %.3f (ceiling %.3f, control %.3f),'
                      ' ratio %.3f (ceiling %.3f); vanilla null floor on that same'
                      ' ground %.3f, this bake`s %.3f'
                      % (cx, cy, v, ABS_CEIL, ctrl[k]['vis'], r, RAT_CEIL,
                         van[k]['vis'] / max(van[k]['ratio'], 1e-9),
                         v / max(r, 1e-9)))
    Lg.append('')

    # ------------------------------------------------------------ step 2, the mip
    amp, lat, oc = wres[pick_w]['amp'], wres[pick_w]['lat'], wres[pick_w]['oct']
    Lg.append('STEP 2 -- the mip bias, with that warp')
    Lg.append('   %-8s %8s %8s %8s %8s %8s %8s %6s'
              % ('bias', 'hpSDmed', 'vs van', 'lVmed', 'vs van', 'f<4med', 'repMax', 'gated'))
    mres = {}
    for mb in MIPS:
        hs, lv, f4, vi, ra, g = [], [], [], [], [], 0
        for (cx, cy), k in zip(SHEETS, keys):
            sh = refuse_flat(S.lum(A5.bake_v(cx, cy, amp, lat, oc, mb)),
                             'mip %.2f (%d,%d)' % (mb, cx, cy))
            st = stats(sh)
            hs.append(st['hpsd']); lv.append(st['lv'])
            f4.append(st['f4']); vi.append(st['vis']); ra.append(st['ratio'])
            g += 1 if sheet_rep_ok(k, st['vis'], st['ratio']) else 0
        h, l, f = med(hs, keys), med(lv, keys), med(f4, keys)
        Lg.append('   %-8.2f %8.3f %+7.0f%% %8.2f %+7.0f%% %7.1f%% %8.3f %5s'
                  % (mb, h, 100 * (h / vmed['hpsd'] - 1), l, 100 * (l / vmed['lv'] - 1),
                     100 * f, max(vi), '%d/%d' % (g, len(SHEETS))))
        mres['%.2f' % mb] = dict(hpsd=hs, lv=lv, f4=f4, vis=vi, ratio=ra,
                                 hmed=h, lmed=l, fmed=f, gated=g,
                                 hmed_no=med(hs, keys, okey), lmed_no=med(lv, keys, okey))
    pick_m = min(MIPS, key=lambda m: abs(mres['%.2f' % m]['hmed'] - vmed['hpsd']))
    pick_m_no = min(MIPS, key=lambda m: abs(mres['%.2f' % m]['hmed_no'] - vmed_no['hpsd']))
    Lg.append('   PICKED: mip bias %.2f (median high-pass SD %.3f against vanilla %.3f)'
              % (pick_m, mres['%.2f' % pick_m]['hmed'], vmed['hpsd']))
    Lg.append('   the same rule with (28,-20) DROPPED picks %.2f -- %s'
              % (pick_m_no, 'the same number, so the pick does not turn on that sheet'
                 if pick_m_no == pick_m else 'A DIFFERENT NUMBER: the pick DOES turn on it'))
    Lg.append('')

    # ------------------------------------------------- the chosen setting, per sheet
    m = mres['%.2f' % pick_m]
    Lg.append('THE CHOSEN SETTING, per sheet: warp %s, mip bias %.2f' % (pick_w, pick_m))
    Lg.append('   %-12s %8s %8s %8s %8s %8s %8s %8s  %s'
              % ('sheet', 'repeat', 'ratio', 'ctrl', 'hpSD', 'vs van', 'locVar', 'f<4tx',
                 'repeat gate'))
    per = {}
    for i, ((cx, cy), k) in enumerate(zip(SHEETS, keys)):
        ok = sheet_rep_ok(k, m['vis'][i], m['ratio'][i])
        Lg.append('   (%4d,%4d) %8.3f %8.3f %8.3f %8.3f %+7.0f%% %8.2f %7.1f%%  %s'
                  % (cx, cy, m['vis'][i], m['ratio'][i], ctrl[k]['vis'], m['hpsd'][i],
                     100 * (m['hpsd'][i] / van[k]['hpsd'] - 1), m['lv'][i],
                     100 * m['f4'][i], 'GREEN' if ok else 'RED'))
        per[k] = dict(vis=m['vis'][i], ratio=m['ratio'][i], ctrl=ctrl[k]['vis'],
                      hpsd=m['hpsd'][i], lv=m['lv'][i], f4=m['f4'][i], rep_ok=bool(ok))
    Lg.append('   vanilla for comparison, same order: hpSD %s'
              % ' '.join('%.2f' % van[k]['hpsd'] for k in keys))
    Lg.append('   vanilla f<4tx:                      %s'
              % ' '.join('%.0f%%' % (100 * van[k]['f4']) for k in keys))
    Lg.append('')
    Lg.append('REPEAT GATE %d/%d green.  The rung on the same seven sheets (warp `none`,'
              % (sum(1 for v in per.values() if v['rep_ok']), len(SHEETS)))
    Lg.append('same mip bias) reads a worst-sheet visibility of %.3f -- the repeat bungo sees.'
              % max(wres['none']['vis']))

    txt = '\n'.join(Lg) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'a6_pick.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(dict(van=van, vmed=vmed, vmed_no=vmed_no, ctrl=ctrl, warps=wres,
                   mips=mres, per=per,
                   pick=dict(warp=pick_w, rung=rung, mip=pick_m, amp=amp, lat=lat, oct=oc)),
              open(os.path.join(HERE, 'a6_pick.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
