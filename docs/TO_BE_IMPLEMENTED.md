# NifSkope — WW Edition: To Be Implemented

## ~~Live decimation has no operator panel~~ — DONE 2026-08-08

**Decimate** is an object-mode operator now: `Ctrl+Shift+D`, or the object menu
beside Join. It halves the selection to start with and arms the redo panel, so
the ratio is scrubbed afterwards with the operation re-running underneath — the
same shape as Merge by Distance.

The simplification is the Simplify dialog's own, driven headlessly, so there is
one decimation in the program and not two that drift. Two of its persisted
settings are overridden for the live path and it is worth knowing why: **Min
Triangles** and **Max Error** both pin the triangle count regardless of the ratio
asked for, which is reasonable for a modal where a human types a number once and
nonsense for a field being dragged. Measured before that: a 224-triangle sphere
asked for half came back with 224.

Checked in `block_dragdrop.sh` on a UV sphere the harness makes for the purpose —
the fixture's 12-triangle cube has nothing redundant to remove and a simplifier
correctly refuses it, which looked exactly like a broken operator for two runs.

## ~~The Block List can drag 71 block types of 563~~ — FIXED 2026-08-08

**Blocks held by a typed link move now.** Dragged out, the owner's field is
cleared; dropped on a block with a field for it, the field is written, replacing
what was there. The field is chosen by scoring what the format declares rather
than from a table — see `wwFieldAccepting`, and note the `Skin` trap recorded in
the change log: taking the FIRST field that would accept a block is wrong,
because some Refs declare a type broad enough to accept anything.

Checked in `block_dragdrop.sh` with a texture set and a shader property, in both
directions. What remains of the original entry is below, for the reasoning.

## The Block List can drag 71 block types of 563 — was OPEN, 2026-08-07zb

Found by trying to drag a `BSLightingShaderProperty` out of its parent, which is
not about those two types at all.

`wwReparentBlocks` models parenting as the **`Children` array**, and only an
`NiAVObject` can be in one — so those are the only blocks that can move. Counted
against `nif.xml`: **71 movable, 492 not**. The 492 include every property
(`BSLightingShaderProperty`, `BSEffectShaderProperty`, `NiAlphaProperty`),
`BSShaderTextureSet`, every `NiExtraData` (`BSXFlags`, `NiStringExtraData`),
every `NiTimeController`, `NiSkinInstance`, all the collision blocks, and every
geometry data block.

**The refusal message is also wrong**, which is the part that reads as a bug
rather than a limit. `wwParentsOf` only scans `Children` too, so dragging a
shader property to blank space answers *"is already a root — it has no parent to
leave"*. It has an owner; the owner points at it through `Shader Property`. The
refusal for dropping it INTO a node ("is not a scene object") is at least honest.

**What it needs**: typed links. A texture set dropped on a shader property sets
that property's `Texture Set`; a property dropped on a shape sets its `Shader
Property`; an extra data dropped on any `NiObjectNET` joins its `Extra Data
List`. That is a table of (child type → owner field), and the drag consults it
when `Children` does not apply. `NifModel::getChildLinks` already gives the
general "what does this block own" relation, which is the other half.

Two cheap things worth doing first, whatever is decided about the feature: make
`wwParentsOf` tell the truth for blocks held by a typed link, and say *"nothing
can re-parent a %1 yet"* rather than claiming it is a root.

## Compiled collision: what is still missing — UPDATED 2026-08-17

~~The 128-triangle cap~~ and ~~the conservative zero bounds~~ both closed
2026-08-17: Elric reference pairs (decompile a vanilla mesh, perturb one vertex
with nifskope-cli set, recompile, diff) gave up the section-tree codec (5-byte
nodes, leaf high byte = section index) and the bounds encoding for BOTH trees
(sqrt-scale nibbles, hierarchical against the parent's dequantized box). The
writer emits real bounds and multi-section packfiles to 511 sections; a
60-mesh sweep including 13-section / 1,660-triangle output is byte-stable with
every tree walking every primitive exactly once. See WW_CHANGES 2026-08-17.

Still open, none of it blocking:

**0. ~~Multi-material compile~~ DONE 2026-08-20.** The run byte-format was
pinned against the vanilla corpus instead of an Elric fixture — 2,490 meshes,
3,898 sections, 9,536 run records — and per-triangle materials now run all the
way from each leaf shape's `Material`, through `CollisionMesh`, into the CMSD
run table and an N-entry hknpBSMaterialProperties (stride 0x18, not 0x20). It
also fixed a live bug in every multi-section packfile we had written: the
literal `1` at section +0x54 pointed every section at run 0. Details in
WW_CHANGES 2026-08-20; guard in `tests/spells/collision_materials.sh`;
independent byte checker in `tools/hkmatrun.py`.

**0b. ~~Decompile flattens a multi-material MESH~~ DONE 2026-08-21.** The other
half of the same round trip, and it is closed: `buildShape` groups the triangles
by the material the run table gives them, writes one `bhkNiTriStripsShape` per
group and gathers the groups under a `bhkListShape`, with the MOPP on top — the
chain the gatherer already walks, so Compile picks the materials straight back up
off the leaves. Toilet01's three come back as three with vanilla's own 46 / 10 /
148 triangles on them, and CeilingFanOff01's six come back as six. A
single-material mesh is byte-identical to what it was: one group takes the whole
vertex array in its original order and skips the remap. See WW_CHANGES
2026-08-21 and `tests/spells/collision_material_roundtrip.sh`.

**1. triangleIsInterior semantics — SHARPENED 2026-08-20, still open.** The
"767 of 867" above was an indexing artifact: the field is in KEY SPACE like
quadIsFlat, at twice its resolution, `(section << primBits) | (2 * prim + half)`.
Read that way the necessary condition is EXACT — **3,037 of 3,037 set bits over
875 meshes sit on a triangle whose three edges are all shared, no exceptions.**

Not sufficient, and not local geometry: 37,508 triangles qualify and only 3,037
are flagged, and the best rule tried reaches F 0.42 (table in WW_CHANGES
2026-08-20c, reprintable with `tools/hkinterior.py --rules`). Ruled out at zero
or near-zero: second-half-of-a-quad, buried inside another connected component,
inward-facing normal, coincident twin, junction vertex, and every
convex/concave/dihedral cut. Every simple closed mesh in the corpus — the 12- and
16-triangle boxes and ducts — carries NO interior bits, so it does not mean
"closed surface" either.

Zero stays, and it is the safe direction: a set bit is a strict subset of
fully-shared triangles, so under-flagging can only add collision work, never skip
a collidable edge.

**The Elric oracle now runs from one command** — `tools/elric_pair.sh`, against
the install at `X:\Programs\Steam\steamapps\common\Fallout 4 1946160\Tools\Elric`
— so this is narrowed rather than blocked. What that harness added on 2026-08-21:

- The bit IS derivable from a NIF: a decompiled ACDuctSmEnd01 recompiled by Elric
  comes back with three interior bits, the count vanilla ships, deterministically.
- It is NOT a function of local geometry: on that same geometry Elric flags three
  DIFFERENT triangles than vanilla, the symmetric partners of them. Candidates sit
  in near-ties, which is why no predicate has ever fitted.
- It is quad-ORDER dependent: over 400 meshes, half 0 carries the bit 553 times,
  half 1 only 77, both halves 52. On the duct every flagged triangle is a half 0
  whose partner is not flagged, prim9 included — and prim9's two halves have the
  same edge signature.
- Also ruled out: the sharper half of the quad (221 flagged halves are the
  BLUNTER one against 157 sharper), anything keyed on quadIsFlat, and "one flat
  edge plus two concave" (fits all six duct triangles, F 0.23 over the corpus).

Next attempt starts by perturbing the duct one thing at a time and diffing — the
lever that is missing is a way to edit mesh geometry headlessly, since
`nifskope-cli set` takes scalars and not a Vector3.

**2. ~~Quad pairing~~ DONE 2026-08-17d.** Elric merges adjacent triangle pairs into quads — bent
ones included (89% of corpus primitives are quads; quadIsFlat = "the four
corners are coplanar", measured clean at 2,965 quads). Pairing in the writer
doubled per-section capacity from 128 triangles to ~256 and halved
packfile sizes. The decode convention for a quad (a,b,c,d) is (a,b,c)+(a,c,d).

**3. ~~Convex sources compile to polytopes in Elric~~ SINGLE SHAPE DONE
2026-08-20.** Filed as a dynamic-only concern; it is not. Measured over 1,500
SetDressing files, 228 static systems are polytope-only and 733 are
compressed-mesh-only, so the compiled class follows the SOURCE and not the motion
type. A body whose leaves are all convex — box, hull, sphere, capsule, wrappers
walked and transforms baked — now compiles to convex shapes either way, and
anything else falls through to the mesh as before. Mass properties are derived
from a measured model (Minkowski-grown volume, grown-hull inertia, vanilla's
dominant major-axis word); see WW_CHANGES 2026-08-20d and
tests/spells/collision_convex.sh.

**3b. Compounds: several convex shapes in ONE body.** What is left of item 3, and
it needs a decode, not a routing change. `hknpEncodeSystem` refuses a compound
whose `dataRawData` is empty, and nothing has ever written an
`hknpDynamicCompoundShapeData` from scratch — so a multi-shape convex body still
compiles to a mesh.

The structure is already measured and it is small: a 0x60 header with 2n+1 at
+0x18, 2n at +0x20, the instance count at +0x28 and a 1 at +0x30, then **2n
records of 32 bytes** from +0x60 — an AABB pair whose last four bytes are two
u16, which reads as a BVH over the instances. The sizes confirm it exactly:
224 bytes for two children, 288 for three, 352 for four, 416 for five, 480 for
six, i.e. `0x60 + 2n × 32`. What is not known is what the two u16 mean (child
links, or a leaf index and an escape), which one Elric pair or a careful read of
three vanilla compounds would settle.

**4. ~~Friction f16: Elric rounds, we truncate~~ DONE 2026-08-20.** Settled by
counting the corpus rather than by an Elric pair: across 1,500 SetDressing files
every discriminating value is the ROUNDED word and none is the truncated one
(restitution 0.4 is 0x3ECD 1,623 times and 0x3ECC never). Since 0.4 is the
Fallout 4 default, every body we compiled was one ULP low. `roundFloat16`,
guarded in collision_compile.sh.

The Elric harness (batch.esf + fixtures.ps1, hands-free desktop run) and the
verification suite (sectree_verify.py, flatbits.py, interior.py) live in the
session scratchpad; the harness pattern is documented in WW_CHANGES 2026-08-17.

## ~~Creating collision on a node that already has it REPLACES it~~ — FIXED 2026-08-20

**Create adds beside now.** `CollisionManager/Create/Replace` defaults to false,
so a second Create on a node wraps both shapes in a `bhkListShape` — which is
what `tlMoveCollisionShape` and the drag-and-drop path already did, so the button
agrees with the rest of the program instead of quietly disagreeing with it.
Replace is still one setting away, and when it is on the question is put once per
Create, defaulting to keeping both.

The dialog also could not be shown headless — a `-no-gui` build is a
QCoreApplication, where constructing a QMessageBox aborts the process — so a
headless caller with Replace turned on now takes the non-destructive answer
instead of dying.

What follows is the original entry, for the reasoning.

## Creating collision on a node that already has it REPLACES it — was OPEN, 2026-08-07x

Found by the body-targeted drop check, which spent its first runs measuring this
instead of itself.

Create attaches to the mesh's parent **node**, and a node holds one
`bhkCollisionObject`. So creating collision from a mesh whose parent already has
collision hands that node's body a new shape and **drops the old one with
`spRemoveBranch`** — the previous shape gone, no question asked, nothing in the
status bar. In the drop harness's fixture, where four cubes shared a root, it read
as: mesh consumed, no new body, no new shape, every total unchanged, and a shape
quietly destroyed.

Reached from the **Create button** as much as from the drop — this is
`attachCollisionShape`'s own behaviour, not the drop's. Left alone because it
wants a decision rather than a patch: refuse, ask, or add beside what is already
there (which is `tlMoveCollisionShape`'s job, and now exists). Adding beside is
probably right and would make Create agree with the drop, but it changes what the
button has always done, so it is not a thing to slip in while fixing a test.

Worked around in the harness by giving the fourth mesh a node of its own, which is
also the realistic case — you do not usually stack collision on one node.

## `collision_drop.sh` stops after check 4 — OPEN, 2026-08-20

Found while re-running the collision harnesses for the Create change, and it is
NOT that change: the harness stops at exactly the same place with Replace on and
with Replace off, and those are the only two behaviours the setting selects.

What is established:

- The in-app log (`release/ww_colldrop_test.log`) ends after check 4 — the
  refusal check — so it is check 5, the first drop that actually CREATES, that
  never reports.
- NifSkope does not crash. The process is still alive and `Responding` when the
  harness's timeout kills it, so this is a stall rather than a fault, and a modal
  dialog is one of the shapes it could take.
- Everything before it passes: the panel accepts drops, the create hook is wired,
  a mesh payload is taken and a non-mesh payload is refused by action.
- The sibling GUI harnesses that touch the same code both pass —
  `collision_per_shape.sh` 8/8 (which also creates collision) and
  `collision_compiled_edit.sh` 7/7.

Whether it predates 2026-08-20 is not established: there is no recorded run of
this harness against the previous build, and the previous binary is gone.

## `block_rename.sh` in list mode HANGS, 7 runs in 10 — OPEN, 2026-08-07t

This is what "the flat list intermittently takes the process down" actually is,
and it is **not** a crash and **not** heap corruption. What is established:

- **No crash.** Windows logged no APPCRASH event for any of them (the only
  NifSkope ones on this machine are from 08-05), and running the whole thing
  under `gdb` catches no fault at all — the process sits there until something
  kills it. Under gdb it hangs 3 times in 3, which says timing.
- **It is the harness that stops, not the program.** A passing run takes **4
  seconds**; a failing one takes **63**, which is the script's own 60-second
  deadline plus start-up. The script then reports "did not finish", and from
  outside that is indistinguishable from a crash. That is the instrument the
  earlier "36 runs, no death" claim was made with, and why it was wrong.
- **List mode only.** Hierarchy is 4-5 seconds every time, dozens of runs.
- **Not the persisted animation setting**, though that was the obvious suspect
  and `GLView/Enable Animations` was indeed left true. Forcing it off is right on
  its own merits — a harness must force the state it measures — and it changed
  nothing: **4 of 8** on the current binary with it forced off.
- **Not a stale incremental build** (7 of 10 on a from-scratch `make clean`
  build), **not the IPC port** (6 of 10 with a unique port per run), and not
  contention with anything else running.

### Narrowed 2026-08-07x, and two earlier readings withdrawn

The harness now runs a **bounded pump** (`processEvents` with a 4-second
deadline) at each step, logging what it saw, and each statement around the
suspect region logs before and after. On a hung run the log stops at a precise
place, and that place is not where it was thought to be:

- **`expandAll` is not it**: 5 ms, leaving 163 visible rows, every run.
- **`setCurrentIndex` is not it** either: it logs "returned".
- **The pump after it never comes back**, though its deadline is four seconds.
  A deadline is only looked at *between* events, so what this says is that a
  single event's handler never returns.
- **Not a repaint storm.** Frames are now COUNTED either side of each pump
  (`wwGlPaintCount`, in glview.cpp): one or two frames, 0–1 ms of painting.
  The earlier reading — "`ww_perf_test.log` filling with `drawGrid` pairs, a
  queue that never empties" — was mostly the instrument. That trace opens and
  appends to a file **inside every frame**, and it sat under `WW_PERF_TEST`, the
  flag you reach for when something is slow. It has its own variable now
  (`WW_GRID_TRACE`), which is the general lesson: never leave a per-frame file
  write under the flag used to measure slowness.
- **Not a modal dialog or any other nested event loop.** A 1.5-second watchdog
  timer, armed before the pump and writing its own file, logs **nothing at all**
  across a 60-second hang. Timer events are dispatched inside a nested loop and
  are not dispatched inside a stuck handler, so this rules the nested loop out
  and rules a genuinely stuck handler in.

**Next**: a stack. That is the one thing never taken — the earlier gdb work asked
whether it *faulted*, not what it was *doing*. A catcher is written and works
(`scratchpad/stack_hang.sh`: launch, wait 25 s, attach `gdb -batch -ex "thread
apply all bt"` if the process is still alive); it simply did not catch a hang in
four attempts, and 4-in-8 needs more than four. Run it with ten. `-Wl,-s` strips
the release binary, so expect Qt DLL frames rather than ours — still enough to
name the widget.

**Not urgent, and worth saying why**: this is reachable only from the harness, at
a point no user passes through. `setCurrentIndex` returns; the hang is in the
program's own event processing afterwards, and a user's event loop is not a
bounded pump — it keeps running, so the same stuck handler would show as one
unresponsive moment rather than a dead process. It is worth the stack, not worth
a night.

## ~~Every structural edit serialises the whole file twice~~ — HALVED 2026-08-07x

**The second pass is gone.** `nifSnapshotOp` no longer saves the model after the
operation: the redo snapshot is taken at the FIRST UNDO instead, because the undo
stack is LIFO and by the time a command is undone every command pushed after it
has been undone too — so the model is already holding exactly the state redo has
to restore. Nothing is given up, and the remaining cost moves off every edit and
onto a keystroke that was going to rewrite the whole model anyway.

Checked in `block_dragdrop.sh` by comparing the **saved bytes of the whole model**
across undo and redo, twice round (the first undo is the one that captures, the
second has to find it already there) — not by looking at the one link that moved,
because a redo that restored the right parent and lost a transform would pass
that and fail this.

### The five hand-built sites keep their second snapshot — DO NOT "finish the job"

`NifSnapshotCommand` is also built by hand in five places (rigging, timeline,
three in unfucktools) and those still serialise twice. This was first written up
here as unconverted stragglers, with the lazy constructor "there if they want
it". **That was wrong**, and converting them would remove working guards:

- **Rigging** (`riggingtools.cpp`) — if the "after" serialise fails it *restores
  the target and warns*. That is failure detection for a transfer that has
  already rewritten the file.
- **Timeline** (`timelineedit.cpp`) — a failed serialise aborts the operation
  rather than recording an undo step it cannot honour.
- **Unfucktools**, all three — `const bool changed = ( before != after );`. They
  push an undo step **only if the repair actually changed something**. Without
  those bytes every no-op repair leaves a junk entry on the undo stack.

So the second serialisation is not overhead in these five: it is paying for
something, on operations run occasionally rather than dozens of times an hour.
The hot path — every array resize, insertion and removal, through
`nifSnapshotOp` — was the one paying twice for nothing, and that is fixed.

What remains below is the original measurement, and the first paragraph of it is
now half true.

## Every structural edit serialises the whole file twice — MEASURED 2026-08-07p

Found while measuring something else, and it is the bigger number of the two.
`nifSnapshotOp` (`src/nifsnapshot.h`) saves the entire NIF to a `QByteArray`
before the operation and again after, so **one undo step costs two whole-file
serialisations**. Every structural edit in the program goes through it.

Measured with `WW_BLOCKDND_BENCH` (see `block_dragdrop.sh`), re-parenting one
block: **88 ms on a 512-block file, 160 ms on 2012**, and the cost does not track
what the operation itself touches — moving a block out of a 3-child parent costs
the same as out of a 2000-child one.

Not acted on. It is a real cost on large files and it is shared by everything, so
it wants its own decision (a diffing undo command for structural edits, or
snapshotting only the affected branch) rather than being bolted onto a drag.

## ~~Block list: wwParentsOf is O(blocks) per moved block~~ — MEASURED, NOT WORTH IT

The suspicion was that `wwParentsOf` walking every block, once per block moved,
plus a sort comparator calling `getParent` per comparison, made a drag slow on a
large file. Measured: it does not show at all in a single-block drag, which is
the gesture anyone actually makes — that is the snapshot above. It shows only in
a multi-block drag, at 5 ms a block on a 512-block file and 15 ms on a 2012-block
one, so a 50-block drop on a very large file spends about 0.7 s there.

Caching the parent map for the duration of a call would buy the rarest gesture on
the largest files roughly a third of a second, and would leave the per-block
`Children` rebuild — also O(blocks) — where it is. Left alone on the numbers.

## ~~Block list: flat list mode is not sound~~ — FIXED 2026-08-07p

**The mode is kept.** Fault 1 was real, worse than filed, and is fixed, with a
harness that fails against the old code. Fault 2 does not reproduce.

1. ~~Blocks inserted while the flat list is showing are not addressable.~~
   **No row was addressable** — every row in the view, not only new ones, so a
   click, a drop and a right-click all hit nothing, anywhere, always. The
   original measurement asked only about the row it had just inserted, which
   cannot tell one broken row from every row being broken.

   `QHeaderView` maintains `length` by deltas; a model change hands a hidden
   section its width back without adding it in, and hiding it again subtracts it
   twice. With 9 of 12 columns hidden it went **negative**, and `visualIndexAt`
   answers -1 for any position past `length`, so `indexAt` had no column and
   therefore no index. Fixed by releasing the columns before any model change and
   applying them after, and by giving each mode its own saved header blob. Full
   account in `WW_CHANGES.md 2026-08-07p`.

   Two guesses in the old entry were wrong and are worth not repeating: the model
   signals were never the problem (they agree with their own row counts), and
   `setListMode()` **does** re-run `wireBlockListSelection()` — last line but one.

2. **It is not a crash. It is a hang, and it is still open** — see the section
   below, which supersedes the "does not reproduce" claim this entry carried for
   part of 2026-08-07. That claim was measured with an instrument that cannot
   tell a crash from a hang: "did the log say done".

New harness `tests/spells/block_list_modes.sh`, 8 checks per mode, drives the
invariant and what it costs the person using the program — both modes, green.

`block_rename.sh`'s list half is written and passes 24 of 24 on the runs that
finish, but it stays OUT of the default gate: in list mode that script hangs 7
runs in 10, which is the open item below. `MODES="hierarchy list"` runs it.

**Still open, small:** drag-and-drop has no coverage in flat list mode. Its code
branches on the model and is believed correct, and now that the view answers
`indexAt` there is nothing structural in the way — `block_dragdrop.sh` seeds no
list mode, so it needs the registry dance `block_list_modes.sh` uses.

**Also open, found while measuring the above:** `NifModel::insertNiBlock` calls
`endInsertRows()` *before* `insertAncestor`/`insertType` add the block's fields,
so a new block is announced with 0 rows and has 24 a moment later, with no second
signal. Nothing observed goes wrong, but a view that laid that row out on the
signal cached a child count that was already stale.

## ~~Block list: Blender-Outliner drag-and-drop and rename~~ — SHIPPED 2026-08-07

Specified 2026-08-05 against Blender's Outliner, with screenshots. **Drag to
move is built**; **rename turned out to have shipped already** in `d5765c4` and
is now covered and fixed for flat list mode. The spec is kept below because it
is what the implementation was measured against.

Shipped: `wwReparentBlocks` in `blocks.cpp` (the primitive, hoisted out of the
Collision Manager's Set Parent as the notes below asked), the three modifiers,
multi-select as one payload, the plural ghost, the target highlight through
`NifModel::dropTargetBlock`, the cursor hint and the four refusals. Covered by
`tests/spells/block_dragdrop.sh` (26 checks) and `block_rename.sh` (15 per
mode).

**What the implementation added to the note below:** drop handling had to go on
`NifTreeView` as view overrides, NOT an event filter and not the model —
`QApplication::notify` routes drag events through the drag manager, so a drag
event sent to the viewport reaches no event filter at all. `wwDeliverDragEvent`
exists so a harness can start where Qt's routing ends.

**And the drag has to be enabled on the MODEL.** `QAbstractItemView` will not
enter `DraggingState` without `Qt::ItemIsDragEnabled` on the pressed index, so
`startDrag()` is never called and the whole feature does nothing — which is
exactly how it first shipped, with a green 26-check harness, because driving the
drop handlers directly leaves that step outside the measurement.

**Reordering by dragging into the gap between rows shipped 2026-08-07b** (user
request, same evening): the block moves to that position in the parent's
`Children` array, with an insertion line, hierarchy mode only. Two block-list
irritations went with it — double-click opened the delegate's integer editor over
the inline rename, and `spEditStringIndex`'s txt icon drew down the entire Value
column.

### Drag to move

Blender's tooltip is the whole spec: **"Move inside collection (Ctrl to link,
Shift to parent)"**, shown at the cursor, with the drag ghost reading
**"objects"** in the plural for a multi-selection and the drop-target row
highlighted. Multi-select must drag as one payload.

**The one place Blender does not map onto NIF.** Blender has two separate
things: a *collection* is organisational and never changes an object's world
transform, while *parenting* is transform-level. A NIF has only `NiNode`
children, so those two collapse into one operation and the distinction has to
be re-cast as what happens to the transform:

| gesture | meaning here |
|---|---|
| plain drop on a `NiNode` | re-parent, **preserving world position** — compensate the local transform, Blender's move-to-collection semantic of "nothing appears to move" |
| **Shift** | re-parent keeping the **local** transform, so the block snaps into the new parent's space |
| **Ctrl** | **link** — add the child link to the new parent and *leave the old one*, so the block has two parents. This is a real NIF capability and the closest thing to Blender's link |

This settles the question left open by `reparentFromBlockList`, which currently
keeps the local transform unconditionally: that becomes the **Shift** behaviour,
and plain drop needs the world-preserving variant written. — *Done as specified;
Set Parent stays `KeepLocal` deliberately.*

Refuse and say why when the target cannot take children, when it is the dragged
block itself, and when it is a descendant of it (cycle). — *All four, the fourth
being a link that already exists.*

### Rename — was already shipped

Blender's inline rename: double-click or **F2** turns the row into an editor in
place with the text selected, Enter commits, Escape cancels.

**It was there all along**, in `d5765c4`: `renameBlockListIndex` +
`BlockListRenameEdit` in `nifskope.cpp`, with `wwPropagateNodeName` carrying the
name into `NiDefaultAVObjectPalette` entries and `NiControllerSequence`
controlled blocks. Nothing measured it, which is how it came to be filed here as
not started — the guess below about `buddy()` and edit triggers was the wrong
mechanism for the right conclusion.

The one real gap, now fixed: it returned unless the index came from the proxy,
so in **flat list mode** F2 and double-click did nothing at all — and the two
models put the name in different columns (proxy 1, flat `ValueCol`).

### Implementation notes

- **View level, not model level.** `NifModel` has no `mimeData`,
  `dropMimeData` or `supportedDropActions`, and Qt's row-move semantics do not
  fit blocks-plus-links — a model-level version would try to move rows. Use a
  `dropEvent` on the block list.
- The list runs through a **proxy** and switches between list and hierarchy
  modes, so the drop index must be mapped back to a block number through
  whichever model is current.
- The multi-selection is already published as raw block numbers
  (`nifskope.cpp`, the `selectionChanged` handler), so multi-drag has a source
  of truth already.
- **Hoist the reparent primitive first.** `reparentFromBlockList` in
  `collisiontools.cpp` already does rebuild-old-`Children`, `addLink`-to-new,
  cycle check and the four refusals. Share it rather than writing a second one
  with its own idea of what is legal.
- Dropping a `BSTriShape` onto the **Collision Manager** is the same payload
  routed to the mesh→collision path; `collisionConsumeSource` is the single
  choke point and already honours the keep-mesh toggle. — **SHIPPED 2026-08-07u**,
  and it was a `dropEvent` on that dock and nothing else. It runs the same call
  the shape popup's Create runs, at the shape type the panel is showing, one body
  per mesh. Covered by `tests/spells/collision_drop.sh`.

  The one thing it could not reuse is the create spell's `isApplicable`: that
  asks the SCENE whether a block has geometry, so it answers no until a frame has
  been drawn with that mesh in it. The drop asks the file instead — full account
  in WW_CHANGES 2026-08-07u.

---

**This is THE backlog.** One file, everything that is left. Consolidated
2026-07-21 from what used to be seven scattered plan documents, each of which
independently claimed to know what was open — and several of which were wrong.

**Last full verification against the code: 2026-07-27.**

The 07-27 sweep found **five** stale claims and **six** items that were missing
from this file entirely. Same failure mode as the 07-21 sweep: two of the five
were features NifSkope has *always* had (stock behaviour filed as new work),
one was reversed by a later user decision and never un-filed, and one had grown
by 14 units while the estimate stayed frozen. Details in the two lists at the
bottom.

---

## How to use this file

1. **Verify against the code before building anything listed here.** These docs
   have been wrong in *both* directions. On 2026-07-21 a single review found
   **nine** stale claims: six Blender-batch items marked open that were fully
   implemented (Mirror editing, Checker deselect, Repeat Last, F9, Rip, Split),
   rest-pose display marked open when it had shipped, a "model landmine" that
   was an untested conjecture and proved false, and `TIMELINE_PLAN.md`'s 43
   unticked checkboxes which were **all** implemented.
2. **The code is the only complete inventory.** Docs are also wrong by
   *omission*: the Skeleton Manager and Pose Manager workspaces — the two
   largest un-started features in the project — existed only as disabled UI
   entries and a line in `CURRENT_STATUS.md`, and were missing from the backlog
   entirely. When asking "what's left", grep for `setEnabled( false )`
   placeholders and greyed menu items as well as reading markdown.
3. **Fast checks**: the `viewport.*` shortcut registry at the top of
   `glview.cpp` settles any viewport feature in seconds. For everything else,
   grep the feature's identifiers.
4. **Check whether the "missing" feature is stock NifSkope.** Two 07-27 stale
   claims (string-index display, nif.xml tooltips) were upstream behaviour that
   had simply never been looked for. Grep `nifmodel.cpp`'s `data()` role
   switches before filing display work.
5. **Grep the code, not just one file.** The 07-27 sweep initially "confirmed"
   the NifItem slab pool as unbuilt because the grep covered `nifmodel.cpp` and
   `nifitem.h` but not `nifitem.cpp`. It had shipped.
6. **A file on disk is not necessarily compiled.** `src/collisiontools.cpp` is a
   committed orphan that no longer builds (see §14). Diff the `.pro`'s exact
   paths against `find src -name '*.cpp'` — matching on basename alone hides it.

**Related files** — none of them carry backlog any more:

| file | role |
|---|---|
| `WW_CHANGES.md` | change log / history. Update it with every batch. |
| `CURRENT_STATUS.md` | session handoff: build rules, working agreement, gotchas |
| `BLOCK_DETAILS_OVERHAUL_PLAN.md` | design detail + binding visual rules |
| `MODELING_TOOLS_PLAN.md`, `UV_EDITOR_PLAN.md` | design detail |
| `TIMELINE_PLAN.md` | historical v1/v2 checklist (all ticked) |
| `PERFORMANCE_PLAN.md` | tiered analysis + measurements, incl. deferral rationale |
| `BONE_WEIGHT_TRANSFER_PLAN.md` | validated skin math, transfer design |
| `COLLISION_MANAGER_HANDOFF.md` | validated packfile offsets, test assets, gotchas |
| `BLOCK_LIST_DETAILS_OVERHAUL.md` | superseded v1 design notes (list side only) |

---

## Summary — everything open, by size

| # | area | size | state |
|---|---|---|---|
| 0 | **Renderer: PBR draws empty frames** | medium | **parked behind one constant** — hottest open bug |
| 1 | **Skeleton Manager** workspace | large | **phase 1 SHIPPED 07-27j** (read-only); phases 2-4 open |
| 2 | **Pose Manager** workspace | large | **SHIPPED 07-22d..l**; prop staging + 4 refinements deferred |
| 3 | Performance 15c + 16 (flattened storage + off-thread parse) | large | deferred as one joint project, prereqs listed |
| 4 | Collision Manager P4 | medium | **unblocked 07-30**; 2 items left, both ready |
| 5 | ~~Block Details: remaining typed editors~~ | small | **SHIPPED 07-30d** — 3 of the 4 were already done |
| 6 | Block Details: array table (P4) | medium | not started |
| 7 | Block Details: whole-file search, recent values/revert, hex viewer (P5) | medium | not started |
| 8 | ~~Block List: summary column, status badges~~ | small | **SHIPPED 07-30e** — badges are coloured text; "unreferenced" is undecidable |
| 9 | Rigging leftovers | medium | partly absorbed by #1 |
| 10 | UV editor: cross-mesh operators | medium | active-mesh-only is current design |
| 11 | Animation: 3 remaining gaps | small | the other 43 items are done |
| 12 | Rendering: spec/gloss **SHIPPED**, PBR **parked**, SSS future | large | see §0 and §12 |
| 13 | CLI follow-ups (see §13) | small–medium | base CLI shipped 07-21e; harness port is 22 files, not 8 |
| 14 | ~~Repo hygiene: orphaned source, 54 uncommitted files~~ | small | **DONE 07-30** — orphan gone, tree clean; verified, not assumed |
| 15 | ~~Viewport: Separate By Material / By Loose Parts~~ | small | **SHIPPED 07-30c** — By Material was impossible; shipped as By Segment |

---

## 0. Renderer — PBR draws empty frames (HOTTEST)

Everything for PBR shipped over 07-27b..e and is then **switched off** by
`static constexpr bool pbrmFeatureEnabled = false;` at `glproperty.cpp:996`.
The gate is in code, not just the UI, so a stale QSettings value cannot switch
it on behind a greyed-out menu — verified by leaving `Settings/Render/PBRM Mode`
set to PBR in the registry and confirming a full frame still rendered.

`BSLightingShaderProperty` shapes draw **nothing** in PBR mode. RenderDoc
confirmed program selection, texture binding and draw submission all work; blend
is disabled with a full write mask; effect-shader shapes still render; the grid
is not occluded. **Eliminated:** per-draw GL state, cubemap completeness,
uniform-block binding, unconditional base-alpha-as-opacity.

**Next diagnostic — now actually possible (07-27q/r).** Every prior finding here
came from captures that contained only Qt's composite blit, so "RenderDoc
confirmed X" in the paragraph above is worth re-checking rather than trusting.
The in-app capture path lands real frames now:

```
WW_RDC_FRAMES=4 WW_RDC_OUT=%TEMP%\pbr\cap NifSkope.exe <file.nif>
rdc open %TEMP%\pbr\cap_paint002_capture.rdc && rdc draws && rdc cbuffer <eid> --stage vs
```

Start by reading the **uniform block contents at the failing draw**, because that
is exactly what the 07-17 line defect turned out to be: `viewportDimensions`
arriving as zeros while the CPU-side struct read correct. A probe on the app side
cannot see that. `rdc cbuffer`, `rdc rt <eid>` (colour target after a specific
draw) and `rdc snapshot <eid>` (full state + shader source) are the tools that
settled it.

Note `rdc debug vertex` shows `0xCCCCCCCC` inputs on the *working* legacy draw
too, so that output is an artifact and cannot be read as evidence.

To resume: flip the constant and re-enable the two greyed menu entries. Full
findings in `WW_CHANGES.md 2026-07-27e`.

### Refraction guard — FIXED 07-27g and 08-09s

The "fixture guards nothing" symptom turned out to be a **real renderer bug**:
`hasRefraction` was only ever assigned in `updateParams`'s no-material branch, so
refraction could not engage on any FO4 mesh backed by a BGSM/BGEM — nearly all of
them — and had been dead since the feature shipped on 07-06. Fixed, and the
fixture rebuilt as an A/B pair against `glass_visor` (same mesh, flag off). Full
account, including why the material is OR'd with the NIF flag rather than
replacing it (0 of 6899 vanilla FO4 materials set `bRefraction`), in
`RENDERER_MATCH_PLAN.md §0`.

The bounded distortion follow-up is now closed against the real
`X01_Torso_VFX.nif`: its `autoLoop` sequence was captured at the authored 1.0
peak with refraction forced on and off. The normal-map sheet becomes a clean,
local framebuffer warp when enabled instead of the visible green/orange map,
and its displacement is capped at eight screen pixels at every resolution.
Loaded-NIF scenes now contribute their opaque geometry before the shared
refraction pass as well.

---

## 1. Skeleton Manager workspace — PHASE 1 SHIPPED 2026-07-27j

**Shipped (phase 1, read-only):** `src/skeletontools.{h,cpp}` +
`Workspaces ▸ Skeleton`. Hierarchy tree, deforming/unused/not-a-bone
classification, per-bone shapes/verts/weight, filters, search, selection sync,
rest-pose toggle, and the two free validation findings (dangling skin bones,
duplicate names). CLI `skeleton <file> [--validate]`, exit 1 on findings.
Verified by `WW_SKELETON_TEST` plus an independent weight cross-check (summed
weight 1688.98 vs 1689 vertices). Writes nothing.

The workspace is appended LAST in the managers list on purpose: the persisted
`UI/Workspace` index is positional, so inserting earlier would reopen a
different workspace for every existing user.

**Still open here:**
- **Phase 2 — validation + prune.** The remaining checks, then
  `Prune unused bones`, which must remap every vertex's bone indices (the Join
  bug in reverse; `WW_CHANGES 2026-07-19b` applies exactly). Also add
  `--prune-unused` to the CLI. Note the *Unused* filter still has no positive
  test case: a 40-mesh sweep of vanilla FO4 armour found zero unused bones, so
  phase 2 needs a constructed fixture.
- **Phase 3 — bone transform + rebind.** The genuinely risky part (§A.3):
  every transform edit must recompute the affected inverse-bind transforms in
  the same undo step, or the mesh tears. Ships only behind the Create Skin
  gauntlet's must-not-move check.
- **Phase 4 — persistent skeleton reference slot**, then octahedral viewport
  bone display.

**Design: `SKELETON_AND_POSE_PLAN.md`** (2026-07-21) — Blender-grounded
(Armature Edit Mode), with the rebind hazard, phasing and CLI surface worked out.

Tooltip contract: *"skeleton hierarchy, rest-pose, bone transform, and
validation workspace."*

**This absorbs several items previously filed as loose rigging tasks** — an
independent persistent skeleton reference is this workspace's prerequisite, not
a separate feature, and zero-weight bone pruning belongs here too.

Building blocks that already exist:
- `Scene::restPoseBlock` + `Node::restWorldTrans()` — rest-pose display, shipped
  (`glnode.cpp:337/353`, five writers in `glview.cpp`).
- Rigging Manager: bone list, donor/target bone compare, transfer, weight paint.
- `BONE_WEIGHT_TRANSFER_PLAN.md` — validated inverse-bind and transfer math.
- FO4 `CustomizationRemapData` decode (see the reference memory / plan doc).

## 2. Pose Manager workspace — SHIPPED 2026-07-22d..l (1 feature + 4 refinements open)

Built: posing engine already worked (live skinning); pose library + blend in
`AnimSetup` (`savePose`/`applyPose`/`readPose`); the dock (`src/posetools.cpp`,
Workspaces ▸ Pose) with a bone list that drives selection, save/apply/delete and
a blend slider. Verified by `WW_POSEDOCK_TEST` + a screenshot. CLI `pose` mirrors
it. Load-screen composition resolved to `merge` (07-22a).

Much more shipped after 07-22d than this section used to admit: 07-22e..l added
bone-by-bone posing, viewport skeleton + click-to-pose, Outfit Studio pose XML,
load-skeleton-from-archive, a folder-based pose library, and the 07-22i batch
(weights overlay, hover name, multi-select, pin, non-destructive posing,
proportional editing, mirror axis). **Screen Archer Menu pose JSON** import
joined them on 08-10 (`AnimSetup::applySamPose`, `PoseImportSamButton`,
`WW_SAMPOSE_TEST`) — import only; writing a SAM pose back out is not built.

**Still open — one feature:**
- **Prop staging** (attach an external NIF under a bone) — 0 hits in
  `posetools.cpp`. Blocked on a question below.

**Deferred refinements** (all from 07-22i, all bounded, none blocking):
- **Pose-mode bone proportional editing** — the edit-mode version shipped; the
  object-gizmo path is parent-space and more intricate. The falloff infra is
  already shared, so this is a contained follow-up.
- **Topology-based mirror pairing** — position pairing works for symmetric
  meshes, which is every real case so far.
- **Weight overlay uses bind positions** (shape world transform × vertex) —
  exact for an unposed mesh, lags the deformed surface once bones are posed.
- **Non-destructive posing is a mode-boundary guarantee**, not a display
  overlay: bone nodes *are* touched while in Pose Mode and restored on exit. A
  never-touch-the-nodes version is a larger rearchitecture.

**Mirror / paste-flipped poses is DONE** — `PoseMirrorButton` +
`ogl->poseMirrorBone()` (`posetools.cpp:192`, `:491`). It was filed as open here
while `poseMirrorBone` already existed; 07-22i says so explicitly.

**Design: `SKELETON_AND_POSE_PLAN.md`** (2026-07-21) — Blender-grounded (Pose
Mode + Pose Library). Key decision recorded there: **a pose IS a
`NiControllerSequence` carrying one key per bone at t=0** — the NIF equivalent
of a Blender Action — so the library reuses the sequence machinery `anim-setup`
and the timeline already provide instead of inventing a pose format.

Tooltip contract: *"character posing, prop staging, reusable pose, and
load-screen composition workspace."*

(The stale line "Nothing built" lived here until 07-27, four shipping days after
the dock landed. The plan's build order — pose library B.1 before bone
transforms A.3 — was followed and is now history rather than guidance.)

**One question still blocks prop staging** (plan §B.6): does it edit the saved
file, or is it preview-only? The other two questions in that section are answered
and should not be re-asked — load-screen composition resolved to `merge` (07-22a)
and the pose library is a folder of files, i.e. cross-file (07-22j).

## 3. Performance — 15c + 16, deferred as ONE joint project

Authoritative analysis, measurements and rationale: `PERFORMANCE_PLAN.md`.
Tier 1, Tier 2 and Tier 3 batch 1 (NifItem slab pool) have shipped.

- **15c — flattened packed storage** for `BSVertexData` rows, and
- **16 — off-thread raw-buffer parse.**

Deferred together on evidence: measurement shows item *construction* dominates
load, and an off-thread parse only pays once flattened storage makes the raw
buffer the model's storage. Prereqs recorded in the plan: full gauntlet per
step, a `QPersistentModelIndex`-over-virtual-rows design, and the `#ARG#`
condition-cache contract either preserved exactly or replaced wholesale.

**Deliberately not doing** (rationale in the plan, do not re-open casually):
- T2.13 draw sorting — needs a draw-loop restructure plus per-shape uniform
  caching (Tier-3 scale), and a bare `glUseProgram` current-check is exactly the
  cache-desync landmine behind the old startup-grid bug.
- T2.14 particle frustum culling — the sim must keep running for correct resume,
  draw-only culling has minimal payoff, and a wrong-space plane test silently
  blanks VFX with no headless test to catch it.

## 4. Collision Manager — P4

P1–P3 have landed; the multi-section compressed-mesh encoder shipped 07-18.
Precisely (re-checked 07-27, the old wording here was ambiguous): a section still
holds ≤255 verts/tris — that is the format — and the encoder now partitions a
mesh into up to **4096** such sections by spatial slab, with per-section domains,
and single-section output stays byte-identical to the validated writer.

- ~~**Compound / instance encoding — BLOCKED.**~~ **UNBLOCKED 07-30, and in fact
  already done.** The blocker was "needs reference pairs to validate against", and
  the §4d encoder campaign supplied exactly that: every vanilla file is a reference
  pair, and the assembler writes **compounds 27/27 and compound instances and
  children byte-identical** to the originals, inside the 470/470 byte-exact
  packfile result. `COLLISION_MANAGER_HANDOFF.md`'s "do not build blind" was
  honoured — it was built against measurements, not guesses. This line survived
  because nobody came back to it after the campaign closed.
- Per-triangle face-material painting — open (0 hits in `collisiontools.cpp`).
- `hknpBSMaterialProperties` beyond the single-material table — open.
- The shipped multi-section encoder still needs an **in-game walk test**.

### 4a. Ragdoll (bone collision) — DECODE FIRST, then the encoder — NEW 07-27o

Viewing and editing per-bone collision **works** as of 07-27o: 32 of the 35
vanilla FO4 actor ragdolls attribute their capsules per bone, Decompile yields one
`bhkRigidBody` + `bhkCapsuleShape` per bone, and the Collision Manager lists them
with a Bone column. What is missing is the way back.

**This is not an encoder task.** A `bhkRagdollSystem` packfile carries five classes
NifSkope does not decode, so there is nothing to round-trip an encoder against —
`decode(encode(decode(x))) == decode(x)` is the only usable test. Do these in
order; each is independently verifiable against a vanilla file:

1. ~~**`hknpRagdollData`** — the packfile root.~~ **DONE 07-27p.** It derives from
   `hknpPhysicsSystemData`, so matching the class name was the whole fix; all 35
   vanilla ragdolls now decode their bodies. Its remaining unread arrays are
   `+0x50` (38 entries on 39 bones — the constraint bindings, see 3) and `+0x80`
   (39 entries, unidentified).
2. ~~**Per-body mass / inertia / damping.**~~ **DONE 07-27p.** Now on
   `HknpBodyPhys`. Strides: `dyn_motion` `0x40`, `dyn_inertia` `0x70`, both measured
   twice (brahmin ragdoll and a 15-body Halloween banner). Indexed by the **motion
   index** at `cinfo +0x0c`, not the body index — that distinction is load-bearing,
   see the 07-27p entry. The arrays carry their own counts at `+0x28` / `+0x38`.
3. ~~**`hknpSphereShape` synthesis.**~~ **DONE 07-27ad.** The plane/face/index
   relArrays at `+0x40`/`+0x44`/`+0x48` only exist on a polytope; on a sphere the
   vertex payload starts at `+0x30 + 0x10`, which *is* `+0x40`, so those fields
   were vertex bytes read as counts (Deathclaw's `+0x44` read 32544 faces) and the
   shared sanity check returned before the sphere branch could run. Vertices are
   read first now, polytope arrays only after the primitives return. **All 35
   vanilla ragdolls decode with zero undecoded shape classes**, including
   Deathclaw 28/28, `Robot/SkeletonRef` 48/48 and `skeletonSentryBodyPart` 24/24.
4. ~~**The joints**~~ **DONE — graph 07-27ae, frames 07-27af, limits 07-28a.**
   The **binding graph** is decoded and on
   `HknpSystem::constraints`: root `+0x50`, 0x18 an entry, constraint pointer at
   `+0x00` (global fixup), child body `+0x08`, parent body `+0x0c`, 8 bytes pad.
   Verified on the brahmin against the skeleton itself — all 38 name a bone and its
   nearest *embodied* ancestor (bodiless intermediates like `LNeckHub` skipped) —
   and all 35 vanilla ragdolls decode, 757 joints of which 237 are hinges.

   **Joint FRAMES also done 07-27af**: `hkTransform` at constraint `+0x30`
   (parent side) and `+0x70` (child side), three basis rows then the pivot, on
   `HknpConstraint::rotA/rotB/pivotA/pivotB`. 755 of 757 joints decode frames; the
   2 that do not are `hknpBreakableConstraintData`, a wrapper with its own layout
   that is deliberately skipped rather than misread.

   **The angular LIMITS also done 07-28a**, via a validated atom type→size table
   (`decodeConstraintAtoms`; sizes and evidence in WW_CHANGES 07-28a). All 757
   constraint objects walk to their exact end, each class with exactly one atom
   sequence. `HknpConstraint` now carries `twist`/`cone`/`plane`/`hinge`,
   `friction`, `motorEnabled` and `breakable`; **757 of 757 joints decode limits**,
   the last two by following the `hknpBreakableConstraintData` wrapper (the
   Vertibird's doors) to the `hkp*` data inside. The predicted values all came
   back exactly: brahmin tail-base twist ±5°, cone 40°, knee hinge -15°..40°.

   Two vanilla hinges store min/max transposed (`+5.00/-5.00`, `+0.10/-0.10`);
   that is the file's own data and is reported, not silently corrected.
5. ~~**`hkaSkeleton`**~~ **DONE 07-28b.** A root object beside `hknpRagdollData`,
   one per ragdoll: `hkArray`s at `+0x18` parentIndices (`hkInt16`), `+0x28` bones
   (16 bytes: a name pointer, always null here, then `lockTranslation`) and `+0x38`
   referencePose (`hkQsTransform`, 48 bytes). The four arrays after those are empty
   in every vanilla ragdoll. On `HknpSystem::bones`; `collision` prints the rest pose.

   **Bone index == body index**: all 757 constraint bindings agree with
   `parentIndices`, and bones == joints + 1 on all 35. Rotations are unit
   quaternions and scales exactly `(1,1,1)` on all 792 bones.

   **Bone names are NOT in the packfile** — every name pointer is null and unfixed.
   They must come from the NIF node list, as the body→node mapping already does.
6. ~~Only then the encoder.~~ **DONE 2026-07-29e**: all 37 vanilla ragdolls
   rebuild byte for byte. The notes below stayed because nobody came back to them.
   Original text: The array machinery in `hknpEncodeCompressedMesh` is
   directly reusable — the ragdoll root uses the same `hkArray` layout at the same
   offsets. Validate by round-trip on all 35 ragdolls.

   **`hknpRagdollData`'s full root layout** (measured 07-28c; every member, not
   just the ones the decoder reads). All arrays are `hkArray`: payload pointer
   patched by a local fixup, count at +8, capacity|0x80000000 at +12.

   | offset | member | stride | count |
   |---|---|---|---|
   | `+0x00` | (empty array) | — | 0 |
   | `+0x10` | body_props | `0x50` | bodies |
   | `+0x20` | dyn_motion | `0x40` | bodies |
   | `+0x30` | dyn_inertia | `0x70` | bodies |
   | `+0x40` | bodyCinfos | `0x60` | bodies |
   | `+0x50` | constraint bindings | `0x18` | bones − 1 |
   | `+0x60` | shape entries | `8` | bodies |
   | `+0x78` | **pointer to `hkaSkeleton`** (global fixup) | — | — |
   | `+0x80` | bone → body index | `4` | **bones** |

   Two traps in there. The `+0x80` array is counted in **bones, not bodies**, and
   the two differ — `Robot/SkeletonRef` has 48 collision bodies but 11 ragdoll
   bones, `skeletonSentryBodyPart` 24 and 9. Reading it at the body count runs off
   the end into garbage. Its contents are the identity map in **all 35** vanilla
   ragdolls, so an encoder emits identity of length `bones`. Shape entries at
   `+0x60` are 8 bytes of nothing but a global fixup to the shape.

   **Shape encoding is a fixed template** (measured 07-28c across all 801 vanilla
   capsules and spheres). A `hknpCapsuleShape` is not a primitive: it is a full
   convex polytope *plus* the two exact end points, and every vanilla one has the
   same topology — **8 verts, 8 planes, 6 faces, 24 indices, 432 bytes**, with the
   index table and the face table **byte-identical across all 778**:

   ```
   faces   00 00 04 04  04 00 04 04  08 00 04 04  0c 00 04 04  10 00 04 04  14 00 04 04
   indices 07 06 02 03  03 02 00 01  07 05 04 06  01 00 04 05  01 05 07 03  02 06 04 00
   ```

   So both tables are constants an encoder embeds verbatim. What varies is only
   the geometry, and it is a **square-section box around the capsule axis** — all
   8 hull verts are equidistant from the A–B segment in all 778, so the corners
   are `±margin` perpendicular to the axis at each end. `convexRadius` (`+0x14`)
   is the capsule radius minus that margin; margin runs 0.000125 to 0.0186 with
   70 distinct values, so it is per-capsule and must be carried, not defaulted.

   Layout: `+0x14` convexRadius, `+0x18` a constant `0.08693` (shared with
   spheres), `+0x30` vertex relArray, `+0x40`/`+0x44`/`+0x48` plane / face / index
   relArrays, `+0x50` and `+0x60` the exact end points (`hkVector4`, w = 1), then
   verts from `+0x70`. Each hull vert's w encodes its own index in the low
   mantissa bits of 0.5.

   `hknpSphereShape` is far simpler: **128 bytes, 4 identical verts** (SIMD
   padding) from `+0x40`, radius in `convexRadius`, no plane/face/index arrays at
   all — which is exactly why reading those three fields on a sphere returns
   vertex bytes, the bug fixed in 07-27ad.

   **Class name hashes** for the encoder's `classNames` table, read out of the
   vanilla files and identical across all 35: `hkaSkeleton` `0xfec1cedb`,
   `hknpRagdollData` `0xdc8f20ab`, `hknpCapsuleShape` `0x60a75f4c`,
   `hknpSphereShape` `0x741e9012`, `hknpConvexPolytopeShape` `0x3ce9b3e3`,
   `hkpRagdollConstraintData` `0xb77d2036`, `hkpLimitedHingeConstraintData`
   `0x51ea603a`, `hkpPositionConstraintMotor` `0x143dd400`,
   `hknpShapeMassProperties` `0xe9191728`, `hknpBreakableConstraintData`
   `0xc40485c7`, `hknpDynamicCompoundShape` `0x4620d11c` / `Data` `0xf33dc3cc`.
   `hkRefCountedProperties` `0x7c574867` already matches what `hknpencode.cpp`
   emits today.

   **The decode is 99.5% of packfile bytes by class** (audited 07-28b). What is
   left, and what the encoder must do about it:

   - `hkpPositionConstraintMotor` — 35 objects, **1 distinct byte pattern** across
     every vanilla ragdoll, shared by all of a ragdoll's joints (98 pointers to the
     one object on the brahmin). A Bethesda preset: type 1 (POSITION), minForce
     -1e6, maxForce 100, tau 0.8, damping 1.0, recovery 5.0 / 0.2. **Emit verbatim.**
   - `hkRefCountedProperties` — 53 objects, **1 distinct byte pattern**. Also emit
     verbatim; there is nothing per-shape in it to author.
   - `hknpShapeMassProperties` — 53 objects but **43 distinct**, so this one is real
     per-shape data (`hkCompressedMassProperties`: centre of mass, inertia and
     major-axis space as `hkHalf` vectors, then mass and volume). It must be decoded
     or recomputed, not copied.

   All three appear exactly 53 times, matching `hknpConvexPolytopeShape` one for
   one — capsule and sphere mass properties are analytic, so only polytopes carry
   precomputed ones. A ragdoll of capsules alone needs none of this.

   Watch the constraint frames: `HknpConstraint::rotA/pivotA` is the **child**
   side and `rotB/pivotB` the **parent** side (measured, see WW_CHANGES 07-28b) —
   they were documented the wrong way round until then, and swapping them on write
   would produce a ragdoll that looks decodable and behaves wrongly.

Working tools for this: `NifSkope -no-gui collision <file>` prints the bindings and
the decode result; `--extract -b N -o F.bin` writes the raw packfile for
`tools/hkparse.py`, which already lists class names, hashes and the object table.

Until 5 lands, Decompile and Compile both warn (default-Cancel) that the trip is
one-way — do not remove those guards.

Full feature spec is preserved in the appendix at the bottom of this file;
validated packfile offsets and test assets live in
`COLLISION_MANAGER_HANDOFF.md`.

### 4b. Cloth collision — DECODED 07-28e, not surfaced — NEW

Reverse-engineered but **no code written**: nothing in the app reads this yet. It
would slot into the existing `collision` CLI, since `--extract` already pulls the
blob out verbatim.

`BSClothExtraData` holds a Havok **Cloth (`hcl`)** packfile — the same container
and version as collision (`hk_2014.1.0-r1`) but with a **0x50 file header where
`bhkPhysicsSystem` writes 0x40**, so section headers are not at a fixed offset.
Find them by searching for `__classnames__` rather than assuming, or the parse
desyncs. 24 classes; the collision-relevant ones are `hclCollidable`,
`hclCapsuleShape`, `hclTaperedCapsuleShape` and `hkaSkeleton`.

**`hclCollidable`** (176 bytes) is an `hkTransform` at `+0x20` (three basis rows,
translation at `+0x50`), a float `0.01` at `+0x84`, a shape pointer at `+0x88`
(global fixup) and an inline name following the convention
`Collidable_<Bone>NNN`. Sets are small — the PrewarDress collides against five:
both thighs, both calves and the spine, with L/R positions mirrored to `±6.62`.

**`hclCapsuleShape`** (96 bytes): A `+0x20`, B `+0x30`, unit axis `+0x40`,
**radius** `+0x50` — radius, *not* length (a torso one is 1.7 long by 9.9 radius).

**`hclTaperedCapsuleShape`** (176 bytes) is a cone frustum with no ragdoll
equivalent. Only four values are authored — A `+0x20`, B `+0x30`, and
`[radiusA, radiusB, length, apexDistance]` at `+0x90`. Everything else is
precomputed SIMD scratch and reproduces exactly from those: cone apex position
`+0x40` = `A - (rA*L/(rB-rA)) * axis`, unit axis `+0x50`, `|B-A|` broadcast
`+0x60`, apex distance broadcast `+0x70`, `-sin(taper)` `+0x80`, and
`[cos, ?, sin, sin^2]` at `+0xa0`.

Checked over 40 cloth meshes / 43 tapered capsules: length, apex distance, apex
position, cos and sin² each reproduce **43 of 43**. One shape's taper sign
disagrees, most likely `rB < rA` (a reversed taper) which the `-sin` reading does
not cover — resolve that before writing an encoder.

**Two traps.** Cloth is in **game units** (the spine collidable sits at z=68.91,
about hip height) while ragdoll collision is in **Havok metres** (0.7455); confuse
them and everything is ~70x wrong. And unlike ragdolls, the cloth `hkaSkeleton`
**keeps its bone names** — all 277 of them — so cloth data is self-describing
where ragdoll data is not.

### 4c. Collision simulation in the viewport — NEW 07-28f, agreed with bungo

The goal, in bungo's words: preview ragdolls and props colliding properly; click a
bone, hold it and drag to move the ragdoll around by that bone; props the same but
without bones; and fling objects at static collision to test it.

**Backend: our own solver, decided 07-28.** Bullet 3.25 is in MSYS2 and was
offered, but `btConeTwistConstraint` is an *approximation* of Havok's twist+cone.
Since 07-28a decoded Havok's exact limits, a purpose-built solver matches in-game
behaviour more closely than Bullet would, and keeps the repo dependency-free in
line with the vendor-under-`lib/` convention.

Method: **XPBD** (extended position-based dynamics, Müller et al., substepped)
rather than sequential impulses. It is markedly more stable for stiff ragdoll
joints — blow-up and jitter are the usual failure modes of the alternative — and
dragging falls out naturally, since moving a body and letting the solver resolve
is exactly what the formulation does. Constraint compliance also maps cleanly onto
the `tau` factor already decoded on every limit.

Phases, each testable before the next:

**STATUS 07-28f: phase 1 DONE — the solver is correct and can be built on.**
`src/physics/ragdollsim.{h,cpp}` and `NifSkope -no-gui simulate <file>`.
**37 of 37** vanilla actor ragdolls settle, none diverge, worst ball-socket
separation across the corpus **0.4 mm**. Joint error converges as h² (brahmin:
8.5e-4 / 6.1e-5 / 6e-6 at 8 / 32 / 96 substeps) and total energy is now
substep-independent, which is the property that was missing. The five faults
found and fixed are written up in WW_CHANGES 07-28f; the ones that change how the
data must be *read* are:

- **Bodies sit on the centre of mass, not the bone origin.** The tensor is about
  the centre of mass and the bone origin is ~0.13 m away, so using the bone origin
  understates the inertia there by m*d² (~27x on the brahmin thigh). The file has
  no centre-of-mass field; the shape centroid is the right substitute and the
  decoded tensor corroborates it.
- **cinfo +0x30 is the body POSITION** — renamed from `com`. It equals the bone
  origin accumulated from `hkaSkeleton`'s reference pose on all 39 brahmin bodies,
  which is also a free cross-check of the skeleton decode.
- **`hkRotation` stores COLUMNS.** Reading the three decoded vectors as rows
  transposes every joint frame; it left 22 of 38 joints violating their own limits
  at rest, with cone angles to 169°. The conjugate fixes it.
- **The plane limit is the angle out of the parent's plane**, not the a0-to-b0
  angle about b2 — that older reading is not a well defined signed angle at all,
  since neither vector is perpendicular to b2.
- **Four Gauss-Seidel sweeps per substep, plus mass splitting.** A ragdoll's
  inverse inertia reaches the hundreds, so one sweep diverges on any body carrying
  several joints. Both numbers are measured, not guessed.

FIXED 07-28p: `dyn_inertia +0x20` holds **inverse inertia**, the field is now
`invInertia` on both `HknpBodyPhys` and `HknpSystem`, `dynamicInertia()` inverts
(including its box fallback, which computed true inertia into an inverse slot), and
the NIF boundary converts both ways so `bhkRigidBody`'s Inertia Tensor holds the
real tensor while the byte round trip is unchanged.

`simulate --selftest` runs eight synthetic rigs whose energy is conserved
analytically; keep it green. It is what made the above findable, by reproducing
the ragdoll blow-up in 6 bodies (`forkh`) instead of 39. Note the trap it exposed:
initial conditions must be a state the rig can actually occupy — a sideways shove
with no angular velocity is not, and the solver's projection of it looks exactly
like a substep-independent leak in the solver itself.

Known-imperfect, deliberately not chased: the light-inertia `fork` rig drifts to
about +10% over 10 s instead of converging to zero. Bounded, no effect on any real
ragdoll, and 16 sweeps do not improve it — most likely the float32 floor in
`v = dx/h` at small h.

1. **Solver core, headless.** `src/physics/`: bodies (mass, diagonal inertia,
   pose, velocities), substepped XPBD integrator, ball-socket joints, and the
   angular limits from `HknpConstraint` (twist / cone / plane / limited hinge).
   Built straight from a decoded `HknpSystem` — no new file parsing. A `simulate`
   CLI command runs it with **no GUI** and reports energy, joint drift and
   penetration, so stability is measured rather than eyeballed. A ragdoll pinned
   at the root must settle, not explode.
2. **Collision.** DONE 07-28g. Closest-point-between-segments covers all three
   capsule/sphere pairings exactly; ground plane; self-collision filtered by
   Havok's filter groups and by a rest-pose overlap test. `--drop`, `--ground`,
   `--no-self`. Corpus: 37/37 settle, 0 diverge, worst penetration 0.4 mm, worst
   joint separation 6.9 mm.
   **07-28i corrected the 07-28g/h diagnosis.** Most of the "hot" machines were
   FALLING THROUGH THE FLOOR, not unstable: collision handled only capsules and
   spheres, every machine is a polytope, and free fall looks like rising energy.
   Compounding it, a body carries several shapes (Liberty Prime: 14 shapes / 12
   bodies) and the build kept only the last. Both fixed. Measure settling by
   SPEED, not energy — energy scales with mass.
   **07-28j: 32 of 37 settle** (< 1 m/s after 10 s), worst penetration 0.04 mm.
   Per-body damping (dyn_motion +0x18/+0x1C) was decoded and unused; using it is
   most of that gain. MEASURE OVER 10 s, NOT 5 — speed at 5 s on a chaotic falling
   body is too noisy to steer by, and earlier decisions here were steered by it.
   **Rejected on corpus evidence, do not retry blind:** 16 solver sweeps instead
   of 4 (fixes the turret, breaks the sentry, 25/37 net) and scaling solverScale
   by correction count rather than joint count (30/37, turret 60 -> 149 m/s).
   **07-28k: adaptive per-joint passes.** Sweeps are spent per joint and only while
   that joint is still visibly violated, so healthy ragdolls cost what they always
   did. Key distinction: a joint RESTING on its limit is not one DRIVEN THROUGH it
   -- `limitAngle` returns the excess, and the 10-degree `FORCED_RAD` threshold is
   measured (worst body across the corpus: 34.6 m/s at 5 deg, 20.1 at 10, 46.3 at
   15). Workshop turret fixed, eyebot 87 -> 20 m/s, corpus worst 87 -> 20.
   **07-28l: 36 of 37 settle**, worst penetration 0.08 mm. Two of the three
   remaining failures were NOT RAGDOLLS: Robot/SkeletonRef is 48 bodies with 10
   joints and 630 of 1128 pairs already overlapping at rest (a parts kit with
   interchangeable pieces stacked in one place), skeletonSentryBodyPart is 15 of
   24 unjointed. Pinning unjointed bodies settles both (9.0 -> 0.58, 72.4 ->
   0.73). `simulate` reports the count; `--jointed-only` pins them. There was no
   self-collision bug here. Also: the 07-28k threshold sweep was measured against
   STALE BUILDS -- verified re-measurement gives 5 deg = 36/37 settled (worst 17.2),
   10 deg = 35/37 (worst 10.0), 15 deg = 35/37 (worst 41.3), so FORCED_RAD is 5 deg.
   **Open: 1 real failure** -- the eyebot at 17.2 m/s. No loose bodies, no rest
   violations; seven hinges on antennae with inverse inertia 4417 along their axis
   vs 3.67 across (ratio 1200).
   **07-28m TRIED AND REVERTED: the exact 3x3 angular solve.** It IS the correct
   maths and it IS worse -- 29/37 settling against 36, mosquito at 420 m/s. The
   per-axis projection leaves a residue perpendicular to the correction, that
   residue is dissipative, and the light creatures depend on it. Gating on
   anisotropy does not help (50:1 catches insect limbs too; mosquito still 395).
   So the gap is NOT "the angular solve is approximate" -- this solver's stability
   partly rests on that approximation, and replacing it needs a real dissipation
   model, not better linear algebra. Do not re-propose the 3x3 solve on its own.
   (superseded) **Open: 3 still moving** -- Robot/SkeletonRef (9.0 m/s) and sentry (2.8) are
   SELF-COLLISION (SkeletonRef -> 0.11 with `--no-self`); body-body is exact only
   for single capsules/spheres, so compounds are the suspect and exact convex
   body-body collision is the next job. Eyebot (20) is the hardest case in the
   corpus: hinges on antennae with inverse inertia 4417 along their axis vs 3.67
   across.
   (superseded) **Open: 4 still moving** — eyebot (87 m/s) and workshop turret (60), both
   HINGE, a convergence problem that 16 sweeps fixes locally; Robot/SkeletonRef
   (9.0) and sentry (4.1), SELF-COLLISION (SkeletonRef drops to 0.11 with
   `--no-self`). Next: per-joint adaptive iteration for stiff hinges, and exact
   body-body collision for compounds.
   (superseded) **Open after 07-28i: 4 of 37 genuinely wrong** — eyebot (200 m/s), workshop
   turret (66), Robot/SkeletonRef (24), radstag (12). 27 of 37 are at rest below
   1 m/s and 6 more are gently rocking. Body-body collision is still exact only
   for single capsules/spheres; a compound is a point set with no faces, so
   polytope-vs-polytope self-collision is NOT handled and is the most likely next
   cause. Ground contact is exact for both.
   (superseded) **Open after 07-28h: 6 of 37 settle hot** (bounded, intact, too energetic) —
   eyebot, Liberty Prime, a sentry, three turrets. All machines. Settled so far:
   the rest-pose joint error was constraints shipping with an UNSET parent
   transform (identity rotation, zero pivot, confirmed in the raw bytes); those
   are now derived from the rest pose as Havok's setInBodySpace would, taking calm
   from 28 to 31 of 37. Body placement is triple-validated and is NOT the problem:
   cinfo position equals the accumulated skeleton origin exactly and cinfo
   orientation matches within 0.1 degrees.
   **The residual is angular** — rebasing fixes pivots, not frames, and frames
   drive the limits (eyebot: 1.8 with `--no-limits`, 7,300 with
   `--only-limit hinge`). Next test, and it is a DATA question not a solver one:
   are these ragdolls authored in a different bind pose from the one the skeleton
   stores? Turret joint 2 has a properly authored pivotB of
   (0.7009, -0.1592, 0.2011) that still misses the rest pose by 0.22 m, which is
   what that would look like.
   Convex polytopes still want a proper centroid: the vertex mean is used, which
   is not the true hull centroid for an unevenly tessellated hull.
3. **Viewport.** Step in the render loop, draw the simulated pose, play/pause/reset.
   **07-28n did the bridge, headlessly.** Use PER-BODY RELATIVE motion, never a
   global ragdoll-to-scene map:
       T_draw_i = worldTrans(node_i) * ( rest_i^-1 * sim_i )
   At rest the bracket is the identity, so the simulated draw equals the static
   draw exactly. A global map is WRONG: node placement and the packfile rest pose
   disagree on 11 of 37 models -- Vertibird by 341 game units, Robot/SkeletonRef
   112.7, turrets 21-47, deathclaw 14.3 -- while rotations agree to 1e-5, so it is
   purely translation. The turret's 47.1 game units is 0.672 Havok metres, the same
   authoring inconsistency 07-28h found from the packfile side. `simulate` reports
   this spread per model.
   Remaining for a GUI session (needs on-screen verification, so not done blind):
   the draw override in glnode.cpp's bhkSystem branch, and play/pause/reset.
4. **Interaction — scoped with bungo 07-28o: a "Physics Sim" viewport mode.**
   A mode alongside Object/Edit (Tab), following `setPoseMode` exactly: leave
   conflicting modes on entry, capture the rest pose, restore it non-destructively
   on exit, emit a status line. **Runs live on entry; Space pauses/resumes; R
   resets.** Bone dragging is the one interaction agreed for the first cut —
   throwing, prop-flinging and click-to-pin were offered and deferred.
   **The drag MECHANIC is done and verified headlessly** (`simulate --drag N`):
   184 drags across the corpus, 0 divergences, worst joint separation 4.0 mm. It
   is `setPinned` + `setPosition`, which XPBD gives for free. Only the mouse-ray
   plumbing is unverified.
   Reuse from Pose Mode: `poseBoneAt`-style picking and the gizmo drag loop.
   Do NOT pin the root while dragging — pinning both ends asks a limb to span
   further than it reaches, which is unsatisfiable and looks like a solver bug.
5. **Static mesh collision.** Capsule vs `hknpCompressedMeshShape` triangles via a
   BVH over the already-decoded mesh.

Simulation doubles as the encoder's acceptance test: a ragdoll that is compiled
(4a item 6) and then falls over correctly is far stronger evidence than a byte
diff, because it exercises masses, inertia, joint frames and limits together.

**Note the units trap.** Ragdoll collision is in Havok metres, cloth in game units
(~70x apart, see 4b) — the solver works in one space and must convert at the edges.

### 4d-spec. Capsule encoder — SHIPPED 07-28t

`hknpEncodeCapsuleShape` is implemented and validated: **819 capsules across 36
actor skeletons, structure byte-exact on all 819, worst vertex error 9.9e-07 m**
(`collision <file> --roundtrip`). What follows is the measured format; the
narrative of what changed is in WW_CHANGES 07-28t.

**Object is always 432 bytes (0x1B0).** One distinct value corpus-wide: the flag
word at +0x10 (`c3 01 00 01`), the four hkRelArray descriptors, the 24-byte face
table, the 24-byte index table, and the two spare plane slots.

| offset | contents |
|---|---|
| +0x00 | 16 bytes zero (vtable + refcount, filled by the loader) |
| +0x10 | `c3 01 00 01` constant |
| +0x14 | float convexRadius |
| +0x18 | u32 material CRC |
| +0x30 | hkRelArray vertices: `08 00 40 00` (count 8, payload +0x70) |
| +0x40 | hkRelArray planes: `08 00 b0 00` (count 8, payload +0xf0) |
| +0x44 | hkRelArray faces: `06 00 2c 01` (count 6, payload +0x170) |
| +0x48 | hkRelArray indices: `18 00 48 01` (count 24, payload +0x190) |
| +0x50 | hkVector4 capA, w = 1.0 |
| +0x60 | hkVector4 capB, w = 1.0 |
| +0x70 | 8 hull vertices, hkVector4; w = 0.5 with the vertex index in the low mantissa byte (`00 00 00 3f`, `01 00 00 3f`, ...). Slot order equals index order, checked on all 778 |
| +0xf0 | 8 plane slots: 6 real, then 2 spares holding `00 00 00 00` x3 + `ee ff 7f ff` |
| +0x170 | face table, 6 x (u16 firstIndex, u8 count=4, u8 flags=4): 0,4,8,12,16,20 |
| +0x190 | index table, 24 bytes: `07 06 02 03 03 02 00 01 07 05 04 06 01 00 04 05 01 05 07 03 02 06 04 00` |

**The hull is a shrunk core, not the capsule.** The solid is that box with every
support plane pushed out by convexRadius — the same offset convention
`hknpShapeMassProperties` uses.

    core = OBB about the segment, padded by `padding` on all THREE local axes

**An OBB, not an AABB.** An earlier revision of this section said AABB; that held
only because the brahmin's 39 capsules are all axis-aligned. 195 of 778 corpus
capsules are tilted, where the AABB reading is out by up to **17 mm** against
4.8e-07 m for the OBB. Do not reintroduce it.

**Local frame**, every line measured 778 of 778, convention *bit set = + side*:

- bit 0 is the capsule axis, set = toward **capA** (so `e0 = -normalize(capB-capA)`)
- bits 1 and 2 are the perpendiculars u and v, with **`u x v = -e0`** (left-handed)
- half-extents: `L/2 + padding` along e0, `padding` on u and v
- planes run **`+u, -v, +v, -u, +e0, -e0`**, as `(n, d)` with `n.x + d = 0` on the
  face. Derived independently from the constant index table and from the stored
  planes; the two agree.

**Two inputs are NOT recoverable from (capA, capB, convexRadius).** Measured, not
assumed — an encoder must take them as optional inputs (see `HknpCapsuleInput`):

- **padding.** `padding/convexRadius` sits at 1/99 but scatters **1.6e-5
  relative** across the corpus, hundreds of ULP, far beyond the rounding of any
  fixed formula. 1/99 is authoring intent, not the stored relationship: an outer
  radius split `convexRadius = 0.99*Rout`, `padding = 0.01*Rout` has exactly that
  ratio. Use `radius/99` only for a NEW capsule.
- **the roll about the axis.** Capsules whose axes agree exactly always agree on
  the roll (34 of 34 groups), but capsules whose axes agree to **0.008 degrees**
  disagree, and no ordering rule (argmin |a_k|, argmax, cyclic) explains the split.
  It is inherited from the authored primitive. A capsule is rotationally symmetric
  so any perpendicular is geometrically valid; only byte-exactness needs the
  original.

**`primRadius` is a choice, not a measurement.** The solid's cross-section is a
rounded square, half-width `R + padding` facing a face and `R + padding*sqrt(2)`
facing a corner. The decoder takes the circumscribed value so it never under-states
the solid. A/B on the settle corpus: inscribed 33/37 (Liberty Prime 34.2 m/s),
circumscribed 34/37 (3.11), the old accidental `sqrt(3)` 34/37 (2.95).

**Trap for anyone measuring the padding:** every corner overhangs the segment
axially, so a closest-point-on-*segment* helper returns `padding*sqrt(3)`, not the
perpendicular `padding*sqrt(2)`. It stays a clean constant across the whole corpus,
so ratio checks pass while the value is 22% too large. Measure against the axis
LINE, unclamped.

### 4d-spec-sphere. Sphere encoder — SHIPPED 07-28u

`hknpEncodeSphereShape`. **30 of 30 byte-exact over the actor skeletons — the whole
object**, because a sphere derives nothing: no core box, so no padding and no roll.

**Always 128 bytes (0x80).** Only three things vary; everything else is one value
corpus-wide (measured over 23 spheres).

| offset | contents |
|---|---|
| +0x00 | 16 bytes zero |
| +0x10 | `11 01 00 01` constant (capsule's is `c3 01 00 01`; the low byte looks like a shape-type tag) |
| +0x14 | float convexRadius — the true radius, there is no core box to add |
| +0x18 | u32 material CRC |
| +0x1c..+0x2f | zero |
| +0x30 | hkRelArray vertices: `04 00 10 00` (count 4, payload +0x40) |
| +0x34..+0x3f | zero |
| +0x40 | the centre, repeated 4x for SIMD, w = 0.5 with index **0** in all four — one vertex, not four corners |

**Trap:** a sphere has NO plane, face or index arrays, and the vertex payload starts
at +0x40 — exactly where a polytope's plane descriptor lives. Reading +0x40/+0x44/
+0x48 as relArrays on a sphere reinterprets vertex floats as counts.

### 4d-spec-polytope. Convex polytope — ENCODER SHIPPED 07-28x

`hknpEncodeConvexPolytopeShape`: **68 of 68 byte-exact** over the actor skeletons.
The layout below was measured over 76 vanilla polytopes with zero violations, and
the encoder now confirms it end to end. Two items at the bottom remain open, and
both bite only when synthesizing NEW geometry, not when rewriting existing.

**Variable length.** Everything follows from the counts:

| region | at | size |
|---|---|---|
| header | +0x00 | 16 bytes zero |
| flag word | +0x10 | `01000143` (74 of 76) or `01000043` (2). Capsule `010001c3`, sphere `01000111` |
| convexRadius | +0x14 | float |
| material CRC | +0x18 | u32 |
| relArrays | +0x30 verts, +0x40 planes, +0x44 faces, +0x48 indices | u16 count + u16 offset from the field |
| vertices | +0x50 | `nv` x 16, **nv always a multiple of 4** |
| planes | +0x50 + nv*16 | `np` x 16, **np = roundup(nf, 4)** |
| faces | planes end | `nf` x 4, **padded to roundup(nf, 4) entries** |
| indices | faces end (after that padding) | `ni` x 1 byte |
| total | | **align16(indices end)** |

Note a polytope's vertices start at **+0x50**, where a capsule keeps capA — the
capsule's end points push its own vertices out to +0x70.

**Face entry** is `u16 firstIndex, u8 numIndices, u8 minHalfAngle`. Checked on all
76: `firstIndex` is the running sum of the counts, and the counts sum to exactly
`ni`. Counts run 3..8, most often 4.

**Vertex w** is 0.5 with the vertex index in the low mantissa byte, as in a capsule
— 2257 of 2296 slots. The other 39 are the multiple-of-4 padding slots, which
repeat an earlier index.

**What still stands between this and an encoder** (updated 07-28w):

1. **`minHalfAngle` — semantics SOLVED, exact quantization not.** It is the sharpest
   edge on the face: the minimum over the face's edges of the angle to the
   neighbouring face, halved and quantized over 0..90 degrees into a byte,
   `byte = round(halfAngle / 90 * 255)`. Correlation 0.9985 over 2120 faces, **67%
   exact and 87% within 1**. Details that mattered: key shared edges by vertex
   POSITION, not index, since the padded vertex arrays carry duplicates; all 76
   polytopes are closed manifolds, so missing neighbours are not the cause of the
   residual; and taking the minimum over all faces instead of edge-adjacent ones
   changes nothing (68.0% vs 67.4%). The residual is systematically positive —
   Havok's stored value is conservatively lower than the true minimum. Preserved
   through the decode as `HknpShape::faceAngles`, so a round trip is unaffected;
   only synthesis needs the last step.
2. ~~**Padding contents**~~ — **CLOSED 07-28x.** The vertex array is padded to a
   multiple of 4 and **every padding slot duplicates the LAST REAL vertex — its
   position and its `w` index tag both** (18 of 18 polytopes carrying padding). So
   the real vertex count is recoverable as `max index in the face loops + 1`, and
   the encoder needs no extra input. Writing each padding slot's own slot number
   into its tag — the obvious reading of "w carries the vertex index" — is what
   made 18 of 68 differ. The plane array is padded to `roundup(nf, 4)` and its
   spare slots are NOT one sentinel the way a capsule's are: `(0,0,1,0)` in 50
   slots, all-zero in 50, something else in 2. Preserved through the decode, so a
   round trip is exact; only NEW geometry needs a rule for them.
3. ~~**`hknpShapeMassProperties`**~~ — **SHIPPED 07-28w.**
   `hknpEncodeShapeMassProperties`, with an hkPackedVector3 writer. 68 of 68 vanilla
   objects round-trip: 64 byte-identical, 4 differing only in the exponent of an
   all-zero vector, which is inert and unrecoverable (zero mantissas record no
   magnitude). For NEW geometry the plane-offset volume and inertia by halfspace
   intersection are still needed; boxes are solved outright and are 28 of 37 in a
   400-file sample.
4. **The `+0x20` major-axis frame — NEW blocker, packing undecoded.** Once noted as
   "one constant value in every file seen"; over 76 objects it takes 23 distinct
   values. As four int16 over 32768 the norm lands near sqrt(3) rather than 1 and
   the largest component does not reliably saturate, so it is not four normalized
   components. Carried verbatim as `HknpShape::massMajorAxis`, which makes a round
   trip exact but leaves no defensible default for a new polytope.

Reuse `hknpEncodeCapsuleShape` as the model: same convex family, same relArray
convention, same w tagging, and validate the same way — structure byte-exact,
geometry as a distance.

### 4d. Compile every collision type — DONE 07-29g (Cloth still deferred)

Goal: write back every collision type, not just compressed meshes. Cloth is
explicitly deferred. When this was written `hknpEncodeCompressedMesh` was the
*only* encoder and the only caller was `collisiontools.cpp`, so everything else
was a one-way trip.

**Where it landed.** A 714-file stride sample of the 34,985-file mesh tree decodes
and reassembles **470 of 470 packfiles byte-identical to what Havok wrote** -- both
roots, every shape class, every constraint kind -- with zero refusals and zero
structural differences. All 37 vanilla ragdolls are in that count.

**What "done" means here, precisely.** Objects whose content is fully modelled are
written from the model: both roots, body records, capsules, spheres, convex
polytopes, compounds, mass properties, `hkaSkeleton`, ragdoll and limited-hinge
constraints, the position motor and the breakable wrapper. Objects whose content
is not reconstructible are written from their stored bytes with their fixups:
compressed meshes and their data objects, compound shape data, material tables,
scaled-convex wrappers, and constraint kinds with no encoder of their own. That
distinction is measured, not asserted -- `--roundtrip` assembles each file twice,
once with the stored bytes and once with everything re-derived, and reports both.

**Compressed-mesh derivation is deliberately NOT part of this.** Rewriting an
unedited mesh from decoded geometry would requantize it and change the file for no
gain; carrying the bytes is strictly better. An EDITED mesh does not want a rewrite
either -- it wants a fresh one, which is what `hknpEncodeCompressedMesh` already
does and what the in-game validation covered. There is no third case.

**Verified through the user-facing path**, not just the CLI: decompiling the
SetDressing billboard yields an editable mesh of 710 vertices and 1089 triangles,
which is the corrected decode exactly (it was 565 / 797 before the shared-vertex
bound landed on 07-29h).

Ordered by dependency:

1. **`hknpShapeMassProperties` — the blocker.** 43 distinct patterns in 53
   objects, so it is real per-shape data (`hkCompressedMassProperties`: centre of
   mass, inertia and major-axis space as `hkHalf` vectors, then mass and volume).
   Convex polytopes cannot be written without it. Decode this first.
2. ~~**Capsule**~~ — SHIPPED 07-28t, see 4d-spec. `hknpEncodeCapsuleShape` plus
   `collision --roundtrip`: 819 capsules, structure byte-exact on all of them.
   ~~**Sphere**~~ — SHIPPED 07-28u, see 4d-spec-sphere: 30 of 30 byte-exact.
3. ~~**Convex polytope**~~ — encoder SHIPPED 07-28x, see 4d-spec-polytope: 68 of 68
   byte-exact on a round trip. Generating a hull for NEW geometry still needs
   qhull, which is already vendored under `lib/qhull`.
4. ~~**Compounds**~~ — encoder SHIPPED 07-28z. `hknpEncodeCompoundShape`, 29/29
   byte-exact over the corpus sweep. **The first shape that is not a
   self-contained blob:** all 60 child-pointer slots hold raw zero, so the binding
   to children and to the CompoundShapeData lives entirely in the fixup tables, and
   the encoder returns an `HknpCompoundFixups` naming the slots the caller must
   patch. Layout: `0xD0 + count*0x80` bytes, instance hkArray at +0x60 (count
   +0x68, `count|0x80000000` +0x6c, LOCAL fixup -> +0xD0), CompoundShapeData via a
   GLOBAL fixup at +0xC0, instances at +0xD0 stride 0x80 (3 rotation rows,
   translation +0x30, scale +0x40, child pointer +0x50). **Still undecoded and
   therefore carried verbatim:** the header +0x70..+0xCF (holds an AABB at
   +0x80/+0x90 and more) and the four non-scale `w` slots per instance, which carry
   payloads behind a printed 0.5, plus a signed zero in row 1. Only static
   compounds remain unmeasured -- the 400-file sample found 14 dynamic and zero
   static.
5. ~~**Ragdoll**~~ — constraint encoders SHIPPED 07-29a, root + packfile assembly
   SHIPPED 07-29e: all 37 vanilla ragdolls rebuilt byte for byte.

   **The constraint objects are fixed-size templates**, which makes this far
   smaller than it looks. Over 37 ragdoll packfiles: `hkpRagdollConstraintData` 416
   bytes on all 521, `hkpLimitedHingeConstraintData` 304 on all 246,
   `hkpPositionConstraintMotor` 48 with ZERO varying words (a pure constant), and
   `hknpBreakableConstraintData` 48 with one (a break threshold at +0x20). Only
   `hkaSkeleton` and `hknpRagdollData` scale with bone count.

   **The atom chain is one fixed sequence per type**, identical on every instance:

       hkpRagdollConstraintData (fills 416 exactly)
         SET_LOCAL_TRANSFORMS(144) SETUP_STABILIZATION(16) RAGDOLL_MOTOR(96)
         ANG_FRICTION(16) TWIST_LIMIT(32) CONE_LIMIT(32) CONE_LIMIT(32) BALL_SOCKET(16)

       hkpLimitedHingeConstraintData (+ 8 bytes alignment tail)
         SET_LOCAL_TRANSFORMS(144) SETUP_STABILIZATION(16) ANG_MOTOR(40)
         ANG_FRICTION(16) ANG_LIMIT(16) TWO_D_ANG(16) BALL_SOCKET(16)

   Field map for the ragdoll type: rotA +0x30/+0x40/+0x50, pivotA +0x60 (zero on all
   521), rotB +0x70/+0x80/+0x90, pivotB +0xa0; friction +0x128; twist min/max
   +0x138/+0x13c; cone max +0x15c (min is a -100 sentinel, the bound a cone has
   not got); plane min/max +0x178/+0x17c. `RAGDOLL_MOTOR` has no varying field
   anywhere in the corpus.

   **The w lanes of the rotation basis vectors are SIMD residue, not data** --
   values in [-1,1] of the same magnitude as the rotation, third row zero almost
   everywhere, and one vanilla pivot w holding outright garbage (0x98d3b2b5). Havok
   ignores them, so a NEWLY AUTHORED constraint cannot be byte-identical to vanilla
   and does not need to be. `HknpConstraint::rawData` carries the source object so a
   rewrite preserves them; from the template alone, 518 of 521 differ in nothing
   else and one differs in a minority flag byte at +0xb0/+0x190.

   **Every constraint object now writes** (07-29b): `hkpLimitedHingeConstraintData`
   246/246 rewrite (244/246 from template), `hkpPositionConstraintMotor` 37/37 and
   parameterless -- 48 bytes of which not one word varies -- and
   `hknpBreakableConstraintData` 3/3, a single break threshold at +0x20. Hinge
   fields: friction +0xf0, min/max +0xfc/+0x100, tau constant 1.0 where ragdoll
   limits use 0.8; its pivotA is real, unlike the ragdoll type's. Every
   template-alone shortfall is accounted for: the misses are exactly the objects
   carrying the minority flag byte at +0xb0/+0x118 (2 hinges, 1 ragdoll).

   ~~**hkaSkeleton**~~ — SHIPPED 07-29c, **37/37 byte-exact rebuilt from the decoded
   bones alone**, no preserved template. Zero header; three hkArray descriptors at
   +0x18/+0x28/+0x38 (pointer, count at +8, `count|0x80000000` at +12); parent
   indices as hkInt16 at +0x90; bone records at `align16(0x90 + 2n)`, 16 bytes each;
   reference pose 48 bytes each; total exactly `pose + 48n`. **Bone name pointers are
   null on all 804** — the ragdoll's copy identifies bones by index, so no string
   table and no fixups beyond the three array pointers. Two traps: the pose **scale
   is 0.99999994, not 1** on 767 of 804 (only the 37 roots are exactly 1), and the
   header carries four **negative zeros** at +0x54/+0x64/+0x74/+0x84.

   **`hknpRagdollData` — LAYOUT MEASURED 07-29d, all 37 with zero violations.**
   Seven hkArrays, each a pointer patched by a LOCAL fixup at `+d`, count at `+d+8`,
   `count|0x80000000` at `+d+12`; payloads run back to back from +0x90 and the object
   ends where the last one does. A GLOBAL fixup at +0x78 reaches the `hkaSkeleton`.

   | desc | contents | count | stride | pad |
   |---|---|---|---|---|
   | +0x10 | body_props | bodies | 0x50 | - |
   | +0x20 | dyn_motion | bodies | 0x40 | - |
   | +0x30 | dyn_inertia | bodies | 0x70 | - |
   | +0x40 | cinfo | bodies | 0x60 | - |
   | +0x50 | constraints | **bones - 1** | 0x18 | 16 |
   | +0x60 | **shape list, NIF node order** | bodies | 0x08 | 16 |
   | +0x80 | bone -> body index (identity on all 37) | **bones** | 0x04 | 16 |

   **The `+0x80` bone-count trap, concretely:** most arrays are per-BODY, but +0x80
   is per-BONE and the constraint count is bones-1, NOT bodies-1. On 34 of 37 the two
   counts are equal so the difference is invisible; the exceptions are `SkeletonRef`
   (48 bodies / 11 bones), `skeletonSentryBodyPart` (24/9) and `TurretMountedSkeleton`
   (5/4) -- parts kits whose extra collision bodies sit outside the ragdoll hierarchy,
   the same thing the simulator's `looseBodies()` handles.

   **`+0x60` is NOT "all zero"** -- an earlier revision of this table said it was,
   because its bytes are. Every slot is patched by a global fixup, so the contents
   are the ragdoll's shape list: the same set the body cinfos name (a bijection on
   all 37) in a DIFFERENT order on 36 of them. That order is the NIF's own -- the
   sequence the `bhkNPCollisionObject` blocks appear in, exact on 35/37, the two
   others being parts kits whose spare bodies have no node. It is therefore **not
   derivable from the packfile**; `HknpSystem::shapeListOrder` records it. A
   depth-first walk of the bone tree matches on 32 of 34, which is a near-miss, not
   the rule. `cinfo+0x12` is the inverse permutation -- the body's slot in that list
   -- and NOT the material index the decoder still reads it as (harmless on
   ragdolls, which carry no material table; unchecked on physics systems).

   **ENCODER + ASSEMBLY SHIPPED 07-29e.** `hknpEncodeRagdollData` and
   `hknpEncodeRagdoll` / `hknpBuildPackfile`: **all 37 vanilla ragdolls decode and
   reassemble byte-identical to the file Havok wrote**, zero structural differences.
   With every shape re-derived instead of copied, 2/37 are still byte-exact and on
   the other 35 every differing byte lands inside a shape object -- a capsule's core
   box cannot survive a float round trip (worst vertex error 1e-06 m), which is why
   `HknpShape` now carries `rawData` and `massRawData` the way constraints do.

   Packfile-level rules, measured on the same 37: `__classnames__` at 0x100, the
   four `hkClass*` reflection names first and the rest **in order of first use**;
   `__types__` empty; objects back to back, each padded to 16, root first and
   `hkaSkeleton` last; local and virtual fixups ascending by source but **global
   fixups in member declaration order**, which puts the root's +0x78 skeleton
   pointer after the fixups for the array whose payload sits at +0x7c0; every fixup
   table padded to 16 with 0xff and carrying **no sentinel entry**; a section-header
   name NUL-padded with 0xff in byte 0x13 alone.

   **Compounds** needed three things the decode did not keep, since it flattens a
   compound into one shape per instance: the owning body, the children AS children
   (every child carries the same body, so the body alone cannot separate them), and
   the `hknpDynamicCompoundShapeData` object, carried whole. Emission order is
   compound, children, then the data -- data LAST though its pointer at +0xC0 sits
   below the child pointers, the same inversion as the root's skeleton pointer --
   and the data object's own local fixup at +0x10 -> +0x40 has to come with it.

   **PHYSICS SYSTEMS TOO — SHIPPED 07-29f.** `hknpEncodeSystem` /
   `hknpEncodePhysicsSystemData`: a 403-file stride sample of the mesh tree
   reassembles **266 of 266 packfiles byte-identical**, zero structural
   differences, one refusal (a body naming no shape). Ragdolls stay 69/69.

   The two roots are the same object; the ragdoll one adds the +0x80 array and the
   +0x78 skeleton pointer, which is why its header is 0x90 and the other's is 0x80.
   An EMPTY array still writes count 0 with the `0x80000000` flag, and dyn_motion /
   dyn_inertia have independent counts (one prop has inertia and no motion).

   **LOCAL fixups follow the same member-declaration order as globals** -- inside a
   CompressedMeshShapeData the section array's fixup sits between the +0x50 and
   +0x60 members. Sorting matched every ragdoll and no compressed-mesh system.

   **Five fields were "constant" only because 37 actor skeletons agreed:**
   `cinfo+0x18`, `dyn_inertia+0x00` and `+0x2c`, `body_props+0x0e` and `+0x10`. All
   recorded now, and body_props is carried whole and patched.

   **Class hashes come from the file's own `__classnames__`**, so a packfile holding
   a class the built-in table never sampled still rewrites.

   **470 OF 470, NO REFUSALS — 07-29g.** `hknpScaledConvexShape` decodes and
   writes (a 112-byte wrapper: shape header, child pointer at +0x30, scale at
   +0x40 -- the scale INFERRED from the slot, not validated against a render), and
   `Reader::ok` no longer truncates structural loops. That flag is sticky, every
   `&& r.ok` loop was gated on it, and one out-of-range read was silently dropping
   the last child of a 12-instance compound from the shape list, the viewport and
   the body attribution with no error anywhere. It surfaced only because the
   assembler refuses to write a compound whose child count disagrees with its
   instance count.

   **Still open:** an encoder that DERIVES a compressed mesh from geometry for
   rewriting. `hknpEncodeCompressedMesh` authors a new one and is in-game
   validated; rewriting an existing one carries its bytes, because each section
   quantizes against its own domain and the partitioning is Havok's.
6. **Round-trip validation** over the corpus — DONE 07-28y for the shipped
   encoders, and it earned its keep: a 700-file stride sample of the full 34,985
   mesh tree caught 8 polytope failures in exactly the categories the skeletons do
   not cover. Cause was the encoder writing `materialCRC` (the body-RESOLVED
   material) instead of `shapeMaterialCRC` (what sits at shape+0x18); the two agree
   on actor skeletons and differ on static architecture. After the fix: polytopes
   269/269, spheres 4/4, mass properties 269/269 (245 exact + 24 inert exponent),
   capsules 78/78 structure. Re-run this whenever an encoder changes —
   `tools/collision_roundtrip/rt_corpus.py`.

**Standing lesson, earned three times in one session:** a corpus that cannot
distinguish two hypotheses is not evidence for either. The brahmin's all-axis-aligned
capsules "confirmed" an AABB core; a small object sample "confirmed" the `+0x20`
field was constant; the actor skeletons "confirmed" the resolved material was the
stored one. Each held until the sample widened.

## 5. Block Details — remaining typed editors — ~~READY~~ SHIPPED 07-30d

- **Colour swatch + picker** — done. `ColorEdit` leads with a swatch that opens
  `ColorWheel::choose`. Alpha over a checkerboard; HDR colours factor their
  intensity out for the 0..1 wheel and multiply it back, because
  `ColorWheel::choose( Color4 )` returns its input untouched above 1.0 and the
  button would otherwise be a silent no-op on exactly the emissive colours people
  want to edit.
- **Missing-file marking** — done. `NifModel::texturePathInfo` drives both the
  Value column's red text and its tooltip from one call, trying the renderer's own
  extension list in the renderer's order.
- ~~**Texture path browse**~~ — **was already there.** `spChooseTexture`
  (`spells/texture.cpp:168`) is an *instant* spell, so every texture row has had a
  clickable browse icon in its Value column all along. Filed here in error.

*Not done:* the thumbnail-in-tooltip half. `TexCache` binds through GL, so a
tooltip cannot ask it for a preview. The route is `DDSTexturePreview`
(`ddspreview.cpp:287`), which decodes to a `QImage` entirely on the CPU — read
the file, decode small, cache the thumbnail, embed it in the rich-text tooltip as
a `data:` URI. Feasible and self-contained, but a job of its own.

**Already done, do not rebuild** (both are upstream NifSkope behaviour that had
never been checked for):

- ~~String-index derived display~~ — `nifmodel.cpp:1385` resolves
  `tStringIndex` against the header string table and returns `"<string> [<idx>]"`,
  with distinct `<invalid string index>` / `<header strings not found>` messages.
  That *is* "the resolved string beside the raw index".
- ~~nif.xml tooltips on field labels~~ — `nifmodel.cpp:1512`, `ToolTipRole` on
  `NameCol`, builds `<p><b>name</b></p><p>description</p>` from
  `NifItem::text()`, which is the nif.xml description field
  (`src/data/nifitem.h:111`, `:583`). Block-type rows also get an ancestor list.

Design detail and the **binding visual rules** (flat `Name | Value` language,
hover-revealed row controls, coloured text not badges): see
`BLOCK_DETAILS_OVERHAUL_PLAN.md`.

## 6. Block Details — array table (P4)

"Open as table" on fixed-compound arrays (Vertex Data, Triangles, bone
weights): virtualized, viewport `pickedElems` sync, per-column stats including
NaN count, CSV export. Designed as the future display layer for the Tier-3
flattened storage (#3), so the two are worth sequencing together.

## 7. Block Details — whole-file search and revert (P5)

- Whole-file value search: the existing filter box gains a This Block / Whole
  File scope toggle; results list `block · field · value` rows that jump on
  click. Deferred/coalesced walk, skips big arrays unless the query is numeric.
- Recent values per field + "revert to loaded value" (capture original on first
  edit).
- Hex viewer.

## 8. Block List — remaining items — SHIPPED 07-30e

Shipped already: search, type chips, category icons, breadcrumb/footer,
Links-to/Referenced-by peek, foldable header.

**Summary** (`wwBlockSummary` in `spells/blocks.cpp`): per-type one-liners —
counts for geometry, target for a controller, diffuse name for a texture set,
bones for a skin, blob size for a packfile. It had a COLUMN of its own until
2026-08-11c, when column 11 became the Block List’s visibility toggles
(`WwVisCol`); the text moved to the block row’s tooltip in flat list mode, and
its defect status to `NifProxyModel`’s own per-block tooltip in hierarchy mode.

**Status badges** are coloured text in that column, not chips, per the house
style: `missing texture` (reuses `texturePathInfo`) and `no geometry`.

- ~~**unreferenced**~~ — **cannot be decided in this model. Do not re-file it.**
  `getParentLinks()` is the Ptr links a block *owns*, not what points at it; the
  reverse relation is `rootLinks`, which `nifmodel.cpp:2838` builds as "every
  block nothing refers to" and then writes back over the footer's Roots array. An
  orphan and a real root are the same thing here. It needs a reverse index the
  model does not keep — a fine feature, but that one, not this one.

(**Drag-to-reparent was considered and rejected** — Set/Clear Parent cover it
safely. Do not build it.)

## 9. Rigging — leftovers

Verified 2026-07-21 against `src/spells/riggingtools.cpp`.

- **Classic NiSkin backend** (`NiSkinData`/`NiSkinPartition` path for
  Skyrim LE/SSE) — essentially unstubbed.
- **`CustomizationRemapNewBonesData`** — open; carries an unresolved
  leaked-pointer question.
- **Shader skinned-flag handling** for newly created skins — open.
- **In-game FaceGen validation** — a manual production check, not code.
- *Persistent skeleton reference* and *zero-weight bone pruning* — see #1; they
  belong to the Skeleton Manager.
- **Flatten, the parts not done** — NEW 2026-08-10j. Rest is taken only from the
  Pose Manager's capture, and only when the flatten base is the PRIMARY, because
  that is the model the capture is keyed to; a skull-marked skeleton in a
  workspace row cannot be flattened at rest. Nothing detects "posed without ever
  capturing a rest", so that state gets the same honest wording as "never posed" —
  there is no reliable signal for it and inventing one out of the undo history was
  ruled out. The flatten also has no output-path dialog: the result lands unsaved
  in Loaded NIFs and Save As is a second step.
- **Pose-follow, the parts not done** — NEW 2026-08-10i, trimmed 2026-08-11.
  A marked row follows the skull-marked document, or the primary when the primary
  has a bone hierarchy.
  - ~~*following a specific loaded row that is not the skull mark*~~ — **RESOLVED
    BY DESIGN** (user, 2026-08-11). There is exactly ONE active skeleton and that
    is the law of this workspace: the skull is single-active, and merge targeting,
    pose-follow resolution and the all-documents snap all read that one pointer.
    Followers track THE skeleton. A second way to name a follow target would be a
    second source of truth, so it is not wanted — do not re-file it.
  - Followers track only the SKELETON's node transforms, so a follower skinned to
    bones the skeleton does not have keeps its own for those (by design, and
    reported nowhere). Still open, as a report.
- ~~**Weapon slot placement**~~ — SHIPPED 2026-08-10h. Marked parts are placed on
  the connect points the meshes publish, rotation composed, chains resolved to any
  depth (`nifMergeWeaponPart`, src/nifmerge.cpp). What is deliberately NOT done:
  no live re-resolution when a part is swapped (the merge is one-way — undo and
  re-merge), and the null forwarder meshes (`HandMadeMuzzleParentObject.nif`, no
  geometry, existing only to republish a point) work but show as an empty row.
  An OMOD/ESP-driven assembler — a workbench-style menu of a weapon's legal
  parts — was considered and is **not wanted** (user, 2026-08-11): the row list
  is whatever is marked, and that is the design. The connect-point research
  behind it, including the 144 DLC meshes missing from the local unpack, is in the
  session scratchpad's `weapon_combos.md`.

## 10. UV editor — cross-mesh operators

Multi-mesh editing shipped (per-shape selection sets, cross-shape picking,
per-shape undo). Operators (merge / mirror / unwrap / pack), pins and hide
remain **active-mesh-only by design**; extending them across meshes is scope
expansion, not a gap.

Also deferred from the UV work: 3D-viewport seam marking (Ctrl+E) and Follow
Active Quads. **Proportional editing was explicitly declined — keep it that way.**

## 11. Animation / Timeline — 3 remaining gaps

**`TIMELINE_PLAN.md`'s 43 unticked boxes are all implemented** (verified by code
sweep 2026-07-21: key inspector, drag+snap, tangent handles, CSV round-trip,
lint, rubber-band multi-select, mute/lock, frames@fps, normalize, summary row,
preview-range band, easing presets, settings persistence). That file is history,
not backlog.

Genuinely open:
- **NiPSys (particle) controllers** are excluded from the Setup Controllers spell.
- **Rename sync** is lint's guided fix, not an automatic on-rename hook.
- **XYZ ↔ quaternion rotation conversion** is deliberately excluded from
  interpolation-type switching.

## 12. Rendering / engine

Rewritten 07-27 — this section said "future/disabled" while most of it shipped.
Plan and risk register: `RENDERER_MATCH_PLAN.md`.

**SHIPPED:**
- **Regression guard** — `WW_RENDER_SHOT` + `tools/render_regression/capture.ps1`,
  7 baselines, camera / scene clock / `showRefraction` / `showParticles` all
  pinned (an unpinned harness silently guards nothing — and the window size has
  to be pinned too, `WW_RENDER_SIZE`, or every compare returns "size mismatch").
- **FO4 spec/gloss on the editor's BRDF** (07-27a) — GGX + Smith + Schlick,
  `_s.R`→F0 at the dielectric 0.04, specular no longer gated on `hasSpecularMap`,
  energy conservation `(1-F)`. Channel assignment (`g`=gloss, `s`=spec) already
  matched the measured calibration.
- **`.pbrm` reader** (07-27b) — `src/io/pbrmfile.{h,cpp}`, Minimal Standard
  slice, fail-closed, CLI `pbrm` / `pbrm-resolve`.
- **Material resolution + auto-replace toggle** (07-27c), **`pbrm_default`
  program and the live mode** (07-27d), **three lighting modes** (07-27e).

**PARKED:** PBR mode and the auto-replace toggle — see §0. This is the whole
reason §12 is still open.

**Still future:** subsurface scattering. Genuinely not started.

**Unverified, needs a real asset:**
- `texAspect` resolution — `getTextureInfo( shaderProp->fileName( 0 ) )` may not
  match the cache key. Unproven; needs an 8:1 texture.
- A shape actually rendering *as* PBR — needs a `.pbrm` folder added under
  Settings ▸ Resources.

**Lightning leftovers** (07-26d shipped the controller-accurate rewrite):
mutation cadence is hardcoded at 1/24 s and amplitude decay at 0.5. Both are
guesses that looked right on screen; neither has been compared side-by-side with
the effect in game. Three *other* interpretations were disproven by rendering
and reverted — subdivisions-as-recursion-depth, Length-as-branch-length, and the
`Animate Arc Offset` re-reading. Do not re-try those without new evidence.

> **Superseded 2026-07-31 — the evidence arrived.** `WW_PDB_COMPARISON.md` §2
> decodes the engine's generator out of the 1.10.155 PDB. Amplitude decay 0.5 is
> **correct**. The 1/24 s cadence **does not exist**: `Lightning::Process` holds
> exactly three float constants (1.0, 0.5, 0.25) and no cadence; the rate comes
> from interpolator 2 (Mutation), which NifSkope does not read.
> Subdivisions-as-depth and Length-as-branch-length are **what the engine does**
> — `GetBranchVerts(s) = (1<<s)*4+4`, and
> `len = Length*0.5^gen + rand(-1,1)*LengthVar*0.25^gen`. Both were tested before
> 07-31i fixed the span rule, i.e. against a bolt drawn between the wrong two
> points, which is why they looked wrong on screen. The displacement shape
> differs too: the engine applies one random 2-D direction per span under a tent
> weight `1 - |t-0.5|*2`, not per-axis midpoint noise, and never normalises the
> amplitude series.

**OPEN 2026-07-31 — the X-01 leg arcs run inside the calf.** bungo: they should
hug the coil. Parked with what is already known, so it is not re-derived:

- The generation rule is settled and shipped (`WW_CHANGES 2026-07-31i`), read out
  of the 1.10.155 PDB: the bolt runs **from the target's origin, along its local
  +Y, for `Length`**, and the engine never resolves a node name. That fixed the
  torso. It did **not** move the legs — it changes direction and length, not the
  origin.
- The origin is the `BoltGeo_01` node, and the asset puts it on the calf bone
  axis, ~5 units in front of the coil, so the bolt runs down *through* the leg
  and only its ends clear the armour.
- The merge is not doing it. Proven three ways: the branch's local transforms are
  byte-identical to `X01_LegRight_Tesla_VFX.nif`'s; `RLeg_Calf_Armor2` has the
  same transform in the skeleton, the armour and the Tesla hardware; and the leg
  file's own `X01_LegRight_Tesla_Lightning:0` carries a node transform of exactly
  minus that bone's translation — a hand-written cancel, i.e. the author knew the
  file attaches there.
- **The next measurement, and the one that decides it:** where `BoltGeo_01` ends
  up *in game* versus in our merge. That separates "the asset is like this" from
  "something still moves it". Everything cheaper has been done.
- Decoded but not adopted: the engine's cross-section is **two crossed strips,
  four verts a ring** (`GetBranchVerts(s) = 4·2ˢ + 4`, `GetBranchTris(s) = 4·2ˢ`,
  `2ˢ` segments), not a camera-facing ribbon. Ours has mitred joints, arc-length
  UVs and texture-aspect tiling that the engine's does not — a visual change with
  no placement benefit.
- **ANSWERED 2026-08-01 — the engine applies no transform at all.**
  `BGSNamedNodeAttach::AttachPolicy::Process` (1.10.155 `0x175710`), reached
  through `AttachPolicy::vftable+8`, is the whole placement rule and it is four
  steps: resolve the argument with `BSUtilities::GetObjectByName`, fall back to
  the root if that finds nothing, `parent->AttachObject( obj, true )`, then
  `BSShaderUtil::InvalidateRenderPasses`. It is a plain scene-graph re-parent —
  nothing is offset, aligned or zeroed, so an attached ArtObject sits exactly
  where its own local transform puts it relative to the named node.

  That closes this question against the merge: our merge does the same thing, and
  the three proofs above already showed the branch transforms are byte-identical
  to the donor's. **The arcs run inside the calf because the asset puts
  `BoltGeo_01` there.** Moving them is an authoring change, not a bug fix — so if
  bungo still wants them hugging the coil, the honest fix is to edit the node, not
  to hunt the merge. The in-game measurement is no longer needed to decide it.
- Tools that make this cheap to resume: `WW_SHOT_TEST=<png>` with
  `WW_SHOT_VIEW=front|back|left|right|top`, `WW_SHOT_TIME`, `WW_SHOT_AT=x,y,z`,
  `WW_SHOT_DIST`; `WW_BOLT_DEBUG=1` logs each bolt's resolved span to
  `release/ww_bolt_debug.log`; `nifskope-cli world` prints world transforms so two
  files can be diffed by name.

## 13. CLI follow-ups

Headless batch mode shipped 2026-07-21e (`src/nifcli.cpp`, docs in `CLI.md`):
`spells / info / list / dump / get / set / cast`, verified end to end. Open
follow-ups, roughly in value order:

- ~~**Parameterise Setup Controllers.**~~ **DONE 2026-07-21f** — logic moved to
  `AnimSetup::setupControllers()` (`src/spells/animationsetup.h`), dialog and
  CLI are both callers, exposed as `anim-setup`. Verified end to end; the
  generated graph passes `Animation/Fix Invalid AV Object Refs` unchanged.
  **GUI verification of the dialog is pending.** The same split is available
  for the other dialog-driven animation spells if they are ever wanted
  headless: Remove From Animation, Duplicate Sequence, Scale Sequence Times,
  Bake B-Spline To Keys.
- **Keyframe I/O from the CLI** — write key arrays from CSV/JSON. The data path
  is plain model arrays and the timeline's CSV round-trip already proves it.
  Pairs naturally with the item above and with the existing Animation lint
  spell, which can verify the result in the same run.
- **Port the `WW_*_TEST` harnesses onto it.** **There are 22, not eight** — the
  count in this file was frozen at 07-21 while 14 more were added (the whole
  `WW_POSE*` family, `WW_MERGEARCH`, `WW_OSPOSE`, `WW_CREATESKIN`, `WW_RENDER*`,
  `WW_PERF`, `WW_UI_SHOT*`). They hand-roll load → act → verify → quit in
  `nifskope_ui.cpp`; most of that is now CLI calls plus a script. Still the
  single biggest available reduction in that file's bulk — and now a
  medium-sized job rather than a small one. Note the GL-dependent ones
  (`WW_RENDER*`, `WW_POSEDRAW`, `WW_UI_SHOT*`) cannot move: `-no-gui` has no
  context, and `-platform offscreen` crashes on context creation (exit 139).
- **JSON output** (`--json` on `info`/`list`/`dump`/`get`) for machine
  consumption. Only worth doing once something actually consumes it.
- **MCP server** — deferred on purpose. A CLI is already drivable from an agent
  shell, which is most of the value. MCP earns its keep only for driving a
  *live* NifSkope, which means extending `IPCsocket::execCommand`
  (`main.cpp:235`, currently a five-line `if` understanding one command) into a
  real protocol. Decide after living with the CLI.

One CLI gotcha worth keeping here: a **running** NifSkope swallows filename
arguments over the UDP IPC handoff and exits 0, so a probe run silently opens the
file in the live session and your harness never fires. Always pass
`--port <unused>` when scripting.

## 14. Repo hygiene — ~~NEW 07-27~~ DONE 07-30

Both halves closed, checked rather than assumed: `src/collisiontools.cpp` no
longer exists (the live file is `src/spells/collisiontools.cpp`), and the working
tree is clean. Original notes below for the record.


- **`src/collisiontools.cpp` is a committed orphan.** It is *not* in
  `NifSkope.pro`; the live file is `src/spells/collisiontools.cpp` (2391 lines,
  Jul 26) and the orphan is the pre-move copy (2372 lines, Jul 11, last touched
  by `d5765c4`). Editing the wrong one compiles clean and changes nothing —
  exactly the kind of silent trap that costs an afternoon. Delete it, or move it
  under a `attic/` path that is obviously not built. It is the **only** such
  orphan: an exact-path diff of the `.pro` against `find src -name '*.cpp'`
  turned up nothing else.
- **~54 files uncommitted** across 07-25..07-27 (skin, lightning, spec/gloss,
  the whole PBRM stack, the regression harness). Committing has never been
  authorised in-session; the working agreement in `CURRENT_STATUS.md` still
  applies.

## 15. Viewport — Separate By Material / By Loose Parts — ~~NEW 07-27~~ SHIPPED 07-30c

Both greyed-out entries are live. `separateSelection` became `separateShapes`, an
N-way core taking a grouper that returns a per-triangle group id; group 0 stays in
the source and each other group becomes a sibling, so Selection, Loose Parts and
Segment are one implementation.

**By Material was not a missing implementation — it was impossible.** A NIF shape
carries exactly one shader property, and the FO4 segment structures carry no
material field either (`BSGeometryPerSegmentSharedData` is User Index / Bone ID /
cut offsets), so there is no per-face material anywhere in the format, on any file.
The entry ships as **By Segment**, the only per-face grouping a NIF has. Do not
re-file By Material.

**By Loose Parts connects by vertex POSITION**, not index — a NIF splits vertices
at every UV/normal seam, so index connectivity fragments a garment into dozens of
pieces. Positions compare bit-exactly (`-0.0` folded onto `0.0`).

Not to be confused with `glview.cpp:15032` **Clear Parent Inverse**, which is
disabled *permanently and correctly* — NIF scene objects store no Blender-style
parent-inverse matrix. That one is in the declined list, not the backlog.

---

## Verified SHIPPED — do not rebuild

Each of these was listed as open in some plan doc and is not. Confirmed against
the code on 2026-07-21, extended 2026-07-27.

| feature | evidence |
|---|---|
| Mirror editing (X) | `GLView::mirrorEditing`, `mirrorPairCache`, context-menu toggle |
| Checker deselect | `GLView::checkerDeselect` + redo panel, W ▸ Specials |
| Repeat Last (Shift+R) | `GLView::repeatLastOperator`, `viewport.repeat_last` |
| F9 panel at cursor | `viewport.panel_to_cursor` |
| Rip (V) / Split (Y) | `glview.cpp` "Split (Y) / Rip (V)", in-place undo |
| Bevel (Ctrl+B) | `GLView::bevelSelection` — needs **GUI testing**, not code |
| Rest-pose display | `Node::viewTrans`/`worldTrans` → `restWorldTrans()` |
| Euler rotation editing | `valueedit.cpp` `mEuler` |
| Tris to Quads (Alt+J) | `GLView::trisToQuads`, works from any select mode |
| Entire timeline v2 checklist | see #11 |
| Stock **Vertex Flags** spell is correct | tested + disproven landmine, `WW_CHANGES 2026-07-21a` |
| Pose mirror / paste-flipped | `PoseMirrorButton`, `ogl->poseMirrorBone()` — `posetools.cpp:192`, `:491` |
| String-index derived display | `nifmodel.cpp:1385` → `"<string> [<idx>]"` — **stock** |
| nif.xml tooltips on field labels | `nifmodel.cpp:1512` `ToolTipRole`/`NameCol` ← `NifItem::text()` — **stock** |
| Proportional editing (O / Shift+O) | `glview.cpp:184`, 8 Blender falloff curves, self-test at `:6327` — **the decline was reversed** |
| NifItem slab pool (perf T3 batch 1) | `nifitem.h:300` + `nifitem.cpp` — the 07-27 sweep first mis-read this as unbuilt |
| Multi-section collision encoder | `hknpencode.cpp:207` — spatial-slab partitioning, cap is now **4096 sections** |

## Explicitly declined — do not implement

- ~~**Proportional editing** (Blender O)~~ — **this decline was reversed.** The
  user later asked for it and it shipped in 07-22i (all 8 Blender falloff
  curves). The stale entry survived here for five days. Left visible on purpose:
  a decline is a snapshot of one conversation, not a permanent law, and this file
  has to be re-read against the change log rather than trusted.
- **Drag-to-reparent** in the Block List — Set/Clear Parent cover it safely.
- **Curated sections / header card** (Block Details P1) — the user chose "keep
  the existing tree, improve it in place" instead.
- **Loop Cut "Even" and "Correct UVs"** — deliberately not carried.
- Draw sorting (T2.13) and particle frustum culling (T2.14) — see #3.
- **Clear Parent Inverse** (`glview.cpp:15032`) — not a gap. NIF scene objects
  store no Blender-style parent-inverse matrix, so the action is permanently
  disabled with a tooltip saying so.
- **Clean-room rewrite for legal independence** — asked and answered 2026-07-26:
  clean-room requires never having read the original, which is already false.
  WW Edition stays an acknowledged BSD-3 derivative. Do not re-open.

## Awaiting verification (not implementation work)

**The working agreement changed on 07-22 and again on 07-26; the old line here
("the agent does not run GUI sessions") is wrong.** Current rules:

- The **agent** verifies GUI behaviour itself, via the `WW_*_TEST` harnesses and
  the CLI — not by handing over a checklist.
- **No interactive GUI launches while the user is working** (07-26). Screenshot
  harnesses are fine; `SetForegroundWindow` never is — it is refused from a
  background process anyway, and one attempt captured the user's Blender window
  instead of NifSkope. Always `--port <unused>`.
- The **user** owns in-game validation. That is the one thing no harness covers.

**Outstanding, no automated coverage:**
- **Bevel** — highest risk, still has no harness (`bevelSelection` is reachable
  only from the key dispatcher and the context menu). Test on a copy.
- The 07-20d..o batches (flag dialog, Block Details batch 1 + diff/Reference
  column, viewport batch, Loop Cut v1→v3), 07-21b pinned fields, performance
  batches 1–3.

**Needs the game, not a harness:**
- Multi-section compiled collision — an in-game walk test.
- Lightning cadence + decay (§12).
- FaceGen output from the rigging tools.

---

## 2026-07-27 verification record

What was actually checked, so the next sweep can trust or re-test it rather than
guess. **Confirmed still open** (grep evidence, all zero-hit unless noted):
Skeleton Manager (`plannedWorkspaces`), prop staging, colour swatch/picker
(`ColorEdit` is spin boxes), texture browse, array table, hex viewer, whole-file
search, Block List summary column + badges, classic `NiSkinData` backend (a lone
`TBD` comment at `riggingtools.cpp:302`), `CustomizationRemapNewBonesData`,
shader skinned-flag, NiPSys controllers (`controllerOptions` covers
BSEffectShader / BSLightingShader / NiAlphaProperty / NiLight / NiAVObject only),
rename-sync-is-a-guided-fix (`isNameMismatch` in `runLint`, `timelineedit.cpp:1563`),
XYZ↔quaternion conversion, perf 15c flattened storage, per-triangle face
materials, `--json`, keyframe CLI I/O.

**Not re-verified** — carried forward on the 07-21 sweep's word: the individual
07-20d..o GUI batches, the `TIMELINE_PLAN.md` 43-item sweep, the Blender-batch
SHIPPED table rows above, and the `PERFORMANCE_PLAN.md` measurements behind the
15c/16 deferral. If any of those matter to a decision, re-check before relying
on them.

---
---

# APPENDIX — Collision Manager feature spec

Preserved verbatim; this is the only copy. Technical handoff (validated
packfile offsets, PyNifly corrections, test assets, phasing, gotchas) is in
`COLLISION_MANAGER_HANDOFF.md`; the interactive UI mockup is
`docs/collision_manager_mockup.html`.

## Collision Manager — CONCEPT (2026-07-09, supersedes "Collision workflow" 07-08)

Third manager dock after Timeline and Mat/Tex Manager. One place to see,
create, edit, and compile collision. Factory `tlCreateCollisionManagerDock`
in a new `src/spells/collisiontools.cpp` (same pattern as
`tlCreateMatTexManagerDock` in meshtools.cpp).

**Implementation status (2026-07-10): functional workflow landed.** The
dock, live compiled/editable browser, row selection sync, Decode/Decode All,
Create Collision, viewport/block-list right-click entry points, Decimate via
the live preview, guided linter, amber budget warnings, donor import,
mass-from-material, Collision -> BSTriShape, snapshot undo and a validated
single-section compressed-mesh compiler are working.
Compiled collision has translucent solid + type-wire display with persisted
Solid/X-ray/Colour-by settings. Multi-section/compound encoding and
per-triangle face-material painting remain in P4.

### Existing building blocks (all in-repo, all working)
- `hknpdecode` — full packfile decoder incl. per-body physics
  (layer/friction/restitution/mass/COM), Body ID binding validated
- Decode / Decode All Compiled Collision spells (legacy-chain output with
  authored physics values)
- `spCreateCVS` "Create Convex Shapes" — BSTriShape/NiNode -> bhkBoxShape
  (auto box detect) / bhkConvexVerticesShape (+ bhkTransformShape,
  bhkListShape, bhkRigidBody, bhkCollisionObject), with **CoACD** convex
  decomposition and its own settings dialog
- vendored libs: **qhull** (hulls), **miniball** (optimal bounding sphere),
  **CoACD** (approx convex decomposition), **meshoptimizer** (decimation,
  used by simplify.cpp)
- decode spell's `buildShape` already builds the bhkNiTriStripsShape mesh
  chain and primitive shapes
- PyNifly's `bhk_autopack.py` — working, game-proven packfile WRITER
  (reference for the encoder; our validated corrections: body_props stride
  0x50, layer at cinfo+0x1C, dyn_motion/dyn_inertia layout)

### Panel A — Collision browser (top)
Tree of every node with collision, one row per bhk(NP)CollisionObject:
node name | state (COMPILED packfile / EDITABLE legacy / mixed) | shape
summary (Box, Hull(24v), Mesh(1.2k tris), Compound(5)) | layer | material |
static/dynamic | mass. Selecting a row selects the block and highlights it
in the viewport (amber preview already draws compiled collision). Buttons:
Decode, Decode All, **Compile** (encoder), Compile All, Delete, Copy To
(pick target node).

### Panel B — Create collision (from selected BSTriShape / NiNode)
Target type: Box (min OBB via PCA + wrapped bhkTransformShape) | Sphere
(miniball) | Capsule (PCA axis + radius, the missing primitive fit) |
Convex Hull (qhull, existing path) | Convex Decomposition (CoACD, existing
path) | Triangle Mesh (buildShape's bhkNiTriStripsShape chain).
Preset: Static / Prop(dynamic) / Custom — writes the validated authoring
values (dynamic: MotionSystem 3, Quality 4, SolverDeact 2; static: 5/0/1;
PenDepth 0.15 etc.). "Replace existing" checkbox.
- **Collision type** dropdown (UI name for the Havok Filter layer): all 57
  Fallout4Layer entries from nif.xml (STATIC 1 ... CHARBUMPER 56).
- **Material** is a SEARCH FIELD (added 2026-07-09): an editable combo —
  type a full or partial name and the 157-entry Fallout4HavokMaterial list
  filters live (QCompleter, ContainsMatching, case-insensitive). A name
  that matches nothing becomes a **custom material**: FO4 material values
  are CRC32 hashes of the name — poly EDB88320, **init 0, no final xor,
  lowercased** (verified: crc("materialwoodcrate")=341181474,
  crc("metal")=104858580 — legacy Material_Metal/_Wood hash from the short
  names "metal"/"wood"). Show the computed CRC next to the field; unknown
  CRCs elsewhere display as Custom(0x...). Custom names are remembered
  (QSettings list) and appear in future searches. Same hashFunctionCRC32
  the obj importer already uses.
- **User presets** (added 2026-07-09): a 💾 button saves the current create
  setup — collision type, material, and all physics values — as a named
  preset next to the built-in Static/Prop entries. Persist via QSettings
  ("CollisionManager/Presets/<name>"), delete via right-click on the entry.
- Buttons are the round-trip pair: **BSTriShape → Collision** (primary
  create action) and **Collision → BSTriShape** side by side.
- **Decimate Collision** button (added 2026-07-10): Blender-style geometry
  reduction for the selected collision mesh or collision-source BSTriShape,
  using the vendored meshoptimizer path already used by `simplify.cpp`.
  The redo panel exposes Ratio and Target Triangles, Preserve Boundaries,
  and a live before -> after vertex/triangle count. Apply it before creating
  Triangle Mesh / Convex Hull / Convex Decomposition collision, or to an
  editable collision proxy; compiled collision prompts **Decode to edit**.
  Preserve node transforms, material/layer, and rigid-body physics, rebuild
  bounds/topology, reject a degenerate result, and wrap the operation in a
  `NifSnapshotCommand` so it is fully undoable.

### Panel C — Physics properties (3ds Max parity, from 07-08 agreement)
Bound to the selected rigid body: Mass, Friction, Restitution, damping,
max velocities, gravity factor, layer, material, motion/quality/solver/
deactivator, COM override. Live edit of the legacy blocks; greyed out for
COMPILED rows with a "Decode to edit" hint (or auto decode-edit-recompile
later).

### Workflow accelerators (agreed 2026-07-10)
- **Right-click entry points**: block list BSTriShape/NiNode -> Havok ->
  Create Collision opens Panel B's logic with the active/last-used preset;
  the viewport object-mode menu gets Create Collision plus contextual Decode
  Collision / Compile Collision when the clicked object has collision.
- **Collision linter**: Check Collision uses the timeline-lint guided-fix
  pattern. Flag visible geometry with no collision, mixed compiled/editable
  state, suspicious hull vertex counts, near-box hulls that should be boxes,
  non-uniform primitive scale, STAIRHELPER bodies without a slope, and
  dangling bhkNPCollisionObjects. All listed checks are implemented; safe
  fixes cover dangling objects and layer-zero inference in one snapshot.
- **Import collision from donor NIF**: choose a file and collision node, copy
  its collision to the active node with transforms adjusted. Compiled donor
  collision may be decoded immediately for editing.
- **Mass from material**: density presets per material family times computed
  shape volume. Show the derived density and mass; allow manual override.
- **Snapshot undo for Compile/Decode**: wrap both structural operations in
  `NifSnapshotCommand` so the entire round trip is one safe undo step.
- **Collision budget**: manager footer totals collision vertices/triangles and
  packfile bytes, with amber thresholds for meshes worth decimating. Raw totals
  plus thresholds and per-row warnings are implemented.
- **Per-triangle mesh materials (P4)**: face-material painting backed by
  hknpBSMaterialProperties once compressed-mesh encoding supports its material
  list.

### Collision <-> BSTriShape round trip
- **Collision -> BSTriShape**: decoded (or legacy) shape geometry to a
  BSTriShape under the same node, so the full mesh toolset (edit mode,
  separate/join, transforms, mirror) can edit it. Spell + manager button.
- **BSTriShape -> collision**: Panel B on the edited mesh, "replace
  existing" — closes the loop decode -> mesh-edit -> rebuild -> compile.

### Viewport display (agreed 2026-07-09): solid, not wireframe
Collision draws as **translucent solid + wire overlay** (Havok Visual
Debugger style), unmistakable next to lit/textured BSTriShapes:
- Fill pass: tris at ~30% alpha, unlit, depth test ON / depth write OFF
  (overlaps blend), polygon offset vs the render mesh, backfaces first and
  darker (volume cue without lighting)
- Wire pass on top at full alpha (current wireframe becomes the overlay)
- Toggles: **X-ray** (fill ignores depth - see through walls),
  **Collision only** (hide render geometry); persisted display options
- Primitives get triangulated proxies (icosphere/capsule) for the fill;
  the synthesized analytic rings stay as their wire pass
- **Colour by** dropdown (one hue channel, so it's a mode):
  **Material** (default: bucket Fallout4HavokMaterial enum names by
  substring into ~12 families - wood browns, metal blue-grey, stone greys,
  glass pale cyan, dirt/gravel ochre, cloth purple, flesh pink...; tooltip
  shows exact material name) | Layer | State (compiled/editable) | Type
- **Wire colour always encodes shape type** regardless of mode: white box,
  yellow hull, cyan sphere/capsule/cylinder, magenta mesh, orange compound
  instance. Selection highlight overrides, as now.
- Manager browser rows show fill+wire swatches = the palette legend
- Compiled and legacy paths route through ONE shared two-pass helper
  (also unifies today's amber-vs-layer-colour inconsistency)
- **Material labels in the viewport** (added 2026-07-09): toolbar mode
  Off / **Selected (default)** / All — a small colored dot + material name
  (halo text, QPainter overlay like the snap indicator) anchored at each
  shell's top center. Declutter rules so it never gets busy: skip labels
  for shapes whose projected screen radius < ~40 px, skip when labels
  would overlap (keep the nearer shape), cap ~20 labels, and "All" only
  applies while Show Collision is on. CRCs reverse-looked-up to names,
  unknown -> Custom(0x...).

### Selection info in the bottom bar (added 2026-07-09)
The main window has no QStatusBar in use — add one. Selecting collision
anywhere (viewport shell click, manager row, block list) shows a one-line
readout: `Crate01 — Box · STATIC(1) · MaterialWoodCrate · COMPILED · 0 kg ·
fric 0.50 rest 0.40`. Material CRCs reverse-looked-up to names (unknown ->
Custom(0x...)). Clicking the readout focuses the manager row. Driven by the
same selection signal the manager uses.

### Transformable collision (added 2026-07-09)
Collision must move/rotate/scale in the viewport exactly like BSTriShapes
(the existing object-mode G/R/S tools). Where the transform writes, by case:
- **Whole body** (shell/row selected): write the OWNING NODE's transform —
  that IS the body placement (Body ID binding). Synergy: Set Origin
  (Shift+Ctrl+Alt+C) already works on plain NiNodes, so it repositions
  collision pivots too.
- **Sub-shape** in a bhkListShape: write (create if absent) its
  bhkConvexTransformShape / bhkTransformShape wrapper Matrix4 — Havok units
  (game translation / 69.99125).
- **Primitives**: sphere center via wrapper translation; capsule axis via
  First/Second Point; uniform scale -> Radius. Non-uniform scale on
  primitives is refused (Havok can't represent it).
- **Mesh shapes**: keep transform in the wrapper while dragging; offer
  "bake into vertices" (NiTriStripsData, game units) on apply.
- **Compiled systems are read-only**: G/R/S on a compiled shell prompts
  "Decode to edit" (one click, then the gizmo continues).

### Encoder: `hknpencode` — compile packfiles WITHOUT Elric
C++ port of PyNifly's packer with our corrections. `src/gl/hknpencode.{h,cpp}`
(paired with hknpdecode) + "Compile Collision" spell:
legacy bhkCollisionObject tree(s) on a node set -> one bhkPhysicsSystem +
per-node bhkNPCollisionObject with Body ID (the binding we validated).
**Status 2026-07-10:** the native writer, manager Compile action, structural
replacement, snapshot undo, and in-process encode -> decode validation are
implemented for one compressed-mesh section (<=255 verts/tris), including
dynamic/static physics, layer, material, density and AABB inertia. Remaining:
multi-section meshes, shared multi-body systems/compounds, primitive-specialized
writers, per-triangle material painting, FileConvert and in-game validation.
1. FixupBuilder (local/global/virtual tables, 0xFF terminators) + classnames
   section writer + 0x40 header / 3 section headers — layouts fully known
2. PSD writer: body_props (0x50/body: friction/restitution trunc-f16 at
   +0x12/+0x14dup/+0x16), BodyCInfo (0x60: shape ptr fixup, layer +0x1C,
   qualityId 0xFF, body index +0x1A, motion idx +0x0C, pos +0x30, quat
   +0x40), ShapeEntry; dyn_motion + dyn_inertia when dynamic (inverseMass,
   density = mass/volume, inertia diagonal — compute from shape like Elric)
3. Shape writers: hknpConvexPolytopeShape (verts w=0x3F000000|idx, planes,
   faces, fvi — inverse of our decoder), hknpSphereShape; compressed mesh
   single-section (<=255 verts/quads) first, multi-section later;
   hknpDynamicCompoundShape for lists/instances (phase 2);
   hknpBSMaterialProperties for mesh materials (phase 2)
4. **Validation, no game needed**: (a) round-trip decode(encode(legacy)) ==
   decode(elric_output) field-by-field on the controlled pairs
   (Documents/3dsMax/export); (b) corpus: encode(decode(vanilla)) ->
   decode == original decode over the 300-file architecture corpus
   (extend tools/batch_validate.py); (c) FileConvert.exe must accept our
   packfiles (it rejects malformed ones — free structural linter)
5. In-game smoke test last: known prop with our compiled collision

### Phasing
- **P1 encoder core**: convex/box/sphere static single-body + spell +
  validation harness (unlocks the full author-in-NifSkope loop; everything
  else is UI around it)
- **P2 manager dock**: browser panel + wire existing spells + physics panel
- **P3 create/convert**: capsule fit, mesh chain button, collision<->
  BSTriShape round trip
- **P4 heavy formats**: compressed-mesh encoder, compounds/instances,
  dynamic props with computed inertia, materials, multi-section meshes

