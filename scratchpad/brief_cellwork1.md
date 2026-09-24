# Lane CELLWORK1 -- cell viewing becomes its own WORKSPACE (BUILD LANE, owns build + exe slots)

## bungo's ruling (2026-09-19 20:40, verbatim)
"Cell viewing will be a new workspace btw"

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` for every timestamp. Never commit, never `git stash`,
  never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/cellwork1_20260919/` (`BUILDING` first, `DONE` last;
  `report.md` incremental, section 0 inside ten tool calls; fallback name `DELIVERABLE_TEXT.md`; `PENDING.md` past
  half context). Pictures only under that folder, NEVER repo-root `images/`.
- You OWN the build slot and the exe slot; no other build lane is live. `tasklist | grep -i -E "Fallout4|NifSkope";
  echo rc=$?` as ITS OWN command before every build and exe run. Fallout4 up = write `BUILD PENDING` and END; never
  wait-loop; NEVER end your turn waiting on your own batch. ONE NifSkope, `--port <unused>` + `WW_WINDOW_AT=1960,40`,
  absolute paths. A NifSkope without `--port` is bungo's: never kill it; rename the exe aside at link time. Build:
  MSYS2 UCRT64, skill `nifskope-ww-build-verify`. Rung ONCE `release/NifSkope.before_cellwork1.exe`; never touch
  another rung; NEVER run any `before_*` rung with a GUI.
- Exe at launch: 23,684,608 B, 2026-09-19 20:58:28, sha1 595f4b9fdef7091fe1b8ec0a9760c92be2e07943.
- Read first: `CONSTITUTION.md` (UI rules: palette = the PBR Material Editor's `skinVars[]` only; flat Name|Value
  trees; uncertain design -> follow Blender's equivalent and state divergences; no descriptions/blurbs in menus);
  HANDOFF top block; MISTAKES.md top 10; `scratchpad/cellview4b_20260919/DELIVERABLE_TEXT.md`,
  `scratchpad/cellview3_20260919/report.md`, `scratchpad/cellview2b_20260919/DELIVERABLE_TEXT.md`; skills
  `nifskope-ww-build-verify`, `ww-test-harness-add`, `nifskope-ww-render-shot`, `ww-legend-matches-picture`,
  and any skill naming the animation workspace / workspaces.

## The work
1. STATE OF PLAY FIRST (report s1, before any edit): how the existing workspace mechanism works -- the animation
   workspace and any other (how a workspace is declared, switched, what it owns: docks, toolbars, menu rows,
   shortcuts, saved layout keys, how the harness forces one; file:line). And everything the cell view owns today and
   where it lives in the NIF window (open path for `.wwcell`, pick dock, identity overlay + legend, ground/overlay
   menu rows, the budget line, `WW_CELL_*` harness variables).
2. THE WORKSPACE. A "Cell" workspace beside the existing ones, built with the SAME mechanism (no second mechanism).
   Opening a `.wwcell` (or File > Open Cell) switches to it; opening a NIF switches back to the workspace that was
   active before. In it: the viewport; the reference inspector (the pick panel's flat Name|Value rows); a reference
   list (flat, filterable by record type / editor id, click selects + frames the reference in the viewport, viewport
   pick selects the row -- Blender's outliner is the reference); the view-mode rows (textured / identity groups +
   its legend) and the cell's one-line census (references, triangles, vertex budget). NIF-only docks (block list,
   block details, header) are hidden in this workspace and come back, unchanged, when leaving it -- prove the NIF
   workspace's saved layout is byte-identical after a round trip (name the QSettings keys; use an isolated scope via
   `WW_SETTINGS_SCOPE`, never bungo's).
3. No new master switch is needed (a workspace is a view, not a feature that changes output); no INI key without a
   menu row; no descriptions in menus. Anything you are unsure of: do Blender's equivalent and write the divergence
   down. Layout questions that are genuinely bungo's go in a short RULINGS OWED list with your recommended default
   NOT shipped as a behaviour he cannot undo.
4. Harness + gate: `tests/spells/cell_workspace.sh` -- workspace switches on cell open; NIF docks hidden then
   restored; list row count == the dump's reference count; list click -> selection == viewport pick of the same
   reference; round-trip layout bytes equal; every row shown failing on the pre-change behaviour (reasoning or a
   source revert in your own build). Before/after: `cell_pick.sh`, `cell_open.sh`, `harness_window.sh`,
   `native_open.sh`, `render_shot.sh`, plus whichever gate covers the animation workspace (name it) -- it must not
   move.
5. Pictures for bungo (full window, second monitor, size proved from `release/ww_harness_window.log`): the Cell
   workspace with Sanctuary open and a reference selected; the same with identity view + legend; the NIF workspace
   after coming back. `QWidget::grab` cannot capture the GL view -- compose with `grabFramebuffer` or
   `QScreen::grabWindow`, and say which. LOOK at them; plain words on what is wrong or ugly.

## Rules
Authored LOD models only; `--road-detail 1`; masters ship OFF; no "fixed/final/true" -- mechanism + refuter; plain
words; do not touch `src/impostor*`, gltf*, bodybuild*, `src/lodgen.cpp`.

## Report
Exe mtime/size/sha1; s1 state of play; what moved where; RULINGS OWED; gate counts before -> after; WW_CHANGES +
HANDOFF text; MISTAKES appended to root MISTAKES.md (top CRLF, byte splice, CR before/after); skill
(`nifskope-ww-add-workspace` or an update to the existing one) to BOTH trees, equal sha1. Final message under 250
words.
