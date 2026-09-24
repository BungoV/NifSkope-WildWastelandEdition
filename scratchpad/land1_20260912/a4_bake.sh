#!/bin/bash
# LAND1 Part A -- the rung arm and the switches-off arm, all fourteen sheets of
# TILING4's FROZEN split, with --road-detail 1 on every bake (bungo's standing
# rule).  The rung is release/NifSkope.before_land1.exe = the 06:31:05 launch
# exe; the arm under test is the new exe with NO new switch, which must be its
# bytes exactly.  Gate A4.
set -u
R=/e/Projects/NifskopeWildWastelandEdition
B=$R/scratchpad/land1_20260912/t5_bake.sh
NEW=$R/release/NifSkope.exe
RUNG=$R/release/NifSkope.before_land1.exe
SEL="-20,24 -20,20 -36,-20 -4,-20 28,-20 -4,16 24,16"
VAL="-24,-24 -12,-20 4,-24 20,-24 -20,4 -8,4 12,8"
for c in $SEL $VAL; do
	IFS=, read -r X Y <<< "$c"
	T="r:$X,$Y,$((X+3)),$((Y+3))"
	bash "$B" "$RUNG" ls_rung "$T" --road-detail 1 | head -1
	bash "$B" "$NEW"  ls_off  "$T" --road-detail 1 | head -1
done
echo "a4_bake done $(date +%H:%M:%S)"
