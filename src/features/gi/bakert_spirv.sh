#!/bin/bash
# Lane RTBAKE1: compile the bake tracer's compute shaders (src/bakert/*.comp) to SPIR-V C arrays in src/bakert/*.spv.h.
# The headers are committed, so a build needs no shader compiler; run this after editing a .comp or common.glsl.
# GLSLANG = glslangValidator (BSD/Apache; RenderDoc ships one), default the RenderDoc 1.45 copy on this machine.
set -eu
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
G="${GLSLANG:-/e/Tools/RenderDoc/RenderDoc_1.45_64/plugins/spirv/glslangValidator.exe}"
# lane VKCOMPUTE1: a kernel converted to HLSL (res/shaders/hlsl/bakert/NAME.hlsl) builds with DXC, -O0 (the source's float
# order, proven byte-exact against the .comp by tests/hlslmatch); the .comp stays as that gate's original. The entry's
# `// hlsl profile: cs_6_N` line picks the shader model. hk <kernel> <target env>
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
HLSL="$ROOT/res/shaders/hlsl/bakert"
hk() {
	T="$(grep -o 'hlsl profile: cs_6_[0-9]' "$HLSL/$1.hlsl" | head -1 | awk '{print $3}')"
	"$DXC" -spirv -fspv-target-env="$2" -T "${T:-cs_6_0}" -E main -O0 -Fo "$1.hlsl.spv" "$(cygpath -m "$HLSL/$1.hlsl" 2>/dev/null || echo "$HLSL/$1.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "$1.hlsl.spv" "bakert_$1_spv" "$1.spv.h" < /dev/null
	rm -f "$1.hlsl.spv"
}
cd "$ROOT/src/features/gi/bakert"
hk rq vulkan1.2
hk bvh vulkan1.1
hk rqx vulkan1.2   # lane RTGI1: whole rays, sure/unsure
ls -la rq.spv.h bvh.spv.h rqx.spv.h
