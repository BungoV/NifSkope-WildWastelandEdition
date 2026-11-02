#!/bin/bash
# Lane RTAO1: the GPU-vs-twin gate of the ray-traced ambient occlusion, no window (`-no-gui rtao`, src/rtddgi.cpp rtaoCli).
# gpurelight's synthetic three-door room (written into <out>/synth on the first run); the CPU writes the shared opaque
# pass's g-buffer from the camera, ao.comp traces the occlusion rays, aoTexel mirrors it on the soup's BVH.
#   tests/rtao/cli_gate.sh <out dir>      (from the tree root; EXE= another exe; SOUP= another soup + RECT/CAM_A/CAM_B/LIGHTS)
# Pass (bars written before the first run, tests/rtao/judge.py): per view, texels with a surface >= 5000, surface agree
# >= 99.9 %, mean twin visibility <= 0.99 (the scene occludes); the kernel: texels off the twin by more than 1/n <= 0.5 %
# and |d| mean <= 0.005; the red (no rays, visibility 1: the screen-space term only) must miss both of those bars.
# Arms: the default knobs (8 rays) and 16 rays with near 0 (every hit counts).
set -u
OUT=${1:?out dir}
EXE=$(cygpath -am "${EXE:-release/NifSkope.exe}")
mkdir -p $OUT/synth
tasklist | grep -qi "^Fallout4.exe" && { echo "Fallout4 up: no run"; exit 98; }
if [ -z "${SOUP:-}" ]; then
	[ -f $OUT/synth/soup.psp ] || timeout 900 $EXE -no-gui gpurelight --out $(cygpath -am $OUT/synth) > $OUT/synth/stdout.txt 2>&1
	SOUP=$(cygpath -am $OUT/synth/soup.psp)
	RECT=${RECT:-20,-116,1020,884}
	LIGHTS=${LIGHTS:-"--light 260,384,200,700,6,5,4 --light 780,384,200,700,4,5,6"}
fi
COMMON="--soup $SOUP --rect $RECT $LIGHTS --size 320,192"
bad=0
run() {
	local name=$1; shift
	mkdir -p $OUT/$name
	timeout 300 $EXE -no-gui rtao $COMMON --out $(cygpath -am $OUT/$name) "$@" > $OUT/$name/stdout.txt 2>&1
	echo "== $name rc=$?"
	cut -c1-400 $OUT/$name/stdout.txt
	python -I "$(dirname "$0")/judge.py" "$OUT/$name/stdout.txt" < /dev/null || bad=1
}
run view_a --cam ${CAM_A:-120,300,140,900,460,60}
run view_b --cam ${CAM_B:-900,700,150,300,200,20}
run view_a16 --cam ${CAM_A:-120,300,140,900,460,60} --rays 16 --near 0
[ $bad -eq 0 ] && echo "RTAO CLI GATE OK" || echo "RTAO CLI GATE FAILED"
exit $bad
