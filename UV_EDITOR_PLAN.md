# UV Editing Workspace — Implementation Plan

Drafted: 2026-07-14. Status as of 2026-07-15: **Phases 1–4 shipped and
user-validated in-app** (see WW_CHANGES.md for the full per-batch record).
Beyond the plan below, also shipped: adjust-last-operation redo panels
(scrubbable values, G/R/S + Merge/Relax/Pack/Smart-Project/Unwrap), sticky
selection (Shared Location) with island welding of merged seams, 0-1 tile
outline, hover-out operator menus matching the 3D viewport, and the 3D ortho
grid decade crossfade.
**Full simultaneous multi-mesh editing shipped 2026-07-15** (the last plan
item — awaiting user validation): cross-shape picking/box/select-all/invert,
combined transforms with per-shape undo commands in one macro, per-shape
selection rendering and 3D sync. Operators (merge/unwrap/pack/…) act on the
active mesh by design — click a mesh's UVs to make it active. Proportional
editing: declined by user, do not implement.

Goal: replace the "UV Manager (Planned)" placeholder with a Blender-style UV
editing workspace: a dockable 2D UV editor working on the mesh being edited in
the 3D viewport, with Blender's selection model, modal G/R/S transforms,
Blender keymap, and two-way selection sync.

## 0. The one structural difference from Blender (read first)

Blender stores UVs **per face corner** (per loop); a mesh vertex can have a
different UV in every face that uses it. NIF `BSTriShape` (and legacy
`NiTriShapeData`) store UVs **per vertex** — one UV per vertex, and UV seams
exist only because the exporter duplicated vertices along seams.

Consequences, stated up front:

- Every UV point in our editor corresponds 1:1 to a mesh vertex. Editing is
  Blender-with-everything-welded-per-vertex; for game meshes (which are
  already seam-split) this looks and feels identical to Blender in practice.
- UV islands fall out naturally: connected components of the triangle graph
  over shared vertex indices.
- "Merge/weld UVs" just sets UVs equal — vertices stay split. Same as Blender.
- Operations that *create* new seams (marking a seam through connected
  geometry, unwrap along new seams) require **vertex duplication** — a real
  topology edit. We already own that machinery (delete/separate/join rebuild
  packed vertex arrays and resync skinning), so it is possible, but it is
  deliberately pushed to the last phase.

## 1. Architecture

**New workspace, not a retrofit of the legacy pop-up.** The existing
`UVWidget` (src/ui/widgets/uvedit.{h,cpp}, spell-launched modal editor) stays
untouched as a fallback. The new editor borrows its proven pieces — texture
slot discovery, `TextureInfo` binding via `TexCache`/`NifSkopeOpenGLContext`,
BSTriShape/legacy UV read paths — but is a new implementation because the
legacy widget's private QUndoStack, modal lifetime, and single-mesh model are
all wrong for a workspace.

New files:

- `src/ui/widgets/uveditorview.{h,cpp}` — `UVEditorView : QOpenGLWidget`, the
  2D canvas: texture underlay, grid, UV wireframe/points/fills, selection
  visuals, modal transform handling, box/circle select overlays.
- `src/uvtools.cpp` — `tlCreateUVManagerDock( NifModel*, QMainWindow*,
  GLView* )` (same factory convention as the Rigging/Vertex Paint managers),
  the operator implementations, and selection-sync glue.

Integration points (all existing patterns):

- `nifskope_ui.cpp` workspace menu: "UV Editing" replaces the disabled
  placeholder; entering it auto-enters Edit Mode on the active shape (same
  auto-flow as the Weight Paint entry). **The workspace docks on the right**
  like the other manager workspaces, with the 2D UV canvas as the dock's main
  body (tool controls above/below it) — resizable against the 3D viewport,
  and floatable/re-dockable like any manager dock.
- Selection sync against `GLView::pickedElems` / `editShapeBlocks` (per-vertex
  picks map 1:1 to UV points; face picks map to triangles).
- Undo: **the model undo stack**, via the same merged
  transaction pattern the 3D element gizmo uses — write-through during a drag,
  one named undo transaction on commit. No private undo stack. UV writes are
  `nif->set<HalfVector2>( vertexRow, "UV", … )` for BSTriShape and
  `setArray<Vector2>` for legacy data, exactly as the legacy editor and
  gizmoReapplyElement already do; the viewport picks the change up through
  the normal dataChanged → shape re-upload path, so the 3D texture preview
  updates live during drags.

## 2. Feature phases

### Phase 1 — Core editor (the milestone that must feel like Blender)

View:
- Texture underlay of the active mesh's diffuse slot (slot picker menu like
  the legacy editor; checker pattern fallback; repeat display outside 0–1
  with the 0–1 tile emphasized), pan (MMB drag), zoom (wheel, cursor-centred),
  Home = frame all, `.` = frame selected, UV grid with subdivision by zoom.
- "Browse custom image..." underlay option (any texture via TexCache, not just
  the mesh's own material) — for re-targeting UVs onto a different atlas.
- UV wireframe over the whole edit mesh, selected elements highlighted with
  the standard palette (active #FF9D00, selected #FF7200), faces fill when
  face-selected.

Selection (Blender keymap):
- Modes: vertex / edge / face / island (`1`/`2`/`3`/`4` while the editor has
  focus).
- Click select, Shift-click extend/toggle, `A` all, `Alt+A` none, `Ctrl+I`
  invert, `B` box select (additive, Shift/Ctrl-drag deselects — same
  convention as our 3D box select), `L` under-cursor linked, `Ctrl+L` select
  linked from selection.
- **Two-way sync with the 3D viewport** (Blender's "UV sync" permanently on,
  which per-vertex UVs make the natural model): selecting verts/faces in the
  UV editor drives `pickedElems` (highlight in 3D), and 3D edit-mode picks
  update the UV editor. Guarded both ways against echo, same pattern as the
  block-list/viewport selection mirror.

Transforms (Blender modal semantics):
- `G` / `R` / `S` with mouse, `X`/`Y` axis constraint, typed numeric input,
  Shift = precision, Ctrl = snap (grid increment; vertex-snap target option),
  Esc/RMB cancel, LMB/Enter commit. Implemented as a 2D modal state machine in
  `UVEditorView` (the 3D gizmo's proven flow, minus the 3D math).
- Pivot selector: bounding-box center / median / 2D cursor.
- **2D cursor with full parity to our 3D-viewport cursor toolkit**:
  - Place with Shift+RMB (same binding as the 3D cursor); numeric U/V readout
    + entry fields in the dock for exact placement.
  - Cursor utils (context menu + Shift+S snap menu): Cursor→Selected,
    Cursor→Active, Cursor→Grid, Cursor→Origin (0,0) and →Tile Center
    (0.5,0.5); Selected→Cursor and Selected→Cursor (Keep Offset — island
    translation without collapsing).
  - Cursor as transform pivot (pivot selector above) and as the merge target
    (`M` → At Cursor) and projection anchor where sensible.
  - Cursor position persists per session and is visible at every zoom
    (crosshair marker like the 3D one).

Data + undo:
- Active edit mesh (all `editShapeBlocks` rendered; the active shape is
  editable in phase 1, the rest drawn dimmed).
- One merged model-undo transaction per gesture; Ctrl+Z in either view undoes
  the same history.

**Build + user GUI gate here before phase 2.**

### Phase 2 — Editing operators and multi-mesh

- Edge and island select fully wired into transforms; island mode drags whole
  islands (Blender island semantics).
- `M` merge menu: at center / at cursor / by distance (UV-space weld of split
  verts; topology untouched).
- `Ctrl+M` mirror X/Y; align menu (align U, align V, straighten, align
  rotation — rotate island so the active edge lies axis-aligned).
- `Shift+S` snap menu grows the pixel entries (selected→pixel corner/center);
  the cursor entries ship in phase 1 with the cursor itself.
- Constrain to Image Bounds toggle; Round to Pixels (corner/center) snapping
  modes.
- `H` / `Alt+H` show/hide faces, mirroring the 3D edit-mode `editHiddenTris`
  machinery (both views honour the same hidden set).
- All meshes in `editShapeBlocks` editable simultaneously (per-shape tint for
  non-active, like Blender multi-object edit).
- Projection operators (operate on the 3D-selected faces, results replace
  their UVs): Project From View, Reset, Cube / Cylinder / Sphere projection.
- Proportional editing (`O`, wheel radius) — cheap in 2D, high value for UV
  touch-ups.

### Phase 3 — Layout tools

- Stitch (`V`): match split-vertex pairs along island borders (same 3D
  position, different vertex index), best-fit rigid transform of the mobile
  island, then weld. This is the operator that makes NIF seam repair pleasant.
- Pack Islands: shelf packing with optional rotation, configurable margin.
- Average Islands Scale (equalize texel density).
- Smart UV Project: angle-threshold chart segmentation + per-chart planar
  projection + pack. (No new vertex splits needed: charts follow existing
  triangle connectivity.)
- Minimize Stretch / Relax: iterative relaxation of interior UVs with the
  island boundary held fixed — the practical light-weight unwrap that covers
  most cleanup before LSCM exists.
- Select Overlap: flag mutually overlapping UV faces (bake/lightmap lint for
  game meshes).
- Stretch overlay (angle/area distortion heatmap), pixel-snap toggle,
  copy/paste UVs between identical-topology shapes.

### Phase 4 — Deferred, needs explicit go-ahead later

- Seam marking in the 3D viewport (`Ctrl+E`, session edge attribute — NIF has
  no seam storage) + **vertex splitting along seams**, reusing the packed
  vertex-array rebuild + `tlSkinResync` machinery. Includes the freebies once
  seams exist: Seams from Islands, and UV **Split** (`Alt+M`) / **Rip** (`V`),
  which in NIF terms are vertex duplication, not per-corner un-welding.
- Real unwrap (LSCM least-squares conformal solve, conjugate-gradient, no new
  dependencies) with pinning (`P`/`Alt+P`, Invert Pins) and
  live-unwrap-on-pin-drag.
- Export UV layout to PNG.
- Optional extra, approximate by design: Follow Active Quads / grid rectify.
  NIF stores only triangles, so this needs quad reconstruction from coplanar
  tri pairs first — results can't fully match Blender's quad-native behavior.

## 3. Risks / notes

- **QOpenGLWidget in a dock** alongside the native QOpenGLWindow viewport:
  the legacy UVWidget already proves TexCache + a second GL context coexist;
  the airspace problem that forced floating tool panels applies to overlays
  *on the GL window*, not to a separate dock, so the dock is safe.
- **Performance**: GL-rendered like the legacy editor (QPainter would choke on
  65k-vert FO4 meshes). Selection hit-testing via uniform screen-space grid
  bucketing per zoom level if naive scans stall on dense meshes.
- **Half-float quantization**: BSTriShape UVs are float16; sub-quantum drags
  must accumulate in float32 editor-side and write the quantized value, or
  slow drags stall at zero (the weight-paint brush already handles the
  analogous half-float issue — same approach).
- **Skinned partition meshes** (`isDataOnSkin`, Skyrim SSE): legacy editor
  handles their split vertex storage; port that read/write path as-is.
- Keymap collisions: every single-letter shortcut is scoped to the UV editor
  widget having focus (the .ui QAction collision lesson from v16 — no global
  shortcuts).

## 4. Verification protocol

Per the working agreement: I build and reason on paper; the user drives the
GUI. Each phase gate = release build green + a short scripted GUI checklist
delivered with the phase (e.g. phase 1: open X01 torso, enter UV Editing,
verify underlay/slots, box-select an ear of UVs, G/X drag with snap, Ctrl+Z in
3D viewport undoes it, 3D selection mirrors).
