#!/bin/bash
# Lane VKFRAME1 (2026-11-12): compile the bloom chain's Fusion Shaders HLSL kernels (res/shaders/hlsl/post/bloom_*.hlsl) to
# SPIR-V C arrays in src/features/vkframe/bloom_NAME.spv.h, as src/features/gi/rtddgi_spirv.sh does for the volume.
# Commit the headers, so a build needs no shader compiler; run this after editing a bloom .hlsl or BloomCommon.hlsli.
# vulkan1.2 = bakert.cpp's device version. DXC= to override the compiler.
set -eu
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
HLSL="$ROOT/res/shaders/hlsl/post"
cd "$ROOT/src/features/vkframe"
for k in down hold up src comb; do
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -Fo "bloom_${k}.spv" "$(cygpath -m "$HLSL/bloom_${k}.hlsl" 2>/dev/null || echo "$HLSL/bloom_${k}.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "bloom_${k}.spv" "vkframe_bloom_${k}_spv" "bloom_${k}.spv.h" < /dev/null
	rm -f "bloom_${k}.spv"
done
# the cell tone map (P6): res/shaders/hlsl/post/tonemap.hlsl -> tonemap.spv.h
for t in tonemap; do
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -Fo "${t}.spv" "$(cygpath -m "$HLSL/${t}.hlsl" 2>/dev/null || echo "$HLSL/${t}.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "${t}.spv" "vkframe_${t}_spv" "${t}.spv.h" < /dev/null
	rm -f "${t}.spv"
done
# the exposure measure (P4): res/shaders/hlsl/exposure/<e>.hlsl -> <e>.spv.h
for e in measure_q; do
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -Fo "${e}.spv" "$(cygpath -m "$ROOT/res/shaders/hlsl/exposure/${e}.hlsl" 2>/dev/null || echo "$ROOT/res/shaders/hlsl/exposure/${e}.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "${e}.spv" "vkframe_${e}_spv" "${e}.spv.h" < /dev/null
	rm -f "${e}.spv"
done
ls -la bloom_down.spv.h bloom_hold.spv.h bloom_up.spv.h bloom_src.spv.h bloom_comb.spv.h tonemap.spv.h measure_q.spv.h
