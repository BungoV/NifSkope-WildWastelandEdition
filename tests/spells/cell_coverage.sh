#!/bin/bash
#
# THE PROBE COVERAGE PASS (lane WALLSTACK1, 2026-10-15; res/shaders/cell_lights.glsl cellPassOut, pass 6).
# Magenta where no probe reaches: the GI row's own sample (rooms, seen fill, lend rule, gap fill) has valid share
# k <= 0.01, so exactly the pixels the probe term lights nothing. Not judged by eye:
#   probe2-4   position and normal at every cell-lit pixel (WW_CELL_LIT_PROBE), the bake as is
#   cov        WW_CELL_PASS="Probe coverage" on the bake as is, its GI dump
#   drop       the same on a copy of the bake whose probes inside a box (DROP: half size round the camera's look-at)
#              stand on the nearest probe outside it (indices, links and the grid's box unchanged): a made gap
#   red        --red nofilter: the drop picture with WW_CELL_PV_RED=nofilter (the plain trilinear's k)
#              --red nolastfill: the bake as is with WW_CELL_PV_RED=nolastfill (no last fill: a pocket no probe
#              reached whose seen fill finds nothing takes the plain trilinear, black in a wall's empty voxels)
# tests/spells/cell_coverage_check.py judges:
#   A  the made gap: the drop picture paints >= 500 magenta pixels inside the box, the bake as is paints 0 there
#   B  each picture's magenta pixels are the pixels where its own dump's k (cell_rooms_check.gi_sample, rebuilt
#      offline) is <= 0.01, on 20000 clean pixels, agree >= 99.5% and the counts within 1% (+ 20)
# RED CONTROLS: --red nofilter must FAIL B; --red nolastfill must FAIL A (Vault 111's room-65 pixel turns magenta).
#
# USAGE  bash tests/spells/cell_coverage.sh [--red nofilter|nolastfill]      (one NifSkope at a time: withlock.sh nifskope)
#        BAKES=<folder with Vault111Cryo/bake + soup.psp> OUT=<absolute dir>

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
BAKES="${BAKES:-$REPO/scratchpad/probeview1_20261002/src_bakes}"
[ -e "$BAKES/Vault111Cryo/bake" ] || { echo "FAIL (2) missing reference bake: $BAKES/Vault111Cryo/bake"; exit 2; }
SCOPE="${SCOPE:-cell_coverage}"
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
case "$RED" in ""|nofilter|nolastfill) ;; *) echo "unknown red $RED"; exit 2 ;; esac
OUT="${OUT:-$REPO/scratchpad/wallstack1_coverage}"
CELL=Vault111Cryo
CAM="${CAM:-350,-512,40}"; VIEW="${VIEW:-4}"; DIST="${DIST:-450}"
DROP="${DROP:-600}"
PORT="${PORT:-14833}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
run="$OUT/$CELL"
mkdir -p "$run"
LOG="$run/coverage${RED:+_red_$RED}.log"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
say "cell_coverage.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
same=1
for f in cell_lights.glsl fo4_default.frag pbrm_default.frag; do
	cmp -s "$REPO/res/shaders/$f" "$(dirname "$EXE")/shaders/$f" || { say "  release/shaders/$f is not res/shaders/$f"; same=0; }
done
check "the exe's shaders are the tree's" "$same"

shoot() {   # shoot <bake dir> <tag> <env...>
	local bake="$1" tag="$2"; shift 2
	local shot="$run/$tag.png" notes="$run/$tag.notes"
	rm -f "$shot" "$notes"
	env WW_CELL_GI_PASSES=1 "$@" WW_CELL_OPEN="$ESM|interior|$CELL" WW_RENDER_CENTER="$CAM" WW_RENDER_DIST="$DIST" WW_RENDER_FOV=70 \
		WW_CELL_DATAROOT="$DATA" WW_CELL_GI_FROM="$(winpath "$bake")" \
		WW_CELL_PROBES="$(winpath "$run/probes.tsv")" WW_CELL_PROBES_HIDE=1 \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="$VIEW" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 1500 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
	[ -s "$shot" ] && echo 1 || echo 0
}

ok=1
if [ -z "$RED" ]; then
	rm -rf "$run/bake" "$run/bake_drop" "$run/dump_cov" "$run/dump_drop"
	cp -r "$BAKES/$CELL/bake" "$run/bake"
	cp "$BAKES/$CELL/soup.psp" "$run/soup.psp"
	python "$(dirname "$0")/cell_coverage_check.py" --drop "$run/bake" "$run/bake_drop" "$CAM" "$DROP" | tee -a "$LOG"
	for p in 2 3 4; do
		[ "$(shoot "$run/bake" probe$p WW_CELL_LIT=1 WW_CELL_GI=1 WW_CELL_LIT_PROBE=$p)" = 1 ] || ok=0
	done
	[ "$(shoot "$run/bake" cov WW_CELL_LIT=0 WW_CELL_GI=0 WW_CELL_PASS="Probe coverage" WW_CELL_GI_DUMP="$(winpath "$run/dump_cov")")" = 1 ] || ok=0
	[ "$(shoot "$run/bake_drop" drop WW_CELL_LIT=0 WW_CELL_GI=0 WW_CELL_PASS="Probe coverage" WW_CELL_GI_DUMP="$(winpath "$run/dump_drop")")" = 1 ] || ok=0
	grep -ho "pass=[^ ]*([^)]*)" "$run/cov.notes" | head -1 | tee -a "$LOG"
	check "the pictures written" "$ok"
	python "$(dirname "$0")/cell_coverage_check.py" "$run" cov drop "$CAM" "$DROP" > "$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	check "A and B hold" "$(grep -q "^coverage PASS" "$run/check.txt" && echo 1 || echo 0)"
elif [ "$RED" = nolastfill ]; then
	[ "$(shoot "$run/bake" red_$RED WW_CELL_LIT=0 WW_CELL_GI=0 WW_CELL_PASS="Probe coverage" WW_CELL_PV_RED="$RED")" = 1 ] || ok=0
	check "the red picture written" "$ok"
	python "$(dirname "$0")/cell_coverage_check.py" "$run" red_$RED drop "$CAM" "$DROP" > "$run/check_red_$RED.txt" 2>&1
	sed 's/^/  /' "$run/check_red_$RED.txt" | tee -a "$LOG"
	check "the red control FAILS A" "$(grep -q "^A FAIL" "$run/check_red_$RED.txt" && echo 1 || echo 0)"
else
	[ "$(shoot "$run/bake_drop" red_$RED WW_CELL_LIT=0 WW_CELL_GI=0 WW_CELL_PASS="Probe coverage" WW_CELL_PV_RED="$RED")" = 1 ] || ok=0
	check "the red picture written" "$ok"
	python "$(dirname "$0")/cell_coverage_check.py" "$run" cov red_$RED "$CAM" "$DROP" > "$run/check_red.txt" 2>&1
	sed 's/^/  /' "$run/check_red.txt" | tee -a "$LOG"
	check "the red control FAILS B" "$(grep -q "^B FAIL red_$RED" "$run/check_red.txt" && echo 1 || echo 0)"
fi
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
