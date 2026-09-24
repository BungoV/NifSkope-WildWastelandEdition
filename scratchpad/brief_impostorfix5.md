# Lane IMPOSTORFIX5 -- make IMPOSTORFIX4's simulations real (BUILD LANE, owns build + exe slots)

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` for every timestamp. Never commit, never `git stash`,
  never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/impostorfix5_20260919/` (`BUILDING` first, `DONE` last;
  `report.md` incremental, section 0 inside ten tool calls; if report.md cannot be written use `DELIVERABLE_TEXT.md`;
  `PENDING.md` past half context). Pictures only under that folder, NEVER repo-root `images/`.
- You OWN the build slot and the exe slot. `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` as ITS OWN command
  before every build and exe run. Fallout4 up = write `BUILD PENDING` and END; never wait-loop and NEVER end your turn
  "waiting" on your own batch -- wait for it in the foreground (bounded), then continue. ONE NifSkope,
  `--port <unused>` + `WW_WINDOW_AT=1960,40`, absolute `E:/...` paths. A NifSkope without `--port` is bungo's: never
  kill it; rename the exe aside at link time. Build: MSYS2 UCRT64, skill `nifskope-ww-build-verify`. Rung ONCE
  `release/NifSkope.before_impostorfix5.exe`; never touch another rung; NEVER run any `before_*` rung with a GUI.
- Exe at launch: 23,625,216 B, 2026-09-19 18:58:55, sha1 ee87eb9ee6bfe45a199cd5c85820c333c6572256.
- ANOTHER LANE (CELLVIEW4) is live and CODE-ONLY: it writes new files + an anchored script, never builds. Do not touch
  `src/cell*`, gltf*, bodybuild*, harnesswindow*.
- Read first: `CONSTITUTION.md`; HANDOFF top block; `scratchpad/impostorfix4_20260919/DELIVERABLE_TEXT.md` (all of
  s5, s6, s3's "the shot"); `scratchpad/impostorfix3_20260919/report.md`; skills `ww-reference-card-diagnose`,
  `nifskope-ww-build-verify`, `ww-anchored-hookup`, `ww-gate-owns-its-fixtures`, `ww-spec-gate-audit`.

## The work
1. `python scratchpad/impostorfix4_20260919/hookup_ramp.py --check`, then `--apply` (the trunk-chip repair: the
   outside-coverage height fill ramps instead of ending on a cliff). Build. Re-bake all five fixture sets (blast N=4,
   blast N=8, leafy maple, dead tree, rock) into YOUR folder, hashed. Measure the same 24 views per subject on the real
   exe; table: IMPOSTORFIX3 real | IMPOSTORFIX4 simulated | your real. Count the texels >12 levels wrong before/after.
   Crop the trunk chips before/after at the same pixel. If the real result does not follow the simulation, say so.
2. Shoot what IMPOSTORFIX4 could not get offline: the leafy maple's MESH grabs at its own 16 bake directions, chrome
   off, size proved from `release/ww_harness_window.log`. Then run the single-frame known-answer control on the maple
   and give the verdict IMPOSTORFIX4 s3 left open (bake of alpha-tested leaves: two-sided, bake-time alpha test, depth
   written by discarded texels). If the cause is a plain defect in the bake, repair it and re-measure; if it needs a
   format or threshold change, PREPARE it (anchored script, `--check` only) and report.
3. Do NOT apply: the alpha cut-off change, the `_n` channel swap, any combine-rule change, any mip cap. Those are
   bungo's rulings or refuted. But re-measure the alpha 0.20 row on the REAL exe for all five subjects (the uniform
   can be set from the harness; if it cannot, say so) so his ruling rests on a real number, not a simulation.
4. Gates: `impostor_draw.sh` (add the row from IMPOSTORFIX4 s6 that fails on exe ee87eb9e for the ramp; raise floors
   on measurement only, never lower), `lodgen_octahedral.sh`; neighbours `render_shot.sh`, `harness_window.sh`,
   `cell_open.sh`. Before/after counts.
5. Pictures for bungo: `00_before_after.png` three rows (IMPOSTORFIX3 card / your card / mesh, 12 azimuths) for bare
   maple N=8 and the leafy maple; the trunk crop pair. LOOK at them; plain words on what is still wrong.

## Rules
Authored LOD models only, never decimate; masters ship OFF; no "fixed/final/true" -- mechanism + refuter; all 24
views, mean + worst; plain words.

## Report
Exe mtime/size/sha1; the tables; gate counts; WW_CHANGES + HANDOFF text for the director; MISTAKES appended to root
MISTAKES.md (top CRLF, byte splice, CR before/after); skill text to BOTH trees with equal sha1.
Final message under 250 words.
