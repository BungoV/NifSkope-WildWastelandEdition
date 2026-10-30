#!/bin/bash
# Lane RESTIRDI1: the GPU-vs-twin gate of ReSTIR DI, no window (`-no-gui restir`, src/rtddgi.cpp restirCli).
#   tests/restir_math/cli_gate.sh <out dir>      (from the tree root; EXE= another exe; SOUP= another soup + RECT/CAM/LIGHTS)
# Default scene: gpurelight's synthetic three-door room (written into <out>/synth on the first run), its two lamps.
# Pass: every frame line of di0 and full reads "light kept differs 0" and "hit/miss differs 0"; red_novis reads mean ratio 1.
#   di0         RESTIRDI0: one light, one candidate, no reuse, no history: one shadow ray a pixel
#   full        RESTIRDI1: both lights, 8 candidates, 4 neighbours, 8 frames
#   red_novis   the novis red (both runs): no shadow, ratio 1
#   red_nohole  the GPU's nohole red: differs only where the soup has alpha-tested cards (the synthetic room has none)
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
COMMON="--soup $SOUP --rect $RECT $LIGHTS --cam $CAM --size 192,108"
bad=0
run() {
	local name=$1; shift
	mkdir -p $OUT/$name
	timeout 240 $EXE -no-gui restir $COMMON --out $(cygpath -am $OUT/$name) "$@" > $OUT/$name/stdout.txt 2>&1
	echo "== $name rc=$?"
	cut -c1-300 $OUT/$name/stdout.txt
}
run di0 --only 0 --candidates 1 --spatial 0 --red notemporal --frames 2 --gpu 1
run full --frames 8 --gpu 1
run red_novis --frames 4 --gpu 1 --red novis
run red_nohole --frames 4 --gpu 1 --red nohole
for n in di0 full; do
	f=$OUT/$n/stdout.txt
	[ "$(grep -c '^frame' $f)" -ge 2 ] && ! grep '^frame' $f | grep -qv 'light kept differs 0 .*hit/miss differs 0;' || { echo "FAIL $n"; bad=1; }
done
grep '^frame' $OUT/red_novis/stdout.txt | grep -q 'mean ratio twin 1.0000 gpu 1.0000' || { echo "FAIL red_novis (the red did not bite)"; bad=1; }
[ $bad -eq 0 ] && echo "RESTIR CLI GATE OK" || echo "RESTIR CLI GATE FAILED"
exit $bad
