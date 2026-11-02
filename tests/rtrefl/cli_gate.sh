#!/bin/bash
# Lane RTREFL1: the GPU-vs-twin gate of the water's ray-traced reflections, no window (`-no-gui rtrefl`, src/rtddgi.cpp
# rtreflCli). A water plane in gpurelight's synthetic three-door room (written into <out>/synth on the first run), its two
# lamps; the CPU writes the water's ray target, refl.comp traces it, reflTexel mirrors it on the soup's BVH.
#   tests/rtrefl/cli_gate.sh <out dir>      (from the tree root; EXE= another exe; SOUP= another soup + RECT/CAM/LIGHTS/WATER)
# Pass (bars written before the first run): per view, texels with a ray >= 500 and twin hits >= 500, the kernel's hit
# agreement >= 99 % and colour within 5 % on the twin's hits >= 95 %; the red (no traced hit, the sky: the old path) must
# read colour agreement < 50 % on the same texels (it fails the bar the kernel passes).
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
	WATER=${WATER:-20}
fi
COMMON="--soup $SOUP --rect $RECT $LIGHTS --water $WATER --size 320,192 --steps 8"
bad=0
run() {
	local name=$1; shift
	mkdir -p $OUT/$name
	timeout 300 $EXE -no-gui rtrefl $COMMON --out $(cygpath -am $OUT/$name) "$@" > $OUT/$name/stdout.txt 2>&1
	echo "== $name rc=$?"
	cut -c1-400 $OUT/$name/stdout.txt
	local f=$OUT/$name/stdout.txt
	python -I "$(dirname "$0")/judge.py" "$f" < /dev/null || bad=1
}
run view_a --cam ${CAM_A:-120,300,140,900,460,60}
run view_b --cam ${CAM_B:-900,700,150,300,200,20}
[ $bad -eq 0 ] && echo "RTREFL CLI GATE OK" || echo "RTREFL CLI GATE FAILED"
exit $bad
