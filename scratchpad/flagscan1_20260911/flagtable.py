#!/usr/bin/env python
"""Section 1: the flag table.  Road STAT bases vs every other STAT base.

The road test is ROADS1's, re-typed (component equality, never a substring):
model path separators normalised, lowercased, a leading 'meshes' component
dropped; first component 'landscape', second 'roads' or 'sidewalks'.

  python flagtable.py census.json
"""

import json
import sys

STATNAMES = {
    2: 'Heading Marker', 4: 'Non Occluder', 6: 'Has Tree LOD',
    7: 'Add-On LOD Object', 9: 'Hidden From Local Map',
    10: 'Headtrack Marker', 11: 'Used as Platform', 13: 'Pack-In Use Only',
    15: 'Has Distant LOD', 17: 'Uses HD LOD Texture', 19: 'Has Currents',
    23: 'Is Marker', 25: 'Obstacle', 26: 'NavMesh - Filter',
    27: 'NavMesh - Bounding Box', 28: 'Show In World Map', 30: 'NavMesh - Ground',
    5: '(deleted)', 18: '(compressed)',
}


def comps(modl):
    p = modl.replace('\\', '/').lower().strip('/')
    c = [x for x in p.split('/') if x]
    if c and c[0] == 'meshes':
        c = c[1:]
    return c


def is_road(rec):
    if rec['sig'] != 'STAT':
        return False
    c = comps(rec['modl'])
    return len(c) >= 3 and c[0] == 'landscape' and c[1] in ('roads', 'sidewalks')


def main(argv):
    d = json.load(open(argv[0]))
    rows = [r for r in d['rows'] if r['sig'] == 'STAT']
    roads = [r for r in rows if is_road(r)]
    others = [r for r in rows if not is_road(r)]
    print('placed STAT bases: %d  (road %d, non-road %d)'
          % (len(rows), len(roads), len(others)))
    print('placements: road direct %d + viaSCOL %d ; non-road direct %d + viaSCOL %d'
          % (sum(r['direct'] for r in roads), sum(r['viascol'] for r in roads),
             sum(r['direct'] for r in others), sum(r['viascol'] for r in others)))
    print()
    print('%-4s %-24s %10s %10s %12s  %s' %
          ('bit', 'name', 'road %', 'other %', 'other n', 'lift'))
    print('-' * 84)
    for b in range(32):
        nr = sum(1 for r in roads if r['flags'] & (1 << b))
        no = sum(1 for r in others if r['flags'] & (1 << b))
        if nr == 0 and no == 0:
            continue
        fr = 100.0 * nr / max(1, len(roads))
        fo = 100.0 * no / max(1, len(others))
        lift = ('%.1fx' % (fr / fo)) if fo > 0 else ('inf' if fr > 0 else '-')
        print('%-4d %-24s %9.1f%% %9.1f%% %12d  %s'
              % (b, STATNAMES.get(b, ''), fr, fo, no, lift))
    print()
    # weighted by placements too -- a flag on one base placed 40,000 times is
    # not the same evidence as a flag on 400 bases placed once.
    rp = sum(r['direct'] + r['viascol'] for r in roads)
    op = sum(r['direct'] + r['viascol'] for r in others)
    print('by PLACEMENT (direct+viaSCOL): road total %d, other total %d' % (rp, op))
    print('%-4s %-24s %10s %10s' % ('bit', 'name', 'road %', 'other %'))
    print('-' * 54)
    for b in range(32):
        nr = sum(r['direct'] + r['viascol'] for r in roads if r['flags'] & (1 << b))
        no = sum(r['direct'] + r['viascol'] for r in others if r['flags'] & (1 << b))
        if nr == 0 and no == 0:
            continue
        print('%-4d %-24s %9.1f%% %9.1f%%'
              % (b, STATNAMES.get(b, ''), 100.0 * nr / max(1, rp),
                 100.0 * no / max(1, op)))
    print()
    # MNAM (Distant LOD mesh list)
    for nm, s in (('road', roads), ('non-road', others)):
        n = sum(1 for r in s if r['mnam'])
        print('MNAM present on %-9s: %d / %d  (%.1f%%)'
              % (nm, n, len(s), 100.0 * n / max(1, len(s))))
    print()
    # REFR header flags over placements
    print('REFR header flags, fraction of DIRECT placements')
    print('%-4s %-28s %10s %10s' % ('bit', 'name(ACTI/STAT/SCOL/TREE)', 'road %', 'other %'))
    print('-' * 58)
    rn = {8: 'LOD Respects Enable State', 9: 'Hidden From Local Map',
          10: 'Persistent', 11: 'Initially Disabled', 15: 'Visible When Distant',
          16: 'Is Full LOD', 5: 'Deleted', 17: 'Unknown17'}
    rd = sum(r['direct'] for r in roads)
    od = sum(r['direct'] for r in others)
    for b in range(32):
        nr = sum(r['refrflags'][b] for r in roads)
        no = sum(r['refrflags'][b] for r in others)
        if nr == 0 and no == 0:
            continue
        print('%-4d %-28s %9.2f%% %9.2f%%'
              % (b, rn.get(b, ''), 100.0 * nr / max(1, rd), 100.0 * no / max(1, od)))


if __name__ == '__main__':
    main(sys.argv[1:])
