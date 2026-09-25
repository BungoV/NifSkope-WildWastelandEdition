#!/bin/bash
# W3: the overview framed on the terrain's bounds (cells x -42..32, y -48..38, measured W3), PERSPECTIVE camera
# (view 8), lodl level 3, from a lod dir (default: the shipped FO4CSLOD = the BEFORE). usage: w3_overview.sh <out.png> [lod dir] [port]
H=/e/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925
F="${2:-/e/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth}"
env WW_LODL_SHEET_CACHE="C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/sheetcache_w3_$(date +%s)" \
  bash $H/shot_seam1.sh "$1" "$F" -42 -48 32 38 8 ${3:-43871} 3 0 0 1600 1000
