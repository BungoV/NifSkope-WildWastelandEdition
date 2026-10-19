#!/bin/bash
#
# THE SPACE STAGE: ROOMS AS SHAPES, THRESHOLDS, ITS RECORD (lane VOLFIT1, 2026-10-19; src/probevolumes.h,
# src/gl/cellvolumeview.h). Judged by tests/spells/cell_volumes_check.py, not by eye.
#
#   synth   eleven scenes built by the checker (house, twins, L room, church, silo, rock, canopy, deck, lean-to,
#           carport, closet) through `NifSkope -no-gui volumes`: rooms against the scene's own rooms, outdoor and
#           in-rock points in no room, coverage re-counted on the written grid, the door / window thresholds with
#           their evidence and outward normals, shape counts per scene, kept ids, the recx hand shapes, the stage
#           record (skipped clean; rebuilt for input soup, identity, output), two exterior cells, the debug
#           view's geometry counts
#   red     every red below, each run must FAIL
#
# RED CONTROLS (each must FAIL):
#   nosky nowall noopen noback nofloor nogrow nofold nomerge onebox boxonly noglass nowedge noevidence noids norecx
#   fpsoup noskip
#
# USAGE  bash tests/spells/cell_volumes.sh [synth|red] [--red <name>]     OUT= the run folder

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
PHASE="${1:-synth}"
RED=""
[ "${2:-}" = "--red" ] && RED="${3:-}"
OUT="${OUT:-$REPO/scratchpad/volfit1/gate}"
CHECK="$REPO/tests/spells/cell_volumes_check.py"
REDS="nosky nowall noopen noback nofloor nogrow nofold nomerge onebox boxonly noglass nowedge noevidence noids norecx fpsoup noskip"
mkdir -p "$OUT"
LOG="$OUT/cell_volumes_${PHASE}${RED:+_red_$RED}.log"
: > "$LOG"
say() { echo "$*" | tee -a "$LOG"; }
fails=0

case "$PHASE" in
synth)
	W="$OUT/synth${RED:+_red_$RED}"
	rm -rf "$W"
	mkdir -p "$W"
	python "$CHECK" synth "$(winpath "$EXE")" "$(winpath "$W")" ${RED:+--red "$RED"} 2>&1 | tee -a "$LOG"
	[ "${PIPESTATUS[0]}" = 0 ] || fails=$((fails + 1))
	;;
red)
	# every red must fail; a red that passes is a gate that cannot see its rule
	for r in $REDS; do
		W="$OUT/synth_red_$r"
		rm -rf "$W"
		mkdir -p "$W"
		v=$(python "$CHECK" synth "$(winpath "$EXE")" "$(winpath "$W")" --red "$r" 2>&1 | tee "$OUT/cell_volumes_red_$r.log" | grep '^VERDICT')
		if echo "$v" | grep -q 'VERDICT FAIL'; then
			say "red $r: FAILS as it must"
		else
			say "red $r: DID NOT FAIL ($v)"
			fails=$((fails + 1))
		fi
	done
	;;
*) say "unknown phase $PHASE"; exit 2 ;;
esac

if [ "$fails" = 0 ]; then say "CELL_VOLUMES $PHASE${RED:+ red $RED}: PASS"; else say "CELL_VOLUMES $PHASE${RED:+ red $RED}: FAIL ($fails)"; fi
[ "$fails" = 0 ]
