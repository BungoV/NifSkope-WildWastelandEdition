#!/bin/bash
# Lane VKCOMPUTE1 (2026-11-12): a converted DDGI kernel against its GLSL original, on the GPU, byte for byte.
# bash tests/hlslmatch/run.sh <out dir> [kernel]   (kernel: border). Needs MSYS2 UCRT64 g++ (Vulkan headers), DXC and
# glslangValidator (DXC / GLSLANG to override). Green: the kernel's HLSL; red: the same with -D <KERNEL>_RED, which must FAIL.
# Last line: `HLSLMATCH GATE OK` or `HLSLMATCH GATE FAILED`.
set -u
OUT="${1:?out dir}"
K="${2:-border}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
G="${GLSLANG:-/e/Tools/RenderDoc/RenderDoc_1.45_64/plugins/spirv/glslangValidator.exe}"
mkdir -p "$OUT"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
RED="$(echo "$K" | tr a-z A-Z)_RED"
g++ -O1 -std=c++17 "$ROOT/tests/hlslmatch/kernelmatch.cpp" -o "$OUT/kernelmatch.exe" || { echo "HLSLMATCH GATE FAILED (g++)"; exit 1; }
( cd "$ROOT/src/features/gi/rtddgi" && "$G" --target-env vulkan1.2 -V -o "$(W "$OUT/${K}_glsl.spv")" "$K.comp" > /dev/null ) \
	|| { echo "HLSLMATCH GATE FAILED (glslang)"; exit 1; }
H="$ROOT/res/shaders/hlsl/rtddgi/$K.hlsl"
"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -Fo "$(W "$OUT/${K}_hlsl.spv")" "$(W "$H")" || { echo "HLSLMATCH GATE FAILED (dxc)"; exit 1; }
"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -D "$RED" -Fo "$(W "$OUT/${K}_red.spv")" "$(W "$H")" || { echo "HLSLMATCH GATE FAILED (dxc red)"; exit 1; }
cd "$OUT"
./kernelmatch.exe "$K" "${K}_glsl.spv" "${K}_hlsl.spv" > green.txt; g=$?
./kernelmatch.exe "$K" "${K}_glsl.spv" "${K}_red.spv" > red.txt; r=$?
cat green.txt
echo "red ($RED):"; tail -1 red.txt
if [ $g -eq 0 ] && [ $r -ne 0 ]; then echo "HLSLMATCH GATE OK"; else echo "HLSLMATCH GATE FAILED"; exit 1; fi
