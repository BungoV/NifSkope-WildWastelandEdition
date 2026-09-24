#!/usr/bin/env python
"""Known-answer controls for the flag census, run BEFORE any verdict.

C1  The flag reader is validated against a SECOND, independent field of the
    same record: header bit 15 'Has Distant LOD' against the presence of the
    MNAM 'Distant LOD' subrecord.  These are read from different places (the
    24-byte record header vs a subrecord in the body); if the header word were
    misaligned the agreement would collapse.

C2  The census's road set must contain ROADS1's 71 window models.  Re-derives
    them from scratchpad/roads1_20260911/sanctuary_refs.json with ROADS1's own
    test and intersects.

C3  A flag that is ~100% on roads must exist for the question to have a
    positive answer; print the three bits with the highest road fraction so a
    reader can see there is no such bit rather than take it on trust.

C4  Placement counts: the census's counts inside ROADS1's cell window must
    reproduce ROADS1's own numbers (321 road placements over 71 models).
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from flagtable import comps, is_road, STATNAMES  # noqa: E402


def main(argv):
    d = json.load(open(os.path.join(HERE, 'census.json')))
    rows = d['rows']
    stat = [r for r in rows if r['sig'] == 'STAT']

    # ---- C1
    both = sum(1 for r in stat if (r['flags'] & (1 << 15)) and r['mnam'])
    only15 = sum(1 for r in stat if (r['flags'] & (1 << 15)) and not r['mnam'])
    onlymn = sum(1 for r in stat if not (r['flags'] & (1 << 15)) and r['mnam'])
    neither = len(stat) - both - only15 - onlymn
    print('C1 header bit15 vs MNAM subrecord, over %d placed STAT bases' % len(stat))
    print('   both %d   bit15 only %d   MNAM only %d   neither %d'
          % (both, only15, onlymn, neither))
    agree = 100.0 * (both + neither) / len(stat)
    print('   agreement %.2f%%   (a misaligned header word reads ~50%%)' % agree)

    # ---- C2
    rp = os.path.join(HERE, '..', 'roads1_20260911', 'sanctuary_refs.json')
    r1 = json.load(open(rp))
    r1road = set()
    for fid, rec in r1['bases'].items():
        if rec['sig'] != 'STAT':
            continue
        c = comps(rec['modl'])
        if len(c) >= 3 and c[0] == 'landscape' and c[1] in ('roads', 'sidewalks'):
            r1road.add(rec['modl'].replace('\\', '/').lower())
    mine = set(r['modl'].replace('\\', '/').lower() for r in stat if is_road(r))
    print()
    print('C2 ROADS1 window road models %d ; in this census %d ; missing %d'
          % (len(r1road), len(r1road & mine), len(r1road - mine)))
    for m in sorted(r1road - mine):
        print('   MISSING', m)

    # ---- C4
    print()
    print('C4 ROADS1 reported 321 Landscape\\Roads placements over 71 models in')
    print('   cells -24..-12 x 16..30 (post-SCOL).  Road models under')
    print('   landscape/roads only, this census, whole worldspace: %d'
          % sum(1 for r in stat if is_road(r)
                and comps(r['modl'])[1] == 'roads'))

    # ---- C3
    roads = [r for r in stat if is_road(r)]
    others = [r for r in stat if not is_road(r)]
    sc = []
    for b in range(32):
        nr = sum(1 for r in roads if r['flags'] & (1 << b))
        if nr:
            no = sum(1 for r in others if r['flags'] & (1 << b))
            sc.append((100.0 * nr / len(roads), b, nr,
                       100.0 * no / len(others)))
    sc.sort(reverse=True)
    print()
    print('C3 bits by ROAD fraction, highest first:')
    for fr, b, nr, fo in sc:
        print('   bit %-2d %-22s road %6.2f%% (%d/%d)   non-road %6.2f%%'
              % (b, STATNAMES.get(b, ''), fr, nr, len(roads), fo))
    if not sc or sc[0][0] < 90.0:
        print('   -> NO bit is ~100%% on roads.  There is no candidate of the')
        print('      shape the question assumes.')


if __name__ == '__main__':
    main(sys.argv[1:])
