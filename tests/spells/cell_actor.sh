#!/bin/bash
#
# PLACED ACTORS IN THE CELL VIEW (lane PLACED1, 2026-10-02; src/cellactor.cpp, docs/PRTP_PLAN.md, the PLACED1
# section).
#
# A cell places an actor as its own reference record type, and the cell view read only the ordinary references:
# no settler, no corpse, no radroach was in it. It now reads them and draws each one at rest: the race's skeleton,
# the skin and outfit models with the body-slot hiding, the pre-built face mesh, posed on the skeleton's bind pose
# at the placed transform. A leveled actor is a dice roll and is refused by name, as is a robot built from parts.
# Not judged by eye. Per camera, two windows shoot the same frame with Cell lights on and the imagespace off: no
# actor drawn (WW_CELL_ACTOR_RED=none, the reference) and the run under test, with its camera, its census and
# its per-actor dump. tests/spells/cell_actor_check.py walks the plugin and the loose meshes itself, resolves and
# poses every placed actor by the published record layouts and judges:
#   K  counts: read / drawn / not shown / refused by reason in the census against the walk
#   F  every placed actor: the walk's fate, looks record, race, sex, skeleton, position, rotation, scale
#   P  every drawn actor: the walk's models, hidden skin parts and face mesh
#   G  every drawn actor: posed bounds within 0.1 unit of the walk's, same triangle count
#   N  nothing moves on screen outside the walk's posed triangles
#   C  the actors show inside them
# Each camera names the stages it carries.
#
# RED CONTROLS (WW_CELL_ACTOR_RED on the run under test); each must FAIL, in every shot, the stages named:
#   --red none    no actor drawn                                   K, F and C
#   --red shift   every actor built 96 units off its skeleton      G and N
#   --red nohide  the outfit hides no skin part                    P (run only in the shots that carry P: a
#                 cell whose outfits are all dice, or a creature, has no hidden part to bring back)
#
# USAGE  bash tests/spells/cell_actor.sh [--red none|shift|nohide]
#        SHOTS="cell:x,y,z:view:dist[:stages] ..." to pick cameras (stages default KFPGNC); RECHECK=1 judges the
#        shots already on disk.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_actor}"
REGKEY="HKCU\Software\NifTools\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {
	wipe_scope
	reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	local gm; gm="$(mktemp)"
	python "$(dirname "$0")/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	printf '%s' "$SCOPE"
}
wipe_scope
trap wipe_scope EXIT
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
case "$RED" in ""|none|shift|nohide) ;; *) echo "unknown red control $RED"; exit 2 ;; esac
OUT="${OUT:-$REPO/scratchpad/placed1_20261002/gate_actor${RED:+_red_$RED}}"
LOG="$OUT/cell_actor.log"
PORT="${PORT:-14793}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
LIT="${LIT:-1}"
# Cameras in WORLD units, each picked with a ray test (a clear line from the eye to the actors) and judged
# green and under every red before it was written here (2026-10-02, pixels inside / outside the actors):
#   1  living residents of Vault 81 (every one hides a skin part under the outfit): 8 on screen,
#      11,151 / 526,589
#   2  the corpses of Malden Center (dead on start, drawn standing; their outfits are dice, nothing hidden:
#      no P): 10 on screen, 12,817 / 522,771
#   3  a radroach of Vault 111 (a creature through the same route; the cell's pod people hide skin parts):
#      1 on screen, 4,987 / 534,881
# A camera whose actors stand behind a wall fails C by design (the walk's triangles do not know walls).
SHOTS="${SHOTS:-Vault81:-1682,-4617,-448:3:220:KFPGNC MaldenCenter01:2728,897,-2729:4:200:KFGNC Vault111Cryo:-1640,1520,-110:3:150:KFPGNC}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_actor.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/cellview.cpp src/cellactor.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

i=0
for shot in $SHOTS; do
	IFS=: read -r cell at view dist stages <<< "$shot"
	stages="${stages:-KFPGNC}"
	i=$((i+1))
	run="$OUT/$i-$cell"
	say "== $i $cell at $at (view $view, distance $dist, stages $stages)"
	if [ "$RED" = "nohide" ] && [ "${stages#*P}" = "$stages" ]; then
		say "  --    $cell $i: carries no stage P; the nohide red is not run here"; continue
	fi
	mkdir -p "$run"
	for tag in off on; do
		[ "${RECHECK:-0}" = "1" ] && break
		rm -f "$run/$tag.png"
		# OFFFROM=<another run's OUT>: take the reference shot from there instead of shooting it again
		if [ "$tag" = "off" ] && [ -s "${OFFFROM:-}/$i-$cell/off.png" ]; then
			cp "$OFFFROM/$i-$cell/off.png" "$run/off.png"; continue
		fi
		red="$RED"
		[ "$tag" = "off" ] && red=none
		cam=""; dump=""
		if [ "$tag" = "on" ]; then
			rm -f "$run/cam.txt" "$run/on.actors.txt"
			cam="$(winpath "$run/cam.txt")"; dump="$(winpath "$run/on.actors.txt")"
		fi
		env ${red:+WW_CELL_ACTOR_RED=$red} ${cam:+WW_CELL_CAM_DUMP="$cam"} ${dump:+WW_CELL_ACTOR_DUMP="$dump"} \
			WW_RENDER_CENTER="$at" WW_RENDER_DIST="$dist" WW_RENDER_FOV=70 \
			WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT="$LIT" WW_CELL_GI=0 WW_CELL_IS=0 \
			WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$SIZE" \
			WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
			WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$run/$tag.notes" 2>&1
	done
	check "$cell $i: the two shots, the camera and the actor dump written" \
		"$([ -s "$run/off.png" ] && [ -s "$run/on.png" ] && [ -s "$run/cam.txt" ] && [ -s "$run/on.actors.txt" ] && echo 1 || echo 0)"
	python "$(dirname "$0")/cell_actor_check.py" "$ESM" "$DATA" "$cell" "$run" "$at" "$stages" >"$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	failed() { case "$stages" in *$1*) grep -q "^$1 FAIL" "$run/check.txt" ;; *) return 0 ;; esac; }
	case "$RED" in
		none) check "$cell $i: the red control FAILS stages K, F and C" "$(failed K && failed F && failed C && echo 1 || echo 0)" ;;
		shift) check "$cell $i: the red control FAILS stages G and N" "$(failed G && failed N && echo 1 || echo 0)" ;;
		nohide) check "$cell $i: the red control FAILS stage P" "$(failed P && echo 1 || echo 0)" ;;
		"") check "$cell $i: the actors pass stages $stages" "$(grep -q "^actor PASS" "$run/check.txt" && echo 1 || echo 0)" ;;
	esac
	if [ -n "$RED" ]; then
		check "$cell $i: the checker's verdict under the red control is FAIL" \
			"$(grep -q "^actor FAIL" "$run/check.txt" && echo 1 || echo 0)"
	fi
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
