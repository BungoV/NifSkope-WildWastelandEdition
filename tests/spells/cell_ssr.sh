#!/bin/bash
#
# THE CELL VIEW'S SCREEN-SPACE REFLECTIONS (lane SSR1, 2026-10-02; src/gl/cellssr.h, docs/PRTP_PLAN.md
# "screen-space reflections").
#
# Not judged by eye. Per view, three windows with Cell lights on, the bounce and the imagespace off, the
# effects hidden (WW_CELL_FX_RED=hide: the checker reads only pixels that end on an opaque surface):
#   p61   probe 61 (the reflection as each draw read it) + WW_CELL_SSR_DUMP (the viewer's own depth, normals
#         and scene color, its ray, march and blurred result, the numbers)
#   on    the picture                    off   WW_CELL_SSR_RED=off: the same picture without reflections
#         (both with WW_CELL_DECAL_RED=none: the placed decals are not the same byte for byte run to run)
# Then tests/spells/cell_ssr_check.py re-does the game's march and blur in numpy from the dumped depth,
# normals and scene color, with the notes' constants and the plugin's own clip distance, and compares:
#   F  the far plane        M  the march        B  the blur        P  the probe picture
#   Z  (a view where nothing may reflect) the probe is black and on equals off, byte for byte
#   L  (a view with reflections) the picture gains light where the rebuild expects reflections
# Bars: agree >= 99% (M, B) or 95% (P, an 8-bit picture) where the value shows AND viewer total / expected
# total within 5%.
#
# VIEWS  "name|cell|look-at|distance|view|pools or zero" a line. The three shipped:
#   walkway      the Vault 111 cryo walkway at eye height: the floor must mirror what stands along it
#   walkway_far  the same walkway from its far end, a longer run of floor
#   topdown      the same floor from straight above: every reflected ray points back at the eye, so the
#                game's angle gate lets none start: zero added light
#
# RED CONTROLS (each must FAIL on the views with reflections):
#   --red off      computed, not applied            (stage P)
#   --red nogap    no 50-unit refusal in the march  (stage M)
#   --red nofade   confidence 1 on every hit        (stage M)
#
# USAGE  bash tests/spells/cell_ssr.sh [--red off|nogap|nofade]
#        RECHECK=1 judges the pictures already in OUT again, without a window.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_ssr}"
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
case "$RED" in ""|off|nogap|nofade) ;; *) echo "unknown red: $RED"; exit 2 ;; esac
OUT="${OUT:-$REPO/scratchpad/ssr1_20261002/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_ssr.log"
PORT="${PORT:-14791}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-1280x720}"
VIEWS="${VIEWS:-walkway|Vault111Cryo|350,-512,40|450|4|pools
walkway_far|Vault111Cryo|1100,-512,40|700|4|pools
topdown|Vault111Cryo|505,-512,-59|150|1|zero}"
RECHECK="${RECHECK:-0}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_ssr.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED (must fail)}$([ "$RECHECK" = 1 ] && echo '   RECHECK (no window)')"
newer=1
for s in src/gl/cellssr.cpp src/gl/cellssr.h src/gl/celllights.cpp src/gl/celllights.h src/glview.cpp src/gl/renderer.cpp \
	src/cellview.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"
step=1
for f in cell_ssr.frag cell_ssr.vert cell_ssr.prog cell_ssr.glsl cell_lights.glsl cell_ao.glsl cell_ao.frag fo4_default.frag \
	pbrm_default.frag; do
	cmp -s "$REPO/res/shaders/$f" "$REPO/release/shaders/$f" || { say "  release/shaders/$f is not res/shaders/$f"; step=0; }
done
check "the shaders the exe loads are the tree's" "$step"

shoot() {	# <run dir> <tag> <cell> <look-at> <dist> <view> <env...>
	local run="$1" tag="$2" cell="$3" center="$4" dist="$5" view="$6"; shift 6
	env "$@" WW_RENDER_CENTER="$center" WW_RENDER_DIST="$dist" WW_RENDER_FOV=70 \
		WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 WW_CELL_GI=0 WW_CELL_IS=0 \
		WW_CELL_FX_RED=hide WW_CELL_CAM_DUMP="$(winpath "$run/$tag.cam")" \
		WW_RENDER_SHOT="$(winpath "$run/$tag.png")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$run/$tag.notes" 2>&1
}

while IFS='|' read -r name cell center dist view kind; do
	[ -n "$name" ] || continue
	[ -n "$RED" ] && [ "$kind" != pools ] && continue
	say "== $name ($cell, look-at $center, $dist away, view $view; $kind)"
	run="$OUT/$name"
	mkdir -p "$run"
	if [ "$RECHECK" != 1 ]; then
		rm -f "$run"/p61.* "$run"/on.* "$run"/off.* "$run"/ssr.bin "$run"/ssr.bin.txt
		redenv=(); [ -n "$RED" ] && redenv=( WW_CELL_SSR_RED="$RED" )
		shoot "$run" p61 "$cell" "$center" "$dist" "$view" "${redenv[@]}" WW_CELL_LIT_PROBE=61 \
			WW_CELL_SSR_DUMP="$(winpath "$run/ssr.bin")"
		if [ -z "$RED" ]; then
			# the pair compares bytes: placed decals off in both (lane PLACED1's red). With them on, two runs of
			# the SAME settings differ by 1/255 on 17..23 pixels of two decal patches (walkway_far, 10-03), which
			# stage L read as light away from every reflection. Decals are blended: they take no reflection.
			shoot "$run" on "$cell" "$center" "$dist" "$view" WW_CELL_DECAL_RED=none
			shoot "$run" off "$cell" "$center" "$dist" "$view" WW_CELL_SSR_RED=off WW_CELL_DECAL_RED=none
		fi
	fi
	have=1
	for f in p61.png ssr.bin ssr.bin.txt; do [ -s "$run/$f" ] || have=0; done
	[ -z "$RED" ] && { [ -s "$run/on.png" ] && [ -s "$run/off.png" ] || have=0; }
	check "$name: every picture and the dump written" "$have"
	python "$(dirname "$0")/cell_ssr_check.py" "$ESM" "$cell" "$run" "$kind" > "$run/check.txt" 2>&1
	# the on / off pair only (L, Z) failed, and the march, blur and probe agree: shoot the pair ONCE more and
	# judge that one, the first kept as on1 / off1 and named in the log. Measured 10-03 on merged main: an on
	# shot alone came out 1/255 off on 3 floor pixels of a view where no ray starts (two full runs), while 4
	# standalone on / off pairs and 3 lone runs of that view were byte-identical. A real stray fails twice.
	if [ -z "$RED" ] && [ "$RECHECK" != 1 ] && grep -q -E "^[LZ] FAIL" "$run/check.txt" \
		&& ! grep -q -E "^[FMBP] FAIL" "$run/check.txt"; then
		say "  RESHOT $name: the on / off pair failed once (kept as on1.png / off1.png):"
		grep -E "^[LZ] FAIL" "$run/check.txt" | sed 's/^/    /' | tee -a "$LOG"
		mv -f "$run/on.png" "$run/on1.png"; mv -f "$run/off.png" "$run/off1.png"
		shoot "$run" on "$cell" "$center" "$dist" "$view" WW_CELL_DECAL_RED=none
		shoot "$run" off "$cell" "$center" "$dist" "$view" WW_CELL_SSR_RED=off WW_CELL_DECAL_RED=none
		python "$(dirname "$0")/cell_ssr_check.py" "$ESM" "$cell" "$run" "$kind" > "$run/check.txt" 2>&1
	fi
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	if [ -n "$RED" ]; then
		check "$name: the red control FAILS" "$(grep -q "^ssr FAIL" "$run/check.txt" && grep -q -E "^[MBP] FAIL" "$run/check.txt" && echo 1 || echo 0)"
	else
		check "$name: the reflections match the independent rebuild" "$(grep -q "^ssr PASS" "$run/check.txt" && echo 1 || echo 0)"
	fi
done <<< "$VIEWS"
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
