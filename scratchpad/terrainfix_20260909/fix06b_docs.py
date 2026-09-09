#!/usr/bin/env python
"""TERRAINFIX step 6b: the two things the .lodt contract still does not say --
what the water fields mean, and what a cell with no LAND record holds -- plus
the reader invariants that still said version 1 only.

NOTE: docs/LODGEN_BTD_FORMAT.md was rewritten by another lane at 16:41 while
this lane held it. These edits are ADDITIVE and anchored; if that lane writes
again from a stale copy they will be lost and must be re-applied."""


def patch(path, pairs):
    b = open(path, 'rb').read().decode('utf-8')
    cr0 = b.count('\r')
    for old, new in pairs:
        c = b.count(old)
        assert c == 1, 'anchor count %d for %r in %s' % (c, old[:60], path)
        b = b.replace(old, new)
    assert b.count('\r') == cr0, 'line endings moved in ' + path
    open(path, 'wb').write(b.encode('utf-8'))
    print('%s patched' % path)


patch('docs/LODGEN_BTD_FORMAT.md', [
    ('''| float | water height, world units |
| uint16 | water type: index into the WATR table, 0xFFFF = worldspace default |
| uint16 | flags: bit 0 has water, bit 1 has land |

16 bytes a cell''',
     '''| float | water height, world units |
| uint16 | water type: index into the WATR table, 0xFFFF = worldspace default |
| uint16 | flags: bit 0 has water, bit 1 has land |

**The water fields, exactly.** A cell has water when its `CELL` `DATA` carries
the has-water bit (0x0002), and only then are the two water fields meaningful:
a cell without water writes height 0 and type `0xFFFF`, and a reader must not
place a plane for it.

  * **height** is RESOLVED, not raw -- the cell's `XCLW` when it carries one and
    it is not one of the three no-water sentinels (`0xFF7FFFFF`, `0x7F7FFFFF`,
    `0x4F7FFFC9`), otherwise the worldspace's `DNAM` default. So a consumer
    places the plane without holding the plugin. Whether a plane is DRAWN is a
    second question the file does not answer: vanilla emits a LOD water quad
    only where the water is exposed above the cell's terrain minimum, which is
    why Sanctuary's default-height cells get none and the harbour's do.
  * **type** is the cell's `XCWT` interned into the WATR table, or `0xFFFF`
    meaning the worldspace default -- which since version 2 is a form id in the
    header (0x9C) rather than a promise the file cannot keep. The default is
    never interned, so `watrCount` counts the types that OVERRIDE the
    worldspace, and `0xFFFF` is not a "missing" value but the commonest case:
    the Commonwealth's default is `ExtOceanWater`.

16 bytes a cell'''),
    ('''**There is no per-block slot table and no present-bitmask**''',
     '''**A cell with NO `LAND` record still has samples, and they are not zero.**
Its plane is the worldspace's default land height (WRLD `DNAM`), and on the row
and column it SHARES with a neighbour -- VHGT's row 32 and column 32 are this
cell's row 0 and column 0 whether or not this cell exists -- it takes those
neighbours' samples under the same maximum rule as any other shared sample. The
default height does NOT take part in that maximum, exactly as it does not in
the shadow heightmap: Far Harbor's default is 0 and its inherited row is around
-250 units, so a maximum against the default would keep the wrong value.

Both halves were wrong until 2026-09-09: the plane was refused outright and the
writer substituted a bare 32767, which is height ZERO and not the default, and
the inherited row went with it. MEASURED against the shadow heightmaps of the
same worldspaces -- the authority, since the Commonwealth one is byte-identical
to Bethesda's own `Commonwealth_fine` -- DiamondCity differed on **167,936 of
172,032** texels (164 landless cells, default land height -2048),
NukaWorldAmphitheater on **97**, DLC03FarHarbor on **62**: one landless cell
ringed by eight that have land, so its row 0 and its column 0, less the single
sample that happened to be exactly 0. All 36,864 Commonwealth cells carry
`LAND`, so it was 0 of 37,748,736 there before and after -- which is why
nothing caught it.

**There is no per-block slot table and no present-bitmask**'''),
    ('''1. `magic == 'LODT'` and `version == 1`; the header is exactly 152 bytes.''',
     '''1. `magic == 'LODT'` and `version` is 1 or 2; the header is exactly 152 bytes
   at version 1 and 160 at version 2. The two version 2 fields sit AFTER the
   ten section offsets, so a reader that takes every section from its offset
   reads both versions with one code path.'''),
    ('''6. Every section offset is `>= 152` and every section lies before the block data.''',
     '''6. Every section offset is `>=` the header size its version declares (152 or
   160) and every section lies before the block data.'''),
    ('''The reference reader refuses - naming the field - on: a file shorter than 152
bytes; a bad magic; an unsupported version; `blockDataOffset < 152` or beyond''',
     '''The reference reader refuses - naming the field - on: a file shorter than 152
bytes; a bad magic; a version outside 1..2; `blockDataOffset` below the header
size its version declares, or beyond'''),
])
