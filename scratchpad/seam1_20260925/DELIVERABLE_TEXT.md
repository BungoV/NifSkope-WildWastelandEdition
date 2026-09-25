# SEAM1 deliverable text (for the overseer to splice; this lane edits none of these files)

## HANDOFF (top block)

Lane SEAM1 is BUILD PENDING: the game was up all session. Branch seam1-20260925 has three commits:

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
