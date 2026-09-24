#!/bin/bash
# LAND1 Part A -- the picture bakes.  Two sheets: (-20,20), the flattest of the
# selection seven and the one warp_sweep.py windowed; and (20,-24), the STEEPEST
# of the validation seven (macro tan 0.3467 = 19.1 deg), measured not chosen by
# eye.  Six panels: vanilla (shipped), plain (the rung), best of (a) DOWNHILL
# DRAG, best of (b) ASPECT ROTATION, best of (c) SLOPE-MODULATED HASH WARP, and
# TILING3's shipped warp at 683 units -- the one bungo called too strong.
# --road-detail 1 on every one.
set -u
R=/e/Projects/NifskopeWildWastelandEdition
B=$R/scratchpad/land1_20260912/t5_bake.sh
NEW=$R/release/NifSkope.exe
ST="--land-sample stochastic --land-hex 256 --land-mip-bias -0.22"
for c in "-20,20" "20,-24"; do
	IFS=, read -r X Y <<< "$c"
	T="r:$X,$Y,$((X+3)),$((Y+3))"
	bash "$B" "$NEW" p_plain "$T" --road-detail 1                                                   | head -1
	bash "$B" "$NEW" p_drag  "$T" --road-detail 1 --land-guide drag:341                              | head -1
	bash "$B" "$NEW" p_asp   "$T" --road-detail 1 $ST --land-guide aspecthex:1.0 --land-guide-scale 256 | head -1
	bash "$B" "$NEW" p_warp  "$T" --road-detail 1 $ST --land-guide flatwarp:1.0 --land-warp 341      | head -1
	bash "$B" "$NEW" p_683   "$T" --road-detail 1 --land-sample warp                                 | head -1
done
echo "pics bake done $(date +%H:%M:%S)"
