# Block List & Block Details — Overhaul Plan

**Status:** Backlog / design notes. **Not being implemented yet** (agreed 2026-07-12).
**Direction:** **Blender-familiar** — lean into the N-panel / icon / drag-reparent
interaction model rather than only bolting polish onto the classic
`Name | Value` trees. (A conservative "classic + additive polish" variant was
considered and set aside.)

A live annotated mockup of the target design was produced in-session (block list
with search/filter/colour-coding/summary/badges/referenced-by, and block details
with a field filter, collapsible sections, a named flag editor, jumpable link
fields, and typed value editors). This document is the written backlog for it.

## Open questions (decide before implementation)

1. **Dock vs. in-place** — enhance NifSkope's existing block-list / block-details
   panels, or promote them into a manager-style dock like the Material / Collision
   managers? (Undecided.)
2. Column set defaults and whether columns are per-block-type aware.
3. How far to take drag-to-reparent given the existing Set Parent / Clear Parent
   (#6) commands already cover parenting safely.

---

## Block List

### Navigation & findability
- **Filter / search bar** (same pattern as the managers): filter by name, block
  type, or block number; live highlight of matches.
- **Quick-filter chips:** Shapes, Nodes, Properties/Shaders, Collision,
  Controllers/Animated, Extra Data, "Has missing resource".
- **Type icons + category colour coding** down the left edge (geometry, node,
  property/shader, collision, controller, extra data, skin) for at-a-glance
  structure reading.
- **Go-to-block** (Ctrl+G): type a block number/name and jump.
- **Breadcrumb** above the list showing the selected block's parent chain.
- **Back / Forward** navigation through recently-selected blocks (browser-style).

### Columns & info
- **Smarter, configurable columns:** Type · Name · a per-type **summary**
  (shape → "6.2k tris / skinned"; controller → target + type; texture set →
  diffuse name; shader → material file; node → child count / bone count).
  Right-click the header to toggle columns.
- **Status badges** (reuse the Material Manager's): MISSING, OUT OF SYNC,
  OVERRIDDEN, non-standard flags, "visible but no collision".
- **Footer totals:** N blocks · S shapes · V verts · T tris · NIF/BS version.

### Relationships
- **References / Referenced-by peek:** select a block and surface what it links
  to and what links to it (side strip or hover-highlight). High value for
  untangling FO4 files.
- **Drag-to-reparent** in Hierarchy view; reorder blocks by drag with automatic
  link fixups (guard against cycles — reuse the Set Parent validation).

### Editing & workflow
- Inline rename (double-click the name).
- Multi-select bulk operations (delete, flag change, retarget).
- Pin / bookmark blocks.
- Two-way selection sync: block list <-> details <-> viewport <-> managers
  (partly present already).

---

## Block Details

### Typed editors (biggest quality-of-life win)
- **Named flag editor:** expand Shader Flags 1/2 (and other bitfields) into a
  checklist of named bits using the existing enum data, instead of a raw hex
  number.
- **Link fields:** show the target block's name inline, click to jump, and a
  dropdown to pick a valid target; invalid links flagged red.
- **Colour swatch** picker; **file path** field with a Browse... button and
  red missing-file state; **enum dropdowns**; **0-1 floats as sliders**;
  vector / matrix **drag-spin** editors.
- **Units helper:** show Havok<->game-unit and radians<->degrees inline.

### Structure & context
- **Field filter box** + collapsible **sections/groups** for long blocks
  (Transform, Shader, Textures, Skin, ...).
- **Derived values** shown read-only beside raw ones (resolved string beside a
  string index; tri/vert counts; bounding sphere).
- **Tooltips from nif.xml** field descriptions; a toggle for hidden/conditional
  fields.
- **Hex view** toggle for binary blobs (collision packfiles, mesh data).

### Actions
- Copy / paste a field value; copy a whole block.
- **Diff two blocks** side by side.
- Per-field "reset to default".
- All inline edits snapshot-undoable (reuse `nifSnapshotOp`).

---

## Cross-cutting
- A Blender-style **N-panel / tabbed Properties** shell: the details pane gets
  tabs (Item · Links · Shader/Material · Notes) instead of one flat tree.
- Consistent theming / icons with the Material and Collision managers.

---

## Suggested phasing (top 6 first)
1. Block-list **filter + type icons/colours**
2. **Named flag editor** in details
3. **Link fields** (jump + pick + validate)
4. **References / Referenced-by** peek
5. Per-type **summary column** in the block list
6. **Field filter + collapsible sections** in details

_This is design-only for now; revisit after the current Material/texture-preview
work settles. See also `TO_BE_IMPLEMENTED.md` #8 (workspaces) which owns the dock
layout this feature would live in._
