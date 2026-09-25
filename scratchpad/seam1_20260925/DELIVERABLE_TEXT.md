# SEAM1 deliverable text (for the overseer to splice; this lane edits none of these files)

## HANDOFF (top block)

**Status at 16:14.** Lane SEAM1 is BUILT and INSTALLED, NOT FLOWN.
- Exe b9fd029b. The whole-map VT (fill ON, cover and height on) went into mods\FO4CSLOD at 16:14. The old files are in scratchpad/seam1_20260925/replaced/, with sha1 before and after.
- Two more fixes landed:
  - 44805f8f, grass tint: the white/"sandy" cover came from reading straight-alpha mips as premultiplied.
  - 59a0dd33: the default-ground path had unknown `\G` escapes.
- Gate results are in the DONE.md table. The north-east fill_model gate is RED from a painted-set mismatch, not from the fill. W4 G1 is RED from the selfAO codegen bytes; the .lodo is not installed.
- bungo's in-game look is owed.

The original three commits:

- **c21eb26a, the Sanctuary edge root.** A BTXT-less quadrant, or a NULL-LTEX layer, now paints the engine default land texture, not the chunk's dominant base.
- **a6e5e8de, `--vt-fill-vanilla`.** It is OFF by default. It blends unpainted ground toward Bethesda's dim-4 LOD colour, and the vanilla sheets are read loose at bake time only.
- **62e53a3b, `.lodo` v5.** This is the optional per-vertex colour stream, written only for shapes with a colour channel AND Vertex_Colors.

Every gate is pre-registered under `scratchpad/seam1_20260925/`. The resume list is at the end of DONE.md.

**Owed, by standing order and not news:** the FO4CS reader of `.lodo` v5, meaning the header words 0xD4/0xD8 and the colour blob.

**Boston look, measured, not fixed.**
- Vanilla object LOD carries no tint of any kind: no vertex colour, black emissive, and a near-grey atlas.
- The candidate is the engine's weather light: the sun colour plus the DALC ambient.

**Open defect, not fixed here: `src/lodtsheets.cpp` ~511-520.**
- The sheet cache names tiles by container stem, and the key does not include the container's identity.
- Two bakes of one worldspace in two folders therefore draw the first bake's tiles.
- Workaround: set WW_LODL_SHEET_CACHE to a fresh directory for each render.

## WW_CHANGES

2026-09-25, lane SEAM1 (BUILD PENDING, not built, not flown):

- **Terrain colour law, §2.5 step 4.**
  - Change: a LAND quadrant with no BTXT, and an ATXT layer naming LTEX 0, now paint the engine's default land texture (CommonwealthDefault01), as the engine does. They no longer paint the chunk's dominant base.
  - Why: this removes the straight-edged dark block at Sanctuary and the flat grey chunks around the map (2019 of 2304 chunks were flat on the shipped bake).
- **New `--vt-fill-vanilla` (panel row too, OFF by default).**
  - What it does: ground that his LAND does not paint is blended, over a measured band, toward Bethesda's own LOD colour for those cells. The tone is matched on the overlap, and the gain is capped at 1.
  - Where vanilla comes from: it is read from `--vanilla-lod-root` at bake time and never shipped.
  - What it never touches: painted cells.
- **`.lodo` version 5.**
  - It carries the vertex colour of the few LOD models the game tints (the Amphitheater, the blasted maples, warehouse roofs, brick shells), and only where the game draws it.
  - A library without colour is the v4 file with a new version number.
  - The NifSkope viewer draws the colour. The in-game reader does not read v5 yet.

## MISTAKES

- **2026-09-25, SEAM1: the native sheet cache served another bake's tiles.**
  - What happened: two control bakes of Commonwealth in two folders rendered the first bake's terrain, because the viewer's sheet cache is keyed by the container stem, not its identity.
  - How it was caught: the control renders did not differ where the bakes did.
  - The rule: every native render of a non-shipped bake sets WW_LODL_SHEET_CACHE to a fresh directory, and the code defect is logged for its own lane.
- **2026-09-25, SEAM1: a `git add` list with one gitignored path added nothing.**
  - What happened: the `&&`-chained commit silently did not run.
  - The rule: read the `git add` result before committing, and keep generated TSVs out of the path list.
- **2026-09-25, SEAM1: the first worktree build compiled none of the lane's edits.**
  - What happened: the code was written while the game was up; the objects copied in afterwards were stamped newer than those sources, so make relinked the old code. Caught by the build log listing 0 of the changed files.
  - The rule: after copying objects, touch every changed source and check the rebuilt-object list names each one (added to skill nifskope-ww-worktree-build §7).
- **2026-09-25, SEAM1: "Landscape\Ground\..." in a C++ string named no file.**
  - What happened: `\G` and `\C` are unknown escapes; g++ warns and drops the backslash, so the engine-default ground texture path (c21eb26a) opened nothing and painted the missing-texture grey. Caught from the Sanctuary picture, not from a gate.
  - The rule: every Windows path literal uses `\` or `/`; `escape_scan.py` over the branch diff reads 0 before a build.
- **2026-09-25, SEAM1: two gates were registered that could not fail.**
  - What happened: the edge gate measures steps, so a flat wrong colour passed it; the first default-colour gate (grey with luminance > 150) read 0.0000 on the broken exe too, because missing-texture grey sits at 100-130. The first grass gate needed cover-255 texels, and the region peaks at 159.
  - The rule: run each gate on the known-broken exe before trusting its GREEN (Measure, don't eyeball). Re-registered forms: DG1 broken 0.1635 RED / fixed 0.0064 GREEN; grass gate by the tint-1 vs tint-0 control.
- **2026-09-25, SEAM1: grass tint read the smallest mip as premultiplied.**
  - What happened: DDS mips are straight alpha; dividing by alpha clamped every alpha-cut grass to white, so ground cover paled the terrain toward sand. Fixed in 44805f8f (alpha-weighted mean).
