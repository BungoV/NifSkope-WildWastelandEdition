# Lane CELLWORK1 -- cell viewing becomes its own workspace

## s0. Facts at launch

- date at lane start: Sat 2026-09-19 21:33:54 CEDT.
- Process check (its own command, before any build): tasklist | grep -i -E "Fallout4|NifSkope" -> rc=1,
  no match. Fallout4 is NOT running. No NifSkope is running.
- Exe on disk at launch: release/NifSkope.exe, 23,684,608 B, 2026-09-19 20:58:28,
  sha1 595f4b9fdef7091fe1b8ec0a9760c92be2e07943 -- matches the brief exactly.
- Lane folder scratchpad/cellwork1_20260919/, BUILDING written.

## s1. STATE OF PLAY (measured before any edit)

### s1.1 The existing workspace mechanism -- what it actually is

One mechanism, and it is smaller than its name suggests. It lives entirely inside
NifSkope::initToolBars() in src/nifskope_ui.cpp:28221-28434.

DECLARATION. A workspace is a manager dock plus a name. Two index-aligned lists:

- workspaceNames (src/nifskope_ui.cpp:28238-28242), 11 entries: Default, Animation, Materials,
  Collision, Rigging, Vertex Paint, UV Editing, Pose, Skeleton, Issue Manager, LOD Generation.
- managers (src/nifskope_ui.cpp:28251-28254), 10 QDockWidget *: dAnimWs, dMatMgr,
  dCollisionMgr, dRiggingMgr, dVertexPaintMgr, dUVMgr, dPoseMgr, dSkeletonMgr, dUnfuckMgr, dLodGen.

Index 0 is Default (no dock); workspace i > 0 is managers.at(i-1). A second near-identical list,
workspaceManagers (src/nifskope_ui.cpp:27556-27559), tags the same docks with the dynamic property
workspaceRole = "manager" and wires exclusive visibility. Every comment in that region says new
entries are APPENDED LAST so stored indices keep pointing where they did.

SWITCHING. activateWorkspace(int) (src/nifskope_ui.cpp:28256-28273) -- a lambda, not a method:
clamp; hide() every manager; for i > 0 setFloating(false), addDockWidget(area, target) (Bottom for
workspace 1 = Animation, Right for the rest), show(), raise(); check the menu action;
QSettings().setValue("UI/Workspace", workspace).

WHAT A WORKSPACE OWNS TODAY: ONE DOCK, AND NOTHING ELSE. It does not hide any other dock, does not
touch toolbars or menus, does not save or restore a per-workspace layout, and assigns no shortcuts.
This is the load-bearing fact for this lane: the mechanism the brief tells me to reuse CANNOT, as it
stands, hide the NIF-only docks. Extending activateWorkspace is unavoidable; building a second
mechanism beside it is what the brief forbids.

PERSISTENCE. One key, UI/Workspace (int), written at src/nifskope_ui.cpp:28271, read only for
logging by WW_DOCKS_TEST (src/nifskope_ui.cpp:8579) and mentioned in a comment at
src/unfucktools.cpp:1110. It is NEVER restored at startup: activateWorkspace(0) runs
unconditionally at src/nifskope_ui.cpp:28433 under the comment "Always open a NIF in the Default
workspace, regardless of the last session's workspace." So the key is written and never consumed --
a live instance of the written-but-never-read defect CONSTITUTION rule 4 names.

The window layout that actually persists is Qt's own, in NifSkope::saveUi()
(src/nifskope_ui.cpp:31546-31582), under these QSettings keys (the _uip literal prefixes the
UI-path group; src/nifskope.h:70, src/nifskope_ui.cpp:260):

- "Window State"_uip   = saveState( 0x074 )   <- the dock layout; the round-trip gate's subject
- "Window Geometry"_uip = saveGeometry()
- "LeftColumn/LayoutSchema"_uip = 2, "LeftColumn/Mode"_uip, "LeftColumn/BlockSplitter"_uip,
  "LeftColumn/NifSplitter"_uip
- restoreUi() replays it at src/nifskope_ui.cpp:31651 with restoreState( windowState, 0x074 )
  (or 0x073 for the one migratable old schema).

Note for the harness: saveUi() returns early and writes NOTHING when any WW_* environment variable
is set (src/nifskope_ui.cpp:31540-31544), so a harness run can never dirty his settings even before
WW_SETTINGS_SCOPE is considered.

MENU / TOOLBAR SURFACE. QToolButton ViewWorkspacesButton (src/nifskope_ui.cpp:28228, InstantPopup)
holding QMenu ViewWorkspacesMenu (:28233), added to the File toolbar ui->tFile at :28409 --
deliberately beside File/Edit "because Blender keeps its workspace tabs in the app-level topbar"
(:28397-28408). The 11 actions are checkable in one exclusive QActionGroup. No workspace has a
keyboard shortcut. syncWorkspaceIndicators (:28416-28428) re-derives the checked entry from WHICH
MANAGER DOCK IS VISIBLE on QMenu::aboutToShow and bolds it -- the menu's idea of "active" is a read
of the docks, never a stored variable.

HOW A HARNESS FORCES ONE. There is no API. activateWorkspace is a lambda captured in other lambdas
and unreachable from outside initToolBars(), so harnesses go at the dock by object name:
WW_DOCKS_TEST does findChild<QDockWidget*>("AnimWorkspaceDock"), setFloating(false), show(), then
checks isVisible() (src/nifskope_ui.cpp:8678-8686); src/hkxanimuitest.cpp:466 does the same.
THE GATE THAT COVERS THE ANIMATION WORKSPACE IS tests/spells/animws.sh (with
tests/spells/hkxanim_ui.sh and tests/spells/hkxmodel_test.sh alongside it) -- named here as brief
item 4 requires; it must not move.

THE LEFT COLUMN IS THE CLOSER PRECEDENT for what this brief wants. dLeft (LeftColumnDock) is ONE
permanent dock holding a QStackedWidget leftColumnStack of three pages; the old ListDock, TreeDock,
HeaderDock and BrowserDock no longer exist as separate docks (asserted at
src/nifskope_ui.cpp:8595-8600). It has a real forceable API,
NifSkope::setLeftColumnMode(LeftNifs | LeftBlocks | LeftHeader) (driven at :8625-8656). So "the
NIF-only docks" -- block list, block details, header -- are in practice ONE dock with pages, which
makes hiding and restoring them far smaller than the brief's wording implies.

### s1.2 What the cell view owns today, and where it lives

Sources, all src/, all 2026-09-19: cellview.cpp/.h (63k, the scene builder), cellclick, cellground,
cellidentity, cellpanel, cellpick, cellpicktest, cellsplat.

OPEN PATH. .wwcell is a registered document type at src/nifskope.cpp:180
({ "Fallout 4 Cell View", "wwcell" }) and handled in the open switch at src/nifskope.cpp:10465-10481:
cellSpecFromEnv (harness override) else cellSpecFromFile, then cellApplyEnvModifiers, then
nifCreateCellScene(nif, spec, &err, &notes). The file is a one-line SPEC, not geometry. There is NO
File > Open Cell entry -- it opens through the ordinary file dialog's type filter.

THE PICK DOCK. CellPickDock / CellPickPanel, built at src/nifskope_ui.cpp:24897-24946. A LOCAL
inside initToolBars(), deliberately not a NifSkope member ("nifskope.h gains no member while another
lane holds that file"). Right dock area, hide() at construction. Flat Name|Value rows straight out
of CellPickTable::rowsFor() (src/cellpick.h:93), plus a summary-or-refusal line (src/cellpanel.h).

THE ONE MENU ROW THE CELL VIEW HAS. QAction "Cell Pick Panel" (src/nifskope_ui.cpp:24919),
checkable, ships UNCHECKED, added to ui->mRender (the View menu) at :24928. Toggling calls
cellPickSetEnabled(on), shows/hides the dock, and pings CellPickBus::sceneChanged(). That is the
complete cell-view menu surface.

GROUND AND OVERLAY ROWS DO NOT EXIST AS MENU ROWS. The brief's s1 asks where they live; the
measurement says nowhere. Every ground and overlay choice is an environment variable only:
WW_CELL_OVERLAY, WW_CELL_NOTERRAIN, WW_CELL_NOWATER, WW_CELL_NOGRID, WW_CELL_MARKERS,
WW_CELL_DISABLED, WW_CELL_LODI, WW_CELL_DATAROOT, WW_CELL_DUMP, WW_CELL_OPEN, WW_CELL_SPLAT_CAP,
WW_CELLSPLAT_LAYER_INDEX, WW_CELLPICK_TEST. The overlay enum is CellOverlay (src/cellview.h:127-143,
with cellOverlayName / cellOverlayFromName / cellOverlayNames); the spec fields a row would drive
are on CellSceneSpec (src/cellview.h:146-161): plugins, world, dataRoot, cx, cy, n, lodiPath,
showDisabled, showMarkers, terrain, water, grid.

IDENTITY OVERLAY + LEGEND = CellOverlay::Identity plus GROUP ids read from a .lodi bake
(CellSceneSpec::lodiPath). Legend colours are checked offline by tests/spells/cell_legend_colour.py.

THE BUDGET / CENSUS LINE IS NOT IN THE UI. It is the notes string built at
src/cellview.cpp:1442-1470 ("source triangles N, welded shapes N, vertices N, triangles N", plus the
painted ground's own note at :1116) and emitted through qInfo() at src/nifskope.cpp:10477. It
reaches a log, never a label. Offline readers: tests/spells/cell_census.py, cell_tri_budget.py,
cell_open_check.py.

THERE IS NO REFERENCE LIST ANYWHERE. The pick panel shows the ONE reference under the cursor. The
only enumeration of a cell's references is the WW_CELL_DUMP text file (src/cellview.cpp:1542-1578),
one line per pick-table ENTRY after a single # header line, and tests/spells/cell_five_refs.py /
cell_five_refs.txt. The table behind both is cellPickTable() (src/cellpick.h:107) -- pure data, no
Qt, no GL -- with refForm, baseForm, baseType, scolPart, baseEdid, model, pos, rot, scale, drawPos,
bmin, bmax, cellX, cellY, persistent, disabled, marker, layer, enableParent, hasLod, precombined,
group, groupSize, triangles. indexOfForm() maps a REFR form to its FIRST entry.

EXISTING CELL GATES: tests/spells/cell_open.sh, tests/spells/cell_pick.sh, with helpers
cell_open_check.py, cell_census.py, cell_tri_budget.py, cell_legend_colour.py, cell_five_refs.py,
cell_splat_compare.py. Fixtures tests/fixtures/sanctuary.wwcell, tests/fixtures/empty.wwcell.

### s1.3 The gap between the brief and the tree, stated before any edit

1. The workspace mechanism shows one dock and hides the other managers. It has no concept of hiding
   anything else, so "NIF-only docks hidden, then restored byte-identically" is new machinery that
   must go INTO activateWorkspace.
2. activateWorkspace is an unreachable lambda. The .wwcell open path and the harness both need it
   promoted to a member.
3. There is no reference list widget at all -- a new panel, not a move.
4. There are no view-mode or census rows in the UI -- new, but driven from CellSceneSpec fields and
   the CellOverlay enum that already exist.
5. UI/Workspace is written and never read back. "Opening a NIF switches back to the workspace that
   was active before" needs a remembered IN-SESSION value, not that key.


---

## s2  WHAT MOVED WHERE

### s2.1  The mechanism: one function, one table, no second mechanism

`activateWorkspace` was a lambda inside `initDockWidgets()`, reachable only from
the three lambdas that captured it. It is now:

- `struct NifSkope::WorkspaceDef` -- `src/nifskope.h`, in the member block after
  `dAnimWs`: one dock, one dock area, a list of COMPANION docks, and one flag,
  `hideNifDocks`.
- `void NifSkope::setWorkspace( int index )` -- `src/nifskope_ui.cpp`, defined
  just above `on_aWindow_triggered()`. Every route goes through it: the
  Workspaces menu actions, the three viewport paint modes (which still read
  `activateWorkspace(4)` -- that name is now a one-line forwarder), the
  `.wwcell` open path, the harness and the gate.
- the table is built in `initDockWidgets()` from the SAME `managers` list that
  drove the old arithmetic, so index 0 is Default with no dock and index i+1 is
  the i-th manager, exactly as `workspace > 0 ? managers.at(workspace-1)` said.
- `Cell` is APPENDED LAST. The persisted `UI/Workspace` value is positional, so
  inserting would silently reopen somebody else's window in the wrong
  workspace. A gate row asserts the list is the old eleven with Cell twelfth.

The ONE capability the mechanism gained is `hideNifDocks`. Everything else about
the switch is byte-for-byte the old behaviour.

### s2.2  The way back is the bytes

Entering a workspace with `hideNifDocks` takes `saveState(0x074)` and remembers
which workspace it came from. Leaving replays those bytes with `restoreState`.
When the destination IS the workspace the snapshot was taken in, NOTHING is
re-docked on top of the restore -- `addDockWidget()` re-docks, and re-docking
changes the very bytes the round trip promises. Going somewhere else from the
cell restores first and then applies that workspace normally, which is correct
and is honestly not byte-identical to anything, because no snapshot of that
destination was ever taken.

The QSettings keys involved: the layout itself is `UI/Window State` (written by
`saveUi()`, `src/nifskope_ui.cpp`), and the active workspace is `UI/Workspace`.
Neither is written during a harness run -- `saveUi()` refuses while any `WW_*`
variable is set -- and the gate runs under `WW_SETTINGS_SCOPE`, never bungo's
scope.

### s2.3  The reference model (new, and it is the editor's model)

`src/cellrefs.h` / `.cpp` -- `CellRefEntry`, `CellRefTable`, `CellBlockEntry`.
Pure data: no Qt widget, no GL, no NifModel, exactly like `CellPickTable` and for
the same reason -- a gate can build a cell, count its references and look one up
by form id with no window and no screenshot.

- One entry per REFR the reader returned, keyed by REFERENCE FORM ID, appended
  at `pushRefr` in `src/cellview.cpp` BEFORE any decision about drawing.
- `CellRefFate` is the builder's own reason a reference is not on screen --
  `Drawn`, `Deleted`, `NoBase`, `Disabled`, `NoModel`, `Marker` -- set on the way
  out of each of `pushRefr`'s five early returns, so it cannot drift from the
  reason the census counts.
- so the list holds the lights, the sound markers, the triggers and the
  primitives, which carry no MODL and never reach the pick table at all.

`CellBlockEntry` is the LIST OF LOADED CELLS bungo asked for: grid coords, the
CELL form id, its EDID, and per-cell reference/drawn counts taken by a WALK of
the reference table (`tallyCells()`), not by a second set of counters that could
disagree with the rows the list shows. Today's single cell is a list of one,
which is the shape a camera-following streaming lane needs.

### s2.4  Cell names, read now

`src/esmdata.h/.cpp`: `CellEntry` gained `edid`, filled from the CELL record's
EDID field in `indexWorldspace()`, with `EsmWorld::cellEditorId(cx,cy)` and
`EsmWorld::cellForm(cx,cy)` to read it back.

The DISPLAY name (FULL) is **not** read, and this is a fact about the data, not
a shortcut: Fallout4.esm is localised, so a CELL's FULL is a four-byte index into
a `.STRINGS` table, and this tree has no string-table reader anywhere. Reading it
is a reader lane (NEXT). Every place a cell is named says `SanctuaryExt03 (-20,7)`
or, for a cell with no editor id, `(-20,7)` -- there is no spelling that means
"we could not be bothered to look".

### s2.5  The workspace panel

`src/cellworkspace.h` / `.cpp` -- `CellWorkspacePanel`, in a dock named
`CellWorkspaceDock`, created in `initDockWidgets()` beside the workspace table.

- the reference LIST: flat, six columns (Type, Editor ID, Form, Tris, Cell,
  State), sortable, with a record-type filter and a text filter.
- the reference INSPECTOR is the existing `CellPickDock` as a COMPANION, not a
  copy: one panel, one set of Name|Value rows, one place a reference is
  described.
- clicking a row selects and frames the reference through `cellPickSelect()` --
  the same tail the mouse already used, so the two doors cannot drift. A
  reference that was never drawn refuses BY NAME ("not drawn (no model)")
  instead of quietly doing nothing.
- the LEGEND and the CENSUS are PARSED from the builder's own notes, not
  recomputed. Lane CELLVIEW3 already caught a second code path printing mauve
  for a grey bucket; one producer is the answer to that class of defect.

### s2.6  The Show popover -- one home, one table

`Show` is a popover that exists ONLY in this workspace, and that is not a rule
being enforced anywhere: the button is a child of this panel, which is a child of
the Cell dock, so it is on screen exactly when the workspace is. A gate row
asserts exactly one such menu exists in the whole window.

It is backed by ONE table, `g_showRows[]` in `src/cellworkspace.cpp`: `id`,
`label`, `default`, and the `CellSceneSpec` field it drives. A later lane adds a
toggle by adding a ROW. `id` is both the QSettings key and the object-name
suffix, so what a gate looks for and what is persisted cannot drift apart. The
group is `CellEditor/Show`.

SHIPPED: Cell borders (bungo's name for the builder's `grid`), Ground, Water,
Markers, Disabled references. Five rows, because those are the five the builder
already has a field for.

NOT SHIPPED, and named rather than faked: Objects, Bare-ground quads highlight
and a Cell name label have nothing behind them in `CellSceneSpec`, so a row for
them would be a control that does nothing. They are in NEXT.

Flipping a row RE-OPENS the same `.wwcell` through the ordinary open path. It is
not a repaint and does not pretend to be one: the overlay colours are welded into
vertex colours when the scene is built, so there is nothing in the document to
recolour. The panel says "rebuilding <file> ..." before it starts, so the pause
is explained rather than looking like a hang. Instant switching is a RULING OWED.

### s2.7  The open path

`src/nifskope.cpp` (a CRLF file -- spliced as bytes, not with a text editor):

- in the `.wwcell` branch: `cellWorkspaceApplyOverrides( cvspec )` before the
  build, `cellWorkspaceNoteOpened(...)` after it, then `setWorkspace( wsCellIndex )`.
  The override block carries ONLY view state -- never the worldspace, the cell,
  the plugins or the data root -- because those came from the file and a view row
  must not be able to change which cell you are in.
- at the top of `NifSkope::load()`: anything that is not a `.wwcell` leaves the
  Cell workspace and goes back to the workspace it came from. One place to enter
  from, one place to leave from; there are many branches in the loader and only
  one of them is the cell.

### s2.8  No new master switch

None was added. A workspace is a view, and the output of every builder in this
tree is identical with it and without it. The cell-pick master keeps its existing
menu row and its OFF default; the Cell workspace turns picking on while you are
in it and puts the state it found back on the way out, which is a workspace doing
what a workspace does, not a second master.

No INI key was added.

---

## s3  RULINGS OWED

Each one is genuinely bungo's, and the recommendation is stated as a
recommendation -- none of them is shipped as a behaviour he cannot undo.

1. **A view row costs a re-open.** Flipping Ground or Markers rebuilds the cell
   (seconds on Sanctuary). The alternative is keeping every overlay's colours as
   a second vertex-colour set and swapping them, which costs memory on every
   cell whether or not anyone flips a row. Recommendation: leave it as a
   re-open, which is what shipped; revisit when a cell is big enough for the
   pause to be the complaint.
2. **Where the Cell dock lives.** It is on the LEFT, where the block list was,
   because that is where an outliner lives in Blender and where the Cell View
   window sits in the Creation Kit. Not shipped as immovable -- it is an ordinary
   dock and can be dragged.
3. **Flat list vs grouping by cell.** Shipped FLAT with a Cell column. Blender
   and the CK would both nest. Nesting makes sorting lie (a tree sorts inside
   each parent, so "heaviest first" stops being the list's order), and sorting is
   what this list is for. Owed: whether he wants grouping once several cells are
   loaded.
4. **The editor tab strip**, its three editors and the one-tab-per-type limit --
   ruled tonight, NOT BUILT, written out in full in
   `scratchpad/cellwork1_20260919/PENDING.md`. Two questions inside it are his:
   whether `Plugin Editor` appears before a record editor exists (recommendation:
   omit it and list it in NEXT, because an entry that opens an empty shell is a
   promise the window cannot keep), and what closing the LAST tab does
   (recommendation: close the window, and the last window quits through the app's
   normal quit path -- which is what the app already does).
5. **`File > New Window` versus one-tab-per-editor.** The existing New Window
   makes an independent document window, which in the new vocabulary is a second
   File Editor -- which the limit forbids. Left exactly as it is today, by
   instruction. Owed: whether it becomes "raise the one File Editor", becomes a
   second window showing the same editor, or stays outside the tab registry.
6. **Which cell a person is in, when several are loaded.** The block has always
   loaded 1/3x3/5x5; nothing says which of them is "the" cell. Today the opened
   cell is simply first in the list. Owed once streaming lands.

---

## s4  NEXT (named, not built)

**First, for lane CELLWORK2:**

1. **Drag and drop plugins into the editor.** Ruled tonight, not built, with the
   measured reason and the exact file:line of both drop gates in `PENDING.md`.
2. **The editor tab strip** -- see `PENDING.md`, which is written to be built
   from without re-reading this report.

**Precombines and previs** (bungo: "Previs and precombines too, remember"). What
the tree holds today, measured: `src/esmdata.*` does not read XCRI or XPRI at
all. There is no combined-mesh index, no previs cell record, and nothing that
distinguishes a precombined reference from a loose one. So this is a READER lane
before it is a view lane: XCRI (the cell's combined-mesh list and its reference
ids) and XPRI, then `Precombined` / `Loose` as a column and as a tint.

**Cell display names.** The CELL FULL is a localised string index; this tree has
no `.STRINGS` / `.ILSTRINGS` / `.DLSTRINGS` reader. One reader, and every cell
gets its real name.

**Show rows with nothing behind them yet**, each needing a builder field first:
Objects (a mesh-visibility toggle), Bare-ground quads highlight, Cell name label
in the viewport, lights, sound markers, triggers and primitives as gizmos, room
bounds and portals, navmesh, precombined-vs-loose tint, previs, water planes,
LOD-only objects, initially-disabled tint, selection outline options, sky/fog off.

**Drawing the modelless references.** They are LISTED and COUNTED now; nothing
draws them. A light is a position and a radius, a trigger is a primitive box --
both are gizmos, and gizmos are a renderer lane.

**"Cells around".** `CellSceneSpec::n` already loads a 1/3x3/5x5 block, so the
static form is a row driving an existing field against the 12M vertex refusal
cap. It is NOT shipped in this lane: the Show table is for view toggles, and a
control that re-reads four times as much plugin data is not one. Camera-following
load/unload is CELLWORK2 by ruling.

**Editing and saving a cell.** The reference model is the object a later lane
edits, with an undo stack over it like the animation workspace's. What does not
exist ANYWHERE in this tree is a WRITER: `src/esmdata.h` has no save, no record
emit and no byte writer. Editing a cell is a plugin-writing lane before it is a
UI lane, and that is the honest order.

---

## s5  THE BUILD, AND WHY THE LANE ENDS BUILD PENDING

`Fallout4.exe` came up at some point between 22:16 and 22:26 (pid 40988, seen by
the process guard immediately before the second build). CONSTITUTION 6: the lane
ends BUILD PENDING. No build was started after that moment, and no GUI harness
was launched after it either -- the pictures need a run, so the pictures are
owed, and nothing below describes a picture nobody has seen.

### The exe the gate actually ran on

    release/NifSkope.exe
      mtime  2026-09-19 22:13:53 +0200
      size   23,803,392 bytes
      sha1   c9db7d09de0a5b89bf054541d338bf6afa7fa508

At the moment it was linked it was newer than every source in the tree
(`find src res NifSkope.pro tests -newer release/NifSkope.exe` came back empty).

**It is now stale**, deliberately, by two files: `src/cellworkspacetest.cpp` and
`src/nifskope.h`, both changed after the gate run to repair what the gate found.
Anyone resuming this lane starts with a build; anyone reading a harness number
below should read it as "measured on the 22:13:53 exe".

The rung is unchanged and was made once:

    release/NifSkope.before_cellwork1.exe
      mtime  2026-09-19 20:58:28 +0200
      size   23,684,608 bytes
      sha1   595f4b9fdef7091fe1b8ec0a9760c92be2e07943

bungo's own window (pid 28576, no `--port` on its command line) holds
`release/NifSkope_inuse_28576.exe`, renamed aside and never killed. It may only
be deleted once `tasklist` shows no NifSkope at all.

### Syntax pass instead of a build

`g++ -fsyntax-only` with the flags out of `Makefile.Release`, on the one
translation unit that changed: `src/cellworkspacetest.cpp` -> **RC=0**. It
proves the file compiles. It proves nothing about the link, nothing about moc,
and nothing about behaviour.

`bash -n tests/spells/cell_workspace.sh` -> parses.

---

## s6  GATE COUNTS, BEFORE -> AFTER

### The new gate, `tests/spells/cell_workspace.sh`

**Before:** the file did not exist, and neither did anything it measures. Its
two source-level RED controls are the before state, reachable as text rather
than by running the old rung (this tree forbids launching an old rung with a
GUI, because it rewrites bungo's Recent Files list):

    git show HEAD:src/nifskope_ui.cpp | grep -c 'NifSkope::setWorkspace('   ->  0
    git show HEAD:src/cellview.cpp   | grep -c 'cellRefTableMutable'        ->  0

**After, run at 22:16 on the 22:13:53 exe: 23 rows, 20 PASS, 3 FAIL, 1 SKIP.**
Every one of the three is written up below, with what it turned out to mean.
The in-window self-test inside it: **31 rows, 2 failures**.

The three failures were defects **in the gate's own claims**, not in the
workspace -- which is the outcome a first gate run is for, and is why the rows
were written to print their numbers rather than a verdict:

1. **`the reference model is not the draw data`** asserted
   `refs.size() >= picks.size()`: model 150, pick table 240. The two are not
   comparable by size at all. The pick table holds one entry per drawn SHAPE,
   so one reference with three shapes is three pick entries; 141 drawn
   references produce 240 of them. The claim that actually separates a model
   from draw data is that the model carries references NOTHING drew, and that
   is what the row now asserts: `drawn < size` -- 150 references, 141 drawn,
   9 undrawn. FIXED in `src/cellworkspacetest.cpp`, unbuilt.

2. **`the cell carries its editor id from the plugin`** asserted a name on
   Sanctuary's own cell. Measured in Fallout4.esm: the Commonwealth has 36,865
   exterior cells, **755** of them carry an EDID, and **(-20,7) is not one of
   them** -- CELL `0x0000DF2E` has neither EDID nor FULL. The row was asserting
   something false about the data. It now asserts what is true (the form id is
   always there; the label is the editor id when the plugin gave one and the
   bare grid when it did not), and the EDID READER is proved by a second run of
   the window on the neighbouring cell **(-21,7), which the master names
   `POIJS021`**. FIXED in both the self-test and the gate, unbuilt.

3. **`RED: git HEAD has no such function`** came back saying HEAD mentions
   `NifSkope::setWorkspace` seven times. It does not: the grep had no `(` on the
   end and was matching the long-standing `NifSkope::setWorkspaceFaceDonor()`.
   A control that matches something unrelated is not a control. FIXED (the
   corrected grep returns 0 on HEAD and 1 on the working tree), unbuilt.

And the one SKIP, which was the most useful line in the run:

4. **the independent count could not be read.** `cell_census.py` prints a
   markdown table, not the shape the scrape expected. Fixing the scrape would
   have turned a skip into a red row, because the two numbers genuinely differ:
   the census says **142** references, the viewer's list says **150**.

   The difference is fully explained, and the explanation is that the viewer is
   right. A new checker, `tests/spells/cell_refs_check.py`, compares the two as
   **SETS** rather than counts, because a count hides which side is wrong:

       cell 0x0000DF2E at -20,7: 142 references in its own child group
       worldspace persistent cell 0x00018AA2: 32795 references in the whole world
       the viewer's list: 150 references
       lost by the viewer     : 0 []
       extra, from the persistent cell: 8
       extra, from NOWHERE the plugin explains: 0 []
       RESULT ok own 142 dump 150 persistent-in-cell 8

   The eight are children of the WORLDSPACE's persistent cell (`0x00018AA2`,
   XCLC 0,0, sitting directly under the world-children GRUP), which are placed
   by POSITION and therefore fall into whichever grid square their XYZ lands in:
   a folding chair, two patrol idle markers, four decals and a light box. The
   census buckets by cell child group and so never sees them; the viewer sweeps
   the persistent cell by position and does. That row now asserts the two set
   claims -- nothing lost, nothing invented -- which is a stronger statement
   than any count, and it passes.

### The workspace whose mechanism this lane reused

`tests/spells/animws.sh`, run WHOLE from inside the new gate (not grepped for):
**rc=0, PASS**, before and after. It did not move.

### Still owed, because they need a run

`cell_pick.sh`, `cell_open.sh`, `harness_window.sh`, `native_open.sh`,
`render_shot.sh` -- not run in this session. They are named in the resume block
in `PENDING.md` with the order to run them in.

---

## s7  THE PICTURES: NOT TAKEN, AND WHAT IS BUILT TO TAKE THEM

Three pictures are owed and none exists. The hook that takes them is written and
compiles, and has never run:

* `WW_CELLWS_SHOTS=<dir>` (+ `WW_CELLWS_SHOTTAG`) in
  `src/cellworkspacetest.cpp`, which writes `cell_workspace.png` and
  `nif_workspace_after.png` at the end of the self-test, each as a self-test ROW
  carrying the size it wrote, so the size can be checked against
  `release/ww_harness_window.log` rather than asserted;
* the identity picture is a third run of the window in `cell_workspace.sh`, with
  `WW_CELL_OVERLAY=identity` and a `.lodi` bake, so the Legend band has real
  group keys in it. With no bake the row is a NAMED SKIP: a picture of a refusal
  band is not a picture of the legend.

**Which grab, and why.** `QWidget::grab()` cannot capture the viewport in this
tree, and the reason is sharper than "GL views do not grab": `GLView` is a
**QOpenGLWindow**, not a widget. It is embedded with
`QWidget::createWindowContainer()`, so it is a separate native window and is not
in the widget tree `grab()` walks at all.

The hook therefore **composes**: `skope->grab()` for the window, then
`GLView::grabFramebuffer()` painted into the CONTAINER WIDGET's rectangle
mapped into window coordinates. The container is the only thing in the widget
tree that knows where the viewport is, and reaching it needed one new accessor,
`NifSkope::getGraphicsView()` (`src/nifskope.h`, beside `getGLView()`).

The alternative, `QScreen::grabWindow()`, was rejected on purpose: it captures
whatever the compositor has, which is correct for the GL view but also correct
for anything sitting on top of the window, and a harness window on the second
monitor cannot promise nothing is on top of it. Composition promises the picture
is of THIS window and nothing else.

I have not seen these pictures. There is nothing here about what is ugly in
them, because saying anything would be inventing it.

---

## s8  TEXT FOR THE DOCUMENTS (I do not edit them; the overseer splices)

### WW_CHANGES.md

```
- A Cell workspace joins the Workspaces menu. Opening a `.wwcell` switches to
  it and opening anything else switches back to the workspace you were in, with
  the NIF editor column restored exactly as you left it -- the layout is replayed
  from saved bytes, not re-derived, so splitter sizes and tab order survive.
- The Cell workspace lists every reference the plugin places in the cell, not
  just the ones on screen: lights, sound markers, triggers, idle markers and
  decals are in the list with the reason they are not drawn. Clicking a row
  selects and frames the reference; picking in the viewport selects the row.
  A reference that was never drawn says so by name instead of doing nothing.
- A Show popover in that workspace carries the cell's view toggles -- cell
  borders, ground, water, markers, disabled references -- and lives only there.
- The cells a view has loaded are a list, each with its grid, its CELL form id
  and its editor id where the plugin gives one.
```

### HANDOFF.md

```
LANE CELLWORK1 (2026-09-19 20:40 -> 22:3x) -- the Cell workspace. BUILD PENDING.

What landed in the tree (not committed, per standing instruction):
  src/cellrefs.{h,cpp}        NEW  the reference model + the loaded-cells list
  src/cellworkspace.{h,cpp}   NEW  the dock, the list, the Show popover
  src/cellworkspacetest.cpp   NEW  WW_CELLWS_TEST, 31 rows, + the picture hook
  tests/spells/cell_workspace.sh   NEW  the gate, 23 rows, runs animws.sh whole
  tests/spells/cell_refs_check.py  NEW  the plugin walk the list is checked against
  src/nifskope.h              WorkspaceDef + setWorkspace + getGraphicsView
  src/nifskope_ui.cpp         the workspace TABLE and the one switch function
  src/nifskope.cpp            enter on .wwcell, leave at the top of load()
  src/cellview.cpp            fills the model; WW_CELL_REFDUMP
  src/esmdata.{h,cpp}         CELL EDID + cellForm read out of the plugin
  NifSkope.pro                the three new sources

State: the exe at 22:13:53 ran the gate at 20 PASS / 3 FAIL / 1 SKIP. All three
failures were wrong CLAIMS in the gate, not defects in the workspace; all three
are repaired in source and UNBUILT, because Fallout4.exe came up (pid 40988).
The three pictures are owed and the hook for them is written and unbuilt.

Resume: scratchpad/cellwork1_20260919/PENDING.md, section RESUME.
Not built by ruling, designed in full in that same file: the editor tab strip
(File / Plugin / Cell Editor), one tab per type, the close X + unsaved prompt,
and plugin drag-and-drop.
```

### MISTAKES.md -- as landed, two entries, both at the top

1. `2026-09-19 -- a gate row that asserted a fact about the data nobody had read
   (lane CELLWORK1)` -- the row that claimed Sanctuary's exterior cell carries
   an editor id. It does not; 755 of 36865 Commonwealth cells do.
2. `2026-09-19 -- a source comment that reported an observation which never
   happened (lane CELLWORK1)` -- a comment that cited a count from a run that
   had never been made.

The file is CRLF and was spliced as bytes both times. Counts: CRLF 10705 ->
10736 -> 10770, lone LF 0 throughout, CR count equal to the CRLF count at every
step.

### The skill

`.claude/skills/nifskope-ww-add-workspace/SKILL.md`, written to BOTH trees,
sha1 `a94f98e0e9123d5bd7be8c105c34453c24cc8ba8` on each. Nine sections. What it
gained from the finished work, over the draft written mid-lane: the model and
the draw table are not comparable by size; an independent reader is compared as
SETS and not counts, and it has to know about the worldspace's persistent cell;
a RED control with a missing `(` matches something unrelated and proves nothing;
a gate row never asserts a fact about the data that nobody has read; and a
full-window picture in this tree has to compose, because GLView is a
QOpenGLWindow and `QWidget::grab()` cannot see it.
