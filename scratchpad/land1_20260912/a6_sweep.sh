#!/bin/bash
# LAND1 Part A -- STAGE 1 of the candidate sweep, on the SELECTION SEVEN only
# (TILING4's frozen split; the validation seven are not opened until the winner
# is picked).  Every bake carries --road-detail 1.  One variant = one out-dir.
#
#   usage: bash a6_sweep.sh <stage-name> <args...>
set -u
R=/e/Projects/NifskopeWildWastelandEdition
B=$R/scratchpad/land1_20260912/t5_bake.sh
NEW=$R/release/NifSkope.exe
VAR="$1"; shift
SEL="-20,24 -20,20 -36,-20 -4,-20 28,-20 -4,16 24,16"
for c in $SEL; do
	IFS=, read -r X Y <<< "$c"
	bash "$B" "$NEW" "$VAR" "r:$X,$Y,$((X+3)),$((Y+3))" --road-detail 1 "$@" | head -1
done
