#!/bin/bash
# TILING4 -- bake ALL FOURTEEN sheets of the frozen split with the real exe, in
# three arms, so gate F3's verdict can be the PRODUCT's and not the prototype's.
# f3_real.py had only the two TILING2 tiles and found the prototype off by as
# much as 0.27 on the repeat in BOTH directions; fourteen chunks at ~6 s a bake
# costs four minutes, so there is no reason to leave the verdict modelled.
#
# Each chunk (cx,cy) at dim 4 covers cells cx..cx+3, cy..cy+3.
#
#   usage: bash f3_full.sh
set -u
R=/e/Projects/NifskopeWildWastelandEdition
B=$R/scratchpad/tiling4_20260912/t4_bake.sh
NEW=$R/release/NifSkope.exe
RUNG=$R/release/NifSkope.before_tiling4.exe

SEL="-20,24 -20,20 -36,-20 -4,-20 28,-20 -4,16 24,16"
VAL="-24,-24 -12,-20 4,-24 20,-24 -20,4 -8,4 12,8"

for c in $SEL $VAL; do
	IFS=, read -r X Y <<< "$c"
	T="r:$X,$Y,$((X+3)),$((Y+3))"
	bash "$B" "$RUNG" fs_rung "$T"                              | head -1
	bash "$B" "$NEW"  fs_stoch "$T" --land-sample stochastic     | head -1
	bash "$B" "$NEW"  fs_warp  "$T" --land-sample warp           | head -1
done
echo "f3_full done $(date +%H:%M:%S)"
