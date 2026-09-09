# The terrain virtual texture — `.lodt` v1, and the ground-cover plane

**THIS EXTENSION WAS REPURPOSED ON 2026-09-09, AND THE MAGIC MOVED WITH
IT.** bungo's ruling: the terrain texture sheets are `.lodt` (they were
`.lodv`), and the whole-worldspace LANDSCAPE file, which was `.lodt`, is
`.lodl` (`docs/LODGEN_BTD_FORMAT.md`). Because the extension now means
something else, the container takes its OWN magic, `LDTX` -- it was `LODV`
-- and both readers refuse the other's file by name: `lodvValidate` names
the landscape file when handed `LODT`, and names a stale `LODV` container
too; `LodtFile::open` names this one when handed `LDTX`. No `.lodv` was
ever written to disk anywhere, so nothing needs converting. The C++ names
(`LodvWriter`, `lodvValidate`, `LODV_ROLE_*`, `src/io/lodvfile.cpp`) did
NOT move.

**Contract version: `magic 'LDTX'`, `version 1`, `headerBytes 256`, tile-table
stride 24, payload alignment 4096.**
**Status: WRITER AND VALIDATOR SHIPPED, off by default** (`--vt`); **NO
CONSUMER** — no `.lodt` reader outside this tree, no tile streamer, no residency
manager. FO4CS's *Improved LOD* module is the first planned one.
**Verified against the writer 2026-09-09** (lane CONTRACTS): every offset,
stride, flag and refusal in §3 was re-read at `src/io/lodvfile.cpp` and matched;
the provenance footer names the lines. No correction was needed.

This is the format contract, the way `docs/LODGEN_VERTEX_PACKING.md` is the
contract for the `.bto` channels. A consumer reads this file; the generator is
`lodgenBakeTerrainVt` in `src/lodgen.cpp` and the container is
`src/io/lodvfile.{h,cpp}`.

**Row order, and the trap.** `.lodt` is **NORTH-UP** — in the tile table and
inside every payload. `.lodl` is **row-0-SOUTH**
(`docs/LODGEN_BTD_FORMAT.md`). Both conventions are live in this codebase and
the mismatch has already cost one consumer a Y mirror, which is why a clear
`ROW_ORDER_NORTH_UP` bit here is a refusal (§3.4 rule 19) rather than a hint.

Two things are described, because they share the same alpha channel:

1. **Ground cover** — the `LTEX → GNAM → GRAS` chain the Creation Kit grows
   grass from, read for the first time, composited per texel against the cell's
   splat paint, gated by terrain slope, and written into the alpha of the
   terrain data sheet, with a matching grass tint mixed into the far albedo so
   that the *stock* engine — which reads no data sheet at all — stops rendering
   meadows as bare dirt.
2. **The pyramid** — the same bake restructured into levels of bordered tiles,
   one binary container per level under `Data\Terrain\`, indexed by a `.lodm`
   of kind `terrainVT`, so a consumer can stream terrain at a fixed memory
   budget instead of loading whole chunk sheets.

Both are **opt-in and off by default**, and off means byte-identical: with
`--no-cover` and `--no-vt` this generator writes exactly the files it wrote
before either existed. That is gated, not asserted
(`tests/spells/lodgen_ground_cover.sh` and `tests/spells/lodgen_terrain_vt.sh`).

---

## 1. Ground cover

### 1.1 What is read

Per LTEX form `L`, over its `GNAM` links `g` (1..4 in the shipped corpus, more
under a grass mod):

```
D(L) = Σ_g density(g)                                (0 when L has no GNAM)
S(L) = ( Σ_g density(g) · maxSlope(g) ) / D(L)       degrees; 0 when D == 0
T(L) = ( Σ_g density(g) · avg(g) ) / Σ_{g: has a tint} density(g)
```

`density` and `maxSlope` are bytes 0 and 2 of the GRAS `DATA` block, which is
32 bytes in every shipped GRAS record; byte 1 (Min Slope) is 0 in all of them,
so a lower gate would be dead code. Bytes 3, 6–7 and 29–31 are stale slots and
are not read. `avg(g)` is the average colour of the grass MESH's diffuse — the
smallest mip of the texture named by `GRAS.MODL`'s one shape, un-premultiplied
by its own alpha above 0.05 — **not** the GRAS record's `Colour Range`, which
is a per-instance random *spread*, and **not** the LTEX's own diffuse, which is
missing on a third of the base game's landscape texture sets.

### 1.2 The per-texel law

Evaluated inside the existing paint loop, on operands it already has:

```
per quadrant:   resolve D, S and T ONCE for the base and every layer
per texel:
  a_i   = the same bilinear opacity the diffuse loop computes, clamped to 0..1
  A     = Σ a_i
  if A > 1:  a_i ← a_i / A ;  A ← 1        (cover side ONLY — see below)
  wBase = 1 − A
  Dtex  = wBase·D(base) + Σ a_i·D(ltex_i)
  Stex  = [ wBase·D(base)·S(base) + Σ a_i·D(ltex_i)·S(ltex_i) ] / Dtex   (0 if Dtex == 0)
  θ     = degrees( acos( clamp(n.z, 0, 1) ) )     the SAME unit normal the msn encodes
  gate  = clamp( (Stex + 5 − θ) / 10, 0, 1 )
  cover = clamp( round( 255 · gate · Dtex / COVER_FULL ), 0, 255 )
```

`COVER_FULL` is **96 by default** and is a *fixed* constant, written into the
container and into the index. 96 is the largest `Density` byte an artist
authored in the shipped corpus, so "one grass at the densest ever authored" is
full cover. `--cover-full` overrides it. Normalising against a per-run maximum
was rejected: it makes two chunks baked in different runs incomparable, and
comparability across runs and across mod setups is the whole point of a
streaming format. A run that saturates says so in its census line
(`clipPainted` / `clipBase`), which is what tells an owner to raise it.

**The renormalised `a_i` never reaches the colour composite.** Renormalisation
is a cover-side correction; the diffuse keeps the opacities it computes today,
or `--cover` and `--no-cover` would paint the handful of texels whose layers sum
past 1 differently and the byte-identity gate would fail for a reason that is
not a cover bug.

**Layer resolution**, chosen so cover and albedo never disagree about what is
growing there:

| case | cover uses |
|---|---|
| `layer.ltex == 0` (NULL) | `D(dominantBase)`, `S(dominantBase)` — the same thing the diffuse paints |
| `layer.ltex` names a form that is not a record | `D = 0`, `S = 0`, and `danglingLtex` increments. A dangling reference is a data error, not paint intent, so it does **not** fall back |
| `land.baseTex[q] == 0` | `dominantBase`, as the diffuse already does |

**The value is ordinal in scale and linear in composition.** It is
`255·gate·Dtex/COVER_FULL`, so 128 does **not** mean "half the ground is grass"
— nothing states what `Density` counts per unit area. But it *is* linear in
`Dtex` everywhere except the clamp, which is why averaging four cover bytes is
the correct filter for a coarser tile, and why the index says
`"ordinal": true, "linearInComposition": true` rather than a bare `ordinal`
that would forbid the filter the pyramid needs.

**Resolution is not the limit for the paint; it is for the gate.** The paint is
33×33 shared-edge samples per cell = 128 units per sample, and the finest
texel is 32 units, so no paint detail is lost and none is invented. The slope
gate does not have that headroom: `θ` comes from central differences over the
same 128-unit heightfield, so it resolves slope four times coarser than the
texel it gates and will over-report cover on ground that is steep at
sub-128-unit scale. **The ±5° half-width is a smoothing constant against a
128-unit operand, not a 32-unit precision claim.**

### 1.3 What it does not model

| not modelled | why |
|---|---|
| **Units From Water** (43 of 107 GRAS carry it) | Whether it is a vertical height difference or a horizontal distance to shore is not established by anything measured, and the two readings need completely different machinery. The data sheet's **B channel already carries shore proximity** at the same resolution, so a consumer that wants to suppress cover near water can do it from a channel we already ship — without the bake guessing. |
| **Per-GRAS multiplicity** | Collapsed to a density-weighted scalar `D` and a density-weighted `S`. Four channels would need room the sheet has not got and a use the consumer has not got at 32 units per texel. |
| **Placement jitter** (`Position Range` up to 76 units) | The field is a probability of cover, not a footprint; at LOD range the smear is sub-pixel. |
| **Object occlusion, precombines, navmesh** | Not in the records read. A paint-derived cover map will show grass under a building whose footprint was never painted out. Vanilla's artists already encoded "same texture, deliberately no grass" as separate records — the `…NoGrass` LTEXes, which carry zero `GNAM` — so the paint carries most of the intent and the residue is an artefact at 32 units per texel. |
| **Engine grass settings** (`iMinGrassSize`, `fGrassStartFadeDistance`) | Runtime policy. The bake describes the ground; the consumer decides where grass stops being drawn. |

### 1.4 Where it is written, and how a reader knows

`<ws>.<dim>.<x>.<y>_data.DDS` is **BC1 (DXT1), 174,888 bytes, alpha 0xFF**
when the chunk has no cover — byte for byte what this generator has always
written — and **BC3 (DXT5), 349,648 bytes, alpha = cover** when it has.

The fourCC is the switch AND it is qualified:

* `hdr[8] = 0x56435757` (`'WWCV'`) and
  `hdr[9] = (coverLawVersion << 24) | round(COVER_FULL)`, `coverLawVersion = 1`,
  in the DDS header's `dwReserved1` (file offsets 32..75, zero in every DDS
  this tree has ever written and ignored by every reader in it).
* **Reader rule:** a DXT5 `_data.DDS` whose `hdr[8]` is not `'WWCV'`, or whose
  decoded alpha is constant 255, carries **no cover** and must be treated as
  DXT1-equivalent.

Without the stamp, an xLODGen sheet's constant-255 alpha would decode as *full
cover on every texel* — grass on rubble and on the ocean floor — and it would
be unrepairable afterwards, because the bytes would contain nothing to repair.

A chunk with no cover writes the alpha 0xFF it always did. Writing cover-0 as
alpha 0 into a BC1 sheet would set punch-through on every block and turn the
sheet — and its transparent index-3 texels — into something new for no reason.

### 1.5 The grass tint

The stock engine reads no data sheet, so the cover plane is worth nothing to
it. The tint is the stock-engine half: it puts the cover into the one texture
the engine definitely samples.

```
Ttex = [ wBase·Dtint(base)·T(base) + Σ a_i·Dtint(ltex_i)·T(ltex_i) ] / Dtint
w    = (cover / 255) · tintStrength          cover = the QUANTISED byte
if w > 0 and Dtint > 0:   color += (Ttex − color) · w
```

`Dtint` sums only the tint-bearing part of each `D`, so a grass whose mesh or
texture cannot be resolved loses its vote on the colour and keeps its vote on
`D`. `tintStrength` defaults to **0.35**.

Placed **after the VCLR multiply**: VCLR is the artist's hand-painted shading of
the *ground*, and the grass sits on top of it; mixed in before, the tint would
be darkened by the artist's dirt. Using the quantised byte, not the float,
means a consumer holding the data sheet reproduces this mix exactly from the
alpha it reads.

**The tint is not invertible.** Recovering the untinted albedo needs per-texel
`Ttex`, which is stored nowhere. `terrain.cover.tintStrength` in the index
records what was folded in so a consumer can **match** it — reproduce the same
mix for geometry it draws itself — not undo it. `--grass-tint 0` writes the
cover plane and leaves the albedo byte-identical, which is the setting for an
FO4CS-only user.

### 1.6 The census line

Printed to stderr, unconditionally under `--cover`, as ONE physical line of
`key=value` tokens with no comma inside any value, because report lines in this
tree are parsed by keyword and never by field position:

```
cover cx=-20 cy=24 dim=4 texels=262144 coverMax=143 paintedPts=… alphaLayerPts=…
  renorm=… maxDtexPainted=… maxDtexBase=… clipPainted=0 clipBase=0
  danglingLtex=0 danglingLtexIds=[] danglingGnam=0 danglingGnamIds=[]
  ltexNoGnam=…/128 grasNoTint=…/107 ltexTotal=… grasTotal=… gnamLinks=…
  ltexWithGnam=… grasDataMin=32 grasDataMax=32 grasWithoutData=0
  grasReads=… nifReads=… texLoads=… ltexResolves=…/… quadrants=…
  coverFull=96.0 tintStrength=0.350 pxNoLand=0 pxNoBase=0 pxUnresolvableLtex=0
```

| class | counters | must be |
|---|---|---|
| **error** | `danglingLtex`, `danglingGnam`, `clipPainted`, `clipBase` | **0 on vanilla** |
| **informational** | `ltexNoGnam`, `grasNoTint` | printed **with a denominator**, never gated at 0 — most landscape textures name no grass *by design*, and nineteen of them are the artists' own "same texture, deliberately no grass" records |
| **census** | `grasReads`, `nifReads`, `texLoads`, `ltexResolves`, `quadrants` | gated as **counts**, which is what makes a per-texel plugin lookup fail deterministically instead of hiding inside a 24-second parse |

---

## 2. The pyramid

### 2.1 Levels, and the one aligned grid

A **level** is named by its `dim` — cells per tile edge. The ladder is ×2 from
the finest level up and it is normative: the header carries `levelDims[8]` so a
consumer holding one container can name its siblings without the index.

The default finest level is **dim 2**, which at 256 content texels is **32 world
units per texel — exactly the density of vanilla's finest terrain ring**. Full
mode (`--vt-finest 1`) adds a dim-1 level at 16 units per texel as a leaf: dim 2
is *always* baked from the paint in both modes, and filtering always starts at
dim 4 reading dim 2, so the two modes produce the same coarse levels and the
same assembled chunk sheets.

**The ladder stops at the coarsest dim that both the worldspace's west and south
divide, and never above 32.** The Commonwealth's −96 divides 1, 2, 4, 8, 16 and
32 and not 64, so there is no single root tile, ever; the dim-32 level (36 tiles
for the Commonwealth) *is* the root, small enough to be permanently resident. A
worldspace that is not tile-aligned **shortens** the ladder and a note says so;
it is not refused, because refusing would give the first non-Commonwealth
worldspace anyone tries a coin-flip chance of failing.

**Every level is anchored to ONE north-west origin**, aligned to the coarsest
dim and therefore to every finer one, so a coarse tile covers **exactly four**
finer ones at every level and a consumer assembling nested grids can take whole
tiles at any level with no resampling. Per-level flooring would have broken this
silently wherever two levels floored the same world edge differently. The index
says `alignedToWorldOrigin: true` and the harness checks the property rather
than trusting the sentence.

### 2.2 Tile geometry and the four sheets

| knob | value | why |
|---|---|---|
| content | 256 texels | lands the default level on vanilla's exact density |
| border | 8 texels a side | a multiple of 4, so a BC 4×4 block never straddles the content/border line — otherwise re-baking a neighbour changes *this* tile's blocks and incremental re-bake and byte-identity both die. Halves cleanly to 4 at mip 1. |
| stored | 272 = 256 + 2×8, 68 blocks | |
| mips | 2 (272 → 136) | mip 1 exists so a trilinear blend to the parent level never has to page the parent. A deeper per-tile chain would re-store the whole pyramid: level *L*'s mip 1 has the same density as level *L+1*'s mip 0. Mip 2 would need border 16. |
| aniso declared | 8 | `B ≥ ⌈A/2⌉` at the sampled mip: mip 0 has 8 ≥ 4, mip 1 has 4 ≥ 4. The container **declares** what its border supports and the consumer clamps its own sampler. |
| sheets | **4** | colour, model-space normal, data, **height** |

**Four sheets, not three.** The fourth is HEIGHT: `R16_UNORM` (DXGI 56),
`pixel = height/8 + 32767` — the encoding the whole-worldspace shadow heightmap
already uses, so the two agree without a consumer converting between them — on
the same tile grid, with the same border, built the same way (finest from the
LAND records, coarser by the box filter of four finer tiles). It is what lets a
consumer build nested grids from the pyramid without going back to the
whole-worldspace heightmap, and it costs 16 bits a texel against the three
colour-class sheets' 12 together: **an all-BC1 tile is 138,720 bytes of colour
classes and 184,960 of height, i.e. the height sheet more than doubles the
tile.** That is the price of carrying geometry, and it is stated here rather
than discovered on disk. The existing full-worldspace heightmap stays exactly as
it is for the shadow path.

Nothing camera-relative, toroidal or morph-banded is baked. Those are runtime
concerns, and baking them would make the files useless to the per-chunk consumer
that comes first.

**The normal sheet is written by the SAME code as a chunk bake.** The height
reconstruction (`lodgenTerrainHeightAt`: bilinear over the 128-unit VHGT grid
with both blend parameters through the quintic ease) and the channel order
(`lodgenTerrainMsnPixel`: R east, G **up**, B north) are one function each,
called by the tile baker and by `lodgenBakeTerrainTextures`. They were twelve
lines COPIED, and the copy kept both of the 2026-09-07 defects the chunk path
had lost -- `int( ngx )` nearest sampling, so all sixteen texels of a 4x4 block
shared one height sample and one normal, and north in green with up in blue.
That reached the stock engine and not only a future consumer, because with
`--vt` on the `.btr` chunk sheets are assembled from these tiles (2.4).

MEASURED offline on the tile's own heights, before the block codec, against
vanilla's shipped sheet for the same tile
(`scratchpad/terrainfix_20260909/vt_msn_sim.py`), tiles `4.-60.36` and
`4.-20.24`: the grid-phase roughness of lane LATTICE fell 2.001 to 0.209 and
2.000 to 0.150 (vanilla 0.065 and 0.031; the known-answer controls read 0.044
on a smooth field and 1.996 on the same field creased every fourth column, a
separation of 45.7x); the left-neighbour difference by x mod 4 went from
98/0/0/0 -- the signature of a zero-order hold -- to 36/98/99/98 against
vanilla's 100/63/64/63; and the mean UP component **as the shader reads it**
went from 0.288 to 0.841 and from -0.151 to 0.943 (vanilla 0.770 and 0.894).
The last of those is the channel order as a picture: with up in blue,
Sanctuary's ground read as facing slightly DOWNWARD.

| sheet | role | format without cover | with cover | colour space |
|---|---|---|---|---|
| 0 | colour | BC1 (71) | BC1 | sRGB |
| 1 | model-space normal | BC1 (71) | BC1 | linear |
| 2 | data — R AO, G wetness, B shore, A cover | BC1 (71) | **BC3 (77)** | linear |
| 3 | height | R16_UNORM (56) | R16_UNORM | linear |

**Per-tile format selection for role 3.** `sheets[k].dxgiFormat` is the format
when that tile's `COVER` bit is clear and `dxgiFormatCover` when it is set; for
every other role the two must be equal. Without this rule a consumer sizing an
upload from a single per-file format would mis-size every cover tile.

### 2.3 The filter

Coarser levels are built from the finer level's **uncompressed staging**, never
from decoded BC blocks: decoding and re-encoding accumulates error at every
level, and the staging costs nothing because the encoder needs it anyway.

Let level *L*'s **content mosaic** `F_L` be the whole rectangle at that level's
density, assembled from the **content regions only** of every tile (borders
excluded — a border is a duplicate of a neighbour's content and including it
would double-count at every seam). Row 0 of the mosaic is the NORTH edge.

A parent tile `(tx, ty)` at level `2·dim` takes its stored texel `(i, j)` from

```
u0 = 2 · ( tx·C + i − B )
v0 = 2 · ( ty·C + j − B )
P(i,j) = ( F(u0,v0) + F(u0+1,v0) + F(u0,v0+1) + F(u0+1,v0+1) + 2 ) >> 2
```

per 8-bit channel independently (16-bit for the height sheet), with `F` edge-
clamped outside the mosaic. **`+2 >> 2`, round-half-up, is the one rounding law**
— the same one `lodgenWriteDds`'s mip chain uses. Two rounding rules for one
filter cannot both hold.

The mosaic is the *definition* and it is what makes a parent's border correct: a
border texel falls outside its own four children's footprint and must come from
a fifth, sixth or seventh child, which "the average of four tiles" cannot
express. It is never materialised — the whole thing would be gigabytes. Only
four child rows are staged at a time (`2p−1`, `2p`, `2p+1`, `2p+2`: a parent's
border reaches 16 child texels past its content, so the row below the obvious
three is needed too), and a row is released the moment no future parent can
reach it.

Two special rules:

1. **The msn sheet is renormalised after the average.** Decode, normalise the
   3-vector, re-encode. A box average of two opposite slopes gives a short
   vector whose decoded tilt magnitude is wrong; the msn is the one sheet whose
   channels are not independent.
2. **The data sheet's alpha averages plainly**, and **a tile with no cover
   stages alpha 0, never 0xFF**. The 0xFF of §1.4 is applied only at pack time
   on the BC1 fallback path and never enters the filter — without that rule a
   parent bordering one grassy child would inherit full cover across three
   quadrants of bare rock. A parent's data sheet is BC3 iff its averaged alpha
   is not everywhere zero.

**Coarse levels are downsamples of fine data, not measurements at that scale,
and that is a documented limitation rather than a bug.** Three of the four data
channels are scale-dependent: AO is a fixed 2,048-unit horizon march, so
`mean(AO) ≠ AO(mean)`; shore proximity is a distance field, and box-filtering a
distance field is not the distance field at half resolution — it fails worst
near the zero crossing, which is the only place it is read; and the msn's
renormalisation fixes the magnitude but the mean of fine normals is still not
the normal of the coarse heightfield. Only **cover**, **albedo** and **height**
filter cleanly. The index says `coarseLevelsAreDownsamples: true` so a consumer
never reads level 16's R as an AO term measured at 256 units per texel.

### 2.4 The chunk sheets

When the pyramid is on and the terrain textures are on, a `.btr` chunk at
`dim = D` is **assembled** from the 2×2 pyramid tiles at level `D/2`, content
regions only, borders cropped, into a 512² staging image that is handed to the
ordinary DDS writer — so its mip chain is built from the assembled image and
not from the tiles' own mips, whose texels are border-contaminated and whose 136
is not a submultiple of the 512 chain.

The assembly happens **inside the pyramid pass**, while the level's two tile
rows are still staged. It cannot be done afterwards from the written container:
that would mean decoding BC blocks and re-encoding them, which §2.3's first
sentence forbids and which would put the assembled sheet a quantisation step
away from a direct bake.

`dominantBase` is scope-dependent — it is what NULL-LTEX layers and
`baseTex == 0` texels paint — so the tile baker computes it over the **enclosing
dim-4 chunk's** cell set, not over the tile's own two cells, and carries it into
the four tiles that compose that chunk.

The sampling grid is identical to a direct bake: at dim 4 a texel centre sits at
`cwX + (px + 0.5)·32`, and the pyramid's composite index gives the same world
points. The `footprint` that picks the source texture's mip is 32 in both cases.

Where it differs, and only there: **the pyramid bakes a one-cell ring around
every tile**, so its AO march and its outer-ring normals have real data, where
the per-chunk path clamps both at the chunk edge. The `.btr` file itself is
untouched — the Land shader still names `<ws>.<dim>.<x>.<y>.DDS` and `_msn.DDS`,
still Shader Type 18, still `UV = (x/4096, 1 − y/4096)` in miniature chunk
space. That is the whole point of assembling the sheet rather than pointing the
mesh at a tile.

**The chunk sheets are not deleted.** The pyramid supplies their bytes; it does
not replace the files. The stock engine needs them and there is no VT consumer
yet.

---

## 3. `.lodt` v1 — the container

**Name:** `Data\Terrain\<EDID>.VT.<dim>.lodt`, one per level.
**Endianness:** little, throughout. **All offsets are absolute file offsets.**
A reader computes `24·tileCount` and `tileTableOffset + 24·tileCount` in
**64-bit**: `tileCount` is u32 and the product with 24 can overflow u32.

`Data\Terrain\` and not `Data\Textures\Terrain\<WS>\`, because the latter is
enumerated by name with a cap that a level's worth of tiles would blow past; and
`Data\Terrain\` is read by exact name and is where the `.lodl` precedent already
lives.

### 3.1 Header — 256 bytes at offset 0

`0xC0..0xFF` is reserved-must-be-zero, so the fields this format will
predictably want next do not each cost a v2 and a whole re-bake. A v1 reader
ignores a zero-filled tail. `COVER_FULL`, the tint strength and the level ladder
are **in the header**, not only in the index, because the coarse root must be
loadable on its own — and a tile's alpha byte is meaningless without its
normalisation constant.

| off | type | name | meaning |
|---|---|---|---|
| 0x00 | char[4] | `magic` | `'L','D','T','X'` (`LODTEX_MAGIC`, 0x5854444C). Deliberately neither `DDS `, nor `LODT` (the LANDSCAPE file's, which this extension named until 2026-09-09), nor the retired `LODV` this container carried before that date: a wrong-but-plausible parse is worse than a refusal, and both of those are refused BY NAME. |
| 0x04 | u32 | `version` | 1 |
| 0x08 | u32 | `headerBytes` | 256 |
| 0x0C | u32 | `flags` | bit 0 `ROW_ORDER_NORTH_UP` (**1**; clear is a refusal), bit 1 `FULL_MODE`, bits 2..31 zero |
| 0x10 | u64 | `fileBytes` | total size of this file |
| 0x18 | u64 | `tileTableOffset` | ≥ 256, 8-aligned |
| 0x20 | u64 | `payloadOffset` | ≥ `tileTableOffset + 24·tileCount`, 4096-aligned |
| 0x28 | u64 | `vhgtCorpusHash` | FNV-1a 64 over every LAND's raw VHGT payload of this worldspace. **Pins heights only.** |
| 0x30 | u64 | `paintCorpusHash` | FNV-1a 64 over the inputs the cover plane and the albedo actually read: every LAND's raw `BTXT`/`ATXT`/`VTXT` in ascending cell order, then every referenced LTEX's `TNAM` and `GNAM`, then every referenced GRAS's `DATA` and `MODL`, in ascending form-id order. This is the hash that catches a grass mod, a retextured splat, or an overridden LTEX — none of which VHGT can see. |
| 0x38 | char[32] | `worldspaceEdid` | ASCII, NUL-terminated, NUL-padded. A 32-character EDID is **refused by the writer**, never truncated. Comparison is case-sensitive. |
| 0x58 | i16×4 | `south, west, north, east` | inclusive cell bounds of the **tile-aligned rectangle this level covers** |
| 0x60 | i16×4 | `worldSouth, worldWest, worldNorth, worldEast` | inclusive cell bounds of the **actual worldspace** (or, for a region bake, of the region — the index then says `partial: true`) |
| 0x68 | u16 | `levelDim` | 1, 2, 4, 8, 16 or 32 |
| 0x6A | u16 | `levelIndex` | 0 = the finest level of the set |
| 0x6C | u16 | `levelCount` | how many levels the set has |
| 0x6E | u16 | `tilesX` | `(east − west + 1) / levelDim`, an **exact** divide |
| 0x70 | u16 | `tilesY` | `(north − south + 1) / levelDim`, an **exact** divide |
| 0x72 | u16 | `contentTexels` | 256 |
| 0x74 | u16 | `borderTexels` | 8 |
| 0x76 | u16 | `storedTexels` | 272 |
| 0x78 | u8 | `mipCount` | 2 |
| 0x79 | u8 | `sheetCount` | 4 |
| 0x7A | u8 | `anisoSupported` | 8 |
| 0x7B | u8 | `compression` | 0 stored raw, 1 zlib (RFC 1950). Any other value is a refusal, so a future codec is a named error and never a misparse. |
| 0x7C | u32 | `tileCount` | `tilesX · tilesY` |
| 0x80 | f32 | `coverNormalisation` | `COVER_FULL` |
| 0x84 | f32 | `tintStrength` | what was folded into the albedo |
| 0x88 | u16[8] | `levelDims` | the ladder, finest first, zero-padded |
| 0x98 | u32 | `indexCrc32` | CRC-32 (0xEDB88320) over the 256 header bytes **with this field zeroed**, then the whole tile table. This closes the hole per-payload CRCs cannot: a flipped bit in an `offset` points the reader at another tile's payload, whose own CRC is valid, and it loads the wrong tile and never notices. |
| 0x9C | u32 | `reserved0` | 0 |
| 0xA0 | ×4 | `sheets[4]` | 8 bytes each: u16 `dxgiFormat`, u16 `dxgiFormatCover`, u8 `role` (0 unused, 1 colour, 2 msn, 3 data, 4 height), u8 `colorSpace` (0 linear, 1 sRGB), u8[2] zero. Sheets beyond `sheetCount` are all zero. |
| 0xC0 | u8[64] | `reserved` | must be zero |

**Padding rule.** `west ≤ worldWest`, `south ≤ worldSouth`, `east ≥ worldEast`,
`north ≥ worldNorth`, with `west ≡ 0 (mod levelDim)`, `south ≡ 0 (mod levelDim)`
and both spans exact multiples of `levelDim`; the pad is the minimum that
satisfies all of it. Cells outside the worldspace are baked as absent-neighbour
edge replicate and their tiles are still PRESENT. Without the padded/unpadded
split, `tilesX` would be a truncating divide validated against the same
truncating divide, and a worldspace whose span is not a multiple of `levelDim`
would silently drop its east and north edge cells while passing every check.

Derived world units are `[west·4096, (east+1)·4096] × [south·4096,
(north+1)·4096]`, the identical arithmetic the heightmap loader uses.

**Deliberately absent:** per-tile world rectangles (derive them), per-tile
min/max height (that is the heightmap's and the `.lodl`'s business, and a second
source of truth for terrain height is a bug generator), LTEX form ids, material
names, source paths, any per-tile string, and per-mip offsets (a tile's mips are
contiguous inside its own payload, so one offset addresses the whole tile).

### 3.2 Tile table — fixed stride 24 bytes, at `tileTableOffset`

Row-major, `index = ty · tilesX + tx`, with **`ty = 0` the NORTH row** of the
padded rectangle and `tx = 0` its west column.

```
cells x ∈ [ west  + tx·levelDim ,  west  + (tx+1)·levelDim − 1 ]
cells y ∈ [ north − (ty+1)·levelDim + 1 ,  north − ty·levelDim ]
```

| off | type | name |
|---|---|---|
| 0x00 | u64 | `offset` — absolute; **0 exactly when the tile is absent** |
| 0x08 | u32 | `storedBytes` — on disk, after compression |
| 0x0C | u32 | `rawBytes` — after inflate; must equal the size the header + this entry's `COVER` bit imply |
| 0x10 | u32 | `crc32` — over the `storedBytes` on disk |
| 0x14 | u16 | `flags` — bit 0 `PRESENT`, bit 1 `COVER`, bits 2..15 zero |
| 0x16 | u16 | `reserved` — 0 |

**An absent tile's 24 bytes are all zero**, not just `offset`: the other five
fields are validated only when `PRESENT` is set, so without this a reader
summing `storedBytes` over the table would sum garbage. The explicit present
flag exists because "offset 0" is exactly the sentinel that gets misread.

### 3.3 Payload

Each present tile's payload begins at a **4,096-byte-aligned** absolute offset
≥ `payloadOffset`, tiles appear in **table-index order**, and **every alignment
pad byte is zero**. Those three rules are what make two runs of the same bake
byte-identical.

**Row order inside a payload is north-up, west-first.** Block row 0 is the
tile's north edge (including its north border), block column 0 its west edge —
and *not* `.lodl`'s row-0-south. Without this sentence a consumer has a 50%
chance of a mirrored world and no way to tell from the file.

Raw payload = the concatenation, in exactly this order, of

```
sheet 0 mip 0, sheet 0 mip 1, sheet 1 mip 0, sheet 1 mip 1,
sheet 2 mip 0, sheet 2 mip 1, sheet 3 mip 0, sheet 3 mip 1
```

Rows are **tightly packed** at `blocksX · blockBytes` for a block sheet and
`storedTexels · 2` for the R16 height sheet. No row padding and no 256-byte
alignment: an upload path with a caller-supplied row pitch accepts any declared
pitch, and the 256-byte-row rules people quote are D3D12 upload-heap rules.

Computed raw size, at content 256 / border 8 / 2 mips:

| | colour | msn | data | height | tile |
|---|---|---|---|---|---|
| no cover | 46,240 | 46,240 | 46,240 | 184,960 | **323,680** |
| cover | 46,240 | 46,240 | 92,480 | 184,960 | **369,920** |

The payload must **not** be progressive: unlike `.lodl`'s height blocks, a
parent's 4×4 BC block is not a subset of a child's texels, and the whole-tile
upload is the operation a consumer actually performs.

**Compression.** `compression = 0` (raw) is the default. When it is 1, **every
present tile is one whole-payload zlib stream** — one seek and one inflate,
because a resident tile always needs every sheet at once, and a mixed file would
make the reader guess. Writer constraints, because a consumer's inflater may be
hand-written and header-only: **CM = 8, CINFO ≤ 7, FDICT clear**, and
`(CMF << 8 | FLG) % 31 == 0`. `storedBytes > rawBytes` is **legal** —
incompressible BC data plus zlib's stored-block overhead reaches it.

### 3.4 Reader validation rules

Refuse — **by name, with the field that failed** — on any of:

1. file smaller than 256 bytes
2. `magic != 'LDTX'` — and `LODT` (the landscape file, now `.lodl`) and the retired `LODV` are each named in the refusal rather than lumped into "bad magic"
3. `version != 1`
4. `headerBytes != 256`
5. `fileBytes` != the actual file size
6. `worldspaceEdid` not NUL-terminated inside its 32 bytes, empty, or holding a byte outside 0x20..0x7E
7. `north < south`, `east < west`, `worldNorth < worldSouth`, `worldEast < worldWest`, or the padded rectangle not containing the world rectangle
8. `levelDim ∉ {1,2,4,8,16,32}`, or `west % levelDim != 0`, or `south % levelDim != 0`
9. either padded span not a whole number of tiles; or `tilesX`/`tilesY` disagreeing with the rectangle; or either < 1
10. `tileCount != tilesX·tilesY` (computed in 64-bit)
11. `contentTexels` not a power of two in 128..1024; `borderTexels % 4 != 0`; `storedTexels != content + 2·border`
12. `mipCount < 1`, or `(borderTexels >> (mipCount−1)) % 4 != 0`, or `(borderTexels >> (mipCount−1)) << (mipCount−1) != borderTexels`, or `(contentTexels >> (mipCount−1)) < 4` — **a multiple of 4 at the coarsest stored mip**, not merely even
13. `sheetCount` outside 1..4; a used sheet with `role == 0` or a duplicated role; a role-1/2/3 sheet whose format is not one of {71, 72, 77, 78} or a role-4 sheet whose format is not 56; `colorSpace > 1`; `dxgiFormatCover != dxgiFormat` on a sheet whose role is not 3; a sheet past `sheetCount` that is not all zero
14. `compression ∉ {0,1}`
15. `tileTableOffset < 256` or not 8-aligned; `tileTableOffset + 24·tileCount > payloadOffset`; `payloadOffset > fileBytes` or not 4096-aligned
16. any entry where `PRESENT` disagrees with `offset != 0`; any absent entry whose 24 bytes are not all zero; or, if present: an unknown flag bit, a non-zero `reserved`, `offset < payloadOffset`, `offset % 4096 != 0`, `offset + storedBytes > fileBytes`, `storedBytes == 0`, `rawBytes` != the size the header and the `COVER` bit imply, or `compression == 0 && storedBytes != rawBytes`
16b. `compression == 1` and a present tile whose first two bytes are not a valid zlib header with CM = 8, CINFO ≤ 7, FDICT clear and `% 31 == 0`
17. every tile present-flagged 0 (a level with no tiles is a broken bake, not an empty world)
18. **`vhgtCorpusHash` or `paintCorpusHash` != the consumer's own hash** of the worldspace it is loading — the same refusal a stale heightmap already earns. *This is the one rule the file-local validator cannot make: it needs the plugin. `lodgen --corpus-hash` prints both hashes for the comparison.*
19. `flags` bit 0 (`ROW_ORDER_NORTH_UP`) clear — no other row order is defined
20. `indexCrc32` != the recomputed CRC over the zeroed-field header plus the tile table
21. `anisoSupported > 2 · (borderTexels >> (mipCount−1))`
22. `levelDims[levelIndex] != levelDim`; `levelDims` not strictly ascending in its non-zero prefix; the prefix length != `levelCount`; `levelIndex ≥ levelCount`

`crc32` is checked **per tile at load time**, not at open — checking every CRC to
open a file would cost the whole point of the index. `indexCrc32` **is** checked
at open: it covers a few hundred kilobytes at most and it is what stops offset
aliasing.

> **Why the reader's `contentTexels` range is wider than the writer's.** Rule 11
> accepts 128..1024; `--vt-content` refuses above 512. The asymmetry is
> deliberate: the writer's ceiling is a *cost guard*, not a format limit, and a
> reader that refused a well-formed 1024 container written by a future tool
> would be wrong. Liberal reader, conservative writer.

---

## 4. The index — a `terrainVT` `.lodm`

**Path:** `Data\Terrain\<EDID>.VT.lodm` — deliberately **not** under
`materials\`, so `lodmSourceCandidate()` can never produce it and the readers
that ignore `kind` can never open it. That function unconditionally prepends
`materials\` for a diffuse and strips only a leading `data\` for a material, so
`Terrain\…` is unreachable from any source lookup.

No parser change was needed. The envelope stays version 1, `lodm` stays 1, and
`family` is `"legacy"` because the parser hard-rejects a third family word and a
new one would split the corpus. **`family` is vestigial for
`kind: "terrainVT"`; `kind` is the discriminator**, and `src/io/lodmfile.h` says
so, which is where that collision gets resolved rather than discovered.

**The per-tile table is not in the index.** Tens of thousands of tiles at ~30
bytes of JSON each is a megabyte parsed on every load, against a few hundred
kilobytes of fixed-stride binary that needs no parse at all. The index names the
containers; the containers carry the tables.

Hash strings are `0x` plus 16 uppercase hex digits (a JSON number would not
survive a double) and the comparison against a container's u64 is numeric.
`levels[].container` paths are **Data-relative** with backslashes and no leading
`Data\`.

```json
{
  "lodm": 1, "family": "legacy", "kind": "terrainVT",
  "terrain": {
    "worldspace": "Commonwealth",
    "extent": { "south": -96, "west": -96, "north": 95, "east": 95 },
    "cellUnits": 4096, "content": 256, "border": 8, "stored": 272,
    "mips": 2, "aniso": 8, "rowOrder": "northUp", "compression": "none",
    "vhgtCorpusHash": "0xD8337D022F637F22",
    "paintCorpusHash": "0x…",
    "sheets": [
      { "role": "color",  "dxgi": 71, "dxgiWithCover": 71, "colorSpace": "sRGB",
        "channels": "RGB albedo, grass tint folded in" },
      { "role": "msn",    "dxgi": 71, "dxgiWithCover": 71, "colorSpace": "linear",
        "channels": "model-space normal, 0.5+0.5 encoded" },
      { "role": "data",   "dxgi": 71, "dxgiWithCover": 77, "colorSpace": "linear",
        "channels": "R sky-free AO, G flow wetness, B shore proximity, A ground cover" },
      { "role": "height", "dxgi": 56, "dxgiWithCover": 56, "colorSpace": "linear",
        "channels": "R16_UNORM, height/8 + 32767, the shadow heightmap's own encoding" }
    ],
    "cover": { "present": true, "normalisation": 96,
               "ordinal": true, "linearInComposition": true, "tintStrength": 0.35 },
    "coarseLevelsAreDownsamples": true,
    "alignedToWorldOrigin": true,
    "levels": [
      { "index": 0, "dim": 2, "tilesX": 96, "tilesY": 96,
        "worldUnitsPerTile": 8192, "contentTexels": 256, "unitsPerTexel": 32,
        "container": "Terrain\\Commonwealth.VT.2.lodt", "tiles": 9216, "present": 9216 }
    ]
  }
}
```

`worldUnitsPerTile` and `contentTexels` are **stated per level, not implied**:
a consumer picking clipmap rings reads those two numbers at load time rather
than deriving them from `dim` and a constant it has to know.

---

## 5. The CLI

| flag | default | effect |
|---|---|---|
| `--cover` / `--no-cover` | off | bake ground cover and the grass tint |
| `--grass-tint F` | 0.35 | 0 keeps the albedo byte-identical and still writes the plane |
| `--cover-full N` | 96 | the fixed normalisation constant, 1..65535 |
| `--dump-cover FILE` | — | also write the raw 512² u8 plane, north-up, headerless |
| `--vt DIR` / `--no-vt` | off | write the pyramid under `<DIR>/Terrain/` |
| `--vt-finest 1\|2` | 2 | cells per tile at the finest level |
| `--vt-content N` | 256 | power of two, 128..512 (a cost guard; the reader accepts 128..1024) |
| `--vt-border N` | 8 | multiple of 4, and still a multiple of 4 after `mips−1` halvings |
| `--vt-mips N` | 2 | stored mips per tile |
| `--vt-compress none\|zlib` | none | payload compression |
| `--vt-btr` / `--no-vt-btr` | on | assemble the `.btr` chunk sheets from the pyramid |
| `--vt-estimate` | — | print the cost and exit without baking |
| `--lodm-check PATH` | — | parse a `.lodm` through this tree's own parser |
| `--lodt-check PATH` | — | validate a `.lodt` by every rule of §3.4, checking every tile CRC |
| `--corpus-hash` | — | both corpus hashes and the LTEX/GRAS census, with no bake |

---

## 6. What is deliberately not done

* **No consumer.** This lane produces files. There is no `.lodt` reader, no tile
  streamer, no residency manager and no terrain-colour consumer yet.
* **No indirection texture, no feedback pass, no physical pool atlas, no
  residency policy, no anisotropy choice.** Every one depends on a pool size and
  a camera the bake does not have; pre-packing tiles into a pool would freeze a
  consumer choice and destroy per-tile streaming. The container **declares** what
  its border supports and the consumer clamps its own sampler.
* **Nothing clipmap-specific.** No camera-relative data, no toroidal layout, no
  morph bands. What a clipmap needs and gets here is the height sheet, one
  aligned grid, and the level ratios stated rather than implied.
* **No water filter on cover.** §1.3; the B channel already carries shore
  proximity.
* **No `_msn` format change.** Vanilla ships DXT5 with a constant-255 alpha; we
  ship BC1 and lose nothing measurable. Changing it is a separate decision with
  its own evidence.
* **No dim-2 or dim-1 `.btr` chunks.** The chunk builder refuses any dim but
  4/8/16/32 and that guard stays: the pyramid's finest levels are baked by the
  *tile* baker, which does not go through it.
* **No untinted pyramid colour sheet.** §2.4 requires the pyramid to be able to
  supply the `.btr` bytes unchanged, so it inherits the tinted albedo. The
  consequence is a real fork, stated rather than hidden: a consumer that draws
  real grass cannot recover the untinted ground from a tinted bake, and
  `--grass-tint 0` is an either/or chosen at bake time, not at load time.

---

## 7. Sample files

**None exist.** No `.lodt` container and no `<EDID>.VT.lodm` index has been
written to disk in this tree or in bungo's mod folder; the pyramid is off by
default (`--vt`) and has never been run for a whole worldspace here. See
`scratchpad/handoff_fo4cs/README.md` §5 for what a first lane must produce and
what it costs.

`lodgen --lodt-check PATH` validates a container by every rule of §3.4 including
every tile CRC, and `--lodm-check PATH` parses an index through this tree's own
parser, so a produced sample can be gated the moment it exists.

---

## Provenance

Section 3 (the container) was re-read against the writer on 2026-09-09; every
offset, stride, flag value and refusal below matched the document as it already
stood. Sections 1, 2 and 4 are bake laws rather than byte layout and are traced
to `src/lodgen.cpp`.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/io/lodvfile.cpp` | `d84adebaf13d6014` | 30,154 | 744 |
| `src/io/lodvfile.h` | `8274ac90ce51055c` | 8,076 | 196 |
| `src/lodgen.cpp` | `6d7388c53a13343e` | 357,479 | 8,286 |

| claim | line | anchor |
|---|---|---|
| magic `'LDTX'`, version 1, header 256 B | `lodvfile.h:67`, `lodvfile.cpp:29-30` | `constexpr quint32 LODTEX_MAGIC = 0x5854444CU;` (in `lodvfile.h`) |
| payload alignment 4096 | `lodvfile.cpp:32` | `constexpr quint64 LODV_PAYLOAD_ALIGN = 4096;` |
| the whole §3.1 header, field by field | `lodvfile.cpp:110-155` | `put32( h + 0x00, LODTEX_MAGIC );` … `put8( s + 5, f.sheets[i].colorSpace );` |
| the same offsets on the read side | `lodvfile.cpp:159-201` | `payloadOffset = get64( h + 0x20 );` |
| `indexCrc32` at 0x98, zeroed while hashing | `lodvfile.cpp:142, 382-391, 568-576` | `// 0x98 indexCrc32 stays zero here, 0x9C reserved0 zero` |
| CRC-32 polynomial 0xEDB88320 | `lodvfile.cpp:234` | `c = ( c & 1 ) ? ( 0xEDB88320U ^ ( c >> 1 ) ) : ( c >> 1 );` |
| a 32-byte EDID is refused, not truncated | `lodvfile.cpp:275` | `fields.worldspaceEdid.isEmpty() \|\| fields.worldspaceEdid.size() > 31` |
| the rect field order south/west/north/east ×2 | `lodvfile.cpp:121-123` | `const qint16 rect[8] = { f.south, f.west, f.north, f.east, …` |
| §3.2 tile entry, 24 B, field by field | `lodvfile.cpp:379-384`, read at `607-612` | `put64( p + 0x00, e.offset ); put32( p + 0x08, e.storedBytes );` |
| payload offset = table end aligned up to 4096 | `lodvfile.cpp:284-286` | `d->payloadOffset = alignUp( d->tableOffset + quint64( LODV_TABLE_STRIDE ) * d->table.size(), …` |
| every pad byte is zero | `lodvfile.cpp:293-296` | `// pad up to payloadOffset stays zero, which is what makes two runs match` |
| §3.4 rules 1, 2, 19, 20, 21, 22 by number | `lodvfile.cpp:428, 434, 553, 588, 556, 562` | `// rule 19`, `// rule 21`, `// rule 22`, `// rule 20` |
| table must not run into `payloadOffset` | `lodvfile.cpp:549-552` | `refused: the tile table runs into payloadOffset` |
| a tile offset below `payloadOffset` or unaligned | `lodvfile.cpp:628-629` | `refused: tile %1 offset is below payloadOffset or not 4096-aligned` |
| per-tile CRC checked at load, not at open | `lodvfile.cpp:664-670` | `refused: tile %1 crc32 %2, recomputed %3` |
| zlib header constraints CM 8 / CINFO ≤ 7 / %31 | `lodvfile.cpp:656-660` | `if ( ( b0 & 0x0F ) != 8 \|\| ( b0 >> 4 ) > 7 \|\| ( b1 & 0x20 )` |
| the sheet roles and the DXGI set | `lodvfile.h:79-96` | `LODV_ROLE_HEIGHT = 4`, `LODV_DXGI_R16_UNORM = 56` |
| flags: north-up = refusal, full mode | `lodvfile.h:92-96` | `LODV_FLAG_ROW_ORDER_NORTH_UP = 1, //!< clear is a refusal` |
| tile flags PRESENT / COVER | `lodvfile.h:101-105` | `LODV_TILE_COVER = 2` |
| `dxgiFormatCover` differs only on role 3 | `lodvfile.h:104-110` | `quint16 dxgiFormatCover = 0; //!< and when it is SET; equal except on role 3` |
| §4 index: `kind: "terrainVT"` and its whole payload | `lodgen.cpp:6758-6840` | `root.insert( QStringLiteral( "kind" ), QStringLiteral( "terrainVT" ) );` |
| `aniso` written as `min(16, 2·(border >> (mips−1)))` | `lodgen.cpp:6772` | `t.insert( QStringLiteral( "aniso" ), qMin( 16, 2 * ( border >> ( mips - 1 ) ) ) );` |
| `coarseLevelsAreDownsamples` / `alignedToWorldOrigin` | `lodgen.cpp:6812, 6817` | `t.insert( QStringLiteral( "coarseLevelsAreDownsamples" ), true );` |
| `partial: true` on a region bake | `lodgen.cpp:6819` | `t.insert( QStringLiteral( "partial" ), true );` |
| per-level `worldUnitsPerTile` / `unitsPerTexel` | `lodgen.cpp:6831-6833` | `o.insert( QStringLiteral( "unitsPerTexel" ), levels[l].dim * 4096 / content );` |
