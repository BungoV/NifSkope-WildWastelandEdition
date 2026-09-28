#!/bin/bash
# FLAT2 before/after: the sea edge (cells 34 -10 .. 41 -3, the coast at x ~ 38), the maps1 Boston camera's
# settings (view 8 ortho, half-width 16384 = 8 cells across, 1600x1600, level 2, slot 0, sheets at dim 2).
# Landscape + objects: the installed .lodl/.lodi (read only). Sheets: the sea bake's own .lodt.
#   before = rung exe + sea_rung sheets (every sheet stored in full)
#   after  = new exe  + sea_on sheets   (one-value sheets stored as 16-byte records)
ME=/e/Projects/NifskopeWWE-flat2/scratchpad/flat2_20260927
P=$ME/pics; mkdir -p $P
INST="/e/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"
port=43310
shot() { # <name> <exe> <sheet dir>
  port=$((port+1))
  if [ -s $P/$1.png ]; then echo "SKIP $1"; return; fi
  env ${WATER:+WW_LODL_WATER=$WATER} NS="$2" SHEETS="$3" LODI_DIR="$INST" LV=2 SLOT=0 SDIM=2 \
    bash $ME/shot.sh $P/$1.png "$INST" Commonwealth 34 -10 41 -3 8 16384 1600 1600 $port
}
shot F8_sea_edge_inapp_before $ME/run_rung/NifSkope.exe $ME/bakes/sea_rung/vt/FO4CSLOD/Commonwealth
shot F9_sea_edge_inapp_after   $ME/run_new/NifSkope.exe  $ME/bakes/sea_on/vt/FO4CSLOD/Commonwealth
# the one-value sheets are the sea floor, which the default flat water sheet hides: the same pair with water off
WATER=0 shot F10_sea_edge_inapp_nowater_before $ME/run_rung/NifSkope.exe $ME/bakes/sea_rung/vt/FO4CSLOD/Commonwealth
WATER=0 shot F11_sea_edge_inapp_nowater_after  $ME/run_new/NifSkope.exe  $ME/bakes/sea_on/vt/FO4CSLOD/Commonwealth
