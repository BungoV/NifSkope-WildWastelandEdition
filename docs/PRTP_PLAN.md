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
| Sky visibility: misses = sky (s18) | GROUND ONLY: the `.loda` AO map (TERRLIVE2) is sky visibility over our painted ground at 32 u -- one up-facing scalar per texel, the same "misses = sky" quantity | probes still need their own, per face (6 directions): under bridges and beside walls one up-facing number is wrong. `.loda` also feeds far-field snow/wetness placement |
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
   DONE 2026-09-30 on branch prtp1-20260930 (not merged): interiors, XCLL,
   LTMP and every placed light read; gate tests/spells/cell_lights.sh, 10
   interiors / 3,945 lights PASS, red control fails. Still open: the ground
   through the terrain splat.
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
   RULED (bungo, 2026-09-30): the whole world is never loaded at once. Each
   exterior cell is rendered and baked with the 5x5 block around it at full
   detail (the game's own loaded grid) and LOD beyond; the walk covers the map.
   Interiors: the whole cell, alone.
6. **PRTP6 -- the probe bake.** Section 1's rows from "G-buffer" down. Output
   format: proposed = the FO4CS in-game baker's `.tbk` so the game already reads
   it (bungo's call; the FO4CS reader otherwise comes last by standing order).

## 2b. Inherited from FO4CS's in-game bake (read 2026-09-30; MISTAKES: this plan was first written without it)

FO4CS ran the Division bake in game, 2026-08-03..08-07: code fallout4-community-shaders/src/TransportBake +
WorldProbes, reports Codex/division-bake-b1..b2o-*.md, rulings Codex/HANDOFF.md ~17350-18680, gap list
Codex/division-deck-coverage-matrix.md (M-01..M-22). Its rulings bind PRTP:
- The deck is authoritative; diverge only if it does not work in FO4 or a divergence measurably wins.
- Statics only. Actors, havok clutter, projectiles, movables, FX: receivers only. Workshop builds: receivers only.
- Doors: baked EXCLUDED (aperture open); every probe-surfel link crossing a door's aperture carries that door's id;
  the runtime attenuates tagged links by door state (closed = zero). No per-state rebakes. Huge movers: same.
- Apertures without doors ("there's not always a door in a doorway"): door REFRs + interior portals + geometric
  opening detection (wall interruption at walkable height; windows the same); a probe at every aperture.
  PREFERRED (bungo 2026-08-07): "is there a gap between two volumes" -- label the AIR cells of the bake volume
  into connected components, mark each interior/exterior by sky visibility, and the narrow NECKS where
  components meet are the apertures (position, size, facing; no REFRs). Height band + size classify it:
  walkable = doorway, raised = window, huge = collapsed wall; boarded = no gap = no probe. Windows get probes
  only; doors also get gated links. The labels are the interior bit for thin-wall leaks. NEVER BUILT in FO4CS
  (wall probes shipped B2f; aperture spawns queued, then parked). Interior cells: finer spacing (~2 m candidate).
- Placement: sector = cell; 280 u (4 m) global-lattice columns; multi-hit column descent (probe per air gap
  >= 140 u, <= 6 levels); wall probes (4 dirs, 96 u standoff, every 240 u up to 960 u), every level ray-verified.
- The bake stores zero lighting; sun, sky and placed lights are evaluated live through the baked transport.
What the game-side bake never achieved, and why NifSkope takes the bake over:
- Its capture read the engine's G-buffer, so it baked EVERYTHING drawn: no statics filter, no door exclusion
  (grep: none in TransportBake/). NifSkope draws only what we pick.
- ~100 ms per probe, display-bound, camera-arrival refusals; the bake had to run in game, unfocused.
- Last gate (B2m/B2n) FAILED: 19.8-46.2% of receivers got exactly zero fill in cell-sized blocks; interiors 4x
  over-lit. B2o deployed, never gated. Parked load-bearers: M-03 bricks, D-16 sun shadow, D-17 per-sector light
  gathering, M-16 distant tier.
Output: `.tbk` v3 ('TBK1', 64-byte header, surfels 32 B, probes 144 B, links 12 B, one file per cell,
sector_%+05d_%+05d.tbk) is what FO4CS's relight already reads. PRTP6 writes it (door ids on links need a v4).

## 3. Open

- `.tbk` compatibility vs a new NifSkope format (proposal above).
- Save names for the PRTP4 capture flights.
