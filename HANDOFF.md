# Handoff — NifSkope, Wild Wasteland Edition

**Read this first.** It is the short document: where things are, how to build,
what will bite you. [WW_CHANGES.md](WW_CHANGES.md) is the detailed history,
[WW_FEATURES.md](WW_FEATURES.md) is what the fork adds, and
[docs/TO_BE_IMPLEMENTED.md](docs/TO_BE_IMPLEMENTED.md) is the single backlog.

**Working directory:** `E:\Projects\NifskopeWildWastelandEdition`
(GitHub: [BungoV/NifSkope-WildWastelandEdition](https://github.com/BungoV/NifSkope-WildWastelandEdition),
branch `main`, `origin` is the fork — never push upstream.)

Updated **2026-08-09**. Edition **0.3.1**. Build green, working tree clean,
pushed through left-panel polish `5df0c36`.

### Window-state diagnostic cleanup safety

`window_state_roundtrip.sh` had an unsafe failure path: its `cmd /c` cleanup
discarded the exit status from deleting/importing the NifSkope registry tree and
then removed the backup unconditionally. A failed run left the real profile with
the test-only values `New Document Cube=0` and `Suppress Save Confirmation=1`.
Both user values were restored to their safe defaults (`1` and `0`), and the
harness now checks direct `reg.exe` process exit codes and retains/reports the
backup on any restore failure. Do not reintroduce cleanup that can erase the
only snapshot without proving the import succeeded.

### This session

One structural UI pass, closed end to end: the left editor is now one permanent
three-mode column instead of four tabified docks.

The compressed-width migration gap is closed. Schema 1 had already saved
`LeftColumnDock` at Qt’s incidental ~260 px content hint, so the old “new dock”
400 px initialization no longer ran. Schema 2 now requests **400 px once**,
after state and geometry replay, then permanently returns width ownership to the
user. The real schema-1 profile measured **400 px** on launch and retained the
existing **164 → 432 px** fold/unfold range; the capture was inspected and
`loaded_nifs.sh` remains **93/93**.

The top **Blocks · Header · NIFs** selector is now a full-width, equal-third
segmented control in selection blue. It shares its skin-backed geometry with
Collision Creation / Simulation: only the two outside ends are rounded, and
each square internal join has one border rather than two beveled corners.
Verified in the release build with `loaded_nifs.sh` **93/93** and
`collision_panel.sh` **41/41**; both generated captures were inspected.

- **Blocks** shows Block List above Block Details.
- **Header** gives the Header the full column.
- **NIFs** shows NIF Browser above Loaded NIFs.
- The buttons are ordered **Blocks · Header · NIFs**. Tab positions carry an
  explicit stable-mode ID, so moving NIFs from the second to the third button
  does not reinterpret an existing saved NIF mode as Header.
- The selector is the first row at the top. Switching it changes only a
  `QStackedWidget` page: the views, models, selection, searches, splitter sizes
  and unsaved Loaded NIFs stay alive in place.
- The old four dock shells are consumed before `restoreUi()`, then deleted. The
  new `LeftColumnDock` is the only core dock Qt ever restores, so no live widget
  is reparented after it enters the saved dock graph.
- Saved-window state is version `0x074`. Existing `0x073` layouts get one
  compatibility replay for unrelated docks/toolbars; mode and both inner
  splitters then persist explicitly under `UI/LeftColumn`.
- The column still folds from **164 to 432 px** in the harness while preserving
  the viewport's 50 px minimum.
- **Inactive pages cannot paint through.** The legacy visibility reset now runs
  before the content widgets enter `QStackedWidget`; doing it afterwards had
  overridden Header's hidden state and exposed its Type column as a vertical
  strip of characters along the viewport divider. The mode harness now requires
  exactly one page to be visible and the screenshots were checked again.

- **The selector and both Block panels were compacted and clarified.** Blocks,
  Header and NIFs use a flat orange-underlined selector with no shortcuts. Block
  List has one toolbar plus an advanced Filters dropdown, accurate
  Block/Name/Summary columns, navigable breadcrumbs, cached totals and a clear
  no-results state. Block Details has one search/pin/overflow row and explicit
  no-selection, no-match and no-pins states.
- **Header is now a standalone file inspector.** It shows source identity and
  NIF/User/Bethesda versions, recursively searches Name/Value/Type without
  replacing its model/root, exposes copy-summary/path actions, and retains the
  useful Type column while folding with the unified dock.

- **Eye and transparency clicks no longer select the row.** The Loaded-NIF view
  owns the full press/release gesture over those glyphs, so no orange/blue
  selection flash appears and no drag begins accidentally.
- **Loaded NIFs has its own search field and real vertical scrolling.** Filtering
  hides source rows without proxy-remapping their drag/action identity. A
  40-row harness probe proves the scrollbar gets a non-zero range.
- **Loaded NIFs reports its live membership.** Its header says total or
  shown/total, carries a glyph legend, and row tooltips add source/unsaved state.
  Empty and no-match states are passive paint. Browser Refresh and filtering are
  now required to preserve the exact Loaded model pointers and persistent rows.
  The 4 px vertical splitter stays non-collapsible and keeps both panes reachable.
- **The row menu is grouped by intent.** File actions, rigging, workspace
  display, tools/revert and removal are separated; skeleton and face-donor use
  the same skull/face icons as the row; duplicate Close/Remove wording is gone.
- **The nearly immovable left divider was a hard 400×240 dock minimum.** Those
  List/Tree dock floors are gone. The tested browser column now travels from
  164 to 432 px while preserving the viewport's 50 px minimum. Collision,
  Rigging and Vertex Paint expose horizontal scrollbars when folded; UV no
  longer adds a redundant 340 px dock floor. Genuine content floors (UV render
  view and timeline graph/lane) remain.
- **Verified:** staged release build green; `loaded_nifs.sh` **91/91**;
  `WW_DOCKS_TEST` **13/13**; `archive_browse_survives_load.sh` **4/4**; and the
  final populated/no-match screenshots were visually checked. The earlier
  `collision_panel.sh` **39/39** and two-cycle window-state pass are unchanged.

### Open

- **`block_drag_live.ps1` has not been run since the multi-parent payload
  change.** It seizes the mouse; ask first, every time.
- The NIF Browser harness covers the real view gates, exact captured payloads,
  both save/load routes and rendered geometry, but no pointer-seizing live mouse
  script was run.
- An auxiliary re-run of `window_state_roundtrip.sh` did not reach its restore
  assertion: both the prior canonical binary and the staged binary stayed open
  after cycle 1's `CloseMainWindow()`. The script restored the registry profile
  each time. This did not reproduce in the dock or Loaded-NIF harnesses and is
  not attributed to the left-panel change; diagnose the close harness separately
  before claiming a fresh two-cycle pass.
- The flat-list **hang** below is still open and still harness-only.
- Everything else in this file's later sections is carried forward and untouched.

Two headlines:

- **The Block List is a direct-manipulation panel now** — drag to re-parent,
  reorder and un-parent; paste follows the pointer *in every window*; a click on
  blank space selects nothing. Details below.
- **The flat list mode is fixed and kept.** It was worse than filed — *no* row in
  it could be clicked, dropped on or right-clicked, not just newly inserted ones
  — and the cause was `QHeaderView`'s cached total going negative, not anything
  about the model. Hierarchy mode was never affected.
- **The thing filed as "the flat list takes the process down" is a HANG, still
  open.** `block_rename.sh` in list mode stops 4 runs in 8: no crash, no fault
  under gdb, no APPCRASH event — a passing run is 4 seconds and a failing one is
  63, the script's deadline. Ruled out: stale build, the IPC port, the inherited
  animation setting, contention — and, as of this session, a repaint storm
  (frames are counted now: one or two) and any nested event loop or modal (a
  watchdog timer logs nothing across a 60-second hang, and timers *do* run inside
  a nested loop). It is **one event handler that never returns**, after
  `setCurrentIndex`. Next step is a stack, and `scratchpad/stack_hang.sh` takes
  one — it just needs more than four attempts to catch a 50% event. Harness-only:
  no user path reaches it.

### The Block List, as it now behaves

| gesture | result |
|---|---|
| drop **on** a `NiNode` | re-parent into it, **preserving world position** |
| **Shift** + drop | re-parent keeping the LOCAL transform |
| **Ctrl** + drop | link — adds the child link and keeps the old parent |
| drop in the **gap** between two rows | reorder to that position in the parent's `Children` |
| drop in the **blank space**, or the gap beside a top-level row | **out** — loses every parent, becomes a root of its own |
| hover a node that would accept the block | it unfolds after ~650ms, and folds back when the drag ends |
| pointer near the top/bottom edge | the list auto-scrolls |
| **Ctrl+V** over a row / over blank space | pastes into that row / with no parent |
| click blank space | selects nothing at all |

A row that **cannot take children is all gap** — there is nothing to drop inside
a mesh, so its whole height reorders. Only a `NiNode` keeps the
third/middle/third split.

`wwReparentBlocks` in `blocks.cpp` is the one primitive, shared with the
Collision Manager's Set Parent. `release/ww_drag.log` records the most recent
drag with no flag to set (`WW_DRAG_LOG=off`, or a path, overrides).

### Where to pick up

*(From the 2026-08-07 block-list sessions. Merging collision shapes in the
Collision Manager, listed here as unstarted, shipped in `WW_CHANGES.md` 08-07zb.)*

The four things that handoff listed are all closed — three fixed, one measured
and deliberately not done. What is left of them:

1. **Drag-and-drop has no coverage in flat list mode.** Nothing structural is in
   the way now that the view answers `indexAt` there; `block_dragdrop.sh` seeds
   no list mode, so it needs the registry dance `block_list_modes.sh` uses. Its
   code branches on the model and is believed correct, and nothing has driven it.
2. **Every structural edit serialises the whole file twice.** `nifSnapshotOp`
   saves the NIF before the operation and again after, for one undo step — 88 ms
   on a 512-block file, 160 ms on 2012, and it does not track what the operation
   touches. It is the largest cost in a drag by a wide margin and it is shared by
   everything, so it wants its own decision. In the backlog.
3. **The two list modes have drifted.** Flat list has no reorder, no drag-out and
   no auto-expand — deliberate, since it is file order rather than anyone's
   children. The mode is being kept; this is a question of how far to take it,
   not whether.

The live drag script is **cleared**: seven scenarios, six verified green in one
run and the seventh read out of that run's own log. Two things it taught, both
worth carrying:

- **A refused target never receives a drop event.** The handler answers with
  `Qt::IgnoreAction` and Qt withholds the `QDropEvent` entirely, so "no DROP
  reached the list" is the correct outcome for a refusal, not a failure.
- **`payload [N]` in the drag log is the block COUNT.** The identity of what was
  picked up is in the `=== drag start … first N ===` header. Reading the count as
  a block number had the script convicting the program in two whole runs.

It also could not fail at all until this session — `Write-Output` inside a
function whose caller wrapped it in `if (-not (…))` put the message *into* the
condition, and a two-element array is truthy however the verdict came out.

### Open, and honest about it

- **The UV Editor fold assertion is not functional coverage.** Its current
  `minimumWidth() < 340` check changes with polish/layout timing. A direct
  `resizeDocks(..., 280)` probe after showing the dock produced **795 px**: the
  wide header rows still impose a real effective floor despite the old explicit
  340 px dock minimum being gone. This was discovered while verifying the left
  editor’s independent 400 px migration and was deliberately not folded into
  that one-change fix.

- **Preset save/rename/remove still has no harness.** The "+" goes through a
  modal `QInputDialog::getText`, so it is not drivable the way the existing
  harnesses drive widgets; covering it means exposing the storage helpers behind
  test-only entry points first. Re-parenting, the other half of this note, is
  covered now — the operation moved into `wwReparentBlocks` and
  `block_dragdrop.sh` drives it.
- **Re-parenting has two transform rules now, on purpose.** The block list's
  plain drop preserves world position; the Collision Manager's **Set Parent**
  keeps the LOCAL transform, which is right for attaching collision to a bone
  and is why it was not changed.
- **`window_state_roundtrip.sh` runs outside the restricted sandbox.** It needs
  `Add-Type` temporary writes under `C:\msys64\tmp`; with that permission it is
  green for two maximised save/restore cycles on the second monitor. A sandbox
  permission failure before NifSkope launches is environmental, not a product
  failure.
- **The title bar reports a stale build.** `NIFSKOPE_REVISION` is baked when
  qmake runs, not when make does, so the About box and title can name an older
  commit than the binary. Cosmetic; fix by regenerating on link the way
  `README.md` already is.
- Parked from the same conversation, by choice: refit-shape-to-mesh (ranked
  highest of these), copy/paste physics between bodies, save-preset-from-an-
  existing-body, change shape type in place, mirror across X, and settling
  whether several selected meshes should make one `bhkListShape` body instead of
  several.

---

## State

Edition **0.3**, on upstream NifSkope 2.0.dev11 (fo76utils `develop` @
`f2587869`).

### What the last session changed

Fourteen commits, `873e02f`…`a901281`. All committed, harnessed and pushed.

The feature is in the table above. What is worth carrying forward is *how* it
went, because the shape of it will repeat:

- **The first version shipped dead, with 26 green checks.** Nothing in this
  codebase set `Qt::ItemIsDragEnabled`, so `QAbstractItemView` never entered
  `DraggingState` and `startDrag()` was never called. The harness drove the drop
  handlers directly — correct, since no synthetic event can enter a native drag
  loop — which put the one broken step outside everything it measured.
- **Four more fixes were made by reading code, none of them right**, while the
  harness climbed to 44 green. What actually found it was
  `tests/spells/block_drag_live.ps1`, driving the physical mouse: one run, one
  `DragEnter`, then silence. Ignoring a drag event ends the drag over the widget,
  and a drag begins on the row being dragged, whose neighbouring gaps refuse as
  no-ops. Dead before it began.
- **Rename was already built** (`d5765c4`) and filed in the backlog as not
  started, because nothing measured it. It had one real gap — proxy-only, so flat
  list mode did nothing at all.
- **Three checks were written that passed for the wrong reason** and had to be
  rewritten: a paste test casting an invalid index, a hover test whose helper
  re-expanded the row before hovering, and a Block Details test calling
  `NifTreeView::isRowHidden`, which shadows Qt's with a different meaning. Two of
  them wasted a build each; the third wasted two.

Everything above is in [WW_CHANGES.md](WW_CHANGES.md) under 2026-08-07 a–n.

### The rule that came out of it

**Ask what your harness enters below, and cover that separately.** Every bug in
this session lived above the point where the tests started: the drag start, the
native event loop, the paint during a modal drag. 82 checks below that line and
zero above it read as thorough and was not.

### Open, not started

- **Scale Inertia Tensor** exists in no UI. The 3ds Max exporter offers it.
- **Phantom / Shape Phantom** are present but greyed: they need
  `bhkSPCollisionObject` + `bhkSimpleShapePhantom`, which nothing here writes.
- **Gravity Factor, Rolling Friction Multiplier, Time Factor, Collision
  Response** are real `bhkRigidBodyCInfo` fields exposed nowhere.
- With **Replace off**, a new shape joins an existing body's `bhkListShape` —
  and the body settings in the create popup then do not apply to it. Decide what
  should happen.

Everything in [WW_FEATURES.md](WW_FEATURES.md) is built, committed, and covered
by at least one harness. Nothing is half-landed. Two things are deliberately
parked and say so in the UI:

- **PBR rendering** — implemented, mode and toggle greyed out.
- **The four mapped features** — growing-op segment attribution, `NiPointLight`,
  `NiPSysColliderManager`, `NiPSysBombModifier`. Researched and written up in
  [docs/FOUR_FEATURES_PLAN.md](docs/FOUR_FEATURES_PLAN.md), not started, on the
  user's instruction.

One thing on the list is unfinished-by-request: the **Shading menu** is longer
than it should be. Simplification was raised and then deferred — do not touch it
without asking.

## Build

Windows, MSYS2 UCRT64. Release:

```bash
C:/msys64/usr/bin/bash.exe -c 'cd /e/Projects/NifskopeWildWastelandEdition && PATH=/ucrt64/bin:/usr/bin:$PATH make -f Makefile.Release -j2'
```

Output is `release/NifSkope.exe`. That is always the correct binary; do not test
against anything else.

**`-j2`, not `-j8`.** This machine has hard-shut-down under sustained all-core
load — Kernel-Power 41 with no bugcheck, which reads as a power or thermal
margin problem rather than a software fault.

**The generated `Makefile*` files hold absolute paths.** They are gitignored, so
a fresh clone is fine, but if the repository folder is ever moved or renamed,
re-run qmake before building or the old path comes back at you as a
file-not-found from the middle of a link.

**After changing the include graph or adding data members to a widely-included
class, re-run qmake before trusting an incremental build:**

```bash
qmake6 -o Makefile NifSkope.pro
```

A stale dependency list once linked an old-layout `.o` and hard-crashed at
startup (`0xC0000005` inside `QHash::findNode`). More generally: **rule out a
stale incremental build before bisecting any access violation or heap
corruption.** `make clean` first. Identical source has crashed 6/6 incremental
and 0/12 clean.

**This bit again on 2026-08-07** and cost an hour: three widely-included headers
gained members, a dozen incremental builds followed, and a harness started dying
half way through with "Free Heap block modified after it was freed". The code
was correct. Clean build, 25 of 25. Do the clean build *first*, not after
reading the diff four times.

**`make clean` breaks the next build**, so know this before you run it: qmake
writes the `icon_res.o` rule with an absolute target path and lists the object
with a relative one, so make stops at `No rule to make target
'GeneratedFiles/.obj/icon_res.o'`. Build it once by hand and carry on:

```bash
windres -i res/icon.rc -o GeneratedFiles/.obj/icon_res.o --include-dir=./res
```

**Set a writable temp when building from a sandboxed shell.** `export
TMPDIR=/tmp TMP=/tmp TEMP=/tmp` — otherwise g++ tries `C:\Windows` for its
intermediates and fails with "Cannot create temporary file". And piping make to
`grep`/`tail` hides its exit status: use `set -o pipefail` or check
`${PIPESTATUS[0]}`, or a failed build reads as a successful one.

Relink can also fail with the exe locked. Kill the straggler and relink; it is
not a code error.

### Version numbers — there are two, on purpose

| | where | what it is |
|---|---|---|
| `WW_VER` | `NifSkope.pro` | this fork's edition number (`0.3`) — title bar, About box |
| `VER` | `build/VERSION` | upstream lineage (`2.0.dev11`) |

`applicationName` stays `"NifSkope 2.0"` because it is the **QSettings key**;
renaming it would strand every existing user's settings. The edition name lives
on `applicationDisplayName`. `NifSkope::migrateSettings` compares against
`NIFSKOPE_VERSION`, so that one has to keep tracking upstream.

To cut 0.3: change `WW_VER` in `NifSkope.pro`. Nothing else.

### README.md is generated

`README.md` is produced at link time by `QMAKE_PRE_LINK` from
`build/README.md.in`, substituting `@VERSION@` and `@WWVERSION@`. **Edit the
`.in` file.** Editing `README.md` directly works until the next link, then
silently reverts.

## Layout

```
src/                    application
  glview.cpp            the 3D viewport — modes, modeling ops, selection, gizmos
  nifskope_ui.cpp       docks, toolbars, menus, workspaces, the WW_* harnesses
  gl/hknpdecode.cpp     compiled Havok collision, read
  gl/hknpencode.cpp     compiled Havok collision, written back
  uvtools.cpp           UV editing workspace
  unfucktools.cpp       Issue Manager
  nifcli.cpp            headless CLI
  wwskin.h              the palette — colours come from here, never literals
  ui/widgets/
    wwnumberfield.*     the one number field
    timeline*.cpp       animation timeline
    physicspanel.*      ragdoll / physics sim
lib/                    vendored deps (qhull, gli, meshoptimizer, libfo76utils)
tests/                  harness wrappers, one per feature area
tools/                  byte-level verifiers, corpus scripts, render regression
docs/                   plans, audits, research, CLI reference
```

No submodules. Everything is vendored, so `git clone` is complete.

Note: source comments cite plan docs by bare filename (`MODELING_TOOLS_PLAN.md`,
`TO_BE_IMPLEMENTED.md`, …). Those files now live under `docs/`. The names are
still unique — grep finds them.

## Testing

Harnesses are environment-gated code paths inside the real binary. Set the flag,
the app drives itself and writes `release/ww_*.log`, and a shell wrapper asserts
on it.

```bash
tests/spells/top_bar.sh
tests/spells/collision_undo.sh
tests/spells/scrub_uniform.sh
```

The collision and menu work added six:

| harness | covers |
|---|---|
| `collision_panel.sh` | the two-button split, both popups, the disabled-shape tooltip, and that every moved control still writes its key |
| `collision_per_shape.sh` | N selected shapes → N bodies, each on its own node, source meshes consumed |
| `quick_favourites.sh` | pinning, the Q menu, Space/Q scoping, the search menu reaching menu actions |
| `spell_search.sh` | the palette: dismissal, positioning, and not listing its own row |
| `nav_keys.sh` | rotate/zoom rebinding, and that letting go stops the camera |
| `collision_compiled_edit.sh` | editing a compiled body in place |
| `window_state_roundtrip.sh` | open, close maximised, open again — the startup crash |

And the block-list session added three:

| harness | covers |
|---|---|
| `block_dragdrop.sh` | 87 checks: that the drag starts at all, the three modifiers, reorder by the gap, drag-out, every refusal, multi-select as one payload and its ordering, the highlight and the painted insertion line, the drag card, auto-expand and its fold-back, paste following the pointer *in a second window*, blank-click deselect, one undo step. `WW_BLOCKDND_BENCH=<n>` also times a move on a file that size |
| `block_rename.sh` | 25 in hierarchy (list mode is out of the gate, see below): F2 and double-click, that nothing else opens on top, no sideways scroll, Escape, the column asymmetry, the txt icon, and that the name reaches the palette |
| `collision_drop.sh` | 7 checks: a mesh dragged onto the Collision Manager gets collision, at the shape type the panel is showing, one body per mesh — and check 2 asks whether the dock accepts drops at all, which is the only thing the harness steps over |
| `block_list_modes.sh` | 8 per mode: that the header's total matches the sections it totals, that every row resolves back to itself through `indexAt`, that a block inserted now is addressable, and that all of it survives switching modes |
| `block_drag_live.ps1` | **the only thing above the native-drag boundary** — drives the physical mouse across 7 drags: into a shut node, into a row its own auto-unfold revealed, into a second root, a root made a child, out to blank space, a mesh row's all-gap reorder, and a refused cycle. See the warning below. |

All three build their fixture from the starter document (`-no-gui new`), so they
need no game corpus at all. `block_rename.sh` and `block_list_modes.sh` seed
`List Mode` into the registry before launch and put it back on exit, because the
mode is read during window construction — the app has to *start* in the mode
under test, and both of them assert that it did rather than assuming.

**Never `ignore()` a drag event you mean to keep receiving.** Ignoring a
DragEnter or DragMove ends the drag over that widget — not one further event
arrives — so the first position the pointer happens to be at decides the whole
gesture. A drag begins on the row being dragged, whose neighbouring gaps refuse
as no-ops, so the block list's drag was dead before it began and stayed that way
through four wrong fixes. Accept the event and put the verdict in the drop
ACTION: `Qt::IgnoreAction` gives the no-drop cursor while the stream stays alive.

**A window that follows the cursor during a drag must be
`Qt::WindowTransparentForInput`.** Otherwise it takes part in hit-testing, and
the moment it passes under the pointer the view underneath gets a `DragLeave` and
stops receiving `DragMove` — so the follower freezes, the drop feedback stops
updating, and the stale caption it is left showing gets read as the program's
verdict on wherever the cursor is now. That arrived as three separate bug
reports: a stuck label, a line that never appeared, and legal drops "refused".

**Anything a drag draws must `repaint()`, not `update()`.** `QDrag::exec()` runs a
native modal loop; a posted update is coalesced and can sit in the queue until
the drag ends, so the paint lands after it stops being useful — indistinguishable
from never painting at all, and reported that way twice. No harness can catch it
either: a harness delivers drag events directly and is never inside the loop that
swallows the paint.

**A drag event cannot be delivered with `QApplication::sendEvent`.**
`QApplication::notify` routes drag and drop through the drag manager, so a
synthetic one reaches neither the widget's `event()` nor any event filter —
measured at zero, to the view and to the viewport both. That is why the drop
handlers are `NifTreeView` overrides and why `wwDeliverDragEvent` exists: a
harness needs an entry point that begins where Qt's routing ends. The override
count it reports is the check that the overrides ran, rather than the hook being
poked directly.

**There is a live-drag test, and it is the only thing above that boundary.**
`tests/spells/block_drag_live.ps1` drives the physical mouse at the block list on
the second monitor and reads `release/ww_drag.log` — which the program writes on
every drag, with no flag to set (`WW_DRAG_LOG=off` disables it, or a path
overrides it). It found in one run what four code-reading fixes missed while the
harness sat at 44 green.

**It SEIZES THE POINTER, so it is run by hand, never fired off.** Placement on
the second monitor keeps a *window* out of the way; the mouse is not per-monitor,
and clicks land wherever the cursor is dragged. It was once run mid-task and
disturbed the user's live session. Ask before every run, and if it has already
answered the question, do not re-run it to confirm.

**And that entry point is exactly how the drag shipped broken.** Driving the drop
handlers covers everything below `startDrag()` — and `startDrag()` was never
called, because `QAbstractItemView` will not enter `DraggingState` unless the
MODEL reports `Qt::ItemIsDragEnabled`. 26 checks green, feature dead. **When a
harness has to enter below the top of a mechanism, name what it stepped over and
cover that separately**: the flags on both models, `dragEnabled()` on the view,
and a real press-and-move. Swap the drag hook for a counting one first — the
production hook ends in `QDrag::exec()`, a modal loop that never returns with
nobody at the mouse.

**Two useful capture levers, both added while chasing things reading was not
finding.** `WW_CAMERA_LOG=<file>` appends every camera reorientation with its
rotation and view state — that is what finally located the startup view being
overwritten, after three carefully-read suspects each turned out innocent. And
`WW_RENDER_VIEW` now accepts a **negative** value, meaning "leave the camera
exactly as startup left it", which is the only way to photograph the startup
view: every other value overrides the thing under test. Its old upper bound was
`ViewWalk`, so asking for `ViewUser` was silently rewritten to Front and
produced a capture that looked like a passing test of a view it had never used.

`window_state_roundtrip.sh` is the one harness that **cannot** source
`_harness.sh`. `saveUi()` deliberately bails out on any `WW_*` variable so
harness layouts never overwrite the user's, and `WW_WINDOW_AT` is a `WW_*`
variable — so the flag that places the window off the primary monitor also
disables the write path the test exists to exercise. It seeds `UI/Window
Geometry` onto the second monitor instead, asserts the window landed there
before doing anything else, and restores the settings key on exit. If you write
another test that needs real settings written, it has the same problem.

`tests/spells/_harness.sh` holds shared setup. 60 flags exist; `grep -rhoE
'WW_[A-Z_]+_TEST' src/ | sort -u` lists them.

Three rules that were learned the hard way:

1. **Run only the harnesses whose code path the change reaches**, and say why
   each is in the list. A blanket run opens a dozen windows and tells you
   nothing about most of them.
2. **A harness must force the state it measures**, never inherit persisted
   `QSettings`. A green suite went red with no code change because
   `GLView/Enable Animations` was left `false` in the registry.
3. **Measure with an invariant that fails on broken code, and prove it fails.**
   A proxy number that merely agrees with correctness is not evidence. Where
   possible the check is committed *before* the fix, so its failure is recorded
   against the unfixed binary.

Three ways that rule got broken in one session, all worth knowing:

- **A threshold the wrong answer also clears.** "More than 20 materials" passed
  for two builds on Oblivion's 32 where Fallout 4 has 157. Name things that
  exist only in the right answer.
- **A check satisfied by the bug itself.** "The material can be typed" asked
  whether the field was editable — and the leftover `setEditable(true)` *was*
  the bug. Open the thing and look inside it.
- **A check that never ran.** Two green assertions about zooming, on a camera
  that had not moved: the pump spun `processEvents` for microseconds while
  `advanceGears` steps on real elapsed time, and the measurement read
  `cameraDistance()` when zoom changes `Zoom`. **Prove the control moves before
  asserting that a change moved it.**

And: **a check that fails intermittently is worse than no check**, because it
teaches you to re-run rather than to look. Poll for a condition with a deadline;
never sample once after a fixed delay.

`grabFramebuffer` does **not** repaint — pump `ogl->update()` +
`processEvents()` twice or you diff a stale frame.

## Landmines

**It will not start?** This was the long-running one, and it is fixed as of
`5983a97` — but the lesson it taught is wrong, so read the correction.

Clearing `UI/Window State` under `HKCU\Software\NifTools\NifSkope 2.0\UI` did
cure it, every time, which is why three sessions treated the saved blob as
corrupt. It never was. `restoreUi()` restored geometry before state, and
replaying a layout saved while **maximised** into a window that
`restoreGeometry()` had just flagged maximised, but that had not been shown yet,
faulted `0xC0000005` in `QLayout::addChildWidget`. Clearing the key worked
because an empty blob makes `restoreState()` a no-op — it removed the symptom,
not the cause. State is restored before geometry now.

**The real lesson: "clearing X fixes it" does not mean X was bad.** Hold one
variable at a time instead. The same 2090-byte blob crashes under a maximised
geometry and restores perfectly under a normal one; swapping only the geometry,
with the blob byte-identical, flips the outcome. That single comparison is what
broke it open after two sessions of bisecting the wrong thing.

Still true and still worth keeping: **a crash that survives reverting the change
that appears to cause it is not caused by that change** — check persisted state
before bisecting further.

**`QHeaderView` keeps its total by adding and subtracting, and a model change
desyncs it.** `length` is the sum of the sections, maintained by deltas rather
than re-derived. Hiding a section subtracts its width and remembers it; changing
the model gives every remembered width back **without adding it to `length`**.
Hide them again and each width comes off twice. The Block List hides 9 of the
NifModel's 12 columns, so after one load `length` was NEGATIVE — and
`visualIndexAt` returns -1 for anything past it, so `QTreeView::indexAt` had no
column and returned no index for any point in the view. Nothing in the flat list
could be clicked, dropped on or right-clicked, and it read as "the rows are
there but dead".

Two rules follow, and `block_list_modes.sh` guards both: **release the columns
before changing a view's model and apply them after**
(`wwReleaseBlockListColumns` / `wwApplyBlockListColumns`), and **a saved header
blob belongs to a model shape** — restoring one saved against the 3-column proxy
onto the 12-column NifModel desyncs the total the same way, so each mode keeps
its own.

**`NifTreeView::isRowHidden( int, const QModelIndex & )` is not
`QTreeView::isRowHidden`.** It shadows it with a different meaning: it ignores the
row number and answers for the item behind the index you passed as the *parent*.
Asking it whether a row is hidden returns something unrelated — under an invalid
root it is always false — and a check built on it measures nothing. Use
`visualRect().isEmpty()`, which is usually the real question anyway.

**"Selected" in the Block List is two things.** Qt's selection and current index,
and `NifModel::selHighlight` — mirrored from the 3D view's object selection, and
the one the row's COLOUR comes from. Clearing one leaves the other showing.

**An invalid root index means "show the whole model" to a QTreeView**, not "show
nothing". Block Details listed every block in the file the first time nothing was
selected.

**CRLF.** Four sources and one doc are CRLF: `src/glview.cpp`,
`src/gl/controllers.cpp`, `src/nifskope.cpp`, `src/spells/havok.cpp`,
`WW_CHANGES.md`. Python text-mode I/O flattens them into a 33,000-line junk
diff — **and so does `sed -i`**, which caught this twice in one session. Use an
editor that preserves them, or binary I/O, and check `git diff --numstat` before
committing.

**A disabled widget shows no tooltip.** It receives no mouse events, so Qt never
delivers it a `ToolTip` event and `setToolTip` on it displays nothing —
silently, in exactly the case where the explanation is needed. Put the text on
an enabled wrapper (`createShapeHost` in `collisiontools.cpp` is the pattern).

**`populatePhysicsEnums()` returns early before the physics editor exists.** It
guards on the editor's own combos, so calling it during construction to fill
create-side lists does nothing, leaving `currentData()` invalid and writing 0 —
which is `MO_SYS_INVALID`, `OL_UNIDENTIFIED` and friends. Fill those after the
editor is built, and again on `modelReset`.

**Collision tables are Fallout 4's, unconditionally.** `materialEnumType()` and
`layerEnumType()` used to walk the BS version down into Oblivion, and a BS
version of 0 — which is what you get before a file is open and from this
program's own new document — reached the bottom rung. Do not reintroduce a
ladder.

**`spRemoveBranch` reads the block-list selection.** Calling it to remove one
block removes the whole published multi-selection. `castCollisionOverSelection`
takes the selection down for the duration of a run for this reason.

**Scene node ids do not follow a renumber.** `Node::nodeId` is assigned when the
Scene builds it, so gathering geometry after an insert reads through a stale id.
Gather before mutating, or ask the `Node` for its own id.

**Colours.** Never introduce a colour literal. `wwSkinColor("name")` from
`src/wwskin.h`, or add a token there. The same discipline applies to the number
field: `wwMakeScrubField` / `WwNumberField`, never a sixth private copy of the
scrub gesture — that is exactly how the five that got deleted came to exist.

**Menus.** `QMenu` paints the icon and the check indicator in the *same* column,
so an icon-bearing checkable item silently loses its checkmark. Checked rows are
styled by fill (blue background, orange text) instead.

**Event filters run before the target's own handler.** Insetting a spin box's
line edit on the *host's* resize gets undone; the correction has to hook the
editor's own `Resize`.

**`QToolButton::sizeHint()` under-reports** on stylesheet-styled buttons, because
QSS padding is not folded in. Take the max of the hint and font-metrics + padding
or the label elides.

## Conventions

- Solo repo: commit straight to `main`. No branches, no PRs.
- **Update [WW_CHANGES.md](WW_CHANGES.md) with every change batch, unprompted.**
  Newest entry at the top, dated, with the measurement that backs it.
- When a feature's design is uncertain, look up Blender's equivalent and follow
  it. State deliberate divergences.
- Do GUI verification via the harnesses rather than handing over a checklist.
  In-game validation belongs to the user.
- Fallout 4 only. Fallout 76, Skyrim and Starfield paths are inherited, not
  maintained, and not to be worked on.
