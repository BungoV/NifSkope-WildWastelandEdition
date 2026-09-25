# EXTENT1 deliverable text (the overseer splices these; the lane edited none of the three files)

## HANDOFF.md (top block, one entry)

**EXTENT1 (2026-09-25, branch extent1-20260925, no src change, no bake).** bungo asked whether the whole
Commonwealth map is covered by the landscape heightmap. It is. The installed `Commonwealth.lodl` header covers
cells -96..95 x -96..95. That is every LAND cell of Fallout4.esm (36,864 of 36,864) and his MO2 order (the same).
Heights sampled at 5 outer cells (121 samples each) match LAND exactly (0 mismatched). Vanilla's non-flat .BTR
tiles lie inside the same square. The director's "-42..33 x -48..39" is the PLACEMENT bounds (184,431 objects
in 4,541 cells), which earlier pictures used as their frame. The grey land around the objects in the old
09_overview was unpainted LAND (32,909 cells have no BTXT/ATXT), not missing terrain. SEAM1's
`--vt-fill-vanilla` sheets, installed at 16:14, paint it.
Pictures (untracked): `scratchpad/extent1_20260925/pics/whole_top_before.png` / `whole_top_after.png`
(top-down, 3200x3224, cells -96..95, all objects), `whole_obl_before.png` / `whole_obl_after.png` (view 8).
Placement cells on grey: 776 -> 8 (the 8 are the Glowing Sea, painted grey in the ESM). Cells with no terrain
under them: 0 -> 0. Refuter: a placement cell whose centre pixel in `pics/top/T.png` is the background colour,
or a LAND cell outside the .lodl header.
Open: the viewer's object layer refuses the whole map (13.1M vertices > 9.5M cap, all-or-nothing). The pictures
are two halves composited (skill `ww-whole-map-picture`). Raising or streaming the cap is a separate question
for bungo.

## WW_CHANGES.md (entry)

### 2026-09-25 -- EXTENT1: the whole-map terrain extent, measured (no code change)
- The `.lodl` already spans the whole worldspace (-96..95, 36,864 LAND cells, header = `EsmWorld::cellBounds()`
  over every CELL with XCLC). No writer change was needed, so there is no red->green code gate. The check
  "header bounds = LAND bounds" is green on the installed file.
- The pictures that looked cut short were framed on the placement bounds (-42..31 x -48..38) or on an old
  render region (-64,-48..31,47). The grey was unpainted LAND before SEAM1's vanilla fill.
- New lane tools (scratchpad only): the whole-map shot driver with a split-half object composite past the
  viewer's 9.5M-vertex cap, a frame check (corners at least 20 px inside), and the census of placement cells
  on grey and on no terrain.

## MISTAKES.md (root, newest at top)

### 2026-09-25 -- a picture's frame was read as the file's extent (EXTENT1)
Two lanes framed whole-map pictures on the PLACEMENT bounds: SEAM1 W3 used -42..32 x -48..38, and BAKE1
09_overview used -64,-48..31,47. The terrain beyond the frame was not drawn, so the pictures looked like a
.lodl that stops at the objects. The brief then asked for a writer fix for bounds the file already had, and a
lane nearly re-baked for it. Rule: a whole-map picture is framed on the `.lodl` header bounds (i32 x4 at 0x08).
The caption states the frame. Before calling anything "the extent", read the header, not a picture.
Skill `ww-whole-map-picture` section 1.
