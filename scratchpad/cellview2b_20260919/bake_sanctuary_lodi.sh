#!/bin/bash
# CELLVIEW2B: bake a `.lodi` that COVERS Sanctuary cell -20,7, so the identity
# overlay rows of tests/spells/cell_pick.sh have an input and bungo can judge
# the ruled 64 u proximity join on a cell he knows.
#
# Every .lodi already in the tree was measured (header H_WEST 0x48, src/lodifile.cpp:30)
# and NONE covers -20,7:
#   horizonout join/prox + bytecheck/new   cells x 0..11,  y -12..-9   (downtown)
#   horizonout scrap/nat                   cells x -4..11, y -16..3
#   defaults1 gate/e_new                   cells x -20..-13, y 24..31
# So it is baked, per skill nifskope-ww-lodgen, with --road-detail 1 (standing
# rule) and the SHIPPED defaults otherwise -- which means the ruled proximity
# identity join, the thing the picture is for.
set -u
REPO=/e/Projects/NifskopeWildWastelandEdition
A=E:/Projects/NifskopeWildWastelandEdition/scratchpad/cellview2b_20260919/lodibake
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
rm -rf "$REPO/scratchpad/cellview2b_20260919/lodibake"
mkdir -p "$REPO/scratchpad/cellview2b_20260919/lodibake/nat"
cd "$REPO" || exit 9
echo "bake start $(date +%H:%M:%S)"
./release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C \
	--terrain-region -20 4 -17 7 --dim 4 --data-root "$DATA" \
	--out-dir "$A" --native "$A/nat" --road-detail 1 \
	> scratchpad/cellview2b_20260919/lodibake.log 2>&1
echo "bake rc=$? $(date +%H:%M:%S)"
grep -E "^native: |^native-identity-join: |^native-scrappable: " scratchpad/cellview2b_20260919/lodibake.log
find scratchpad/cellview2b_20260919/lodibake -name "*.lodi" -printf "%s %p\n"
