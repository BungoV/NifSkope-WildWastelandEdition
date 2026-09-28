#!/usr/bin/env python3
"""WATER1: where the whole-map sea ring comes from. Cells whose ground is flat
at the file's no-land fallback (-352) vs cells with relief, and on which side
of the land the fallback cells touch it.
    python searing.py <file.lodl>
"""
import os
import sys
from collections import Counter

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tests', 'spells'))
from lodl_open_authority import Lodt  # noqa: E402

d = Lodt(sys.argv[1])
fb, relief = set(), set()
for cy in range(d.minY, d.maxY + 1):
    for cx in range(d.minX, d.maxX + 1):
        lo, hi, wh, wt, fl = d.cell(cx, cy)
        (fb if (hi - lo < 1 and abs(lo + 352) < 0.5) else relief).add((cx, cy))
xs = [c[0] for c in relief]
ys = [c[1] for c in relief]
print('cells not flat at -352: %d, span x %d..%d y %d..%d; flat -352 cells %d'
      % (len(relief), min(xs), max(xs), min(ys), max(ys), len(fb)))
side, cliff, low = Counter(), Counter(), Counter()
for (x, y) in fb:
    for dx, dy, s in ((1, 0, 'W'), (-1, 0, 'E'), (0, 1, 'S'), (0, -1, 'N')):
        n = (x + dx, y + dy)
        if n in relief:
            side[s] += 1
            if d.cell(*n)[1] > 450:
                cliff[s] += 1
            if d.cell(*n)[0] < 450:
                low[s] += 1
print('fallback cells touching land, by the side of the land they lie on: %s' % dict(side))
print('  the land cell they touch rises above 450: %s' % dict(cliff))
print('  the land cell they touch dips below 450 (water can meet a shore): %s' % dict(low))
