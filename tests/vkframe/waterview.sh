#!/bin/bash
# Lane VKWATER1 (2026-11-13, row G5): the water's census and verdict (vkwater.h waterCensusOf / waterVerdict, vkwater.inc
# waterCaps) against the present device. The inputs come from the shipped HLSL twins (DXC SPIR-V, spirv-cross --reflect):
# every one gets a GL texture of the app's format on its unit; the census must read each back (name, kind, format, levels,
# anisotropy, unit), every input the verdict lets move must be taken by sharedTarget and every one it refuses for a format
# must be refused by sharedTarget; the device caps must match the features vkpresent.cpp enables. Reds must FAIL.
# bash tests/vkframe/waterview.sh <out dir>. Needs MSYS2 UCRT64 g++, E:/Tools/dxc, RenderDoc's spirv-cross.
# Not while NifSkope or the game runs. Last line: `WATERVIEW GATE OK` or `WATERVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY=$(command -v python)
DXC=${DXC:-E:/Tools/dxc/bin/x64/dxc.exe}
SC=${SPIRV_CROSS:-E:/Tools/RenderDoc/RenderDoc_1.45_64/plugins/spirv/spirv-cross.exe}
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "WATERVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
W="$ROOT/res/shaders/hlsl/water"
ARGS=()
# the default Ultra twin, the Medium twin (both as the app injects WW_CELL_FOGLEAK + WW_FRAME_UBO), the Medium base;
# DXC drops cell_lights.glsl's unused texel buffers and cube-map array: waterview.cpp adds those two by their GLSL names
for tw in "ultra=fo4_waterlook_ps__WW_CELL_FOGLEAK+WW_FRAME_UBO" "medium=fo4_waterlookm_ps__WW_CELL_FOGLEAK+WW_FRAME_UBO" \
	"mbase=fo4_waterlookm_ps"; do
	tag=${tw%%=*}; src=${tw#*=}
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -O0 -fspv-preserve-bindings -T ps_6_0 -E main -fvk-b-shift 0 0 -fvk-t-shift 64 0 \
		-fvk-s-shift 128 0 -fvk-u-shift 192 0 -I "$W" -I "$W/.." "$W/$src.hlsl" -Fo "$OUT/$tag.spv" > "$OUT/$tag.dxc.log" 2>&1 \
		|| { echo "WATERVIEW GATE FAILED (dxc $src)"; exit 1; }
	"$SC" "$OUT/$tag.spv" --reflect --output "$OUT/$tag.json" > /dev/null 2>&1 || { echo "WATERVIEW GATE FAILED (reflect $tag)"; exit 1; }
	ARGS+=("$tag=$OUT/$tag.json")
done
"$PY" -I "$ROOT/tests/vkframe/waterview_prep.py" "$OUT/slots.txt" "${ARGS[@]}" || { echo "WATERVIEW GATE FAILED (prep)"; exit 1; }
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/waterview.cpp" -o "$OUT/waterview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "WATERVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/waterview.exe" "$ROOT" "$OUT/slots.txt" 2>&1 | tee "$OUT/waterview.txt"
tail -1 "$OUT/waterview.txt" | grep -q "WATERVIEW GATE OK"
