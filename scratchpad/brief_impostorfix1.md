# Lane IMPOSTORFIX1 -- the impostor card is still a spray of fragments: find out WHICH HALF is wrong

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` for every timestamp. Never commit, never `git stash`,
  never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/impostorfix1_20260919/` (`BUILDING` first, `DONE` last;
  `report.md` incremental, section 0 inside ten tool calls; `PENDING.md` past half context).
- PHASE 1 IS OFFLINE: no build, no `release/NifSkope.exe` run, no exe-starting spell (another lane holds the exe slot
  for its gates). Python + PIL/numpy on files only. Do not edit any `src/` file in phase 1.
- Read first: `CONSTITUTION.md`; HANDOFF top block; `scratchpad/impostorshow_20260919/PENDING.md` (the previous lane's
  state -- its claims are CLAIMS); `docs/LODGEN_IMPOSTOR_SPEC.md`; `res/shaders/impostor_oct.{vert,frag}`;
  `src/impostoroct.*`, `src/impostorcard.*`, `src/gl/impostordraw.*`; the bake at `src/nifskope_ui.cpp` ~22680-23260
  and `src/lodgen.cpp` ~2990-3060 (frameOffset); skills `ww-control-calibration`, `ww-silhouette-compare`,
  `ww-simulate-before-build`, `ww-analytic-fixture-gate`, `ww-spec-gate-audit`.

## What the director saw (2026-09-19 13:25)
`scratchpad/impostorshow_20260919/images/10_orbit_maple_blasted_n4.png`: the mesh row is a clean bare trunk with two
branches; the card row, at EVERY azimuth and both elevations, is a trunk-ish column surrounded by dozens of detached
flakes, streaks and blocky stair-step patches spread over the whole frame. Mean IoU 0.35; with the 3-frame blend OFF
0.44. The previous lane called this "follows the tree's real shape" and blamed thin twigs + the alpha threshold. The
director does not accept that: a single un-blended frame viewed from exactly its own bake direction is a PHOTOGRAPH
of the mesh and must overlap it almost perfectly. 0.44 means something basic is still wrong. Look at the picture
yourself first.

## Phase 1 -- offline, decide bake vs draw (report s1-s3)
1. KNOWN-ANSWER CONTROL ON THE SHEET. From a fixture set (`scratchpad/impostorshow_20260919/fixture/blast_n4/...`,
   also rock_n4 and blast_n8): decode the colour sheet's coverage exactly as the spec says, cut out ONE frame, and
   compare its silhouette with the mesh render from that frame's own bake direction (the previous lane's orbit
   frames under its scratchpad, or the `diag*` folders; if no mesh render exists at an exact frame direction say so
   and list which one phase 2 must shoot). Report per frame: IoU after the frame's `frameOffset` and extents are
   applied by the SPEC's arithmetic, written by you in Python independent of the C++/GLSL. If the frames themselves
   are flaky/blocky (look at them: are the flakes IN the sheet?) the BAKE or its BC compression / mip / coverage
   encoding is at fault -- show the texels (`ww-texel-picture`). If the frames are clean photographs, the DRAW is at
   fault.
2. PYTHON REFERENCE CARD. Write the spec's whole draw in numpy (direction -> grid cell, 3 weights, per-frame UV with
   frameOffset, height parallax, coverage decode, threshold) and render the card for the same 24 views from the
   SHEETS alone. Compare (a) reference vs mesh, (b) reference vs the viewer's card pictures. (a) good + (b) bad = the
   GLSL/C++ draw is wrong, and the diff image says where. (a) bad = the method as specified or the data is wrong; then
   switch stages off one at a time (no parallax; nearest frame only; no frameOffset; coverage un-decoded) and table
   the IoU of each so the guilty stage is named by measurement.
3. Specific suspects to confirm or clear with numbers, not opinion: height sampled where coverage is 0 (undefined
   height -> huge parallax throws -> detached flakes); parallax done in one step with no clamp; blending coverage of
   three frames BEFORE the test vs testing each frame; BC compression blocks in the alpha/height channels (stair-step
   patches are 4x4-block shaped?); mips sampled across frame borders (no per-frame clamp/gutter); N=4 simply too
   coarse for a 3-frame blend of a thin subject (then say what N the spec's own floor implies); frame picked from
   the view DIRECTION vs from the camera POSITION relative to the card centre.
4. End phase 1 with a verdict line per suspect (GUILTY n.nn -> n.nn / CLEARED / NOT TESTABLE OFFLINE) and the exact
   source change you propose (file:line, before/after), plus a gate row that FAILS on today's exe for each.
   Park at `PHASE 1 DONE -- BUILD SLOT WANTED` and stop. The director gives you the slot.

## Rules
No "fixed/final/true" -- mechanism + refuter. Never lower a floor. Never pick the flattering view. Plain words.
Append your own mistakes to root MISTAKES.md (top of that file is CRLF; byte splice). Final message under 250 words,
with the three most telling pictures named by path.
