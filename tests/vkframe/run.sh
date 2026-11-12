#!/bin/bash
# Lane VKFRAME1 (2026-11-12, item 3): the bloom chain on Vulkan (the Fusion Shaders HLSL, res/shaders/hlsl/post) against the
# GL chain (the tree's cell_bloom_*.frag) on dumped inputs, half-float bits compared level by level.
# bash tests/vkframe/run.sh <out dir> [<WW_CELL_HDR_DUMP .hdr>]
# Needs MSYS2 UCRT64 g++ (Vulkan headers), DXC (DXC= to override) and python with numpy.
# Green: every job EXACT (or BOUND: max 1 fp16 ulp, "fp16 rounding of a float-order difference"). Reds (BLOOM_DOWN_RED,
# BLOOM_HOLD_RED, BLOOM_UP_RED) must FAIL job real_A. Last line: `VKFRAME BLOOM GATE OK` or `VKFRAME BLOOM GATE FAILED`.
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
g++ -O1 -std=c++17 "$ROOT/tests/vkframe/bloommatch.cpp" -o "$OUT/bloommatch.exe" || { echo "VKFRAME BLOOM GATE FAILED (g++)"; exit 1; }
for k in down hold up; do
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -Fo "$(W "$OUT/bloom_$k.spv")" "$(W "$H/bloom_$k.hlsl")" > /dev/null \
		|| { echo "VKFRAME BLOOM GATE FAILED (dxc $k)"; exit 1; }
	R="BLOOM_$(echo $k | tr a-z A-Z)_RED"
	"$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -D "$R" -Fo "$(W "$OUT/bloom_${k}_red.spv")" "$(W "$H/bloom_$k.hlsl")" > /dev/null \
		|| { echo "VKFRAME BLOOM GATE FAILED (dxc $k red)"; exit 1; }
done
"$PY" -I "$ROOT/tests/vkframe/bloom_gl_ref.py" "$(W "$OUT/jobs")" "$HDR" < /dev/null 2>/dev/null | grep -v Warning || true
[ -f "$OUT/jobs/real_A/job.txt" ] || { echo "VKFRAME BLOOM GATE FAILED (gl reference)"; exit 1; }
rc=0
cd "$OUT"
for j in real_A real_B syn_A syn_B; do
	./bloommatch.exe "jobs/$j" bloom_down.spv bloom_hold.spv bloom_up.spv > "green_$j.txt"; g=$?
	grep -E "^(time|BLOOMMATCH|device)" "green_$j.txt"
	[ $g -eq 0 ] || { echo "GREEN $j FAILED"; grep -v " 0 of " "green_$j.txt" | head -20; rc=1; }
done
for k in down hold up; do
	case $k in down) a="bloom_down_red.spv bloom_hold.spv bloom_up.spv" ;; hold) a="bloom_down.spv bloom_hold_red.spv bloom_up.spv" ;;
		up) a="bloom_down.spv bloom_hold.spv bloom_up_red.spv" ;; esac
	./bloommatch.exe jobs/real_A $a > "red_$k.txt"; r=$?
	echo "red BLOOM_$(echo $k | tr a-z A-Z)_RED: $(tail -1 red_$k.txt)"
	[ $r -ne 0 ] || { echo "RED $k DID NOT FAIL"; rc=1; }
done
if [ $rc -eq 0 ]; then echo "VKFRAME BLOOM GATE OK"; else echo "VKFRAME BLOOM GATE FAILED"; exit 1; fi
