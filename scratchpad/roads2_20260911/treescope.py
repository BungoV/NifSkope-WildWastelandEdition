"""Work item 5's gate, re-grounded after an audit showed the brief's own gate
could not fail.

THE BRIEF'S GATE IS VACUOUS, and the rung proves it: the four `SetDressing\\`
false positives are absent from `--list-impostor-candidates --candidates trees`
on the OLD exe too, on Sanctuary (20 lines, none of them set dressing) and over
the whole Commonwealth (36 lines, none of them set dressing).  The reason is in
`nifcli.cpp`'s lister: `if ( !b.hasLod ) return;` runs BEFORE the tree test, and
every one of the seven `Tree*` props outside the landscape set has `MNAM` absent
and header bit 15 clear.  So no candidate list, on any exe, could ever have
shown them.

What can be measured instead, and what this script prints:

1. every placed base in the Commonwealth whose model file starts with `tree`,
   with its record signature, its distant-LOD state, its placement count, and
   how the OLD and the NEW rule classify it -- the table, not the count
   (ww-spec-gate-audit part 3);
2. the invariant that must NOT move: every base under a `trees` folder or under
   `Landscape\\`, which is what the clause exists to catch;
3. the consequence of the flip, stated honestly: with `hasLod` false on all
   seven props, the change moves nothing that ships today -- which is also why
   the byte-identity gate over the Sanctuary region stayed green with the tree
   clause changed in the same build.

Both rules are typed from `src/lodgen.cpp:2031` (read, not guessed); the exe's
own answer is checked independently through the candidate lister, whose 36 lines
and 20 lines are identical on both exes.

usage: treescope.py <census.json>
"""

import json
import sys
from collections import Counter

BS = chr(92)


def comps(modl):
    p = modl.replace(BS, '/').lower()
    return [x for x in p.split('/') if x]


def in_trees_folder(modl):
    p = modl.replace(BS, '/').lower()
    return '/trees/' in p or p.startswith('trees/')


def file_of(modl):
    return modl.replace(BS, '/').lower().rsplit('/', 1)[-1]


def old_rule(modl):
    """lodgenIsTreeModel as the rung shipped it: a trees folder anywhere, or a
    file name starting with `tree`, unscoped."""
    if in_trees_folder(modl):
        return True
    return file_of(modl).startswith('tree')


def new_rule(modl):
    """The shipped rule after this lane: the filename clause only under
    `Landscape\\`, a leading `meshes` and a leading `lod` dropped first."""
    if in_trees_folder(modl):
        return True
    if not file_of(modl).startswith('tree'):
        return False
    c = comps(modl)
    i = 0
    if i < len(c) and c[i] == 'meshes':
        i += 1
    if i < len(c) and c[i] == 'lod':
        i += 1
    return i + 1 < len(c) and c[i] == 'landscape'


def main(argv):
    cen = json.load(open(argv[0]))
    rows = cen['rows']

    print('placed bases in the census: %d' % len(rows))
    t = Counter()
    flips = []
    for r in rows:
        m = r['modl']
        if not m:
            continue
        o, n = old_rule(m), new_rule(m)
        t[(o, n)] += 1
        if o != n:
            flips.append(r)

    print('')
    print('classification of every placed base')
    print('   old tree / new tree : %d' % t[(True, True)])
    print('   old tree / new NOT  : %d   <- the flip' % t[(True, False)])
    print('   old NOT  / new tree : %d   (must be zero: the clause only narrows)'
          % t[(False, True)])
    print('   neither             : %d' % t[(False, False)])

    print('')
    print('THE FLIP, every row (ww-spec-gate-audit: the table, not the count)')
    print('%-8s %-5s %-7s %-5s %-7s %-8s  %s'
          % ('formid', 'sig', 'bit15', 'mnam', 'direct', 'viaSCOL', 'model'))
    for r in sorted(flips, key=lambda r: -r['direct']):
        print('%-8s %-5s %-7s %-5d %-7d %-8d  %s'
              % (r['formid'], r['sig'], bool(r['flags'] & 0x8000), r['mnam'],
                 r['direct'], r['viascol'], r['modl']))
    print('   placements affected: %d direct + %d viaSCOL'
          % (sum(r['direct'] for r in flips), sum(r['viascol'] for r in flips)))
    print('   of the flipped bases, how many carry a distant LOD mesh: %d'
          % sum(1 for r in flips if r['mnam'] > 0 or (r['flags'] & 0x8000)))

    print('')
    print('THE INVARIANT THAT MUST NOT MOVE: bases the clause exists for')
    keep = [r for r in rows if r['modl'] and new_rule(r['modl'])]
    byfolder = Counter()
    for r in keep:
        c = comps(r['modl'])
        i = 1 if c and c[0] == 'meshes' else 0
        if i < len(c) and c[i] == 'lod':
            i += 1
        byfolder['/'.join(c[i:i + 2])] += 1
    for k, v in sorted(byfolder.items(), key=lambda kv: -kv[1]):
        print('   %-28s %d bases' % (k, v))
    print('   total kept: %d bases, %d direct placements'
          % (len(keep), sum(r['direct'] for r in keep)))
    withlod = [r for r in keep if r['mnam'] > 0 or (r['flags'] & 0x8000)]
    print('   of those, carrying a distant LOD mesh (the only ones any consumer'
          ' of the test can reach): %d bases' % len(withlod))


if __name__ == '__main__':
    main(sys.argv[1:])
