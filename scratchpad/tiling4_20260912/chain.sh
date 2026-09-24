#!/bin/bash
# TILING4 gate F4 -- the harness chain on the new exe, at TILING2/TILING3 baselines.
cd /e/Projects/NifskopeWildWastelandEdition
L=scratchpad/tiling4_20260912/logs
for h in lodl_open lodgen_terrain lodgen_terrain_vt lodgen_roads \
         lodgen_ground_cover lodgen_terrain_pbrm lodgen_native \
         lodgen_panel_run lod_generation ui_align water_ui; do
	echo "=== $h  $(date +%H:%M)"
	bash tests/spells/$h.sh > $L/c_$h.log 2>&1
	rc=$?
	echo "   rc=$rc :: $(grep -E "^[0-9]+ checks" $L/c_$h.log | tail -1) :: $(grep -E "^RESULT" $L/c_$h.log | tail -1)"
done
echo "=== chain done $(date +%H:%M)"
