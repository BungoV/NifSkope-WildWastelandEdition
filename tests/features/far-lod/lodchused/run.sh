#!/bin/bash
# lane FIX1101A lodch (11-01, LATE ITEMs 11 + 16 + 20): the far objects USE the bake's per-vertex AO and sky by default,
# on the instanced rings the cell view draws, offline (no window). NifSkope -no-gui farobjcensus builds Concord's three
# object rings instanced, as the cell view does, and prints per ring "farobjcensus ring N channels: AO on|off (P
# placements from the per-vertex stream, ...), sky on|off (P placements, V vertices, factor mean ...); T stream texels".
#   green (defaults): every ring AO on with stream placements > 0, sky on with placements > 0, stream texels > 0
#   red   : WW_LODL_AO_RED=off WW_LODL_SKY_RED=off (today's rule: no channel unless WW_LODL_AO=1) must FAIL
#   red0  : the same check on the base exe (BASE=<exe>, main d78ec45b): no channels line at all, must FAIL
#   bash tests/features/far-lod/lodchused/run.sh [EXE=<NifSkope.exe>] [BASE=<exe>] [LODI=<Commonwealth.lodi>] [OUT=<dir>]
# Prints one line per arm and "LODCHUSED RESULT PASS|FAIL".
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
TREE=$(cd "$HERE/../../../.." && pwd)
EXE=${EXE:-$TREE/release/NifSkope.exe}
BASE=${BASE:-}
LODI=${LODI:-E:/Projects/Fallout 4 Mods/mods/Fo4CS_Bakes/FO4CSLOD/Commonwealth/Commonwealth.lodi}
OUT=${OUT:-${TMPDIR:-$TREE/drop}/lodchused_run}
mkdir -p "$OUT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
RINGS=(--ring -18,14,-12,20,-17,15,-13,19,0 --ring -32,0,7,39,-18,14,-12,20,0 --ring -48,-16,15,47,-32,0,7,39,1)
census() {	# census <tag> <exe> [env...]
	local tag=$1 exe=$2; shift 2
	env -u WW_LODL_AO -u WW_LODL_SKY -u WW_LODL_AO_RED -u WW_LODL_SKY_RED "$@" timeout 300 "$exe" -no-gui farobjcensus \
		"$LODI" "${RINGS[@]}" --instanced > "$OUT/$tag.txt" 2> "$OUT/$tag.err"
	grep -E "channels:|instanced:" "$OUT/$tag.txt" | sed 's/^/     /'
}
verdict() {	# PASS when all 3 rings use the AO stream, the sky and the stream texels
	local f=$1 n ok
	n=$(grep -c "farobjcensus ring [0-9]* channels:" "$f")
	ok=$(grep "farobjcensus ring [0-9]* channels:" "$f" \
		| grep -cE "AO on \([1-9][0-9]* placements from the per-vertex stream.*sky on \([1-9][0-9]* placements.* [1-9][0-9]* stream texels.*ground contact on \([1-9][0-9]* placements")
	if [ "$n" -ge 3 ] && [ "$ok" = "$n" ]; then echo PASS; else echo FAIL; fi
}
echo "== green ($EXE)"
census green "$EXE"
g=$(verdict "$OUT/green.txt")
echo "   green: $g"
echo "== red: WW_LODL_AO_RED=off WW_LODL_SKY_RED=off"
census red "$EXE" WW_LODL_AO_RED=off WW_LODL_SKY_RED=off
r=$(verdict "$OUT/red.txt")
echo "   red: $r (must be FAIL)"
echo "== red: WW_LODL_AO_RED=noground (r2, LATE ITEM 25: the v12 ground-contact ramp not drawn)"
census redg "$EXE" WW_LODL_AO_RED=noground
rg=$(verdict "$OUT/redg.txt")
echo "   redg: $rg (must be FAIL)"
[ "$rg" = FAIL ] || r=PASS
r0=FAIL
if [ -n "$BASE" ]; then
	echo "== red0: base exe ($BASE)"
	census red0 "$BASE"
	r0=$(verdict "$OUT/red0.txt")
	echo "   red0: $r0 (must be FAIL)"
fi
if [ "$g" = PASS ] && [ "$r" = FAIL ] && [ "$r0" = FAIL ]; then echo "LODCHUSED RESULT PASS"; exit 0; fi
echo "LODCHUSED RESULT FAIL"
exit 1
