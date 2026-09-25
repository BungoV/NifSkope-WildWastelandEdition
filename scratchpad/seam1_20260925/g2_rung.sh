#!/bin/bash
# SEAM1 G2 before-bake: Fallout4.esm alone, cells -28,-36..-17,-25 (3x3 dim-4 chunks around 127-tile untouched area)
NS="$1"; TAG="$2"
O=E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/ctl/$TAG/g2
mkdir -p "$O/vt" "$O/scr"
"$NS" -no-gui lodgen "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" --worldspace 3C --terrain-region -28 -36 -17 -25 --dim 4 --out-dir "$O/scr" --tex-dir "$O/scr/textures" --vt "$O/vt" --vt-height --vt-density 16 --cover > "$O/log.txt" 2>&1
echo "$TAG/g2 rc=$?"
