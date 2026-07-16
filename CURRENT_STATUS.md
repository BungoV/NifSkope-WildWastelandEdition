# Wild Wasteland NifSkope — Current Status

Updated: **2026-07-14**

This is the short handoff document. `WW_CHANGES.md` remains the detailed change
history, `BONE_WEIGHT_TRANSFER_PLAN.md` describes the transfer design, and
`repo-tests/tests/rigging/QA_EVIDENCE_2026-07-13.md` contains the existing native
rigging evidence.

## Working agreement

- The user performs live GUI and game debugging. Codex should change/read code,
  reason about it on paper, and run proportionate builds or non-GUI tests only.
- Do **not** launch NifSkope, Blender, or another GUI unless the user explicitly
  changes that instruction.
- Keep subagent use low; do not delegate unless the user explicitly asks.
- Production repository: `E:\Projects\ClaudeNifskope`
- Writable staging mirror:
  `C:\Users\bungo\Documents\Codex\2026-07-12\good-the-build-works-and-i\work\staging`
- Make edits in staging with `apply_patch`, sync only the changed files to the
  production repository, then compile there.
- Release build command:
  `C:\msys64\usr\bin\bash.exe -c 'cd /e/Projects/ClaudeNifskope && PATH=/ucrt64/bin:/usr/bin:$PATH make -f Makefile.Release -j4'`

## Current implemented state

- Rigging workspace/manager with donor-to-receiver bone and weight transfer,
  atomic preflight/rollback/Undo, FO4 validation, and byte-exact
  `CustomizationRemapData` generation.
- Blender-like Object/Edit/Weight Paint/Segment Paint/Vertex Paint workflows,
  selection masking, continuous painting, accumulated weight option, camera
  navigation, and mode memory.
- Receiver/donor bone management including filtering, multi-selection, inline
  rename/F2, contextual add/remove/transfer operations, and segment/subsegment
  entries distinguished in the same list.
- FO4 segment/subsegment creation, deletion/reassignment, ID editing, face
  visualization, and binary Segment Paint membership editing.
- Vertex color and vertex-alpha painting in its own workspace, using the shared
  NifSkope color chooser.
- Expanded Block List: search/filter chips, category icons, Blender-style inline
  scene-object renaming, multi-selection, and navigation/context improvements.
- Unified viewport shading/effects menu, Blender numpad views/navigation,
  axis-only orthographic grids, and the normal perspective ground grid.
- NIF Browser combines configured archives and loose mesh paths, keeps Available
  NIFs above a separate Loaded NIFs pane, and uses explicit right-click/drag to
  enroll files in a combined workspace. Only selected loaded documents render or
  participate as donors; the primary remains editable.

## Latest completed change: data-only background document layer

Explicitly enrolled Loaded NIFs are no longer hidden `NifSkope` windows. Each
is a `BackgroundNifDocument` — a parsed `NifModel` plus source identity (loose
path or configured game + archive path) and workspace-group root — with no
main window, dock/toolbar/menu construction, or GL viewport. Details:

- enrolling a donor costs one NIF parse, still one per event-loop turn through
  the existing queue;
- promotion (**Make Primary / Edit** or double-click) is the lazy UI-attach
  step: it creates a hidden real window, reloads the NIF from its original
  source, and runs the normal primary switch. Background documents cannot be
  edited, so the re-parse is lossless; failed reloads keep the entry and report
  on the status bar;
- the Loaded NIFs pane, selection wiring, context menus, isolate/show/hide-all,
  and the combined viewport preview treat windows and data-only documents
  uniformly (same palette). Removing a data-only entry = closing it;
- closing the visible primary still closes the whole workspace group, deleting
  data-only members outright;
- Rigging's donor chooser uses the new `NifSkope::selectedWorkspaceModels()`
  (model/display-path pairs for both kinds) instead of the window-only list;
- background parses run in `MSG_TEST` message mode (no modal dialogs), touch no
  recent-file history, and emit no load signals.

## Known limitation and likely next optimization

`NifModel` parsing is still UI-thread-bound, so one very large NIF still pauses
the application for the duration of its own parse (batches yield between
files). With windows out of the enroll path, the remaining cost **is** the
parse. Threaded parsing must not be attempted by simply moving the existing
`NifModel` or Qt widgets to a worker thread; their thread affinity and
connected renderer/model signals need an explicit safe boundary first. A safe
route would be parsing into a thread-neutral intermediate (e.g. raw block
buffers) off-thread and constructing the `NifModel` on the UI thread from that.

## Verification at latest change

- Release build: **PASS** on 2026-07-14 after the usability follow-ups
  (multi-select right-click enroll, automatic primary row, opaque shaded
  preview). Pre-existing warnings only (`GLView::gizmoEnd` unused locals,
  `meshtools.cpp` unused `tlOwnerLabel`).
- **Launch-crash incident (resolved):** the first follow-up build crashed at
  startup (0xC0000005 in QHash `findNode`) because `Makefile.Release` had a
  stale dependency list — `riggingtools.o` did not list `src/glview.h`, so it
  was linked with the old `GLView` layout after a data member was added.
  Fixed by re-running `qmake6 -o Makefile NifSkope.pro` (deps now correct) and
  rebuilding. Lesson: after adding members to widely-included classes or
  changing the include graph, re-run qmake6 before trusting an incremental
  build. Startup verified from the console; window appears and was closed
  immediately.
- Output: `E:\Projects\ClaudeNifskope\release\NifSkope.exe`
- No GUI was launched; the user still needs to verify in the GUI:
  right-click **Add N Selected to Loaded NIFs** enrolling every selected row,
  the primary always showing in Loaded NIFs with the Block List primary
  palette, the combined preview rendering opaque with readable shading and
  correct occlusion against the primary, donor visibility toggles, the
  Rigging donor chooser offering background documents, promotion via
  double-click and context menu (including a configured-resource donor), and
  primary close taking the whole workspace group with it.

## UV Editing workspace (phase 1 + feedback batch, 2026-07-14)

`UV_EDITOR_PLAN.md` is the authoritative plan; phase 1 is implemented in
`src/uvtools.cpp` (UVEditorView canvas + tlCreateUVManagerDock) with GLView
gaining `elementSelectionChanged` / `setElementSelectionExternal` /
`setElementPickMode` for two-way selection sync, and `viewTransform()` made
public for Project From View.

Feedback batch 1 (built green): Blender-look background/tile colors,
progressive grid fade on zoom, **UV Sync Selection** toggle (⇄), island-select
echo bug fixed, **Unwrap** (U / bar button: Angle-Based LSCM + Project From
View), **UV Map** selector for legacy multi-set meshes.

Feedback batch 2 (built green 2026-07-14 20:09, startup smoke-tested):
**Object Mode read-only display** (primary white + secondaries colored; no
longer forces Edit Mode on open — this is the real fix for the "sync doesn't
work / editor empty" report), **Repeat** image/grid toggle (default off =
0-1-only Blender look via shader `tileMode`), **Pixels** grid toggle (real
texture resolution via `glGetTexLevelParameteriv`, shader `drawPixelGrid`) +
**Pixel** snap target and Snap-menu pixel entries, right-click **Place 2D
Cursor Here** as the first item, and env cubemap/mask removed from the
underlay slot list.

User GUI checklist (still needs a real pass): enter the UV Editing workspace
on a textured FO4 mesh (auto-enters Edit Mode); verify the underlay/slot
picker, dark-gray tile + whitish progressive grid, pan/zoom/framing, all four
select modes (esp. **Island** now filling correctly) + box select mirroring
the 3D viewport both ways, the **Sync** button's on/off behavior, G/R/S with
axis constraint + numeric + snap popover, live 3D texture preview during
drags, one Ctrl+Z per gesture, **Unwrap** on a face selection (check island
packing/orientation), Project From View, cursor placement/snap menu, and
settings-bar state persisting.

Known post-feedback caveats to watch for in the GUI pass: LSCM unwrap quality
on high-genus or near-degenerate patches; Project From View ignores per-shape
world transforms (camera direction is exact, in-plane offset is not); Unwrap
overwrites UVs in place (undoable) but does not create new seams (phase 4).

## Deferred/future work already represented in the UI

- Skeleton Manager and Pose Manager workspaces are placeholders.
- Game-accurate lighting, PBR channel support, and subsurface scattering remain
  future/disabled features.
- Fully skinning an entirely unskinned target still requires creation of the
  complete FO4 skin structures and packed vertex layout.
- In-game deformation, FaceGen/customization, and final visual behavior remain
  manual production checks.

