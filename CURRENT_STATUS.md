# Wild Wasteland NifSkope — Current Status

Updated: **2026-07-16**

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

## Latest completed change: Block List row-hiding fix (RESOLVED, confirmed) — commit `b0b330d`

Version-gated shader-property rows (Skyrim BSLightingShaderProperty rows, and
the FO76 rows: GTEFO76, Num SF1 / SF1) leaked back into the Block List after
switching between shader blocks. **Confirmed fixed by the user.**

Root cause — this was the misleading part: the hiding pass genuinely ran and
applied on *every* click (the trace showed 29/58 rows hidden at that instant),
but Qt stores hidden rows as **persistent index handles**, and model activity
during a block-to-block switch silently invalidated those handles **with no
reset signal**. The hidden set was still "there," just full of dead references,
so the rows re-drew visible before paint. Earlier reset-repair hooks never fired
because no reset signal was emitted.

Why verification kept lying: every probe tested the *first* shader block clicked
in a session, and the first block always hides correctly. The bug only appears
on the second and later shader blocks reached by clicking through the Block
List. The fix was found by upgrading the probe to sweep **all** shader
properties through the real Block List click path — it then reproduced instantly
(block 10 clean; blocks 14/18/23/28/32 each leaking exactly 29 rows).

The fix: the view now re-derives the row hiding inside a `doItemsLayout`
override — right before it rebuilds its layout — so no matter what invalidated
the stored persistent set, the version-gated rows are re-hidden with fresh
handles before anything is drawn. Guarding individual invalidation sites was
abandoned in favor of this single re-derivation point. Sweep now reports 29/29
hidden on every shader block through the same click path the user uses.

- Build: **PASS**, exe timestamped 13:18 on 2026-07-16.
- Verified via the all-blocks sweep probe on the user's file; user confirmed in
  the GUI (clicking through several shapes' shader properties).
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
- Blender-style modeling tools: Loop Cut, Edge Slide, Subdivide, Inset,
  Dissolve, Symmetrize, Flip/Recalculate Normals, Add Primitive, and the W
  Specials quick menu (edit + object mode).
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
- Fully skinning an entirely unskinned target still needs the complete FO4 skin
  structures and packed vertex layout created.
- In-game deformation, FaceGen/customization, and final visual behavior remain
  manual production checks.
