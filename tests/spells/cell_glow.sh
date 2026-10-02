#!/bin/bash
#
# THE CAMERA-FACING GLOW CARDS (lane GLOW1, 2026-10-01; src/cellview.cpp "billboards", docs/PRTP_PLAN.md, the
# GLOW1 section).
#
# A GlowFillCloudy card sits on each of the Vault's pod bases under an NiBillboardNode: the game turns it to the
# camera. The cell view welded it flat, edge-on to the eye, and its haze was gone.
# Not judged by eye. Per camera, two windows shoot the same frame with Cell lights on and the imagespace off (its
# exposure would move every pixel): every billboard welded flat (WW_CELL_GLOW_RED=1, the reference) and the run
# under test, plus the camera dump. tests/spells/cell_glow_check.py predicts each card's circle on screen from its
# own walk of the plugin and the meshes, and judges:
#   K  the census's count of turned shapes equals the walk's
#   N  nothing moves outside the predicted circles
#   C  the cards show inside them (>= 3 cards brighten their circle by a mean >= 0.5/255; the material is a
#      faint haze by design, a few levels, so the bar is set to it and above the render's own noise)
# Each camera names the stages it carries. From the walkway's start the cards are far enough to show (C) and
# their circles cover the whole frame, so nothing is left for N. Beside the third pod pair there is room for N,
# and the same cards add under 0.5/255 (measured 2026-10-02: +0.03 to +0.41), so that camera does not carry C.
#
# RED CONTROL:  --red flat   the run under test welds flat too (WW_CELL_GLOW_RED=1; must FAIL stage K in every
#                            shot and stage C in every shot that carries it)
#
# USAGE  bash tests/spells/cell_glow.sh [--red flat]
#        SHOTS="cell:x,y,z:view:dist[:stages] ..." to pick cameras (stages default KNC); RECHECK=1 judges the
#        shots already on disk.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_glow}"
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
case "$RED" in ""|flat) ;; *) echo "unknown red control $RED"; exit 2 ;; esac
OUT="${OUT:-$REPO/scratchpad/glow1_20261001/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_glow.log"
PORT="${PORT:-14762}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
# the cryo walkway at eye height looking down the pod row, from its start and from beside the third pod pair
SHOTS="${SHOTS:-Vault111Cryo:350,-512,40:4:450:KC Vault111Cryo:1100,-512,40:4:700:KN}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_glow.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/cellview.cpp src/lodgen.cpp src/nativeemit.h src/gl/glnode.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

i=0
for shot in $SHOTS; do
	IFS=: read -r cell at view dist stages <<< "$shot"
	stages="${stages:-KNC}"
	i=$((i+1))
	run="$OUT/$i-$cell"
	say "== $i $cell at $at (view $view, distance $dist, stages $stages)"
	mkdir -p "$run"
	for tag in flat on; do
		[ "${RECHECK:-0}" = "1" ] && break
		rm -f "$run/$tag.png"
		red=""
		{ [ "$tag" = "flat" ] || [ -n "$RED" ]; } && red=1
		dump=""
		[ "$tag" = "on" ] && { rm -f "$run/cam.txt"; dump="$(winpath "$run/cam.txt")"; }
		env ${red:+WW_CELL_GLOW_RED=$red} ${dump:+WW_CELL_CAM_DUMP="$dump"} \
			WW_RENDER_CENTER="$at" WW_RENDER_DIST="$dist" WW_RENDER_FOV=70 \
			WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 \
			WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$SIZE" \
			WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
			WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$run/$tag.notes" 2>&1
	done
	check "$cell $i: the two shots and the camera written" \
		"$([ -s "$run/flat.png" ] && [ -s "$run/on.png" ] && [ -s "$run/cam.txt" ] && echo 1 || echo 0)"
	python "$(dirname "$0")/cell_glow_check.py" "$ESM" "$DATA" "$cell" "$run" "$at" "$stages" >"$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	case "$RED" in
		flat) redc=1
			case "$stages" in *C*) grep -q "^C FAIL" "$run/check.txt" || redc=0 ;; esac
			check "$cell $i: the red control FAILS stage K, and stage C where the shot carries it" \
				"$(grep -q "^K FAIL" "$run/check.txt" && [ "$redc" = "1" ] && echo 1 || echo 0)" ;;
		"") check "$cell $i: the cards pass stages $stages" "$(grep -q "^glow PASS" "$run/check.txt" && echo 1 || echo 0)" ;;
	esac
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
