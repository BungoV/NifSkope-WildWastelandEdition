#!/bin/bash
# Lane VKFRAME1 round 9: the overlay target against the present blit, timing only (plan.md section 5a items 8-11).
# bash tests/vkframe/overlay.sh <post2 out dir> (run TIME=1 post2.sh into it first: its spv, inputs and postfxmatch.exe).
# Vulkan (raster, one chain): copyms = the copy-back into the 4x MS frame (today), resolve + blit = the present's two
# halves on Vulkan, copyss = the copy-back straight into the swapchain image, ovclr = the overlay clear, ovcomp = the
# overlay composited premultiplied (blend=pre) over it. GL: overlay_gl_ref.py (present_blit, ov_clear, ov_clear_ms4).
# Quiet GPU only (under 5 %, no dxc / hlsl_check / build_all).
set -u
OUT="${1:?post2 out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="$(command -v python)"
J="$OUT/jobs/ov_frame"
mkdir -p "$J"
cp "$OUT/jobs/tm_frame/pc_0.bin" "$J/pc_0.bin" || { echo "OVERLAY FAILED (no tm_frame job: TIME=1 post2.sh first)"; exit 1; }
printf '%s\n' 'img src8 rgba8 1920 1045 in_real/cur8.bin' 'img ovs rgba8 1920 1045 -' 'img fr rgba8 1920 1045 - ms4' \
	'img ring rgba8 1920 1045 -' 'img sc rgba8 1920 1045 -' 'pass copyms taa_copy8.spv pc_0.bin fr - src8 - - - -' \
	'pass resolve resolve - ring - fr - - - -' 'pass blit blit - sc - ring - - - -' \
	'pass copyss taa_copy8.spv pc_0.bin sc - src8 - - - -' 'pass ovclr clear - ovs - - - - - -' \
	'pass ovcomp taa_copy8.spv pc_0.bin sc - ovs - - - - blend=pre' > "$J/chain.txt"
cd "$OUT"
for r in 1 2 3; do
	"$PY" -I "$ROOT/tests/vkframe/overlay_gl_ref.py" "$(cygpath -m "$OUT/jobs")" < /dev/null 2>&1 | tail -1
	POSTFXMATCH_RASTER=1 ./postfxmatch.exe jobs/ov_frame/chain.txt spv time | grep -E '^time'
done
