#!/usr/bin/env python3
"""bridge_effect.py -- what the EXACT rule-D bridge does to the body table.

bridge_exact.py showed the exact shore test finds 545 pairs where WATER1's
decimated point clouds found 218, and 340 bodies where WATER1 reported 590.
That is only an improvement if the bodies it produces are the ones a colour
would be attached to.  This prints the twenty biggest under each variant, so
the question "does the sea swallow the painted marshes" is a table, not a
worry.
"""

import collections
import json
import os
import sys

import numpy as np

W1 = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  '..', 'water_20260909'))
sys.path.insert(0, W1)

import ccl                      # noqa: E402
import bridge_exact as BX       # noqa: E402


def group_of(uf, keys):
    g = {}
    for k in keys:
        g.setdefault(uf.find(k), []).append(k)
    return g


def main():
    d, wet, blab, bodies = BX.load_blab()
    info = {int(k): v for k, v in bodies.items()}
    keys = sorted(info)
    pairs = BX.exact_pairs(blab.astype(np.int32), BX.GAP)

    uf = ccl.UF(max(keys) + 2)
    for (a, b) in sorted(pairs):
        A, Bb = info[a], info[b]
        if abs(A['h'] - Bb['h']) > 0.01:
            continue
        if (A['typeIdx'] == Bb['typeIdx']) or A['inherits'] or Bb['inherits']:
            uf.union(a, b)
    grp = group_of(uf, keys)

    rows = []
    for r, parts in grp.items():
        area = sum(info[k]['area'] for k in parts)
        tc = collections.Counter()
        for k in parts:
            tc[info[k]['typeIdx']] += info[k]['area']
        painted = [t for t in tc if t != 0xFFFF]
        ti = max(painted, key=lambda t: tc[t]) if painted else 0xFFFF
        edid = next((info[k]['edid'] for k in parts if info[k]['typeIdx'] == ti),
                    info[parts[0]]['edid'])
        rows.append((area, edid, info[parts[0]]['h'], len(parts),
                     sorted(set(info[k]['edid'] for k in parts))))
    rows.sort(reverse=True)
    print('EXACT bridge: %d bodies' % len(rows))
    print('  %12s %-26s %8s %6s  forms swallowed' % ('area', 'edid', 'height', 'parts'))
    for area, edid, h, n, forms in rows[:15]:
        print('  %12d %-26s %8.1f %6d  %s' % (area, edid[:26], h, n, ', '.join(forms)))

    # how much painted water ends up inside the body whose winning form is the
    # worldspace default -- the number that decides whether this rule destroys
    # the thing bungo asked for
    sea = rows[0]
    print()
    print('the biggest body is %s, %d texels, assembled from %d components '
          'carrying %d distinct forms' % (sea[1], sea[0], sea[3], len(sea[4])))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
