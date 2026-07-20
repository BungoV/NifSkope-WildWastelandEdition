# NifSkope WW — Performance Plan (2026-07-20)

Holistic survey of where time goes, from four parallel code audits: file load,
per-frame rendering, the data-model layer, and UI signal reactions. Every claim
below was verified against source with file:line references. Nothing has been
changed yet — this is the plan.

## What is already good (verified — do not "fix")

- **Build flags:** release compiles `-O3 -march=haswell` (AVX2+F16C). No win left here.
- **Idle:** no repaints when nothing changes. The 144 Hz `advanceGears` timer is a
  heartbeat, not a repaint pump (`glview.cpp:15984`); `update()` calls are coalesced.
- **GPU path:** vertex/index data uploaded once into VAO/VBO (XXH3-keyed LRU
  geometry cache, `glcontext.cpp:1080`), skinning on the GPU via bone UBO. No
  per-frame CPU vertex re-transform, no per-frame `glReadPixels`; picking and
  the color eyedropper are click-gated FBO renders.
- **Load fundamentals:** O(n) array growth with one batched insert signal
  (`nifmodel.cpp:596`), condition results cached per item with the
  fixed-compound row-0 shared cache (`nifmodel.cpp:2537`), schema strings
  interned via shared `NifSharedData` (no per-row QString copies), archives and
  materials lazy in `GameManager`, and `swapModels()` detaches all four tree
  views during parse (`nifskope.cpp:1969`) — the measured ~520 ms
  "attached-reload" figure is mostly *already avoided* in the production path;
  what remains is the reattach + the un-gated post-load consumers below.
- **In-tree reference patterns to copy:** the collision dock's
  visibility-gated + coalesced `queueRebuild` (`spells/collisiontools.cpp:1037`),
  the UV dock's `rebuildOrDefer`/`viewportRebuildPending` (`uvtools.cpp:5703`),
  `NifProxyModel::reset(fast=true)`.

---

## Tier 1 — low effort, high or guaranteed impact

> STATUS 2026-07-20: items 1–4 SHIPPED (commit `629dfcf`, WW_CHANGES.md
> "Performance batch 1", harnesses green, GUI verify pending). Items 5–8 open.

1. **Stop re-indexing the game archives on every file load.**
   `populateConfiguredNifBrowser` (`nifskope.cpp:2576`, fired from every
   `completeLoading` at `:1088`) deletes and rebuilds the entire `BA2File`
   archive index, re-scans every configured resource path, re-enumerates the
   full merged file list, and rebuilds thousands of `QStandardItem`s —
   regardless of whether the NIF Browser dock is even visible. Fix: cache the
   archive index + item tree keyed by (game, resource-path set), rebuild only
   when those change; per load, rebuild only the "Loaded NIFs" group
   (`rebuildLoadedNifsBrowserGroup:2708` is already separate); skip entirely
   while the dock is hidden (rebuild on show). *Probably the single largest
   per-load win available for its effort.*

2. **Visibility-gate the hidden-dock reactors.**
   - Timeline: `refresh` → `scanModel` walks **every block** on each coalesced
     dataChanged/reset (`timeline.cpp:940`, `1009`) even with the dock closed.
   - Rigging dock: `refresh` (`spells/riggingtools.cpp:5405`, connected to
     `currentNifIndexChanged` **and** `objectSelectionChanged`) clears and
     repopulates the bone/donor trees on every selection change, hidden or not
     (the debug log at `:6256` even prints `visible=` without acting on it).
   - Post-load: header-tree reattach and the two `refreshRowHiding()` calls
     (`nifskope_ui.cpp:2835`) run with docks hidden.
   Fix for all: early-return when hidden + mark pending + refresh on
   `visibilityChanged`, exactly like the collision dock.

3. **Block Details tree: `uniformRowHeights=true`** (`nifskope.ui:717`).
   It is the only big view with it off; non-uniform heights force Qt to call
   the delegate's `sizeHint` — which does a `horizontalAdvance` text measure
   (`nifdelegate.cpp:239`) — for every laid-out row on every relayout. One-line
   change; verify no row actually needs a variable height.

4. **`NifTreeView::updateConditions` must not descend arrays.**
   The dataChanged reactor (`nifview.cpp:316`) recurses with the default
   `descendArrays=true`, so an edit inside the shown block walks every element
   of its 38k-row arrays (evalVersion + evalCondition + setRowHidden each),
   twice (tree + header). `refreshRowHiding` already deliberately passes
   `false` with a comment explaining why ("made every click take seconds") —
   apply the same to `updateConditions` / scope it to the changed row. Same for
   the header's load-time `updateConditions(0..20)` call (`nifskope_ui.cpp:4829`).

5. **Wrap the remaining bulk-write loops in `setState(Processing)`.**
   Two known unwrapped storms:
   - `riggingEnsureVertexColors` (`spells/riggingtools.cpp:3469`): 38k
     `set<ByteColor4>` calls under `holdUpdates` — which does **not** suppress
     per-leaf `dataChanged` (only `state` does) — so every write also pays the
     `invalidateDependentConditions` sibling substring-scan (`nifmodel.cpp:2586`).
   - First application of viewport transform gestures: `ChangeValueCommand` is
     pushed per leaf and only enters Processing when `idxs.size() > 1`
     (`undocommands.cpp:92`) — on the *initial* push every command is size-1,
     so a 38k-vert gesture emits 38k signals (undo/redo replay is fine, the
     first apply is not).
   This is the project's own established fix pattern; low risk.

6. **Per-frame micro-hoists (render loop):**
   - `Scene::draw` constructs a `QSettings` and reads
     `CollisionManager/CollisionOnly` **every frame** (`glscene.cpp:385`) —
     registry access per frame; hoist to a cached member updated on change.
   - `drawGrid` calls `qEnvironmentVariableIsSet("WW_PERF_TEST")` per frame
     (`glscene.cpp:505`) — read once into a static.
   - Compile the per-frame `glGetError` drain loop (`glview.cpp:2220`) out of
     release builds — it is a driver sync point every frame.

7. **`applyBlockListFilter`: early-out and coalesce.** On rows
   inserted/removed it runs immediately and un-coalesced (`nifskope.cpp:756`),
   and `directMatch` builds a formatted search string per block **even when the
   search box is empty** (`:1649`). Early-out on empty filter; route the rows
   signals through the existing `scheduleBlockFilter` coalescer.

8. **`updateHeader` block-type lookup → `QHash`.** `blockTypes.indexOf(name)`
   is a linear QString scan per block (`nifmodel.cpp:505`), O(blocks × types)
   on every structural edit / holdUpdates release.

## Tier 2 — medium effort, solid wins

9. **Cache the edit-mode overlay behind a dirty flag.** The whole overlay —
   world-vert array, unique-edge dedup, `filledTris`, wire/point soups
   (`glview.cpp:1840–2102`) — is rebuilt O(V+T) with fresh QSet/QHash/QVector
   allocations on **every repaint**, including pure camera orbits (the soups are
   world-space; the camera doesn't change them). Measured 0.6–0.9 ms/frame.
   Rebuild only on selection/geometry/deform change. Apply the same fix to the
   wireframe overlay (`:1803`) and the weight/vertex/segment paint mask
   (`:1281`), which share the pattern.

10. **Persistent scene transform cache.** `Scene::transform` clears
    `transformCache` and re-walks the entire node graph every repaint
    (`glscene.cpp:366`; `bhkRigidBody` nodes additionally do NIF model queries
    per frame, `glnode.cpp:476`). Keep the cache across frames; invalidate
    per-node on controller writes / animation-time change. Paused + static
    camera should recompute nothing.

11. **Batch multi-block removal.** Every `removeNiBlock` triggers a **full-model
    `updateLinks()` rebuild** (`nifmodel.cpp:805` → `:2629`), and at least five
    spells loop it highest-first (`havok.cpp:1886`, `optimize.cpp:98,255`,
    `spells/riggingtools.cpp:4164`, `skeleton.cpp:1243`,
    `animationsetup.cpp:858`) = M full rebuilds for M removals. `holdUpdates`
    already defers `utLinks` — wrapping those loops may be nearly free; else add
    a batch-remove API. Also note editing a single link field triggers a full
    link rebuild (`nifmodel.cpp:3130`) — bulk link writers must be Processing-wrapped.

12. **Memoize named field lookups in per-vertex tool loops.**
    `getItem(row, "Name")` is an uncached linear scan with per-child string
    compare (`basemodel.cpp:600`); tools call it per vertex
    (`spells/riggingtools.cpp:3443,3485`, `glview.cpp:5967,6008,8277,8900,9012`, …).
    Resolve the field's child index once from row 0 (fixed compounds are
    structurally identical rows) and index directly thereafter.

13. **Sort opaque draws by program/texture.** `setupProgram` re-runs full
    uniform + texture binding per shape per frame with no material batching
    (`renderer.cpp:104`; the code itself carries a "TODO: Hotspot" at
    `glmesh.cpp:789`). Sort the opaque pass by program → texture set; alpha pass
    keeps depth order.

14. **Frustum-cull active particle/lightning systems.** Simulation + draw run
    when playing regardless of visibility (`glscene.cpp:477–497`). Paused/absent
    gating is already correct.

## Tier 3 — structural (big effort, biggest ceilings)

15. **The NifItem explosion — the tax on everything.** Each packed
    `BSVertexData` row materializes ~30–40 individually heap-allocated
    `NifItem`s (~88–96 B each) — *both* precision variants of every field are
    built regardless of which one the file uses (`insertType` does no cond
    check, `nifmodel.cpp:1166`). A 38k-vert shape ⇒ ~1.5M allocations,
    ~75–80 MB, ~40× the on-disk size, in scattered heap blocks that defeat the
    cache for every later column-wise walk. Escalation ladder:
    a. **Slab/arena allocator** for NifItems during load — pure allocation-rate
       win, no semantic change (lowest risk, do first).
    b. **Skip statically-dead fields** — the mutually-exclusive
       Vector3/HalfVector3 variants alone are ~25% of per-vertex nodes; requires
       care with the row-0 shared-condition cache (rows must stay structurally
       identical — the known landmine).
    c. **Flattened packed storage for fixed compounds** (real fix, biggest
       risk): store vertex data as a typed array with a virtualized item
       facade. This collides with the `#ARG#` condition cache, the deferred
       conditional-array machinery, and every spell that walks rows —
       gate behind the full headless harness suite.

16. **Off-UI-thread parsing.** No background machinery exists in the load path
    (parse even pumps the event loop mid-load via `sigProgress`). The safe
    route stays as previously designed: parse to thread-neutral raw block
    buffers off-thread, construct the `NifModel` on the UI thread. Structural,
    pairs naturally with 15c.

17. **Continue retiring snapshot undo.** `nifSnapshotOp` serializes the whole
    file twice per structural op and undo reloads the entire model
    (`nifsnapshot.h:75`); `TlShapeStateCommand` is the established in-place
    replacement — convert Delete/Merge/Join per-op as already planned.

## Measurement protocol

Baseline before/after each tier with the existing `WW_PERF_TEST` probe
(`nifskope.cpp:3305` phase timings; `nifskope_ui.cpp:1818` attached-reload
probe) on the 38k-vert reference file; add a frame-time readout (rolling avg in
the status bar or ww log) before starting Tier 2 render work. Tier 1 items are
safe to land on inspection + existing harnesses; Tier 3 requires the full
headless gauntlet (join/sep/copypaste/createskin/extrude/dupfreeze) green at
every step.

## Landmines (from the surveys — respect these)

- Do not alter `getConditionCacheItem` / `#ARG#` shared-cache semantics
  (`nifmodel.cpp:2537–2584`) without exhaustive round-trip tests.
- The deferred conditional-array growth (0-length `Bone Weights`/`Bone Indices`
  until cascade) bites any path that pre-copies rows — known, documented.
- `glshape.cpp:680`: the geometry-cache hash-recompute gate ignores
  same-count content changes — verify edits invalidate the VBO (correctness
  check to fold into any render work).
- `src/collisiontools.cpp` vs `src/spells/collisiontools.cpp` are
  near-duplicates; `src/spells/` is the live copy — confirm before editing.
