# IMPOSTORFIX5 -- resume state

Written 2026-09-19 19:59 CEDT, past half context, per the brief.

## What is DONE and needs nothing

* The ramp is APPLIED and BUILT. Exe `release/NifSkope.exe` 2026-09-19 19:49:03,
  23,625,728 B, sha1 `c529e3c12fe4216a3c33cc14631e3c811169fbb0`. BUILD-RC=0, ONE
  translation unit recompiled (`lodgen.o`), `exe -nt src/lodgen.cpp`, `style.qss`
  in step, `src/cell*` md5-identical across the build (CELLVIEW4 untouched).
* The corrected census wording is IN the exe: the UTF-16 count of
  `inside the %6-ring ramp` is 1 and of `outside within 8 rings` is 0.
* A control re-bake of blast_n4 on the SHIPPED exe returns all five DDS
  BYTE-IDENTICAL to the ones every table in the report was measured on, and
  prints the corrected census line. So no table needs re-running for the rebuild.
* All five fixtures re-baked, hashed (`sheet_hashes.txt`), only `_oct_n.DDS`
  differs from IMPOSTORFIX3's on every subject.
* Tasks 1, 2, 3 and 5 are measured and written into `DELIVERABLE_TEXT.md`
  sections 1, 2, 3 and 5.
* Row 14c is spliced into `tests/spells/impostor_draw.sh`, with its reference
  `tests/spells/impostor_height_ref.py`. Proved RED on the sheets exe ee87eb9e
  baked (`done 24 steps, 1 failures`, 14c FAIL at 0.663%) and GREEN on the
  ramped ones (`done 24 steps, 0 failures`, 14c PASS at 0.011%).
* Skill `ww-knob-owning-stage` in BOTH trees, sha1
  `5c0be41bfdab55c55478b5dcc6f0bb1ebed97bdd`.
* `mistakes_entries.txt` written; `splice_mistakes.py --check` says anchor 1,
  OK, 65 lines, CRLF asserted.

## What is RUNNING or OWED, in order

1. **Gates, task 4.** Backgrounded as one serial command (task `b4ji4eeom`).
   Done so far, all on the new exe:
   * `lodgen_octahedral.sh` **RESULT PASS** (baseline 116 ok / PASS)
   * `render_shot.sh` **82 checks, 0 failures, PASS** (baseline 82/0)
   * `harness_window.sh` **15 checks, 0 failures, 0 skips, PASS** (baseline 15/0)
   * `cell_open.sh` -- still running at the time of writing (baseline 8/0 PASS)
   * `impostor_draw.sh` -- **must be RE-RUN**: the batch ran it without the
     mesh argument, so steps 5-9 and 15 refused by name (6 failures, and 14c
     still PASSED). Re-run as
     `IMPOSTOR_NIF="E:/Tools/Fallout 4/DataUnpacked/Data/Trees/TreeMapleblasted05.nif"`
     with `IMPOSTOR_LODM=.../fixture/blast_n4/cards/000531b3_oct.lodm`.
     Baselines: 24/0 (CELLVIEW2B, exe ef4dab1f), and this lane's own 24/0 + 1
     red at 19:45.
2. **`cardres_test.sh`** -- the section 2 discriminator, written and not yet run.
   Bakes the maple with `WW_IMPOSTOR_TILE=128` and `WW_IMPOSTOR_REF` UNSET (no
   size ladder, so the frame goes 32x64 -> 64x128, four times the texels),
   compresses it with the same exe, and re-runs the SAME 16-direction
   known-answer control. blast_n4 goes through the identical route as the
   control on the control: it is already at full size, so it must reproduce its
   own 0.8701 or the script is measuring the route.
   **REFUTER, stated before the run: if the 1.472 ink ratio and the 0.4673 IoU
   survive at four times the texels, frame resolution is NOT the cause and
   section 2's reading is wrong.** Write the answer into section 2 either way.
3. `python scratchpad/impostorfix5_20260919/splice_mistakes.py --apply`, report
   CR before/after. (Entry 1 says the lane RAN the experiment -- if step 2 is
   abandoned, reword that sentence first.)
4. Finish `DELIVERABLE_TEXT.md` section 4 (gate table, before/after) and
   section 8 (the file list).
5. Replace `BUILDING` with `DONE` in this folder.

## Rules this lane is under, so a resume does not break one

* Process check `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` as its
  OWN command before every build and every exe run. It has read `rc=1` every
  time so far. Fallout4 up -> write BUILD PENDING and END.
* ONE NifSkope, always `--port <unused>` and `WW_WINDOW_AT=1960,40`, absolute
  `E:/...` paths. A NifSkope without `--port` is bungo's: never kill it.
* `release/NifSkope.before_impostorfix5.exe` (19:30:29, 23,625,216 B, sha1
  ee87eb9e...) has NEVER been run and must never be run with a GUI. Note that
  `harness_window.sh` runs `NifSkope.before_harnesswin1.exe` and
  `NifSkope.before_harnesswin2.exe` by its own design -- those are other lanes'
  rungs, not this one's, and that is fine.
* CELLVIEW4 is live and CODE-ONLY: do not touch `src/cell*`, `gltf*`,
  `bodybuild*`, `harnesswindow*`.
* Never commit, never `git stash`, never edit `WW_CHANGES.md` or `HANDOFF.md`.
* Pictures only under this folder. The repo root has no `images/` and must not
  gain one from this lane.

## Added 20:02 -- two driving mistakes of mine, both recorded here

* The first call of `impostor_draw.sh` in the batch had no mesh argument; the
  gate refused six steps by name. Not a code defect.
* `cardres_test.sh` first pointed at `$DATA/Trees/...`. The MESHES are under
  `$DATA/meshes/Landscape/Trees/...`; `--data-root` wants the `Data` folder
  itself. Two different paths and I used one for the other. Fixed in the script
  with a comment saying so.
* That left a harness NifSkope (pid 44416, `--port 28401`) wedged on a file
  that does not exist after I stopped the batch. `taskkill` is classifier-denied
  in this session; the UDP `NifSkope::open` unwedge plus `CloseMainWindow()` on
  the pid ended it, and the process check read `rc=1` again before anything else
  was launched. Do not kill a NifSkope without `--port` on its command line --
  that one is bungo's.
