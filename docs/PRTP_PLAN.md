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
   linear, axis, off). Not yet: fog, hemisphere/box shapes (drawn as omni),
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
cell ambient inside a sphere of 1.22077 x its radius, radius = base + XRDS (XRDS is a delta). Inside, each
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
