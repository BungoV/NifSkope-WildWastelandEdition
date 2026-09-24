"""TILING3 gate F3 -- the repeat gone AND the grain kept, ON THE SAME BAKE.

The brief's gate is one sentence and it is deliberately hard to pass: a green on
one by losing the other is a red.  So both numbers come off the SAME file, on
both tiles, from the real exe, and both are stated beside vanilla's value on the
same ground:

    repeat  tiling_visibility at 10.667 texels (341.3333 world units / 32 upt),
            in 8-bit luminance levels.  Its own floor is a 12-period null sweep.
            LOWER IS BETTER; vanilla's own reading is the target, not zero.
    grain   the SD of hp_residual(r=2) -- everything finer than 5 texels.
            VANILLA'S VALUE IS THE TARGET, not more and not less: `average`
            passed the repeat test at 35 % of vanilla's grain and bungo
            rejected it on sight ("only solid color blobs").

The variants compared are all REAL DDS written by release/NifSkope.exe:

    rung    release/NifSkope.before_tiling3.exe, this lane's floor
    off     the new exe with --land-detail-source none: must BE the rung
    ship    the new exe at its shipped defaults (vanilla _msn + crevice)
    stoch   the shipped defaults plus --land-sample stochastic (the PROPOSAL)

    python f3_gate.py  ->  logs/f3_gate.txt
"""
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

P = 341.3333 / 32.0
TILES = [('t2024', -20, 24), ('t2020', -20, 20)]
VARIANTS = ['rung', 'off', 'ship', 'stoch']
# the ceilings a6_pick.py froze, on the 22 shipped sheets it measured
ABS_CEIL, RAT_CEIL = 0.264, 0.448


def sheet(var, tile, cx, cy):
    return os.path.join(HERE, 'out', var, tile, 'tex',
                        'Commonwealth.4.%d.%d.DDS' % (cx, cy))


def nums(path):
    a = S.Dds(path).level(0)
    lum = S.lum(a)
    vis, fl = T.tiling_visibility(lum, P)
    return (float(vis), float(fl), float(T.hp_residual(lum, r=2).std()),
            float(S.local_var(lum).mean()))


def main():
    L = ['TILING3 gate F3 -- repeat AND grain, on the same bake, both tiles', '']
    L.append('repeat = amplitude at 10.667 texels (8-bit luminance levels), lower better')
    L.append('null   = the instrument`s own 12-period floor on that same sheet')
    L.append('grain  = SD of everything finer than 5 texels; VANILLA IS THE TARGET')
    L.append('')
    rows = {}
    for tile, cx, cy in TILES:
        vp = S.van_sheet(cx, cy)
        vv = nums(vp)
        rows[(tile, 'vanilla')] = vv
        L.append('chunk (%d,%d)' % (cx, cy))
        L.append('   %-10s %9s %9s %9s %9s %9s'
                 % ('variant', 'repeat', 'null', 'grain', 'grain/van', 'localVar'))
        L.append('   %-10s %9.3f %9.3f %9.3f %9s %9.2f'
                 % ('VANILLA', vv[0], vv[1], vv[2], '1.000', vv[3]))
        for var in VARIANTS:
            p = sheet(var, tile, cx, cy)
            if not os.path.exists(p):
                L.append('   %-10s (not baked)' % var)
                continue
            n = nums(p)
            rows[(tile, var)] = n
            L.append('   %-10s %9.3f %9.3f %9.3f %9.3f %9.2f'
                     % (var, n[0], n[1], n[2], n[2] / max(vv[2], 1e-9), n[3]))
        L.append('')

    L.append('THE GATE, stated as the brief states it -- both on the same bake:')
    L.append('')
    L.append('   %-10s %-8s %9s %9s %9s %9s  %s'
             % ('variant', 'tile', 'repeat', 'vanilla', 'grain%', 'ratio', 'verdict'))
    verdicts = {}
    for var in VARIANTS:
        allgood = True
        for tile, cx, cy in TILES:
            if (tile, var) not in rows:
                allgood = False
                continue
            n = rows[(tile, var)]
            v = rows[(tile, 'vanilla')]
            ratio = n[0] / max(v[0], 1e-9)
            grainpc = 100.0 * n[2] / max(v[2], 1e-9)
            # the repeat half: absolute ceiling, with vanilla's own reading as
            # the per-sheet floor when vanilla itself is already over it
            repok = (n[0] <= ABS_CEIL if v[0] < ABS_CEIL else n[0] <= v[0])
            # the grain half: within a quarter of vanilla's, either way. Below
            # is the `average` failure bungo rejected; far above is invented
            # detail, which is the other way to fail the same sentence.
            grok = 0.75 <= n[2] / max(v[2], 1e-9) <= 1.25
            ok = repok and grok
            allgood = allgood and ok
            L.append('   %-10s %-8s %9.3f %9.3f %8.0f%% %9.3f  %s%s'
                     % (var, tile, n[0], v[0], grainpc, ratio,
                        'repeat OK' if repok else 'REPEAT RED',
                        ', grain OK' if grok else ', GRAIN RED'))
        verdicts[var] = allgood
    L.append('')
    for var in VARIANTS:
        L.append('   %-10s %s' % (var, 'PASSES BOTH HALVES ON BOTH TILES'
                                  if verdicts[var] else 'does not pass both halves'))
    L.append('')
    L.append('A CAVEAT THE NUMBERS THEMSELVES FORCE, and it is not a small one.')
    L.append('On both of these sheets the reading sits BELOW the instrument`s own')
    L.append('12-period null sweep -- vanilla`s included (0.201 against a null of')
    L.append('0.449). A null sweep`s maximum over twelve wrong periods is a')
    L.append('significance floor for ONE sheet in isolation, and at 512 texels on a')
    L.append('single chunk it is simply too coarse to separate these readings. The')
    L.append('threshold that IS usable is the one a6_pick.py froze on the 22 shipped')
    L.append('vanilla sheets -- absolute 0.264, the worst of them -- and that is what')
    L.append('the verdicts above are graded against. Stated here rather than left for')
    L.append('someone to find in the table.')
    L.append('')
    L.append('WHAT THE SHIPPED DEFAULT DOES AND DOES NOT DO. `ship` is the ruling:')
    L.append('vanilla`s `_msn` byte for byte and the crevice term on our colour. It')
    L.append('does NOT touch the repeat -- the warp is OFF by default, because the')
    L.append('7-sheet selection in a6_pick.py did NOT meet its own gate (6 of 7) and')
    L.append('this lane refuses to ship a proposal as a pass. `stoch` is that')
    L.append('proposal, one flag away, and its two numbers are printed above so')
    L.append('bungo can decide on the numbers rather than on a recommendation.')
    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'f3_gate.txt'), 'w', newline='\n') as f:
        f.write(txt)


if __name__ == '__main__':
    main()
