# Wild Wasteland NifSkope — Current Status

Updated: **2026-07-20** — branch `feature/timeline` @ `8323dc9`, build green
(exe 2026-07-19 21:38), all headless harnesses passing.

This is the short handoff document. `WW_CHANGES.md` remains the detailed change
history, `BONE_WEIGHT_TRANSFER_PLAN.md` describes the transfer design,
`UV_EDITOR_PLAN.md` is the authoritative UV plan, and
`repo-tests/tests/rigging/QA_EVIDENCE_2026-07-13.md` contains the native
rigging evidence.

## Working agreement

- The user performs live GUI and game debugging. The agent should change/read
  code, reason about it on paper, and run proportionate builds or non-GUI tests
  only.
- Do **not** launch NifSkope, Blender, or another GUI unless the user explicitly
  changes that instruction.
- Keep subagent use low; do not delegate unless the user explicitly asks.
- Production repository: `E:\Projects\ClaudeNifskope`
- The correct binary is always the one we build here:
  `E:\Projects\ClaudeNifskope\release\NifSkope.exe`
- Release build command:
  `C:\msys64\usr\bin\bash.exe -c 'cd /e/Projects/ClaudeNifskope && PATH=/ucrt64/bin:/usr/bin:$PATH make -f Makefile.Release -j4'`
- After adding data members to widely-included classes or changing the include
  graph, **re-run `qmake6 -o Makefile NifSkope.pro` before trusting an
  incremental build** — a stale dependency list once linked an old-layout `.o`
  and hard-crashed at startup (0xC0000005 in QHash `findNode`).
- Relink can fail with the exe locked/in use; kill stragglers and force the
  relink rather than assuming a code error.

## Latest completed work: 2026-07-17 → 07-19 (details in `WW_CHANGES.md`)

Everything below is built, headlessly verified, and committed through
`8323dc9`; **GUI verification by the user is pending** unless noted.

- **07-17 modeling mega-batch** — Knife (K), quad layer (F / Alt+J / Ctrl+T),
  Bevel (Ctrl+B, rip+offset+bridge), Smooth, plus the remaining modeling
  backlog. Bevel is the highest-risk item: GUI-test it on a copy of a mesh.
- **07-18 Create Skin rewritten** (`4b47668`) — the first version corrupted
  unskinned meshes (caught by the automated gauntlet, not the GUI). Now builds
  skin blocks at block level, serializes, byte-patches desc/DataSize/records,
  and reloads through the loader. Full gauntlet green, including a donor
  transfer run on its own output. Model landmine documented in
  `WW_CHANGES.md 2026-07-18b`: any spell doing `set<BSVertexDesc>` +
  `updateArraySize` on an unchanged vertex count is suspect (stock Vertex
  Flags spell included).
- **07-18** — clone-freeze fix (`loadIndex` populates with signals live; wrap
  in Loading state), 65,535-vertex cap in Duplicate/Join,
  duplicate-into-new-shape offer at the cap, Blender X delete in object
  mode/Block List with cursor-anchored confirm popups.
- **07-19 batch** (`8323dc9`) — Copy/Paste Branch handle multi-selection and
  slot every pasted root; **Join (Ctrl+J) is rigging-aware** (BSSkin union +
  per-vertex bone-index remap, vertex colors, superset-format promotion) and
  merges donor segments **into matching dismemberment slots** (subsegments /
  Per-Segment-Data / SSF preserved); **Separate (P) is skin- and
  segment-aware** (own BSSkin clone, prefix-sum segment rebuild, orphan-vertex
  compaction); paint-mode viewport selector auto-acquires a target; viewport
  mode menus docked as the `tMode` toolbar and the redundant animation bar
  retired (all functions live in the Timeline dock); NIF Browser keeps
  expanded folders across loads.
- Headless harnesses added along the way (env-gated in `nifskope_ui.cpp`):
  `WW_JOIN_TEST`, `WW_SEP_TEST`, `WW_WP_TEST`, `WW_COPYPASTE_TEST`,
  `WW_UI_SHOT`, `WW_BROWSER_TEST`, plus byte-level verifiers under
  `tools/join_test/` and `tools/copypaste_test/`.
- Output: `E:\Projects\ClaudeNifskope\release\NifSkope.exe`

## Current implemented state

- Rigging workspace/manager with donor-to-receiver bone and weight transfer,
  atomic preflight/rollback/Undo, FO4 validation, and byte-exact
  `CustomizationRemapData` generation.
- Blender-like Object/Edit/Weight Paint/Segment Paint/Vertex Paint workflows,
  selection masking, continuous painting, accumulated weight option, camera
  navigation, and mode memory.
- Receiver/donor bone management: filtering, multi-selection, inline rename/F2,
  contextual add/remove/transfer, and segment/subsegment entries in one list.
- FO4 segment/subsegment creation, deletion/reassignment, ID editing, face
  visualization, and binary Segment Paint membership editing.
- Vertex color and vertex-alpha painting in its own workspace, using the shared
  NifSkope color chooser.
- Expanded Block List: search/filter chips, category icons, Blender-style inline
  scene-object renaming, multi-selection, navigation/context improvements, and
  the version-gated shader-property row hiding (fixed above).
- Unified viewport shading/effects menu, Blender numpad views/navigation,
  axis-only orthographic grids, and the normal perspective ground grid.
- Blender-style modeling tools: Extrude, Fill/Bridge, Loop Cut, Edge Slide,
  Subdivide, Inset, Dissolve, Symmetrize, Flip/Recalculate Normals, Add
  Primitive, Knife, Bevel, Merge, Smooth, delete (X), and the W Specials quick
  menu (edit + object mode).
- Rigging-aware object ops: Join (Ctrl+J) unions BSSkin bones/weights and
  merges segments by dismemberment slot; Separate (P) clones its own skin,
  rebuilds segment ranges, and compacts orphan vertices; Copy/Paste Branch
  (Ctrl+C/V) handle multi-block selections. Create Skin binds an unskinned
  FO4 mesh to a skeleton (byte-patch + reload pipeline).
- NIF Browser combines configured archives and loose mesh paths; Available NIFs
  above a separate Loaded NIFs pane; explicit right-click/drag enrollment into a
  combined workspace. Only selected loaded documents render or act as donors;
  the primary remains editable.

## Data-only background document layer

Explicitly enrolled Loaded NIFs are `BackgroundNifDocument`s — a parsed
`NifModel` plus source identity (loose path or configured game + archive path)
and workspace-group root — with no main window, dock/toolbar/menu, or GL
viewport. Promotion (**Make Primary / Edit** or double-click) lazily creates a
hidden real window, reloads the NIF from source, and runs the normal primary
switch. Loaded-NIFs pane, selection wiring, context menus, isolate/show/hide-all,
and the combined preview treat windows and data-only documents uniformly.
Closing the visible primary closes the whole workspace group. Rigging's donor
chooser uses `NifSkope::selectedWorkspaceModels()`. Background parses run in
`MSG_TEST` mode (no modal dialogs), touch no recent-file history, emit no load
signals.

Known limitation: `NifModel` parsing is still UI-thread-bound, so one very large
NIF pauses the app for its own parse. A safe threading route parses into a
thread-neutral intermediate (raw block buffers) off-thread and constructs the
`NifModel` on the UI thread — do **not** move an existing `NifModel`/Qt widget
to a worker thread.

## UV Editing workspace (phase 1 + feedback batches)

`UV_EDITOR_PLAN.md` is authoritative. Phase 1 in `src/uvtools.cpp` (UVEditorView
canvas + tlCreateUVManagerDock); GLView gained `elementSelectionChanged` /
`setElementSelectionExternal` / `setElementPickMode` for two-way selection sync
and public `viewTransform()` for Project From View. Shipped: Blender-look
background/tiles, progressive grid fade, **UV Sync Selection** toggle, **Unwrap**
(Angle-Based LSCM + Project From View), **UV Map** selector, Object-Mode
read-only display, **Repeat**/**Pixels** grid toggles, pixel snap, and
right-click **Place 2D Cursor Here**.

Caveats to watch in a GUI pass: LSCM quality on high-genus/near-degenerate
patches; Project From View ignores per-shape world transforms (camera direction
exact, in-plane offset not); Unwrap overwrites UVs in place (undoable) but
creates no new seams (phase 4).

## Deferred/future work

- Skeleton Manager and Pose Manager workspaces are placeholders.
- Game-accurate lighting, PBR channel support, and subsurface scattering remain
  future/disabled.
- Fully skinning an unskinned target: DONE 07-18 (Create Skin, rewritten to
  byte-patch + reload after the first version corrupted meshes — see
  `WW_CHANGES.md 2026-07-18b`). Still open: classic NiSkin backend,
  independent persistent skeleton reference, zero-weight bone pruning.
- In-game deformation, FaceGen/customization, and final visual behavior remain
  manual production checks.
