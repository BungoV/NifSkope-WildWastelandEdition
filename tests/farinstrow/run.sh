#!/bin/bash
# lane SKYSHARDS3 (CODE1030, 10-30): the per-draw row an instanced far ring's commands read with the Bindless draws
# row on, offline (no window). NifSkope -no-gui farobjcensus builds Concord's three object rings instanced, as the cell
# view does, and prints per ring the commands, the program the renderer gives the ring (CellFarInst::bindlessTwinAllowed,
# the same call renderer.cpp's twin choice makes) and how many commands then read ANOTHER draw's row
# (ww_bindless.glsl: wwDrawIdx = wwDrawBase + gl_BaseInstance, and a far command's base instance is a placement row).
#   green: every ring drawn with its own program, 0 foreign rows, 0 past the ring
#   red  : WW_CELL_FARINST_RED=blrow (today's main: the twin) must FAIL (foreign > 0)
#   bash tests/farinstrow/run.sh [EXE=<NifSkope.exe>] [LODI=<Commonwealth.lodi>] [OUT=<dir>]
# Prints one line per arm and "FARINSTROW RESULT PASS|FAIL".
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
TREE=$(cd "$HERE/../.." && pwd)
EXE=${EXE:-$TREE/release/NifSkope.exe}
LODI=${LODI:-E:/Projects/Fallout 4 Mods/mods/Fo4CS_Bakes/FO4CSLOD/Commonwealth/Commonwealth.lodi}
OUT=${OUT:-${TMPDIR:-$TREE/drop}/farinstrow_run}
mkdir -p "$OUT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
RINGS=(--ring -18,14,-12,20,-17,15,-13,19,0 --ring -32,0,7,39,-18,14,-12,20,0 --ring -48,-16,15,47,-32,0,7,39,1)
census() {	# census <tag> [env...]
	local tag=$1; shift
	env -u ONLY -u SKIP "$@" timeout 300 "$EXE" -no-gui farobjcensus "$LODI" "${RINGS[@]}" --instanced \
		> "$OUT/$tag.txt" 2> "$OUT/$tag.err"
	grep -E "bindless rows:" "$OUT/$tag.txt" | sed 's/^/     /'
}
verdict() {	# verdict <file>: PASS when every ring line reads its own program, 0 foreign, 0 past
	local f=$1 n bad
	n=$(grep -c "bindless rows:" "$f")
	bad=$(grep "bindless rows:" "$f" | grep -vcE "drawn with its own program; 0 read another draw's row, 0 past")
	if [ "$n" -ge 3 ] && [ "$bad" = 0 ]; then echo PASS; else echo FAIL; fi
}
fail=0
census green
g=$(verdict "$OUT/green.txt")
census red WW_CELL_FARINST_RED=blrow
r=$(verdict "$OUT/red.txt")
# the red must be red for the rule, not for a missing line: it needs 3 ring lines with foreign > 0 somewhere
rn=$(grep -c "bindless rows:" "$OUT/red.txt")
rf=$(grep "bindless rows:" "$OUT/red.txt" | grep -cvE "; 0 read another draw's row")
[ "$g" = PASS ] && echo "ok   green (own program): PASS" || { echo "FAIL green (own program): $g -- $OUT/green.txt"; fail=1; }
if [ "$r" = FAIL ] && [ "$rn" -ge 3 ] && [ "$rf" -ge 1 ]; then
	echo "ok   red WW_CELL_FARINST_RED=blrow: FAIL ($rf of $rn rings read foreign rows)"
else
	echo "FAIL red WW_CELL_FARINST_RED=blrow: $r ($rf of $rn rings foreign) -- $OUT/red.txt"; fail=1
fi
[ $fail = 0 ] && echo "FARINSTROW RESULT PASS" || echo "FARINSTROW RESULT FAIL"
exit $fail
