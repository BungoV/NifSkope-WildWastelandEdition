#!/bin/bash
# LAND1 gate A3 -- BORDER CONTINUITY, and gate A7 -- thread identity.
#
# A3 asks the only question that matters for a rule that reads the terrain: is
# the sheet a function of WORLD position alone, or of the region rectangle the
# baker happened to be given?  Three region rectangles all covering the chunk
# (-20,20), plus the same for its eastern neighbour (-16,20):
#
#   split20   r:-20,20,-17,23      the chunk alone
#   split16   r:-16,20,-13,23      its neighbour alone
#   joint     r:-20,20,-13,23      BOTH in one bake  -> the ring offsets differ
#   joint4    r:-24,20,-13,23      both again from a THIRD origin
#
# Every chunk sheet that appears in more than one of these must be byte-
# identical in all of them, for every rule.  A ring-anchored lattice, or a
# gradient that reaches past the ring and clamps, cannot survive this.
#
# THE FLOOR THAT MUST FIRE is not synthetic: the SAME chunk baked at two macro
# scales (1024 and 512) must DIFFER, or the guide is not reading the heightmap
# at all and the identity above would be identity by inaction.
#
# A7: the joint bake at 1 chunk thread against 16.
set -u
R=/e/Projects/NifskopeWildWastelandEdition
B=$R/scratchpad/land1_20260912/t5_bake.sh
NEW=$R/release/NifSkope.exe
ST="--land-sample stochastic --land-hex 256 --land-mip-bias -0.22"
run() { # <variant> <tile> <args...>
	local v="$1" t="$2"; shift 2
	bash "$B" "$NEW" "$v" "$t" --road-detail 1 "$@" | head -1
}
for rule in "off" "aspecthex:1.0" "drag:171" "aspect:1.0" "slopewarp:1.0" "flatwarp:1.0"; do
	tag="$(echo "$rule" | tr -d ':.' | tr '[:upper:]' '[:lower:]')"
	G="--land-guide $rule"
	[ "$rule" = "off" ] && G=""
	EX="$ST $G"
	[ "${rule%%:*}" = "slopewarp" ] && EX="$EX --land-warp 341"
	[ "${rule%%:*}" = "flatwarp" ]  && EX="$EX --land-warp 341"
	run "c_${tag}_split20" "r:-20,20,-17,23"  $EX
	run "c_${tag}_split16" "r:-16,20,-13,23"  $EX
	run "c_${tag}_joint"   "r:-20,20,-13,23"  $EX
	run "c_${tag}_joint4"  "r:-24,20,-13,23"  $EX
done
# the floor: the same chunk, two macro scales -- these MUST differ
run c_floor_s1024 "r:-20,20,-17,23" $ST --land-guide aspecthex:1.0 --land-guide-scale 1024
run c_floor_s512  "r:-20,20,-17,23" $ST --land-guide aspecthex:1.0 --land-guide-scale 512
# A7: threads
run c_thr1  "r:-20,20,-13,23" $ST --land-guide aspecthex:1.0 --chunk-threads 1
run c_thr16 "r:-20,20,-13,23" $ST --land-guide aspecthex:1.0 --chunk-threads 16
echo "a3_bake done $(date +%H:%M:%S)"
