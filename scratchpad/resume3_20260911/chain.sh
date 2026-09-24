#!/bin/bash
# RESUME3 gate R5: the harness chain, SEQUENTIAL, one NifSkope instance at a time.
#
# Its own lock, three lines, so a second copy is impossible rather than merely
# unlikely (`nifskope-ww-resume-pending` section 5: two copies of a chain ran at
# once on 2026-09-11 and every harness after the first collided on its fixed
# port). Each `env VAR=` goes on the CHILD, never as a prefix to the helper
# function -- bash keeps a prefix assignment on a FUNCTION call in the
# environment afterwards, which silently hands the next picture-taking spell the
# previous one's SHOT path.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
OUT="$ROOT/scratchpad/resume3_20260911/chain"
mkdir -p "$OUT/logs"
cd "$ROOT"

mkdir "$OUT/.lock" 2>/dev/null || { echo "REFUSED: a chain is already running ($OUT/.lock)"; exit 8; }
trap 'rmdir "$OUT/.lock" 2>/dev/null' EXIT

SUM="$OUT/summary.txt"
: > "$SUM"

run () {
	local label="$1"; shift
	local secs="$1"; shift
	{
		echo "### $label start $(date +%H:%M:%S)"
		timeout "$secs" "$@" > "$OUT/logs/$label.log" 2>&1
		echo "### $label rc=$?  $(date +%H:%M:%S)"
		grep -E "checks, [0-9]+ failures|checks [0-9]+, failures [0-9]+|^RESULT|^PASS|^FAIL" \
			"$OUT/logs/$label.log" | tail -4
		# harnesses that print RESULT PASS and no count: count their own ok lines
		if ! grep -qE "checks" "$OUT/logs/$label.log"; then
			echo "   ok-lines: $(grep -cE '^  ok' "$OUT/logs/$label.log")" \
			     "  FAIL-lines: $(grep -cE '^  FAIL' "$OUT/logs/$label.log")"
		fi
		echo
	} >> "$SUM" 2>&1
}

echo "chain start $(date +%H:%M:%S)" >> "$SUM"
ls -l --time-style=full-iso release/NifSkope.exe >> "$SUM"

# --- the generator suites the change reaches --------------------------------
run lodgen_terrain        900 bash tests/spells/lodgen_terrain.sh
run lodgen_terrain_vt    1500 bash tests/spells/lodgen_terrain_vt.sh
run lodgen_roads          900 bash tests/spells/lodgen_roads.sh
run lodgen_ground_cover   900 bash tests/spells/lodgen_ground_cover.sh
run lodgen_terrain_pbrm   900 bash tests/spells/lodgen_terrain_pbrm.sh
run lodgen_native         900 bash tests/spells/lodgen_native.sh
run lodgen_stage_times    900 bash tests/spells/lodgen_stage_times.sh
run lodgen_identity       900 bash tests/spells/lodgen_identity.sh
run lodgen_merge          900 bash tests/spells/lodgen_merge.sh
run lodgen_card_arrays   1200 bash tests/spells/lodgen_card_arrays.sh
run lodgen_texture_arrays 900 bash tests/spells/lodgen_texture_arrays.sh
run lodgen_impostor_cards 1200 bash tests/spells/lodgen_impostor_cards.sh
run lodgen_octahedral     900 bash tests/spells/lodgen_octahedral.sh

# --- the GUI side -----------------------------------------------------------
run lodgen_panel_run     1500 bash tests/spells/lodgen_panel_run.sh
run lod_generation       1800 bash tests/spells/lod_generation.sh
run lodl_open             900 bash tests/spells/lodl_open.sh
run ui_align              600 env SHOT="$OUT/ui_align.png" bash tests/spells/ui_align.sh
run water_ui              600 bash tests/spells/water_ui.sh

echo "chain end $(date +%H:%M:%S)" >> "$SUM"
cat "$SUM"
