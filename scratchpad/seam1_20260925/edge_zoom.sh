#!/bin/bash
# The Sanctuary edge zoomed 4x: the 01_sanctuary_oblique framing is 8x8 cells (-23..-16, 18..25); 4x = 2x2 cells
# on the block's WEST edge (x = -20.00, the largest step, 12.9 lum): cells -21..-20, 21..22, view 8, look-at Z 6690.
# usage: edge_zoom.sh <out.png> [lod dir] [port]   (default lod dir: the shipped FO4CSLOD = the BEFORE)
H=/e/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925
F="/e/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"
env WW_LODL_SHEET_CACHE="C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/sheetcache_edge_$(date +%s)" \
  OBJ_DIR="$F" LODL_FILE="$F/Commonwealth.lodl" bash $H/shot_seam1.sh "$1" "${2:-$F}" -21 21 -20 22 8 ${3:-43872} 1 0 6690
