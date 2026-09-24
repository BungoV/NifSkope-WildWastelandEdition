#!/bin/bash
# AUDIT1 step 3: every output file of a bake, decoded by the INDEPENDENT Python
# readers in tests/spells/ and by nothing the writer printed.
#   usage: bash decode_all.sh <bakedir> [more bakedirs...]
# Writes <bakedir>/../decode_<label>.log and prints one line a reader.
#
# The readers, and what each one owes the audit:
#   lodgen_lodl_pyramid.py   .lodl  pyramid-is-a-drop, cell range, water fields   (NEW, lane AUDIT1)
#   lodgen_vt_check.py       .lodt  header, tile coverage, border, georef, ladder
#   lodgen_native_decode.py  .lodo+.lodi  every refusal the contract names
#   lodgen_native_fields.py  .lodo+.lodi  every word lane, with floors
#   lodgen_native_cut.py     .lodo+.lodi  spheres, cones, the cut partition
#   lodgen_census_check.py   the bake census against the output on disk
#   lodgen_bakerec_gate.py   .lodb  the record against the tree it records
# Nothing here writes to the bake.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
PY="${PY:-/c/Users/bungo/AppData/Local/Programs/Python/Python39/python}"
S="$ROOT/tests/spells"

for D in "$@"; do
	LABEL="$(basename "$D")"
	LOG="$(dirname "$D")/decode_$LABEL.log"
	: > "$LOG"
	say () { echo "$*" | tee -a "$LOG"; }
	run () {  # run <name> <cmd...>
		local name="$1"; shift
		if [ ! -x "$PY" ] && ! command -v "$PY" >/dev/null; then say "$name: NO PYTHON"; return; fi
		local out rc
		out="$("$@" 2>&1)"; rc=$?
		printf '%s\n' "--- $name: rc=$rc" >> "$LOG"
		printf '%s\n' "$out" >> "$LOG"
		local n f
		n="$(printf '%s\n' "$out" | grep -cE '^\s*(ok|FAIL) ')"
		f="$(printf '%s\n' "$out" | grep -cE '^\s*FAIL ')"
		echo "$LABEL  $name  rc=$rc  checks=$n  fails=$f"
	}
	say "===== $LABEL  $(date '+%F %T')"
	LODL="$(find "$D" -name '*.lodl' | head -1)"
	LODO="$(find "$D" -name '*.lodo' | head -1)"
	LODI="$(find "$D" -name '*.lodi' | head -1)"
	LODB="$(find "$D" -name '*.lodb' | head -1)"
	say "  .lodl ${LODL:-none}"
	say "  .lodo ${LODO:-none}"
	say "  .lodi ${LODI:-none}"
	say "  .lodb ${LODB:-none}"
	[ -n "$LODL" ] && run lodl_pyramid "$PY" "$S/lodgen_lodl_pyramid.py" "$LODL" --stride 4
	for VT in $(find "$D" -name '*.VT.*.lodt' | sort); do
		run "vt_tiles($(basename "$VT"))" "$PY" "$S/lodgen_vt_check.py" tiles "$VT"
		run "vt_border($(basename "$VT"))" "$PY" "$S/lodgen_vt_check.py" border "$VT"
		run "vt_georef($(basename "$VT"))" "$PY" "$S/lodgen_vt_check.py" georef "$VT"
	done
	if [ -n "$LODO" ] && [ -n "$LODI" ]; then
		run native_decode "$PY" "$S/lodgen_native_decode.py" "$LODO" "$LODI"
		run native_fields "$PY" "$S/lodgen_native_fields.py" "$LODO" "$LODI"
		run native_cut    "$PY" "$S/lodgen_native_cut.py" "$LODO" "$LODI"
	fi
	[ -n "$LODB" ] && run rowkeys "$PY" "$ROOT/scratchpad/audit1_20260916/rowkeys.py" "$LODB"
	say "----- $LABEL done $(date '+%F %T')"
done
