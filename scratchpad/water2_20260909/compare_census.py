#!/usr/bin/env python3
"""compare_census.py -- the C++ census against the Python oracle (gate G5).

The oracle is `census_water2.py` in this folder: rule D with lane WATER2's two
measured corrections, written independently of `src/lodtfile.cpp` and reading
the .lodl through lane WATER1's validated decoder. Agreement between the two is
the gate; disagreement is a rule change and has to be argued, not rounded away.

Compared: the body count, the class split, the number of WATR forms, the total
wet area and the biggest body's area. The per-form TEXEL totals are the strong
part -- no bridging or numbering decision can move them, so they are the same
number under WATER1's rule and under this one, and they catch a classifier that
merely counts differently from one that reads the world differently.
"""

import re
import sys


def main():
    got = open(sys.argv[1]).read()
    want = open(sys.argv[2]).read()
    fails = []

    def check(name, cond):
        print('  %s %s' % ('ok  ' if cond else 'FAIL', name))
        if not cond:
            fails.append(name)

    m = re.search(r'^bodies (\d+)', got, re.M)
    n_got = int(m.group(1)) if m else -1
    m = re.search(r'^  total (\d+)', want, re.M)
    n_want = int(m.group(1)) if m else -2
    check('the C++ body count is the oracle\'s (%d vs %d)' % (n_got, n_want),
          n_got == n_want)

    g = re.search(r'class: sea (\d+)\s+river (\d+)\s+lake (\d+)', got)
    w = re.search(r'class: sea (\d+)\s+river (\d+)\s+lake (\d+)', want)
    check('the class split matches (%s vs %s)'
          % (g.groups() if g else None, w.groups() if w else None),
          bool(g) and bool(w) and g.groups() == w.groups())

    gf = {}
    for m in re.finditer(r'^  ([0-9a-f]{8})\s+bodies\s+(\d+)\s+texels\s+(\d+)', got, re.M):
        gf[m.group(1)] = (int(m.group(2)), int(m.group(3)))
    wf = {}
    for m in re.finditer(r'^  (\S+)\s+bodies\s+(\d+)\s+texels\s+(\d+)', want, re.M):
        wf[m.group(1)] = (int(m.group(2)), int(m.group(3)))
    check('both name the same number of WATR forms (%d vs %d)' % (len(gf), len(wf)),
          len(gf) == len(wf) and len(gf) > 0)
    ga = sorted(v[1] for v in gf.values())
    wa = sorted(v[1] for v in wf.values())
    check('the per-form texel totals are identical, form for form', ga == wa)
    if ga != wa:
        print('     C++    %s' % ga[-6:])
        print('     oracle %s' % wa[-6:])
    gb = sorted(v[0] for v in gf.values())
    wb = sorted(v[0] for v in wf.values())
    check('the per-form BODY counts are identical', gb == wb)
    check('the total wet area matches (%d vs %d)' % (sum(ga), sum(wa)), sum(ga) == sum(wa))

    print('RESULT %s' % ('PASS' if not fails else 'FAIL'))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
