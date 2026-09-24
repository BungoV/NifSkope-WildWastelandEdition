# Lane HARNESSWIN1 -- a harness forces its own window size (CODE-ONLY, no build, no exe run)

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` for every timestamp. Never commit, never `git stash`,
  never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/harnesswin1_20260919/` (`BUILDING` first, `DONE` last,
  first word `harnesswin1`; `report.md` incremental, section 0 inside ten tool calls; `PENDING.md` past half context).
- ANOTHER LANE (IMPOSTORSHOW) IS LIVE, OWNS THE BUILD SLOT, and is editing `src/nifskope_ui.cpp`, `src/glview.cpp`,
  `NifSkope.pro`, `src/impostor*`, `src/gl/impostordraw.*`. YOU DO NOT BUILD, DO NOT RUN `release/NifSkope.exe`, DO NOT
  run any `tests/spells/*.sh` that starts the exe, and DO NOT EDIT any existing `src/` file or `NifSkope.pro` in place.
  Your deliverable is: new files where new files serve, plus ONE refusing hook-up script per skill `ww-anchored-hookup`
  (exact-once anchors carrying the file's real line ending, `--check` writes nothing, CR/LF byte assert) that the
  director applies after IMPOSTORSHOW lands. Run `--check` yourself and quote its output; note that anchors inside
  `src/nifskope_ui.cpp` may move under you -- choose anchors far from line ~22000-23300 (the impostor bake/harness
  area) and re-run `--check` last thing.
- Read first: `CONSTITUTION.md`; HANDOFF top block; MISTAKES.md top 10; skills `ww-anchored-hookup`,
  `ww-test-harness-add`, `nifskope-ww-build-verify`; memory rule "a GUI harness forces the state it measures, never
  inherits QSettings".

## The defect
`tests/spells/native_open.sh` row (c) is RED: covered 0.8978 < 0.90. Cause found by lane HORIZONOUT: the window
geometry persisted in QSettings is MAXIMIZED; `restoreGeometry()` restores it on the harness run too; the viewport
comes out 1822x989 and `WW_RENDER_SIZE` is floored -- the harness measures bungo's last window, not the code. See
`src/nifskope_ui.cpp` ~1391-1660 (WW_WINDOW_AT placement, restoreGeometry, showMaximized), ~22071-22090
(WW_RENDER_SIZE), `src/nifskope.cpp` ~1580 and ~3415, and the note in `src/impostorpreviewtest.cpp:58`.

## The work
1. State of play (report s1): every place a harness run inherits window geometry / maximized state / dock layout from
   QSettings; every harness that reads `WW_RENDER_SIZE`; which `tests/spells/*.sh` set it and to what. file:line.
2. The repair (no toggle, no INI key -- it is a repair): when a harness run is detected (`WW_WINDOW_AT` set, or any
   `WW_*_TEST` armed -- say which predicate you chose and why), the window does NOT restore persisted geometry or the
   maximized state, is shown normal at `WW_WINDOW_AT` with an explicit size (`WW_WINDOW_SIZE=WxH`, default chosen so a
   1024x1024 viewport fits on the second monitor 1920x1080 at 1960,40 -- if it cannot fit, say so and say what the
   honest maximum is), and does NOT write its geometry back to QSettings on close (bungo's window layout must survive a
   harness run byte-for-byte: name the settings keys). The viewport size a harness actually got is logged in one line
   every harness log can quote, and a harness whose `WW_RENDER_SIZE` was floored says REFUSED with both numbers instead
   of measuring.
3. Gate text (not run): `tests/spells/harness_window.sh` -- (a) with a maximized geometry planted in an ISOLATED
   settings scope (never bungo's), the harness window comes up normal at the asked size; (b) the planted settings are
   byte-identical after the run; (c) red control: the old path floors; (d) `native_open.sh` (c) re-measured. Write the
   script; mark every row NOT RUN.
4. `PENDING.md` = the exact director commands: apply hook-up, build, gates, neighbours.

## Rules
No "fixed/final/true" -- mechanism + refuter. Plain words. Masters-OFF does not apply: this is a repair, no switch.

## Report
0 tree state at launch; 1 state of play; 2 the hook-up script + `--check` output; 3 gate script; 4 WW_CHANGES +
HANDOFF text for the director; 5 MISTAKES (append your own to root MISTAKES.md, CRLF at the top of that file, byte
splice); 6 skill text if a repeatable procedure appeared. END with `BUILD PENDING` + three plain sentences. Final
message under 200 words.
