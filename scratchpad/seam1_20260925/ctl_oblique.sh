#!/bin/bash
# SEAM1 controls in PERSPECTIVE (bungo: every picture is the 08_boston_oblique camera unless he asks for top-down):
# view 8, the 01_sanctuary_oblique framing (cells -23,18..-16,25, look-at Z 6690), terrain sheets from each control
# bake, the .lodl and objects from the shipped FO4CSLOD (read-only). Sequential: one NifSkope at a time.
H=/e/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925
F="/e/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"
mkdir -p $H/pics/ctl
p=43861
for k in a b c; do
  # a FRESH sheet cache per render: the default cache names tiles by container STEM and reuses a tile newer than
  # the container, so two bakes of one worldspace in two folders draw the first one's tiles (measured 13:1x)
  env WW_LODL_SHEET_CACHE="C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/sheetcache_ctl_$k" OBJ_DIR="$F" LODL_FILE="$F/Commonwealth.lodl" bash $H/shot_seam1.sh $H/pics/ctl/ctl_${k}_oblique.png \
    "$H/ctl/rung/$k/vt/FO4CSLOD/Commonwealth" -23 18 -16 25 8 $p 2 0 6690
  p=$((p+1))
done
echo ALLDONE
