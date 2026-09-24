#!/bin/bash
# DEFAULTS2: GUI gates one at a time, never in parallel.
cd /e/Projects/NifskopeWildWastelandEdition
D=scratchpad/defaults2_20260923
for g in lodgen_octahedral impostor_draw impostor_trunk lodgen_panel_run; do
	if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then echo "REFUSED $g: game up"; continue; fi
	S=$(date +%s); echo "== $g start $(date +%H:%M:%S)"
	timeout 3000 bash tests/spells/$g.sh > $D/gate_$g.log 2>&1
	echo "== $g rc=$? $(( $(date +%s) - S ))s end $(date +%H:%M:%S)"
	grep -E "checks, |RESULT" $D/gate_$g.log | tail -2
done
echo CHAIN-DONE
