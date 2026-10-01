#!/bin/bash
#
# THE CELL VIEW LIT BY THE CELL'S OWN LIGHTS (lane PRTP3, 2026-10-01; src/gl/celllights.h).
#
# Not judged by eye. Per interior, one window per pass:
#   lit      the Cell lights row on: the picture bungo looks at
#   unlit    the row off: the picture the cell view always drew (and the program swap never fires)
#   probe 1  every cell-lit fragment writes the placed lights' irradiance / 4, raw
#   probe 2  ... its world position's high bytes, 3 its low bytes (16 bits an axis)
#   probe 4  ... its world normal
# Then tests/spells/cell_lit_check.py rebuilds each sampled pixel's irradiance from ITS OWN walk of
# Fallout4.esm (the lights' DATA, FNAM, XRDS, XLIG and rotation; no NifSkope code) with the
# docs/PRTP2_LIGHT_MODEL.md formulas, and compares.
#
# RED CONTROLS (each must FAIL):  --red linear   the radial curve without its 2.2
#                                 --red axis     spots aimed along local -Z
#                                 --red off      the row off: no probe is served at all
#                                 --red ambientlit  the Ambient Only lights drawn as ordinary lights
#                                 --red hemiomni    hemisphere and box lights drawn as plain omni lights
#                                                   (lane HEMI1; fails on the pixels the shapes decide)
#
# USAGE  bash tests/spells/cell_lit.sh [--red linear|axis|off|ambientlit|hemiomni]
#        CELLS="..." to pick interiors; the camera stands at CAM_<cell> (x,y,z look-at) if set,
#        DIST_<cell> / VIEW_<cell> (lane HEMI1) override DIST / VIEW for that cell.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_lit}"
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
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
OUT="${OUT:-$REPO/scratchpad/prtp3_20261001/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_lit.log"
PORT="${PORT:-14741}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
CELLS="${CELLS:-Vault111Cryo DmndSolomonsHouse01 DmndRadio01}"
# looking down on light clusters (the whole-cell framing leaves too few pixels); the Vault's west end
# holds its big aimed spots, so --red axis has something to break
: "${CAM_Vault111Cryo:=-4600,-280,0}" "${CAM_DmndSolomonsHouse01:=1450,-20,150}"
# lane HEMI1: the radio booth's two hemisphere lamps (refs 00139F49, 00187B03) face down; a level look
# across the booth from inside shows the walls above their plane, which the half space leaves dark
: "${CAM_DmndRadio01:=1617,99,230}" "${VIEW_DmndRadio01:=4}" "${DIST_DmndRadio01:=250}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_lit.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/gl/celllights.cpp src/cellview.cpp src/gl/renderer.cpp src/esmdata.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

shoot() {   # shoot <cell> <tag> <env...>
	local cell="$1" tag="$2"; shift 2
	local shot="$OUT/$cell.$tag.png" notes="$OUT/$cell.$tag.notes"
	rm -f "$shot" "$notes"
	local camvar="CAM_$cell" distvar="DIST_$cell" viewvar="VIEW_$cell" cam=()
	[ -n "${!camvar:-}" ] && cam=( WW_RENDER_CENTER="${!camvar}" WW_RENDER_DIST="${!distvar:-${DIST:-1400}}" WW_RENDER_FOV=70 )
	local view="${!viewvar:-${VIEW:-1}}"
	# WW_CELL_SHADOW=0: the PRTP2 evaluation is unshadowed (tests/spells/cell_shadow.sh judges the shadows)
	env WW_CELL_SHADOW=0 "$@" "${cam[@]}" \
		WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
	[ -s "$shot" ] && echo 1 || echo 0
}

for cell in $CELLS; do
	say "== $cell"
	lit=1; [ "$RED" = "off" ] && lit=0
	redenv=(); [ -n "$RED" ] && [ "$RED" != "off" ] && redenv=( WW_CELL_LIT_RED="$RED" )
	ok=1
	[ "$(shoot "$cell" lit WW_CELL_LIT=$lit "${redenv[@]}")" = 1 ] || ok=0
	[ "$(shoot "$cell" unlit WW_CELL_LIT=0)" = 1 ] || ok=0
	for p in 1 2 3 4; do
		[ "$(shoot "$cell" probe$p WW_CELL_LIT=$lit WW_CELL_LIT_PROBE=$p "${redenv[@]}")" = 1 ] || ok=0
	done
	check "$cell: six pictures written" "$ok"
	grep -h "cell lighting:" "$OUT/$cell.lit.notes" | head -1 | tee -a "$LOG"
	line="$(python "$(dirname "$0")/cell_lit_check.py" "$ESM" "$cell" "$OUT" 2>&1 | tail -1)"; rc=$?
	say "  $line"
	if [ "$RED" = "axis" ] && [ "${line#*no spot-lit pixels}" != "$line" ]; then
		say "  skip  $cell: no spot-lit pixels in frame, the axis red has nothing to break"
	elif [ "$RED" = "hemiomni" ] && [ "${line#*too few shape-decided pixels}" != "$line" ]; then
		say "  skip  $cell: no hemisphere or box light decides a pixel in frame, the hemiomni red has nothing to break"
	elif [ -n "$RED" ]; then
		check "$cell: the red control FAILS the check" "$([ "${line#*FAIL}" != "$line" ] && echo 1 || echo 0)"
	else
		check "$cell: the probes match the independent PRTP2 evaluation" "$([ "${line#*PASS}" != "$line" ] && echo 1 || echo 0)"
	fi
	# lane HEMI1: the hemisphere view must keep its hemisphere-decided pixels (a reframing cannot hide them)
	if [ "$cell" = "DmndRadio01" ] && [ -z "$RED" ]; then
		nh="$(printf '%s' "$line" | sed -n 's/.*(\([0-9]*\) by a hemisphere).*/\1/p')"
		check "$cell: the frame holds 200+ pixels a hemisphere's plane decides (${nh:-0})" "$([ "${nh:-0}" -ge 200 ] && echo 1 || echo 0)"
	fi
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
