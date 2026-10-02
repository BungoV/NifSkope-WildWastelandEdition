#!/bin/bash
#
# NO SURFACE DRAWS OVER THE HAZE IN FRONT OF IT (lane FXD1, 2026-10-02; src/gl/glscene.cpp drawDeferredShapes,
# src/gl/glshape.cpp wwDecalDrawsFirst, docs/PRTP_PLAN.md).
#
# bungo's Vault 111 cryo walkway: small dark marks stood out of the haze at the far door. They were the wall's
# blended decals (vent slits, stencilled labels), drawn after the haze cards and so over them.
#
# Not judged by eye. Per camera, four windows shoot the same frame with Cell lights on and the imagespace off:
# the effects hidden (WW_CELL_FX_RED=hide, the surfaces alone), the run under test, and the two position
# probes (WW_CELL_LIT_PROBE=2,3). tests/spells/cell_fxdepth_check.py judges them:
#   R  the gate has a subject (enough dark-side surface pixels with haze over their peers)
#   D  no dark holes: a surface pixel keeps at least a share of the lift its peers at the same distance took
#
# RED CONTROL:  --red late   WW_CELL_FXD_RED=late, the decals in the old order (must FAIL stage D with at least
#                            REDMIN hole pixels per camera)
#
# The bar is 0 hole pixels per camera. Measured on these two cameras before the gate first ran: the old order
# holds 74 and 43 hole pixels (so REDMIN = 20, under half the smaller), the fix none.
#
# USAGE  bash tests/spells/cell_fxdepth.sh [--red late]
#        SHOTS="cell:x,y,z:view:dist:bar ..." to pick cameras; RECHECK=1 judges the shots already on disk.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_fxdepth}"
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
OUT="${OUT:-$REPO/scratchpad/fxd1_20261002/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_fxdepth.log"
PORT="${PORT:-14752}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-1600x1000}"
# bungo's eye-height look down the cryo walkway at the far door, and the effects gate's walkway camera
SHOTS="${SHOTS:-Vault111Cryo:384,-480,60:3:260:0 Vault111Cryo:300,-512,40:3:500:0}"
REDMIN="${REDMIN:-20}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_fxdepth.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
case "$RED" in ""|late) ;; *) say "  unknown red control $RED"; exit 2 ;; esac
newer=1
for s in src/gl/glscene.cpp src/gl/glshape.cpp src/gl/glshape.h src/gl/glnode.cpp src/gl/renderer.cpp \
	src/gl/celllights.cpp src/cellview.cpp res/shaders/fo4_effectshader.frag; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

i=0
for shot in $SHOTS; do
	IFS=: read -r cell at view dist bar <<< "$shot"
	i=$((i+1))
	run="$OUT/$i-$cell"
	say "== $i $cell at $at (view $view, distance $dist)"
	mkdir -p "$run"
	for tag in hide on p2 p3; do
		[ "${RECHECK:-0}" = "1" ] && break
		rm -f "$run/$tag.png"
		extra=()
		case "$tag" in
			hide) extra=( WW_CELL_FX_RED=hide ) ;;
			on) [ "$RED" = "late" ] && extra=( WW_CELL_FXD_RED=late ) ;;
			p2) extra=( WW_CELL_LIT_PROBE=2 "WW_CELL_CAM_DUMP=$(winpath "$run/cam.txt")" ) ;;
			p3) extra=( WW_CELL_LIT_PROBE=3 ) ;;
		esac
		env "${extra[@]}" WW_RENDER_CENTER="$at" WW_RENDER_DIST="$dist" WW_RENDER_FOV=70 \
			WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 \
			WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$SIZE" \
			WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
			WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$run/$tag.notes" 2>&1
	done
	check "$cell $i: the four shots and the camera written" \
		"$([ -s "$run/hide.png" ] && [ -s "$run/on.png" ] && [ -s "$run/p2.png" ] && [ -s "$run/p3.png" ] && [ -s "$run/cam.txt" ] && echo 1 || echo 0)"
	python "$(dirname "$0")/cell_fxdepth_check.py" "$run" "${bar:-0}" "$run/holes.png" >"$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	case "$RED" in
		late) n="$(sed -n 's/^D FAIL  \([0-9]*\) hole.*/\1/p' "$run/check.txt")"
			check "$cell $i: the red control FAILS stage D with >= $REDMIN hole pixels (${n:-none})" "$([ "${n:-0}" -ge "$REDMIN" ] && echo 1 || echo 0)"
			check "$cell $i: the red control still has a subject (R)" "$(grep -q "^R PASS" "$run/check.txt" && echo 1 || echo 0)" ;;
		"") check "$cell $i: no surface draws over the haze (R and D)" "$(grep -q "^fxdepth PASS" "$run/check.txt" && echo 1 || echo 0)" ;;
	esac
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
