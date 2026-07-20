# Block Details Overhaul v2 — CONCEPT (2026-07-20)

Focused successor to the Details half of `BLOCK_LIST_DETAILS_OVERHAUL.md`
(2026-07-12). Much of that doc's top-6 has since shipped: block-list
search/icons/chips, the Links/Referenced-by peek, the Block Details field
filter (Ctrl+Shift+F), the named flag dialogs (Shader/Node/BSX, 2026-07-20d),
and kind-tagged flag copy/paste incl. right-click spells (2026-07-20e). This
concept absorbs what remains and pushes into **workflow speed**: fewer clicks
to find a field, edit it, and repeat the edit across blocks.

**Interactive mockup:** `docs/block_details_mockup.html` (same convention as
the collision manager concept).

**Direction held from v1:** Blender-familiar. The classic raw tree never goes
away — it becomes the "Raw" view behind a toggle, and everything below layers
on top of the same NifModel.

---

## What's slow today (the problems being solved)

1. **The tree mirrors the binary layout, not the task.** A
   BSLightingShaderProperty is ~60 rows; editing Glossiness means scroll or
   filter every time. Field order is file order, not importance order.
2. **Link fields are numbers.** "112" tells you nothing; jumping goes through
   the block list's Links menu; re-targeting a link means typing a block
   number you first had to look up.
3. **Compound values read poorly.** Rotation is a 3×3 matrix in nested rows;
   colors are four float rows; vectors take three clicks to edit.
4. **No cross-block workflows.** Comparing two shaders, or applying the same
   change to five blocks, is manual repetition. (Flags now have copy/paste —
   nothing else does.)
5. **Big arrays are unusable for inspection.** 38k Vertex Data rows answer no
   real question ("what are vertex 1523's weights?" "are any weights NaN?").
6. **Repeated tweak sessions re-dig the same fields.** No pinning, no
   recently-edited memory.

---

## The concept, top to bottom

### 1. Header card (always visible above the fields)

- Type icon + block number + type name; **inline-editable Name**.
- **Clickable breadcrumb** of the scene-parent chain (jump anywhere up).
- Per-type **summary line**: shape → "4.0k v · 4.5k t · skinned (62 bones) ·
  7 segs"; shader → material path with a red MISSING badge; sequence →
  "24 controlled blocks · 3.2s".
- Quick actions: ▲ parent · Links peek (reuse) · ★ pin block · ⚡ spells ·
  **Copy Values / Paste Values** (see §4).
- **View toggle: Curated | Raw | Diff** (Raw = today's tree, unchanged).

### 2. Curated sections (the core change)

Per-type **section templates** map the important fields into task-shaped,
collapsible groups; collapse state persists per block type. Unlisted fields
fall into an automatic "Other" section; the full raw tree stays one toggle
away. First-class templates for the daily types:

- `BSTriShape` / `BSSubIndexTriShape` — Geometry (counts, bounds, Vertex
  Desc *read-only* summary chips), Skin, Segments, Links (shader, alpha).
- `BSLightingShaderProperty` / `BSEffectShaderProperty` — Material file,
  Colors, Surface (glossiness/specular/emissive), UV transform, Flags (chip
  row opening the flag dialog), Texture Set link.
- `BSShaderTextureSet` — the 10 slots as path fields (see §3).
- `NiNode` — Transform, Children, Flags chips, Extra Data.
- `BSSkin::Instance` + `BoneData` — skeleton root, bone list w/ jump chips.
- `NiAlphaProperty`, `BSXFlags`, `NiControllerSequence`, `bhk*` basics.

Fallback for unknown/rare types: header card + Raw tree only — no template
maintenance burden for the long tail.

**Implementation anchor:** this is the `NifBlockEditor` pattern (already in
tree, used by the bounds/light/material/transform spells) grown into a real
panel: typed editor widgets bound to model indices, refreshed on dataChanged.
It must consult `evalCondition`/`evalVersion` so version-gated fields never
render (same rules as the tree's row hiding).

### 3. Typed field editors (curated view; the delegate keeps working as-is in Raw)

- **Numeric:** DragSpinBox scrubbing everywhere (the viewport panels' widget,
  promoted to a shared location). 0–1 floats scrub as sliders; angle fields
  display degrees.
- **Rotation:** shown/edited as Euler XYZ (matrix under the hood — the
  transform-edit spell already does this math).
- **Vector3 / Color:** single-row triple/quad scrub; colors get a swatch +
  picker (ColorWheel exists).
- **Link fields:** `[icon] BSShaderTextureSet "Armor_d" #113  [jump] [pick]` —
  pick opens a searchable popup of type-compatible blocks, plus "New…"
  (insert block + link). Invalid links red. This is v1's #3, unbuilt.
- **Flags:** compact chip row of set bits; click opens the 2026-07-20d dialog.
- **Texture/material paths:** Browse against the configured-resources VFS
  (the NIF Browser's BA2 index — now cached, so this is cheap), red
  missing-file state, thumbnail tooltip via TexCache.
- **String-index fields:** resolved string shown beside the raw index
  (read-only derived value).
- **Tooltips from nif.xml** descriptions on every label (data already parsed).

### 4. Cross-block speed (the "new features")

- **Generalized value copy/paste.** Extend the flag clipboard pattern to any
  field: kind = the item's type/strType (+ a structure fingerprint for
  compounds/branches). Right-click any row (Raw or Curated): Copy Value /
  Paste Value. **Multi-block paste:** with a block-list multi-selection,
  Paste Values applies to every selected block that has the field — one undo
  macro, writes under `Processing`. This turns "set 5 shaders' glossiness"
  into copy → select 5 → paste.
- **Pinned fields.** ★ any field → it joins a "Pinned" section at the top for
  that block type, persisted in settings. A shader-tuning session becomes:
  select block, pinned Glossiness/Specular right there, scrub, next block.
- **Diff mode.** Third view toggle: current block vs. a same-type block picked
  from a dropdown. Two value columns, differences highlighted, per-row
  "◀ take theirs" and an "apply all" — built from the same section templates.
  Answers "why does this shader render differently" in seconds.
- **Recent values + revert.** Per-field dropdown of the session's last N
  values; "revert to loaded value" (capture original on first edit).
- **Whole-file value search.** The existing filter box gains a scope toggle:
  This Block / Whole File. Whole-file results list `block · field · value`
  rows that jump on click. Deferred/coalesced walk, skips big arrays unless
  the query is numeric.

### 5. Array & table tooling

- **"Open as table"** on fixed-compound arrays (Vertex Data, Triangles, bone
  weights): a spreadsheet dock — rows = elements, columns = fields,
  **virtualized** (only visible rows materialize widgets/strings).
- **Two-way viewport sync:** table selection ↔ edit-mode `pickedElems` (the
  UV editor already proves the sync pattern). Click a bad vertex in the
  table, see it highlighted in 3D.
- **Column stats** row (min/max/mean/NaN count) — finds rogue weights and
  NaN normals immediately.
- **CSV export/import** (timeline dock precedent) for external tooling.
- **Hex viewer** toggle for blob fields (collision packfiles, BSPositionData).

---

## Guardrails (hard-won this month — do not violate)

- `uniformRowHeights` stays true in tree views; rich editors are fixed-height
  inline or popups, never variable-height rows.
- Every bulk write path (multi-block paste, table edits, CSV import) runs
  under `setState(Processing)` + one span emit, with merged value commands or
  `TlShapeStateCommand` for array-shaped edits. No per-leaf signal storms.
- **No path may edit Vertex Desc** except the validated byte-patch pipeline;
  the table view treats desc-affecting columns as read-only (the 2026-07-18b
  corruption zone).
- Bind `QPersistentModelIndex` only for visible/curated fields — never per
  array element (they register in the model; thousands are a real cost).
- Curated sections must re-check field conditions on dataChanged (the shared
  row-0 condition cache makes this cheap).
- The array table reads packed rows on demand — designed so it can later sit
  directly on Tier-3 flattened storage (PERFORMANCE_PLAN.md 15c) as its
  display layer.

## Recommended answers to v1's open questions

- **Dock vs in-place:** in-place upgrade of the existing Block Details dock
  (keep muscle memory + layouts); only the array table and Diff open as
  separate docks/windows. Manager-style promotion adds nothing here.
- **Columns per-type:** superseded — per-type curation happens in sections,
  not columns.
- **Drag-to-reparent:** stays out (Set/Clear Parent commands cover it safely).

## Phasing

1. **P1 — Header card + curated sections** for the top-5 types + the typed
   **link editor** (jump/pick/new). The biggest daily-use win; everything
   else hangs off this shell.
2. **P2 — Field editors**: scrub/sliders, Euler rotation, color swatch,
   texture browse+missing, string-index derived display; **pinned fields**.
3. **P3 — Generalized copy/paste + multi-block paste + Diff mode.**
4. **P4 — Array table** (virtualized, viewport sync, stats, CSV).
5. **P5 — Whole-file search, recent values/revert, hex viewer.**

Each phase ships behind the Curated/Raw toggle, so Raw is always the escape
hatch and nothing regresses power users.
