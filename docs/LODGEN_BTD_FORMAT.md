# `.lodl` v1 / v2 - the whole-worldspace landscape file

**THE EXTENSION CHANGED ON 2026-09-09 AND THE BYTES DID NOT.** bungo's
ruling: this file is `.lodl`; `.lodt`, which it used to be, now names the
TERRAIN TEXTURE sheets (`docs/LODGEN_TERRAIN_VT.md`). The magic is still
`LODT` on disk, deliberately, because the gate on the rename is that a
`.lodl` is byte-identical to the `.lodt` the same worldspace wrote the day
before. What separates the two formats is the texture container's own new
magic, `LDTX`: **each reader refuses the other's file BY NAME**, so a
yesterday's `.lodt` opened as a terrain texture says it is the landscape
file, and vice versa. The C++ names (`LodtFile`, `lodtWrite`,
`src/lodtfile.cpp`) did NOT move -- they are internal, and they appear in
no file on disk and in no command.

**Contract versions: `magic 'LODT'`, `version 1` (header 0x98 = 152 bytes) and
`version 2` (header 0xA0 = 160 bytes, adding the worldspace default water).**
**The writer defaults to version 2**; the reader accepts 1 and 2.
**Status: WRITER, READER AND .btd CONVERSION SHIPPED** (`src/lodtfile.cpp`).

**A CONSUMER EXISTS, AND IT KNOWS VERSION 1 ONLY.** FO4CS lane LODT1 (wave 71,
2026-09-05) reads this file as the far-field heightmap source -
`src/FarField/FarFieldLodtFormat.h` and `FarFieldLodtSource.h` in
`E:\Projects\Fo4CommunityShaders\wt-fixfirst`, gated by
`tests/lodt_format_tests.cpp`. Its `kVersion` is `1u` and it refuses anything
else, so **a freshly generated version-2 file will be refused by the shipped
FO4CS parser until that lane teaches it version 2**. The zero-effort fallback
exists and needs no rebuild on our side: `WW_LODL_VERSION=1`, or
`LodtOptions::headerVersion = 1`, writes the exact bytes the writer produced
before version 2. See `scratchpad/handoff_fo4cs/README.md` §4.

This document is the contract that parser was written against, and it stays the
contract; the parser is not a second one.

This is the format LODGEN writes and FO4CS meshes from.

Written as a document before the code, which paid for itself: the per-quadrant
LTEX slot table was specified inside each block, and that only works while a
block is one cell - a level-3 block spans 8x8 cells, so 256 quadrants. It is a
global section now, as FO76 has it.

Commonwealth output: **34.3 MB in 6.8 s**, heights reconstructing to the ESM's
own LAND corners exactly.

It takes Fallout 76's *idea* - one file per worldspace, per-cell tables, a LOD
pyramid, zlib block streaming - and **not** its layout, because FO76 samples
terrain at 128 per cell and Fallout 4's LAND is fixed at 33x33. Reusing their
block sizes would mean upsampling FO4 data 4x: sixteen times the bytes for zero
extra information.

---

## What it replaces

| retired | replaced by |
|---|---|
| `.btr` terrain chunks | this file |
| per-chunk `<chunk>.DDS` diffuse | runtime blend from the stored LTEX alphas |
| per-chunk `_msn.DDS` normals | derived from heights at runtime |
| per-chunk `_data.DDS` (AO/wetness/shore) | the AO section here; wetness and shore dropped |
| terrain vertex channels | the sections below |
| LOD water quads inside `.btr` | the water fields here; the mesher tessellates |

`.bto` object chunks are **unaffected** - objects cannot be meshed from a
heightfield, so they keep their own pipeline, descriptor and vertex channels.

**This ends the stock-engine fallback for terrain.** Without a mesher a `.lodl`
draws nothing and vanilla falls back to its own LOD. That is a deliberate
choice: every earlier channel decision was made under a "stock FO4 must render
this" rule that no longer applies to terrain.

---

## Location

    Data\Terrain\<WorldspaceEditorID>.lodl

**FO4 has no `Data\Terrain\`** - it puts LOD under `Data\Meshes\Terrain\<WS>\`
with objects in an `Objects\` subfolder (FO4CS `docs/RE/far-field-terrain-lod.md`
9.1). FO76 *does* use `Data\Terrain\`, which is where `Appalachia.btd` lives. So
taking FO76's location costs nothing: there is no folder to collide with, and
this is not a mesh, so it does not belong beside the `.bto`s.

The extension is **`.lodl`, not `.btd`** - a converted Appalachia and its source
sit in the same folder without either shadowing the other, and no tool has to
guess from a shared extension which dialect it is holding.

The stem is the worldspace **editor ID**, as with the `HeightMap` textures, and
for the same reason - the loader keys on `GetFormEditorID()` after climbing
`parentWorld` while `parentUseFlags` has `kUseLandData`, so a child worldspace
sharing its parent's land inherits the parent's file and needs none of its own.

---

## Identity

    magic    "LODT"      NOT "BTDB"
    version  1 or 2

**The magic must differ from FO76's**, even with a distinct extension. Our
sample rate and sections differ, so a genuine BTD reader handed this file would
not fail - it would *misparse*, which is plausible garbage rather than a
refusal. The magic makes that a clean rejection whatever the file is named.

---

## Header

| offset | type | field |
|---|---|---|
| 0x00 | char[4] | magic `LODT` |
| 0x04 | uint32 | version (1 or 2) |
| 0x08 | int32 | cell min X (west) |
| 0x0C | int32 | cell min Y (south) |
| 0x10 | int32 | cell max X (east) |
| 0x14 | int32 | cell max Y (north) |
| 0x18 | uint32 | samples per cell edge (32 from FO4, 128 from FO76) |
| 0x1C | uint32 | block edge in samples |
| 0x20 | uint32 | LOD level count |
| 0x24 | float | minimum height, world units |
| 0x28 | float | maximum height, world units |
| 0x2C | float | height quantum, world units per stored step |
| 0x30 | uint32 | LTEX form-ID count |
| 0x34 | uint32 | WATR form-ID count |
| 0x38 | uint32 | GCVR form-ID count (0 = no ground cover) |
| 0x3C | uint32 | AO samples per cell edge (0 = no AO) |
| 0x40 | uint32 | overview samples per cell edge (0 = no overview) |
| 0x44 | uint32 | section-present flags |
| 0x48 | uint64 | offset: LTEX form-ID table |
| 0x50 | uint64 | offset: WATR form-ID table |
| 0x58 | uint64 | offset: GCVR form-ID table |
| 0x60 | uint64 | offset: **quadrant slot table** |
| 0x68 | uint64 | offset: per-cell table |
| 0x70 | uint64 | offset: coarse overview |
| 0x78 | uint64 | offset: **AO plane** |
| 0x80 | uint64 | offset: block directory |
| 0x88 | uint64 | offset: block data |
| 0x90 | uint64 | total file size |
| **0x98** | float | **v2 only** — worldspace default water height |
| **0x9C** | uint32 | **v2 only** — worldspace default water type, a WATR form ID |

Header is **0x98 = 152 bytes at version 1** and **0xA0 = 160 bytes at
version 2**. Every section offset is measured from the start of the file, so the
header size is not something a reader has to compute — but it **is** the floor a
reader checks `blockDataOffset` against, and the writer refuses if its own
assembled header is not the size its version declares.

**What version 2 adds, and why.** Without those two fields, the per-cell
"water type `0xFFFF` = the worldspace default" is a promise the file cannot
keep: the default's WATR form appears in **no** table, because the writer
deliberately does not intern it — that is exactly what makes `0xFFFF`
distinguishable from "explicitly this type". The height is there too, so a
consumer can resolve an inheriting cell's water plane from the file alone.
Version 1 files carry neither and report **0** and "no default water"; a reader
**must not** treat that zero as a form ID.

`section-present flags`: bit 0 terrain colour, bit 1 ground cover, bit 2 AO,
bit 3 water. A reader checks the bit, not the offset.

Cell bounds are **inclusive**, matching the `HeightMap` texture convention, so
the world rectangle is `[west*4096, (east+1)*4096] x [south*4096, (north+1)*4096]`.

**Every offset is 64-bit.** FO76 uses int32 relative to a base, which is fine at
1.4 GB and a cliff shortly after; the 804-cell target is already ~2 GB.

**Everything in the file is little-endian**, whatever the host.

**What the FO4 writer puts in the variable header fields**, unless the caller
overrides them: `block edge` **32**, `LOD level count` **4**, `overview samples`
**8**, `AO samples` **8**, `height quantum` **8.0**, `samples per cell edge`
**32**, `header version` **2**. A `.btd` source overrides the block edge, the
sample rate and the quantum. A reader must still read all of them, never assume
them.

**ROW 0 IS SOUTH**, in every grid this file carries - the block payloads, the
coarse overview and the AO plane alike. That is the writer's own cell order
(`cell row 0 first`) and it is **the opposite of the `HeightMap` DDS**, which is
north-up. A reader converting to a north-up texel space **mirrors Y**, and one
that forgets produces a map that is plausible everywhere and correct nowhere.
`.lodt` is north-up; the two conventions are both live in this tree and the
mismatch has already cost one consumer a Y mirror.

---

## Sources

The generator takes **either** source and writes the same format. The sample
rate is whatever the source provides; it is recorded in the header and is never
assumed by a reader.

| source | rate | notes |
|---|---|---|
| Fallout 4 ESM `LAND` | 32 | Vanilla's `VHGT` is 1,096 bytes - 33x33. The ceiling is the **engine's parser**, not the plugin format: a 129x129 `VHGT` is 16,648 bytes against a 65,535 subrecord limit, and `VNML` at 49,923 still fits. An extended record is a code change, not a format change. |
| Fallout 76 `.btd` | 128 | Four times FO4's detail, read directly. For a ported worldspace this is the only path that keeps it - if the port went through FO4 `LAND` records, the detail was already lost at import. |
| a future extended `LAND` | whatever it carries | If the record is ever widened and the engine taught to read it, nothing here needs changing. |

**That last row is the reason the rate is a header field rather than a
constant.** Fallout 76 has **zero** `LAND` records - measured, against Fallout
4's 37,020 - because terrain left the plugin entirely once it went to 128
samples a cell. If FO4's record is ever extended the same pressure applies in
reverse, and a format that hardcoded 32 would have to be rev'd rather than
simply written with a different header value.

---

## Height encoding

    stored = height / quantum + 32767   decode: height = (stored - 32767) * quantum

uint16 around a fixed midpoint, with the **quantum in the header** rather than
baked in. **Not** FO76's normalise-between-file-min-and-max, which makes
precision depend on the worldspace's height range and lets a wrong header
silently rescale every height in the file.

| source | quantum | why |
|---|---|---|
| FO4 `LAND` | **8** | `VHGT`'s own storage quantum - signed bytes scaled by 8 - so this is **exactly lossless**, wasting no precision and inventing none. Also matches the `HeightMap` texture and xLODGen's FO4 convention. |
| FO76 `.btd` | **finer** | Their normalised uint16 resolves ~0.59 units on Appalachia. A fixed 8 would quantise that 13x coarser and throw away real data. |

An earlier draft hardcoded 8, which is right for FO4 and wrong the moment a
`.btd` is the source - the reason this is a field.

**Choosing the quantum for a `.btd`.** The binding constraint is the *fixed*
32767 bias: every height has to land inside an int16's worth of steps either
side of it, so

    quantum >= max(|minHeight|, |maxHeight|) / 32767

and the finest legal value is that bound taken exactly. Two wrong answers were
tried first. `(max - min) / 65535` - which an earlier draft of this document
gave - is only correct when the range is centred on zero; on a 0..40000 range it
overflows the encoding by a factor of two. Rounding the bound up to a power of
two parses fine and quietly costs up to 2x the quantisation error: measured on
`EXM1PittWorldspace.btd`, worst-case error 0.98 units at a power-of-two quantum
against 0.32 at the exact bound.

The residual 0.32 is half a quantum and is not removable: their grid starts at
`minHeight` and ours is centred on zero, so the two step grids sit half a step
apart. Only a bias field would close it, and it is not worth one.

Range is `+/- 32767 * quantum`: at 8 that is +/-262,136 units against the
Commonwealth's actual -8,320..44,872, so ~90% of the code space goes unused.
That is the price of matching the source quantum exactly, and it is worth it.

---

## Tables

**LTEX form IDs** - `uint32 * count`. Per-sample alphas index into this.

**WATR form IDs** - `uint32 * count`. Per-cell water type indexes into this.

**GCVR form IDs** - `uint32 * count`, immediately followed - in the same
section, no separate offset - by `uint16[8]` ground cover slots per quadrant,
`cellsX*2` by `cellsY*2` row-major, `0xFFFF` unused. The slots are present iff
the count is non-zero, so a FO4 source writes an empty section and a reader that
checks the count never looks further. Eight rather than the LTEX table's six
because that is what a `.btd` quadrant carries.

**Quadrant slot table** - `uint16[6]` per quadrant, `cellsX*2` by `cellsY*2`,
row-major. Slots 0-4 are the quadrant's five strongest LTEX layers as indices
into the LTEX table; slot 5 is its base texture; `0xFFFF` means unused.

**Quadrant addressing, exactly.** A cell has four quadrants numbered
`q = (row >= spc/2 ? 2 : 0) + (col >= spc/2 ? 1 : 0)`, so **q0 = SW, q1 = SE,
q2 = NW, q3 = NE** in the file's own row-0-south space. The quadrant's row in
the table is

```
qx = (cx - minX)*2 + (q & 1)
qy = (cy - minY)*2 + (q >> 1)
at = quadrantTableOffset + (qy * (cellsX*2) + qx) * 6 * 2
```

The same `(qx, qy)` addressing serves the GCVR slots, at stride 8 words instead
of 6.

**Where the five layers come from, and what is dropped.** A quadrant may carry
more than five `ATXT` layers. The writer ranks them by **peak opacity** over the
17x17 opacity grid and keeps the strongest five, so what is dropped is the least
visible by construction. Measured: 89.5% of Commonwealth quadrants have no
layers at all and **99.27% fit in five**.

Global rather than per block, because a coarse block spans many cells - level 3
covers 8x8 of them. Slot assignment is a property of the world, not of a LOD
level. 1.77 MB for the Commonwealth, and measured **10.2% of quadrants carry any
layer at all**, matching the 10.5% the ESM scan predicted.

**Per-cell table** - one record per cell, row-major from (west, south):

| type | field |
|---|---|
| float | minimum height in this cell |
| float | maximum height in this cell |
| float | water height, world units |
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

16 bytes a cell - 590 KB for the Commonwealth, 10 MB for the 804-cell port.
Flat and uncompressed on purpose: it is the culling and water-plane lookup,
wanted before any block is decompressed.

---

## Coarse overview

An **uncompressed** whole-worldspace height grid at `overview samples per cell
edge`, at a fixed offset, needing no block reads and no inflate:

    uint16[cellsY * k][cellsX * k]    k = overview samples per cell edge

At `k = 8` that is 2.4 MB for the Commonwealth and 41 MB for the 804-cell port -
small enough to stay resident for the life of the process.

FO76 does the same thing with its LOD4 arrays, and the reason is sound: a
renderer wants a whole-world silhouette for the horizon, for culling and for
shadow casting *before* it has decided which blocks to stream, and paying a
zlib inflate for that is the wrong shape. Everything else in the file is
demand-loaded; this is the part that never is.

---

## Blocks

A block is **`block edge` samples square**, from the header, at every level.
Level 0 stores the source rate; each coarser level halves it, so a block covers
twice the linear world extent and its byte size never changes.

**Block edge is a field, not a constant, so a FO76 file converts without
resampling.** The natural choice is one block per cell at level 0 - which is
what FO76 does and what we do:

| source | samples per cell | block edge | blocks per cell at level 0 |
|---|---|---|---|
| Fallout 4 `LAND` | 32 | 32 | 1 |
| Fallout 76 `.btd` | 128 | 128 | 1 |

A smaller block edge is legal and gives finer streaming granularity at the cost
of more directory entries; a converter should simply carry the source's.

At rate 32 the level table works out as:

| level | cells covered | samples per cell edge | blocks, Commonwealth | blocks, 804-cell |
|---|---|---|---|---|
| 0 | 1x1 | 32 | 36,864 | 646,416 |
| 1 | 2x2 | 16 | 9,216 | 161,604 |
| 2 | 4x4 | 8 | 2,304 | 40,401 |
| 3 | 8x8 | 4 | 576 | 10,101 |

**Level numbering: 0 is the FINEST**, matching mip convention, and
`levelCount - 1` is the coarsest. The words "coarser" and "finer" are used
throughout rather than "above"/"below", which read both ways.

**Block directory** - one record per block:

| type | field |
|---|---|
| uint64 | offset of the compressed payload, absolute |
| uint32 | compressed size |
| uint32 | uncompressed size |

**Ordered COARSEST first**, then finer, row-major within each level. A streaming
reader walks the pyramid top-down, so the levels it always needs are contiguous
at the start of the directory and of the file.

Blocks at level `L` tile the worldspace at `2^L` cells each:

    blocksX(L) = ceil(cellsX / 2^L)
    blocksY(L) = ceil(cellsY / 2^L)
    first(L)   = sum over j > L of blocksX(j) * blocksY(j)
    index(L, bx, by) = first(L) + by * blocksX(L) + bx

There is **one directory range**: terrain blocks only. AO is not blocked - see
below.

**Blocks are PROGRESSIVE.** The coarsest level stores a full 32x32 grid; every
FINER level stores **only the samples its coarser parent does not already
have** - three quarters of the grid, since doubling resolution adds three new
samples per existing one. Nothing is stored twice anywhere in the pyramid.

This is FO76's scheme and the arithmetic gives it away: their height region is
`0x6000` bytes = 12,288 int16, exactly 3/4 of a 128x128 grid.

It is worth the extra complexity for a reason beyond the ~33% saving: it matches
what a streaming renderer actually does. Approaching a cell, the coarse samples
are already resident and only the **increment** is fetched. Self-contained
blocks would re-read data already held at every refinement step.

**The coarse levels must be a SUBSAMPLE, not an average.** Progressive storage
requires it - "the samples the parent does not have" presumes the parent's
samples are a subset of the child's - and both intended consumers want the same
thing:

  * **geomorphing** needs a coarse vertex to *be* a fine vertex, so morphing is
    between a stored height and the interpolation of its two coarse neighbours;
  * **clipmaps** need levels to nest exactly, since each level is a window on
    the same global sample grid at half the rate.

The cost is a little more aliasing than a filtered pyramid would give. That is
the trade progressive storage makes, and it is the right one here: a filtered
pyramid cannot be stored progressively at all, because its coarse values are not
samples of the fine grid.

**Block payload**, zlib deflate. Up to four planes, each stored the same way:

    uint16[N]   heights
    uint16[N]   LTEX alphas
    uint16[N]   terrain colour, 5-5-5      (only when the colour flag is set)
    uint16[N]   ground cover mask          (only when the ground cover flag is set)

Ground cover is an 8-bit mask widened to `uint16` rather than given a plane of
its own width. That wastes a byte a sample before compression and none after -
the high byte is a constant run - and it buys one emit path for all four planes
instead of two. No FO4 source writes it; a `.btd` does.

`N` is `blockEdge^2` at the coarsest level and three quarters of that at every
finer level - precisely `(blockEdge/2)^2 * 3`. Sample order at the finer levels
is the three new positions per parent sample, row-major: **right, below,
below-right**.

**The planes are stored back to back and the OPTIONAL ONES SHIFT THE INDICES.**
Heights are always plane 0 and alphas always plane 1. Colour is plane 2 **only
when section flag bit 0 is set**; ground cover is plane 3 when colour is present
and **plane 2 when it is not**. A reader that hardcodes 3 for ground cover reads
a FO76 file correctly and a hypothetical colourless ground-cover file wrongly.
The byte address of a sample is

```
raw[ (plane * N + k) * 2 ]        k = the in-block sample index below
```

**Finding a sample, the operation the whole layout exists to serve.** For a
full-rate global sample `(gx, gy)`:

1. **Which level stores it** - the coarsest level whose stride divides both
   coordinates:
   `L = 0; while (L < coarsest && gx % (1<<(L+1)) == 0 && gy % (1<<(L+1)) == 0) L++;`
   That is what "progressive" means: a sample appears **exactly once** in the
   pyramid.
2. **Which block** - `lx = gx >> L`, `ly = gy >> L`;
   `bi = lx / blockEdge`, `bj = ly / blockEdge`;
   `idx = first(L) + bj * blocksX(L) + bi` with `first(L)` as defined above.
3. **Where in the block** - `wx = lx % blockEdge`, `wy = ly % blockEdge`; then
   at the coarsest level `k = wy * blockEdge + wx`, and at any finer level
   ```
   px = wx / 2, py = wy / 2
   sub = ( wx odd && wy even) ? 0 : ((wx even && wy odd) ? 1 : 2)
   k   = (py * (blockEdge/2) + px) * 3 + sub
   ```

**Inflating a block.** The directory's compressed payload is a **plain zlib
stream** (RFC 1950) - the writer strips the four-byte size prefix Qt's
`qCompress` puts on, so no consumer needs Qt. Deflate level 9. The uncompressed
size is in the directory entry, so a reader sizes its output buffer without
guessing.

**A consumer that walks a SUBSAMPLED grid needs more than a one-block cache.**
The progressive pyramid puts consecutive samples of such a grid on *different*
levels: a step of 4 alternates between the level that stores every 4th sample and
the one that stores every 8th, and a one-block cache then misses on every single
sample. The reference reader's `--verify-only` took 4m48s for 18,496 Appalachia
samples before it had one.

**Shared edges.** A Fallout 4 cell's `VHGT` is 33×33: its row 32 and column 32
are the north and east neighbours' row 0 and column 0. This file stores 32 a
cell, so each of those samples has exactly one slot - the north/east cell's row
0 or column 0 - and it holds the **maximum over every cell that carries the
sample**: the cell itself, the one to its south (row 32), the one to its west
(column 32), and for the corner the one to its south-west (`[32][32]`). In a
well-formed record all of them agree and the rule is invisible. 439
Commonwealth cells at the world's rim are flat -352 filler whose edges
disagree with the real terrain beside them; there the slot keeps the terrain.

**A cell with NO `LAND` RECORD still writes a plane, and it is not zero.** Its
row 0 and column 0 are its south and west neighbours' row 32 and column 32, and
those are real terrain; everything else in the cell takes the **worldspace's
default land height** (`WRLD DNAM`). The default deliberately does **not** take
part in the seam maximum on an inherited sample - Far Harbor's default is 0 and
its inherited row is around -250, so a maximum against the default would keep
every inherited sample wrong. A cell with no record and no south, west or
south-west neighbour writes no plane at all, and a reader gets the default land
height for every sample in it.

Measured against the shadow heightmaps, which apply the seam maximum whether or
not the sample's own cell exists: before this rule, DiamondCity differed on
**167,936 of 172,032** texels (164 landless cells against a default of -2048),
NukaWorldAmphitheater on 97, DLC03FarHarbor on 62 (**one** landless cell, ringed
by eight with land). The Commonwealth has **not one** landless cell, which is
why four days of byte-identity gates said nothing. Gated by
`tests/spells/lodl_write.sh`, which bakes NukaWorldAmphitheater's `.lodl` **and**
its heightmap and requires all 114,688 texels to agree, having first checked
that the worldspace HAS landless cells so the check can fail.

The rule was measured before it was chosen: over all 2,322,432 edge texels of
a full ESM dump, it is the only one of six candidates that reproduces the
`Commonwealth_fine` shadow heightmap - 0 mismatches, against 8,675 for "the
cell's own row 0". It was then chosen because the heightmap and this file are
read as ONE surface, terrain from here and far shadows from there, and a row
they disagree on is a ridge that casts a shadow without being drawn. The
cross-check rebuilds the same maximum from the ESM; on the Commonwealth it
raises 129 of 36864 sampled seam positions and every sample still round-trips
exactly. A `.btd` has no shared edge and is unaffected.

**There is no per-block slot table and no present-bitmask**, and both absences
were corrections rather than simplifications:

  * The **quadrant slot table is global** (see Tables). Specifying it per block
    was wrong the moment a block spans more than one cell - a level-3 block
    covers 8x8 cells, so 256 quadrants, which a four-entry block header cannot
    describe. FO76 keeps it global for the same reason.
  * The **alpha present-bitmask was dropped**. Its job was to avoid writing 16
    bits a sample for the 89.5% of quadrants that carry no layers - but those
    samples are a run of identical words, which is exactly what zlib collapses
    to nothing. Measured: adding both the alpha and colour planes took the
    Commonwealth from 27.0 MB to 32.0 MB, about 5 MB for 75 MB of raw planes.
    The bitmask would have bought branching, not bytes.

**LTEX alpha packing**, per sample: five stored alphas, three bits each, `0..7`
- fifteen bits in a uint16. The quadrant's base texture is the **bottom layer**,
carried in slot 5 of the quadrant table and given no alpha of its own. Six
alphas at three bits would be eighteen bits and does not fit; five plus a base
that needs none is how six textures reach a sample.

The five are **independent opacities composited in order over the base**, the
way FO4's own `ATXT`/`VTXT` layers paint over one another. They are *not* a
partition of unity and they do **not** sum to 7.

That distinction was got wrong first, and the way it was got wrong is worth
keeping. An earlier draft of this document asserted the base was *implicit*,
taking "whatever the stored layers do not" - which implies the five can never
sum past 7, a testable invariant. The converter duly tested it and reported
**0 violations in 245,760,000 samples** of `EXM1PittWorldspace.btd`, and that
was written down as confirmation. It confirmed nothing: that worldspace has
**zero land textures**, so every alpha word in it is zero. Running Appalachia,
which has 43, broke the same check on **72% of 21.6 billion samples** at once.

A check that cannot fail on the input it is run against is not a check. The
number is still reported by the converter, as a statistic about the source
rather than as a gate.

None of this changes the bytes: the storage is five 3-bit fields in a uint16 on
both sides, so alpha words are copied through the converter **verbatim**. Only
the documented meaning was wrong.

**Which field is which layer** - measured, not read off the loader. Field `s`
(bits `3s..3s+2`) pairs with slot `s` of the quadrant table; slot 0 is the
**top** layer, drawn last. libfo76utils reverses both the fields and the slot
ids on read so they line up by index, which is where the pairing comes from,
but reading a convention is not the same as checking it. So `--btd-probe`
counts, under each candidate pairing, nonzero alphas whose paired slot is
*empty*: on 441 Appalachia cells, **19,484 of 14.8M** under this pairing
(0.13%, authoring residue with no texture to show) against **5.6M (38%)**
reversed. Ground cover, same test on its mask bits: **0** against 10.6M.

**Bit 15** is not part of the packing. The source codec calls it a sixth
layer's 1-bit alpha; it is set on **0** of 7.2M Appalachia samples measured
and copies through as zero. A FO4 source never sets it.

**Terrain colour** is FO4's `VCLR` or FO76's per-sample colour. It is **not**
derivable from the alphas - independently authored tinting that vanilla
multiplies over the blended terrain - and it is sparse: **2,444 of 36,986**
Commonwealth `LAND` records carry it, 6.6%. The rest write white, which
compresses away.

The stored layout is **R at bits 11-15, G at 6-10, B at 0-4**, bit 5 unused -
the FO4 path's own packing of 8-bit `VCLR`. A `.btd` stores **A1R5G5B5**
(R 10-14, G 5-9, B 0-4, A 15), per libfo76utils' codec for
`pixelFormatRGBA16`, and the converter repacks it. The first converter read it
as RGB565, taking the alpha bit plus four bits of red as "red"; what exposed
that was not a test but a sanity check on the numbers it printed: under 565 the
untinted Pitt worldspace decoded to (24, 16, 16), an asymmetric tint that could
not be neutral, and under A1R5G5B5 to exactly **(16, 16, 16, A=1)** -
mid-grey, the untouched value. Appalachia averages R 14.6 G 14.4 B 14.1 under
the correct layout. The alpha bit is set on every sample measured and carries
no information we keep.

## Ambient occlusion

**A FLAT plane, not blocks** - `uint8`, `cellsX*k` by `cellsY*k` where `k` is the
header's AO samples per cell edge, uncompressed, at its own offset. One byte per
texel, row-major, **row 0 SOUTH** like everything else here, addressed
`aoOffset + ay * (cellsX*k) + ax`. The plane's size is exactly
`cellsX*k * cellsY*k` bytes, which is what makes `--refresh-ao` a single
in-place write.

**255 means "nothing occludes this".** A reader that leaves the plane, or opens a
file with section flag bit 2 clear, must return 255 - the value that changes no
picture - so a consumer that forgot to check `aoSamples` darkens nothing rather
than everything.

**How it is computed**, stated so a consumer can reproduce it rather than guess
at its meaning: from the stored height word at every `spc/k`-th sample, eight
directions (the four axes and the four diagonals), marching `step = 1,2,3,4,7,10`
coarse samples, keeping the steepest upward slope in each direction, summing
`slope/(1+slope)` over the eight, and writing
`clamp(1 - occl/8 * 1.6, 0, 1) * 255 + 0.5`. Sample spacing is `4096/k` world
units, diagonals scaled by 1.41421. The coordinate is clamped to the plane's
edge, so the world's rim marches against itself.

The **same function** serves the writer and `lodtRefreshAo`, reading the file's
own height words back through the reader - which is why a refreshed plane is
byte-identical to a freshly written one rather than merely close.

The spec first called for blocked AO mirroring the terrain pyramid. At 8 samples
a cell the whole Commonwealth is **2.4 MB**, small enough that a block
directory, a compression pass and a second addressing scheme would all be
overhead for nothing.

Coarse by design: AO is smooth and has nothing sharp to lose, so 75 MB at height
rate would buy no detail anyone could see. Measured on the Commonwealth: 1536 by
1536, 255 distinct values.

**What it is for, and what it must not touch.** This is horizon-based sky
occlusion - eight directions, how much of the sky dome a point can see - and it
should gate the **ambient / sky-light term only**. The sun is already shadowed
by the cascades and the baked heightmap; multiplying this plane over the direct
term as well double-darkens every cast shadow at distance. And it is a
**far-field** input: the near field has SSAO, which resolves the same thing at
higher frequency, so the renderer should fade this in as SSAO fades out with
distance rather than sum the two.

That is the same structure Kojima Productions describe for Death Stranding 2 -
a dedicated mid/long-range occlusion map driving skylight, because GI + SSAO
stopped producing enough occlusion once ambient light dominated at distance.
Their stated result is that distant terrain keeps reading as geometrically
complex after the geometry itself has been reduced, which is the whole point of
carrying this plane.

---

---

## What is NOT in this file, and why

| omitted | reason |
|---|---|
| **wetness** | Non-derivable, but measured median 17/255 - a close-up effect in a file viewed at LOD distance. |
| **shore proximity** | Water is *in this file*, so `sample height - local water plane` is a runtime subtraction. Adding water removed the reason to store the channel derived from it. |
| **slope, aspect, curvature** | Derivable from heights, which are in the same file. |
| **sky visibility** | On a heightfield it is the same number as AO - measured r = 0.969. |

**The governing rule for what IS carried: if Fallout 76's format holds it, we
hold it.** FO4CS is writing the reader, so "Fallout 4 has no equivalent" is not
a reason to discard data a FO76 source provides - it only means the section is
absent when the source is an FO4 ESM. That is why terrain colour and ground
cover are in the block payload above rather than in this table, where an earlier
draft had wrongly put them.

---

## Sizes

Uncompressed payload, heights + LTEX alphas, all four levels:

| worldspace | cells | samples/side | uncompressed | expected zlib |
|---|---|---|---|---|
| Commonwealth | 192^2 | 6,144 | ~200 MB | 80-120 MB |
| FO76 port | 804^2 | 25,728 | ~3.5 GB | 1.5-2 GB |

The port matches Appalachia's own sample count (25,728 square) and lands near
its 1.4 GB - the arithmetic closing on itself is a good sign the shape is right.

**For scale, vanilla's entire Commonwealth terrain and object LOD is 230.3 MB** -
3,060 `.BTR` plus 465 `.BTO` across all four levels (FO4CS
`docs/RE/far-field-terrain-lod.md` 9.1). So this file at 80-120 MB replaces the
`.BTR` half of that while carrying strictly more: LTEX alphas at full sample
rate, per-cell water with type, and AO, none of which the meshes hold.

**VRAM is not this number.** The file is streamed: only blocks for visible cells
at their distance-appropriate level are resident, so the working set is bounded
by view distance and the pyramid, not by worldspace size. A resident set of
roughly 100x100 cells at level 0 is about 20 MB of heights. That property is the
whole point - the `.btr` path's cost scales with what was *baked*, this scales
with what is *seen*.

---

## Converting a Fallout 76 `.btd`

**A `.btd` converts to `.lodl` structurally, not by resampling.** Most of it is
a repack; the format was shaped so it can be.

| FO76 `.btd` | `.lodl` | work |
|---|---|---|
| cell bounds, min/max height | same | copy |
| LTEX form IDs | LTEX table | copy |
| GCVR form IDs | GCVR table | copy |
| per-cell min/max height | per-cell table | copy |
| cell quadrant land textures | per-quadrant LTEX slots | copy |
| cell quadrant ground covers | per-quadrant GCVR slots | copy |
| LOD4 arrays | coarse overview | copy the height array |
| height / LTEX blocks | height / LTEX blocks | **payload copies unchanged** |
| terrain colour blocks, A1R5G5B5 | colour in the block payload, 5-5-5 | move into the block, **repack** |
| ground cover mask blocks | ground cover in the block payload | move into the block |
| 128x128 blocks | 128x128 blocks | **block edge is a field** - no resplit |
| progressive, 3/4 per level | progressive, 3/4 per level | **same scheme** |
| 3-bit alphas over a base layer | same packing | copy, verbatim |
| heights normalised min..max | heights at a quantum | **rescale**: `quantum = maxAbs / 32767`, re-centred on 32767 (see Height encoding) |
| int32 offsets from a base | uint64 absolute | widen |
| *(none)* | water | from `SeventySix.esm`: `XCLW`, `XCWT`, `WATR` |

`lodgen --from-btd <file> --btd-probe` answers the layout questions from the
`.btd` alone in about thirty seconds, without writing anything: the field-slot
pairing under both candidates, ground cover likewise, bit 15 usage, and the
colour channel means and distinct-word count under A1R5G5B5. Run it on a new
source before converting it; it is what turned three assumptions in this table
into measurements.

`--verify-only`, on either path, skips the write and runs every cross-check
against an EXISTING `<dir>/Terrain/<name>.lodl`: heights (exact for FO4, half
a quantum for a `.btd`), alpha words, colour words and ground cover, all
against the source. About a minute for Appalachia against twenty-five to
convert it. A consumer that wants to know whether a file still matches its
plugin asks this.

**Only two things are real work.** The height rescale, because their encoding is
range-normalised and ours is quantum-based - and even that is one multiply, with
the quantum chosen so nothing is lost. And water, which has to come from the
plugin because their terrain file carries none at all.

Everything else is a header rewrite and a copy: the block payloads transfer byte
for byte, because the progressive scheme, the 3-bit alpha packing and a
configurable block edge were all chosen to match.

---

## The renderer this is shaped for

**Geometry clipmaps with geomorphing** - nested windows on one global sample
grid, each level covering twice the extent at half the rate, morphing between
levels so nothing pops.

Three properties of the format exist for that consumer:

  * **Progressive blocks.** A clipmap holds every level resident by definition -
    that is what a clipmap *is* - so "you must walk the parent chain" costs
    nothing. It is the same reason geomorphing wants it: the coarse level you
    morph *from* is already loaded.
  * **Subsampled, exactly nesting levels.** Clipmap levels are windows on the
    same grid; morph targets are interpolations of coarse neighbours. Both need
    a coarse vertex to *be* a fine vertex.
  * **The coarse overview** is effectively the outermost clipmap level, and the
    one that must never stall on a decompress.

As the viewer moves, a clipmap updates edge strips rather than whole levels.
Those strips cross block boundaries freely - blocks are 32x32 samples on a
global grid, not an addressing unit the renderer has to respect.

---

## Opening one in NifSkope

`File > Open` on a `.lodl` MESHES it, exactly as it does a FO76 `.btd`: the file
stores no triangles, so a picker asks for a cell rectangle, a detail level and a
**plane**, and `nifCreateLodtTerrainScene` (`src/btdterrain.cpp`) builds a
Fallout 4 document out of the answer. The reader is `src/lodtfile.cpp`; there is
no second one.

  * **Detail level** `L` takes the file's own `samples per cell edge >> L`, so
    the same number means the same number of halvings on both formats while the
    world spacing differs -- which is the whole reason the rate is a header
    field. A bare open takes the whole worldspace at whichever level first fits
    the vertex budget starting from 8 samples a cell.
  * **Heights are the geometry.** The Height view writes no vertex-colour
    channel at all, so its vertex descriptor is the `0x0041B00000650407` the
    `.btd` route has always written and the two scenes are the same bytes.
  * **Every other plane paints that surface as vertex colours**, one at a time,
    and only that plane is read: AO, the land-texture blend composited over the
    quadrant's base, the terrain colour word, the ground-cover mask, per-cell
    water height and type, the per-cell flags and height range, and the coarse
    overview. The build prints what the plane MEASURED -- its range, how many
    samples carry anything -- because a plane that reads back as one constant
    and a plane that was never read look identical in a picture.
  * **A section the file does not carry is refused in words**, naming the
    section flags, rather than drawn black.
  * Headless: `NifSkope.exe -no-gui lodl <file.lodl> --info` prints the header
    and the plane keys; `--region X0 Y0 X1 Y1 --lod N --plane KEY -o OUT.nif`
    builds through the same generator. In the GUI, `WW_LODL_REGION=
    "x0,y0,x1,y1,lod[,plane]"` and `WW_LODL_PLANE=<key>` skip the picker, which
    is how the harness and the render hook drive it.
  * Gate: `tests/spells/lodl_open.sh`, whose right-hand side is
    `tests/spells/lodl_open_authority.py` -- an independent decoder of the
    header, the flat sections and the progressive pyramid, sharing no code with
    `lodtfile.cpp`.

---

## What a reader may assume, and what it must refuse

### Invariants

1. `magic == 'LODT'` and `version` is 1 or 2; the header is exactly 152 bytes
   at version 1 and 160 at version 2. The two version 2 fields sit AFTER the
   ten section offsets, so a reader that takes every section from its offset
   reads both versions with one code path.
2. `fileSize == the u64 at 0x90`, exactly.
3. Cell bounds are inclusive and non-empty: `minX <= maxX`, `minY <= maxY`.
4. `samplesPerCell > 0`, `blockEdge > 0`, `levelCount > 0`, `quantum > 0`.
5. `blockDataOffset >= blockDirectoryOffset`, and
   `blockCount == (blockDataOffset - blockDirectoryOffset) / 16` - the directory
   is the whole span between the two offsets and nothing else is in it.
6. Every section offset is `>=` the header size its version declares (152 or
   160) and every section lies before the block data.
7. A section whose flag bit is clear is **absent**, whatever its offset says. A
   reader checks the **bit**, not the offset.
8. Height decodes as `(word - 32767) * quantum` for **every** grid in the file -
   blocks and overview alike - so the two are directly comparable without a
   conversion.
9. Every sample appears **exactly once** in the pyramid.
10. On an FO4 source, heights round-trip **exactly** against the ESM's `VHGT`; on
    a `.btd` source, within half a quantum (their grid starts at `minHeight`,
    ours is centred on zero, so the two step grids sit half a step apart and only
    a bias field would close it).
11. Alpha words, colour words and ground-cover words round-trip **exactly** from
    either source.
12. On an FO4 source the GCVR count is 0 and the ground-cover section is empty -
    `Fallout4.esm` carries **zero** GCVR records, measured.

### Refusals, by name

The reference reader refuses - naming the field - on: a file shorter than 152
bytes; a bad magic; a version outside 1..2; `blockDataOffset` below the header
size its version declares, or beyond
the file, or before the directory; a total-size field that is not the file size;
empty cell bounds; a degenerate rate, block edge or level count; a non-positive
quantum. It **does not** refuse a missing section: it returns the neutral value
(255 for AO, 32767 for a height, `0xFFFF` for a colour word, 0 for ground cover).

`--verify-only` runs every cross-check against an existing file without writing,
in about a minute for Appalachia against twenty-five to convert it. A consumer
that wants to know whether a file still matches its plugin asks that.

## Sample files

These exist on disk today, written by this generator on 2026-09-05 and read by
FO4CS in its 00:18 session on 2026-09-06:

```
E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl              35,953,286 B
                                             DLC03FarHarbor.lodl             9,195,806 B
                                             NukaWorld.lodl                  7,182,348 B
                                             DiamondCity.lodl                   53,220 B
                                             NukaWorldAmphitheater.lodl         38,104 B
```

The companion shadow heightmaps, which must agree with this file sample for
sample, are beside them under
`Textures\Terrain\Commonwealth\` (`Commonwealth.HeightMap.-96.-96.95.95.-8320.44872.dds`
and `Commonwealth_fine.HeightMap.…`).

**One known disagreement, unfixed.** FO4CS reconstructed all five files through
its own parser and compared against the generator's own heightmaps: Commonwealth
0 wrong of 37.7M pixels, Nuka-World 0 of 4.3M, Diamond City 0 of 172k, **Far
Harbor 62 of 20.2M**. The 62 are the generator writing one extra edge row and
column per cell into the heightmap that it never stores in the `.lodl`. It is a
NifSkope-side gap and it is bungo's call -
`scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md` item 2.

## Open

  * **Compression is zlib**, decided: it is vendored already, the blocks are
    small, and every consumer can already inflate. zstd compresses better and
    faster and would be a new dependency on both sides - a version-2 block flag
    if it is ever wanted.
  * **Reader first.** Nothing verifies a writer except a reader. Two now exist:
    this repo's own `LodtFile` and an independent one in FO4CS written from this
    document, plus `tests/spells/lodl_open_authority.py`, a third decoder that
    shares no code with `lodtfile.cpp`.

---

## Provenance

Every byte offset and rule above was read at these lines. **`src/lodtfile.cpp`
is under active edit by another lane** (it grew from 1,534 to 1,682 lines and
gained header version 2 while this page was being written), so the ANCHOR TEXT
is authoritative and the line numbers are the state below.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodtfile.cpp` | `efaf56673b8e09d2` | 65,592 | 1,706 |
| `src/lodtfile.h` | `804f949cd3d75e4b` | 11,868 | 252 |

| claim | line | anchor |
|---|---|---|
| magic `'LODT'` (unchanged by the `.lodl` rename) | `lodtfile.h` | `constexpr quint32 LODL_MAGIC = 0x54444F4CU;` |
| versions 1..2, header sizes 0x98 / 0xA0 | 48-51 | `constexpr quint32 LODL_VERSION = 2;` … `LODL_HEADER_V2 = 0xA0;` |
| section flag bits 0..3 | 53-56 | `constexpr quint32 SECT_COLOUR = 1u << 0;` |
| cell flag bits, water-type sentinel | 58-60 | `constexpr quint16 WATER_TYPE_DEFAULT = 0xFFFFU;` |
| everything is little-endian | 72 | `//! Little-endian appenders. Everything in the file is LE regardless of host.` |
| AO row 0 is SOUTH | 135 | `Row 0 is SOUTH (the grid's own order, cell row 0 first)` |
| the AO march: 8 directions, step ladder, 1.6 gain | 141-180 | `static QByteArray lodtComputeAo(`, `for ( int step = 1; step <= 12; step += ( step < 4 ? 1 : 3 ) )` |
| overview built cell by cell, row 0 south | 457 | `g[row * ( size_t( cellsX ) * k ) + col] =` |
| written version, `WW_LODL_VERSION`, refusal | 489-500 | `if ( qEnvironmentVariableIsSet( "WW_LODL_VERSION" ) )` |
| the whole header, in write order | 496-523 | `h.u32( LODL_MAGIC );` … `const qsizetype offSizeAt = h.size(); h.u64( 0 );` |
| v2's two default-water fields at 0x98 / 0x9C | 533-534 | `h.f32( src.defaultWaterHeight ); h.u32( src.defaultWaterType );` |
| the writer checks its own header size | 538-540 | `if ( h.size() != ( version >= 2 ? LODL_HEADER_V2 : LODL_HEADER_V1 ) )` |
| section write order and offset patching | 572-645 | `patch64( hdr, offLtexAt, pos );` |
| GCVR table then slots, contiguous, no own offset | 596-597 | `for ( size_t s = 0; s < gcvrSlots.size(); s++ )` |
| per-cell record: 3 floats then 2 u16 | 618-624 | `t.f32( cellMinH[s] );` |
| AO plane written flat at its own offset | 645 | `t.b = lodtComputeAo( aoGrid, cellsX * aoS, cellsY * aoS, aoS, quantum );` |
| `blocksX/Y(L) = ceil(cells / 2^L)` | 652-653 | `auto blocksX = [&]( int L ) { return ( cellsX + ( 1 << L ) - 1 ) >> L; };` |
| plain zlib, level 9, 4-byte prefix stripped | 702-704 | `QByteArray z = qCompress( rawBatch[k], 9 );` |
| directory entry u64 / u32 / u32, absolute | 712-714 | `dirBuf.u64( pos );` |
| directory ordered COARSEST first | 719-720 | `const int coarsest = levels - 1;` … `for ( int L = coarsest; L >= 0; L-- ) {` |
| progressive: full grid coarsest, 3/4 finer | 742-755 | `blk.u16( at( plane, L, ox + x + 1, oy + y ) );` |
| plane order 0 h, 1 alpha, 2 colour, 3 gcvr | 749-754 | `emitPlane( 0 );` |
| the seam MAXIMUM rule | 1066-1070, 1036 | `hh = qMax( hh, sm.sRow[cc] );` |
| five layers ranked by peak opacity | 958-963 | `std::sort( rank.begin(), rank.end(),` |
| FO4 writes no ground cover | 1054-1059 | `FO4 has no ground cover HERE: measured, Fallout4.esm carries 0 GCVR records` |
| height encode `h/quantum + 32767`, round-half-up | 67-70 | `static inline quint16 lodtHeightWord( double h, double quantum )` … `qBound( 0.0, std::floor( h / quantum + 32767.0 + 0.5 ), 65535.0 )` — ONE encoder for every plane and for the landless fallback |
| q order SW/SE/NW/NE | 1073 | `const int q = ( r >= 16 ? 2 : 0 ) + ( cc >= 16 ? 1 : 0 );` |
| alpha packing, five 3-bit fields | 1087-1089 | `word \|= quint16( quint16( qBound( 0.0f, alpha * 7.0f + 0.5f, 7.0f ) ) << ( s * 3 ) );` |
| colour packing R11 G6 B0, bit 5 unused | 1092-1094 | `( ( l->colors[r][cc][0] >> 3 ) << 11 )` |
| `.btd` quantum = `maxAbs / 32767`, floored | 1156 | `const float quantum = qMax( maxAbs / 32767.0f, 1.0f / 4096.0f );` |
| `.btd` rate and block edge forced to 128 | 1162-1163 | `o.samplesPerCell = 128;` |
| `.btd` colour is A1R5G5B5, repacked | 1318-1323 | `const int r = ( v >> 10 ) & 0x1F;` |
| reader reads 0x98 first, then the prefix | 1388-1414 | `buf = file.read( LODL_HEADER_V1 );` … the LDTX and bad-magic refusals |
| the reader's offset refusal | 1410 | `return fail( QStringLiteral( "header offsets do not fit the file" ) );` |
| version range refusal | 1417-1419 | `this reader knows %2..%3` |
| v2 fields read at 0x98 / 0x9C | 1420-1423 | `defWaterH = rd<float>( buf, 0x98 );` |
| every header field's offset, from the reader | 1426-1449 | `spc = int( rd<quint32>( buf, 0x18 ) );` |
| degenerate-field refusals | 1457-1470 | `return fail( QStringLiteral( "degenerate rate/block/level count" ) );` |
| per-cell record addressing, stride 16 | 1488 | `const qsizetype at = qsizetype( oCell ) + s * 16;` |
| quadrant addressing `(qx, qy)`, stride 6 words | 1503-1510 | `const int qx = ( cx - minX ) * 2 + ( quad & 1 );` |
| which level stores a sample | 1519-1527 | `while ( L < coarsest && ( gx % ( 1 << ( L + 1 ) ) ) == 0` |
| `first(L)` sums the coarser levels | 1528-1530 | `for ( int j = coarsest; j > L; j-- )` |
| the 4-byte zlib prefix put back for `qUncompress` | 1554-1558 | `z.resize( 4 );` |
| in-block index at both level kinds | 1592-1602 | `k = ( py * ( blkEdge / 2 ) + px ) * 3 + sub;` |
| ground cover shifts to plane 2 without colour | 1665 | `return planeSample( gx, gy, ( sect & SECT_COLOUR ) ? 3 : 2 );` |
| AO returns 255 when absent | 1682-1686 | `// 255 = nothing occludes this` |
| AO plane addressing | 1689 | `const qsizetype at = qsizetype( oAo ) + qsizetype( ay ) * aw + ax;` |
| the writer's default options, `headerVersion = 2` | `lodtfile.h:78-96` | `int headerVersion = 2;` |
| v1 reports no default water | `lodtfile.h:148-154` | `bool hasDefaultWater() const { return ver >= 2 && defWaterType != 0; }` |
| block cache sizing note for subsampled walks | `lodtfile.h:191-197` | `a consumer that reads a SUBSAMPLED grid needs more` |
