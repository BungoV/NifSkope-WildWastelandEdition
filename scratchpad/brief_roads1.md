# Lane ROADS1 -- roads and decals baked into far terrain the way vanilla does it

## Header
- Tree: `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, working tree. Nothing is committed (CONSTITUTION 8). Exe at launch: `release/NifSkope.exe` 2026-09-11 11:27:12, 21,137,920 B (TERRAIN-R: the `.lodt` mask + emissive sheets, `family: pbr`; TERRAIN-R has LANDED, so its mask-channel law is the one you write into -- read its block in HANDOFF.md and the rewritten `docs/LODGEN_TERRAIN_VT.md`). Its picture `scratchpad/terrain_r_20260911/images/ours_vs_vanilla_tile.png` shows the road network vanilla's sheet carries and ours lacks: that is your target. Rollback rung, ONCE: `release/NifSkope.before_roads1.exe`.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block -- RESUME, the newest lane block, the GAP REVIEW ruling (1) "We do the same with roads and decals as vanilla"; `docs/LODGEN_PARITY.md` (line ~47: "Terrain texture bakes do not rasterize road meshes; vanilla's bakes do (the Sanctuary loop road is plainly visible in vanilla's tile ...)" -- and everything else that page measured about vanilla's graded bakes); `docs/LODGEN_TERRAIN_VT.md` (the sheets you write into: colour, msn normal, mask -- read TERRAIN-R's block in the handoff for the mask's channel law as it lands, if TERRAIN-R has landed; if not, the data sheet as it stands); `src/lodgen.cpp` `lodgenBakeTerrainTextures` (~5466) and `lodgenBakeTerrainVt` (~7076), and the object loader `lodgenLoadModel` + the `road`/`concrete` path test at ~298 (what already recognises a road piece and why); `docs/MISTAKES.md` lodgen sections; `MISTAKES.md` root from 2026-09-11 on.
- Skills you MUST invoke: `nifskope-ww-lodgen`, `nifskope-ww-vanilla-compare` (the deliverable IS vanilla's tile beside ours), `ww-control-calibration` (a road-presence metric needs a floor: a tile with no road, and a ceiling: vanilla's own tile), `ww-texel-picture`, `fo4cs-census-field`, `ww-anchored-hookup`, `nifskope-ww-build-verify`, `ww-spec-gate-audit` (before reproducing any PARITY number).
- Build rules: ONE build (+ counted relinks). Markers `scratchpad/roads1_20260911/BUILDING` / `DONE`. Game check before the link. bungo's window renamed aside. `ls scratchpad/*/BUILDING` empty at launch. Small regions only (Sanctuary 9-chunk -- it has the loop road), own out-dir, never his installed files.

## bungo's ruling (verbatim, HANDOFF.md 10:0x)
"We do the same with roads and decals as vanilla" -> road and decal meshes are rasterised into the far-terrain sheets at bake, so far roads are visible under our bake as they are under vanilla's.

## The work
1. **Measure vanilla first** (`nifskope-ww-vanilla-compare` + PARITY): on the Sanctuary tile(s), which placed objects vanilla's far sheet visibly carries: roads (which model paths / which record types: STAT road pieces, the `*\Roads\*` folder, decal meshes with `BSDecal`-style shader flags or `Decal` in the material), and whether vanilla bakes them into the COLOUR only or also into the NORMAL (measure on vanilla's `_msn`: does the road's edge show?). Report the rule you infer from the measurement, with the tiles and pixel windows that show it.
2. **The rasteriser.** In the terrain bake, after the splat and VCLR and before the grass tint (state the order and why -- roads sit on the ground, grass grows beside them), project the region's road/decal meshes top-down onto the tile's texel grid: for each texel, the topmost such triangle covering it supplies colour (from its material's diffuse, through the same resolver TERRAIN-R shares, with the mesh's UVs and vertex colour), and, if item 1 says vanilla does, a normal/mask contribution. Alpha-tested decals honour their alpha. The set of meshes = placed refs whose base passes the road/decal test (state the test: folder + record type + material flags -- never a bare substring: MISTAKES.md has the "sTREEt" lesson).
3. **Module and fallback** (CONSTITUTION 10): a `--roads` switch (on by default under both targets, because vanilla does it) and `--no-roads` = byte-identical to the rung's bake (gate).
4. **Census**: roads rasterised (meshes, triangles, texels touched) per chunk; decals likewise; refusals by name (a road mesh that failed to load, a decal with no resolvable material).
5. **Gate**: road-presence metric on the Sanctuary loop road tile: the fraction of texels along vanilla's road centreline (extracted from vanilla's own sheet by its colour, as a mask) that our sheet paints within a colour tolerance of vanilla's; floor = the rung's bake (no road) reads near 0; ceiling = vanilla vs itself reads 1. Also the whole-tile colour error vs vanilla before/after (PARITY's own metric, `ww-spec-gate-audit` on its number first).
6. **Region bake** before/after; stock path with `--no-roads` byte-identical (cmp every file).
7. **Contract**: `docs/LODGEN_TERRAIN_VT.md` gains a "Roads and decals" section (the order in the bake, the mesh test, the channels touched) with provenance; `docs/LODGEN_PARITY.md`'s line ~47 becomes the measured after-state.
8. Build, then the chain: `lodgen_terrain.sh`, `lodgen_terrain_vt.sh`, `lodgen_ground_cover.sh`, the parity spell if one exists, `lodl_open.sh`, `ui_align.sh`, `water_ui.sh` at their baselines (read them from the newest lane block). Every count that moves is explained by name.
9. Pictures (`nifskope-ww-vanilla-compare`, `ww-texel-picture`): `cmp_sanctuary_road.png` = vanilla's shipped far sheet | ours before | ours after, same tile, same crop, labels burned in; `road_mask_and_metric.png` = the centreline mask over the three; a render-hook top view of the region with the pyramid on. Described before cited.
10. Documents: `scratchpad/roads1_20260911/WW_CHANGES_ENTRY.md`, `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md` (entries start with `## `); report `scratchpad/lane_roads1_report.md`.

## Gates (this lane)
- R1 vanilla measured first: the inferred rule with its evidence windows, BEFORE the rasteriser is written (report section 1 timestamped before section 2's code).
- R2 road-presence metric: after >= a stated threshold derived from the ceiling, floor near 0, both printed.
- R3 `--no-roads` byte-identical to the rung.
- R4 census words written and moving (a region with no roads reads 0).
- R5 exe newer than every changed file; drivers rebuilt; rung == launch bytes.
- R6 no NifSkope left running; game down at every launch.

## Rules
- One build (+ counted relinks). No mask-channel law changes (TERRAIN-R's), no native object files, no panel rows. If TERRAIN-R is still alive when you launch, you were launched by mistake: stop and write PENDING.md.
- Never a bare substring test for "road". Small regions only. Never his installed files. Never `git stash`, never commit. Plain language.

## Report
`scratchpad/lane_roads1_report.md`, incremental (PENDING.md first if past half your context):
- `## 0. Pre-registered gates`
- `## 1. What vanilla bakes` (the measurement, windows, the inferred rule)
- `## 2. The rasteriser` (order, mesh test, channels)
- `## 3. Build and gates` (mtime table; gate table; stock cmp)
- `## 4. Pictures`
- `## 5. Owed / red / bungo's calls`
- `## 6. Mistakes`
- `## 7. Finished-work skill review`
