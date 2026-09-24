# Lane UI5 -- BUILD PENDING resume

**Written 2026-09-11 while the outcome was still open, so that a death or a
timeout costs nothing on disk (CONSTITUTION 1b). If
`scratchpad/ui5_20260910/DONE` exists, THIS FILE IS SPENT -- read the DONE line
and `HANDOFF_BLOCK.md` instead.**

## Why it is pending

The code is applied, the objects are compiled and the link is the only step
left. `release/NifSkope.exe` is held by **bungo's own window** -- pid 8428,
launched 2026-09-11 05:32:30, command line
`"E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe"` with **no
`--port`**, so it is an interactive window and it is his. It is never killed and
no harness may run beside it. The lane's build rule requires
`tasklist | grep -i -E "Fallout4|NifSkope"` to print `rc=1` immediately before
the link; it prints `rc=0`.

Fallout4.exe is DOWN (checked; the game is not the blocker).

## What is already on disk

* **Code applied**, through the refusing script
  `scratchpad/ui5_20260910/hookup.py` -- 9 edits over 4 files, 9 of 9 anchors
  matched once, CR 0 -> 0 on every file. `--check` output in `hookup_check.txt`,
  `--apply` output in `hookup_apply.txt`. Re-running `--check` now REFUSES every
  edit as already present, which is "applied", not "failed".
  - `src/wwskin.h` 14,041 B (declarations + the measurement)
  - `src/nifskope_ui.cpp` 1,507,501 B (`wwBarRowMenuItemQss`, the calibration in
    `wwAlignBarRow`, `applyRow` takes the rule)
  - `src/wateruitest.cpp` 49,925 B (`menuInk()` + group M, 14 checks)
  - `tests/spells/water_ui.sh` 12,744 B (the 14 gate names, floor 48 -> 62)
* **Syntax pass clean** on both changed translation units with the real
  `Makefile.Release` flags: `src/wateruitest.cpp` RC=0, `src/nifskope_ui.cpp`
  RC=0 (`sx_UI5.sh` at the repo root -- the resuming lane deletes it).
  `bash -n tests/spells/water_ui.sh` OK.
* **Objects compiled, COMPILE-RC=0, zero `error:` lines**
  (`scratchpad/ui5_20260910/compile_objects.sh`, log `compile.log`).
  `GeneratedFiles/.obj/wateruitest.o` 05:38:50, `nifskope_ui.o` 05:40:32;
  `find src res tests -newer GeneratedFiles/.obj/nifskope_ui.o` prints nothing.
  **Only the link is owed.**
* `scratchpad/ui5_20260910/BUILDING` is UP. The resuming lane removes it and
  writes `DONE` when the gates have run, whatever colour they are.

## The resume, in order

1. `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` must print **rc=1**.
   If a NifSkope is up, read its command line: `--port` = a stray harness, kill
   it; no `--port` = bungo's, wait, never touch it.
2. `bash scratchpad/ui5_20260910/build.sh`. **No `qmake`**: `NifSkope.pro` did
   not change (group M went into the existing `src/wateruitest.cpp`, not a new
   translation unit). Expect `BUILD-RC=0` in well under a minute, because the
   objects are already built -- if it recompiles for minutes, something touched
   a header and the object read-back below is the thing to check.
3. `cmp res/style.qss release/style.qss` must be silent (the sheet is copied at
   link time; this lane did NOT edit `res/style.qss`, but the copy is still the
   gate).
4. Exe-newer sweep over the WHOLE working set, not one file:
   `git status --porcelain -- src res tools tests` -> every named path must be
   older than `release/NifSkope.exe`.
5. `bash scratchpad/ui5_20260910/gates.sh` -- sequential, one instance, behind
   its own `.gatelock`. Logs land in `scratchpad/ui5_20260910/logs/`.
6. `python scratchpad/ui5_20260910/make_pictures.py` (BEFORE =
   `scratchpad/ui4_20260910/images/strip_after.png`, AFTER = the chain's
   `toprow_after.png`).
7. Fill the BUILD lines in `HANDOFF_BLOCK.md` and `WW_CHANGES_ENTRY.md`, write
   report sections 4-8, `rm scratchpad/ui5_20260910/BUILDING`, write
   `scratchpad/ui5_20260910/DONE`.
8. Delete `sx_UI5.sh` at the repo root.

## THESE NUMBERS ARE PREDICTIONS, NOT RESULTS

Group M has never been executed. Its own defects are found by running it
(`ww-test-harness-add` section 9: a harness that has never run is a draft).

| gate | predicted | baseline |
|---|---|---|
| `water_ui.sh` | **73 checks, 0 failures, PASS** (floor 62) | 59 / 0 on the 21:02:12 exe, floor 48 |
| M2 worst ink offset | **+0.5 px** against a row centre of 17.0 | -6.0 / -5.5 (shipped) |
| M1 menu bar height | **35**, y 0 | 35 |
| M3 the menu bar's own hint | **35** (`<= 35` passes) | 20 |
| M4 with the row's sheet off | worst about **-4.5** (must be `< -2`) | -4.5 (probe case 1 / G) |
| M5 floor with padding 2/2 | worst about **-6.5**, RED by name | -6.5 (probe case 0) |
| `ui_align.sh` | 11 / 0 | 11 / 0 |
| `top_bar.sh` | 43 / 5 (the same five `Panels lists the ... dock`) | 43 / 5 |
| `files_tab.sh` | 28 / 2 (Qt's own `QLineEditIconButton`) | 28 / 2 |
| `animws.sh` | 57 / 0 | 57 / 0 |

The refuter to watch: **M3**. If the menu bar's own size hint reads above 35,
the row would grow the next time `wwAlignBarRow` runs, and the change does not
ship however good the ink offset is (CONSTITUTION 7 -- report the number,
refuse). The standalone probe read 35 for exactly this sheet, so a different
number in the application means the application's menu bar carries something the
probe's does not (a corner widget, a frame), and that is the thing to find.

## What no gate here covers

The hover band. Giving a title the row's padding makes its painted box the whole
row, so `QMenuBar::item:selected` (`res/style.qss:49`) now paints a 35-px band
instead of a 20-px one. That is what "centred in the row" means geometrically
and it is Blender's behaviour, but it was not separately asked for, no count
sees it, and it belongs in the picture and in what bungo is told.
