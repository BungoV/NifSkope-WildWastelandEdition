---
name: ww-retire-a-surface
description: Remove a dock, panel or tool from NifSkope Wild Wasteland Edition (E:\Projects\NifskopeWildWastelandEdition) when bungo has ruled that its replacement is the one surface -- the seven places a dock is still reachable from, the workspace INDEX that is stored in QSettings and silently shifts when a list loses an element, the harnesses that drove it, the user-facing strings that name it, and the inventory table the director needs before anything is ported. Use for every "retire the old X" ruling; lanes WATER7 (the Water Marking dock), WATER8 (its menu entries) and UI6 (the Animation Manager) each re-derived this.
---

# NifSkope WW: retire a surface

Repo `E:\Projects\NifskopeWildWastelandEdition`. Written from lane UI6
(2026-09-11), which retired the Animation Manager dock after bungo's *"old and
outdated"* and three screenshots of what was wrong with it.

## 1. The inventory comes FIRST, and it is not a port

Before a line is deleted, write the table: **everything the old surface offered,
and whether the new one has it.** Read the old widget's header -- its members
are the inventory -- not its screenshots.

* Three columns is a trap. Two are enough: *what it had* and *YES / PARTIAL /
  NO*, with the PARTIALs saying in what way.
* **Nothing is ported silently.** A lane that quietly reimplements one missing
  control has spent a build on a decision that was the director's and bungo's.
  The table goes in the report and in the handoff, and the lane stops there.
* Say which losses are large in plain words. UI6's table had sixteen rows and
  one sentence that mattered: *"the largest by far is editing a NIF's own
  animation keys -- the new dock is a Havok clip editor and the NIF side of it
  is read-only."*

## 2. The seven places a dock is still reachable from

`grep -rn "<DockObjectName>\|<dMemberName>\|\"<Its Title>\"" src/ tests/ tools/`
and expect all of these, because UI6 found all of them:

1. its construction (`new QDockWidget`, `setObjectName`, `addDockWidget`) and
   every `connect` on it;
2. the **workspace manager list** and the **View > Panels** list -- usually two
   separate `QList<QDockWidget *>` literals with the same members in the same
   order;
3. its `toggleViewAction()` lambda (the re-dock-when-reopened guard);
4. a button somewhere else that opens it (UI6: a "Timeline dock..." push button
   in the render toolbar's Animation panel);
5. the **block-type -> dock** table behind "Open in ..." on a block's context
   menu;
6. the in-application harnesses that name it -- both the ones that TEST it and
   the ones that merely list it (`WW_DOCKS_TEST`'s `dockNames[]`);
7. **user-facing strings that mention it by name** in files that have nothing to
   do with it. UI6 shipped its first link with two still in: a spell's dialog
   telling the user to use the retired dock's channel copy/paste, and a physics
   panel tooltip naming its timeline. `grep -rn "<Its Title>" --include=*.cpp`
   after the retirement, not before.

## 3. THE WORKSPACE INDEX IS STORED, AND A LIST THAT LOSES AN ELEMENT SHIFTS IT

`activateWorkspace` indexes a `managers` list by position and writes the chosen
index to `QSettings` (`UI/Workspace`). Delete the first element and every stored
index points one place to the left: a user whose last workspace was Materials
opens Collision.

**Put the replacement in the retiring dock's SEAT.** UI6's list was
`{ dTimeline, dMatMgr, ..., dLodGen, dAnimWs }` and became
`{ dAnimWs, dMatMgr, ..., dLodGen }` -- same length, same indices for everything
else, and the new dock inherits the menu entry the old one had.

That last part is worth checking on its own: UI6 found the replacement had been
APPENDED at index 10 while the menu carried eleven names (0..10), so the entry
that would have opened it did not exist and **no Workspaces entry could reach
the new dock at all.** Count the names against the list before you trust either.

## 4. The member, and what a null one costs

Two members usually exist: `QDockWidget * dFoo;` and `FooWidget * foo = nullptr;`.

* A member with **no initialiser** must be DELETED, not left unassigned -- it is
  an indeterminate pointer the moment the dock stops being constructed.
* A member that IS initialised to `nullptr` may stay, with a comment saying it
  is permanently null and why, when call sites already guard it (`if ( foo )`).
  That is the cheap, safe half of a retirement; deleting the widget CLASS is a
  separate lane and is named as owed. Check first whether its translation unit
  also holds something the whole window uses -- UI6's `timeline.cpp` owns the
  procedural icon set (`tlMakeIcon`), so the file stays whatever happens to the
  class.

## 5. Guarded blocks delete cleanly; a widget's own code does not

If the part being removed is behind `#ifdef WW_SOMETHING`, strip the blocks
mechanically and trust the guard: the translation unit was written to compile
without the macro, so what is left is valid by construction. A script that
tracks `#if` nesting and REFUSES on an `#else` at depth 1 does it in one pass
(UI6: 14 blocks and 512 lines out of `timeline.cpp`, 4 and 52 out of its header,
`g++ -fsyntax-only` RC=0 on the first try).

Never do the same by hand to unguarded code in the same run.

## 6. The harnesses: re-aim, retire, or refuse -- and say which

A harness that drove the retired surface has three honest futures, and a lane
picks one PER HARNESS and states it:

* **Re-aim** it at the replacement when the coverage is still wanted. The widget
  names change; so do the WAITS (`nifskope-ww-build-verify` and
  `ww-test-harness-add` both carry this) -- the new surface may rebuild on a
  timer where the old one rebuilt synchronously, and `processEvents()` does not
  fire a timer that has not expired. Re-aim the check's SUBJECT too where the
  new surface is shaped differently, and say so in the check's own text.
* **Retire** it with the surface, naming what coverage goes with it.
* **Refuse in words**: leave the harness compiling and make it write a line that
  names the lane, the date and what it used to measure, instead of silently
  measuring nothing. UI6 did this for `WW_ANIMPLAY_TEST` and `WW_ROTKEY_TEST`.

Record the pre-change baseline of every one of them before the first edit, or
the count that moves afterwards cannot be explained.

## 7. Gate the removal, with a control

* `findChild<QDockWidget *>( "<OldObjectName>" ) == nullptr`, with the same
  search finding the replacement as its floor.
* The menus: the dock-toggle list and the Workspaces list dumped into the log by
  name, so "no entry names it" is a printed list and not an assertion.
* **A control run of the kept rollback rung** settles every count that moved.
  UI6's `loaded_nifs.sh` read 166/3 against a baseline of 166/0; the rung read
  166/1, twice, which is what separated "this lane broke one" from "one was
  already red".
