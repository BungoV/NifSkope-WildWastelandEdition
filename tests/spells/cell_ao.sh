#!/bin/bash
#
# THE CELL VIEW'S AMBIENT OBSCURANCE (lane AO1, 2026-10-01; src/gl/celllights.h, docs/PRTP_PLAN.md "ambient obscurance").
#
# Not judged by eye. Per interior, two windows with Cell lights + Imagespace on and the fog off (so the
# obscurance is the only difference between them), the effects hidden (WW_CELL_FX_RED=hide: steam and glow
# cards lie over most of an interior and the checker reads only pixels that end on an opaque surface):
#   on    the obscurance applied: WW_CELL_AO_DUMP (the opaque pass's view normal + linear depth, the raw
#         obscurance, the final) and WW_CELL_IS_DUMP (the measure pass's linear light, full size)
#   off   WW_CELL_AO_RED=off: the same frame without it (the measure pass only)
# Then tests/spells/cell_ao_check.py rebuilds the game's obscurance in numpy from the dumped depth and normals
# with its own constants (the game's INI defaults) and compares:
#   E  the dumped normals belong to the surface the dumped depth describes (and a mirrored control does not)
#   A  the raw obscurance and its depth key        B  the bilateral blur of the dumped raw
#   R  the game's history rule, where it can fire, vs a frame-by-frame run of that history
#   C  the light the picture got (on / off) vs the checker's own final, upsampled
#   D  the obscurance is there (>= 2% of the geometry under 0.9)
#
# RED CONTROLS (each must FAIL its stage):  --red off      computed, not applied       (stage C)
#                                           --red radius   half the game's radius      (stage A)
#                                           --red noblur   the raw obscurance applied  (stage B)
#                                           --red noreset  the plain mean, no history  (stage R)
#
# USAGE  bash tests/spells/cell_ao.sh [--red off|radius|noblur|noreset]
#        CELLS="..." to pick interiors; the camera stands at CAM_<cell> (x,y,z look-at) if set.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_ao}"
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
case "$RED" in ""|off|radius|noblur|noreset) ;; *) echo "unknown red: $RED"; exit 2 ;; esac
STAGE=""; [ "$RED" = off ] && STAGE=C; [ "$RED" = radius ] && STAGE=A; [ "$RED" = noblur ] && STAGE=B
[ "$RED" = noreset ] && STAGE=R
OUT="${OUT:-$REPO/scratchpad/ao1_20261001/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_ao.log"
PORT="${PORT:-14747}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
CELLS="${CELLS:-Vault111Cryo DmndSolomonsHouse01 GoodneighborTheThirdRail}"
: "${CAM_Vault111Cryo:=-4600,-280,120}" "${CAM_DmndSolomonsHouse01:=1450,-20,150}" "${CAM_GoodneighborTheThirdRail:=2932,-636,100}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_ao.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED (must fail stage $STAGE)}"
newer=1
for s in src/gl/celllights.cpp src/gl/celllights.h src/glview.cpp src/gl/renderer.cpp res/shaders/cell_ao.frag \
	res/shaders/cell_ao.glsl res/shaders/fo4_default.frag res/shaders/pbrm_default.frag; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

shoot() {	# <run dir> <tag> <AO red or empty> [dump]
	local run="$1" tag="$2" red="$3" dump="${4:-}" camvar="CAM_$cell" cam=() redenv=() aodump=()
	[ -n "${!camvar:-}" ] && cam=( WW_RENDER_CENTER="${!camvar}" WW_RENDER_DIST="${DIST:-350}" WW_RENDER_FOV=70 )
	[ -n "$red" ] && redenv=( WW_CELL_AO_RED="$red" )
	[ -n "$dump" ] && aodump=( WW_CELL_AO_DUMP="$(winpath "$run/ao.bin")" )
	env "${redenv[@]}" "${aodump[@]}" "${cam[@]}" \
		WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_FOG=0 \
		WW_CELL_FX_RED=hide \
		WW_CELL_IS=1 WW_CELL_IS_DUMP="$(winpath "$run/$tag.hdr")" \
		WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="${VIEW:-5}" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$run/$tag.notes" 2>&1
}

for cell in $CELLS; do
	say "== $cell"
	run="$OUT/$cell"
	mkdir -p "$run"
	rm -f "$run"/on.* "$run"/off.* "$run"/ao.bin "$run"/ao.bin.txt
	shoot "$run" on "$RED" dump
	shoot "$run" off off
	check "$cell: both pictures and every dump written" \
		"$([ -s "$run/on.png" ] && [ -s "$run/off.png" ] && [ -s "$run/on.hdr" ] && [ -s "$run/off.hdr" ] \
			&& [ -s "$run/ao.bin" ] && [ -s "$run/ao.bin.txt" ] && echo 1 || echo 0)"
	python "$(dirname "$0")/cell_ao_check.py" "$run" > "$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	if [ -n "$RED" ]; then
		check "$cell: the red control FAILS stage $STAGE" "$(grep -q "^$STAGE FAIL" "$run/check.txt" && echo 1 || echo 0)"
	else
		check "$cell: the obscurance matches the independent rebuild" "$(grep -q "^ao PASS" "$run/check.txt" && echo 1 || echo 0)"
	fi
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
