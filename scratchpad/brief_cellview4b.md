# Lane CELLVIEW4B -- apply, build and gate CELLVIEW4's never-compiled code (BUILD LANE, owns build + exe slots)

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` for every timestamp. Never commit, never `git stash`,
  never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/cellview4b_20260919/` (`BUILDING` first, `DONE` last;
  `report.md` incremental, section 0 inside ten tool calls; fallback name `DELIVERABLE_TEXT.md`; `PENDING.md` past
  half context). Pictures only under that folder, NEVER repo-root `images/`.
- You OWN the build slot and the exe slot; no other lane is live. `tasklist | grep -i -E "Fallout4|NifSkope"; echo
  rc=$?` as ITS OWN command before every build and exe run. Fallout4 up = write `BUILD PENDING` and END; never
  wait-loop; NEVER end your turn waiting on your own batch -- wait in the foreground, then continue to DONE. ONE
  NifSkope, `--port <unused>` + `WW_WINDOW_AT=1960,40`, absolute paths. A NifSkope without `--port` is bungo's: never
  kill it; rename the exe aside at link time. Build: MSYS2 UCRT64, skill `nifskope-ww-build-verify`. Rung ONCE
  `release/NifSkope.before_cellview4.exe`; never touch another rung; NEVER run any `before_*` rung with a GUI.
- Exe at launch: 23,625,728 B, 2026-09-19 19:49:03, sha1 c529e3c12fe4216a3c33cc14631e3c811169fbb0.
- Read first: `CONSTITUTION.md`; HANDOFF top block; `scratchpad/cellview4_20260919/PENDING.md` and
  `DELIVERABLE_TEXT.md`; `scratchpad/cellview3_20260919/report.md`; skills `nifskope-ww-build-verify`,
  `ww-build-pending-resume`(if present), `ww-anchored-hookup`, `ww-test-harness-add`, `nifskope-ww-render-shot`,
  `ww-legend-matches-picture`.

## The work
1. `python scratchpad/cellview4_20260919/hookup.py --check` (expect 13 of 13), then `--apply`. Build. The code has
   NEVER been compiled: repair compile errors in the lane's own new files freely; any change to the design is
   reported, not silent.
2. Shoot Sanctuary -20,7 top-down, same camera as `scratchpad/cellview3_20260919/images/after_ground.png`, and
   compare with the SIMULATED target `scratchpad/cellview4_20260919/images/sim_m20_7_blended.png` per quad (mean
   colour difference; the simulation is unlit, so state how you normalise lighting). The director saw a straight
   SEAM in the simulation where the top-right quadrant meets its neighbours: decide with the record bytes whether the
   game's data has that seam (quadrants carry different layer sets) or the simulation/viewer made it; if the viewer
   makes it, repair it.
3. The root became `BSOrderedNode` so transparent ground passes sort by block order. CELLVIEW4 wrote its own refuter:
   this re-sorts EVERY transparent shape in the cell. Test it: a cell with glass/alpha objects (the downtown cell)
   before vs after, picture diff outside the ground must be zero or explained. If it is not, put the ground alone
   under its own ordered node.
4. Vertex budget: print counted vs the 12M cap for Sanctuary and downtown; the refusal path must still refuse (row).
5. Bare quads 24 -> 6 and the marker (`markerxheading.nif`, REFR 00066245) gone from the downtown picture: confirm
   on the real exe, gate rows that fail on exe c529e3c1's behaviour (by reasoning or source revert in your own
   build, never by launching a rung with a GUI).
6. Gates before/after: `cell_pick.sh`, `cell_open.sh`; neighbours `render_shot.sh`, `harness_window.sh`,
   `native_open.sh`, `impostor_draw.sh`.
7. Pictures for bungo, before | after, same camera: Sanctuary ground, downtown. LOOK; plain words on what is wrong.

## Rules
Authored LOD models only; `--road-detail 1`; masters ship OFF; a repair gets no toggle; no "fixed/final/true" --
mechanism + refuter; never pick the flattering view; plain words.

## Report
Exe mtime/size/sha1; sim-vs-real table; seam verdict; ordered-node verdict; gate counts; WW_CHANGES + HANDOFF text;
MISTAKES appended to root MISTAKES.md (top CRLF, byte splice, CR before/after); skills to BOTH trees, equal sha1.
Final message under 250 words.
