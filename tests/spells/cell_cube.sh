#!/bin/bash
#
# THE INTERIOR CUBE MAP AT THE GAME'S STRENGTH (lane CUBE1, 2026-10-01; res/shaders/cell_lights.glsl cellCubeGame).
#
# Not judged by eye. Per interior view, one window per pass, the same camera each time:
#   lit         the Cell lights row on: the picture bungo looks at (and cubeold, the pair's other half)
#   probe 2/3   every cell-lit fragment writes its world position (high, low bytes), 4 its world normal
#   probe 50    ... the cube map reflection before any light, / 4 (light-free: cell_lit.sh owns the light)
#   probe 51/52 ... the texture coordinate's fraction, 16 bits (u, v), and the material's number in blue
#   probe 53    ... probe 4's rounding residual (a 16-bit normal: the reflection is steep in the normal)
# The material numbers come from WW_CELL_CUBE_DUMP ("cubetag=N|material", written by the probe 51 run).
# Then tests/spells/cell_cube_check.py parses each material file, reads the cube map and the _s map out of
# the game's archives, decodes them, and rebuilds the game's interior env term at every sampled pixel.
#
# RED CONTROL (must FAIL in at least one view; a view with too little data says SKIP, never FAIL):
#   --red cubeold     the old scale: the cube texel squared, x the env map scale x the specular strength,
#                     the default cube map when the material has none (what PRTPGI drew)
#
# The legacy program and the PBR program (WW_PBRM_MODE=pbr; shapes with no .pbrm) are judged apart.
# CELLS entries are "EDID" or "EDID@x,y,z" (a second camera in the same cell; the shots are named EDID@x,y,z).
#
# USAGE  bash tests/spells/cell_cube.sh [--red cubeold]

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_cube}"
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
export DATA
RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
OUT="${OUT:-$REPO/scratchpad/cube1_20261001/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_cube.log"
PORT="${PORT:-14747}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
MODES="${MODES:-legacy pbr}"
CELLS="${CELLS:-Vault111Cryo Vault111Cryo@-15,-421,345}"
: "${CAM_Vault111Cryo:=-4600,-280,0}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_cube.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/gl/celllights.cpp src/gl/renderer.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
# the program reads release/shaders, which the build refreshes from res/shaders
for s in cell_lights.glsl fo4_default.frag pbrm_default.frag; do
	cmp -s "$REPO/res/shaders/$s" "$REPO/release/shaders/$s" || { say "  release/shaders/$s is stale"; newer=0; }
done
check "the exe and its shaders are current" "$newer"

shoot() {   # shoot <entry> <file label> <tag> <env...>
	local entry="$1" lab="$2" tag="$3"; shift 3
	local cell="${entry%%@*}" at="" cam=()
	[ "$entry" != "$cell" ] && at="${entry#*@}"
	local shot="$OUT/$lab.$tag.png" notes="$OUT/$lab.$tag.notes"
	rm -f "$shot" "$notes" "$OUT/$lab.$tag.cam" "$OUT/$lab.$tag.cubetags"
	local camvar="CAM_$cell"
	[ -z "$at" ] && at="${!camvar:-}"
	[ -n "$at" ] && cam=( WW_RENDER_CENTER="$at" WW_RENDER_DIST="${DIST:-1400}" WW_RENDER_FOV=70 )
	# unshadowed and unfogged, as cell_oren.sh (the reflection probe carries no light anyway)
	local try
	for try in 1 2; do
		env WW_CELL_SHADOW=0 WW_CELL_FOG=0 "$@" "${cam[@]}" WW_CELL_CAM_DUMP="$(winpath "$OUT/$lab.$tag.cam")" \
			WW_CELL_CUBE_DUMP="$(winpath "$OUT/$lab.$tag.cubetags")" \
			WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" \
			WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
			WW_RENDER_VIEW="${VIEW:-1}" WW_RENDER_CLEAN=1 \
			WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
		# a start whose settings scope came up without the game's archives resolves no material at all (seen once
		# in 70 starts: 358 "not found in archives" lines against 2). That is the machine, not the picture: once more
		[ "$(grep -c 'not found in archives' "$notes")" -gt 50 ] || break
		echo "  NOTE  $lab.$tag: start $try came up without the game's archives" >> "$LOG"
		[ "$try" = 1 ] && rm -f "$shot" "$OUT/$lab.$tag.cam" "$OUT/$lab.$tag.cubetags"
	done
	[ -s "$shot" ] && echo 1 || echo 0
}

red_failed=0
for entry in $CELLS; do
	cell="${entry%%@*}"
	camvar="CAM_$cell"
	target="${entry#*@}"; [ "$entry" = "$cell" ] && target="${!camvar:-0,0,0}"
	for mode in $MODES; do
		label="$entry.$mode"
		say "== $entry ($mode)"
		modeenv=(); [ "$mode" = pbr ] && modeenv=( WW_PBRM_MODE=pbr )
		redenv=(); [ -n "$RED" ] && redenv=( WW_CELL_LIT_RED="$RED" )
		ok=1
		if [ -z "$RED" ] && [ "$mode" = legacy ]; then
			[ "$(shoot "$entry" "$label" lit WW_CELL_LIT=1)" = 1 ] || ok=0
			[ "$(shoot "$entry" "$label" cubeold WW_CELL_LIT=1 WW_CELL_LIT_RED=cubeold)" = 1 ] || ok=0
		fi
		for p in 2 3 4 50 51 52 53; do
			[ "$(shoot "$entry" "$label" probe$p WW_CELL_LIT=1 WW_CELL_LIT_PROBE=$p "${modeenv[@]}" "${redenv[@]}")" = 1 ] || ok=0
		done
		check "$label: pictures written" "$ok"
		line="$(python "$(dirname "$0")/cell_cube_check.py" "$ESM" "$OUT" "$label" "$target" "$mode" 2>&1 | tail -1)"
		say "  $line"
		if [ -n "$RED" ]; then
			# only a real FAIL counts: a SKIP (too little data) is not the red failing
			[ "${line#cube FAIL}" != "$line" ] && red_failed=$((red_failed+1))
		else
			# a SKIP fails the green: the gate must see enough env-mapped surface in every view it names
			check "$label: the reflection matches the independent evaluation" "$([ "${line#cube PASS}" != "$line" ] && echo 1 || echo 0)"
		fi
	done
done
[ -n "$RED" ] && check "the red control FAILS the check in at least one view ($red_failed)" "$([ "$red_failed" -ge 1 ] && echo 1 || echo 0)"
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
