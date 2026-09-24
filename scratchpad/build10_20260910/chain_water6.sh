#!/bin/bash
# lane BUILD10 -- the WATER6 harness chain, one NifSkope instance at a time.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
OUT=$ROOT/scratchpad/build10_20260910
cd "$ROOT" || exit 2
mkdir -p "$OUT/images"

run () {
	local label="$1"; shift
	local secs="$1"; shift
	if tasklist | grep -qi Fallout4; then echo "### $label SKIPPED: game up"; return; fi
	echo "### $label start $(date +%H:%M:%S)"
	timeout "$secs" "$@" > "$OUT/gate_$label.txt" 2>&1
	echo "### $label rc=$?  $(date +%H:%M:%S)"
	grep -aE "checks, [0-9]+ failures|^PASS|^FAIL|PASS$|FAIL \(|gates green" "$OUT/gate_$label.txt" | tail -8
	echo
}

run water_weights 900 bash tests/spells/water_weights.sh
run water_flow    900 bash tests/spells/water_flow.sh
run water_mark    900 bash tests/spells/water_mark.sh
SHOT=E:/Projects/NifskopeWildWastelandEdition/scratchpad/build10_20260910/images \
	run water_window  900 bash tests/spells/water_window.sh
run lodl_water    900 bash tests/spells/lodl_water.sh
run lodl_open     900 bash tests/spells/lodl_open.sh
echo "CHAIN DONE $(date +%H:%M:%S)"
