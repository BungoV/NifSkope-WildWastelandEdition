DONE -- EXTENT1, 2026-09-25 18:04 (clock read). Branch extent1-20260925 from 2b99143f. No src change, no build
pending, no re-bake. The install was not written: Commonwealth.lodl is still 06:01, sha1 b4466203c987.
The VT files were not touched.

## Verdict: C4. The .lodl already covers the whole map; the pictures were framed short
- `.lodl` header: cells [-96,-96]..[95,95]. LAND in Fallout4.esm: 36,864 of 36,864 cells, bounds -96..95.
  LAND in his MO2 order: the same. The header comes from `EsmWorld::cellBounds()` (every CELL with XCLC), so C1
  (WRLD object bounds) and C2 (LAND read from some groups only) are both refuted.
- Heights at 5 outer cells ((-80,2), (2,70), (2,-85), (-60,-60), (60,60)), 121 samples each: 0 mismatched
  against LAND.
- Vanilla .BTR: 1,239 tiles of 3,000 bytes or more against 1,240 LAND-relief tiles. The one relief tile that is
  small is still relief. All of them lie inside -96..95 (census.txt).
- C3 (vanilla's outer terrain is not LAND) is refuted too: vanilla's outer relief is LAND relief.
- The "-42..33 x -48..39" is the PLACEMENT bounds: 184,431 placements in 4,541 cells, -42..31 x -48..38.
  0 placement cells lie outside the .lodl.
- What bungo saw:
  - (a) Frames set on the placement bounds (SEAM1 W3) or on an old region (BAKE1 09_overview, -64,-48..31,47).
  - (b) 32,909 unpainted LAND cells (no BTXT/ATXT) drawn flat grey until SEAM1's `--vt-fill-vanilla` sheets
    (installed 16:14).

## Gates (numbers)
- Header bounds = LAND bounds: green on the installed file. There was no red: no code was wrong. A red->green
  code gate therefore does not exist, and I did not make one up.
- Three outer cells' heights: 0 mismatches (above). Interior byte identity: not applicable (no re-bake).
- Placement cells with NO terrain under them (the centre pixel is the background in the terrain-only top-down
  render):
  - before: 0 of 4,541;
  - after: 0 of 4,541.
  - The cell-to-pixel map was chosen by correlating placements with object pixels over the 4 flips:
    0.437 / 0.438 for flipx 0 flipy 0, next best 0.24.
- Placement cells ON GREY (chroma <= 2.2, calibrated on the before picture: unpainted LAND p95 2.2, median 2.0):
  - before (pre-fill VT sheets from SEAM1's replaced/): 776 cells / 9,767 placements (742 unpainted, 34 painted);
  - after (installed): 8 cells / 97 placements. All 8 are painted LAND whose own texture is grey (Glowing Sea).
  - LAND cells on grey: 31,501 -> 9 of 36,864.
- Object cap:
  - The whole map needs about 13.1M vertices, over the viewer's 9.5M cap. The cap is all-or-nothing, and one
    probe drew no objects at all.
  - A coarser WW_LODI_LEVEL cannot help: the cluster ladder is off and the level is clamped.
  - Slot 1 would drop 31,315 placements.
  - So the objects were drawn in two halves (cells x <= -9 and x >= -8) with one camera and composited.
    63,724 + 120,707 = 184,431 drawn, 0 dropped.
  - Overlap: both halves drew over the same pixels on 0.083% of object pixels (top-down) and 0.46% (oblique).
- Frames (frame_check.py, background corners agree, content bbox inside every edge):
  - top-down 3200x3224: margins 27/27/39/39 px;
  - oblique 3200x1504: margins 39/40/58/56 px. All >= 20.

## Pictures (untracked, pics/)
- whole_top_before.png, whole_top_after.png: top-down (view 1), cells -96..95, ortho 400000, all objects.
- whole_obl_before.png, whole_obl_after.png: 08_boston_oblique camera (view 8), cells -96..95, ortho 570000.
- In both pairs, "before" = the same .lodl/.lodi and camera with the pre-fill VT sheets. The before oblique shows
  the full relief (the western mountains, the southern edge) in grey: the heights were always there.
- Parts: pics/top/{T,A,B,T0,A0,B0}.png and pics/obl/ the same. Logs with each.

## Build / commits
- exe: release/NifSkope.exe 17:43:19, sha1 53f8f18c (the lane rung, unmodified branch point).
- Commits: 1b2f49db (census), 8022d464 (tools), 4541d4b7 (grey census), plus this report commit.

## What the bake needs
Nothing. The .lodl is correct. The unpainted-LAND grey is SEAM1's fill, already installed.

## Refuters
- A LAND cell outside the .lodl header.
- A placement cell whose centre pixel in pics/top/T.png is the background colour.
- A placement cell still on flat grey in whole_top_after.png outside the Glowing Sea.
- A log whose drawn counts do not sum to 184,431.

## Skill review
- Loaded: nifskope-ww-worktree-build, nifskope-ww-render-shot, ww-lodl-offline-census, ww-artefact-localise,
  nifskope-ww-lodgen.
- Wished for: a whole-map picture recipe (the frame, the object cap, before/after) -- none existed.
- Written: E:\Projects\Claude\.claude\skills\ww-whole-map-picture\SKILL.md (frame on the header not the
  placements, dim-16 sheets, split-half composite past the 9.5M cap, SHEETS_DIR before/after, the two counts,
  frame check). One pointer line added to ww-whole-map-lod-bake's Pictures section.
- NOT mirrored into E:\Tools\AISkills (outside this lane's write scope, and that repo has other lanes'
  uncommitted edits). Overseer: add it under lod-generation/references.
