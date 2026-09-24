---
name: nifskope-ww-add-workspace
description: How to add a WORKSPACE to NifSkope Wild Wasteland Edition (E:\Projects\NifskopeWildWastelandEdition) -- the one table and the one switch function in src/nifskope_ui.cpp, why the entry is APPENDED and never inserted, how a workspace that hides the NIF editor column gets back byte-identically (saveState/restoreState, not a re-derived layout), how a document type opens into one, where per-workspace view toggles live (one popover, one table, one QSettings group), and the gate rows that prove all of it inside the running window. Use before adding a workspace, a workspace-only dock, or a view-toggle popover; lane CELLWORK1 (2026-09-19) built the Cell workspace from this and found the mechanism was smaller than its name.
---

# NifSkope WW: how a workspace is added

Reference implementation: the **Cell** workspace -- `src/cellworkspace.{h,cpp}`,
`src/cellrefs.{h,cpp}`, `NifSkope::setWorkspace` in `src/nifskope_ui.cpp`, and the
self-test `src/cellworkspacetest.cpp` behind `WW_CELLWS_TEST`, gated by
`tests/spells/cell_workspace.sh`.

## 0. What a workspace IS here, and what it is not

A workspace is a **layout**: which docks are up, where, and which are not. It is
not a feature, it does not change any builder's output, and therefore:

* **it gets no master switch.** Masters ship OFF with a menu row (bungo's
  standing rule) and exist for features that change what the program produces. A
  view cannot qualify. If you find yourself writing a master for a workspace,
  the thing you are adding is not a workspace.
* **it gets no INI key.** No INI key without a menu row, and the workspace's own
  menu entry is that row.
* the entry in the Workspaces menu is a **label only**. No description, no
  blurb, no "(Planned)".

## 1. The table, and why appending is not a style preference

`NifSkope::WorkspaceDef` (`src/nifskope.h`, member block):

```cpp
struct WorkspaceDef {
    QDockWidget * dock = nullptr;            // nullptr only for Default
    Qt::DockWidgetArea area = Qt::RightDockWidgetArea;
    QVector<QDockWidget *> companions;       // come up WITH it, go away with it
    bool hideNifDocks = false;               // replaces the NIF editor column
};
```

built in `initDockWidgets()` from the existing `managers` list, so index 0 is
Default (no dock) and index i+1 is the i-th manager -- the same arithmetic the
old lambda did with `workspace > 0 ? managers.at(workspace-1)`.

**APPEND. Never insert.** `QSettings` key `UI/Workspace` holds a POSITIONAL
index. Inserting an entry silently reopens somebody else's window in a different
workspace than they left it in, and nothing anywhere will report that. Put a gate
row on the whole list, by name and order:

```
check "the workspace list is the old one with <Yours> APPENDED" names == expected
```

## 2. One switch function, and every route goes through it

`void NifSkope::setWorkspace( int index )`. Before lane CELLWORK1 this was a
lambda **inside** `initDockWidgets()`, reachable only from the lambdas that
captured it -- which is why every harness that ever wanted a workspace reached
past the mechanism and poked a dock by `objectName` instead. That is the smell to
look for: if a harness in this tree pokes a dock directly, the thing that owns
that dock has no reachable API.

Menu actions, document-open paths, viewport mode buttons, the harness and the
gate all call `setWorkspace`. Keep any old lambda name alive as a one-line
forwarder rather than editing its call sites:

```cpp
auto activateWorkspace = [this]( int workspace ) { setWorkspace( workspace ); };
```

## 3. Hiding the NIF editor column: the way back is the BYTES

Most workspaces sit beside the NIF editor. One that REPLACES it (a cell has no
block list, no block details, no header -- three panels that can never fill) sets
`hideNifDocks`, and then owes the person their layout back exactly.

```cpp
if ( def.hideNifDocks ) {
    if ( !workspaceLayoutSaved ) {
        workspaceLayoutBefore = saveState( 0x074 );   // Qt's own bytes
        workspaceLayoutSaved  = true;
        workspaceBeforeCell   = workspaceIndex;
    }
} else if ( workspaceLayoutSaved ) {
    restoreState( workspaceLayoutBefore, 0x074 );
    restoredExact = ( index == workspaceBeforeCell );
    workspaceLayoutSaved = false;
}
```

Three rules that are each a defect if broken:

* **Do not re-derive the layout.** "Show the docks that were showing" is not the
  same layout: sizes, splitter positions and tab order are in the bytes and
  nowhere else.
* **When the destination IS the workspace the snapshot came from, re-dock
  NOTHING on top of the restore.** `addDockWidget()` re-docks even a dock already
  in that area, and that changes the very bytes the round trip promises. Going
  somewhere ELSE restores first and then applies that workspace normally -- which
  is correct and is honestly not byte-identical to anything, because no snapshot
  of that destination was ever taken. Say so in the comment; do not claim both.
* **Hiding an already-hidden dock changes no state and no bytes**, so it is safe
  as belt-and-braces after a restore. Showing one is not.

The gate row is a byte comparison, not a look:

```cpp
skope->setWorkspace( 0 );  QByteArray before = skope->saveState( 0x074 );
skope->setWorkspace( yours );
skope->setWorkspace( 0 );  QByteArray after  = skope->saveState( 0x074 );
add( "the NIF workspace layout is byte-identical after a round trip", before == after );
```

...followed immediately by the RED control, or the comparison is not measuring
anything: `setWorkspace( someOther )` must produce bytes that are NOT equal.

## 4. Opening a document into a workspace

One place to enter, one place to leave.

* **Enter** in the branch of the loader that recognises the document type, after
  the load succeeded: `setWorkspace( wsYourIndex )`.
* **Leave** at the TOP of `NifSkope::load()`, keyed on the suffix -- not in the
  NIF branch. There are many branches down there and only one of them is yours;
  a leave written per-branch is a leave that will be forgotten by the next one.
* Go back to `workspaceBeforeCell` (the remembered in-session index), **not** to
  Default and **not** to `UI/Workspace`. Default silently discards a layout the
  person chose.

`src/nifskope.cpp` is a **CRLF** file. Splice it as bytes with a Python script
that asserts `count(anchor) == 1` and prints the CR count before and after. Do
not use a text editor on it and do not trust `grep` about its line endings.

## 5. View toggles that belong to ONE workspace

bungo, 2026-09-19: *"a lot of visual / visibility toggles that will only live in
the cell editor workspace, that are specific to cell stuff"*. References:
Blender's viewport Overlays popover, the Creation Kit's View menu.

* **ONE popover**, not loose checkboxes. Label + control, no descriptions.
* It exists only in that workspace **because of where it lives** -- a child of
  the workspace's panel, which is a child of the workspace's dock. Do not
  "enforce" the restriction with a visibility rule; make it structural, then
  assert it: `findChildren<QMenu*>("YourShowMenu").size() == 1`.
* **ONE table** behind it:

```cpp
struct ShowRow { const char * id; const char * label; bool def;
                 bool YourSpec::* field; };
static const ShowRow g_showRows[] = { ... };
```

  A later lane adds a toggle by adding a ROW -- not a widget, a member, a connect
  and a settings key in four places that will drift. `id` is BOTH the QSettings
  key and the object-name suffix, so what a gate looks for and what is persisted
  cannot disagree.
* **One QSettings group**, named in the table's comment and nowhere else
  (`CellEditor/Show`).
* **Only ship a row that has a field behind it.** A row whose toggle does nothing
  is worse than a missing row: it is a control that lies. Name the rest in NEXT.
* If flipping a row costs a REBUILD (baked vertex colours cannot be repainted),
  say so in the panel before it starts -- "rebuilding <file> ..." -- so the pause
  is explained rather than looking like a hang, and put the cost in RULINGS OWED.

## 6. The workspace's list is a MODEL, not the draw data

If the workspace lists things, list what EXISTS, not what was drawn. Lane
CELLWORK1's first list came from the pick table -- the draw data -- and was
therefore missing every light, sound marker, trigger and primitive in the cell,
because those carry no model and are never welded into the scene.

* keep a pure-data model (no Qt widget, no GL, no NifModel) keyed by the record's
  own id, filled at the single funnel every record passes through, BEFORE any
  decision about drawing;
* record WHY something is not on screen as an enum set on the way out of each
  early return, so the reason cannot drift from the count;
* join model-to-draw ONCE, where the list is built, and store the join. Then a
  row click and a viewport pick cannot select different things -- which is a gate
  row: drive both and compare;
* a row with nothing drawn refuses **by name** ("not drawn (no model)") rather
  than quietly doing nothing;
* dump the model (`WW_*_REFDUMP`) so a gate can check it against an INDEPENDENT
  reader that shares no code with the viewer -- as **SETS, not counts**. A count
  hides which side is wrong. Lane CELLWORK1's list held 150 references and the
  independent walk found 142; comparing the sets said in one line that nothing
  was lost and nothing was invented, and that the 8 extra are children of the
  WORLDSPACE's persistent cell (`0x00018AA2`, XCLC 0,0, directly under the
  world-children GRUP), which are placed by POSITION and land in whichever grid
  square their XYZ falls in. The independent reader buckets by cell child group
  and never sees them. `tests/spells/cell_refs_check.py` is the pattern.
* **The model and the draw table are not comparable by SIZE.** The pick table
  holds one entry per drawn SHAPE -- 141 drawn references produced 240 pick
  entries -- so `model.size() >= picks.size()` is not a claim about anything.
  What separates a model from draw data is that the model carries references
  NOTHING drew: assert `drawn < size` and print the fate counts.

## 7. The gate

`tests/spells/<your>_workspace.sh` + a `WW_<YOUR>_TEST` self-test compiled into
the app. The window is where the claims live: a wrong workspace, a wrong list and
a subtly re-docked panel all LOOK like a working viewer in a screenshot.

Rows that are not optional:

1. the exe is newer than every source the gate covers
2. the self-test ran and wrote its report (with a FLOOR on the row count)
3. every self-test row passed
4. each headline claim is present in the report **by name**, so a report that
   silently loses a row cannot pass by arithmetic
5. RED: with the document NOT open, the report FAILS
6. the list is the same SET as an independent read (see 6 above)
7. the layout bytes are equal after a round trip, and a different workspace's
   bytes are not
8. the workspace list is the old one with yours appended
9. exactly one of your popovers exists in the window
10. **the gate of the workspace whose mechanism you reused, run whole** (for
    CELLWORK1: `tests/spells/animws.sh`). Not grepped for -- run.

Every new row gets a floor on the other side, and the source-level controls are
`git show HEAD:<file> | grep -c <thing>` coming out **0**: the pre-change state
is reachable as text, and this tree forbids launching an old rung with a GUI
because it rewrites bungo's Recent Files list.

**Two ways a row lies, both found on CELLWORK1's first run:**

* **A RED control that matches something else is not a control.**
  `grep -c 'NifSkope::setWorkspace'` returned 7 on HEAD -- every hit was the
  long-standing `setWorkspaceFaceDonor()`. Put the `(` on the end, and check the
  control returns 0 on HEAD **and** non-zero on the working tree before trusting
  either.
* **A row never asserts a fact about the DATA that nobody has read.** The gate
  asserted the opened cell carries an editor id, with a comment saying
  "Sanctuary's exterior does". It does not: CELL `0x0000DF2E` has neither EDID
  nor FULL, and only 755 of the Commonwealth's 36865 exterior cells have one.
  Measure first, with a reader that shares no code with the thing under test,
  and put the number next to the row. (The reader still gets proved -- on a cell
  that DOES have a name: `(-21,7)`, `POIJS021`.)

## 8. A picture of the whole window

`GLView` in this tree is a **QOpenGLWindow**, not a QOpenGLWidget: it is
embedded with `QWidget::createWindowContainer()`, so it is a separate native
window and `QWidget::grab()` -- which walks the widget tree -- cannot see it at
all. A window grab therefore comes back with a hole where the viewport is.

Compose instead: `skope->grab()` for the window, then
`GLView::grabFramebuffer()` (which reads the buffer without repainting) painted
into the CONTAINER widget's rectangle mapped into window coordinates. The
container is the only thing in the widget tree that knows where the viewport is;
`NifSkope::getGraphicsView()` returns it.

`QScreen::grabWindow()` is the other honest route and captures the GL view
correctly, but it captures whatever is on top of the window too, and a harness
window on the second monitor cannot promise nothing is. Say which one was used.

Make the picture a gate ROW that prints the size it wrote, so the size can be
checked against `release/ww_harness_window.log` instead of asserted.

## 9. The two traps that cost lane CELLWORK1 a build each

* **`std::clamp( int&, int, qsizetype )` does not compile.** `QVector::size()` is
  `qsizetype` in Qt 6. Wrap it: `std::clamp( i, 0, int( v.size() ) - 1 )`.
* **A new source file needs a qmake run, not just a `.pro` edit.**
  `Makefile.Release`'s dependency lists are frozen when it is generated, so make
  exits 0 having never heard of your file. Run qmake and then
  `grep -c <yourfile> Makefile.Release` before believing the build.
