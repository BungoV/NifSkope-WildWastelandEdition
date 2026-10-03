#!/bin/bash
#
# PLACED DECALS IN THE CELL VIEW (lane PLACED1, 2026-10-02; src/celldecal.cpp, src/esmplaced.cpp,
# docs/PRTP_PLAN.md, the PLACED1 section).
#
# A cell places a decal as a reference whose base is a texture set carrying decal data. The cell view skipped
# every one ("skipped, no model on the base: TXST 540" in Vault111Cryo). It now projects each one's box onto the
# opaque triangles inside it, the game's way, and draws the result lit like the surface.
# Not judged by eye. Per camera, two windows shoot the same frame with Cell lights on and the imagespace off (its
# exposure would move every pixel): no decal drawn (WW_CELL_DECAL_RED=none, the reference) and the run under
# test, with its camera, its census and its per-decal dump. tests/spells/cell_decal_check.py walks the plugin
# and the loose meshes itself, builds every decal's box by the rule (its own ray for the decals without a box
# primitive) and judges:
#   K  counts: read / drawn / refused by name in the census against the walk
#   G  each drawn box against the walk's box (centre within 1 unit, sizes within 0.5%)
#   N  nothing moves on screen outside the walk's boxes
#   C  the decals show inside them
# Each camera names the stages it carries.
#
# RED CONTROLS (WW_CELL_DECAL_RED on the run under test); each must FAIL, in every shot, the stages named:
#   --red none   no decal drawn                              K and C
#   --red wide   every box twice as wide and as high         G and N
#   --red axis   projected along the wrong local axis        G
#
# USAGE  bash tests/spells/cell_decal.sh [--red none|wide|axis]
#        SHOTS="cell:x,y,z:view:dist[:stages] ..." to pick cameras (stages default KGNC); RECHECK=1 judges the
#        shots already on disk.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_decal}"
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
case "$RED" in ""|none|wide|axis) ;; *) echo "unknown red control $RED"; exit 2 ;; esac
OUT="${OUT:-$REPO/scratchpad/placed1_20261002/gate_decal${RED:+_red_$RED}}"
LOG="$OUT/cell_decal.log"
PORT="${PORT:-14792}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
# Cameras in WORLD units, each picked with the checker's own ray (a clear line from the eye to the decals) and
# judged green and red before it was written here:
#   1  a Vault 111 corridor full of rust and grime (17 decals in sight). Their boxes cover the whole frame, so
#      there is no "outside" to judge: no N.
#   2  a ward of Milton General (box decals, 3 in sight). No N: green keeps 100.000% of its 209,744 outside
#      pixels, but the wide red moves only 115 of them (99.945%), so this camera cannot tell wide from right.
#   3  the Vault's cryo walkway bungo named. No decal within 900 units is in sight (moss on the far wall
#      only), so no C; 273,114 pixels outside the boxes carry N (green 99.997%, the wide red 94.973%).
SHOTS="${SHOTS:-Vault111Cryo:-5218,-110,265:5:250:KGC MiltonGeneral01:-1270,2107,731:5:250:KGC Vault111Cryo:384,-480,60:3:260:KGN}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_decal.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/cellview.cpp src/celldecal.cpp src/esmplaced.cpp src/cellrefs.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

i=0
for shot in $SHOTS; do
	IFS=: read -r cell at view dist stages <<< "$shot"
	stages="${stages:-KGNC}"
	i=$((i+1))
	run="$OUT/$i-$cell"
	say "== $i $cell at $at (view $view, distance $dist, stages $stages)"
	mkdir -p "$run"
	for tag in off on; do
		[ "${RECHECK:-0}" = "1" ] && break
		rm -f "$run/$tag.png"
		# OFFFROM=<another run's OUT>: take the reference shot from there instead of shooting it again
		if [ "$tag" = "off" ] && [ -s "${OFFFROM:-}/$i-$cell/off.png" ]; then
			cp "$OFFFROM/$i-$cell/off.png" "$run/off.png"; continue
		fi
		red="$RED"
		[ "$tag" = "off" ] && red=none
		cam=""; dump=""
		if [ "$tag" = "on" ]; then
			rm -f "$run/cam.txt" "$run/on.decals.txt"
			cam="$(winpath "$run/cam.txt")"; dump="$(winpath "$run/on.decals.txt")"
		fi
		env ${red:+WW_CELL_DECAL_RED=$red} ${cam:+WW_CELL_CAM_DUMP="$cam"} ${dump:+WW_CELL_DECAL_DUMP="$dump"} \
			WW_RENDER_CENTER="$at" WW_RENDER_DIST="$dist" WW_RENDER_FOV=70 \
			WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 \
			WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$SIZE" \
			WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
			WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$run/$tag.notes" 2>&1
	done
	check "$cell $i: the two shots, the camera and the decal dump written" \
		"$([ -s "$run/off.png" ] && [ -s "$run/on.png" ] && [ -s "$run/cam.txt" ] && [ -s "$run/on.decals.txt" ] && echo 1 || echo 0)"
	python "$(dirname "$0")/cell_decal_check.py" "$ESM" "$DATA" "$cell" "$run" "$at" "$stages" >"$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	failed() { case "$stages" in *$1*) grep -q "^$1 FAIL" "$run/check.txt" ;; *) return 0 ;; esac; }
	case "$RED" in
		none) check "$cell $i: the red control FAILS stages K and C" "$(failed K && failed C && echo 1 || echo 0)" ;;
		wide) check "$cell $i: the red control FAILS stages G and N" "$(failed G && failed N && echo 1 || echo 0)" ;;
		axis) check "$cell $i: the red control FAILS stage G" "$(failed G && echo 1 || echo 0)" ;;
		"") check "$cell $i: the decals pass stages $stages" "$(grep -q "^decal PASS" "$run/check.txt" && echo 1 || echo 0)" ;;
	esac
	if [ -n "$RED" ]; then
		check "$cell $i: the checker's verdict under the red control is FAIL" \
			"$(grep -q "^decal FAIL" "$run/check.txt" && echo 1 || echo 0)"
	fi
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
