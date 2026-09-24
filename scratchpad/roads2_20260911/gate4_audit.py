"""ww-spec-gate-audit on FLAGSCAN1's "0.716 vs 0.629" before reproducing it.

Three checks the skill asks for:

1. READ THE SOURCE THAT PRODUCED THE NUMBER.  Both values come from one line of
   scratchpad/flagscan1_20260911/hw_m8_8.txt, written by hw_tile.py: the
   road_ground row's AUCgrey (0.716) and the TOP of that row's displaced-floor
   band (0.629).  The same row's AUCbr is also 0.629, so the pair as written in
   the brief is ambiguous on its face; this script prints the row so the
   comparison is unambiguous.

2. IS THE APPROXIMATION ONE-SIDED?  hw_tile.py splits road bases into elevated
   and ground by ONE clause -- the third path component is highwayoverpass or
   bridge -- while the shipped C++ refuses a base when `hasLod` is true OR the
   folder clause fires.  Those are different DOMAINS (the NATIVE1b lesson), so
   the two halves of the gate are not about the same sets of texels unless the
   clauses agree on the bases this chunk actually places.  This script measures
   the disagreement per base.

3. PRINT THE TABLE, NOT THE COUNT.  Every base the two rules disagree about is
   listed with its placement count on the tile.

usage: gate4_audit.py <census.json> <refs.json> <cx> <cy>
"""

import json
import os
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
FS = os.path.join(HERE, '..', 'flagscan1_20260911')
sys.path.insert(0, FS)
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))

from flagtable import comps, is_road                          # noqa: E402
from placements import load                                   # noqa: E402

HASLOD_BIT = 1 << 15


def raised_folder(modl):
    """The shipped lodgenIsRaisedRoadModel(), re-typed: component equality,
    optional leading `meshes`, then landscape / roads / (highwayoverpass|bridge)
    and at least one component after it."""
    c = comps(modl)
    return (len(c) >= 4 and c[0] == 'landscape' and c[1] == 'roads'
            and c[2] in ('highwayoverpass', 'bridge'))


def hw_elev(modl):
    """hw_tile.py's own split: the third component, whatever the folder depth."""
    c = comps(modl)
    sub = c[2] if len(c) > 2 else ''
    return sub in ('highwayoverpass', 'bridge')


def main(argv):
    cen = json.load(open(argv[0]))
    refs = argv[1]
    cx0, cy0 = int(argv[2]), int(argv[3])

    rows = {int(r['formid'], 16): r for r in cen['rows'] if r['sig'] == 'STAT'}
    roads = {f: r for f, r in rows.items() if is_road(r)}

    pl = load(refs)
    cnt = Counter(p['base'] for p in pl)

    print('CHECK 1 -- the line the two numbers come from')
    txt = os.path.join(FS, 'hw_m8_8.txt')
    for ln in open(txt):
        if 'AUC' in ln:
            print('   ' + ln.rstrip())
    print('   the gate reads road_ground AUCgrey 0.716 against that row\'s')
    print('   displaced-floor top 0.629; the same row\'s AUCbr is also 0.629.')

    print('')
    print('CHECK 2 -- do the two exclusion clauses cover the same bases?')
    print('%-8s %-6s %-6s %-6s %-6s  %s'
          % ('formid', 'plc', 'hasLod', 'mnam', 'folder', 'model'))
    dis = 0
    tot = Counter()
    for f, r in sorted(roads.items(), key=lambda kv: -cnt.get(kv[0], 0)):
        n = cnt.get(f, 0)
        if n == 0:
            continue
        hl = bool(r['flags'] & HASLOD_BIT)
        fol = raised_folder(r['modl'])
        he = hw_elev(r['modl'])
        cpp = hl or fol
        tot['bases'] += 1
        tot['plc'] += n
        if cpp:
            tot['cpp_refused_bases'] += 1
            tot['cpp_refused_plc'] += n
        if he:
            tot['hw_elev_bases'] += 1
            tot['hw_elev_plc'] += n
        if cpp != he:
            dis += 1
            tot['dis_plc'] += n
            print('%-8s %-6d %-6s %-6d %-6s  %s'
                  % (r['formid'], n, hl, r['mnam'], fol, r['modl']))
    if dis == 0:
        print('   (none -- the two clauses agree on every road base placed here)')
    print('')
    print('road bases placed on (%d,%d): %d  placements %d'
          % (cx0, cy0, tot['bases'], tot['plc']))
    print('   refused by the shipped C++ rule (hasLod OR folder): %d bases, %d placements'
          % (tot['cpp_refused_bases'], tot['cpp_refused_plc']))
    print('   called elevated by hw_tile.py (folder only):        %d bases, %d placements'
          % (tot['hw_elev_bases'], tot['hw_elev_plc']))
    print('   bases the two disagree about: %d (%d placements)' % (dis, tot['dis_plc']))

    print('')
    print('CHECK 3 -- mnam vs bit 15 on every road base placed here')
    t = Counter()
    for f, r in roads.items():
        if cnt.get(f, 0) == 0:
            continue
        t[(bool(r['flags'] & HASLOD_BIT), r['mnam'] > 0)] += 1
    for k in sorted(t):
        print('   bit15=%-5s mnam>0=%-5s  %d bases' % (k[0], k[1], t[k]))


if __name__ == '__main__':
    main(sys.argv[1:])
