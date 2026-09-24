# Lane UINOTES2 -- the transport-row toggles as icons, Ctrl+A, the owed pictures and the owed mouse drag (bungo 2026-09-12 06:0x-06:1x)

## Header
- Tree: `E:/Projects/NifskopeWWE_ui` -- a robocopy of the main tree taken 2026-09-12 06:15 (everything but
  scratchpad/, of which the *.md and *.py files and uinotes1_20260912/ came along). YOU WORK ONLY IN THE COPY. Never
  open, edit, build or run anything under `E:/Projects/NifskopeWildWastelandEdition`: lane ROADS4 is building there
  at the same time. The copy holds ROADS4's half-done edits to `src/lodgen.h` / `src/nifcli.cpp` as of 06:15 -- do
  not touch them; if the copy fails to compile in those files, report it and stop that build. The director merges by
  your CHANGED_FILES.txt. NOTHING committed, never `git stash`, in either tree. UI files only:
  `src/animworkspace.cpp/.h`, `src/animdopesheet.*`, `src/animworkspacetest.cpp`, `src/nifskope_ui.cpp`
  (`GLView::sequenceChanged` ~24752 if the Ctrl+A fix needs it), `res/style.qss`, `res/` icon SVGs, `src/wwskin.h`.
  Stay OUT of every lodgen file (a lodgen lane may be live in the same tree: `src/lodgen.*`, `src/nifcli.cpp`,
  `src/lodgenmanager.cpp`, `src/btdterrain.*`, `src/lodtfile.*`, `docs/LODGEN_*`, `tests/spells/lodgen_*`).
- Exe at launch (in the copy): `release/NifSkope.exe` 2026-09-12 05:48:33, 21,817,856 B (UINOTES1b's). qmake FIRST in the copy (the tree moved), then make. Rung ONCE: `release/NifSkope.before_uinotes2.exe`. Markers
  `scratchpad/uinotes2_20260912/BUILDING` / `DONE`; report `scratchpad/lane_uinotes2_report.md`, incremental (all in the copy). Own `--port` (not 43113-43115), and before EVERY GUI launch check `tasklist` for a NifSkope with `--port` from the main tree (ROADS4's): if one is up, wait; never two GUI instances; the NifSkope WITHOUT `--port` is bungo's.
- Game down for every build and exe launch; count Fallout4.exe SEPARATELY from NifSkope.exe; if the game is up when a
  build is due, PENDING.md. One NifSkope at a time, `--port`, second monitor; never touch a NifSkope without `--port`.
- Read first: CONSTITUTION.md, HANDOFF.md top block (UINOTES1/1b block), `scratchpad/lane_uinotes1_report.md`
  (`## 7` for the toggles, `## Build (UINOTES1b)` for the Ctrl+A chain and the three candidate fixes, `## 11` for
  the pictures), `scratchpad/brief_uinotes_20260912.md` (his rulings verbatim), `src/animworkspace.cpp` ~660-700.
- Palette `wwskin.h` tokens only; icons = the same SVG set, weight and size as ruling 4's transport icons; Blender's
  equivalent where unsure; no descriptions/blurbs; tooltips name the action and its shortcut.

## His words, verbatim
- 06:0x: "okay, new icons look good, what is the "pose" button and the round dot that's not centered button?"
- 06:1x, offered "both toggles drawn as icons in the same set, with a lit state when on": "Both icons"

## The work
1. The two transport-row toggles (`poseCheck` "pose", `autoKeyCheck` the dot glyph, animworkspace.cpp ~671-674)
   become ICONS in ruling 4's SVG set: Auto-key = Blender's record dot; Pose-with-gizmo = a bone-with-gizmo glyph
   (Blender has no exact equivalent: say what you drew and why). Same weight and size as the other transport icons;
   a lit (accent) state when on, plain when off; tooltips "Pose with the gizmo" / "Auto-key gizmo transforms" with
   their shortcuts if any. Gate: `animws.sh` asserts both buttons have an icon and no text, the same icon size as
   the play button, the on/off pixel colours distinct, and the tooltips by text.
2. Ctrl+A (UINOTES1b's one red: `selectAll` keeps 1 of 4 because `GLView::sequenceChanged` -> `setSequenceByName`
   -> `list->setCurrentItem`, ClearAndSelect). Fix so Ctrl+A keeps every row while the viewport still follows the
   CURRENT row: the report names three candidates; pick by the smallest blast radius (a guard on the driven
   re-entry, or `setCurrentItem(it, QItemSelectionModel::NoUpdate)` shaped) and state the refuter. Gate: animws (o)
   4 of 4; the existing single-click-drives-viewport checks unchanged.
3. The owed pictures for rulings 3, 6, 6a, 7, 7a and 9 (`scratchpad/uinotes2_20260912/images/`), each at 1:1, taken
   through the harness (`SHOT=`), plus one 1:1 overview of the finished dock and the transport row at 2:1 showing
   the two new icons on and off.
4. The owed mouse drag: a real drag of a list row is bungo's to do; you prove the drop path once more the way
   UINOTES1b did (commitListOrder) and say in one line what he should try and what he should see.
5. Build per `nifskope-ww-build-verify`; chain animws, hkxanim_ui, ui_align, water_ui, files_tab, top_bar,
   skeleton_overlay with the baselines animws 210/1, hkxanim_ui 48/1, ui_align 15/0, water_ui 84/0, files_tab 29/1,
   top_bar 43/5, skeleton_overlay 5/1 (run them on the rung first if any baseline is doubted); name skipped ones.
6. Documents in `scratchpad/uinotes2_20260912/`: `WW_CHANGES_ENTRY.md` (`## 2026-09-12 — <title>`, em dash),
   `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md`, `CHANGED_FILES.txt` (A/M, CR/LF byte counts per file:
   `src/nifskope.cpp` is mixed CRLF, `src/nifskope_ui.cpp` LF-only); skills you add or amend listed.

## Rules
Plain language; every number beside its floor; no "final/true" claims; incremental writes; timestamps read from
`date +%H:%M` in the same turn.
