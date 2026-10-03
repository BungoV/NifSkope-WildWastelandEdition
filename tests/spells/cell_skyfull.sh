#!/bin/bash
#
# THE WHOLE SKY IN THE LOOKDEV PREVIEW (lane SKYFULL1, 2026-10-03; src/gl/lookdevstage.cpp).
#
# bungo: the preview kept showing the low-res cube. The Sky, Sun, Clouds and Moon rows now ship ON; the cube is
# the reflection source and the NAMED fallback when the dome refuses; the stars (the game's stars shape, the
# weather's Stars row, the clock's stars alpha, the GMST turn) draw on the dome; an interior draws the sky only
# with Show Sky (CELL DATA bit 7).
#
# Not judged by eye. Shots (Lookdev, CommonwealthClear, game day 4, eye height, tilted up by WW_RENDER_PITCH):
#   <cell>_<h>_after   no row pins: the shipped defaults            (h = 12 day, 19 dusk, 23 night)
#   <cell>_<h>_cube    all four rows pinned OFF: the old picture
#   <cell>_23_nostars  WW_LOOKDEV_RED=nostars: the same night without the stars (the N comparator)
#   pin0_/rung_<cell>  no pitch, noon: this exe pinned OFF vs the exe before the lane (BEFORE=) unpinned
#   int_cryo/museum    Vault111Cryo (closed) and ConcordMuseum01 (Show Sky), no pins
# Then tests/spells/cell_skyfull_check.py (stages S B O T N I; its header says what each is).
#
# RED CONTROLS (each must FAIL):
#   --red off          the "after" shots pin all four rows OFF (the cube returns)            S B
#   --red nodome       WW_LOOKDEV_RED=nodome: the dome's file never resolves (cube fallback) S B
#   --red nostars      WW_LOOKDEV_RED=nostars: the stars go missing without a word         S T N
#   --red interiorsky  WW_LOOKDEV_RED=interiorsky: the closed vault draws the sky            I
#
# USAGE  bash tests/spells/cell_skyfull.sh [--red off|nodome|nostars|interiorsky]
#        CELLS="sanctuary concord" HOURS="12 19 23" RECHECK=1 (judge the files already shot)
# Run under the nifskope lock (withlock.sh nifskope ...).

set -u
. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
BEFORE="${BEFORE:-$REPO/release/NifSkope.before_skyfull1.exe}"
SCOPE="${SCOPE:-cell_skyfull}"
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
trap wipe_scope EXIT
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
OUT="${OUT:-$REPO/scratchpad/skyfull1_20261003/gate${RED:+_red_$RED}}"
PORT="${PORT:-14881}"
SIZE="${SIZE:-960x600}"
CELLS="${CELLS:-sanctuary concord}"
HOURS="${HOURS:-12 19 23}"
RECHECK="${RECHECK:-0}"
# dist 20 (the eye sits 2 x dist = 40 from the look-at): at 400 the eye stood 800 out, inside Concord's houses and
# under the Sanctuary hill, and the frames showed no sky. The cell, its look-at (Sanctuary above the roofs: LAND z ~7816 + ~600; Concord the street, z ~6200 + 150), per hour the view and the tilt (the sun/moon in frame:
# noon sun 56 deg up in the south, dusk sun on the west horizon, 23:00 the moon 46 deg up south-west)
declare -A XY=( [sanctuary]="-19,22" [concord]="-15,17" )
declare -A AT=( [sanctuary]="-75776,92160,8400" [concord]="-60200,73500,6350" )
declare -A VIEW=( [12]=5 [19]=3 [23]=5 )
declare -A PITCH=( [12]=25 [19]=12 [23]=25 )
PINOFF=( WW_LOOKDEV_SKY=0 WW_LOOKDEV_SUN=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 )
AFTER=()
case "$RED" in
	off) AFTER=( "${PINOFF[@]}" ) ;;
	nodome|nostars|interiorsky) AFTER=( WW_LOOKDEV_RED="$RED" ) ;;
	"") ;;
	*) echo "unknown red $RED"; exit 2 ;;
esac

shoot() {   # shoot <exe> <tag> <open tail> <look-at or ''> <view> <pitch> <hour> [env...]
	local exe="$1" tag="$2" open="$3" at="$4" v="$5" p="$6" h="$7"; shift 7
	local shot="$OUT/$tag.png" cam=()
	rm -f "$shot" "$OUT/$tag.notes"
	[ -n "$at" ] && cam=( WW_RENDER_CENTER="$at" WW_RENDER_VIEW="$v" WW_RENDER_DIST=20 WW_RENDER_FOV=70 )
	[ -n "$at" ] && [ "$p" != 0 ] && cam+=( WW_RENDER_PITCH="$p" )
	env WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR="$h" WW_LOOKDEV_DAY=4 WW_LOOKDEV_GROUND=0 \
		WW_LOOKDEV_PLUGINS="$ESM" WW_LOOKDEV_CLOUDTIME=0 WW_LODGEN_RESOURCES="$DATA" WW_CELL_IS=1 WW_CELL_LIT=1 \
		"$@" "${cam[@]}" WW_CELL_OPEN="$ESM|$open" WW_CELL_DATAROOT="$DATA" \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$exe" --port "$PORT" \
		"$(winpath "$REPO/tests/fixtures/empty.wwcell")" > "$OUT/$tag.notes" 2>&1
	echo "  $tag $([ -s "$shot" ] && echo shot || echo NO-PICTURE)  $(grep 'lookdev sky:' "$OUT/$tag.notes" | tail -1 | cut -c1-150)"
}

mkdir -p "$OUT"
GREEN="$REPO/scratchpad/skyfull1_20261003/gate"
# a red re-shoots only what it sabotages; the comparators (and the stages it does not aim at) come from the green run
[ -n "$RED" ] && [ "$RECHECK" != 1 ] && cp -n "$GREEN"/*.png "$GREEN"/*.notes "$OUT"/ 2>/dev/null
echo "cell_skyfull.sh $(date '+%F %T')${RED:+  RED CONTROL: $RED}  out: $OUT"
if [ "$RECHECK" != 1 ]; then
	[ -x "$EXE" ] || { echo "no exe $EXE"; exit 2; }
	for c in $CELLS; do
		open="Commonwealth|${XY[$c]}|1"
		for h in $HOURS; do
			shoot "$EXE" "${c}_${h}_after" "$open" "${AT[$c]}" "${VIEW[$h]}" "${PITCH[$h]}" "$h" "${AFTER[@]}"
			[ -n "$RED" ] && continue
			shoot "$EXE" "${c}_${h}_cube" "$open" "${AT[$c]}" "${VIEW[$h]}" "${PITCH[$h]}" "$h" "${PINOFF[@]}"
		done
		case " $HOURS " in *" 23 "*)
			[ "$RED" = nostars ] || [ -z "$RED" ] && \
				shoot "$EXE" "${c}_23_nostars" "$open" "${AT[$c]}" "${VIEW[23]}" "${PITCH[23]}" 23 WW_LOOKDEV_RED=nostars ;;
		esac
		if [ -z "$RED" ]; then
			shoot "$EXE" "pin0_$c" "$open" "${AT[$c]}" "${VIEW[12]}" 0 12 "${PINOFF[@]}"
			shoot "$BEFORE" "rung_$c" "$open" "${AT[$c]}" "${VIEW[12]}" 0 12
		fi
	done
	if [ -z "$RED" ] || [ "$RED" = interiorsky ]; then
		shoot "$EXE" int_cryo "interior|Vault111Cryo" "" 1 0 12 "${AFTER[@]}"
		shoot "$EXE" int_museum "interior|ConcordMuseum01" "" 1 0 12 "${AFTER[@]}"
	fi
fi
python "$REPO/tests/spells/cell_skyfull_check.py" "$OUT"
