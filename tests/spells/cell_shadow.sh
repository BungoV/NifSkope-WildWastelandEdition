#!/bin/bash
#
# THE CELL LIGHTS' SHADOWS (lane SHADOW1, 2026-10-01; src/gl/celllights.h, docs/PRTP_PLAN.md 2k).
#
# Not judged by eye. Per interior, one window per pass, the same camera each time:
#   lit        the Cell lights row on, shadows on: the picture bungo looks at
#   noshadow   the same with WW_CELL_SHADOW=0 (the pair's other half)
#   probe 2/3  every cell-lit fragment writes its world position (high, low bytes); probe 2 also dumps the
#              cell's probe soup (the bake's own triangles, <cell>.psp)
#   probe 4    ... its world normal
#   probe 7    ... the shadow factors of slots 0..2; that pass dumps the slot list (<cell>.shadow.txt)
# Then tests/spells/cell_shadow_check.py re-traces each sampled point's ray to each slot's light against the
# soup with its own Moller-Trumbore (no NifSkope code) and compares shadowed / lit.
#
# RED CONTROL (must FAIL):  --red noshadow   the maps rendered, every factor read as 1
#
# USAGE  bash tests/spells/cell_shadow.sh [--red noshadow]
#        CELLS="..." to pick interiors; the camera stands at CAM_<cell> (x,y,z look-at) if set.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_shadow}"
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
OUT="${OUT:-$REPO/scratchpad/shadow1_20261001/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_shadow.log"
PORT="${PORT:-14743}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
CELLS="${CELLS:-Vault111Cryo DmndSolomonsHouse01}"
: "${CAM_Vault111Cryo:=-4600,-280,0}" "${CAM_DmndSolomonsHouse01:=1450,-20,150}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_shadow.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/gl/celllights.cpp src/cellview.cpp res/shaders/cell_lights.glsl res/shaders/cell_shadowdepth.frag; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

shoot() {   # shoot <cell> <tag> <env...>
	local cell="$1" tag="$2"; shift 2
	local shot="$OUT/$cell.$tag.png" notes="$OUT/$cell.$tag.notes"
	rm -f "$shot" "$notes"
	local camvar="CAM_$cell" cam=()
	[ -n "${!camvar:-}" ] && cam=( WW_RENDER_CENTER="${!camvar}" WW_RENDER_DIST="${DIST:-1400}" WW_RENDER_FOV=70 )
	env "$@" "${cam[@]}" \
		WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="${VIEW:-1}" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
	[ -s "$shot" ] && echo 1 || echo 0
}

for cell in $CELLS; do
	say "== $cell"
	redenv=(); [ -n "$RED" ] && redenv=( WW_CELL_SHADOW_RED="$RED" )
	rm -f "$OUT/$cell.shadow.txt" "$OUT/$cell.psp"
	ok=1
	if [ -z "$RED" ]; then
		[ "$(shoot "$cell" lit WW_CELL_LIT=1)" = 1 ] || ok=0
		[ "$(shoot "$cell" noshadow WW_CELL_LIT=1 WW_CELL_SHADOW=0)" = 1 ] || ok=0
	fi
	[ "$(shoot "$cell" probe2 WW_CELL_LIT=1 WW_CELL_LIT_PROBE=2 \
		WW_CELL_PROBES="$(winpath "$OUT/$cell.probes.tsv")" WW_CELL_PROBE_SOUP="$(winpath "$OUT/$cell.psp")")" = 1 ] || ok=0
	for p in 3 4; do
		[ "$(shoot "$cell" probe$p WW_CELL_LIT=1 WW_CELL_LIT_PROBE=$p)" = 1 ] || ok=0
	done
	[ "$(shoot "$cell" probe7 WW_CELL_LIT=1 WW_CELL_LIT_PROBE=7 "${redenv[@]}" \
		WW_CELL_SHADOW_DUMP="$(winpath "$OUT/$cell.shadow.txt")")" = 1 ] || ok=0
	[ -s "$OUT/$cell.psp" ] && [ -s "$OUT/$cell.shadow.txt" ] || ok=0
	check "$cell: pictures, soup and slot list written" "$ok"
	[ -s "$OUT/$cell.shadow.txt" ] && say "  $(cut -c1-300 "$OUT/$cell.shadow.txt")"
	line="$(python "$(dirname "$0")/cell_shadow_check.py" "$OUT" "$cell" 2>&1 | tail -1)"
	say "  $line"
	if [ -n "$RED" ]; then
		check "$cell: the red control FAILS the check" "$([ "${line#*FAIL}" != "$line" ] && echo 1 || echo 0)"
	else
		check "$cell: the renderer's shadows match the independent re-trace" "$([ "${line#*PASS}" != "$line" ] && echo 1 || echo 0)"
	fi
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
