#!/bin/bash
# Lane VKFRAME1 (2026-11-12, P4): the exposure measure (res/shaders/hlsl/exposure/measure_q.hlsl) on Vulkan against GL's
# cell_is_measure_q.frag on the same multisample frame and stencil.
# bash tests/vkframe/measure.sh <out dir> [<WW_CELL_HDR_DUMP .hdr>]
# Needs MSYS2 UCRT64 g++ (Vulkan headers), DXC (DXC= to override) and python with numpy.
# Green: every job MEASUREMATCH EXACT. Red MEASURE_Q_RED (no stencil test, sample 0 only) must FAIL real_A.
# Last line: `VKFRAME MEASURE GATE OK` or `... FAILED`.
set -u
OUT="${1:?out dir}"
HDR="${2:-X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH: its python has no numpy
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p "$OUT"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
K="$ROOT/res/shaders/hlsl/exposure/measure_q.hlsl"
g++ -O1 -std=c++17 "$ROOT/tests/vkframe/measurematch.cpp" -o "$OUT/measurematch.exe" || { echo "VKFRAME MEASURE GATE FAILED (g++)"; exit 1; }
sp() { "$DXC" -spirv -fspv-target-env=vulkan1.2 "$@" > /dev/null || { echo "VKFRAME MEASURE GATE FAILED (dxc $*)"; exit 1; }; }
sp -T cs_6_0 -E main -Fo "$(W "$OUT/measure_q.spv")" "$(W "$K")"
sp -T cs_6_0 -E main -D MEASURE_Q_RED -Fo "$(W "$OUT/measure_q_red.spv")" "$(W "$K")"
sp -T vs_6_0 -E vs -Fo "$(W "$OUT/gen_vs.spv")" "$(W "$ROOT/tests/vkframe/srccomb_gen.hlsl")"
sp -T ps_6_0 -E ps -Fo "$(W "$OUT/gen_ps.spv")" "$(W "$ROOT/tests/vkframe/srccomb_gen.hlsl")"
"$PY" -I "$ROOT/tests/vkframe/measure_gl_ref.py" "$(W "$OUT/jobs")" "$HDR" < /dev/null 2>/dev/null | grep -E "^(job|MEASURE)" || true
[ -f "$OUT/jobs/real_A/job.txt" ] || { echo "VKFRAME MEASURE GATE FAILED (gl reference)"; exit 1; }
rc=0
cd "$OUT"
for j in real_A real_P syn_A; do
	./measurematch.exe "jobs/$j" measure_q.spv gen_vs.spv gen_ps.spv > "green_$j.txt"; g=$?
	grep -E "^(time|MEASUREMATCH|measure )" "green_$j.txt"
	[ $g -eq 0 ] || { echo "GREEN $j FAILED"; tail -5 "green_$j.txt"; rc=1; }
done
[ -f jobs/real_A/gl_ms.txt ] && echo "GL real_A: $(tr '\n' ' ' < jobs/real_A/gl_ms.txt)"
./measurematch.exe jobs/real_A measure_q_red.spv gen_vs.spv gen_ps.spv > red.txt
echo "red MEASURE_Q_RED: $(grep MEASUREMATCH red.txt)"
grep -q "MEASUREMATCH real_A FAIL" red.txt || { echo "RED DID NOT FAIL"; rc=1; }
if [ $rc -eq 0 ]; then echo "VKFRAME MEASURE GATE OK"; else echo "VKFRAME MEASURE GATE FAILED"; exit 1; fi
