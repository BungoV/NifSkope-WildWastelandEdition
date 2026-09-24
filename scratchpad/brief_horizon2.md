# Lane HORIZON2 -- the terrain horizon sheet over-occludes: find the cause, prove it with a third witness, fix it

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main, ONLY lane in the tree. Exe at launch:
  `release/NifSkope.exe` 2026-09-18 21:59:46, 22,952,448 B, sha1 e578b76f9d7a2d011363e4300a2c94967e14d605 (HORIZON1's
  final; re-read and print it, first line of the report). Rung ONCE before your first build:
  `release/NifSkope.before_horizon2.exe` (never delete any `release/NifSkope.before_*.exe`,
  `release/NifSkope.archlock1_rung.exe`, `release/NifSkope.at_0117.exe`, or a `NifSkope_inuse_*.exe`). Markers
  `scratchpad/horizon2_20260918/BUILDING` (touch FIRST, REMOVE when you write DONE) / `DONE` (first word `horizon2`).
  Report `scratchpad/horizon2_20260918/lane_horizon2_report.md`, INCREMENTAL (section 0 inside your first ten tool
  calls); `PENDING.md` past half your context. Never commit, never `git stash`, never edit `WW_CHANGES.md` or `HANDOFF.md`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command before every build and every exe
  run. Fallout4 up = no build and no exe run: do everything else, then PENDING.md headed `BUILD PENDING`, stop; never
  wait-loop on the game. A NifSkope with no `--port` is bungo's window: rename the exe aside as
  `NifSkope_inuse_<pid>.exe` at link time, never kill it; a harness NifSkope (has `--port`) that wedges is ended by
  sending it `NifSkope::open <scene>` as a UTF-16LE UDP datagram on its port (src/main.cpp IPCsocket), never by kill.
  Headless runs: `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time, second monitor only, every path ABSOLUTE
  `E:/...`, every gate script passes the scene file positionally.
- Build: MSYS2 UCRT64, `export PATH=/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`, `mingw32-make -f Makefile.Release -j8`,
  make's exit code is the gate; `nifskope-ww-build-verify` INCLUDING its new object-vs-header check. ONE background
  waiter at a time. `date` for every timestamp.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (the HORIZON1 and LODIV7 LANDED blocks); root `MISTAKES.md`
  top 20; `scratchpad/horizon1_20260918/lane_horizon1_report.md` IN FULL (s3 the sheet, s5 the refuter, s8 the gate
  with the G3 numbers, s12 the rows, s13 NEXT); `docs/LODGEN_NATIVE_LODO_LODI.md` s4.10 + the role-7 section of
  `docs/LODGEN_TERRAIN_VT.md`; the horizon marcher and `maxAlong` in the sources the report names; the terrain
  horizon refuter (`--horizon-refute`, `WW_HORIZON_REFUTE_DUMP`).
- Skills: `nifskope-ww-render-shot` (the `horizon` rows and the SUN/CONTROL bullet), `nifskope-ww-lodgen`,
  `nifskope-ww-build-verify`, `ww-test-harness-add`, `ww-control-calibration`, `ww-texel-picture`.

## The defect, as measured by HORIZON1 and seen by the director
- Gate G3 terrain: 51.69% balanced agreement at azimuth 240 / elevation 15 against the in-bake reference cast (floor
  97%); at elevation 5 no disagreement; at 15 the reference finds 59 lit texels of 4,100 and the sheet ~2. Mean error
  3.65 deg on a mean REFERENCE elevation of 63.6 deg. The viewer's note line for chunk 4.4.-12: terrain horizon
  14.47..87.18 deg, mean 64.37, **0.0% lit at a 15-degree sun** -- every terrain texel in every picture is dark,
  including open ground and the shoreline. The object stream on the same chunk: mean 26.68 deg, 97.73% agreement,
  control 60.59%.
- The suspicious fact is not the 51.69%, it is that BOTH the sheet AND the reference say the terrain's mean skyline is
  above 60 degrees. A texel on the shore looking out over water must read a skyline near 0 in that bin. If the
  reference says otherwise, the reference shares the bug (same lattice, same reach handling, same treatment of
  "outside the loaded data"), and a 97% agreement floor between two wrong witnesses would be worthless.

## The work (each step lands in the report before the next)
1. **A third witness that cannot share the bug.** A Python script over the RAW inputs (the BTD heightmap through the
   existing reader, plus the `.lodi` placements' world boxes / the `.lodo` meshes -- no lattice, no mip, no
   `maxAlong`): for TEN hand-picked receivers on chunk 4.4.-12 (three on open shoreline facing water, three on open
   flat ground, two in a street between kit houses, two at a building's foot), cast rays at 1-degree azimuth steps
   over the full reach and record the true skyline per bin. Table: receiver, position, per-bin true elevation, the
   sheet's stored elevation, the in-bake reference's elevation. The shoreline receivers' water-facing bins MUST read
   below 5 deg from the third witness; if the sheet/reference read 40+ there, the cause is in the bake's data
   handling, not in the rule. This table is the report's centrepiece and every later claim points at a row of it.
2. **Locate the cause** from the table, by elimination, each candidate refuted or confirmed with a number: (a) terrain
   outside the chunk / outside the loaded cells treated as occluding (a wall at the data edge), (b) the max-Z lattice's
   mip choice in `maxAlong` (a coarse mip's max-Z read as if it were at the receiver's distance), (c) the receiver's own
   square / tangent-plane rule (HORIZON1 s13 candidates), (d) the reach of 127,561 u reading beyond the data, (e) unit
   or axis confusion (bin 0 = north clockwise, Z up, 8 u a heightmap texel), (f) placements' boxes inflated (a kit house
   box vs the mesh). One paragraph per candidate with the row that kills or keeps it.
3. **Fix the cause in the bake**, nothing else. Way back unchanged (`--no-terrain-horizon`, `--lodi-v7`). If the fix
   changes the object stream's numbers too, say so and re-run its refuter. Re-bake chunk 4.4.-12 with the HORIZON1
   recipe (`scratchpad/horizon1_20260918/bake.sh`).
4. **Prove it**: G3 terrain to the 97% floor at elevations 5/15/30/60 x azimuths 120/240 against the in-bake
   reference, AND the ten receivers of step 1 within 2 deg of the third witness in every bin (the third witness is the
   gate now: `tests/spells/lodgen_horizon.sh` gains G6 = the ten-receiver table, with a red control that feeds it the
   old exe's sheet). The viewer note line on chunk 4.4.-12 at sun 120,15 must show terrain lit > 0% and the shoreline
   lit in the picture. Neighbours before/after on your final exe: `lodgen_horizon.sh` (22/1 -> 23+/0), `lodi_v7.sh`
   12/0, `lodl_channels.sh` PASS, `lodgen_slab.sh` 16/0, `native_open.sh` 17/0/2, `render_shot.sh` 82/0, `lodl_open.sh`
   23/0, `lodgen_native.sh` PASS.
5. **Pictures** `scratchpad/horizon2_20260918/images/`: the HORIZON1 framings (`chunk_horizon_{full,close}_e15_a120`
   and `_a240`, plus e05 and e30) BEFORE (HORIZON1's, copied and named) and AFTER, the control render beside the real
   one, and the raycast overlay. Captions through `ww-texel-picture`: the number in the caption is the number in the
   report.
6. **Docs**: the corrected numbers wherever HORIZON1 quoted the old ones (`LODGEN_NATIVE_LODO_LODI.md`,
   `LODGEN_TERRAIN_VT.md`, `LODGEN_CENSUS.md`, `FO4CS_IMPROVED_LOD_PLAN.md` s9), a MISTAKES entry for whichever
   witness shared the bug (a refuter that shares the bake's data path is not a refuter), skill deltas delivered as text.

## Rules
- No decimation, authored LOD models only, `--road-detail 1`. Masters ship OFF. No "fixed/final/true": mechanism +
  refuter. Every number in the report is from a log you name.
- Do not touch the object stream's format or the viewer beyond what the terrain fix forces (say why if it does).

## Report (`lane_horizon2_report.md`, incremental)
0 exe at launch + rung; 1 the ten-receiver table; 2 the cause by elimination; 3 the fix (diff summary, files, lines);
4 the proof (G3 numbers before/after, G6, viewer note line); 5 neighbours; 6 build (mtime, size, sha1, `find src tests
res -newer` empty, object-vs-header check); 7 pictures + captions; 8 docs + skill text; 9 WW_CHANGES paragraph +
HANDOFF LANDED block for the director to splice; 10 rows for bungo; 11 MISTAKES entries written; 12 finished-work skill
review. END with `DONE` (first word `horizon2`) and five plain sentences for bungo, ending with: his open window needs a
restart.
