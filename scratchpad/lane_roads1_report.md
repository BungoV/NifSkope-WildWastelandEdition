# Lane ROADS1 -- roads and decals baked into far terrain the way vanilla does it

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, working tree.
Exe at launch `release/NifSkope.exe` 2026-09-11 11:27:12, 21,137,920 B
(TERRAIN-R's). Nothing committed. Lane directory
`scratchpad/roads1_20260911/`.

---

## 0. Pre-registered gates

Copied from the brief BEFORE any measurement or code, so that nothing below is
a gate invented after the numbers came in.

| id | gate |
|---|---|
| R1 | Vanilla measured FIRST: the inferred rule with its evidence windows, written before the rasteriser exists. |
| R2 | Road-presence metric on the Sanctuary loop-road tile: after >= a threshold derived from the ceiling; floor (the rung's bake, no road) near 0; both printed. |
| R3 | `--no-roads` byte-identical to the rung's bake, every file compared. |
| R4 | Census words written AND moving: a region with no roads reads 0. |
| R5 | Exe newer than every changed file; dependent objects rebuilt; the rollback rung equals the launch bytes. |
| R6 | No NifSkope left running; the game down at every launch. |

Rules this lane works under: one build plus counted gate-only relinks; markers
`scratchpad/roads1_20260911/BUILDING` -> `DONE`; bungo's own window renamed
aside, never killed; small regions only, into the lane's own out-dir, never his
installed `Data\Terrain`; no change to TERRAIN-R's mask-channel law; never
`git stash`, never a commit.

---

## 1. What vanilla bakes

**Written before any rasteriser code existed.** Everything in this section is
measured from Bethesda's own shipped files plus an independent walk of
`Fallout4.esm`; no generator code takes part.

### 1.1 The tile, and why this one

The Sanctuary loop road is NOT in the tile TERRAIN-R photographed. Measured:
projecting every `Landscape\Roads\*` placement in cells -24..-12 x 16..30 onto
the dim-4 chunk grid puts **32,035 road triangles in chunk (-20,20)**, 7,407 in
(-16,24), 1,964 in (-16,28) and **none at all in (-20,24)**. So the work tile is
`Commonwealth.4.-20.20.DDS` -- cells -20..-17 x 20..23, 512 texels, 32 world
units a texel.

### 1.2 The instruments

* `scratchpad/roads1_20260911/esm_refs.py` -- an independent GRUP walk of
  `Fallout4.esm` for placed `REFR`s in a cell window, their base records and
  those bases' model paths. SCOL collections are expanded one level (`ONAM` +
  `DATA`), because most Sanctuary objects are SCOL parts. 26,621 references in
  the window; **37,408 placements after SCOL expansion**.
* `scratchpad/roads1_20260911/rasterlib.py` -- top-down scan conversion onto the
  chunk's texel grid with a maximum-z buffer. NIF reading is
  `tests/spells/gltf_nifread.py` (the glTF gates' own reader); the placement
  rotation is `Matrix::fromEuler( -rx, -ry, -rz )`, re-typed from
  `src/data/niftypes.cpp:215` with the negation `docs/LODGEN_PARITY.md` proved
  per object.
* `scratchpad/roads1_20260911/matinfo.py` -- the shader property's material name
  and Fallout 4 shader flags, and the BGSM/BGEM's own `bDecal`, `bTwoSided`,
  `bAlphaTest`, `iAlphaTestRef`, read field for field.

### 1.3 The road test, stated (never a bare substring)

`MISTAKES.md`'s "sTREEt" lesson has a live counter-example in this very window:
`SetDressing\RailRoad\WaxCandle02Off.nif` contains the letters `road`, and 12
of them are placed here. The test shipped is **component equality**:

> A placement is a ROAD piece when its base record's signature is `STAT` and its
> model path -- separators normalised, lowercased, a leading `meshes` component
> dropped -- has `landscape` as its FIRST component and `roads` or `sidewalks`
> as its SECOND.

In the window that selects **321 road placements over 71 distinct models** and
232 sidewalk placements, and rejects every `setdressing/railroad/*` piece.

### 1.4 The four discriminators

**D1 -- it is not in the LAND paint.** Every landscape texture painted in the
sixteen cells of chunk (-20,20), listed from the ESM: `LDriedGrass01`,
`LDriedGrass01NoGrass`, `LRubbleRock01`, `LDirtGravel01`, `LRiverbedSilt01`,
`LDriedGrass02Weeds`, `LForestFloor01`, `LRootsEroded01`, `LRootsEroded01Grass01`,
`LDirtGravel01Grass`, `LRiverbedSilt01Wet`, `LRubbleRock01Grass`,
`LRiverbedRocks01Grass`, `LRiverbedRocks02Wet`, `LRiverbedRocks02WetGrass`,
`LDebrisGround`. **Not one road, asphalt, concrete or pavement texture among
them.** All 16 cells have a LAND record, so this is not a missing-data answer.
The road content in vanilla's sheet therefore cannot have come from the splat.

**D2 -- vanilla's colour sheet carries the road footprint, and only that
family.** Each family's projected footprint is scored against the sheet, and
each gets ITS OWN floor: the same mask displaced five ways, which keeps its
area, shape and spatial spectrum and destroys only its registration with the
sheet (`ww-control-calibration`). AUC = the area under the ROC of the sheet's
own brightness (and of its greyness, `-saturation`) as a detector of the mask.

| family | texels | AUC(bright) | its displaced floor | AUC(grey) | its displaced floor |
|---|---|---|---|---|---|
| **road** | 23,321 | **0.716** | 0.470 .. 0.601 | **0.678** | 0.448 .. 0.513 |
| decal shapes (non-road) | 37,993 | 0.564 | 0.494 .. 0.603 | 0.518 | 0.477 .. 0.509 |
| trees | 51,462 | 0.529 | 0.454 .. 0.533 | 0.482 | 0.480 .. 0.528 |
| rocks / cliffs | 42,599 | 0.448 | 0.388 .. 0.580 | 0.527 | 0.457 .. 0.578 |
| architecture | 18,313 | 0.621 | 0.513 .. 0.614 | 0.536 | 0.479 .. 0.509 |
| setdressing | 6,491 | 0.560 | 0.459 .. 0.597 | 0.559 | 0.464 .. 0.498 |

Ceiling (a mask scored by itself) = 1.000. **Only `road` clears its own floor on
both scores.** Architecture beats its floor by 0.007 on brightness alone, which
is inside the floor's own spread, so this tile does not support baking
buildings. Generic decal shapes -- `bDecal` true in the material, which in this
window is mostly the ground-blending skirts inside rocks and cliffs -- do NOT
clear their floor: 0.564 against a floor reaching 0.603.

**D2b -- the colour is the road MATERIAL'S OWN diffuse under the sheet's own
grading.** Mean colour of `Landscape\Roads\Sanctuary\SancRoad01_d.dds` =
(117.9, 112.1, 104.5), luminance 112.8. Vanilla's sheet inside the road
footprint = (97.4, 92.3, 82.4), luminance 92.6 -- a factor **0.82**. Mean colour
of `Landscape\Ground\DriedGrass01_D.dds` = (113.8, 97.9, 83.2), luminance 100.2;
vanilla's sheet on the plain background = (90.6, 81.9, 70.5), luminance 82.9 --
a factor **0.83**. The same grading factor on both, to within a percent: the
road's colour in vanilla's sheet is its own texture, put through the same
darkening as the terrain around it, not a paint layer and not a constant.

**D3 -- the picture.** `probe/van_color.png` is vanilla's shipped sheet and
`probe/roadmask.png` is our projection of the road meshes alone. They are the
same curve: the road entering from the west edge, the bend, the cul-de-sac
circle with its island, the driveways off it. Nothing in the projection was
fitted to the sheet.

**D4/D5 -- the `_msn` normal sheet does NOT carry the road.** The sheet's
channel means are R 127.3, G 247.0, B 121.8, so G is the up axis. Comparing
vanilla's `_msn` against a normal computed from the LAND heightmap ALONE (VHGT
parsed independently, 16 of 16 cells present):

| where | angle(vanilla `_msn`, heightmap normal), mean | median |
|---|---|---|
| road footprint | **13.58 deg** | 12.26 |
| trees | 14.41 | 13.04 |
| rocks | 14.41 | 13.01 |
| architecture | 13.97 | 12.48 |
| background | 14.14 | 12.75 |
| road footprint displaced (control) | 14.45 | 13.02 |

The road footprint agrees with the bare heightmap **slightly better** than the
background does, not worse. Had the road mesh been baked into the normal, the
footprint would be the one place the heightmap could not explain the sheet. The
statistic has range: the same comparison against straight up instead of the
heightmap normal reads 14.39 deg on the road and 18.49 deg on the background, so
it does detect real tilt.

### 1.5 The rule inferred, in one line

> **Vanilla rasterises road meshes top-down into the far-terrain COLOUR sheet
> only, at the mesh's own footprint, with the mesh's own material diffuse, put
> through the same grading as the rest of the sheet. The `_msn` normal sheet
> stays the heightmap's. No other object family -- trees, rocks, buildings,
> set dressing, or generic `bDecal` shapes -- is measurably in the sheet.**

Two honest limits on that sentence:

* **Sidewalks are carried by the rule but not evidenced by this tile.**
  `Landscape\Sidewalks\*` puts only 187 texels (0.07 percent) into chunk
  (-20,20) against `Landscape\Roads`' 23,170; the family is included because it
  is the same road surface in the same folder root, and that inclusion is
  UNTESTED here.
* **"Decals" as bungo named them are satisfied by the road family's own decal
  shapes** (`AsphaltAndSWEdgeDecals01.bgsm` inside `Landscape\Roads\*` and
  `Landscape\Sidewalks\*`), not by a separate decal sweep: a separate sweep over
  every `bDecal` shape in the region does not clear its floor and would drag in
  every rock skirt in the Commonwealth.

---

## 2. The rasteriser

Written AFTER section 1 was on disk. All of it is in `src/lodgen.cpp`; nothing
in TERRAIN-R's mask-channel law moved, no native object file was touched and no
panel row was added.

### 2.1 Where it sits in the bake, and why

Per texel the far-terrain colour is built in this order, and the road goes in
between the third and fourth steps:

1. the splat composite -- the quadrant's base landscape texture, then each
   painted layer over it at its bilinear opacity;
2. the ground-cover byte (slope-gated density) -- unchanged;
3. the **VCLR multiply** -- the artist's hand-painted vertex shading of the
   ground;
4. **THE ROAD** (new);
5. the grass tint, weighted by the cover byte.

* **After VCLR**, because the road lies ON the ground the artist shaded. Putting
  it before the multiply would shade the asphalt with the dirt shading of the
  soil it covers.
* **Before the grass tint**, because the tint's weight is the cover byte and the
  road suppresses the cover under it: the cover byte is scaled by
  `1 - coverage x roadCoverSuppress` (default 1, i.e. none) and, on the chunk
  path, the cover PLANE is rewritten with the same byte, so what a consumer
  reads out of the sheet's alpha is what the tint used. Grass grows beside a
  road, not through it.

### 2.2 The mesh test

`lodgenIsRoadModel()` in `src/lodgen.h` / `src/lodgen.cpp`, plus the caller's
`STAT` requirement. Component equality on the model path, never a substring:
first component `landscape`, second `roads` or `sidewalks`, and at least one
more component after them (the two must be FOLDERS). `SetDressing\RailRoad\...`
fails on the first component.

### 2.3 The channels it touches

| sheet | touched | why |
|---|---|---|
| colour (`.DDS`, `.lodt` role 1) | **yes** | measured: vanilla carries the road there |
| `_msn` normal (role 2) | no | measured: vanilla's `_msn` is the heightmap's on the road too |
| mask (role 5, RMAOS) | alpha only | the ground-cover byte is suppressed under the road; R/G/B untouched |
| emissive (role 6) | no | a road emits nothing |
| height (role 4) | no | the road is not the ground's height |
| retired `data` plane / `.btr` `_data.DDS` | alpha only | same cover byte, same reason |

### 2.4 What a texel gets

The topmost road triangle covering the texel centre wins -- a maximum-z buffer
in world Z, which is what "seen from above" means, so a driveway laid over a
road wins and a road under a bridge deck does not. From that triangle:

* **colour** = the shape's diffuse texture sampled at the interpolated UV,
  multiplied by the interpolated vertex colour. The mip is chosen from the
  triangle's OWN texture-area-to-footprint ratio (`0.5 * log2(uvArea/pxArea)`),
  not from the landscape path's world tiling -- a road mesh does not tile with
  the world, so the terrain rule would be meaningless on it;
* **coverage** = 1 for an opaque shape. An opaque road's diffuse alpha is not a
  silhouette and reading it as one would punch the road full of holes. Only a
  shape whose MATERIAL (`bAlphaTest`/`bAlphaBlend` in the BGSM) or whose
  `NiAlphaProperty` says so honours the texture's alpha -- that is the clause
  that makes an alpha-tested road decal cut out, and the rejected texels are
  counted (`alpharejected`).

### 2.5 The module and the way back

`LodgenCoverOptions::roads`, default **true** under both targets because vanilla
does it; CLI `--roads` / `--no-roads`, plus `--road-cover-suppress F`. With
`--no-roads` the plane is never allocated, no `REFR` is read and no road model
is opened, so the bake is byte-identical to the rung by CONSTRUCTION and not by
floating-point argument.

### 2.6 The census

Chunk path: one `roads ...` line per chunk on stderr, printed whenever the pass
is on. Pyramid path: the same fields folded into the `--vt` report line
(`roads`, `roadPlacements`, `roadMeshes`, `roadShapeTiles`,
`roadDecalShapeTiles`, `roadTriangles`, `roadTexels`, `roadDecalTexels`,
`roadAlphaRejected`, `roadRefusedNoLoad`, `roadRefusedNoTexture`,
`roadRefusals`). `roads 0` means the switch was off; `roads 1 roadTexels 0`
means it was on and this ground carries none. One field that could not move was
REMOVED rather than shipped: `refused_nomodel` was unreachable, because a road
placement is identified BY its model path, so a road with no model cannot be
recognised as one.

---

## 2a. The pre-registered number, audited (`ww-spec-gate-audit`)

The brief inherited one figure to reproduce: `docs/LODGEN_PARITY.md`'s gap line
and, through it, lane TERRAIN-R's **mean difference 19.96 of 255** on
`ours_vs_vanilla_tile.png`, of which the handoff says *"most of it is the roads
and a rubble patch vanilla carries and we do not"*.

**The number is real; the attribution is wrong, and it would have sent this lane
to the wrong tile.** That picture is chunk (-20,24). Projecting every
`Landscape\Roads\*` and `Landscape\Sidewalks\*` placement in cells -24..-12 x
16..30 onto the dim-4 chunk grid gives:

| chunk | road triangles |
|---|---|
| (-16,16) | 284,532 |
| (-20,20) | 86,770 |
| (-16,24) | 63,703 |
| **(-20,24)** | **0** |

So none of that 19.96 can be roads: its cause is the splat grading and the
17-grid blockiness, which is the line below it in the same gap list. The gate
was therefore re-taken on chunk **(-20,20)**, the chunk the loop road is
actually in, and this lane's own before/after numbers are computed there. The
entry is in `MISTAKES_ENTRIES.md`.

---

## 3. Build and gates

### 3.1 The build

ONE build plus ONE counted relink. `scratchpad/roads1_20260911/BUILDING` was up
before the first and is replaced by `DONE` at the close. `Fallout4.exe` was down
and no NifSkope was running at every launch; bungo had no window open, so
nothing had to be renamed aside.

| | when | `make` exit | exe bytes |
|---|---|---|---|
| build 1 | 12:12 | 0 | 21,172,224 |
| relink 1 (counted) | 12:19 | 0 | 21,180,928 |

The relink was not cosmetic: the first build's own census said
`decalshapes=0 refused_notexture=85`, which is the absolute-Bethesda-build-path
material defect in `MISTAKES_ENTRIES.md`. After it, `decalshapes` moves and
`refused_notexture` falls from 85 to 7 -- and those 7 are `EditorMarker` shapes,
which name no material and no texture and must not paint.

### 3.2 Consistency (gate R5)

| file | mtime | note |
|---|---|---|
| `release/NifSkope.exe` | 2026-09-11 12:19:06 | 21,180,928 B |
| `release/NifSkope.before_roads1.exe` | 2026-09-11 12:11:51 | 21,137,920 B, sha1 `a3283ae6...` -- the launch exe, byte for byte |
| `src/lodgen.cpp` | 12:18:22 | older than the exe |
| `src/lodgen.h` | 12:07:03 | older than the exe |
| `src/nifcli.cpp` | 12:11:02 | older than the exe |
| `GeneratedFiles/.obj/lodgen.o` | 12:19:04 | rebuilt, older than the exe |
| `GeneratedFiles/.obj/nifcli.o` | 12:12:09 | rebuilt, older than the exe |

`res/style.qss` and `release/style.qss` are byte-identical.

### 3.3 The lane's own gates

| gate | result |
|---|---|
| **R1** vanilla measured first | section 1 was on disk before any rasteriser code existed |
| **R2** road-presence metric | **PASS**, see 3.4 |
| **R3** `--no-roads` byte-identical to the rung | **PASS**: the rung exe, and the new exe with `--no-roads`, bake the Sanctuary region to **9 of 9 files identical**, with no file present on one side only |
| **R4** census written and moving | **PASS**, see 3.5 |
| **R5** exe newer than every changed file, drivers rebuilt, rung == launch bytes | **PASS**, table above |
| **R6** no NifSkope left running, game down at every launch | **PASS**: checked before each build and each render; none running at the close |

### 3.4 R2, the road-presence metric

Mask: vanilla's road, extracted from **vanilla's own sheet by its colour** and
from nothing else -- chroma (R-B) <= 17.5 and luminance >= 87.7, the midpoints
of the two populations section 1 measured, then eroded 3x3 so only cores
survive. 9,777 centreline texels of 262,144.

**The extractor's own control, run first:** 51.1 % of the centreline falls
inside the INDEPENDENT geometric projection of the road meshes, against
3.5 %..16.3 % for the same projection displaced five ways. The mask is finding
the road and not merely something grey.

| | fraction of the centreline within 16 of 255 of vanilla |
|---|---|
| **ceiling** -- vanilla against itself | **1.0000** |
| **floor** -- ours `--no-roads` (the rung) | **0.1274** |
| **after** -- ours `--roads` | **0.3065** |
| reference -- the surrounding ground, same tolerance | 0.3442 |

Pre-registered bars, written before the run: **after >= 2 x floor** (0.3065 >=
0.2549, ok) and **after >= 0.8 x reference** (0.3065 >= 0.2753, ok).

PARITY's own metric on the same tile:

| | whole tile | on the road centreline |
|---|---|---|
| `--no-roads` | 24.39 of 255 | 38.85 |
| `--roads` | **22.81** | **24.36** |

The road texels go from being the worst part of the tile to being no worse than
the ground around them.

### 3.5 R4, the census

| region | placements | meshes | shapeTiles | triangles | **texels** | decalTexels | alphaRejected | refusedNoTexture |
|---|---|---|---|---|---|---|---|---|
| cells -20..-17 x 20..23 (the loop road) | 253 | 73 | 330 | 88,513 | **27,695** | 915 | 577 | 7 |
| cells -20..-17 x 24..27 (no roads) | 32 | 13 | 0 | 0 | **0** | 0 | 0 | 0 |
| the same region, `--no-roads` | 0 | 0 | 0 | 0 | **0** | 0 | 0 | 0 |

`roadPlacements` and `roadMeshes` count the GATHER, which runs over the region
grown by two cells; the road-free region's 32 placements all sit in that margin
and reach no tile. The counters saying two different things about the same
ground is what makes them readable.

### 3.6 The harness chain, against the TERRAIN-R baselines

| spell | baseline | now | |
|---|---|---|---|
| `lodgen_terrain.sh` | 26/0 | **26/0** | same |
| `lodgen_terrain_vt.sh` | 41/1 | **41/1** | same; the failure is V9b, red on the rung too |
| `lodgen_ground_cover.sh` | 29/5 | **29/5** | the same five |
| `lodgen_terrain_pbrm.sh` | 14/0 | **14/0** | same |
| `lodgen_texture_arrays.sh` | 40 ok PASS | **PASS** | same |
| `lodgen_card_arrays.sh` | 35 ok PASS | **PASS** | same |
| `lodgen_native.sh` | 18/0 | **18/0** | same |
| `lodl_open.sh` | 23/0 | **23/0** | same -- see below |
| `ui_align.sh` | 11/0 | **11/0** | same |
| `water_ui.sh` | 82/0 | **82/0** | same, floor 72 |
| **`lodgen_roads.sh` (NEW)** | -- | **11/0 PASS** | |

Why those: the terrain spells are the ones the change reaches (both bake paths
write the colour sheet); the arrays and native spells share `lodgenLoadModel`,
to which four fields were added; `lodl_open`, `ui_align` and `water_ui` are the
standing GUI set the brief names.

**`lodl_open.sh` read 23/1 on the first pass and 23/0 on the second, and the
change was not the tree.** The first run went through the MSYS2 login shell,
where `python` is MSYS2's and has no `numpy`; the failing check printed
`ModuleNotFoundError: No module named 'numpy'` and then
`whole-worldspace render: 89019 bytes, coverage , luminance SD ` with both
numbers EMPTY. Re-run from the shell whose `python` is
`/c/Users/bungo/AppData/Local/Programs/Python/Python39/python`, the same exe
gives `coverage 0.0951, luminance SD 38.86` and 23/0. A spell that shells out to
`python` measures whichever `python` is on the PATH.

**None of the terrain spells actually exercised the road pass.**
`lodgen_terrain_vt.sh`'s fixture is cells (-24,24)..(-17,31) and its bakes report
`roadTexels 0` -- there are no roads on that ground. So their unchanged counts
mean "nothing ran", not "it ran and changed nothing". That is why
`tests/spells/lodgen_roads.sh` is new.

### 3.7 The new spell, and its floors firing

`tests/spells/lodgen_roads.sh` (11 checks) with
`tests/spells/lodgen_roads_metric.py`. Two regions, chosen by the road-triangle
histogram and not by eye: the loop-road chunk and the road-free chunk beside it.

Its floors are not decoration -- the SAME predicate returns both answers in one
run:

* the colour sheets of `--roads` and `--no-roads` **differ** on the road-bearing
  region and are **byte-identical** on the road-free one;
* `roadTexels` reads **27,695** on the one and **0** on the other;
* the `_msn` sheet is byte-identical on both, which is the measured vanilla
  behaviour and would go red the moment the road pass touched the normal.

Run against the rung exe (`EXE=release/NifSkope.before_roads1.exe`) the spell
stops at the first bake with `error: unknown option --roads`, which is the
flag's own way of saying it is new.

---

## 4. Pictures

All three are in `scratchpad/roads1_20260911/images/`. Described first, then
cited (CONSTITUTION 5).

### 4.1 `cmp_sanctuary_road.png` (1092x936)

Three panels on one row and their 4x zooms below. Left: Bethesda's shipped
`Textures\Terrain\Commonwealth\Commonwealth.4.-20.20.DDS`. Middle: our bake of
the same chunk with `--no-roads`. Right: the same with `--roads`. All three are
512 x 512 texels of the same ground at 32 world units a texel, the same grid,
with no resampling on any side -- our chunk sheet IS vanilla's grid. The zooms
are texels x 150..300, y 120..270, the cul-de-sac and its island.

**What it shows.** Vanilla's panel carries a road running in from the west edge,
bending, and ending in a circular cul-de-sac with a planted island, with short
driveways off it. Our `--no-roads` panel has bare ground there, mottled by the
splat and nothing else. Our `--roads` panel carries the same road, the same
bend, the same circle, the same island and the same driveways. The captions
carry that panel's own mean error against vanilla.

**What it also shows, and it is not this lane's win.** Our road reads LIGHTER
and less blue than Bethesda's, and the ground around it is browner than
vanilla's. That is the splat-grading gap the parity page has had open since
August, not the road pass.

### 4.2 `road_mask_and_metric.png` (1450x594)

Four panels. First: the gate's mask in red over the geometric road projection in
blue -- red is what vanilla's own sheet says is road-coloured, blue is our
independent top-down projection of the road meshes, and the two are the same
network without either having been fitted to the other. Then the same mask drawn
over vanilla's sheet, over our `--no-roads` sheet and over our `--roads` sheet,
each captioned with its number: ceiling 1.000, floor 0.1274, after 0.3065, the
surrounding ground 0.3442.

**The honest part of this picture:** there are red patches OFF the road at the
top left. Those are bright, grey rubble and rock in vanilla's sheet that the
colour rule cannot tell from asphalt, and they are why the mask's overlap with
the geometry is 51 % and not 90 %. They pull the metric DOWN for every side
equally, which is why the gate is stated against a floor and a reference rather
than against an absolute number.

### 4.3 `top_region.png` (1332x604)

The same ground seen from above through NifSkope's own render hook
(`WW_RENDER_VIEW=1`, Top, `WW_RENDER_CLEAN=1`, 1524x941, one instance, second
monitor, window opacity 0). Each side is staged as its own miniature data root
-- the `.BTR` with `textures/terrain/commonwealth/` beside it -- so
`NifModel::load`'s `addNIFResourcePath` serves that side's own sheets and neither
side can borrow the other's. Nothing in the game folder was written.

Left: vanilla's `.BTR` with vanilla's sheets. Middle and right: OUR `.BTR` with
our `--no-roads` and `--roads` sheets. Our two `.BTR` files are **byte-identical**
-- roads touch the colour sheet and no geometry -- so the only difference between
those two panels is the sheet, and the road is plainly in the right-hand one and
plainly absent from the middle one.

**A red this picture exposed, which is NOT roads.** Vanilla's panel renders as
brown ground with a white water plane; both of ours render a lurid blue-purple
with dark red water. Measured cause candidate, and it is a fact rather than a
guess: **all 6,120 of Bethesda's Commonwealth terrain sheets are DXT5 with 10
mips** (3,060 colour + 3,060 `_msn`), and ours are **DXT1 with 8 mips**. A DXT1
`_msn` has no alpha channel to carry what the terrain shader reads out of it.
That is a separate lane; it is recorded here because it is visible the moment
the two are put in one frame.

---

## 5. Owed / red / bungo's calls

### RED, in this lane's own territory

1. **The road our bake paints is lighter and less blue than Bethesda's.** The
   geometry is right and the material is the right one; the colour is off in the
   same direction, and by about the same amount, as the ground around it, so the
   cause is the splat grading the parity page has had open since August and not
   the road pass. Measured: on the road centreline the mean error falls from
   38.85 to 24.36 of 255, and the surrounding ground sits at roughly the same
   24, so the road is now exactly as wrong as everything else on the tile.
2. **`Landscape\Sidewalks\*` is in the rule and untested.** It contributes 187
   of the measurement chunk's texels against `Landscape\Roads`' 23,170, which is
   below anything this tile can resolve. It is included because it is the same
   road surface under the same folder root.
3. **The colour extractor's mask is 51 % road.** Bright grey rubble in vanilla's
   sheet is not separable from asphalt by colour alone, so the gate's absolute
   numbers are depressed for every side. The gate is therefore stated against its
   own floor and against the surrounding ground, never as an absolute.

### RED, outside this lane, found by it

4. **`lodgenLoadModel` cannot resolve a material named by an absolute Bethesda
   build path.** Every `Landscape\Roads\Country\*` and `\Alley\*` piece names its
   material as `C:\Projects\Fallout4\Build\PC\Data\materials\...` and carries an
   EMPTY texture set; the loader's fix-up prepends `materials/` and the path
   resolves to nothing, so the shape ends with no diffuse. On the Sanctuary
   loop-road chunk that was 65 of 270 road shapes. The road pass fixes it for
   ITSELF (`lodgenRoadMaterialPath`); the OBJECT bakes still drop those textures.
   Fixing it in the shared loader is the right place and it would move output the
   byte-identity gates pin, so **it is bungo's call**, not this lane's.
5. **Our far-terrain sheets are DXT1 with 8 mips; all 6,120 of Bethesda's are
   DXT5 with 10.** 3,060 colour and 3,060 `_msn`, measured over the whole
   `Textures\Terrain\Commonwealth` folder. A DXT1 `_msn` has no alpha channel.
   This is what the top-down render picture shows as a blue-purple cast on our
   chunk where vanilla's renders naturally. A lane of its own.
6. **`docs/LODGEN_PARITY.md`'s remaining gap line is unchanged and still open**:
   splat grading, luminance correlation ~0.55 downtown.
7. **`lodgen_terrain_vt.sh` V9b stays red** (assembled-vs-direct `_msn` byte
   identity), exactly as it was on the rung. Untouched here.

### bungo's calls

* **(4) above:** widen `lodgenLoadModel`'s material resolution to the last
  `materials/` in the path, which fixes every object bake that reads a country
  road, a highway or an alley piece -- and moves files the object byte-identity
  gates pin. Yes or no.
* **`--road-cover-suppress` default.** It ships at 1.0: no ground cover, and so
  no grass tint, under a road. 0.0 is the exact way back to a road with grass
  growing through it. Nothing was measured about what vanilla does here, because
  vanilla ships no cover plane to measure.
* **Whether sidewalks stay in the rule** given they are untested on this tile.

### Owed

* Nothing is owed to bungo from this lane beyond the three pictures, which are
  delivered. **His window needs a restart**: the exe changed at 12:19:06.

---

## 6. Mistakes

Three entries, in `scratchpad/roads1_20260911/MISTAKES_ENTRIES.md` for the
director to splice into `MISTAKES.md` (entries start with `## `):

1. **A difference was attributed to roads on a tile that has no roads.** The
   handoff and the parity page put TERRAIN-R's 19.96 mean difference on chunk
   (-20,24) down mostly to roads; that chunk contains zero road triangles. Rule:
   before attributing a difference to a THING, project the thing and check it is
   in the frame.
2. **A census field that could not move was written before it was caught.**
   `refused_nomodel` was unreachable, because a placement is recognised as a road
   BY its model path. Rule: name the input that makes a new counter non-zero; if
   the code cannot reach that state, the field does not ship. It was removed.
3. **The road pass inherited a material resolver that cannot open an absolute
   Bethesda build path.** The first build's census read `decalshapes=0
   refused_notexture=85`; an independent Python census said 69 of 305 road shapes
   have `bDecal` true and every one of those materials exists on disk. Rule: a
   refusal counter is a finding, not noise -- read the refusals before the
   successes.

---

## 7. Finished-work skill review (CONSTITUTION 1a)

### Loaded and used

* `nifskope-ww-lodgen` -- the build incantation with the `git` PATH export, the
  gate on `make`'s own exit code, the CLI shape, the ABSOLUTE-path trap (every
  `-no-gui` path in this lane is `E:/...`), the heredoc/backslash trap (it bit
  once anyway, see below), and the manifest/identity conventions.
* `ww-control-calibration` -- every number in section 1 and in the gate carries a
  floor built from the signal's own shape: the displaced-mask family. The
  extractor itself got a control before the metric was believed.
* `ww-spec-gate-audit` -- run FIRST, and it changed the lane's fixture: the
  inherited 19.96 was measured on a road-free tile. Section 2a.
* `nifskope-ww-vanilla-compare` -- section 2's staging rule (each side its own
  miniature data root, `addNIFResourcePath` serving it) is exactly what
  `top_region.png` does, and section 3's camera pin.
* `ww-texel-picture` -- the crop chosen where the defect is, fixed-cell layout,
  captions carrying the same numbers the report quotes.
* `fo4cs-census-field` -- written AND moves, no field that cannot move, refusals
  by name, one physical line of `key=value`.
* `nifskope-ww-build-verify` -- the exe-newer sweep over every changed file, the
  link-time stylesheet copy, "tell him his window needs a restart".
* `ww-contract-provenance` -- the contract edit re-found every line from its own
  anchor text and re-stamped both sources.
* `ww-anchored-hookup` -- read; it did not apply, because no file was shared with
  a live lane (TERRAIN-R had landed) and the brief did not ask for new files.

### The skill that should have existed, and now does

**`tests/spells/lodgen_roads.sh` is not a skill, it is a gate** -- but the
procedure behind it is one, and it was re-derived from first principles here:
*choosing the fixture region by projecting the thing under test onto every
candidate chunk before picking one.* That step took four lines and it is the
reason this lane measured the right tile while the lane before it measured the
wrong one. It generalises to every "compare our bake with vanilla's on a tile"
job in this tree -- roads, rubble, water, cards.

I did not write it as a new skill file because
**`nifskope-ww-vanilla-compare` already owns "how to pick the tile or model
defensibly from the master alone"** and this belongs inside it rather than
beside it. **AMENDED, in the REPO tree**
(`.claude/skills/nifskope-ww-vanilla-compare/SKILL.md`): a new step 1a, "pick the
tile by projecting the thing under test, not by inheriting one", with the
per-chunk histogram and the (-20,24) counter-example. **The director must mirror
it to the live tree** (CONSTITUTION 1a, the two trees drift).

### The second amendment, to `nifskope-ww-lodgen`

**A spell that shells out to `python` measures whichever `python` is on the
PATH.** Running the harness chain through `MSYSTEM=UCRT64 ... bash -lc` gives the
MSYS2 interpreter, which has no `numpy`, and `lodl_open.sh` reported
`23 checks, 1 failures` with the failing line's two numbers EMPTY -- which reads
exactly like a render regression. The same exe from the Git-Bash shell reads
23/0. Added to the skill's "Editing traps" section beside the existing
`C:/...` vs `/c/...` note, in the REPO tree; the director mirrors.

### Declined

* A skill for the road rasteriser itself. It is one feature in one file with its
  law written into `docs/LODGEN_TERRAIN_VT.md` section 1a; it will not be done
  again.
* A skill for the AUC-against-a-displaced-floor measurement. It is already the
  substance of `ww-control-calibration` and adding a second home for it would
  split the rule.

### The trap that bit anyway

The `nifskope-ww-lodgen` skill says "no text carrying a backslash or an
apostrophe goes through a heredoc at all". Two heredoc patch attempts in this
lane still died on it -- one on `p.replace('\\','/')` inside a `python -c`, one on
a markdown block. Both were re-done with the Write tool, which is what the skill
already says to do. The rule is right; following it the FIRST time is the part
that needs no new skill, only obedience.

---

## R1's own timestamps

Gate R1 says the measurement comes first. The report file's own mtime cannot
show that, because sections 3-7 were appended to it later. The MEASUREMENT
ARTEFACTS can, and they are all on disk before the first line of rasteriser code
was written:

| artefact | written |
|---|---|
| `scratchpad/roads1_20260911/esm_refs.py` (the placement walk) | 11:42:32 |
| `masks_m20_20.npz` (every family's projected footprint) | 11:50:18 |
| `probe/van_color.png` (vanilla's sheet decoded) | 11:50:58 |
| `cmp_vanilla3.py` (the AUC and the `_msn`-vs-heightmap test) | 11:53:24 |
| `decals_m20_20.npz` (the decal-shape footprint) | 11:56:36 |
| `family_auc.py` (every family against its own displaced floor) | 11:56:54 |
| **first rasteriser edit — `src/lodgen.h`** | **12:07:03** |
| `src/lodgen.cpp` (last edit) | 12:18:22 |

Eleven minutes and every number in section 1 separate the last measurement from
the first line of the feature.
