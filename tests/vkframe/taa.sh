#!/bin/bash
# Lane VKFRAME1 (2026-11-12, P5): the Modern temporal AA resolve (res/shaders/hlsl/post/taa_resolve.hlsl) on Vulkan against
# GL's taa2.frag on the same inputs.
# bash tests/vkframe/taa.sh <out dir> [<WW_CELL_HDR_DUMP .hdr>]
# Needs MSYS2 UCRT64 g++ (Vulkan headers), DXC (DXC= to override) and python with numpy.
# Green: every job TAAMATCH EXACT or BOUND. Red TAA_RESOLVE_RED (no luma weights) must FAIL real_A.
# Last line: `VKFRAME TAA GATE OK` or `... FAILED`.
set -u
OUT="${1:?out dir}"
HDR="${2:-X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH: its python has no numpy
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p "$OUT"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
K="$ROOT/res/shaders/hlsl/post/taa_resolve.hlsl"
g++ -O1 -std=c++17 "$ROOT/tests/vkframe/taamatch.cpp" -o "$OUT/taamatch.exe" || { echo "VKFRAME TAA GATE FAILED (g++)"; exit 1; }
sp() { "$DXC" -spirv -fspv-target-env=vulkan1.2 -O0 "$@" > /dev/null || { echo "VKFRAME TAA GATE FAILED (dxc $*)"; exit 1; }; }
sp -T cs_6_0 -E main -Fo "$(W "$OUT/taa.spv")" "$(W "$K")"
sp -T cs_6_0 -E main -D TAA_RESOLVE_RED -Fo "$(W "$OUT/taa_red.spv")" "$(W "$K")"
"$DXC" -T cs_6_0 -E main -Fo "$(W "$OUT/taa.dxil")" "$(W "$K")" > /dev/null || { echo "VKFRAME TAA GATE FAILED (dxil)"; exit 1; }
"$PY" -I "$ROOT/tests/vkframe/taa_gl_ref.py" "$(W "$OUT/jobs")" "$HDR" < /dev/null 2>/dev/null | grep -E "^(job|real|TAA)" || true
[ -f "$OUT/jobs/real_A/job.txt" ] || { echo "VKFRAME TAA GATE FAILED (gl reference)"; exit 1; }
rc=0
cd "$OUT"
for j in real_A real_H real_M real_L real_N real_X real_Y syn_A syn_M; do
	./taamatch.exe "jobs/$j" taa.spv > "green_$j.txt"; g=$?
	grep -E "^(time|TAAMATCH|o[0-2] )" "green_$j.txt"
	[ $g -eq 0 ] || { echo "GREEN $j FAILED"; rc=1; }
done
./taamatch.exe jobs/real_A taa_red.spv > red.txt
echo "red TAA_RESOLVE_RED: $(grep TAAMATCH red.txt)"
grep -q "TAAMATCH real_A FAIL" red.txt || { echo "RED DID NOT FAIL"; rc=1; }
if [ $rc -eq 0 ]; then echo "VKFRAME TAA GATE OK"; else echo "VKFRAME TAA GATE FAILED"; exit 1; fi
