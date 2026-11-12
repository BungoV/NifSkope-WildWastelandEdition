#!/bin/bash
# Lane VKCOMPUTE1 (2026-11-12): a converted kernel against its GLSL original, on the GPU, byte for byte.
# bash tests/hlslmatch/run.sh <out dir> [kernel]   (kernel: border reloc blend ...; bvh rq rqx = bakert's). Needs MSYS2
# UCRT64 g++ (Vulkan headers), DXC and glslangValidator (DXC / GLSLANG to override). Green: the kernel's HLSL; red: the same
# with -D <KERNEL>_RED, which must FAIL. DXC runs -O0: its optimizer turns a / b into a * (1 / b) and min/max into NMin/NMax
# (blend: 3-44 ulp off glslang's); unoptimized, the float order is the source's. The entry's `// hlsl profile: cs_6_N`
# line picks the shader model (ray queries need cs_6_5). Last line: `HLSLMATCH GATE OK` or `HLSLMATCH GATE FAILED`.
set -u
OUT="${1:?out dir}"
K="${2:-border}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
G="${GLSLANG:-/e/Tools/RenderDoc/RenderDoc_1.45_64/plugins/spirv/glslangValidator.exe}"
case "$K" in
	bvh|rq|rqx) SET=bakert ;;
	*) SET=rtddgi ;;
esac
ENV=vulkan1.2
[ "$K" = bvh ] && ENV=vulkan1.1
mkdir -p "$OUT"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
# a kernel built from another's source with a define (restirgi_filt = restirgi.comp / .hlsl with -DRESTIRGI_FILTER)
SRC="$K"
DEF=()
[ "$K" = restirgi_filt ] && { SRC=restirgi; DEF=(-DRESTIRGI_FILTER); }
RED="$(echo "$SRC" | tr a-z A-Z)_RED"
H="$ROOT/res/shaders/hlsl/$SET/$SRC.hlsl"
T="$(grep -o 'hlsl profile: cs_6_[0-9]' "$H" | head -1 | awk '{print $3}')"
T="${T:-cs_6_0}"
g++ -O1 -std=c++17 "$ROOT/tests/hlslmatch/kernelmatch.cpp" -o "$OUT/kernelmatch.exe" || { echo "HLSLMATCH GATE FAILED (g++)"; exit 1; }
( cd "$ROOT/src/features/gi/$SET" && "$G" --target-env "$ENV" -V "${DEF[@]}" -o "$(W "$OUT/${K}_glsl.spv")" "$SRC.comp" > /dev/null ) \
	|| { echo "HLSLMATCH GATE FAILED (glslang)"; exit 1; }
"$DXC" -spirv -fspv-target-env="$ENV" -T "$T" -E main -O0 "${DEF[@]}" -Fo "$(W "$OUT/${K}_hlsl.spv")" "$(W "$H")" || { echo "HLSLMATCH GATE FAILED (dxc)"; exit 1; }
"$DXC" -spirv -fspv-target-env="$ENV" -T "$T" -E main -O0 "${DEF[@]}" -D "$RED" -Fo "$(W "$OUT/${K}_red.spv")" "$(W "$H")" || { echo "HLSLMATCH GATE FAILED (dxc red)"; exit 1; }
cd "$OUT"
./kernelmatch.exe "$K" "${K}_glsl.spv" "${K}_hlsl.spv" > green.txt; g=$?
./kernelmatch.exe "$K" "${K}_glsl.spv" "${K}_red.spv" > red.txt; r=$?
cat green.txt
echo "red ($RED):"; tail -1 red.txt
if [ $g -eq 0 ] && [ $r -ne 0 ]; then echo "HLSLMATCH GATE OK"; else echo "HLSLMATCH GATE FAILED"; exit 1; fi
