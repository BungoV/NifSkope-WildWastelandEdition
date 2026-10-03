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
   LANDED 2026-10-01: the PRTP band's "Cell lights" row (ships off) swaps the
   cell view to fo4_cell.prog: every placed light on at load (omni + spot),
   interior DALC and directional from XCLL / the lighting template. Gate
   tests/spells/cell_lit.sh (probes vs an independent walk of the ESM; reds:
   linear, axis, off). Not yet: fog (light shapes: see 2z),
   Ambient Only, the PBR (pbrm) and effect shaders.
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

## 2h. The far map -- lane PRTPFAR (2026-10-01)

bungo: "the top down bakes from the division games for far areas ... so that GI works on areas far from you".
The Division's distant shading (slide 57) and FO4CS's own charter for its far tier ("one sky-lit probe hoisted
above each sector's roofline ... stored per sector ... relit with the same live sun/sky feeds", Codex HANDOFF):
a place with no near probes loaded still gets measured bounce light.

`NifSkope -no-gui probefar --lodl <world>.lodl --lodi <world>.lodi --out <dir>` (src/probefar.cpp). Built from
the worldspace's LOD files, never from loaded cells:
- Ground: the `.lodl` heightfield every 8 samples (1024-unit quads), colour = the mean of the `.VT.32.lodt`
  colour sheet over the quad (sRGB decoded to linear). Water: the cell's water plane over quads below it.
- Buildings: the `.lodi` occluder boxes (fitted INSIDE each object's own LOD mesh, so never too big). A box
  raises the roofline of every cell its footprint crosses (a long building over a border first stood
  1583 units over its neighbour's probe; the gate caught it).
- One probe per cell at the cell's middle, 512 units over the roofline (ground top, water, boxes).
- The same bake as the near probes (2g), surfel cell 1024, one `.tbk` v3 file per 16 x 16 cells
  (`sector_X_Y.tbk`, X = floor(x / 16384)); the folder's `far.txt` states the square, the surfel cell and the
  hoist, because the `.tbk` header does not carry the square.
- Sizes measured on the whole Commonwealth (36864 probes, 1.9M triangles): surfel 512 / a file a cell = 731 MB;
  1024 / 16 x 16 = 265 MB, 51 s; 2048 = 161 MB. Irradiance against the brute-force reference (Concord block,
  48 probes): 512 -> 0.011 / 0.026, 1024 -> 0.027 / 0.046, 2048 -> 0.053 / 0.094 (median / p95). 1024 kept.

Gate: `tests/spells/probe_far.py <exe> <lodl> <lodi> <workdir> --ref <prtp_reference.exe>` on an 11 x 11 block
around Concord: heights (every sample inside its cell's stored range), one probe per cell at its middle, the
roofline (each probe at least the hoist over every soup vertex in its cell), colour from the sheet, the
structure check, and the reference gate. PASS; `--red shift` (heights a cell off), `--red hoist` (under the
roofs) and `--red manifest` (no far.txt) FAIL. The FO4CS reader comes last, by standing order.

## 2i. The bounce, relit by the cell's lights -- lane PRTPGI (2026-10-01)

bungo: "simulate the GI for me with lights ... based on the surfels and sky visibility generated".
`src/probegi.{h,cpp}`, run by the cell view after a bake (or `WW_CELL_GI_FROM=<bake folder>`):
1. **Surfels.** Each unique surfel takes the cell's lights (PRTP2 radial ^2.2, spot cone, N.L) with a shadow
   ray from P + 2N to the light, stopping 24 units short (the fixture). Interior directional: unshadowed.
   B = albedo_linear x E.
2. **Probes.** Every link: radiance B over its solid angle w x scale x 4pi, into a six-axis ambient cube
   (E per axis = sum B Omega max(axis.dir, 0)), renormalised by (linked + unlinked) / linked.
3. **Grid.** Voxels (48 units up, <= 400k) next to a surface only; each blends the probes within
   2 x the median probe spacing by (1 - d^2/r^2)^2, **only probes it can see** (a ray voxel -> probe).
   Uploaded as six z-slabs (one per cube axis) of RGBA16F, rgb = E x weight, a = weight.
4. **View.** Legacy: + albedo x E(N) / pi. PBR (`pbrm_cell.prog`): rho x keepInd x E(N) / pi, and the cell's
   lights go through the PBR BRDF (irradiance colour x curve x pi, the sun's convention).
Gate: `tests/spells/cell_gi.sh` (A/B/C/D, independent rebuild; reds noshadow/flip/novis/off).
Open: room ids (a probe and a voxel in different rooms never mix even when a ray slips through a gap); the
sky term outdoors (waits for the weather reader); the bake still ignores glass tint.

### 2j. The cell's imagespace (lane IMGS1, 2026-10-01)

The game's chain after lighting, transcribed from the shipped shader archive (tonemap PS, luminance downsample,
LUT PS), applied to an interior from its CELL XCIM -> IMGS (HNAM, CNAM, TNAM, TX00). Row "Imagespace" in the
PRTP band (ships off; needs Cell lights).
1. **Adapted.** A measure pass redraws the frame into an RGBA32F target (1/4 size) with cell-lit fragments
   writing raw linear light; every other program writes nothing there (renderer.cpp masks it). Mean luma
   (0.2125, 0.7154, 0.0721) over the pixels a cell-lit fragment reached (stencil bit 0), Inf/NaN as 0. The
   game adapts over time; we show the converged value.
2. **Exposure.** clamp(HNAM[8] / (adapted + 0.001), HNAM[5], HNAM[4]); x = 2 x exposure x hdr.
3. **Tonemap.** Hable, E = HNAM[1], white 11.2.
4. **Grade.** Saturation around luma, tint mix (luma x tint, amount), contrast x (brightness x c - adapted) +
   adapted.
5. **Display.** pow 1/2.2, then the 16^3 LUT (256x16 strip, x = r + 16 b, y = g) at c x 0.9375 + 0.03125.
6. **Bloom** (lane BLOOM1). The tonemap PS adds a quarter-size bloom target (bilinear) to the HDR before the
   exposure. That target: a quarter-size copy of the HDR, per tap HNAM[3] x max(0, c - HNAM[2]), a 15-tap
   blur (radius 7, weights exp(-2 x^2 / 49) normalized, integer texel offsets) vertical with the bright pass
   then horizontal plain. Built on the CPU from the measure pass (already a quarter size; a full-size dump is
   box-averaged 4x4), uploaded on unit 11. ASSUMED: the game fills its quarter copy with a 4x4 average (not
   read yet). Only cell-lit fragments receive it (glass/effects get no spill).
Gate: `tests/spells/cell_is.sh` + `cell_is_check.py` (A the adapted mean + exposure from an independent ESM
and archive read, rel 1e-4; B the bloom's size, texels past the threshold and peak vs the checker's own;
P the picture vs the numpy chain + bloom over the full-size dump on opaque cell-lit pixels, >= 97% inside the
3x3 range +-3/255). Reds nolut / noexp / nograde / nobloom FAIL P (nobloom on cells whose bloom moves >= 5%).
Not yet: adaptation over time, exteriors (the weather's imagespace).

### 2k. The lights' shadows (lane SHADOW1, 2026-10-01)

Which lights: LIGH flags 0x400 (shadow spot), 0x800 (shadow hemisphere), 0x1000 (shadow omni). Fallout4.esm
places 750, 17 and 407 of them. The game's receiver (the shipped shader archive's shadow-mask shaders): a spot
compares its perspective depth minus a bias with 9 taps (3x3, a texel apart) / 9; a hemisphere uses a
paraboloid (radial distance / radius minus the bias), with everything behind its plane at 0; an omni uses a
dual paraboloid (not located yet). The default map is 2048, halved per 750 units of distance.
What we draw (no row of its own: part of Cell lights):
1. **Maps.** A depth cube for each of the 16 shadow lights whose reach is nearest the camera (512 a face edge,
   D16, a cube-map array on unit 10). Depth = distance to the light / radius, written by
   `cell_shadowdepth.frag`. The casters are the sun map's (opaque, depth-writing, no effects) minus the
   alpha-tested ones. Casters nearer the light than its near clip (DATA + XLIG delta) cast nothing. A map is
   re-rendered only when its slot gets a new light or the document changes.
2. **Receiver.** `cellShadowF`: the point lifted 1.5 texels along its normal, its distance minus 1 unit,
   3x3 taps a texel apart (each a hardware 2x2 compare) / 9. It multiplies the light in both programs.
   A hemisphere is 0 behind its plane. Its plane faces local +X: 14 of the 17 placed aim it down (measured,
   `scratchpad/shadow1_20261001/hemi_axis.py`).
3. **Light buffer.** A 5th texel per light: slot (-1 none), kind, near clip, XLIG Shadow Depth Bias.
Divergences: a cube instead of a (dual) paraboloid, so the texel at 512 matches a 2048 paraboloid's centre
texel at 1024. No distance halving. The game's shadow budget is unread (we do 16, then unshadowed). The XLIG
bias is read but not applied (the engine's scale for it is unread). Alpha-tested casters
cast nothing (the game alpha-tests them).
Gate: `tests/spells/cell_shadow.sh` + `cell_shadow_check.py`. Probe 7 writes slots 0..2's factors. The check
re-traces each sampled point's ray to the light with its own Moller-Trumbore over the cell's probe soup
(no NifSkope code). It needs >= 85% agreement on re-traced shadowed points and on lit ones, >= 40 of each
(Solomon's frame holds ~50 shadowed; 40 of 40 bounds the share over 92%). Only points the check itself finds in
reach and facing the light count: effect glow spills 1..11 into the probe's "0". First green: Vault111Cryo
98.6% / 99.2% of 7,738, DmndSolomonsHouse01 100% / 98.5% of 908.
Red noshadow (factors read as 1) FAILS. `cell_lit.sh` now pins WW_CELL_SHADOW=0 (its PRTP2 sum is unshadowed).

### 2l. The interior fog (lane FOG2, 2026-10-01)

The game fogs an interior with the weather fog's formula and packing (FOG1, `lookdev_fog.glsl`), fed from the
cell instead of the weather (a room's lighting template would win; we have no rooms yet). Read from the game
code (private notes), every field per its own Inherits flag (XCLL 88): from the lighting template (LTMP ->
LGTM DATA, the same layout) when the flag is set, else from XCLL; a cell without XCLL reads the template.
| Field | XCLL | Inherits |
|---|---|---|
| near, far | 12, 16 | 0x8, 0x10 |
| power, max | 36, 76 | 0x100, 0x200 |
| colours near / far / high near / high far | 8 / 72 / 100 / 104 | 0x4 |
| their scales | 112 / 116 / 120 / 124 | 0x4 |
| height mid, range (near band, far band), high density | 92, 96, 128, 132, 108 | 0x4 |
Clamps after the read: far <= 0 or > 163840 reads 163840; near <= 0 or > far reads 0.17 far. So no interior is
fog-free; 756 of Fallout4.esm's 964 interiors store no near and get 0.17 far. Colours are byte / 255 x scale,
written straight to the sky's fog colours (no time-of-day blend), then pow 2.2 at packing like the weather's.
Height is world z. No fog sun indoors (INFERRED: the directional fog term follows the sun).
Both cell programs fog once, linear, before the imagespace (the PBR program skips its weather-fog line on a
cell-lit draw). Pin WW_CELL_FOG=0 publishes none; the census echoes the fields and where each came from.
Gate: `tests/spells/cell_fog.sh` + `cell_fog_check.py`: its own plugin walk and its own copy of the formula,
against fog probe 6 (alpha, height blend) and 7 (colour) at positions from probes 2 + 3, the camera from
WW_CELL_CAM_DUMP. Fog probe 8 echoes the distance and height each fragment's fog read; only pixels where that
matches the position probes count (at least 60% must), since some surface over Solomon's house and the Vault
served the fog probes and not the position ones (named in MISTAKES.md). Found in lane EFX1: the effect shapes
(the Vault's steam), which take no cell uniforms; probe passes now skip them (2o) and the same-surface share is
100% in all three cells (was 74-83%).
Reds noclamp / nogamma / noinherit must each FAIL in at least one of the three cells.

### 2m. The game's diffuse: Oren-Nayar (lane ON1, 2026-10-01)

The game's legacy (spec / gloss) light shaders and its sun do not use Lambert: their diffuse is Oren-Nayar
(read from the FO4CS transcription of the shipped deferred light shaders, which was itself read op for op):
sigma = 1 - gloss, A = 1 - 0.5 s2 / (s2 + 0.57), B = 0.45 s2 / (s2 + 0.09), the azimuth cosine taken from the
UNnormalised tangent-plane projections of V and L (a quirk of the game's code; the textbook form normalises
them), x sinL sinV / max(NdotL, NdotV); diffuse = (max(cosPhi, 0) B geom + A) x NdotL. A rough surface lit
head-on reads about a third darker than under Lambert, and brighter toward grazing back-light.
`cell_lights.glsl` cellOren multiplies each placed light's diffuse and the cell's directional light on the legacy
program; the ambient, the bounce and the reflection's lighting stay as before. The PBR program is unchanged.
Probe 1 stays the Lambert irradiance (cell_lit.sh); probe 8 is the Oren-Nayar sum / 4 seen from the camera, probe 9
the gloss it used. Gate `tests/spells/cell_oren.sh` + `cell_oren_check.py` (lights from cell_lit_check.py's walk,
the diffuse written out again), two Vault111Cryo views (CELLS entries "EDID@x,y,z"; Solomon's house was dropped,
its view holds 2 lit legacy pixels). Red `lambert` must FAIL in at least one view; too little data is SKIP (a
green failure, never a red's). `WW_CELL_LIT_RED=normalised` (the textbook cosPhi) stays a lever but is not a red:
measured, its gap to the game's form peaks at about half the 8-bit tolerance in the Vault (p99 0.53x), so no
check at this precision can fail it.

### 2n. The game's back-light rim term (lane RIM1, 2026-10-01)

Every legacy light shader (point, shadowed point, spot) and the sun add a second diffuse term next to the BRDF:
rim = saturate(dot(V, -L)) x (1 - NdotV)^0.01 x (1 - gloss), times NdotL and the light, so the shadow and the
radial / cone weight scale it like the diffuse (read from the FO4CS transcription of the shipped light shaders;
the PBR branch drops it). The (1 - NdotV)^0.01 is about 1 except exactly head-on. It is bright where the camera
looks toward a lamp across a rough surface. `cell_lights.glsl` cellRim adds it to cellOren's factor for the
placed lights and the cell's directional light; probe 8 carries the sum. Red `norim` (WW_CELL_LIT_RED, bit 32)
drops it; measured before the build, 7.9% of Vault view 1's lit pixels move past the tolerance (lit share falls
to about 92%, under the 95% bar). Gate: cell_oren.sh, reds `lambert` and `norim`.
Two LIGH flags opt a light out. The game compiles a separate light-shader variant for each, and the shipped
variants were compared pairwise (same variant with and without the bit, instructions diffed):
"No Rim Lighting" (0x80000, 15,208 placed refs) removes exactly the rim and nothing else; "Ignore Roughness"
(0x40000, 56 refs) removes the rim AND the Oren-Nayar shaping (the diffuse is max(NdotL, 0)), and leaves the
specular as it was. No shipped variant with either bit carries the rim. Most interiors are 50-70% No-Rim lights
(Institute Concourse 1,125 of 1,753); Vault111Cryo is the outlier at 42 of 843. celllights.cpp packs the two
flags with No Specular into the light's 4th texel's w (1 noSpec, 2 noRim, 4 ignoreRoughness); the notes echo
`norim=N ignorerough=N`. Red `rimflags` (bit 64) ignores the two flags.
Probe 8 cannot see them: over 22 shot sets, the pixels where honoring the flags moves probe 8 by more than twice
the tolerance were 0-126 (InstituteConcourse 6 of 18,029 lit), and the red passed. A no-rim light's rim is
small beside the whole diffuse at /4. Probe 10 writes the placed lights' rim alone x 4 (cellRimSum, set by
cellSumLights); offline, from the position / normal / gloss probes already shot, honoring the flags moves it past
twice the tolerance on 12,178 of Vault view 1's 180,685 clean pixels and 1,157 of the Institute's 29,446.
cell_oren_check.py judges it over the pixels where either side shows a rim (at least 95% within tolerance).
Result (exe 2026-10-01 16:34): green 100% / 100% (rim 100% / 99.1%); red rimflags FAILs view 1 (rim 43.4%),
norim and lambert FAIL both views (rim 0%). The checker's neighbour-gloss limit went from 2/255 to 12/255:
at 2 only flat-gloss surfaces survived, and view 2 had passed on the untextured steam sheets (gloss 0.97);
with no limit at all its pixels still agree 99.9%.

### 2o. Effects set in the NIF; refraction-only shapes (lane EFX1, 2026-10-01)

A BSEffectShaderProperty that names no .bgem keeps its whole look in the NIF. The cell view drew those through
the lit program, untextured: the Vault 111 ground steam (MistGroundWaterSteam02Mini.nif) came out as flat dark
shapes on the cryo floor (bungo, circled). lodgen keeps the property's serialized block (LodSrcShape effectBlock,
cell view only); cellview writes it back as the bucket's effect property with its controller link cut. The
welded scene holds one frame, so a looping float controller on it (Base Color Scale, falloff opacities, alpha)
is drawn at its time-weighted mean over the keys. Vault111Cryo: 107 such buckets.
A BSLightingShaderProperty with Shader Flags 1 bit 15 (Refraction) shows in game only as a bend of what is behind
it; WaterSplashDrips.nif's scrolling ring takes a normal map as its diffuse. Drawn lit, it was a solid swirled
disk on the walkway. lodgen keeps the source's Refraction Strength (LodSrcShape refractStrength); cellview keys
such shapes into their own buckets ("R|strength") and writes bit 15 and the strength back, so the viewer's
screen-space refraction preview (renderer.cpp, second pass) bends the scene behind them (Vault111Cryo: 3 buckets).
Measured: WW_RENDER_REFRACTION=0 on the same camera brings the solid disks back (36k px differ at 1920x1080),
so the preview, not a skip, is what hides them. A probe pass skips them like the effects (bsshape.cpp).
The effect program takes no cell uniforms, so in a harness probe pass (WW_CELL_LIT_PROBE, WW_CELL_FOG_PROBE) an
effect wrote its own colour over the surface measured; bsshape.cpp skips effect shapes there (wwCellProbePass).
Pictures keep every effect. lodgen: the block is read after every field the bake uses, so the bake is unchanged
by construction; lodgen_native_baseline --check shows the same 8 stale region / arrays files as before.

### 2p. Effects in the exposure measure (lane EXPO1, 2026-10-01)

The game's effects land in the HDR target that its eye adaptation and bloom read. The measure pass (probe 6) masked
every program that was not cell-lit, so the steam never raised the exposure. The renderer now lets the effect
programs draw into the measure too (wwCellImageSpaceMeasuresEffects); the cell-lit effect program writes its
linear colour there (2q). Red: WW_CELL_IS_RED=nofx (the effects masked out of the measure, as before).

### 2q. Effects drawn the game's way (lane EFX2, 2026-10-01)

The Vault's cryo walkway showed a white haze the game frame does not have. The game's effect shaders (the effect
groups of Shaders011.fxp, transcribed) say why:
- No room light. The effect vertex shader sums no light; the pixel shader's only lighting term is
  mix( c, c x emit, lighting influence ), emit a colour scripts set (white when unset). The viewer's effect shader
  multiplied by its own view light (D). Dropped in the cell view.
- Soft effects (BGEM Soft / SF1 Soft Effect): alpha x saturate( (scene depth - depth) / soft depth ) x a near fade
  smoothstep( 0.075, 0.5, depth / soft depth ), depths in game units. For a greyscale-to-palette effect the fade
  scales the palette's row, not the alpha. The game subtracts a near term inside the near fade; it is inferred to
  be about one unit and read as none. The opaque depth is blitted once per transparent pass (Scene::grabEffectDepth).
- Fog: the surface fog formula; a blended effect mixes toward the fog colour, an additive one is scaled by (1 - f).
- Linear colour (texture and base colour decoded with 2.2), then the cell's imagespace on its own pixel; the bloom
  is not added again (the frame under it already has it). Approximation: the game blends in HDR and tonemaps the
  sum; we tonemap each effect pixel before the blend. A later lane could composite from a linear frame.
The cell-lit variant is fo4_effectcell.prog (fo4_effectshader.frag compiled with WW_CELLLIGHTS + WW_CELL_FX); the
renderer swaps it in by name when cell lights are on. Reds: WW_CELL_FX_RED=legacy | nosoft | nolin | hide.
Gate tests/spells/cell_fx.sh (imagespace off, two Vault111Cryo cameras): nothing but the effects moves, the fades
only take away, and the effects' mean change is 0.39x (walkway) and 0.86x (far end) the viewer shader's.
Under the nosoft red that ratio is 0.999 / 0.998: the Soft fade, not the dropped light or the fog, removes the
haze. Walkway, imagespace on: frame mean 109.6 -> 93.6 (out of 255). cell_is.sh 3/3 PASS with the effects in the
measure (Vault: the steam leaves 4% of the frame as opaque cell-lit pixels to compare, 99.71% within 3/255).

### 2r. Ambient Only lights scale the ambient (lanes AMBO1 + AMBO2, 2026-10-01)

A light with LIGH flag 0x100000 lights nothing directly (AMBO1 drops it from the direct lights). It changes the
cell ambient inside a sphere of 1.22077 x its radius (or inside its linked box, 2z), radius = base + XRDS (XRDS is a delta). Inside, each
channel's ambient sum (the DALC rows dotted with (N,1)) is multiplied by pow(color/255, 2.2) x dimmer before the
ambient's own 2.2; dimmer 0.5 leaves about 0.22 of the ambient. Per pixel: the first light in plugin order that
holds the point wins, and it replaces the ambient, never adds. No edge fade, no camera rule. The game culls the
light by its plain radius against the view; not modelled (a screen-edge strip only). Vault111Cryo's three spheres:
(-1011,1911,-69) 1677, (-2410,32,30) 668, (-3594,-222,157) 1264 (the radii first written here were XRDS alone).
Census: 39 placed in 23 cells, at most 5 in one cell; the shader takes 16. Summary "ambientonly=N
ambientvolumes=N". Reds WW_CELL_LIT_RED=ambientlit (drawn as direct lights again; run with CELLS=Vault111Cryo,
Solomon has none) and ambientfull (the scale ignored). Gate cell_lit.sh's Ambient Only view (look-at
-4200,-250,0, eye 800 away; probe 11 = the ambient sum x 8): inside 100.0% of 410,294 px, outside 99.9%; red
ambientfull 0.0%.

### 2s. Camera-facing glow cards (lane GLOW1, 2026-10-02)

A shape below an NiBillboardNode is turned to the camera by the game. The cell view welded it into the cell
flat: the Vault's GlowFillCloudy discs on the cryo pod bases (Effects\Ambient\GlowFillCloudy.nif, node
GlowMesh128, mode 4; material AmbGlowFillCloudyHalf.bgem through a material swap) lay horizontal, edge-on from
eye height. Now the generator's loader records the nearest billboard ancestor per shape (transform and mode;
geometry untouched, bakes do not move: lodgen_native_baseline.sh --check 24 of 24 identical), and the cell view gives each
such shape, per placement, its own bucket under its own NiBillboardNode (translation = the world pivot, scale =
placement x node), which the viewer turns to the camera. Every mode is turned the same way (card plane = screen
plane). Cap 8192 per cell, the rest welded flat and counted. Census line "billboards: N shapes turned to the
camera, M welded flat". Vault111Cryo: 165 (167 on 10 models, 2 under an opposite-state enable parent).
Rides the Cell lights row. Red WW_CELL_GLOW_RED=1 (welded flat as before).
How much it shows: little. The material's alpha is 0.2 and the effect shader applies it twice (0.04), the base
texture's alpha is at most 115/255, so a card is under 2% opaque. Measured over the flat reference, imagespace
off: from the walkway's start (cards 700 to 930 units away, overlapping) the best circle gains a mean +1.93/255;
beside a pod (220 to 430 units) the best gains +0.32. Holding the Soft fades at 1 does not change the near
figure (+0.040 against +0.029 over the frame). With the imagespace on the frame moves by about one level
(the exposure measure sees the cards). So this is the haze, not a pool of light on the floor: the floor under
each base is lit by the placed lights there (radius 46 fade 7.82, radius 72 fade 2.34, radius 97 fade 1.88,
all drawn since 2a) and by the bloom (bloom on - off beside a pod: max +88/255, >= 8 levels on 0.62% of the
frame).
Gate tests/spells/cell_glow.sh (checker's own plugin and mesh walk; each camera names its stages): K the
census count = the walk's (165 = 167 - 2), N 99.999% of 235,914 px outside the predicted circles unmoved (near
camera), C 5 of 33 circles gain >= 0.5/255 (far camera). Red flat: K 0 turned, C 0 of 33.
Not done: the references' emittance (XEMI) tint on the cards (white is used; it can only tint or darken).

For section 3 (Open), one line:
- Vault 111 pod bases: the glow cards are drawn (2?) but add under 2%; compare in game beside a pod whether the
  spill is the haze or the lit floor (lights + bloom).

### 2t. The walkway's highlight pools; the lights' specular held by a gate (lane POOL1, 2026-10-02)

The bright pools on the Vault 111 cryo walkway floor in the game are not the placed lights' specular. Read from
the game's shipped shaders:
- A light flagged Non Specular (LIGH 0x8000) is drawn with the light shader variant that writes zero to its
  specular target. Of the 29 lights nearest the walkway 14 carry the flag; the rest add a broad, weak specular
  (floor gloss about 0.43: mean 0.023 linear before the 0.37 mask), in the game and in the cell view alike.
- The pools are the composite's env term: out += refl x envI x envScale x D, with
  envI = mask x 3 x min(sqrt(sat(gloss - 0.3)), 1), envScale = the material's env mask scale (at most 50),
  D = 3 x the pixel's own diffuse light (no albedo), and
  refl = lerp(cube, screen-space reflection colour x a scale, min(its confidence x a scale, 1)).
  The floor's material (V111HallFloor01) has screen-space reflections on, env scale 1.5, _s red mean 0.37, green
  mean 0.433: a full-confidence reflection adds about 0.6 x the reflected scene colour x D, against about
  0.004 x D from its dim cube map. Because of D it peaks under each lamp: the pools. The neighbouring materials
  (V111Concrete02, V111Metal04) have the reflections off.
- The reflection chain in the shipped shaders: a ray setup pass (the view ray reflected about the gbuffer normal,
  only where the material's flag is set); a 32-step march over a depth pyramid (starts 4 mips down, a 4x4 dither
  on the start, confidence = centre-of-screen fade x ray-length fade over half a screen x depth-gap fade,
  squared); a 5-tap blur run twice (weights 0.0939 0.2042 0.3040 0.3040 0.0939, taps without a hit skipped);
  then the composite above.
Not built: the cell view draws no screen-space reflections. Open before a lane can build them: what fills the
pass's four scale constants (colour scale, the ray's facing gate, the normal's vertical scale, the confidence
scale), and whether the reflected colour is this frame's or the last one's. The result enters through the env
term (lane CUBE1's).
Built: nothing drawn changes. The Non Specular flag (already honoured) is held by a gate. tests/spells/
cell_spec.sh: probe 30 = the placed lights' specular / 4 before the mask; cell_spec_check.py rebuilds it per pixel
from the plugin's lights with the game's form (n = 2^(10 gloss + 1), D = NdotH^n (n + 2) / 2 pi, the geometry
select, Schlick at 0.2, min(D G F / 4, 15) x pi). Two views of the walkway: agree 100.0% / 100.0% (where a
highlight shows 100.0% of 871 px / 99.8% of 412 px), viewer total / expected total 0.998 / 1.003 (bar 5%). Red
WW_CELL_SPEC_RED=nonspec (the flag ignored): 85.4% / 61.0% where shown, totals 1.117 / 1.341, FAIL in both
views. Left open: the cell view multiplies the lights' specular by the material's specular colour; the game's
deferred lights read only the specular scale (white on this floor).

### 2u. The game's screen-space ambient obscurance (lane AO1, 2026-10-02)

The game darkens creases, corners and the ground under clutter with a screen-space pass. Its settings are the
INI's, the same in every cell: radius 108.2, bias 0.6, intensity 7.1 (game units). Nothing comes from the cell,
its imagespace or its lighting template. It runs at half the view from the opaque pass's depth and normals:
- depth mips, each the min of 2x2 of the one above (5 levels);
- per pixel 5 taps over 2 turns (angle step 2.512) on a disc of radius x 100 / depth pixels, each tap's depth
  read from the mip floor(log2 reach) - 3; A = max(0, 1 - intensity x sum f^3 max((v.n - bias') / (v.v + 0.01),
  0) / r^6), f = max(r^2 - v.v, 0), bias' = bias + 10 max(d - 0.3, 0) + 5 |ndc|^2, d = depth / 7000;
- the tap pattern turns by a random angle each frame (only up to depth 3500) and the game keeps 0.99 of the
  history, so a still view shows a time average. The history starts over from the frame's value when that is
  0.95 or more and the history is under 0.7;
- a bilateral blur across, then down: 7 taps 2 pixels apart (0.153170, 0.444893, 0.422649, 0.392902), cut by
  2000 x the depth-key difference.
It multiplies everything the game's deferred composite writes (direct light, ambient, specular, emissive,
reflections) before the fog. Blended surfaces and effects are drawn after it and do not take it.

In the viewer (part of the Cell lights row, no menu row, no INI key): wwCellAoPass draws the opaque cell-lit
shapes once more (probe 20: view normal + depth in game units) into a full-size float target, cell_ao.frag runs
the mips, the raw pass and the two blurs at half size, and the cell programs multiply their lit color by the
bilinear sample before their fog (never when blending is on). A still view = the mean over 8 evenly spaced
angles, plus the history restart as a closed-form average (it can fire on 0.4-0.5% of pixels and lifts them by
about 0.2). Cost 3-22 ms a frame at 960x600. The texture sits on unit 16 of the cell programs.

Pins: WW_CELL_AO=0 (none computed), WW_CELL_AO_RED=off (computed, not applied) | radius (half) | noblur |
noreset (the plain mean), WW_CELL_AO_DUMP=<file>.

Gate tests/spells/cell_ao.sh + cell_ao_check.py (an independent numpy rebuild from the dumped depth and
normals, effects hidden in both windows), Vault111Cryo / DmndSolomonsHouse01 / GoodneighborTheThirdRail:
- E the dumped normals belong to the dumped depth's surface: 97.4 / 93.7 / 83.9% (x unmirrored: 58.7 / 64.2 / 56.2%)
- A the raw obscurance within 0.01: 100% each
- R the history restart vs a frame-by-frame run: mean |d| 0.0061 / 0.0047 (third cell: 4 px, not judged)
- B the blur within 0.01: 100% each
- C the light the picture got (with / without) vs the rebuild, within 0.02: 100.00% of 523,868 / 482,695 /
  112,773 px = 97 / 92 / 99% of the geometry
Reds on Vault111Cryo, each fails its stage: off -> C 48.89%, radius -> A 56.22%, noblur -> B 58.59%,
noreset -> R mean |d| 0.1939.

With the imagespace on, the obscurance lowers the measured light and the exposure rises a little, as in game
(the door-room picture's mean goes 61.2 -> 61.5 of 255 while its creases darken).

Assumed, not measured: the depth mips are point sampled, the composite's upsample is bilinear, and 8 evenly
spaced angles stand in for the game's continuous random angle (closed form vs frame-by-frame: max 0.068).
Open: compare one in-game still against the viewer at the Vault 111 cryo walkway; these three assumptions are
what such a capture would settle.

### 2v. Placed models drop their root transform; a cell opens in its first-load state (lane MISS1, 2026-10-02)

bungo circled two spots on the Vault 111 cryo walkway where geometry was missing. Located first (camera of the
marked shot: look-at 384,-480,60, view 3, distance 260, FOV 70; both marks lie on the plane z = -74):
- Mark A, world about (124,-699,-74): ref 0000194b STAT V111RPit2WallMid02
  (Interiors\Vault\Vault111\Rooms\V111RPit2WallMid02.nif), shape "V111RPit2WallMid02:25", the alpha-tested
  floor grate quad.
- Mark B, world about (310..446,-347,-74): ref 0000192f, same model, shapes ":25" (grate) and ":18" (trim).
Every other shape the plugin puts in the two boxes (16 references each) was already drawn in place. No decal
lies in either mark (one TXST reference within 60 units of mark B, none at A); no actor within 60 units.

The rule. A placed model's ROOT node takes the reference's transform; the transform stored on the root in the
model file is not applied. V111RPit2WallMid02 stores a half turn about Z there, so the cell view drew the wall
piece turned around (its box 150.5 units off) and the grate landed under the wall. Proof from the game's own
data: the cell's combined meshes (Fallout4 - MeshesExtra.ba2, meshes\precombined\<cell form>_<hash>_oc.nif)
store each instance's transform and world bound; over Vault111Cryo 57 shapes of root-carrying models fit only
"root dropped", 0 fit only "root kept" (4 either, 5 neither); InstituteConcourse 16 / 0 (139 either).
Game-wide 305 of 19,912 loose placeable models carry a root transform.

The start state. A reference with an enable parent (XESP) starts shown when the PARENT's start state differs
from the XESP "opposite" bit; the parent's own state comes from its initially-disabled flag or its own parent.
The cell view used only the reference's own flag and the bit. Vault111Cryo: 88 parented, 86 change (parents
002075e4 and 000481d4, both initially disabled; all in the pod room and the entrance). InstituteConcourse:
1939 parented, 1592 change (XMarker 00248509 alone holds 900 the old view drew and the game starts hidden).

Built (commit 0e82541b on miss1-20261002; main 5431a9f2 merged in as 8f224946, rebuilt, every gate below
run on the merged exe):
- src/lodgen.cpp/.h: lodgenNativeLoadModelPlaced(), the node chain without the root. src/cellview.cpp loads
  placed references through it and follows the enable-parent chain (a parent outside the loaded block counts
  as enabled). Markers\BlackPlane01.nif is drawn (no marker flag on its base; all 19 references are in the
  cell's combined list). src/esmdata.cpp: a clothing record's world model is MOD2 / swap MO2S.
- The marker flag (0x00800000) is honoured for ACTI, DOOR, FURN, STAT only, as the record definitions have it.
- Gate tests/spells/cell_refs.sh + cell_refs_drawn_check.py (own plugin walk, own model reader). Per cell five
  rows: census (plugin = viewer's list), drawn (every reference with a model the game shows at start is
  drawn), hidden (none of the others is), placed (each drawn box against a box rebuilt from the model file
  without its root, bar 0.05 units), anchor (the combined meshes side with "root dropped").
  Green, 14 rows: Vault111Cryo 3087 references, 1397 / 1397 drawn, 1690 others 0 drawn, 1394 boxes worst
  0.001; DmndSolomonsHouse01 200, 193 / 193, 7; InstituteConcourse 6714, 3960 / 3960, 2753, 3954 boxes worst
  0.001. Named SKIPs: Dmnd has no root-carrying model (anchor); platformhelperfree01 not judged.
  Reds on Vault111Cryo, each FAILS: WW_CELL_REFS_RED=root (the old placement) fails the placed row, worst
  error 150.485 against the 0.05 bar, 12 boxes over it, the three V111RPit2WallMid02 references first;
  WW_CELL_REFS_RED=parent (the old start rule) fails the drawn row (19 the game shows are not drawn: ceiling
  lamps, light beams, fixtures of the pod room) and the hidden row (7 the game hides are drawn).
- tests/spells/cell_open_check.py and cell_glow_check.py mirror the root rule, the black plane and the
  enable-parent rule. Existing gates re-run green on the merged exe: cell_open.sh (placement boxes, its own
  copy of the rules), cell_glow.sh (the cards' transform now comes from the root-less chain; stage K reads
  167 turned, 0 on references that start disabled, where the old rule gave 165 + 2), cell_fx.sh (effect meshes
  go through the new load), cell_lit.sh (the lit room whose walls moved). cell_lights.sh is not reached: the
  light list is unchanged.

Open:
- The light list's on/off rule still ignores enable parents: Vault111Cryo 28 of 849 placed lights are lit by
  the view and off at the game's start; InstituteConcourse 1265 of 1776. One rule, one place (the light
  list in src/cellview.cpp); left to the light lanes.
- The LOD bake and the near bake still compose the root transform.
- Weapons assembled from parts (the 10mm pistol in Vault111Cryo), particle-only effects (FXDripsBig,
  FXSteamVent01), decals and actors are not drawn.
- "A parent outside the loaded block counts as enabled" has no test: no such reference in the three gate
  cells or Vault75.
- The cell view has no way to show a later quest state (the roughly 1,600 Institute references that wait on
  a quest are now hidden). A view option is bungo's call.
- 5 shapes in Vault111Cryo's combined meshes fit neither rule; not explained.

### 2w. The interior cube map reflection at the game's strength (lane CUBE1, 2026-10-02)

Indoors the game adds the environment cube map in its final composite, not in the material's light pass, and
it is far weaker and far less "mirror" than the viewer drew it. Read op for op from the game's composite
shader (the same listing lane EXPO1 worked from):
reflection = cube(R, mip) x 3 x spec x min(sqrt(sat(gloss - 0.3)), 1) x min(envScale^2 x 50, 50) x D,
where spec = the material's specular scale x the _s map's red (saturated), gloss = the material's smoothness
x the _s map's green, envScale the material's "Environment Mapping Mask Scale" as the game packs it, and D
the DIFFUSE light that reached the pixel (ambient + lights, shadows included). mip = (1 - gloss) x 6 +
view depth x 0.001953. So: a surface in the dark reflects nothing; a surface with gloss under 0.3 reflects
nothing; there is no fresnel, no specular color and no env mask texture in it. Only a material that names its
own env map reflects: indoors there is no cell-wide fallback cube, so a material without one gets none (the
viewer used to give those the default cube).
The game keeps its env cubes in an sRGB array (128 x 128, 8 mips), so its sampler decodes each texel BEFORE
the filter and the shader decodes nothing. renderer.cpp wwCellCubeDecodeMode reads the bound cube's internal
format: an sRGB-tagged cube is mode 3 (the sampler decodes: the game's order), an untagged one mode 1 (decoded
in the shader after the filter, the nearest an untagged texture allows). `cellCubeMat` carries (spec scale,
smoothness, mode, env scale); mode 2 = a .pbrm shape, which keeps the PBR program's own image-based law, 0 =
no own cube. `cell_lights.glsl` cellCubeGame returns the term's factor K and cellLit adds K x the diffuse
light (Ed). Both programs take it: fo4_default.frag (legacy) and pbrm_default.frag (a BGSM drawn through the
PBR program, WW_CELL_PBR); the PBR program got the _s map on a new sampler `CellSpecMap` for this. The effect
program is untouched (the block is compiled out under WW_CELL_FX).
Red `cubeold` (WW_CELL_LIT_RED, bit 512) is the old law: texel squared x env scale x the Lambert sum, and the
default cube on materials without their own.
Probes (WW_CELL_PROBE): 50 = K / 4; 51 / 52 = the uv's fraction in 16 bits (u, v) with the material tag in
blue; 53 = the normal's rounding residual. `WW_CELL_CUBE_DUMP=<file>` writes `tag|material` rows so a checker
can name each pixel's material.
Gate `tests/spells/cell_cube.sh` + `cell_cube_check.py`: the checker shares no code with the viewer (its own
BGSM reader, its own BA2 / DX10 reader, BC5 decode, cube face table, mip chain and trilinear filter); it
rebuilds K per pixel from the position / normal / uv probes and the material files and compares it with probe
50. Bars: at least 500 steady pixels (fewer = SKIP), 200 lit, 97% within 3/255 + 5%, 95% of the lit ones.
Two Vault111Cryo views x both programs. A start that comes up without the game's archives (seen once in 70)
is shot once more and noted in the log.
Result (exe 2026-10-02 11:11, main a1e25b20 merged in, branch head 4a9ebe9b): green PASS in 4 of 4 views,
agree 99.9 / 99.9 / 99.9 / 100.0% (lit 99.8 / 99.9 / 99.9 / 100.0%), viewer/expected 0.996 / 0.995 / 0.984 /
0.992, over 18,508 / 10,497 / 4,523 / 4,506 steady pixels (view 1 legacy, view 1 PBR, view 2 legacy, view 2 PBR).
Red cubeold FAILS 4 of 4: agree 31.8 / 32.7 / 31.9 / 2.2% (lit 0.4 / 0.4 / 0.4 / 0.0%), viewer/expected 0.157 /
0.078 / 0.154 / 0.082: the old law drew about a tenth of the game's reflection on lit metal.
The obscurance (lane AO1) multiplies the lit colour after cellLit, so it scales this reflection too, as the
game's does; probes 50-53 write the output after that multiply and are not touched by it.
With the lane merged, cell_lit.sh, cell_oren.sh and cell_spec.sh stay PASS (run on main 5431a9f2 merged).
Not a red: the decode order. The checker with decode-before-filter still passes shots drawn with
decode-after-filter (agree 99.0-99.9%, viewer/expected 0.964-0.993): on these 128-pixel cubes at the mips the
Vault's materials use, the two orders differ by less than the 8-bit tolerance. The order follows the game's
texture format, not the gate.
Open: (1) an untagged cube (not seen in the Vault; a mod's uncompressed cube without the sRGB format) is
decoded after the filter; (2) the env scale's packing (x^2 x 50, capped at 50) is read from the composite, the
material side that writes it was read in lane notes only; (3) exteriors are out of scope: there the game also
blends a cell / sky cube, which this lane does not draw.

### 2x. Decals draw before the blended pass: no dark marks in the haze (lane FXD1, 2026-10-02)

bungo circled dark spots "passing through the particles" at the far door of the Vault 111 cryo walkway (camera
of the marked shot: look-at 384,-480,60, view 3, distance 260, FOV 70, 1600x1000; eye 644,-480,60). Located
first, with the effects hidden and shown and the two position probes (lift = shown - hidden, summed over RGB):

| mark | pixel | world | distance | lift before -> after (neighbours) | shape under it |
|---|---|---|---|---|---|
| upper left | 730,410 | -491,-594,178 | 1147 | 18 -> 410 (411) | ref 00001932 V111RWallEx01.nif, its decal shapes ":14" (V111GreebsAlpha01DECAL.BGSM) and ":34" (V111LabelSet02.BGSM) |
| mid left | 707,447 | -461,-628,116 | 1116 | 7 -> 257 (261) | same wall |
| upper right | 852,420 | -463,-396,160 | 1115 | 0 -> 428 (426) | same wall |
| mid right | 839,446 | -494,-415,120 | 1142 | 168 -> 351 (352) | same wall |
| lower right | 883,511 | -455,-347,14 | 1108 | 60 -> 217 (222) | ref 00001931 V111RWallCrL01.nif ":34" |
| lower left | 672,511 | -455,-683,14 | 1119 | 60 -> 156 (156) | ref 00001933 V111RWallCrR01.nif ":14", ":34" |

Every one is a decal of the lighting shader with alpha blend (vent slits, stencilled labels), on an opaque
wall about 1100 units away, behind all the walkway's haze. The same holds for the floor stripes
(V111FloorStripeRestricted03, V111LabelSet03.BGSM) and the cryo pods' label decals nearer the eye. It is not
the soft fade: `WW_CELL_FX_RED=nosoft` and `legacy` leave the marks as they are.

The mechanism. A cell welds each material's shapes into one bucket and every bucket has the same origin. The
second pass sorts by the origin's view depth with a stable sort, so in a cell it keeps the buckets in the order
they were made: effect buckets first, material-named buckets after. A blended decal therefore drew after every
haze card and laid the bare wall colour (times its alpha) over the haze in front of it.

The game's order (Todd's treat): decals, opaque and blended, are drawn before the sorted blended pass. So nothing was changed about depth writes or the sort;
`Scene::drawDeferredShapes` now draws the lighting shader's second-pass decals first
(`Shape::wwDecalDrawsFirst()`: decal by the shader flags or by the material file, not refraction; Cell lights
on only, so a plain NIF view is untouched). On bungo's frame 725 pixels change and every one gets brighter;
with the imagespace on, the upper-left mark goes [59 60 49] -> [207 206 196]. In a bright room the same rule
dims a decal by what the haze takes from its neighbours of equal brightness (held-out camera -4600,-280,120
view 5: 444 pixels, all darker; band by band of surface brightness their lift is within 3 levels of the
unchanged neighbours', e.g. -30 against -30 and -34 against -32).

Gate `tests/spells/cell_fxdepth.sh` + `cell_fxdepth_check.py` (independent, no shape list). Per camera: effects
hidden, the run under test, probes 2 and 3. A pixel's peers are the pixels within 12 px whose surface is at
the same distance from the eye (within 4 %); a nearer surface is no peer, which is how the checker leaves out
the geometry in front of the haze. A judged pixel is a hole when it keeps under 0.6 of its peers' median lift.
Not judged: silhouettes (3 x 3 distance spread over 2 %; the picture is smoothed there and the probe is not --
89 false pixels without this rule), pixels brighter than their peers, and places where the peers disagree
(lower quartile under 0.85 of the median: an effect lying on the surface, such as the pod's frosted pane).
- R: enough judged pixels. 306,192 and 178,032 (>= 20,000).
- D: hole pixels in groups of >= 3. Green 0 and 0 (bar 0). Red `--red late` (`WW_CELL_FXD_RED=late`, the old
  order): 65 px in 9 groups and 44 px in 5 groups, FAIL on both cameras (the red must reach 20).
- Thresholds were set on the two cameras before the gate ran; the clean frames stay at 0 up to 0.75. A third
  camera never used for that (-4600,-280,120) is clean too (0 holes, 106,976 judged) but has no subject for
  the red, so it is not in the gate.
Re-run on the same exe: `cell_fx.sh` PASS (it shoots the haze through the changed pass), `cell_is.sh` PASS
(the final picture of the same room), `cell_glow.sh` PASS (K 167; the glow cards share the second pass).

Open.
- The red catches four of the six marks; the two label marks are brighter than their neighbours without the
  haze, and the checker does not judge brighter pixels.
- Census of Vault111Cryo's blended lighting-shader shapes (269 models, 1423 placements): 301 placed shapes carry
  the decal flag and a material file (drawn first when the file says decal, as for every mark above), 58 carry the decal flag on the model only, with no material
  file (NpcPipboyGroundTake01 "ScreenDust:0", V111GearDoorConsole01 "Lid:1", ...), 0 are plain glass. The
  cell's lighting bucket writes fixed shader flags without the decal bits (src/cellview.cpp, the bucket
  writer), so those 58 are not known as decals and stay in bucket order after the haze. The gate finds no hole on
  the three cameras. Carrying the flag into the bucket changes bucket keys, so it was left for a lane that owns
  the bucket writer.
- Seen, not judged: the cryo pod's window frost (CryoPod02 "Window:9", CryopodFrost.BGEM, effect shader with
  depth write) draws a hard-edged bright streak at 332..342, 430..480 of bungo's frame, the same before and
  after this change.

### 2y. `.tbk` v4: both sides of a thin wall, room ids, glass tint; the bake sees the fixed world only (lane BAKE4, 2026-10-02)

The rule: the bake sees the fixed world only; items, actors, corpses, decals and effects are receivers.

What changed. A cell of the surfel grid (70 units) seen from both sides of a thin wall used to keep one side;
the other side's light was refused as "turned away" or housed in the next cell (2f). Version 4 keeps the second
side in the same cell as a back surfel, and each ray links the side whose face it hit. Every probe names its
room; a probe in an opening names both sides. Light passing a glass pane is tinted by it. v3 is still read by
the cell view; `probebake --tbk 3` writes the old file byte for byte; the far map (2h) stays on v3.

The file (little endian; the v3 body is unchanged):
- header, 64 bytes: int32 magic, version (4), kind, cellX, cellY; float surfel cell size (70); uint32 surfel
  count ns, probe count np, link count nl, flags; uint32 reserved[6]. reserved[0] = back surfel count nb,
  reserved[1] = room box count nx, reserved[2] = what was modelled (bit 1 sides, 2 rooms, 4 doors, 8 glass).
- v3 body: ns surfels x 32 bytes, np probes x 144, nl links x 12.
- v4 tail, in this order:
  1. nb back surfels x 32 bytes, same layout as a surfel, keyed by the same cell as the front one;
  2. nl link records x 8 bytes, one per link in link order: u8 side (0 = the cell's surfel, 1 = its back
     surfel), u8 tint[3] (the glass on the way, 255 = clear), u32 door (the door reference the link passes, 0 = none);
  3. np probe records x 32 bytes, one per probe in probe order: u8 skyTint[8][3] (per octant, the sky seen
     through glass, 255 = clear), u32 room[2] (room[0] = the probe's room, 0 = none; room[1] = 0xFFFFFFFF, except
     a probe in an opening, which names the room on the other side there, 0 = outdoors);
  4. nx room boxes x 32 bytes: u32 room, float lo[3], float hi[3], u32 reserved.
- size = 64 + 32 ns + 144 np + 12 nl + 32 nb + 8 nl + 32 np + 32 nx.
- a room id = (hash of the bake rectangle << 16) | (n + 1), so ids of two bakes do not collide.

What a reader (FO4CS, last by standing order; read-only for this lane) must change for v4:
1. accept version 4; the size check adds 32 x reserved[0] + 8 x linkCount + 32 x probeCount + 32 x reserved[1];
2. a link whose side is 1 resolves against the back surfels (their own table, same cell key), not the front ones;
3. multiply a link's radiance by tint / 255; multiply an octant's sky by skyTint / 255;
4. which room a point is in: the room boxes first, else the nearest probe's room; a froxel between probes of
   two rooms does not blend them (the open line of 2f);
5. the door reference is information only for now (a later step can drop a link while its door is shut);
   nothing else moves: surfel, probe and link layouts and the keying are v3's.
A v3 reader that checks the exact size refuses a v4 file. Until the reader is updated, bake for it with `--tbk 3`.

Rooms. The placer's enclosed rooms (2e) are named; a probe takes the room of the cell it stands in. Two defects
were found by the gate and removed: an opening's cut cells took whichever side reached them first (now decided
by the opening's plane), and a probe standing in a solid cell took the first air neighbour in a fixed order (the
far side of a thin wall; now the neighbour the probe can see).

Glass. A pane is a shape WITH A MATERIAL FILE READ whose material says: blending on, source alpha over
(factors 6 / 7), not a decal, environment mapped, not soft, opacity above 0. Per triangle the light let through is
T = 1 - a (1 - c): a = opacity x map alpha x vertex alpha, c = the map's linear color x vertex color (effect
shader: opacity is the material's alpha squared, c times base color and scale). A pane never stops a ray; a
link's tint is the weighted mean T over the rays that land in the surfel's cell; a two-sided pane counts once.
Why the material file is required: in Vault111Cryo, with the shape's own NIF flags allowed to stand in, 33 of
the 37 shapes fed were drip splashes (15), lamp covers (14) and klaxon shells (4). Why environment mapped and
not soft: all 6899 archive materials were read; 122 effect and 5 lighting materials are panes; mist, beams and
glow cards are blended over but soft and not environment mapped (39), additive materials add light and take
none (48). The first feed took every blended shape: Solomon's house's one "pane" was a 480-unit mist sphere tinting
29% of the link weight; Vault111Cryo fed 133,145 triangles and tinted 47%.

The fixed world. The soup takes placed STAT MSTT TREE FURN CONT ACTI TERM FLOR LIGH (a static collection's
parts by their own types) and doors as boxes only. Pick-up items and actors are left out by type, effect shapes
and decals per shape. The cell view may draw more (clothing ground models, placed actors, decals); none of it
reaches the soup, the albedo, the `.tbk` or the room ids. Stage T below holds this.

Gates (exe of the last merge; numbers are the green run unless marked red):
- `tests/spells/probe_bake.py rooms` (a built scene: 7 rooms, 2 doors, 8 panes): PASS, 272 probes re-traced at
  1024 rays, 64671 links (back side 10964, through a door 1184, tinted 1287), 80 probes named in 7 known rooms.
  Reds: `--red oneside` FAIL (0 back links, unlinked mean 0.1200 against 0.0138), `--red rooms` FAIL (0 probes
  named), `--red glass` FAIL.
- `tests/spells/probe_bake.py synth`: PASS (265 probes, 66796 links, 2256 to a back side; `--tbk 3` byte-identical
  to the exe from before the lane). Reds oneside / octant / normal FAIL.
- `tests/spells/prtp_reference.py` (an independent brute-force tracer, extended for v4 on its own): rooms PASS
  (96 probes x 8192 rays: total median 0.022 p95 0.072, sky through glass worst 0.0033); reds glass (sky through
  glass 0.0265 over 0.01) and oneside (0.049 / 0.507 over 0.10 / 0.25) FAIL. Concord PASS (658 probes, 246772
  links, 65592 to a back side: total median 0.025 p95 0.082); red oneside FAIL (0.076 / 0.386); a Concord bake
  with one side only FAILS the reference (0.082 / 0.381, unlinked mean 0.1745 against 0.0181).
- `tests/spells/probe_glass.sh` (real cells; the checker reads the plugin and every material file itself):
  stage A census, B the soup's panes, C the light, T the reference types.
  Vault111Cryo: 4 panes (108 triangles, mean T 0.67) of 1085 blended or effect shapes; 1275 of 279905 links
  tinted; C seen 0.980, mass 0.893, clear 1.0000; T 1349 references, 0 outside the list.
  NorthEndMeanPastries: 15 panes (348 triangles: a diner window and 14 counter sneeze guards), 465 of 12864
  links tinted (3% of the link weight); C seen 0.938, mass 0.860, clear 0.9986; T 238 references, 0 outside.
  DmndSolomonsHouse01: 0 panes, 0 of 4614 links tinted; T 157 references, 0 outside.
  Bars (set before the real cells were run, never moved): seen >= 0.80, mass 0.75..1.33, clear >= 0.97.
  Reds: `--red haze` (every blended shape) FAIL A, 704 shapes wrongly fed, 177,984 triangles;
  `--red ignored` FAIL A and B (0 fed, the checker has 108); `--red items` (pick-up items let into the soup)
  FAIL T: 47 references outside the list in Vault111Cryo (MISC 35, ALCH 4, AMMO 4, WEAP 3, ARMO 1), 36 in
  DmndSolomonsHouse01 (MISC 22, ALCH 14).
- `tests/spells/cell_gi.sh` (the relight reads v4): Vault111Cryo and Solomon PASS, every stage 99.7% or better;
  `--red flip` FAILS stage B (58.1%). `cell_lit.sh` PASS. `lodgen_native_baseline.sh --check` PASS (the loader
  edit moves no far-LOD byte).
- `tests/spells/probe_far.py` (the far map stays v3): PASS, 121 probes over 121 cells; `--red shift` FAILS (842
  heights outside their cell). Give it the exe as an absolute Windows path: it starts the exe as given.
What v4 gives one real cell (Vault111Cryo, same soup): links 259698 -> 279905; the sphere share refused as
turned away 0.0341 -> 0; unlinked mean 0.2089 -> 0.1982; 10745 second sides kept in their own cell (7068 were
housed next door in v3). The relit picture moves by 2 levels or more on 26% of the pixels in the gear-door room
and 9.5% in the pod room (by 8 levels or more on 3.4% and 0.2%); the glass itself takes 0.012% of the light
there (the Vault has four small panes), 1.3% in NorthEndMeanPastries.

Not modelled: a pane's view-angle falloff (54 of the 122 pane materials use it) and palette alpha (7); a
blended shape with no material file is never a pane; frost films (blended over, no environment map) take no
light; one pane material name exists twice with different content (materials/shared/glasstile01.bgem is
additive, materials/interiors/hightech/glasstile01.bgem is a pane), so the rule goes by the full path.

Question for bungo, no behavior changed: movable statics (MSTT) are in the soup. The record cannot tell a
fixed one from a simulated one: its flags and fields carry no such thing; whether the object is physics-driven
is in the model's collision data. Vault111Cryo places 371: 323 are effects (mist, glow cards, drips, beams;
already left out per shape), 45 are knock-about set dressing (oxygen tanks, folding chairs, cardboard boxes,
vault suit boxes, traffic cones), 3 are fixed machinery (the gear room gate, two generators). Keep them all,
drop the type, or decide per model from its collision?

For section 3 (Open):
- remove the line "`.tbk` v4: two surfel sides per cell (gives back the refused thin-wall weight), room ids (2f)": done here.
- add: FO4CS's reader takes `.tbk` v3 only; the five changes for v4 are listed in this section; until then bake with `--tbk 3`.
- add: glass in the bake has no view-angle falloff and no palette alpha; frost films and blended shapes without a material file take no light.
- add: MSTT in the bake: fixed and simulated cannot be told apart from the record (bungo's call).

### 2z. Light shapes: hemisphere and box lights (lane HEMI1, 2026-10-02)

A placed light has one of four shapes, decided in this order: LIGH flag 0x800 = hemisphere; else 0x400 / 0x4000 =
spot; else, if the reference carries a linked ref under keyword LightBoxLink (XLKR, KYWD 00115705) to a
reference that has primitive bounds (XPRM) = box; else omni. A hemisphere and a box are the omni light (same
radial curve, same color) cut by a volume, with no fade at the cut:
- hemisphere: lit only where (P - light) . axis >= 0; the axis is the light's local +X under its placed rotation
  (14 of the 17 placed aim it down).
- box: lit only inside the LINKED reference's box (its position, its rotation, half extents = |XPRM bounds| x the
  light's scale). The light's own position and radius still give the falloff; the box only cuts.
The cell view drew both as plain omni lights (2a "Not yet"). Now the light buffer carries the shape (8 texels a
light: texel 1.w = -3 hemisphere, -4 box; texels 5-7 the box's three rows), and the cell shader, the shadow
pass's light list and the bounce relight (2i) all cut by it.
Census (Fallout4.esm): 17 hemisphere lights placed (8 LIGH bases), 1877 box-linked omni lights, 2 box-linked
spots (the spot wins, the box is ignored). Summary note "shapes=hemisphere N box N (box link unresolved N)
ambientboxes=N".

Ambient Only lights (corrects 2r): the shape rule never looks at the Ambient Only flag, so an Ambient Only light
linked to a box scales the ambient inside that BOX, whatever its radius, not inside the sphere of 1.22077 x
radius. 29 of the 39 placed are box-linked (10 keep the sphere), among them all three in Vault111Cryo:
001EF28A box centre (-1025,1848,-72) half (757,780,1374); 001EF2A4 (-2377,25,27) half (440,232,547); 002097B0
(-3626,-271,322) half (811,648,641). 2r's three spheres are no longer drawn there. ASSUMED from how the game
builds the light, NOT measured on a game frame: one capture in Vault111Cryo at about (-4500,-250,0) settles it
(inside the old sphere, west of the box face at x = -4437: full ambient if the box is right).

Red WW_CELL_LIT_RED=hemiomni (bit 256): every hemisphere and box drawn as the omni it was, and the Ambient Only
boxes as spheres again.
Gate tests/spells/cell_lit.sh (the checker reads the plugin itself: flags, linked ref, bounds, rotation):
- DmndRadio01, look-at (1617,99,230) eye 250 away: 2 hemispheres in the cell, 606 pixels a hemisphere's plane
  decides (floor 200), agree 99.9%; all pixels 100.0%.
- CabotHouse01, look-at (765,91,380) eye 250 away (ground floor under two upstairs lamps whose boxes end at the
  upper floor): 33 box lights in the cell, 3934 pixels cut off by a box and 1415 lit inside one (floor 200
  each), agree 100.0%.
- the Ambient Only view of 2r: 3 boxes; inside 100.0% of 312,232 px, outside 99.9% of 123,453; 96,490 px where
  box and sphere differ (floor 1000) agree 100.0%.
- red hemiomni: all three views FAIL. DmndRadio01 shape-decided agree 0.3%, CabotHouse01 0.1%, Ambient Only
  box-decided 0.0% (outside 21.8%). Red ambientfull still fails the Ambient Only view (inside 0.0%).
Also rerun: cell_shadow.sh (Vault 96.9% / 99.7%, Solomon 89.4% / 99.2%), cell_spec.sh (100.0%), cell_oren.sh
(100.0%), cell_ao.sh (3 cells), cell_refs.sh on Vault111Cryo (1397 drawn of 1397), all PASS.
The bounce (2i): cell_gi_check.py's surfel relight now cuts by the shape too and counts the surfels a shape
decides: Vault111Cryo stage A 400 surfels, 5 decided by a light's shape, agree 100.0% (the same dump against
an omni-only sum agrees 98.8%, still over the 97% bar, so five surfels cannot carry a red of their own; the clip
itself is held by cell_lit's hemiomni red). Stages B-E unchanged: 100.0 / 100.0 / 100.0 / 99.9%.
Not done: cell_spec_check.py and cell_oren_check.py still treat every light as omni; in their views the shapes
decide at most 0.25% of the sampled pixels (240 of 99,549), far under their pass bars. cell_cube.sh and
cell_fxdepth.sh were not run by this lane.

Open:
- Ambient Only lights linked to a box fill the box (2z): from how the game builds lights, not from a frame.
  Capture Vault111Cryo standing at about (-4500,-250,0): ambient full there = box, dimmed = sphere.

### 2aa. Placed decals and placed actors are drawn in the cell view (lane PLACED1, 2026-10-02)

bungo asked whether decals, props and skeletons are in the cell view. Two kinds of placed content were not:
projected decals (a reference whose base is a texture set carrying decal data) and placed actors (their own
reference record type, which the cell view never read). Both are drawn now, whenever a cell is shown: no menu
row, no INI key; environment variables only for the red controls.

Part 1, decals. The game's side was read first (private notes); the repo says "the game's decal pass".
- The box. Frame = the reference's rotation as a placed model uses it. Width along local +X, height along local
  -Z, projection along local +Y. With a box primitive on the reference: centre = the reference position, sizes =
  twice the primitive's bounds (they are half extents; width x, height z, depth y), no ray. Without one: a ray
  from the reference along +Y, 1000 units; no hit = no decal; centre = the hit, width and height = the decal
  record's sizes times the reference's own size scales, depth = the record's depth. The reference scale is not
  read.
- Which surfaces. A surface takes the decal where its face normal against the projection is >= 0.3; below that it
  fades with the shading normal, saturate((dot - 0.3) / 0.25); alpha under 4/255 is dropped. The game applies
  decals before lighting with one blend for the whole pass, so the material file's own blend and test are not
  used and a decal is lit like the surface under it.
- How the cell view does it. The game projects in screen space; the cell view clips the welded opaque triangles
  inside each box on the CPU (same box, same angle rule per triangle instead of per pixel) and draws the pieces
  blended, with no depth write, through the lit program, so cell lights and their shadows fall on them. Named
  differences: the ray runs against the drawn opaque triangles instead of the collision, and starts 1 unit
  behind the reference (a decal placed exactly on its surface otherwise misses it); no distance fade.
- Refused by name, counted in the census line: a size the game rolls (min != max without a box) or a picture it
  picks from a 2x2 sheet at random; nothing opaque in the box.
- Numbers: Vault111Cryo 540 read, 513 drawn (34 by their box, 479 by a ray; 442,701 triangles), 26 dice,
  1 no surface. MiltonGeneral01 185 read, 185 drawn. Vault81 121 / 121. MaldenCenter01 41 read, 23 drawn, 18 dice.
- Sources: src/esmplaced.cpp/.h (the decal records), src/celldecal.cpp/.h (box, ray, clip), a small hunk in
  src/cellview.cpp (intake in the reference funnel, the pieces after the weld, the census line).
- Gate tests/spells/cell_decal.sh + cell_decal_check.py (own plugin walk, own model reader, own ray). Stages:
  K census against the walk; G every drawn box against the walk's (centre 1 unit, sizes 0.5%); N pixels outside
  every projected box equal the decal-less shot; C every decal the camera sees changes pixels inside its own box.
  Green, three cameras: a Vault111Cryo corridor K G PASS (513 of 513 boxes), C 17 of 17 decals in sight (78,513
  pixels changed; the boxes cover the frame, no N); a MiltonGeneral01 ward K G PASS (185 of 185), C 3 of 3; the
  Vault111Cryo walkway bungo named K G PASS, N 99.997% of 273,114 (no decal within 900 units in sight: no C).
  Reds (WW_CELL_DECAL_RED), each FAILS: none (no decal drawn) fails K and C (0 of 17, 0 of 3 in sight; 0 of 513
  and 0 of 185 drawn); wide (twice the width and height) fails G in all three (0 of 513, 0 of 185) and N at the
  walkway (94.973%); axis (projects along -Z) fails G DECAL_AXIS_NUMBERS.
  The Milton camera does not carry N: green keeps 100.000% of its 209,744 outside pixels, but the wide red moves
  only 115 of them (99.945%), so that camera cannot tell wide from right; N is judged at the walkway.

Part 2, actors (interiors).
- The chain, from the published record layouts: the placed actor's base; the record its looks come from (the
  template chain while the "traits" template flag is set; a leveled list on the way is a dice roll unless it has
  one always-taken entry); race -> skeleton for the sex, skin, height; the skin's armor addons for that race;
  the outfit's armors (a leveled item list only when it is not a dice roll); a skin addon is hidden when an
  outfit armor wears one of its body slots; the pre-built face mesh by the looks record's form id, hair and
  facial hair hidden by the slots that cover them. Scale = reference scale x race height x the middle of the
  record's height range.
- The pose. Every part is skinned on the CPU onto the skeleton's bind pose and placed by the reference
  transform; the triangles go into the cell's own buckets (key "ACTOR"), so they are lit and shadowed like any
  surface. No rig per actor: the cell lights' shadow pass does not skin.
- Refused by name in the census line: leveled list (a dice roll), no body model (robots are built from parts),
  no race, no skeleton, no geometry, not an actor. Dead-on-start actors ragdoll in the game; they are drawn
  standing in bind pose and the line says how many. Outfit pieces that are a dice roll are left off and counted
  ("short of outfit pieces"): such an actor stands in its underwear.
- Numbers: Vault81 33 read, 31 drawn (all 31 hide a skin part), 1 not shown, 1 no body model. MaldenCenter01 47
  read, 24 drawn (23 dead on start; 20 human, 4 first-generation synths; all 24 short of outfit pieces), 8 not
  shown, 14 leveled, 1 no body model. Vault111Cryo 27 read, 13 drawn (11 pod occupants, 2 radroaches), 2 not
  shown, 12 leveled. Creatures come through the same route (the radroach); robots do not.
- Sources: src/cellactor.cpp/.h (records, chain, skinning), a small hunk in src/cellview.cpp (the actor loop,
  the census line, the dump WW_CELL_ACTOR_DUMP).
- Gate tests/spells/cell_actor.sh + cell_actor_check.py (own plugin walk, own skinning). Stages: K census; F
  every placed actor's fate, looks record, race, sex, skeleton, position, rotation, scale; P models, hidden skin
  parts, face mesh; G posed bounds within 0.1 unit and the same triangle count; N nothing moves outside the
  posed triangles; C the actors show inside them.
  Green, three cameras: Vault81 (living, 8 on screen) K F P G PASS, N 99.999% of 526,589, C 81.3% of 11,151;
  MaldenCenter01 (corpses, 10 on screen; no P, nothing hidden) K F G PASS, N 100.000% of 522,771, C 72.9% of
  12,817; Vault111Cryo (a radroach) K F P G PASS, N 100.000% of 534,881, C 60.2% of 4,987.
  Reds (WW_CELL_ACTOR_RED), each FAILS: none (no actor drawn) fails K, F and C in all three (C 0 px);
  ACTOR_SHIFT_NUMBERS
  nohide (the outfit hides nothing) fails P: 0 of 31 in Vault81, 2 of 13 in Vault111Cryo; not run in Malden
  (nothing is hidden there, named in the gate).

After main's MISS1 (merged before the gate runs above).
- Start state: actors and decals follow the enable-parent chain like every reference (decals through the same
  intake; the actor loop asks the same function, and actors are in the table since an actor can be a parent).
  Malden Center moved from 1 not shown / 21 leveled to 8 / 14. Both checkers carry their own chain over every
  placed record of the plugin; the viewer's table holds the cell only (a parent outside counts as enabled): on
  every reference of the four gate cells the two rules agree (0 of 3114, 3760, 4653, 3141 differ).
- Root transform: the decal checker's receivers drop the root as the viewer's do. Actors are not reached: of 65
  skeleton and part files one root carries a transform, and all its shapes are skinned.

The bake. Placed actors, corpses, their gear and decals are drawn and are receivers only. The probe soup is
filled in the placement loop by base type; actors ride that loop, so their soup role is forced to "out" by name
(not only by type); decal pieces are cut after the loop and no soup call sits on that path. The bake's albedo,
the room ids and the .tbk come from the soup; the LOD and near bakes are another program path that includes
none of this lane's sources. Why: the game's bakes hold no actors and no projected decals.
Measured in Vault111Cryo with probing on, the lane's actors + decals on against both off: soup references 1339
and 1339, soup triangles 1,451,459 and 1,451,459, doors 36 and 36, and the two soup files are the same bytes.
With actors on, the notes line names them among the references left out by type ("NPC_ 13").

Existing gates re-run on the merged exe, and why each is reached:
GATES_RERUN

Open.
- Exterior cells show no actors (the exterior reference gate demands every row be an ordinary reference).
- Actors are not in the reference list or the pick table: they cannot be selected.
- Corpses stand; nobody is animated; robots are refused; dice outfits are left off.
- The face meshes, the hidden-part rule and the decal look have not been compared with the game on screen.
- Decals: no distance fade, no parallax variant, the ray against drawn triangles instead of collision.

### 2ab. The Fraternal Post / Pickman Gallery mismatch: mist cards in the probe pictures (lane FRAT1, 2026-10-02)

Section 3 carried this as open: seen from straight above, FraternalPost11501 (center 553,2170,400, distance
600) and PickmanGallery01 (562,440,150) parted from the diffuse check on 13% / 9% of the clean pixels, with
"overlay sheets over the walls" as the suspect. Asked: per pixel, which side is wrong, the viewer or the
checker?

Neither formula. The picture that was wrong was the POSITION probe the checker reads, and it was already
repaired when the lane was cut.

What covers the rejected pixels. 99% of Fraternal Post's "wall strip" (it is the flat TOPS of the wall kit,
z = 128, normal straight up, not a wall face) lies inside three placements of
`Effects\Ambient\MistLargeRoundDusty01.nif` (refs 0017D953, 0017D94F, 0015184B), a blended effect-shader card
(`AmbBeamMistRoundDusty.BGEM`) hanging 491 units over the room; 55% of the floor group lies inside 0015184B.
The cell has 33 such placements, Pickman Gallery 72 (56 MistLargeRoundDusty01 + 16 MistLargeRound01); read
from the plugin, independent of the viewer. No decal and no dirt sheet is involved.

The mechanism, per pixel (the same exe and camera, the probe passes with and without the effect shapes):

| | Fraternal Post | Pickman Gallery |
|---|---|---|
| clean pixels / rejected, effects in the probes | 69,885 / 6,629 (9.5%) | 66,058 / 1,399 (2.1%) |
| clean pixels / rejected, effects out | 83,647 / 0 | 71,080 / 0 |
| of the rejected: position high byte changed | 100% | 100% |
| ... by exactly one level down on x, y and z | 82.5% | 92-98% |
| of the rejected: probe 8 (the diffuse) changed | 0.1% | 0.0% |
| decoded position off the true surface | over 40 units on 100%, median 445 | same |

The card is about 1% opaque (a blend fit over probes 2 / 3 / 4 gives a median of 0.01). That cannot move a dark
diffuse value by one 8-bit level, but it lowers any byte near 127 by one, and the position probe's high byte is
such a byte. One level is 256 units on each axis, 443 in all. The neighbours shift together, so the checker's
"clean" filter (position step under 40) keeps them, and the checker evaluates its correct formula 443 units
away from the surface. "2.2x on the wall tops, 0.66x on the floor" was the model at the wrong place. Where the
card is thicker every probe carries its colour and the clean filter drops most of those pixels.

So: the viewer's diffuse was right, the checker's formula was right, the picture bungo looks at was right (there
the mist is meant to blend). The 13% / 9% were measured by lane RIM1 on an exe from before 347742a2 (lane EFX1,
2026-10-01 17:06: no effect or refraction shape is drawn in a probe pass). With that commit the same cameras
agree on every clean pixel. Nothing in the drawing changed in this lane.

What the lane adds, so that it cannot come back unseen:
- `WW_CELL_LIT_RED=probefx` (check-only, red bit 4096): `wwCellProbePass()` answers false, the effect and
  refraction shapes are drawn into the probe passes again.
- `tests/spells/cell_oren.sh`: three more gate views, `FraternalPost11501@553,2170,400~600`,
  `PickmanGallery01@562,440,150~600`, `PickmanGallery01@470,475,150~450`; a CELLS entry may end `~<distance>`;
  `--red probefx`. The checker (`cell_oren_check.py`) is unchanged.
- Green [first merge], agree 100.0% in all five views: Vault 7,180 lit (rim 99.7%), Vault second camera 4,682
  (rim 99.2%), Fraternal Post 8,770 (rim 99.8%), Pickman from 600 above 11,526, Pickman closer camera 10,577 of
  12,896 clean.
- Red `--red probefx` [first merge], bar 97%: FAILS in 4 of the 5 views. Fraternal Post 90.8% (rim 17.9%),
  Pickman closer camera 45.3%, Vault 98.3% with the rim at 82.4% (the rim bar fails it), Vault second camera
  95.4%. Pickman from 600 above only drops to 98.0% and passes: its rejected pixels are one patch at
  468,478,504, which is why the closer camera over that patch is in the list.
- Re-run on the same exe: `cell_lit.sh` PASS (Vault 99.9%, lit 99.8%; Solomon's house 100.0%; the Ambient Only
  view 100.0%), `cell_spec.sh` PASS (100.0%, viewer / expected 1.010 and 1.016).

Open.
- The Pickman view from 600 above does not fail the red by itself (98.0%); it is in the gate for the green.
- The rule this rests on is wider than these probes: nothing blended may draw into a pass that writes data as
  colour. Any later data pass (a new probe number, a bake pass) has to ask `wwCellProbePass()` or its like.

### 2ac. The game's screen-space reflections (lane SSR1, 2026-10-02)

(Letter: the next free one after 2z; renumber at splice if another lane lands first.)

Why: the Vault 111 cryo walkway shows bright pools under its lamps in the game. Lane POOL1 (2t) read them as
the game's screen-space reflections. Built and measured here: the pass is the game's, but at eye height it
adds little. The pools are already in the opaque lit frame (the placed lights and lane CUBE1's cube term).

The chain, read from the game's shaders and their setup code:
- The game draws its opaque frame WITHOUT the env term, then at half the view: a ray per flagged pixel (the
  view ray mirrored about the normal, the normal's world z doubled first; kept when it points more than 0.2
  into the view), a march of at most 32 steps over the min-of-2x2 depth mips (4 levels; at the finest level a
  ray 50 units or more behind the surface is refused, and a refusal is a miss), the hit's color with a
  confidence c = screen-edge fade x travelled-distance fade x sat(1 - 25 x depth gained / (far - near)),
  stored as c squared; a 5-tap blur across, then down, where taps without confidence hand their weight on.
- Its composite takes lerp(cube term, reflection x the same factor, min(confidence, 1)) x the diffuse light.
  So where the march finds nothing the cube term stands, and the result is never larger than the reflected
  color can make it.
- The four scales: color 1.0, angle gate 0.2, normal z 2.0, confidence 1.0 (the defaults; bungo's INIs
  override none). near = 15; far = the cell's clip distance (XCLL, or its lighting template's when it
  inherits), capped by the far LOD distance. Vault111Cryo: 10000.
- The march samples THIS frame before the env term (no feedback: a reflection never holds a reflection).
- The flag: an environment-mapped material whose material file has "Screen Space Reflections" on.

Here: `src/gl/cellssr.{h,cpp}` + `res/shaders/cell_ssr.{frag,vert,prog,glsl}`. wwCellSsrPass draws the
frame's opaque cell-lit fragments once more (probe 60: linear lit color without reflection, obscurance and
fog; alpha = the flag) into a full-size float target, runs the four stages over it and lane AO1's depth
pyramid (one pyramid, not two), and the two cell programs (fo4_default.frag, pbrm_default.frag) mix the
result into their cube term: `cellSsrMix` in cell_ssr.glsl is the one place it enters the picture.
Interiors only; rides the Cell lights row; needs the obscurance pass (WW_CELL_AO=0 leaves none). Texture
unit 17. With another lane's probe on (WW_CELL_LIT_PROBE other than 61) the reflection stays out, so those
probes compare their own term.
ASSUMED: the constant rows' order (implied by every use); the pyramid the march loads is the obscurance's;
point-sampled ray inputs; a bilinear composite read. DEVIATIONS: float targets (the game keeps 8 bits); a
NIF-only material (no material file) carries no flag; exteriors not done; the march reads linear light before
the cell view's per-fragment tone map.
Pins: WW_CELL_SSR_RED=off | nogap | nofade; WW_CELL_SSR_DUMP=<file>; probe 61 (the reflection a draw read).

Gate `tests/spells/cell_ssr.sh` + `cell_ssr_check.py`: the checker re-does ray, march and blur in numpy from
the viewer's dumped depth, normals and scene color with the constants above and the plugin's own clip
distance. Stages F (far plane), M (march), B (blur), P (probe picture), L (the picture gains light only where
the rebuild has reflections), Z (a view where nothing may reflect: probe black, on equals off byte for byte).
Bars: agree >= 99% (M, B) / 95% (P) within 0.002 + 2%, over the pixels where the rebuild OR the viewer shows a
value, and viewer / expected total within 5%. Views: walkway (eye height), walkway_far, topdown (zero).
Result (exe 2026-10-02 22:34, main 34a7ab60 merged, branch head 28d06fb4): green PASS 3 of 3 views:
M 100.0 / 100.0%, B 100.0 / 100.0%, P 98.8 / 99.2%, totals 0.998 / 0.999; L: the picture changes on 4731 /
5458 pixels by +0.40 / +0.28 of 255 on average, 0 pixels away from a reflection; topdown Z: 876800 flagged
pixels, 0 rays, picture identical. Reds FAIL on both walkway views: off P 0.0 / 0.0%; nogap M 94.7 / 95.4%
(bar 99); nofade M 0.3 / 1.7%, totals x11.3 / x10.9.
Measured size (Vault111Cryo, 1280x720, eye height): mean confidence 0.008; on vs off differ on 6.7% of the
pixels by 0.55/255 on average (max +25/255); the pass costs about 50 ms a frame here.
Sibling gates rerun: cell_spec (the cube/specular chain the mix sits in), cell_cube (its probe sits after the
mix), cell_lit and cell_ao (the pass reuses the obscurance's pyramid and targets).

### 2ad. A cell opens in a quarter of the time: geometry beside the document, files read ahead (lane SPEED1, 2026-10-02)

The rule: nothing about the picture, the counts or the saved file may change. Only where the bytes wait, and
which thread fetches them.

Where the time and the memory went (measured first, timers behind `WW_CELL_SPEED_DUMP=<file>`; shelter, 49 s):
- welded shapes written into the document one row at a time: 9.9 s, and +3.4 GB (about 1.5 kB a vertex for rows
  that need 72 bytes);
- texture files read one by one on the drawing thread during the first picture: 22.9 s (1261 files);
- model files read and parsed, one thread: 4.6 s;
- drawing itself: 0.2-0.3 s. One core busy.
Models were already shared between placements (3687 placed objects = 1020 model reads). That suspicion was wrong.

What changed.
1. Side geometry store (`src/cellmesh.h/.cpp`). The welded vertex and triangle arrays of a cell's shapes are
   kept beside the document, and the renderer draws from them (`gl/bsshape.cpp`). The document's two row arrays
   of such a shape stay empty ("waiting"). They are written the moment something needs rows:
   - a save (all waiting shapes, in block order: the file is the same bytes as before);
   - any reader that asks for the array by name: one net in `BaseModel::getItemInternal`, which writes that
     shape's rows before handing out an empty array (186 by-name readers in 36 files are covered by it, not by
     186 edits);
   - `NifModel::updateHeader` leaves a waiting shape's arrays alone and adds the waiting bytes to its block
     size, so the header is right without the rows.
   A plain open forces 0 shapes.
2. Texture read-ahead (`src/celltexahead.h/.cpp`). When a material of a cell document is resolved, the names
   the renderer will ask for (its own `fileName(slot)` for slots 0-9) go to up to 8 worker threads that find
   and read the FILE BYTES. Decode and hand-off to the graphics card stay on the drawing thread. The drawing
   thread takes ready bytes, waits when a worker is on that file, and reads itself otherwise. At the bound on
   held bytes (384 MB) the workers WAIT; nothing read is thrown away.
3. Model read-ahead (`src/cellmodelahead.h/.cpp`). The distinct model + swap pairs of the cell are parsed on
   up to 8 workers (`qBound(1, cores - 2, 8)`), each under its own run of item slots
   (`src/data/nifitemcache.h`); the builder takes the answers in its own order, so the document is the same
   as a one-thread build.
4. A bake run with no lit picture (`WW_CELL_PROBE_BAKE` set, `WW_CELL_LIT` and `WW_CELL_GI_DUMP` not set)
   neither reads nor draws the placements the probe soup leaves out (2y's rule: disabled references, markers,
   the types `soupRole` gives 0, `sky\` and `water\` models, placed actors). The list is `soupRole` itself, not
   a copy. Measured (hold 6, merged tree, one run each way, OS counters): Vault111Cryo 63 of 1455 placements not
   loaded, 17.5 -> 11.1 s, 2999 -> 2478 MB; NorthEndMeanPastries 52, 7.9 -> 5.5 s; DmndSolomonsHouse01 37,
   6.2 -> 5.5 s. The bake's files are the same bytes. The soup's "left out by type" row may read higher in the
   lean run where the full run lost a placement to a model that does not load (Vault: WEAP +2, failed 5 -> 4).

Measured (hold 3, merged tree with 2w-2y; the old path is the same program with `WW_CELL_SPEED_RED=slow`;
fastest of 2 old runs against the median of 3 new; OS counters; one other NifSkope window open):

| cell | seconds old -> new | peak memory old -> new | cores busy | picture | counts |
|---|---|---|---|---|---|
| Vault111Cryo | 23.8 -> 6.8 (71% fewer) | 4444 -> 2477 MB (44% less) | 1.0 -> 2.4 | 50 px differ (two old runs: 30; allowed 184) | 20 lines same |
| BostonMayoralShelter01 | 40.5 -> 10.1 (75%) | 7890 -> 4954 MB (37%) | 1.0 -> 3.3 | 0 px | 20 lines same |
| Commonwealth -21,6, 3x3 | 38.3 -> 9.1 (76%) | 7596 -> 4146 MB (45%) | 1.0 -> 3.1 | 124 px (old: 68; allowed 336) | 28 lines same |

Seconds include program start and the 2.5 s the test waits before its picture. In-process: 4.5 / 7.2-7.8 /
6.2-7.2 s. Model workers on the shelter: 1 thread 4.6 s, 2: 2.8, 4: 2.3, 8: 1.1-1.3, 14: 1.65 (the summed
parse time grows with the workers; inferred: the allocator). Texture bytes on the drawing thread: 0.6-0.7 s.

Not done, and why.
- GPU instancing: drawing is 0.2-0.3 s of a 7-10 s load (under 5%).
- The largest stage left is the hand-off of textures to the graphics card on the drawing thread (about 3 s on
  the shelter). It needs a second GL context or compressed uploads off-thread; not in this lane.
- Memory hand-back (candidate d): four cells opened one after another in one window (WW_CELL_SPEED_REOPEN,
  hold 4) ended at 4685, 5213, 5277, 4825 MB with a peak of 5922 MB: it levels off. The old path's walk was not
  measured (did not fit a hold); nothing was changed for d.
- A 5x5 block was not re-timed (needs 15+ GB free and a quiet machine).

Gate. `tests/spells/cell_speed.sh` + `cell_speed_check.py` (reads the pictures, the count lines, the saved
file and the OS counters itself): PICTURE (differing pixels within 4 x the old path's own run-to-run
difference + 64), COUNTS (every census line the same), SAVE (same bytes; every second waiting shape asked by
name first, all must have rows), GAIN (floors: 50% of the seconds, 33% of the peak memory). Reds, each seen
failing: `slow` (the old path against itself fails the floor), `transform` (one placement moved: 261,827 px),
`rows` (waiting rows not written at save: different bytes), `nonet` (the by-name net off: 0 of 256).
`tests/spells/cell_speed_bake.sh` + `cell_speed_bake_check.py`: the bake's files with and without the skip are
the same bytes; red `leanred` (skips half of what the soup DOES take) must differ.

Measurement-only environment variables (no INI key, no menu row): `WW_CELL_SPEED_DUMP`, `_TAG`, `_RED`,
`_THREADS`, `_REOPEN`.

Decal receivers in key order (round 3): after main's placed decals came in, the same program welded Vault111Cryo
to 2975079..2975085 vertices across five runs (main's own exe twice: 2975083 vs 2975079); with decals off all
counts and the saved bytes matched. The receivers were taken in the bucket hash's order, which is seeded per run.
They are now taken in key order.

### 2ae. Going through the game cell by cell: one visit, a list of steps (lane PRTP5, 2026-10-02)

Step 5's runner. Its end product is BAKED PROBES; the per-cell check-up (what loaded, what is missing) is the first
step it knows, the probe bake is the second, and one visit does both, so the game is opened once. Source
`src/cellcensustest.cpp`; nothing in the menus, no INI key; everything is `WW_CELL_CENSUS_*` environment.

WHAT A VISIT IS. An interior is visited whole and alone. The exterior grid is cut into TILES of 5x5 cells that do not
overlap (`WW_CELL_CENSUS_BLOCK`, odd; the tile of cell v is centred on `floor(v/5)*5 + 2`), and a tile is loaded
ONCE through the cell view's own door (the `.wwcell` spec). Before, every exterior cell was opened as the 5x5 around
itself, so each cell's references were built 25 times. The whole world is never loaded.

THE STEPS (`WW_CELL_CENSUS_STEPS`, default `census`):
- `census`: one tab-separated row PER CELL of the tile (which cell a placed object belongs to comes from the
  plugin). 46 columns: key kind world x y form edid block tile slice status refs refs_drawn refs_block placements
  shapes verts tris lights_cell lights_block lit omni spot skip_off skip_noradius skip_black ambient_only light_types
  lights_approx models_loaded models_failed tex_asked tex_missing mats_unreadable far cover_px fb steps bake_probes
  bake_files build_ms bake_ms render_ms total_ms rss_mb note. The load's own figures (shapes, lights lit, seconds,
  memory, the names of what failed) stand on the tile's first row; the other rows carry `^` there. The file lives
  under the NifSkope folder (`release/cell_census/`), never in the repo.
- `bake`: the headless probe bake as it is on main (`WW_CELL_PROBE_BAKE`), into `WW_CELL_CENSUS_BAKE=<folder>`
  (`<folder>/I_<FORM>` or `<folder>/<worldspace>`), probes only in the tile's own cells. The runner sets the bake's
  variables per load and counts the files it left; the bake code itself is untouched (lane BAKE4 owns it).
  FILE VERSION: `WW_CELL_CENSUS_TBK` = 4 (default: both sides of a thin wall, room ids, glass tint) or 3 (what
  FO4CS reads today). The runner hands the builder `WW_CELL_PROBE_BAKE_TBK` and then READS THE VERSION BACK from
  every file the visit wrote; another version fails the visit. OPEN: the cell view's bake call does not read that
  variable (it always writes the writer's default, v4). The four lines that would make it listen go into
  `src/cellview.cpp` after `bs.red = ...`; my edit of that file was refused by the permission system (text in
  notes\prtp5\STATUS.md), so a v3 run FAILS its own check today instead of handing FO4CS v4 files. bungo decides.

THE RING (`WW_CELL_CENSUS_MARGIN`, 0 to 4 cells). A tile T is loaded as T + 2M cells; rows and probes are only for the
tile's own cells. The check-up needs no ring. THE EXTERIOR BAKE DOES: a probe sees only what is loaded in its visit
(the bake traces its rays against the loaded scene; `src/probebake.h` `rayMax = 131072`, 32 cells, beyond which a
ray is sky), so a probe at a tile's edge with nothing loaded next door takes the neighbor's buildings for open sky.
How wide the ring must be is a look decision (1 cell = 4096 units); it sets the cost below.

NOTHING PLACED = NO SCENE. A tile (with its ring) in which the plugin places nothing gets its rows without a load:
1356 of the Commonwealth's 1600 tiles. Measured: 25 rows in 3 ms. With the bake step on, such a tile IS loaded (its
ground still needs probes).

TOO BIG, OR IT KILLED THE WINDOW. A tile holding more than `WW_CELL_CENSUS_REFS_MAX` references (12000) is opened
cell by cell, and so is a tile a window died on (`<file>.pending` names the load in progress; `<file>.split` keeps
the tiles to split). Each row's note says which. From the plugin: 12 of the 1600 tiles are over 12000.

MEMORY. A window does not hand back what a big load took (measured 1.48 MB a reference kept; tile -18,7 with 3602
references took the window from 5.8 to 13.5 GB). It stops starting loads past `WW_CELL_CENSUS_RSS_MAX` (8000 MB) or
`WW_CELL_CENSUS_BUDGET` seconds; the next window goes on where the file ends. A long pass is a chain of windows.

SEVERAL WINDOWS. `WW_CELL_CENSUS_SLICE=i/N`: the visits (an interior, a tile) are dealt out in plan order, window i
takes every N-th, writes `<file>.part<i>of<N>.tsv`; `tests/spells/cell_census_merge.py` joins the parts (it joins
and never tidies, so a doubled cell stays doubled for the checker to see). Built for any N, proven with 2. A cell is
in exactly one slice because a tile is.

GATE `tests/spells/cell_census.sh` + `cell_census_check.py` (its own group walk of the plugin, its own tile and
slice arithmetic, no NifSkope code): every cell of every visited unit has EXACTLY ONE row, no row is outside the
plan, each row's references and placed lights match the plugin (the cell's own and the load's), a count-only row
stands only where the plugin places nothing, a row opened alone says why and the plugin agrees. Sample: 20 interiors
spread over the plugin (Vault111Cryo, CabotHouse01 among them), the two 5x5 tiles the 3x3 around Sanctuary falls in,
one tile with nothing placed. Then three more windows: the bake proof, the split rule, the ring.
`--slices N`, `--slice i/N` + `--merge` (for windows run by different lock holders), `--cells FILE`, `--whole` (not
run: after the wave).

MEASURED 2026-10-02 (the sample, the split and the timings on the exe of 11:18 / 14:13, main a1e25b20 merged in;
the bake proof and the ring again on the exe of 15:17 with main 37b5451d merged in, see the last line):
- Sample GREEN: 95 rows from 23 visits (20 interiors, 3 tiles = 75 cells), slices 60 + 35 rows, 0 refused, 0
  crashed; the windows' own checks 190 of 190 over 4 windows; the checker 539 of 539.
- Bake proof (one small interior, SanctuaryBasementJahani, 217 references): ONE visit wrote the row and the bake's
  2 files (48,260 bytes), 16 probes; the bake took 2.2 s inside that load. Checker 5 of 5 and 11 of 11.
- Split: a 3x3 tile with the limit pulled to 1000 (1785 references) and a small tile with a planted dead-run file:
  18 cells opened alone, 9 notes of each kind; window 128 of 128, checker 148 of 148.
- Ring: cell -20,7 loaded as 3x3 (1785 references for 150 of its own), one row, 259 probes baked in the same visit
  (1 file, 2.4 MB), the bake 0.8 s inside a 30.3 s load, the visit 41.5 s. Checker 13 of 13 and 5 of 5.
- Seconds, first lock slot (12:23-12:30): 17 interiors 14.2 s each (fit 3.5 s + 19.0 ms a reference); tile -18,7
  (25 cells, 3602 references) 122 s = 4.9 s a cell; a tile with nothing placed 0.003 s. The 18 lone cells: 4.8 s each.
- Seconds, second slot with another lane's window running (13:51-14:00): tile -23,7 (4928 references) 280 s, of it
  the build 102 s; Vault111Cryo 148 s. Per reference 1.7 to 2.5 times the first slot's figure.
- Reds, each run inside NifSkope (14:57-15:05, 8 minutes for all five), 5 of 5 FAILED as they must: `dropcell`
  (checker 1 failure of 9 checks, 8 s), `stale` (6 of 41, 229 s), `dropslice` (4 of 35, 52 s), `doubleslice` (2 of
  35, 102 s), `nobake` (the window's own check AND the checker fail, 21 s). The finished sample run again: PASS in
  75 s, both windows found nothing to load and closed by themselves.
<<BAKE4>>

WHOLE-GAME ESTIMATE (INFERRED: the measured fit laid over the plugin's own counts; `scratchpad/.../projection2.py`).
Fallout4.esm: 1195 interiors (527,390 references), Commonwealth 36,864 cells (701,769 references).
- The check-up alone, 5x5 tiles, no ring: 532 loads for the exteriors 7.1 h, interiors 4.4 h: 11.4 h in one window
  (15.8 h if every load were as slow as the second-slot ones). The old design by the same fit: 175 h for the
  exteriors alone.
- With the bake, ring of 1 cell: 5x5 tiles (7x7 loads) 51 h = 2.1 days in one window; 3x3 tiles (5x5 loads) 48.5 h
  = 2.0 days; lone cells (3x3 loads) 3.9 days. Of that the bake itself is 8.2 h (0.8 s a cell, measured on ONE
  cell) and the interiors' bake 0.4 x their load (ONE cell). Ring of 2 cells: 6.6 days at 5x5.
- Why the ring costs so much more than 7x7 / 5x5 = 2 times: 26 tiles (downtown) are over the limit with their ring
  and fall to lone cells, each a 3x3 load, so their references are read 9 times; over the whole map every reference
  is read 5.3 times. NEXT: let a too-big tile fall to smaller tiles before lone cells, and raise the limit once lane
  SPEED1's lighter load is in; the floor is 2 times.
- Two windows at once: NOT measured as a pair of bake runs (one worktree holds one lock slot). If they did not slow
  each other the times halve (5.7 h; 1.1 days). What was measured is my window beside another lane's: loads 1.7 to
  2.5 times as slow, which would leave two windows no faster than one. Treat the halved figures as the best case.

ITEMS AND ACTORS (offline, from the plugin, `cell_census_check.py share`; MEASURED). Of 1,238,037 placed records in
the plan: pick-up items 41,738 (3.4%), actors 7,501 (0.6%), everything else 95.9%. Interiors: items 5.3%, actors
0.7%. Exteriors: items 1.9%, actors 0.5%. Per cell (items + actors): interiors median 2.3%, nine in ten under 63.6%;
exteriors median 0%, nine in ten under 3.6%, most 27.6%. So leaving them out of the bake saves about 4% of what is
read (inferred: about 20 minutes of the 11.4 h, 1 to 2 h of the 2.1 days); the cell view does not load actors
today anyway.

WHAT THE SAMPLE'S CHECK-UP FOUND: 10 of 95 cells could not load something. Models not on disk: crow markers,
StaticCollectionPivotDummy, autoloadmarker01, drips / steam / fire / leaf effects, Deathclaw_AmbushWallslideFX,
VaultUnderLightAnimatedFlicker, 10mmRecieverDummy. Textures missing: Default_n, Gray, GrognakJanBack_n/_s,
ModelKitBase_n/_s, AmbientBeams02_d, testpond01_s. Materials unreadable: 1 to 3 in four cells. One light in
CabotHouse01 drawn as omni. Lights over the sample: 1545 placed = 1530 lit + 9 off + 3 black + 3 ambient only
(Vault111Cryo 849 = 840 + 4 + 2 + 3). The placed-armor garbage names of the first report are gone (lane MISS1).
Every exterior row still says `far=none`: the cell view draws nothing beyond what is loaded.

### 2af. Probe previews: the deck's debug views, built (lane PROBEVIEW1, 2026-10-03)

The Division deck shows its probe system through debug views: the GI result alone, sky visibility, the surfels and a probe's links. NifSkope now has all of them behind one Pass drop-down in the PRTP band:

| Pass | What it shows | Source |
|---|---|---|
| Combined | the normal frame | - |
| GI | grid irradiance on the normal, E / pi | the relit six-axis grid (unit 13) |
| Sky visibility | the probes' open-sky share on the normal | a second grid of the same voxels, blended with the same weights from each probe's 8 octants averaged to 6 axes |
| Surfel color | each surfel's albedo as a splat | the bake's surfels |
| Surfel light | each surfel's outgoing light B / pi | the relight's B |
| + picked probe | lines to every linked surfel | the bake's links, resolved per probe |

What this proves for FO4CS: the sky grid is the same 6-slab layout as the GI grid and needs no new sampler. The links resolve per probe from the stored deltas.

Still open (proposals only; the ranked list is in notes/deck1/DECK_MATRIX.md section F): BOUNCE2 (multi-bounce), ROOMCLAMP1 (doorways), BRICK1, SKYPIC1, FOGGI1, GPURELIGHT1 and STATICCACHE1. Each one is judged in a Pass view.

### 2ag. Lit effects and the one tone map (lanes FXLIT1 + HDR1, 2026-10-03)

#### Lit effects (FXLIT1)
- What it does: an effect material that sets the effect-lighting flag is multiplied, per pixel, by
  mix(1, directional + sum over the placed model's up to four lights of color x pow(1 - sat(d/r)^2, 2.2)
  x cone, lightingInfluence).
- Which four lights: the viewer picks them by the game's rule, per model, once at cell open
  (src/gl/cellfxlit.h).
- Effects without the flag are unchanged, pixel for pixel.
- Material swaps decide the influence. A placement's swap record (else its base's first swap) can replace
  the BGEM. In the Vault, the dusty mist (0.95, gradient) becomes the bright mist (1.0, no gradient). Any
  checker that reads placed materials must apply the swap.
- Gate tests/spells/cell_fx.sh:
  - Stage L uses probes 70..74 (model serial, position, multiplier). The probes write depth so that the
    nearest card wins.
    - Vault walkway: 99.9% of 516027 px agree, total ratio 1.000.
    - Third Rail: 99.9% of 3580 px agree, total ratio 1.000.
    - Reds white / nofade / all / nopower FAIL at both cameras.
  - Stage U (unflagged effects unchanged against the exe before) is judged at the Third Rail (513 px).
  - Stage N keeps a 3 px margin around anything the nosoft shot touches.

#### One tone map (HDR1)
- The game sums surfaces and effects in its linear HDR target and runs the imagespace once. The cell view
  used to tone-map each fragment and blend the effects in display space, so stacked haze cards each added
  their own tone-mapped value (the walkway's far door went white).
- Now, while the cell's imagespace draws, the main draw goes into a multisampled RGBA16F frame
  (src/gl/cellhdr.h):
  - Cell programs and cell effects write linear light (cellIsLinear).
  - The stencil marks the last writer: 1 cell, 2 other.
  - cell_hdr.prog then runs the imagespace (bloom once, exposure, curve, grade, LUT) on the 1s and copies
    the 2s as written.
  - Depth and stencil are blitted back first. The refraction copy follows the frame's format.
- Gate tests/spells/cell_is.sh stage Q: on blended pixels (stencil 3), the picture equals the chain over
  the dump's linear sum + bloom, >= 95% within 3x3 +-3/255.
  - Green: Cryo 99.69%, walkway 97.73%, Solomon 99.94%, Third Rail 98.38%. Stage P is unchanged.
  - Red WW_CELL_HDR_RED=perfrag FAILS Q where blends cover >= 10% of the frame: 80.6 / 78.5 / 72.4%.
  - bungo's walkway camera (350,-512,40 view 4 dist 450) is a gate entry (Vault111Cryo@walk).
- Not covered: workspace frames (several scenes) and pick / probe / measure passes keep the old path.
- After the merge of main (HEMI1), the lit-effect sum reads the light buffer through CELL_TPL. Stage U's
  BEFORE exe is a main build. The merged build is green on cell_fx, cell_fxdepth, cell_is (Q 99.69 / 97.92 /
  99.94 / 98.94), cell_spec and cell_lit.

### 2ah. Outdoors the bounce row takes the sky and the sun (lane SKY1, 2026-10-02)

Built in cd54e9d8, gated in 26212267, merged with main a54dfd62 (PROBEVIEW1's passes) in the lane's merge commit.

**What it was.** The bounce row relit the baked probes with the cell's placed lights only. Outdoors that is a few
lamps: the sun and the sky, which light everything out there, never reached a surfel or a probe, and the weather's
ambient was laid on every surface at full strength, under a porch as much as in the street.

**When it applies.** Only in an exterior the weather is lighting: Scene mode Lookdev with a weather resolved (the
existing mode and its existing weather / hour controls; no new control, row or INI key). In the plain viewport light
there is no weather, so the row stays as it was and its notes line says `gi sky: none (...)`. Interiors bake every
sky octant at 0 and the code does not run for them: an interior is the same byte for byte (gate, below).

**The three terms, and what each one physically is.**

1. *Sun at a surfel.* Irradiance `E = sun color x max(0, N.L)` where one ray from the surfel (lifted 2 units along
   its normal) toward the sun reaches 400,000 units through the bake's own triangle soup without a hit. The surfel
   then leaves `B = albedo x E` like it does for a placed light, and the links carry it to the probes. Why the soup
   and not the visibility grid: the grid only knows probe-to-voxel sight lines inside the baked volume; it cannot
   answer "does the sun reach this surfel". The soup is the same geometry the bake traced its links through.
2. *Sky at a probe.* A probe stores, per octant, the share of that octant's rays that left the scene (`skyVis`) and
   the mean glass tint those rays crossed (`skyTint`, v4). The weather gives a six-axis directional ambient (what a
   surface facing each axis takes from the whole sky and surroundings). For each of the six axes the probe's sky
   irradiance is `pi x ambient(axis) x mean over the four octants on that axis's side of (skyVis x skyTint)`.
   The `pi` puts it in the same unit as the gathered bounce (the shader divides by pi).
3. *The weather's ambient where the grid stands in.* The viewer lays the weather's ambient on every surface
   unshadowed. Where the bounce grid is valid the grid's sky term REPLACES it (the program takes away `share x its
   own ambient`, share = how much of the pixel's grid sample is valid; probe 90 shows that share). Without this the
   sky would be counted twice and a porch could never be darker than the street.

**What is exact.** The weather colors and the sun direction are the ones the view is drawn with (same record, same
hour blend). The visibility and tint are the bake's stored numbers, untouched. The sun's shadow uses the bake's own
triangles. The link gather, the grid and the shader blend are the row's existing path.

**What is approximated (say these to anyone reading the picture).**
- Eight directions against six axes: an axis takes the plain mean of its four octants; the cosine weighting inside
  an octant is ignored, and the sky is taken as even inside an octant.
- The weather's ambient is a "what a surface facing this way receives" color, not a sky radiance. Using it scaled by
  visibility is right in the open for an up-facing surface (all four upper octants open: exactly the weather's
  ambient) and under full cover (0). On flat open ground the four lower octants see the ground (visibility 0), so a
  wall takes about half the weather's sideways ambient from the sky term and a down-facing surface almost none; the
  rest of what the weather's flat ambient used to give them now has to come from the real bounce (the sunlit ground
  through the links). That is physically the right source, but it is darker than the weather's authored fill.
- One sun ray per surfel: a hard shadow edge at surfel size, no penumbra.
- The sun passes glass untinted (the soup's glass list is not consulted for the sun ray); the sky's tint is applied.
- No clouds: neither the sun nor the sky is dimmed by the cloud layers.
- The sky lights probes only. It does not light surfels, so the sky's own bounce off surfaces (sky -> wall ->
  porch) is not in. The sun's bounce is.
- A room's sky through a window is left out (interiors untouched; the bake stores 0 for them).
- Legacy (non-PBR) shapes switch from the flat ambient to the directional grid sky where the grid stands in; their
  ambient specular stays. PBR shapes' bounce takes the material's AO in this mode.
- The sun's strength follows the viewer's PBR sun scale (1 unless pinned), as the direct sun does.
- One exterior bake is kept for the weather/hour re-relight (0.4 s after a change; no new bake).

**Controls for gates only.** `WW_CELL_SKY_RED=off|novis|notint|sunthrough|keepamb`; with `WW_CELL_GI_DUMP` an
exterior also writes `gi_sky.bin`, `gi_sun.bin`, `gi_sky.txt`. Probe 90 = the share of the weather ambient replaced.

**Gate.** `tests/spells/cell_sky.sh` + `cell_sky_check.py`. The checker reads the plugin's weather and climate
itself, traces its own sun rays through the soup, gathers its own links and blends its own grid; nothing calls
NifSkope. Cameras at eye height (Concord's street is near z 6200), picked from the bake's sky shares.

| cell / view | stage | green | red that must fail |
|---|---|---|---|
| Concord (657 probes, 26865 surfels) | W weather | ambient up 0.1304 0.2388 0.4045, sun 0.759, 0.013 deg off | -- |
| | U sun | 5739 sunlit of 15857 facing; agree 100.0% | sunthrough: 10076 shaded lit, total 2.242 |
| | S sky | 386 probes see sky; agree 100.0%, 1.000 | off: 0.004 (no sky file); novis: 4.337 |
| | B totals | agree 100.0% (sky 69.7% of the total, lamps 0.39%) | |
| | C grid | 3283 voxels; agree 100.0% | |
| open street | D probe 5 / 90 | 100.0%, 0.999 / 100.0%, 1.000 | off: 0.000 |
| covered spot | D probe 5 / 90 | 100.0%, 0.998 / 100.0%, 1.000 | novis: probe 5 total 2.805 |
| covered spot | O | upper sky 0.215 of the open sky (viewer 0.215); an open street probe: 0.90 | |
| open / covered | R | darker where the grid stands in: 100% / 100% (79 / 100 levels) | keepamb: 0.0% |
| Graygarden (279 probes, greenhouses) | T glass | 141 probes through glass; agree 100.0% | notint: 0.0%, 1.144 |
| DmndSolomonsHouse01 | I interior | probe 5, lit picture, gi_surfels / probes / grid byte-identical, plain and Lookdev | |

After the merge with main a54dfd62 the green reran PASS with the interior compared against main's own exe.

### 2ai. Trees in the far map (lane TREE1, 2026-10-02)

(Letter: next free after 2ah SKY1 at splice time.)

What: `probefar` (2h) put ground, water and the .lodi occluder boxes in the far soup and no trees, so a wooded
hillside bounced like bare ground and a far probe could stand inside a canopy. Now every .lodi placement whose
.lodo base is a tree goes in as its COARSEST authored slot (lodgen never decimates; no model is simplified).

How (src/probefar.cpp, class FarTrees + the tree loop in probeFarBuild):
- Texture layers from Objects/<world>.LodgenArrays.txt (`source` column), else the chunk manifests' M/A lines;
  one BC3 array DDS read per layer. Each alpha-tested triangle keeps the share of its texels that pass (cover),
  shrunk about its centroid by sqrt(cover), in the mean linear color of the passing texels.
- Canopy: each cell's roofline is raised by every authored vertex (all slots) of the trees in it AND by every
  tree vertex that went into the soup (a shrunk leaf can cross into the next cell; MISTAKES 2026-10-02).
- Census: `far: trees N of M ... (T triangles from soup triangle F, keeping A of their area), tree materials K
  (without a texture 0), cells a canopy raised C`. Tree triangles are soup[F:].
- Reds (`--red`): notrees, treebox (a solid box per tree), treeshift (every tree one cell east), canopy (roofline
  ignores trees).

Gate (tests/spells/probe_far.py + the independent reader tests/spells/probe_far_trees.py, own BC3 decode on
lodgen_native_decode.py): every placed tree in the soup once (centroid match, tol 0.5), no stray tree triangle,
tree area within 0.05 of what the textures keep, canopy >= hoist; with --treeref the bake against a brute-force
trace of the reference (soup ground + boxes + first-slot models cut texel by texel, kmax 8): irradiance median
<= 0.10, p95 <= 0.25, and median <= half the no-tree bake's. Bar set before any tree bake.

Numbers (merged exe, 2026-10-03):
- Concord block -20,12..-10,22: 8771/8771 trees, stray 0, twice 0, area 0.985, canopy 512, roofline 512;
  bar 0.059/0.103 (no trees 0.230). Own-soup reference 0.053/0.108 (2h without trees was 0.027/0.046: open).
- Wooded block -24,20..-14,30: 9166/9166, area 0.985, canopy 512, bar 0.056/0.088 (no trees 0.284).
- Reds all FAIL: notrees 0/8771; treebox stray 105252, area 10.021; treeshift stray 214987; canopy roofline
  -2356; hoist -1488.
- Whole Commonwealth: 70 s, 280 MB (was 51 s, 265 MB): 85582 of 85582 trees, 2,013,012 triangles keeping 0.328
  of their area, 30 materials, 0 untextured, 3713 cells raised.

Open: the far map's own-soup reference error doubled with the trees in (still inside the bars); the cell view
has no far-map consumer, so the eye-height renders are offline (scratch eyeshot.py); FO4CS reader comes last.

### 2aj. The relight bounces until it settles (lane BOUNCE2, 2026-10-03)

(Letter: next free after 2ai TREE1 at splice time.)

What: the probe relight (2i) did one bounce. Each surfel's light B = albedo x E came from the cell's lights
(and outdoors the sun), the probes gathered it, and that was the end. The deck's row C11 (slides 46-49) feeds
each surfel the light its probe gathered on the previous pass. Now the relight repeats this until it settles.

How (src/probegi.cpp, after the probe hash in step 3):
- Which probe each surfel reads: from the surfel's point + 2 units along its normal, every probe within the
  grid radius r that is visible (a BVH ray) and in the surfel's room. The weight is (1 - d^2/r^2)^2, the same
  as the grid's. If none qualifies inside r, the closest allowed probe within 2r is used with weight 1. If
  there is none at all, the surfel takes no feedback.
- The surfel's room is the nearest .tbk v4 room box to p + 17.5n, within 35 units. A probe matches when either
  of its two pext room ids is the surfel's room. A surfel in no box takes any visible probe in reach (only
  the wall ray guards it).
- A pass: B_k = B_1 + albedo x E_feed(k-1) / pi, where E_feed is the n^2 blend of the facing axes as in the
  renderer. Then the probes re-gather with the same expression order as pass 1 (links, then the unlinked
  factor, then outdoors the sky part).
- Stop when the largest change per surfel <= 1e-3 x the brightest B, or at 64 passes. The bar is set for
  quality. bungo's rule: never cut passes for speed; report the time instead.
- Pin WW_CELL_GI_PASSES=n runs exactly n passes. n = 1 is the old relight byte for byte. Gate reds:
  WW_CELL_GI_RED=rooms (feedback ignores rooms and walls) and =grow (feedback albedo 1.5).
- Census: `gi bounce: N passes (settled), last change X of the brightest Y, gain G; F of S surfels read a probe
  (C the closest only), R in a known room; feed rays, blocked, probes in another room refused; ms`.
- Dump: gi_bounce.bin (last B per surfel), gi_feed.bin (each surfel's probes + weights + room), gi_passes.txt.
  gi_surfels.bin keeps pass 1. gi_probes.bin and the grid are the last pass's.
- No menu row, no INI key. Settled is the default.

Gate (tests/spells/cell_gi.sh + cell_gi_check.py):
- Stage F, an independent twin. It builds its own feed lists (own rays, own room boxes) on 400 sampled
  surfels, and counts every dumped read from another room and the sampled reads through a wall. It then
  repeats the passes with its own gather and compares the pass count, the per-pass sum of B (1%) and the last B
  (97% agree).
- Settled: the change per pass must fall, and gain over the source <= 1/(1-albedo). The source is the direct
  light plus, outdoors, the sky once off the surfaces.
- Stage P: the Pass view's GI at 1 pass and settled, same camera. No pixel darker, mean brighter.
- The one-pass pin against the exe from before the lane: every dump .bin byte-identical. The picture is
  within GPU noise (at most 1 level on 100 pixels; the old exe against itself differs that way too).
- cell_pass and cell_sky pin WW_CELL_GI_PASSES=1, because they rebuild one pass and compare with a one-pass
  exe.

Numbers (lane exe, 2026-10-03):
- Vault111Cryo: 8 passes, gain 1.6327 (bound 2.3595, albedo 0.576), rate 0.409 per pass. 20881 of 26978
  surfels fed (3170 by the closest probe). 69816 reads from another room refused. 83 ms.
  - The door room (box -596443128): 74% of its surfels get no direct light, and 85% of its settled light is
    bounce.
  - Pass GI view, door camera: mean 32.66 -> 42.57. Overview: 41.87 -> 47.66.
- DmndSolomonsHouse01: 4 passes, gain 1.1274 (bound 1.1784), 8 ms.
- Concord (Lookdev CommonwealthClear 12:00, the street camera): 5 passes, gain 1.4496 (1.0802 over its
  source, bound 1.3455), 71 ms. Pass GI 73.21 -> 74.18 (the sky dominates).
- Every F: reads from another room 0, through a wall 0. The one-pass pin is byte-identical in all three places.
- Reds, all FAIL F:
  - rooms: reads from another room 26956 / 966 / 46826, through a wall 991 / 1177 / 2350;
  - grow: 64 passes, never settled, last B agree 22.7 / 20.9 / 13.8%;
  - onepass: twin 1 pass against 8 / 4 / 5, last B agree 34.4 / 99.8 / 58.8%.
- Time: the bounce adds 8-83 ms to a 0.1-0.2 s relight.

Open:
- Room boxes cover surfel air poorly: 8% of surfels lie inside a box and 46-53% within 35 units. A surfel in
  no box is guarded by the wall ray alone.
- The GPU relight, bricks, the sky picture and fog GI are out of scope. The FO4CS reader comes last.

## 3. Open

- `.tbk` v4: two surfel sides per cell (gives back the refused thin-wall weight), room ids (2f).
- Glass tint in the bake.
- Ambient Only lights: done in 2r. Optional: compare in game at Vault 111's west end.
- Save names for the PRTP4 capture flights.
- FraternalPost11501 seen from straight above (center 553,2170,400, distance 600) and PickmanGallery01 (562,440,150):
  probe 8 and the diffuse check part on 13% / 9% of clean pixels, flags honoured or not. The Fraternal patch
  is a wall strip beside two lamps; probe 8 reads about 2x the model there and 0.2x on the floor under them.
  No single light's removal explains it, and the pixels are front-facing. Overlay sheets (dirt / decal cards over
  the walls) are the suspect. Not a gate view until named (diag: scratchpad/rim1_20261001/green_diag.py).
