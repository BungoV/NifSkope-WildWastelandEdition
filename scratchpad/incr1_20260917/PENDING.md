# INCR1 -- PENDING

Written 2026-09-17 (see the `date` line in `lane_incr1_report.md`). If this lane
is resumed in a fresh context, this is the state.

## Done and proved

1. Step 1 (survey), step 2 (the `.lodj` per-chunk cache), step 3
   (`lodgen_layout.sh` leg (c)), step 4 (the new gate
   `tests/spells/lodgen_incremental.sh`, six arms with floors), step 5 (the
   panel: NO ROW, reason in the report section 6), step 6 (docs + both mistake
   ledgers). All written up in `scratchpad/incr1_20260917/lane_incr1_report.md`.
2. Built: `release/NifSkope.exe` 2026-09-17 06:34, 22,534,144 B. Rung
   `release/NifSkope.before_incr1.exe` was taken before the first build.
3. The build needs git on PATH inside the MSYS2 shell, which it is NOT by
   default in this tree:
   `export PATH=/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH` then
   `mingw32-make -f Makefile.Release -j8`. Without it the link step dies with
   `git: command not found` AFTER every object has compiled.

## What is left

* **Step 7, in flight.** `scratchpad/incr1_20260917/run_gates.sh` runs the seven
  gates one at a time into `scratchpad/incr1_20260917/gates_after/`, with
  `summary.txt` carrying `<gate> rc=<rc> <seconds>s` a line. Re-run it if it was
  interrupted; it is idempotent. Then the before/after table (BEFORE figures:
  `lodgen_bakerec.sh` 21 checks / 3 failures -- (e)x2 plugin-not-named, (h)x1
  peak working set; `lodgen_layout.sh` 22 checks / 5 failures, cited from
  ARCHLOCK1's recorded table because my own BEFORE run was contaminated by a
  mid-run edit).
* **Step 8, the pictures.** `scratchpad/incr1_20260917/pics.sh` +
  `pics_compose.py` are written and NOT yet run. They must run AFTER the gates:
  one GUI at a time, `--port 42977`, `WW_WINDOW_AT=1960,40`, second monitor.
  Output goes to `scratchpad/incr1_20260917/images/`.
* **`DONE` marker**, first word `incr`, with the one-line verdict.

## Rules that still bind

Game check as its own command before every build and every exe launch. One GUI
harness at a time. Never commit, never `git stash`, never edit `WW_CHANGES.md`
or `HANDOFF.md` (the changelog text for the director is section 8 of the
report). Never edit a harness that is running. Every path absolute `E:/...`.
