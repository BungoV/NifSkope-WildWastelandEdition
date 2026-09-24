#!/bin/bash
# DEFAULTS1 item 5: the LOD Generation panel rows round-trip, and the full
# column is photographed on BOTH exes so the moved rows can be read side by side.
#
# ONE NifSkope at a time, second monitor (WW_WINDOW_AT is set by _harness.sh),
# each run on its own unused --port.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
OUT="$ROOT/scratchpad/defaults1_20260912/pics"
mkdir -p "$OUT"

run() {
	local who="$1" exe="$2" port="$3"
	echo "== $who =="
	rm -f "$ROOT/release/ww_lodgen_test.log"
	WW_LODGEN_SHOT_FULL="$(cygpath -m "$OUT/panel_${who}_full.png")" \
		EXE="$exe" PORT="$port" \
		timeout 400 bash "$ROOT/tests/spells/lod_generation.sh" \
		> "$ROOT/scratchpad/defaults1_20260912/panel_${who}.txt" 2>&1
	echo "  rc=$?  $(grep -a ' checks, ' "$ROOT/scratchpad/defaults1_20260912/panel_${who}.txt" | tail -1)"
	tail -2 "$ROOT/scratchpad/defaults1_20260912/panel_${who}.txt"
	ls -l "$OUT/panel_${who}_full.png" 2>/dev/null | sed 's/^/  /' || echo "  NO PICTURE"
}

run new  "$ROOT/release/NifSkope.exe"                    42371
run rung "$ROOT/release/NifSkope.before_defaults1.exe"   42372
