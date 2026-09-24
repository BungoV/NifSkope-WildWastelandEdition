# PENDING -- lane CELLWORK1, written 2026-09-19 21:55

Everything here was RULED by bungo mid-lane and is NOT BUILT. It is written
down at full precision so the next lane starts from a design, not a memory.

## PENDING-1  The editor tab strip (bungo, 2026-09-19 ~22:0x)

FOUR rulings arrived inside fifteen minutes. Only the LAST is in force; the
earlier ones are kept because they show what stayed constant (three editors,
one window can host any of them, each can also be its own window) and what
changed (the CONTROL: drop-down -> tabs).

1. *"the cell editor would be opened through here, but you have to select it in
   the new button ... open it in the current window, or in a new window"*
   -- a drop-down before `Workspaces`. SUPERSEDED.
2. *"We should call it something different then, like Plugin / Cell Editor"*
   -- records folded into the cell editor. SUPERSEDED.
3. *"let's have it be a selector for three things, File Editor, Plugin Editor,
   Cell Editor, and then add an option to launch them each in separate windows.
   File editor is the current thing we have in nifsope, with the workspaces and
   such."* -- the THREE NAMES, still in force.
4. **FORM IN FORCE** -- of the empty right-hand part of the top toolbar row:
   *"we have a lot of free space here, so we could add these in here, as like
   tabs you can open for each, or you can drag into and out, to form new
   windows."*

Dead names: `NIF Editor`, `Plugin / Cell Editor`. Dead form: a drop-down button.

### The shape ruled

- An **EDITOR TAB STRIP** in the free space of the TOP TOOLBAR ROW, right of
  `... Workspaces | LOD 0 | Animation | Collision`, running to the window's
  right edge. Browser-style tabs. Palette `wwSkinColor` only, **label only** --
  no icons demanded, no descriptions.
- A tab = ONE OPEN EDITOR IN THIS WINDOW. Clicking a tab switches the window to
  that editor: its own docks, its own toolbars, its own `Workspaces` list.
- A `+` on the strip (and the strip's context menu) opens another editor as a
  new tab: `File Editor`, `Plugin Editor`, `Cell Editor`.
- Dragging a tab OUT tears it into its own top-level window; dragging a tab INTO
  another window's strip docks it there.
- The `File Editor` tab ALWAYS exists in the first window and is today's UI
  unchanged. Switching away from it and back must restore its layout
  **byte-identical**.

So there are now THREE axes: Window > Editor (tab) > Workspace (layout).
This lane builds the third and leaves the second written down.

### Order of work he ruled

1. Cell Editor workspace green  <- THIS LANE
2. the strip with click-switching, `+` / context menu, and **"Move to New
   Window"** in a tab's context menu -- the honest interim for tear-off
3. real drag-out / drag-in ONLY if it fits; otherwise PENDING/NEXT with what it
   needs: one process, several top-level windows each owning an editor, and a
   written answer to *what state moves with a tab* (document, camera, selection,
   undo stack, per-workspace layout bytes)

### The `Plugin Editor` tab, and the honest way to ship it

bungo left the call to the lane, with the condition that it state nothing
false. **Recommendation: OMIT it from `+` and list it in NEXT.** A tab that
opens an empty shell is a promise the window cannot keep -- there is no record
editor behind it, and a person who opens it has swapped their layout for
nothing. A greyed entry is a menu row whose entire content is "not yet", which
this house does not ship. When the records lane lands, it adds its own tab.
Overrule this by ruling, not by default.

### What already exists to build it on (measured, not assumed)

- New window: `NifSkope * NifSkope::createWindow( const QString & fname = QString(), bool background = false )`
  declared `src/nifskope.h:163`, defined `src/nifskope_ui.cpp:1543`, already
  used by `on_aWindow_triggered()` (`src/nifskope_ui.cpp:33882`) and by the
  recents right-click "Open in New Window" (`src/nifskope_ui.cpp:33075-33083`).
  "Move to New Window" is `createWindow()` plus setting that window's editor --
  no new window-spawning machinery is needed and none should be written.
- The top row is `ui->tFile`; the `Workspaces` button is added to it at
  `src/nifskope_ui.cpp:28408`. The strip is a widget added after the trailing
  group, with a stretch before it or an expanding size policy -- the free space
  bungo circled is that toolbar's unused width, not a new bar.
- The workspace table, `NifSkope::setWorkspace(int)` and the per-workspace
  dock/layout byte save-restore are what THIS lane lands. The editor axis sits
  above them: an editor owns a LIST of workspace indices and a layout snapshot.

### Naming

- UI strings, QSettings group names, gates and the report say `File Editor`,
  `Plugin Editor`, `Cell Editor`. Internal identifiers may stay `cell*`.
- The strip has no label of its own, so the "button label" ruling owed from the
  drop-down form is DISCHARGED by this change -- nothing is owed on it.

### Gate rows ruled for it (none of these exist yet)

1. the strip exists in the top row's free space, right of the trailing group
2. clicking a tab switches the window's editor, and the `Workspaces` list
   changes with it
3. `Move to New Window` produces a second top-level window hosting that editor,
   the first window otherwise unchanged (harness rule holds: ONE NifSkope
   process, second monitor -- the gate proves a second WINDOW, not a process)
4. the `File Editor` layout is **byte-identical** after switching away and back
   (`saveState(0x074)` compared as bytes -- the proof this lane already uses)
5. the File Editor's Workspaces list is unchanged: same count, names and order

### Divergences to state when it is built

- Browser tabs CLOSE; an editor tab that is the window's only tab cannot, or
  the window becomes nothing. Blender areas never close to empty either.
- Blender's workspace tabs sit in the topbar and switch LAYOUT; these switch
  EDITOR. Two rows of tabs would be the literal Blender answer and is rejected:
  `Workspaces` is already a button on that row.

### Explicitly NOT in scope, by ruling

- A load-order / plugin picker. With no plugin chosen the Cell Editor opens
  EMPTY with its own open-cell action. NEXT.
- Opening a `.wwcell` by File > Open, drag-drop or `WW_CELL_OPEN` keeps working
  exactly as it does and lands in the Cell Editor in the current window. That
  path must not be routed through the strip.

### AMENDMENT: one tab per editor type (bungo, same sitting)

Verbatim: *"Limit is one tab per file editor, plugin editor and cell editor"*.

- At most ONE File Editor, ONE Plugin Editor, ONE Cell Editor exist **in the
  process** -- three tabs maximum in total, whether they sit in one strip or
  are torn off into their own windows.
- `+` / the context menu offer only editors that are NOT open yet.
- Asking for an editor that is already open -- including opening a `.wwcell`
  while the Cell Editor lives in another window -- ACTIVATES its existing tab
  and raises its window. It never makes a second one.
- Consequence, and it is a simplification: a tab's IDENTITY IS ITS EDITOR TYPE.
  There is no per-instance state to key, no "Cell Editor 2", and the process
  holds one registry of three slots.
- Extra gate row: requesting an already-open editor yields NO second tab (and
  the existing one is raised).

RULINGS OWED (carried into the report): NifSkope's existing `File > New Window`
opens a second window that is, in the new vocabulary, a second File Editor --
which the one-tab-per-type limit forbids. Its behaviour is LEFT AS IT IS TODAY
in this lane, by instruction. The question owed is whether `New Window` becomes
"raise the one File Editor", becomes a second WINDOW showing the same File
Editor, or keeps making independent document windows and is simply outside the
tab registry.

### AMENDMENT: a close X per tab, with the unsaved prompt (bungo, same sitting)

Verbatim: *"Add an X to close each one of these, but add a warning if anything
is unsaved from each, to save it or not"*.

- Each editor tab carries a close X, browser style.
- Closing an editor with unsaved changes asks Save / Don't Save / Cancel.
  Cancel keeps the tab.
- **REUSE, DO NOT REWRITE.** The prompt already exists:
  `bool NifSkope::saveConfirm()` -- declared `src/nifskope.h:147`, called from
  `NifSkope::closeEvent` at `src/nifskope.cpp:7703` (and from the open/replace
  paths at 8712, 9862, 9898, 4424). The modified state it asks about is
  `isModified()` on the document wrapper (`src/nifskope.cpp:1426`, which also
  carries `unsavedInMemory` at 1404 for work that has never been on disk).
  A second prompt must not be written.
- **The headless rule is already satisfied by that path.** `closeEvent` discards
  without asking when `NifSkope::wwHeadlessRun()` is true (`src/nifskope.h:138`,
  the guard and its long rationale at `src/nifskope.cpp:7608-7650`). The tab X
  must route through the SAME guard, so a harness run can never block on it.
- **The hook to add now:** `isModified()` on the editor interface. The Cell and
  Plugin editors answer `false` today -- honestly, because neither can edit yet
  -- so the prompt is wired the day editing lands and nothing has to be
  retro-fitted then.
- Closing the WINDOW asks once per modified editor it hosts; any Cancel aborts
  the whole close. A closed editor comes back through `+`.
- Extra gate rows: an X exists per tab; a modified File Editor raises the
  prompt (asserted by the harness, never by a human); Cancel keeps the tab; an
  unmodified editor closes silently.

RULINGS OWED (carried into the report): what closing the LAST tab does.
Recommendation, stated as a recommendation and NOT shipped as a behaviour he
cannot undo: closing the last tab closes the window, and closing the last
window quits through the app's normal quit path -- which is what browsers and
Blender do AND what this app already does on window close, so it adds no new
rule. It is owed because it is the one place where a tab control can end the
program.

### AMENDMENT: drag and drop plugins into the editor (bungo, same sitting)

Verbatim: *"Also, drag and drop plugins into the editor"*.

**NOT BUILT IN THIS LANE, and the decision is deliberate.** The instruction
allowed it only "if it is cheap where you already handle `.wwcell` drops". It
is not, for a measured reason: the `.wwcell` drop is not a special case in the
drop handler at all -- a dropped file is accepted by ONE extension gate and
then sent to the NIF loader.

- `GLView::dragEnterEvent` -- `src/glview.cpp:22275`
- `GLView::dropEvent` -- `src/glview.cpp:22379`
- the window re-routes both to GLView at `src/nifskope_ui.cpp:33195` and `:33213`
- the second gate: `static QStringList validExternalNifPaths(...)` at
  `src/nifskope.cpp:9978`, which filters on `NifSkope::fileExtensions()` and
  carries a comment saying it MUST agree with `GLView::dragEnterEvent` or a
  file is accepted on hover and silently discarded on release.

So adding `.esm/.esp/.esl` means touching both gates and then adding a branch
that does NOT go to the NIF loader -- which is precisely the thing the
instruction forbids getting wrong ("Never route a dropped plugin into the File
Editor's NIF loader"). Two coupled gates plus a new destination is not a cheap
addition to an already-full lane, and a half-done version silently loads a
plugin as a NIF.

**For lane CELLWORK2, first item in NEXT.** What it needs:
1. add the three plugin suffixes to `NifSkope::fileExtensions()`'s drop path --
   or, better, a separate `pluginExtensions()` so the Open dialog's NIF filter
   is not widened as a side effect;
2. the same set in `GLView::dragEnterEvent`, or the hover/release disagreement
   the existing comment warns about comes straight back;
3. in `dropEvent`, branch BEFORE `validExternalNifPaths`: a plugin activates the
   Cell Editor, is remembered as that editor's current plugin, and its file name
   goes in the census line;
4. the honest refusal, one status line, named not silent:
   `plugin loaded: pick a cell -- not built`.


---

# RESUME (lane CELLWORK1, BUILD PENDING 2026-09-19 22:3x)

`Fallout4.exe` was up (pid 40988), so no build and no GUI run happened after
22:16. Nothing below is half-landed: the tree compiles and the last full gate
run is written up in `report.md` s6 with every number it produced.

## What the resume starts with

1. **Process guard.** `Fallout4.exe` down. bungo's NifSkope (no `--port`) keeps
   `release/NifSkope_inuse_28576.exe`; rename the live exe aside again if a new
   one of his is up, never kill it. Delete `NifSkope_inuse_28576.exe` only once
   `tasklist` shows no NifSkope at all.

2. **Build.** Two files changed after the last link:

       src/cellworkspacetest.cpp   the two repaired rows + the picture hook
       src/nifskope.h              inline NifSkope::getGraphicsView()

   `src/nifskope.h` is included very widely, so this is close to a whole build,
   not a two-file one. No `.pro` change since the last qmake run, so **no qmake
   is owed** -- `Makefile.Release` already knows `cellworkspace`, `cellrefs` and
   `cellworkspacetest` (25 / 12 / present). Confirm the exe's MTIME MOVED; a
   build that says "Nothing to be done" and rc=0 has not built anything.

   Syntax pass already done, RC=0, on `src/cellworkspacetest.cpp`. That is not a
   link and not behaviour.

3. **`ww_build_cellwork1.sh` at the repo root is this lane's build script and
   must be deleted at lane end.** `sx_CELLWORK1.sh` is already deleted.

## Then, in this order

4. `bash tests/spells/cell_workspace.sh` -- expect **0 failures**, or a named
   reason. The run of 22:16 was 23 rows (20 PASS / 3 FAIL / 1 SKIP); the file
   has grown rows since and shed one, so do NOT expect 23 again -- count what
   it prints and write that number down. The three that were red are repaired:
   * the model-vs-draw row no longer compares sizes (`drawn < size` instead);
   * the cell-name row no longer claims Sanctuary's cell has an EDID (it has
     none -- 755 of 36865 Commonwealth cells do), and the READER is proved by a
     second run on `(-21,7)` = `POIJS021`;
   * the RED control's grep has its `(` (it was matching `setWorkspaceFaceDonor`).
   New in the same file and never run: `tests/spells/cell_refs_check.py` (passes
   standalone against the 22:16 dump: `RESULT ok own 142 dump 150
   persistent-in-cell 8`), the named-cell run, and the identity-overlay run.

5. The before/after set, none of which has been run this lane:
   `cell_pick.sh`, `cell_open.sh`, `harness_window.sh`, `native_open.sh`,
   `render_shot.sh`. `animws.sh` is already run WHOLE inside the new gate and
   was rc=0 PASS on the 22:13:53 exe.

6. **The three pictures**, which the gate now takes by itself into
   `scratchpad/cellwork1_20260919/pictures/`:
   * `cell_workspace.png` -- the Cell workspace, Sanctuary, a reference selected
   * `cell_workspace_identity.png` -- the same with the identity overlay, so the
     Legend band has real `.lodi` group keys (needs a bake; the gate falls back
     to the one under `release/scratchpad/cards_agg_20260911/`, and NAMED-SKIPS
     if there is none)
   * `nif_workspace_after.png` -- the NIF workspace after coming back

   Each is a self-test ROW that prints the size it wrote: check that size against
   `release/ww_harness_window.log` rather than asserting it. **Then LOOK at them
   and say in plain words what is wrong or ugly** -- that part of the brief is
   entirely undelivered and no substitute for it exists in the report.

7. `report.md` s5 and s7 both say the exe is stale and the pictures do not
   exist. Both statements have to be replaced with what the resumed run
   measured, not edited around.
