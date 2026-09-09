# LANE LODTOPEN -- open a .lodt in NifSkope like a FO76 .btd

Header: tree E:\Projects\NifskopeWildWastelandEdition, branch main, 73 uncommitted
paths under bungo's "Not yet" (NO commits). Read CONSTITUTION.md first, then the
HANDOFF.md top block, then docs/LODGEN_BTD_FORMAT.md (the .lodt contract).
Skills to invoke via the Skill tool before choosing any route: `nifskope-ww-lodgen`
(the .lodt reader/writer/verify, the .btd import, the harness rules),
`nifskope-ww-build-verify` (the ONLY way to build and to prove the exe is newer),
`nifskope-ww-panel-style` (if any widget is added), `nifskope-ww-render-shot`
(for the proof pictures), `nif` if the scene graph is touched. Model: Opus 5.

bungo's words, verbatim: "We need a way to open it, just like we can open Fo76's
map terrain file." Then: "I need to view the AO baked and other stuff too."
He wants to VERIFY the terrain file by looking at it -- EVERY plane it carries:
heights, the baked AO plane, water height/type, and any other plane the format
document lists. Each plane must be viewable on its own (a channel selector, the
way WW_LOD_CHANNEL exposes generated channels, or the way the .btd route exposes
its planes), not only the height mesh. The G5 pictures include one per plane
(`lodt_open_ao.png`, `lodt_open_water.png`, ...), same camera as the height view.

## The work
1. FIRST establish, from the code, exactly how a FO76 `.btd` is opened and shown
   in this fork today (File > Open? a menu action? what scene it builds -- a
   height mesh, an image, planes?). Write that down in the report with file:line.
   The `.lodt` must open the SAME way, through the same entry point, with the
   same kind of scene, so that opening a `.btd` and opening the `.lodt` converted
   from it look identical. If `.btd` opening does not exist as a GUI route and
   only the CLI import does, say so and stop for the director's word before
   inventing a new viewer.
2. Implement `.lodt` opening through that route: file-type registration, the
   reader (src/lodtfile.cpp already reads the file -- reuse it, do not write a
   second reader), the scene construction (heights as terrain geometry at the
   file's stated spacing and origin, water and AO planes selectable the way the
   .btd route exposes its planes, or as the generated-channel view
   WW_LOD_CHANNEL already knows). A whole Commonwealth .lodt is ~226 MB; opening
   must not load every plane at full resolution into the scene at once -- do what
   the .btd route does for its size, and state the memory and time on the
   Commonwealth file.
3. Gates, pre-registered:
   - G1 byte identity: `tests/spells/lodt_write.sh` (or the skill's named gate)
     still hashes the Commonwealth `.lodt` identically -- the writer is untouched.
   - G2 parity: take one FO76 `.btd` (or a small FO4 worldspace if no .btd is on
     disk: Diamond City 3C-child or Nuka-World), convert it to `.lodt` with the
     existing CLI, open BOTH through the GUI route headlessly and photograph each
     with the render hook from the same pinned camera: pixel diff must be 0 over
     the terrain, or the difference is explained by a stated cause and shown.
   - G3 harness: a WW_*_TEST harness (`tests/spells/lodt_open.sh`) that opens
     Commonwealth.lodt (find one on disk: the FO4CS mod folder's Data\Terrain, or
     regenerate with the CLI into scratchpad/lodt_20260909/ -- state which) and
     asserts vertex count, bounds equal to the file header's, and a non-blank
     render; it prints PASS and its counts, exit code gates.
   - G4 the existing suite the change reaches (name which harnesses and why
     the rest were skipped).
   - G5 a picture for bungo: `scratchpad/lodt_20260909/lodt_open_commonwealth.png`
     (the whole worldspace) and `lodt_open_closeup.png` (Boston, zoomed), plus
     the parity pair from G2.
4. Build through `nifskope-ww-build-verify` ONLY. Check Fallout4.exe with tasklist
   before every build and before every exe launch; if the game is up, finish the
   code and the harness, write the report, and END BUILD PENDING -- never build
   beside the game, never poll for it. One NifSkope instance ever; second monitor;
   bungo's own open window is renamed aside, never killed, and the report says
   his window needs a restart.
5. WW_CHANGES.md: one entry, measured numbers only. docs/LODGEN_BTD_FORMAT.md: a
   short "opening in NifSkope" paragraph if the route needs documenting.

## Rules
- Absolute paths on every exe argument (a relative --out-dir resolves against
  the exe's folder). LF-only files; Python byte count before and after any edit
  to src/. Commit NOTHING. Do not touch the writer or the CLI verify path. Do not
  add a settings row or a dialog without the panel-style skill.
- Every NifSkope launch is one job at a time; CLI sweeps included.

## Report
scratchpad/lane_lodt_open_report.md, written incrementally:
1. How .btd opens today (file:line). 2. What was built (files, entry point).
3. Gates G1-G5 with numbers and the exe/source mtimes. 4. Memory and time on the
Commonwealth file. 5. Mistakes (or "none") -- ALSO append them to MISTAKES.md
at the repo root yourself. 6. Finished-work skill review: skills used, ones you
wish existed, ones written or amended in E:\Projects\Claude\.claude\skills, or
why you declined. Final message under 25 lines, picture paths first, BUILD
PENDING stated in the first line if that is how it ended.
