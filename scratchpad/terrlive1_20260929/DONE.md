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
