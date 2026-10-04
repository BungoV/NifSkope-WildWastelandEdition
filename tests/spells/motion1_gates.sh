#!/bin/bash
#
# LANE MOTION1 (2026-10-04): THE CAMERA PATH, THE GAME'S TEMPORAL AA, THE SUN CASCADES IN MOTION.
# src/gl/campath.h (WW_RENDER_PATH / --path), src/gl/gametaa.h (the Temporal AA row), src/gl/sunshadow.h.
#
# Not judged by eye. Stages, each a launch of the exe (one window at a time, our port only):
#   off      the row OFF (as shipped) = the before-lane exe, byte for byte: a Sanctuary cell shot and a lookdev
#            shot with the cascades on (the fit now reads the unjittered projection), each against the
#            before-lane exe run twice (its own run-to-run floor)
#   path     the Sanctuary path with the row off, twice: frame k is the same picture on every run
#   taa      the same path with the row on, twice (byte-stable), frame 46 dumped and rebuilt in numpy by
#            motion1_taa_check.py (constants, jitter, motion vectors, the resolve, the clamp)
#   csm      a lookdev path around the known-answer cube with the cascades on (and once with the temporal AA
#            on: the jitter must not move a cascade), judged per frame by motion1_csm_check.py
#
# RED CONTROLS (each must FAIL its stage):
#   --red noclamp      WW_TAA_RED=noclamp     the rebuild's clamp stage (C)
#   --red wrongjitter  WW_TAA_RED=wrongjitter the jitter stage (J)
#   --red nosnap       WW_CSM_RED=nosnap      the snapping stage (S)
#   --red wrongsplit   WW_CSM_RED=wrongsplit  the per-frame fit (F)
#
# USAGE  bash tests/spells/motion1_gates.sh [--only off,path,taa,csm] [--red <name>]   (OUT=<dir> to choose)

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

EXE="${EXE:-$REPO/release/NifSkope.exe}"
BEFORE="${BEFORE:-$REPO/release/NifSkope.before_motion1.exe}"
ONLY=""
RED=""
while [ $# -gt 0 ]; do
	case "$1" in
		--only) ONLY=",$2,"; shift 2 ;;
		--red) RED="$2"; shift 2 ;;
		*) echo "unknown argument $1"; exit 2 ;;
	esac
done
case "$RED" in ""|noclamp|wrongjitter|nosnap|wrongsplit) ;; *) echo "unknown red: $RED"; exit 2 ;; esac
OUT="${OUT:-$REPO/scratchpad/motion1_20261004/gate${RED:+_red_$RED}}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
LOG="$OUT/motion1.log"
: > "$LOG"
PORT="${PORT:-14817}"
SCOPE=motion1gates
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
GAME="${GAME:-/x/Programs/Steam/steamapps/common/Fallout 4/Data}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
CUBE="$DATA/Meshes/Architecture/Quarry/QryCube01.nif"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-1280x720}"
HOUR=11
DUMPF=46	# the temporal AA's frame index: 16 pre-roll + path frame 30, mid-move

wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	local gm; gm="$(mktemp)"
	python "$HERE/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	printf '%s' "$SCOPE"
}
wipe_scope
trap wipe_scope EXIT
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
want() { [ -z "$ONLY" ] || [[ "$ONLY" == *",$1,"* ]]; }
harness_alive() {
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}

say "motion1_gates.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED (must fail)}"
{ sha1sum "$EXE" "$BEFORE"; } | tee -a "$LOG"
newer=1
for s in src/gl/gametaa.cpp src/gl/gametaa.h src/gl/campath.cpp src/gl/campath.h src/gl/sunshadow.cpp src/glview.cpp \
	src/glview.h src/nifskope_ui.cpp src/main.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"
step=1
for f in game_taa.frag game_taa.prog game_taa_mv.frag game_taa_mv.prog game_taa_copy.frag game_taa_copy.prog cell_ao.vert \
	ww_sunshadow.glsl; do
	cmp -s "$REPO/res/shaders/$f" "$REPO/release/shaders/$f" || { say "  release/shaders/$f is not res/shaders/$f"; step=0; }
done
check "the shaders the exe loads are the tree's" "$step"

# ---- the paths ----
# Sanctuary (-19,22): a two-second dolly over the street toward the houses, eye ~400 above the ground
cat > "$OUT/sanctuary.path" <<'EOF'
# lane MOTION1: Sanctuary Hills, a two-second dolly
fps 30
frames 61
key 0  -77200 90600 8250   -75776 92160 7900  70
key 1  -76300 90500 8280   -75500 92300 7900  70
key 2  -75300 90600 8300   -75200 92500 7900  70
EOF
sed 's/^frames 61/frames 32/' "$OUT/sanctuary.path" > "$OUT/sanctuary_short.path"
# lookdev: a quarter orbit around the 512 cube (centred on the origin), the ground below it
cat > "$OUT/cube.path" <<'EOF'
# lane MOTION1: a quarter orbit around QryCube01
fps 24
frames 49
key 0  1600 -600 700   0 0 -100  60
key 1  1300 300 650    0 0 -100  60
key 2  500 1300 600    0 0 -100  60
EOF

cellshot() {	# cellshot <exe> <tag> <env...>   one Sanctuary run (a still, or the path when WW_RENDER_PATH is set)
	local exe="$1" tag="$2"; shift 2
	rm -f "$OUT/$tag".png "$OUT/$tag"_*.png "$OUT/$tag"_path.txt
	local alive; alive="$(harness_alive)"
	[ -n "$alive" ] && { say "  REFUSED $tag: our harness NifSkope still running (pid $alive)"; return 1; }
	env "$@" WW_CELL_OPEN="$ESM|Commonwealth|-19,22|1" WW_CELL_DATAROOT="$DATA" WW_CELL_IS=0 \
		WW_CELL_DECAL_RED=none WW_CELL_FX_RED=hide WW_RENDER_CLEAN=1 WW_RENDER_FOV=70 \
		WW_RENDER_CENTER=-75776,92160,7950 WW_RENDER_VIEW=4 WW_RENDER_DIST=1800 \
		WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" WW_RENDER_SIZE="$SIZE" \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 1500 "$exe" --port "$PORT" "$(winpath "$SPEC")" \
		> "$OUT/$tag.notes" 2>&1 < /dev/null
	say "  $tag rc=$? $(ls "$OUT/$tag".png "$OUT/$tag"_*.png 2>/dev/null | wc -l) picture(s)"
	wipe_scope
}
LOOK=( WW_LIGHTING_MODE=lookdev WW_LOOKDEV=1 WW_VIEW_TRANSFORM=standard WW_EXPOSURE_EV=0 WW_LOOKDEV_SKY=0 WW_LOOKDEV_SUN=0
	WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 WW_LOOKDEV_GROUND=1 WW_LOOKDEV_SHADOWS=1 WW_LOOKDEV_HOUR=$HOUR
	WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_SS=0 WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1
	WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0 WW_RENDER_VIEW=8 )
lookshot() {	# lookshot <exe> <tag> <env...>
	local exe="$1" tag="$2"; shift 2
	rm -f "$OUT/$tag".png "$OUT/$tag"_*.png "$OUT/$tag"_path.txt "$OUT/$tag.csm.txt"
	local alive; alive="$(harness_alive)"
	[ -n "$alive" ] && { say "  REFUSED $tag: our harness NifSkope still running (pid $alive)"; return 1; }
	wipe_scope
	env "${LOOK[@]}" "$@" WW_SETTINGS_SCOPE=$SCOPE WW_RENDER_SIZE="$SIZE" \
		WW_CSM_ECHO="$(winpath "$OUT/$tag.csm.txt")" WW_LODGEN_RESOURCES="$(winpath "$DATA")" \
		WW_LOOKDEV_DATA="$(winpath "$GAME")" WW_RENDER_SHOT="$(winpath "$OUT/$tag.png")" \
		timeout 600 "$exe" --port "$PORT" "$(winpath "$CUBE")" > "$OUT/$tag.notes" 2>&1 < /dev/null
	say "  $tag rc=$? $(ls "$OUT/$tag".png "$OUT/$tag"_*.png 2>/dev/null | wc -l) picture(s)"
	wipe_scope
}
same() {	# same <a.png> <b.png> -> 1 when the pixels are identical
	python - "$1" "$2" <<'EOF'
import sys
from PIL import Image
import numpy as np
try:
    a = np.asarray(Image.open(sys.argv[1]).convert('RGBA')); b = np.asarray(Image.open(sys.argv[2]).convert('RGBA'))
    print(1 if a.shape == b.shape and (a == b).all() else 0)
except Exception:
    print(0)
EOF
}
samedir() {	# samedir <prefix a> <prefix b> <n> -> "identical/total"
	local a="$1" b="$2" n="$3" k ok=0
	for k in $(seq 0 $((n - 1))); do
		local f; f="$(printf '_%03d.png' "$k")"
		[ "$(same "$a$f" "$b$f")" = 1 ] && ok=$((ok + 1))
	done
	echo "$ok/$n"
}

if [ -z "$RED" ] && want off; then
	say "== off: the row off = the before-lane exe"
	cellshot "$BEFORE" off_cell_before1
	cellshot "$BEFORE" off_cell_before2
	cellshot "$EXE" off_cell_new
	floor="$(same "$OUT/off_cell_before1.png" "$OUT/off_cell_before2.png")"
	say "  the before-lane exe against itself (Sanctuary): identical=$floor"
	check "off: Sanctuary, the new exe = the before-lane exe, byte for byte" \
		"$(same "$OUT/off_cell_new.png" "$OUT/off_cell_before1.png")"
	lookshot "$BEFORE" off_look_before1
	lookshot "$BEFORE" off_look_before2
	lookshot "$EXE" off_look_new
	say "  the before-lane exe against itself (lookdev, cascades on): identical=$(same "$OUT/off_look_before1.png" "$OUT/off_look_before2.png")"
	check "off: lookdev with the cascades on, the new exe = the before-lane exe, byte for byte" \
		"$(same "$OUT/off_look_new.png" "$OUT/off_look_before1.png")"
	check "off: the cascades' echo is the before-lane exe's" \
		"$(cmp -s "$OUT/off_look_new.csm.txt" "$OUT/off_look_before1.csm.txt" && echo 1 || echo 0)"
fi

P="WW_RENDER_PATH=$(winpath "$OUT/sanctuary.path")"
PS="WW_RENDER_PATH=$(winpath "$OUT/sanctuary_short.path")"
if [ -z "$RED" ] && want path; then
	say "== path: the Sanctuary path, the row off, twice"
	cellshot "$EXE" path_off_a "$P"
	cellshot "$EXE" path_off_b "$P"
	r="$(samedir "$OUT/path_off_a" "$OUT/path_off_b" 61)"
	say "  frames identical run to run: $r"
	check "path: 61 frames written, every one the same on a second run" "$([ "$r" = "61/61" ] && echo 1 || echo 0)"
	check "path: the sidecar names 61 frames" "$([ "$(grep -c '^frame ' "$OUT/path_off_a_path.txt" 2>/dev/null)" = 61 ] && echo 1 || echo 0)"
fi

if [ -z "$RED" ] && want taa; then
	say "== taa: the same path with the Temporal AA row on, twice; frame $DUMPF dumped"
	rm -rf "$OUT/dump_a" "$OUT/dump_b"
	cellshot "$EXE" path_taa_a "$P" WW_TAA=1 WW_TAA_DUMP="$(winpath "$OUT/dump_a")" WW_TAA_DUMP_FRAME=$DUMPF
	cellshot "$EXE" path_taa_b "$P" WW_TAA=1 WW_TAA_DUMP="$(winpath "$OUT/dump_b")" WW_TAA_DUMP_FRAME=$DUMPF
	r="$(samedir "$OUT/path_taa_a" "$OUT/path_taa_b" 61)"
	say "  frames identical run to run: $r"
	check "taa: every frame the same on a second run" "$([ "$r" = "61/61" ] && echo 1 || echo 0)"
	differs=0
	for k in 0 30 60; do f="$(printf '_%03d.png' $k)"; [ "$(same "$OUT/path_taa_a$f" "$OUT/path_off_a$f")" = 0 ] && differs=$((differs + 1)); done
	check "taa: the row changes the picture (frames 0, 30, 60 differ from the row off: $differs/3)" "$([ $differs = 3 ] && echo 1 || echo 0)"
	python "$HERE/motion1_taa_check.py" "$OUT/dump_a" > "$OUT/taa_check.txt" 2>&1
	sed 's/^/  /' "$OUT/taa_check.txt" | tee -a "$LOG"
	check "taa: the numpy rebuild agrees" "$(grep -q '^taa PASS' "$OUT/taa_check.txt" && echo 1 || echo 0)"
	check "taa: the dumps of the two runs are the same bytes" \
		"$(for f in cur hist_in hist_out out mv depth; do cmp -s "$OUT/dump_a/$f.bin" "$OUT/dump_b/$f.bin" || echo x; done | grep -q x && echo 0 || echo 1)"
fi

if [ -z "$RED" ] && want csm; then
	say "== csm: a quarter orbit around the cube, the cascades on (then the Temporal AA on too)"
	lookshot "$EXE" csm_path "WW_RENDER_PATH=$(winpath "$OUT/cube.path")"
	python "$HERE/motion1_csm_check.py" "$OUT/csm_path_path.txt" $HOUR > "$OUT/csm_check.txt" 2>&1
	sed 's/^/  /' "$OUT/csm_check.txt" | tee -a "$LOG"
	check "csm: every frame's cascades are the spec's and the grid holds still" "$(grep -q '^csm PASS' "$OUT/csm_check.txt" && echo 1 || echo 0)"
	lookshot "$EXE" csm_path_taa "WW_RENDER_PATH=$(winpath "$OUT/cube.path")" WW_TAA=1
	python "$HERE/motion1_csm_check.py" "$OUT/csm_path_taa_path.txt" $HOUR > "$OUT/csm_check_taa.txt" 2>&1
	sed 's/^/  /' "$OUT/csm_check_taa.txt" | tee -a "$LOG"
	check "csm: the same with the Temporal AA on" "$(grep -q '^csm PASS' "$OUT/csm_check_taa.txt" && echo 1 || echo 0)"
	a="$(grep '^frame ' "$OUT/csm_path_path.txt" | sed 's/.*| csm=/csm=/' | md5sum)"
	b="$(grep '^frame ' "$OUT/csm_path_taa_path.txt" | sed 's/.*| csm=/csm=/' | md5sum)"
	check "csm: the jitter moves no cascade (every frame's echo the same with the row on and off)" "$([ "$a" = "$b" ] && echo 1 || echo 0)"
fi

case "$RED" in
	noclamp|wrongjitter)
		say "== red $RED: the short Sanctuary path, frame $DUMPF dumped"
		rm -rf "$OUT/dump_red"
		cellshot "$EXE" red_$RED "$PS" WW_TAA=1 WW_TAA_RED=$RED WW_TAA_DUMP="$(winpath "$OUT/dump_red")" WW_TAA_DUMP_FRAME=$DUMPF
		python "$HERE/motion1_taa_check.py" "$OUT/dump_red" --expect-red $RED > "$OUT/taa_check.txt" 2>&1
		sed 's/^/  /' "$OUT/taa_check.txt" | tee -a "$LOG"
		check "red $RED: the gate FAILS" "$(grep -q '^taa FAIL' "$OUT/taa_check.txt" && echo 1 || echo 0)" ;;
	nosnap|wrongsplit)
		say "== red $RED: the cube orbit"
		lookshot "$EXE" red_$RED "WW_RENDER_PATH=$(winpath "$OUT/cube.path")" WW_CSM_RED=$RED
		python "$HERE/motion1_csm_check.py" "$OUT/red_${RED}_path.txt" $HOUR --expect-red $RED > "$OUT/csm_check.txt" 2>&1
		sed 's/^/  /' "$OUT/csm_check.txt" | tee -a "$LOG"
		check "red $RED: the gate FAILS" "$(grep -q '^csm FAIL' "$OUT/csm_check.txt" && echo 1 || echo 0)" ;;
esac

say ""
if [ "$fails" = 0 ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
