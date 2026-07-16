# NifSkope — Wild Wasteland Edition: Change Log

## 2026-07-16 — Viewport menus move into the viewport (floating bottom bar)

Follow-up to the header-menu batch below: the Select/Add/Object (and edit/
paint) menu buttons leave the render toolbar and now live in a floating
Blender-dark rounded bar centered on the 3D viewport's bottom edge. Like the
redo panels, the bar is a frameless `Qt::Tool` window (`WA_ShowWithoutActivating`,
`WA_TranslucentBackground` for real rounded corners) because the native GL
viewport paints over child widgets. It's glued to the viewport by the same
event-filter hooks as the redo panels (Move/Resize), hides on minimize,
returns on restore, and first shows via a deferred call after the main
window's first Show. Buttons: flat text, hover highlight, selection-blue
open state, no menu-indicator arrows (Blender look). Mode swapping and the
shared `GLView::populate*Menu()` builders are unchanged;
`NifSkope::positionViewportMenuBar()` centers it (redo panels keep the
bottom-left corner).

## 2026-07-16 — Viewport RMB menu retired; Blender-style header menus

The 3D viewport no longer has a context menu. A plain right-click (click, not
an RMB zoom drag) now drops the gizmo / 3D cursor on the surface under the
mouse — what Shift+RMB used to do. Shift+RMB is retired (both press handlers
removed); RMB-drag zoom and RMB-cancel of the box/circle select gadgets are
unchanged (box-cancel now also suppresses the gizmo drop on release, like
circle-cancel already did). The keyboard menu key opens the W quick menu.

Everything the RMB menu carried moved to Blender-style viewport header menus —
flat text buttons right of the mode selector on the render toolbar, swapping
with the mode:

- **Object mode:** Select · Add · Object (transform/snap/set origin/duplicate/
  join/parent/show-hide)
- **Edit mode:** Select · Mesh (transform/snap/origin/extrude/duplicate/
  separate/symmetrize/normals/floating decal/show-hide/delete) · Vertex
  (merge/remove doubles/smooth/dissolve) · Edge (loop cut/subdivide/edge
  slide) · Face (extrude/inset/fill-bridge)
- **Paint modes:** Select · Weights/Paint/Segments (fill selection + show/hide)

The buttons rebuild their menus on aboutToShow from new
`GLView::populate*Menu()` functions; the W quick menu shares them (a Transform
section — Move/Rotate/Scale — now heads W in both modes, hidden while
painting), so the entry points cannot drift apart. The block-data spell
submenus (Mesh/Havok/…) left the viewport entirely — they remain on the Block
List / block-tree context menus, which are untouched. The old RMB-menu-only 3D
Cursor items were already covered by the Snap… (Shift+S) menu. Implementation:
`NifSkope::contextMenu`'s graphicsView branch deleted, `contextMenuEvent`
rewritten to place the cursor, `contextMenuShiftModifier` removed (GLView
layout change — qmake6 re-run).

## 2026-07-16 — Skyrim/FO76 rows: FINAL fix (hidden-row state rots between clicks)

The user was right and the stale-instance theory was wrong. Reproduced with a
probe sweep that clicks every BSLightingShaderProperty through the real Block
List path: the FIRST block visited hides correctly (29/29 rows), every
SUBSEQUENT block leaks all 29 — matching the screenshot exactly (their block
32 vs the probe's block 10; earlier probes only ever tested the first block).

Root cause: QTreeView keeps hidden rows as QPersistentModelIndexes, and model
activity during a block switch silently INVALIDATES them (no modelReset — the
reset-override never fired). The hiding pass ran and verified 29/58 hidden at
click time (trace-proven), then the stored set rotted before paint.

Fix: `NifTreeView::doItemsLayout()` override re-derives row hiding for the
current root right before the view rebuilds its layout (re-entry-guarded,
skipped while the model is loading). Whatever invalidates the stored set, the
rows are re-hidden with fresh indexes before anything is drawn. Sweep now
reports 29/29 hidden on every shader block via the real click path.

## 2026-07-16 — Row hiding on expansion (tree mode); Skyrim/FO76 rows re-verified

User re-report: greyed Skyrim + FO76 rows visible on a shader property.
Probe-verified on the CURRENT build against the same file: all version-gated
rows (Skyrim flag variants, GTEFO76 data, Num SF1/SF1) ARE hidden — the
screenshot matches the pre-fix row set, i.e. a stale running NifSkope
instance (the app hands off to a running instance, so opening a NIF can
silently reuse an old process; fully close all windows after an update).
One real gap found and fixed anyway: the root-scoped hiding pass never
covered whole-model TREE mode (invalid root) or rows first revealed by
expanding — an `expanded` hook now re-applies hiding for whatever becomes
visible. Probe now logs row types alongside names.

## 2026-07-16 — W: Blender-style Specials quick menu

`W` over the viewport (user request) opens the 2.7x-style Specials menu — the
one-key hub for the modeling operators (hover-out AutoCloseMenu, like the
other operator popups). Guarded so W stays camera-forward in free-camera /
walk mode and ignored while a text field has focus.

- **Edit mode:** Subdivide, Smooth Vertices…, Merge…, Remove Doubles…
  (= Merge by Distance with the redo-panel default), Dissolve Vertices |
  Extrude Region…, Fill / Bridge…, Inset Faces…, Edge Slide… | Flip Normals,
  Recalculate Normals, Symmetrize… | Hide Selection, Reveal All, Invert
  Selection. Selection-dependent items grey out. (Loop Cut is deliberately
  absent — it needs the cursor on an edge, so it stays on Ctrl+R.)
- **Object mode:** Add Primitive…, Duplicate, Join | Snap…, Set Origin….

## 2026-07-16 — Modeling tools batches 3-5: eight operators at once

Everything remaining from `MODELING_TOOLS_PLAN.md` except Bevel (deferred:
correct tri-mesh corner terminations are a mesh-corrupter risk; better zero
than wrong). Prior work committed first as d5765c4. All BSTriShape-only,
Processing-batched writes, area-weighted normal refresh, Redo Panel v2.

- **Loop Cut (Ctrl+R)** — ring walk from the edge under the cursor across
  tri-pair "quads" (diagonal chosen by opposite-tri validity + most-parallel
  continuation; a/b chain orientation propagated); ladder re-triangulation
  (2(C+1) tris per quad, winding matched to the original face normal); new
  ring verts are true row interpolations (`tlWriteLerpVertex`: UVs, normals,
  top-4 bone weights). Panel: Number of Cuts (1-64). No slide in v1 — select
  the new ring and use Edge Slide.
- **Edge Slide (Shift+V)** — the selection slides along its unselected
  neighbor edges; the panel's Factor (-1..1) IS the modal. Positions via
  ChangeValueCommand transaction + in-transaction normal refresh.
- **Subdivide** (menu) — midpoint split of the selected edges/faces
  (1/2/3-marked-edge tri splits, winding preserved).
- **Inset Faces (I)** — region inset via the extrude plan machinery (dup
  boundary + rim band), duplicates pulled inward perpendicular to the region
  normal. Panel: Thickness / Depth (armed at 0 — scrub to inset).
- **Dissolve Vertices (Ctrl+X)** — each interior vert's fan is removed and
  its 1-ring re-capped (ear clip); boundary/non-manifold verts skipped with a
  count. Also cleans up extrude scaffolds.
- **Symmetrize** (menu) — mirror across a local axis with seam weld: keeps
  one side, snaps near-plane verts onto the plane, mirrored copies with
  flipped winding + mirrored normals/tangents. Panel: Direction (6-way enum —
  first Enum consumer of Redo Panel v2) / Merge Distance. v1: crossing
  triangles are dropped, not bisected; bone weights copy unmirrored.
- **Flip Normals / Recalculate Normals** (menu) — winding reversal of the
  selected faces / area-weighted vertex-normal recompute, both as single
  ChangeValueCommand transactions.
- **Add Primitive (Shift+A, object mode)** — Plane / Cube / Cylinder / UV
  Sphere as a new BSTriShape at the 3D cursor, cloned from the active shape's
  vertex layout + shader/alpha properties (skinned templates rejected), with
  normals/tangents/UVs generated. Panel: Size / Segments.
- New shared machinery: `TlShapeStateCommand` (generic in-place undo for
  arbitrary single-shape rewrites — snapshots the Vertex Data + Triangles
  subtrees as typed values via tlCaptureValues/tlRestoreValues; no model
  reload), `TlExtrudeCommand` generalized to an apply closure (Inset reuses
  it), `tlPushPositionCommands`, `vertexOpTarget` shared validation.
- DEFERRED (explicit): Bevel; in-place undo for Delete/Merge/Duplicate (still
  snapshot-flash on Ctrl+Z); old Merge/Select-Linked panels not yet migrated
  to Redo Panel v2.

## 2026-07-16 — Modeling tools batch 2: Fill (F) + Bridge Edge Loops

`MODELING_TOOLS_PLAN.md` Phase 2, the connection cluster. One smart operator
(`GLView::smartConnect`, `F` in edit mode with the pointer over the viewport —
routed via eventFilter so `F` stays Front View elsewhere; also in the context
menu as "Fill / Bridge…"):

- **Rim extraction** (`tlExtractLoops`): explicit edge picks if present, else
  mesh boundary edges (exactly one adjacent non-degenerate face) with both
  endpoints selected; chained into ordered loops DIRECTED ALONG THE HOLE
  (reverse of the adjacent surface winding, so caps/bands wind correctly).
  Non-manifold rims (3+ rim edges at a vertex) are rejected with a message.
  Scaffold/degenerate triangles are ignored throughout.
- **Fill** — ONE closed loop selected: ear-clip cap in the loop's best-fit
  plane (Newell normal, dominant-axis projection, reflex/containment ear test,
  fan fallback for pathological rims). Adds n−2 triangles, no new verts.
  Redo panel: `Flip Normals`.
- **Bridge Edge Loops** — TWO loops (both closed rings or both open runs):
  band of triangles between them. B is aligned to A by nearest start vertex
  (+`Twist`) and sampled-distance direction choice; equal-count loops band as
  quads, unequal counts zip by normalized arc length. `Number of Cuts` inserts
  interpolated rings (equal counts only — clamped otherwise and by the 65,535
  vert budget): each ring vertex is a true row interpolation via
  `tlWriteLerpVertex` — position, UV, normal/tangent renormalized, **bone
  weights merged/top-4/renormalized**. Redo panel: Cuts / Twist / Flip Normals.
- **In-place undo** via the new shared `TlMeshGrowCommand` (append-only ops:
  undo shrinks the arrays and restores saved rim normals + Data Size — no
  snapshot, no reload flash, panel scrubbing included).
- All writes under Processing (no dataChanged storms), one per-shape
  notification; affected normals recomputed area-weighted.

## 2026-07-16 — Extrude: in-place undo (no more "model turns off" on Ctrl+Z)

Follow-up to the user's undo-flash report: Extrude no longer uses a
whole-model snapshot for undo.

- **TlExtrudeCommand** (glview.cpp): redo applies the extrude plan in place;
  undo shrinks the vertex/triangle arrays back, restores the re-pointed region
  triangles, moved interior-cap positions, refreshed normals (pre-captured in
  the constructor), Data Size and bounds — instant, no `nif->load()` reload,
  no visible flash. Both the E-key path and the redo-panel re-run push this
  command, so **scrubbing Move X/Y/Z in the Extrude panel is now flash-free**
  too. (Delete/Merge/Duplicate still snapshot — same conversion is possible
  per-op later if their undo flash bothers.)
- **Stale-pick sanitation**: `refreshPickedElementPositions` now REMOVES picks
  whose vertex/triangle indices no longer exist (an in-place undo shrinks
  arrays under a live selection) instead of merely skipping their position
  refresh.

## 2026-07-16 — Skyrim rows on FO4 NIFs: the REAL fix (model reset wipes hidden rows)

Third attempt at this bug (user: "Skyrim properties are still visible"), and
this time root-caused with the probe harness rather than patched at a call
site. The predicate (`NifTreeView::isRowHidden`) was always right, and the
hiding pass DID run and apply (verified: 29 of 58 rows hidden on the X01
BSLightingShaderProperty right after selection) — but **a model reset lands
during load completion and `QTreeView::reset()` silently clears ALL
hidden-row state**, un-hiding everything after the fact.

- `NifTreeView::reset()` override: after every model reset, a deferred
  `refreshRowHiding()` re-applies hiding once the root/current are restored.
  Also covers snapshot undo/redo (`nif->load()` → modelReset → reset).
- New public `NifTreeView::refreshRowHiding()`: whole-block re-apply that
  defers itself while the model is loading/processing (the old inline pass
  silently bailed on `state != Default` and stayed stranded, because a later
  select() of the same block skips setRootIndex).
- Safety nets so no path can strand again: `completeLoading` → refresh (tree +
  header), `NifSkope::select()` same-root branch → refresh, `currentChanged` →
  refresh.
- Diagnosis trail preserved: WW_EXTRUDE_TEST probe now also dumps row-hiding
  state (predicate vs actual view) for the first BSLightingShaderProperty, and
  nifview.cpp has env-gated `wwHideTrace` logging to ww_hide.log.
- Bonus probe measurement: the vertex-delete now takes ~0.6 s on the 600-vert
  probe shape (was 10 s before yesterday's Processing fix).

## 2026-07-15 — Vertex picking: no more picking through opaque geometry

Regression from the floating-vert pick fix (user report): the screen-space
nearest-vertex search ignored depth, so back-side vertices could steal a click
through the surface. `nearestScreenVertex` now occlusion-tests each candidate
that would become the winner: one raycast at the vertex's own screen position,
distance compared along the view ray (a vertex ON the hit surface passes the
0.1%+0.01u tolerance, so ordinary surface verts and floating spur verts in
front both still pick; an occluded candidate is skipped so a visible
second-best can win). X-ray mode (Alt+Z) deliberately keeps picking through.

## 2026-07-15 — Topology ops: 10× freeze fix (dataChanged storms) + tolerant normals

User reports: deleting a vertex froze NifSkope for seconds; the "Could not find
Normal subitem" warning reappeared when extruding after a delete.

- **Freeze root-caused and fixed, measured via the WW_EXTRUDE_TEST harness**:
  compacting the packed vertex array emits a dataChanged per leaf write
  (~15 per moved row × thousands of rows), and every live view reacted to each
  one — the UV editor re-read all UVs and rebuilt islands *per write*.
  One deleted vertex on a 600-vert probe shape: **10,058 ms → 1,110 ms** after
  wrapping the mutation in `setState(BaseModel::Processing)` (the proven
  writeLiveUVs pattern) with a single per-shape dataChanged at the end.
  Applied to: deleteGeometry, mergeVertices, duplicateElements,
  tlExtrudeApplyPlan, Rip UV Faces, Smart UV Project. This also eliminates all
  mid-mutation view reactions — the prime suspect for the condition-cache
  poisoning behind the Normal warning (getItem() gates on evalCondition, cached,
  and BSVertexData rows delegate to row 0's same-position child via
  getConditionCacheItem — a mid-op false evaluation would stick).
- **Normal lookups made tolerant**: tlAccumulateAreaNormals /
  tlRecalcNormalsSubset / tlPushNormalCommands now probe rows with the
  NON-reporting `getItem( row, "Normal" )` and skip incomplete rows instead of
  spamming parse warnings.
- Probe harness extended (delete-then-extrude, shape selected, event loop
  pumped between ops): appended rows remain fully structured in every
  model-level sequence tried — the poisoning needs live-view timing that the
  Processing wrap now prevents outright.
- KNOWN (deliberate, documented): Ctrl+Z of a topology op is a whole-model
  snapshot restore — the scene visibly reloads for a moment. Fix would be
  in-place undo commands per op (planned as a follow-up, like UVEditCommand).

## 2026-07-15 — Extrude round 3: pick floating verts, self-edge fix, probe harness

User reports: can't select extruded (spur) vertices; parsing warning
"Vertex Data [2863]: Could not find Normal subitem" when extruding from an
extruded vertex.

- **Floating verts now pickable**: vertex picking raycast the surface first, so
  a spur vert floating in FRONT of the mesh handed the pick to the triangle
  behind it. `nearestScreenVertex()` (extracted from the off-surface fallback)
  now competes with the hit triangle's corner — whichever is closer to the
  cursor on screen wins. Fixes selection of extruded verts everywhere.
- **Self-edge fix**: the scaffold triangle (v, v', v') exposed a degenerate
  (v', v') edge to the induced-edge scan, so extruding the extruded vert built
  garbage walls instead of a spur. Degenerate triangles are now excluded from
  region detection, induced edges, winding lookup, and explicit edge picks —
  extruding the new vert correctly pulls out another spur, and extruding
  {v, v'} together ribbons the scaffold edge into a real quad (Blender flow).
- **Normal-subitem warning investigated with an in-app probe harness**
  (`WW_EXTRUDE_TEST=1 NifSkope.exe <file>` → ww_extrude_test.log, temp code in
  nifskope_ui.cpp createWindow): appended Vertex Data rows are fully structured
  (all 15 children, conditions evaluate, Normal/UV/Vertex accessible) through
  the full extrude sequence, two chained extrudes, and a snapshot undo/redo
  reload, on the user's own X01_Torso.nif. The corruption path was the removed
  commit-time consolidation (snapshot undo interleaved with the move's open
  ChangeValueCommand transaction) from the previous build. KEY LEARNING:
  `getItem(name)` requires `evalCondition(item)` (cached) — if a warning like
  this reappears, suspect condition-cache poisoning, and re-run the probe.

## 2026-07-15 — Extrude fixes: vertex extrude + no reload flash (user feedback)

- **Single-vertex extrude now works** (was rejected). Blender's vertex extrude
  pulls out a bare edge; NIF has no loose edges, so each extruded vert becomes
  a **zero-area scaffold triangle (v, v', v')** — it draws as exactly the new
  edge line in wireframe, a later edge extrude of it stitches real wall quads
  (the scaffold provides the induced edge), and Merge by Distance drops it as
  degenerate. Mixed selections work: verts on extruded edges become walls,
  isolated verts become spurs.
- **"Model disappears and reloads" after extrude FIXED**: the commit-time
  "consolidation" undid + re-ran the extrude/move snapshots, and snapshot undo
  reloads the whole model (visible flash). Removed. Instead the chained move's
  commit now pushes area-weighted **normal-recalc ChangeValueCommands into its
  own transaction** (`tlPushNormalCommands` / shared `tlAccumulateAreaNormals`)
  — normals are correct for the final cap position with no reload, and the
  extrude+move remains two clean undo steps. The redo panel's snapshot re-run
  (which does reload) now only happens when a value is actually adjusted.

## 2026-07-15 — Modeling tools batch 1: Redo Panel v2 + Extrude Region (E)

First batch of `MODELING_TOOLS_PLAN.md` — the fork's first *geometry-creating*
operator, plus the generalized panel system every later operator rides on.

### F0.a — Redo Panel v2 (generalized typed operator parameters)
- `GLView::TlOpParam` { Float / Int / Bool / Enum, label, value, range, step,
  enum names } + `armOperatorPanelEx( title, params, undoSteps, seed )`,
  `reapplyOperatorEx( params )` (undoes the whole gesture — possibly several
  undo entries — restores the seed selection, re-runs via the op-provided
  `lastOpExRerun` callback, re-counts the entries it pushed), signal
  `operatorPanelEx`. Stale-guarded by undo index like the existing panels.
- New floating panel `OperatorExRedoPanel` (nifskope_ui.cpp): 8 recycled rows,
  each Float/Int → DragSpinBox, Bool → QCheckBox, Enum → QComboBox; same
  redoPanelQss styling, collapsible header, freeze-on-stale, mutual-exclusion
  and bottom-left positioning as the other three panels (all loops updated).

### Phase 1 — Extrude Region (`E`, edit mode; context menu "Extrude Region…")
- Blender semantics: region-of-faces extrude duplicates ONLY the boundary
  verts, re-points the region faces onto the duplicates (surface detaches as
  the moving cap; interior verts ride along), stitches outward-facing side
  walls (verified winding: edge a→b in cap winding → tris (a,b,b')+(a,b',a')).
  Edge runs (explicit edge picks, or mesh edges induced by vertex picks)
  extrude to a ribbon; loose verts are rejected (NIF has no loose edges).
- Flow: E → geometry created in one snapshot → cap selected → chained modal
  move (full gizmo modal: axis constraint, snap, numeric). Commit or Esc arms
  the "Extrude Region and Move" panel (Move X/Y/Z world + Flip Normals) and
  immediately **consolidates**: undoes the extrude + move and re-runs both as
  a single snapshot with the offset applied inside the op — one Ctrl+Z per
  extrude, and the new wall/cap normals are recomputed for the final
  positions (`tlRecalcNormalsSubset`, area-weighted, packed ByteVector3;
  layouts without a Normal field no-op).
- Plan/apply split (`tlExtrudePlanBuild` read-only → `tlExtrudeApplyPlan`
  inside `nifSnapshotOp`) because a snapshot op always pushes — invalid
  selections abort before any mutation. 65,535-vertex budget guard.
  BSTriShape (FO4) only; one mesh per extrude (v1). Row duplication carries
  UVs/weights/colors; `tlUpdateBounds` refreshes bounds.
- World→local offset via the scene node transform (`tlWorldToLocalDelta`,
  documented approximation for deformed skinned cages).

## 2026-07-15 — UV editor: full simultaneous multi-mesh editing (last Phase 2 item)

The final UV-editor plan item: selection and editing now span every mesh in the
edit session, Blender multi-object style. Previously `selVerts`/`selEdges`/
`selFaces` were active-shape-only; non-active shapes drew as dimmed dead
wireframes. All in `src/uvtools.cpp`.

**Architecture** — per-shape selection state lives in `UVShapeData` (selVerts/
selEdges/selFaces/viewport3DVerts/hiddenFaces/hiddenVerts/pinnedVerts). The
editor's existing members remain the ACTIVE shape's live working copy (so the
~100 operator references stay untouched); `stashActiveSelection()` /
`adoptActiveSelection(s)` swap them on active-shape switches; for non-active
shapes the stored sets are authoritative in place.

- **Picking** — `pickShapeAt` finds the closest vertex (else edge body, else
  face body) across all shapes; clicking another mesh's UVs makes it the active
  shape (Blender). Plain click is exclusive across all shapes; extend keeps
  others. `L` select-linked also hops to the shape under the cursor.
- **Box select** — applies to every shape (all modes incl. islands + sticky).
- **A / Deselect / Invert / mode switch** — loop all shapes; derived edge/face
  sets rebuilt per shape via the extracted `uvDeriveEdgesFaces`.
- **Transforms** — G/R/S gather `XVert{shape,idx,…}` from every shape's
  selection; live writes notify per shape; commit pushes one `UVEditCommand`
  per shape wrapped in an undo-stack **macro** (one Ctrl+Z per gesture). The
  adjust-panel re-apply (Move/Rotate/Scale) regroups per shape the same way.
  Pivot spans the combined selection; island welds rebuilt per touched shape.
- **Sync** — `syncSelectionFromViewport` buckets `pickedElems` by shape both
  ways; `pushShapeSelectionToViewport(s)` pushes one block (GLView's
  `setElementSelectionExternal` merges per block); frame-selected spans all.
- **Rendering** — every edit-session shape draws its full selection state
  (fills, colored wires, edge highlights, dots); active shape additionally
  shows active-vertex emphasis, pins, and the stretch overlay.
- **Scoped active-only by design** (documented): operators (merge, mirror,
  unwrap, pack, …), pins, hide/reveal — they act on the active mesh; click a
  mesh to make it active first. Extending operators cross-mesh is a possible
  follow-up.

## 2026-07-15 — UV editor: sticky selection (merged seams move as one point)

User request: merged UVs should *behave* connected — moving one must not tear
the seam back open. Implemented Blender's **Sticky Selection (Shared Location)**
in `src/uvtools.cpp`:

- `UVShapeData::coPosVerts` — per-shape map of co-located mesh vertices (split
  seams), built in `loadShapeUVs` by bucketing quantized 3D positions then
  confirming exact equality (same scheme as Stitch). Static per topology.
- **Sticky picking** — in vertex/edge modes, picking or box-selecting a UV also
  selects co-located partners sitting at the same UV spot (±1e-5, transitive via
  `expandSticky`), so they move/rotate/scale together. Face mode intentionally
  unaffected (Blender behaviour). Toggleable "Sticky" button next to Sync
  (persisted `UVEditor/StickySelection`, default on); turn off to pull
  coincident UVs apart individually.
- **Island welding** — island detection (`uvRebuildIslands`, extracted from
  `loadShapeUVs`) now also unions co-located verts whose UVs coincide: a merged
  seam becomes ONE island for island-select/L/pack/average. Islands recompute on
  every reload (undo/ops) and after transform commits, so separating a seam
  splits them again live.
- Structural: closed the anonymous namespace after `UVEditCommand` so the new
  helpers share file scope with `readShapePositions` (was an ambiguous overload).

## 2026-07-15 — Shading menu: Material Contributions / Viewport Effects as text toggles

- The Material Contributions buttons (Diffuse, Normal, Specular, …) no longer
  render as filled blue buttons with icons. They are now plain text entries
  (`Qt::ToolButtonTextOnly`, transparent background) whose **active** state is
  the highlight: **blue fill (#4772b3) + orange text (#ff9d00)**, hover #555555.
  Shared `channelToggleQss` in `nifskope_ui.cpp`.
- Refraction / Particles under Viewport Effects converted from checkmark menu
  items to the same text-toggle widgets (full-width, one per row) so the whole
  dropdown uses one presentation. All behaviour (settings persistence, scene
  flags, Shift-solo on contributions, per-mode enable/disable) unchanged.

## 2026-07-15 — UV editor: redo-panel consistency patch (design, scrubbing, transforms)

User feedback on the first adjust-panel iteration: colors/UI didn't match the 3D
viewport's redo panels, values couldn't be scrubbed by dragging, and transform
gestures (G/R/S) had no panel at all. Full consistency patch in `src/uvtools.cpp`:

- **UVDragSpinBox** — exact clone of the 3D redo panels' DragSpinBox: hold LMB on
  the value and drag left/right to scrub (Shift = fine), plain click to type,
  hover reveals ‹ › step arrows, margin clicks step, scrub highlight overlay.
- **Panel redesign** — same QSS as `redoPanelQss` (#2f2f2f body, #202020 border,
  #cccccc labels), collapsible bold "˅ Title" header (uvSetPanelTitle /
  uvTogglePanelCollapse clones), grid of right-aligned labels + 150px fields,
  bottom-left at 10px like `positionRedoPanel`. Stale gestures now **freeze** the
  panel's inputs (3D behaviour) instead of hiding it; re-arming re-enables.
- **Transform redo panels** — committing a G/R/S gesture arms the panel like the
  3D viewport's transformGesture panel: Move (dU/dV), Rotate (Angle°), Scale
  (Scale U/V, axis-constrained gestures fill the untouched axis with 1). Re-runs
  recompute absolute targets from the gesture's original UVs + pivot
  (`lastOpXVerts`/`lastOpPivot`) after undoing the previous commit.
- **Unwrap (Angle Based)** gained a Margin parameter (island border + spacing,
  default 0.02) and its own adjust panel; `unwrapSelection( float margin )`.
- Operator plumbing generalised to multi-param (`QVector<float>`, kinds 1-8, spec
  table in the dock glue); `commitTransformUndo` now reports whether it pushed.
  Removed the eager `checkOperatorPanelStale` (3D panels check lazily on re-run).

## 2026-07-15 — UV editor: 0-1 tile outline + menu consistency with the 3D viewport

- **0-1 tile outline** — Blender-style soft white border (`0.90/0.92/0.95 @ 0.5α`,
  grid line width) drawn around the unit UV tile under the wireframes, so the
  working space reads even with an image filling it edge-to-edge.
- **Menu consistency** (user report: UV editor menus differed from the 3D
  viewport pop-ups in design, color and features). The UV editor's operator
  pop-ups (Merge / Snap / Unwrap / Mirror-Align / Layout Tools) now match the
  3D viewport's exactly:
  - `UVAutoCloseMenu` — a local clone of glview.cpp's `AutoCloseMenu`: closes on
    hover-out (46 px apron, 60 ms poll), Blender-style, instead of lingering.
  - `addSection()` title headers ("Merge", "Snap", "Unwrap", …) like the 3D ones.
  - Naming: snap items now use "Selection to Cursor" / "Cursor to Selected"
    wording (was "Selection → Cursor"); proper "…" ellipsis everywhere (was
    "..."), and "…" consistently marks operators that pop the adjust panel
    (By Distance…, Smart UV Project…, Pack Islands…, Minimize Stretch…) —
    same convention as the 3D viewport's "By Distance…"/"Select Linked by Angle…".
  - Feature parity: inapplicable items are now shown disabled instead of
    silently doing nothing (snap selection items without a selection, Cursor to
    Active without an active vert, Relax/Stitch/Copy without a selection);
    merge menu gained the 3D menu's separator-before-By-Distance layout.

## 2026-07-15 — UV editor: adjust-last-operation panel (operator redo)

The UV editor's parameterized tools ran with hardcoded values and no way to tune
them (user report: "no popup for merging vertices … to select distance"). Added a
Blender-style **adjust-last-operation panel**: a floating overlay in the canvas'
bottom-left corner that appears after the operator runs and **re-runs it live**
as the value is edited (undo previous result → restore the operator's original
selection → re-run with the new parameter). All in `src/uvtools.cpp`.

- Covered operators: **Merge by Distance** (Distance, default = ½ grid step),
  **Minimize Stretch** (Iterations, default 20), **Pack Islands** (Margin,
  default 0.01), **Smart UV Project** (Angle Limit, default 66°).
- Safety: the gesture is guarded by the undo-stack index — any unrelated stack
  change (a transform, Ctrl+Z, a spell) disarms and hides the panel instead of
  corrupting the stack. A re-run that pushes nothing (e.g. distance too small to
  merge anything) is remembered so the next adjustment doesn't undo a foreign
  command. Rebuilds/mode switches also disarm (seed selection no longer valid).
- Smart UV Project is a whole-model snapshot op: its re-run undoes the snapshot
  (model reload) and synchronously rebuilds the editor before re-projecting.
- Plumbing: `applyUVEditUndoable` now reports whether a command was pushed;
  `mergeSelection` takes an optional distance, `packIslands` an optional margin;
  new `armOperatorPanel` / `reapplyUVOperator` / `cancelOperatorPanel` /
  `checkOperatorPanelStale` + `operatorPanelCb`/`resizedCb` dock callbacks.

## 2026-07-15 — 3D grid decade crossfade + UV editor Phases 3 & 4

Big feature batch. Build green, startup smoke-tested. **Not yet GUI-tested by the
user** — in particular the two *mesh-modifying* ops (Rip/Split, Smart UV Project)
change topology and want in-app + in-game verification before further work stacks
on top. Multi-mesh simultaneous editing (the one remaining Phase 2 item) is **not**
in this batch — it's a core selection-model refactor, deliberately sequenced after
this batch is validated.

### 3D viewport grid — full Blender-style decade crossfade
- The orthographic grid no longer snaps between 1/2/5×10ⁿ "nice" steps (which
  popped at each threshold). It now draws **three power-of-ten levels** whose
  brightness crossfades with the sub-decade zoom position (`levelAlpha = {1-frac,
  1, frac}`, smoothstep-eased). Lines shared between levels (every 10th, every
  100th) are reinforced by alpha-over blending, so the fine/minor/major hierarchy
  emerges continuously with **no popping** across a decade boundary. Axis lines are
  drawn opaque over the faded grid. `src/gl/glscene.cpp` (ortho path).

### UV editor — Phase 3 layout tools (`UVs ▸ Layout Tools…`, context menu)
- **Pack Islands** — shelf-packs the selected (or all) islands into the 0-1 tile.
- **Average Islands Scale** — equalises per-island texel density (uv-area/3D-area).
- **Minimize Stretch (Relax)** — uniform-Laplacian relaxation of the selected face
  region's interior UVs, boundary pinned (20 iterations).
- **Stitch** (`V`) — welds selected UVs that share a 3D position across a seam to
  their average, re-joining separated islands.
- **Select Overlapping UVs** — flags faces whose UV triangles genuinely overlap
  (Sutherland-Hodgman intersection area; edge-adjacent faces skipped), sweep-pruned.
- **Copy / Paste UVs** — by vertex index; paste onto the same or identical-topology
  meshes.
- **Show Stretch Overlay** — per-face area-distortion heatmap (blue = compressed,
  green = even, red = stretched), bucketed & drawn over the island fill.

### UV editor — Phase 4 unwrap / topology
- **LSCM pinning** — `P` pins the selected UVs (shown red, Blender-style), `Alt+P`
  unpins, **Invert Pins**, **Unwrap (Live, Pinned)** solves LSCM holding pins fixed
  at their current UVs (no re-pack). `lscmSolveComponent` gained a caller-supplied
  pin map, falling back to the auto extreme-pair when <2 pins are in a component.
- **Rip / Split** (`Y`) — duplicates the vertices shared between the selected faces
  and the rest of the mesh, freeing the selection into its own island. FO4
  BSTriShape only; packs skin weights inline so the duplicated rows carry weights
  automatically (no skin-partition resync). One whole-model snapshot undo.
- **Smart UV Project** (Unwrap menu) — region-grows charts by inter-face normal
  angle (66°), auto-splits shared verts along chart borders, LSCM-solves each chart
  on the original positions, density-normalises and shelf-packs into 0-1, and writes
  it all (grown vertex array + UVs + remapped triangles) in one snapshot. FO4 only.
- **Export UV Layout to PNG** — renders the wireframe of all loaded shapes (active
  brighter) to a transparent PNG at texture resolution via QPainter; save dialog.
- Topology ops refresh the 3D viewport (`ogl->updateScene()`) and rebuild the editor
  afterward; pins reset on data rebuild; all new geometry code lives in
  `src/uvtools.cpp` (no `glview.h` change, so no full-rebuild/stale-Makefile risk).

## 2026-07-15 — Viewport A key routed by pointer (reliable), focus guard fixed

- **`A` in the 3D viewport now works every time the cursor is over it**, not
  "sometimes". `A` was handled only in `GLView::keyPressEvent`, so it needed
  the viewport to hold keyboard focus — and focus-follows-mouse via
  `QEvent::Enter` alone misses when focus is taken while the pointer is already
  inside the viewport. `A` (select-all toggle) and `Alt+A` (deselect) are now
  routed by pointer-over-viewport in the qApp event filter, exactly like G/R/S,
  so they no longer depend on focus. Text fields still keep the key.
- Also removed a wrong guard (`!ogl->isActive()`) from the Enter focus handler
  — `QWindow::isActive()` tracks the top-level window, not viewport focus, so
  it was suppressing the hover-focus; the handler now focuses on entry
  whenever a text field isn't being edited.

## 2026-07-15 — Focus-follows-mouse for the 3D viewport

- Hovering the **3D viewport** now gives it keyboard focus, so `A` and other
  `keyPressEvent`-based shortcuts fire without a prior click — matching the UV
  editor (which already grabbed focus on `enterEvent`). Implemented in the
  qApp event filter: on `QEvent::Enter` for the embedded GL window it focuses
  the viewport container and calls `ogl->requestActivate()` (the same call the
  free camera uses to take keys), skipping while a text field is being edited.
  Many viewport shortcuts already worked on hover via the filter's
  pointer-over-viewport routing; this covers the focus-dependent ones too.

## 2026-07-15 — Instant UV Undo, themed block-list nav arrows

- **UV Undo/redo is now instant** (Blender-like), no more disappear-then-
  re-render. UV edits were being stored as whole-model snapshots, whose
  Undo did a full `nif->load()` → `modelReset` → editor clear → deferred
  rebuild, so the UVs blanked for a frame. Replaced with a lightweight
  `UVEditCommand` that stores only the changed vertices' old/new UVs and
  patches them in place (same direct `set<HalfVector2>`/re-resolve-by-vertex
  path as the live drag — so it's robust against the stale-index/round-trip
  issues that made the original per-vertex `ChangeValueCommand` collapse the
  layout, without reloading the model). The editor refreshes via the command's
  `dataChanged`; the undo-stack rebuild now fires only after a *structural*
  (model-reloading) Undo, detected by the editor having been cleared.
- **Block List back/forward buttons re-iconed.** They used Qt's
  `SP_ArrowBack`/`SP_ArrowForward` standard icons, which render solid black and
  clashed with the dark toolbar. They now use themed grey chevron glyphs
  (`tlMakeIcon` "chevron_left"/"chevron_right").

## 2026-07-15 — UV editor Phase 2 (operators): merge, mirror/align, hide-faces, projections, bounds

- **Merge (`M`)**: At Center (single average), At Cursor, By Distance (weld
  verts within ~½ the grid step to their group average). UV-space only —
  topology untouched, one Undo step.
- **Mirror / Align (`Ctrl+M`)**: Mirror U/V about the pivot (bbox center /
  median / 2D cursor per the Pivot selector); Align U/V (collapse selection to
  its mean column/row); Straighten to Left / Bottom.
- **Hide / reveal faces (`H` / `Alt+H`)**: UV-local face hiding, honoured by
  drawing and picking in every mode (a vertex is hidden only when all its
  faces are). Reset on rebuild; independent of the 3D edit-mode hidden set.
- **Projections** added to the Unwrap menu (`U`): Cube (dominant position
  axis per vertex), Cylinder (angle + height), Sphere (longitude + latitude),
  alongside the existing Angle-Based Unwrap and Project From View.
- **Constrain to Image Bounds** toggle (bar "Bounds", persisted): clamps every
  UV edit (drags + operators) into the 0-1 tile. **Round to Pixels** context
  action snaps the selection to texel corners.
- Context menu gains a **UVs** submenu (Merge / Mirror-Align / Unwrap-Project /
  Round to Pixels / Hide / Reveal) so the operators are discoverable without
  the shortcuts. All operators respect Object Mode (read-only) and the
  whole-model-snapshot Undo path.
- Proportional editing intentionally **not** implemented (per request).

## 2026-07-15 — Version-mismatched rows actually hidden; brush starts painting

- **Real cause of Skyrim rows showing on FO4 NIFs found + fixed.** The
  `isRowHidden` version check was correct, but `NifTreeView::setRootIndex`
  never re-ran `updateConditionRecurse` for a newly shown block — only the
  current field's subtree was refreshed on `currentChanged`, so sibling fields
  kept stale (visible) row states when switching blocks. `setRootIndex` now
  re-applies row hiding over the whole block, so version-mismatched fields
  (e.g. Skyrim shader-flag variants, `Num SF1`/`SF1`, GTEFO76 data — all with
  verconds false for BSVersion 130) are hidden on Fallout 4 NIFs. Verified in
  nif.xml: those fields' verconds (`#BSVER# < 130`, `#BS_132_139#`,
  `#BS_GTE_F76#`) are all false at BSVersion 130, and `flags()`/`evalCondition`
  already grey them, confirming `evalVersion` returns false for them.
- **Clicking the "Brush" toggle now starts painting.** Previously it only
  toggled brush-vs-select when a paint mode was already active, so clicking it
  in the Vertex Paint / Rigging workspace (before pressing the manager's Start
  Painting) did nothing. It now starts painting from the open manager's Start
  button (Vertex Paint or Weight Paint) when no paint mode is active; if a mesh
  (or bone) isn't selected yet it reverts and shows a status hint.

## 2026-07-15 — UV texture colour fix, edge/fill visibility, brush visibility, version-mismatched rows

- **UV editor texture no longer desaturated.** The underlay colour-conversion
  mode now matches the legacy UV editor exactly by BSVersion: FO4 (bsver 130)
  shows the diffuse **raw** (mode 0) instead of sRGB-compressing it (mode 1,
  which double-encoded and washed it out); the diffuse is sRGB-compressed only
  on Skyrim SE (≥151); normals use BC5 UNORM/SNORM reconstruction. `UVTexSlot`
  now carries the real `colorMode` per slot; custom underlays display raw.
- **UV edges/wires now clearly visible** over both the dark checker and a
  bright texture (unselected edges lightened to a bright cool grey), and the
  faint island fill bumped up slightly.
- **Selected faces fill in every select mode** (Blender behaviour): any face
  whose three UV corners are all selected gets the orange fill, derived from
  the vertex ground truth — so selecting in Vertex/Edge/Face/Island all fill.
- **Paint "Brush" toggle visible in the paint workspaces.** It now appears
  whenever the Vertex Paint or Rigging manager dock is open (not only once a
  paint stroke mode is active), so it's present as soon as you're in the paint
  context; it still sits between the element-select buttons and Deformed.
- **Version-mismatched fields are hidden (Skyrim rows on FO4 NIFs).**
  `NifTreeView::isRowHidden` skipped the version check for fields with a type
  condition, so version-conditioned typed fields (the Skyrim shader-flag
  variants) showed on Fallout 4 NIFs. A field that fails its version condition
  is now always hidden, regardless of the row-hiding mode.

## 2026-07-14 — Weight/Vertex/Segment "Brush" toggle re-iconed

- The paint/select **Brush** toggle button (`ViewportWeightPaintBrushButton`,
  shown in the viewport toolbar only while a paint mode is active) still used
  `:/btn/skinned`; it now uses the greyscale brush glyph so it reads as an
  actual brush. This is the button the earlier "give the brush a proper icon"
  feedback was really about (previously the mode-selector icon and Deformed
  button were changed instead). It appears only in Weight/Vertex/Segment Paint
  modes and can land in the toolbar's ">>" overflow on a narrow window.

## 2026-07-14 — UV island fills, grid-under-image, redesigned greyscale mode icons

- **UV editor: grid hidden when an image is loaded.** With a real texture
  underlay bound, the subdivision grid is suppressed (Blender shows the image,
  not the grid); the checker/empty background still shows the grid. A loaded
  image is also displayed brighter (0.96 vs the checker's 0.75).
- **UV editor: subtle white island fills.** Every visible UV face now gets a
  faint translucent-white fill (Blender's face theme colour) so islands read
  as solid shapes instead of bare wireframe; selected faces keep the brighter
  orange fill on top. Object-mode read-only views fill per-shape in that
  shape's colour. Grid lines further subdued to a cool, uniform subtle grey.
- **Redesigned viewport-mode icons (all greyscale `tlMakeIcon` glyphs):**
  Object = isometric cube, Edit = wireframe triangle with vertex handles,
  Vertex Paint = triangle with per-vertex grey dots, Weight Paint = a proper
  paintbrush (slim handle, ferrule, tapered bristle tuft), Segment Paint = a
  shape split into greyscale segments. One consistent family, accurate to each
  mode.
- **"Deformed" cage button re-iconed.** It used `:/btn/skinned`, which read as
  a brush sitting next to the word "Deformed" and showed in Edit Mode too. It
  now uses a distinct greyscale deformation-lattice glyph (`mode_deform`), so
  nothing brush-like appears outside the weight/vertex-paint context.

## 2026-07-14 — UV undo corruption fix, workspace default, mode-menu greyscale, ortho grid

- **UV Undo no longer collapses the layout.** UV edits (G/R/S gestures, snap,
  unwrap) now Undo as **whole-model snapshots** (`NifSnapshotCommand` /
  `nifSnapshotOp`) instead of per-vertex `ChangeValueCommand`s — completely
  robust against the index-staleness / value round-trip issues that could put
  every UV on a single point. A gesture snapshots the model at start (before
  the live writes) and pushes before/after on commit; snap/unwrap wrap their
  raw writes in `nifSnapshotOp`. Since snapshot Undo reloads the model
  (firing `modelReset`), the editor now rebuilds itself and re-syncs the
  selection on any `undoStack` `indexChanged`. Trade-off: an Undo/redo briefly
  reloads the model rather than patching values in place.
- **Open in the Default workspace.** A newly opened NIF now always starts in
  the Default workspace (was: restored the last session's workspace).
- **Workspace menu marker fixed.** The active-workspace indicator is derived
  from which manager dock is actually visible on every menu open
  (`aboutToShow`), so a dock closed via its own X (or the exclusive-visibility
  logic) can't leave a stale radio dot on the wrong entry; the active entry is
  also bolded for a clearer highlight than the small dot.
- **Material Contributions menu icons greyscaled.** The few colorful resource
  icons (Diffuse, Vertex Color, Specular, Glow, Reflections) are desaturated
  and lifted to the toolbar's grey tone so the channel mixer reads as one
  consistent greyscale system.
- **Ortho grid zoom consistency.** The planar grid's minor subdivisions now
  crossfade by on-screen spacing (`drawGrid` gained a `minorFade`): they fade
  out as they get dense toward a "nice number" re-base and back in afterwards,
  softening the whole-grid pop when zooming. (A full Blender-style decade
  crossfade of the major lines is a larger follow-up if still wanted.)

## 2026-07-14 — UV editor feedback batch 3: Blender look, focus/keys, Ctrl+X freeze fix, brush icon

- **Ctrl+X freeze in Weight/Segment Paint fixed.** `fillRiggingWeightSelection`
  / `fillSegmentPaintSelection` each emit a stroke whose commit serializes the
  whole model into an Undo snapshot; under keyboard autorepeat (or a double
  delivery) those stacked synchronously and froze the UI. Now guarded by a
  `paintFillPending` flag, run deferred via `QTimer::singleShot(0)` so they
  can't re-enter the key event, and the GLView key handlers skip autorepeat
  (`event->isAutoRepeat()`), matching the eventFilter path.
- **UV editor key reliability (A / G / R / S).** Added focus-follows-mouse:
  `enterEvent` grabs keyboard focus when the pointer enters the canvas (unless
  a text field is mid-edit), so the single-key shortcuts fire on hover without
  a prior click. Plain **A** now toggles select-all/deselect-all (second press
  clears), matching the 3D viewport; Alt+A still force-deselects.
- **Weight Paint brush icon** is now a whitish `tlMakeIcon("brush")` glyph
  (diagonal handle, darker ferrule, tapered bristles) in the same family as
  the Object/Edit mode-selector icons, replacing the colored `:/btn/skinned`.
- **UV editor Blender-look pass:**
  - Grid: base **8** subdivisions (was 4), ×8 finer levels that fade in on
    zoom; uniform subtle whitish lines (alpha 0.26/0.20/0.13), no
    bright/bold 0-1 border. With Repeat off, the grid + pixel lines are now
    **confined to the 0-1 tile** (shader `inGridTile`) — outside is plain
    background, like Blender.
  - Edges lightened to a readable medium gray; **vertex dots now show in
    every select mode** (dark unselected dots, orange selected/active) instead
    of only in Vertex mode.
  - 2D cursor is now Blender's **red/white dashed ring + crosshair ticks**
    (QPainter overlay in paintGL), matching the 3D viewport's cursor, instead
    of the small cross+dot.
- Note: the 3D viewport's orthographic grid and 3D cursor were already
  Blender-styled in earlier work; no 3D-grid change this batch pending the
  user pointing at the specific aspect that looks off.

## 2026-07-14 — UV editor feedback batch 2: object mode, tiling, pixels, cursor menu, env filter

- **Object Mode display**: the UV editor no longer forces Edit Mode on open.
  In Object Mode it shows the selected mesh's UVs read-only — the primary
  (active) mesh in white, each secondary-selected mesh in a distinct color
  (6-color palette) so overlapping layouts read apart. Editing (G/R/S, box
  select, unwrap, project, pick) is blocked with a status hint; pan/zoom/frame
  and the snap-menu cursor ops still work. Follows `objectSelectionChanged`.
  Switch to Edit Mode (Tab / mode selector) to edit. This also resolves the
  "sync doesn't work / editor is empty" report: the editor is now populated in
  every mode, and sync-off only hides faces in Edit Mode.
- **Repeat toggle** (bar2): off by default = show only the 0-1 tile with a
  dark outside (Blender default; texture clamped, shader `tileMode=1`); on =
  repeat image + grid across every tile. Legacy pop-up UV editor unaffected
  (shader `tileMode` defaults to 0/repeat when unset).
- **Pixel grid toggle** (bar2, "Pixels"): draws a subtle grid at the underlay
  texture's real pixel boundaries, fading in as pixels grow past a few screen
  px. Resolution is queried from the bound texture
  (`glGetTexLevelParameteriv`). New shader `drawPixelGrid` (0 = disabled, so
  legacy editor unaffected).
- **Pixel snapping**: Snap menu gains "Selection → Pixel" and "Cursor →
  Pixel"; the snap popover gains a **Pixel** Snap Target (Ctrl-drag lands the
  base point on the nearest texel corner, with the snap indicator). All need a
  texture underlay with a known resolution, else a status hint.
- **Right-click "Place 2D Cursor Here"** is now the first context-menu item
  (exact click position), alongside the existing Shift+RMB placement.
- **Env textures removed from the underlay list**: FO4 texture slot 4
  (environment cubemap — renders as garbage in 2D) and slot 5 (env mask) are
  skipped in the slot picker.
- Island fill already derived-from-vertices from batch 1 keeps working in all
  the above.

## 2026-07-14 — UV editor feedback batch: Blender look, sync toggle, island fix, Unwrap, UV sets

- **Blender look**: background is now Blender's neutral #2B2B2B; the
  untextured 0-1 tile renders as mid gray (#393939) so the whitish grid lines
  carry the contrast, matching the 3D viewport's dark-gray-plus-light-lines
  scheme instead of the old white tile.
- **Progressive grids**: the two finer grid levels no longer pop in at fixed
  zoom gates — each fades in over the octave before its gate while zooming in
  (full strength at half the gate), like Blender's UV editor.
- **UV Sync Selection button** (⇄, leftmost on the settings bar, persisted):
  ON keeps the existing both-ways mirroring with the 3D viewport. OFF makes
  the UV selection fully local and — per Blender — shows and hit-tests only
  the faces currently selected in the 3D viewport (the 3D selection keeps
  being tracked as the visibility filter; nothing is pushed back).
- **Island select fixed**: the deferred selection echo from the viewport
  (vertex-typed picks) was wiping the derived edge/face sets, so island
  clicks lost their fill and face membership. syncSelectionFromViewport now
  ignores unchanged-vertex echoes entirely and re-derives edge/face
  membership from the vertex ground truth in island mode; the island fill is
  additionally derived from vertices at draw time, so it can never be lost to
  an echo again.
- **Unwrap** (U key, `Unwrap ▾` bar button): "Unwrap (Angle Based)" runs a
  real LSCM (least-squares conformal map) over the selected faces — split
  into connected components (the existing per-vertex splits act as seams),
  each solved with two pinned extreme vertices via Jacobi-preconditioned CG
  on the normal equations (no assembled matrix, no new dependencies),
  components rescaled to uniform texel density from their 3D area, shelf-
  packed and normalized into the 0-1 tile; one undo transaction. "Project
  From View" projects the selection along the current 3D camera
  (object-space positions; the camera direction is exact, per-shape world
  offsets are not — noted limitation).
- **UV Map selector** (bar2): switches between UV coordinate sets on legacy
  NiTriShape-era meshes ("UV Sets" array), keeping selection since topology
  is shared. FO4 BSTriShape stores exactly one UV channel in its vertex
  format, so the combo is informative-but-disabled there — a second UV map
  cannot be added to FO4 meshes without the game ignoring it.
- GLView::viewTransform() made public for Project From View (access change
  only, no layout change).

## 2026-07-14 — UV Editing workspace, phase 1

- New **UV Editing** workspace (replaces the "UV Manager (Planned)"
  placeholder; right-docked like the other managers; entering it auto-enters
  Edit Mode). New file `src/uvtools.cpp`: `UVEditorView` 2D GL canvas +
  `tlCreateUVManagerDock` factory. The legacy pop-up UV editor is untouched.
- Canvas: texture underlay with slot picker (FO4 lighting/effect shader
  slots), custom image browse (for atlas retargeting), alpha toggle, the
  legacy editor's `uvedit.prog` grid (zoom-gated levels), MMB pan, wheel
  zoom-to-cursor, Home/`.` framing. All edit-mode meshes render; non-active
  shapes draw dimmed; active shape editable.
- Selection: Vertex/Edge/Face/Island modes (`1`–`4` + bar buttons), click /
  Shift-toggle, LMB-drag box select (plain adds, Shift/Ctrl removes — 3D
  convention), `A`/`Alt+A`/`Ctrl+I`, `L` island under cursor, `Ctrl+L` grow
  to islands. Element marking follows the shared palette: active #FF9D00,
  selected #FF7200, unselected near-black wires/points; face fills in
  translucent orange; vertex dots only in vertex mode (Blender behavior).
- **Two-way selection sync** with the 3D viewport: new GLView
  `elementSelectionChanged` signal (coalesced, emitted via the
  `recordSelection()` choke point + selection undo/redo) drives UV-side
  mirroring; UV-side changes push back through new
  `GLView::setElementSelectionExternal()` (records selection undo, rederives
  world positions) and `setElementPickMode()`. Pick-mode changes mirror both
  ways; island mode surfaces in 3D as vertices.
- Transforms: Blender modal `G`/`R`/`S` with `X`/`Y` axis constraint, typed
  numeric input, Shift precision, Esc/RMB cancel, LMB/Enter commit. Live
  write-through during the drag (Processing-state batch + one dataChanged per
  step so the textured 3D preview follows), one merged model-undo transaction
  per gesture on commit — Ctrl+Z is shared with everything else. Half-float
  UVs accumulate in float32 editor-side.
- Snap bar (per the Blender popover): magnet toggle (Ctrl inverts), Snap
  Target Increment/Grid/Vertex (vertex target shows an indicator dot and
  excludes dragged verts), Snap Base Closest/Center/Median/Active, Affect
  Move/Rotate/Scale, rotation increment, grid step; persisted under
  `UVEditor/*`. Pivot selector: BBox Center / Median / 2D Cursor.
- 2D cursor with the 3D cursor's toolkit: Shift+RMB places, U/V spin boxes,
  `Shift+S` snap menu (Selection→Cursor / Keep Offset / Grid;
  Cursor→Selection/Active/Grid/Origin/Tile Center), cursor as pivot,
  crosshair + red dot marker, `Shift+C` resets.
- Data model note honored throughout: NIF UVs are per-vertex (one UV per
  vertex; seams are split verts), so UV points map 1:1 to mesh vertices.
  BSTriShape (FO4) + legacy NiTriShape/NiTriStrips supported; SSE
  skinned-partition data and Starfield BSGeometry are out of scope phase 1.
- GOTCHA for future work in this file: Qt's `slots` macro ate a local
  variable named `slots` (declaration compiled to nothing) — keep Qt keyword
  names out of identifiers in Qt-including TUs.

## 2026-07-14 — UV editing workspace: plan drafted

- `UV_EDITOR_PLAN.md` added: phased plan for a Blender-style UV editing
  workspace (dockable 2D editor, Blender keymap and modal G/R/S, two-way
  selection sync with 3D edit mode, model-stack undo, later stitch/pack/
  projections, deferred seam+LSCM unwrap). Documents the per-vertex-UV
  structural difference from Blender's per-loop UVs. Awaiting user approval;
  no code changes yet.

## 2026-07-14 — Build fix: startup crash from stale incremental object

- The usability follow-up build crashed on launch (access violation in QHash
  `findNode`). No source change was at fault: `Makefile.Release` carried a
  stale dependency list in which `riggingtools.o` did not depend on
  `src/glview.h`, so adding the `sessionDocumentPreviewColors` member to
  `GLView` left that object compiled against the old class layout while the
  rest of the program used the new one. Re-running
  `qmake6 -o Makefile NifSkope.pro` regenerated correct dependencies
  (`src/glview.h` is now listed), the stale object was rebuilt, and startup
  was verified from the console. Rule of thumb: re-run qmake6 after changing
  the include graph or the layout of widely-included classes.

## 2026-07-14 — Loaded NIFs usability follow-ups

- NIF Browser right-click now respects multi-selection: right-clicking a row
  inside the current selection offers **Add N Selected to Loaded NIFs** and
  queues every selected row (same cooperative one-per-turn queue as the Load
  Selected button); right-clicking outside the selection still acts on the row
  under the cursor only. Previously the context menu always enrolled just the
  clicked row, silently ignoring the rest of the selection.
- The primary document now always appears in the Loaded NIFs list — marked the
  Block List way (light-blue row, orange text, arrow icon) — even when it was
  never explicitly enrolled, so the list and the viewport always agree about
  what is being edited. Its context menu works from the automatic row too;
  "Remove from Loaded NIFs" is disabled for it since the primary cannot leave
  its own workspace view.
- The combined secondary-document preview is now fully opaque instead of 38%
  translucent. To keep opaque geometry readable, per-face lambert shading
  against a fixed light is baked into vertex colors once per soup rebuild
  (the neutral gray-blue tone is preserved). The preview now writes depth so
  it occludes and is occluded by the primary correctly, and its polygon offset
  is biased away from the camera so the editable primary always wins
  coincident-surface z-fights against the read-only backdrop.

## 2026-07-14 — Data-only background document layer

- Loaded NIFs enrolled from the NIF Browser are no longer full hidden NifSkope
  windows. Each is now a `BackgroundNifDocument`: a parsed `NifModel` plus its
  source identity (loose path, or configured game + archive path) and its
  workspace-group root — with no QMainWindow, no dock/toolbar/menu construction,
  and no GL viewport. Adding many donors therefore costs one NIF parse each,
  nothing more; parses still run one per event-loop turn through the existing
  queue so input and repainting stay responsive.
- Promotion to primary is now the lazy UI-attach step: **Make Primary / Edit**
  (or double-click) creates a hidden real window, reloads the NIF from its
  original source (loose file or the primary's combined configured archive),
  then runs the ordinary primary switch. Background documents have no editing
  UI and can never be dirty, so the re-parse is lossless. Trade-off: promoting
  now re-parses once, while enrolling donors became much cheaper; a failed
  reload leaves the data-only entry untouched and reports on the status bar.
- The Loaded NIFs pane, its selection wiring, both context menus, isolate /
  show-all / hide-all actions, and the combined viewport triangle-soup preview
  all handle both real windows and data-only documents with the same palette
  and semantics. Removing a data-only entry and closing it are the same
  operation, since nothing else owns it; closing the visible primary still
  closes every member of the workspace group, deleting data-only members
  outright.
- Rigging's donor chooser now enumerates `NifSkope::selectedWorkspaceModels()`
  — model/display-path pairs covering windows and background documents alike —
  instead of window-only `selectedWorkspaceDocuments()`. Donor capture to a
  temporary NIF is unchanged.
- Configured-resource extraction was factored into
  `extractConfiguredNifBytes()`, shared by the window loader and the background
  loader. Background parses use `MSG_TEST` message mode so a batch enroll can
  never raise modal error dialogs; failures drop the entry with a status-bar
  message. Background documents also no longer touch recent-file history or
  emit load signals, and they never call `setCurrentFile()`.

## 2026-07-13 — Rigging workspace and atomic transfer

- Camera projection and saved user-view transforms are now session-only. A
  newly launched NifSkope starts from the configured startup direction in
  perspective, while opening another NIF in the same window preserves the
  current camera instead of snapping back to the startup orientation.
- Added multi-NIF document sessions. Files opened together appear as compact
  tabs while retaining independent models, selections, Undo stacks, dirty
  markers, managers, and editor state. Activating a tab makes it the primary
  editable document; visible secondary documents form a neutral, read-only,
  non-pickable combined viewport preview for complete armor-set assembly.
- Made Loaded NIF ingestion cooperative and much lighter on the active window.
  Selected donors are parsed one per event-loop turn so input and repainting can
  run between files; hidden documents no longer install application-wide input
  filters, rebuild their unused session/browser UI, or rescan configured
  resources after loading. Parsing remains deliberately serialized because the
  current NifModel and renderer are UI-thread objects; promoting a donor lazily
  enables its input handling and NIF Browser only when it becomes primary.
- Document-tab context actions toggle secondary visibility, isolate one
  secondary with the primary, show/hide all, unload preview geometry, or close
  a document with the normal save confirmation. Open secondary documents are
  offered directly by Rigging's donor chooser; their current in-memory state is
  captured to a temporary auto-removed NIF, so unsaved donor edits participate
  without modifying the donor file.
- Renamed **Archive Browser** to **NIF Browser** and made its tree the home of
  an expandable **Loaded NIFs** session category, removing the cramped document
  tabs from the main toolbar and the separate strip above the browser. Loaded
  entries retain primary/secondary state, activation and context controls. The
  same tree combines archived meshes with loose NIF/BTO/BTR files
  under the selected game Data folder, searches both sources together, supports
  extended multi-selection, and loads all selected meshes as independent
  session documents through a dedicated **Load Selected** button. Double-click
  also opens archive entries in a new document instead of replacing a populated
  primary document.
- NIF Browser now builds its **Available NIFs** hierarchy automatically from
  the current game's resource paths configured in Settings. The resource
  manager merges every linked BA2/BSA and loose directory into one virtual
  list, resolves overrides without duplicate paths, and loads either source
  identically. Available files stay above the separate **Loaded NIFs** session
  category; a compact Refresh button rebuilds the list after external changes.
  The confusing Recent Archives/Recent Files controls were removed from the
  browser (their legacy File-menu entries remain available).
- Fixed configured mesh discovery for Skyrim/Fallout-era resource profiles:
  their normal renderer cache intentionally excludes `.nif`, which previously
  left mostly terrain files visible. NIF Browser now creates an independent,
  mesh-only virtual archive from the same ordered Settings paths, accepting
  NIF/BTO/BTR files from BA2/BSA archives, full Data directories, and direct
  loose `meshes` folders while preserving configured override precedence.
- Moved loaded session documents into a dedicated resizable lower pane beneath
  Available NIFs. Primary documents use the established orange `#FFA040`,
  visible secondaries use reddish-orange `#FF602A`, and hidden/unloaded
  secondaries use selection blue `#4772B3`; double-click and the document
  context menu retain primary/visibility/close controls.
- Cleaned the legacy toolbar separator left behind by the removed viewpoint
  actions, keeping Display and the combined Center/Frame control together and
  placing the transform-group separator after them.
- Moved the Refraction and Particles switches into a persistent **Viewport
  Effects** section of the unified shading menu. Both settings now remain open
  while toggled and persist between sessions.
- Rebound the orthographic viewport commands to Blender's numpad layout:
  Numpad 7 Top, Numpad 1 Front, Ctrl+Numpad 3 Left, Numpad 9 Opposite/Flip,
  and Numpad 5 Perspective/Orthographic. Top-row 1/2/3 remain geometry modes.
- Completed viewport-scoped Blender numpad navigation: Numpad 3 Right;
  Ctrl+Numpad 1/3/7 Back/Left/Bottom; Numpad 2/4/6/8 fixed 15-degree orbit;
  Shift+2/4/6/8 view-plane pan; Numpad +/- zoom; Numpad / local-view toggle;
  Numpad decimal Frame Selected; and Home Frame All. Numeric fields retain
  their numpad input, while Numpad 0 remains reserved for active-camera support.
- Removed the now-redundant Viewpoint toolbar dropdown; axis snapping remains
  available through the 3D orientation gizmo and Render menu.
- Orthographic views now use a Blender-style screen-aligned grid that fills the
  viewport, adapts its 1/2/5 subdivision spacing to zoom, stays anchored to
  world zero while panning, distinguishes major and minor lines, colors the
  visible world axes red/green/blue, and renders behind geometry without
  z-fighting. It appears only when the camera rotation is exactly aligned to
  Top/Bottom, Front/Back, or Left/Right; arbitrary User Orthographic angles
  hide it. Perspective view retains the grounded grid.
- Orbiting an orthographic view with MMB or the numpad now follows Blender's
  Auto Perspective behavior, restoring the horizontal perspective ground grid
  while arbitrary User Orthographic views remain gridless until orbited.
- Added disabled **UV Manager**, **Skeleton Manager**, and **Pose Manager**
  entries below the implemented workspaces. They reserve the future workflows
  without creating empty docks or changing persisted workspace indexes; their
  tooltips describe UV editing, skeleton/rest-pose management, and reusable
  load-screen posing with props.
- Combined **Center Viewpoint** and **Frame Selected** into one compact focus
  dropdown between Display Options and the transform controls. Frame Selected
  retains Blender's Numpad-decimal behavior and fits the active selection;
  Center Viewpoint only resets the camera pivot.
- Removed the obsolete material-channel icon grid from the lightbulb popup,
  including its empty placeholder buttons and duplicate Normals entry. The
  popup remains the home of directional, colour, ambient, brightness, tone-map,
  frontal-light, and PBR environment controls. Its remaining file button is now
  explicitly labelled **Choose PBR Environment Cubemap**; it does not override
  the single cubemap authored by Specular/Gloss materials.
- Added an inline **Block List search** for block number, type, displayed value,
  and object name. Space-separated terms combine as AND filters, hierarchy
  parents remain visible for matching descendants, Ctrl+F focuses the field,
  Esc clears it, and the filter survives list/hierarchy switches and file loads.
- Added compact Block List category icons for scene nodes, geometry, skinning,
  materials, textures, animation, collision, particles, lights, cameras, extra
  data, and otherwise generic blocks. Dedicated artwork uses an orange node
  point, wireframe cube with orange selected edges, colored material/texture
  symbols, green animation control, floating particles, lightbulb, camera, and structured-data document;
  Block Details remains undecorated.
- F2 or double-clicking the visible object name now renames uniquely named
  `NiAVObject` scene objects directly inside their Block List row (Enter
  accepts; Esc cancels), without a modal dialog. Double-clicking the block type
  does not rename it. Empty or duplicate names are rejected, and the atomic
  rename path keeps object palettes and controller-sequence node names
  synchronized as one Undo step.
- Enabled **Vertex Paint** as a real viewport mode. It reuses Weight Paint's
  Blender-style camera, vertex/edge/face selection masks, continuous swept LMB
  brush, brush/select toolbar toggle, Tab mode memory, and one Undo snapshot per
  drag. A dedicated **Vertex Paint** workspace and **Vertex Paint Manager** expose
  separate **Color (RGB)** and **Alpha** paint
	channels: RGB preserves alpha and Alpha preserves RGB. Missing packed vertex
	colours initialize to opaque white as one undoable structural operation. The
	RGB brush now docks the complete NifSkope color chooser directly below the paint
	controls in live-edit mode, including its synchronized RGB/HSV fields, hex value,
	standard and custom palettes, screen picker, and drag-to-scrub numeric controls.
- Added a compact **All / Bones / Segments** filter beside the bone search box.
  It filters both receiver and donor lists; Segments includes subsegments and
  donor mesh parent rows remain visible whenever they contain matching rows.
- Fully enabled **Segment Paint** for FO4 `BSSubIndexTriShape`. Segments and
  subsegments are binary face channels with continuous swept LMB painting,
  selection masking, the shared Brush toggle, wheel zoom, face/edge/vertex
  selection tools, and Ctrl+X fill. Each stroke is one Undo snapshot.
- Segment edits rebuild the format's contiguous triangle ranges atomically:
  triangles are regrouped, parent/subsegment ranges and shared data are rebuilt,
  and every face retains one exclusive owner. The manager can add segments and
  subsegments, edit stored subsegment User/Bone IDs with F2 or double-click,
  and delete either kind with affected faces safely reassigned to Segment 0.
- Receiver and donor segment rows participate in multi-selection and drag/drop.
  Binary membership can be transferred from multiple secondary meshes onto the
  active receiver channel using nearest-surface face correspondence. The entire
  transfer is one Undo step and donor meshes remain unchanged.
- The donor bone/segment list, donor action button, and donor prompt are hidden
  when the viewport selection contains only the primary receiver; the receiver
  list expands to the full manager width.
- Segment Paint keeps its evaluated surface automatic and hides the unrelated
  Deformed Cage toolbar control; the Brush selection/painting toggle remains.
- Expanded the viewport mode selector in the requested order: **Object Mode**, **Edit
  Mode**, **Vertex Paint**, **Weight Paint**, and **Segment Paint**.
- **Weight Paint** is now a first-class viewport mode. Selecting it activates the Rigging
  workspace, starts the existing weight-paint tool for the selected (or first available)
  bone, and stays synchronized with the Rigging Manager. Object Mode and Edit Mode exit
  weight painting cleanly.
- Added **Manual weight painting** to the Rigging Manager. Select a skin bone, choose
  Add, Subtract, Replace, or Smooth, then paint directly on the target with a cyan
  screen-space brush. Weight, Strength, and Radius match the familiar Blender workflow;
  the wheel resizes the brush and RMB/Esc exits.
- Each mouse drag is one whole-model Undo entry. Every touched FO4 vertex is reduced to
  at most four deterministic influences and renormalized; Smooth uses one-ring topology,
  bone bounds are recalculated, and standard-only `CustomizationRemapData` is refreshed
  from the packed result when present. Face-sculpt RemapData remains the original
  standard-skeleton snapshot by design.
- Painting is restricted to the active target surface. X-ray controls backface inclusion,
  the selected-bone heatmap refreshes after each stroke, and the Rigging Manager now
  reacquires its target correctly after snapshot Undo/Redo. Native coverage reports
  `weightPaint=clean` and verifies Add/Subtract/Replace normalization plus byte-exact Undo.
- Added a read-only **Donor geometry overlay** to the Rigging Manager. It loads any
  compatible donor shape into a cyan, non-pickable viewport triangle soup aligned through
  the same skeleton-root-relative space guard as transfer, with Filled, Wireframe, Opacity,
  and Clear controls. It never enters the NIF model or Undo stack.
- Added **Show selected-bone weights**: selecting a skin bone paints the target with a
  Blender-style blue→cyan→yellow→red 0..1 heatmap and reports affected vertices plus maximum
  weight. The per-corner colour buffer is viewport-only and coexists with the donor overlay.
- Both Rigging visual buffers disappear while the workspace is hidden, restore on show, and
  clear on target changes, structural edits, atomic transfer, or Undo/Redo. Native-window
  coverage byte-checks the model and now reports `donorOverlay=clean weightHeatmap=clean`.
- Packaged this visualization increment separately as
  `dist/NifSkope-WW-Rigging-Visuals-2026-07-13.zip`, preserving the earlier release archive.
- Added a repository-owned `RiggingIntegration.pro` target and
  `tests/rigging/run.ps1` runner. `run.ps1 -Build` performs an isolated full-source
  build, then runs the native-window suite against versioned target/donor fixtures;
  subsequent runs reuse the executable and complete in about 22 seconds.
- Added a dedicated **Rigging** entry to the Workspaces selector. It activates an
  exclusive right-side Rigging Manager with live target/skin status, bone inventory,
  validation tools, and an expandable advanced workflow.
- Added **Transfer Bones and Weights...** as the primary one-dialog workflow. It
  generates `CustomizationRemapData`, imports the required donor hierarchy, binds
  donor bones, and transfers weights as one atomic Undo entry.
- Its confirmation now summarizes donor and target geometry, used/shared/new bone
  bindings, missing used donor nodes, and whether `CustomizationRemapData` will be
  created or updated (including its expected byte size). Expandable details list the
  affected bones, while incompatible shape spaces are rejected before any mutation.
- The atomic workflow now performs its surface mapping before confirmation and reports
  median, 95th-percentile, and maximum snap distances as percentages of donor span,
  classifying the geometry as Close, Caution, or Poor. The prepared weights are reused
  for the write, avoiding a second mapping pass; cancellation or rejection remains
  mutation-free and Poor matches retain Cancel as the safe default.
- A failed inner step restores the complete pre-transfer model snapshot. Successful
  runs report imported nodes, bound bones, transferred vertices, and remap size.
- Rollback warnings now identify the exact failed workflow step and clearly warn the
  user to close without saving if automatic snapshot restoration itself ever fails.
- Preview and weight-transfer surface mapping now show immediate, responsive progress
  and check for cancellation every 16 target vertices. Cancelling Preview changes
  nothing; cancelling the atomic workflow restores RemapData, imported nodes, and
  bindings through its outer snapshot and creates no Undo entry.
- Workspace buttons invoke the same persistent SpellBook path as the menus; selection
  state is synchronized so actions operate on the active skinned shape.
- Native-window coverage now clicks the primary Rigging workspace button end-to-end,
  verifies the manager refreshes from 10 to 69 bones after its model reset, confirms
  the application-owned Undo stack receives exactly one entry, and restores the
  original model through that same stack before closing the window.
- Reconciled `BONE_WEIGHT_TRANSFER_PLAN.md` with the release-candidate implementation.
  It now distinguishes the completed existing-skin FO4 workflow from the deferred
  unskinned-target backend, mapping controls, independent skeleton reference, donor
  overlay, classic-skin backend, and in-game/manual production-asset validation.
- Hardened geometry ingestion against non-finite positions and out-of-range triangle
  indices before they can reach incident-face indexing. Degenerate faces use a safe
  nearest-corner fallback, and isolated donor vertices fall back to their own normalized
  skin instead of producing an empty transfer result.
- The native harness now rejects every GUI save dialog, chooses Discard for close/save
  prompts, verifies the donor's expected 68-bone identity at startup, and byte-compares
  both versioned fixtures at shutdown. This prevents GUI testing from ever overwriting
  its source assets; malformed-topology rejection is covered as `topologyGuard=clean`.
- Added a reusable `run.ps1` real-asset mode that runs the native atomic spell on external
  read-only NIFs, validates/reloads the generated output, proves byte-exact Undo/Redo, and
  compares weights by bone name with a vanilla reference. Vanilla female-head→BigBeard01
  hairline reproduces the validated ~0.34 mean L1 / ~83.6% dominant agreement (`Caution`),
  while the male-head self-surface case reaches 2.26e-06 / 99.88% (`Close`). No Blender or
  DCC path is involved.
- Real-asset validation now compares post-transfer and post-reload FO4 validator counts with
  the original target's baseline and includes detailed validator text on failure. A 1,523-
  vertex `MaleBody` standard-only self-transfer passes `Close`, 58→58 bones, 18,276 remap
  bytes, 1.70e-07 mean L1, 100% dominant agreement, and validation 0→0.
- Fixed atomic RemapData sequencing for standard-only donors: face-sculpt transfer still
  captures the original standard skin before the weight write, while standard-only transfer
  regenerates the blob afterward from NifModel's exact packed float16 values. The versioned
  suite locks both branches and now reports `standardOnly=clean`.
- Production guards correctly rejected three incompatible body/outfit trials before mutation:
  two `Poor` geometry/rest-pose pairs and a geometrically `Close` OldMaleBody/MaleBody pair
  with an incompatible shared `Chest_skin` rest pose.
- A compatible different-surface standard-skinned outfit case also passes natively:
  female Combat Armor Mid torso → Lite torso reports `Caution`, 32 target vertices, 8→8
  bones, 384 remap bytes, 0.0439682 mean L1, 100% dominant agreement, and validation 0→0.
- Archived the complete fixture/production/rejection matrix in
  `tests/rigging/QA_EVIDENCE_2026-07-13.md`; the only remaining release check is visual and
  FaceGen behavior inside Fallout 4.
- Completed final source/parity review and packaged the audited Release application as
  `dist/NifSkope-WW-Rigging-2026-07-13.zip`, excluding the test-only integration executable
  and including the plan, change log, and dated QA evidence.
- Marked read-only Rigging spells as constant and write spells as undoable, avoiding
  misleading non-undoable warnings while preserving the existing confirmation path.
- Fixed skin-bone enumeration to address the `Bones` array itself instead of its first
  element; this restores correct list, comparison, validation, and transfer preflights.
- Release build and native-window smoke test pass. The 1,689-vertex integration fixture
  finishes with 76 blocks, 70 nodes, 69 bones, 20,268 remap bytes, four advanced Undo
  entries or one combined Undo entry, and survives undo/redo plus save/reload.
- Expanded regression coverage proves donor-dialog cancellation and confirmation
  rejection are mutation-free, verifies a mid-workflow import failure restores the
  target byte-for-byte with zero Undo entries, and checks live workspace enablement
  plus the initial 10-bone inventory through a real NifSkope window.
- Added generated, non-destructive varied fixtures: the production Duplicate spell
  creates a two-shape donor and the picker is verified to select the named second
  shape; a target changed from 1,689 to 1,690 vertices and from 3,230 to 3,229
  triangles completes transfer, validation, one-step Undo setup, and save/reload.
  Every resulting vertex retains normalized weights with in-range bone indices.
- Rebuilt the viewport-shading dropdown as a compact material-inspection popover:
  Flat, Unlit, and Shaded are exclusive modes, while future Game Lighting remains
  visible but disabled. A Specular/Gloss workflow is active and the future PBR
  workflow is shown disabled. The persistent channel mixer groups Color, Surface,
  Lighting, and Alpha controls as compact pressed/unpressed buttons. Specular and
  Gloss are separate renderer controls; PBR-only Roughness/Metallic/AO channels are
  hidden while Specular/Gloss is active. Each display mode remembers its own mask,
  Shift-click solos/restores a contribution, and Reset restores supported channels.
  Legacy duplicate channel buttons were removed from the Render toolbar and lighting
  flyout so the mixer is the single channel-control surface.
- Expanded the Block List into a navigation workspace without changing NIF data:
  category quick filters, working search in both flat and hierarchy views, Ctrl+G
  number/name/type jump, browser-style back/forward history, scene-parent breadcrumb,
  session pins, outgoing/referenced-by link menus, and live block/shape/vertex/triangle
  totals. Hierarchy rows now expose concise type-aware summary tooltips. Block Details
  also has a recursive field-name/value filter (Ctrl+Shift+F) that retains matching
  parents and expands matching branches.

## 2026-07-12 — Rigging: target-derived bone bounds

- Weight transfer now recalculates every `BSSkin::BoneData` bounding sphere from the
  target vertices and newly written weights using the existing `spUpdateBounds` path.
- Weight writes plus bound updates are grouped into one snapshot undo operation.
- Added duplicate-name, 256-bone, and exact four-slot target preflight guards.

## 2026-07-12 — Rigging: shared-pose binding guard

- Existing-node binding now verifies shared bones' skeleton-root-relative rest poses
  as well as their BoneData records before copying any missing binding.
- Restores the prior `holdUpdates` state after structural binding so nested model
  operations are not released prematurely.

## 2026-07-12 — Rigging: minimal donor bone-node import

- Added **Rigging → Import Donor Bone Nodes...** for the missing-hierarchy step.
- Imports only plain `NiNode` ancestors required by donor slots with positive weights;
  copies Name/Flags/Transform and explicitly rebuilds only planned `Children` links.
- Leaves donor controllers, collision, extra data, sibling meshes, and unrelated
  descendants behind; avoids unsafe partial link remapping/branch serialization.
- Preflights duplicate/empty/case-colliding names, cycles, detached or multi-parent
  paths, unsupported node subtypes, invalid transforms, and incompatible existing-node
  rest poses. The import is repeat-safe and wrapped in one snapshot undo operation.

## 2026-07-12 — Rigging: transfer compatibility guards

- Restricted preview/write transfer actions to FO4 BS version 130 and reject donor files
  with a different BS version.
- Transfer now requires donor and target shapes to have matching skeleton-root-relative
  transforms before running raw-position closest-point mapping, preventing silent
  cross-file local-space mismatches.

## 2026-07-12 — Rigging: deep FO4 skin validation

- Expanded **Validate FO4 Skin** with skeleton-root reachability, NiNode type and
  duplicate-name checks, 256-bone enforcement, finite/nonzero BoneData transforms,
  valid bounds, `Num Scales` consistency, and shared skin/BoneData ownership checks.
- Vertex validation now treats every positive representable weight as active and warns
  about repeated active indices.
- Detects multiple or byte-stale `CustomizationRemapData` blocks, not just wrong length;
  byte equality is checked only for standard-skeleton skins because faceBones RemapData
  intentionally differs from the current sculpt-bone weights.
- Corrected scene-parent traversal to scan `NiNode.Children`; `getParentLinks()` is not
  an incoming scene-parent API. Root-relative comparisons now exclude root-local transforms.

## 2026-07-12 — Rigging: harden existing-node binding

- Hardened **Bind Donor Bones (existing nodes)** before hierarchy import: malformed
  skin/BoneData array counts and duplicate bone names now abort before mutation.
- Same-name target nodes must belong to the selected skin's `Skeleton Root` subtree;
  donor and target node rest poses are compared root-relative before binding.
- Wrapped the coupled BSSkin/BoneData resize and record copy in one whole-model
  snapshot undo operation.

## 2026-07-12 — Rigging: bind donor bones to existing nodes

- Added **Rigging → Bind Donor Bones (existing nodes)...**, the first structural
  BSSkin write increment.
- Adds only donor bones actually used by the donor mesh and only when an unambiguous
  same-name `NiNode` already exists in the target; missing hierarchy import remains
  explicitly deferred.
- Requires shared donor/target BoneData transforms to agree within `0.001` before
  copying inverse-bind records, preventing cross-bind-space deformation.
- Extends `BSSkin::Instance.Bones` and `BSSkin::BoneData.Bone List` together, copies
  bounds/transforms, enforces the uint8 256-bone limit, and reports unavailable nodes.

## 2026-07-12 — Rigging: FO4 skin validator

- Added read-only **Rigging → Validate FO4 Skin** before structural bone writes.
- Checks `BSSkin::Instance` and `BSSkin::BoneData` counts/array alignment, valid and
  unique bone links, four-slot vertex skin layout, weight normalization, in-range
  nonzero bone indices, and `CustomizationRemapData == vertexCount × 12` when present.
- Reports a compact summary with detailed problems and never mutates the model.

## 2026-07-12 — Rigging: CustomizationRemapData generation

- Added **Rigging → Generate CustomizationRemapData** for skinned FO4 shapes.
- Encodes the current standard-skeleton skin as the engine's exact 12-byte-per-vertex
  layout: four little-endian float16 weights followed by four uint8 bone indices.
- Preserves half-float encodings through `qfloat16`, zeroes indices for zero-weight
  slots, updates an existing named block, or creates and attaches a new
  `NiBinaryExtraData` named `CustomizationRemapData`.
- The action is intentionally limited to BS version 130 and requires four skin slots.

## 2026-07-12 — Bone & Weight Transfer plan + FO4 faceBones research

- Reverse-engineered and byte-verified FO4 `*_faceBones.nif`
  `CustomizationRemapData` (per-vertex 4×float16 weights + 4×uint8 bone
  indices) and `CustomizationRemapNewBonesData`. Proved RemapData is a copy of
  the mesh's standard-skeleton skinning: an encoder regenerating it from
  `BaseFemaleHead`'s skin reproduces the vanilla blob byte-for-byte.
- Added `BONE_WEIGHT_TRANSFER_PLAN.md`: design for an in-place donor→target
  skinning transfer feature (transfers bone bindings *and* weights, not just
  weights into existing vertex groups like Blender's DataTransfer). Covers the
  geometry core (closest-point-on-triangle + barycentric), the FO4 `BSSkin`
  write path (distinct from the classic `NiSkinInstance`/`NiSkinData` handled
  in `skeleton.cpp`), a vanilla ground-truth validation strategy, phasing, and
  RemapData generation as a follow-on. No code yet — planning only.
- Specced a new **"Rigging" workspace** as the feature's UI home, wired into the
  existing Workspaces selector (`nifskope_ui.cpp` ~L1719–1818) beside Default/
  Animation/Materials/Collision — new `tlCreateRiggingManagerDock` factory plus
  five documented edit points. Scope covers bone list, donor/target bone
  comparison, weight transfer, numeric + brush weight painting, and a shared
  skin-data layer over FO4 `BSSkin` and classic `NiSkin*`.
- Designed **cross-file donor + skeleton reference** as the primary workflow
  (§6.6), reusing the proven `importDonorCollision` pattern
  (`collisiontools.cpp:1356`: standalone `NifModel::loadFromFile`, bsVersion
  guard, branch splice with block remap). Core API takes `(NifModel*, block)`
  pairs so donor/skeleton can come from any file. Also specced a read-only
  **donor viewport overlay** (§9.3) reusing the collision-preview draw path.
- **Phase-1 transfer prototype validated** (Python, scratchpad): closest-point +
  barycentric weight transfer. Self-transfer is near-exact (meanL1 0.0001, 100%
  dominant-bone match); head→hairline reaches ~83% agreement with vanilla
  hand-authored skin (residual is legitimate, not a bug — see §7.1). Confirms
  the algorithm and motivates manual weight paint.
- **Inverse-bind write math validated** (§7.2): `skinToBone = inv(boneWorld) @
  meshBindWorld`, matrices as-stored, world = parent@local — reproduces vanilla
  `BSSkin::BoneData` transforms to ~1e-5 across female/male head, beard, hairline
  (all share bind offset T(0,-0.88,120.84) = HEAD position). Round-trip matches
  float32 to 1.6e-5. **All risky skin math (weights + bone transforms +
  RemapData) now de-risked in Python.** Remaining = block assembly + C++/UI.
- **C++ core ported and verified** against Python via UCRT64 g++: closest-point-
  on-triangle, 4x4 multiply/inverse, inverse-bind (`skincore.cpp`) all pass; the
  end-to-end transfer (`skintransfer.cpp`) reproduces the Python head→hairline
  result **exactly** (meanL1 0.00000, 100% dominant match). Validated Python +
  C++ prototype preserved in-repo at `tools/rigging_prototype/` with README.
  Transfer/skin algorithm is now implementation-ready; only NifModel I/O + UI
  remain (need the Qt build).
- **Rigging spell page built into the app** — new `src/spells/riggingtools.cpp`
  (in `NifSkope.pro`), all compiling clean into `release/NifSkope.exe`:
  1. **List Skin Bones** (read-only) — bones a shape is bound to (NiSkinInstance
     or FO4 BSSkin::Instance).
  2. **Compare Bones with Donor...** (read-only, cross-file) — loads a donor NIF
     (`NifModel::loadFromFile`), diffs bone sets (shared / donor-only / target-
     only). The pre-transfer preview.
  3. **Preview Transfer from Donor...** (read-only) — runs the validated transfer
     core (geometry read via NifModel `get<Vector3>`/`Triangle`, closest-point +
     barycentric + top-4) and reports stats (bones used, influence histogram,
     snap distances). No write.
  4. **Transfer Weights (existing bones)...** (WRITE, undoable) — transfers donor
     weights into the target's existing bone slots; influences to absent bones
     are dropped + renormalized. Only rewrites per-vertex Bone Weights/Indices,
     no structural change. Full bone-adding transfer (new nodes + BoneData) is
     the next step.
  Transfer geometry/skin core is ported from `tools/rigging_prototype/`. Gotcha
  hit + fixed: `slots` is a Qt macro — renamed the local. In-app verify pending.

## 2026-07-12 — Compact unified Shader Flags editor

- Added a 460×360 combined editor for compatible `Shader Flags 1` and
  `Shader Flags 2` rows. It builds its flag list from the active `nif.xml`
  bitflag metadata, so Fallout 4 and Skyrim use their own names and bit
  positions without changing the NIF's two-field storage.
- A live text filter searches flag names, semantic categories and storage
  locations such as `F2 bit 25`, keeping the 64-bit set manageable inside the
  compact window. Clicking anywhere on a row toggles it; the footer shows live
  F1/F2 hex values plus selected and visible counts.
- Applying writes each bit back to its original 32-bit field as one snapshot-
  undoable operation. Cancelling leaves both values untouched.

## 2026-07-12 — Unified mode button and full color studio

- Restyled the Object/Edit Mode selector as the same bordered icon-and-text
  dropdown used by Panels and Workspaces, while keeping Tab and external mode
  changes synchronized with its checked item, label, and icon.
- Rebuilt the shared Color3/Color4 chooser used throughout block details into a
  Paint.NET-style color studio: HSV wheel, synchronized RGB/HSV/hex/alpha
  values, previous/current swatches, preset and persistent custom palettes,
  plus an eyedropper that samples any pixel from any connected screen — even
  outside NifSkope. Color3 fields omit alpha; Color4 fields preserve it.
- RGB, HSV, and alpha numeric fields now use the transform redo panel's
  Blender-style scrub interaction: drag horizontally to adjust, hold Shift for
  fine control, click without moving to type, and use hover-only step arrows.
  The arrows disappear and the field brightens while a scrub is active.
- Gave the color studio a NifSkope-native compact layout and paired every
  numeric RGB/HSV/alpha well with a color-aware horizontal slider. The chooser
  now uses a full HSV disc, tighter previous/current and palette groups,
  section dividers, a copyable hex field, and a restrained manager-style
  footer while retaining screen picking and persistent custom colors.
- Fixed the hidden Color3 alpha widgets appearing as orphan controls in the
  dialog's upper-left corner. Replaced the Windows eyedropper's full virtual-
  desktop overlay with low-latency global cursor/button polling and direct
  desktop-pixel sampling, allowing reliable picks from other applications and
  multiple displays without repainting a monitor-sized transparent window.
- Widened the screen-picker tooltip so its instructions are not clipped, and
  made mouse/keyboard completion edge-triggered as well as state-triggered.
  Quick clicks, right-click cancellation, and Escape can no longer fall wholly
  between polling ticks and leave NifSkope stuck in the modal sampler.
- Removed the sampler's nested application-modal `exec()` path. The parent
  color dialog now remains visible, sampling runs in a non-modal local event
  loop, and focus is restored explicitly after completion, preventing Windows'
  modal warning sound and the hidden-dialog lockout after an external pick.

## 2026-07-12 — New "Vertex Colours" viewport shading mode

- Added a fifth entry to the viewport **shading menu**: *Vertex Colours*. It
  renders the per-vertex **colour and alpha as the albedo** while keeping the
  authored **normal and specular/gloss** maps active — i.e. only the diffuse is
  replaced with the vertex colours; lighting still responds to the surface.
- Reuses the existing diffuse-force path (`Scene::VisVertexColors`, a new
  VisMode): the `BaseMap` is forced to white and `vertexColorOverride` is set to
  0 so the shader's `albedo = baseMap.rgb * C.rgb` collapses to the vertex
  colour, and `alpha = C.a * baseMap.a * alpha` keeps the vertex alpha. Vertex
  colours are shown regardless of the Vertex-Colours display toggle or the
  shader's `SLSF2_Vertex_Colors` flag; a mesh with no vertex colours renders
  white. Applies to the Fallout 4 and Skyrim/OB lighting-shader paths.
- New procedurally-drawn `shade_vertexcolor` toolbar icon (a sphere painted
  with colour patches).

## 2026-07-12 — Label BGSM textures by the material's own slot order

- When listing a BGSM's own textures, the tree was labelling them with the
  `BSShaderTextureSet` slot order — but a BGSM's internal texture array uses a
  different order (`BGSM1_*`/`BGSM20_*` in glproperty.cpp): index 2 is the
  specular/gloss map, not glow. So a BGSM's spec map showed up as "Glow/Skin".
  Material-file textures are now labelled by that native BGSM order (index 2 →
  Spec/Gloss, 3 → Greyscale, 4 → Environment, …), matching what the renderer
  samples. The `BSShaderTextureSet` fallback still uses the texture-set order.
- BGEM textures were already labelled correctly (their file order matches the
  effect shader's inline fields: Source, Greyscale, Env Map, Normal, Env Mask).

## 2026-07-12 — Material file (BGSM/BGEM) is the source of truth for textures

- In Fallout 4 the material is almost always linked, so the Material Manager now
  treats a linked **BGSM/BGEM as authoritative** and lists **its own textures**
  as the unfolded children of the material row (labelled by slot), rather than
  reading the NIF `BSShaderTextureSet`. This overrides the texture set when a
  readable material is present, and also fixes materials that have no texture
  set at all or whose textures aren't hooked into the shader/effect node — e.g.
  `x01_arms.bgsm` in X01_ArmRight.nif, whose preview was blank because it has no
  `BSShaderTextureSet`. Selecting the material previews the material's diffuse;
  each slot row previews its own texture.
- The NIF texture set / effect inline texture fields are used only as a
  **fallback** when there is no readable material file.
- Material-file texture rows are **preview-only** (they are defined by the
  BGSM/BGEM, not a NIF field): they are non-editable and the Browse… picker
  skips them. NIF-backed rows keep their editable Path column.
- Caveat: a referenced `.dds` must still be reachable in the loaded archives or
  as a loose file; if it is missing, the row shows MISSING / the preview shows
  "texture not found" rather than a blank pane.

## 2026-07-12 — Fix "Could not find Index subitem" spam on load

- The Material Manager rebuild walked a `BSShaderTextureSet`'s `Textures`
  array via `rowCount()` without checking the array index was valid. A
  `BSLightingShaderProperty` driven purely by a BGSM file has **no Texture
  Set**, so the index was invalid — and `rowCount()` of an invalid index
  returns the ROOT child count, so the loop iterated every header/block/footer
  row and called `resolveString()` on each, logging *"resolveString: Could not
  find \"Index\" subitem."* once per top-level item (seen loading
  X01_ArmRight.nif). Guarded with `array.isValid()`.
- Hardened the four other texture-array loops that shared the same pattern
  (Find Duplicates signature, Copy/Paste Material, Copy/Paste Texture Set) so
  a missing texture set can no longer trigger the same walk — and, in the
  Paste paths, can no longer write strings into unrelated top-level blocks.

## 2026-07-12 — Hierarchy submenu nested under Block

- The block-list right-click **Hierarchy** submenu (Set Parent / Clear Parent)
  no longer sits pinned at the very top of the context menu. It is now inserted
  **directly beneath the Block category**, and the leading separator that
  isolated it was dropped so it reads as a normal submenu.

## 2026-07-12 — Correct texture-slot labels in the Material Manager

- Fixed the material tree mislabelling texture slots. The old label list was
  mis-ordered and invented slots ("Wrinkles", "Displacement", "Extra"), so on
  Fallout 4 meshes the **specular/gloss map (slot 7) showed as "Wrinkles"**.
  Labels now follow the authoritative `BSShaderTextureSet.Textures` slot map in
  nif.xml: `0 Diffuse · 1 Normal · 2 Glow/Skin · 3 Height · 4 Environment ·
  5 Env Mask · 6 Subsurface · 7 Spec/Gloss · 9 Reflectivity · 10 Lighting`.
- Slot 7 is **version-aware**: it reads *Spec/Gloss* on Fallout 4 / 76
  (BS version ≥ 130, matching the renderer binding slot 7 as `SpecularMap`) and
  *Backlight* on Skyrim, where that slot is the back-lighting map.

## 2026-07-12 — Material Manager sizing & texture-preview interaction

- Removed the now-unused `tlScanMaterialTextures` helper (its only caller was
  dropped when the per-texture slot list went away), clearing the last
  unused-function warning from `meshtools.cpp`.
- Texture-preview channels are now **image-editor / Blender style**: a plain
  **left-click isolates a single channel** (switches the view to just that one,
  shown greyscale); **Shift+click** adds or removes a channel from the current
  selection. Previously every button was an independent toggle.
- Made a selected **alpha a no-op when mixed with colour channels** — e.g.
  `R+A` now shows the red channel instead of washing to white. Alpha still
  reads on its own via the single-channel greyscale path.
- The **UV grid overlay is now adaptive**: the 0..1 space always shows bright
  quarter (major) lines and a boxed border, and **finer subdivisions fade in as
  you zoom in**, like Blender's UV editor (up to 256 divisions at max zoom).
- The **Material Manager dock now opens at a comfortable default width (~690px)**
  the first time it is shown, with the texture preview taking the larger share
  below a compact material list. This is a one-time default (persisted via
  `MatTexManager/InitialWidthSet`), so the user's saved layout and drags win
  afterward.
- Added an **always-on vertical scrollbar** to the *Materials in file* list.

## 2026-07-12 — Material Manager tree & preview refinements

- Made the Material Manager tree **material-first**: the primary column is now
  **Material** (the `.bgsm`/`.bgem` file, or shader type) that unfolds into its
  textures, with the owning **Mesh** name in the adjacent column. Column-aware
  navigation and the right-click *Reveal* action follow suit (Material → shader
  block, Mesh → owning node, Path → the string field).
- Selecting a material now highlights **only that material row** in blue/orange,
  not its texture children — matching how Blender highlights a parent without
  its children.
- Texture preview channels: a single selected channel shows **greyscale**; two
  or more compose **opaquely** so a zero/low alpha (common on diffuse and
  spec/gloss maps) can no longer make the preview go black. A selected alpha
  adds as grey.
- Consolidated find & replace into a single **Replace...** button that opens a
  small Notepad++-style dialog (Find what / Replace with / Match case / Replace
  All), leaving just the live **Filter rows** box on the toolbar.
- Removed the preview's redundant per-texture slot list and type buttons now
  that textures are selectable directly in the tree; the preview pane is a
  single full-width canvas. Selecting a material previews its diffuse; selecting
  a texture row previews that texture.

## 2026-07-12 — Material Manager & texture-preview pass

- Rebuilt the **Material Manager** to be genuinely material-centric rather than
  a flat imitation of the Collision Manager's chevrons. Each material/node is
  now one compact collapsible **parent row** with its textures listed beneath
  it as semantic child rows (Node / Texture / Path / Status columns). Groups
  fold and expand via the chevron, double-click, or the row context menu
  (**Fold / Expand This Node's Rows**), matching the Collision Manager's row
  density and expansion behaviour.
- Constrained the texture-preview **wheel zoom** to a sane range (0.125x–8x)
  instead of the previous unrestricted zoom, and de-duplicated redundant zoom
  updates below a 0.0001 threshold.
- Fixed **multi-channel toggling** in the texture preview: the channel mask is
  now applied as an independent 4-bit R/G/B/A field (`mask & 15`) so channels
  can be enabled in any combination rather than clobbering one another.
- Added **middle-mouse camera pan** to the Texture Preview window, Blender
  style — hold the middle button to drag the view; release restores the cursor.
- Reordered the block context menu so the **Block** submenu sits directly
  beneath **Transform** regardless of static spell-registration order, and
  guarded `SpellBook::sltSpellTriggered` so native application actions hosted in
  the book (e.g. the Block List Hierarchy submenu) no longer fall through the
  spell-casting path.
- Traced and unified the **posed-skeleton edit rendering**: edit-mode overlays
  and the shaded mesh now derive their world transform from the same
  `shapeRenderTrans` (inverse camera × the shape's `viewTrans`), so a skinned /
  posed shape's edit wireframe follows the identical skinned positions used to
  draw its shaded surface.
- Relocated the **Material Manager** from the bottom dock to the right dock
  area so it sits alongside the Collision Manager, and restricted it to the
  left/right areas. It no longer opens as a wide floating window; the Materials
  workspace now activates a vertical right-side inspector.
- Mirrored the Collision Manager's design language onto the Material Manager:
  matching panel margins/spacing (6px), a bold **Materials in file — N
  material(s), M texture(s)** inventory header, a 150px minimum table height,
  alternating row colours, and the same blue/orange selection styling.
- Rebuilt the Material Manager list as a real **`QTreeWidget`** (materials own
  their textures as native children) so it behaves like the Collision Manager
  node list: textures start folded, unfold via double-click or the native `>`
  expand arrow (single-click no longer toggles), the sort indicator shows only
  on the column being sorted by, and the list has its own horizontal
  scrollbar. Path editing is restricted to the Path column via an item
  delegate (F2 / drag-drop / browse); double-clicking a texture opens the
  archive browser, and the right-click **Fold/Expand This Material's Textures**
  drives the native expansion state.
- Nested the **Texture Preview** at the bottom of the Material Manager inside a
  draggable vertical splitter, updating live as rows are selected. A **Detach**
  button (or the toolbar's *Texture Preview…*) pops it out into its own window;
  closing that window re-docks it under the list.

## 2026-07-11 — Scene hierarchy parenting

- Added Blender-style **Set Parent** (`Ctrl+P`) and **Clear Parent** (`Alt+P`)
  menus in object mode, with matching viewport context-menu actions.
- Any `NiNode` subclass can be a parent. Any compatible `NiAVObject` scene
  block can be a child, including `NiNode`, `BSTriShape`,
  `BSSubIndexTriShape`, and other renderable scene-object subclasses.
- Added keep-world and keep-local parenting modes, optional additional-parent
  links, multi-selection with the active node as parent, cycle prevention, and
  snapshot undo. Clear Parent can preserve the child's world transform.
- Non-scene data/property blocks are rejected rather than producing invalid
  `NiNode` child links. Clear Parent Inverse remains visibly unavailable
  because the NIF scene graph has no Blender-style parent-inverse field.
- Added a native **Hierarchy** submenu to the Block List context menu. It
  exposes Set Parent and Clear Parent for the current single or multi-row
  selection; `Ctrl+P` and `Alt+P` now also work while the pointer is over the
  Block List.
- Replaced manager entries in **Panels** with an adjacent **Workspaces** menu:
  Default, Animation, Materials, and Collision. Only the selected manager
  workspace is displayed, and the choice persists between sessions.
- Added **Anim Static** and **Stairhelper** collision-creation presets with
  appropriate layer and fixed/keyframed physics defaults.
- Collision Layer and Material are now compact searchable selectors with
  contained scrolling. The complete FO4 layer table is exposed explicitly,
  including Stair Helper (31), and the physics label is shortened to
  **Material**.
- The Materials workspace now docks along the bottom, preserving viewport
  width and giving the texture preview a wider canvas. Panels and Workspaces
  now use monochrome icons drawn by NifSkope's existing toolbar icon system
  instead of platform-native file/desktop icons.
- Corrected vanilla hknp body decoding: collision filters are read from body
  cinfo `+0x14` (with compatibility for early local builds that used `+0x1C`),
  so `airportroomstairs01.nif` now resolves its ramp body as Stair Helper (31)
  instead of Static. Convex-body material IDs are resolved through the
  embedded `hknpBSMaterialProperties` table, recovering StoneStairs for that
  body instead of displaying the unrelated `0x26067D15` header value.
- Collision Layer and Material selectors now repopulate after a file load and
  use a committing contains-filter completer: typing narrows the popup, and
  clicking a result writes that exact enum value rather than reverting during
  the following model refresh.
- Fixed editable Layer/Material searches writing value zero while the user was
  still typing. They now have dedicated commit paths: incomplete filter text
  performs no model write, and choosing a real result writes only that field
  before rebuilding the manager. Collision Layer therefore no longer snaps
  back to Static after selection.
- Restored cross-file **Copy Branch / Paste Branch** on Windows by returning
  branch and block clipboard MIME names to an ASCII format. Paste remains
  compatible with clipboard data produced by the interim Unicode-separator
  builds, and the Block List once again recognizes branch data instead of
  hiding Paste Branch from its right-click menu.

## 2026-07-10 — Collision Manager feedback pass

- Streamlined the Collision Manager while retaining NifSkope's native dark Qt
  styling: sorting now lives entirely in the clickable column headers; a
  contextual split action exposes Decompile Selected/All or Compile Selected;
  Check Collision and Import Donor are compact actions; refresh and Collision
  -> BSTriShape (renamed Create Editable Mesh Copy) moved to More. Creation now
  distinguishes New collision material from Selected collision material,
  exposes Optimize Source Mesh beside Create Collision, and hides method-specific
  controls when irrelevant. Compiled rows use a concise read-only summary with
  Decompile to Edit Physics instead of a wall of disabled fields, while editable
  rows show the inspector and a collapsed Advanced physics section.
- Normalized Fallout 4 Collision Manager labels to Bethesda's 3ds Max
  exporter vocabulary without changing stored enum values. The base material
  list now uses names such as `ActorCrabArmored`, `Bone`, `WeaponAxe1Hand`
  and `WoodBarrel`; collision layers use `Tree`, `Prop`, `Small Debris`,
  `ShellCasing`, `Character Controller`, `NavMesh Cut`, `spellTrigger`, and
  the other exporter spellings. Internal CK identifiers and exact CRCs remain
  visible in material tooltips.
- Added Bethesda exporter-style **Collision group and advanced physics**
  controls for editable rigid bodies: Keyframed, Linked Group, Collision
  within Group, Wind, packed collision-filter Group, local center of mass,
  inertia-tensor diagonal, allowed penetration, and Deactivator Type. Center,
  inertia, filter flags and filter group are also preserved through the native
  hknp Compile/Decompile round trip. Settings whose compiled byte layout is not
  yet validated (Keyframed, Wind, nonstandard quality/solver/deactivator and
  penetration) remain fully editable but deliberately block Compile instead
  of being silently lost. Phantom/Shape Phantom are shown with an explicit
  disabled explanation until their distinct object/body graphs and hknp
  classes are implemented.
- Restored the complete six-mode creation strip from the visual design:
  **Box** (PCA-oriented fit), **Sphere** (bounding-sphere fit), **Capsule**
  (principal-axis fit), **Hull** (qhull), **Decomp** (CoACD), and **Mesh**
  (`NiTriStripsData` / `bhkNiTriStripsShape` / MOPP chain). Preset, collision
  type, material and Replace now feed the created body; Decimate remains
  beside the conversion controls.
- Collision rows are expandable. Each collision object/body is the parent and
  every compiled or editable child shape appears underneath it with its own
  type, material and state. Expansion survives live refreshes.
- Restored the preview controls for colour mode, solid, X-ray, collision-only
  and labels. They now affect both compiled and decompiled collision, while
  collision-only hides render geometry but keeps the ground grid visible.
- Renamed collision **Decode / Decode All** actions to **Decompile / Decompile
  All**, and renamed the Timeline dock to **Animation Manager**.
- Expanded editable physics controls with motion system, quality, solver
  deactivation, damping and velocity limits; matched Collision Manager row
  selection colours to the Block List; redesigned the Panels dropdown.
- Rebuilt the Physics section around named enum selectors: collision layer,
  Havok material, motion system, quality and solver deactivation now show
  readable names and write directly to the selected editable rigid body/shape.
  Convex Decomposition is now correctly nested as a Convex generation method
  rather than presented as a collision shape type.
- Added persistent sorting by Node, Shape, Collision type, Material, Mass or
  State, with ascending/descending order and clickable-column synchronization.
  Collision rows now have a right-click menu for decompile, decompile all,
  compile status, lint, expand/collapse, Block List selection, material copy,
  creation-default material and refresh.
- Material CRCs now resolve through the active game's Havok material enum after
  decompilation. A `+` material action stores custom name/value pairs and writes
  their numeric value to editable shapes. Decompiled layer zero is normalized
  to STATIC or PROPS from body motion, eliminating misleading UNIDENTIFIED
  collision types in both new and already-decompiled files.
- Corrected collision-material naming against Bethesda's 3ds Max exporter
  labels: CRC 911716378 now displays as `Concrete` instead of the fabricated
  `StoneConcrete`, while tooltips retain `MaterialStoneConcrete` and the exact
  hexadecimal/decimal CRC. The Create field now searches all 157 Fallout 4
  enum entries and accepts exporter labels, internal IDs, legacy shortened
  names, custom names and numeric CRCs. Unlisted values are explicitly shown
  as `Unknown (0x...)` instead of being mistaken for decimal material names.
- Simplified creation buttons to Box, Sphere, Capsule, Convex and Mesh, changed
  them to neutral gray styling, and removed the redundant creation-panel
  collision-type selector; body type is edited in Physics.
- Added a non-destructive live collision preview for Convex, CoACD decomposition,
  accurate Mesh and decimated Mesh creation. A Blender-style floating operator
  panel appears at the viewport's bottom-right with triangle percentage, hull
  precision, decomposition threshold and maximum hull controls. Changes are
  debounced and drawn as an amber world-space overlay; Apply writes the chosen
  collision graph while Cancel/close clears the overlay without touching the
  NIF. The Collision Manager's Decimate action now opens this preview at 50%
  instead of modifying render geometry through the old Simplify dialog.
- Matched the collision preview operator to the transform shortcut panels:
  compact dark layout, neutral action buttons, close/collapse header and the
  same Blender-style number wells. Triangle percentage can be held and dragged
  left/right (Shift-drag for fine control, click to type), with throttled live
  preview updates during the scrub; hull precision and threshold support the
  same interaction.
- Completed the workflow accelerators around the manager: **Import Donor**
  copies an editable or compiled collision branch from another same-version
  NIF; **Mass from Material** calculates closed-mesh volume and applies a
  density-family mass estimate; and **Collision -> BSTriShape** creates a
  visible, editable render proxy under the collision's owning node. Structural
  operations are snapshot-undoable where applicable.
- Expanded **Check Collision** into a guided linter. It now reports dangling
  objects, unidentified layer 0, mixed compiled/editable state, hulls over 64
  vertices, box-like hulls, non-uniformly scaled primitives, STAIRHELPER
  primitives with no slope, and visible geometry with no collision in its
  node hierarchy. Its safe-fix pass removes dangling branches and infers layer
  1/10 from body motion in one undo step.
- Added amber per-row and footer collision budget warnings for meshes over 500
  triangles, packfiles over 128 KiB, or files over 2,000 collision triangles /
  256 KiB. Warnings point directly to the live Decimate workflow.
- **Decompile** and **Decompile All** now each create one whole-model snapshot
  undo step, including the multi-system command.
- Added the native `hknpencode` writer and enabled **Compile** for editable
  collision. It writes an FO4 hk_2014.1.0 compressed-mesh packfile with local,
  global and virtual fixups, material/layer, static or dynamic physics,
  calculated density and AABB inertia; replaces the legacy branch with a
  `bhkPhysicsSystem` + `bhkNPCollisionObject`; and rejects the operation unless
  NifSkope's own decompiler successfully round-trips the generated geometry.
  The first writer is deliberately one section (maximum 255 vertices and 255
  triangles); the manager directs larger meshes through Decimate first.

## 2026-07-10 — Collision Manager first functional slice

- Added a right-side **Collision Manager** dock to the Panels menu. Its live
  browser lists every compiled `bhkNPCollisionObject` and editable
  `bhkCollisionObject` with owning node, shape summary, collision layer,
  material CRC, mass, and state. Selecting a row selects the real NIF block.
- Wired the existing **Decode**, **Decode All**, **Create Convex Shapes**, and
  **Simplify/Decimate** operations into the manager. The physics panel
  live-edits mass, friction, restitution, and collision layer on editable
  rigid bodies; compiled rows remain read-only with a Decode hint.
- Added **Create Collision…** as the user-facing Havok creation spell and as
  a direct object-mode viewport right-click action. Compiled collision gets a
  matching contextual **Decode Collision** action; both remain available from
  the normal block spell menu too.
- Added a first **Check Collision** pass (dangling references, mixed compiled
  and editable state, suspicious >64-vertex hulls) and a footer budget showing
  collision vertices, triangles, and compiled packfile bytes.
- Compiled collision now supports the manager's persisted **Solid**, **X-ray**,
  and **Colour by** settings: translucent fill plus a full-alpha type-coloured
  wire overlay. Selection highlight still overrides the palette.
- This first-slice note is superseded by the completed workflow/encoder entries
  above. The manager was release-built and passed an offscreen startup test
  while loading BoxStaticCollision.nif.

## 2026-07-09 — Motion / mass / layer decoded into bhkRigidBody

- **Decoded rigid bodies now carry real physics instead of defaults.** The
  decode spell fills each created bhkRigidBody's Rigid Body Info from the
  packfile: collision layer, friction, restitution, damping, gravity factor,
  max velocities, and for dynamic bodies mass + center of mass, with the
  authoring enum values the 3ds Max exporter writes (dynamic: Motion System 3
  / Quality 4 / Solver Deactivation 2; static: 5 / 0 / 1).
- Sources, all validated against the controlled Elric pairs
  (Documents/3dsMax/export) and the CK FileConvert XML oracle:
  - PSD+0x10 body_props, **stride 0x50 per body**: friction (trunc-float16)
    +0x12, restitution +0x16
  - PSD+0x20 dyn_motion / PSD+0x30 dyn_inertia (present only on dynamic
    systems): damping/velocity defaults; inverseMass +0x04 (PropCollision
    mass 10 -> 0.1 exact), density +0x08, inertia diagonal +0x20
  - BodyCInfo **+0x1C = collision layer** (matches raw Havok Filter 10/1/31),
    +0x0C = motion index (0x7fffffff = static body)
- Cross-checked against BadDogSkyrim/PyNifly's independent RE (which also
  confirms the Body ID / node-placement rule and documents a working packfile
  *writer*). Two PyNifly errors found and corrected by our controlled pairs:
  their body_props stride 0x110 only fits single-body files (real stride
  0x50), and their collisionResponse@+0x10A reads past the entry.

## 2026-07-09 — Body ID binding: each node places its own body

- **The real placement rule found** (supersedes the earlier "root space"
  entry, which was wrong): each `bhkNPCollisionObject` names its body via the
  **Body ID** field, and that body is placed by **that node's transform**.
  The hknpBodyCinfo position/orientation is only Elric's rest pose — on
  vanilla stair helpers the two differ, and the node transform is the one
  that puts the ramp at ground level (cinfo floated it a story up: the
  stubborn stair-helper offset). On many files node transform == cinfo
  position, which is why cinfo-only ever looked plausible.
- **Preview**: each collision object draws exactly its own body (matched by
  Body ID) in its own node's space — duplication is structurally impossible.
  The decoder tags every shape with its owning body's id and no longer
  composes the cinfo transform into the shapes.
- **Decode spell**: one bhkRigidBody + bhkCollisionObject per body, attached
  to that body's own node (the exact inverse of Elric's compile, matching the
  pre-Elric Max-export layout). Shapes with no owning body decode with body 0,
  same rule as the preview.

## 2026-07-09 — Shared collision system no longer duplicated

- **Root cause of "duplicated + wildly rotated" collision found** (credit:
  user spotted the shared reference): one bhkPhysicsSystem is often
  referenced by several nodes (a platform node plus its ramp nodes all point
  at the same block). The preview drew the whole system once per referencing
  node, each in that node's own space — so N copies at N different
  transforms. Now the system is drawn once, from its first referencing
  collision object.
- **Decode spell matches**: consolidates the shapes onto the first node and
  removes every referencing bhkNPCollisionObject plus the system (previously
  it removed only the first, leaving the others with dangling references).

## 2026-07-09 — Collision decode hardening

- **Body rotation formula corpus-validated**: batch-checked against 300
  vanilla architecture files (21 transformed helper bodies) — the shipped
  convention wins by 4–14x over every alternative; residual differences are
  physically expected ramp-tip overhangs. Validator: batch_validate.py.
- **CMS→CMSD pairing exact**: compressed-mesh geometry is resolved through
  its real pointer (global fixup at +0x60), not file order — vanilla files
  order objects differently than tool exports (was causing skewed duplicate
  collision).
- **Material write fixed**: decoded material CRCs now actually land in the
  created shapes' Material fields (the previous nested write failed with
  "Could not find Material subitem" warnings).

## 2026-07-09 — Collision body transforms (stair helpers, multi-body files)

- **Stair helpers and multi-body collision now land in the right place.**
  Each Havok body's position + orientation (hknpBodyCinfo) is decoded and
  applied — previously separate bodies (stair slope helpers, secondary
  volumes) collapsed to the origin, which also looked like "duplicated,
  slightly offset" collision overlapping the main mesh. Body transforms
  compose with compound instance transforms, and shapes shared by several
  bodies are instanced once per body. Applies to both the preview and the
  Decode Compiled Collision spell (wrapped as transform shapes).
- Validated against vanilla BldgBrick3Story1x2ResEntB.nif (stair helper at
  its correct offset and 30° slope).
- Note: the CK's FileConvert.exe (Downloads/Examples) works as a reference
  XML dumper for pure-Havok blobs but rejects Bethesda-custom classes
  (hknpBSMaterialProperties) — the native decoder handles those fine.

All changes made to this fork on top of fo76utils/nifskope (develop @ f2587869).
Newest entries first. This document is kept up to date with every change batch;
`TO_BE_IMPLEMENTED.md` holds the forward-looking backlog.

---

## 2026-07-08 — Compiled collision: preview + decode to editable blocks

- **Elric-compiled collision (bhkPhysicsSystem) is now decoded natively**
  (src/gl/hknpdecode.*): Havok 2014 packfile parsing with convex polytopes
  (boxes/hulls), compressed triangle meshes, spheres, capsules, and
  compound-shape **instance transforms** (position/rotation/scale per shape,
  byte-validated against pre-Elric bhkConvexTransformShape matrices).
- **Preview**: with Show Collision enabled (display options), compiled
  collision draws as amber wireframe — display only, blocks untouched.
- **Right-click > Havok > "Decode Compiled Collision"** converts a
  bhkPhysicsSystem back to its pre-Elric form: bhkBoxShape /
  bhkConvexVerticesShape / bhkSphereShape / bhkCapsuleShape /
  NiTriStripsData chains, transformed instances wrapped in
  bhkConvexTransformShape / bhkTransformShape with the verbatim matrix,
  multiple shapes in a bhkListShape, all under a fresh bhkRigidBody +
  bhkCollisionObject. "Decode All Compiled Collision" converts every system
  in the file. MOPP is left empty (Update MOPP Code / Elric rebuilds it).
- Capsule radii are recovered within ~0.3% (Elric shrinks them into a core
  hull + margin); sphere centers fold into the wrapper transform.
- Fixed: nav gizmo-compass fading out when collision display is on (leaked
  GL_LINE polygon mode broke the QPainter overlay).
- Not yet decoded: material / mass / motion metadata (new bhkRigidBody gets
  defaults), multi-body ragdoll systems.

## 2026-07-08 — Fixes + snap-to-active + compiled-collision groundwork

- **Edge loop select fixed**: Alt+click was being eaten by NifSkope's
  background color sampler (that's why the viewport background turned
  white/orange/black). In edit mode Alt+LMB is now always a selection click.
- **Snap menu (Shift+S)**, edit mode: new **Selection → Active** (collapse
  the selection onto the last-picked element) and **Cursor → Active**.
- **Circle select got the Deselect panel**: each paint stroke pops the
  gesture redo panel (like box select); Deselect re-applies the same brush
  stroke subtractively.
- **Compiled collision investigated** (bhkNPCollisionObject →
  bhkPhysicsSystem): the Elric-compiled blob is a Havok 2014 packfile;
  convex shapes are fully decoded, triangle meshes partially. Analysis
  tools in `tools/hkdump.py` + `tools/hkparse.py`; findings + display plan
  in TO_BE_IMPLEMENTED.md.

## 2026-07-08 — Blender selection batch

- **Circle select (C)**: brush-paint selection in object and edit mode. LMB
  paints select, MMB paints deselect, mouse wheel resizes the brush, RMB/Esc
  exits. One undo step per paint stroke. (3D-cursor placement moved off the
  C key — see below.)
- **Select More / Less (Ctrl+= / Ctrl+-)**: grow the edit-mode selection one
  connectivity ring, or shrink it by dropping its boundary elements.
- **Edge loop select (Alt+click, Shift+Alt extends)**: walks the most
  collinear continuation edge (per-vertex, skipping edges that share a
  triangle with the current one); boundary edges walk the mesh boundary —
  so it traces borders and straight strips well even on triangulated meshes.
  Turns sharper than ~60° stop the loop.
- **Shift+RMB places the 3D cursor** (the true Blender binding). Plain C no
  longer places the cursor; Shift+C (cursor to picked median) is unchanged.
  The Shift+RMB alternate context-menu pick is retired.
- **Frame Selected (Numpad-. or .)**: centers and zooms the camera on the
  current selection (object or edit mode); with nothing selected it frames
  the whole model. Also in the Viewpoint toolbar dropdown.
- **Collapsible redo panels**: clicking the ˅ title of any redo panel
  (transform / operator / box select) collapses it to just its title bar,
  ˃ expands it again — Blender-style.

## 2026-07-08 — Modal transforms own the mouse (unbounded drag)

- **G/R/S drags no longer stop at the viewport edge.** The modal gesture now
  grabs the mouse for its whole life (Blender): dragging over the block list,
  docks, or outside the window keeps moving the selection, and when the cursor
  hits a screen edge it wraps around to the opposite side so the drag is
  unbounded. The keyboard is grabbed too, so X/Y/Z axis locks, typed values,
  Enter/Esc all reach the gesture even when the block list had focus.

## 2026-07-08 — Box-select redo panel + Blender-look panels

- **Box-select redo panel**: applying a box select pops a small panel at the
  bottom-left with a **Deselect** button — one click re-applies the same
  rectangle subtractively (i.e. "I meant to deselect that"). The rectangle is
  screen-space, so use it before moving the camera.
- **Redo panels restyled like Blender**: bold "˅ Move / Rotate / Scale"
  header, values stacked vertically ("Move X" / Y / Z with right-aligned
  labels), dark rounded value wells, Axis/Orientation rows underneath. Same
  dark styling on the Merge/Select-Linked and Box Select panels.
- **Drag-number fields polished**: the ‹ › step arrows now only appear while
  hovering the field (and hide while typing), and the field lights up while
  you click-drag to scrub its value — matching Blender's number widgets.

## 2026-07-08 — Box select is now additive

- **Box select (B)** no longer replaces the selection: a plain drag only ever
  **adds** what's inside the box, and **Shift- or Ctrl-drag deselects** what's
  inside the box. Use A (deselect all) or click empty space to start fresh.
  Applies to object and edit mode alike.

## 2026-07-08 — Operator redo panels, context menu, camera re-center, selection memory

- **Operator redo panel** (Blender-style, bottom-left of the viewport): after
  **Merge by Distance** or **Select Linked by Angle**, a floating panel shows the
  distance / sharpness value in a drag-number field; scrubbing or typing a new
  value undoes and re-runs the operation live. The old pop-up dialogs are gone
  (the Render-menu "Select Linked by Angle..." entry now uses the panel too).
  If something else touches the undo stack the panel freezes instead of
  corrupting history. Transform and operator panels swap — only one shows at a
  time.
- **Edit-mode redo panel**: the Move/Rotate/Scale panel now also appears after
  vertex/edge/face transforms (G/R/S in edit mode), and editing its values
  re-applies the element transform. (Initial value display for rotate/scale
  wired same day.)
- **Drag-number fields (DragSpinBox)** fixed: click-drag left/right scrubs the
  value (Shift = fine), plain click types, hover shows ‹ › step arrows. The
  original implementation never received mouse events (the spin box's internal
  line edit swallowed them); it now event-filters the line edit.
- **Menu auto-close** fixed: operator pop-ups (Delete/Merge/Separate/Snap/Set
  Origin) close when the pointer moves away. Hover-out is now detected by a
  60 ms cursor-position timer (mouse-move events are never delivered while a
  menu has the pointer grab).
- **Viewport right-click menu** extended, both modes: Select All (A) /
  Deselect All / Invert (Ctrl+I) / Box Select (B), Snap… (Shift+S), Set
  Origin… (Shift+Ctrl+Alt+C), and a 3D Cursor submenu (snap cursor to
  picked/world origin; move verts to cursor in edit mode, snap node to cursor
  in object mode). Select All / Deselect All are also new as explicit actions
  (the A key keeps its Blender toggle behaviour).
- **Camera re-center button** on the render toolbar (bracket-frame icon, next
  to the Viewpoint dropdown): one click re-centers the camera on the model
  (same as Render > Center, Shift+C).
- **Edit-mode selection memory**: leaving edit mode remembers each mesh's
  selected vertices/edges/faces; re-entering edit mode on the same object
  restores them (world positions re-derived, element pick modes re-enabled to
  match). Deselecting everything is remembered too. Cleared when a new file is
  loaded. Caveat: selections are keyed by block number, so operations that
  renumber blocks (e.g. Separate) can shift what a remembered selection refers
  to; out-of-range entries are dropped safely.
- **Version string** renamed to "NifSkope - Wild Wasteland Edition 0.1
  (build rev, date)".

## 2026-07-08 (earlier) — Snap persistence, node transforms from the block list

- Snap settings persist across sessions (target mode, base, affect, align
  rotation, default-on, grid step, rotation step).
- G/R/S work on a node picked in the **block list** (not just a viewport
  pick): the shortcut is routed app-wide while the pointer hovers the
  viewport; the gizmo and a new always-on origin dot (orange) appear for
  geometry-less nodes (NiNode, lights) so they are visible and grabbable.
- Snap marker sits on the snap **target** (Blender), axis-constrained snaps
  move only along the locked axis, element scale respects the axis constraint.
- Merge (M): At Center / At Cursor / By Distance with union-find welding,
  triangle compaction and skin resync.
- Timeline: switching the displayed animation no longer changes the block
  selection.

## 2026-07-07 — Edit-mode operations batch

- **Delete** (X): Blender-style menu — Vertices / Edges / Faces / Only Faces;
  packed BSTriShape vertex compaction, triangle reindex, bounds + skin resync
  (NiSkinData remap, stale NiSkinPartition dropped; FO4 inline skin needs no
  fixup). Snapshot-undoable.
- **Box select** (B) in object + edit mode with X-ray awareness; object mode
  tests actual geometry (not node origins, which sit at the skeleton root for
  skinned meshes). **Invert selection** (Ctrl+I) in both modes.
- **Selection undo** (Ctrl+Z steps back through selections on a dedicated
  stack; a mesh edit resets it so Ctrl+Z undoes the edit).
- **Select Linked** (Ctrl+L; by-angle variant with adjustable sharpness),
  hide/unhide (H / Alt+H), Separate (P) / Join (Ctrl+J) / Duplicate (Shift+D).
- Shading modes: Flat / Solid / Shaded (exclusive) + independent Wire overlay
  and X-ray; display options persist. Set Origin menu (Shift+Ctrl+Alt+C):
  geometry↔origin↔cursor, works on plain NiNodes.
- Particle effect shading: particles get the BSEffectShaderProperty features
  (falloff, greyscale palette, env cube-map) without breaking alpha masks;
  NiPSysColorModifier lifetime gradients.

## 2026-07-06 — Blender transform suite + particle/VFX preview

- Draggable gizmo handles (arrows/rings/boxes), numeric G/R/S input, transform
  orientation (Global/Local/Parent/View) + pivot menus, element pick modes
  (1/2/3), snap to vertex/edge/face with align-rotation, 3D cursor (C /
  Shift+C), transform redo panel (floating, bottom-left).
- FO4 particle preview: CPU NiPSysUpdateCtlr simulation (box/cyl/sphere/mesh
  emitters, gravity/drag/colour/scale modifiers, flipbooks), procedural
  lightning (BSProceduralLightningController) with textured bolt ribbons,
  BSPositionData decoded + Generate spell fixed (raw u16 triangle indices),
  screen-space refraction preview.
- Multi-node object selection with stencil silhouette outlines; free camera
  (Shift+F).

## 2026-07-04 — Animation timeline dock (v1–v3)

- New bottom dock: sequence selector, per-controller keyframe lanes, value
  graph, transport controls, key inspector; drag/snap/insert/delete/copy/
  paste/scale/nudge keys, easing + tangent handles, interpolation switching,
  CSV round-trip, lint with guided name fix.
- Rigging spells: Setup Controllers, Remove From Animation, Duplicate+Scale
  Sequence, Bake B-Spline. Solo view (Alt+Q). Modal Blender gizmo (G/R/S,
  X/Y/Z, Ctrl snap) with Auto-Key.
- Undo model: value edits merge into ChangeValueCommand transactions;
  structural edits snapshot the whole model (NifSnapshotCommand).

---

_Known gaps / backlog: see `TO_BE_IMPLEMENTED.md`._

## 2026-07-13 — NIF Browser resource warning fix

- The configured-resource mesh indexer now ignores dedicated `textures` and
  `materials` directories and silently skips other unreadable/non-mesh paths.
  Expected resource mismatches no longer generate one modal warning per path;
  the skipped count is available in the `Available NIFs` tooltip.
- Added independent **Load Archives** and **Load Loose NIFs** source toggles.
  Either source can be viewed alone, while enabling both merges them into the
  same virtual folder tree using normal configured-resource precedence.
- Opening a browser result no longer enrolls it in the combined workspace.
  **Add to Loaded NIFs** on the file context menu, or drag selected browser
  files into the lower pane, to add them explicitly. New entries start inactive;
  lower-pane multi-selection controls which secondary NIFs are rendered and
  offered to donor tools, while the orange primary remains the editable file.
  The loaded list now uses ordinary Block-List-like grey/selection styling and
  explicit as-needed scroll bars.
- Fixed ordinary NIF Browser double-click/Open creating a successfully loaded
  but hidden document. Foreground opens now create and activate a visible
  NifSkope window; only explicit **Add to Loaded NIFs** background-loads it.
- Loaded NIF selection now uses the exact Block List palette: light-blue and
  primary orange text for the editable document, dark-blue and secondary
  orange text for selected donors, and the normal grey row for inactive NIFs.
- Loaded NIFs are now true background model containers: they are never shown,
  so adding one no longer flashes a black window. Closing the visible primary
  closes its entire loaded-NIF workspace instead of promoting hidden document
  windows one by one.

## 2026-07-11 — Material workspace and collision usability

- Collision Manager content now scrolls inside the dock, so collapsing the
  Advanced section cannot push its footer below the taskbar.
- Mass from Material uses winding-independent hull volume and reports open or
  degenerate collision clearly.
- Material Manager now keeps its resource table focused, while Texture Preview
  is a separate movable, deliberately non-dockable tool window. Material slots
  are labelled by purpose, R/G/B/A can be isolated or combined, the canvas can
  zoom and pan, and the UV-tile overlay persists independently. Owner resources
  use compact Collision-Manager-style folding in the material list.
- Added Material spells and matching manager actions to fill a
  BSShaderTextureSet from BGSM, safely or fully synchronize shader properties,
  compare/lint material setups, copy/paste complete setups, batch-assign to
  selected shapes, select every user, and find/share identical shader blocks.
- Manager status badges identify missing, out-of-sync, overridden and shared
  resources. The Animation Manager overhaul was recorded in the backlog.
- The three viewport shading icons are consolidated into one dropdown with
  Flat, Solid, Shaded, and Normal + Spec/Gloss modes; the diagnostic mode
  removes diffuse colour while retaining authored normal and gloss response.
