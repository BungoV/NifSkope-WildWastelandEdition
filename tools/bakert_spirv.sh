#!/bin/bash
# Lane RTBAKE1: compile the bake tracer's compute shaders (src/bakert/*.comp) to SPIR-V C arrays in src/bakert/*.spv.h.
# The headers are committed, so a build needs no shader compiler; run this after editing a .comp or common.glsl.
# GLSLANG = glslangValidator (BSD/Apache; RenderDoc ships one), default the RenderDoc 1.45 copy on this machine.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
G="${GLSLANG:-/e/Tools/RenderDoc/RenderDoc_1.45_64/plugins/spirv/glslangValidator.exe}"
cd "$ROOT/src/bakert"
"$G" --target-env vulkan1.2 -V --vn bakert_rq_spv -o rq.spv.h rq.comp
"$G" --target-env vulkan1.1 -V --vn bakert_bvh_spv -o bvh.spv.h bvh.comp
"$G" --target-env vulkan1.2 -V --vn bakert_rqx_spv -o rqx.spv.h rqx.comp   # lane RTGI1: whole rays, sure/unsure
ls -la rq.spv.h bvh.spv.h rqx.spv.h
