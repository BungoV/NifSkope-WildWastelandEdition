# The terrain virtual texture — `.lodt` v2, and the ground-cover plane

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

**Contract version: `magic 'LDTX'`, `version 2`, `headerBytes 256`, tile-table
stride 24, payload alignment 4096.**

**VERSION 2, 2026-09-11 — THE SHEETS TOOK THE OBJECT TEXTURE FAMILY.** bungo's
ruling, verbatim: *"you can mirror how it's set up for the .lodm"*, and
*"we just add the coverage for whatever's missing in terrain textures that lod
objects have in the texture department"*. Version 1's third sheet was role 3,
`data` — R sky AO, G flow wetness, B shore proximity, A ground cover, a set of
channels terrain invented for itself. Version 2 replaces it with the object
family's own (`docs/LODGEN_LODM_FORMAT.md` §2.1):

* **role 5 `mask`** — the `rmaos` slot's channels in the `rmaos` slot's order:
  **R roughness, G metallic, B AO, A ground cover**;
* **role 6 `emissive`** — RGB, BC1, written ONLY when at least one layer's
  material supplies an emissive map, absent otherwise and named as absent in the
  index;
* colour, model-space normal and height are unchanged;
* **shore proximity and wetness are DROPPED.** Shore is a runtime subtraction
  from the `.lodl` water planes (`docs/LODGEN_BTD_FORMAT.md`, "What is NOT in
  this file, and why", already says so of the landscape file); wetness is a
  close-up effect and far wetness is a weather state the runtime owns.

`family` in the index is **`"pbr"` and it means it** — it was `"legacy"` and
this page called it vestigial. A legacy material is CONVERTED at bake (gloss
inverted into roughness, metallic 0), so what ships is PBR whatever the source
was, and the index carries a per-layer census of which rule served each
landscape texture so the word can be audited rather than trusted.

**A v1 file is REFUSED, not converted** (§3.4 rule 3). No `.lodt` pyramid has
ever been written to disk outside this tree — the writer is opt-in behind
`--vt`, and bungo's installed `Data\Terrain` holds no `.lodt` (checked
read-only, 2026-09-11) — so there is nothing in the world to convert, and a
converter would be a second definition of channels that no longer mean the same
thing.

**The `.btr` chunk sheets did not change.** `<ws>.<dim>.<x>.<y>_data.DDS` on the
stock path still carries R AO, G wetness, B shore, A cover under §1.4's stamp,
because the stock engine reads those files and their bytes are pinned by a
byte-identity gate. The pyramid still STAGES that plane to supply them (§2.4);
what changed is what the CONTAINER stores.
**Status: WRITER AND VALIDATOR SHIPPED, off by default** (`--vt`); **NO
CONSUMER** — no `.lodt` reader outside this tree, no tile streamer, no residency
manager. FO4CS's *Improved LOD* module is the first planned one.
**Re-derived against the writer 2026-09-11** (lane TERRAIN-R): §2.2, §2.5, §3 and
§4 were rewritten for version 2 and every line number in the provenance footer
was found again from its own anchor text against the sources stamped there.

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
**alpha-weighted** mean, `Σ rgb·a / Σ a`, over the first mip no longer than
1024 of the texture named by `GRAS.MODL`'s one shape, refused when the mean
alpha is under 0.05 — **not** the GRAS record's `Colour Range`, which
is a per-instance random *spread*, and **not** the LTEX's own diffuse, which is
missing on a third of the base game's landscape texture sets.

It was once the smallest mip divided by its own alpha. That is wrong: a DDS
mip chain is straight alpha, so the smallest mip's RGB is the unweighted
average over the atlas's transparent gaps, and dividing by a mean alpha of
0.07–0.30 clamped every alpha-cut grass under Sanctuary and cell −24,−8 to
white (lane SEAM1, `grass_census.txt`): the cover tint washed his green grass
sandy. Measured against mip 0, the 1024 cap moves the mean by ≤ 2.5/255
(`grass_mipcheck.py`).

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
| `layer.ltex == 0` (NULL) | the engine default (`ESM_LTEX_ENGINE_DEFAULT`): `D = 0`, `S = 0` — the same thing the diffuse paints |
| `layer.ltex` names a form that is not a record | `D = 0`, `S = 0`, and `danglingLtex` increments. A dangling reference is a data error, not paint intent, so it does **not** fall back |
| `land.baseTex[q] == 0` | the engine default, as the diffuse already does |

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

## 1a. Roads and decals in the far colour

bungo, 2026-09-11 10:0x, verbatim: *"We do the same with roads and decals as
vanilla"*. What vanilla does was MEASURED first, on Bethesda's own shipped
`Textures\Terrain\Commonwealth\Commonwealth.4.-20.20.DDS` -- the Sanctuary
loop-road chunk, cells -20..-17 x 20..23, 512 texels at 32 world units each --
against an independent top-down projection of the placed road meshes
(lane ROADS1, `scratchpad/lane_roads1_report.md` section 1).

### 1a.1 What vanilla was measured to do

1. **The road is not in the LAND paint.** All sixteen cells of that chunk have a
   LAND record, and the sixteen landscape textures painted across them are
   `LDriedGrass01`, `LDriedGrass01NoGrass`, `LRubbleRock01`, `LDirtGravel01`,
   `LRiverbedSilt01`, `LDriedGrass02Weeds`, `LForestFloor01`, `LRootsEroded01`,
   `LRootsEroded01Grass01`, `LDirtGravel01Grass`, `LRiverbedSilt01Wet`,
   `LRubbleRock01Grass`, `LRiverbedRocks01Grass`, `LRiverbedRocks02Wet`,
   `LRiverbedRocks02WetGrass`, `LDebrisGround`. Not one road, asphalt, concrete
   or pavement texture among them.
2. **It is at the road MESHES' footprint, and at no other family's.** Each
   family's projected footprint was scored against vanilla's sheet, each against
   ITS OWN floor -- the same mask displaced five ways, area, shape and spatial
   spectrum preserved, registration destroyed. AUC of the sheet's brightness as
   a detector of the mask:

   | family | texels | AUC(bright) | its displaced floor | AUC(grey) | floor |
   |---|---|---|---|---|---|
   | **road** | 23,321 | **0.716** | 0.470 .. 0.601 | **0.678** | 0.448 .. 0.513 |
   | generic `bDecal` shapes | 37,993 | 0.564 | 0.494 .. 0.603 | 0.518 | 0.477 .. 0.509 |
   | trees | 51,462 | 0.529 | 0.454 .. 0.533 | 0.482 | 0.480 .. 0.528 |
   | rocks / cliffs | 42,599 | 0.448 | 0.388 .. 0.580 | 0.527 | 0.457 .. 0.578 |
   | architecture | 18,313 | 0.621 | 0.513 .. 0.614 | 0.536 | 0.479 .. 0.509 |
   | set dressing | 6,491 | 0.560 | 0.459 .. 0.597 | 0.559 | 0.464 .. 0.498 |

   Ceiling (a mask scored by itself) 1.000. Only `road` clears its own floor on
   both scores.

   **Provenance, added by lane ROADS2.** The brightness column reproduces
   exactly -- 0.628 re-measured against the 0.629 recorded. The GREYNESS column
   does **not**, and the fault is in the score, not in either script.
   Saturation on this sheet takes 281 distinct values over 262,144 texels and
   ONE of them covers 140,305 -- 53.5 percent of the tile -- while `tile2.auc()`
   ranks with `np.argsort`, which breaks ties by array index in raster order. So
   a spatially clustered mask gets a grey AUC that depends on WHERE its texels
   sit inside the tie block. Re-running ROADS1's own script today gives 0.757
   where the table says 0.716; with the ties broken at random it is 0.731
   repeatably (0.7316 / 0.7308 / 0.7308 on three seeds); **tie-averaged, which
   is the AUC's own definition when ties exist, it is 0.731**. Brightness has
   1,499 distinct values and no dominant block, which is why that half
   reproduces. **Read every grey AUC in this section to two digits at most, and
   gate on the brightness column.** `scratchpad/roads2_20260911/gate4b.py` has
   the tie-averaged implementation and the audit.
3. **The colour is the road material's own diffuse under the sheet's own
   grading.** `SancRoad01_d.dds` averages luminance 112.8 and vanilla's sheet
   reads 92.6 inside the footprint -- a factor 0.82. `DriedGrass01_D.dds`
   averages 100.2 and the sheet reads 82.9 on the plain background -- 0.83. The
   same grading on both, to within a percent.
4. **The `_msn` normal sheet does NOT carry it.** Vanilla's `_msn` against a
   normal computed from the LAND heightmap alone disagrees by 13.58 deg on the
   road footprint and 14.14 deg on the background (displaced control 14.45) --
   the road agrees with the bare heightmap slightly BETTER than its
   surroundings, where a baked road mesh would have to disagree. The statistic
   has range: against straight up instead of the heightmap normal the same
   comparison reads 14.39 deg on the road and 18.49 deg on the background.

### 1a.2 The rule

> Road meshes are scan-converted top-down into the **colour sheet only**, at the
> mesh's own footprint, with the mesh's own material diffuse. The normal sheet
> stays the heightmap's. No other object family is baked.

### 1a.3 The mesh test

`lodgenIsRoadModel()` plus the caller's `STAT` requirement. **Component
equality, never a substring** -- `MISTAKES.md`'s "sTREEt" lesson has a live
counter-example in Sanctuary, where twelve `SetDressing\RailRoad\
WaxCandle02Off.nif` are placed and the letters `road` are in every one of their
paths:

    the model path, separators normalised, lower-cased, a leading `meshes`
    component dropped, has `landscape` as its FIRST component and `roads` or
    `sidewalks` as its SECOND, with at least one component after them.

`Landscape\Sidewalks\*` **was** carried by the rule on the argument that it is
the same road surface under the same folder root, and that inclusion was
recorded here as untested -- it contributes only 187 of the Sanctuary chunk's
texels against `Landscape\Roads`' 23,170. **Lane ROADS2 tested it on a tile that
has enough of it and reversed it.** See 1a.3b.

Two other rules decide which paths are roads at all:

* `lodgenIsRaisedRoadModel()` -- `landscape` / `roads` /
  (`highwayoverpass` | `bridge`) / at least one more component, by the same
  component equality;
* `lodgenIsSidewalkModel()` -- `landscape` / `sidewalks` / at least one more
  component, by the same component equality.

And one neighbouring clause was narrowed by the same lane:
`lodgenIsTreeModel()` used to match a `trees` component ANYWHERE in the path,
which caught `SetDressing\TreeSwing01.nif`, `TreeNoose01_Branch.nif` and five
siblings -- swings and gallows props, not trees. It is now scoped to a
`landscape` first component. Over every placed base in the Commonwealth census:
137 bases classify as trees under both rules (135 `Landscape\Trees`, 2
`Landscape\Plants`), **7 flip out** (82 placements, none of which carries a
distant LOD mesh, so no card or impostor ever came of them) and **none flips
in**; 36 of the 137 kept bases carry a distant LOD mesh, which is exactly the
36 lines `--list-impostor-candidates --candidates trees` prints for the whole
worldspace.

### 1a.3b Which road families are painted, and the numbers that decided it

Two families that the folder rule accepts are **refused by default**, each on a
measurement made on a tile that carries enough of it -- chunk (-8,8) downtown,
cells -8..-5 x 8..11. The statistic is the clearance of a family's AUC above the
TOP of the same mask displaced five ways (tie-averaged brightness): positive
means the family is visible in the sheet beyond what an unregistered mask of its
own shape would score.

| family, chunk (-8,8) | texels | vanilla | painted | refused (the default) |
|---|---|---|---|---|
| raised road (`HighwayOverpass`, `Bridge`) | 72,264 | **-0.009** | **+0.314** | **+0.001** |
| flat road, same tile | 27,988 | +0.011 | +0.124 | +0.068 |
| non-road control | 145,001 | -0.028 | -0.005 | -0.094 |

| pavements, chunk (-8,8) | texels | vanilla | painted | refused (the default) |
|---|---|---|---|---|
| clearance, pure kerb texels | 15,696 | **-0.102** | +0.187 | -- |
| mean luminance, whole kerb mask | 17,801 | **86.7** | 128.1 | **102.1** |
| mean luminance, flat road mask | 11,266 | 94.8 | 112.4 | 107.0 |

Read in one line: **vanilla paints no highway deck and no pale kerb into the far
colour** -- both read at or below their own floors in Bethesda's own sheet --
while the flat road does read above its floor in vanilla and must stay. Raised
pieces are drawn as objects at distance and carry their own Distant LOD meshes,
which is the mechanism. The refusal therefore has two arms, either of which
fires: the base carries a Distant LOD mesh (MNAM present, header bit 15 set --
audited on this tile, 151 road bases / 759 placements, the `hasLod` arm and the
folder arm disagree on **zero**), or the path is under the raised folders.

`--road-raised` and `--road-sidewalks` put each family back, and every refusal
is counted and NAMED in the census: `roadRefusedRaised`, `roadRaisedBases`,
`roadRefusedSidewalk`, `roadSidewalkBases`, and the `roadRefusals` list, e.g.
`raised-haslod Landscape\Roads\HighwayOverpass\HWDoubleEndCapL03.nif`.

### 1a.4 Where it sits in the per-texel composite

Between the VCLR multiply (1.5) and the grass tint (1.5), in both the chunk path
and the tile path:

```
splat composite  ->  cover byte  ->  VCLR multiply  ->  ROAD  ->  grass tint
```

* **After VCLR**, because the road lies ON the ground the artist shaded and must
  not be shaded a second time.
* **Before the tint**, because the tint's weight is the cover byte and a road
  suppresses the cover under it: `cover *= 1 - coverage x roadCoverSuppress`
  (default 1). Grass grows beside a road, not through it. On the chunk path the
  cover PLANE is rewritten with the same byte, so what a consumer reads out of
  the sheet's alpha is what the tint used.

The `coverage` in that formula is the road plane's alpha **unscaled by
`--road-opacity`** (lane ROADS3): the opacity says how strongly the paint is
mixed into the colour, not whether there is a road there. A road painted at
`--road-opacity 0` still suppresses the cover under it, because the mesh is
still lying on the ground. At the default of 1 the two are the same value, so
this distinction changes no shipped byte -- it decides what the knob MEANS at
its other settings.

A texel in a cell with no LAND record gets no road: the whole composite is
skipped there, and such a texel is not terrain.

### 1a.5 What one texel gets

The topmost road triangle covering the texel CENTRE wins -- a maximum-z buffer
in world Z, so a driveway laid over a road wins and a road under a bridge deck
does not. This is `--road-composite max-z`, the default, and lane ROADS2 kept it
by measurement after expecting to replace it -- see 1a.5b. From that triangle:

* **colour** = the shape's diffuse sampled at the interpolated UV, multiplied by
  the interpolated vertex colour, then **lerped toward that texture's own
  average by `1 - roadDetail`**. **`--road-detail` defaults to 1.0 since
  2026-09-12** (lane ROADS4), which is the sampled texel itself with no flatten:
  bungo ruled on the picture, *"--road-detail 1 is always on, do not ever use
  road detail 0, that looks terrible"*. It was 0 until then, and `--road-detail
  0` still reproduces those bakes byte for byte, so 1a.5c below describes what
  that switch does and why it was once the default, not what happens now.
  The mip comes from the triangle's own texture-area-to-footprint ratio,
  `0.5 x log2(uvArea / pxArea)`, not from the landscape path's world tiling: a
  road mesh does not tile with the world;
* **coverage** = 1 for an opaque shape -- an opaque road's diffuse alpha is not a
  silhouette and reading it as one would punch the road full of holes. Only a
  shape whose MATERIAL (`bAlphaTest` / `bAlphaBlend`) or whose `NiAlphaProperty`
  says so honours the texture's alpha. That is the clause that cuts out an
  alpha-tested road decal, and the refused texels are counted.

**The material is resolved by the last `materials/` in its path.** Every
`Landscape\Roads\Country\*` and `\Alley\*` piece names its material as an
absolute Bethesda build path (`C:\Projects\Fallout4\Build\PC\Data\materials\
Landscape\Roads\AsphaltAndSWEdgeDecals01.BGSM`) and carries an EMPTY texture
set. The road pass keys on the last `materials/` and resolves them; the shared
`lodgenLoadModel` still does not, which is a live defect for the OBJECT path and
is recorded as such.

### 1a.5b The alternative composite, and why it is not the default

`--road-composite blend` paints the pieces in order -- ascending mean world Z,
non-decal before decal -- and composites `dst = lerp(dst, src, srcAlpha)`
instead of letting the topmost triangle overwrite. It was built expecting to
win, on the theory that the seam lived at the piece joins, and it lost. On
chunk (-20,20), against the road-presence metric in
`tests/spells/lodgen_roads_metric.py` (road mask from VANILLA's own colour --
chroma R-B <= 17.5 and luminance >= 87.7 -- eroded once to a centreline, scored
as the fraction of centreline texels within 16/255 per channel):

| variant | metric | bar 1 = 2x floor 0.2694 | bar 2 = 0.8x reference 0.3228 | piece-boundary gradient |
|---|---|---|---|---|
| the pipeline before ROADS2 | 0.3061 | ok | FAIL | 9.752 |
| **max-z + detail 0 (the default)** | **0.3404** | **ok** | **ok** | **5.996** |
| blend + detail 1 | 0.2481 | FAIL | FAIL | -- |
| blend + detail 0 | 0.2669 | FAIL | FAIL | 6.204 |
| vanilla, for scale | 1.0000 | -- | -- | 5.271 |

Blend genuinely wins three of the five measures that were looked at -- local 5x5
SD (6.542 against 6.820), the UV-phase R2 (0.033 against 0.052) and mean road
colour error (12.50 against 14.09) -- which is why it is kept behind the flag
rather than deleted. It loses the two the harness gates on. **If a later lane
changes the road colour or the grading, re-run all four variants before assuming
this ranking still holds.**

### 1a.5c Why the diffuse is flattened to one colour a material

bungo, 2026-09-11 16:3x, verbatim: *"look at the roads, there is a visible seam
while vanilla doesn't have it"*. Measured on chunk (-20,20), the seam is neither
a piece-join artefact nor an alpha-compositing bug: it is **the road diffuse's
own texture pattern printed at footprint scale**. At 32 world units a texel a
256-world-unit UV repeat lands every **8.01 bake texels**, so the material's own
light and dark stripes go straight into the sheet as regular banding.

| | ours, full detail | vanilla | floor |
|---|---|---|---|
| within-material luminance correlation along the road | **0.852** | 0.016 | -- |
| R2 of a phase fit against the UV repeat | **0.140** | 0.013 | 0.022 |
| local 5x5 SD, on the road | **10.33** | 6.62 | -- |
| local 5x5 SD, off the road (control) | 5.52 | 5.38 | -- |

Three other candidate mechanisms were refuted with numbers: **0 of 474** road
materials set `bAlphaBlend` (6 of 512 shapes do blend, through their own
`NiAlphaProperty`, which is why the shape-level test in 1a.5 exists); the median
covering-Z spread at a road texel is 12.299 world units, so pieces are not
z-fighting; and **0 of 474** shapes are mip-clamped. Vanilla's far road measures
as one flat colour a material -- its own diffuse average under the sheet's
grading -- so that is what `--road-detail 0` writes.

After it, on the shipped exe: all piece boundaries 9.752 -> **5.718** against
vanilla's 5.271; the 54 feathered boundaries 11.837 -> **3.988** against
vanilla's 4.242; local 5x5 SD on the road 10.375 -> **4.369** against vanilla's
5.533. **That last row is now an error in the other direction** -- our road
interior is smoother than vanilla's -- and it is the known cost of detail 0.

### 1a.5d How strongly the paint is mixed in, and what vanilla's road actually is (lane ROADS3, 2026-09-12)

`--road-opacity A` (default **1.0**, clamped to 0..1) scales the road plane's
alpha into the composite of 1a.4:

```
colour = ground + ( roadColour - ground ) * coverage * roadOpacity
```

At 1.0 the multiply is not performed and the branch is entered on exactly the
same condition as before, so the default is the previous bake's BYTES by
construction and not an argument about `1.0f` -- the same discipline
`g_landGrade != 1.0f` uses in 2.5f.

**Provenance of every number below.** Two chunks, measured on the exe of
2026-09-12 03:06:21 (lane GRADE1's, copied aside as
`release/NifSkope.before_roads3.exe`, md5 `6af74b4b4667ce50c4506a2d42a04fdf`):
(-20,20) Sanctuary and (-8,8) downtown, 4x4 cells each, 512 texels at 32 world
units a texel, against Bethesda's own `Commonwealth.4.<x>.<y>.DDS` on the same
grid with no resampling on either side. Scripts and logs:
`scratchpad/roads3_20260911/r3_law.py`, `r3_chroma.py`, `r3_sim.py`, logs
`f1_law.txt`, `f1_chroma.txt`, `f2_sim.txt`. Instrument self-test 14 of 14
(`r3_selftest.py`, `s0_selftest.txt`), including a planted law recovered to four
decimals and a sheet built NOT as the law leaving 95.1 % unexplained.

**1. Vanilla's far road is a wash that follows the ground under it, not a
paint.** Road luminance regressed on the mean luminance of the NON-road texels
within 8 texels (256 world units) of it, on the same sheet:

| field | slope on the local ground | corr | road L | rise over that ground |
|---|---|---|---|---|
| vanilla, (-20,20) | **+0.714** | +0.442 | 92.02 | +1.02 |
| ours, (-20,20) | +0.339 | +0.300 | 88.30 | +16.85 |
| our own unpainted ground, (-20,20) | +0.637 | +0.562 | 66.06 | -5.39 |
| vanilla, (-8,8) | **+0.755** | +0.472 | 94.17 | +1.91 |
| ours, (-8,8) | +0.209 | +0.088 | 106.09 | +2.29 |
| our own unpainted ground, (-8,8) | +0.565 | +0.338 | 102.13 | -1.67 |

Floors, both sides, on the same texels: the local-ground field translated by a
large random shift reads slope **-0.009** (worst |slope| 0.298) over five draws;
a known-answer road of a FIXED colour built from these sheets reads **+0.000**;
a known-answer road of `ground + 4` reads **+1.000**. Vanilla's road tracks its
neighbourhood at least as strongly as unpainted ground does; ours at half.

**2. The rise, which is the number to design against.** Road mean minus the mean
of the 1..8-texel band outside the mask: vanilla **+4.29** on (-20,20) and
**+4.40** on (-8,8) -- two tiles a whole biome apart -- against ours **+29.96**
and **+3.84**. Ours is right on (-8,8) to 0.56 of a level and 25.67 levels too
contrasty on (-20,20), because our paint is a fixed material colour (99.05 and
106.68) while our ground swings 60.96 -> 102.12 between the tiles.

**3. The hue is NOT a defect and is not a knob.** Road minus surround, opponent
axes `b_y = B - (R+G)/2` and `r_g = R - G`: vanilla +2.30 / -2.14 and ours
+3.35 / -3.25 on (-20,20); vanilla +4.10 / -3.07 and ours +1.96 / -1.24 on
(-8,8). Same sign, same direction, every gap under 3 levels of 255. On the road
texels themselves at Sanctuary, vanilla `b_y` -12.62 against ours -12.39 and
saturation 0.162 against 0.161.

**4. The edge WIDTH is refused as unresolvable, with its number.** Vanilla's
4.29-level rise sits under a local 5x5 SD of 6.59 levels, a signal-to-noise of
**0.65**; a width fitted there reads the terrain's texture, not the road. What
can be read is the profile: vanilla reaches full value at d = +1 and is flat
across the width (92.70, 92.57, 92.54, 92.18 ...), while ours ramps 79.86 ->
86.48 -> 93.45 over three texels, plateaus near 96.5 and climbs to 105.5 in the
core -- a **darker outer band around a brighter core** (the alpha-blended skirt
of 1a.5 over a far darker ground, with the wider trunk material inside it).
Biggest step inside the road, as a max second difference of the profile: ours
**3.88** against vanilla's **1.31** on (-20,20); ours **1.25** against vanilla's
**4.43** on (-8,8), i.e. already the smoother of the two there.

**5. WHY THE DEFAULT IS STILL 1.0.** Every candidate was simulated on the rung's
own sheets, using this section's own composite, before any code was written
(`r3_sim.py`; the tint is inert on these chunks because the cover plane is empty
and `--grade` is 1.0 with its multiply branched over, so the sheet's RGB on a
road texel IS the road plane's). Two refusals came out of it, both arithmetic:

* **On (-8,8) no opacity can match vanilla's road brightness at all.** The
  composite can only land the road between our ground (102.12) and our paint
  (106.68); vanilla's road is at **94.59**, 7.53 levels outside that interval.
  The same holds for the hue read as an absolute rather than as a rise:
  `d(b_y)` is -6.5 to -6.7 for every rule including the default, because our
  GROUND's own `b_y` there is -12.79 against vanilla road's -6.25.
* **On (-20,20) the gates are mutually exclusive by 22 levels.** Vanilla's
  absolute level wants a = 0.83, vanilla's rise wants a = 0.326, the step wants
  a <= 0.25 and a local SD inside 20 % of vanilla's wants a >= ~0.75. Those 22
  levels are the GROUND's: ours is 19 levels darker than vanilla's on that
  chunk (68.69 against 83.52), which 2.5f records as a per-cell CONTENT
  difference with a near-zero mean, and lane TILING2's addendum says in writing
  not to chase with the road pass.

The table, both tiles (road L, and rise over the surround; vanilla is
92.52 / +4.29 and 94.59 / +4.40). Three rows were BAKED on the built exe of
2026-09-12 04:10:38 and are marked so; the other two are the offline pricing:

| `--road-opacity` | (-20,20) | (-8,8) |
|---|---|---|
| **1.000 (default)** -- BAKED | 99.05, +29.96 | 106.68, +3.84 |
| 0.830 -- BAKED | 92.43, +23.34 | 105.89, +3.05 |
| 0.500 -- priced | 80.01, +10.91 | 104.40, +1.56 |
| 0.326 -- BAKED | 73.09, **+4.01** | 103.39, +0.55 |
| 0.250 -- priced | 70.48, +1.39 | 103.26, +0.42 |

**6. A per-texel ground-relative mode was built, simulated and REJECTED with its
numbers**, and is recorded so it is not proposed again without new evidence:
choosing `a` per texel so the result sits a fixed rise above the local non-road
ground lands the (-20,20) road 25 levels below vanilla with a rise of **-1.8**
(more than half its texels clamp to a = 0, because the ground directly under the
road is brighter than the disc mean for most of them), and halves the (-8,8)
local SD to 0.48 of vanilla's. A flat opacity beats it on every row of both
tables.

**7. `--road-detail` was re-tested and stayed 0 -- and was then overruled by
bungo on 2026-09-12, who looked at both and said *"--road-detail 1 is always
on, do not ever use road detail 0, that looks terrible"*. The measurement
below still holds and is why the flag exists; it is not what decides the
default any more (1a.5, 1a.5e).** The residual after the best
wash correlates with the full-detail bake's departure from its flat average at
**+0.0275** against a phase-twin floor of 0.0270 mean / 0.0644 max on (-20,20),
and **+0.0162** against 0.0124 / 0.0163 on (-8,8) -- the correlation IS the
floor at every blur radius, and the best-fit strength is negative. 1a.5c's
conclusion survives a test that could have overturned it.

**8. What was measured on the built exe** (2026-09-12 04:10:38, 21,489,152 B,
md5 `fe65cc978f3896881140c2eea57c69c6`; logs under
`scratchpad/roads3_20260911/logs/`).

The byte-identity claim in the first paragraph is now a `cmp` result and not an
argument about the code. Every file of both bakes compared, not a sample
(`r3_f2.sh`, `f2_bytes.txt`): on (-20,20) the new exe with no flag reads **9 of
9 identical** to the rung, `--road-opacity 1` **9 of 9**, `--roads-legacy` **9 of
9** against the rung's own `--roads-legacy`; on (-8,8) **10 of 10** in all three
arms. The compare is shown able to fail in the same run: `--road-opacity 0.326`
moves 3 files on (-20,20) and 4 on (-8,8), and **every one of them is colour** --
the `tex/Commonwealth.4.<x>.<y>.DDS` sheet and the `.lodt` virtual-texture
levels. The `_msn` normal sheet, the `_data` sheet, the `.bto` objects, the
`.lodl` and the `.lodm` are byte-identical at **every** setting, so the switch
reaches the road colour and nothing else.

What the baked sheets read (`r3_f3.py`, `f3_gates.txt`), against vanilla's own:

| field | road L | rise | local 5x5 SD | biggest step |
|---|---|---|---|---|
| vanilla (-20,20) | 92.52 | +4.29 | 6.59 | 1.31 |
| default, a = 1 | 99.05 | +29.96 | 7.26 | 3.88 |
| baked a = 0.326 | 73.09 | **+4.01** | 4.20 | 1.69 |
| baked a = 0.83 | **92.43** | +23.34 | 6.72 | 3.35 |
| vanilla (-8,8) | 94.59 | +4.40 | 6.44 | 4.43 |
| default, a = 1 | 106.68 | +3.84 | 5.42 | 1.25 |
| baked a = 0.326 | 103.39 | +0.55 | 4.29 | 1.04 |

**The two-tone skirt, measured directly.** ROADS2's `seam.py` re-run on these
bakes (`f3g_seam.txt`) correlates road luminance with the road mesh's own
interpolated vertex alpha: vanilla **+0.001**, ours at a = 1 **-0.792**, at 0.83
-0.694, at 0.326 **-0.436** -- and our unpainted ground's own floor on the same
texels is **-0.325**. So the opacity knob walks the skirt signature from -0.79
toward the ground's -0.33 and can never reach vanilla's 0: the ramped skirt
geometry is still underneath, and opacity dilutes it rather than removing it.
The feathered-boundary luminance gradient (54 texels, vanilla 4.242) reads 3.979
at a = 1, 3.352 at 0.83, 2.979 at 0.326, all inside vanilla's, with
displaced-boundary floors of 3.10 to 4.81.

**9. How good the offline pricing actually was, measured rather than guessed.**
An earlier revision of this section said the simulation was right to "about half
a level". Baked and compared texel by texel, it is right on the AGGREGATES --
road mean luminance within **0.286** of a level on (-20,20) and **0.221** on
(-8,8) -- and is not right per texel: mean absolute difference **1.583** levels,
99th percentile 7.341, worst **15.279** (1.527 / 5.745 / 13.802 downtown),
because the bake passes through 8-bit quantisation and BC1 block compression and
the simulation does not. Read it as a licence to choose WHICH settings to bake,
never as a substitute for baking them, and never as a picture.

The harness chain on this exe matched lane GRADE1's baselines row for row:
`lodgen_roads` 11/0, `lodgen_terrain` 26/0, `lodgen_terrain_vt` 41/1,
`lodgen_ground_cover` 29/5, `lodgen_terrain_pbrm` 14/0, `lodgen_native` 0
failures in all seven sections, `lodl_open` 23/0, `lod_generation` 116/0. The
one red row in `lodgen_terrain_vt` is **V9c**, and it is not this pass's: the
rung exe, run as a control, fails it with digit-for-digit identical numbers
(E/W seam 188.074, interior 13.243, ratio 14.20, edge step 14.348).

### 1a.5e The road models carry TERRAIN, and it is a third of the road plane (lane ROADS4, 2026-09-12)

bungo, 2026-09-12: *"the issue with the roads is, these meshes have some terrain
included there, you can see the sharp mesh terrain being included into the
chunk's bake"*. He is right, and it is not a skirt, not a shading term and not
anything the composite above does wrong. It is what Bethesda modelled.

Fallout 4's road pieces -- `Landscape\Roads\Sanctuary\SancRoadStr01.nif` and
its siblings -- contain shapes whose MATERIAL lives under
`materials\Landscape\Ground\`: `CommonwealthDefault01.bgsm`,
`DirtGravel01.bgsm`. They are the verge and the junction fill, geometry that
carries landscape colour inside a road model. The road pass paints them, because
they are shapes in a road NIF, and so the far sheet gets a hard-edged patch of
ground colour sitting in the middle of the road plane, at road detail:

| chunk | road texels | won by a `Landscape/Ground/` material | share |
|---|---|---|---|
| (-20,20) Sanctuary | 23,116 | 8,337 | **36.1%** |
| (-8,8) | 11,069 | 2,756 | **24.9%** |

| what | ours | vanilla | our floor |
|---|---|---|---|
| two-tone step, surface minus patch, (-20,20) | **25.28** | 2.53 | -- |
| two-tone step, (-8,8) | **13.13** | 1.76 | -- |
| gradient across the patch boundary, (-20,20) | **15.387** | 5.362 | 6.077 |
| gradient across the patch boundary, (-8,8) | **8.010** | 5.138 | 4.805 |

The floor is the same boundary texel set displaced five ways on the same sheet,
per `ww-control-calibration`; vanilla sits within a level of its own floor on
both tiles and ours sits two and a half times above it on (-20,20).

**The discriminator is the material's FOLDER, never its file name.** A name-stem
list put the two biggest contributors (`CommonwealthDefault01`,
`SancSW01.BGSM`) in an unclassed bucket and hid the whole finding. The rule is
`lodgenRoadMaterialIsGround()`: normalise with `lodgenRoadMaterialPath()` (which
keys on the LAST `materials/`, see 1a.5) and ask whether the result contains
`materials/landscape/ground/`.

**`--road-ground-paint 0..1`** is the coverage multiplier for such a shape. It
**defaults to 1.0**, which is the behaviour above unchanged, and it ships as the
instrument that refuted its own candidate rather than as a fix:

| `--road-ground-paint` | 1.0 | 0.75 | 0.5 | 0.25 | 0 |
|---|---|---|---|---|---|
| boundary gradient, (-20,20) | 15.387 | 19.243 | 23.810 | 28.598 | **33.352** |
| the patch, levels from vanilla | -5.47 | -11.61 | -17.94 | -24.50 | **-29.22** |
| R5 road-presence metric | 0.3078 | 0.2935 | 0.2854 | 0.2833 | **0.2783** |

Fading the terrain shapes out makes the seam **monotonically worse**, because
the patch darkens toward our own ground (85.53 -> 61.78) while the asphalt
beside it does not move at all (110.82 -> 111.49): the step it makes with the
road surface more than doubles. The multiply is on **coverage**, not on paint
strength, so 0 also stops such a shape suppressing ground cover -- which is the
behaviour a consumer expects from a shape that is not painting.

**What the numbers actually indict is the asphalt's own tone.** Our road surface
sits at luminance 110.82 where vanilla's is 93.54, while our terrain class is
already within 5.47 levels of vanilla's. That is `roadOpacity`, which lane
ROADS3 measured and refused to set, and this lane's seam number is new evidence
on the same knob and reaches the same refusal: `--road-opacity 0.326` gives
vanilla's own seam on BOTH tiles (5.304 vs 5.362; 4.242 vs 5.138) and destroys
the road-presence metric (R5 0.1576, both bars fail, centreline colour error
23.64 -> 30.87); `--road-opacity 0.83` is the only value that passes both R5
bars (0.3545 >= 0.3271) while improving the seam to 12.630; and (-8,8) prefers
the opposite direction to (-20,20). **Unset, deliberately.**

**Two hypotheses are closed by measurement and should not be re-opened without
new evidence.** There is no skirt to suppress: skirt-only texels are **0** on
both chunks, a skirt triangle is the max-z winner on 455 of 23,116 and 556 of
11,069 texels and never alone. And vertex alpha carries no signal: luminance
against vertex alpha reads **-0.0345** on ours and **-0.0359** on vanilla on
(-20,20), **+0.0404** and **+0.0766** on (-8,8). The **-0.792** in an earlier
lane's note was an instrument artefact -- a `np.zeros` alpha buffer that read
every shape without a vertex-alpha channel as fully transparent.

The census reports it: `ground_shapes` and `ground_texels` on the census line,
`roadGroundPaint` / `roadGroundShapes` / `roadGroundTexels` in the meta report.

### 1a.6 The channels touched

| sheet | touched | why |
|---|---|---|
| colour (role 1) | yes | measured: vanilla carries the road there |
| `_msn` normal (role 2) | no | measured: vanilla's `_msn` is the heightmap's on the road too |
| mask (role 5, RMAOS) | alpha only | the ground-cover byte is suppressed under the road; R/G/B untouched |
| emissive (role 6) | no | a road emits nothing |
| height (role 4) | no | the road is not the ground's height |
| retired `data` plane and the `.btr` `_data.DDS` | alpha only | the same cover byte, the same reason |

### 1a.7 The census

Chunk path: one `roads ...` line per chunk on stderr while the pass is on.
Pyramid path: the same fields in the `--vt` report line -- `roads`,
`roadPlacements`, `roadMeshes`, `roadShapeTiles`, `roadDecalShapeTiles`,
`roadTriangles`, `roadTexels`, `roadDecalTexels`, `roadAlphaRejected`,
`roadRefusedNoLoad`, `roadRefusedNoTexture`, `roadRefusals`, and from lane
ROADS2 also `roadComposite` (`max-z` or `blend`), `roadDetail`, from
lane ROADS3 `roadOpacity`,
`roadBlendTexels`, `roadRaisedIncluded`, `roadRefusedRaised`, `roadRaisedBases`,
`roadSidewalksIncluded`, `roadRefusedSidewalk`, `roadSidewalkBases`. `roads 0`
means the switch was off; `roads 1 roadTexels 0` means it was on and this ground
carries none. `roadPlacements` and `roadMeshes` count the GATHER, which runs over the
region grown by two cells, so a region whose roads all sit in that margin
reports placements without texels -- measured on cells -20..-17 x 24..27:
`roadPlacements 32 roadMeshes 13 roadShapeTiles 0 roadTriangles 0 roadTexels 0`.

### 1a.8 The way back

`--no-roads`. The plane is never allocated, no `REFR` is read and no road model
is opened, so the bake is byte-identical to the bake before roads existed by
CONSTRUCTION -- measured: 9 of 9 files identical over the Sanctuary region, and
re-measured on the ROADS2 exe against the pre-ROADS2 exe's own road-free bake,
9 of 9 identical.

**`--roads-legacy`** is the way back to the road pass as it was before lane
ROADS2, in one token: it means `--road-composite max-z`, `--road-detail 1`,
`--road-raised` and `--road-sidewalks` together, from lane ROADS3 also
`--road-opacity 1`, and from lane ROADS4 also `--road-ground-paint 1` -- all
no-ops while those are the defaults, written down so the way back stays the
way back if a default is ever moved. `--road-detail 1` stopped being a no-op
on 2026-09-12 in the other direction: it is now the default, so the legacy
token and the default agree on it. Anything named explicitly on
the same command line still wins, so the flag can also be used to move exactly
one thing away from the old behaviour. Measured: a `--roads --roads-legacy` bake
on the ROADS2 exe is **byte-identical to the pre-ROADS2 exe's own `--roads`
bake, 9 of 9 files**, over cells -20..-17 x 20..23. And a `--roads` bake with
nothing else named is byte-identical, 9 of 9 files, to one with all four knobs
spelled out at their defaults -- so the defaults are exactly those four and not
a fifth unstated one.

## 2. The pyramid

### 2.1 Levels, and the one aligned grid

A **level** is named by its `dim` — cells per tile edge. The ladder is ×2 from
the finest level up and it is normative: the header carries `levelDims[8]` so a
consumer holding one container can name its siblings without the index.

The default finest level is **dim 2**, which at 256 content texels is **32 world
units per texel — exactly the density of vanilla's finest terrain ring**. Full
mode (`--vt-finest 1`, or `--vt-density 8`) makes dim 1 the finest level at 16
units per texel at content 256, or 8 at content 512; every coarser level, dim 2
included, is filtered from the one below it. Only the finest level is baked from
the paint. (Measured 2026-09-23, lane VTNORMAL1: the 8 u bake's dim-2 normal
carries the normal sheets of §2.2b, r 0.942 / 0.866, which a paint bake could
not.)

**One choice names the finest texel size** (lane VTNORMAL1): `--vt-density 32`
is dim 2 at content 256, `16` is dim 2 at content 512, `8` is dim 1 at content
512, byte-identical to those `--vt-finest` / `--vt-content` pairs. The command
line's default is still 32 u (the previous bytes); the panel's *Finest texel
size* row defaults to **16 u**.

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
| sheets | **3, 4 or 5** | colour, model-space normal, **mask**, then height when it was asked for, then **emissive** when any layer supplies one |

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
| 0 | 1 colour — RGB albedo, the grass tint folded in | BC1 (71) | BC1 | sRGB |
| 1 | 2 model-space normal | BC1 (71) | BC1 | linear |
| 2 | **5 mask — `rmaos`: R roughness, G metallic, B AO, A ground cover** | BC1 (71) | **BC3 (77)** | linear |
| 3 | 4 height (only with `--vt-height`) | R16_UNORM (56) | R16_UNORM | linear |
| 4 | **6 emissive — RGB, no alpha** (only when a layer supplies one) | BC1 (71) | BC1 | linear |

Role **3 (`data`) is retired** and is refused by name in a v2 container, so a
file written by something that still believed role 3 meant AO/wetness/shore/cover
is diagnosable rather than merely invalid.

**THE MASK LAW, per layer.** The three channels come from the layer's own
material, through ONE resolver shared with the object path
(`lodgenResolveMaterialMask`, `src/lodgen.h`), and every layer's answer NAMES
the rule that produced it:

| rule | when | R roughness | G metallic |
|---|---|---|---|
| `pbrm` | a `.pbrm` parses beside (or as) the TXST's `MNAM` material — the same-name discovery rule the renderer uses | its RMAOS **R**, or its `roughness` constant | its RMAOS **G**, or its `metallic` constant |
| `legacy-inverted` | a legacy material or a bare `_s` map | **`1 − smoothness × _s.G`** | **0** |
| `none-default` | nothing to read | **1.0** — fully rough, the honest unknown | 0 |

*Metallic is derived from a PBRM or not at all* (bungo, 09:4x: *"that should
only get derived from PBRM"*). A legacy layer contributes **0**, never a guess
from its specular colour.

**The gloss is the `_s` map's GREEN channel, and that is a measured fact rather
than a convention.** Every Fallout 4 landscape `_s` map is **BC5U** — a
two-channel block format, R then G, with no blue and no alpha (measured over the
unpacked corpus: `Textures/Landscape` holds 824 DDS files, of which 495 are BC5U
and every one of those is an `_s` or a normal; the diffuses are 100 DXT1, 226
DXT5 and 2 DXT3). `lodgenLegacyGloss( smoothness, specGreen )` is the one
definition of the gloss, called by the object arrays pass and inverted here, so
the object sheets and the terrain sheet cannot drift apart about one material.

**B is the same sky AO the retired data sheet carried in its R** — the eight-
direction horizon march — unchanged in value and moved one channel.

Two reaches live under that one sentence and they are not the same number.
The per-vertex march in `lodgenTerrainChannels` steps 1, 2, 3, 4, 7, 10, 13
and 16 times 128 units, so it does reach **2,048**. The march in BOTH sheet
composites is `for ( float dist = 128; dist <= 2048; dist *= 1.5 )`, which
stops at 128, 192, 288, 432, 648, 972 and **1,458** — 2,048 is the loop's
bound, not a distance it ever samples. Section 2.5h depends on the
difference and measures it.

With `--terrain-object-ao` this byte carries a SECOND visibility fraction
multiplied into the first, from the placed objects rather than from the
ground's own horizon (section 2.5h). Without the switch the byte is exactly
what it was.

**The blend is the colour's blend, exactly.** Per texel: the quadrant's base
layer, then each painted layer by the same bilinear opacity the diffuse
composites with, in the same order, un-renormalised. **VCLR is NOT applied to
the mask** — it is the artist's hand-painted shading of the ground's COLOUR —
and neither is the grass tint.

**Per-tile format selection, and the one cover carrier.** `sheets[k].dxgiFormat`
is the format when that tile's `COVER` bit is clear and `dxgiFormatCover` when it
is set. **Exactly one sheet may declare two different formats, and it must be
the mask (role 5) or the colour sheet (role 1)** — the header says which by
declaring the pair. Two carriers, or a carrier on any other role, is a refusal
(§3.4 rule 13): a consumer sizing an upload from a single per-file format would
otherwise mis-size every cover tile.

### 2.2a Where the ground cover lives, and the number behind the choice

`.lodm` §2.1 gives the object family two alpha slots: the colour sheet's is
**coverage** (opacity) and the mask's is **subsurface**. Terrain's fourth
channel is ground cover, and it had to take one of them. **It takes the mask's**,
`--vt-cover-in-color` is the exact way back, and the reason is NOT size:

* **Bytes: the two are identical, measured.** The cover format is selected per
  TILE by the `COVER` bit, so a cover-free tile is BC1 either way and a cover
  tile is BC3 on exactly one sheet either way. Measured on cells −20..−19 ×
  24..25, `--vt-height --cover`, both arms: **746,752 bytes** at level 2 and
  **374,016** at level 4, `storedBytesTotal` **739,840** in both, two cover tiles
  in both. With `--no-cover`: **655,456** and **327,776** in both. A tile is
  **369,920 B** with cover and **323,680 B** without, whichever sheet carries it.
* **The stock `.btr` path tolerates a BC3 colour sheet, also measured**, so that
  is not the discriminator either: **2,001 of 2,001** of vanilla's own shipped
  `Textures\Terrain\Commonwealth\*.DDS` chunk colour sheets are **DXT5**, and
  1,999 of 1,999 `_msn` sheets are too. DXT5 is the only format the engine has
  ever been given for that slot.
* **The discriminator is what the slot MEANS.** The colour sheet's alpha is the
  one slot the object family defines as OPACITY, and `.lodm` §2.1 tells a
  consumer to alpha-test it. A consumer written against that law would punch
  holes in the ground wherever grass is thin. The mask's alpha is subsurface,
  which nothing alpha-tests, and substituting ground cover for it is a named
  substitution the index records.

**This is bungo's call to make, and it is open** — `--vt-cover-in-color` reaches
the other arm today and costs one flag, one format pair and no second code path.

### 2.2b Where the normal sheet comes from (lane VTNORMAL1, 2026-09-23)

The `msn` sheet of the finest level is computed from the LAND heights, unless a
normal-sheets folder is set (`--msn-cache`, panel *Normal sheets folder*; bungo's
ruling: use his upscaled normal sheets, downsampled). Then each finest tile's
normal, border included, is taken from the folder's dim-4 sheets
`<ws>.4.<x>.<y>_msn.DDS` (or `<ws>.4.<x>.<y>.png`, §7a.3; the folder is searched
as the `--msn-cache` row of §5 says). Each sheet is read once as unit vectors,
reduced to the level's texel size by a box filter in vector space (sum,
renormalise), encoded, and copied texel for texel; at 8 units a texel the
reduction is a copy. Sheet row 0 is NORTH, R is +east, G up, B +north (measured
by flipping: every r against the heights normal and vanilla's own sheet drops to
about 0.13). A border texel reads the neighbouring chunk's sheet, so seams match
by construction. A sheet is kept only while a tile row can still reach it (about
0.5 GB at 8 u). Coarser levels are filtered from the finest by §2.3, which
already averages the msn as a vector and renormalises it.

A texel whose chunk has no sheet keeps the heights normal. The `vt:` census
counts finest tiles as `normalMsnCache` (all content texels from sheets),
`normalHeights` (none) and `normalMixed` (some), plus `msnSheetsRead` and
`msnSheetsMissing`; the five words are written whether or not the folder is set.
The index says the same in `terrain.normalSource` (§4).

**The chunk sheets are not changed by this.** When the `.btr` chunk sheets are
assembled from the pyramid (§2.4), the heights normal is kept in a second staging
plane, filtered alongside, and the assembly reads that. With the same folder the
chunk sheets are byte-identical to the rung, including the dim-8 `_msn`; without
the folder the whole bake is byte-identical to the rung.

**Measured, 16 u, a 2x2-chunk region:** sheets on against off differ by 16.0
degrees on average at the finest level. Against his sheets, ON reads r 0.939 east
/ 0.836 north, OFF 0.576 / 0.633. The north loss is our BC1 encoder, not the
transfer (§7a.3).

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
2. **The cover carrier's alpha averages plainly**, and **a tile with no cover
   stages alpha 0, never 0xFF**. The 0xFF of §1.4 is applied only at pack time
   on the BC1 fallback path and never enters the filter — without that rule a
   parent bordering one grassy child would inherit full cover across three
   quadrants of bare rock. A parent's carrier sheet is BC3 iff its averaged alpha
   is not everywhere zero.
3. **The mask's R and G average plainly too, and that is correct where AO's is
   not.** Roughness and metallic are material constants resampled, so the mean of
   four is the mean material. AO is a fixed-radius horizon march and is
   scale-dependent in exactly the way the paragraph below describes.
4. **The emissive sheet averages plainly** and is present in every tile of a
   container or in none: its presence is a header field, not a per-tile one, so a
   tile's payload size stays a function of the header and its `COVER` bit.

**Coarse levels are downsamples of fine data, not measurements at that scale,
and that is a documented limitation rather than a bug.** Three of the four data
channels are scale-dependent: AO is a fixed-reach horizon march (it samples out
to 1,458 units; 2,048 is only the loop's bound, §2.2), so
`mean(AO) ≠ AO(mean)`; and the msn's renormalisation fixes the magnitude but the
mean of fine normals is still not the normal of the coarse heightfield. Only
**cover**, **albedo**, **roughness**, **metallic**, **emissive** and **height**
filter cleanly. (Version 1 also listed shore proximity here, as a distance field
that box-filters worst near its zero crossing — the one place it is read. It is
gone, which removes that case rather than fixing it.) The index says `coarseLevelsAreDownsamples: true` so a consumer
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

`dominantBase` — what NULL-LTEX layers and `baseTex == 0` texels paint — is
**no longer scope-dependent** (lane SEAM1, 2026-09-25). It used to be the most
common base of the enclosing dim-4 chunk, and a chunk whose dominant base
differed from its neighbours' painted a hard-edged block on the chunk grid
(Sanctuary, cells -20..-16 x 20..24: steps 12.9 / 11.3 / 11.9 / 8.8 luminance
at its four borders against interior tile borders of 1.5 and less). It is now
the one world-wide texture the engine itself paints there,
`ESM_LTEX_ENGINE_DEFAULT` = `Landscape\Ground\CommonwealthDefault01_{d,n,s}.dds`
(the game's `sDefaultLandDiffuseTexture:Landscape` family, read from the exe's
string table; see §2.5 step 4). The chunk and tile bakers use the same constant,
so V9a's byte identity holds by construction.

The sampling grid is identical to a direct bake: at dim 4 a texel centre sits at
`cwX + (px + 0.5)·32`, and the pyramid's composite index gives the same world
points. The `footprint` that picks the source texture's mip is 32 in both cases.

It differed in one place until 2026-09-10, and that difference is now gone:
**the pyramid bakes a one-cell ring around every tile**, so its AO march and
its outer-ring normals had real data where the per-chunk path clamped both at
the chunk edge. The chunk path bakes on the SAME ring now, through the same
`lodgenTerrainFillRing` and the same `lodgenTerrainGridSample`, so the
assembled colour and `_msn` sheets are byte-identical to a direct bake with
the ground-cover tint ON as well as off (V9a and V9b in
`tests/spells/lodgen_terrain_vt.sh`), and the step in the normal across a
chunk seam fell from 4.07x the interior step to 2.75x east/west and from 3.78x
to 2.87x north/south, with the interior control unmoved at 1.80 and 1.60
(V9c, same file).

**Where the two neighbours disagree, the cell wins** (bungo, 2026-09-10,
verbatim: "The cell owns it then"). Bethesda's landscape does not always agree
with itself across a shared cell edge: over the cells x = -24..-17 the shared
VHGT row y=31|32 differs by 2, 1, 4, 6, 9, 8, 7 and 4 units of 8 -- 16 to 72
world units -- while y=23|24, y=27|28, y=32|33 and both east seams differ by
exactly 0. `lodgenTerrainFillRing` therefore fills ONLY the samples BEYOND its
inner unit: a ring cell never writes the chunk's (or the tile's) own boundary
row or column, so the chunk's copy of a disagreeing row stays, and the hairline
disagreement is kept AT the seam instead of being carried inwards. A bilinear
tap reaches one grid step, so before this rule a moved boundary row showed up to
**7 texels** inside the sheet -- past the 4-texel band the normal's own central
difference can reach. Inside the inner unit the fill order is unchanged (south
to north, west to east, later wins), which is also the mesh path's convention in
`lodgenWriteLandChunk`, so the two stay consistent. Both bakers get the rule
from the one shared filler, which is what keeps V9a and V9b byte-identical.

**One sheet is still not identical between the two paths, and it is named**:
the `_data` sheet's wetness channel is a flow accumulation over the WHOLE
sample grid its baker is handed, and a tile's grid is not a chunk's, so no
ring can make those two agree. The chunk baker therefore keeps the CHUNK's
grid for `lodgenTerrainChannels` (`chgt`) while its normal and its AO march
read the ring, and the harness says so instead of pinning a bar it cannot
hold. Fixing it means giving wetness a domain that is not the bake unit --
a separate track. The `.btr` file itself is untouched — the Land shader still names `<ws>.<dim>.<x>.<y>.DDS` and `_msn.DDS`,
still Shader Type 18, still `UV = (x/4096, 1 − y/4096)` in miniature chunk
space. That is the whole point of assembling the sheet rather than pointing the
mesh at a tile.

**The chunk sheets are not deleted.** The pyramid supplies their bytes; it does
not replace the files. The stock engine needs them and there is no VT consumer
yet.

### 2.5 Ring 0, and the ONE formula the runtime must follow

bungo's ruling, 2026-09-11 09:2x: far terrain is **hybrid by band**. The band
touching the loaded 5x5 cell grid — ring 0 — is blended at RUNTIME from the
`.lodl`'s per-texel LTEX weights, so the loaded-cell edge is seamless; ring 1 and
outward sample this pyramid; the two cross-fade across ring 0. That only works if
the two agree on the same texel, so the runtime's formula is stated HERE, once,
and the generator gate below reproduces it.

**At a world point `(wx, wy)`, in this order:**

```
1  cell   = floor(wx/4096), floor(wy/4096)          cell-local (clx, cly)
2  q      = (cly >= 2048 ? 2 : 0) + (clx >= 2048 ? 1 : 0)        the quadrant
3  layer opacity a_i = BILINEAR over the quadrant's 17x17 VTXT grid
4  colour = diffuse( base )                          base = BTXT, or the
                                                     ENGINE DEFAULT land texture
                                                     when it is 0 (lane SEAM1)
5  for each ATXT layer i, IN RECORD ORDER:
       if a_i <= 0.001: skip                         (and it is SKIPPED, not
                                                      blended with a tiny weight)
       colour = colour + ( diffuse(ltex_i) - colour ) * clamp(a_i, 0, 1)
       ltex_i == 0 paints the same engine default
6  colour *= VCLR / 255                               bilinear over the 33x33
                                                     grid; ABSENT on most cells
7  colour += ( Ttex - colour ) * (cover/255) * tintStrength      the grass tint,
                                                     AFTER the VCLR multiply
8  colour *= grade                                    the colour GRADE, 2.5f.
                                                     DEFAULT 1.0, in which case
                                                     this step does not exist:
                                                     the multiply is branched
                                                     over, so ring 0 and the
                                                     pyramid agree bit for bit
                                                     at the default. A ring-0
                                                     runtime that ships a grade
                                                     other than 1 must read the
                                                     SAME k the sheets were
                                                     baked with -- the bake
                                                     census prints it as
                                                     `landGrade`
```

**Each source diffuse is sampled the same way**: `u = frac(wx/T)`,
`v = frac(wy/T)` at the mip
`clamp( log2( max(1, unitsPerTexel / (T/textureWidth)) ), 0, maxMip )`,
trilinear, where **T = 341.3333 world units, the engine's own landscape texture
repeat**: twelve repeats a cell, six a quadrant. The runtime and the pyramid
must use the SAME T or ring 0 seams by the texture's own detail, which is the
visible half of a seam. `--land-tiling <units>` overrides it for a user who has
changed `fLandTextureTilingMult`; `--land-tiling 2048` reproduces every sheet
written before 2026-09-11 byte for byte.

**T IS THE ENGINE'S, AND IT IS NOT IN THE DATA.** No LAND, LTEX or TXST field
carries a tiling scale — LTEX is EDID + TNAM + HNAM + SNAM + GNAM, TXST is
texture paths and flags, checked over every landscape texture in the Sanctuary
region. The number is read out of `Fallout4.exe` **1.10.155.0** (65,319,936
bytes), re-derived from the binary by
`scratchpad/splat1_20260911/s2c_engine_tiling.py`:

* `fLandTextureTilingMult:Landscape`, one copy, file 0x2C84DD8 / VA
  0x142C861D8. Its `Setting` record `{vtable, data, name}` at file 0x36E83A8
  carries **data 0x3FC00000 = 1.5f**; the neighbouring records decode to
  `bCurrentCellOnly` 0, `iMaxGrassTypesPerTexure` 2, `fTexturePctThreshold`
  0.005, which is what says the stride and the field order are right. The
  setting is absent from `Fallout4_Default.ini`, so 1.5 is what runs.
* The data slot (VA 0x1436E97B0) has exactly ONE code reference, at VA
  0x1403A74C6: `xmm6 = 4.0 / mult` (0x1403A74E5, 0x1403A74ED; falls back to
  16.0 at 0x142C4B1BC when the setting is 0), then
  `xmm2 = 1.0 / xmm6 = mult/4 = 0.375` (0x1403A75FD, 0x1403A760F).
* The loop it feeds (0x1403A7620 outer / 0x1403A7650 inner, both
  `cmp .., 0x11 ; jl`) is the **17x17 quadrant vertex grid**, and it stores an
  8-byte (u,v) pair per vertex at 0x1403A76C7 with `u = col * 0.375`.
* 17 vertices = 16 quads = one quadrant = 2,048 world units, so the vertex
  spacing is 128 units and **T = 128 / 0.375 = 341.3333**.

Addresses are for the 1.10.155 build and are re-derived, never typed, by the
script above. In the generator the value lives in one place, `lodgenLandTiling()`
(`src/lodgen.h`), and all FOURTEEN sampling sites — colour, mask and emissive,
in both the stock chunk bake and the pyramid — read it. The normal sheet
(`_msn`) is computed from VHGT and no tiling term reaches it; that it is
byte-identical at both tiling values is a gate, not an assumption.

**What the tiling does NOT fix.** It removes the speckle and it does not close
the whole-tile colour difference against vanilla. The grading — vanilla's
uniform x0.82-0.83, measured by ROADS1 — remains the open item "splat
calibration vs vanilla grading".

**VCLR is not the grading it looks like.** Measured on the Commonwealth:
**2,362 of 36,864 cells carry a VCLR at all.** Over the Sanctuary region
(cells −20..−17 x 24..27) **11 of 16 cells carry one and the bytes run
203..255**; over cells −20..−17 x 20..23, 16 of 16 carry one, over 170..255
(lane SPLAT1, `s3_candidates.py`). The range **249..255** this page carried
until 2026-09-11 does not reproduce and is withdrawn. VCLR is therefore not
white — but it is still not the grading: removing the multiply entirely moves
the sheet's local variance by **0.02 of a 52-unit excess**, and the cells that
carry NO VCLR show the LARGER excess (63.98 against 52.97). The grading that
actually moves the colour is the layer WEIGHTS and the grass tint, which is why
the gate's floor stays a blend that drops the weights rather than one that
drops VCLR.

**THE GENERATOR GATE** (`tests/spells/lodgen_terrain_model.py ring0`). An
INDEPENDENT implementation of the seven steps above — its own ESM walk, its own
BC1/BC3/BC5U decoding, its own mip choice and taps, nothing imported from the
generator — compared against the pyramid's level-0 colour at the same texel.
Reported per tile as a mean, a p95 and a max in sRGB 8-bit units, with a FLOOR
(a blend that ignores the per-texel weights) that must read far worse and a
CEILING (the bake re-decoded against itself) that must read exactly 0. Measured
2026-09-11 on two Sanctuary tiles, `--grass-tint 0`:

| tile (sw cell) | texels | mean | p95 | max | floor: weights ignored | ceiling |
|---|---|---|---|---|---|---|
| (−20, 24) | 2,704 | **3.26** | 8 | 14 | **13.70** (4.2x) | **0** over 73,984 texels |
| (−18, 24) | 2,704 | **3.59** | 8 | 17 | **15.15** (4.2x) | **0** over 73,984 texels |

The residual is the colour sheet's own BC1 block quantisation plus the two
samplers' differences, not a disagreement about the law. **The grass tint (step
7) is the one term the independent model does not carry** — it needs the grass
mesh's own average diffuse — so the gate is run against a `--grass-tint 0` bake
and the tint's size is stated separately: `lodgen_ground_cover.sh` measures a
mean tint delta of **26.97/255** over the 256 highest-cover texels of a chunk.
A runtime that folds the tint in from the cover byte and `tintStrength`, as
§1.5 states, reproduces it exactly.

### 2.5a Two optional colour switches, and what they cost (lane TILING2, 2026-09-11)

**The formula above is unchanged at the defaults, and the defaults are what
ships.** Both switches are off, so every byte of a default bake is what it was
before this section existed (gate F2: 9 files per tile, 0 differ against the
pre-lane exe, with `--land-sample footprint --blend-edges off` spelled out
explicitly as well as left unsaid).

bungo's complaint, 2026-09-11 19:2x, over a dim-4 Sanctuary sheet: *"you can see
the tiling pattern of each texture, which is not good, hard blend edges also
appear in some places, and yeah, it's muddy or blurry looking"*. Three
complaints, measured against Bethesda's own 22 shipped dim-4 sheets before any
code was written (`scratchpad/tiling2_20260911/logs/t3_laws.txt`,
`t3b_seam.txt`):

* **the repeat.** 0 of 22 shipped sheets read the 341.3333-unit repeat above
  their own null floor. Vanilla's ceiling — the worst of the 22 — is an
  amplitude of **0.264** of 255 (0.448 over that sheet's own floor). Ours at
  the default reads **1.037** and **1.261** on the two Sanctuary tiles: four
  times vanilla's worst. The `--land-tiling` fix of the same day did not create
  that repeat, it made an existing one about 1.7x more prominent (the 2,048-unit
  bake read 0.502 and 0.450 on the dimensionless scale).
* **the edges.** Our sheets have FEWER hard edges than vanilla's (52 and 190 of
  6,000 sampled, against a median of 523) and they are slightly wider (w50 5.00
  and 4.00 texels against a median of 4.12, inside vanilla's 2.50..6.50 range).
  The defect is POSITIONAL: on (-20,24) all 14 interior quadrant lines carry
  **23.6% more gradient** than that sheet's own mean, where vanilla's worst of
  22 is 10.0%, and 13 of 52 sub-texel edges sit on a quadrant line — 25.0%
  against a 9.2% chance, exact binomial **p = 0.00066**. On (-20,20) the same
  statistic is inside vanilla's range, which is why bungo saw it "in some
  places".
* **the blur.** Below 4 texels vanilla carries 22.6% of its variance and we
  carry 2.7% — a factor of eight; local variance is 38% and 17% low. **No term
  this bake can compute explains vanilla's fine detail**: ten candidates (the
  land diffuse at the footprint mip and at mip ±1, ±2, the exact footprint box
  mean, the fully averaged texture, VCLR, the shipped `_msn` slope, the best of
  eight directional shadings of that slope, and our own sheet) correlate with
  vanilla's high-pass residual at **|r| <= 0.006** against phase-twin floors of
  the same size, with the model validated at **r = +0.79** against our own bake
  at zero shift (`logs/t4_corr.txt`, `logs/t4b_align.txt`). It is REFUSED rather
  than imitated, and there is nothing to route to a grading lane either.

**`--land-sample footprint|average`** (default `footprint`) changes step 4 and
step 5's `diffuse()` only. `average` reads the landscape diffuse's `maxMip`
texel — a landscape texture ships a full mip chain to 1x1 and one repeat IS the
whole texture, so that texel is the exact average over one repeat, and no term
at T can reach the sheet. Measured consequence: the repeat falls to **0.092**
on (-20,24), under vanilla's worst by 2.9x. Its price is the detail: local
variance 12.29 -> 4.84 where vanilla is 19.81. There is a proof that what
remains on (-20,20) (0.564) is not the texture repeat: with `average` the sheet
is **byte-identical at `--land-tiling 341.3333` and at `--land-tiling 2048`**.

**`--land-detail k`** (default 0) lerps back k of the footprint sample's
departure from that average, and it buys the repeat back at exactly the same
rate: k = 0/0.15/0.25/0.35/0.50 reads a repeat of
0.092/0.175/0.268/0.366/0.532 for a local variance of
4.84/5.00/5.22/5.58/6.45. Vanilla's ceiling is crossed at **k = 0.246**, by
which point the knob has recovered 0.37 of the 14.97 local-variance levels the
average bake is missing. The repeat and the texture's own detail are one signal.

**`--blend-edges off|quadrant`** (default **`quadrant`** since 2026-09-23 --
bungo's ruling "Yes, default on", lane DEFAULTS2; it was `off` until then, and
`--blend-edges off` is the exact way back, byte for byte; the panel row *Quadrant
edges* defaults to Cross-faded), with **`--blend-margin`**
(default 128 world units = 4 texels, the 17x17 opacity grid's own spacing).
`quadrant` cross-fades the NEIGHBOURING quadrant's composite — its own layer
set, its own 17x17 opacities read past its edge, evaluated at the SAME world
point — over that margin either side of every 2,048-unit line, with a quintic
ease that is exactly 0.5 ON the line, so the two sides meet on one value.
Measured: the 14-line seam statistic falls from 1.236 to **0.955** on (-20,24)
and from 1.065 to **0.847** on (-20,20), below vanilla's own median of 1.041 on
both, at no measurable cost anywhere else (local variance -0.9%, spectrum
distance 1.227 -> 1.226, repeat +1.3%, fine-scale share unchanged).
**The chunk's own edge is blended too** (lane BLENDSEAM1, 2026-09-23). A
quadrant line on the chunk edge is a quadrant line like the other seven: both
writers read the neighbouring chunk's paint out of the same one-cell ring the
height grid already carries, so each side of a chunk boundary meets the other on
the same 50/50 mix. Only a cell with no LAND falls back to the quadrant's own
colour. Until that day this paragraph named the chunk edge as a limitation, and
it was true of the STOCK writer only: the pyramid -- whose sheet ships -- had
always blended it, so with the blend on the two writers disagreed on every
texel within the margin of the chunk edge (6,718 of 1,048,576 over the four
V9a chunks, max 25 levels, all within 3 px of the edge) and V9a-1/-2 were red.
Measured on those four chunks, the step across the two internal chunk
boundaries (boundary step over mean step, x / y): blend off 1.510 / 1.633, the
old stock blend 1.513 / 1.644 (it did nothing there), the pyramid and now both
writers 1.340 / 1.431. The stock writer keeps the ring's paint in its own array,
filled only with the blend on, so the dominant base, the cover constants and
`--blend-edges off` are what they were.

**Both switches are implemented TWICE, and that is deliberate.** The colour
sheet a `.btr` chunk carries is written by the PYRAMID pass (§2.4), not by the
stock per-chunk path: a region bake with `--vt --tex-dir` never reaches the
chunk path's composite at all. A colour change made at one site only is
silently absent from every file on disk. The pyramid copy is colour-only —
roughness, metalness, emissive and the cover opacities are NOT cross-faded,
which is what keeps `_data`, `_msn`, the `.lodm`, the BTO, the BTR and the
manifest byte-identical at every setting of both switches (gate F2 again: each
switch moves exactly the chunk colour DDS and the two `.lodt` containers).

**THE RING-0 CONSEQUENCE, for whoever writes the runtime.** §2.5's formula is
the contract between the pyramid and the runtime's ring-0 blend. If a bake is
made with `--land-sample average` then the runtime's ring 0 must average too,
or ring 0 and ring 1 will differ by exactly the texture detail these switches
remove — which is the visible half of a seam, the same reason T must match.
The switches are therefore a BAKE-WIDE choice, not a per-chunk one.

### 2.5b The colour law amended: where a chunk's sheets come from (lane TILING3, 2026-09-11)

**§2.5's formula still describes how a chunk's colour is COMPOSITED. It no longer
describes, on its own, what is WRITTEN.** Two classes of chunk now bypass it
entirely and take Bethesda's own shipped sheet instead, and one class keeps it and
gains a term. This is bungo's ruling, and his words are the specification:

> "so now we do not use our own normal map if that is toggled, but reuse these ones
> for terrain chunks."

> "out of bounds terrain blends are not included in the actual cells out of bounds,
> they never were, so we can't recover the color data anymore, because it was baked
> in a different tool outside of fo4."

**The decision, per chunk, in the order it is taken** (`lodgenVanillaChunkSheets`,
one function, called from BOTH writers -- the stock per-chunk path and the
pyramid/VT path -- so a change made here cannot be silently absent from half the
files on disk, which is the trap §2.5a records):

1. `--land-detail-source none` -> nothing below happens. The bytes are exactly
   what this document described before this section existed. Proved file by file
   against the pre-lane exe.
2. **A vanilla `_msn` exists for the chunk** -> the output `_msn` **IS that file,
   byte for byte**. Our normal bake is discarded for that chunk. The only question
   asked is whether the file exists: there is no quality guard, no threshold and no
   composite on this path.
3. **The chunk has no land paint on any cell** (no base texture and no alpha layer
   on any quadrant of any of its cells) -> the output COLOUR **IS vanilla's file,
   byte for byte**. That colour was baked outside the Creation Kit and is not
   recoverable from the ESM, so compositing it from the ESM produces the wrong
   ground, not an approximation of the right one.
4. **Otherwise** -> §2.5's composite stands, plus the crevice term of §2.5c.

**The classification in 3 is PER CHUNK, by "any cell has paint".** Not per quadrant
and not per cell: the pyramid path assembles a chunk sheet from four virtual-texture
tiles and has no per-quadrant seam at the point the sheet is written. A partly
painted chunk therefore counts as painted and keeps our composite, which is the safe
direction -- it never replaces ground we do have with ground we merely found.

**The vanilla sheets are read as LOOSE FILES under `--vanilla-lod-root`, never
through the resource stack.** This is load-bearing and not a convenience: the
resource stack would serve our own previously installed output out of the game's
`Data`, and a bake would "reuse vanilla" by copying yesterday's copy of itself, with
every measurement of the result agreeing with itself and meaning nothing. A file is
accepted only if it is longer than 128 bytes and begins with `DDS `.

**What this moves and what it does not.** Only the chunk colour DDS and the chunk
`_msn` DDS. `_data`, the BTR, the BTO, the BTO manifest, `Commonwealth.VT.2.lodt`,
`Commonwealth.VT.4.lodt` and `Commonwealth.VT.lodm` are byte-identical at every
setting, on both test tiles.

**The `_msn` is no longer format-invariant, and any reader that assumed it was must
be fixed rather than appeased.** Our bake writes DXT1 (174,888 bytes at 512x512 with
a full mip chain); Bethesda's `_msn` is BC5 (349,680 bytes). `tests/spells/lodgen_ground_cover.sh`
asserted the old size and now pins `--land-detail-source none` on its own bakes for
that reason.

**The census.** Every bake prints, unconditionally, in its `report` line:
`landDetail`, `vanillaRoot`, `msnCopied`, `msnOurs`, `colCopied`, `colOurs`,
`chunksLayered`, `chunksLayerless`, `chunksLayerlessNoVanilla`, `chunksShaded`,
`landShade`. `chunksLayerlessNoVanilla` is the one to watch: it counts chunks that
have neither paint of their own nor a vanilla sheet to borrow, and it was **0**
everywhere it has been measured -- Bethesda ships a complete 48x48 dim-4 grid over
cells -96..95, which covers the whole worldspace. Measured classes:

| region | chunk sheets | layered | layerless | layerless without vanilla | `_msn` copied | colour copied |
|---|---|---|---|---|---|---|
| (-20,24) one chunk | 1 | 1 | 0 | 0 | 1 | 0 |
| (-20,20) one chunk | 1 | 1 | 0 | 0 | 1 | 0 |
| (-36,20)..(-21,35) | 16 | 7 | 9 | 0 | 16 | 9 |
| (-48,-24)..(-1,23) | 180 (dim 4 and dim 8) | 121 | 59 | 0 | 180 | 59 |

### 2.5c The crevice term, and the two shadings that measure zero

For a chunk that keeps our composite, the fine detail of vanilla's `_msn` is added
to the colour as a **crevice darkening**:

```
detail(x,y) = normalise(vanilla _msn at mip 0) - normalise(vanilla _msn at mip 2)
dL(x,y)     = kDiv * ( d/dx detail.east + d/dy detail.north )
```

`dL` is added equally to all three 8-bit channels, rounded and clamped. `kDiv` is
`--land-shade`, default **-3.242**. Mip 2 is four texels = 128 world units = exactly
our height grid's step, so "detail" means precisely "the relief our own normal
cannot know about"; it is a definition, not a tunable. Both levels are read through
the same trilinear sampler the fit used.

**-3.242 is the median coefficient fitted on seven shipped vanilla sheets**, with
the same sign on 7 of 7 and beating its own phase-twin floor on 7 of 7 (median
r -0.1046 against a twin of +0.0006). It recovers about **1 %** of the colour's fine
variance. That is small and is stated as small.

**Two better-motivated shadings were fitted first and both read zero.** They are
recorded here so nobody fits them again:

* **A Lambert shading of the same detail** -- `kE*dEast + kN*dNorth + kU*dUp`, whose
  three coefficients ARE the light direction and strength -- reads R^2 0.0000 at a
  twin floor of 0.00001, with `kU` flipping from -1.34 to +0.45 sheet to sheet. The
  fit machinery recovers an injected 0.5 as 0.5000, so the null belongs to the data.
  A Lambert dot cannot see a rill: the rill's two walls tilt opposite ways and their
  dots cancel. The divergence does not cancel, which is why it is the term that
  survives.
* **Micro-steepness**, `acos(up fine) - acos(up coarse)` -- the slope-angle
  difference, no light and no view dependence -- reads median |r| **0.0039** against
  the crevice term's **0.1046**, with sign agreement 4 of 7, which is chance. Its
  cos-space form is the same null, so the `acos` is not hiding anything, and taking
  `coarse` from our own baked `_msn` instead of vanilla's mip 2 moves the reading by
  at most 0.0019.
* **There is no measurable hue shift with either.** Correlated against each colour
  channel's own residual, micro-steepness reads +0.0015 / +0.0023 / +0.0021 with the
  three channels moving together to within 0.0018 and the saturation residual at
  -0.0016. The shipped term is therefore a pure darkening, applied equally to all
  three channels -- which is what the measurement supports and no more.

**What no per-texel law from the `_msn` can do.** A 22-column basis -- the three
normal components, slope magnitude, `nz^2`, the divergence, and the `_msn` high-pass
at eight blur radii -- ceilings at R^2 **0.018 / 0.023** on the two test tiles. Of a
colour residual with SD 4.476 / 5.459, SD 0.6 / 0.8 is explainable and SD 4.4 / 5.4
is not. **About 98 % of vanilla's fine colour is not a function of vanilla's fine
normal**, and the remainder's moments are indistinguishable from the whole's. The
grain is texture, sampled finer than the footprint with its repeat broken; §2.5a's
switches and the experimental domain warp are where that half of the problem lives.

**One lead is open and is not shipped.** Asked as an envelope question -- is the
ground more DETAILED where it is micro-steep, rather than darker -- `|dTheta|`
reads +0.1269 median against a twin floor of +0.0040, beats that twin on 7 of 7
sheets and beats the divergence's own envelope on 5 of 7. Micro-steepness predicts
WHERE vanilla's grain lives but not its sign, so using it means modulating an
amplitude rather than adding a level: a different term, a second fitted parameter,
and a build this lane did not spend.

### 2.5d `vanilla-blend`, for reshaped terrain

`--land-detail-source vanilla-blend` writes a COMPOSITE `_msn` instead of vanilla's
file: vanilla's fine detail (the same mip 0 minus mip 2 field) added to OUR coarse
normal, with up recomputed as `sqrt(max(0, 1 - east^2 - north^2))` so the stored
normal stays unit length. It exists because a worldspace whose heights we have
changed must not wear vanilla's relief verbatim. It is never the default, and the
colour side is unchanged by it.

### 2.5e The hex tiling, which breaks the repeat without straining anything (lane TILING4, 2026-09-12)

§2.5a measured the defect: the land textures' 341.3333-unit repeat reads at an
amplitude of **1.037** and **1.261** on the two Sanctuary tiles, where the worst
of Bethesda's 22 shipped dim-4 sheets reads **0.264**. TILING3's answer,
`--land-sample stochastic`, warped world position smoothly before the lookup.
It works -- the repeat falls to 0.183 -- but the warp must strain the ground by
about **0.72** of a texel per texel to break the phase, and bungo saw the
strain: *"the proposal looks pretty good, but maybe it could use some
improvement"* (2026-09-12 00:0x, over `cmp_tiling3.png`). The strain IS the
swirl; a warp gentle enough not to swirl does not break the repeat.

**So `--land-sample stochastic` now means a histogram-preserving hex tiling**
(Heitz & Neyret 2018) and the warp is reachable as `--land-sample warp`. For a
land-texture lookup at world position `(wx, wy)`:

1. `(px, py) = (wx, wy) / S`, where `S` is the cell size in world units
   (`--land-hex`, shipped **256.0**; one land repeat is 341.3333).
2. Skew onto a triangle lattice -- `sx = px - 0.57735026918962576 * py`,
   `sy = 1.15470053837925152 * py` -- and take the enclosing triangle's three
   vertices and its barycentric weights. The lattice index is computed in
   **double**: at the far edge of the worldspace a float index quantises.
3. Each vertex `(i, j)` hashes to a fixed offset in the texture's own repeat,
   through the SAME hash the warp already uses (`lodgenWarpHash`) -- one hash in
   the file, not two.
4. The three offset taps are blended variance-preserving:
   `mean + (Σ w_k (s_k - mean)) / sqrt(Σ w_k²)`, with `mean` the land
   texture's own average colour. Dividing by `sqrt(Σ w_k²)` rather than by
   `Σ w_k` is what keeps the grain's contrast across a cell boundary instead of
   fading it towards the mean, which is the whole point of the operator.
5. **Alpha is never blended.** The alpha comes from the largest-weight tap. A
   variance-preserving blend of a constant 1.0 alpha would read about 1.07.
6. A mip bias of **-0.22** (`--land-mip-bias`) restores the grain the blend
   softens.

Being a pure function of world position it is seamless across chunk and cell
boundaries, and identical at any chunk-thread count: measured, 97 files and 0
differing between `--chunk-threads 1` and `16` on a 16-chunk block.

**It is NOT the default, and the number that decides that is the repeat.** The
fourteen shipped sheets of this lane's frozen selection/validation split were
baked by the real exe in three arms and scored by one piece of code
(`f3_full.sh`, `f3_full.py`): the hex tiling passes the repeat law on **4 of 7
and 5 of 7**. Four sheets miss on the amplitude -- (-20,20) at 0.623 against
that sheet's own 0.366 no-repeat control, and (-4,-20) 0.338, (4,-24) 0.324,
(-12,-20) 0.301 against the 0.264 absolute ceiling -- and (-36,-20) misses on
the RATIO law although its amplitude falls by a factor of twenty there, 1.501 to
0.073: that law divides the amplitude by the sheet's own no-repeat floor, and
when the amplitude collapses the floor collapses with it (the warp fails the
same sheet the same way, ratio 0.686 against 0.595). The hex offsets break the
phase BETWEEN tiles; they do nothing to the land texture's own 10.667-texel
period INSIDE one tap, which is what the four amplitude sheets are carrying.
**On the product TILING3's warp passes the repeat on more sheets than the hex
tiling does, 11 of 14 against 9 of 14**, so replacing it is a trade and not a
free improvement. The shipped default passes on **0 of 14** (0.531 to 1.618),
which is the defect of 2.5a restated on fourteen sheets. A capped warp composed
on top of the tiling (strain 0.5) was swept offline: it bought no repeat at all
and cost the swirl, so it was not built into the shipped path.

**What it does buy**, on the same fourteen real bakes: the swirl reading -- the
structure-tensor orientation coherence of the 1-5 texel grain over each sheet's
own phase-twin floor, the repeat notched out -- passes on **13 of 14** sheets
under the hex tiling against **7 of 14** under the warp. The warp reads 2.048
to 2.737 on every one of the fourteen; the hex tiling reads 0.859 to 2.084, and
its one red sheet, (-20,20), is a sheet where the shipped default is already
red and where the hex tiling reads BELOW it (2.084 against the default's 2.186,
ceiling 2.021) -- so on fourteen shipped sheets the hex tiling does not make a
single sheet's swirl worse than the build it replaces. Per-sheet grain stays
within 20 % of that build's on **14 of 14** sheets, against the warp's **1 of
14** (the warp's -1.00 mip bias is what does that; the tiling needs -0.22). On
chunk (-20,24), whole sheet: repeat **0.148** against vanilla's 0.201 and the
warp's 0.183, swirl r **1.113** against vanilla's 1.994 and the warp's 2.221,
grain 3.834 against vanilla's 4.476.

What the hex tiling substitutes for the swirls is a soft blotchiness at its own
cell scale, which no instrument in this lane gates and which
`scratchpad/tiling4_20260912/images/sheet_tiling4.png` shows at 1:1.


### 2.5f The colour grade, and the tone that is NOT a grade (lane GRADE1, 2026-09-12)

`--grade <k>` multiplies every baked colour texel by `k` immediately before
quantisation, in BOTH writers -- after the road composite and the grass tint,
before the crevice term. **The default is 1.0 and at 1.0 the multiply is not
performed at all** (`if ( g_landGrade != 1.0f )`), so a bake without the flag is
the previous bake's bytes by construction. The bake's own census line carries
`landGrade <k>` on both paths, so a sheet can never be read against the wrong
value.

The knob exists to answer a question, **not because a value was found**. What
the lane measured, and what it refuses:

**The transfer curve ours -> vanilla is not a curve.** On the two reference
chunks, on matched texels, with road texels excluded by differencing a
`--no-roads` bake and with no cover texels existing to exclude:

| model | (-20,24) RMS | (-20,20) RMS | verdict |
|---|---|---|---|
| identity | 19.94 | 22.65 | the error to beat |
| constant gain | 17.44 (k = 0.892) | 19.59 (k = 1.161) | **signs disagree** |
| affine | 9.47 (slope 0.019) | 11.32 (slope 0.185) | degenerate: predicts the mean |
| gamma | 9.48 (g = 0.038) | 11.36 (g = 0.157) | degenerate, same reason |
| sRGB slip, linear -> sRGB | 78.51 | 59.65 | refuted |
| sRGB slip, sRGB -> linear | 56.64 | 68.55 | refuted |
| BC1 codec floor | 0.497 | 0.527 | the floor all of it sits on |

Both degenerate fits "win" only by ignoring their input -- their RMS is
vanilla's own standard deviation on those texels (9.478 and 11.899) -- because
our sheet and vanilla's barely correlate texel to texel at all (r = 0.034 on
(-20,24), 0.314 on (-20,20)).

**The gain has no single value.** Over a 25-tile census (cells -24..-8 by
12..28, dim 4, shipped defaults) the per-tile optimum runs **0.615 .. 1.241**,
mean 0.892, sd 0.144; over the 96 land cells of six of those tiles it runs
**0.699 .. 1.388** with mean **1.004**. The pooled optimum is 0.8403 and cuts
the pooled RGB RMS from 24.724 to 19.644 (-20.5 %) while making 6 of 25 tiles
worse -- including (-20,20), 21.897 -> 28.488, measured through the binary.

**The gate "error reduced on BOTH reference tiles" is therefore unreachable for
any constant, and this is arithmetic and not a tuning failure.** The RGB error
of a gain `k` on a tile is a parabola in `k` with its vertex at that tile's own
`k_opt = <o,v>/<o,o>`; error rises strictly away from the vertex in both
directions. (-20,24) has `k_opt` = 0.8916 and (-20,20) has 1.1180, one either
side of 1. So every `k < 1` raises the error on (-20,20) and every `k > 1`
raises it on (-20,24), and `k = 1` is the switch's off value. Measured, whole
tile, RGB RMS:

| k | (-20,24) | (-20,20) |
|---|---|---|
| 0.8403 (pooled) | 18.11 | 28.49 |
| 0.8916 | **17.56** | (raises) |
| 1.0000 (default) | 20.13 | 21.90 |
| 1.1180 | (raises) | **20.54** |

**What the difference actually is.** The residual's only sign-consistent partner
is our own brightness (r = +0.835 and +0.827, phase twins -0.056 and +0.012):
after the best gain we are still too bright where we are bright. Height sits AT
its phase-twin floor on both tiles; slope and AO clear their floors on one tile
each with opposite signs; VCLR is ruled out by its own value (the per-texel
multiplier's mean luminance is 254.9 of 255 on six tiles, and dividing it back
out moves the fitted gain by 0.0003). The per-cell gains are blocky and straddle
one. That is a per-cell **content** difference -- which textures we blend where
-- and it belongs to the lane that owns the composite, not to a grade.

**Saturation.** Ours is more saturated than vanilla on every tile measured
(0.2453 vs 0.2215 and 0.2763 vs 0.2206; census means 0.2428 vs 0.2189). A gain
leaves HSV S exactly unchanged and the fitted gammas would crush it to 0.01 --
neither model is the mechanism. A chroma pull `c' = L + s(c - L)` fitted per
tile scatters from -0.256 to 1.301, and its best global value moves the error by
at most 0.15 levels, a fifth of the codec floor; it is not shipped.

**The grass tint stands at 0.35 unfitted**, because the ground-cover plane is
empty on all 25 census tiles: every `_data` sheet carries `dwReserved1 = 0`,
which the writer sets exactly when `coverMax == 0`, and the decoded alpha is
zero on 100 % of texels. At cover 0 the tint branch is not taken, so there is no
texel on which a strength could be fitted.

### 2.5g Terrain-guided land sampling (lane LAND1, 2026-09-12)

bungo asked two questions over the TILING3 warp: *"the warp is too strong, what
is it set to?"* (683 world units, two land repeats) and *"since we're reusing
vanilla terain normals and slope maps, might as well use them to guide this a
bit"*. `--land-guide` is the answer to the second, and it is **OFF by default**;
`--land-guide off` is the pre-LAND1 bake byte for byte, measured on all fourteen
sheets of the frozen split, every file of every tile.

**The macro field is built from the HEIGHTMAP, not from the `_msn`, and that was
a measurement.** The slope-weighted angular disagreement between the two macro
azimuths, against the floor of the heightmap's own disagreement between two
adjacent scales:

| macro scale | `_msn` vs heightmap | heightmap L vs 2L |
|---|---|---|
| 256 | 10.56 deg | 6.34 |
| 512 | 10.02 deg | 8.34 |
| **1024** (default) | **10.38 deg** | **10.83** |
| 2048 | 11.40 deg | 15.02 |

At 1024 and 2048 the `_msn` sits inside the heightmap's own octave-to-octave
uncertainty: it carries nothing the heightmap does not. Two structural reasons
finish the argument — the `_msn` is a per-chunk sheet with edges while the
height grid is a ring with one whole cell (4,096 world units) of real data on
every side, and the `_msn` does not exist where vanilla ships none.

**The macro gradient.** `lodgenLandMacroGradient` is a Sobel 3x3 over the ring
height grid at a half-step of `--land-guide-scale / 2` world units — 9 height
reads, computed ONCE per texel outside the `sampleLtex` lambda, which runs per
layer per texel. Downhill is `(-dz/dx, -dz/dy)`. `--land-guide-scale` is
**REFUSED outside 128..2048** because the safe macro reach is 3,840 units (the
one-cell ring, less the VT tile's 256-unit border), so the Sobel can never reach
the grid's clamped edge and the field stays continuous across every border.

**The five rules**, each a switch, none of them a default:

| `--land-guide RULE[:K]` | what it does | K |
|---|---|---|
| `drag` | the sample slides DOWNHILL by K x the macro normal's xy | world units |
| `aspect` | the sampling frame rotates by the downhill azimuth about the macro lattice CELL CENTRE, blended toward identity by the macro slope | 0..1 |
| `aspecthex` | the same rotation carried by the HEX lattice's three taps, each vertex rotating the plane about itself, joined by the barycentric variance-preserving blend that already joins the three offsets. Seamless AND shear-free by construction. Needs `--land-hex` | 0..1 |
| `slopewarp` | TILING3's hash warp, amplitude x the macro slope's weight | multiplier on `--land-warp` |
| `flatwarp` | the same, x (1 - that weight) | multiplier on `--land-warp` |

**The rotation is weighted on the MAP, not on the ANGLE, and that is load-bearing.**
`p + w(R(p) - p) = ((1-w)I + wR)p` is a similarity — uniform scale plus rotation,
**zero shear** — and it is continuous across `atan2`'s branch cut. Weighting the
angle by a slope weight below 1 would turn a due-west slope into a seam of up to
2*pi*w, and the swirl instrument would read the shear.

**BOTH `sampleLtex` sites are patched.** With `--vt` on, which is how every
region bake is run, the chunk sheet is assembled from the pyramid's tiles and
the stock per-chunk composite is never reached; a change made at one site only
does nothing on disk. The stock site is patched too so `--no-vt` and the
byte-identity gates stay honest.

**What it is worth, on the real exe and the frozen split** (instruments imported
unchanged from lane TILING4; 119 sweep bakes, all rc=0):

- **The hex lattice is what moves the repeat; the guide rules ride on it.** On
  their own, at macro scale 1024, the five rules pass the repeat law on 0, 0, 0,
  1 and 2 of the selection seven; `--land-hex 256` alone passes 4 of 7 and
  `aspecthex` on top of it passes the same 4 of 7.
- On top of `--land-sample stochastic` (hex 256 + mip bias -0.22), the winner
  `--land-guide aspecthex:1.0 --land-guide-scale 256` reads **5 of 7 and 5 of 7**
  against that arm's 4 of 7 and 5 of 7. That is the honest size of it: one sheet,
  and 0.054 of worst-sheet repeat.
- **Rule (a) alone cannot hide the repeat, and least of all on flat ground** —
  registered as a refuter before anything was baked, then measured. `drag:341`
  moves the flattest sheet's repeat 1.278 -> 1.126 (-11.9 %) and the steepest's
  1.501 -> 1.010 (-32.7 %); neither is within a factor of three of the ceiling.
- **The macro scale barely matters**: worst-sheet repeat 0.660 / 0.673 / 0.661 /
  0.675 at 256 / 512 / 1024 / 2048. That spread is not monotone, so it is noise.
  The default stays 1024; 256 won the tie-break by 0.001 and is reported as a
  tie-break.
- **Where it clearly helps is steep ground.** On (20,-24), the steepest of the
  validation seven (macro tan 0.3467), the repeat goes 0.916 -> **0.152** (gate
  0.264) at a grain of 2.792 against vanilla's 2.140, while the 683-unit warp
  gets only to 0.280 at a grain of **3.890** — 82 % above vanilla's, which is the
  "too strong" bungo was looking at.
- **The price is G2-band**, and it is stated rather than left out: per-sheet band
  error against the rung goes 7/7 -> 5/7 on the selection set and 7/7 -> 4/7 on
  the validation set. A rotated sampling frame redistributes energy between the
  radial bands, which is exactly what G2-band was written to notice.
  `aspecthex:0.5` keeps 6/7 of it for 0.015 of worst-sheet repeat.

**Gate F3 is NOT MET, for reasons that are inherited rather than caused**: G1 is
outside +/-20 % on both sets and so is the RUNG (-22.7 % and -37.0 %), and the
selection set's one swirl failure, chunk (-20,20), is a sheet the rung already
fails (2.223 against a ceiling of 2.021). No change to the sampler can pass a
gate its own floor fails.

**Seamless and deterministic, measured not asserted.** Every term is a pure
function of WORLD position: four region rectangles with three different ring
origins, all six rules, **72 file comparisons, 0 differing**; and 1 chunk thread
against 16, **15 files, 0 differing**. The floor for that gate is not synthetic —
the same chunk at macro scale 1024 against 512 moves the colour sheet and
nothing else, so the continuity above is not identity by inaction.

Pictures: `scratchpad/land1_20260912/images/a_land_guide_flat.png` and
`a_land_guide_slope.png` — six panels each (vanilla, plain, best of each rule
family, and the 683-unit warp), on warp_sweep.py's own window at 3:1 with the
whole sheet beneath, every panel a real DDS off disk and every bake carrying
`--road-detail 1`.

### 2.5j THE RULE: a texel depends on its WORLD POSITION only (lane VT1, 2026-09-16)

**Every term of the land sampler — the footprint, the hex tap, the warp, the
guide, the mip bias — is a function of the texel's world position and of the
worldspace's own data, and of nothing else.** Not of the tile it happened to be
baked inside, not of the chunk, not of the region rectangle, not of the thread.
A colour sheet assembled from pyramid tiles (§2.4) and a direct chunk bake of
the same ground are then the same bytes, which is exactly what check V9a-1 of
`tests/spells/lodgen_terrain_vt.sh` asks.

The rule is written here because it was broken for four days without being
visible in a picture, and because the "seamless and deterministic" paragraph
above states the property while measuring something narrower: its 72
comparisons are three ring origins on the SAME writer, so they cannot see two
different writers disagree.

**What broke it.** `lodgenTerrainFillRing` (`src/lodgen.cpp`) resolves a VHGT
row that two cells both carry by bungo's 2026-09-10 ruling, *the cell owns its
own rows* — but only inside the fill's INNER UNIT. Outside it, in the ring, the
fill is plain later-wins and the north (or east) cell's copy stands. The chunk
baker's inner unit was the dim-D chunk and the tile baker's was its own dim-D/2
tile, so the same world sample came out as one cell's copy in one grid and the
neighbour's in the other — up to 64 world units apart, on Bethesda's own
y=31|32 cell line.

That is invisible while nothing reads the ring. The macro slope reads it: it is
a Sobel at ±512 world units over exactly those samples (§2.5g), and under the
ruled default the warp amplitude is `341 × (1 − min(1, slope / 0.5))`. So a ring
sample moved the amplitude, the amplitude moved the land lookup, and 33 texels
of two chunks took a different colour — **4 and 27 bytes of 174,888**, every one
of them BC1 selector bits, in a band about 16 texels wide where the tile seam
meets the chunk's north edge.

**The fix.** The tile baker names the box of the CHUNK it will be assembled
into (`lodgenVtFloorTo( cellX0, 2 * dim )` on both axes — every pyramid level
shares one origin, §2.1, so that chunk is fixed with no option and no level
index in it). One filler, one rule, two callers who now agree about which cell
owns a sample. The chunk baker is unchanged, so a direct bake still writes the
bytes it wrote before.

**What to check when this area is touched again.** A determinism claim about
the sampler has to compare the TWO WRITERS, not one writer twice. And any term
that reaches beyond its own texel — the macro slope, an erosion gradient, an AO
march — reaches into the ring, where two bakers can disagree again the moment
they are handed different inner units.

### 2.5h Object occlusion in the far terrain (lane GROUND1, 2026-09-12)

`--terrain-object-ao` gives the far terrain ambient occlusion from the PLACED
OBJECTS, not only from its own horizon. **Off by default, and off is the
previous bake's bytes** — not to a tolerance: where nothing is in reach the
term returns exactly `1.0f` by early return and `vis * 1.0f` is bitwise
`vis`.

**The law.** The same eight-direction horizon march the terrain already runs
against its own heights, run a second time against a height field of the
objects, and combined by MULTIPLICATION because both are visibility
fractions:

```
occl += maxSlope / (1 + maxSlope)                for each of 8 directions
visObj = clamp( 1 - occl/8 * 1.6 * strength, 0, 1 )
ao8    = (visTerrain * visObj) * 255 + 0.5
```

One free function, `lodgenObjectSkyVis()`, is called from the stock chunk
composite and from the pyramid tile baker, so the two sites cannot drift
apart — the rule this document states as "both composites or it does
nothing". Coarser pyramid levels are not touched: the box filter averages
whatever level 2 produced, with the same scale-dependence section 2.3
already records for AO.

**The height field.** A world-aligned lattice of **128-unit** squares holding
the maximum Z of the placed geometry over each square, rasterised top-down
from each placement's **level-0 LOD mesh** — not the cluster spheres. A sphere
the size of a church is a hemisphere of occlusion the church does not cast;
the LOD mesh is the silhouette the player actually sees at that distance, and
it is already loaded by the chunk pass. `topAt()` takes the nearest square
and **never interpolates**: interpolating a max-Z field invents roof heights
no geometry has, and the march steps 128 units anyway. The operator is a max
into a world-aligned `floor(w/128)` lattice — commutative, associative, and
carrying no chunk-relative origin — so neither thread count nor
single-chunk-versus-region can move a byte.

**A base with no distant-LOD mesh is refused by name into the census**, not
silently skipped: it is not drawn at distance, so it does not shadow at
distance. On the measured region that is 11,826 placements over 450 bases.

**The reach is 1,458 units and it was measured, not asserted.** The march
reads 56 points; a texel none of whose 56 points lands on an occupied square
cannot move at all. Over the Commonwealth region (—24,24)..(—17,31), level 2,
1,048,576 content texels: **860,624 darkened texels, every one with its
occluder at 1,458 units or nearer and none farther**; 2,962 more darkened
with no occupied sample of their own, and **all 2,962** share a 4x4 block
with a texel that has one (block-codec endpoint refitting: drops median 6,
p95 16, max 30, against 1,007 texels that came out BRIGHTER, which a
multiplication by a number at most 1 cannot do at all). That is inside the
one-cell widening the ledger already applies, and
`docs/LODGEN_LEDGER_FORMAT.md` section 2 carries it as row 9.

**The strength default is 0.5 and that is a measurement.** 1.0 is the law as
written, and at 1.0 the term saturates: on that region the AO byte falls from
mean 211.55 to 93.33 and **276,234 of 1,048,576 texels clamp flat to zero**,
at which point "under a tree" and "under a tower" are the same byte and the
difference between them is gone before the sheet is written. 0.5 is the
largest sampled strength at which nothing clamps anywhere in the region (the
darkest texel keeps 24 of 255), so it is the largest value that still spends
the whole channel on a difference.

| strength | AO mean | AO min | texels moved | mean drop | max drop | texels at 0 |
|---|---|---|---|---|---|---|
| off | 211.55 | 65 | — | — | — | 0 |
| 0.15 | 191.84 | 49 | 833,111 (79.5 %) | 24.81 | 83 | 0 |
| 0.25 | 178.79 | 41 | 851,276 (81.2 %) | 40.35 | 116 | 0 |
| **0.50** | 146.40 | 24 | 861,873 (82.2 %) | 79.27 | 198 | 0 |
| 1.00 | 93.33 | 0 | 863,586 (82.4 %) | 143.55 | 255 | 276,234 |

It is **one region and a forested one** (45,222 of 147,456 lattice squares
occupied, canopy tops median 9,139.8). A bare region clamps later and a
denser one sooner, and neither was measured. Four sampled points are not a
curve. It is not a fit to an artefact either: vanilla's far terrain carries
no object occlusion at all, so there is nothing to score an absolute strength
against — the knob exists for the same reason `--road-opacity` does.

**Refused in combination with `--lodl`**, exit 2 with the reason named: the
`.lodl`'s own AO plane is computed from the container's stored heights by the
one function that also serves `--refresh-ao`, so putting objects there would
make a refreshed plane differ from the written one without saying so. Bake
the sheets with the switch and write the `.lodl` in a separate run without
it; the plane is unchanged either way.

**Where it sits among the other terms, and why that order.** The object
occlusion is the LAST thing to touch the AO byte and it touches only the AO
byte. It reads the height field the bake is using, so any pass that RESHAPES
the ground must run before it or the march would read a surface the sheet
does not show. Nothing here reads the object term back: the normal sheet, the
colour composite, the crevice term and the grade are all computed from the
heights and the paint, never from this byte, so the order is a one-way
dependency rather than a loop.

`--dump-object-ao FILE` writes the lattice itself (magic `OBJH`, then int32
`gx0, gy0, gw, gh`, float32 cell, then `gw*gh` float32 rows south to north,
west to east; an empty square holds -1e30f). It exists because the gate that
asks "is the ground under a building darker than the same ground elsewhere"
needs the footprint from somewhere other than the darkening map, or it is
circular. **Since lane SLAB1 it appends a SECOND `gw*gh` float32 plane, the
MINIMUM Z of the same squares** (an empty square holds +1e30f there). It is a
debug file and not a shipped format; a reader tells the two versions apart by
length, `24 + n*4` against `24 + n*8`
(`tests/spells/lodgen_slab_mask.py:read_objh`).

#### 2.5h(2) THE SLAB LATTICE: a deck is not a block (lane SLAB1, 2026-09-18)

**The defect.** The march above reads ONE number per 128-unit square, the
object's MAXIMUM Z, and `maxSlope` treats it as the top of something standing
on the ground. An elevated highway deck 1,000 units up therefore shades the
road under it as though the deck reached down to the tarmac. Measured on chunk
4.4.-12 (100 lattice squares all holding max Z 2415.9, world x 19712..20992,
y -41856..-40576): the mask sheet's B under the deck was **57.3 of 255**,
against **234.5** on open ground 4,000 units away.

**The law.** The lattice carries a MINIMUM plane beside the maximum one, and
each square the march visits is classified against the sample's own terrain
height `h0`, nearest square first:

```
empty                covered = false
minZ <= h0   WALL     wall = max( wall, (maxZ - h0) / d );  covered = false
else         CEILING  if covered: ceilOpen = min( ceilOpen, (minZ - h0) / d )

wallBlocked = wall / (1 + wall)
no ceiling seen:  occl += wallBlocked                                  // the old float, exactly
otherwise:        occl += min( 1, wallBlocked + (1 - ceilOpen/(1 + ceilOpen)) )
```

A wall blocks the sweep from the HORIZON up to its own elevation, as it always
did. A ceiling blocks from the elevation of its nearest escape UP TO THE
ZENITH, which is a smaller set the further up it is. The two blocked sets are
`0 .. F(wall)` and `F(ceilOpen) .. 1`, so their union is the SUM capped at 1,
not the max: a wall standing under a ceiling still blocks everything.
`covered` is why a canopy 1,000 units away that does not pass over the sample
contributes nothing as a ceiling and still shades through its trunk square as
a wall.

**The crossover, which is a law and not a tuning choice.** The two readings
cross at `sqrt( 128 * 1458 ) = 432` units of clearance — the geometric mean of
the nearest and furthest march steps. Above it the slab reading is BRIGHTER
than the max-Z reading; below it DARKER, because a cover that low really does
shut the sky out and the max-Z reading was letting it off. Measured through
the shipped function at strength 0.5, on a plate over everything
(`WW_OBJAO_SLAB_TEST`): H = 1000 gives 0.525468 new against 0.290780 old,
H = 200 gives 0.296502 new against 0.512195 old.

**Measured on the mask sheet**, chunk 4.4.-12, finest level, B channel means
(`tests/spells/lodgen_slab_mask.py`):

| rectangle, WORLD units | old law | slab law | move |
|---|---|---|---|
| under the deck, x 19712..20992, y -41856..-40576 | 57.316 | 83.948 | **+26.632** |
| the whole chunk, every content texel | 134.689 | 158.619 | +23.930 |
| open ground, no object over any square (control) | 234.488 | 234.488 | **0.000** |
| every marched square a WALL (refuter) | 232.999 | 233.059 | +0.060 |

**The switch.** `--terrain-object-ao-slab 0|1`, default 1, and
`--no-terrain-object-ao-slab` for the old reading. It is a SUB-TOGGLE of
`--terrain-object-ao` and not a dial: `--terrain-object-ao` itself is
unchanged, still OFF by default, and its `--terrain-object-ao-strength`
semantics are untouched. With the master off the sub-toggle moves not one byte
of any output file, and with the master on `--no-terrain-object-ao-slab`
reproduces the pre-lane sheets BYTE FOR BYTE — all ten output files of chunk
4.4.-12 `cmp` identical against the 2026-09-18 05:18:11 exe's bake.

**The census** gains `objAoSlab` (which law ran) and `objAoSlabSquares` (how
many occupied squares stand more than one cell clear of the ground): 24,729
occupied and 13,678 slabs on this chunk, reproduced out of the `OBJH` dump and
`--dump-land` by a reader that shares no code with the counter. See
docs/LODGEN_CENSUS.md 6.1.

**The gate** is `tests/spells/lodgen_slab.sh` — 16 checks, four bakes, the OFF
identity with a flipped-bit refuter, the census word, the four rectangles
above, and the law itself on three synthetic fields through the shipped
function with the old reading refused by the same bars.

---

### 2.5i The hydraulic erosion pass (lane GROUND1, 2026-09-12)

bungo, over a vanilla/ours `_msn` comparison: *"We lose all the fluvial, erosion
features and other topographical features"*, and *"the diffuse of vanilla lod
land textures, it looks like there's variety to it, some geological features
shown"*. This is the pass that puts some of that back, and the section states
what it does, what it measured, and the one statistic that is still short.

**What it is.** A droplet erosion run on its own lattice at BAKE resolution, not
on the LAND heightfield. Nothing it does reaches the mesh, the `.lodl`, the
heights, or the loaded terrain — it produces a **delta field**, and that delta is
read in two places and nowhere else:

* the `_msn` sheet, as a gradient ADD at both writers (the stock chunk pass and
  the pyramid tile baker);
* the colour composite, as a crevice-form shading, immediately before the grade.

**The switches.** `--erosion <strength>` (default **0**, off), `--erosion-iterations
N` (feedback rounds, clamped 1..8, default 4), `--erosion-seed <u32>` (default 1).
It composes with `--land-detail-source`, which gains a fifth value `erosion`.
`--erosion 0` is not a tolerance: with no token in argv the whole tree comes out
byte-identical to the previous exe's bake, ledger included.

**The lattice.** Cell = one bake texel (32 u at dim 4). The field holds
**cell-normalised** height, `h / cell`, so a slope is a slope; holding world
height there was the first of five measured defects and it produced a mean
|delta| of 13,327 world units.

**The feedback rounds, which are the fluvial claim.** Droplets traced on a field
that never changes lay independent scribbles — measured as an across/along
anisotropy of 0.81 to 0.99, against vanilla's 1.479: the right amount of relief
pointing nowhere. The pass now runs `N` rounds; round 2's water finds round 1's
grooves. Inside a round the droplets read the field as it stood at the START of
the round and write into a separate accumulator, so the order they are visited in
cannot reach a byte. The border therefore grows with the rounds —
`rounds * MAX_STEPS + HP_RADIUS`, 264 cells at 8 rounds — and that is why the
clamp is 8 and not 16.

**The high pass, and why it is not a tuning choice.** After the droplets, the
local mean of the delta over a box of 8 cells is subtracted from it. The
load-bearing reason is that **the LOD terrain must not drift from the loaded
terrain**: the full-resolution cells are never eroded, so anything this pass adds
at a scale the eye can carry across the LOD boundary is a seam. The high pass
makes the net height change over any patch wider than 17 cells exactly zero — the
drift is bounded by construction, not by taste. 8 cells sits well above the
3.5-texel channel spacing measured on 22 vanilla sheets, so the channels
themselves pass through.

**The colour gets a shading and NOT a palette.** Lane TILING3 regressed vanilla's
fine colour on vanilla's fine normal per texel and put an R-squared ceiling of
0.018 to 0.023 on any such law: about 98 per cent of vanilla's fine colour is not
a function of its fine normal. A rock-on-scoured, sediment-on-deposit palette is
exactly such a law, so there is none. What the colour gets is the crevice term's
own form at the crevice term's own fitted coefficient (`g_landShade`, -3.242
levels per unit of detail-normal divergence), computed from THIS pass's relief.
Under `--land-detail-source erosion` vanilla's crevice term does not also run:
one sheet, one crevice term, or it is shaded twice from two different surfaces.

**Where it sits among the other terms, and why that order.** The erosion delta is
computed FIRST of the two GROUND1 terms and the object occlusion (2.5h) is
computed LAST, and neither reads the other's intermediate:

* the erosion writes the `_msn` gradient and the colour shading. It never reads
  the AO byte;
* the object occlusion marches the BAKE heights — which this pass does not
  change, because the delta goes to the sheets and not to the heightfield — and
  writes only the mask sheet's B channel, which no colour or normal term reads.

So the two are independent by construction rather than by ordering luck, and
turning one off cannot move the other's bytes.

**How close it gets, measured with the same instrument that measured vanilla.**
Eight of our sheets over two four-chunk regions, against the medians of 22
vanilla sheets, at `--erosion 1 --erosion-iterations 4`:

| statistic | vanilla median | ours, off | ours, on | distance |
|---|---|---|---|---|
| fine SD (relief finer than 4 texels) | 0.2762 | 0.0398 | 0.2608 | -5.6 % |
| fine share of gradient variance | 0.4064 | 0.0365 | 0.4962 | +22.1 % |
| across/along anisotropy | 1.4792 | 0.6261 | 1.1797 | -20.2 % |
| fine amplitude vs coarse slope, r | +0.5003 | +0.4453 | +0.2896 | **-0.211, RED** |

Vanilla's fine relief gets stronger where the ground is steeper, at r = +0.50;
ours does too at +0.29 — the right sign, a bit over half the strength. The cause
is named: `MAX_MOVE` and `DELTA_CLAMP`, the two constants that stop a droplet
carving a mountain, bind hardest exactly where the capacity is largest, which is
the steep ground. Loosening them from 0.125/2.0 to 0.5/6.0 moved r from +0.10 to
+0.29 and is where they stand; loosening further was not tried against the pit
statistic.

**What it costs.** +3.0 s per dim-4 chunk, the whole bake 2.9 times as long
(6.20 s to 18.20 s on a four-chunk region, one thread). The lattice for a dim-4
chunk is 1,115,136 cells at 4 rounds against 451,584 at one. Not measured at
dim 8, 16 or 32.

**What it does NOT touch.** The `.lodl` (byte-identical with the pass on), the
chunk mesh (`.BTR` byte-identical), the land texture repeat law, and the vanilla
sheet copy path of `--land-detail-source vanilla` / `vanilla-blend`.

---

---

### 2.6 The vanilla-colour fill (lane SEAM1, 2026-09-25) -- `--vt-fill-vanilla`, OFF by default

**What it is for.** Most of the Commonwealth's LAND paints nothing: of its 2,304 dim-4
chunks, 2,023 carry LAND with no BTXT on any quadrant and no ATXT layer. §2.5 step 4
paints those with the engine's one default land texture (lane SEAM1's first commit).
That texture is right in kind and wrong in colour: Bethesda's own LOD sheets for the
same cells carry each region's colour (the Glowing Sea is ~49 luminance, the
north-east ~90), baked outside the Creation Kit and not recoverable from the ESM
(§2.5b). bungo, 2026-09-25: blend the terrain the in-game blended tiles do not cover
to the vanilla colour on those tiles, "in a proper way".

**The law, per finest-level texel, after §2.5's whole composite:**

    painted cell  a LAND quadrant with a BTXT or any ATXT layer (a NULL-LTEX layer counts)
    d             world distance from the texel to the nearest painted cell (0 inside one)
    w             smoothstep(0, band, d); w = 0 on a painted cell, so its texels are untouched;
                  w = 1 on a cell with NO LAND record (lane BAKE2, 2026-09-25, below)
    colour        colour + (T(V) - colour) * w          (RGB, 0..1; alpha untouched)
    V             Bethesda's dim-4 LOD diffuse, Mitchell-Netravali bicubic, B = C = 1/3
    T             tone + saturation match fitted on the overlap (below)

* **V is read as a loose file under `--vanilla-lod-root`**, through
  `lodgenReadVanillaSheet`, never through the resource stack -- §2.5b's reason:
  the stack would serve our own installed output. `<root>/Textures/Terrain/<WS>/<WS>.4.<x>.<y>.DDS`,
  (x, y) the chunk's SW cell, 512 texels = 32 units a texel, row 0 = north. A
  replacer at 1024 or 2048 is read at its 512 mip; any other size is refused and
  counted. The file is READ, never written, copied or shipped.
* **T is fitted once a bake** on the OVERLAP: painted cells with an unpainted cell
  within 3 cells (Chebyshev). Cell means on both sides -- ours from a 16-texel
  bake of each overlap tile through `lodgenBakeVtTile` itself (so the fit sees the
  whole composite: roads, tint, grade), vanilla's from the mean of its 128 x 128
  texels a cell. Luminance (Rec. 709 weights) gets an offset and a gain CAPPED AT
  1; chroma (rgb - lum) is scaled by the RMS ratio and shifted by the mean-chroma
  difference. The cap is measured: an uncapped gain (1.34 on Sanctuary north)
  amplified vanilla's baked relief light to 8 cell steps over the bar against
  vanilla's own 2.
* **The bar** = p99 of vanilla's own adjacent cell-mean luminance steps over the
  overlap and the ring (the unpainted cells within 3 cells of a painted one).
  **The band** = ceil(p95 over the ring of |lum ours - lum T(V)| / bar) cells,
  at least 1, at most 8. No-LAND ring cells are left out of the p95.
* **A cell with no LAND record is filled whole** (lane BAKE2, 2026-09-25). It has no
  colour of ours: the generator writes its flat placeholder grey (luminance 129.6,
  chroma 2) there. Blending FROM that grey over the band painted a pale halo round
  pre-war Sanctuary's playable block, which bungo saw on the top-down picture: no-LAND
  cells one, two and three cells off the LAND edge read +38, +30 and +19 luminance
  over vanilla, against +11 (the tone match) six cells out, while vanilla's own texels
  there are flat. Those cells were also in the band's p95, where the grey set the band
  (4 cells on pre-war). Both are removed: w = 1 on a no-LAND cell, and it does not size
  the band. The Commonwealth has LAND on every cell of -96..95, so its fill is unchanged
  by construction. Gate: `scratchpad/bake2_20260925/halo_gate.py` (RED on the exe
  before the change: 100 of 180 near cells over vanilla + offset + 8).
* **Vanilla's grid is the worldspace's own** (lane BAKE2, 2026-09-25). A dim-4 sheet
  `<WS>.4.<x>.<y>.dds` has its SW cell on the grid of `LODSettings/<WS>.LOD` (int16
  left, int16 bottom, int32 stride, int32 lodMin, int32 lodMax), not on multiples of 4.
  The Commonwealth and pre-war say -96,-96 and Nuka-World -32,-32 (phase 0,0: the old
  addressing, byte for byte); Far Harbor says -73,-59, so its sheets sit at x = 3,
  y = 1 mod 4 and a multiple-of-4 lookup found none of them. The phase is
  ((left mod 4), (bottom mod 4)), read from the vanilla root; no file = 0,0, and the
  census says which (`grid=3,1(LODSettings -73,-59)` or `grid=0,0(default)`).
  Measured on Far Harbor: 178 sheets read, 0 chunks missing, flat-grey VT.4 cells
  1517 -> 0 of 3584.
* **Where it runs.** Every finest-level tile, right after its bake, colour plane
  only. Coarser levels, their mips and the assembled `.btr` chunk sheets inherit it
  through the existing box filter (§2.3, §2.4). A tile whose cells and one-cell
  ring are all painted is skipped whole.
* **The census line** (report, only when asked): `vanillaFill overlapCells= ringCells=
  fitTiles= gain= rawGain= offset= sat= cshift= bar= p95= bandCells= tilesTouched=
  texelsFilled= texelsNoVanilla= vanillaChunksMissing= vanillaSheetsRead= noLandCells=
  noLandRingCells= texelsNoLand= grid=X,Y(source) root=`.
  With fewer than 2 overlap cells to fit on, the line says the fill is off and why.

**What is pinned.** Off is the bake before the fill existed, byte for byte, by
construction (every new line sits behind the switch; to be measured on the first
build: a fill-off bake against the pre-fill exe). On: every role but colour is byte-identical on every tile,
and colour too on every fully painted tile (gate FG2,
`scratchpad/seam1_20260925/fill_gate.py`).

**The offline proof, before the build** (`scratchpad/seam1_20260925/fill_model.py`,
the same definitions; A = the bake before SEAM1, B = §2.5 with the engine default,
F = the fill, V = vanilla; cell-mean border steps between painted and unpainted
cells, and the step AT the line against a line bar from vanilla's own at-line steps):

| region | bar | band | A max (over bar) | B max (over) | F max (over) | F at-line max / line bar |
|---|---|---|---|---|---|---|
| Sanctuary north, cells -32..-10 x 22..34 | 12.38 | 1 | 19.00 (4 of 28) | 12.53 (1) | 12.78 (1) | 9.86 / 10.15 |
| Glowing Sea edge, -44..-24 x -44..-26 | 20.56 | 2 | 76.22 (6 of 31) | 19.41 (0) | 17.82 (0) | 9.81 |
| north-east, -4..16 x 20..36 | 10.86 | 2 | 23.63 (1 of 28) | 15.78 (2) | 15.43 (1) | 9.81 |

The fill never adds a step over the bar that B does not already have, and inside
the band its steps between two unpainted cells stay within vanilla's own (Sanctuary
north p95 8.25 against vanilla's 9.88). **These are model numbers; the C++ is BUILD
PENDING** and its gate reads the baked file (`VT2=<fill bake> fill_model.py ...`,
A = the file, plus the file-vs-model agreement line).

**What it does not do.** It does not touch a painted cell, the normal, the mask,
the height or the emissive. It does not follow a plugin that reshapes terrain:
vanilla's colour belongs to vanilla's ground, so a worldspace whose heights moved
should leave it off (as `vanilla-blend` exists for the normal, §2.5d).

### 2.6b The landless-cell height fill (lane FIX1, 2026-09-26) -- `--land-fill-vanilla`, OFF by default

**What it is for.** A cell with no LAND record has no height of its own, and the
generator wrote the worldspace's default land height there: flat. Pre-war Sanctuary
(SanctuaryHillsWorld) has LAND only round the Sanctuary block, yet 101 LOD
placements stand in 15 cells east of it, 6,400 to 9,200 units up, over that flat
ground: every one floated more than 1,500 units, and the worst step at a LAND /
no-LAND edge beside them was 8,664 units. The game draws its own terrain LOD there
(the shipped `.BTR`, whose ground in those cells is the Commonwealth's hills).

**The law.** With the switch on, a cell with no LAND takes its 33x33 heights from the
game's dim-4 terrain LOD, `Meshes/Terrain/<WS>/<WS>.4.X.Y.BTR` under
`--vanilla-lod-root`, **read as input only** (no stock `.BTR` ships). The chunk's
Land triangles are rasterised onto the 128-unit grid, barycentric, once a chunk, on
the grid phase of `LODSettings/<WS>.LOD` (as §2.6 reads it). A cell any of whose
samples no triangle covers is not filled at all (a partial cell would be a cliff). A
sample a cell WITH land also holds stays that cell's: real terrain wins, the `.lodl`
seam rule. It moves the `.lodl` heights and the VT height grid (so the height and
normal sheets); colour, cover and mask still read the cell as landless. Census:
`landless-cell fill: N cells filled` on the `.lodl` stage, `landless-cell fill
(vanilla terrain LOD heights): cells asked A, filled F; dim-4 chunks read R,
missing M` on the chunk stage. The panel runs it whenever *Fill unpainted ground
with vanilla's colour* is ticked: that row already reads vanilla's LOD for the
ground no LAND paints, and this is the same ground's shape.

**Gate** (`scratchpad/fix1_20260926/fix3/float_gate.py <lodl> <lodi>`, pre-registered
before the fixed bake; pre-war region -28..2 x -12..25 at BAKE2's switches):
switch off = the bake before the change, byte for byte but the `.lodb`; switch on =
landless placements floating more than 1,500 units **101 in 15 cells -> 0**, the
edge step 8,664 -> 200 units, placements on LAND cells unchanged (1,249, none
floating); the objects (`.lodo`, `.lodi`) do not move. The Commonwealth has LAND on
every cell of -96..95, so it is unchanged by construction.

---

## 3. `.lodt` v2 — the container

**Name:** `Data\FO4CSLOD\<EDID>\<EDID>.VT.<dim>.lodt`, one per level.
**Endianness:** little, throughout. **All offsets are absolute file offsets.**
A reader computes `24·tileCount` and `tileTableOffset + 24·tileCount` in
**64-bit**: `tileCount` is u32 and the product with 24 can overflow u32.

NOT `Data\Textures\Terrain\<WS>\`, because that folder is enumerated by name
with a cap that a level's worth of tiles would blow past; the containers are read
by exact name, off the index.

MOVED 2026-09-16: it was `Data\Terrain\<EDID>.VT.<dim>.lodt` until that day,
beside the `.lodl`. bungo 2026-09-16 19:3x, "The folder should be called FO4CSLOD maybe, so it'd be Data/FO4CSLOD, sound fine?" (lane LAYOUT1) — so the `.lodl` precedent moved with it and both
now sit in the worldspace's own folder under the one root. The `"container"`
strings inside the index moved with the files (lane LAYOUT1;
`tests/spells/lodgen_layout.sh` legs (a) and (b)).

### 3.1 Header — 256 bytes at offset 0

`0xD0..0xFF` is reserved-must-be-zero, so the fields this format will
predictably want next do not each cost a version and a whole re-bake. A reader
ignores a zero-filled tail. (It was `0xC0..0xFF` in v1; the mask and emissive
descriptors took the first sixteen bytes of it, which is exactly the use the
tail was reserved for.) `COVER_FULL`, the tint strength and the level ladder
are **in the header**, not only in the index, because the coarse root must be
loadable on its own — and a tile's alpha byte is meaningless without its
normalisation constant.

| off | type | name | meaning |
|---|---|---|---|
| 0x00 | char[4] | `magic` | `'L','D','T','X'` (`LODTEX_MAGIC`, 0x5854444C). Deliberately neither `DDS `, nor `LODT` (the LANDSCAPE file's, which this extension named until 2026-09-09), nor the retired `LODV` this container carried before that date: a wrong-but-plausible parse is worse than a refusal, and both of those are refused BY NAME. |
| 0x04 | u32 | `version` | **2** (1 is the retired four-sheet layout and is refused BY NAME) |
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
| 0x79 | u8 | `sheetCount` | 3 to 9 — colour, msn, mask, then height when asked for, then emissive when a layer supplies one, then the horizon sheets (role 7) LAST and contiguous, `azimuths / 4` of them, on ONE level only (§3.5) |
| 0x7A | u8 | `anisoSupported` | 8 |
| 0x7B | u8 | `compression` | 0 stored raw, 1 zlib (RFC 1950). Any other value is a refusal, so a future codec is a named error and never a misparse. |
| 0x7C | u32 | `tileCount` | `tilesX · tilesY` |
| 0x80 | f32 | `coverNormalisation` | `COVER_FULL` |
| 0x84 | f32 | `tintStrength` | what was folded into the albedo |
| 0x88 | u16[8] | `levelDims` | the ladder, finest first, zero-padded |
| 0x98 | u32 | `indexCrc32` | CRC-32 (0xEDB88320) over the 256 header bytes **with this field zeroed**, then the whole tile table. This closes the hole per-payload CRCs cannot: a flipped bit in an `offset` points the reader at another tile's payload, whose own CRC is valid, and it loads the wrong tile and never notices. |
| 0x9C | u32 | `reserved0` | 0 |
| 0xA0 | ×10 | `sheets[10]` | 8 bytes each: u16 `dxgiFormat`, u16 `dxgiFormatCover`, u8 `role` (0 unused, 1 colour, 2 msn, **3 RETIRED `data`**, 4 height, **5 mask**, **6 emissive**, **7 horizon**), u8 `colorSpace` (0 linear, 1 sRGB), **u8 `mipSkip`**, u8 zero. `mipSkip` is 0, or 1 on a HALF-RESOLUTION sheet (`--vt-half-aux`, lane VTNORMAL1): the sheet stores the full sheet's mips 1.. and no mip 0, so its stored side is `storedTexels >> 1` and it carries `mipCount − 1` mips. The writer sets it on every sheet but the colour one (roles 2, 4, 5 and 6); the reader refuses it on the colour sheet (§3.4 rule 13). It raises no version: a file without it is byte-identical to before, and a reader that predates it refuses a half file by rule 16 (a tile's `rawBytes` no longer matches the size the header implies) rather than misparsing it. Sheets beyond `sheetCount` are all zero. **Ten since 2026-09-18**: v1 held four and its reserved tail began at 0xC0, v2 held six, and the four horizon slots came out of the same tail. The stride is still 8 and nothing before 0xA0 moved, so a reader that stops at six parses an OLD container correctly and simply cannot see a horizon sheet — which is why role 7 also raises no version: it is additive, and `--no-terrain-horizon` writes the file without it byte for byte. |
| 0xF0 | u8[16] | `reserved` | must be zero |

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

Raw payload = the concatenation of every sheet's every mip, **sheet-major and
mip-minor, in the header's own `sheets[]` order** — so a reader walks the header
rather than a table in this page:

```
sheet 0 mip 0, sheet 0 mip 1, sheet 1 mip 0, sheet 1 mip 1, ...
```

With every option on that is colour, msn, mask, height, emissive.

Rows are **tightly packed** at `blocksX · blockBytes` for a block sheet and
`storedTexels · 2` for the R16 height sheet. No row padding and no 256-byte
alignment: an upload path with a caller-supplied row pitch accepts any declared
pitch, and the 256-byte-row rules people quote are D3D12 upload-heap rules.

Computed raw size, at content 256 / border 8 / 2 mips. A BC1 sheet is
**46,240** bytes (68x68 blocks at mip 0 plus 34x34 at mip 1, 8 bytes a block), a
BC3 sheet **92,480**, the R16 height sheet **184,960**:

| | colour | msn | mask | height | emissive | tile |
|---|---|---|---|---|---|---|
| no cover | 46,240 | 46,240 | 46,240 | 184,960 | — | **323,680** |
| cover | 46,240 | 46,240 | 92,480 | 184,960 | — | **369,920** |
| cover, `--vt-cover-in-color` | 92,480 | 46,240 | 46,240 | 184,960 | — | **369,920** |
| cover + emissive | 46,240 | 46,240 | 92,480 | 184,960 | 46,240 | **416,160** |

**The mask sheet costs exactly what the retired data sheet cost**, and the two
homes for the cover byte cost exactly the same as each other — measured, §2.2a.
The emissive sheet is the only thing that adds bytes, and only where a layer
supplies one.

A half-resolution sheet (`mipSkip` 1, §3.1) at 2 mips stores only the full
sheet's mip 1 (136 texels a side): 9,248 bytes for BC1 (34x34 blocks), 18,496
for BC3, 36,992 for the R16 height sheet (computed from the same arithmetic; the
reader sizes a mip as `storedTexels >> (mip + mipSkip)`). Colour keeps its full
size.

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

### 3.5 The horizon sheet -- role 7: RETIRED, reader-only (lane HORIZONOUT,
2026-09-19)

**THE BAKED HORIZON: WHAT WAS TRIED, AND WHY IT IS GONE** (lanes HORIZON1-3,
2026-09-18/19). Three lanes baked the horizon so a far shadow could be LOOKED UP
instead of cast: this role-7 terrain sheet, and a per-vertex object horizon
stream in the `.lodi` (version 8, `docs/LODGEN_NATIVE_LODO_LODI.md` §4.11). The
sheet held, for every texel, the elevation of the highest thing it can see in
each of `azimuths` compass directions, four bins a sheet in R8G8B8A8, on the
coarsest pyramid level whose texel was no wider than 128 units. bungo ruled the
route out on 2026-09-19, after the first perspective picture of it against a
ray-cast sun: *"As you can see, the end result is terrible"*, *"So, for now, we
revert back to identity data per LOD object from the preauthored LODs"*, *"So
yeah, horizon goes bye bye now, we're back to identity"*. The two numbers behind
the ruling: at a low sun the baked object horizons disagreed with a ray-cast sun
on **50-58 %** of object pixels (lane SUNSIM1), while the identity far shadow
map simulated at 64 u disagreed on about **9 %** (lane HORIZON4).

**What the shipped exe does now.** A bake writes **no role-7 sheet**, and
`--no-terrain-horizon`, `--vt-horizon-texel`, `--horizon-near-skip` and
`--horizon-refute` are gone from the parser -- an unknown switch fails by name.
The `vt:` census line carries no `horizon*` word. The viewer no longer draws the
sheet (`src/btdterrain.cpp`, with its dead `cellsPerTile` helper).

**What the READER still does, and this is deliberate.** `LODV_ROLE_HORIZON`,
its validation in `src/io/lodvfile.*`, and the repeated-role machinery in
`src/lodtsheets.*` (`openForRole`, the `occurrence` argument, the per-role
count) all **STAY**, so a `.lodt` set baked by
`release/NifSkope.before_horizonout.exe` still opens, still validates and still
says what it carries. Role 7 is R8G8B8A8, four azimuth bins to a sheet,
`LODV_HORIZON_BINS_PER_SHEET = 4`, `LODV_HORIZON_MAX_SHEETS = 4`, one byte a
bin, `0.3529°` a step.

**Way back to the data itself.** `release/NifSkope.before_horizonout.exe`, with
`--vt-horizon-texel` and the rest. Proof that removing the producer moved
nothing else: on chunk 4.4.-12, the rung exe at `--no-terrain-horizon` and the
shipped exe at its defaults write **byte-identical** `.BTO`, `.BTO.manifest.txt`,
`.BTR`, `Commonwealth.VT.1/2/4.lodt` and all three `.DDS` -- nine of eleven
files. The two that differ are the manifest (the rung writes
`terrain.horizon "none"` and `horizonSheets 0` on each level; the shipped exe
writes neither key) and the bake record (its own exe size, timestamp, paths,
switch list and the `vt:` census line).


### 3.4 Reader validation rules

Refuse — **by name, with the field that failed** — on any of:

1. file smaller than 256 bytes
2. `magic != 'LDTX'` — and `LODT` (the landscape file, now `.lodl`) and the retired `LODV` are each named in the refusal rather than lumped into "bad magic"
3. `version != 2`. **A version 1 file is named for what it is**, not lumped
   into "bad version": its third sheet is role 3 `data` — R sky AO, G flow
   wetness, B shore proximity, A ground cover — and three of those four channels
   no longer exist. There is no converter and nothing to convert; re-bake
4. `headerBytes != 256`
5. `fileBytes` != the actual file size
6. `worldspaceEdid` not NUL-terminated inside its 32 bytes, empty, or holding a byte outside 0x20..0x7E
7. `north < south`, `east < west`, `worldNorth < worldSouth`, `worldEast < worldWest`, or the padded rectangle not containing the world rectangle
8. `levelDim ∉ {1,2,4,8,16,32}`, or `west % levelDim != 0`, or `south % levelDim != 0`
9. either padded span not a whole number of tiles; or `tilesX`/`tilesY` disagreeing with the rectangle; or either < 1
10. `tileCount != tilesX·tilesY` (computed in 64-bit)
11. `contentTexels` not a power of two in 128..1024; `borderTexels % 4 != 0`; `storedTexels != content + 2·border`
12. `mipCount < 1`, or `(borderTexels >> (mipCount−1)) % 4 != 0`, or `(borderTexels >> (mipCount−1)) << (mipCount−1) != borderTexels`, or `(contentTexels >> (mipCount−1)) < 4` — **a multiple of 4 at the coarsest stored mip**, not merely even
13. `sheetCount` outside 1..10 (`LODV_MAX_SHEETS`, §3.1); a used sheet carrying the **retired role 3**
    (refused by name, saying what role 3 used to mean); a used sheet with
    `role == 0` or `role > 7`; a duplicated role other than 7; more than four
    role-7 horizon sheets, or horizon sheets that are not the last sheets and
    contiguous (§3.5); `mipSkip` > 1, `mipSkip` >= `mipCount`, or `mipSkip` on
    the colour sheet; a **missing colour, msn or
    mask** sheet — a pyramid without one of those three is a broken bake, not a
    cheaper one, and a reader that discovers the absence at sample time cannot
    say so; a role-1/2/5/6 sheet whose format is not one of {71, 72, 77, 78} or a
    role-4 sheet whose format is not 56, or a role-7 sheet whose format is not 28
    (R8G8B8A8); `colorSpace > 1`; **more than one sheet
    declaring `dxgiFormatCover != dxgiFormat`**, or such a sheet whose role is
    neither 5 (mask) nor 1 (colour) — exactly one sheet carries the ground-cover
    alpha and the header says which by declaring the pair; a sheet past
    `sheetCount` that is not all zero (its `mipSkip` included)
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

**Path:** `Data\FO4CSLOD\<EDID>\<EDID>.VT.lodm`, beside the level containers (it
was `Data\Terrain\<EDID>.VT.lodm` until lane LAYOUT1, 2026-09-16, §3) — deliberately **not** under
`materials\`, so `lodmSourceCandidate()` can never produce it and the readers
that ignore `kind` can never open it. That function unconditionally prepends
`materials\` for a diffuse and strips only a leading `data\` for a material, so
`Terrain\…` is unreachable from any source lookup.

No parser change was needed. The envelope stays version 1 and `lodm` stays 1.
**`family` is `"pbr"` since 2026-09-11 and it is no longer vestigial**: the
sheets ARE the object family's, so the word describes them. A legacy source is
converted at bake — gloss inverted into roughness, metallic 0 — and
`terrain.maskRules` below counts how many landscape textures came by which road,
so the word is auditable rather than asserted. `kind` remains the discriminator
for which payload object to read.

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
  "lodm": 1, "family": "pbr", "kind": "terrainVT",
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
      { "role": "mask",   "dxgi": 71, "dxgiWithCover": 77, "colorSpace": "linear",
        "channels": "rmaos: R roughness, G metallic, B sky-free AO, A ground cover" },
      { "role": "height", "dxgi": 56, "dxgiWithCover": 56, "colorSpace": "linear",
        "channels": "R16_UNORM, height/8 + 32767, the shadow heightmap's own encoding" }
    ],
    "emissive": "none",
    "dropped": {
      "shoreProximity": "runtime: subtract the .lodl water body plane from the height at the sample",
      "wetness": "not baked: a close-up effect; far wetness is a weather state the runtime owns"
    },
    "maskRules": { "pbrm": 0, "legacyInverted": 14, "noneDefault": 0,
                   "withRoughnessMap": 14, "withMetallicMap": 0, "withEmissiveMap": 0,
                   "distinctLtex": 14, "roughnessDefault": 1.0, "metallicDefault": 0.0 },
    "cover": { "present": true, "normalisation": 96,
               "ordinal": true, "linearInComposition": true, "tintStrength": 0.35 },
    "coarseLevelsAreDownsamples": true,
    "alignedToWorldOrigin": true,
    "levels": [
      { "index": 0, "dim": 2, "tilesX": 96, "tilesY": 96,
        "worldUnitsPerTile": 8192, "contentTexels": 256, "unitsPerTexel": 32,
        "container": "FO4CSLOD\\Commonwealth\\Commonwealth.VT.2.lodt",
        "tiles": 9216, "present": 9216 }
    ]
  }
}
```

`worldUnitsPerTile` and `contentTexels` are **stated per level, not implied**:
a consumer picking clipmap rings reads those two numbers at load time rather
than deriving them from `dim` and a constant it has to know.

**`emissive` is `"none"` or `"present"`, in words.** A consumer must be able to
tell "this worldspace emits nothing" from "the writer forgot", and a black sheet
says neither. **`dropped`** names what version 1 carried and version 2 does not,
and where to get it instead, so a reader looking for a channel that was removed
on purpose finds the answer rather than a gap. **`maskRules`** is the per-layer
rule census: `pbrm + legacyInverted + noneDefault == distinctLtex`, and a
worldspace served entirely by `noneDefault` is a pyramid whose roughness is the
1.0 floor everywhere — which the number says out loud instead of shipping as a
measurement. The numbers above are the ones measured on the Sanctuary region
(cells −20..−17 x 24..27) on 2026-09-11.

**Form 0, the null LTEX, is a layer too, and it counts under `noneDefault`**
(lane VTFIX1, 2026-09-24). A layer whose LTEX is 0, in a chunk with no dominant
base, is painted with the none-default mask constants, and the mask cache keeps
an entry for it. Until VTFIX1 that entry was stored and never counted, so on the
whole Commonwealth `distinctLtex` was 101 against a rule sum of 100 (lane VTBAKE1:
`pbrm 0 + legacyInverted 99 + noneDefault 1`). It now reads `noneDefault 2`, and
the census identity above holds. Since lane SEAM1 (2026-09-25) a null layer
paints the engine default, which resolves through its `_s` like any legacy
layer, so form 0 no longer reaches the mask cache: expect `noneDefault` one
lower and `legacyInverted` one higher on a whole-map bake that contains a null
layer or a BTXT-less quadrant, and the identity unchanged. The gate is `tests/spells/lodgen_vtfix.sh` G1,
which checks the `.lodm` of a whole-map `--vt` bake. A region with no null layer,
such as Sanctuary's 14 = 14, does not move.

**Two optional keys (lane VTNORMAL1), both absent otherwise**, so an index
without them is byte-identical to before:

* `halfAux: true` and, on every sheet, `mipSkip` (0 on colour, 1 on the rest) and
  `texels` (the stored side, `stored >> mipSkip`) when half-resolution sheets
  were written (§3.1).
* `normalSource {rule msnCache|heights|mixed, filter, tilesMsnCache,
  tilesHeights, tilesMixed, sheetsRead, sheetsMissing}` when a normal-sheets
  folder was set (§2.2b). `rule` is `msnCache` when no finest tile took the
  heights normal, `heights` when none took a sheet, `mixed` otherwise.

---

## 5. The CLI

| flag | default | effect |
|---|---|---|
| `--cover` / `--no-cover` | off | bake ground cover and the grass tint |
| `--grass-tint F` | 0.35 | 0 keeps the albedo byte-identical and still writes the plane |
| `--cover-full N` | 96 | the fixed normalisation constant, 1..65535 |
| `--dump-cover FILE` | — | also write the raw 512² u8 plane, north-up, headerless |
| `--roads` / `--no-roads` | **on** | rasterise placed road meshes into the colour sheet (§1a). On by default under both targets because vanilla does it; `--no-roads` is byte-identical to the bake before roads existed |
| `--road-cover-suppress F` | 1.0 | how much of the ground cover a road removes under itself, 1 = all of it, 0 = leave the cover plane alone |
| `--road-detail F` | **1.0** | §1a.5, how much of the road texture's own detail survives: 1 is the sampled texel, 0 flattens each material to its average. **Defaulted to 0 until 2026-09-12**; bungo ruled for 1 on a picture. `--road-detail 0` reproduces every earlier bake byte for byte |
| `--road-ground-paint F` | **0** (since 2026-09-12, lane DEFAULTS1: bungo excluded the grass meshes inside the road NIFs from the road plane; `--road-ground-paint 1` is the way back, and the "default 1.0" below is the history) | §1a.5e, the COVERAGE multiplier for a shape inside a road model whose material lives under `materials/Landscape/Ground/` -- the verge, modelled as terrain. Such shapes win 36.1 % of the road plane on chunk (-20,20). **No value is recommended**: lane ROADS4 baked 1 / 0.75 / 0.5 / 0.25 / 0 and the seam gets monotonically WORSE (15.387 -> 33.352), so the default is 1.0 = the previous bytes. Being on coverage, 0 also stops such a shape suppressing ground cover |
| `--vt DIR` / `--no-vt` | off | write the pyramid and its index under `<DIR>/FO4CSLOD/<EDID>/` (§3, §4; `<DIR>/Terrain/` until 2026-09-16) |
| `--vt-finest 1\|2` | 2 | cells per tile at the finest level |
| `--vt-content N` | 256 | power of two, 128..512 (a cost guard; the reader accepts 128..1024) |
| `--vt-density 32\|16\|8` | 32 | world units per texel at the finest level: 32 = `--vt-finest 2 --vt-content 256`, 16 = `--vt-finest 2 --vt-content 512`, 8 = `--vt-finest 1 --vt-content 512` (byte-identical to those pairs). Refused beside `--vt-finest` / `--vt-content`, and for any other non-zero value. The panel's *Finest texel size* row defaults to 16 |
| `--vt-half-aux` | off | §3.1 `mipSkip`: the normal, mask, height and emissive sheets store only mips 1..; colour keeps its size. Refused with `--vt-mips 1`. Whole Commonwealth with cover (estimator): 0.93 / 3.40 / 12.9 GB at 32 / 16 / 8 u against 2.22 / 8.26 / 29.7. Panel row *Half-resolution normal, mask, height and emissive tiles*, unticked by default |
| `--vt-border N` | 8 | multiple of 4, and still a multiple of 4 after `mips−1` halvings |
| `--vt-mips N` | 2 | stored mips per tile |
| `--vt-height` | **off** | §2.2 layer 3: a fourth R16_UNORM height sheet per tile, on the same tile grid and border as the other three, finest from the LAND records and coarser by the same box filter. **Off by default and it stays off** -- it is uncompressed where the other three are BC1, so at content 256 / border 8 / 2 mips a height sheet is **184,960 B** against a BC1 sheet's **46,240 B** (§3.3), and a tile goes from **138,720 B** of colour classes to **323,680 B** without cover, **369,920 B** with (§2.2, §3.3). Not passing it is byte-identical to the bake before the layer existed. The panel row is **Terrain → Carry a height layer** (`LodgenVtHeightCheck`), also unticked by default |
| `--vt-fill-vanilla` | **off** | §2.6: blend the ground no LAND record paints toward Bethesda's dim-4 LOD colour, read as loose files under `--vanilla-lod-root`, tone-matched on the overlap with a measured band. Colour only; painted cells untouched. Panel row *Fill unpainted ground with vanilla's colour* (`LodgenVtFillVanillaCheck`), unticked by default |
| `--land-fill-vanilla` | **off** | §2.6b: a cell with no LAND takes the heights of the game's own dim-4 terrain LOD (`.BTR` under `--vanilla-lod-root`, read as input, never shipped) in the `.lodl` and the VT height grid. The panel runs it with *Fill unpainted ground with vanilla's colour* |
| `--vt-cover-in-color` / `--vt-cover-in-mask` | mask | which sheet's alpha carries the ground cover (§2.2a). The two cost the same bytes, measured; the default is the mask because the colour sheet's alpha is the object family's OPACITY slot |
| `--vt-compress none\|zlib` | none | payload compression |
| `--vt-btr` / `--no-vt-btr` | on | assemble the `.btr` chunk sheets from the pyramid |
| `--vt-estimate` | — | print the cost and exit without baking |
| `--lodm-check PATH` | — | parse a `.lodm` through this tree's own parser |
| `--lodt-check PATH` | — | validate a `.lodt` by every rule of §3.4, checking every tile CRC |
| `--corpus-hash` | — | both corpus hashes and the LTEX/GRAS census, with no bake |
| `--land-detail-source none\|vanilla\|vanilla-blend` | **`vanilla`** | §2.5b. `vanilla` copies vanilla's `_msn` byte for byte, and the colour too on chunks with no land paint; `none` is the bytes from before §2.5b existed |
| `--vanilla-lod-root PATH` | `E:/Tools/Fallout 4/DataUnpacked/Data` | where vanilla's sheets are read, **as loose files**, never through the resource stack |
| `--land-shade K` | **-3.242** | §2.5c, the crevice coefficient, in 8-bit luminance levels per unit of detail-normal divergence; 0 keeps the reuse and drops the shading |
| `--land-sample footprint\|average\|stochastic\|warp` | `footprint` | §2.5a. `stochastic` is the experimental repeat fix and as of lane TILING4 it means the HEX TILING of §2.5e: `--land-hex 256` with a mip bias of -0.22. `warp` is TILING3's domain warp, 683 / 1024 / 1 octave with a mip bias of -1.00, kept reachable so its measurements can be repeated; each word turns the other geometry off |
| `--land-warp A`, `--land-warp-lattice L`, `--land-warp-octaves N` | **341** / 1024 / 1 | a smooth deterministic warp of world position before the texture lookup. A pure function of world position, so seamless across chunk and cell boundaries and identical at any thread count. 0 = off. **Defaults since 2026-09-12 are bungo's pick (lane DEFAULTS1)**: warp 341, mip bias -0.22, hex 256, guide `flatwarp:1.0`; the way back to the 2026-09-11 bake, byte for byte, is `--land-hex 0 --land-warp 0 --land-mip-bias 0 --land-guide off` |
| `--land-mip-bias B` | **-0.22** | mip bias on the land-texture lookup, to restore the grain a warp smooths away. Clamped to -8..8 |
| `--land-hex UNITS` | **256** | §2.5e, the hex cell size in world units. 0 = off, and off with the other three sampler switches off is the pre-TILING4 bake byte for byte (see the `--land-warp` row) |
| `--grade K` | **1.0** | §2.5f, the colour grade: every baked colour texel times K, in both writers, after the road and the tint and before the crevice term. At 1.0 the multiply is BRANCHED OVER, so no flag and `--grade 1.0` are the previous bake's bytes (24 of 24 files, two tiles). Clamped to 0..4. **No value is recommended**: lane GRADE1 measured 25 tiles and the per-tile optimum runs 0.615..1.241, straddling 1. The pooled optimum is 0.8403 (-20.5 % pooled RGB RMS, worse on 6 of 25) |
| `--land-guide off\|drag\|aspect\|aspecthex\|slopewarp\|flatwarp[:K]` | **`flatwarp:1.0`** (since 2026-09-12, lane DEFAULTS1) | §2.5g, terrain-guided land sampling: the macro shape of the ground, read from the HEIGHTMAP (never from vanilla's `_msn` -- §2.5g measures why), steers the land-texture lookup. `K` is the rule's strength, and its units differ per rule. `off` is the pre-LAND1 bake byte for byte on all fourteen sheets of the frozen split. An unknown rule is REFUSED by name on stderr and the default stands (`flatwarp:1.0`, not `off`). `aspecthex` needs `--land-hex`. **No rule is recommended**: lane LAND1 measured 119 bakes and its winner, `aspecthex:1.0 --land-guide-scale 256` on `--land-hex 256`, buys one sheet of seven on the repeat law and costs G2-band 7/7 -> 5/7 |
| `--land-guide-scale UNITS` | 1024 | §2.5g, the world-unit scale of the macro gradient (a Sobel 3x3 at a half-step of UNITS/2 over the ring height grid). **REFUSED outside 128..2048**, because the safe macro reach is 3,840 units -- the one-cell ring less the VT tile's 256-unit border -- and past it the Sobel would read the grid's clamped edge and the field would stop being continuous across region borders. Measured to barely matter: worst-sheet repeat 0.660 / 0.673 / 0.661 / 0.675 at 256 / 512 / 1024 / 2048 |
| `--land-guide-slope TAN` | 0.5 | §2.5g, the macro slope at which a rule reaches full strength; the weight is `smoothstep` in `tan(slope) / TAN`, so flat ground is left alone and `flatwarp` is its complement |
| `--sheet-format vanilla\|legacy` | **`legacy`** | §7a. `vanilla` writes the far COLOUR sheet in the container vanilla ships -- DXT5 with the mip chain run to 1x1, 10 levels at 512 -- instead of DXT1 with 8. Measured over all 6,120 shipped Commonwealth sheets: every one is DXT5, 512x512, 10 mips, `dwReserved1` zero, alpha constant 255. The switch changes the CONTAINER and the LENGTH of the chain and nothing else: with alpha 255 the BC1 punch-through mode never fires, so mips 0..7 decode bit-identically to the legacy sheet (maxdiff 0 on every one) and a render of the two is the same picture, 0 pixels differing. `legacy` is the pre-TERRAINFMT1 bake byte for byte (10 files, 0 differ) |
| `--msn-cache DIR\|auto` | off on the command line; the panel's empty row is `auto` | §7a. **Since lane VTNORMAL1 the same sheets are also the terrain pyramid's normal (§2.2b).** Each sheet is looked for at `<DIR>/<file>`, then `<DIR>/Textures/Terrain/<world>/<file>`, then `<DIR>/Terrain/<world>/<file>` (<world> = the name up to its first dot), so DIR may be the sheets' own folder, a MOD ROOT, a Data folder or a Textures folder. `auto` takes the LAST `--resource` folder (archives skipped) that holds a `*.4.*_msn.DDS`, loose or under `Textures/Terrain/<world>/` or `Terrain/<world>/`, wider than 512 px -- vanilla's dim-4 `_msn` are 512, so an unpacked Data does not qualify -- or none; the census line reads `msnCacheDir <folder> (auto)`. In the panel `none` turns the sheets off (the command line has no `none`: leaving the switch out is off). The DDS form `<DIR>/<name>_msn.DDS` is read first: R8G8B8A8_UNORM (DXGI 28) behind a DX10 header, one mip, vanilla's channel order (R east, G up, B north), the stored G USED and the triple renormalised; any other DDS is refused on stderr and the `.png` is tried next. The PNG form: read the `_msn` for each chunk from `<DIR>/<sheet stem>.png` instead of baking or copying one, and write it UNCOMPRESSED as B8G8R8A8 behind a DX10 header with a full mip chain. The cache's own layout is measured, not assumed: its R is east (r 0.956 against vanilla's R) and its G is north (r 0.794 against vanilla's B); its B is identically 0 on 14 of 16 sampled sheets, so UP is DERIVED as `sqrt(max(0, 1 - east² - north²))` and the triple is RENORMALISED, because a median 0.11 % of texels (worst chunk 26.28 %) have `east² + north² > 1` and a clamp would flatten them. A cache miss falls through to the ordinary path and is counted. Off is the previous bytes; on, the `.lodb` ledger's switch hash moves and nothing else in it does |
| `--incremental OUT-DIR` | off | Rebake only the chunks whose INPUTS changed since the bake that wrote `OUT-DIR/<Worldspace>.lodb`, plus every chunk within one cell of one. **Every region bake writes that ledger**, so the first incremental run needs only a previous ordinary one. REFUSES rather than silently full-baking when there is no ledger, when the region or the switch digest differs, or when `--atlas`/`--arrays`/`--impostors` is asked for -- those three build ONE region-wide product out of the whole written `.BTO` list. The merge and the far-ring simplify are NOT refused: both rewrite one `.BTO` at a time. Prints a census line every run; `9 of 9 dirty` means the diff found nothing to skip and says so. Full contract, dependency map and refusal list: `docs/LODGEN_LEDGER_FORMAT.md` |

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
* **No water filter on cover.** §1.3. Version 1 pointed at the data sheet's B
  channel for shore proximity; that channel is gone, and a consumer that wants to
  suppress cover near water subtracts the `.lodl`'s water body plane from the
  height at the sample instead.
* **No shore proximity and no baked wetness.** Both dropped by bungo's ruling of
  2026-09-11 09:5x. Shore is a runtime subtraction the `.lodl` already supports;
  wetness is a close-up effect at a distance nobody views this pyramid from, and
  far wetness is a weather state. Neither is deprecated-but-written: the channels
  do not exist.
* **The object bake does not yet consume the shared mask resolver.** The law
  lives in one place (`lodgenResolveMaterialMask`) and the GLOSS is genuinely
  shared code (`lodgenLegacyGloss`, called by the arrays pass and inverted here),
  but the object path still takes its PBR answer from a `.lodm` sidecar rather
  than from a `.pbrm`. Wiring it is a separate lane; until then a model whose
  material is a PBRM bakes its objects legacy and its terrain PBR.
* **No PBRM has been exercised end to end on terrain.** The vanilla corpus
  contains none: 14 of 14 landscape textures on the Sanctuary region resolved
  `legacy-inverted`, 0 `pbrm`. The PBRM arm is gated on a fixture, not on
  shipped data, and that is stated rather than implied by a green suite.
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

## 7a. The far sheet format, and the `_msn` cache

Measured by lane TERRAINFMT1 on 2026-09-12. Every number here came off the
shipped files or off a bake, and the ones that were NOT measured say so.

### 7a.1 What vanilla ships

All **6,120** Commonwealth far-terrain sheets under
`Textures/Terrain/Commonwealth` were read (3,060 colour + 3,060 `_msn`; by
dimension 2,304 / 576 / 144 / 36 in each family):

* fourCC **`DXT5`** on every one, no exceptions, in both families;
* 512x512, **10 mips** -- the chain run to 1x1, not to the 4x4 block floor;
* declared mip count equals the number of levels the BYTES hold on all 200
  files that were checked level by level; no trailing bytes;
* `dwReserved1` (DDS header offsets 32..75) **zero on every one of the 6,120**;
* **ALPHA constant 255** in both families -- one distinct value over
  13,107,200 texels on each side of a 50 + 50 sample. Vanilla's alpha carries
  nothing.

Because vanilla ships no `_data` sheet at all, the `WWCV` cover stamp in that
sheet's `dwReserved1` cannot collide with anything vanilla writes. There is no
reconciliation to make.

What could NOT be established: what the ENGINE does with either alpha. The
FO4 Community Shaders tree at `wt-fixfirst` carries no terrain-LOD shader
replacement, so there was no `_msn` sampling to cite and none is guessed at
here.

### 7a.2 `--sheet-format vanilla`

On chunk `Commonwealth.4.-20.24`, cells -20 24 .. -17 27, dim 4, `--vt --cover
--road-detail 1`: the colour sheet comes out **DXT5, 10/10 mips, 349,680
bytes** -- vanilla's exact byte count and format -- with no trailing bytes,
`dwReserved1` zero and alpha 255 only.

**The switch cannot change a colour.** With alpha constant 255 the BC1
punch-through mode never triggers, so the colour block a DXT5 sheet stores and
the colour block a DXT1 sheet stores are the same bytes: mips 0..7 decode
bit-identically between the two containers (maxdiff 0 on every level), and a
headless render of the two sheets on the same mesh at the same pinned camera
differs in **0 pixels**. What the switch buys is the two extra mip levels and
the container vanilla uses. The `_msn` is untouched by it on any chunk where
`--land-detail-source vanilla` is in force, because a copied sheet is never
re-encoded.

### 7a.3 `--msn-cache`

The cache this was written against is a cleaned 2K `_msn` set, 2,304 PNGs at
2048x2048 RGB named `Commonwealth.4.<x>.<y>.png`, covering exactly the 2,304
dim-4 vanilla `_msn` sheets, 15.49 GiB. Its layout is NOT vanilla's and was
measured rather than assumed -- see the `--msn-cache` row in §5 for the channel
correspondences and the renormalisation, and the lane report for the
correlations against a phase-randomised twin.

The written sheet, chunk (-20,24): **B8G8R8A8_UNORM, 2048x2048, 12/12 mips,
22,369,768 bytes**, alpha 255, unit length mean 1.0000 with the worst texel
**0.68 levels** off unit (vanilla's own sheet: mean 1.0215, worst texel 77.67
levels off). Block-grid residue at period 4 -- mean |neighbour difference| on
the lines where the index is a multiple of 4 over the mean elsewhere, 1.0 = no
grid -- **1.0029** against the rung's **1.1210**.

**Why uncompressed, with the refuter.** Running the same cleaned sheet through
this tree's own colour-block rule (`lodgenEncodeBC1Block`: min/max luminance
endpoints, four-colour palette, per 4x4 block -- a BC3 colour block is the same
block) takes the grid residue to **1.9849**. The blocks come back. That number
is harsher than vanilla's own 1.1210 because our endpoint rule is min/max
luminance and not least squares, so the DIRECTION is certain and the MAGNITUDE
is encoder-specific. **BC7 is not implemented in this tree** -- there is no BC7
encoder anywhere in it -- so the only alternatives written were uncompressed
and the existing block formats.

Size, for a decision that is not this document's to make: vanilla's 6,120
sheets are 2,140,041,600 B = 1.99 GiB in total. Uncompressed 2048 `_msn` for
the 2,304 dim-4 chunks is 48.00 GiB; uncompressed 1024, 12.00 GiB; BC7 2048
with a full chain would be 12.00 GiB, BC7 2048 mip 0 only 9.00 GiB, BC7 1024
3.00 GiB. **Load cost was not measured.**

**Since 2026-09-18 the DDS form is read first** (bungo's upscaled set,
`<name>_msn.DDS`, §5), and **since 2026-09-23 the folder also feeds the pyramid**
(§2.2b, lane VTNORMAL1). Measured over Sanctuary (-20,24): the L02 normal against
his sheet box-reduced to 32 u reads r 0.9449 east / 0.8624 north, which is
exactly this tree's BC1 of that sheet (100 % of texels within 1/64). The north
loss is the luminance endpoint rule above -- `lodgenEncodeBC1Block` weights north
(blue) at 0.114 -- the same finding as the grid residue. A least-squares / PCA
endpoint fit for normal sheets would lift north to about 0.925 (simulated); it
moves the no-folder bytes, so it is a separate lane and bungo's call. At 8 u the
finest level reads 0.966 / 0.935.

### 7a.4 What these switches are NOT

Neither switch affects the blue-purple cast on a generated chunk. That cast is
the terrain `.BTR`'s VERTEX COLOURS: under `opts.terrainIdentity` the generator
writes the land material class, wetness, ambient occlusion and shore proximity
into the "Vertex Colors" slot, and a consumer that multiplies the albedo by the
vertex colour then renders the chunk blue-purple. Measured on chunk (-20,24)
with vanilla's own sheets on both meshes at one pinned camera: vanilla's `.BTR`
carries neutral vertex colours (237.3 / 237.3 / 237.3 mean under
`WW_RENDER_FLAT`), ours carries 50.9 / 49.3 / 202.1, and
`--no-terrain-identity` puts ours back to 237.2 / 237.2 / 237.2 and its lit
render 0.48 levels from vanilla's against a before of 137.77.
`--no-identity` is a DIFFERENT switch (the OBJECT identity channels) and leaves
the terrain `.BTR` byte-identical. Whether the identity channels should keep
that slot is not a question a measurement answers.

**Since 2026-09-12 that question is answered and the default is the other way
round** (lane DEFAULTS1). bungo ruled at 15:53 that the terrain `.BTR`'s
identity vertex colours are not used -- "We don't bake BTR for FO4CS, and so we
do not use of that data for it at all" -- and at 15:56 that "Legacy terrain
bakes stay as they were, no extra data for FO4CS to be included in them. Only
the .lod ones have new data in them." So `LodgenTerrainOptions::terrainIdentity`
and `lgTerrainIdentity` are now **false**: a bake with no switches writes
vanilla's land descriptor `52776558133763` with neutral vertex colours, and
`--terrain-identity` is the opt-in that puts the four channels back. The same
ruling turned the OBJECT identity channels off by default, with `--identity` as
their opt-in. The FO4CS data those channels duplicated lives in the `.lodt` /
`.lodl` / `.lodo` / `.lodi` files, which did not change, and the
`.bto.manifest.txt` sidecar is now written either way. Gate:
`tests/spells/lodgen_defaults.sh` phase (c) reads the descriptor of every
`.BTR` and `.BTO` of a default bake at dim 4, 8, 16 and 32.

---

## Provenance

Sections 2.2, 2.2a, 2.5, 3 and 4 were rewritten for container version 2 on
**2026-09-11** by lane TERRAIN-R. Every line number below was found again from
its own ANCHOR TEXT against the sources stamped here, never shifted by a delta
(`ww-contract-provenance` step 3, script
`scratchpad/terrain_r_20260911/anchors.txt`); each anchor was found exactly
once. The measured numbers in 2.2a and 2.5 were re-derived from the artefacts
they describe rather than copied forward.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/io/lodvfile.cpp` | `9fe897224195b467` | 33,426 | 799 |
| `src/io/lodvfile.h` | `ff1225d83723d5ee` | 9,807 | 223 |
| `src/lodgen.cpp` | `6259c599cb16a0f3` | 414,253 | 9423 |
| `src/lodgen.h` | `3a1ae3c4d8bef0a0` | 34,977 | 639 |
| `src/esmdata.cpp` | `6f6854a989e485b3` | 29,968 | 940 |
| `src/esmdata.h` | `5475f77563462c2f` | 14,191 | 329 |

| claim | line | anchor |
|---|---|---|
| magic `LDTX`, header 256 B, payload alignment 4096 | `lodvfile.h:67`, `lodvfile.cpp:32` | `constexpr quint32 LODTEX_MAGIC = 0x5854444CU;` |
| **version 2**, and that a v1 file is refused by name | `lodvfile.h:91`, `lodvfile.cpp:29, 468` | `constexpr quint32 LODTEX_VERSION = 2;` / `refused: version 1 container -- four sheets whose ` |
| the role set: mask 5, emissive 6, data 3 retired | `lodvfile.h:101-102` | `LODV_ROLE_MASK = 5,` / `LODV_ROLE_EMISSIVE = 6` |
| six sheet descriptors at 0xA0, reserved tail from 0xD0 | `lodvfile.h:107, 163`, `lodvfile.cpp:148, 197` | `constexpr int LODV_MAX_SHEETS = 6;` / `LodvSheetDesc sheets[LODV_MAX_SHEETS];` (the two 0xA0 loops are the write and the read side, in that order) |
| the cover carrier is whichever sheet declares two formats | `lodvfile.cpp:220` | `const quint16 fmt = ( cover && sd.dxgiFormatCover != sd.dxgiFormat )` |
| rule 13: role 3 refused by name | `lodvfile.cpp:555` | `refused: sheet %1 has role 3 \`data\` (R AO, G wetness, ` |
| rule 13: colour, msn and mask are all required | `lodvfile.cpp:591` | `refused: a version 2 container must carry the colour ` |
| rule 13: exactly one cover carrier | `lodvfile.cpp:581` | `refused: sheets %1 and %2 both declare a ` |
| §2.2 the three mask rules and their words | `lodgen.h:376`, `lodgen.cpp:1787` | `enum LodgenMaskRule` / `void lodgenResolveMaterialMask( const QString & dataRoot, const QString & matName,` |
| §2.2 the gloss law, shared with the object arrays pass | `lodgen.h:392`, `lodgen.cpp:1782, 4657` | `float lodgenLegacyGloss( float smoothness, float specGreen );` / `r = b8( lodgenLegacyGloss( smooth, sG ) );` |
| §2.2 the per-LTEX resolution and its rule census | `lodgen.cpp:6768` | `struct LodgenVtMaskCache` |
| §2.2 the mask texel: cover, roughness, metallic, AO | `lodgen.cpp:7200` | `out.mask[size_t( j ) * S + i] =` |
| §2.3 the mask filters plainly on all four channels | `lodgen.cpp:7318` | `out.mask[o] = ( ( ( acc[3][3] + 2 ) >> 2 ) << 24 )` |
| §2.2 the emissive sheet is decided before any container opens | `lodgen.cpp:7532` | `wantEmissive = maskCache.withEmissive > 0;` |
| §3.1 the sheet descriptors the writer emits | `lodgen.cpp:7580` | `h.sheets[2] = { LODV_DXGI_BC1_UNORM, maskCoverFmt, LODV_ROLE_MASK, 0 };` |
| §4 `family: "pbr"` | `lodgen.cpp:7776` | `root.insert( QStringLiteral( "family" ), QStringLiteral( "pbr" ) );` |
| §4 `dropped` and `maskRules` | `lodgen.cpp:7846, 7865` | `t.insert( QStringLiteral( "dropped" ), dropped );` / `t.insert( QStringLiteral( "maskRules" ), rules );` |
| §2.2 the layer's material is MNAM and its `_s` map is TX07 | `esmdata.h:138`, `esmdata.cpp:501` | `struct EsmLtexTextureSet` / `const EsmLtexTextureSet & EsmWorld::ltexTextureSet( quint32 ltexForm ) const` |
| §5 `--vt-cover-in-color` | `lodgen.h:483` | `bool coverInColor = false;` |

### ROADS1, 2026-09-11

Section 1a is new. Its measurements come from Bethesda's shipped
`Commonwealth.4.-20.20.DDS` and `_msn`, from `Fallout4.esm`, and from the
scripts under `scratchpad/roads1_20260911/` (`esm_refs.py`, `rasterlib.py`,
`placements.py`, `matinfo.py`, `measure_vanilla.py`, `cmp_vanilla2.py`,
`cmp_vanilla3.py`, `family_auc.py`, `road_metric.py`); the after-state numbers
are re-derived from the artefacts, not copied forward.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodgen.cpp` | `9aa5f79eab53d9b8` | 439,719 | 10043 |
| `src/lodgen.h` | `d3c07904243c8b87` | 38,967 | 707 |

| claim | line | anchor |
|---|---|---|
| §1a.3 the road test | `lodgen.h:395`, `lodgen.cpp:5669` | `bool lodgenIsRoadModel( const QString & modelPath );` / `bool lodgenIsRoadModel( const QString & modelPath )` |
| §1a.4 the switch and the cover suppression | `lodgen.h:376, 381` | `bool roads = true;` / `float roadCoverSuppress = 1.0f;` |
| §1a.4 the road applied between VCLR and the tint, chunk path | `lodgen.cpp:6586` | `const quint32 rp = roadPlane[size_t( py ) * RES + px];` |
| §1a.4 the same on the tile path | `lodgen.cpp:7734` | `const quint32 rp = roadPlane[size_t( j ) * S + i];` |
| §1a.5 the topmost triangle wins, and what a texel gets | `lodgen.cpp:5858` | `void rasterise( float wx0, float wyTop, float upt, int S,` |
| §1a.5 the material path rule | `lodgen.cpp:5764` | `QString lodgenRoadMaterialPath( const QString & matName )` |
| §1a.7 the census fields | `lodgen.h:400`, `lodgen.cpp:5716` | `struct LodgenRoadCensus` / `QString LodgenRoadCensus::line() const` |

### TILING2, 2026-09-11

Section 2.5a is new. Its vanilla numbers come from Bethesda's 22 shipped dim-4
sheets and its own numbers from bakes into `scratchpad/tiling2_20260911/out/`
made by `release/NifSkope.exe` (21:52:22, 21,466,624 bytes), measured by the
scripts under `scratchpad/tiling2_20260911/` (`t1_lib.py`, `t3_laws.py`,
`t3b_seam.py`, `t4_corr.py`, `t4b_align.py`, `t5_variants.py`, `t6_local.py`)
with the logs beside them. No number here is copied forward: each was re-read
off the artefact it describes. Every line below was found again from its own
anchor text against the sources stamped here (`ww-contract-provenance` step 3).

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodgen.cpp` | `104b11a7d30b4dd9` | 472,149 | 10791 |
| `src/lodgen.h` | `a9a9f28e57df191a` | 53,019 | 940 |
| `src/nifcli.cpp` | `177d8fab32d01d42` | 303,805 | 6782 |

| claim | line | anchor |
|---|---|---|
| §2.5a the land-sample switch and its default | `lodgen.h:115`, `lodgen.cpp:5868` | `bool lodgenLandSampleAverage();` / `static bool  g_landSampleAverage = false;` |
| §2.5a the detail knob, clamped to [0,1], default 0 | `lodgen.h:117`, `lodgen.cpp:5883` | `float lodgenLandDetail();` / `float lodgenLandDetail()` |
| §2.5a the edge switch and its margin | `lodgen.h:135, 137`, `lodgen.cpp:5870` | `int lodgenBlendEdges();` / `float lodgenBlendMargin();` / `static int   g_blendEdges        = 0;` |
| §2.5a the averaged sample, at BOTH sampling sites — the anchor occurs exactly twice and that is the claim | `lodgen.cpp:6965` (stock chunk path) and `lodgen.cpp:8254` (pyramid path) | `if ( !lodgenLandSampleAverage() )` |
| §2.5a the chunk path's quadrant composite, lifted so a neighbour can be evaluated | `lodgen.cpp:6990` | `auto quadComposite = [&]( const EsmLand & pl, int pq,` |
| §2.5a the pyramid path's COLOUR-ONLY copy — the one that writes the sheets | `lodgen.cpp:8333` | `auto quadColorAt = [&]( const EsmLand & pl, int pq,` |
| §2.5a the cross-fade is quintic and exactly 0.5 on the line, both paths | `lodgen.cpp:7070, 8376` | `// Perlin's quintic, halved: 1 at the line -> 0.5` |
| §2.5a the four flags | `nifcli.cpp:6055, 6059, 6064, 6068` | `else if ( t == QLatin1String( "--land-sample" ) ) {` / `else if ( t == QLatin1String( "--blend-edges" ) ) {` |

### TILING3, 2026-09-11

Sections 2.5b, 2.5c and 2.5d are new, and six rows were added to §5. Their vanilla
numbers come from Bethesda's shipped dim-4 sheets read as LOOSE FILES under
`E:/Tools/Fallout 4/DataUnpacked/Data` (never through the resource stack, for the
reason §2.5b gives); their own numbers come from bakes into
`scratchpad/tiling3_20260911/out/` made by `release/NifSkope.exe` (23:26:29,
21,484,032 bytes) and by the rung `release/NifSkope.before_tiling3.exe`
(21,466,624 bytes), measured by the scripts under `scratchpad/tiling3_20260911/`
(`d1_geology.py`, `d2_deep.py`, `d3_shade.py`, `d4_steep.py`, `a3_abc.py`,
`a4_warp.py`, `a5_tune.py`, `a6_pick.py`, `f3_gate.py`) with the logs beside them.
No number here is copied forward: each was re-read off the artefact it describes.
Every line below was found again from its own anchor text against the sources
stamped here (`ww-contract-provenance` step 3).

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodgen.cpp` | `ac99f2e200eb7117` | 495,829 | 11400 |
| `src/lodgen.h` | `55b5080937b0fa95` | 59,663 | 1062 |
| `src/nifcli.cpp` | `12fee6ea82323842` | 307,451 | 6845 |

| claim | line | anchor |
|---|---|---|
| §2.5b the mode enum and its accessor | `lodgen.h:206` | `int lodgenLandDetailSource();` |
| §2.5b the default IS `vanilla` | `lodgen.cpp:6105` | `static int g_landDetailSource = LODGEN_LANDDETAIL_VANILLA;` |
| §2.5b the loose-file root and its default | `lodgen.cpp:6103` | `static QString g_vanillaLodRoot =` |
| §2.5b a vanilla sheet is read, and only as a loose file | `lodgen.cpp:6198` | `bool lodgenReadVanillaSheet( const QString & ws, int dim, int chunkX, int chunkY,` |
| §2.5b the per-chunk paint test -- "any cell has paint" | `lodgen.cpp:6235` | `bool lodgenChunkHasLandPaint( const EsmWorld & world, int chunkX, int chunkY, int dim )` |
| §2.5b the ONE decision function, and the `none` early-out | `lodgen.cpp:6391, 6399` | `void lodgenVanillaChunkSheets( const EsmWorld & world, const QString & ws, int dim,` / `if ( g_landDetailSource == LODGEN_LANDDETAIL_NONE )` |
| §2.5b the `_msn` copy, taken only on the `vanilla` mode | `lodgen.cpp:6409` | `if ( haveMsn && g_landDetailSource == LODGEN_LANDDETAIL_VANILLA ) {` |
| §2.5b both writers call the same pair -- the anchor occurs exactly twice and that is the claim | `lodgen.cpp:7898` (stock chunk path) and `lodgen.cpp:9602` (pyramid path) | `lodgenVanillaChunkSheets( world,` |
| §2.5b the single write site for both sheets | `lodgen.cpp:6437` | `bool lodgenWriteChunkSheets( const QString & base, int res,` |
| §2.5b the census, printed unconditionally in `report` | `lodgen.cpp:9876` | `const LodgenVanillaReuse vr = lodgenVanillaReuseCensus();` |
| §2.5c the detail field, mip 0 minus mip 2 through the sampler | `lodgen.cpp:6261` | `static bool lodgenVanillaMsnDetail( const QByteArray & bytes, int res,` |
| §2.5c the crevice term itself | `lodgen.cpp:6319` | `static int lodgenShadeWithCrevice( const QByteArray & msnBytes, int res,` |
| §2.5c the shipped coefficient -3.242, clamped to +/-64 | `lodgen.cpp:6112, 6156` | `static float g_landShade = -3.242f;` / `g_landShade = kDiv < -64.0f ? -64.0f :` |
| §2.5d the blend, with up recomputed | `lodgen.cpp:6360` | `static int lodgenBlendVanillaDetail( const QByteArray & msnBytes, int res,` |
| §5 the three new flags | `nifcli.cpp:6104, 6117, 6122` | `else if ( t == QLatin1String( "--land-detail-source" ) ) {` / `"--vanilla-lod-root"` / `"--land-shade"` |

### TILING4, 2026-09-12

Section 2.5e is new and two rows of §5 moved (`--land-sample` gained `warp`;
`--land-hex` is new). Its vanilla numbers come from Bethesda's shipped dim-4
sheets read as LOOSE FILES under `E:/Tools/Fallout 4/DataUnpacked/Data`; its own
numbers come from bakes into `scratchpad/tiling4_20260912/out/` made by
`release/NifSkope.exe` (2026-09-12 02:08:57, 21,487,616 bytes) and by the rung
`release/NifSkope.before_tiling4.exe` (21,484,032 bytes), and from the
fourteen-sheet sweep under `scratchpad/tiling4_20260912/`
(`s1e_law.py` the swirl law, `h1_sweep.py`, `h2_rescore.py`, `h3_sweep.py`,
`h4_pick.py`, `h4b_flip.py`, `t4_gates.py`, `f2_gate.py`, `f3_real.py`,
`f3_full.sh` + `f3_full.py` -- all fourteen sheets baked by the real exe in
three arms, which is where every count in 2.5e comes from -- and
`hex_parity.py`) with the logs beside them. No number here is copied forward:
each was re-read off the artefact it describes. Every line below was found
again from its own anchor text against the sources stamped here
(`ww-contract-provenance` step 3).

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodgen.cpp` | `ede9807373f12959` | 503,111 | 11557 |
| `src/lodgen.h` | `939b36f451ec4e92` | 62,900 | 1113 |
| `src/nifcli.cpp` | `df0f4a2230766684` | 309,875 | 6881 |

| claim | line | anchor |
|---|---|---|
| §2.5e the cell size, 0 = off | `lodgen.cpp:6081` | `static float g_landHexSize = 0.0f;` |
| §2.5e the two lattice constants, in double | `lodgen.cpp:6086` | `static const double LODGEN_HEX_SKEW  = 0.57735026918962576;` |
| §2.5e the triangle, its three vertices and its weights | `lodgen.cpp:6096` | `void lodgenLandHexCell( double wx, double wy, double size,` |
| §2.5e the per-vertex offset, through the warp's own hash | `lodgen.cpp:6122` | `static inline double lodgenLandHexOffset( qint32 i, qint32 j, quint32 k )` |
| §2.5e the variance-preserving blend, and alpha taken from the largest-weight tap | `lodgen.cpp:6133` | `FloatVector4 lodgenLandHexTap( const DDSTexture16 * tex,` |
| §2.5e the setter, clamped at 0 | `lodgen.cpp:6183` | `void lodgenSetLandHexSize( float units )` |
| §2.5e BOTH sampling sites call it -- the anchor occurs exactly twice and that is the claim | `lodgen.cpp:7666` (stock chunk path) and `lodgen.cpp:8990` (pyramid path) | `lodgenLandHexTap( tex, swx, swy, TILE` |
| §2.5e the declarations | `lodgen.h:225` | `float lodgenLandHexSize();` |
| §5 `stochastic` means the hex tiling, 256 units at bias -0.22 | `nifcli.cpp:6090` | `lodgenSetLandHexSize( 256.0f );` |
| §5 `--land-hex` on its own | `nifcli.cpp:6116` | `else if ( t == QLatin1String( "--land-hex" ) ) lodgenSetLandHexSize( next().toFloat() );` |

### GRADE1, 2026-09-12

Section 2.5f is new, step 8 of the §2.5 ring-0 formula is new, and `--grade`
is a new row of §5. Vanilla's numbers are Bethesda's shipped dim-4 sheets read
as LOOSE FILES under `E:/Tools/Fallout 4/DataUnpacked/Data`; ours are bakes into
`scratchpad/grade1_20260911/out/` made by `release/NifSkope.exe`
(2026-09-12 03:06:21, 21,489,152 bytes, md5 `6af74b4b4667ce50c4506a2d42a04fdf`)
and, for the "the flag is new" gate only, by the rung
`release/NifSkope.before_grade1.exe` (2026-09-12 02:08:57, 21,487,616 bytes).
The instruments are `scratchpad/grade1_20260911/gradelib.py` with
`g0_controls.py` (the known-answer controls: 24 checks, 0 failures, a synthetic
gamma and gain recovered to 3 decimals before any verdict was formed),
`g1_curve.py` (the three models), `g2_position.py` (multi-scale and the residual
correlations, every r against its own phase-twin floor), `g3_cells.py` (96 land
cells), `g4_census.py` (25 tiles), `g5_decide.py` (the region split, the
parabola, the pooled optimum) and `g6_gates.py` (the five ship gates, measured
through the binary: 8 checks, 0 failures), with the logs beside them. The road
mask is the difference between the default bake and a `--no-roads` bake, because
roads are ON by default and `--roads` is a no-op. No number here is copied
forward; each was re-read off the artefact it describes.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodgen.cpp` | `0758cb7cd498ac53` | 504,834 | 11593 |
| `src/lodgen.h` | `19d7aed0e6917c6e` | 63,508 | 1122 |
| `src/nifcli.cpp` | `28ee83fcc1ed16e5` | 310,457 | 6889 |

| claim | line | anchor |
|---|---|---|
| §2.5f the grade's state, default 1.0 | `lodgen.cpp:6260` | `static float g_landGrade = 1.0f;` |
| §2.5f the setter, clamped 0..4 | `lodgen.cpp:6305` | `void lodgenSetLandGrade( float k )` |
| §2.5f/§2.5 step 8 the multiply, branched over at 1.0 -- the anchor occurs exactly TWICE and that is the claim: the stock chunk writer and the pyramid writer both grade | `lodgen.cpp:7962` (chunk) and `lodgen.cpp:9258` (pyramid) | `if ( g_landGrade != 1.0f )` |
| §2.5f the census prints the value, so a sheet cannot be read against the wrong k | `lodgen.cpp:8035` (JSON) and `lodgen.cpp:10110` (text) | `lodgenLandGrade()` |
| §2.5f the declarations | `lodgen.h:270` | `void lodgenSetLandGrade( float k );` |
| §5 `--grade` | `nifcli.cpp:6166` | `else if ( t == QLatin1String( "--grade" ) ) lodgenSetLandGrade( next().toFloat() );` |

### ROADS4, 2026-09-12

Section 1a.5e is new and 1a.5's colour bullet is amended: `--road-detail`
defaults to **1.0** from this lane, by bungo's ruling on a picture, and
`--road-detail 0` reproduces every earlier bake byte for byte (9 files on chunk
(-20,20) and 10 on (-8,8), `bake.log` excluded because it records the command
line). `--road-ground-paint` is a new row of section 5.

Vanilla's numbers are Bethesda's shipped dim-4 sheets read as LOOSE FILES under
`E:/Tools/Fallout 4/DataUnpacked/Data`; ours are region bakes into
`scratchpad/roads4_20260912/out/` (eleven variants on two chunks) made by
`release/NifSkope.exe` (2026-09-12 06:31:05, 21,819,904 bytes), with the rung
`release/NifSkope.before_roads4.exe` (2026-09-12 05:48:33, md5
`980e64c1aa4e5478b5833d83ebea9655`) used for the before column. The instruments
are `scratchpad/roads4_20260912/r4lib.py` (the projection: which shape wins each
texel, its material, its vertex alpha -- the alpha buffer initialised to ONES,
which is the correction of the earlier -0.792 finding), `r4_material.py` (the
folder classifier), `r4_bake.sh`, `r4_gp.py` (the variant table, `logs/gp.json`),
`r4_gates.py` (`logs/gates.json`) and `r4_pics.py`, with the seam sets re-read by
`scratchpad/roads2_20260911/seam.py` into `seam_r4_*.json`. The road mask is the
difference between a bake and a `--no-roads` bake of the same generator;
vanilla has no such pair, so section 1a.5e's width comparison uses the PROJECTED
road width, a world fact from the ESM, and says so.

What this lane could NOT meet, in its own words: `tests/spells/lodgen_roads.sh`
is 11 checks / 1 failure on this exe -- R5 `after` 0.3078 against bar 2 = 0.3223,
short by 0.0145 -- and the cause is the detail default flip alone, measured on
the rung before the build (detail 0 reads 0.3435 and passes).

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodgen.cpp` | `913e9f64d1cf5378` | 508,963 | 11680 |
| `src/lodgen.h` | `bd31aff30f21db1d` | 69,831 | 1228 |
| `src/nifcli.cpp` | `ce8c1c8005cf66d3` | 314,814 | 6952 |

| claim | line | anchor |
|---|---|---|
| 1a.5 road detail defaults to 1.0 | `lodgen.h:798` | `float roadDetail = 1.0f;` |
| 1a.5e the ground-paint multiplier, default 1.0 | `lodgen.h:839` | `float roadGroundPaint = 1.0f;` |
| 1a.5e the census field | `lodgen.h:932` | `int groundTexels = 0;` |
| 1a.5e the folder rule | `lodgen.cpp:6854` | `bool lodgenRoadMaterialIsGround( const QString & matName )` |
| 1a.5e the multiply, on COVERAGE and before the `cov <= 0` drop | `lodgen.cpp:7133` | `if ( sh.groundMat ) {` |
| 1a.5e the flag is set from the material NAME, before the file is opened | `lodgen.cpp:7272` | `out.groundMat = m.ground;` |
| 5 `--road-ground-paint` | `nifcli.cpp:6308` | `else if ( t == QLatin1String( "--road-ground-paint" ) ) {` |

### LAND1, 2026-09-12

This lane shipped two independent features and they were stamped against two
different exes, which is said here rather than smoothed over.

**Section 2.5g (`--land-guide`) and its three §5 rows** come from bakes into
`scratchpad/land1_20260912/out/` made by `release/NifSkope.exe` (07:42:22,
21,861,376 bytes, sha1 `902223bd99dba4bfaf5d621fe36eb12fc4cf0272`) -- 119 sweep
bakes plus the gate bakes, every one rc=0 and every one carrying
`--road-detail 1` -- graded by lane TILING4's instruments imported UNCHANGED
(`h1_sweep.py`, `t4_gates.py`, `t4_lib.py`, `f3_full.py`, `pool.json`) and by
this lane's own `a2_msn.py`, `a3_gate.py`, `a4_gate.py`, `a5_controls.py`,
`a6_score.py`, `a_pics.py` with the logs beside them under `logs/`. The macro
heights are read from `scratchpad/mountains_20260907/land_all.bin`, the
whole-worldspace VHGT dump written by `--dump-land`, which is the same data
lodgen itself reads.

**The INCR1 rows below and the `--incremental` §5 row** were measured against the
exe carrying BOTH parts (08:42:33, 21,935,616 bytes, sha1
`1e4e2c5cc5a0f34e058dbe67a9ac6fd9d52d8968`): gates B2, B3, B4 and B5 and the
picture are that exe's bakes, under `scratchpad/land1_20260912/out/b3/`.

**A THIRD exe** (09:21:04, 21,935,616 bytes, sha1
`0e5b65d69f4cb532b7f2f24b8a8caa381fe8f3fb`, object
`GeneratedFiles/.obj/nifcli.o` 09:20:58 -- the object timestamp is the check,
because `exe -nt src` is satisfied by a link the other translation unit
triggered; `lodgen.o` is 08:32:06 and unchanged, no `lodgen.cpp` edit having
happened since) differs from 08:42:33 in the switch digest ONLY, and exists
because the harness chain on 08:42:33 came back with two reds this lane had
introduced: `lodgen_roads.sh` R1 (two `--no-roads` runs are byte-identical)
and `lodgen_native.sh` check 5 (the stock bake is byte-identical with and
without `--native`). Both were the ledger's `switches` field digesting a
DESTINATION PATH. `--vt` now keeps its token and loses its value, `--native` and
`--native-mesh-report` leave the digest entirely, and `--native` gains the
whole-region refusal it always needed -- see `docs/LODGEN_LEDGER_FORMAT.md`
sections 3 and 4, which are the contract, not this file.

**THE SHIPPING EXE IS A FOURTH** (09:32:37, 21,951,488 bytes, sha1
`3e1914a0637b66f438d873e0230b1e8c04d7c806`), and NOT for anything in this lane:
six files from another lane's merge arrived in the tree between 09:21 and 09:31
with their **mtimes preserved** (08:26-08:46), so `tools/ww_build.sh`'s
exe-newer-than-sources gate passed over a tree whose objects were stale --
`animdopesheet.o` was 04:34:58 against an `animdopesheet.cpp` of 08:32:23, and
`make -n` wanted six translation units. **An exe newer than a source file is not
an exe built from it**, and a copy that preserves timestamps defeats every
mtime gate at once. The rebuild touched `animdopesheet`, `animworkspace`,
`animworkspacetest`, `hkxanimuitest`, `nifskope` and `nifskope_ui`; `nifcli.o`
is 09:20:58 and `lodgen.o` 08:32:06 in BOTH exes, so **no lodgen code differs
between 09:21:04 and 09:32:37**. `lodgen_roads`, `lodgen_native`,
`lodgen_native_baseline`, `lodgen_identity` and `animws` were re-run on the
shipping exe regardless.

The B2-B5 numbers were NOT re-measured against either later exe; none of their
argument vectors contains `--vt`, `--native` or `--incremental --native`, so
none of their digests moved, and that is a reasoned carry-forward stated rather
than hidden. The line numbers and hashes below ARE re-read against the shipping
sources.

Note that the 2.5g line numbers above were re-found, not carried over: Part B
moved two of them (`lodgen.cpp:6297` -> `6304`, `nifcli.cpp:6175` -> `6516`),
which is exactly the failure mode stamping the anchor TEXT beside the number
exists to catch.

No number here is copied forward: each was re-read off the artefact it
describes. Every line below was found again from its own anchor text against the
sources stamped here (`ww-contract-provenance` step 3).

What this lane could NOT meet, in its own words: **gate F3 is NOT MET on either
set** -- repeat 5/7 and 5/7 against a 7/7 bar, G1 outside +/-20 % on both sets
INCLUDING THE RUNG (-22.7 % and -37.0 %), and G2-band DOWN from 7/7 to 5/7 and
4/7. The G1 and swirl failures are the rung's own and no change to the sampler
can pass a gate its own floor fails; the G2-band loss is this lane's, is caused
by the rotation, and is reported as the price rather than left out. That is why
the switch ships OFF and the default is unchanged.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodgen.cpp` | `3b928277034b0406` | 535,632 | 12320 |
| `src/lodgen.h` | `b03d36199f8d50a7` | 75,006 | 1342 |
| `src/nifcli.cpp` | `7fa7b42e6a0dba83` | 338,407 | 7405 |

| claim | line | anchor |
|---|---|---|
| 2.5g the six rules, as an enum and nowhere else | `lodgen.h:255-260` | `LODGEN_LANDGUIDE_OFF       = 0,` |
| 2.5g the rule, strength, scale and slope reference, all static, all default to OFF | `lodgen.cpp:6118` | `static int   g_landGuideRule     = LODGEN_LANDGUIDE_OFF;` |
| 2.5g the guide context -- the ring height grid and its offsets, nothing tile-local | `lodgen.cpp:6131` | `struct LodgenLandGuideCtx` |
| 2.5g the macro gradient is a Sobel 3x3 at a half-step of scale/2 world units | `lodgen.cpp:6149` | `static void lodgenLandMacroGradient( const LodgenLandGuideCtx & ctx,` |
| 2.5g the slope weight, `smoothstep` in tan/slopeRef | `lodgen.cpp:6175` | `static inline double lodgenLandGuideWeight( double tangent )` |
| 2.5g the rotation weights the MAP, not the angle -- `((1-w)I + wR)`, a similarity, zero shear, continuous across `atan2`'s cut | `lodgen.cpp:6188` | `static void lodgenLandGuideRotate( const LodgenLandGuideCtx & ctx,` |
| 2.5g the rule switch, and that OFF returns before anything is read | `lodgen.cpp:6220, 6224` | `static void lodgenLandGuidedWarp( const LodgenLandGuideCtx & ctx,` / `if ( g_landGuideRule == LODGEN_LANDGUIDE_OFF ) {` |
| 2.5g `--land-guide-scale` is REFUSED outside 128..2048, and the comment says by the RING | `lodgen.cpp:6304` | `if ( units >= 128.0f && units <= 2048.0f )` |
| 2.5g the hex vertex's world position, so each tap rotates about ITSELF | `lodgen.cpp:6401` | `static inline void lodgenLandHexVertexPos( qint32 i, qint32 j, double size,` |
| 2.5g `aspecthex` rides the hex lattice's three taps | `lodgen.cpp:6445` | `if ( guide && g_landGuideRule == LODGEN_LANDGUIDE_ASPECTHEX ) {` |
| 2.5g the macro gradient is computed ONCE a texel, outside the per-layer lambda, at BOTH sampling sites | `lodgen.cpp:7992, 9312` | `lodgenLandMacroGradient( lguide, double( wx ), double( wy ),` |
| 2.5g the guided warp replaces the plain one at BOTH sampling sites -- the stock composite at 8027 and the PYRAMID at 9411, which is the one that writes a `--vt` bake's sheets | `lodgen.cpp:8027, 9411` | `lodgenLandGuidedWarp( lguide, wx, wy, mgx, mgy, &swx, &swy );` |
| 5 `--land-guide`, and that an unknown rule is refused BY NAME on stderr with `off` standing | `nifcli.cpp:6516, 6542` | `else if ( t == QLatin1String( "--land-guide" ) ) {` / `is not one of off\|drag\|aspect\|aspecthex\|slopewarp\|flatwarp; off stands` |
| 5 `--land-guide-scale` and `--land-guide-slope` | `nifcli.cpp:6551, 6552` | `else if ( t == QLatin1String( "--land-guide-scale" ) ) lodgenSetLandGuideScale( next().toFloat() );` |
| INCR1 the ledger structs, and the refusal enum beside them | `lodgen.h:1287, 1296, 1335` | `struct LodgenLedgerEntry` / `struct LodgenLedger` / `LODGEN_INCR_NO_LEDGER,` |
| INCR1 the per-chunk input digest, and that its ring loop is ONE cell wide on all four sides | `lodgen.cpp:12070` | `QString lodgenChunkInputDigest( const EsmWorld & world, int dim, int cx, int cy,` |
| INCR1 the LTEX texture-set row of the dependency map -- the blind spot a loose-file override would otherwise have walked through | `lodgen.cpp:12107, 12140, 12144` | `auto feedLtex = [&]( quint32 form ) {` |
| INCR1 an asset is digested by its BYTES, through the same reader the bake uses, which is why `--data-root` may sit on the skip list | `lodgen.cpp:12032` | `static QString lodgenLedgerAssetDigest( const QString & dataRoot, const QString & relPath,` |
| INCR1 the switch digest and its TWO skip lists -- token+value dropped, and token kept with the value dropped | `nifcli.cpp:2532, 2552, 2557` | `static const char * const gLgSwitchSkip[] = {` / `static const char * const gLgSwitchSkipValue[] = {` |
| INCR1 `--vt` keeps its token and loses its path, which is what made two identical `--no-roads` bakes write two different ledgers | `nifcli.cpp:2573` | `h.addData( a.at( i ).toUtf8() );   /* the flag, never its path */` |
| INCR1 the five refusals, each naming itself and exiting before a byte is written | `nifcli.cpp:3717, 3730, 3736, 3763, 3783` | `refused: --incremental has nothing to diff against -- ` |
| INCR1 the whole-region refusal names FOUR passes, not five: the merge and the far-ring simplify are per-file loops and are not refused | `nifcli.cpp:3763, 3783` | `refused: --atlas, --arrays and --impostors each build ONE region-wide ` / `refused: --native builds ONE .lodo/.lodi pair for the whole region ` |
| INCR1 why `--native` is refused: the pair is collected INSIDE the chunk pass, one placement a reference and one lighting sample a vertex | `lodgen.cpp:3784, 4069` | `if ( lodgenNativeActive() ) {` / `if ( lodgenNativeActive() && opts.identity )` |
| INCR1 the one-cell neighbour widening, applied on top of the digest's own ring | `nifcli.cpp:3846` | `if ( j.cx <= dx + d && dx <= j.cx + d && j.cy <= dy + d && dy <= j.cy + d ) {` |
| INCR1 the census line, printed every run | `nifcli.cpp:3859` | `"%6 by neighbour)" )` |
| INCR1 the ledger is written LAST, AFTER the merge has rewritten every `.BTO` -- the defect gate B3 caught | `nifcli.cpp:4115, 4177` | `THE LEDGER GOES HERE, LAST` |

Section 7a and the two new rows in section 5 were written on **2026-09-12** by
lane TERRAINFMT1. Every line number below was found again from its own ANCHOR
TEXT against the sources stamped here, never shifted by a delta
(`ww-contract-provenance` step 3); each anchor was found EXACTLY ONCE, and the
count was asserted rather than eyeballed. Every measured number in 7a was
re-derived from the artefact it describes -- the 6,120 shipped sheets, the
bakes under `scratchpad/terrainfmt1_20260912/bake/`, and the renders under
`scratchpad/terrainfmt1_20260912/images/` -- not copied forward from a brief.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodgen.cpp` | `3b15ed9e8ed9b805` | 591,582 | 13,612 |
| `src/lodgen.h` | `0e512da98c08b8a0` | 86,565 | 1,550 |
| `src/nifcli.cpp` | `11002abc4072c060` | 351,366 | 7,586 |

| claim | where | anchor |
|---|---|---|
| 7a.2 the two sheet formats are an enum, so `legacy` is a value and not an absence | `lodgen.h:376` | `enum LodgenSheetFormat` |
| 7a.2 the accessor pair the writer reads | `lodgen.h:382` | `int lodgenSheetFormat();` |
| 7a.3 the cache directory accessor | `lodgen.h:406` | `QString lodgenMsnCacheDir();` |
| 7a.2 the default is LEGACY, which is why no flag is the previous bytes | `lodgen.cpp:6581` | `static int g_sheetFormat = LODGEN_SHEETFMT_LEGACY;` |
| 7a.3 the uncompressed B8G8R8A8 writer, DX10 header, chain to 1x1 | `lodgen.cpp:6830` | `static bool lodgenWriteDdsBgra8(` |
| 7a.3 the cache reader, its channel law, the derived UP and the renormalisation | `lodgen.cpp:6906` | `static bool lodgenMsnFromCache(` |
| 7a.4 the terrain vertex colour that carries the identity channels | `lodgen.cpp:952` | `nif->set<ByteColor4>( row, "Vertex Colors", ByteColor4( FloatVector4(` |
| 7a.4 the CLI default for the terrain identity channels is OFF (bungo 2026-09-12; was ON) | `nifcli.cpp:7666` (was 6474) | `bool lgTerrainIdentity = false;` |
| 7a.4 the CLI default for the OBJECT identity channels is OFF (bungo 2026-09-12; was ON) | `nifcli.cpp:7549` (was 6412) | `bool lgIdentity = false;` |
| 7a.4 `--terrain-identity` is the opt-in that puts the four channels back | `nifcli.cpp:8335` (was 7013) | `else if ( t == QLatin1String( "--terrain-identity" ) ) lgTerrainIdentity = true;` |
| §5 `--sheet-format` parsing | `nifcli.cpp:6738` | `"--sheet-format"` |
| §5 `--msn-cache` parsing | `nifcli.cpp:6761` | `"--msn-cache"` |
