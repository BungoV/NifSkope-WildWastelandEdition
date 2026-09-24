"""AUDIT1 step 5: verify the plan §5 / census §6.3 rows that claim DONE, by
reading the fields off a real pair rather than quoting the page that claims them.

A row that says DONE and whose field is zero on every bake is not DONE, it is a
field nobody has looked at. Each number below is read with the tree's own
independent decoder.

usage: python step5_probe.py <bake tree> [more trees...]
"""
import glob
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SPELLS = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'tests', 'spells')
sys.path.insert(0, SPELLS)
import lodgen_native_decode as D                        # noqa: E402

MESH_WATERTIGHT = 4


def one(tree):
    o = glob.glob(os.path.join(tree, 'FO4CSLOD', '*', '*.lodo'))
    i = glob.glob(os.path.join(tree, 'FO4CSLOD', '*', '*.lodi'))
    if not o or not i:
        print('%-16s no pair' % os.path.basename(tree))
        return
    lodo = D.read_lodo(o[0])
    lodi = D.read_lodi(i[0])
    ho, hi = lodo['header'], lodi['header']

    # row 1 / census 1: slotInstances[4], v5
    slots = hi.get('slotInstances', [0, 0, 0, 0])
    moved = sum(1 for s in slots if s)

    # row 2 / census 2: fullTriangles per base, .lodo v4
    bases = lodo.get('bases', [])
    ft = [b.get('fullTriangles', 0) for b in bases]
    nz = sum(1 for v in ft if v)

    # row 3 / census 3: cardCount at 0xD0, and the rows that carry a layer
    layers = sum(1 for b in bases if b.get('cardLayer', 0xFFFF) != 0xFFFF)

    # row 5: LODO_MESH_WATERTIGHT on the mesh rows
    meshes = lodo.get('meshes', [])
    wt = sum(1 for m in meshes if m.get('flags', 0) & MESH_WATERTIGHT)

    print('%-16s v%d/%d  inst %-7d slots %s (%d of 4 non-zero)'
          % (os.path.basename(tree), ho['version'], hi['version'],
             hi['instanceCount'], slots, moved))
    print('%-16s   bases %-6d fullTriangles non-zero %-6d distinct %-5d max %d'
          % ('', len(bases), nz, len(set(v for v in ft if v)),
             max(ft) if ft else 0))
    print('%-16s   cardCount %-5d bases with a cardLayer %-5d '
          'cardCorpusHash %016x'
          % ('', ho.get('cardCount', -1), layers, ho.get('cardCorpusHash', 0)))
    print('%-16s   meshes %-6d watertight %-6d  occluders %d'
          % ('', len(meshes), wt, hi.get('occluderCount', -1)))
    print('%-16s   slot sum %d == instanceCount %s'
          % ('', sum(slots), 'YES' if sum(slots) == hi['instanceCount']
             else 'NO'))


for t in sys.argv[1:]:
    one(t)
