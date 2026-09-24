# Lane IMPOSTORFIX3 -- apply the PROVEN impostor repairs, nothing that waits on a ruling

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` for every timestamp. Never commit, never `git stash`,
  never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/impostorfix3_20260919/` (`BUILDING` first, `DONE` last;
  `report.md` incremental, section 0 inside ten tool calls; `PENDING.md` past half context). Pictures only under
  that folder, NEVER repo-root `images/`.
- You OWN the build slot and the exe slot. `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` as ITS OWN command
  before every build and exe run. Fallout4 up = park at `BUILD PENDING`, never wait-loop. ONE NifSkope,
  `--port <unused>` + `WW_WINDOW_AT=1960,40`, absolute `E:/...` paths. A NifSkope without `--port` is bungo's: never
  kill it; rename the exe aside at link time. Build: MSYS2 UCRT64, skill `nifskope-ww-build-verify`. Rung ONCE
  `release/NifSkope.before_impostorfix3.exe`; never touch another rung.
- Exe at launch: 23,504,384 B, 2026-09-19 15:14:27, sha1 af457755... (last verified).
- Read first: `CONSTITUTION.md`; HANDOFF top block; `scratchpad/impostorfix2_20260919/report.md` (s7 ranked list) and
  `PENDING.md`; `scratchpad/impostorfix1_20260919/PENDING.md` + `DELIVER.md`; skills `ww-reference-card-diagnose`,
  `nifskope-ww-build-verify`, `ww-spec-gate-audit`. IMPOSTORFIX2's numbers are SIMULATIONS: your job is to make them
  real or report that they did not hold.

## The work (repairs, no toggles)
1. Height fill outside coverage: dilate 8 texels from fully covered texels instead of jumping to the card plane
   (`src/lodgen.cpp` ~2659, `lodgenRepairOctHeight`). Keep the bake-time self-check. Re-bake all five fixture sets
   (blast N=4, blast N=8, maple, dead tree, rock). Measure the same 24 orbit views per subject; table simulated gain vs
   real gain. The rock must come back over its earlier 0.7944 or you say why not.
2. The rock's placement: IMPOSTORFIX2 says the card sits ~10 px off the rock (shape improved, position did not).
   Name the cause with numbers (extents / centre / frameOffset / pivot) and repair it if it is a defect.
3. `tests/spells/impostor_bc_decode.py:26`: the BC3 alpha ramp is one step short (always low). Repair it against a
   known-answer block (hand-built 16 bytes with every index), show gate row 14 of `impostor_draw.sh` before/after, and
   re-measure anything today's reports derived from that decoder that changes by more than rounding.
4. Do NOT apply: the alpha-threshold change (0.0627 -> 0.20) and the `_n` height<->sway swap -- both are bungo's owed
   rulings. But PREPARE them so a yes costs one short lane: for each, the exact diff as a refusing anchored script
   (skill `ww-anchored-hookup`, `--check` only), the spec wording change, and one picture pair (today vs with the
   change, same 12 azimuths, bare maple N=4 and rock) made with the numpy reference card so bungo can SEE what he
   rules on. Do not apply the height-consistency rejection (it costs the rock 0.065).
5. `impostor_draw.sh`: raise the floor on measurement only (never lower), add a row that fails on exe af457755 for
   repair 1. Neighbours: `lodgen_octahedral.sh`, `render_shot.sh`, `harness_window.sh` (RUN_NATIVE_OPEN=0).
   Also answer in one line: CELLVIEW1's harness grab came out 1822 px wide AFTER HARNESSWIN1 landed -- does the
   impostor harness get the size it asks for now? Quote `release/ww_harness_window.log`.
6. Regenerate for bungo: `00_before_after.png` (three rows: exe 161568a5's card / your card / mesh, 12 azimuths) for
   bare maple N=4 AND N=8, the orbit GIFs, the distance strip. LOOK at them; describe what is still wrong in plain
   words. No "follows the shape" unless the picture shows it.

## Rules
Authored LOD models only, never decimate; masters ship OFF; no "fixed/final/true" -- mechanism + refuter; never pick
the flattering view; plain words. Do not touch gltf*, bodybuild*, cellview*, cellpick*, harnesswindow* files.

## Report
Exe mtime/size/sha1; per-subject before -> after (24 views); gate counts; WW_CHANGES + HANDOFF text for the director;
the two prepared rulings with picture paths; your MISTAKES appended to root MISTAKES.md (top of file CRLF, byte
splice, print CR before/after); skill update text if `ww-reference-card-diagnose` needs one (both trees).
Final message under 250 words.
