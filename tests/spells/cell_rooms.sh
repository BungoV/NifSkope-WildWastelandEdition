#!/bin/bash
#
# ROOMS, LOAD DOORS AND THE OUTSIDE OF THE SHELL (lane ROOMCLAMP1, 2026-10-03; src/proberooms.h, src/probebake.h,
# docs/PRTP_PLAN.md 2am). Judged by tests/spells/cell_rooms_check.py and cell_gi_check.py, not by eye.
#
#   synth   seven scenes built by the checker (lane ALPHATEST2 adds a chain-link fence hall and a faded wall;
#           before them:) (two rooms behind a 2-unit wall, an L room beside two closets that
#           touch along an edge, an A-frame nave, a round two-floor tower with a hatch, glass between two rooms
#           and to the outdoors), each through `NifSkope -no-gui probegi` (place, bake, relight, dump): the labels
#           against the scene's own rooms, the panes and the hatch naming both sides, and the shader's blend
#           (cellGiRoomSample, redone in Python) on the dark side of the 2-unit wall no brighter than the dark
#           room's own probes, no sample without weight
#   cells   ConcordMuseum01, Vault111Cryo, OldNorthChurch01 (pitched roofs), PrydwenHull01 (a round hull), each
#           through cell_sky.sh's skyint phase (bake + relight + dump + its own check), then: the load doors
#           solid (Museum: from CAPTURE1's cube probe no ray escapes through a load door), no probe on the back
#           of a one-sided face or on a roof top, the back-face shares re-traced; the GI grid's two slots
#           re-gathered (cell_gi_check stages B C: each slot only its room's probes)
#           and (stage H) on the dump's own surfels the rooms uncover nothing the plain grid covers
#
# RED CONTROLS (each must FAIL):
#   synth  --red noclamp    one value a voxel (WW_CELL_GI_RED=noclamp): the dark side reads the lit room
#          --red conn26     the floods 26-connected: the two closets are one room
#          --red boxes      each room its bounding box: the L's notch and the nave's eaves indoors
#          --red glasswall  glass names no room: the panes name nothing
#          --red nomask     lane ALPHATEST2: alpha-tested walls solid all over: the fence splits A|B, fade too
#          --red noscale    lane ALPHATEST2: the soup without its alpha scales: the faded wall splits A|B
#   cells  --red open       load doors as openings: rays escape through the Museum's exit door
#          --red backface   the bake keeps probes whose rays mostly meet backs (WW_PROBE_BAKE_RED=backface)
#          --red backmax    the bar at 1 (WW_CELL_PROBE_BACKMAX=1): the same
#          --red floor      the placer stands probes on backs (WW_PROBE_RED=floor)
#          --red emptyslot  the shader's twin weighs a room's empty slots (stage H: pipes black, magenta)
#
# USAGE  bash tests/spells/cell_rooms.sh [synth|cells] [--red <name>]     OUT= the run folder; CELLS= to pick
#        cells ("museum:ConcordMuseum01 cryo:Vault111Cryo ...")

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
PHASE="${1:-synth}"
RED=""
[ "${2:-}" = "--red" ] && RED="${3:-}"
OUT="${OUT:-$REPO/scratchpad/roomclamp1_20261003/gate}"
CELLS="${CELLS:-museum:ConcordMuseum01 cryo:Vault111Cryo church:OldNorthChurch01 prydwen:PrydwenHull01}"
MUSEUM_EYE="144.5,-9.6,48.6"     # CAPTURE1's Museum cube probe
CHECK="$REPO/tests/spells/cell_rooms_check.py"
mkdir -p "$OUT"
LOG="$OUT/cell_rooms_${PHASE}${RED:+_red_$RED}.log"
: > "$LOG"
say() { echo "$*" | tee -a "$LOG"; }
fails=0

case "$PHASE" in
synth)
	W="$OUT/synth${RED:+_red_$RED}"
	mkdir -p "$W"
	python "$CHECK" synth "$(winpath "$EXE")" "$(winpath "$W")" ${RED:+--red "$RED"} 2>&1 | tee -a "$LOG"
	[ "${PIPESTATUS[0]}" = 0 ] || fails=$((fails + 1))
	;;
cells)
	env=()
	case "$RED" in
		"") ;;
		open) env=( WW_CELL_PROBE_LOADDOOR_RED=open ) ;;
		backface) env=( WW_PROBE_BAKE_RED=backface ) ;;
		backmax) env=( WW_CELL_PROBE_BACKMAX=1 ) ;;
		floor) env=( WW_PROBE_RED=floor ) ;;
		emptyslot) env=() ;;   # lane ROOMCLAMP1: the checker's twin weighs empty slots (stage H)
		*) say "unknown red $RED"; exit 2 ;;
	esac
	R="$OUT/cells${RED:+_red_$RED}"
	for spec in $CELLS; do
		tag="${spec%%:*}" cell="${spec#*:}"
		d="$R/skyint/$tag"
		mkdir -p "$d"
		say "== $cell ($tag)"
		# the bake's back-face list and the soup's refs beside the run (cell_rooms_check reads both)
		env "${env[@]}" WW_CELL_PROBE_BACKDUMP="$(winpath "$d")\\back.tsv" WW_CELL_PROBE_SOUP_REFS="$(winpath "$d")\\refs.tsv" \
			OUT="$R" CELLS="" SKYINT="$spec" PHASE="skyint check" SCOPE="cell_rooms_$tag" \
			bash "$REPO/tests/spells/cell_sky.sh" > "$R/$tag.sky.log" 2>&1
		grep -h "^  rooms: grid\|^  gi rooms:" "$d/lit.notes" 2>/dev/null | tee -a "$LOG"
		if ! grep -q "PASS  $cell" "$R/$tag.sky.log"; then
			say "  FAIL cell_sky skyint ($R/$tag.sky.log)"
			fails=$((fails + 1))
			continue
		fi
		if [ "$tag" = museum ]; then
			python "$CHECK" doors "$ESM" "$d" --cell "$cell" --eye "$MUSEUM_EYE" 2>&1 | sed 's/^/  /' | tee -a "$LOG"
			[ "${PIPESTATUS[0]}" = 0 ] || fails=$((fails + 1))
		fi
		python "$CHECK" back "$d" --sample 16 2>&1 | sed 's/^/  /' | tee -a "$LOG"
		[ "${PIPESTATUS[0]}" = 0 ] || fails=$((fails + 1))
		python "$REPO/tests/spells/cell_gi_check.py" "$ESM" "$cell" "$d" BC 2>&1 | sed 's/^/  /' | tee -a "$LOG"
		[ "${PIPESTATUS[0]}" = 0 ] || fails=$((fails + 1))
		python "$CHECK" holes "$d" ${RED:+--red $RED} 2>&1 | sed 's/^/  /' | tee -a "$LOG"
		[ "${PIPESTATUS[0]}" = 0 ] || fails=$((fails + 1))
	done
	;;
*)
	echo "usage: bash tests/spells/cell_rooms.sh [synth|cells] [--red <name>]"
	exit 2
	;;
esac

if [ "$fails" = 0 ]; then
	say "cell_rooms $PHASE${RED:+ (red $RED)}: PASS"
	exit 0
fi
say "cell_rooms $PHASE${RED:+ (red $RED)}: FAIL ($fails)"
exit 1
