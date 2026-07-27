# Modeling Tools — Geometry Creation & Connection Plan

> **BACKLOG MOVED — 2026-07-21.** This file is **design detail / history only**.
> The single authoritative list of what is left to implement is
> **`TO_BE_IMPLEMENTED.md`**. Do not record open work here and do not trust any
> status claim below: this file's own claims have been wrong in both directions
> before, which is exactly why the backlog was consolidated into one place.


Drafted: 2026-07-15. Status 2026-07-16: **ALL BATCHES SHIPPED except Bevel**
(user testing pending). Live: Extrude (E), Fill/Bridge (F, with Cuts/Twist),
Loop Cut (Ctrl+R), Edge Slide (Shift+V), Subdivide, Inset (I), Dissolve
Vertices (Ctrl+X), Symmetrize, Flip/Recalc Normals, Add Primitive (Shift+A),
all on in-place undo (TlExtrudeCommand / TlMeshGrowCommand /
TlShapeStateCommand) with Redo Panel v2. DEFERRED: Bevel (tri-mesh corner
terminations — do not ship a guess); Delete/Merge/Duplicate still
snapshot-undo; old Merge/Select-Linked panels not yet on Panel v2. Kernel
still lives as glview.cpp statics; promote when the UV editor needs it.
Goal: take the fork from a mesh *editor* (select/transform/delete/merge/rip)
to a mesh *modeler*: Blender's geometry **creation** (Extrude, Inset, primitives)
and **connection** (Fill, Bridge, Connect, Loop Cut) operators, in the 3D
viewport's edit mode, each with a full Blender-style adjust-last-operation
panel (numbers + checkboxes + enum dropdowns, like Extrude Region and Move /
Smart Fill).

Companion docs: `UV_EDITOR_PLAN.md` (shipped), `WW_CHANGES.md` (per-batch log).

---

## 1. Hard constraints (NIF / FO4, read first)

- **BSTriShape only for v1** (same scope as Rip / Smart UV Project). Legacy
  NiTriShape gets a "not supported" status message.
- **Triangles only.** No quads/n-gons anywhere in NIF. Every "face" concept
  (inset, fill, bevel profile) must be defined tri-natively; "quad-like"
  behaviour is emulated per triangle pair only where cheap.
- **quint16 triangle indices → hard 65,535-vertex cap.** Every op that grows
  the vertex array must pre-check `numVerts + toAdd <= 0xFFFF` and abort with
  a status message otherwise. Central guard in the topology kernel.
- **Packed vertex rows** ("Vertex Data", stride = `(DataSize - numTris*6) /
  numVerts`): grow via `Num Vertices` + `updateArraySize` + row copy
  (`tlCopyItemValues` / `uvCopyVertexRow` pattern), then recompute
  `Data Size = newN*stride + numTris*6`. Positions may be HalfVector3 —
  write via the established set<HalfVector3>/full-precision-aware path
  (`tlSetVertexLocal`).
- **Skinning is inline** (FO4 BSSkin: weights/indices live in the vertex row).
  Duplicated verts keep weights automatically; **interpolated** verts (fill,
  loop cut, subdivide, bevel) must lerp bone weights of their parents and
  renormalize (pick the 4 heaviest influences).
- **Normals/Tangents/Bitangents are packed ByteVector3 per vertex.** Policy:
  every op that creates or reshapes faces recomputes area-weighted vertex
  normals for the *affected* verts, then re-runs tangent-space generation for
  the shape (reuse `spells/tangentspace.cpp` logic scoped to one shape; there
  is also `spells/normals.cpp` for face normals). "Flip Normals" is a redo
  param on Extrude/Fill, implemented as triangle winding reversal on the new
  faces.
- **Undo = one whole-model snapshot per op** (`nifSnapshotOp`), exactly like
  Rip / Smart Project / Merge. The redo panel re-runs by undo + re-execute
  with new params (existing `reapplyOperator` pattern, stale-guarded by undo
  index).
- **UV editor coexistence:** topology ops fire modelReset/dataChanged; the UV
  editor already rebuilds on structural undo. New verts must get sane UVs
  (copied for extrude/duplicated verts, interpolated for cuts) so the UV
  editor never sees garbage.

---

## 2. Foundation layer (F0) — build once, every operator rides on it

### F0.a Generalized operator parameter system + Redo Panel v2
Today's redo panels are hardcoded: gizmo (3 spinboxes + 2 combos), operator
(1 spinbox), box (1 button); UV panel (2 spinboxes). The screenshots demand
arbitrary typed parameter lists.

- `struct TlOpParam { enum Type { Float, Int, Bool, Enum }; QString label;
  Type type; double value; double mn, mx, step; int decimals; QStringList
  enumNames; }`.
- `GLView` gains `lastOpParams` (QVector<TlOpParam>), `lastOpRun`
  (std::function re-run callback), seed selection, undo index — superseding
  the current int-kind system (kinds 1–3 migrate onto it).
- New signal `operatorPanelEx( const QString & title, const
  QVector<TlOpParam> & params )`; `reapplyOperatorEx( params )`.
- `nifskope_ui.cpp` panel builds rows dynamically inside the existing
  redoPanelQss frame: Float/Int → DragSpinBox, Bool → QCheckBox, Enum →
  QComboBox (styles already in redoPanelQss). Rows are created once (say 10
  max) and shown/retitled per op — same recycle trick as the UV panel.
- Same collapse header / ✕ / freeze-on-stale behaviour as today.
- (Later, optional) UV editor panel migrates to the same spec type.

### F0.b Topology kernel (shared helpers, promote to `src/gl/meshtopo.{h,cpp}`)
Generalize what Rip / Smart UV Project / Delete / Merge already do ad hoc:

- `topoVertexBudgetOk( nif, iShape, adding )` — 65,535 guard + status.
- `topoAppendCopies( nif, iShape, QVector<int> srcVerts )` → first new index.
  (Row duplication: positions, UVs, normals, colors, weights ride along.)
- `topoAppendLerp( nif, iShape, va, vb, float t )` and
  `topoAppendBary( nif, iShape, tri, bary )` — attribute-interpolating append:
  position, UV, normal (slerp-ish then renorm), color, **skin weights (merge +
  renormalize, top 4)**.
- `topoSetTriangles( nif, iShape, tris )` — rewrite tri array + Data Size.
- `topoRecalcNormals( nif, iShape, const QSet<int> & verts )` — area-weighted,
  then tangent-space refresh for the shape.
- `topoEdgeMap( tris )` — edge → adjacent tri list (exists in several ops;
  centralize).
- `topoBoundaryLoops( tris, selVerts )` — extract boundary edges (1 adjacent
  tri) touched by the selection and chain them into **ordered loops** (the
  key structure for Extrude-edges, Fill, Bridge).
- `topoEdgeRing( tris, startEdge )` — walk the perpendicular ring (Loop Cut);
  seed from the existing Alt+click edge-loop walker in glview.cpp.
- `topoSelectNew(...)` — hand the created verts/faces back to `pickedElems`
  (Blender: extrude/inset select the new geometry so a follow-up G/S works).

### F0.c Modal input reuse
The gizmo modal system (grab/wrap, axis constraint, numeric entry, Ctrl snap,
status line) already exists. New ops chain into it:
- Extrude = create geometry, then `gizmoBeginElement(move)` with the axis
  pre-locked to the averaged region normal (free with a second E / G).
- Inset/Bevel = light custom modal (mouse distance = thickness/width, wheel =
  segments, Shift fine, numeric entry) — reuse the numericBuf pattern.
- Loop Cut = hover preview (draw the candidate ring polyline), wheel = cuts,
  click → slide modal → click commits / RMB centers.

---

## 3. Operator catalog (phased)

### Phase 1 — Extrude cluster (forces F0 into existence)
1. **Extrude Region (E)** — vertex/edge/face selections. Duplicate the
   selection's *boundary* verts, keep interior verts, stitch side-wall quads
   as tri pairs along the boundary loops, move the (new) cap along the
   averaged normal via modal move.
   Redo: `Move X/Y/Z` (floats), `Orientation` (Global/Local/Normal/View),
   `Flip Normals` (bool). NIF notes: side-wall winding from boundary
   direction; cap keeps original UVs, side walls copy the boundary verts' UVs
   (degenerate in UV space — user unwraps after; documented).
2. **Extrude Edges/Verts only** — same machinery, no cap logic: an edge run
   extrudes to a tri-pair ribbon; lone verts extrude to... nothing connectable
   (NIF has no loose edges!) → **vertex/edge extrude must always produce
   faces**; loose-vert extrude is rejected with a status hint.
3. **Duplicate (Shift+D)** — exists; migrate its (none) params + note in docs.

### Phase 2 — Connection cluster ("Smart Fill" family)
4. **Fill (F)** — selection resolves to one ordered boundary loop → cap it:
   ear-clipping triangulation in the loop's best-fit plane.
   Redo: `Beauty` (bool: prefer well-shaped tris), `Flip Normals`.
5. **Bridge Edge Loops** — selection resolves to **two** disjoint ordered
   loops (or open runs) → connect with a tri-pair band.
   Redo (the Smart Fill screenshot, tri-native): `Number of Cuts` (int),
   `Twist` (int, rotates loop correspondence), `Interpolation`
   (Linear/Blend), `Smoothness` (float), `Merge` (bool) + `Merge Factor`
   (float — collapse the two loops together instead of banding), `Flip
   Normals`. Cuts create interpolated rings via `topoAppendLerp` (weights
   lerp!). Open-vs-closed loop handling = "Connect Loops" enum
   (Open/Closed/Auto).
6. **Connect Vertex Path (J)** — two selected verts sharing triangles: split
   the shared triangle fan so an edge chain connects them (v1: direct
   neighbours-of-common-triangles only; full multi-face knife path deferred).
   Redo: none (or `Cuts` later).
7. **Weld / Merge** — exists (M); note: cross-*mesh* connection stays Join
   (Ctrl+J) + Merge by Distance; a "Bridge across two meshes" needs Join
   first (documented workflow, not a new op).

### Phase 3 — Refinement cluster
8. **Subdivide** — selected edges/faces; each selected edge gains `Cuts`
   midpoints (`topoAppendLerp`), incident triangles re-triangulated.
   Redo: `Number of Cuts` (int), `Smoothness` (float — post-relax of new
   verts).
9. **Loop Cut (Ctrl+R)** — edge-ring walk from the hovered edge, insert N
   evenly spaced cut rings, then slide modal.
   Redo: `Number of Cuts` (int), `Factor` (float, −1..1 slide),
   `Smoothness`. Tri-mesh definition: the ring walk crosses tri-pair "quads"
   (the two tris sharing the crossed edge); ring ends at boundaries/poles.
10. **Inset Faces (I)** — per-region: shrink the region boundary inward,
    band the rim with tri pairs.
    Redo: `Thickness` (float), `Depth` (float, along normal), `Individual`
    (bool: per-face instead of per-region).
11. **Edge/Vertex Slide (G,G)** — move selection constrained along its
    adjacent edge directions. Redo: `Factor`. (Pure transform — no topology;
    cheap win, big workflow value with Loop Cut.)

### Phase 4 — Finishing cluster
12. **Bevel (Ctrl+B)** — edge bevel: replace selected edges with `Segments`
    parallel strips using `Profile` (0..1) curvature. V1 scope: edges only,
    no vertex bevel, no custom profiles.
    Redo: `Width` (float), `Segments` (int), `Profile` (float), `Clamp
    Overlap` (bool).
13. **Smooth Vertices** — Laplacian relax of selected verts (the UV editor's
    Minimize Stretch, in 3D). Redo: `Factor`, `Iterations`, `Preserve
    Boundary` (bool).
14. **Dissolve Edges/Verts (Ctrl+X)** — remove while keeping the surface:
    merge across the element and locally re-triangulate.
15. **Symmetrize** — mirror the mesh across an axis, weld the seam.
    Redo: `Axis` (enum ±X/±Y/±Z), `Merge Distance`.
16. **Flip / Recalculate Normals (Shift+N / Alt+N)** — expose as edit-mode
    selection-scoped ops (wraps existing normals/tangentspace spell logic) +
    redo panel `Inside` (bool).

### Phase 5 — Creation from nothing (object mode)
17. **Add Primitive (Shift+A)** — Plane / Cube / Cylinder / UV Sphere as a
    new BSTriShape sibling: unit size at 3D cursor, correct vertex layout
    (desc flags from a template or the active shape), shader property cloned
    from the active shape (`tlCloneShapeWithProps` pattern) so it renders,
    simple box/spherical UVs, bounds set.
    Redo: `Size` (float), segment counts (ints per primitive), `Align`
    (World/View/Cursor). This is the modder's "collision proxy / decal plane /
    blockout" tool and needs no new kernel — it's `tlCloneBlock` + generated
    arrays.

---

## 4. Cross-cutting policies

- **Selection hand-off:** every op ends with the *new* geometry selected
  (Blender), via `topoSelectNew` → `pickedElems` (+ selection-undo record).
- **Status line** reports counts: "Extruded 42 verts, 80 tris".
- **UVs of created geometry:** duplicates copy; interpolated verts lerp;
  brand-new walls/caps get the nearest source UV (degenerate) — the UV
  editor + Unwrap is the intended fix-up path. Never leave NaN/garbage.
- **The 65,535 cap** aborts BEFORE any model mutation (single snapshot means
  a mid-op abort would still be one clean undo, but don't rely on it).
- **Skinned meshes:** after any topology op, bounds via `tlUpdateBounds`;
  weights handled per-vert as above; no NiSkinPartition concerns on FO4
  (inline), legacy partitioned meshes are out of scope with BSTriShape-only.
- **Testing per op (the validated recipe):** flat plane NIF + a skinned FO4
  body part; check Num Vertices / Num Triangles / Data Size consistency,
  weights on new verts, normals direction, one-Ctrl+Z revert, redo-panel
  re-run correctness, then an in-game load of the skinned test.

## 5. Explicitly OUT of scope
- **Proportional editing — declined by the user. Do not implement.**
- Quads/n-gons, modifiers, sculpt/remesh, custom bevel profiles, full knife
  tool (K) with arbitrary through-face cutting (J's v1 covers the common
  case), multi-mesh bridge without Join, legacy NiTriShape support.

## 6. Suggested build order
F0.a + F0.b together (panel + kernel) → Extrude Region (proves both) →
Fill + Bridge (the connection payoff) → Loop Cut + Slide → Subdivide/Inset →
Bevel → Smooth/Dissolve/Symmetrize/Normals → Add Primitive (anytime after
F0.b — it's nearly independent). Each op ships + gets user-tested as its own
batch, same cadence as the UV editor phases.
