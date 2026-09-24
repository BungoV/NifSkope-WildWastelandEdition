#!/bin/bash
# BUILD12 gate chain -- one NifSkope instance at a time, sequential.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
OUT=$ROOT/scratchpad/build12_20260910
mkdir -p "$OUT/logs" "$OUT/images"
SUM="$OUT/gates_summary.txt"
: > "$SUM"

run () {
	local label="$1"; shift
	local secs="$1"; shift
	echo "### $label start $(date +%H:%M:%S)" | tee -a "$SUM"
	( cd "$ROOT" && timeout "$secs" "$@" ) > "$OUT/logs/$label.log" 2>&1
	local rc=$?
	echo "### $label rc=$rc  $(date +%H:%M:%S)" | tee -a "$SUM"
	grep -aE "checks, [0-9]+ failures|^PASS$|^FAIL$|PASS \(|FAIL \(| PASS$| FAIL$|checks run:" "$OUT/logs/$label.log" | tail -6 | tee -a "$SUM"
	echo | tee -a "$SUM"
}

# 1. the new gate, with both pictures.  env, not a prefix on the shell
# function: a `VAR=x func` assignment PERSISTS after a bash function returns
# and would leak SHOT into ui_align.sh below, overwriting this picture.
run water_ui 240 env SHOT="$OUT/images/topbar_after.png" \
	TABSHOT="$OUT/images/watertab.png" bash tests/spells/water_ui.sh

run water_weights 400 bash tests/spells/water_weights.sh
run water_flow    900 bash tests/spells/water_flow.sh
run water_mark    400 bash tests/spells/water_mark.sh
run water_window  400 bash tests/spells/water_window.sh
run lodl_water    400 bash tests/spells/lodl_water.sh
run lodl_open     400 bash tests/spells/lodl_open.sh

echo "=== neighbours ===" | tee -a "$SUM"
run ui_align      300 env SHOT="$OUT/images/seam_after.png" bash tests/spells/ui_align.sh
run top_bar       300 bash tests/spells/top_bar.sh
run loaded_nifs   300 bash tests/spells/loaded_nifs.sh
run files_tab     300 bash tests/spells/files_tab.sh
run hkxanim_ui    300 bash tests/spells/hkxanim_ui.sh
run animws        400 bash tests/spells/animws.sh

echo "CHAIN DONE $(date +%H:%M:%S)" | tee -a "$SUM"
