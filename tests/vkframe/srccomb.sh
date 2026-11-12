#!/bin/bash
# Lane VKFRAME1 (2026-11-12, P2 + P3): the bloom's source (bloom_src.hlsl) and combine (bloom_comb.hlsl) on Vulkan against
# the GL programs (cell_bloom_src.frag + the resolve blit, cell_bloom_comb.frag) on the same inputs.
# bash tests/vkframe/srccomb.sh <out dir> [<WW_CELL_HDR_DUMP .hdr>]
# Needs MSYS2 UCRT64 g++ (Vulkan headers), DXC (DXC= to override) and python with numpy.
# Green: every job SRCMATCH EXACT or BOUND (1 fp16 ulp) and COMBMATCH EXACT or BOUND (sparse dispersed-channel filter steps,
# see srccombmatch.cpp); control syn_E (no dispersion) COMBMATCH EXACT. Reds BLOOM_SRC_RED (no stencil test) and
# BLOOM_COMB_RED (no dispersion) must FAIL job real_A. Last line: `VKFRAME SRCCOMB GATE OK` or `... FAILED`.
set -u
OUT="${1:?out dir}"
HDR="${2:-X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH: its python has no numpy
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p "$OUT"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
H="$ROOT/res/shaders/hlsl/post"
g++ -O1 -std=c++17 "$ROOT/tests/vkframe/srccombmatch.cpp" -o "$OUT/srccombmatch.exe" || { echo "VKFRAME SRCCOMB GATE FAILED (g++)"; exit 1; }
sp() { "$DXC" -spirv -fspv-target-env=vulkan1.2 "$@" > /dev/null || { echo "VKFRAME SRCCOMB GATE FAILED (dxc $*)"; exit 1; }; }
for k in src comb; do
	R="BLOOM_$(echo $k | tr a-z A-Z)_RED"
	sp -T cs_6_0 -E main -Fo "$(W "$OUT/bloom_$k.spv")" "$(W "$H/bloom_$k.hlsl")"
	sp -T cs_6_0 -E main -D "$R" -Fo "$(W "$OUT/bloom_${k}_red.spv")" "$(W "$H/bloom_$k.hlsl")"
done
sp -T vs_6_0 -E vs -Fo "$(W "$OUT/gen_vs.spv")" "$(W "$ROOT/tests/vkframe/srccomb_gen.hlsl")"
sp -T ps_6_0 -E ps -Fo "$(W "$OUT/gen_ps.spv")" "$(W "$ROOT/tests/vkframe/srccomb_gen.hlsl")"
"$PY" -I "$ROOT/tests/vkframe/srccomb_gl_ref.py" "$(W "$OUT/jobs")" "$HDR" < /dev/null 2>/dev/null | grep -E "^(job|SRCCOMB)" || true
[ -f "$OUT/jobs/real_A/job.txt" ] || { echo "VKFRAME SRCCOMB GATE FAILED (gl reference)"; exit 1; }
rc=0
cd "$OUT"
for j in real_A real_B syn_A syn_C syn_D syn_E; do
	./srccombmatch.exe "jobs/$j" bloom_src.spv bloom_comb.spv gen_vs.spv gen_ps.spv > "green_$j.txt"; g=$?
	grep -E "^(time|SRCMATCH|COMBMATCH|source|combine)" "green_$j.txt"
	[ $g -eq 0 ] || { echo "GREEN $j FAILED"; tail -5 "green_$j.txt"; rc=1; }
done
grep -q "COMBMATCH syn_E EXACT" green_syn_E.txt || { echo "CONTROL syn_E (no dispersion) NOT EXACT"; rc=1; }
[ -f jobs/real_A/gl_ms.txt ] && echo "GL real_A: $(tr '\n' ' ' < jobs/real_A/gl_ms.txt)"
./srccombmatch.exe jobs/real_A bloom_src_red.spv bloom_comb.spv gen_vs.spv gen_ps.spv > red_src.txt
echo "red BLOOM_SRC_RED: $(grep SRCMATCH red_src.txt)"
grep -q "SRCMATCH real_A FAIL" red_src.txt || { echo "RED src DID NOT FAIL"; rc=1; }
./srccombmatch.exe jobs/real_A bloom_src.spv bloom_comb_red.spv gen_vs.spv gen_ps.spv > red_comb.txt
echo "red BLOOM_COMB_RED: $(grep COMBMATCH red_comb.txt)"
grep -q "COMBMATCH real_A FAIL" red_comb.txt || { echo "RED comb DID NOT FAIL"; rc=1; }
if [ $rc -eq 0 ]; then echo "VKFRAME SRCCOMB GATE OK"; else echo "VKFRAME SRCCOMB GATE FAILED"; exit 1; fi
