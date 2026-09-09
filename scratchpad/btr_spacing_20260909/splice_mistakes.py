import io

P = 'E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md'
b = open(P, 'rb').read()
before_cr, before_lf = b.count(b'\r'), b.count(b'\n')
anchor = b'Newest at the top.\n\n'
assert b.count(anchor) == 1, b.count(anchor)

entry = b'''## 2026-09-09 -- lane BTRSPACING (terrain LOD vertex spacing), two entries

1. **Took the minimum gap between sorted unique coordinates as a grid step.**
   The first pass reported `Commonwealth.4.0.0.BTR` as a "13129 x 13129 grid at
   step 1.248 units, 0.0% occupied" -- 172 million grid points for a
   4215-triangle mesh. A vanilla terrain LOD chunk is an ADAPTIVE
   triangulation, so the smallest gap anywhere in it is not a step and the
   derived grid is meaningless. Found by disbelieving the absurd occupancy
   figure and re-deriving spacing as `4096 / sqrt(columns per cell)` instead.
   Rule: **on an irregular mesh, resolution is columns over area; a gap
   histogram describes the mesh's SHAPE, never its resolution** -- and a
   derived figure implying an impossible magnitude is a broken instrument, not
   a finding.

2. **Generalised from the tile at the origin.** The first three measurements
   were taken only on the `(0,0)` tiles the brief named, and level 4 there is
   50.5% grid-aligned with twice the vertex budget and a near-unindexed
   triangle-soup layout (11713 stored vertices for 1982 distinct columns).
   That produced a written-down belief that vanilla terrain LOD carries
   sub-LAND-grid detail. It does not: `Commonwealth.4.0.0.BTR` is the LEAST
   grid-aligned and the DENSEST of all 2304 level-4 chunks in the worldspace.
   Found only by sweeping all 3060 Commonwealth `.BTR` files, which was not in
   the plan until the level-4 result disagreed with levels 8/16/32. Rule
   (CONSTITUTION 4, "the whole corpus, not a sample"): **a per-tile
   measurement is not a per-worldspace fact until the corpus is swept, and the
   cheapest sweep goes FIRST, not last.** The sweep cost 90 seconds; believing
   one tile cost two wrong conclusions.

'''

out = b.replace(anchor, anchor + entry, 1)
open(P, 'wb').write(out)
n = open(P, 'rb').read()
print('CR', before_cr, '->', n.count(b'\r'), ' LF', before_lf, '->', n.count(b'\n'))
print('bytes', len(b), '->', len(n))
assert n.count(b'\r') == 0
