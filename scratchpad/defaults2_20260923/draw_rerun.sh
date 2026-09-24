#!/bin/bash
# impostor_draw with the subject IMPOSTORDEPTH2 used (gates2.sh), after the main chain finishes.
cd /e/Projects/NifskopeWildWastelandEdition
D=scratchpad/defaults2_20260923
until grep -q CHAIN-DONE "C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/208f5149-d04c-4de8-9611-6134127ac30b/tasks/bq73pmu55.output"; do sleep 5; done
tasklist | grep -qi fallout4 && { echo "GAME UP, stop"; exit 1; }
echo "== impostor_draw start $(date +%H:%M:%S)"
IMPOSTOR_LODM="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostortear1_20260923/bake4x/blast_n4/cards/000531b3_oct.lodm" \
IMPOSTOR_NIF="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleblasted05.nif" \
	timeout 3000 bash tests/spells/impostor_draw.sh > $D/gate_impostor_draw.log 2>&1
echo "== impostor_draw rc=$? end $(date +%H:%M:%S)"
tail -1 $D/gate_impostor_draw.log
echo DRAW-DONE
