# Block List right-click menu — audit and overhaul plan, 2026-08-01

Eight-agent pass: three readers mapped the menu (how it is assembled, every
spell it can show, what the rest of the app offers for a selected block), four
lenses judged it (completeness, redundancy, taxonomy, search), one synthesis.
Line numbers were re-verified against the tree by the synthesiser.

Nothing here is implemented. This is the plan.

# Block List Right‑Click Menu — Overhaul Plan

Verified against the tree as it stands today: `NifSkope::contextMenu` at `E:\Projects\ClaudeNifskope\src\nifskope_ui.cpp:13274-13474`, `SpellBook` at `E:\Projects\ClaudeNifskope\src\spellbook.cpp` (364 lines; `cast` :110, `checkActions` :195, `newSpellRegistered` :217, Transform/Block hoist :243-259), `Spell` at `E:\Projects\ClaudeNifskope\src\spellbook.h` (`page()` :71, `hint()` :73, `constant()` :77, `undoable()` :79, `batch()` :87). Where the three maps disagree, the fourth reviewer's corrections are right and I have adopted them: Flatten Branch (non‑recursive) and Simplify are *not* pageless, Optimize Indices *does* appear on block rows.

## 1. The Search Field

**Do not put a QLineEdit in the QMenu. Build a command palette.** A popup QMenu holds a keyboard grab, so a hosted line edit only receives keys the menu chooses to forward, `QMenu::keyPressEvent` otherwise spends every printable key on first‑letter jump, click‑to‑focus through a `QWidgetAction` is the flakiest interaction in Qt, a filtered menu cannot expand a submenu inline (so hits must be hoisted with proxy actions), and the popup is anchored where it was `exec`'d so it cannot grow or re‑centre. Every one of those is novel code fighting the widget. The palette shape already ships twice in this repo — `wwFlagListDialog` (`src/spells/flags.cpp:196-320`) and `pickNifFromBrowser` (`src/nifskope.cpp:5081-5104`).

**The design.** A frameless modal `QDialog` (`WWSpellPalette`), 640×420, centred on the main window: one `QLineEdit`, one `QTreeView` over a `QStandardItemModel` + `QSortFilterProxyModel`, three columns — Entry, Group, Shortcut — and a one‑line description strip at the bottom fed by `Spell::hint()`.

**How it gets its rows.** It does not read the spell registry. It walks the `QAction` tree of a live, already‑built `SpellBook`, so the four hand‑added native actions (Set/Clear Parent, Transfer Normals from N Selected, the diff/paste trio) are searchable for free, and each row's payload is just the `QAction*`; choosing a row calls `action->trigger()`. Lifetime is handled by not opening the palette from inside the menu: the "Search…" row sets a bool, `contextBook.exec(p)` at `:13473` returns, and the palette is opened afterwards with `contextBook` still alive on the stack.

**Two entry points.** A `Search…` row as the first action of the Block List menu (`Ctrl+Shift+P` shown in its label), and a window‑level `QAction` on the same shortcut that opens the palette against the Block List's current index without a right‑click at all. The second one is the actual speed win; the menu row is discoverability.

**Matching.** `simplified().split(' ', SkipEmptyParts)`, every term must appear, case‑insensitive substring — verbatim `applyBlockListFilter` semantics (`nifskope.cpp:2763-2786`), not the un‑split `contains` in `flags.cpp:254`. The searchable key is label + group title + shortcut text + `hint()`, with everything after `'\t'` stripped from the label first (the Hierarchy labels embed their shortcut in the action text). Including the group title is what resolves `Copy`/`Paste`/`Choose`/`Edit`/`Material` colliding across pages — "block copy" and "transform copy" become distinguishable queries.

**Inapplicable entries are shown, greyed, at the bottom**, with the Group column intact. This is the palette's whole advantage over a filtered menu: the single biggest confusion in this menu is that Batch, Sanitize, Optimize and Error Checking silently vanish on a block row by design (`spellbook.cpp:198-214`), and a search that says "Update All Tangent Spaces — Batch — not applicable to this block" answers the question a "no results" row cannot.

**Keyboard.** Field has focus on open. Up/Down move, Enter triggers the highlighted row, Esc clears the field and only closes when it is already empty (mirrors the Block List's Esc binding at `nifskope.cpp:1038-1040`). The top hit is auto‑highlighted after each keystroke **unless it is marked destructive**, in which case nothing is highlighted until you arrow onto it — two keystrokes and Enter must never reach Crop To Branch. Disabled rows cannot be highlighted.

**Cost of choosing the palette:** no type‑to‑filter in place; one extra keystroke to open; a second widget to maintain. Accepted — it is boring code, and it is the only version that can show what is *not* available.

**Verification:** `WW_SPELLSEARCH_TEST` harness following the existing convention (`WW_ISOLATE_TEST` at `nifskope_ui.cpp:893`): open at a known block, type, assert the hit list, press Enter, assert the right spell cast and that a destructive row was not auto‑highlighted.

## 2. What to Add

Ordered by value. I rejected more than I accepted; see the end of this section.

- **`Delete N Block(s)`** — flat verb row, calls `ogl->deleteBlocksWithConfirm(QVector<int>)` (`glview.cpp:10422`) with the block‑list selection. This is the single biggest gap: multi‑select, branch closure, dangling‑parent pruning, one confirmation, one undo step, and today it is reachable only from the viewport's `Object ▸ Delete` / `X`.
- **`Select & View` submenu** — `Select Branch`, `Select Same Type`, separator, `Frame in Viewport` (`GLView::frameSelected`, `glview.cpp:4772`), `Hide` (`hideSelected` :19087), `Isolate Selected` (:6808), `Restore All Hidden` (:6920). All six run off `ogl->objSelection`, which `wireBlockListSelection` (`nifskope.cpp:2663-2719`) already fills from the list selection. The two Select entries exist because three entries in this menu consume a multi‑selection and nothing in it can build one.
- **`Compile Collision`** and **`Check Collision`** in the Collision group. Decompile is currently a one‑way trip from this menu. Implement by hoisting `compileSelectedCollision` / `lintCollision` (`collisiontools.cpp:1622` / `:1726`, both private dock members) into free functions in the same file — exactly the shape `tlOpenMaterialEditor` already has at `meshtools.cpp:719` — and registering two spells over them.
- **`Edit Material…`** and **`Select All Geometry Using This Material`** in the Material group, wrapping `tlOpenMaterialEditor` (`meshtools.cpp:719`) and the dock handler at `meshtools.cpp:1777`. Right‑clicking a shape currently offers six ways to check and copy a material and no way to open it.
- **`Export .OBJ…`**, **`Export .glTF…`**, **`Import .OBJ as Collision…`** in the Import/Export group. Both exporters are already block‑scoped (`obj.cpp:470-502` prints a message box whose only job is to tell you which block it guessed; `gltf.cpp:1320` refuses without a selection) yet live only in File ▸ Export.
- **`Take All Reference Values (N)`** in the trailing diff group, calling `wwTakeAllReferenceValues()` (`nifskope.cpp:3442`, takes no index). It is offered today only when you right‑click a *differing field* (`nifskope_ui.cpp:13426`), not from the list where you choose which block to compare.
- **`Pin This Block` / `Unpin This Block`** in the trailing group, against `blockListPins` (`nifskope.cpp:920-927, 1002-1010`). The ★ button and the pin store live on the Block List; the only Pin menu entry lives on the other widget.
- **`Open in Collision Manager` / `Open in Material Manager` / `Open in Rigging`** — **one dynamically‑titled row**, resolved from the clicked block's type, not three rows. Navigation is one‑directional today (`collisiontools.cpp:2484`, `meshtools.cpp:1631-1636` both jump *out* to the Block List).
- **`Rename…\tF2`** in the flat verb row, invoking `renameBlockListIndex` (`nifskope.cpp:3697`) — the path F2 and double‑click already use, which has no menu entry, while the discoverable route is the modal spell.
- **`Join Selected Shapes`** in Geometry, enabled on 2+ selected geometry rows (`joinSelectedObjects`, `glview.cpp:10171`). Low priority; ship last.
- **`contextBook.setToolTipsVisible(true)`** — one line, and the four hand‑written tooltips at `:13314, :13318, :13376, :13438` start rendering. Confirmed absent: only `glview.cpp:8511`, `nifskope.cpp:1876/2043/2131/2511` and `nifskope_ui.cpp:11031` call it.

**Rejected on the "longer without being faster" test:** `Assign to Selected Shapes` (Paste Material Setup already does this from the same menu); `Expand Branch` / `Collapse Branch` (QTreeView already does it with Right/Left and `*`); `Snap…` and `Set Origin…` (viewport‑modal, need the gizmo); `Create Editable Mesh Copy` and `Mass from Material…` (dock‑only is correct — they belong to a workflow, not to a block).

## 3. What to Remove or Demote

- **The self‑disabling confirmation.** `spellbook.cpp:126` fires the generic "cannot be undone" prompt for nearly every write, because `undoable()` is overridden `true` by six spells in the whole tree (all in `riggingtools.cpp`). Ticking "Do not ask me again" writes `Settings/Suppress Undoable Confirmation` and from then on Crop To Branch, Remove Branch, Convert and Apply Transformation run silently. **Fix by inverting it:** add `virtual bool destructive() const { return false; }`; drop the generic prompt for non‑destructive spells entirely; give destructive ones a specific, **unsuppressible** confirmation naming what is lost ("Delete 214 of 217 blocks?"). Marked `destructive`: Crop To Branch, Remove Branch, Remove, Flatten Branch (both), Convert, Apply Transformation. This is the highest‑severity item in the whole plan and it is independent of everything else.
- **`Crop To Branch`** — keep, but move to the bottom of the Block group behind a separator, rename `Crop File To This Branch…`, and give it the count‑naming confirm above. It currently sits two rows from Copy with no guard of its own (`blocks.cpp:1847-1899`).
- **`Enforce Node Name Authority`** (`meshtools.cpp:2116`) — remove from the context menu; menubar/Unfuck only. Its `isApplicable` does not even name the index parameter; it appears on every block row and rewrites every palette entry in the file.
- **`Decompile All Compiled Collision`** (`havok.cpp:1958`) — remove from the context menu for the same reason: whole‑file action offered from one block. Keep the single‑block `Decompile Compiled Collision`.
- **`Convert`** — gate `isApplicable` on `nif->isNiBlock(index)`. Today it is `Q_UNUSED(nif); return index.isValid();` (`blocks.cpp:1902`), so it shows on every field row and no‑ops there.
- **Havok `Create*`: eight entries → four.** Keep `Create Collision…`, `Create Box/Sphere/Capsule Collision`. Delete `Create Convex Hull Collision` and `Create Convex Decomposition Collision` (`havok.cpp:914/928`) — they are `spCreateCVS` with `Enable CoACD` forced false/true into **persistent** QSettings, so picking one silently reconfigures `Create Convex Shapes` and the Collision Manager forever. Fold the CoACD choice into the `Create Collision…` dialog as a checkbox; `Create Convex Shapes` and `Create Accurate Mesh Collision` remain reachable through that chooser.
- **Transfer Normals, twice.** Keep the hand‑added `Transfer Normals from %n Selected…` (it can carry a count; a spell cannot, because `checkActions` matches on `name()` at `spellbook.cpp:205`) and make `spTransferNormals::isApplicable` return false when `blockListSelection` holds the clicked block plus ≥1 other geometry block. One entry visible at a time, both code paths retained.
- **`Fix Bip01`** (`skeleton.cpp:34`) and the dead `spScanSkeleton` class (`skeleton.cpp:205`, registration commented out at :235) — delete. The spell reads a `skel.dat` no shipping build can write, and is gated to NIF 4.0.0.2 anyway.
- **`File Offset`** — demote from unconditional top level into `Info`. Keep it: format RE is half of what this codebase does.
- **Loose `Material` action** (`spMaterialEdit`, `materialedit.cpp:95`) — rename `Edit NiMaterialProperty…` and give it a group; it currently shares a label with the Material submenu a few rows away.
- **Renames:** `A -> B` → `Recompute B Frame from A` (`havok.cpp:1209`); `Fix Geometry Data Names` → `Zero Geometry Group ID` (`fo3only.cpp:12`, it sets Group ID to 0 and renames nothing).
- **Stray separator:** `nifskope_ui.cpp:13434` adds a separator before knowing whether any of the three actions under it apply. Build the actions into a local list first, add the separator only if non‑empty.
- **Dead wire:** remove the `kfmtree` → `contextMenu` connect at `nifskope.cpp:1472`; the slot returns for any sender that is not tree/list/header (`:13288`).
- **Unsure — confirm with bungo before doing:** demoting the four numbered Rigging steps (`1. Generate CustomizationRemapData` … `4. Transfer Weights`) to the dock only, keeping `List Skin Bones`, `Compare Bones`, `Validate FO4 Skin`, `Create Skin`, `Rebind`, and the atomic `Transfer Bones and Weights…` in the menu. The dock numbers and gates them; the flat submenu lets you run step 4 before step 1. But these are his own spells and he may drive them from here by habit.
- **Unsure:** unifying the two rename implementations (`spRenameNodeSynced`'s `tlPropagateNodeName` vs the inline editor's `propagateSceneObjectName`). Worth doing, but they are two different propagation bodies and merging them needs its own test.

## 4. The New Taxonomy

**Mechanism first, because this is what makes it implementable.** `page()` is overloaded five ways — submenu label, CLI namespace (`nifcli.cpp:197`), Unfuck membership (`nifskope_ui.cpp:11157`, `unfucktools.cpp:463`), undo‑prompt exemption (`spellbook.cpp:126`, `page() != "Array"`), and a model‑signal switch (`spellbook.h:87`, `batch()` returns true for pages Batch/Block/Mesh, and `spellbook.cpp:134` puts the model into `Processing` when it does). Editing `page()` strings to fix the menu would silently change four unrelated behaviours. So: **add `virtual QString group() const { return page(); }`**, build submenus from `group()`, leave `page()` frozen as the internal id. And replace the Transform/Block hoist (`spellbook.cpp:243-259`) with one explicit ordered `QStringList` that the same remove‑then‑insert logic walks — the mechanism already exists, it just needs generalising from two hardcoded titles to a declared list. Without that table the order stays an artefact of link order in `NifSkope.pro:305-337`.

Top to bottom on a Block List block row:

**`Search…`  Ctrl+Shift+P** — first action, then a separator.

**Flat verbs, no submenu** — `Copy Branch` (Ctrl+C), `Paste Branch` (Ctrl+V), `Duplicate Branch` (Ctrl+D), `Delete N Block(s)`, `Rename…` (F2). These five are the author's own frequency signal: they are where the hotkeys are, and today all of them are two levels deep. Separator.

**Then, in this exact order:**

1. **Block** — Insert, Remove, Copy, Paste, Paste Over, Move Up, Move Down, Convert Block Type…, Flatten Branch, Flatten Branch (non‑recursive), Sort Children By Name, Replace References With…, separator, Crop File To This Branch…
2. **Hierarchy** — Set Parent…, Clear Parent…. Stays directly under Block; that placement is deliberate (`nifskope_ui.cpp:13320-13337`) and correct.
3. **Add** (was Node) — Attach Node, Attach Effect, Attach Property, Attach Extra Data, Attach Parent Node. Only two of the old Node entries needed a NiNode; the page name was wrong and Rename has moved to the verb row.
4. **Transform** — Apply, Clear, Copy, Paste, Edit, Scale Vertices. Unchanged.
5. **Select & View** — Select Branch, Select Same Type, separator, Frame in Viewport, Hide, Isolate Selected, Restore All Hidden.
6. **Geometry** — Flip UV, Flip Faces, Flip Normals, Face Normals, Smooth Normals, Transfer Normals…, Edit UV, Prune Triangles, Remove Duplicate Vertices, Remove Unused Vertices, Simplify, Generate LODs, Join Selected Shapes, separator, Stripify, Triangulate, Stitch Strips, Unstitch Strips.
7. **Recompute** — Update Bounds, Update Bounding Sphere, Update Tangent Space, Generate Meshlets and Update Bounds, Optimize Indices, Update Triangles From Skin, Update MOPP Code. Everything you run *after* an edit, split out of the 26‑entry Mesh page.
8. **Skinning** (was Rigging + Skeleton + two strays from Mesh) — List Skin Bones, Validate FO4 Skin, Compare Bones with Donor…, Preview Transfer from Donor…, separator, Transfer Bones and Weights…, Create Skin (bind to node)…, Rebind to Reference Skeleton…, separator, Make Skin Partition, Fix Bone Bounds, Mirror armature.
9. **Material** (absorbs Texture, Shader, Color) — Edit Material…, Choose Material…, Check Material, Compare with BGSM/BGEM, Sync Shader Property from BGSM/BGEM…, Fill BSShaderTextureSet from BGSM, Copy Material Setup, Paste Material Setup, Select All Geometry Using This Material, separator, Copy JSON to Clipboard, Clone and Copy to Clipboard, Save as New…, Save Edited Material…, separator, **Textures ▸** (Info, Export, Embed, Export UV Layout, Add Flip Controller, Edit Flip Controller, and the nine legacy `Add … Map` NiTexturingProperty spells, which can never fire on a Bethesda file from Skyrim on).
10. **Collision** (was Havok) — Create Collision…, Create Box Collision, Create Sphere Collision, Create Capsule Collision, separator, Compile Collision, Check Collision, Decompile Compiled Collision, separator, Recompute B Frame from A, Calculate Spring Length, Pack Strips, Convert to bhkListShape, Convert to bhkConvexListShape.
11. **Animation** — Setup Controllers…, Remove From Animation…, Duplicate Sequence…, Scale Sequence Times…, Bake B‑Spline To Keys…, Generate BSPositionData, Convert Quat‑ to ZYX‑Rotations, Replace Entries (the ex‑String Palette single‑entry page). `Fix Invalid AV Object Refs` moves to Fix; `Attach .KF` moves to Import/Export.
12. **Flags** — Flags…, Vertex Flags…, Copy Flags, Paste Flags. Four items already adjacent at top level with no page; one `group()` line each.
13. **Import / Export** — Export .OBJ…, Export .glTF…, Import .OBJ as Collision…, Attach .KF…, separator, Convert to Internal Geometry, Convert to External Geometry, Save As .mesh, Choose .mesh File…, separator, Export Binary, Import Binary.
14. **Fix** (was Sanitize + Error Checking + Optimize) — block‑scoped members only: Combine Shapes, Zero Geometry Group ID, Fix Invalid AV Object Refs, Sort Keys. The whole‑file members keep their `page()` and stay in the menubar and the Unfuck dialog, which already merges the three by `page()=="Sanitize" || sanity() || checker()`.
15. **Info** — Referenced By, File Offset, Texture Info. **Only these three**; the other 34 `constant()` spells stay where they are and get a read‑only marker in their icon rather than being duplicated into a second home. Duplicating them would make the menu longer, which is the thing to avoid.
16. **`Open in <Manager>`** — one row, dynamic title.

**Separator, then the Block‑List‑only trailing group** — Pin This Block / Unpin This Block, Set as Diff Reference, Clear Diff Reference (n), Take All Reference Values (N), Paste `<field>` to N Block(s). Separator added only if at least one of these exists.

**Gone as categories:** Batch (a scope, not a category — and it never appears on a block row anyway; two verbatim label collisions with Mesh and nine "…All" twins), Node, Havok, Skeleton, Rigging, Texture, Shader, Color, Bounds, Morph, Header, Footer, String Palette, Array, Optimize, Error Checking, Sanitize. Header/Footer `Update` and the field‑scope one‑entry pages (Bounds ▸ Edit, Morph ▸ Save Vertices To Frame, Array ▸ Update/Move) become flat actions in the Block Details menu where they actually fire — a submenu that can only ever hold one item costs a hover for nothing. That takes the block‑row top level from 46 entries to 16 submenus plus 5 verbs plus the trailing group.

## 5. Order of Work

Each batch is one commit and leaves the app shippable.

**1 — Safety and free wins.** `Spell::destructive()`, unsuppressible specific confirmation for destructive spells, generic prompt dropped for the rest. Gate `spConvertBlock::isApplicable` on `isNiBlock`. Conditional separator at `:13434`. `setToolTipsVisible(true)` on the SpellBook. Remove the dead `kfmtree` connect. Rename `A -> B` and `Fix Geometry Data Names`. No taxonomy change, no new rows. Ship this first even if nothing else lands.

**2 — Multi‑select correctness.** `spRemoveBranch::cast` and `spDuplicateBranch::cast` read `blockListSelection` the way `spCopyBranch` does (`blocks.cpp:1430-1447`). Add `Delete N Block(s)` on `deleteBlocksWithConfirm`. Make `spTransferNormals::isApplicable` stand down when the selection‑driven action is showing. This is where the menu stops lying about scope.

**3 — Grouping mechanism, no membership changes.** Add `Spell::group()` defaulting to `page()`. Replace the Transform/Block hoist with the ordered `QStringList` and drive the whole top level from it. Menu contents identical; order now declared. Prove it by reordering `SOURCES` in `NifSkope.pro` and observing the menu does not move.

**4 — The taxonomy.** Assign `group()` to every spell including the 20 pageless ones, fold the eight one‑entry pages, rename Havok→Collision and Node→Add, split Mesh into Geometry/Recompute, merge Rigging+Skeleton into Skinning and Texture/Shader/Color into Material, merge Sanitize/Error Checking/Optimize into Fix. Collapse the Havok `Create*` family from eight to four. Drop `Enforce Node Name Authority` and `Decompile All` from the context menu. Delete `Fix Bip01` and `spScanSkeleton`. Fix the colliding leaf labels (`Choose Material…`, `Choose .mesh File…`, `Update Array Size`, `Copy Transform`, `Edit NiMaterialProperty…`). Biggest diff, zero behaviour change beyond the deletions.

**5 — Verb row and trailing group.** Hoist the five verbs flat, add `Rename…\tF2`, add `Pin This Block` and `Take All Reference Values (N)`.

**6 — New capabilities.** `Select & View` submenu, `Import / Export` rows, `Edit Material…` + `Select All Geometry Using This Material`, `Compile Collision` + `Check Collision` (hoisting the two dock members to free functions), `Open in <Manager>`, `Join Selected Shapes`.

**7 — The palette.** `WWSpellPalette` dialog, the `Search…` row, the `Ctrl+Shift+P` window action, `Spell::hint()` seeded from the 16 Unfuck blurbs at `nifskope_ui.cpp:11085-11118`, and the `WW_SPELLSEARCH_TEST` harness. Last, because it is far more useful over a taxonomy that is already correct — and because if batches 3–6 land, the palette is a convenience rather than a rescue.

**8 — Deferred, ask first.** Demoting the numbered Rigging steps to the dock; unifying the two rename propagation paths.