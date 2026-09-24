#!/bin/bash
# lane BUILD10 -- the WATER5 harness chain, one NifSkope instance at a time.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
OUT=$ROOT/scratchpad/water5_20260910
cd "$ROOT" || exit 2
mkdir -p "$OUT/images"

run () {
	local label="$1"; shift
	local secs="$1"; shift
	if tasklist | grep -qi Fallout4; then echo "### $label SKIPPED: game up"; return; fi
	echo "### $label start $(date +%H:%M:%S)"
	timeout "$secs" "$@" > "$OUT/gate_$label.txt" 2>&1
	echo "### $label rc=$?  $(date +%H:%M:%S)"
	grep -aE "checks, [0-9]+ failures|^PASS|^FAIL|PASS$|FAIL \(|flow gates green|SKIP" "$OUT/gate_$label.txt" | tail -8
	echo
}

run water_mark   900 bash tests/spells/water_mark.sh
run water_flow   900 bash tests/spells/water_flow.sh
SHOT=E:/Projects/NifskopeWildWastelandEdition/scratchpad/water5_20260910/images \
	run water_window 900 bash tests/spells/water_window.sh
run lodl_water   900 bash tests/spells/lodl_water.sh
run lodl_open    900 bash tests/spells/lodl_open.sh
echo "CHAIN DONE $(date +%H:%M:%S)"
ls -la --time-style=+%H:%M:%S "$OUT/images/" 2>&1
