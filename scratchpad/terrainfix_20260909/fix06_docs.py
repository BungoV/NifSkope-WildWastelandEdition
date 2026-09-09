#!/usr/bin/env python
"""TERRAINFIX step 6: the two contracts say what the bytes now are."""

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
    # --- identity: two versions now --------------------------------------
    ('''    magic    "LODT"      NOT "BTDB"
    version  1
''',
     '''    magic    "LODT"      NOT "BTDB"
    version  2                  (1 is still read)
'''),
    ('''**The magic must differ from FO76's**''',
     '''**Version 2** (2026-09-09) appends the worldspace's own default water height
and WATR form to the END of the header, after the ten section offsets. Nothing
else moved: every field a version 1 reader knows is at the byte it was, and
every section is addressed by an absolute offset out of the header, so a reader
that does not assume the first section begins at 0x98 reads a version 2 file
correctly. The writer emits 2; `WW_LODT_VERSION=1` writes the version 1 bytes
exactly, which is the way back for a consumer that has not learned 2 yet, and
the harness proves it is exact rather than similar -- same file, shifted by the
eight bytes appended.

Why it was needed: a cell's water type of `0xFFFF` means "the worldspace
default", and the default's WATR form is deliberately NOT interned in the WATR
table (that is what keeps "inherited" distinguishable from "explicitly this
type") -- so it appeared nowhere in the file and `0xFFFF` was an unresolvable
promise.

**The magic must differ from FO76's**'''),
    # --- header table ----------------------------------------------------
    ('''| 0x90 | uint64 | total file size |

Header is **0x98 = 152 bytes**.''',
     '''| 0x90 | uint64 | total file size |
| 0x98 | float | **v2** worldspace default water height (WRLD `DNAM`) |
| 0x9C | uint32 | **v2** worldspace default water type, the WRLD `NAM2` WATR form; 0 = none |

Header is **0x98 = 152 bytes at version 1, 0xA0 = 160 at version 2**. The two
version 2 fields are at the END deliberately: the ten section offsets keep
their places, so the only thing a version 1 reader gets wrong is an assumption
it should not have made (that the first section starts immediately after the
header it knows).'''),
    # --- per-cell water semantics ----------------------------------------
    ('''| float | water height, world units |
| uint16 | water type: index into the WATR table, 0xFFFF = worldspace default |
| uint16 | flags: bit 0 has water, bit 1 has land |

16 bytes a cell''',
     '''| float | water height, world units |
| uint16 | water type: index into the WATR table, 0xFFFF = worldspace default |
| uint16 | flags: bit 0 has water, bit 1 has land |

**The water fields, exactly.** A cell has water when its `CELL` `DATA` carries
the has-water bit, and only then are the two water fields meaningful; a cell
without water writes height 0 and type `0xFFFF` and a reader must not draw a
plane for it.

  * **height** is RESOLVED, not raw: the cell's `XCLW` when it carries one and
    it is not one of the three no-water sentinels (`0xFF7FFFFF`, `0x7F7FFFFF`,
    `0x4F7FFFC9`), otherwise the worldspace's `DNAM` default. So a consumer
    never has to hold the plugin to place the plane. (Whether a plane is DRAWN
    is a second question: vanilla emits a LOD water quad only where the water
    is exposed above the cell's terrain minimum, which is why Sanctuary's
    default-height cells get none and the harbour's do.)
  * **type** is the cell's `XCWT` interned into the WATR table, or `0xFFFF`
    meaning the worldspace default -- which since version 2 is a form id in the
    header rather than a dangling word. The default is never interned, so
    `watrCount` counts cells that OVERRIDE the worldspace, and `0xFFFF` is not
    a "missing" value: it is the commonest case (the Commonwealth's default is
    `ExtOceanWater`).

16 bytes a cell'''),
    # --- landless cells ---------------------------------------------------
    ('''**There is no per-block slot table and no present-bitmask**''',
     '''**A cell with NO `LAND` record still has samples, and they are not zero.**
Its plane is the worldspace's default land height (`DNAM`), and on the row and
column it SHARES with a neighbour -- VHGT's row 32 and column 32 are this
cell's row 0 and column 0, whether or not this cell exists -- it takes those
neighbours' values under the same maximum rule as any other shared sample. The
default height does not take part in that maximum, exactly as it does not in
the shadow heightmap: Far Harbor's default is 0 and its inherited row is around
-250 units, so a maximum against the default would keep the wrong value.

This was wrong until 2026-09-09 in both halves -- the plane was refused
outright and the writer substituted a bare 32767, which is height ZERO rather
than the default, and the inherited row was lost with it. MEASURED against the
shadow heightmaps of the same worldspaces, which are the authority (the
Commonwealth one is byte-identical to Bethesda's own `Commonwealth_fine`):
DiamondCity 167,936 texels of 172,032 (164 landless cells, default -2048),
NukaWorldAmphitheater 97, DLC03FarHarbor 62 (ONE landless cell ringed by eight
that have land: its row 0 and its column 0, less the single sample that
happened to be exactly 0). The Commonwealth's 36,864 cells all carry `LAND`, so
it was 0 of 37,748,736 before and after -- which is why nothing caught this.

**There is no per-block slot table and no present-bitmask**'''),
])

patch('docs/LODGEN_TERRAIN_VT.md', [
    ('''| sheet | role | format without cover | with cover | colour space |''',
     '''**The normal sheet is written by the SAME code as a chunk bake.** The height
reconstruction (`lodgenTerrainHeightAt`: bilinear over the 128-unit VHGT grid
with both blend parameters through the quintic ease) and the channel order
(`lodgenTerrainMsnPixel`: R east, G **up**, B north) are one function each,
called by the tile baker and by `lodgenBakeTerrainTextures`. They were twelve
lines copied, and the copy kept both of the 2026-09-07 defects the chunk path
had lost -- `int( ngx )` nearest sampling, so all sixteen texels of a 4x4 block
shared one height sample and one normal, and north in green with up in blue.
That reached the stock engine and not only a future consumer, because with
`--vt` on the `.btr` chunk sheets are assembled from these tiles (2.4).

MEASURED offline on the tile's own heights, before the block codec, against
vanilla's shipped sheet for the same tile
(`scratchpad/terrainfix_20260909/vt_msn_sim.py`), on `4.-60.36` and `4.-20.24`:
the grid-phase roughness of lane LATTICE fell 2.001 to 0.209 and 2.000 to 0.150
(vanilla 0.065 and 0.031; the known-answer controls read 0.044 on a smooth
field and 1.996 on the same field creased every fourth column, a separation of
45.7x), the left-neighbour difference by x mod 4 went from 98/0/0/0 -- the
signature of a zero-order hold -- to 36/98/99/98 against vanilla's 100/63/64/63,
and the mean UP component **as the shader reads it** went from 0.288 to 0.841
and from -0.151 to 0.943 (vanilla 0.770 and 0.894). The second of those is the
channel order stated as a picture: with up in blue, Sanctuary's ground read as
facing slightly DOWNWARD.

| sheet | role | format without cover | with cover | colour space |'''),
])
