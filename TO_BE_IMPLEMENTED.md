# NifSkope – WW Edition — To Be Implemented

Backlog of features/fixes agreed for later. Ordered roughly by priority.

## Particle / VFX preview (2026-07-06) - implemented, in-app verify pending
- Particle sprites bound their texture on a stale GL texture unit -> wrong
  texture; now always unit 0 with BSEffectShaderProperty source-texture and
  raw-path fallbacks, plus the shader's (BGEM-aware) emissive tint.
- Mesh/array emitter spawn points were cached in world space at load time,
  before the scene graph was complete -> ~100-unit offset for
  X01_Torso_Tesla_Lightning:0; points are now emitter-local and the node
  transform is resolved fresh at simulation time (also follows animation).
- NiPSysMeshEmitter now honours Emission Type (face surface/center, edge)
  by sampling triangles instead of raw vertices.
- BSProceduralLightningController preview: jagged bolt between the rig's
  *_Start / *_End nodes, midpoint-displacement jitter re-rolled at 24 Hz,
  branches, width/childWidthMult/arcOffset/fade flags honoured,
  Generation/Mutation gated by the sequence's bool timeline keys.
- Bolt strips are textured with the BGEM beam texture (boltstrip.prog),
  V runs along the bolt (256x2048 tile-V sheets) with the shader property's
  animated UV offset/scale applied; drawn after the transparent pass to
  avoid alpha-blend darkening squares.
- Particle flipbook: per-particle random subtexture cell from NiPSysData
  Subtexture Offsets (4x4 lightning atlas), passed as a vertex attribute.

## Mesh / engine features (need in-game or careful testing)

### 1. BSPositionData mesh-emitter spawn distribution - ROOT CAUSE FOUND (2026-07-06), in-game verify pending
- **Was:** the game spawned particles in a single spot from generated data.
- **Cause:** the "numTris*3 + 2" tail of BSPositionData is NOT half-floats:
  it is the mesh's triangle index list as RAW uint16 (verified byte-level
  against vanilla edison_pa_vfx.nif Edison_Torso_Lightning:0), plus a
  trailing (14, 0). The old spell left the region zeroed, so the engine
  sampled triangle (0,0,0) forever -> one spot.
- **Fix shipped:** Generate BSPositionData now writes the triangle indices
  as u16 bit patterns inside the hfloat array (qfloat16 round-trip) and the
  vanilla (14, 0) tail. Meaning of the 14 is still unknown (only one vanilla
  sample); if a regenerated mesh misbehaves in game, compare that value.

### 2. Merge vertices by distance (edit mode)
- Blender "Merge > By Distance": weld picked vertices within a threshold,
  remap triangles, drop unused verts. Requires rewriting the packed BSTriShape
  vertex array (position/normal/tangent/UV/color/weights) and reindexing.
  Snapshot-undoable. Risky – rewrites vertex data, must be tested carefully.

### 3. Detach / separate selected geometry (edit mode)
- Blender "Separate (P)": move selected faces into a new BSTriShape block
  (copy vertex data + triangles, reparent under a NiNode, wire shader/alpha).
  Complementary to merge; same BSTriShape-rewrite risk.

### 4. Rest-pose display in edit mode
- On entering edit mode on an animated frame, show the mesh at its authored
  (unanimated) node transform, like Blender. Needs a per-node override in the
  render transform pipeline for `Scene::restPoseBlock` (already stored).

## Object-mode selection + parenting

### 5. Multi-node selection in object mode - DONE (2026-07-07)
- Shift+click multi-select (objSelection/objActive), Block List active vs
  secondary colours, and the stencil silhouette outline (active #FF9D00,
  secondary #FF7200, white while transforming) all shipped. The old
  selection wireframe was removed in favour of the outline; the Wire
  toggle is now an independent overlay. Underpins #6.

### 6. Parent / Clear-Parent windows (Ctrl+P / Alt+P)
- **Ctrl+P → Set Parent** dialog: choose the target socket/parent node, and
  whether to unlink from previous parent(s) or keep them. All selected nodes
  get parented to the **active (last-selected)** node. (Ref: Blender "Set
  Parent To" menu.)
- **Alt+P → Clear Parent** dialog: Clear Parent / Clear and Keep Transform /
  Clear Parent Inverse. (Ref: Blender "Clear Parent" menu.)
- Integrates with #5 (multi-select + active node).

## Manager / UI

### 7. Material & texture browser with search
- In the Material / Texture Manager, a "Browse…" button per row that opens a
  resource tree (like the existing node/material selector) with a name search,
  to pick a texture/material path instead of typing it.

---
_Completed items live in the git history on `feature/timeline`._

## Proportional editing (Blender O key)
Falloff-weighted vertex transforms in edit mode: moving/rotating/scaling picked
elements also affects nearby unselected vertices, weighted by distance inside
an adjustable falloff radius (mouse wheel resizes during the modal). Falloff
curves: smooth / sphere / linear / constant. Needs: falloff radius state +
wheel handling inside element modals, weight computation in
gizmoUpdateElement(), circle indicator drawn around the pivot.

## Feedback batch (2026-07-06) - implemented, in-app verify pending
All 6 items done (transform-gizmo depth scaling, origin dot selection colours,
edge gradient in lines.geom/drawline.glsl, free-camera keyboard grab +
hover-entry, Ctrl-pick / Shift-path-select, operator panel axis+orientation
combos with hide-on-reselect). Notes for verification:
- Free camera now enters on Shift+F whenever the pointer hovers the viewport
  (no click needed) and grabs the keyboard while flying.
- Edit mode: Ctrl+LMB = extend/toggle element, Shift+LMB = shortest-path
  select from the active element (BFS; falls back to plain extend across
  shapes/element types or disconnected geometry).
- Operator panel: Axis combo shows for Rotate, Orientation for Move/Rotate;
  a stale gesture greys the panel instead of hiding it; panel hides when a
  different object becomes active.
