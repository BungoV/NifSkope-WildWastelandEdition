#!/usr/bin/env python
"""IS THE LADDER'S FIRST STEP SELECTABLE? (lane NATIVE1c)

`docs/FO4CS_IMPROVED_LOD_PLAN.md` carries one number out of rung 3: at a
one-pixel tolerance the ladder's first step **is not selected anywhere in the
Commonwealth**, because the median level-1 cluster deviates by 3.80 percent of
its model's own diagonal -- 38 units on a 1,000-unit building -- and

    screenErrorPx = geometricError x scale x projectionScale / distance

reaches 1 px only past 52,100 units.  The cause named there is not the ladder:
it is that "full detail" in the v3 library was already Bethesda's LOD mesh, a
mean of 47.7 triangles for a whole building, with nothing left to remove.

This reads the SAME number back out of a `.lodo` so the plan row can be closed
with a measurement instead of an argument.  Run it on both arms: `--library
near` must bring the distance DOWN, and the gate is a stated ceiling.

    python tests/spells/lodgen_ladder_select.py <ws.lodo> [--max-distance 52100]

Prints the median level-1 deviation, the distance at which it first subtends
one pixel, `N checks, M failures` and `RESULT PASS`/`FAIL`; exit 1 on failure.
"""
import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from lodgen_native_cut import read_lodo  # noqa: E402

#: The reference projection the plan and `lodgen_native_cut.py` both use:
#: a 1,920 x 1,080 display at a 70-degree horizontal field of view.
#: A reference constant, not a format constant.
PROJECTION_SCALE = 960.0 / math.tan(math.radians(35.0))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('lodo')
    ap.add_argument('--max-distance', type=float, default=52100.0)
    ap.add_argument('--tolerance', type=float, default=1.0)
    a = ap.parse_args()
    L = read_lodo(a.lodo)
    checks = fails = 0

    def check(name, ok, extra=''):
        nonlocal checks, fails
        checks += 1
        if ok:
            print('  ok   %s %s' % (name, extra))
        else:
            fails += 1
            print('  FAIL %s %s' % (name, extra))

    # every level-1 cluster, its error, and its own mesh's diagonal
    errs = []
    pct = []
    for ci, cl in enumerate(L['lods']):
        if cl['level'] != 1:
            continue
        m = L['meshes'][L['clusters'][ci]['meshId']]
        diag = math.sqrt(m['ex'] ** 2 + m['ey'] ** 2 + m['ez'] ** 2)
        errs.append(cl['err'])
        if diag > 0:
            pct.append(cl['err'] / diag * 100.0)
    check('L1 the library has level-1 clusters to measure', bool(errs), '(%d)' % len(errs))
    if not errs:
        print('%d checks, %d failures' % (checks, fails))
        print('RESULT FAIL')
        return 1
    errs.sort()
    pct.sort()
    medErr = errs[len(errs) // 2]
    medPct = pct[len(pct) // 2] if pct else 0.0
    # screenErrorPx = err * scale * PS / d = tolerance, at scale 1
    dist = medErr * PROJECTION_SCALE / a.tolerance
    print('level1Clusters %d' % len(errs))
    print('level1MedianError %.4f units' % medErr)
    print('level1MedianPercentOfDiagonal %.4f' % medPct)
    print('level1SelectableAtPx %.1f from %.0f units' % (a.tolerance, dist))
    check('L2 the median level-1 step is selected at %.0f px inside %.0f units'
          % (a.tolerance, a.max_distance), dist <= a.max_distance,
          '(reads %.0f units)' % dist)
    print('%d checks, %d failures' % (checks, fails))
    print('RESULT %s' % ('PASS' if not fails else 'FAIL'))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
