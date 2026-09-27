# AUDIT1 -- every baked LOD map: what it must ultimately be, and what it is now (started 2026-09-27 04:10, `date`-read)

Read-only. Subject = the MAPS1 bake (lane AO2 decal round, branch ao2-20260926, E:\Projects\NifskopeWWE-bake2),
79 pictures in scratchpad\maps1\full. Water "different data" = water5 regenerated.lodl (v3, 346 bodies).

## 1. Skills loaded
nifskope-ww-lodgen, search-lean, ww-channel-view-refuter, ww-legend-matches-picture, ww-texel-picture.

## 0. Who reads any of this today (one fact that colours every row)
- **In the game: nobody draws it yet.** FO4CS's reader (READER1, wave 95) lives in
  `E:\Projects\Fo4CommunityShaders\wt-telem1` (not wt-fixfirst as the brief said). It parses `.lodo` v4-v6 and
  `.lodi` v3-v10 and logs "parsed, not drawn" (`src/ImprovedLOD/ImprovedLODRuntime.cpp:186,191`; Codex/HANDOFF.md:13-20).
  `ImprovedLODLoad.h:10` reads only the `.lodl` header. No shader under `res/Effects` names ImprovedLOD.
- **The stock path that IS live** reads the old `.BTO` channels per VERTEX: identity in vertex colour R+G, AO in B,
  sway in A, sky in UV2.x, ground contact in Eye Data (`src/FarField/FarFieldLodBtoChannels.h`, cited in
  docs/FO4CS_IMPROVED_LOD_PLAN.md:225-228).
- **The intended readers** are written in the plan: R1 reads the per-vertex AO (plan:1716) and the sky, and uses the
  group as the far-shadow caster identity (plan:1718, 1592). R2 samples terrain colour, normal and mask
  from clipmap rings (plan:590) and works out shore distance at runtime from the `.lodl` water planes (plan:616).
- So "who reads it" in the table below = the NifSkope viewer that drew the picture, plus the planned FO4CS rung.
  Nothing is wrong *in game* today, because nothing native is drawn in game yet. The wrongs below are wrongs in
  the data that the rungs will inherit.

## 2. His named points, answered

### 2.1 Ground contact on buildings (03): "a texture map that is not usable?"
**Yes, it is not usable as it stands. The number is correct, but it is stored at the wrong grain.**
- What it must be: a darkening that is strong where a wall meets the ground and fades out over the first 256 units
  going up. The doc defines it PER VERTEX: clamp(1 - (z - terrainZ)/256) (src/lodgen.cpp:4677-4681;
  LODGEN_VERTEX_PACKING.md:44). The old `.BTO` stores it per vertex in Eye Data, and FarFieldLodBtoChannels.h reads it
  that way.
- What it is: the native `.lodi` keeps ONE byte per placed piece, the average of that ramp over the piece's vertices
  (nativeemit.cpp:2518). Every kit piece is one flat grey, so a building comes out as a mosaic of flat tiles. That
  is the "texture" look.
- E1: I recomputed the per-vertex ramp from the terrain and the mesh vertices. Its per-piece average matches the stored
  byte with correlation 0.99993 and a mean difference of 0.14 (11,416 pieces, aud_ground.json). The byte is the
  average, as the doc says.
- E2: 24% of pieces run over at least half of the ramp from their bottom vertex to their top vertex. 77% of all
  vertices sit at 0 (no contact) and 10% at full contact. One grey per piece throws that away.
- E3: stored mean 29.85. 7,765 of 11,416 pieces are 0. A piece shorter than 256 u averages 14.0, a piece
  256-1024 u averages 35.0, and a piece of 1024 u or more averages 62.9. The value follows the piece's size,
  not the ground.
- E4: my offline redraw with the MAPS1 camera matches picture 03 (coverage overlap 0.865, mean grey difference 8.6).
  So A1 is picture 03, and A2 is the same data per vertex: 423,000 pixels differ.
- Verdict: **WRONG GRANULARITY.** Fix, pick one:
  (a) an FO4CS-side fix: compute it in the vertex shader from the vertex's world height and the terrain height the rung
  already samples. No bake change is needed, and the byte can be dropped. This is the cheaper fix.
  (b) a format change: add a per-vertex ground stream beside the sky stream (4.10).
- Pictures: `pics\A1_ground_contact_as_stored.png` (today) and `pics\A2_ground_contact_per_vertex.png` (what it
  should look like).

### 2.2 Is 04 the identity map?
**Yes. 04 is the building identity: the `.lodi` v7 group table, the key the far shadows use to tell "my own
building" from "someone else's".** Mostly right, with two real defects.
- E5: the two Hub City towers are one group each: 744 pieces up to z 9524, and 730 pieces up to z 8877.
  Trinity Church is one group of 25 pieces. In 04 they read as single colours, which is right.
- E6 (defect 1, **group ids are per chunk**): a building that crosses a chunk line is split in two. The Diamond City
  ballpark (165 pieces) is spread over 4 groups, and 14 of its pieces sit in chunk 39's group 73 only because of
  the line.
- E7 (defect 2, **the 64-unit join over-welds**): the ballpark's main group (40,22) holds 420 pieces over 8,623 x
  11,073 u, because the ballpark is welded to the Deco buildings around it. The row houses in group (24,65) are
  one "building" of 530 pieces spanning 12,696 u.
- Region numbers: 476 groups, 289 of them single pieces. Largest groups: 744, 730, 530, 442, 420.
- Verdict: **RIGHT, with WRONG GRANULARITY at the edges.** Fix (bake code): key groups over the whole file, not
  per chunk, so a building is never cut at a chunk line. Join by mesh contact (a few units, not 64) and by SCOL/
  precombine membership, with a size cap so a whole street cannot become one building.

### 2.3 05 is unusable; the object id needs to be per building
**Agreed. 05 is not a map, and it is not the per-building id.** It draws the LOW BYTE of the per-piece id as grey
(src/lodinative.cpp:845-851).
- E8: 256 greys shared by 11,414 pieces, about 45 pieces each. The ids go up to 6,117, so the grey wraps 24 times.
  Two unrelated pieces often get the same grey.
- The per-building id already exists: it is the group (04), and it is the one to keep.
- Verdict: **NOT A MAP / DUPLICATE of 10.** Fix (view change): remove the `identityraw` view, or relabel it
  "debug: low byte of the piece id".

### 2.4 10: does it look right, "same one single texture for all the buildings"?
**The data is right. The picture misleads.** 10 gives one random colour per placed piece. Every Boston building is
built from the same small kit pieces (wall, trim, corner), so every building turns into the same confetti and it
reads like one texture on everything.
- E9: 11,416 placements, 11,414 distinct piece ids (per chunk), 11,280 refs, 239 SCOL parts. One tower is 744 pieces.
- What it is for: per-piece work (fade dither, the random seed, debugging a single ref). It is not a building map;
  04 is.
- Verdict: **RIGHT BUT THE PICTURE MISLEADS.** Fix (view change): caption it "one colour per placed kit piece;
  buildings = map 04".

### 2.5 The ground normal map has no detail from the objects (roads, railway tracks, concrete pieces, decals)
**Correct. The terrain normal sheet is built from the heightmap only, on purpose.** Roads and flat objects are painted
into the colour sheet and into mask A, but never into the normal sheet. The doc says this matches vanilla:
"measured: vanilla's `_msn` is the heightmap's on the road too" (docs/LODGEN_TERRAIN_VT.md:786-795).
- E10: the normal sheet follows the heightmap slope with correlation 0.80 (east) and 0.83 (north), against -0.10 on the
  wrong axis (MAPS1 refuter). It carries nothing else.
- Against his goal (detail from objects): **EMPTY of object detail, by an earlier choice to match vanilla.**
  Fix (bake code, medium): where the bake already paints a road, track, concrete slab or decal into the colour
  sheet, also rasterise that mesh's own normals (its normal map) into the msn, using the same footprint and blend.
  Vanilla parity then becomes an opt-out switch.

### 2.6 36 cell height range: what does it do, and why is it so pixelated?
**It is the per-cell lowest and highest ground height, one pair per 4096-unit cell. Its only uses are culling bounds
and the water test "is the water above this cell's lowest ground?"** (LODGEN_BTD_FORMAT.md:336-360).
- It is blocky because there is one value per cell by definition. The view paints (max - min) / (largest range) as a
  brightness on the lit terrain (btdterrain.cpp:1411-1415), so each cell is one flat square: 8 x 8 cells in this
  picture.
- It holds nothing a player would see.
- Verdict: **NOT A MAP (the picture misleads).** Fix (view change): drop it from the map list, or draw it as a cell
  grid of numbers.

### 2.7 Water does not show water; water meshes are just the terrain meshes; water should be flat
**He is right. None of the MAPS1 water pictures shows a water surface.** The terrain viewer draws no water geometry
at all (btdterrain.cpp: the only "water" lines are labels and notes, :771 and :1422). Every water picture (34-44, 67-79)
is a number painted onto the ground mesh. So the "water" follows the hills.
- The data for flat water does exist, but only in the v3 file (`--water-bodies`, not the default):
  - E11: 346 bodies, each with one height. In this region, body 1 (class sea, 450 u, form 00000018) covers
    6,572 squares, the river body 43 (578 u) covers 340, and several small lakes sit at 450.
  - E12: I drew every wet square as a flat sheet at its body's height with the MAPS1 camera: 6,989 squares,
    96,469 water pixels, and 0 squares where the ground pokes above its water. The Charles comes out as one flat
    sheet with the bridges over it.
- The default bake (v2) cannot draw water right:
  - E13: its "has water" flag is set on ALL 36,864 cells, so it cannot tell water from land.
  - Its water height is per 4096-unit cell. River cells read 450 with the water +345.5 above the ground, against
    -746.1 elsewhere, so only a height comparison separates them.
- Verdict: the data is **RIGHT in v3 and WRONG/COARSE in v2**. The pictures are **NOT MAPS of water**.
  Fixes:
  - view change: draw the body planes flat, as A3 does;
  - bake switch: make `--water-bodies` (v3) the default;
  - FO4CS side: R2 must draw the body planes (plan:616).
- Picture: `pics\A3_water_as_flat_surfaces.png`.

### 2.8 Ground cover is empty
**Yes. On this bake the `.lodl` ground cover plane is empty (GCVR count 0, and view 39 is 0 px against the default).**
The terrain VT mask sheet's alpha DOES carry cover (mask A mean 31.5 over 6,553,600 texels, view 06), and that is
the place the plan names for it (plan:635, 1506). So ground cover lives in mask A, and the `.lodl` plane is a
leftover.
- Verdict: 39 **EMPTY** (the bake did not collect GCVR). 06 **RIGHT**.
- Fix: a view change (point "ground cover" at mask A), or bake code (fill the `.lodl` plane) only if something is
  going to read it. Nothing in the plan does.

## 3. The full table

Reader shorthand:
- **V** = the NifSkope viewer that drew the picture: `WW_LODL_CHANNEL` (src/lodinative.cpp:419-434, drawn at 790-887)
  or `WW_LODL_PLANE` (src/btdterrain.cpp:743-761, painted at 1300-1437).
- **off** = the MAPS1 offline decoder only (nothing in the product draws it).
- **FO4CS-R0** = wt-telem1 `src/ImprovedLOD/`: validates the headers and parses the tables, draws nothing
  (Runtime.cpp:186,191; Lodt.h:2; Lodm.h:2; Load.h:10).
- **plan** = docs/FO4CS_IMPROVED_LOD_PLAN.md, the rung that is meant to read it.

Fix size words, as the brief gives them: view change / bake switch / bake code / format change / FO4CS side.

### 3a. Buildings -- per object and per vertex (.lodi / .lodo)
| # | map | who reads | must ultimately be | is now | verdict | fix (size) |
|---|---|---|---|---|---|---|
| 02 | ao (vertex AO x piece AO) | V `ao`; FO4CS-R0 parses; plan:1716 R1 draws it | per-vertex darkening in creases and between buildings | per-vertex stream x per-piece byte; mean 172.4, reader = note | RIGHT | none |
| 03 | ground | V `ground`; FO4CS-R0 parses; stock path reads it per vertex (FarFieldLodBtoChannels.h) | per-vertex ramp: 1 at the ground, 0 by 256 u up | ONE byte per piece = the average of that ramp (E1-E4) | WRONG GRANULARITY | FO4CS side: compute per vertex from world z and terrain height (drop the byte); or format change: per-vertex stream |
| 04 | identity (group) | V `identity`; plan:1592,1718 far-shadow caster id | one id per building, stable across chunks | 476 groups in view; towers and church right (E5); cut at chunk lines (E6); 64 u join welds neighbours (E7) | RIGHT, WRONG GRANULARITY at edges | bake code: file-wide group ids; contact/SCOL join with a size cap |
| 05 | identityraw | V `identityraw` (low byte, lodinative.cpp:845-851) | nothing; a debug view | 256 greys for 11,414 pieces (E8) | NOT A MAP / DUPLICATE of 10 | view change: remove or relabel "debug" |
| 10 | placement | V `placement`; per-piece work (fade, seed) | one id per placed piece | one hash colour per piece; kit pieces make every building the same confetti (E9) | RIGHT BUT THE PICTURE MISLEADS | view change: caption "per kit piece; buildings = 04" |
| 11 | scrappable (flag bit 6) | V `scrappable`; plan s9 rule 4 drops workshop casters | set on workshop-owned pieces | ABSENT: file is v7, the bit is v9, written only with `--scrappable` | EMPTY (switch off) | bake switch `--scrappable` when s9 rule 4 is wanted |
| 12 | seed | V `seed`; the card/tree variation | a random number per tree | trees only (buildings 0, black); mean 3.5 over all pieces | RIGHT | none |
| 13 | selfao (library vertex) | V `selfao`; lodo vertex 0x0F | a model's own crease darkening, shared by every copy | mean 233; near white on buildings, darker on trees | RIGHT (weak on flat kit walls, as expected) | none |
| 14 | sky (vertex stream) | V `sky`; plan:1718 R1 reads it | per-vertex share of open sky, buildings included | open up-facing 183.5 vs covered 52.7 (MAPS1 refuter) | RIGHT | none |
| 15 | sway | V `sway` (lodo 0x0E) | wind bend weight, trees only | trees only, 0 on buildings; mean 9.1 | RIGHT | none |
| 21 | view-space normal (stock ch 8) | V `WW_LOD_CHANNEL=8` | debug of the shading normal | normals seen from the camera, terrain + objects | RIGHT (a debug view) | none |
| 22 | ao, lit | V (L06 = L00 byte for byte) | the lit picture with AO applied | identical to 16 (0 px) | DUPLICATE of 16 | view change: drop one |
| 23 | per-object flags bits 0-5 | off; FO4CS-R0 parses | which pieces are trees / alpha / water-tight etc. | classes 0/1/4/16 over 72k/1.4k/26k/5k triangles | RIGHT | none |
| 24 | LOD mesh normals (world) | off (lodo vertex normal, oct 12:12) | the mesh's shading normal | face agreement 0.971; flat roofs 53.7% of pixels | RIGHT | none |
| 25 | occluder boxes | off; FO4CS-R0 parses; meant for culling (NATIVE doc:79 "a few boxes per cell for buildings and hills") | building-sized boxes | 340 boxes in the whole Commonwealth, 95 here; median half-size 2.6 x 105 x 192 u, 98% thinner than 128 u: wall slabs, no hills (aud_occ.json) | WRONG DATA (too thin to hide anything far off) | bake code: fit boxes inside the GROUP's volume, add hill boxes from terrain |
| 26 | piece AO byte 0x10 | off; multiplied into 02 | per-piece occlusion from neighbours | one grey per piece | RIGHT (it is the coarse half of 02) | none |
| 27 | sky byte 0x11 (per piece) | off; v6 fallback | fallback when the stream is absent | one grey per piece | DUPLICATE of 14 at coarser grain | format change (small): drop on v7+, or keep as fallback |
| 28 | sky vertex stream (top-down) | off (= 14 from above) | as 14 | as 14 | DUPLICATE of 14 | none (a second angle) |
| 29 | vertex-AO stream alone | off | the per-vertex half of 02 | per-vertex grey | RIGHT | none |
| 30 | vertex colour alpha (lodo v5 stream) | off; FO4CS-R0 parses | the model's own vertex alpha where it has one | 199 of 11,416 pieces carry the stream; alpha reads one flat value in view | EMPTY in practice | none needed; keep it as a pass-through |
| 31 | vertex colour RGB | off | the model's own vertex tint | white on trees and a few kit pieces (199 pieces) | RIGHT (vanilla content) | none |
| -- | position, UV, tangent, cardLayer, drawKey, flag bits 7-8, aggregates (0), horizon (retired) | FO4CS-R0 parses | structure | structure / absent | NOT A MAP | none |
| -- | cold identity u16 | V via 05/10 | the stock manifest index | per-chunk index, max 6,117 | RIGHT (structure) | none |

### 3b. Building textures + tree cards (Objects\*.DDS, sheet space)
Two facts behind every row:
- (i) MAPS1's "A512" set is `LodgenArrays.512x512`, 14 layers, and it holds tree and leaf textures, not buildings.
  The building textures are in the 128x128 set (12 layers) and the 256x256 set (18 layers), and they are vanilla's
  shared LOD materials (BldgBrickLargeLOD, DecoLargeLOD, HitTech...). A few shared textures cover every building,
  which is how vanilla authored them.
- (ii) 10 of the 114 array layers are the same texture triple stored twice, because the same material is spelled two
  ways (`c:\projects\...` vs `materials\lod\...`): BldgBrickLarge 0/12, DecoLarge 1/13, HitTechStructure 4/15,
  NCALarge 10/16, Billboard02 0/31... (aud_arrays.json).

| # | map | who reads | must ultimately be | is now | verdict | fix (size) |
|---|---|---|---|---|---|---|
| 46 / 55 | colour _d (mesh set / card set) | off; FO4CS-R0 parses the .lodm; plan R4 | the LOD textures, one layer per material | right textures; 46 captioned "Building" but shows trees; 10 duplicate layers across sets | RIGHT BUT THE PICTURE MISLEADS (46); DUPLICATE layers | view change: caption 46 "tree/leaf array"; bake code (small): key layers by texture triple |
| 50 / 59 | normal _n XY (Z rebuilt) | off; plan R4 | tangent normals (mesh) / view normals (cards) | cards: left edge X -0.119, right +0.104 (right way round) | RIGHT | none |
| 49 / 58 | height _n B | off | cards: silhouette depth; meshes: unused | meshes: 132 on 100% of covered texels (all 7 sets); cards: 52..207 | RIGHT on cards; EMPTY on meshes (by rule) | format change (small): meshes' _n as 2 channels |
| 53 / 62 | sway _n A | off; plan:965 R4 sway | cards: sway weight; meshes: sway lives in the vertex | meshes 0 everywhere; cards 0..255 mean 91 | RIGHT on cards; EMPTY on meshes (by rule) | as 49 |
| 48 / 57 | gloss _gsaos R | off; plan R4 | per-material gloss | means 16-97 | RIGHT | none |
| 51 / 60 | spec _gsaos G | off | per-material spec | means 20-112 | RIGHT | none |
| 45 / 54 | AO _gsaos B | off | cards: baked tree AO; meshes: unused (the vertex carries AO) | meshes 255 on every texel; cards 0..255 mean 196 | RIGHT on cards; EMPTY on meshes | as 49 |
| 52 / 61 | SSS _gsaos A | off | leaf translucency | 512x512 leaves 255, buildings 0-41, cards 255 | RIGHT | none |
| 47 / 56 | emissive _g | off | glowing windows and signs | 0 on every texel of every set (g_max 0); the manifest names no glow source for any of the 114 layers, because vanilla's LOD materials have none | EMPTY (no source) | bake code (small): write no _g file when empty; glowing windows would need a new source (the full model's glow map) |

### 3c. Ground (.lodt VT sheets, .lodl planes, references)
| # | map | who reads | must ultimately be | is now | verdict | fix (size) |
|---|---|---|---|---|---|---|
| 01 | flat default | V | the reference every channel is compared to | a floor | NOT A MAP (reference) | none |
| 16 | lit reference (AO decal) | V | the look reference | as it should be | NOT A MAP (reference) | none |
| 17 | lit default | V | reference | reference | NOT A MAP (reference) | none |
| 18 | raw colour (stock ch 12) | V | reference, unlit | reference | NOT A MAP (reference) | none |
| 20 | emissive (VT role 6) | V `emissive`; FO4CS-R0 Lodt | glowing ground (none in vanilla) | ABSENT, 0 px against the default | EMPTY (nothing on the ground glows) | none |
| 06 | mask A: ground cover | V `mask-a`; plan:635,1506 | where grass cards grow, 0 on roads | mean 31.5, 0 on roads and the river bank | RIGHT | none |
| 07 | mask B: sky AO (terrain) | V `mask-b`; plan:1590 R2 | ground darkening from the hills AND from the buildings (street canyons) | a horizon march over terrain only (VT.md:966): valleys 169.7, ridges 231.6; ground under buildings 227.1 is BRIGHTER than open ground 216.5 | WRONG DATA against the final look (by design today) | bake code: include the LOD object meshes in the horizon march |
| 08 | mask G: metallic | V `mask-g` | 0 for ground | constant 0 | RIGHT (a spare channel) | none |
| 09 | mask R: roughness | V `mask-r` | per-texture roughness | mean 172, 0..247 | RIGHT | none |
| 19 | normal as drawn | V `normal`; plan:590 R2 | ground normal + detail from roads, tracks, slabs, decals | heightmap only (E10) | EMPTY of object detail (choice to match vanilla) | bake code (medium): stamp flat-object normals into the msn |
| 65 | msn as stored | off | as 19 | as 19; R east 200.5 / west 58.6 on slopes | as 19 | as 19 |
| 66 | msn in world XYZ | off | as 19 | as 19 | DUPLICATE of 65 (another colour code) | none |
| 63 | colour sheet | off; plan:590 R2 | the ground's colour with roads and decals painted in | roads, lots and grass painted; right way round | RIGHT | none |
| 64 | height sheet (role 4) | off; R2 | fine height for the far terrain | raw 32676..33091; building pads show as flat squares | RIGHT | none |
| 40 | height (.lodl) | V `height`; FO4CS-R0 header only; far-shadow heightmap source (plan:222) | the whole-world height | exact to the ESM (lodgen gates) | RIGHT | none |
| 41 | overview (.lodl) | V `overview` | an 8-per-cell height grid kept in memory (BTD_FORMAT:371) | that | RIGHT, NOT A MAP (a lookup table, drawn as shading) | none |
| 33 | blend weights (LTEX) | V `blend`; the bake (to build colour) | the input to the colour sheet | one hashed colour per land texture, blended | RIGHT, not needed at run time | none |
| 37 | colour (.lodl, VCLR 5-5-5) | V `colour` | the cell vertex tint (BTD_FORMAT:625) | near white across the region | RIGHT BUT THE PICTURE MISLEADS (a tint, not the ground colour; 63 is the colour) | view change: caption "vertex tint" |
| 32 | AO plane (.lodl) | V `ao` | as 07 | terrain only; corr with relief 0.61 (07: 0.63) | DUPLICATE of 07 at a coarser grain, same missing buildings | format change (small): drop it, or bake code as 07 |
| 39 | ground cover (.lodl) | V `groundcover` | as 06 | GCVR 0, 0 px against the default | EMPTY (the ground cover lives in 06) | view change: point "ground cover" at 06 |
| 36 | cell height range | V `cellrange` | min/max per cell for culling and the water test | one value per 4096 u cell | NOT A MAP (the picture misleads) | view change: drop or print numbers |
| -- | VT roles 3 and 7, road/flat masks | -- | retired / folded into colour and mask A | no bytes | NOT A MAP | none |

### 3d. Water
The viewer draws no water surface. Every row below is a number painted on the ground mesh. That is why he sees
"water" following the hills.

| # | map | who reads | must ultimately be | is now | verdict | fix (size) |
|---|---|---|---|---|---|---|
| 35 | land/water flags (v2) | V `cellflags`; FO4CS-R0 header only | has-water only where water shows | set on ALL 36,864 cells (E13) | WRONG DATA | bake code (small): set bit 0 only where water > the cell's lowest ground |
| 43 | water height (v2) | V `waterheight` | a flat plane at the body height | one float per cell; river cells 450 | RIGHT data, WRONG GRANULARITY (a cell, not a shore line); picture not flat | bake switch: `--water-bodies` default; view change: draw flat |
| 44 | water type (v2) | V `watertype` | which WATR material | one index per cell; default + one other cell here | RIGHT (coarse) | none |
| 34 / 38 / 42 | body id / flow / shore on v2 | V | the v3 planes | ABSENT on v2 (0 px) | EMPTY (v2 does not carry them) | bake switch: `--water-bodies` default |
| 67 | body id (v3) | V `bodyid`; plan:616 R2 | which body each 128 u square belongs to | 346 bodies; here body 1 (class sea, 450) = 6,572 squares, river 43 (578) = 340 | RIGHT BUT THE PICTURE MISLEADS (drawn on ground, not flat) | view change: draw flat, as A3 |
| 68 | cell flags (v3) | V | as 35 | 0 px against 35 | DUPLICATE of 35 (same defect) | as 35 |
| 69 | flow (v3) | V `flow`; R2 | river direction + speed | river 100% flowing at 189.8 deg, sea 0% | RIGHT | none |
| 70 | height (v3 file) | V `height` | as 40 | as 40 | DUPLICATE of 40 | none |
| 71 | distance to shore (v3) | V `shore`; plan:616 | shore fade | bank 4.0 steps vs inner 60.0, dry 255 | RIGHT | none |
| 72 | water height (v3 file) | V | as 43 | 0 px against 43 | DUPLICATE of 43 | none |
| 73 | water type (v3 file) | V | as 44 | 0 px against 44 | DUPLICATE of 44 | none |
| 74 | whole Commonwealth flags (v2) | off | as 35 | all one colour: every cell flagged | WRONG DATA (as 35) | as 35 |
| 75 | whole Commonwealth water height (v2) | off | as 43 | per-cell blocks along the rivers | RIGHT, coarse | as 43 |
| 76 | whole Commonwealth water type (v2) | off | as 44 | per-cell blocks | RIGHT, coarse | none |
| 77 | whole Commonwealth body id (v3) | off | bodies with true shore lines | sea around the coast, rivers and lakes inside | RIGHT | none |
| 78 | whole Commonwealth flow (v3) | off | as 69 | rivers coloured, sea flat | RIGHT | none |
| 79 | whole Commonwealth shore (v3) | off | as 71 | bank lines | RIGHT | none |
| -- | dye plane, stroke store, water in VT / objects | -- | a marking layer / the flow edit source / none | absent / source only / none | NOT A MAP | none |
| A3 | water drawn flat (this audit) | off | what 67 should look like | 6,989 flat squares, 0 with ground above the water (E12) | the right picture | view change in NifSkope to match |

## 4. Ranked: what is WRONG or EMPTY and matters for the final product (biggest first)
1. **Water is not drawn as water, and the default bake cannot say where water is.**
   - The viewer draws no surface.
   - The v2 has-water flag is on every cell (E13), and its water height is per 4096 u cell.
   - The v3 bodies are right (E11, E12) but opt-in.
   - Fix: bake switch (v3 default) + view change (flat body planes) + FO4CS side (R2 draws the planes).
2. **Ground contact is one grey per kit piece instead of per vertex (03).** The stock path has it per vertex; the
   native file lost that.
   - Fix: FO4CS side (compute per vertex from world z and the terrain height, cheapest), or a format change
     (a per-vertex stream).
3. **Building identity (04) cuts buildings at chunk lines and welds whole streets.** Diamond City is split over 4
   groups; row houses weld into 12.7 k-unit blocks. The far shadows key on this.
   - Fix: bake code (file-wide ids, contact/SCOL join, size cap).
4. **Ground AO and sky (07, 32) ignore buildings.** Street canyons are no darker than open ground; ground next to
   buildings is even brighter (227 vs 217).
   - Fix: bake code (march objects too).
5. **The ground normal map has no object detail (19/65).** No roads, tracks, slabs or decals, on purpose for
   vanilla parity.
   - Fix: bake code, medium (stamp object normals where colour is already stamped).
6. **Occluder boxes are wall slabs (25).** Median 5 u thick, 340 in the whole world, no hills: too small to hide
   anything at LOD range.
   - Fix: bake code (boxes per building group + hills).
7. **Empty channels shipped as files and bytes:**
   - emissive _g on every array (0 everywhere, no source);
   - mesh _n B/A (constant 132/0);
   - mesh _gsaos B (255);
   - `.lodl` ground cover (GCVR 0);
   - `.lodl` AO plane (a coarser copy of mask B);
   - per-piece sky byte (a coarser copy of the stream).
   - Fix: format change / bake code, small each.
8. **10 of 114 texture array layers are duplicates**: the same texture under two spellings of its material path.
   - Fix: bake code, small.
9. **Pictures that mislead:**
   - 05 (low byte, not a map);
   - 10 (per kit piece, reads as one texture);
   - 36 (a culling table);
   - 37 (a tint);
   - 46 (captioned "Building", shows trees);
   - all water planes painted on the ground;
   - duplicates 22, 28, 66, 68, 70, 72, 73.
   - Fix: view changes.

Not wrong, for the record: AO (02), sky (14), sway, seed, selfao, normals (24, cards), colour sheet, roughness,
ground cover in mask A, flow, shore, height.

## 5. Pictures (full size, labelled at the top)
- `C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\audit1\pics\A1_ground_contact_as_stored.png`: today's ground contact, one grey per piece (matches 03: overlap 0.865, grey difference 8.6).
- `...\scratchpad\audit1\pics\A2_ground_contact_per_vertex.png`: the same data per vertex, as the doc defines it.
- `...\scratchpad\audit1\pics\A3_water_as_flat_surfaces.png`: the v3 water bodies drawn flat at their heights,
  with the MAPS1 camera (6,989 squares, 0 below the ground).
- Camera: view 8 ortho, rot -63.5593, 0, 133.3081, look-at -4096,-24576,0, 1600x1624, 20.48 u a pixel. Reproduced
  with Euler order x-y-z; terrain outline overlap with P_height 0.848, against 0.499 for the next-best order (aud_cam.json).

## 6. Skills: loaded, wished for, written; deviations
- **Loaded:** nifskope-ww-lodgen, search-lean, ww-channel-view-refuter, ww-legend-matches-picture, ww-texel-picture.
- **Wished for:** a skill that draws a LOD bake offline in the viewer's own camera. I re-derived it: the Euler order
  fit, the raster, and the control against the renderer's PNG.
- **Written:** `ww-lod-offline-picture`, in `E:\Projects\Claude\.claude\skills\ww-lod-offline-picture\` and
  `E:\Tools\AISkills\ww-lod-offline-picture\`. It holds SKILL.md plus the six scripts (aud_lib, aud_cam,
  aud_ground, aud_render, aud_control, label).
- **Deviations, stated:**
  - (1) Early on I ran one Bash heredoc (`python - <<'EOF'`) for a camera check. It hung, I stopped it, and I redid
    it as aud_cam2.py. Later I ran two empty or no-op heredoc / `python -c` lines by mistake. None of them wrote or
    changed anything. The brief forbids heredocs; these are recorded, not hidden.
  - (2) The brief pointed at wt-fixfirst for the FO4CS reader, but READER1 lives in wt-telem1.
  - (3) The only writes outside audit1 are the two skill folders above, which section 6 of the brief asks for.
  - No builds, bakes, commits, installs or launches.
