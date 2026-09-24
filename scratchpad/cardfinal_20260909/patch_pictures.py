#!/usr/bin/env python3
"""Pick the frame pair the DEFECT is on, not the pair whose silhouettes are
closest at mip 0.

The first pass chose the tightest pair in the NEW library, and on TreeHero01 that
pair carried only 6/255 of border alpha in the old one while the sheet's worst is
56/255 -- so the picture understated the very thing it exists to show. The pair
is now the one whose border carries the MOST neighbour alpha at the BEFORE
library's deepest shipped mip, falling back to the tightest pair when the sheet
never bled at all.
"""
import sys

P = 'scratchpad/cardfinal_20260909/make_pictures.py'
s = open(P, encoding='utf-8').read()


def rep(old, new):
    global s
    n = s.count(old)
    if n != 1:
        print('anchor matched %d times: %r' % (n, old[:70]))
        sys.exit(1)
    s = s.replace(old, new)


rep('''def pick_pair(a, N, fw, fh):
    """the horizontally adjacent frame pair whose silhouettes come closest"""''',
    '''def pick_worst(a, N, fw, fh, k):
    """the horizontally adjacent frame pair whose shared border carries the most
    neighbour alpha at mip k -- the defect the mip law removes, so the picture is
    of the place it happens rather than of a place it does not"""
    cur = a
    for _ in range(k):
        cur = boxdown(cur)
    fwk, fhk = max(1, fw >> k), max(1, fh >> k)
    best, at = -1, None
    for j in range(N):
        for i in range(N - 1):
            x = (i + 1) * fwk
            col = cur[j * fhk:(j + 1) * fhk, x - 1:x + 1, 3]
            v = int(col.max()) if col.size else 0
            if v > best:
                best, at = v, (i, j)
    return at, best


def pick_pair(a, N, fw, fh):
    """the horizontally adjacent frame pair whose silhouettes come closest"""''')

rep('''        (i, j), run = pick_pair(aA, N, scA['fw'], scA['fh'])''',
    '''        # the pair the BEFORE library bleeds worst on, at its own deepest shipped
        # mip; if it never bleeds, the tightest pair in the new library
        (i, j), worstB = pick_worst(aB, scB['oct'], scB['fw'], scB['fh'], mB - 1)
        if worstB <= 0:
            (i, j), _run = pick_pair(aA, N, scA['fw'], scA['fh'])''')

open(P, 'w', encoding='utf-8', newline='').write(s)
print('written')
