#!/bin/bash
#
# THE PLACED LIGHTS' SPECULAR, AND THE NON SPECULAR FLAG (lane POOL1, 2026-10-01; res/shaders/cell_lights.glsl
# cellSumLights / cellSpecGame).
#
# Why this gate exists: the Vault 111 cryo walkway shows bright highlight pools in the game that the cell view
# does not. Measured from the game's shaders, they are NOT the placed lights' specular: the lamps over the
# walkway are flagged Non Specular (LIGH 0x8000) and the game's light shader variant for such a light writes no
# specular at all. (The pools are the game's screen-space reflections on the floor, which the cell view does not
# draw; docs/PRTP_PLAN.md.) This gate holds the specular the cell view does draw to the game's form, and keeps a
# Non Specular light from ever adding one.
#
# Not judged by eye. Per camera, one window per pass, the same camera each time:
#   probe 2/3  every cell-lit fragment writes its world position (high, low bytes), 4 its world normal
#   probe 9    ... the gloss the lights used (red; the legacy program only)
#   probe 30   ... the placed lights' specular / 4, before the material's mask, raw
# Then tests/spells/cell_spec_check.py rebuilds each sampled pixel's sum from the plugin's lights (the
# cell_lit_check.py walk) with the game's specular written out again, V from the camera dump, and compares.
#
# RED CONTROL (must FAIL in at least one view; a view with too little data says SKIP, never FAIL):
#   --red nonspec     the lights' Non Specular flag ignored (WW_CELL_SPEC_RED=nonspec)
#
# CELLS entries are "EDID@x,y,z@view@dist" (the camera's look-at, WW_RENDER_VIEW and eye distance).
#
# USAGE  bash tests/spells/cell_spec.sh [--red nonspec]

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_spec}"
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
OUT="${OUT:-$REPO/scratchpad/pool1_20261001/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_spec.log"
PORT="${PORT:-14747}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
# the walkway from the side (bungo's camera), and the floor at eye height beside the lamps' spec-lit corner
CELLS="${CELLS:-Vault111Cryo@300,-512,40@3@500 Vault111Cryo@384,-480,60@3@260}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_spec.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/gl/celllights.cpp res/shaders/cell_lights.glsl; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
cmp -s "$REPO/res/shaders/cell_lights.glsl" "$REPO/release/shaders/cell_lights.glsl" || { say "  release/shaders/cell_lights.glsl is stale"; newer=0; }
check "the exe and its shaders are current" "$newer"

shoot() {   # shoot <entry> <tag> <env...>
	local entry="$1" tag="$2"; shift 2
	local cell="${entry%%@*}" rest="${entry#*@}"
	local at="${rest%%@*}"; rest="${rest#*@}"
	local view="${rest%%@*}" dist="${rest#*@}"
	local label="${cell}@${at}"
	local shot="$OUT/$label.$tag.png" notes="$OUT/$label.$tag.notes"
	rm -f "$shot" "$notes" "$OUT/$label.$tag.cam"
	# unshadowed and unfogged: the sum is the placed lights alone (cell_shadow.sh / cell_fog.sh judge those)
	env WW_CELL_SHADOW=0 WW_CELL_FOG=0 "$@" WW_RENDER_CENTER="$at" WW_RENDER_DIST="$dist" WW_RENDER_FOV=70 \
		WW_CELL_CAM_DUMP="$(winpath "$OUT/$label.$tag.cam")" \
		WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
	[ -s "$shot" ] && echo 1 || echo 0
}

red_failed=0
for entry in $CELLS; do
	label="${entry%%@*}"; rest="${entry#*@}"; label="$label@${rest%%@*}"
	say "== $entry"
	redenv=(); [ -n "$RED" ] && redenv=( WW_CELL_SPEC_RED="$RED" )
	ok=1
	for p in 2 3 4 9 30; do
		[ "$(shoot "$entry" probe$p WW_CELL_LIT=1 WW_CELL_LIT_PROBE=$p "${redenv[@]}")" = 1 ] || ok=0
	done
	check "$entry: pictures written" "$ok"
	line="$(python "$(dirname "$0")/cell_spec_check.py" "$ESM" "${entry%%@*}" "$OUT" "$label" 2>&1 | tail -1)"
	say "  $line"
	if [ -n "$RED" ]; then
		[ "${line#spec FAIL}" != "$line" ] && red_failed=$((red_failed+1))
	else
		check "$entry: the specular matches the independent evaluation" "$([ "${line#spec PASS}" != "$line" ] && echo 1 || echo 0)"
	fi
done
[ -n "$RED" ] && check "the red control FAILS the check in at least one view ($red_failed)" "$([ "$red_failed" -ge 1 ] && echo 1 || echo 0)"
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
