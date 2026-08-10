# What the Wild Wasteland Edition adds

Everything below is new relative to the fork point:
**fo76utils/nifskope `develop` @ `f2587869`**, preserved in this repository as
the single `upstream-base` commit. Upstream's own features — the archive
browser, resource extraction, glTF, the Qt 6 / core-profile port — are not
listed here; they are inherited and still present.

|  |  |
|---|---|
| Commits since the fork point | **383** |
| `src/` | **145 files changed, +112,219 / −7,902 lines** |
| New source files under `src/` | **57** |
| Spells (menu operations) | **158 → 204** (+46) |
| Rebindable shortcut actions | **53** |
| Headless regression harnesses | **61** |

Everything here was built and tested against Fallout 4 files. Where a number is
quoted below, it came out of a harness or a corpus run — the dated entry in
[WW_CHANGES.md](WW_CHANGES.md) has the measurement.

---

## 1. Workspaces

Stock NifSkope has one layout. This has **ten**, on one button, each with a
purpose-built dock: Default, Animation, Materials, Collision, Rigging, Vertex
Paint, UV Editing, Pose, Skeleton, Issue Manager. Switching one hides the rest
and remembers the choice.

## 2. Collision — read, edit, and write back

The largest single body of work, and the thing that did not exist anywhere
before.

Fallout 4 ships collision as a **compiled Havok `hknp` packfile** inside
`bhkPhysicsSystem`'s Binary Data. NifSkope could show you the blob. This
edition:

- **Decodes** the packfile into editable blocks — shapes, bodies, mass
  properties, ragdoll joint graphs, joint frames, constraint atom chains,
  `hkaSkeleton`.
- **Encodes** it back. This is the part that makes it an editor rather than a
  viewer. Verified by rebuilding vanilla assets byte for byte:

  | | rebuilt byte-exact |
  |---|---|
  | Ragdoll packfiles | **37 / 37** |
  | Physics systems | **266 / 266** |
  | Shapes | **470 / 470** |
  | Convex polytopes | **68 / 68** |
  | Spheres | **30 / 30** |
  | Capsules | all in corpus |
  | `hkaSkeleton`, from scratch | **37 / 37** |

- **Collision Manager** workspace: per-shape tree, material naming, layer,
  motion quality, solver, friction and restitution read from the file's own
  bytes; click a shape in the viewport to select its row and back again.
- Named collision-body presets and custom material aliases are reusable Library
  files under `<NifSkope Library>/Collision`, not registry-only payloads.
- **Ragdoll simulation** in the viewport — bodies fall, land and settle
  according to the constraints the file actually declares. Grab and drag a bone;
  the chain goes taut instead of tearing open. Adaptive solver for stiff hinges.
- **Physics Sim** viewport mode with six tools, a visible floor, and a
  projectile you can fire at the model.
- New spells: Create Collision, Create Accurate Mesh Collision, Compile
  Collision, Check Collision, Decode Compiled Collision (single and all), Fix
  Collision Layer, Remove Broken Collision, Import OBJ Collision.
- Per-bone collision attribution recovered, with a Bone column and two-way sync.

Along the way this found and fixed real decode bugs — a quarter of all collision
meshes were decoding wrong, and a sticky flag was hiding shapes entirely.

## 3. Rigging and skinning

A donor → receiver bone and weight transfer pipeline that did not exist:

- **Rigging Manager** workspace: receiver and donor bone lists in one tree with
  filtering, multi-select, inline rename, contextual add/remove/transfer.
- Atomic transfer with preflight, rollback and Undo.
- Deep **Fallout 4 skin validation** with findings you can act on.
- Byte-exact **`CustomizationRemapData`** generation.
- **Create Skin** — binds an unskinned Fallout 4 mesh to a skeleton. (The first
  implementation corrupted meshes; an automated gauntlet caught it, and it was
  rewritten as build-blocks → serialize → byte-patch → reload.)
- Weight painting with brush settings, selection masking, continuous strokes,
  accumulated weight, and X-mirroring.
- **Segments and subsegments**: create, delete, reassign, edit IDs, visualise on
  faces, and edit membership by painting.
- Rigging-aware object ops — **Join (Ctrl+J)** unions BSSkin bones and weights,
  remaps per-vertex bone indices, promotes to the superset vertex format, and
  merges donor segments into matching **dismemberment slots**; **Separate (P)**
  clones its own skin, rebuilds segment ranges by prefix sum and compacts orphan
  vertices.
- 12 rigging spells, addressable from the CLI as well as the menus.

## 4. Blender-shaped editing in the viewport

Object mode and edit mode, vertex/edge/face selection, and the operator set:

**Extrude · Bevel · Knife · Loop Cut · Edge Slide · Inset · Subdivide ·
Dissolve · Bridge Edge Loops · Fill · Merge · Smooth · Symmetrize · Flip and
Recalculate Normals · Triangulate · Tris to Quads · Add Primitive · Delete (X)**

with the interaction model that goes around them:

- **Modal transforms** that own the mouse (unbounded drag), with G/R/S, axis
  constraints, typed numeric entry and snapping.
- **Operator redo panels** — adjust the last operation after the fact.
- The **W Specials** quick menu, in edit and object mode.
- A **quad layer** over triangle data, so quad-only operators (Loop Cut) work on
  Fallout 4 meshes and stay quads.
- Blender numpad views, focus-follows-mouse, box select (additive), selection
  memory, cursor-anchored confirmation popups.
- Ctrl+V / Ctrl+E / Ctrl+F open the Vertex / Edge / Face menus at the pointer.
- Vertex picking that does not pick through opaque geometry.
- 65,535-vertex cap enforced on Duplicate and Join, with an offer to duplicate
  into a new shape at the limit (a user-reported corruption).

## 5. UV editing

A full UV editing workspace (`src/uvtools.cpp`, ~5,800 lines) that had no
predecessor:

- Canvas with the Blender look — tiles, progressive grid fade, 0–1 outline,
  island fills.
- **Unwrap** (angle-based LSCM) and **Project From View**.
- Operators: merge, mirror, align, hide faces, projections, bounds, pin.
- **UV Sync Selection** two-way with the 3D viewport, sticky selection so merged
  seams move as one point, multi-mesh simultaneous editing.
- UV set selector, Repeat/Pixels grid toggles, pixel snap, 2D cursor.
- Its own operator redo panels, matching the 3D viewport's.

## 6. Animation

- **Timeline dock**: one lane per controlled block, keyframe editing, a value
  graph, sequence selector, and transport — all in one place, replacing the old
  animation toolbar.
- Sequence bindings resolved through the file's **object palette**, so a
  sequence points at what it really controls.
- Rotation key insert, sort controlled blocks, scale sequence, duplicate
  sequence, remove from animation, bake B-spline.
- **Controller flag bits** implemented as the engine reads them (decoded from
  the 1.10.155 debug symbols, not guessed).
- Four missing **particle modifiers** added; particle and VFX preview.
- Procedural **lightning** rebuilt on the engine's own rules — bolts animate
  again, and read the controller they were given.
- **Freeze Animation** — collapse a sequence to a still pose.
- `anim-setup`, the CLI's animation rigging command.

## 7. Pose and skeleton

- **Pose Manager**: pose a skeleton bone by bone with hierarchy and cumulative
  transforms, blending, weights, pinning, proportional editing, mirror axis,
  multi-select, non-destructive mode.
- A **pose library** that is just a folder of files, configurable in Settings.
- **Outfit Studio pose XML** import (its rotation is a vector, not Euler — read
  out of the Outfit Studio source, not assumed).
- **Screen Archer Menu pose JSON** import (absolute transforms, not deltas, and
  its yaw/pitch/roll turn about X/Y/Z in that order — measured over 80 real
  poses against the power-armour skeleton, then confirmed in SAM's own source).
  Import only; NifSkope does not write SAM poses.
- **Skeleton Manager** with Blender-style armature rendering.
- Load a skeleton **from a game archive**, reusing the NIF Browser.
- Mark one loaded NIF as the skeleton and have the rest snap to it.

## 8. Multi-file work

- **Loaded NIFs** pane beside Available NIFs, with explicit enrolment into a
  combined workspace.
- Two-way NIF Browser drag: available file → Loaded NIFs; loaded live model →
  Save As, with the row retained. Available `.nif` files can be starred and
  filtered; favorites live under the configurable NifSkope Library.
- Explorer `.nif` drops work over the native viewport and every editor panel.
  One menu can adaptively replace only the disposable starter (then enroll any
  remaining files), preserve a real primary by adding to Loaded NIFs, open
  independent windows, or cancel without changing the workspace.
- A **data-only background document layer**: enrolled files are parsed
  `NifModel`s with no window, dock or GL context. Promotion to primary is lazy.
- Workspace-root isolation and Save/Discard/Cancel protection for generated or
  edited in-memory rows, including Reload, Remove, Revert and workspace close.
- **Merge Into** — merge NIFs into one poseable file, honouring `AttachT` so
  ArtObjects land on the right limb. The skull marker is optional for ordinary
  clothing/prop merges; when its model is selected it is hierarchy-validated
  and becomes the rig target automatically. Verified to carry everything.
- **Weapon parts** — a third row mark beside the skeleton skull and the face
  donor, and the only one that is a set: mark the base weapon NIF and every part
  you want on it, and Merge Into **assembles the gun** in row order and lands it
  in a posed hand. Placement is read out of the meshes themselves — a part
  declares the connect point it plugs into (`C-Muzzle`), a placed part publishes
  the matching `P-Muzzle` with the node it rides and a full transform, rotation
  included — so chains resolve: a receiver places a barrel, and that barrel's own
  muzzle point places the suppressor. A muzzle flash — which declares no connect
  points at all — goes at the very end of that chain, on the farthest-forward
  point the assembly actually publishes. Any combination is allowed, cross-weapon
  included; nothing is ever refused. The summary reports what it noticed — a part
  whose shape names the target already carries, or one asking for a point nothing
  in the assembly publishes — and a target with no `WEAPON` bone is a root merge,
  said out loud rather than left to the picture.
- Combined preview and donor selection across the whole workspace group.

## 9. Materials and rendering

- **Materials workspace**: BGSM/BGEM tree, texture preview, and textures labelled
  by the *material's* own slot order — the material file is the source of truth.
- Material spells: edit, check (one and all), compare, copy/paste setup, sync
  shader ↔ material, select material users, fill shader texture set.
- Unified, compact **Shader Flags editor**, and a generic flag dialog so Node and
  BSX get the same UI. Flags copy and paste.
- **Vertex Colours** viewport shading mode; full colour studio.
- `.pbrm` material reading and PBR rendering path (currently parked behind a
  greyed-out toggle — see [docs/TO_BE_IMPLEMENTED.md](docs/TO_BE_IMPLEMENTED.md)).
- Fixed: **refraction was dead for every BGSM-backed Fallout 4 mesh**.
- Refraction uses the authored normal map as a resolution-independent local
  distortion, and Loaded NIFs share the opaque source it refracts; animated VFX
  no longer expand into a dark viewport-sized silhouette.
- Fixed: geometry rendering dark after an edit-mode transform.
- Fixed: selected geometry, edges and vertices showing through solid geometry
  with x-ray off.
- Fixed: the startup grid/axes bug — raw `glUseProgram(0)` after QPainter
  overlays was desynchronising the renderer's program cache.

## 10. Block List and Block Details

- Block List: search and filter chips, category icons and a per-type dot,
  inline Blender-style renaming, multi-selection, foldable groups, a block
  **summary** row that says what a block *is*, and a rebuilt context menu with a
  real taxonomy (`Spell::group()`).
- Block Details: link jump and pick, decoded flags in place, paste a field to
  many rows, **pinned fields**, sticky expansion state, and **diff against a
  reference file** in its own column.
- Version-mismatched rows are actually hidden, derived lazily — which is also
  what fixed slow click-select on high-poly blocks.
- A broken texture path now looks broken.

## 11. Issue Manager

Scans the open file and reports **real** defects, grouped by class, each with a
one-click fix: absolute texture paths, broken collision, zero-area triangles,
missing controllers, name-authority mismatches, and more. Fix buttons run the
corresponding spell, so there is one implementation per fix, not two.

Destructive spells declare themselves (`Spell::destructive()`), and the
confirmation prompt asks in the spell's own words instead of a generic warning.

## 12. Headless CLI

`NifSkope -no-gui` — the program without a window ([docs/CLI.md](docs/CLI.md)):

```
spells · info · list · world · dump · get · set · cast
skeleton [--validate] · merge · pose · anim-setup
```

`world` prints each object's **world** transform, so two files can be diffed by
name — which is how "the merge put the effects exactly where the skeleton had
them" gets checked rather than asserted. `dump` applies the same version and
condition row-hiding the GUI does, so a healthy mesh does not read as corrupt.

## 13. Interface

- A **skin** (`src/wwskin.h`) — one palette, one place. Colours come from named
  tokens, never literals.
- One **number field** (`src/ui/widgets/wwnumberfield.{h,cpp}`) replacing five
  divergent copies of the same drag-to-scrub gesture. Applied to ~370 fields;
  ~60 are excluded by design, each with its reason at the call site. Blender
  parity: hover arrows, drag to scrub, Shift for precision, Ctrl to snap, Esc
  and RMB to cancel, typed expressions (`1024/3`, `sqrt(2)`, `rad(90)`), and one
  drag setting a whole X/Y/Z row.
- **Rebindable shortcuts** (Settings ▸ Shortcuts, with search), 53 actions.
- Swappable select / place-gizmo mouse buttons.
- A **command palette** for spells, searchable by name.
- The viewport header rebuilt on Blender's: mode selector, shading, overlays,
  and LOD / Animation / Collision as dropdown buttons that stay visible and grey
  out when they do not apply.
- Starter scene — opens on a cube on the grid, like Blender, instead of nothing.
- Performance: three tiers of work; secondary refresh went **54 ms → 7 ms per
  edit**.

## 14. Testing

60 environment-gated harnesses (`WW_*_TEST`) drive the real binary and assert on
numeric state, with shell wrappers under `tests/`. Plus byte-level verifiers
under `tools/` for the collision round-trip, join, copy/paste, create-skin and
vertex-flags paths, and a render-regression baseline.

This is why several entries above can quote "470 of 470" rather than "works on
my machine".

---

## Not in this edition

Stated plainly so nobody goes looking:

- **PBR rendering** is implemented but parked behind a disabled toggle.
- Game-accurate lighting and subsurface scattering are future work.
- The classic NiSkin backend, independent persistent skeleton references and
  zero-weight bone pruning are not done.
- Four mapped-but-unbuilt features are written up in
  [docs/FOUR_FEATURES_PLAN.md](docs/FOUR_FEATURES_PLAN.md).
- The full backlog is [docs/TO_BE_IMPLEMENTED.md](docs/TO_BE_IMPLEMENTED.md).

Formats for games other than Fallout 4 are inherited from upstream and were not
removed, but no work here targets them and nothing here is tested against them.
