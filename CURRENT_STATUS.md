# Wild Wasteland NifSkope — Current Status

Updated: **2026-07-21** — branch `feature/timeline` @ `f1d7932` (Loop Cut v3),
build green, all headless harnesses passing.

Since the 07-20 revision of this doc: the 07-20d..o batches landed (generic flag
dialog + copy/paste, Block Details batch 1 and its diff/Reference-column
follow-ups, the viewport batch, Loop Cut v1→v3). On 07-21: the outstanding
"Vertex Flags model landmine" was tested and **disproven**
(`WW_CHANGES 2026-07-21a`), and **pinned fields** shipped in Block Details
(`WW_CHANGES 2026-07-21b`, harness `WW_PINNED_TEST`).

**Doc hygiene, 07-21.** Three plans were found to overstate what is open, and
have been corrected: `TO_BE_IMPLEMENTED.md` listed six Blender-batch items
(Mirror editing, Checker deselect, Repeat Last, F9, Rip, Split) that were all
implemented; `BLOCK_DETAILS_OVERHAUL_PLAN.md`'s phasing predated the decision to
drop curated sections and did not reflect that P3 had shipped; and the Vertex
Flags landmine was an untested conjecture. **Standing rule: verify against the
code before building anything a plan lists as open** — the `viewport.*` shortcut
registry at the top of `glview.cpp` is the fastest check for viewport features.

This is the short handoff document. `WW_CHANGES.md` remains the detailed change
history, `BONE_WEIGHT_TRANSFER_PLAN.md` describes the transfer design,
`UV_EDITOR_PLAN.md` is the authoritative UV plan, and
`repo-tests/tests/rigging/QA_EVIDENCE_2026-07-13.md` contains the native
rigging evidence.

## Working agreement

- **CHANGED 2026-07-22 (user: "Do it yourself please, I'm busy").** The agent
  now performs its **own GUI verification** instead of handing the user a test
  checklist. Drive the real app from an env-gated `WW_*_TEST` harness and assert
  programmatically — prefer numeric state (model values, `Node::bounds()`, block
  counts) over screenshots, with `grabFramebuffer` pixel diffs as corroboration.
  **`grabFramebuffer` does not repaint** — pump `ogl->update()` +
  `processEvents()` twice first or you diff a stale frame.
- The user still validates **in-game** behaviour and anything needing their real
  assets (armour sets, skeletons, load order). Escalate those.
- *(superseded: "the user performs live GUI debugging; do not launch NifSkope".)*
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
  transfer run on its own output. The "model landmine" that entry generalised
  from — any spell doing `set<BSVertexDesc>` + `updateArraySize` on an
  unchanged vertex count is suspect, stock Vertex Flags included — was a
  **conjecture, and was DISPROVEN on 2026-07-21** (`WW_CHANGES 2026-07-21a`,
  harness `WW_VERTEXFLAGS_TEST`). Create Skin's bug was specific to
  *creating* arrays that had 0 children until the deferred cascade; flipping
  a live/dead bit on already-materialised fields is safe.
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

**As of 2026-07-21 this section is no longer the backlog.** Everything left to
implement — including the Skeleton Manager and Pose Manager workspaces, which
used to appear only here and were therefore invisible to any backlog review —
now lives in **`TO_BE_IMPLEMENTED.md`**, the single consolidated list. Keep
future deferrals there, not here.
- Game-accurate lighting, PBR channel support, and subsurface scattering remain
  future/disabled.
- Fully skinning an unskinned target: DONE 07-18 (Create Skin, rewritten to
  byte-patch + reload after the first version corrupted meshes — see
  `WW_CHANGES.md 2026-07-18b`). Still open: classic NiSkin backend,
  independent persistent skeleton reference, zero-weight bone pruning.
- In-game deformation, FaceGen/customization, and final visual behavior remain
  manual production checks.
