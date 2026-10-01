#!/bin/bash
#
# THE GAME'S DIFFUSE: OREN-NAYAR ON THE CELL LIGHTS (lane ON1, 2026-10-01; res/shaders/cell_lights.glsl cellOren).
#
# Not judged by eye. Per interior, one window per pass, the same camera each time:
#   lit        the Cell lights row on: the picture bungo looks at
#   lambert    the same with the diffuse back to Lambert (the pair's other half; = --red lambert)
#   probe 2/3  every cell-lit fragment writes its world position (high, low bytes), 4 its world normal
#   probe 8    ... the placed lights' Oren-Nayar diffuse / 4, seen from the camera, raw
#   probe 9    ... the gloss that diffuse used (red; the legacy program only)
#   probe 10   ... the placed lights' rim alone x 4 (lane RIM1: big enough for the per-light rim flags to show)
# Then tests/spells/cell_oren_check.py rebuilds each sampled pixel's sum from the plugin's lights (the
# cell_lit_check.py walk) with the game's diffuse written out again, V from the camera dump, and compares.
#
# RED CONTROL (must FAIL in at least one view; a view with too little data says SKIP, never FAIL):
#   --red lambert     the diffuse factor read as 1 (what PRTP3 drew)
#   --red norim       the game's back-light rim term dropped (lane RIM1; measured before it was built: 7.9% of
#                     Vault view 1's lit pixels move past the tolerance)
#   --red rimflags    the lights' No Rim Lighting / Ignore Roughness flags ignored (lane RIM1; in probe 8 only 6
#                     of 18,029 lit Institute pixels move, so probe 10 judges it: 12,178 of Vault view 1's clean
#                     pixels move past twice the tolerance there, measured before it was built)
# (WW_CELL_LIT_RED=normalised, the textbook azimuth cosine, is NOT a red: measured 2026-10-01 its gap to the
# game's form peaks at about half the 8-bit tolerance in the Vault, so no check at this precision can fail it.)
#
# CELLS entries are "EDID" or "EDID@x,y,z" (a second camera in the same cell; the shots are named EDID@x,y,z).
#
# USAGE  bash tests/spells/cell_oren.sh [--red lambert|norim|rimflags]

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_oren}"
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
OUT="${OUT:-$REPO/scratchpad/on1_20261001/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_oren.log"
PORT="${PORT:-14745}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
# Solomon's house was dropped 2026-10-01: its view has 2 lit legacy pixels (the rest is PBR or unlit)
CELLS="${CELLS:-Vault111Cryo Vault111Cryo@-15,-421,345}"
: "${CAM_Vault111Cryo:=-4600,-280,0}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_oren.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/gl/celllights.cpp res/shaders/cell_lights.glsl; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
# the program reads release/shaders, which the build refreshes from res/shaders
cmp -s "$REPO/res/shaders/cell_lights.glsl" "$REPO/release/shaders/cell_lights.glsl" || { say "  release/shaders/cell_lights.glsl is stale"; newer=0; }
check "the exe and its shaders are current" "$newer"

shoot() {   # shoot <entry> <tag> <env...>
	local entry="$1" tag="$2"; shift 2
	local cell="${entry%%@*}" at="" cam=()
	[ "$entry" != "$cell" ] && at="${entry#*@}"
	local shot="$OUT/$entry.$tag.png" notes="$OUT/$entry.$tag.notes"
	rm -f "$shot" "$notes" "$OUT/$entry.$tag.cam"
	local camvar="CAM_$cell"
	[ -z "$at" ] && at="${!camvar:-}"
	[ -n "$at" ] && cam=( WW_RENDER_CENTER="$at" WW_RENDER_DIST="${DIST:-1400}" WW_RENDER_FOV=70 )
	# unshadowed and unfogged: the sum is the placed lights alone (cell_shadow.sh / cell_fog.sh judge those)
	env WW_CELL_SHADOW=0 WW_CELL_FOG=0 "$@" "${cam[@]}" WW_CELL_CAM_DUMP="$(winpath "$OUT/$entry.$tag.cam")" \
		WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="${VIEW:-1}" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
	[ -s "$shot" ] && echo 1 || echo 0
}

red_failed=0
for entry in $CELLS; do
	say "== $entry"
	redenv=(); [ -n "$RED" ] && redenv=( WW_CELL_LIT_RED="$RED" )
	ok=1
	if [ -z "$RED" ]; then
		[ "$(shoot "$entry" lit WW_CELL_LIT=1)" = 1 ] || ok=0
		[ "$(shoot "$entry" lambert WW_CELL_LIT=1 WW_CELL_LIT_RED=lambert)" = 1 ] || ok=0
	fi
	for p in 2 3 4 8 9 10; do
		[ "$(shoot "$entry" probe$p WW_CELL_LIT=1 WW_CELL_LIT_PROBE=$p "${redenv[@]}")" = 1 ] || ok=0
	done
	check "$entry: pictures written" "$ok"
	line="$(python "$(dirname "$0")/cell_oren_check.py" "$ESM" "${entry%%@*}" "$OUT" "$entry" 2>&1 | tail -1)"
	say "  $line"
	if [ -n "$RED" ]; then
		# only a real FAIL counts: a SKIP (too little data) is not the red failing
		[ "${line#oren FAIL}" != "$line" ] && red_failed=$((red_failed+1))
	else
		# a SKIP fails the green: the gate must see enough of the game's diffuse in every view it names
		check "$entry: the diffuse matches the independent evaluation" "$([ "${line#oren PASS}" != "$line" ] && echo 1 || echo 0)"
	fi
done
[ -n "$RED" ] && check "the red control FAILS the check in at least one view ($red_failed)" "$([ "$red_failed" -ge 1 ] && echo 1 || echo 0)"
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
