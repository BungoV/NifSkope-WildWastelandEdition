#!/bin/bash
#
# THE CELL'S IMAGESPACE (lane IMGS1, 2026-10-01; src/gl/celllights.h, docs/PRTP_PLAN.md 2j).
#
# Not judged by eye. Per interior, one window with Cell lights + Imagespace on shoots the picture (on.png)
# and dumps the measure pass at full size (WW_CELL_IS_DUMP: the raw linear frame + the echo). Then
# tests/spells/cell_is_check.py walks Fallout4.esm ITSELF (CELL XCIM -> IMGS), reads the LUT out of the
# Misc archive itself and runs the game's chain in numpy over the dump:
#   A  the adapted luminance and the exposure NifSkope used = the mean luma of the dump over the pixels a
#      cell-lit fragment reached (stencil bit 0, written after the floats), the HNAM clamp
#   B  the bloom (lane BLOOM1) NifSkope echoed = the checker's own from the dump (4x4 box, bright pass,
#      15-tap Gaussian vertical then horizontal)
#   P  the picture = the chain over the dump + the bloom on opaque cell-lit pixels (stencil == 1): >= 97%
#      inside the 3x3 range of the rebuild +-3/255 (the shot is antialiased, the dump is not), >= 5000 px
# The cameras stand close (VIEW 5, DIST 350): a far view leaves the cell a few percent of the frame.
#
# RED CONTROLS (each must FAIL stage P):  --red nolut    the LUT skipped
#                                         --red noexp    exposure 1
#                                         --red nograde  the cinematic grade skipped (grade cells only)
#                                         --red nobloom  the bloom not added (cells whose bloom reach
#                                                        is >= 5% of the compared pixels only)
#
# USAGE  bash tests/spells/cell_is.sh [--red nolut|noexp|nograde|nobloom]
#        CELLS="..." to pick interiors; the camera stands at CAM_<cell> (x,y,z look-at) if set.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_is}"
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
OUT="${OUT:-$REPO/scratchpad/imgs1_20261001/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_is.log"
PORT="${PORT:-14746}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
CELLS="${CELLS:-Vault111Cryo DmndSolomonsHouse01 GoodneighborTheThirdRail}"
GRADED="${GRADED:-GoodneighborTheThirdRail}"   # cells whose IMGS grade is not identity (CNAM/TNAM)
: "${CAM_Vault111Cryo:=-4600,-280,120}" "${CAM_DmndSolomonsHouse01:=1450,-20,150}" "${CAM_GoodneighborTheThirdRail:=2932,-636,100}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_is.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/gl/celllights.cpp src/gl/celllights.h src/cellview.cpp src/esmdata.cpp src/glview.cpp src/gl/renderer.cpp \
	res/shaders/cell_lights.glsl res/shaders/fo4_default.frag res/shaders/pbrm_default.frag; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

for cell in $CELLS; do
	say "== $cell"
	run="$OUT/$cell"
	mkdir -p "$run"
	rm -f "$run/on.png" "$run/on.hdr" "$run/on.hdr.txt" "$run/on.notes"
	camvar="CAM_$cell"; cam=()
	[ -n "${!camvar:-}" ] && cam=( WW_RENDER_CENTER="${!camvar}" WW_RENDER_DIST="${DIST:-350}" WW_RENDER_FOV=70 )
	redenv=(); [ -n "$RED" ] && redenv=( WW_CELL_IS_RED="$RED" )
	env "${redenv[@]}" "${cam[@]}" \
		WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 WW_CELL_GI=0 \
		WW_CELL_IS=1 WW_CELL_IS_DUMP="$(winpath "$run/on.hdr")" \
		WW_RENDER_SHOT="$(winpath "$run/on.png")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="${VIEW:-5}" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$run/on.notes" 2>&1
	check "$cell: the picture and the dump written" \
		"$([ -s "$run/on.png" ] && [ -s "$run/on.hdr" ] && [ -s "$run/on.hdr.txt" ] && echo 1 || echo 0)"
	sed 's/^/  echo /' "$run/on.hdr.txt" 2>/dev/null | tee -a "$LOG"
	python "$(dirname "$0")/cell_is_check.py" "$ESM" "$cell" "$run" > "$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	if [ "$RED" = nograde ] && ! echo " $GRADED " | grep -q " $cell "; then
		say "  skip  $cell: identity grade, the nograde red has nothing to remove"
	elif [ "$RED" = nobloom ] && ! awk '/^bloom reach/ { exit !($3 + 0 >= 5) }' "$run/check.txt"; then
		say "  skip  $cell: the bloom moves under 5% of the compared pixels, the nobloom red has little to remove"
	elif [ -n "$RED" ]; then
		check "$cell: the red control FAILS stage P" "$(grep -q "^P FAIL" "$run/check.txt" && echo 1 || echo 0)"
	else
		check "$cell: the exposure and the picture match the independent rebuild" \
			"$(grep -q "^imagespace PASS" "$run/check.txt" && echo 1 || echo 0)"
	fi
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
