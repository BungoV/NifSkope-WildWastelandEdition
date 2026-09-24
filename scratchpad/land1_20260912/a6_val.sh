#!/bin/bash
# LAND1 gate A6 -- the VALIDATION seven, opened only now that the winner is
# picked on the selection seven.  Three arms: TILING4's pick, the winner, and
# the winner at the default macro scale.
set -u
R=/e/Projects/NifskopeWildWastelandEdition
B=$R/scratchpad/land1_20260912/t5_bake.sh
NEW=$R/release/NifSkope.exe
ST="--land-sample stochastic --land-hex 256 --land-mip-bias -0.22"
VAL="-24,-24 -12,-20 4,-24 20,-24 -20,4 -8,4 12,8"
for c in $VAL; do
	IFS=, read -r X Y <<< "$c"
	T="r:$X,$Y,$((X+3)),$((Y+3))"
	bash "$B" "$NEW" g_stoch     "$T" --road-detail 1 $ST | head -1
	bash "$B" "$NEW" g_ahs_s256  "$T" --road-detail 1 $ST --land-guide aspecthex:1.0 --land-guide-scale 256 | head -1
	bash "$B" "$NEW" g_ahs100    "$T" --road-detail 1 $ST --land-guide aspecthex:1.0 | head -1
done
echo "val done $(date +%H:%M:%S)"
