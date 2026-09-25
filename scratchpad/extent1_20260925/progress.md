# EXTENT1 progress

## 17:46 (clock read)
- Worktree E:\Projects\NifskopeWWE-extent1, branch extent1-20260925 from 2b99143f. Objects copied from main (make -n = 0 there,
  exe 08:01 newer than last src commit 06:01); REVISION includers' objects deleted; rung built 17:43:19, sha1 53f8f18c,
  kept as release/NifSkope.before_extent1.exe. No src change so far.
- LOCALISED (no build): the premise is refuted. The installed Commonwealth.lodl header says cells [-96,-96]..[95,95]
  (sha1 b4466203), and BAKE1's own lodl.log says "v2 cells 192x192, land 36864 ... cross-check 36864 samples, 0 mismatched".
  [-42,-48]..[33,39] is the RENDER region: SEAM1's seam_shots.sh / w3_overview.sh pass WW_LODL_REGION=-42,-48,32,38
  (labelled "terrain bounds", measured in w2_cells.py as the PLACEMENT bounds x -42..31 y -48..38), widened to whole
  VT tiles by btdterrain.cpp. -> candidate C4 (the picture's frame), not C1/C2/C3.
- census.py (numbers in census.txt): LAND in 36864 of 36864 cells in his load order AND in Fallout4.esm alone;
  relief (max-min>0) in 17,972 cells, bounds -77..76 both axes. Vanilla level-4 .BTR: 2304 tiles; 1239 are >= 3000 B,
  and those are exactly the LAND-relief tiles (1240; 0 big-without-relief, 1 relief tile small). The 1015 tiles between
  2629 and 3000 B are all flat -352 filler. .lodl heights at outer cells (2,70) (-60,-60) (-80,2) (2,-85) (60,60):
  121 samples each, 0 mismatched vs LAND.
- Next: pictures (top-down + oblique, whole -96..95 frame and the old frame), no bake needed.
