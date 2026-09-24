#!/bin/bash
# Lane GROUND1 Part A, gates A2a and A2b.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W="$ROOT/scratchpad/ground1_20260912/work"
source "$W/bake.sh"

echo "=== A2b: the switch OFF against the rung exe, whole out-dir ==="
bake NifSkope.before_ground1.exe a2b_rung -24 24 -17 31
bake NifSkope.exe               a2b_new  -24 24 -17 31
if diff -r --brief "$W/a2b_rung/mod" "$W/a2b_new/mod" > "$W/a2b_mod.diff" 2>&1 \
	&& diff -r --brief "$W/a2b_rung/obj" "$W/a2b_new/obj" > "$W/a2b_obj.diff" 2>&1 \
	&& diff -r --brief "$W/a2b_rung/tex" "$W/a2b_new/tex" > "$W/a2b_tex.diff" 2>&1; then
	echo "A2b PASS: rung and new exe byte-identical with the switch off"
else
	echo "A2b FAIL"; cat "$W/a2b_mod.diff" "$W/a2b_obj.diff" "$W/a2b_tex.diff" | head
fi
echo "  files compared: $(find "$W/a2b_rung" -type f | wc -l)"

echo
echo "=== A2b floor: the same new exe WITH the switch must differ ==="
bake NifSkope.exe a2b_on -24 24 -17 31 --terrain-object-ao
if diff -r --brief "$W/a2b_new/mod" "$W/a2b_on/mod" > /dev/null 2>&1; then
	echo "A2b FLOOR FAIL: the switch changed nothing -- A2b is measuring nothing"
else
	echo "A2b floor PASS: the switch on differs from the switch off"
fi

echo
echo "=== A2a: a region with no LOD-bearing placement, switch ON ==="
for R in "36 16 39 19" "40 8 43 11" "28 -28 31 -25" "-4 44 -1 47"; do
	set -- $R
	bake NifSkope.exe probe_$1_$2 $1 $2 $3 $4 --terrain-object-ao > /dev/null 2>&1
	P=$(grep -ao 'objAoPlacements [0-9]*' "$W/probe_$1_$2.log" | head -1)
	S=$(grep -ao 'objAoSquares [0-9]*' "$W/probe_$1_$2.log" | head -1)
	T=$(grep -ao 'objAoTexels [0-9]*' "$W/probe_$1_$2.log" | head -1)
	echo "  region $R: $P $S $T"
done
