# Lane SEAM1 -- the hard-edged dark ground block at Sanctuary in the FO4CSLOD bake

Director brief 2026-09-25 11:59. Opus 5.5, in-session agent. Worktree E:\Projects\NifskopeWWE-seam1, branch
seam1-20260925 from main ea0ca708. Read scratchpad/brief_lodnight_common.md (main tree copy) FIRST; its rules apply.
Skills: nifskope-ww-worktree-build, nifskope-ww-lodgen, nifskope-ww-render-shot, ww-lodl-offline-census,
ww-texel-picture, ww-artefact-localise, mo2-mod-content-census, ww-whole-map-lod-bake.

## bungo's words (verbatim, 2026-09-25 on 01_sanctuary.png of the BAKE1 pictures)
"See the terrain here? The hard line with no blending?"

## What he is pointing at
E:\Projects\NifskopeWWE-bake1\scratchpad\bake1_20260925\pics\01_sanctuary.png (cells -22,19..-17,24, top view,
1600 px wide = 6 cells, ~267 px a cell). A darker ground block with STRAIGHT edges: left edge ~x 535, top ~y 325,
bottom ~y 1390, running off the right side; the ground inside is dark soil/green, outside light tan, and the edge
has no blend. BAKE1's INDEX.md called it "a changed ground texture on those cells (not checked against the plugins)".
That was never measured. The bake: E:\Projects\Fallout 4 Mods\mods\FO4CSLOD (read-only for this lane until step 5),
built by the run-copy exe 27a7bb29 (bake1-20260925, merged into main).

## Candidates (pre-registered; name the winner with its measurement, never by look)
C1 A plugin in his load order overrides LAND (texture layers) for exactly the block's cells and the author left the
   border unblended -- then the game shows it too. Discriminator: the block's edges fall on CELL (or quadrant)
   borders, and the winning LAND plugin (or its BTXT/ATXT/VTXT content) differs inside vs outside. Compare to a
   vanilla-only VT bake of the same cells (Fallout4.esm alone): if the vanilla bake has no edge, name the plugin.
C2 Our VT pyramid: tiles at different pyramid levels / a fallback tile / a tile baked from a different source level
   meet there. Discriminator: the edges fall on VT TILE borders (not cell borders); the texel density or level id
   differs across the edge.
C3 The --cover (grass) channel or the height sheet feeds the albedo differently across the edge. Discriminator:
   the edge vanishes in the same cells baked with --cover off (and/or height off).
C4 The shot recipe (lodl level 2 + VT sheet selection in shot.sh) and not the baked files. Discriminator: the edge is
   absent when the same .lodt texels are read straight from the file (ww-texel-picture) with no renderer.
Measure the edge position in WORLD units first and snap it against the cell grid, the quadrant grid and the VT tile
grid; that one number rules two of the four out.

## The work
1. Localise (no build needed): world coordinates of the edge; which grid it sits on; the LAND winner per cell over
   the block and its ring; the texel read of the .lodt on both sides.
2. Red/green controls: a small region bake (the block + one cell margin) with (a) his load order, full settings,
   (b) Fallout4.esm only, (c) --cover off. Bake into scratchpad/seam1_20260925/, NEVER into FO4CSLOD.
3. Verdict: which candidate, with its numbers. If C1 (the mod's own data): STOP, no code change; report the plugin
   and whether the game shows the same edge (vanilla engine LOD for those cells from the load order, if measurable).
4. If ours (C2/C3/C4): fix it at its root in src/, with a gate that FAILS on the current code (prove it red), stays
   byte-identical on a vanilla-only bake away from the edge, and passes on the fix. Game gate before every build:
   `tasklist //FI "IMAGENAME eq Fallout4.exe" | grep -q Fallout4.exe` = GAME UP (an error also = GAME UP) -> no
   build, write the code, end BUILD PENDING. THE GAME WAS UP AT 11:59: expect BUILD PENDING unless it closes.
5. Only after a built, gated fix: re-bake just the affected part into mods\FO4CSLOD (the one folder you may write),
   first copying every file you will replace to scratchpad/seam1_20260925/replaced/ (untracked); list before/after
   sha1 of each replaced file; nothing else under E:\Projects\Fallout 4 Mods\ is touched.
6. Pictures (untracked, scratchpad/seam1_20260925/pics/): the same Sanctuary view before and after, and the edge
   zoomed 4x from both; plus the three control bakes side by side.

## Rules
Common brief rules. No push, no merge (the director merges). Commit text only, by explicit path, incrementally.
Never kill bungo's NifSkope window; one harness NifSkope at a time, second monitor, unused --port. Never touch his
modlist/plugins/INI. No "fixed" claims -- state mechanism + refuter. This repo is PUBLIC: never name the engine PDB.

## Report
scratchpad/seam1_20260925/DONE.md + DELIVERABLE_TEXT.md (HANDOFF text / WW_CHANGES text / MISTAKES text) +
progress.md every 15 min with the clock READ. Last DONE.md section: the skill review (loaded / wished / written).
Final message under 250 words, plain: the candidate with its numbers, the fix (or why none), gates red->green,
BUILD PENDING or not, picture paths.
