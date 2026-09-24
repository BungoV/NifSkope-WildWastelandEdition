#!/bin/bash
cd /e/Projects/NifskopeWildWastelandEdition
D=scratchpad/defaults2_20260923
until grep -q DRAW-DONE "C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/208f5149-d04c-4de8-9611-6134127ac30b/tasks/b7p2g8flo.output"; do sleep 5; done
tasklist | grep -qi fallout4 && { echo "GAME UP, stop"; exit 1; }
echo "== byte_gate bc start $(date +%H:%M:%S)"
PHASES=bc timeout 3500 bash tests/spells/lodgen_byte_gate.sh > $D/gate_lodgen_byte_gate.log 2>&1
echo "== byte_gate rc=$? end $(date +%H:%M:%S)"
tail -4 $D/gate_lodgen_byte_gate.log
echo BYTE-DONE
