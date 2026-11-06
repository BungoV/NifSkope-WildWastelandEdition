#!/bin/bash
# Lane RTXGI1: compile the DDGI volume's compute kernels (src/rtddgi/NAME.comp) to SPIR-V C arrays in src/rtddgi/NAME.spv.h,
# the way src/features/gi/bakert_spirv.sh does for the bake tracer. Commit the headers, so a build needs no shader compiler; run
# this after editing a .comp or ddgi_common.glsl. Without the headers src/features/gi/rtddgi/rtddgi_vk.inc compiles to a stub and
# the volume runs on its CPU twin (the census line says so).
# GLSLANG = glslangValidator (BSD/Apache; RenderDoc ships one), default the RenderDoc 1.45 copy on this machine.
set -eu
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
G="${GLSLANG:-/e/Tools/RenderDoc/RenderDoc_1.45_64/plugins/spirv/glslangValidator.exe}"
cd "$ROOT/src/features/gi/rtddgi"
# lane RESTIRDI1: restir (ReSTIR DI on the volume's BVH, src/features/gi/rtddgi/restir_vk.inc; it includes restir_common.glsl)
# MODULES1 RESUME 15: airrays (the lamps' shadow in the air, route b of RESEARCH_AIR.md; src/features/gi/rtddgi/airrays_vk.inc)
# RTEMIS1 part 2: restirgi (ReSTIR GI, src/features/gi/rtddgi/restirgi_vk.inc; it includes trace_body.glsl, trace.comp's helpers)
for k in trace blend border reloc restir airrays restirgi; do
	"$G" --target-env vulkan1.2 -V --vn "rtddgi_${k}_spv" -o "${k}.spv.h" "${k}.comp"
done
ls -la trace.spv.h blend.spv.h border.spv.h reloc.spv.h restir.spv.h airrays.spv.h restirgi.spv.h
