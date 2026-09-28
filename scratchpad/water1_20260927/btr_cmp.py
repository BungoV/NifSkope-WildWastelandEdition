#!/usr/bin/env python3
"""WATER1 follow-up 2 gate: a chunk bake before and after the water depth bake was dropped.

  btr_cmp.py <old dir> <new dir> [--floor]

Per .btr pair (same relative path), read with tests/spells/gltf_nifread.py:
  * every block that is NOT a water shape must be byte-identical (land shapes carry the terrain
    identity: class in R, wetness in G -- so an identity-ON pair proves those bytes did not move);
  * every water shape (a shape whose old vertex desc has VF_COLORS and no other colour user, i.e.
    the BSSubIndexTriShape named for water) must keep its vertex positions and triangles exactly,
    and the new one must carry no VF_COLORS and an 8-byte vertex;
  * the file sets and block type lists must match.
--floor flips one byte inside the first land block of the first new file in memory: the gate must FAIL.
"""
import glob
import os
import sys

sys.path.insert(0, 'E:/Projects/NifskopeWWE-water1/tests/spells')
import gltf_nifread as G  # noqa: E402


def is_water(sh, nif=None):
    # AMENDED 2026-09-28 21:5x after the first run found 0 water shapes: the generator's water shapes are
    # UNNAMED; they hang under a NiNode named "WATER". Name OR parent-node name, as the docstring meant.
    if 'water' in (sh['name'] or '').lower():
        return True
    par = nif.nodes.get(sh.get('parent')) if nif is not None else None
    return bool(par) and 'water' in (par['name'] or '').lower()


def main():
    old, new = sys.argv[1], sys.argv[2]
    floor = '--floor' in sys.argv
    rel = lambda d: sorted(os.path.relpath(p, d) for p in glob.glob(os.path.join(d, '**', '*.btr'), recursive=True))
    ro, rn = rel(old), rel(new)
    fails = []
    if ro != rn:
        fails.append('file sets differ: %d old, %d new' % (len(ro), len(rn)))
    blocks = same = water = 0
    flipped = False
    for r in [x for x in ro if x in rn]:
        a, b = G.Nif(os.path.join(old, r)), G.Nif(os.path.join(new, r))
        if floor and not flipped:
            for i in range(b.numBlocks):
                if i not in b.shapes or not is_water(b.shapes[i], b):
                    k = b.start[i] + b.size[i] // 2
                    b.data = b.data[:k] + bytes([b.data[k] ^ 0x01]) + b.data[k + 1:]
                    flipped = True
                    break
        if [a.type[i] for i in range(a.numBlocks)] != [b.type[i] for i in range(b.numBlocks)]:
            fails.append('%s: block type lists differ' % r)
            continue
        for i in range(a.numBlocks):
            blocks += 1
            if i in a.shapes and is_water(a.shapes[i], a):
                water += 1
                sa, sb = a.shapes[i], b.shapes[i]
                if sa['verts'] != sb['verts'] or sa['tris'] != sb['tris']:
                    fails.append('%s block %d: water positions or triangles moved' % (r, i))
                if sb['va'] & G.VA_COLORS or sb['stride'] != 8:
                    fails.append('%s block %d: new water shape still has colours / stride %d' % (r, i, sb['stride']))
                continue
            if a.data[a.start[i]:a.start[i] + a.size[i]] == b.data[b.start[i]:b.start[i] + b.size[i]]:
                same += 1
            else:
                fails.append('%s block %d (%s) differs' % (r, i, a.type[i]))
    print('%d files, %d blocks: %d non-water blocks byte-identical, %d water shapes compared%s'
          % (len(ro), blocks, same, water, ' (FLOOR: one byte flipped)' if floor else ''))
    for f in fails[:10]:
        print('  FAIL', f)
    if not ro or not water:
        print('REFUSED: nothing to compare')
        sys.exit(2)
    print('FAIL' if fails else 'PASS')
    sys.exit(1 if fails else 0)


main()
