# Handoff — NifSkope, Wild Wasteland Edition

**Read this first.** It is the short document: where things are, how to build,
what will bite you. [WW_CHANGES.md](WW_CHANGES.md) is the detailed history,
[WW_FEATURES.md](WW_FEATURES.md) is what the fork adds, and
[docs/TO_BE_IMPLEMENTED.md](docs/TO_BE_IMPLEMENTED.md) is the single backlog.

Updated **2026-08-07** after the block-list drag-and-drop session.
Branch `main` at `a901281`, build green, working tree clean, everything pushed.

Two headlines:

- **The Block List is a direct-manipulation panel now** — drag to re-parent,
  reorder and un-parent; paste follows the pointer; a click on blank space
  selects nothing. Details below.
- **The flat list mode is not sound** and is the top backlog item. Blocks
  inserted while it is showing are not addressable, and it intermittently takes
  the process down. Neither is drag-and-drop's or rename's doing — both were
  A/B'd out. Hierarchy mode is unaffected.

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

Four things, in the order they are worth doing:

1. **Multi-window breaks paste-follows-pointer.** Every window registers the
   hover probe into one slot (`setBlockListHoverProbe`), last one wins, and it
   declines when its own window is not active — so in a second document paste
   silently falls back to selection-based. Re-register on window activation.
2. **The live-drag script covers one scenario, and three more are written but
   UNVERIFIED.** `tests/spells/block_drag_live.ps1` is the only coverage above
   the native-drag boundary, where every one of this session's bugs lived. It
   seizes the mouse — see the warning below — so it needs a deliberate run.
3. **`wwParentsOf` is O(blocks) per moved block**, and the multi-block sort
   comparator calls `getParent` per comparison. Invisible on normal files;
   measure before caring, then cache the parent map for the duration of a call.
4. **The two list modes have drifted a long way.** Flat list has no reorder, no
   drag-out and no auto-expand — deliberate, since it is file order rather than
   anyone's children — but with item 0 (flat list not sound) unresolved it is
   worth deciding whether that mode is being kept at all.

### Open, and honest about it

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
- **`window_state_roundtrip.sh` no longer runs on this machine**, and it fails
  before launching the binary, so it is currently covering nothing. Two separate
  environment problems: `Add-Type` cannot write its temp file when PowerShell is
  launched from MSYS2 bash (run the script from PowerShell instead), and then
  its geometry-magic guard reads `0xCB` because `-shl` on a `[byte]` truncates
  in PowerShell 5.1 — `GetI` needs `[int]` casts. Small, but it is the startup
  crash's only net.
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

Edition **0.2**, on upstream NifSkope 2.0.dev11 (fo76utils `develop` @
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
| `WW_VER` | `NifSkope.pro` | this fork's edition number (`0.2`) — title bar, About box |
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
| `block_dragdrop.sh` | 82 checks: that the drag starts at all, the three modifiers, reorder by the gap, drag-out, every refusal, multi-select as one payload and its ordering, the highlight and the painted insertion line, the drag card, auto-expand and its fold-back, paste following the pointer, blank-click deselect, one undo step |
| `block_rename.sh` | 24 per mode: F2 and double-click, that nothing else opens on top, no sideways scroll, Escape, the column asymmetry, the txt icon, and that the name reaches the palette |
| `block_drag_live.ps1` | **the only thing above the native-drag boundary** — drives the physical mouse. See the warning below. |

Both build their fixture from the starter document (`-no-gui new`), so they need
no game corpus at all. `block_rename.sh` seeds `List Mode` into the registry
before launch and puts it back on exit, because the mode is read during window
construction — and because **switching it at run time is what the flat-list
fault takes down**.

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
