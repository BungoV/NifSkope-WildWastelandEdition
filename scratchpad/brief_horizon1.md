# Lane HORIZON1 -- horizon maps for far LOD shadows: a skyline per LOD vertex and per terrain texel

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main, ONLY lane in the tree; starts AFTER LODIV7 lands (the
  director launches you and fills in the exe line). Exe at launch: `release/NifSkope.exe` = LODIV7's final: 2026-09-18 19:05:11, 22,764,544 B, sha1
  28ac412c6da96e0707e492c175822d2076016dc5 (re-read and print it yourself; first line of the report). Rung ONCE before your first build:
  `release/NifSkope.before_horizon1.exe` (never delete any `release/NifSkope.before_*.exe`,
  `release/NifSkope.archlock1_rung.exe`, `release/NifSkope.at_0117.exe`, or a `NifSkope_inuse_*.exe`). Markers
  `scratchpad/horizon1_20260918/BUILDING` (touch FIRST, REMOVE when you write DONE) / `DONE` (first word `horizon`).
  Report `scratchpad/horizon1_20260918/lane_horizon1_report.md`, INCREMENTAL; `PENDING.md` past half your context.
  Never commit, never `git stash`, never edit `WW_CHANGES.md` or `HANDOFF.md`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command before every build and every exe
  run; Fallout4 up = NO BUILD and NO EXE RUN (bungo is playing, 19:12 on, and said not to interrupt anything): do every
  step that needs neither (measurements with the Python readers, the design, ALL the C++ and script and doc edits,
  the refuter scripts), then write PENDING.md headed `BUILD PENDING` with the exact resume point and stop; the
  director resumes you when the game is down. Never wait-loop on the game. A NifSkope with no `--port` is bungo's window: rename the exe aside as
  `NifSkope_inuse_<pid>.exe` at link time, never kill it. Headless runs: `--port <unused>` + `WW_WINDOW_AT=1960,40`,
  one at a time, second monitor only, every path ABSOLUTE `E:/...`.
- Build: MSYS2 UCRT64, `export PATH=/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`, `mingw32-make -f Makefile.Release -j8`,
  make's exit code is the gate (`nifskope-ww-build-verify`). ONE background waiter at a time. `date` for every
  timestamp; never type a time from feel.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (the RULED far-shadow line of 2026-09-18 17:4x, the LODIV7
  LANDED block); root `MISTAKES.md` top 20; `docs/FO4CS_IMPROVED_LOD_PLAN.md` 270-300 (the far-shadow rulings of
  2026-09-11: one shadow representation a placement, cells inside the loaded grid drop out of the far casters by the
  cell-range table, self-shadowing excluded by identity) and its census words; `docs/LODGEN_NATIVE_LODO_LODI.md` s4
  (the v7 group table and the per-vertex sky stream LODIV7 added -- your object stream mirrors the sky stream), s4.8;
  `docs/LODGEN_TERRAIN_VT.md` v2 header (roles 0/2/5/6, tile = 512 content + 8 border, finest 8 world units a texel,
  BC formats, the `.lodm` index words) and 2.5h (the object-AO term); `docs/LODGEN_CENSUS.md` 6.1;
  `src/lodgen.cpp` ~8146 `LodgenObjectHeightField` and `lodgenObjectSkyVis()` (SLAB1's 128-unit min+max lattice
  marched in 8 azimuths, distances 128..2048 x1.5, reach 1458 -- it IS a horizon march that collapses to one scalar;
  you keep the per-azimuth maximum elevation instead), `src/lodgenao.h` (`LodgenAoScene`, `skyVisibility`),
  `src/nativeemit.cpp` (the `place`/`perVertex` loop where LODIV7 casts the per-vertex sky), `src/lodinative.cpp`
  (the `WW_LODL_CHANNEL` seam), `src/btdterrain.cpp` (the sheet sampler), `src/lodtsheets.{h,cpp}` (roles).
- Skills (repo `.claude/skills`): `nifskope-ww-build-verify`, `ww-test-harness-add`, `ww-channel-view-refuter`,
  `ww-clearance-instrument`, `ww-population-refuter`, `nifskope-ww-render-shot`, `ww-texel-picture`,
  `ww-control-calibration`. Any procedure you invent twice becomes a skill under
  `scratchpad/horizon1_20260918/skills_proposed/<name>/SKILL.md` with frontmatter.

## bungo's words, verbatim (2026-09-18 17:4x .. 18:0x)
"B sounds good" (option B = horizon maps baked offline, over option A = fixing the FO4CS far shadow map with back-face
casting and bias). "I just want a cheap way to render shadows for distant LOD objects, which are usually pretty simple
geometry wise. And with no artifacts. And with those shadows also being cast at me from behind, so like a tall tower 2
kilometers away from me would cast a shadow on me on a low sun angle." "Fo4CS far map covers the terrain, but the lod
objects needed identity, so it never worked." "Okay, implement that, also, what about the performance cost and will
the bakes weight much?"

The division of labour, ruled: the horizon bake is LOD RECEIVING shadow (from itself, its neighbours, the terrain,
casters kilometres away) -- no runtime shadow map, no identity, no bias, one compare. The tower shadowing HIM is the
FO4CS far map's job, whose caster set becomes the placements OUTSIDE the loaded grid by the `.lodi` cell ranges (no
LOD twin of a loaded object, so no identity test except the grid-edge band, where LODIV7's group is the witness).
FO4CS reads all of it LAST, by standing order: you write the CONTRACT, you build nothing there.

## The work (in this order; each step lands in the report before the next)
1. **Measure before designing.** (a) The `.lodo` edge-length distribution over the drawn slot-0 meshes of the
   Commonwealth region pair `scratchpad/viewfix_20260917/urban_ao/nat/FO4CSLOD/Commonwealth/` (or LODIV7's v7 pair
   if it re-emitted one): p50, p90, p99, max edge length in world units, and the count of triangles with an edge over
   512 / 1024 / 2048 units, per base model for the top 20 offenders -- this is the size of the "shadow line smeared
   across a flat quad" limit, and it goes to bungo as a number. (b) The tallest placements in the region (top 10 by
   world-space bound top above the terrain under them) and, from the FO4 climate/ESM data the tree already reads
   (`src/esmdata.cpp`), the lowest sun elevation the Commonwealth uses; reach = height / tan(elevation), tabled, so the
   march reach is chosen from data. (c) The sizes: the region's v7 `.lodi` bytes per placement, the drawn vertex count
   per placement (the sky stream's length), the finest tile's bytes per role, and from them the projected bytes of
   every option in step 2 for the region AND scaled to the populated worldspace (count populated chunks from the
   Commonwealth `.btd`/land data, not from the 192x192 rim).
2. **The object horizon stream, `.lodi` v8.** Mirror LODIV7's per-vertex sky stream: `offVertexHorizon`,
   `u32 first[instanceCount+1]`, then per library vertex `A` bytes, one an azimuth, each the maximum elevation of any
   occluder in that azimuth bin as `u8 = round(elevation_deg / 90 * 255)` above the vertex's tangent plane (rays below
   the plane are the surface itself and are not cast; N.L handles that side). `A` = 16 azimuths, 22.5 degrees, bins
   centred on north and stepping clockwise (say the convention in s4.10 with a figure); the sun azimuth at runtime
   interpolates the two neighbouring bins. Cast in the `place`/`perVertex` loop: the near part against the scene
   `LodgenAoScene` (own triangles, other placements, the terrain, neighbouring chunks' placements, exactly the v6/v7
   population), the far part by the slab lattice march extended to the reach from step 1b (the lattice steps grow x1.5,
   so cost is logarithmic in reach -- state the step count). `--lodi-v7` writes LODIV7's bytes (way back, byte-identical
   gate). Census: `vertexHorizonBytes`, `horizonAzimuths`, `horizonReach`, `horizonSteps`, mean bytes a vertex.
3. **The terrain horizon sheet, `.lodt` role 7.** Two RGBA sheets (bins 0-7, bins 8-15), BC3, at a COARSE level: 64
   world units a texel (an 8x8-texel... no: 64x64 texels a cell), NOT the 8-unit finest -- shadows are low-frequency by
   the FO4CS doctrine of 2026-08-24 and the finest would cost 19 GB a worldspace. Cast per texel from the terrain
   height plus 4 units, the same march as step 2 against the same objects (the object-AO term's lattice already holds
   them) and against the terrain itself (the `.btd` heightfield, reach from step 1b). Index words in `.lodm`:
   `horizon: azimuths 16, texel 64, reach R`. `--no-terrain-horizon` = today's bytes (way back, byte-identical gate).
   Census: `horizonTexels`, `horizonMeanElev`, `horizonReach`.
4. **The viewer.** `WW_LODL_CHANNEL=horizon` with `WW_SUN=<azimuth_deg>,<elevation_deg>`: objects and terrain drawn
   flat, lit where the sun clears the interpolated horizon, dark where it does not, with a 1-degree smoothstep at the
   edge (the Sloan-Cohen softening; the width is a knob, say its name); `WW_LODL_CHANNEL=horizonbin=<n>` draws one
   bin's elevation as grey on both. Note lines READ BACK from the uploaded bytes (mean, min, max, N). THE REFUTER before
   any picture: for the close framing, a RAY-CAST REFERENCE image -- for each drawn vertex (objects) and for the
   terrain texels in view, cast ONE ray toward the given sun through the same `LodgenAoScene` + heightfield at full
   resolution, no bins -- and the horizon render must agree with it on >= 97% of pixels at sun elevations 5, 15, 30,
   60 degrees for two azimuths; the disagreeing pixels are TABLED by cause (bin interpolation / flat-quad smear /
   reach). A control that must go RED: the same comparison with the bins rotated by 90 degrees.
5. **Pictures**, `scratchpad/horizon1_20260918/images/`, chunk 4.4.-12, the CHANVIEW1 framing (`render_slots_e.sh`,
   close = `24900 -41300 2600 8 450`; full = the hotfix 7c arguments), on a fresh bake with the chunkD3 recipe plus
   your switches: `chunk_horizon_{full,close}_e{05,15,30}_a{120,240}.png`, the reference beside each in
   `horizon_vs_raycast_*.png` with the disagreement in red, and `horizon_bins_close.png` (16 bins as a grid). Captions
   carry the report's numbers. Never a proxy of the channel (MISTAKES 05:0x); never a desktop capture.
6. **Gate** `tests/spells/lodgen_horizon.sh` (`ww-test-harness-add`): G1 way back byte-identical for both switches;
   G2 stream + sheet layout invariants, refusals by name; G3 the ray-cast agreement floor with the rotated-bin control
   red; G4 a synthetic field self-test (`WW_HORIZON_TEST`): a lone 1000-unit box on flat ground -- the horizon at a
   texel D units away in the box's azimuth is atan(1000/D) within one u8 step, and 0 in the opposite azimuth; G5
   neighbours before/after on your final exe with owners: `lodgen_native.sh`, `lodi_v7.sh`, `lodl_channels.sh`,
   `lodgen_slab.sh` (16/0), `native_open.sh` (17/0/2), `render_shot.sh` (82/0), `lodl_open.sh` (23/0).
7. **Docs + the consumer contract.** `docs/LODGEN_NATIVE_LODO_LODI.md` s4.10 (the stream, the bin convention, the
   figure); `docs/LODGEN_TERRAIN_VT.md` role 7; `docs/LODGEN_CENSUS.md` the words; and in
   `docs/FO4CS_IMPROVED_LOD_PLAN.md` a new section "The far-shadow contract (2026-09-18 rulings)": (i) LOD receivers
   shade from the horizon stream/sheet, one compare, the smoothstep width; (ii) the far map's caster set = placements
   outside the loaded grid by cell range, receivers = the near world only; (iii) the group is the witness in the
   grid-edge band only; (iv) what FO4CS may NOT do (sample the far map on a LOD receiver). Provenance per line. The
   `nifskope-ww-render-shot` skill text for the new names delivered in the report (director applies to both trees).

## Gates (pre-registered)
- G1 both ways back byte-identical; G2 layout + refusals; G3 >= 97% agreement at 8 sun positions, control red;
  G4 synthetic box within one u8 step; G5 neighbours unchanged; G6 sizes tabled and within the projection of step 1c.

## Rules
- Writer + reader + viewer + docs + tests. No decimation, no subdivision of authored LOD (the long-edge answer is
  bungo's ruling, delivered as the step-1a table, not as code). `--road-detail 1`. Masters: the two new switches
  default ON only because bungo ruled the feature ("implement that"); the reach, azimuth count and softening width
  are knobs with their defaults stated in one place.
- Every number in a caption is in the report. No "fixed/final/true". A picture reads the shipped file.

## Report (`lane_horizon1_report.md`, incremental)
0 exe at launch + rung; 1 the measurements (edge lengths, tallest, sun floor, reach, sizes projected); 2 the stream;
3 the sheet; 4 the viewer + note lines; 5 the ray-cast refuter table (8 sun positions, disagreement by cause); 6
pictures; 7 gate counts + each refuter red once; 8 neighbours; 9 build (mtime, size, sha1, newer-than list); 10 docs +
contract + skill text; 11 WW_CHANGES paragraph + HANDOFF LANDED block; 12 rows for bungo (long edges, azimuths 16 vs
8 vs 32 with the bytes, reach, softening width, the coarse texel size); 13 MISTAKES entries (top of root MISTAKES.md,
the moment you see one); 14 finished-work skill review. END with `DONE` (first word `horizon`) and five plain
sentences for bungo, ending with: his open window needs a restart.
