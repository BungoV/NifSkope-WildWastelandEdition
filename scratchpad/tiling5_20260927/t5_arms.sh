#!/bin/bash
# TILING5 -- bake the fourteen frozen TILING4 chunks (selection + validation) for
# one arm with the new exe.  Environment variables (WW_TILING5_BETA,
# WW_TILING5_MACRO) pass straight through to the exe for the tuning sweep.
#
#   usage: t5_arms.sh <arm-name> [extra lodgen args...]
set -u
HERE=/e/Projects/NifskopeWWE-tiling5/scratchpad/tiling5_20260927
EXE="${EXE:-$HERE/run_new/NifSkope.exe}"
ARM="$1"; shift
SEL="-20,24 -20,20 -36,-20 -4,-20 28,-20 -4,16 24,16"
VAL="-24,-24 -12,-20 4,-24 20,-24 -20,4 -8,4 12,8"
S0=$(date +%s)
for c in $SEL $VAL; do
	IFS=, read -r X Y <<< "$c"
	bash "$HERE/t5_bake.sh" "$EXE" "$ARM" "r:$X,$Y,$((X+3)),$((Y+3))" "$@" | head -1
done
echo "ARM $ARM done in $(( $(date +%s) - S0 ))s  $(date +%H:%M:%S)"
