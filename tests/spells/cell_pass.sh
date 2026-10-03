#!/bin/bash
#
# THE PRTP BAND'S PASS DROP-DOWN (lane PROBEVIEW1, 2026-10-02; src/gl/celllights.h, src/gl/cellprobeview.h).
# The Division deck's probe views: GI (the grid's E / pi on the normal), Sky visibility (the probes'
# open-sky share on the normal), the surfels (their color, their light) and a picked probe's links.
#
# Not judged by eye. A bake is relit (WW_CELL_GI_FROM, a copy of an existing bake) and photographed:
#   probe2-4   the position and normal at every cell-lit pixel (WW_CELL_LIT_PROBE)
#   pass1-4    WW_CELL_PASS=1..4 with the Cell lights and GI rows OFF (a Pass draws on its own)
#   links      Pass 3 with one probe picked (WW_CELL_PV_PROBE), the overlay's dump (WW_CELL_PV_DUMP)
#   combined   Pass 0 against the pre-lane exe (BEFORE_EXE), GI on and off: byte-identical
# Then tests/spells/cell_pass_check.py rebuilds every value from the .tbk files, the soup and its own
# trilinear sampler (stages S T G K F L N Z, its docstring).
#
# RED CONTROLS (each must FAIL its stage):  --red direct    G  the lights leak into the GI pass
#                                           --red nonormal  G  the grid sampled facing up, not on the normal
#                                           --red open      K  every direction open to the sky
#
# USAGE  bash tests/spells/cell_pass.sh [--red direct|nonormal|open]
#        CELLS (default Vault111Cryo; concord opt-in); a red reuses the green run's probe pictures and dump.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
# the reference is a whole runtime FOLDER (exe + its own shaders/), main before the last landed lane;
# the overseer refreshes scratchpad/before_main at each landing (COMMIT names it)
BEFORE_EXE="${BEFORE_EXE:-$REPO/scratchpad/before_main/NifSkope.exe}"
BAKES="${BAKES:-$REPO/scratchpad/probeview1_20261002/src_bakes}"
for f in "$BEFORE_EXE" "$(dirname "$BEFORE_EXE")/shaders" "$BAKES"; do
	[ -e "$f" ] || { echo "FAIL (2) missing reference: $f"; exit 2; }
done
SCOPE="${SCOPE:-cell_pass}"
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
BASE="${OUT:-$REPO/scratchpad/probeview1_20261002/gate}"
OUT="$BASE${RED:+_red_$RED}"
LOG="$OUT/cell_pass.log"
PORT="${PORT:-14791}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
# concord is opt-in: in a Lookdev exterior the cell program draws nothing (probe 2 is the plain picture, the
# pre-lane exe too), so its pictures carry no data; S and T still judge its sky bake
CELLS="${CELLS:-Vault111Cryo}"
# camera A, the Vault 111 cryo walkway; Concord from the street
: "${CAM_Vault111Cryo:=350,-512,40}" "${VIEW_Vault111Cryo:=4}" "${DIST_Vault111Cryo:=450}"
: "${CAM_concord:=-59392,71680,300}" "${VIEW_concord:=4}" "${DIST_concord:=1600}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_pass.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/probegi.cpp src/gl/celllights.cpp src/gl/cellprobeview.cpp src/cellview.cpp src/glview.cpp \
	res/shaders/cell_lights.glsl res/shaders/fo4_default.frag res/shaders/pbrm_default.frag; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"
same=1
for f in cell_lights.glsl fo4_default.frag pbrm_default.frag; do
	cmp -s "$REPO/res/shaders/$f" "$(dirname "$EXE")/shaders/$f" || { say "  release/shaders/$f is not res/shaders/$f"; same=0; }
done
check "the exe's shaders are the tree's" "$same"

shoot() {   # shoot <exe> <run dir> <cell> <tag> <env...>
	local exe="$1" run="$2" cell="$3" tag="$4"; shift 4
	local shot="$run/$tag.png" notes="$run/$tag.notes"
	rm -f "$shot" "$notes"
	local cv="CAM_$cell" vv="VIEW_$cell" dv="DIST_$cell" open=() look=()
	if [ "$cell" = concord ]; then
		open=( WW_CELL_OPEN="$ESM|Commonwealth|-15,17|1" WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear
			WW_LOOKDEV_HOUR=12 WW_LOOKDEV_SUN=0 WW_LOOKDEV_GROUND=0 WW_LOOKDEV_PLUGINS="$ESM" WW_CELL_IS=0 )
	else
		open=( WW_CELL_OPEN="$ESM|interior|$cell" )
	fi
	env "$@" "${open[@]}" WW_RENDER_CENTER="${!cv}" WW_RENDER_DIST="${!dv}" WW_RENDER_FOV=70 \
		WW_CELL_DATAROOT="$DATA" WW_CELL_GI_FROM="$(winpath "$run/bake")" \
		WW_CELL_PROBES="$(winpath "$run/probes.tsv")" WW_CELL_PROBES_HIDE=1 \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="${!vv}" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 1500 "$exe" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
	[ -s "$shot" ] && echo 1 || echo 0
}

for cell in $CELLS; do
	say "== $cell"
	run="$OUT/$cell"
	green="$BASE/$cell"
	mkdir -p "$run"
	rm -rf "$run/bake"
	cp -r "$BAKES/$cell/bake" "$run/bake"
	cp "$BAKES/$cell/soup.psp" "$run/soup.psp"
	ok=1
	if [ -z "$RED" ]; then
		rm -rf "$run/dump"
		for p in 2 3 4; do
			[ "$(shoot "$EXE" "$run" "$cell" probe$p WW_CELL_LIT=1 WW_CELL_GI=1 WW_CELL_LIT_PROBE=$p)" = 1 ] || ok=0
		done
		[ "$(shoot "$EXE" "$run" "$cell" pass1 WW_CELL_LIT=0 WW_CELL_GI=0 WW_CELL_PASS=1 \
			WW_CELL_GI_DUMP="$(winpath "$run/dump")")" = 1 ] || ok=0
		grep -h "  gi" "$run/pass1.notes" | head -1 | cut -c1-240 | tee -a "$LOG"
		grep -ho "pass=[^ ]*([^)]*)" "$run/pass1.notes" | head -1 | tee -a "$LOG"
		[ "$(shoot "$EXE" "$run" "$cell" pass2 WW_CELL_LIT=0 WW_CELL_GI=0 WW_CELL_PASS="Sky visibility")" = 1 ] || ok=0
		if [ "$cell" != concord ]; then
			for p in 3 4; do
				[ "$(shoot "$EXE" "$run" "$cell" pass$p WW_CELL_LIT=0 WW_CELL_GI=0 WW_CELL_PASS=$p \
					WW_CELL_PV_DUMP="$(winpath "$run/pass$p.pv.txt")")" = 1 ] || ok=0
			done
			# the surfel under every pixel (each splat colored by its index + 1)
			[ "$(shoot "$EXE" "$run" "$cell" pass3id WW_CELL_LIT=0 WW_CELL_GI=0 WW_CELL_PASS=3 WW_CELL_PV_ID=1)" = 1 ] || ok=0
			# the probe with the most links within 250 units of the look-at
			cv="CAM_$cell"
			k="$(python "$(dirname "$0")/cell_pass_check.py" --pick "$run" "${!cv}")"
			echo "$k" > "$run/links.probe"
			[ "$(shoot "$EXE" "$run" "$cell" links WW_CELL_LIT=0 WW_CELL_GI=0 WW_CELL_PASS=3 WW_CELL_PV_PROBE="$k" \
				WW_CELL_PV_DUMP="$(winpath "$run/links.pv.txt")")" = 1 ] || ok=0
			for g in 1 0; do
				[ "$(shoot "$EXE" "$run" "$cell" combined_gi${g}_new WW_CELL_LIT=1 WW_CELL_GI=$g WW_CELL_PASS=0)" = 1 ] || ok=0
				[ "$(shoot "$BEFORE_EXE" "$run" "$cell" combined_gi${g}_old WW_CELL_LIT=1 WW_CELL_GI=$g)" = 1 ] || ok=0
			done
			# the pre-lane exe against itself: the run-to-run floor Z is judged by
			[ "$(shoot "$BEFORE_EXE" "$run" "$cell" combined_gi1_old2 WW_CELL_LIT=1 WW_CELL_GI=1)" = 1 ] || ok=0
		fi
	else
		# a red: the green run's probe pictures and dump, one pass picture shot with the red
		for f in probe2.png probe3.png probe4.png probe2.notes; do cp "$green/$f" "$run/$f" 2>/dev/null || ok=0; done
		rm -rf "$run/dump"; cp -r "$green/dump" "$run/dump" 2>/dev/null || ok=0
		case "$RED" in
			direct|nonormal) tag=pass1; pass=1 ;;
			open) tag=pass2; pass=2 ;;
			*) echo "unknown red $RED"; exit 2 ;;
		esac
		[ "$(shoot "$EXE" "$run" "$cell" $tag WW_CELL_LIT=0 WW_CELL_GI=0 WW_CELL_PASS=$pass WW_CELL_PV_RED="$RED")" = 1 ] || ok=0
	fi
	check "$cell: the pictures written" "$ok"
	python "$(dirname "$0")/cell_pass_check.py" "$cell" "$run" > "$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | tee -a "$LOG"
	want=""
	case "$RED" in direct|nonormal) want=G ;; open) want=K ;; esac
	if [ -n "$want" ]; then
		check "$cell: the red control FAILS stage $want" "$(grep -q "^$want FAIL" "$run/check.txt" && echo 1 || echo 0)"
	else
		check "$cell: every stage matches the independent rebuild" "$(grep -q "^pass PASS" "$run/check.txt" && echo 1 || echo 0)"
	fi
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
