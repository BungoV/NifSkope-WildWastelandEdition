#!/bin/bash
#
# Lane SKEL2 -- the BASELINES, measured on the rung (release/NifSkope.exe
# 2026-09-11 07:06:04, UI6) BEFORE a line of this lane's code is written.
#
# Two jobs:
#
#  1. the numbers every gate this lane touches reads TODAY, so a count that
#     moves after the build can be named rather than guessed at;
#  2. item 6 of the brief -- gates (c) and (e) of skeleton_overlay demand
#     EXACT framebuffer equality and BUILD11 measured them varying 0..37 px
#     across four runs. Five identical runs here give the spread the
#     tolerance has to clear.
#
# One NifSkope at a time, every window on the second monitor (_harness.sh),
# each run its own port.
set -u
. "$(dirname "$0")/../../tests/spells/_harness.sh"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="$ROOT/release/NifSkope.exe"
SRC="$ROOT/fixtures/human_male_vanilla.nif"
CLIP="$ROOT/fixtures/Running_To_Slide_And_Back_To_Running.hkx"
OUT="$ROOT/scratchpad/skel2_20260910/logs"
mkdir -p "$OUT"

run_overlay_gates () {          # run_overlay_gates <n> <port>
	local n="$1" port="$2"
	rm -f "$ROOT/release/ww_skeloverlay_test.log"
	WW_SKELOVERLAY_TEST=1 \
	WW_SKELOVERLAY_CLIP="$(winpath "$CLIP")" \
	WW_SKELOVERLAY_FRAME=46 \
	WW_SKELOVERLAY_FPS=60 \
		"$NS" --port "$port" "$(winpath "$SRC")" >/dev/null 2>&1
	if [ -f "$ROOT/release/ww_skeloverlay_test.log" ]; then
		cp "$ROOT/release/ww_skeloverlay_test.log" "$OUT/base_skeloverlay_run$n.log"
		echo "run $n: $(grep -m1 'checks,' "$OUT/base_skeloverlay_run$n.log") | (c) outside $(grep -m1 'outside the overlay.s own mask' "$OUT/base_skeloverlay_run$n.log" | sed 's/.*mask: //;s/ .*//') | (e) backdiff $(grep -m1 'differing after toggling back off' "$OUT/base_skeloverlay_run$n.log" | sed 's/.*off: //')"
	else
		echo "run $n: NO LOG"
	fi
}

echo "===== (c)/(e) spread: five identical runs on the rung ====="
for n in 1 2 3 4 5; do
	run_overlay_gates "$n" "$(( 42330 + n ))"
done

echo
echo "===== WW_POSEDRAW_TEST on the rung ====="
rm -f "$ROOT/release/ww_posedraw_test.log"
WW_POSEDRAW_TEST=1 "$NS" --port 42341 "$(winpath "$SRC")" >/dev/null 2>&1
if [ -f "$ROOT/release/ww_posedraw_test.log" ]; then
	cp "$ROOT/release/ww_posedraw_test.log" "$OUT/base_posedraw.log"
	cat "$OUT/base_posedraw.log"
else
	echo "NO LOG"
fi

echo
echo "===== WW_POSEEXTRAS_TEST on the rung ====="
rm -f "$ROOT/release/ww_poseextras_test.log"
WW_POSEEXTRAS_TEST=1 "$NS" --port 42342 "$(winpath "$SRC")" >/dev/null 2>&1
if [ -f "$ROOT/release/ww_poseextras_test.log" ]; then
	cp "$ROOT/release/ww_poseextras_test.log" "$OUT/base_poseextras.log"
	cat "$OUT/base_poseextras.log"
else
	echo "NO LOG"
fi

echo
echo "===== WW_SKELETON_TEST on the rung ====="
rm -f "$ROOT/release/ww_skeleton_test.log"
WW_SKELETON_TEST=1 "$NS" --port 42343 "$(winpath "$SRC")" >/dev/null 2>&1
if [ -f "$ROOT/release/ww_skeleton_test.log" ]; then
	cp "$ROOT/release/ww_skeleton_test.log" "$OUT/base_skeleton.log"
	cat "$OUT/base_skeleton.log"
else
	echo "NO LOG"
fi

echo
echo "BASELINES DONE"
