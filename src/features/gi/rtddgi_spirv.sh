#!/bin/bash
# Lane RTXGI1: compile the DDGI volume's compute kernels (src/rtddgi/NAME.comp) to SPIR-V C arrays in src/rtddgi/NAME.spv.h,
# the way src/features/gi/bakert_spirv.sh does for the bake tracer. Commit the headers, so a build needs no shader compiler; run
# this after editing a .comp or ddgi_common.glsl. Without the headers src/features/gi/rtddgi/rtddgi_vk.inc compiles to a stub and
# the volume runs on its CPU twin (the census line says so).
# GLSLANG = glslangValidator (BSD/Apache; RenderDoc ships one), default the RenderDoc 1.45 copy on this machine.
set -eu
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
G="${GLSLANG:-/e/Tools/RenderDoc/RenderDoc_1.45_64/plugins/spirv/glslangValidator.exe}"
# lane VKCOMPUTE1: the kernels converted to HLSL (res/shaders/hlsl/rtddgi/NAME.hlsl) build with DXC; the .comp stays as the
# original tests/hlslmatch compares them with. vulkan1.2 = bakert.cpp's device version (SPIR-V 1.5, as the glslang ones)
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
HLSL="$ROOT/res/shaders/hlsl/rtddgi"
cd "$ROOT/src/features/gi/rtddgi"
# lane RESTIRDI1: restir (ReSTIR DI on the volume's BVH, src/features/gi/rtddgi/restir_vk.inc; it includes restir_common.glsl)
# MODULES1 RESUME 15: airrays (the lamps' shadow in the air, route b of RESEARCH_AIR.md; src/features/gi/rtddgi/airrays_vk.inc)
for k in trace blend reloc restir airrays; do
	"$G" --target-env vulkan1.2 -V --vn "rtddgi_${k}_spv" -o "${k}.spv.h" "${k}.comp"
done
for k in border; do
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -Fo "${k}.hlsl.spv" "$(cygpath -m "$HLSL/${k}.hlsl" 2>/dev/null || echo "$HLSL/${k}.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "${k}.hlsl.spv" "rtddgi_${k}_spv" "${k}.spv.h" < /dev/null
	rm -f "${k}.hlsl.spv"
done
ls -la trace.spv.h blend.spv.h border.spv.h reloc.spv.h restir.spv.h airrays.spv.h
