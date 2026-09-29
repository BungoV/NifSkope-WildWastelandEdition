# TERRLIVE1 -- three LOD terrain render options (FULL / HYBRID / DYNAMIC) + prebaked road and flat-object decals

Launched 2026-09-29 03:59 (clock read). Worktree E:\Projects\NifskopeWWE-terrlive1, branch terrlive1-20260929 from
origin/main 9df26ffd. Rung exe = this worktree's first build, release/NifSkope.before_terrlive1.exe, sha1 a25dd5da
(04:04, 0 sources changed; main's objects proven current by `make -n` = 0 g++ lines).

## 0. Numbers first (written 2026-09-29 04:08, before any code; offline, from the installed whole-map bake)

Source: E:\Projects\NifskopeWWE-night\scratchpad\merge1_20260929\stage (read-only), its log cw_chunks_full.log line 435
and Commonwealth.flat_objects_report.txt.

### Bytes of today's bake, by kind
| part | bytes |
|---|---|
| VT.2.lodt (16 u/texel, 9,216 tiles) | 11,432,696,928 |
| VT.4.lodt (32 u) | 2,863,062,112 |
| VT.8.lodt (64 u) | 716,647,520 |
| VT.16.lodt (128 u) | 179,997,792 |
| VT.32.lodt (256 u) | 45,706,336 |
| everything else (3,651 files: .lodl .lodi .lodo .lodb .lodj, Objects\*, reports) | 667,615,180 |
| total | 15,905,725,868 |

### Decal library (estimate; the real count comes out of the bake and replaces this)
- A "piece" = one model path + its effective material swap (the swap changes the picture). Distinct models:
  roads 359 (census `roadMeshes`), flat objects 1,604 painted models (report rows with "painted x>0": debris 1,309,
  pad 215, decal 41, path 22, rail 17). About 1,963 pictures before swaps; 1,228 of 18,727 road placements carry a swap,
  so a few dozen more.
- Placements: roads 18,727, flat objects 61,850 = 80,577 decals.
- Picture density chosen: 4 world units a texel (4x finer than VT.2's 16 u, so the hybrid near field is never blurrier
  than FULL), longest side capped at 1024 texels. Footprint per piece estimated from the census: roads
  11,411,371 texels x 256 u^2 / 18,727 = ~156,000 u^2 a placement (~395 u square); flats (report `squares` x 256 u^2 /
  placements) ~42,000 u^2 (~205 u square); bounding box ~1.5x the covered area.
  Texels: roads 359 x 156k x 1.5 / 16 = ~5.2 M; flats 1,604 x 42k x 1.5 / 16 = ~6.3 M; total ~11.5 M texels.
- Picture bytes a texel: colour + coverage BC3 1 B, normal BC5 1 B (2 channels, z rebuilt), full mip chain x 1.33
  = ~2.7 B a texel [correction 04:53: the normal sheet shipped as BC3, not BC5 -- it carries the stamp weight in
  alpha -- so it is the same 1 B a texel; the estimate stands] -> **~31 MB for the whole library** (estimate).
- Placement record: 32 B (position 3 x f32, rotation quaternion 4 x snorm16, scale f32, piece u16, draw class u8, pad,
  source form id u32) -> 80,577 x 32 = **2.6 MB**, plus a per-cell index (36,864 cells x 8 B = 0.3 MB).

### Totals per option (estimate)
| option | holds | bytes |
|---|---|---|
| FULL | everything today + decals | 15.906 GB + ~0.034 GB = ~15.94 GB |
| HYBRID | VT.8/16/32 + non-.lodt + decals | 0.942 + 0.668 + 0.034 = **~1.64 GB** (10.3% of FULL) |
| DYNAMIC | non-.lodt + decals | 0.668 + 0.034 = **~0.70 GB** (4.4%) |

### Road-box screen coverage (estimate, before measuring)
Downtown Boston (cells -5..2 x -10..-3, 64 cells = 1.07e9 u^2): roads + pavements cover roughly a quarter to a third
of the ground there; a decal box is ~1.5x its covered area and flats add their own, so the decal pass should touch
0.5-1 box layers per ground pixel on average in that view (measured below with the real boxes).

### Bake time
Whole map today: textures stage 5,091 s of 6,792 s. VT.2 is 9,216 of the 12,276 tiles and every coarser level is a box
filter of the finer one, so the textures stage is almost all VT.2. HYBRID bakes its finest level at 64 u directly
(576 tiles): expected ~1/16 of the VT.2 tile work. Measured on the Boston box below.

## 1. Design calls (written 2026-09-29 04:17, before code; each one is mine to settle, logged here)

1. **Option switch.** `--terrain-option full|hybrid|dynamic`, default `hybrid` (bungo's ruling). It changes what the
   texture pyramid does and nothing else:
   - full = today's pyramid, unchanged.
   - hybrid = the pyramid's finest level is dim 8 (64 u a texel at content 512), baked DIRECTLY rather than box-filtered
     from VT.2/VT.4, so VT.2 and VT.4 are never computed. VT.8 bytes therefore differ from FULL's VT.8 (filtered); the
     difference is measured below.
   - dynamic = no pyramid at all (no .lodt, no .VT.lodm).
   Recipes and spells that relied on the old default now get hybrid. The way back is `--terrain-option full`.
   The byte gate runs FULL with the flag spelled out.
2. **Decals on every option.** Two new files beside the .lodl, both additive:
   - `<ws>.lodd` = the decal library, one picture set per DISTINCT piece;
   - `<ws>.lodg` = the placements plus a per-cell index.
   A piece = model path + effective material swap (the swap changes the picture; ROADS1's rule). Every road placement
   and every painted flat object (FLAT1's rule, unchanged) becomes one decal placement.
3. **Picture.** Each piece is scan-converted in its OWN frame (placement transform removed) by the existing road/flat
   rasteriser, same laws (road composite, detail, ground-material paint, flat order, normal stamp), at 4 local units a
   texel, longest side capped at 1024. Colour + coverage and the stamped normal + its weight, each BC3 with a full mip
   chain. A flat object's "buried" test needs the ground under ONE placement, so the picture paints every fragment
   (rise 0); the ground hides what is under it at draw time. No mask sheet: the stamp writes only coverage into the
   mask (it suppresses cover), and coverage is the colour's alpha.
4. **Projection.** A placement's box = the piece's local XY bounds x its local Z range padded 128 u, projected along the
   box's local -Z. Draw order stored in the file: roads by mean Z, then opaque flats by mean Z, then over-flats by mean Z
   (the rasteriser's order).
5. **.lodb.** A new `terrain <option>` line (unknown kinds are ignored by old readers). The new files also appear as
   product rows, because the record lists every file in the folder.
6. **Preview.** A new offscreen renderer inside NifSkope.exe (`lodgen --terrain-preview`): the .lodl heights as a mesh,
   the live splat from the .lodl's LTEX slots and 3-bit weights with the game's landscape textures (LTEX -> TXST
   through the loaded plugins), the decals as projected boxes, the far baked levels, a cross-fade. GL timer queries
   around each pass. The same program renders all three options, so the three pictures differ only by option.
7. **Live splat limits, stated.** It reproduces the .lodl's data: layer order is the file's slot order (ranked by peak,
   slot 0 on top), weights are 3-bit, VCLR is 5-bit. It does not reproduce the bake's quadrant cross-fade, grass tint,
   erosion shading or the vanilla-colour fill of unpainted cells. Those show up as the live-vs-baked difference that
   the crossover measurement reads; DYNAMIC loses the vanilla fill outright.

## 2. What was written (2026-09-29 04:53; build 1 = c742b7a3, RC 0, 0 warnings in the changed files)

- `src/io/loddecal.h/.cpp` -- the two new files and their readers:
  - `.lodd` = LODD v1, 256-byte header, 80-byte piece table, name blob, 16-aligned BC3 chains (colour+coverage, then
    stamped normal+weight), per-picture CRC, table CRC, header CRC.
  - `.lodg` = LODG v1, 32-byte placements (position, snorm16 quaternion, scale, piece, draw class, source form),
    a per-cell index (row 0 south) of record ids in draw order, data CRC, header CRC.
  - `lodgen --decal-check <file|dir>` reads both back, checks every CRC, the pairing (table CRC + piece count), every
    record's piece, the draw order, unit quaternions, and decodes mip 0 and the last mip of every picture.
- `src/lodgen.cpp` -- `lodgenBakeDecals` (the library + placements, from the road/flat gather the pyramid already
  uses, each piece rasterised once in its own frame by the same rasteriser), the option switch
  (`lodgenTerrainOptionApply`: hybrid = finest level dim 8 baked directly), and `lodgenLtexPicture` (for the preview).
- `src/nifcli.cpp` -- `--terrain-option full|hybrid|dynamic` (default hybrid), the decals stage on every bake that
  writes a pyramid or would have, dynamic writes no .lodt; `--decal-check`; `--terrain-preview <spec.json>`.
- `src/lodgenmanager.cpp` -- a "Terrain" row (Full / Hybrid / Dynamic) in the LOD generator panel, label + control only,
  saved in LodGeneration/terrainOption.
- `src/lodbfile.*` -- a `terrain <option>` row after `switches`; `decals:` is a census keyword.
- `src/terrainpreview.*` -- the offscreen preview (section 4).
- No .lodl/.lodi/.lodo/.lodt format changed. Nothing here needed a format change.

## 3. Byte gate: FULL against today's bake (Boston box -8 -12 3 -1; written 2026-09-29 05:16)
Rung = release/NifSkope.before_terrlive1.exe (a25dd5da); new = c742b7a3 with `--terrain-option full`. Same recipe
(MERGE1 bake.sh copy), same box. `python gate_full.py bakes/rung bakes/full`:
* **GATE GREEN: 232 of 232 files the same.** 213 are sha1-identical, which is every shipped file: .bto 17, .btr 17, .dds 98,
  .lodi, .lodj 17, .lodl, .lodm 24, .lodo, .lodt 2 (VT.2 + VT.4). The other 19 are equal once the environment is
  masked: 17 chunk-cache .key files (only their `inputs`/`switches` digests move, because the switch vector now
  carries the option; the `bto`/`manifest` digests are equal), the flat-objects report (it names the run folder),
  and the .lodb (bake folder path, exe byte size in the header, wall-clock seconds, the flat report's hash, plus
  the rows the option adds). New files, additive: Commonwealth.lodd 51,991,408 B, Commonwealth.lodg 370,756 B.
* First run was RED with 19 findings: all of them were those environment differences (read one by one), so the
  masks were added to gate_full.py by name, never a blanket skip.
* **Sabotage, red as it should be:** a copy of the FULL tree with one bit flipped in VT.2.lodt, one census count
  changed in the .lodb (69806 -> 69807) and one digit changed in a .key's `bto` digest -> `GATE RED: 4 findings`,
  all three caught (the 4th is the copy's own folder name in the flat report, an artefact of copying the tree).

## 4. Chain 1 bakes (build 1 = c742b7a3) and what they showed (written 2026-09-29 05:39)
Boston box, same recipe, one after another (sum of the two lodgen runs, wall clock):
| bake | lodl + chunks | meshes | textures | mod bytes | scr bytes | .lodt |
|---|---|---|---|---|---|---|
| rung (a25dd5da) | 7 + 439 s | 263.6 s | 129.6 s | 477,024,982 | 75,110,360 | VT.2 + VT.4 |
| FULL | 7 + 561 s | 287.0 s | 140.8 s | 529,388,259 | 75,110,360 | VT.2 + VT.4 |
| hybrid (build 1) | 8 + 574 s | 287.9 s | 152.5 s | 529,388,279 | 75,110,360 | VT.2 + VT.4 (see below) |
| dynamic | 8 + 438 s | 292.2 s | 8.4 s | 465,651,464 | 53,086,640 | none |
* The decal stage is the difference between rung and FULL: 82-87 s (gather ~39 s, pictures ~43 s) plus
  52,362,164 B of new files (.lodd 51,991,408 + .lodg 370,756). The rest of the wall-clock spread is machine
  noise (meshes 263 -> 287 s with no mesh code changed; IDENT2 was baking alongside).
* The decal files are sha1-identical across FULL, hybrid and dynamic, and each pair reads back through its own
  reader (`lodgen --decal-check`): 908 pieces, 10,061 placements, 0 CRC mismatches, pair matched (names CRC
  851e3ca9), 0 picture decode failures, 0 bad records.
* **Build 1's hybrid did not skip VT.2/VT.4 in a full-product bake.** My own design call: the .btr chunk sheets
  (dims 4 and 8) are assembled from the pyramid's dim 2 and dim 4 staging, so build 1 kept the finest level when
  chunk sheets were asked for. That meant hybrid = FULL + nothing saved. Fix (build 2 = 8afeef50): a new
  `LodgenVtOptions::writeFinestDim`; hybrid in a chunk-sheet bake stages dims 2 and 4 for the sheets and neither
  encodes nor writes them. The written containers carry the header a pyramid starting at dim 8 would carry
  (levelIndex/levelCount/levelDims shifted). A VT-only bake (no chunk sheets) starts the pyramid at dim 8
  outright. Default 0 = today's bytes, so FULL is re-gated on build 2 (chain 2).
* **Dynamic writes no .btr chunk texture sheets** (scr/textures: 27 DDS fewer, 22,023,720 B less), because those
  sheets come out of the pyramid it does not run. The chunk meshes are still written. Whether the vanilla-format
  chunk sheets are still wanted under dynamic is bungo's call (owed).
* The Boston box's own pyramid stops at dim 4 (its south edge -12 is not a multiple of 8), so under hybrid it
  writes no .lodt at all; its far levels come from the whole-map hybrid bake.

## 5. Chain 2 (build 2 = 8afeef50), gates (written 2026-09-29 05:59)
| bake | lodl + chunks | meshes | textures | mod bytes | .lodt |
|---|---|---|---|---|---|
| FULL (build 2) | 9 + 580 s | 267.9 s | 157.3 s | 529,388,268 | VT.2 + VT.4 (62,855,680 B) |
| hybrid (build 2) | 8 + 563 s | 271.4 s | 158.1 s | 466,530,558 | none written; dims 2 + 4 staged for the chunk sheets (45 tiles) |
* **FULL gate on build 2: GATE GREEN, 232 of 232 the same** (213 sha1-identical, 19 after the named masks),
  decals additive as before. gate_full2.txt.
* **Sabotage: hybrid vs rung, `--expect-red`: GATE RED, 4 findings** -- VT.2.lodt and VT.4.lodt only in the
  rung, VT.lodm only in the rung, and the .lodb. Every .btr (17), chunk sheet .dds (98) and .key (17) is still
  byte-identical to the rung: staging the levels without writing them leaves the chunk sheets exactly as they were.
* **Bake time in a full-product bake: no saving.** Hybrid's texture stage is 158.1 s against FULL's 157.3 s,
  because the chunk sheets need the dim 2 and dim 4 tiles baked anyway, and the tile bake (not the encode or the
  write) is the cost. Hybrid saves the bytes there (62.9 MB of .lodt, 12% of this box's mod folder), not the time.
  The pyramid-only saving is measured in chain 3 (Boston pair with `--no-vt-btr`, and the whole map VT-only).

## 6. Chain 3: the pyramid-only bake-time saving, and the whole map (written 2026-09-29 07:38)
**Boston pair with `--no-vt-btr`** (chunk sheets baked the older way, so FULL and hybrid differ only in the
pyramid levels they bake):
| bake | lodl + chunks | meshes | textures | mod bytes | .lodt |
|---|---|---|---|---|---|
| FULL `--no-vt-btr` | 15 + 1344 s | 319.4 s | 850.9 s | 529,393,483 | VT.2 + VT.4, 62,855,680 B |
| hybrid `--no-vt-btr` | 12 + 1281 s | 302.5 s | 814.2 s | 472,293,139 | VT.8 (4 tiles), 5,588,224 B |
* Pyramid-only saving on Boston: **36.7 s of the texture stage** (4.3%), 57.3 MB of .lodt. The older chunk-sheet
  path is the bulk of both (it is why the default recipe assembles them from the pyramid: 157 s against 851 s).
* So on the recipe we ship (chunk sheets from the pyramid) hybrid saves **bytes, not time**; the time saving
  exists only when nothing downstream needs the fine levels.

**Whole map, hybrid, VT-only** (`--vt ... --tex-dir <scratch> --no-vt-btr --terrain-option hybrid`, same exe):
* **1337 s wall** for the whole run: decals 296.0 s (gather 156.3 s, pictures 136.7 s), the rest load + the
  VT.8/16/32 pyramid (756 tiles). For scale only, not like for like: the staged whole-map FULL bake's texture
  stage was 5090.7 s (dims 2..32, 12,276 tiles, chunk sheets assembled, `--no-collapse-uniform`).
* Bytes: VT.8 572,673,120 + VT.16 134,597,104 + VT.32 42,224,736 = **749,494,960 B** of .lodt; decals .lodd
  161,076,176 + .lodg 3,261,484 = **164,337,660 B** (2,818 pieces, 80,577 placements, read back 0 CRC
  mismatches). Staged FULL .lodt: 15,238,110,688 B (uniform tiles NOT collapsed there; VT.2 alone 11.43 GB).

## 7. The preview: GPU cost, pictures, crossover (written 2026-09-29 08:10; build 4 = 5eff5b6c)
`lodgen --terrain-preview <spec.json>` (src/terrainpreview.cpp). Offscreen GL 4.3, RTX 5070 Ti, driver 610.74. The
times are GL timer queries, median of 30 frames after 1 warm-up. Only the ground is drawn: no objects, no
buildings, no LOD cards. Specs come from `make_spec.py`, and `pv.sh <name>` runs one spec under the turn.
Builds 3 and 4 changed only terrainpreview.cpp:
- build 3: relative spec paths now resolve against the spec's own folder, and a CSV that cannot be written is
  counted as a failure. The first crossover run wrote its CSV nowhere and still said "no failures".
- build 4: an empty base slot is the engine's default land set (esmdata.h ESM_LTEX_ENGINE_DEFAULT), not grey.
  Before this, big grey patches covered the live splat. Dynamic vs FULL on the oblique view went from 19-24/255
  to 6-10/255.

**GPU ms (terrain / decals / lighting / total).** fade = 8192..12288 u.
| view | FULL | HYBRID | DYNAMIC |
|---|---|---|---|
| Boston ortho, 1200x1200 | 0.055 / 0.001 / 0.009 / **0.065** | 0.073 / 0.014 / 0.009 / **0.097** (3,095 boxes) | 0.095 / 0.025 / 0.009 / **0.129** (10,061 boxes) |
| street, 1920x1080, 150 u above the ground | 0.030 / 0.001 / 0.011 / **0.042** | 0.119 / 0.046 / 0.011 / **0.176** (2,564 boxes) | 0.119 / 0.056 / 0.011 / **0.187** (10,061 boxes) |
| whole map ortho, 1600x1600, grid stride 4 | 0.371 / 0.001 / 0.023 / **0.394** | 0.372 / 0.008 / 0.023 / **0.403** (2,383 boxes) | 0.422 / 0.106 / 0.037 / **0.564** (80,577 boxes) |

The live splat alone is the terrain column minus FULL's: +0.02 to +0.09 ms. The decals alone are the decals
column.

**Decal layers per ground pixel.** Rasterised = box faces drawn; inside = pixels that fall inside a box.
- Boston: hybrid 0.377 / 0.355 (max 12); dynamic 0.692 / 0.650 (max 12).
- Street: hybrid 4.781 / 0.957 (max 60); dynamic 5.744 / 0.960 (max 108). At eye level, most boxes the view
  looks through are rasterised for nothing. Culling boxes by screen area is the obvious saving (owed).

**Pictures against FULL.** Mean |diff| per pixel, 0..255, the title bar excluded:
- Boston: hybrid 6.10, dynamic 7.66.
- Street (sky included): hybrid 5.88, dynamic 5.90.
- Whole map: hybrid 0.80, dynamic **9.95**.
- Dynamic's whole map is darker (mean 68.4 against 77.4) and flat brown. The live splat carries none of the
  baked colour work (tint, vanilla fill of the ring outside the map, cover). The flat brown is my reading of the
  whole_side_by_side picture (below); the number is the 9.95.
- At eye level, FULL's 16 u level is a blur. The live splat is sharp. That is the case for HYBRID.

**Crossover.** Mean |baked hybrid level (VT.8, 64 u) - FULL (VT.2, 16 u)| per pixel, in 2048 u distance bins,
bins under 2000 pixels skipped. The crossover is the first bin from which every later bin is at or under
**4/255**. Its CSVs are pics/crossover_*.csv.
* **Street view (eye 150 u above the ground): 6,144 u.** Bins: 0 k 8.1, 2 k 8.7, 4 k 6.7, 6 k 3.8, 8 k 2.3,
  10 k 2.2, 12 k 1.6.
* Oblique view (eye 1,500 u above the ground, looking down on downtown): 34,816 u. The 64 u level is 3.3-5.4
  from FULL at every distance there, so it never gets far from FULL, and 4/255 is crossed late. At 6/255 it is met
  from the first bin (2,048 u).
* So the fade I used is 8,192..12,288 u: the street crossover rounded up to 2 cells, plus a 1-cell band. On the oblique view,
  DYNAMIC (live splat + decals) is 6-10/255 from FULL, and the live splat alone is 7-16/255 (no tint, erosion, fill or quadrant fade in the
  preview's splat). A hand-over band will therefore show a step of about that size wherever it sits. Making the
  live splat match the bake's colour work is owed before the band can be invisible.
* The mean |diff| is blind to blur: a 64 u level can score near FULL while looking softer. Stated, not fixed.

**Pictures** (60 px title bar each), in E:\Projects\NifskopeWWE-terrlive1\scratchpad\terrlive1_20260929\pics\:
boston_{full,hybrid,dynamic,side_by_side}.png, street_{...}.png, whole_{...}.png, ao_{boston,street}_{full_ao16,a_map32,c_none}.png.
One LTEX (000464c5) has no texture path and is drawn grey. The bake draws it grey too (same resolver).

## 8. Close-range AO study (look and bytes; no choice shipped)
The AO is the FULL mask sheet's B (sky + object AO), multiplied onto the lit colour. The reference is FULL with
AO at 16 u (lod 0 of VT.2). Two masks on the pictures:
- "under an object" = the reference is darker than FULL by more than half. In the game a building stands there.
- "open ground" = every other pixel that is not sky.

| | Boston: open-ground diff vs reference | street: open-ground diff vs reference | bytes, Boston box | bytes, whole map |
|---|---|---|---|---|
| (a) one-channel AO map at 32 u (BC4, 0.5 B/texel, full mips) | **4.13** | **6.74** | 1,572,864 | 402,653,184 |
| (b) AO inside the decals | not measurable | not measurable | +12.9 MB (BC4 on 19.3 M decal texels) | +39.9 MB |
| (c) nothing (SSAO only) | 25.53 | 36.48 | 0 | 0 |

- AO16 changes open ground by 26.0 (Boston) and 36.5 (street) on its own. Under objects it is black; that is
  51% of the Boston picture and 41% of the street one.
- (b) cannot carry world AO. One piece picture is shared by every placement of it (908 pieces for 10,061
  placements), so it can only hold the piece's own AO, not the ground's. Its bytes are shown for scale.
- (c) leaves the whole baked darkening to SSAO. SSAO cannot see LOD objects that are not drawn near the eye.
- (a) is the only one of the three that keeps the look: 4-7/255 from the 16 u reference, at 1/4 of the texels.
- Pictures: pics/ao_*.png. The choice is bungo's.

## 9. What is owed
* bungo's eye on the pictures, and the choice of option default (HYBRID is the code default, per the brief).
* Live-splat fidelity before the cross-fade can hide: tint, erosion, vanilla fill, quadrant cross-fade, and the
  sample average. The preview's splat has none of them, and that is the 7-16/255 gap (CSV column live_vs_full).
* Box culling by screen size for decals at eye level (5.7 rasterised layers per ground pixel for 0.96 inside).
* Whether dynamic should still write the .btr chunk texture sheets (it writes none today, because no pyramid runs).
* The AO choice (section 8).
* A like-for-like whole-map FULL VT-only time (the 5090.7 s here is a different recipe).
* FO4CS readers for .lodd/.lodg (FO4CS is built last, standing order).
* The TILING6 hook: `vec3 ltexFetch( float layer, vec2 uv, vec2 dx, vec2 dy )` in kTerrainFs
  (src/terrainpreview.cpp). Every live-splat texture read goes through it.

## 10. Builds, DeepSeek, skills (written 2026-09-29 08:13)
Builds, all `bash tools/ww_build.sh` under the turn, RC 0:
- rung = release/NifSkope.before_terrlive1.exe a25dd5da
- 1 = c742b7a3
- 2 = 8afeef50 (hybrid stages the levels below dim 8 and writes none of them; lodgen.cpp final here)
- 3 = 50283d4b (spec-relative paths, CSV failure counted)
- 4 = 5eff5b6c (the engine default land layer; every preview number above)
Builds 3 and 4 touched only terrainpreview.cpp, so build 2's FULL gate covers the bake code that ships.

DeepSeek: read the R2 terrain plan, the VT doc and the BTD format doc for the numbers-first section -- pro, 301 s, $0.1443
DeepSeek: mapped the code for the road/flat paint, the VT level writer, the .lodb writer, the CLI parser and panel, the terrain viewer and the render hook -- pro, 565 s, $0.1833

Skills:
- loaded: search-lean, nifskope-ww-worktree-build, deepseek-offload
- written:
  - E:\Projects\Claude\.claude\skills\ww-lodgen-option-byte-gate
  - E:\Projects\Claude\.claude\skills\ww-terrain-preview
- wished for: a turn-lock wrapper skill that ALWAYS releases (trap on exit); pv.sh does this for the preview
  only. Also a "compare two PNGs by region mask" helper; I wrote the AO split inline.

## 11. Rework (coordinator 08:19 and its correction): why the squares and the dirt outline (written 2026-09-29 08:30)
bungo's rulings:
- FULL is ditched.
- Outside our painted area, HYBRID shows vanilla's own dim-4 LOD diffuse.
- One blend joins ours and vanilla.

He circled two spots in whole_full.png: (a) square steps at the north edge (px 890-1060, 540-620), and (b) a
dark dirt outline along the west edge (px 540-600, 620-900).

Everything below was measured before any code change. Scripts are in `edge/`.

**Measured on the staged MERGE1 bake, VT.8 (64 u a texel), against vanilla's own
`Textures/Terrain/Commonwealth/Commonwealth.4.x.y.DDS`.**
- `edge/cells.py` classes the cells from the .lodl's quadrant slots: 4,086 cells have an LTEX slot, 32,778 have
  land and none, 0 have no land.
- `edge/why.py` gives luminance, 0..255, in bands of signed distance to the painted cells (negative = inside).

| band (u) | ours, north | vanilla V, north | ours, west | V, west | lit picture, west |
|---|---|---|---|---|---|
| -16384..-8192 | 80.1 | 80.4 | 76.9 | 79.6 | 82.3 |
| -2048..-1024 | 70.9 | 79.1 | 69.3 | 80.4 | 74.1 |
| -1024..0 | 68.1 | 79.3 | 67.5 | 80.3 | 72.4 |
| 0..+1024 | 67.1 | 80.7 | 67.1 | 80.8 | 72.1 |
| +1024..+2048 | 68.4 | 81.1 | 68.4 | 80.0 | 73.7 |
| +4096..+6144 | 76.6 | 79.0 | 77.3 | 80.6 | 84.3 |
| +8192..+12288 | 82.0 | 79.3 | 83.5 | 81.9 | 92.7 |

* **(b) The dirt outline is in our baked colour, not in vanilla, the light or the heights.**
  - Our colour dips to about 67 over roughly 4 km either side of the edge. Vanilla stays flat at 79-81 through it.
  - The lit picture follows the colour: lit / colour = 1.07 at the edge and 1.07-1.11 on both sides. So the
    light adds no step, which refutes a heightfield edge.
  - 67.0 is our engine-default land texture exactly. `edge/why_quad.py` shows the empty quadrants of edge cells
    read 66.9-67.2 at every depth.
  - Object AO would sit under objects, not on a constant-67 strip, so AO is refuted.
  - Two sources make the strip:
    1. The fill's "painted" is per CELL. The empty QUADRANTS of a painted edge cell keep our default ground and
       are never filled: 38,254 of the 61,478 texels in the first 1 km inside the west edge.
    2. The fill band starts at weight 0 on the painted cell's edge. The first 2 km outside the edge is still
       mostly our default ground: w = smoothstep(0, 8192, d) is 0.16 at 2048 u.
  - The painted quadrants at the fringe are also darker than deep inside (68.5 at 0-1 km, 76.6 deep inside,
    west). That is his LAND paint thinning out.
* **(a) The squares are the same default ground, cut on cell and quadrant lines.**
  - The fill band is per texel (lodgenVtFillDistance), so it is not the whole-cell band. Its start, and the
    untouched empty quadrants, follow the 4096 u cells and 2048 u quadrants.
  - Measured on the render: the colour jump across the painted-cell boundary is 12.4 with the class map where it
    is. Shifted by ±1024 u it is 8.5-8.8, and any 2 px pair is 9.2. So the step sits on the cell grid within one
    pixel (492 u), in both x and y.
* **The tone match pushes vanilla hard on saturation, lightly on brightness.**
  - T(V) is 2.5-3.4 lum over V.
  - Its saturation is x1.9 (mean |rgb - lum| 6.5 -> 12.3).
  - The gain of 0.62 flattens vanilla's own relief by 38%.
  - Deep inside, our colour is within -2.7..+0.4 of UNTOUCHED vanilla (both regions). So untouched vanilla needs
    no tone match to meet us.

## 12. The fix: law 2, and FULL ditched (written 08:48, build 5 at 08:42)

- **Offline model first** (edge/model.py, staged VT.8 as ours, vanilla sheets as V): colour = mix(V, ours, w),
  w = smoothstep(0, B, distance INSIDE the painted quadrants to the nearest unpainted quadrant).
  West box, luminance by signed distance (-16k..+4k u):
  - V: 79.0 ... 80.9 80.0 80.2 80.3; old ours: 76.6 ... 68.7 67.0 67.5 69.2 76.1 (the outline).
  - B = 4096: worst dip 0.21, 6.6% of our painted texels blended; B = 8192: 0.24, 12.6%; B = 12288: 0.20, 18.3%.
  - North box: the same, dips 0.25-0.37.
  - Chosen: **B = 8192 u (2 cells)**, the old census band; 4096 is as flat but the ramp is twice as steep
    (north: ours is 89 against vanilla's 75 deep inside, a 14-point ramp).
- **Code** (lodgen.cpp, lodgenVtFillTile): painted per QUADRANT (the .lodl's own grain); outside the painted
  quadrants the texel is vanilla's diffuse UNTOUCHED (no tone match); inside, ours rises over 8192 u.
  The tone fit still runs for its census line only. Census line gains "law=2 band=8192 paintedQuads= texelsVanilla=".
  The band is one constant, LODGEN_VT_FILL_BAND in lodgen.h, read by the bake and the preview.
- **Preview** (terrainpreview.cpp): the same law live. It builds the painted-quadrant weight map from the .lodl
  and reads vanilla's dim-4 diffuse live from the game's sheets (the "blend to vanilla" line prints band,
  sheets, bytes). HYBRID near and DYNAMIC = mix(vanilla, live splat, w). The crossover counts only ground that
  is wholly ours (w = 1). Spec key "blend": false turns it off.
- **DYNAMIC's outside (my call, logged):** vanilla's own dim-4 diffuse, read live, the same sheets as the bake's
  fill. Its bytes are counted (game files, not shipped).
- **Not done:** replacing HYBRID's all-vanilla far tiles by one-value records that tell a reader "draw vanilla
  here". That needs a reader contract (a format meaning), so it stays out; the saving is estimated below.
- **FULL ditched:** removed from --terrain-option (the parser says "takes hybrid or dynamic"), the help text and the
  panel's Terrain row (the combo now finds its item by data, not index). The enum keeps Hybrid = 1, Dynamic = 2.
  The byte-gate skill is marked HISTORICAL.
- **Gate written before the bake, proven red on the old file** (edge/gate_law2.py; outside <= 2.0, dip <= 1.0):
  law-1 stage VT.8: north outside 6.61, dip 10.55; west outside 6.45, dip 9.53 -> FAIL.

## 13. The whole-map HYBRID rebake under law 2, and its gate (written 09:07)

- Build 5 (run_new), chain4.sh: whole map, VT only, the chain-3 recipe, --terrain-option hybrid. **1,332 s**
  (law 1 on build 4: 1,337 s; the same within noise). rc 0.
  - Census: law=2 band=8192 paintedQuads=15893 (the .lodl says 15,891: 2 quadrants whose only layer is a NULL
    LTEX count as painted in the bake and not in the .lodl; logged, not chased) texelsVanilla=142,883,264
    texelsNoVanilla=405,248 vanillaChunksMissing=196 (sea chunks with no vanilla sheet keep ours).
- **Gate (edge/gate_law2.py) PASS**, against the law-1 stage FAIL:
  | box | outside mean abs lum(ours) - lum(vanilla) | outline dip | first km inside / outside (vanilla) |
  |---|---|---|---|
  | north steps, law 1 | 6.61 | 10.55 | 74.8 / 67.2 (76.2 / 75.4) |
  | north steps, law 2 | **1.39** | **0.00** | 75.8 / 74.9 (76.2 / 75.4) |
  | west outline, law 1 | 6.45 | 9.53 | 68.7 / 67.0 (81.1 / 81.0) |
  | west outline, law 2 | **1.48** | **0.27** | 80.5 / 80.5 (81.1 / 81.0) |
  - The look difference against vanilla's own diffuse outside: mean 1.4-1.5 luminance (BC1 and resampling);
    the profile outside sits 0.4-0.5 under vanilla's everywhere, the same offset on both sides of the edge.
  - North: ours is 89 deep inside against vanilla's 75; law 2 ramps 89 -> 76 over the 8 km band, no sink.
- **Bytes, whole map, HYBRID:** .lodt levels 749,494,960 (VT.8 572.7 MB, VT.16 134.6 MB, VT.32 42.2 MB; the same
  as law 1, the tiles are fixed-size) + decals .lodd 161,076,176 + .lodg 3,261,484 = **913.8 MB**.
  - 577.4 MB (77%) of the pyramid sits on tiles with no painted quadrant (edge/vanilla_tiles.py). Their colour is
    vanilla's now, so a reader could draw it from vanilla's sheets instead; that is a reader contract (a
    format meaning), so it is not done here. Owed as bungo's call.

## 14. Pictures, GPU, crossover, live-vs-baked parity, step check (written 09:29)

**Pictures** (one per view, 60 px title bar), E:\Projects\NifskopeWWE-terrlive1\scratchpad\terrlive1_20260929\pics2\:
- bungo's circled spots, before and after: close_north_steps_{before,after}_{baked,dynamic}.png,
  close_west_outline_{before,after}_{baked,dynamic}.png. "before baked" = the MERGE1 stage (law 1), "after baked"
  = the law-2 whole-map bake, "dynamic" = the live ground on build 4 (before) and build 5 (after).
- whole_{hybrid,dynamic}.png, boston_{hybrid,dynamic}.png, street_{hybrid,dynamic}.png.

**GPU ms (terrain / decals / lighting / total), median of 30 frames, RTX 5070 Ti, build 5 with the live blend:**
| view | HYBRID | DYNAMIC |
|---|---|---|
| Boston ortho 1200x1200 | 0.083 / 0.014 / 0.009 / **0.107** (3,095 boxes) | 0.112 / 0.025 / 0.009 / **0.146** (10,061) |
| street 1920x1080 | 0.127 / 0.046 / 0.011 / **0.184** (2,564) | 0.128 / 0.056 / 0.011 / **0.195** (10,061) |
| whole map 1600x1600 | 0.374 / 0.008 / 0.023 / **0.405** (2,383) | 0.404 / 0.105 / 0.025 / **0.534** (80,577) |
- Against build 4 (section 7) the live vanilla read costs +0.01 ms (Boston, street) and nothing measurable on the
  whole map.

**DYNAMIC's bytes:** decals (the same .lodd/.lodg as HYBRID, 164.3 MB) + vanilla's dim-4 sheets read live from the
game (not shipped): whole map 2,304 sheets, 805,662,720 bytes, 0 missing (read at mip 2 for the 8192 mosaic);
Boston/street 9 sheets 3.1 MB; north close-up 54 sheets 18.9 MB; west 72 sheets 25.2 MB.
Building the whole-map blend (weight map 3072x3072 + 2,304 sheet decodes) took 15.8 s at view load.

**Live vs baked now agree on the outside and in the band** (edge/parity.py, per-pixel |lum baked - lum live|
on the close-ups, split by quadrant class):
| spot | vanilla outside | painted, in the 8 km band | painted, deeper |
|---|---|---|---|
| north steps | 1.03 | 2.01 | 3.67 |
| west outline | 0.95 | 3.05 | 5.25 |
- The deeper difference is not the blend: live after vs live before (build 4, no blend) is 0.03 (north) and 1.07
  (west) on deep painted ground. It is the live splat being 3-5 lum darker than the baked levels (the colour work
  the brief says to leave alone). The dark blocky patches in close_west_outline_after_dynamic.png are that: the
  six darkest quadrants are all painted, 4-24 km inside, and live 17-21 lum under baked; they are the same in the
  before picture.
- Whole map: DYNAMIC is 1.84 from HYBRID (mean |diff|, lum 73.7 vs 74.1); in phase 1 it was 9.95 from FULL
  (lum 70.1): the vanilla outside is what DYNAMIC was missing.

**Step check (edge/step_align.py on the whole-map pictures, the colour jump across the painted-cell edge, shifted
class map):**
| box (px) | picture | any 2 px pair | shift 0 | shift -1024 u (x / y) | shift +1024 u (x / y) |
|---|---|---|---|---|---|
| north 890-1060 x 540-620 | before (pics/whole_full.png) | 10.09 | **16.24** | 9.70 / 13.21 | 10.37 / 12.47 |
| north | after HYBRID | 10.16 | 10.36 | 9.76 / 9.58 | 9.74 / 11.30 |
| north | after DYNAMIC | 11.12 | 10.24 | 9.66 / 9.30 | 9.56 / 11.18 |
| west 540-600 x 620-900 | before | 9.96 | 6.94 | 6.64 / 7.07 | 7.08 / 6.55 |
| west | after HYBRID | 12.05 | 13.17 | 12.36 / 12.56 | 12.13 / 12.87 |
- North: the shift-0 peak (16.2 against ~10) is gone (10.4 against 9.6-11.3). West never had a grid-cut step (its
  fault was the dark band, which the dip gate measures); after, it carries vanilla's own relief (pair 12.1).

**Crossover (baked 64 u level vs the 16 u reference, on ground wholly ours, w = 1; 4/255, 2048 u bins, bins
under 2000 px skipped):**
- Oblique (eye 1,500 u): met from **30,720 u** (phase 1, all ground: 34,816 u). Bins 2-14 k: 3.6-4.2; 16-28 k:
  4.2-5.3; 30 k on: 2.0-3.9.
- Street (eye 150 u): **not measurable**. Only the 0-4 km bins have 2000+ px of wholly-ours ground (8.1, 8.7,
  5.2); beyond 6 km the street view sees 2-1,443 px a bin of it (the rest is in the band or vanilla).
- So on wholly-ours ground the 64 u level stays 3.5-5 from the 16 u one out to ~30 km. The preview fade stays
  8,192..12,288 u (unchanged); a hand-over there shows a step of about 4-5/255.

## 15. Decal read-back on the law-2 whole-map bake (written 09:37)
- `lodgen --decal-check whole_hybrid_law2/mod/FO4CSLOD/Commonwealth` (dc.sh, under the turn, released 09:37:19), rc 0:
  - .lodd: 2,818 pieces (roads 642, flat 1,627, flat-over 549), 161,076,176 bytes, 2,774 with a normal stamp,
    **0 CRC mismatches**, 0 picture decode failures.
  - .lodg: 80,577 placements, 2,857 cells used (busiest 311), 3,261,484 bytes; bad piece/class/scale/rotation,
    draw-order breaks, bad or unsorted index all **0**.
  - pair matched (names table CRC f34c4cd6 on both).
- Branch terrlive1-20260929: rework commit a843685b pushed (not merged); this section follows it.

## 16. Optional rule paint outside our ground (bungo: "Yes, option 2, make it optional in the baking settings")

Design calls, logged 2026-09-29 10:00 before any code:

- **Switch.** CLI `--outside-paint vanilla|rule` (default vanilla = today's law 2, byte for byte). `rule`
  needs `--vt-fill-vanilla` (the rule reads vanilla's diffuse to choose). Panel row "Outside paint"
  (Vanilla / Rule), label and control only. The .lodb gains an `outside rule` row only when on, so an
  OFF .lodb is unchanged.
- **Where the rule applies.** Colour only. The unpainted quadrants (w = 0 in law 2) take the rule colour R
  instead of vanilla V; the 8,192 u band inside our ground blends ours into R (c = R + (ours - R) w).
  Normals, mask, cover, height outside stay as today.
- **Texture set.** The LTEX records the worldspace's own LAND uses (base textures + layers, counted by area),
  keeping those with at least 0.5 % of the painted area and a loadable diffuse. Each one's colour for
  matching = its 1x1 mip (the repeat average) times the land grade.
- **The rule, per 512 u sample** (8 per cell): V = vanilla's mean over the sample's 16 x 16 vanilla texels;
  height and slope from the LAND heights (vanilla's LOD heights where a cell has no LAND). A prior
  P(texture | slope bin, height bin) is counted over OUR painted quadrants (so rock goes on steep ground,
  as Bethesda's own layers put it). The 8 most likely textures for the sample's bin are tried alone and in
  pairs; a pair's mix weight is the least-squares fit to V, clamped to 0..1; score = colour error (8-bit
  units) - 2 x log prior. Lowest score wins.
- **Layers per texel.** 2 per sample (A, B and A's weight); a texel reads its 4 surrounding samples
  bilinearly, so up to 8 textures meet at one texel, usually 2-4 (duplicates merge).
- **Storage.** A small id/weight map in a NEW file `<ws>.lodr` next to the VT files: 64-byte header
  ("LODR", version, cell extent, samples per cell, palette count, band, sizes, CRC32), the palette as
  LTEX form ids, then zlib-compressed planes A, B, W (u8, 255 = no sample). Not computed live from
  height + vanilla colour, because the choice needs the whole-map texture statistics and vanilla's
  sheets; the renderer should only read ids and weights, exactly like the .lodl's splat. No existing
  format changes.
- **Hybrid's far baked levels CARRY the rule paint.** They are box-filtered from the finest level, which the
  fill already rewrites, so they cost no extra bytes and the near (live) and far (baked) ground show the
  same textures at the switch-over distance; stopping them at the painted edge would put a vanilla/rule
  seam exactly at the fade.
- **DYNAMIC** writes only the .lodr (it bakes no VT colour); the live draw reads it.
- **TILING6 hook.** The live rule ground calls ltexFetch like the live splat does (one splat function,
  fed either grid).
- **Edge gate with the switch on.** The skill's clause "outside within 2.0 of vanilla" measures "vanilla
  untouched", which the switch gives up on purpose. Kept: the dip clause (<= 1.0); added: the step
  across the painted edge (first km inside vs first km outside, <= 2.0), and the drift outside is
  reported, not gated. The law-1 stage must still fail the adapted gate.

### 16.1 Build 6 (written 10:19)
- All sources compiled first time (build 6, 10:16-10:17, BUILD-RC=0, exe df981d25). Copied to run_rule/.
- Chain 6 started: whole-map HYBRID bake with the switch OFF (the byte gate against the law-2 head bake),
  then the same bake with `--outside-paint rule`.

### 16.2 Switch OFF = the head's bytes (written 11:23)
- The turn was held by IDENT2 from 10:18 to 11:01; chain 6 ran from 11:01.
- whole_off (build 6, switch off, the head's whole-map HYBRID recipe): rc 0, 1,299 s.
- `edge/gate_off.py whole_hybrid_law2/mod whole_off/mod` -> all 7 files SAME by sha1 (VT.8/16/32, .lodm, flat
  report with its "# Override file:" row masked, .lodd, .lodg), no new file: **GREEN** (gate_off_green.txt).
  The red half of the proof is run on the rule bake below.

### 16.3 The whole-map bake with the switch on (written 11:50)
- whole_rule (`--outside-paint rule`, else the same recipe): rc 0, **1,513 s against 1,299 s off: +214 s (+16%)**.
  Of that, building the rule map is 67.6 s (buildMs); the rest is the rule colour per outside texel in the tiles.
- **Bytes added, whole map: 2,687,771 (the .lodr, 2.6 MB).** The VT levels keep their sizes (stored uncompressed,
  same tiles); only their colour outside changes. Nothing else changes (.lodm, .lodd, .lodg, flat report SAME).
- The rule census: grid 1536 x 1536 at 512 u, all 2,359,296 samples set (no vanilla or height gaps), 254,288 of
  them on painted ground (the statistics), 1,941,958 pairs + 417,338 singles; mean colour error to vanilla at the
  samples 7.24 (8-bit RGB), luminance 2.96. Palette 36 textures (64 dropped under the 0.5% share, 1 with no
  loadable texture); 10 of the 36 end up unused. The default land set (no LTEX, ffffffff) is one of them (10.4%).
- **Red half of the OFF gate:** `gate_off.py whole_hybrid_law2/mod whole_rule/mod --expect-red` -> VT.8/16/32 differ,
  a new .lodr: RED as expected (gate_off_red.txt). So the gate sees the switch.
- **Read-back:** `--rule-check` on the .lodr: OK, cells -96,-96 + 192x192, grid 1536x1536 at 512 u, samples 2,359,296
  (pairs 1,941,958), palette 36 (unused 10), band 8192 u. Damaged copies are refused: one flipped bit -> "CRC
  mismatch", a wrong magic -> "not a .lodr", cut at 1,000 bytes -> "file is 1000 bytes, the header says 2687771".
- **Edge gate with the switch on** (`gate_law2.py VT.8 --rule`, now exits 1 on FAIL):
  whole_rule north_steps dip 0.00, step 0.78 -> PASS; west_outline dip 0.54, step 0.10 -> PASS.
  Sabotage: the law-1 bake under the same clauses: dips 9.47 / 8.69, steps 7.38 / 1.62 -> FAIL (exit 1).
- **Drift from vanilla outside** (`edge/drift.py` OFF vs ON; OFF's outside is vanilla's dim-4 diffuse), mean |d lum|:
  | zone | VT.32 (256 u) | VT.8 (64 u) | signed (VT.8) | p95 (VT.8) |
  |---|---|---|---|---|
  | band 0-8 km outside | 6.20 | 7.20 | +1.96 | 20.3 |
  | near 8-32 km | 6.65 | 7.45 | +1.62 | 22.3 |
  | far > 32 km | 4.34 | 4.97 | -0.78 | 20.6 |
  | all outside | 4.50 | 5.14 | -0.60 | 20.8 |
  Inside our painted ground it moves 0.58-0.66 on the average: that is the inner 8 km band, which now blends to
  the rule paint instead of vanilla. In the gate boxes: north steps 3.35, west outline 7.02 (the rule ground reads
  ~4.5 lum darker than vanilla there: 76.6 against 81.0 in the first km outside).

### 16.4 DYNAMIC and the .lodb row (written 12:01)
- DYNAMIC with the switch (`--terrain-option dynamic --outside-paint rule`, VT-only path): rc 0, 381 s; it writes
  the .lodr (2,687,771 bytes, sha1 7626cd4f, **byte-identical to HYBRID's**) beside the decals and no VT level. The
  rule step is 38 s of it (buildMs 38,155; HYBRID's was 67.6 s while the tile workers shared the CPU).
  So DYNAMIC adds 2.6 MB and ~38 s.
- Small region bake, cells -32..-25 x 8..15 on the west edge (bake_small.sh), off and on:
  - on: the .lodb gains `switch --outside-paint rule`, the row `outside	rule` after `terrain	hybrid`, and
    `product	Commonwealth.lodr	<sha1>`; off has none of the three. The rest of the diff is time stamps, run paths,
    the VT.8 product hash and the stage times.
  - The region's .lodr reads back: cells -32,8 + 8x8, grid 64x64, 4,096 samples (3,802 pairs), palette 10, 6,536 bytes.
    A region bake takes its texture statistics from the region's own painted ground (10 textures here).

### 16.5 GPU, pictures and the look in view (written 12:04)
Preview with run_rule (build 6), specs from make_spec_rule.py: spec_rule_gpu.json (Boston, street: the section-14
cameras) and spec_rule_pics.json (whole, low hills view, the two close-ups). OFF views read the OFF bake's levels
with "rule": false; ON views read the rule bake's levels and its .lodr. Total GPU ms, median of 30 frames:

| view | HYBRID off | HYBRID on | DYNAMIC off | DYNAMIC on |
|---|---|---|---|---|
| Boston (ortho, 1200 px) | 0.106 | 0.118 | 0.145 | 0.166 |
| street (eye level) | 0.183 | 0.190 | 0.194 | 0.201 |
| whole map (1600 px) | 0.409 | 0.396 | 0.449 | 0.513 |
| low view over the outside hills | 0.187 | 0.243 | 0.198 | 0.306 |

- Cost: up to +0.06 ms on the whole map live (DYNAMIC) and +0.06 / +0.11 ms on the hills view, where most of the
  screen is outside ground drawn live. HYBRID's whole map is flat (0.409 -> 0.396, noise): its far levels already
  hold the rule colour, so nothing extra is fetched.
- Pictures (60 px title bars; left panel HYBRID baked, right DYNAMIC live): pics3/whole_off.png, whole_on.png,
  north_steps_on.png, west_outline_on.png, hills_off.png, hills_on.png (+ gpu_boston_*, gpu_street_*).
- Look in view (`edge/pic_drift.py` off vs on, same camera):
  - whole map: mean |d lum| 3.75 (HYBRID) / 3.37 (DYNAMIC); mean colour off (80, 74, 64) -> on (80, 74, 61) baked:
    the rule ground is a touch more olive (blue -3 to -4).
  - low hills view: far half |d lum| 7.62 baked / 12.50 live, near half 0.28 / 1.74. What changes: vanilla's grey
    rock on the far slopes becomes brown and olive ground; the rule picks rock less often than Bethesda painted it.
- Seen in the close-ups, the same off and on: the quadrant-shaped edge of the yellow ground at the north steps
  (the law-2 picture of section 14 shows it too). The switch does not add or remove it.

### 16.6 Docs, skill, commit (written 12:06)
- docs/LODGEN_TERRAIN_VT.md §2.6c (the switch, texture set, rule, layers, .lodr layout, HYBRID/DYNAMIC, measures).
- Skill ww-terrain-edge-measure: section 7 (the --rule gate clause, drift.py, gate_off.py, --rule-check), in
  E:/Projects/Claude/.claude/skills and E:/Tools/AISkills (identical).
- New scratch tools: edge/gate_off.py, edge/drift.py, edge/pic_drift.py, make_spec_rule.py, chain6.sh, chain7.sh,
  bake_small.sh, build.sh; the patch scripts and rule_impl.cpp.txt kept as the record of the source edits.
- DELIVERABLE_TEXT.md: HANDOFF / WW_CHANGES / MISTAKES lines for the rule paint.
- Pictures stay local (pics3/*.png, not committed, like pics2).
