#!/usr/bin/env bash
# The bake byte-identity gate of lane NATIVEVIEW1.
#
# This lane touched readers only, so the SAME one-chunk bake run by the exe
# before the lane and by the exe after it must produce byte-identical files.
# Usage: bash bake_identity.sh <exe> <out-root>
set -u
EXE="$1"
O="$2"
mkdir -p "$O/obj" "$O/tex" "$O/mod" "$O/native"
"$EXE" -no-gui lodgen \
	"X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" \
	--worldspace 3C --terrain-region -20 24 -17 27 --dim 4 \
	--out-dir "$O/obj" --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" \
	--vt "$O/mod" --tex-dir "$O/tex" \
	--native "$O/native" --native-mesh-report "$O/native/mesh_report.txt" \
	--land-guide aspecthex --land-guide-scale 256 --land-hex 256 \
	--terrain-object-ao --erosion 1 --erosion-iterations 4 --erosion-seed 7 \
	--msn-cache "E:/Tools/Upscale/esrgan-bat/output" --sheet-format legacy \
	--road-detail 1 --cover --arrays --atlas \
	--no-terrain-identity \
	> "$O/bake.log" 2>&1
echo "exit $?"
