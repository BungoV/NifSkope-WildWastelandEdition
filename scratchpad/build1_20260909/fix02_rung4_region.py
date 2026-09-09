#!/usr/bin/env python
"""BUILD1: rung 4 of lodgen_terrain.sh asked the pyramid for a chunk sheet the
region could not produce.

`assembleChunkRow` (src/lodgen.cpp) writes a dim-D chunk's sheets from the 2x2
content blocks of the level whose dim is D/2, and only once BOTH child tile
rows of a parent row are staged. Rung 4 copied rung 3's region
`--terrain-region -20 24 -19 25`, which is 2x2 CELLS -- a quarter of the dim-4
chunk (-20,24), which spans cells -20..-17 / 24..27. The pyramid therefore had
one tile row and wrote no sheet, and the check read a missing file.

Measured on the built exe: with `-20 24 -19 25` the run exits 0, writes
Commonwealth.VT.2.lodv / .4.lodv and NO *_msn.DDS; with `-20 24 -17 27` it
writes Commonwealth.4.-20.24{,_msn,_data}.DDS and msnstat reads
`UP=G D0=76 D1=32 D2=51 D3=32`.

Rung 3's own region is left alone: the direct bake builds the chunk that
CONTAINS the region, so it never needed the whole chunk.

LF-only file; the CR count must stay 0.
"""
import os

P = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 '..', '..', 'tests', 'spells', 'lodgen_terrain.sh')
P = os.path.normpath(P)

b = open(P, 'rb').read()
assert b.count(b'\r') == 0, b.count(b'\r')

OLD = (b'mkdir -p "$W/vt/obj" "$W/vt/tex" "$W/vt/mod"\n'
       b'"$NS" -no-gui lodgen "$ESM" --worldspace 3C \\\n'
       b'\t--terrain-region -20 24 -19 25 --dim 4 \\\n')
NEW = (b'mkdir -p "$W/vt/obj" "$W/vt/tex" "$W/vt/mod"\n'
       b'# THE REGION IS A WHOLE CHUNK HERE, unlike rung 3. `assembleChunkRow` builds\n'
       b'# the dim-4 sheets out of the 2x2 content blocks of the dim-2 level, and only\n'
       b'# once BOTH child tile rows of a parent row are staged; the chunk (-20,24)\n'
       b'# spans cells -20..-17 / 24..27, so rung 3\'s 2x2-cell region left the pyramid\n'
       b'# with one row and it wrote no sheet at all. The direct bake in rung 3 does\n'
       b'# not care -- it builds the chunk that CONTAINS the region.\n'
       b'"$NS" -no-gui lodgen "$ESM" --worldspace 3C \\\n'
       b'\t--terrain-region -20 24 -17 27 --dim 4 \\\n')
assert b.count(OLD) == 1, b.count(OLD)
b = b.replace(OLD, NEW)

assert b.count(b'\r') == 0
open(P, 'wb').write(b)
print('patched %s, %d bytes, CR %d' % (P, len(b), b.count(b'\r')))
