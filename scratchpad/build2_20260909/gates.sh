#!/bin/bash
# BUILD2 gate chain. STRICTLY SEQUENTIAL: one NifSkope instance at a time
# (CONSTITUTION 6). Each harness writes its own log; the summary lines are
# echoed as they land so a poll of this file reads the numbers.
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
OUT=scratchpad/build2_20260909
mkdir -p "$OUT/logs"

run () {                       # run <label> <timeout-seconds> <command...>
	local label="$1"; shift
	local secs="$1"; shift
	echo "### $label  start $(date +%H:%M:%S)"
	timeout "$secs" "$@" > "$OUT/logs/$label.log" 2>&1
	local rc=$?
	echo "### $label  rc=$rc  $(date +%H:%M:%S)"
	grep -E "checks, [0-9]+ failures|^PASS|^FAIL|^[0-9]+ checks" "$OUT/logs/$label.log" | tail -4
	echo
}

run render_shot   1800 bash tests/spells/render_shot.sh
run lodl_write     900 bash tests/spells/lodl_write.sh
run lodgen_terrain 900 bash tests/spells/lodgen_terrain.sh
run lodgen_identity 900 bash tests/spells/lodgen_identity.sh
run lodgen_terrain_vt 1800 bash tests/spells/lodgen_terrain_vt.sh
run lod_generation 900 bash tests/spells/lod_generation.sh
run btd_terrain    900 bash tests/spells/btd_terrain.sh
run lodgen_impostor_cards 2400 bash tests/spells/lodgen_impostor_cards.sh
echo "ALL DONE $(date +%H:%M:%S)"
