#!/bin/bash
#
# THE CELL-LIT EFFECTS (lane EFX2, 2026-10-01; src/gl/celllights.h wwCellFxRed, docs/PRTP_PLAN.md 2q).
#
# Not judged by eye. Per camera, four windows shoot the same frame with Cell lights on and the imagespace off
# (its exposure would move every pixel with the effects): the effects hidden (WW_CELL_FX_RED=hide, the
# reference), the run under test, the viewer's effect shader (legacy) and the Soft fades held at 1 (nosoft).
# tests/spells/cell_fx_check.py judges them:
#   N  nothing but the effects moves
#   S  the Soft fades only take away, and take away somewhere
#   H  the haze drops: the effects' mean change over the frame <= the camera's limit x the viewer shader's
#      (measured 2026-10-01: 0.39 on the walkway, 0.86 at the far end; limits 0.6 and 0.95)
#
# RED CONTROLS:  --red legacy   the viewer's effect shader (must FAIL stage H)
#                --red nosoft   the fades held at 1 (must FAIL stage S)
#
# USAGE  bash tests/spells/cell_fx.sh [--red legacy|nosoft]
#        SHOTS="cell:x,y,z:view:dist:hmax ..." to pick cameras; RECHECK=1 judges the shots already on disk.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_fx}"
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
OUT="${OUT:-$REPO/scratchpad/efx2_20261001/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_fx.log"
PORT="${PORT:-14746}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
# the Vault's cryo walkway (the ground steam, bungo's side-by-side) and its far end
SHOTS="${SHOTS:-Vault111Cryo:300,-512,40:3:500:0.6 Vault111Cryo:-4600,-280,120:5:350:0.95}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_fx.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/gl/celllights.cpp src/gl/celllights.h src/gl/renderer.cpp src/gl/glscene.cpp src/gl/glproperty.cpp \
	res/shaders/cell_lights.glsl res/shaders/fo4_effectshader.frag res/shaders/fo4_effectcell.frag; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

i=0
for shot in $SHOTS; do
	IFS=: read -r cell at view dist hmax <<< "$shot"
	i=$((i+1))
	run="$OUT/$i-$cell"
	say "== $i $cell at $at (view $view, distance $dist)"
	mkdir -p "$run"
	for tag in hide on legacy nosoft; do
		[ "${RECHECK:-0}" = "1" ] && break
		rm -f "$run/$tag.png"
		red=""
		case "$tag" in hide|legacy|nosoft) red="$tag" ;; on) red="$RED" ;; esac
		env ${red:+WW_CELL_FX_RED=$red} WW_RENDER_CENTER="$at" WW_RENDER_DIST="$dist" WW_RENDER_FOV=70 \
			WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 \
			WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$SIZE" \
			WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
			WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$run/$tag.notes" 2>&1
	done
	check "$cell $i: the four shots written" \
		"$([ -s "$run/hide.png" ] && [ -s "$run/on.png" ] && [ -s "$run/legacy.png" ] && [ -s "$run/nosoft.png" ] && echo 1 || echo 0)"
	python "$(dirname "$0")/cell_fx_check.py" "$run" "${hmax:-0.8}" >"$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	case "$RED" in
		legacy) check "$cell $i: the red control FAILS stage H" "$(grep -q "^H FAIL" "$run/check.txt" && echo 1 || echo 0)" ;;
		nosoft) check "$cell $i: the red control FAILS stage S" "$(grep -q "^S FAIL" "$run/check.txt" && echo 1 || echo 0)" ;;
		"") check "$cell $i: the effects pass N, S and H" "$(grep -q "^effects PASS" "$run/check.txt" && echo 1 || echo 0)" ;;
		*) say "  unknown red control $RED"; fails=$((fails+1)) ;;
	esac
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
