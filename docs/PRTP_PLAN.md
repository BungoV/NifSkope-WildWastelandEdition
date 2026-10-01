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
   DONE 2026-09-30, merged to main: interiors, XCLL,
   LTMP and every placed light read; gate tests/spells/cell_lights.sh, 10
   interiors / 3,945 lights PASS, red control fails. Still open: the ground
   through the terrain splat.
2. **PRTP2 -- vanilla light model, research.** How the stock game lights a
   pixel: point/spot falloff, DALC, interior ambient, fog. Todd's treat FIRST, RVA per
   build, plus the stock shader bytecode. Written down per term before any
   renderer edit. (DeepSeek never sees Todd's treat.)
   WRITTEN 2026-09-30: docs/PRTP2_LIGHT_MODEL.md (point, spot, sun, DALC, fog,
   fade, summation). Open: the fog packing and the spot half-angle, both for
   the PRTP4 capture.
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
   BAKE WRITTEN 2026-09-30 (lane PRTPBAKE): section 2g. The band's Bake writes
   `.tbk` v3 sector files; gate tests/spells/probe_bake.py (independent re-trace).

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

## 2c. Probe placement -- lane PRTPPLACE (2026-09-30)

`src/probeplace.{h,cpp}`; the cell view places probes when `WW_CELL_PROBES=<tsv>` is set
(`WW_CELL_PROBES_N` = middle NxN cells, `WW_CELL_PROBE_SOUP=<psp>` dumps its triangles); the CLI
`nifskope -no-gui probeplace --soup <psp> --rect minX,minY,maxX,maxY --out <tsv>` re-runs a dumped soup.
Spells: `tests/spells/cell_probes.sh` (bake + photos), `tests/spells/probe_place.py synth|retrace`.

- The lattice, the column descent and the wall stacks are FO4CS's B2f/B2n, number for number (2b above).
- Divergences: rays hit the render triangles of statics, not Havok collision; the column runs from the soup's
  top + 16 to its bottom - 16 (FO4CS: player z + 2048 / - 8192); no probe cap.
- The soup: STAT MSTT TREE FURN CONT ACTI TERM FLOR LIGH + LAND. Left out: disabled refs, markers, clutter
  types, Sky\ meshes (distant clouds roofed all of Concord), Water\ planes (no collision in game), effect/
  glass/decal shapes, alpha-tested shapes under Landscape\ (leaf and grass cards). DOOR = box only.
- Openings (the ruling in 2b, built): the soup voxelized at 35 u; an opening is a straight-through neck in a
  thin wall (jambs, lintel, sill all present), with a roof on at least one side, not into a pocket shorter
  than 140. Searched at 4 wall angles (0, 22.5, 45, 67.5 degrees: Concord stands at 45). Classes: breach
  if wider or taller than 400, doorway if the sill is within 60 and it is 140 tall, else window. A door
  standing in it only tags it (its ref). One probe per opening (dedupe 105 u), at sill + min(120, h/2).
- Gates: synth (7 known openings incl. a 45-degree house, 4 traps, every lattice probe re-traced) and
  retrace on a real cell's soup (300 columns re-traced exactly, wall probes must still see their wall).
  Reds `--red wall|aperture|frames` and a `--red wall` retrace must FAIL; all do.
- Numbers, Concord 3x3 around (-15,17): first-hit 1892 (FO4CS 1892), interior 691 (FO4CS 1502), wall 923
  (FO4CS 1513), openings 173 (doorway 46, window 121, breach 6). The interior/wall gap is the physics vs
  render-triangle difference plus FO4CS's clutter and its taller column; not chased. Museum of Freedom
  interior (ConcordMuseum01, spacing 280): 609 probes, 110 openings, 8 with a door in them.
- Limits: walls between the 4 angles by more than 11.25 degrees; openings under 70 u; interior spacing is
  still 280 (FO4CS's ~2 m candidate is `WW_CELL_PROBES_SPACING`). A doorway as wide as the hallway at the
  hallway's END is not found: its jamb is the hallway's own wall, too long to read as a thin jamb.
- Glass (bungo 2026-09-30): left out of placement (it is not a wall to a probe), and in PRTP6 it passes
  light, tinted by its material when the material has a tint. Not an occluder, not a surfel.

## 2d. The interior rule -- lane PRTPPLACE (bungo 2026-09-30: "some rooms are tight and separated")

The lattice alone leaves small rooms and hallways without a probe. On top of it, from the 0-degree voxel frame:
- Walkable cell = air standing on solid with at least 4 air voxels (140 u) above it. Covered = solid above it
  before the grid top. Every walkable cell inside an opening's slab is CUT, so the openings split rooms.
- Room = covered walkable cells, 4-connected, joined across one voxel of height. Each room edge is classed:
  door (next to a cut cell, or under a window), open (next to uncovered walkable air), drop (no floor next to
  it). Filters in order: under 4 cells = too small; no door, open or drop edge = SEALED (crawlspaces under
  house floors, hollows in walls: no probe); drop on half the edges or more = LEDGE (furniture tops, shelves);
  open + drop over 30% of the edges = OPEN (porches, awnings: the lattice covers them). The rest are enclosed.
- Clearance per cell = the nearest wall along 8 horizontal rays at EYE height (capped 600). Measured at eye
  height, not on the floor: tables, beds and display bases are not walls (the floor measure put 2,774
  probes in the museum). Hallway cell = the narrowest wall-to-wall span through it is 210 or less.
- Room probe at the enclosed room's widest cell (skipped if a probe already stands within 70 of it).
- Cover fill, widest cells first: a cell that sees no probe (line of sight, at the cell's sample height)
  within 200 -- or within 70 in a hallway, so hallway probes stand at most 140 apart -- gets a probe, at
  the real floor + min(eye, half the ceiling height).
- Numbers: Museum of Freedom 609 -> 975 probes (room 25, cover 341; 32 enclosed rooms, 7 sealed);
  Concord 3x3 3,679 -> 4,335 (room 131, cover 525; 144 enclosed, 173 sealed). Rooms step 144 / 332 ms.
- Gates (synth building E: a hallway and two rooms behind doorways): every room gets a probe; every 35 u
  floor point at eye height sees a probe within its radius + 45; hallway probes no more than 175 apart.
  `--red coverage` (the fill off) must FAIL: 58 blind points, a 280 gap. It does.

## 2e. The PRTP band (Cell workspace; bungo: in the Cell workspace, named PRTP)

Show probes | Place | Bake. Place re-opens the cell with the probes placed over the whole loaded block
(`CellSceneSpec::probes`); the kind rows (first hit, interior, wall, doorway, window, breach, room, cover,
All) are read from the placer's census lines in the builder's notes, their swatches from the one color
table the markers draw with (`cellProbeKinds()`). Show probes off = placed and counted, not drawn. Bake
(lane PRTPBAKE) places and bakes in one re-open; the note line then says "baked N files, S surfels, L links:
<folder>". Gate: `WW_CELLWS_PRTP=1` on tests/spells/cell_workspace.sh's run: 17 PRTP rows (stage 3 opens the
written files and checks their magic and count).

## 2f. The froxels (FO4CS Volumetric Air, read only 2026-09-30; bungo: "how do the froxels get placed on areas that are covered from all probes?")

What FO4CS does today (wave 101, `res/Effects/VolumetricAir/`, `src/Effects/VolumetricAir.cpp`):
- The froxel grid is camera-space (screen tiles x depth slices), not placed in the world. Froxels behind
  the column's nearest depth are zero (MaterialCS).
- It reads NO probes. Per froxel, light = sun x Henyey-Greenstein phase x shadow cascade x cloud mask x
  far-field mask, plus ONE sky term: the weather's directional ambient (DALC) evaluated at world up, the
  same value for every froxel (LightScatteringCS 171-229, VolumetricAirCommon 164-179). No point lights.
- Interior cells: the pass refuses (`Refusal::Interior`, "interior-no-sun"); no air indoors at all.
- The skylighting probe volume (skylighting_probe.hlsli) feeds surfaces only (ambient IBL, sun, tiled
  ambient); nothing in the air includes it.
So under a roof or porch in an exterior, the air gets the full open-sky term: the sky leaks in.

What PRTP gives it (FO4CS's lane, after PRTP6 ships a bake):
- Sky term per froxel: interpolate the nearest baked probes' sky visibility (SH) at the froxel's world
  position instead of the global up value. Covered air then darkens exactly as the surfaces under it do.
- Interiors: the same lookup with the room's probes lets the pass run indoors (lamps x probe irradiance)
  instead of refusing.
- A froxel between probes of two rooms must not blend across a wall. That needs the room tag: `.tbk` v4
  adds a room id per probe and the door ids on links (2c); the lookup keeps only the probes whose room
  matches the froxel's (found by a point-in-room test on the room boxes the bake writes, else the nearest
  probe's room). A doorway probe belongs to both rooms.
- Nothing here changes FO4CS code from this repo: the v4 fields are additive (a v3 reader skips them).

## 2g. The bake -- lane PRTPBAKE (2026-09-30)

Output: FO4CS's own `.tbk` v3 (`src/TransportBake` in FO4CS, read only): 64-byte header, 32-byte surfels,
144-byte probes, 12-byte links, `sector_%+05d_%+05d.tbk` per 4096-unit sector, keys by float floor
division, surfel cell 70. Every sector file carries each surfel its probes link to, so a file reads alone.
The folder is `<NifSkope>/prtp_bake/<world or cell>`; copying it into the game's TransportBake folder is the
user's step, never done by NifSkope. `WW_CELL_PROBE_BAKE_DIR` moves it; `WW_CELL_PROBE_BAKE=<folder>` bakes
on every probing build (headless runs).

How (src/probebake.cpp, CLI `probebake --soup ...`):
- Rays: a Fibonacci sphere of N (default 2048), each worth 4pi/N, from each probe through the soup's BVH
  (src/probebvh.h, shared with the placer). A miss is sky, per octant; a hit lands in a 70-unit cell.
- Pass 1 settles each cell's surfel: hits sorted into six bins by the side they face (normal turned toward
  the ray's probe). The cell keeps the side most rays saw plus every side not opposed to it: mean position
  (kept inside the cell), mean normal, mean albedo.
- Pass 2 per probe: sky and surface per octant, mean distance and RMS, one link per cell hit with weight
  = its solid angle / 4pi and the mean direction.
- THE THIN-WALL RULE. A 70-unit cell can hold both faces of a wall. `.tbk` v3 has one surfel per cell,
  so a probe on the far side would link to a surfel facing away and pull light through the wall. Such a
  link was refused and its weight went to `unlinkedWeight` (the relight renormalizes over the rest).
  SINCE 2026-10-01 THE SECOND SIDE IS HOUSED NEXT DOOR, still v3: the opposed faces become their own
  surfel in a free neighbor cell (nothing hit there, nothing claimed), tried from the neighbor most along
  their normal down to 45 degrees off it, in key order; a refused link goes to it instead. The surfel's
  position is kept 1/1000 of a cell inside its home (on the shared face float32 can key it next door).
  Concord: 6197 of 10362 two-sided cells housed; refused weight 0.150 -> 0.063 of the sphere. Then the
  links are sorted (weight, then key) and capped at 1024 (the relight reads any count; FO4CS's own
  in-game bake keeps 256, which cost Concord 0.04 of the sphere on busy probes); the rest is unlinked.
- THE REFERENCE GATE (tests/spells/prtp_reference.py + tests/prtp_reference.cpp, 2026-10-01). The synth
  gate proves the bake re-traces itself; this proves what a probe's links reconstruct matches what the
  probe sees, on a real cell. A brute-force tracer that shares no code with the bake (every triangle per
  ray, its own jittered directions) traces K probes under a test field (albedo x (0.25 + 0.75 max(0,n.s)),
  sky 0 as in the relight). Gated: sky per octant (worst 0.01), the total (median 10%, p95 25%) and the
  band-2 SH irradiance over 26 normals, what a surface reads (median 10%, p95 25%). Concord, 96 probes:
  sky 0.003, total 0.038 / 0.118, irradiance 0.065 / 0.173 PASS; shuffled albedo and mirrored directions
  FAIL. Before the second side and the cap: irradiance 0.160 / 0.296 FAIL. The tracer is built by hand:
  g++ -O3 -std=c++17 -static -pthread tests/prtp_reference.cpp, passed with --ref.
- Deterministic: fixed chunks merged in chunk order; 1 thread and all threads give byte-identical files.
- Albedo (src/probealbedo.cpp), LINEAR like FO4CS's G-buffer surfels. Objects: the diffuse map at a
  coarse mip (<= 64 texels) at the triangle's UV centroid, times its vertex color in gamma, then decoded.
  Ground: the terrain splat per 128-unit quad, each pass's map mean times its VCLR, laid in draw order by
  its mean opacity; without a splat, a stated dirt tone (sRGB 110, 100, 85) times VCLR. No map = grey 128.
- Glass is not in the soup today, so rays pass it untinted. Tint is open.
- Interior cells have no sky (`ProbeBakeSpec::noSky`, set from the cell view for an interior): a ray that
  leaves through an opening meets the unloaded void, so its weight is unlinked and every octant's sky is 0.
  Settled in-lane (FO4CS documents exterior bakes only; its Volumetric Air refuses interiors, 2f).

Gate: tests/spells/probe_bake.py. `synth` bakes a scene of known answer (ground plane plus a closed room)
with 1 thread and all threads (must match byte for byte), then re-traces every probe in numpy with its own
Moller-Trumbore and its own model of the surfel sides: sky per octant, distances, link weights (L1),
directions, the unlinked budget, room probes see no sky, ground probes see the sky turned away from the
room, albedo, and no link faces away. Two reds (`--red octant`, `--red normal`) must FAIL. `check <dir>`
runs the structure and budget checks on any baked folder. A `--no-sky` leg re-bakes the synth scene and requires
the same links, zero sky and the sky's weight in unlinked. Sanctuary -20,7 (256 probes), Concord -15,17 (658) and
the Museum interior (975, sky 0) pass.

## 3. Open

- `.tbk` v4: two surfel sides per cell (gives back the refused thin-wall weight), room ids (2f).
- Glass tint in the bake.
- Save names for the PRTP4 capture flights.
