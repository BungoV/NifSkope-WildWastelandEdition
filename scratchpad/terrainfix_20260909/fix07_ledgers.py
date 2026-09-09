#!/usr/bin/env python
"""TERRAINFIX step 7: WW_CHANGES.md and MISTAKES.md.

Both files are LF-only at the top, and both may be written by another lane in
the same window -- the entries are kept verbatim in the lane report so the
director can re-splice them if they are lost."""

CHANGES = '''# NifSkope — Wild Wasteland Edition: Change Log

## 2026-09-09 - The pyramid was writing the 2026-09-07 normal map, and a cell with no landscape read as flat zero

Three defects, one theme: a second copy of something that already had one home.

**THE VIRTUAL-TEXTURE TILE BAKER HAD BOTH 2026-09-07 DEFECTS.**
`lodgenBakeVtTile` sampled the height grid with `int( ngx )` -- NEAREST, so all
sixteen texels of a 4x4 block read one height sample and got one normal -- and
wrote north in green with up in blue, the conventional order. Those are exactly
the two the per-chunk baker lost on 2026-09-07; this was a COPY of those twelve
lines and kept them. It is not a future consumer's problem: with `--vt` on, the
`.btr` chunk sheets are ASSEMBLED from these tiles
(`docs/LODGEN_TERRAIN_VT.md` 2.4), so a `--vt` bake shipped the pre-2026-09-07
`_msn` into the stock engine under the same file name.

Both paths now call `lodgenTerrainHeightAt` (bilinear through the quintic ease)
and `lodgenTerrainMsnPixel` (R east, G up, B north). MEASURED offline on the
tile's own heights out of a written `.lodt`, before the block codec, against
vanilla's shipped sheet for the same tile
(`scratchpad/terrainfix_20260909/vt_msn_sim.py`):

| tile | statistic | pyramid, before | after | vanilla |
|---|---|---|---|---|
| 4.-60.36 | grid-phase roughness | 2.001 | **0.209** | 0.065 |
| 4.-20.24 | grid-phase roughness | 2.000 | **0.150** | 0.031 |
| 4.-60.36 | left-diff by x mod 4 | 98/0/0/0 | 36/98/99/98 | 100/63/64/63 |
| 4.-20.24 | left-diff by x mod 4 | 93/0/0/0 | 40/93/95/93 | 100/67/68/67 |
| 4.-60.36 | mean UP as the shader reads it | 0.288 | **0.841** | 0.770 |
| 4.-20.24 | mean UP as the shader reads it | -0.151 | **0.943** | 0.894 |

The roughness is lane LATTICE's phase-conditional statistic; its known-answer
controls read 0.044 on a smooth analytic field and 1.996 on the same field
creased every fourth column, a separation of 45.7x, and the old pyramid sheet
sits AT that creased ceiling. The left-difference fractions are 2026-09-07's
zero-order-hold signature. The last row is the channel order stated as a
picture: with up in blue, Sanctuary's ground read as facing slightly DOWNWARD.

Gate: `tests/spells/lodgen_terrain.sh` rung 4 bakes the same chunk WITH `--vt`
and asserts the assembled sheet's up channel is green and that its
left-difference classes 1..3 clear 20% (nearest gives 0), with vanilla's own
sheet read beside it as the known-answer control.

**AND THE FLAT-NORMAL FILL WAS THE RENDERER'S CONSTANT IN THE WRONG BYTE
ORDER.** All five `0xFFFF8080U` fills in `lodgen.cpp` came from
`src/gl/renderer.cpp`, where a `quint32` is RGBA bytes; these buffers are ARGB,
so the value decoded as east +1, up 0 -- a sideways normal, wrong under BOTH
channel orders. `LODGEN_MSN_FLAT = 0xFF80FF80U` is flat ground in the order we
write. Only reachable where a tile or a mosaic row is missing, which is why it
never showed.

**A CELL WITH NO `LAND` RECORD WAS WRITTEN AS FLAT HEIGHT ZERO, AND LOST THE
ROW ITS NEIGHBOURS SHARE WITH IT.** VHGT's row 32 and column 32 ARE the north
and east neighbours' row 0 and column 0, whether or not that neighbour has a
record of its own; the shadow heightmap applies the seam maximum there
regardless, and the `.lodt` refused the plane outright and substituted a bare
32767 -- height ZERO, not the worldspace's default land height. The two files
are read by FO4CS as ONE surface, so a sample they disagree on is the
ridge-that-casts-a-shadow-without-being-drawn of 2026-09-05c.

MEASURED, every worldspace with a written `.lodt`, against its own shadow
heightmap texel for texel
(`scratchpad/terrainfix_20260909/lodt_vs_heightmap.py`, an independent decoder;
the heightmaps are the authority because the Commonwealth one is byte-identical
to Bethesda's `Commonwealth_fine`):

| worldspace | texels | differing, before | landless cells |
|---|---|---|---|
| Commonwealth | 37,748,736 | **0** | 0 of 36,864 |
| NukaWorld | 4,326,400 | **0** | 0 |
| DLC03FarHarbor | 20,207,616 | **62** | 1 in the diffed area |
| NukaWorldAmphitheater | 114,688 | **97** | 110 of 112 |
| DiamondCity | 172,032 | **167,936** | 164 of 168 |

Far Harbor's 62 are ONE cell, (14,-6), ringed by eight that have land: its row
0 (32 texels, from the south neighbour's row 32) and its column 0 (rows 1..31
from the west neighbour's column 32), less the single sample that happened to
be exactly 0. DiamondCity's 164 landless cells read 0 where the heightmap says
-2048, that worldspace's default land height. The Commonwealth has not one
landless cell, which is why four days of byte-identity gates said nothing.

The default height does not take part in the maximum on an inherited sample,
exactly as it does not in the heightmap -- Far Harbor's default is 0 and its
inherited row is around -250, so a max against the default would have kept all
62 wrong. The per-cell min/max now covers the inherited row too, so a renderer
culling on it cannot cull what the file draws.

Gate: `tests/spells/lodt_write.sh` bakes NukaWorldAmphitheater's `.lodt` AND
its heightmap and requires every one of the 114,688 texels to agree, having
first checked that the worldspace HAS landless cells so the check can fail. Run
against the shipped files it reads 97 and goes red.

**`.lodt` VERSION 2: THE WORLDSPACE'S OWN WATER.** A cell's water type of
`0xFFFF` means "the worldspace default", and the default's WATR form is
deliberately not interned in the WATR table -- that is what keeps "inherited"
distinguishable from "explicitly this type" -- so it appeared nowhere in the
file and `0xFFFF` was unresolvable. Version 2 appends the default water height
(`DNAM`) and form (`NAM2`) AFTER the ten section offsets, so every offset a
version 1 reader uses is at the byte it was. The reader accepts 1 and 2.

**FO4CS's shipped `.lodt` parser (lane LODT1, wave 71) pins `kVersion = 1u` and
refuses anything else**, so a fresh bake is refused there until that lane
learns version 2. The way back needs no rebuild on our side: `WW_LODT_VERSION=1`
writes the version 1 bytes exactly, and the harness proves EXACTLY -- same
file, every section offset shifted by the eight bytes appended, everything past
the header byte-identical.

Also: `tests/spells/lodt_write.sh` printed `RESULT PASS`/`FAIL` and threw the
exit code away -- the python block was the script's last command and never
called `sys.exit`, so the harness reported success on any measurement at all.
It exits on its verdict now.

'''

CH = 'WW_CHANGES.md'
b = open(CH, 'rb').read().decode('utf-8')
head = '# NifSkope — Wild Wasteland Edition: Change Log\n'
assert b.startswith(head), repr(b[:60])
cr0 = b.count('\r')
b = CHANGES + b[len(head):].lstrip('\n')
assert b.count('\r') == cr0, 'CRLF neighbours disturbed'
open(CH, 'wb').write(b.encode('utf-8'))
print('%s: entry added, CR %d (unchanged)' % (CH, cr0))

MIS = '''Newest at the top.

## 2026-09-09 -- a fixed defect was left standing in the second writer of the same file

**What was done.** The 2026-09-07 round fixed the terrain `_msn`'s nearest
sampling and its channel order in `lodgenBakeTerrainTextures`, wrote the
measurements up, and gated it. `lodgenBakeVtTile` held a COPY of those twelve
lines and was not touched; the 2026-09-09 lattice round noticed it and left it
for a lane. In between, any `--vt` bake wrote the pre-2026-09-07 sheet into the
very file name the gate checks, because the chunk sheets are assembled from the
pyramid tiles.

**What was true instead.** A defect is a property of an OUTPUT, not of a
function. Two writers of one file name are one defect with two homes.

**How it was found.** By reading `lodgenBakeVtTile` for this lane's brief, not
by any gate: the harness baked without `--vt`, so it never saw the other
writer.

**The rule.** When a defect in a generated file is fixed, grep for every other
producer of that file or channel before closing it, and put the fixed code in
ONE function that both call (CONSTITUTION 10, "what is shared lives in the
shared code"). A gate that exercises one of two paths is half a gate: rung 4 of
`lodgen_terrain.sh` now bakes the same chunk both ways.

## 2026-09-09 -- a constant copied across a byte-order boundary

**What was done.** `0xFFFF8080U` was used at five places in `lodgen.cpp` as the
flat-normal fill, copied from `src/gl/renderer.cpp` where it is the flat
TANGENT-space normal.

**What was true instead.** The renderer's `quint32` is RGBA bytes in memory;
these buffers are ARGB (`0xFF000000 | R << 16 | G << 8 | B`). The same 32 bits
mean (128,128,255) there and (255,128,128) here -- east +1, up 0, a sideways
normal, wrong under both channel orders.

**How it was found.** Reading the VT baker's fills while fixing its channel
order, and noticing the value was flat under neither convention.

**The rule.** A literal that crosses a byte-order or channel-order boundary is
RE-DERIVED at its destination, never copied; and a fill that is normally
overwritten still gets a name (`LODGEN_MSN_FLAT`), because a nameless magic
number is what makes such a copy invisible.

## 2026-09-09 -- a four-worldspace check that could not fail on three of them

**What was done.** The `.lodt` writer's landless-cell fallback returned a bare
32767 while every other height in the file goes through the worldspace's
default land height, and the seam maximum was skipped for a cell with no `LAND`
record. The byte-identity gate ran on the Commonwealth.

**What was true instead.** All 36,864 Commonwealth cells carry `LAND`, so the
Commonwealth cannot exercise either rule. DiamondCity was wrong on 97.6% of its
texels, NukaWorldAmphitheater on 97 and Far Harbor on 62, against the shadow
heightmaps -- and the FO4CS check that reported Far Harbor's 62 apparently did
not report DiamondCity's 167,936.

**How it was found.** Diffing every deployed `.lodt` against its own deployed
heightmap offline, which needed no build and took one script.

**The rule.** A gate is run on the input that HAS the case, and the harness
says so out loud: `lodt_write.sh` now asserts the test worldspace has landless
cells before asserting they are right. "A check that cannot fail on the input
it is run against is not a check" -- the same sentence this repo already had in
`docs/LODGEN_BTD_FORMAT.md` about the Pitt alpha invariant.

## 2026-09-09 -- a harness that printed its verdict and returned success

**What was done.** `tests/spells/lodt_write.sh` ended with a python block that
printed `RESULT PASS` or `RESULT FAIL` and never called `sys.exit`; that block
was the script's last command, so the script's exit code was python's 0
whatever it measured.

**What was true instead.** Every run of it was a pass.

**How it was found.** Reading the script to add a case to it.

**The rule.** A harness's verdict is its EXIT CODE. Before adding a check to a
harness, run it against the state the check is supposed to catch and watch the
script -- not its output -- go red.

'''

MI = 'MISTAKES.md'
b = open(MI, 'rb').read().decode('utf-8')
anchor = 'Newest at the top.\n'
assert b.count(anchor) == 1
cr0 = b.count('\r')
b = b.replace(anchor, MIS)
assert b.count('\r') == cr0
open(MI, 'wb').write(b.encode('utf-8'))
print('%s: 4 entries added, CR %d (unchanged)' % (MI, cr0))
