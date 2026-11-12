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
# RTEMIS1 part 2: restirgi (ReSTIR GI, src/features/gi/rtddgi/restirgi_vk.inc; it includes trace_body.glsl, trace.comp's
# helpers): GLSL only, built by glslang (no HLSL twin yet)
for k in restirgi; do
	"$G" --target-env vulkan1.2 -V --vn "rtddgi_${k}_spv" -o "${k}.spv.h" "${k}.comp"
done
# lane VKCOMPUTE1: every kernel now builds from HLSL (restir last, 11-12); $G stays for a kernel that goes back to its .comp
# -O0: DXC's optimizer reorders float math (a / b -> a * (1 / b), NMin/NMax); unoptimized it keeps the source's order, which
# tests/hlslmatch proves byte-exact against the .comp. The entry's `// hlsl profile: cs_6_N` line picks the shader model.
for k in border reloc blend trace airrays restir; do
	T="$(grep -o 'hlsl profile: cs_6_[0-9]' "$HLSL/${k}.hlsl" | head -1 | awk '{print $3}')"
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T "${T:-cs_6_0}" -E main -O0 -Fo "${k}.hlsl.spv" "$(cygpath -m "$HLSL/${k}.hlsl" 2>/dev/null || echo "$HLSL/${k}.hlsl")"
	python -I "$ROOT/tools/spv_header.py" "${k}.hlsl.spv" "rtddgi_${k}_spv" "${k}.spv.h" < /dev/null
	rm -f "${k}.hlsl.spv"
done
# RTEMIS1 11-08: restirgi's modes 2.. as their own kernel (no ray code: the filter and a-trous passes' registers)
"$G" --target-env vulkan1.2 -V -DRESTIRGI_FILTER --vn rtddgi_restirgi_filt_spv -o restirgi_filt.spv.h restirgi.comp
ls -la trace.spv.h blend.spv.h border.spv.h reloc.spv.h restir.spv.h airrays.spv.h restirgi.spv.h restirgi_filt.spv.h
