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

## Visual language rules (feedback 2026-07-20 — binding)

The first mockup draft looked like a web app; **rejected**. The overhaul must
read as the same application as the rest of WW NifSkope. Concretely:

- **The curated view IS a Name | Value tree** (QTreeView + delegate), with
  alternating rows, the standard fonts and colors, and section headers that
  expand/collapse exactly like compound rows do today. Not a form of custom
  widgets, not cards.
- **Reference aesthetic = the shipped Shader Flags dialog** and the Block
  List header: flat #383838/#454545 surfaces, 1px #303030 borders, grey
  #a8a8a8 secondary text, flat auto-raise tool buttons.
- **No pills, no filled badges, no colored chips, no filled-bar sliders.**
  Errors are red *text* ("(missing)"), links are colored *text*, scrubbable
  numbers are marked with a dotted underline only, and flag summaries are
  plain text ("F1 0x8040028B — Specular, Skinned, +6").
- **Extra controls hide until hover** (↗ jump, ▾ pick, ★ pin, Edit…) at the
  row's right edge — rows stay as quiet as today's until pointed at.
- The header is **two plain text lines** (breadcrumb, summary) plus one flat
  button row, styled like the existing Block List breadcrumb/footer lines.
- Diff highlighting uses the existing conventions (orange = the
  viewport-selection accent) rather than new colors.

## The concept, top to bottom

### 1. Header (always visible above the fields)

Two quiet text lines + one flat button row (block-list header styling):

- Line 1: block number · **inline-editable Name** (flat until hovered) ·
  type name in grey.
- Line 2: **clickable breadcrumb** of the scene-parent chain.
- Line 3: per-type **summary text**: shape → "4.0k v · 4.5k t · skinned
  (62 bones) · 7 segs"; shader → material path with "(missing)" in red;
  sequence → "24 controlled blocks · 3.2s".
- Button row: ▲ parent · Links n/m (reuse) · ★ pin · Copy Values ·
  Paste Values · **view toggle Curated | Raw | Diff** (Raw = today's tree,
  unchanged).

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

**Implementation anchor:** per the visual rules, the curated view is a
tree (QTreeView or QTreeWidget) whose rows BIND to model indices — section
rows are plain parent rows, field rows show the model value through the
existing delegate (which is where the typed editing already happens). The
`NifBlockEditor` machinery (bounds/light/material/transform spells) is the
precedent for index-bound refresh-on-dataChanged; the delegate work in §3
extends the *existing* `NifDelegate`, so Raw view gets the same editor
upgrades for free. Curated rows must consult `evalCondition`/`evalVersion`
so version-gated fields never render (same rules as the tree's row hiding).

### 3. Typed field editors (curated view; the delegate keeps working as-is in Raw)

- **Numeric:** DragSpinBox scrubbing everywhere (the viewport panels' widget,
  promoted to a shared location) — visually just the value with a dotted
  underline until hovered, per the visual rules; 0–1 floats scrub with a
  finer step, angle fields display degrees.
- **Rotation:** shown/edited as Euler XYZ (matrix under the hood — the
  transform-edit spell already does this math).
- **Vector3 / Color:** single-row triple/quad scrub; colors get a swatch +
  picker (ColorWheel exists).
- **Link fields:** value cell reads `Armor_d [113 → 114]  BSShaderTextureSet`
  (target name in link color, type in grey); hover reveals flat ↗ jump and
  ▾ pick buttons at the row edge — pick opens a searchable popup of
  type-compatible blocks, plus "New…" (insert block + link). Invalid links
  = red text. This is v1's #3, unbuilt.
- **Flags:** value cell shows hex + a grey named-bit summary ("F1 0x8040028B
  — Specular, Skinned, +6"); hover reveals Edit… opening the 2026-07-20d
  dialog.
- **Texture/material paths:** hover reveals a Browse (…) button resolving
  against the configured-resources VFS (the NIF Browser's BA2 index — now
  cached, so this is cheap); missing file = red text, thumbnail tooltip via
  TexCache.
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
