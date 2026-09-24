# Lane ROADS4 -- the road meshes carry terrain-shaped skirt geometry; the far bake must not paint it as road

## Header
- Tree: E:\Projects\NifskopeWildWastelandEdition, branch main. Nothing is committed, never git stash. Exe at launch: from the UINOTES1b landing line in HANDOFF.md (2026-09-12 05:48:33, 21,817,856 B, sha256 b62c4998... (UINOTES1b's exe: every UI ruling built; UI chain green except the known reds; the lodgen chain NOT yet run on it -- see item -1)). Rung ONCE: release/NifSkope.before_roads4.exe. Markers scratchpad/roads4_20260912/BUILDING / DONE. Report scratchpad/lane_roads4_report.md, incremental; PENDING.md past half context.
- Read first: CONSTITUTION.md; HANDOFF.md top block (RULING 05:0x on --road-detail, the ROADS3 and ROADS2 blocks); scratchpad/lane_roads3_report.md sections 1.6 (the profile: ours ramps over three texels, plateaus, climbs again) and F3e' (luminance vs mesh vertex alpha: vanilla +0.001, ours -0.792, our ground's floor -0.325); scratchpad/lane_roads2_report.md (76 vertex-alpha shapes, 52 with the Vertex Alpha shader flag, the feathering rule it shipped); docs/LODGEN_TERRAIN_VT.md 1a.5 (the road path: colour, coverage, --road-composite, --road-detail, --road-opacity, --roads-legacy).
- Skills: nifskope-ww-lodgen, ww-control-calibration, ww-simulate-before-build, ww-prototype-is-not-the-product, ww-sheet-diff, ww-texel-picture, ww-spec-gate-audit, nifskope-ww-build-verify, ww-anchored-hookup, ww-contract-provenance. Stay out of the animation/UI files (src/anim*, hkx*, nifskope_ui.cpp, ui/*, wwskin.h): lane UINOTES1b's territory.

## bungo's words (verbatim, 2026-09-12 05:0x-05:1x)
"--road-detail 1 is always on, do not ever use road detail 0, that looks terrible"
"the issue with the roads is, these meshes have some terrain included there, you can see the sharp mesh terrain being included into the chunk's bake"

## What is known going in
- The road NIFs are not just the asphalt: they carry shoulder / skirt triangles shaped like the terrain around the road, feathered by VERTEX ALPHA (76 shapes; 52 with the Vertex Alpha shader flag, ROADS2). In the game those triangles fade into the landscape. Our bake paints them as opaque road paint (coverage 1 for an opaque shape, colour = diffuse x vertex colour), so the sheet shows a hard band of terrain-shaped mesh around every road: the darker outer band / two-tone ROADS3 measured (the profile ramps 79.86 -> 86.48 -> 93.45 over three texels, plateaus near 96.5, climbs to 105.5 in the core; max second difference 3.88 vs vanilla 1.31 on (-20,20)). Vanilla's sheet shows NO trace of that geometry: correlation of road luminance with the mesh vertex alpha +0.001 vs ours -0.792. --road-opacity shrinks it and cannot remove it (ROADS3 F3e').
- ROADS2 already reads vertex alpha for the FEATHERING of piece boundaries; it does not turn the skirt's alpha into coverage, and it does not exclude skirt geometry.

## Item -1, FIRST (inherited from lane UINOTES1b, which suspended when the game came up)
- GAME CHECK: `tasklist | grep -i "^Fallout4"` -- count Fallout4.exe SEPARATELY from NifSkope.exe, never in one number
  (UINOTES1b built three times with the game up because its guard added the two). If Fallout4.exe is up (it was, pid
  48328, since 05:42:58), do the OFFLINE work below (items 1-2: measurements on the existing ROADS3 bakes and the
  python simulations) and poll for the game every 5 minutes; nothing is built and no exe is launched while it is up.
  If it is still up when the offline work is finished, end BUILD PENDING with PENDING.md.
- Once the game is down, BEFORE your rung and before any build: `bash scratchpad/uinotes1_20260912/lodgen_chain.sh`
  on the 05:48:33 exe and compare row for row with lodgen_roads 11/0, lodgen_terrain 26/0, lodgen_terrain_vt 41/1
  (V9c known), lodgen_ground_cover 29/5, lodgen_terrain_pbrm 14/0, lodgen_native green, lodl_open 23/0,
  lod_generation 116/0; then `bash scratchpad/uinotes1_20260912/ui_chain.sh after` for the six UI harnesses on that
  exe (expected hkxanim_ui 48/1, ui_align 15/0, water_ui 84/0, files_tab 29/1, top_bar 43/5, skeleton_overlay 5/1).
  Write both tables into `scratchpad/uinotes1_20260912/RESUME_BY_ROADS4.md` and append one line to
  `scratchpad/lane_uinotes1_report.md` under `## Build (UINOTES1b)` saying ROADS4 ran them, with the date/time read
  from `date`. Any row that differs from the expected number is reported, not fixed (not your lane's code).
- Then your rung `release/NifSkope.before_roads4.exe` (a copy of the 05:48:33 exe) and the work below.

## The work
0. FIRST: the --road-detail default flip (his ruling). src/lodgen.h:784 `float roadDetail = 0.0f;` -> `1.0f`; help text src/nifcli.cpp ~5686-5689; docs/LODGEN_TERRAIN_VT.md (1a.5c and the flag table) say the default is 1 by his ruling, quoted, and that 0 is the flat-colour mode he rejected; check src/nifcli.cpp:6442 (legacy branch) still reads right. Gate G0: a default bake of (-20,20) == a `--road-detail 1` bake byte for byte; `--road-detail 0` reproduces scratchpad/roads3_20260911/out/new_default byte for byte. This item is not optional and is not a candidate.
1. Measure the skirt before any rule: per road shape, classify triangles as trunk (all three vertex alphas 1) vs skirt (any vertex alpha < 1), and per material; count texels painted by skirt-only triangles on (-20,20) and (-8,8); report their luminance vs the ground under them and vs vanilla at the same texels. That number is the size of the defect and the floor for G1.
2. Candidate rules, simulated offline first (ww-simulate-before-build), then ONE built: (a) vertex alpha becomes COVERAGE for the road plane (skirt fades to the ground where alpha -> 0, not just its colour blended), (b) skirt triangles excluded from the road plane entirely (paint the trunk only), (c) alpha as coverage for colour AND the cover-suppression (so ground cover returns under the fading skirt). Pick by the gates; ship the winner as the default with a switch back (`--road-skirt paint|fade|drop` or similar, old behaviour reachable, `--roads-legacy` untouched).
3. Gates, registered before ranking (ww-spec-gate-audit): G1 correlation of road luminance with mesh vertex alpha within the floor of vanilla's +0.001 (the phase-twin floor from ww-control-calibration); G2 the cross-road profile's max second difference on (-20,20) within vanilla's 1.31 + floor, and (-8,8) not worse than today's 1.25; G3 ROADS2's feathered-boundary metric not worse than today's 3.979 (vanilla 4.242) and the solid-boundary control unchanged; G4 the road MASK width (texels the road paints) within 1 texel of vanilla's visible road width on both chunks; G5 F2 byte identity: the switch's old setting reproduces the rung bytes on both chunks, every file; G6 ROADS3's F4 harness rows unchanged (11/0, 26/0, 41/1 with V9c known red, 29/5, 14/0, native all-green, 23/0, 116/0).
4. Pictures: scratchpad/roads4_20260912/images/: the Sanctuary crop of road_detail_look.py's window at 4:1, vanilla | rung (detail 1) | winner, plus the profile chart re-drawn with the winner; every panel a real bake.
5. Documents: WW_CHANGES_ENTRY.md (`## 2026-09-12 — <title>`, em dash), HANDOFF_BLOCK.md, MISTAKES_ENTRIES.md, docs/LODGEN_TERRAIN_VT.md 1a.5 amended, BAKE_INSTRUCTION.md (scratchpad/lodui1_20260911/) told what changed, a skill if a procedure repeats.

## Rules
tasklist for Fallout4.exe / NifSkope.exe before any build or exe launch; game up = BUILD PENDING. Bungo's window (no --port) never touched; rename the exe aside at link time. One build, counted relinks. Plain language, every number beside its floor, no final/true claims, incremental writes, timestamps from `date +%H:%M` only. His Data\Terrain never touched; region bakes only.
