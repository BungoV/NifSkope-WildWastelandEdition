#!/usr/bin/env python
"""Sections 2 and 4.

2  The best candidate bit (bit 15 'Has Distant LOD' -- the only bit with any
   material presence on roads at all) listed out: which ROAD bases carry it,
   and what the NON-road bases that carry it are, by top-level model folder
   and by placement count.
   Also: the 71 window models ROADS1 named, each with its flags, so a reader
   can see the bake set itself is not flagged.

4  The tree-flag agreement: bit 6 'Has Tree LOD' and bit 15 'Has Distant LOD'
   on the bases lodgenIsTreeModel() calls trees vs the ones it does not.
   lodgenIsTreeModel (src/lodgen.cpp:2031, read only) is re-typed here:
      model contains '\\trees\\' or '/trees/', case-insensitive, OR the file
      name starts with 'tree'.
"""

import collections
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from flagtable import comps, is_road, STATNAMES  # noqa: E402

BIT = 15


def is_tree_model(modl):
    """src/lodgen.cpp:2031 lodgenIsTreeModel, re-typed."""
    m = (modl or '').lower()
    f = m.replace('\\', '/').rsplit('/', 1)[-1]
    return ('\\trees\\' in m) or ('/trees/' in m) or f.startswith('tree')


def folder(r):
    c = comps(r['modl'])
    return '/'.join(c[:2]) if len(c) >= 2 else (c[0] if c else '(no model)')


def main(argv):
    d = json.load(open(os.path.join(HERE, 'census.json')))
    stat = [r for r in d['rows'] if r['sig'] == 'STAT']
    roads = [r for r in stat if is_road(r)]
    others = [r for r in stat if not is_road(r)]

    print('=== 2a  ROAD bases carrying bit %d (%s): %d of %d'
          % (BIT, STATNAMES[BIT], sum(1 for r in roads if r['flags'] & (1 << BIT)),
             len(roads)))
    for r in sorted((r for r in roads if r['flags'] & (1 << BIT)),
                    key=lambda r: -(r['direct'] + r['viascol']))[:40]:
        print('   %-34s %-52s %5d  MNAM=%s'
              % (r['edid'][:34], r['modl'][:52], r['direct'] + r['viascol'],
                 r['mnam0'][-40:] or '-'))
    print()
    print('    the same, NOT carrying it, top 12 by placements:')
    for r in sorted((r for r in roads if not (r['flags'] & (1 << BIT))),
                    key=lambda r: -(r['direct'] + r['viascol']))[:12]:
        print('   %-34s %-52s %5d'
              % (r['edid'][:34], r['modl'][:52], r['direct'] + r['viascol']))

    print()
    fl = [r for r in others if r['flags'] & (1 << BIT)]
    print('=== 2b  NON-road bases carrying bit %d: %d bases, %d placements'
          % (BIT, len(fl), sum(r['direct'] + r['viascol'] for r in fl)))
    c = collections.Counter()
    cp = collections.Counter()
    for r in fl:
        c[folder(r)] += 1
        cp[folder(r)] += r['direct'] + r['viascol']
    print('    by model folder (top 30 by placements):')
    for k, v in cp.most_common(30):
        print('      %-40s %5d bases %8d placements' % (k, c[k], v))
    print()
    print('    top 25 individual bases by placements:')
    for r in sorted(fl, key=lambda r: -(r['direct'] + r['viascol']))[:25]:
        print('      %-38s %-46s %6d'
              % (r['edid'][:38], r['modl'][:46], r['direct'] + r['viascol']))

    # --- the 71 window models
    print()
    print('=== 2c  the road models ROADS1 projected into chunk (-20,20), flags')
    rp = os.path.join(HERE, '..', 'roads1_20260911', 'sanctuary_refs.json')
    r1 = json.load(open(rp))
    want = set()
    for fid, rec in r1['bases'].items():
        if rec['sig'] != 'STAT':
            continue
        cc = comps(rec['modl'])
        if len(cc) >= 3 and cc[0] == 'landscape' and cc[1] == 'roads':
            want.add(rec['modl'].replace('\\', '/').lower())
    sel = [r for r in roads if r['modl'].replace('\\', '/').lower() in want]
    nz = [r for r in sel if r['flags']]
    print('    %d of the %d window road models found in the census; %d have ANY'
          ' header flag set at all' % (len(sel), len(want), len(nz)))
    for r in nz:
        bits = [b for b in range(32) if r['flags'] & (1 << b)]
        print('      %-40s flags 0x%08X %s' % (r['edid'][:40], r['flags'],
                                               [STATNAMES.get(b, b) for b in bits]))
    if not nz:
        print('      (none -- every one of them has flags 0x00000000)')

    # --- section 4
    print()
    print('=== 4  tree flags against lodgenIsTreeModel()')
    tr = [r for r in stat if is_tree_model(r['modl'])]
    nt = [r for r in stat if not is_tree_model(r['modl'])]
    print('    bases the heuristic calls a tree: %d (%d placements)'
          % (len(tr), sum(r['direct'] + r['viascol'] for r in tr)))
    print('    bases it does not:                %d (%d placements)'
          % (len(nt), sum(r['direct'] + r['viascol'] for r in nt)))
    for b in (6, 15):
        a = sum(1 for r in tr if r['flags'] & (1 << b))
        z = sum(1 for r in nt if r['flags'] & (1 << b))
        print('    bit %-2d %-18s tree-set %5.1f%% (%d/%d)   non-tree %5.1f%% (%d/%d)'
              % (b, STATNAMES[b], 100.0 * a / max(1, len(tr)), a, len(tr),
                 100.0 * z / max(1, len(nt)), z, len(nt)))
    # TREE record type, for comparison: the generator also treats sig TREE as a tree
    tree_sig = [r for r in d['rows'] if r['sig'] == 'TREE']
    print('    placed TREE-signature bases: %d' % len(tree_sig))
    for b in (6, 15):
        a = sum(1 for r in tree_sig if r['flags'] & (1 << b))
        print('       bit %-2d on TREE records: %d/%d' % (b, a, len(tree_sig)))
    # what the heuristic's tree set actually looks like
    print('    tree-set model folders (top 15 by bases):')
    cc = collections.Counter(folder(r) for r in tr)
    for k, v in cc.most_common(15):
        print('      %-40s %d' % (k, v))
    # where the heuristic might be wrong: non-'trees' folders it swept in
    odd = [r for r in tr if 'trees' not in folder(r)]
    print('    tree-set members NOT under a */trees folder: %d' % len(odd))
    for r in sorted(odd, key=lambda r: -(r['direct'] + r['viascol']))[:20]:
        print('      %-38s %-46s %6d' % (r['edid'][:38], r['modl'][:46],
                                         r['direct'] + r['viascol']))


if __name__ == '__main__':
    main(sys.argv[1:])
