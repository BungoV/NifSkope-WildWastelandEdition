#!/bin/bash
# Lane MODULES1 RESUME 15: the air rays' gate, no window (`-no-gui airrays`, src/features/gi/rtddgi.cpp airraysCli).
#   tests/airrays/cli_gate.sh <out dir>      (from the tree root; EXE= another exe; SOUP= another soup + RECT/CAM/LIGHTS)
# Default scene: gpurelight's synthetic three-door room (written into <out>/synth on the first run), its two lamps.
# The camera stands still, so after F frames the running share must reach the exact share sum w vis / sum w (CPU walk).
# Bars (set before the first run):
#   still       256 frames: no froxel without a GPU lamp, |gpu - exact| mean <= 0.02, within 0.10 >= 99 %
#   red_shadow  noshadow: mean |gpu - exact| > 0.05 (the room's shadows are real)
#   red_hist    nohist (2 lamps a frame, no history): mean |gpu - exact| > 3 x still's
#   cost        a 240 x 135 x 64 grid (1080p at 8 px columns): wall ms a step, printed, no bar
set -u
OUT=${1:?out dir}
EXE=$(cygpath -am "${EXE:-release/NifSkope.exe}")
mkdir -p $OUT/synth
tasklist | grep -qi "^Fallout4.exe" && { echo "Fallout4 up: no run"; exit 98; }
if [ -z "${SOUP:-}" ]; then
	[ -f $OUT/synth/soup.psp ] || timeout 900 $EXE -no-gui gpurelight --out $(cygpath -am $OUT/synth) > $OUT/synth/stdout.txt 2>&1
	SOUP=$(cygpath -am $OUT/synth/soup.psp)
	RECT=${RECT:-20,-116,1020,884}
	CAM=${CAM:-120,300,140,900,460,60}
	LIGHTS=${LIGHTS:-"--light 260,384,200,700,6,5,4 --light 780,384,200,700,4,5,6"}
fi
COMMON="--soup $SOUP --rect $RECT $LIGHTS --cam $CAM --grid 40,24,32 --near 8 --far 1600"
run() {
	local name=$1; shift
	mkdir -p $OUT/$name
	timeout 300 $EXE -no-gui airrays $COMMON --out $(cygpath -am $OUT/$name) "$@" > $OUT/$name/stdout.txt 2>&1
	echo "== $name rc=$?"
	cut -c1-400 $OUT/$name/stdout.txt
}
run still --frames 256
run red_shadow --frames 256 --red noshadow
run red_hist --frames 256 --red nohist
run cost --frames 4 --time-grid 240
python -I tests/airrays/judge.py $OUT
