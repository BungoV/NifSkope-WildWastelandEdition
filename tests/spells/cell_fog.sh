#!/bin/bash
#
# THE INTERIOR FOG (lane FOG2, 2026-10-01; src/gl/celllights.h WwCellLighting::hasFog, docs/PRTP_PLAN.md 2l).
#
# Not judged by eye. Per interior, one window per pass, the same camera each time:
#   lit        the Cell lights row on, the fog on: the picture bungo looks at
#   nofog      the same with WW_CELL_FOG=0 (the pair's other half)
#   probe 2/3  every cell-lit fragment writes its world position (high, low bytes)
#   fog 6 / 7  every fogged fragment writes its fog alpha and height blend / its fog colour ^ 1/2.2
#   fog 8      ... the distance and height its fog read (the checker keeps only the pixels whose top surface
#              is the one probes 2 / 3 saw: a translucent card serves the fog probes and not the position ones)
# Then tests/spells/cell_fog_check.py walks the plugin itself (XCLL, LTMP -> LGTM, Inherits, the game's
# clamps), evaluates the fog formula at each sampled pixel and compares.
#
# RED CONTROLS (each must FAIL in at least one cell):
#   --red noclamp    near / far as stored (GoodneighborWarehouse01 stores near 0)
#   --red nogamma    the colours without their 2.2 power
#   --red noinherit  every field from XCLL, the Inherits flags ignored
#
# USAGE  bash tests/spells/cell_fog.sh [--red noclamp|nogamma|noinherit]
#        CELLS="..." to pick interiors; the camera stands at CAM_<cell> (x,y,z look-at) if set.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_fog}"
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
OUT="${OUT:-$REPO/scratchpad/fog2_20261001/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_fog.log"
PORT="${PORT:-14744}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
CELLS="${CELLS:-Vault111Cryo DmndSolomonsHouse01 GoodneighborWarehouse01}"
: "${CAM_Vault111Cryo:=-4600,-280,0}" "${CAM_DmndSolomonsHouse01:=1450,-20,150}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_fog.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/gl/celllights.cpp src/cellview.cpp src/esmweather.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

shoot() {   # shoot <cell> <tag> <env...>
	local cell="$1" tag="$2"; shift 2
	local shot="$OUT/$cell.$tag.png" notes="$OUT/$cell.$tag.notes"
	rm -f "$shot" "$notes" "$OUT/$cell.$tag.cam"
	local camvar="CAM_$cell" cam=()
	[ -n "${!camvar:-}" ] && cam=( WW_RENDER_CENTER="${!camvar}" WW_RENDER_DIST="${DIST:-1400}" WW_RENDER_FOV=70 )
	env "$@" "${cam[@]}" WW_CELL_CAM_DUMP="$(winpath "$OUT/$cell.$tag.cam")" \
		WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="${VIEW:-1}" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
	[ -s "$shot" ] && echo 1 || echo 0
}

red_failed=0
for cell in $CELLS; do
	say "== $cell"
	redenv=(); [ -n "$RED" ] && redenv=( WW_CELL_FOG_RED="$RED" )
	ok=1
	if [ -z "$RED" ]; then
		[ "$(shoot "$cell" lit WW_CELL_LIT=1)" = 1 ] || ok=0
		[ "$(shoot "$cell" nofog WW_CELL_LIT=1 WW_CELL_FOG=0)" = 1 ] || ok=0
	fi
	for p in 2 3; do
		[ "$(shoot "$cell" probe$p WW_CELL_LIT=1 WW_CELL_LIT_PROBE=$p)" = 1 ] || ok=0
	done
	for p in 6 7 8; do
		[ "$(shoot "$cell" fog$p WW_CELL_LIT=1 WW_CELL_FOG_PROBE=$p "${redenv[@]}")" = 1 ] || ok=0
	done
	check "$cell: pictures written" "$ok"
	say "  $(grep -o 'fog=near=[^|]*' "$OUT/$cell.fog6.notes" | head -1 | cut -c1-300)"
	line="$(python "$(dirname "$0")/cell_fog_check.py" "$ESM" "$cell" "$OUT" 2>&1 | tail -1)"
	say "  $line"
	if [ -n "$RED" ]; then
		[ "${line#*FAIL}" != "$line" ] && red_failed=$((red_failed+1))
	else
		check "$cell: the renderer's fog matches the independent evaluation" "$([ "${line#*PASS}" != "$line" ] && echo 1 || echo 0)"
	fi
done
[ -n "$RED" ] && check "the red control FAILS the check in at least one cell ($red_failed)" "$([ "$red_failed" -ge 1 ] && echo 1 || echo 0)"
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
