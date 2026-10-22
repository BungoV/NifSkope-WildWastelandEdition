STATUS: DONE

# OPENCELL1 report

- Branch: `cloud-opencell1`, from `main` at 5c98715.
- Commits: `3803938` (dialog, catalog, menu, test), plus this report (which also adds a wait cursor while plugins are read).
- Files: new `src/ui/opencellcatalog.*`, `src/ui/opencelldialog.*`, `src/ui/opencellmenu.*`, `tests/opencell/*`, `tests/spells/opencell_check.sh`. Hook-up: NifSkope.pro (+6 lines), nifskope_ui.cpp (+5), nifskope.cpp (+2: an opened .wwcell goes into Recent Cells). Line endings kept.

## What it does

- File > Open Cell...:
  - Data folder (remembered), plugin list with tick boxes; drag to set load order.
  - Import plugins.txt, or an MO2 profile's plugins.txt. A modlist.txt beside it resolves plugins from `<MO2>/overwrite`, then `<MO2>/mods/<enabled mod>`, then Data.
  - Masters are added automatically from TES4 MAST.
  - Worldspace list (editor ID + FULL name) plus "Interior".
  - Filterable cell list (editor ID, name, x, y; interiors: editor ID, name, form), an x,y field, size 1/3/5.
- Plugins are read with libfo76utils ESMFile, the reader EsmWorld and the cell view use. Every group id is enumerated, so a later plugin's new cells are listed too.
- Names come from FULL. A localized plugin's FULL is a string-table index, so it is shown blank rather than as a number.
- Open writes `<World>_<x>_<y>_<n>.wwcell` or `interior_<cell>.wwcell`. It goes beside the newest recent cell, else in AppData/cells. The cell then opens via `NifSkope::openFile`, the same path as any .wwcell.
- File > Recent Cells: the last 10, kept in QSettings.

## Checked

- `tests/spells/opencell_check.sh`: `tests/opencell/opencelltest.cpp` builds synthetic plugins in code: Base.esm (TES4, a WRLD with a persistent cell and three exterior cells, two interiors), Mod.esp (master Base.esm, a WRLD override, a new cell, its own WRLD), and a localized Loc.esm.
  - Green: 19/19 checks pass. These include the masters, the Data order, plugins.txt, MO2 resolve, the world, cell and interior lists, the .wwcell bytes (exterior and interior), and Recent Cells capped at 10.
  - Red: `WW_OPENCELL_RED=writer` swaps x,y and fails the exterior byte check (exit 1). Script result: "OPENCELL CHECK PASS".
- Build: the full Linux build of the app was not finished. Qt 6.4 (apt) is too old for this tree, which needs Qt 6.6 or newer. A Qt 6.8.3 build made from source was still compiling the app when the session stopped. The dialog and catalog compile and link in the unit test. The 5-line hook in nifskope_ui.cpp has not been compiled. The local session should build it.

## Open questions

- MISTAKES.md does not exist in the repo.
- The cell list has no "name" for localized masters such as Fallout4.esm, because no strings-table reader exists. Should one be added?

## Follow-up (lane LEFTOVERS1, 2026-10-22)

- Strings tables: done. A localized plugin's FULL names come from `Strings\<plugin>_<lang>.STRINGS` (and DL/IL), loose
  or inside `<plugin> - *.ba2`; Language box in the dialog, default `en`. Off `WW_OPENCELL_STRINGS=0`, red
  `WW_OPENCELL_RED=strings`. Synthetic tables and a BA2 in the test; 30/30 checks.
- .esl: confirmed. `.esl` files rank with the masters, light-flagged `.esp` files with the plugins, and both are read.
  One of each is in the synthetic test. The reader maps an ESL's forms into a full load-order slot (not the game's FE
  space); references resolve inside the same loaded set. Not checked against a real .esl yet.
