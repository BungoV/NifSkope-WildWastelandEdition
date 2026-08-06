# Handoff — NifSkope, Wild Wasteland Edition

**Read this first.** It is the short document: where things are, how to build,
what will bite you. [WW_CHANGES.md](WW_CHANGES.md) is the detailed history,
[WW_FEATURES.md](WW_FEATURES.md) is what the fork adds, and
[docs/TO_BE_IMPLEMENTED.md](docs/TO_BE_IMPLEMENTED.md) is the single backlog.

Updated **2026-08-05** after the startup-crash and collision-authoring session.
Branch `main` at `0016f02`, build green, working tree clean, everything pushed.

Two headlines:

- **The startup crash is fixed** — `restoreUi()` restores state before geometry.
  It was never the saved layout. See Landmines.
- **NIFs open on Blender's three-quarter view**, the Collision Manager fits its
  own columns, and collision authoring gained saved presets, a row menu that
  creates, a Parent column and re-parenting.

### Open, and honest about it

- **Two things have no harness.** Preset save/rename/remove, and
  `reparentFromBlockList`. The preset "+" goes through a modal
  `QInputDialog::getText` and re-parenting is only reachable through a
  `QMenu::exec`, so neither is drivable the way the existing harnesses drive
  widgets — covering them means exposing the storage helpers and the operation
  behind test-only entry points first. Re-parenting writes `Children` arrays on
  two nodes, so it is the one to do first.
- **Re-parenting keeps the LOCAL transform**, so a body moves in world space
  when the new parent sits elsewhere. Right for attaching collision to a bone,
  wrong for tidying a hierarchy. Preserving world position was deliberately not
  decided.
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

All committed, all covered by harnesses, nothing half-landed.

- **Collision authoring, rebuilt around the block structure.** Two buttons in
  the order the blocks nest: **Create Collision Body…** (the object, the body,
  and every physics value) and **Create Collision Shape…** (shape type,
  material, replace), the second disabled until a body row is selected. The
  reasoning is in [WW_CHANGES.md](WW_CHANGES.md) — only `Material` is per-shape;
  everything else is in `bhkRigidBodyCInfo`, one block up.
- **Several selected shapes make several bodies**, each on its own `NiNode`,
  because a `NiAVObject` holds exactly one `Collision Object`. Measured against
  the corpus: 625 of 625 stock FO4 bodies target a `NiNode`, none a shape.
- **Converting a mesh to collision consumes the mesh.** Duplicate it first if
  you want it kept.
- **The Collision Manager lost about twenty controls** — display row to
  **Overlays ▸ Collision Display**, row operations to the list's right-click
  menu, creation into the two popups.
- **Quick Favourites** (Blender's): right-click any menu entry or search result
  to pin it, **Q** to open. **Space** in the viewport opens the search menu;
  playback moved to **Shift+Space**.
- **Viewport navigation is rebindable** — rotate, zoom and the fly keys are in
  Options ▸ Shortcuts like everything else.
- **NifSkope opens maximised.**

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
