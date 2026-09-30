# PRTP -- precomputed radiance transfer probes, baked by NifSkope

Started 2026-09-30 09:23 on bungo's word: "you can start working on PRTP on the
nifskope side, for the bakes, remember, we needed to render each cell from the
game with lighting on as a prerequisite first". This is phase 3 of the Cell
Manager campaign (FO4CS_IMPROVED_LOD_PLAN.md section 8), and his word is the
explicit signal phase 2 (viewport lighting) needed.

Source: Stefanov, "Global Illumination in Tom Clancy's The Division", GDC 2016.
The FO4CS side already keeps a slide-by-slide ledger of it
(Fo4CommunityShaders/Codex/division-deck-coverage-matrix.md) and an in-game
baker that follows most of it. NifSkope becomes the OFFLINE baker: whole map,
game closed, no F4SE run.

Ruled 2026-09-30: no surfels or probes inside the LOD lanes; the ground keeps
the `.loda` AO map. Probes happen here.

## 1. What the deck needs, and what NifSkope has today

| Need (slide) | Today in NifSkope | Gap |
|---|---|---|
| Whole cell loaded: every REFR, SCOL parts, LAND (s16) | Cell view (src/cellview.*, cellworkspace.*): exterior cell or NxN block, SCOL expanded, enable-parent state, one load per model, welded draw | interiors (no LAND, interior CELL group); ground is flat VCLR, not the LTEX splat |
| Lights of the cell | none: LIGH is drawn only as its mesh; XLIG/XRDS never read | read LIGH DATA + REFR XLIG/XRDS; XCLL + LGTM for interiors |
| Lit like the game ("lighting on") | weather sun, ambient, DALC 6-axis via lookdev (lookdevstage.cpp, esmweather.cpp) | placed point/spot lights with vanilla falloff; interior ambient; fog; proof against game captures |
| G-buffer from 6 faces per probe: albedo, normal, depth (s16) | none | offscreen 6-face capture of the cell scene |
| Placement: 4 m raycast grid, a probe at EVERY hit, wall probes, <= 1000 per sector (s21-22) | none | CPU raycast over the welded scene (collision not needed: render geometry) |
| Sky visibility: misses = sky (s18) | none | from the capture depth |
| Surfels: 1 m cells keyed by position AND principal normal axis (s24) | none | FO4CS lacks the normal key (their M-04): do it right here |
| 4 m irradiance bricks, probes gather from bricks (s25) | none | FO4CS lacks bricks (their M-03): do it right here |
| Layout probes -> brick factors (6 face weights) -> bricks -> surfels, per sector (s26-32) | none | one file per cell (sector = cell, 4096 u) |
| Surfels store position/normal/albedo only -- lighting-free, relit live (s26) | -- | the bake never stores light |

## 2. Lanes, in order (one at a time)

1. **PRTP1 -- the cell, complete.** Interiors open in the cell view; LIGH refs
   read as lights (DATA radius/colour/flags, XLIG/XRDS overrides) and listed in
   `WW_CELL_DUMP`; XCLL/LGTM read; ground textured through the terrain splat.
   Gate: the dump against an independent Python walk of Fallout4.esm, whole
   plugin, lights included (vanilla corpus is the gate).
2. **PRTP2 -- vanilla light model, research.** How the stock game lights a
   pixel: point/spot falloff, DALC, interior ambient, fog. PDB FIRST, RVA per
   build, plus the stock shader bytecode. Written down per term before any
   renderer edit. (DeepSeek never sees the PDB.)
3. **PRTP3 -- viewport lights.** The cell view drawn with the cell's lights,
   sun/DALC or interior ambient, fog, per PRTP2. Renderer work.
4. **PRTP4 -- ground truth.** RenderDoc captures of the stock game at named
   cells (flights when the game is down; bungo names the saves); one pixel gate
   per term, measured, not eyeballed.
5. **PRTP5 -- render each cell.** A harness walks every exterior cell and every
   interior, renders it lit, writes a census row per cell (drawn, lights,
   refusals). This is the prerequisite bungo named, done for the whole game.
6. **PRTP6 -- the probe bake.** Section 1's rows from "G-buffer" down. Output
   format: proposed = the FO4CS in-game baker's `.tbk` so the game already reads
   it (bungo's call; the FO4CS reader otherwise comes last by standing order).

## 3. Open

- `.tbk` compatibility vs a new NifSkope format (proposal above).
- Save names for the PRTP4 capture flights.
