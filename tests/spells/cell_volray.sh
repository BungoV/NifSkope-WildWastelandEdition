#!/bin/bash
#
# THE SPACE STAGE'S SHAPES AGAINST THE REAL TRIANGLES (lane VOLRAY1, 2026-10-27; src/probevolcheck.cpp).
# Judged by tests/spells/cell_volray_check.py.
#
#   synth   known-answer .space files (exact fit PASS, a wall poke, a missing leg, a cross-wall overlap FAIL), the
#           fitter's own output on lroom, church, thin wall, twins, house (each PASS), its mutations (each FAIL),
#           and GPU == CPU on the L room (--rays both; named SKIP without an RT device)
#   red     every red below, each run must FAIL
#   cell    one real soup: OUT= folder, SOUP= soup.psp, SPACE= .space folder or file, ARGS= extra volcheck flags
#
# RED CONTROLS (each must FAIL): known nomutate norefine nofine
#
# USAGE  bash tests/spells/cell_volray.sh [synth|red|cell]     OUT= the run folder

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
PHASE="${1:-synth}"
OUT="${OUT:-$REPO/scratchpad/volray1/gate}"
CHECK="$REPO/tests/spells/cell_volray_check.py"
REDS="known nomutate norefine nofine"
mkdir -p "$OUT"
LOG="$OUT/cell_volray_${PHASE}.log"
: > "$LOG"
say() { echo "$*" | tee -a "$LOG"; }
fails=0

case "$PHASE" in
synth)
	W="$OUT/synth"
	rm -rf "$W"
	mkdir -p "$W"
	python "$CHECK" synth "$(winpath "$EXE")" "$(winpath "$W")" 2>&1 | tee -a "$LOG"
	[ "${PIPESTATUS[0]}" = 0 ] || fails=$((fails + 1))
	;;
red)
	for r in $REDS; do
		W="$OUT/synth_red_$r"
		rm -rf "$W"
		mkdir -p "$W"
		v=$(python "$CHECK" synth "$(winpath "$EXE")" "$(winpath "$W")" --red "$r" 2>&1 | tee "$OUT/cell_volray_red_$r.log" | grep '^VERDICT')
		if echo "$v" | grep -q 'VERDICT FAIL'; then
			say "red $r: FAILS as it must"
		else
			say "red $r: DID NOT FAIL ($v)"
			fails=$((fails + 1))
		fi
	done
	;;
cell)
	"$EXE" -no-gui volcheck --soup "$(winpath "$SOUP")" --space "$(winpath "$SPACE")" --out "$(winpath "$OUT")" \
		--png "$(winpath "$OUT/volcheck.png")" ${ARGS:-} 2>&1 | tee -a "$LOG"
	[ "${PIPESTATUS[0]}" = 0 ] || fails=$((fails + 1))
	;;
*) say "unknown phase $PHASE"; exit 2 ;;
esac

if [ "$fails" = 0 ]; then say "CELL_VOLRAY $PHASE: PASS"; else say "CELL_VOLRAY $PHASE: FAIL ($fails)"; fi
[ "$fails" = 0 ]
