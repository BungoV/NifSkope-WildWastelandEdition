# NifSkope – WW Edition — To Be Implemented

Backlog of features/fixes agreed for later. Ordered roughly by priority.

## Mesh / engine features (need in-game or careful testing)

### 1. BSPositionData mesh-emitter spawn distribution
- **Symptom:** `Generate BSPositionData` on a mesh used by a `NiPSysMeshEmitter`
  (e.g. `X01_Torso_Tesla_Pulse:0` in `X01_Torso_Tesla_VFX.nif`) makes the game
  spawn particles in a single spot instead of across all faces.
- **Notes:** the generated layout (positions + normals + numTris*3 + 2 zeros)
  matches vanilla `edison_pa_vfx.nif` for the lightning mesh, so the format is
  probably right; the clustering may be an emitter setting (emit-from
  vertices/edges/faces) or a different data expectation. Needs a byte-level
  diff of a known-good mesh-emitter file + in-game iteration.

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

### 5. Multi-node selection in object mode
- Shift+Left-click to select multiple nodes. Block Details shows only the
  **last (active)** selected node. Highlight the active node one colour and the
  other selected nodes another, Blender-style, in the Block List.
- **Viewport outline:** selected geometry gets an outline in the 3D view; the
  outline colour depends on whether it is the **active (last-selected)** node
  or a **previously-selected** node. Use Blender's colours (active = brighter
  white/orange, selected = darker orange).
- Wireframe overlay is **disabled by default** on selected nodes in object
  mode (the outline replaces it); the Wire toggle can still turn it on.
- This underpins #6 (multi-node parent needs the selection set + active node).

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
