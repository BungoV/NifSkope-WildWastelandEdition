# Lane ROADS2 -- feathered, blended roads (no seams); raised highways excluded; the tree filename clause scoped

## Header
- Tree: `E:\Projects\NifskopeWildWastelandEdition`, branch `main`. Nothing is committed. Exe at launch: `release/NifSkope.exe` 2026-09-11 19:08:42, 21,435,904 B (RESUME3's DONE exe). Rung ONCE: `release/NifSkope.before_roads2.exe`. Markers `scratchpad/roads2_20260911/BUILDING` / `DONE`. Every timestamp from `date +%H:%M` in the same step. One NifSkope instance ever; game check before the link; bungo's window renamed aside. Region bakes only, own out-dir.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block -- the CLOCK CORRECTION, the ROADS1 block, the FLAGSCAN1 paragraph, the 16:3x RULINGS paragraph (this lane's law), the RESUME3 block (the tiling default you now bake with); `scratchpad/lane_roads1_report.md` IN FULL (the rasteriser: order in the composite, the mesh test, `rasterlib.py`, `matinfo.py`, the road-presence metric and its floor/ceiling, the (-20,20) tile); `scratchpad/lane_flagscan1_report.md` sections 2 and 5 (the raised-road bit-15 pattern, the (-8,8) elevated-vs-flat scores, the four tree false positives); `scratchpad/pic_chunk_20260911/NOTES.md` (the seam bungo saw); `docs/LODGEN_TERRAIN_VT.md` section 1a (ROADS1's contract text); `src/lodgen.cpp` `lodgenIsRoadModel()` / `lodgenIsTreeModel()` and the road rasteriser; `MISTAKES.md` root from 2026-09-11.
- Skills: `nifskope-ww-lodgen`, `nifskope-ww-vanilla-compare`, `ww-control-calibration`, `ww-texel-picture`, `ww-sheet-diff`, `fo4cs-census-field`, `ww-anchored-hookup`, `nifskope-ww-build-verify`, `ww-contract-provenance`, `ww-spec-gate-audit` (before reproducing ROADS1's or FLAGSCAN1's numbers).

## bungo's words (verbatim, HANDOFF.md 16:3x)
"look at the roads, there is a visible seam while vanilla doesn't have it" -- over chunk (-20,20) ours vs vanilla.

## The work
1. **Measure the seam first** (section 1 before code): on chunk (-20,20), the colour gradient magnitude along road-piece boundaries (project each road placement's footprint; boundaries = texels where the topmost piece changes) in OUR sheet vs VANILLA's at the same texels; control = the same metric on boundaries where both pieces are solid asphalt with no feather (should be ~equal), and a displaced-boundary floor. Print the numbers; that is the gate's baseline.
2. **Read what the road meshes actually carry**: for the 71 road models, per shape: vertex ALPHA present and its range (edges/end caps ~0 -> 1?), the material's alpha BLEND flag vs alpha TEST (BGSM/BGEM fields via `matinfo.py`), `bDecal`, two-sided, draw order hints (`SortOrder`? the NIF's alpha property flags). Table it. State the compositing rule vanilla's result implies (ROADS1 measured the colour = material x grading; measure whether feathered edges in vanilla's sheet follow the vertex-alpha ramp: sample vanilla along a road edge vs the mesh's alpha at those texels).
3. **The rasteriser change**: composite instead of overwrite -- for each texel, pieces in a stated order (by height, then by the material's decal/blend flag so blended joints go on top), `dst = lerp(dst, src, srcAlpha)` with `srcAlpha = materialOpacity x vertexAlpha` for alpha-blended shapes, 1 for opaque, alpha-test as before; the ground under a feathered edge shows through. Behind the existing `--roads` switch; `--road-composite max-z|blend` with `max-z` = ROADS1's exact bytes (the way back), default `blend`.
4. **Raised highways excluded** (FLAGSCAN1 lead 1): `lodgenIsRoadModel()` refuses `Landscape\Roads\HighwayOverpass\*` and `...\Bridge\*` (and any base carrying Has Distant LOD = the raised set, state which rule you use and why); gate = on chunk (-8,8) the elevated family no longer paints (its projected texels in our sheet equal the no-roads bake within tolerance) while flat road still clears its floor (0.716 vs 0.629 baseline).
5. **Tree filename clause scoped** (FLAGSCAN1 lead 2): `startsWith("tree")` applies only under `Landscape\` (or `Landscape\Trees\`); the four SetDressing false positives (rope pile, noose branch, two tree swings) drop out; gate = candidate list on Sanctuary: the four absent, the 19 trees present (LODUI1's L2 numbers).
6. **Sidewalks**: measure on a tile with >= 5,000 sidewalk texels whether vanilla bakes them (ROADS1 left it untested at 187 texels); keep or drop the folder with the number.
7. Build, then the chain: `lodgen_roads.sh` (11/0 + your new checks), `lodgen_terrain.sh`, `lodgen_terrain_vt.sh` (41/1 V9b), `lodgen_ground_cover.sh`, `lodgen_terrain_pbrm.sh`, `lodgen_native.sh`, `lodgen_panel_run.sh`, `lod_generation.sh`, `ui_align.sh`, `water_ui.sh` at their baselines from RESUME3's block; `--road-composite max-z` byte-identical to the rung (cmp every file); `--no-roads` byte-identical.
8. Pictures: `cmp_seam.png` (vanilla | ours max-z | ours blend, 4x over the joint bungo pointed at, gradient numbers burned in), `cmp_highway.png` ((-8,8) before/after), `cmp_sanctuary_road_v2.png` (the ROADS1 picture re-taken on the new exe, same crop). Described before cited.
9. Documents: `scratchpad/roads2_20260911/WW_CHANGES_ENTRY.md`, `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md` (entries start with `## `); report `scratchpad/lane_roads2_report.md`; contract section 1a amended with provenance.

## Gates
- S1 seam metric measured before code; after: ours within a stated factor of vanilla on feathered boundaries; solid-boundary control unchanged; floor red.
- S2 `--road-composite max-z` == rung bytes; `--no-roads` == rung bytes.
- S3 raised highways no longer painted; flat road still clears its floor.
- S4 the four tree false positives gone; 19 trees intact.
- S5 sidewalks decided by a number.
- S6 exe newer than every changed file; drivers rebuilt; rung == launch bytes; no NifSkope left running.

## Rules
- One build (+ counted relinks). No tiling changes (RESUME3's). No tone/grading changes (GRADE1's). Never his installed files. Never `git stash`, never commit. Plain language.

## Report
`scratchpad/lane_roads2_report.md`, incremental (PENDING.md first past half context): `## 0. Pre-registered gates`, `## 1. The seam, measured`, `## 2. What the road meshes carry`, `## 3. The composite and the exclusions`, `## 4. Build and gates`, `## 5. Pictures`, `## 6. Owed / red / bungo's calls`, `## 7. Mistakes`, `## 8. Finished-work skill review`.
