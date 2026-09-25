#!/bin/bash
# SEAM1 before/after with the 08 camera (view 8, 1600x1600, bungo's "just like on the pic I gave you").
# usage: seam_shots.sh <before|after> <sanc|whole> [port]
#   sanc  = the 01_sanctuary_oblique framing: cells -23,18..-16,25, lodl level 2, look-at Z 6690
#   whole = the terrain bounds used for W3 (w3_overview.sh): cells -42,-48..32,38, lodl level 3, 1600x1000
# Everything is read from the FO4CSLOD folder as it is at the time of the call (before = shipped, after = installed).
# A FRESH WW_LODL_SHEET_CACHE per render (the stem-keyed cache defect).
H=/e/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925
F="/e/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"
T=$1; R=$2; P=${3:-43891}
C="C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/sheetcache_${T}_${R}_$(date +%s)"
mkdir -p $H/pics/seam
if [ "$R" = sanc ]; then
  env WW_LODL_SHEET_CACHE="$C" bash $H/shot_seam1.sh $H/pics/seam/sanctuary_${T}_oblique.png "$F" -23 18 -16 25 8 $P 2 0 6690
else
  env WW_LODL_SHEET_CACHE="$C" bash $H/shot_seam1.sh $H/pics/seam/whole_${T}_oblique.png "$F" -42 -48 32 38 8 $P 3 0 0 1600 1000
fi
