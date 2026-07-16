# NifSkope – WW Edition — To Be Implemented

Backlog of features/fixes agreed for later. Ordered roughly by priority.

## Modeling tools — geometry creation & connection (2026-07-15) — CONCEPT
Full plan in `MODELING_TOOLS_PLAN.md`: generalized operator redo panel
(floats + checkboxes + enums), shared topology kernel (append/interpolate
verts incl. skin weights, boundary loops, edge rings), then phased operators —
Extrude (E), Fill (F), Bridge Edge Loops, Connect (J), Subdivide, Loop Cut
(Ctrl+R), Inset (I), Slide, Bevel (Ctrl+B), Smooth, Dissolve, Symmetrize,
Flip/Recalc Normals, Add Primitive (Shift+A). BSTriShape-only, 65,535-vert
cap guard, snapshot undo per op. Proportional editing stays declined.

## Particle / VFX preview (2026-07-06) - DONE, user-verified 2026-07-07
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
- Particle effect-shader shading (2026-07-07): particles.frag now applies the
  BSEffectShaderProperty (.bgem) features flat sprites previously missed —
  normal-map view-angle falloff (useFalloff/hasRGBFalloff), greyscale-to-palette
  colour+alpha gradients (GreyscaleMap), and env cube-map reflection. Emissive
  tint (glowColor/glowMult) moved from the CPU pre-multiply into the shader.
  Billboard tangent frame is identity in view space, so the sampled normal is
  the view-space normal directly. glparticles.cpp binds NormalMap→u1,
  GreyscaleMap→u2, CubeMap→u3 (neutral gray cube always bound so the
  samplerCube never shares a unit with a sampler2D). In-app verify pending.
  Still unhandled: NiPSysColorModifier (NiColorData) lifetime colour gradient.

## Mesh / engine features (need in-game or careful testing)

### 0. Delete Vertices/Edges/Faces (edit mode) - DONE 2026-07-07, verify pending
- Blender-style X / Delete menu (Vertices / Edges / Faces / Only Faces) in edit
  mode. GLView::showDeleteMenu + deleteGeometry(mode); tlDeleteGeometry rewrites
  the BSTriShape: removes triangles per mode, compacts the packed vertex array
  (forward-copy: new index <= old, so no clobber) and reindexes, updates
  Num Vertices/Num Triangles/Data Size + bounds. Faces mode removes orphaned
  verts (Blender); Only Faces keeps them. Legacy NiTriShapeData drops triangles
  only. Snapshot-undoable.
- **Skin data (DONE 2026-07-07):** tlSkinResync (mirrors NifSkope's own
  spRemoveWasteVertices) now runs after each delete — it remaps NiSkinData bone
  vertex-weight indices through the vertex remap, and drops a now-stale
  NiSkinPartition so the user can regenerate it (Make Skin Partition). FO4
  BSSkin::Instance has neither (inline skin, already compacted), so it no-ops
  there. Partition blocks are removed after all shape edits, highest block first.
  Deeper future option (not needed for FO4): remap a partition in place instead
  of dropping it.

### 0b. Box select (B) + Invert selection (Ctrl+I) - DONE 2026-07-07, verify pending
- **Box select (B):** GLView::beginBoxSelect (routed via NifSkope::eventFilter,
  pointer-over-viewport, edit+object) arms a rubber-band; mousePress/Move/Release
  drive boxSelectDrag; paintGL draws the dashed rectangle. applyBoxSelect: object
  mode picks shapes whose origin projects into the box (no primary set — keeps the
  remembered boxSelectPrevActive if it survives, else -1, so you click to set a
  primary, Blender-style); edit mode selects verts/edges/faces per pickMode.
  X-ray ON = everything in the box; X-ray OFF = front-facing only (geometric
  triangle normal vs view dir — an approximation of true occlusion; a depth-buffer
  test would be exact, future). Shift adds, Ctrl removes, else replaces.
- **Invert (Ctrl+I):** GLView::invertSelection — object mode inverts membership
  over all visible shapes; edit mode inverts verts/edges/faces per enabled pickMode
  over editShapeBlocks. Routed via eventFilter too.
- B and Ctrl+I had no prior binds (plain I = scale key mapping, unaffected).

### 1. BSPositionData mesh-emitter spawn distribution - DONE, user-verified 2026-07-07
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

### 3. Detach / separate selected geometry (edit mode) - DONE 2026-07-07 (Selection only)
_Separate > Selection, Join (Ctrl+J) and Duplicate (Shift+D, obj+edit) shipped.
Remaining: Separate By Material / By Loose Parts (greyed in the P menu);
object-mode duplicate/separate are BSTriShape-only (NiNode branch duplicate is
future). Edit-mode selection undo (Ctrl+Z through selection history) still TODO._

### 3z. (superseded) old Separate notes
- **P key in edit mode** opens a Blender-style "Separate" menu. For now
  implement **Separate > Selection** only (the other Blender entries — By
  Material, By Loose Parts — can be greyed/omitted for later).
- **Separate > Selection:** the selected verts/edges/faces move into a NEW
  BSTriShape (its own block, reparented under a NiNode), leaving the rest in
  the original mesh. Copy the full packed vertex data for the moved verts
  (position/**normal**/tangent/bitangent/UV/color/weights) + the moved
  triangles reindexed; wire the same shader/alpha property (or a copy).
- **Preserve normals exactly** on the separated geometry (do NOT recompute
  them). The seam verts keep their original authored normals, so if the user
  later joins the pieces back the shading across the seam stays seamless.
- Snapshot-undoable. BSTriShape-rewrite risk — test carefully.

### 3b. Join geometry (object mode) - Ctrl+J
- **Ctrl+J in object mode** joins the selected compatible geometry nodes into
  the active (last-selected) node, Blender-style. "Compatible" = same block
  type + matching vertex format (BSVertexDesc) + same shader/alpha setup so
  the merged vertex buffer is valid.
- Append each source mesh's vertex data (transformed into the active node's
  local space) and triangles (reindexed by the running vertex offset) onto the
  active BSTriShape; keep normals/tangents as-is so a prior Separate round-trips
  seamlessly. Remove the now-empty source blocks. Snapshot-undoable.
- Inverse of #3; shares the BSTriShape vertex-array read/write helpers, so do
  the two together.

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

### 6. Parent / Clear-Parent windows (Ctrl+P / Alt+P) — DONE (2026-07-11)
- **Ctrl+P → Set Parent** supports Object (Keep Transform), Object (Keep
  Local Transform), and Link to Additional Parent. With multiple selections,
  the active (last-selected) `NiNode` is the parent; a target-node picker is
  shown when no selected parent is available.
- Parent targets may be any `NiNode` subclass. Children may be any
  `NiAVObject` subclass, including `NiNode`, `BSTriShape`,
  `BSSubIndexTriShape`, and other compatible scene-object blocks. Pure mesh
  data, property, controller, and extra-data blocks are intentionally excluded
  because the NIF schema does not permit them in a `NiNode` Children array.
- **Alt+P → Clear Parent** supports Clear Parent and Clear and Keep Transform.
  Clear Parent Inverse is displayed disabled because NIF scene nodes do not
  have Blender's separate parent-inverse matrix.
- Both operations are snapshot-undoable, prevent hierarchy cycles, update all
  relevant `NiNode` Children/Effects links, and preserve world transforms when
  requested. The commands are also available from the object-mode viewport
  context menu.

## Manager / UI

### 9. Block List & Block Details overhaul (design-only, 2026-07-12)
- Blender-familiar redesign of the block list (search + quick filters, type
  icon/colour coding, per-type summary column, status badges, referenced-by
  peek) and block details (field filter + collapsible sections, named flag
  editor, jumpable/validated link fields, typed value editors). Full notes and
  phasing in **`BLOCK_LIST_DETAILS_OVERHAUL.md`**. Direction locked as
  Blender-familiar; dock-vs-in-place still open. Not scheduled yet.

### 8. Blender-style workspaces - DONE (2026-07-12)
- A workspace switcher (tabs/dropdown near the top) with three default
  workspaces, each a saved dock/panel layout:
  - **Default** — clean layout; the bottom dock area is empty (no timeline,
    no Material/Texture editor shown by default).
  - **Materials / Textures** — the Material / Texture editor takes the bottom
    slot (where the timeline sits in the Animation workspace).
  - **Animation** — the animation timeline in the bottom slot.
- Switching a workspace restores that workspace's saved layout (which docks
  are visible, where, and their sizes). The bottom slot is the shared spot
  the timeline vs. Material/Texture editor swap into.
- Impl notes: layouts are `QMainWindow::saveState()`/`restoreState()` blobs
  keyed per workspace; the timeline (`dTimeline`) and Material/Texture
  Manager (`dMatMgr`) docks already exist, so the workspace mostly toggles
  their visibility + tabifies/positions them at the bottom. Persist the
  active workspace + custom layouts in settings.

### 7. Material & texture browser with search - DONE (user-confirmed 2026-07-07)
- The Material / Texture Manager has a "Browse…" button (operates on the
  selected row) that opens FileBrowserWidget filtered to textures/materials,
  which has its own "Path Filter" search field. Picks a resource path from the
  game archives instead of typing it. (meshtools.cpp tlCreateMatTexManagerDock
  + ui/widgets/filebrowser.cpp)

---
_Completed items live in the git history on `feature/timeline`._

## Display compiled FO4 collision (bhkPhysicsSystem) — INVESTIGATED 2026-07-08, feasible
Goal: render Elric-compiled collision (bhkNPCollisionObject → bhkPhysicsSystem
"Binary Data") in the viewport like legacy bhkShape trees.

**Findings (validated against before/after Elric pairs in
`C:\Users\bungo\Documents\3dsMax\export\{meshes,processed}`):**
- The blob is a standard **Havok 2014.1 binary packfile** (magic
  57E0E057/10C0C010, v11, 64-bit pointers, little-endian): header, 3 sections
  (`__classnames__`, `__types__` (empty), `__data__`), and local/global/
  virtual fixup tables. Virtual fixups map object offsets → class names, so
  objects can be located without the full type system.
- FO4 uses the **hknp** engine. Only two geometry classes appear across box /
  convex / trimesh samples:
  - **hknpConvexPolytopeShape** (box AND convex hull): FULLY DECODED —
    convexRadius float; then hkRelArray descriptors (u16 count + u16 offset
    relative to the descriptor's own address): vertices (hkVector4, w =
    vertex id), planes (hkVector4), faces (u16 firstIndex + u8 numIndices +
    u8 flags), u8 vertex indices. Verified: box verts ±0.13287 == raw
    bhkBoxShape half-extents exactly.
  - **hknpCompressedMeshShapeData** (trimesh): hkcdStaticMeshTree-style —
    quantization domain AABB + float grid steps + per-section packed
    vertices (bit-packed against the domain) + u8[4] primitive indices.
    Located all pieces in the sample; vertex bit-unpacking still to work out
    (known format, community references: hkxpack / ck-cmd).
- Scale: Havok units × 69.99125 = game units (same as legacy bhk).
- Body transforms live in hknpPhysicsSystemData's bodyCinfos (needed for
  multi-body systems, e.g. ragdolls; single static bodies are identity).

**Implementation plan:**
1. Port the packfile walker (header/sections/classnames/virtual fixups) to
   C++ — `tools/hkparse.py` in this repo is the working reference.
2. Decode hknpConvexPolytopeShape → wireframe verts/faces (done on paper).
3. Decode hknpCompressedMeshShapeData vertex packing (compare against the
   raw NiTriStripsData ground truth to validate).
4. Hook into Node::drawHavok: when bhkNPCollisionObject links a
   bhkPhysicsSystem, parse the blob (cache per block), draw like
   drawHvkShape with the ×69.99125 scale.
5. bhkRagdollSystem shares the container — same code path later.

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

## Blender feature batch (agreed 2026-07-08)

### Mirror editing (X-mirror)
Blender's "Mirror Editing" checkbox: edit-mode transforms applied to a vertex
also apply (X-negated) to its mirror partner across the object's X axis.
Build a mirror-pair vertex map on entering edit mode (position match with a
small tolerance); unpaired verts move normally. Toggle lives in the redo /
snap panel area. Great for armor meshes.

### Checker deselect
Deselect every Nth element of the current selection (order = walk along
connectivity from the active element, Blender-ish). Redo panel with
Nth/offset DragSpinBoxes.

### Repeat Last (Shift+R)
Re-run the last operator with its stored parameters (transform, merge,
select-linked, box deselect... the redo-panel state already stores them).

### F9 — reopen redo panel at cursor
F9 pops the current redo panel next to the mouse cursor (it lives at the
viewport's bottom-left otherwise).

### Smooth Vertices
Laplacian smooth on the selected verts (average with edge neighbors),
factor + iterations in a redo panel. Low-risk vertex rewrite; good for
fixing lumpy geometry.

### Rip (V) and Split (Y)
Split (Y): duplicate the selected verts and reassign the selected faces to
the copies, detaching them in place (no new block — unlike Separate).
Rip (V): split along the selected edge path and enter a move modal on the
ripped side. Shares Separate's vertex-array machinery.

### Extrude (E)
Duplicate the selected boundary verts, bridge triangles between original and
copy, then enter a move modal on the copies. Normals/UVs on the new side
walls need care. Medium-high effort.

### UV editing overhaul (HUGE undertaking)
A proper UV editing workspace, roughly in stages:
1. **UV viewer dock**: draw the edited mesh's UV layout (triangles in UV
   space) with the base texture underneath; sync selection with the 3D view.
2. **UV editing**: move/rotate/scale selected UVs with the same modal G/R/S +
   redo-panel machinery; snap/pin support; write back into the packed
   BSTriShape vertex data (HalfVector2 UV) undoably.
3. **Unwrap operators** (stretch goal): mark seams on edges, LSCM-style
   unwrap or at least planar/box projection per selection.
Ties into the Materials/Textures workspace idea (#8). Should get its own
plan document before starting.

## Proportional editing (Blender O key)
Falloff-weighted vertex transforms in edit mode: moving/rotating/scaling picked
elements also affects nearby unselected vertices, weighted by distance inside
an adjustable falloff radius (mouse wheel resizes during the modal). Falloff
curves: smooth / sphere / linear / constant. Needs: falloff radius state +
wheel handling inside element modals, weight computation in
gizmoUpdateElement(), circle indicator drawn around the pivot.

## Animation Manager visual overhaul (deferred 2026-07-11)

Keep the current animation tools working, but reorganize the workspace before
adding more features:

1. Channel hierarchy on the left and a scalable key canvas on the right, with
   Blender-style Summary and per-channel display modes.
2. Compact transport, current-frame and playback-range controls across the
   top; clear playhead and frame labels in the canvas.
3. Filters for Selected, Visible, Transform, Visibility and Material tracks.
4. Distinct key colours for translation, rotation, scale and animation events.
5. Box-select, duplicate, move, scale and snap keys directly in the canvas;
   frame the selected keys on demand.
6. Interpolation/easing controls and a collapsible exact-value key inspector.
7. Markers and named animation events on their own visible lane.
8. Warning badges for broken targets, empty sequences and overlapping keys.

This is intentionally backlog-only until the Material workspace pass is
stable; do not remove or rename the existing Animation workspace meanwhile.

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
