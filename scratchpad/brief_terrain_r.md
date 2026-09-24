# Lane TERRAIN-R -- the terrain pyramid takes the object texture family: mask (roughness / metallic / AO / cover), emissive, a real family word; and the ring-0 weights-blend-vs-pyramid colour gate

## Header
- Tree: `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, working tree. Nothing is committed (CONSTITUTION 8). Exe at launch: `release/NifSkope.exe` 2026-09-11 10:14:23, 21,101,056 B (NATIVE1b, `.lodo`/`.lodi` v3). Rollback rung, taken ONCE before your first link: `release/NifSkope.before_terrain_r.exe`.
- Read first, in order: `CONSTITUTION.md`; `HANDOFF.md` top block -- the RESUME paragraph, the newest lane block, then EVERY "RULING bungo 2026-09-11" paragraph about far terrain (09:2x hybrid by band; 09:3x roughness "if PBRM is used to bake it, it gets roughness, if a vanilla legacy material, its gloss gets inverted into roughness"; 09:4x metallic "only get derived from PBRM"; 09:5x "mirror how it's set up for the .lodm" and "we just add the coverage for whatever's missing in terrain textures that lod objects have in the texture department"); `docs/LODGEN_TERRAIN_VT.md` IN FULL (the `.lodt` contract: the four sheets, 2.2; the data sheet's stamp and cover law, 1.4-1.5; the index, 4); `docs/LODGEN_LODM_FORMAT.md` sections 1-2 (the family law, the `textures` slot table with `gsaos` / `rmaos` and `emissive`); `docs/LODGEN_BTD_FORMAT.md` sections "Ambient occlusion", "Water bodies", "What is NOT in this file, and why" (shore proximity is a runtime subtraction; wetness is a close-up effect); `src/lodgen.cpp` around `lodgenBakeTerrainVt` (~7076) and `lodgenBakeTerrainTextures` (~5466), and the object bake's material reading (~1401, ~1962-2001: how the specular slot / `specMult` / a PBRM-backed material are resolved -- the SAME resolution serves terrain layers); `src/io/lodvfile.{h,cpp}` (the container); `MISTAKES.md` root from 2026-09-11 on.
- Skills you MUST invoke (repo tree `.claude/skills/`): `nifskope-ww-lodgen`, `ww-standalone-writer-gate` (the container's new sheet roles: fixture, independent decoder, two-write identity, mutations refused by name), `ww-contract-provenance` (rewrite `docs/LODGEN_TERRAIN_VT.md` and the `.lodm` page's terrainVT section), `ww-control-calibration` (the roughness and colour comparisons need a floor and a ceiling), `ww-texel-picture` (the sheet pictures), `fo4cs-census-field` (every new census word), `ww-anchored-hookup` (edits to files another lane could own), `nifskope-ww-build-verify`.
- Build rules: ONE build (+ counted gate-only relinks). Markers `scratchpad/terrain_r_20260911/BUILDING` / `DONE`. Game check immediately before the link. bungo's window renamed aside, never killed. `ls scratchpad/*/BUILDING` empty at launch. Small regions only (Sanctuary 9-chunk), headless, own out-dir, never his installed files.

## bungo's rulings (verbatim in HANDOFF.md; the law here)
- Far terrain WILL be lit physically by FO4CS. Roughness: PBRM-backed layer -> its roughness map; legacy material -> gloss inverted (1 - gloss). Metallic: PBRM only; legacy contributes 0, never a guess. No subsurface for terrain.
- The terrain sheets take the OBJECT texture family (`.lodm` 2.1): a real `family` word; one MASK sheet laid out as `rmaos` = R roughness, G metallic, B AO, A = terrain's own fourth (ground cover -- propose, with numbers, whether cover stays in the mask's A or in the colour sheet's A as the object family's "coverage"; the director rules on your proposal, so state both costs); an EMISSIVE sheet when any layer supplies one (absent = none, named in the index); colour, model-space normal and height sheets kept.
- Dropped from the sheets: shore distance (runtime subtraction from the `.lodl` water planes) and baked wetness (close-up effect; far wetness = weather state, FO4CS runtime).
- Ring 0 = runtime blend from the `.lodl` per-texel LTEX weights; ring 1 out = the pyramid; cross-fade across ring 0. GENERATOR GATE owed: the ring-0 weights blend and the pyramid's baked colour agree on the same texel.

## The work
1. **Layer material resolution shared with the object bake.** For each LTEX -> TXST layer, resolve the material the way the object bake does (BGSM / PBRM beside it): the diffuse, the normal, and now the mask source: PBRM -> its roughness (and metallic) maps; legacy BGSM -> the `_s` specular map's gloss channel, inverted; nothing -> roughness default (state it; 1.0 = fully rough is the honest "unknown") and metallic 0. ONE function, called by the object bake and the terrain bake (CONSTITUTION 10: what is shared lives in shared code); the object bake's siblings still pass (`lodgen_card*`, `lodgen_arrays*` at their baselines).
2. **The mask sheet.** Per texel, through the SAME blend as the colour (layer opacities, base layer, VCLR not applied to roughness), R roughness, G metallic, B AO (moved from the old data sheet's R), A per item 3. BC3 when any channel needs alpha, else BC1 -- state the rule the way 1.4 states the cover stamp, with a header stamp a reader can qualify.
3. **Cover's home.** Option A: mask A = cover (mirror of subsurface's slot). Option B: colour sheet A = cover (the object family's "coverage" slot; colour becomes BC3 always). Measure both: bytes per tile, and whether the stock-engine `.btr` chunk path (which reads the colour sheet) tolerates a BC3 colour sheet (the stock tolerance gate). Propose one; ship it behind the ruling but with the OTHER reachable by a flag until the director rules, if that costs less than a page.
4. **The emissive sheet.** Written only when at least one layer's material carries an emissive map (`_g` legacy / `_e` pbr per the `.lodm` table); absent otherwise, and the index says `emissive: none`. Gate both directions with floors.
5. **Family word.** The `terrainVT` `.lodm` index writes `family: "pbr"` on the FO4CS target and MEANS it; the sheets[] block lists the new roles and channels; `lodvValidate` refuses a v1 (four-sheet, `data` role) container by name and the reader rule for v1 files is stated (refuse, not convert -- no v1 pyramid exists outside this tree; say so after checking his installed `Data\Terrain` READ-ONLY).
6. **Drop shore and wetness** from the container; the census names what was dropped and why; the `.lodl` water planes are named as the shore source in the contract.
7. **The ring-0 gate.** A standalone check (Python or a `--verify` mode): for N random texels in the Sanctuary region, blend the colour from the `.lodl` weights + the resolved layer diffuses + VCLR + the grass tint exactly as the contract states the runtime must, and compare against the pyramid's level-0 baked colour at the same texel: report the per-tile mean and max colour error in sRGB 8-bit units; floor = a deliberately un-graded blend (no VCLR) shown red; ceiling = the bake compared against itself (0). The contract states the runtime formula the gate implements, so FO4CS has one law to follow.
8. **Standalone gate before/after** (the `.lodt` fixture set from lane VT; counts recorded on the rung first).
9. **Region bake** (Sanctuary 9-chunk, `--vt` on, FO4CS target): sizes before/after per sheet; the stock path (`--no-vt`, no `--cover`) byte-identical with the rung (cmp every file).
10. **Contract rewrites** (`ww-contract-provenance`): `docs/LODGEN_TERRAIN_VT.md` (sheets, stamps, index, the ring-0 formula, the drop list), the terrainVT section of `docs/LODGEN_LODM_FORMAT.md` (family no longer vestigial).
11. Build, then the chain: standalone gate (after), `lodgen_terrain_vt.sh`, `lodgen_ground_cover.sh`, `lodgen_terrain.sh` (26/0), `lodl_open.sh` (23/0), the card/array suites (baselines from their logs on the rung), `lodgen_native.sh` (13/0), `ui_align.sh` (11/0), `water_ui.sh` (86/0). Every count that moves is explained by name.
12. Pictures (`ww-texel-picture` for the sheets; the render hook for terrain): `mask_sheet_tile.png` (one tile's R/G/B/A as four greyscale panels, captions with the channel law), `emissive_presence.png` (a tile with and one without), `ring0_blend_vs_bake.png` (the same 64x64 texel window: runtime-formula blend | baked | abs diff x8), and a render-hook top view of the region with the pyramid's colour on (as `nifskope-ww-vanilla-compare` does, beside vanilla's own far sheet for the same tile). Described in two sentences each before citing.
13. Documents: `scratchpad/terrain_r_20260911/WW_CHANGES_ENTRY.md`, `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md` (entries start with `## `); report `scratchpad/lane_terrain_r_report.md`.

## Gates (this lane)
- T1 roughness per texel vs the near material's roughness at the same layer mix (the resolver's own answer on the un-blended layer) within tolerance; floor = un-inverted gloss shown red; census names the rule per layer (pbrm | legacy-inverted | none-default).
- T2 metallic present iff a PBRM metallic layer exists (both ways, floor each way); legacy layers read 0.
- T3 emissive sheet present iff a layer has an emissive map (both ways).
- T4 the ring-0 gate: mean/max colour error printed per tile, floor red, ceiling 0.
- T5 stock path byte-identical with VT/cover off.
- T6 standalone gate before/after; one mutation per new role/stamp refused by name.
- T7 exe newer than every changed file; every driver rebuilt and `-nt`-checked; rung == launch bytes.
- T8 no NifSkope left running; game down at every launch.

## Rules
- One build (+ counted relinks). No roads (ROADS1), no panel rows (LODUI1), no native object changes (NATIVE1b's files: `src/lodofile.*`, `src/lodifile.*`, `src/nativeemit.*` are NOT yours; if the shared material resolver must be called from them, hook-up script only, `--check`, and say so).
- Small regions only. Never his installed files (read-only listing at most). Never `git stash`, never commit. Plain language.

## Report
`scratchpad/lane_terrain_r_report.md`, incremental (PENDING.md first if past half your context):
- `## 0. Pre-registered gates` (T1-T6 predicted; baselines on the rung)
- `## 1. The shared material resolver` (what the object bake did, what terrain now shares, the per-layer rule census on Sanctuary)
- `## 2. The sheets` (mask, emissive, cover's home with both costs, stamps, index)
- `## 3. The ring-0 formula and its gate`
- `## 4. Standalone gate before/after`
- `## 5. Build and gates` (mtime table; gate table with baselines and logs; stock cmp)
- `## 6. Pictures`
- `## 7. Owed / red / bungo's calls` (cover's home is one)
- `## 8. Mistakes`
- `## 9. Finished-work skill review`
