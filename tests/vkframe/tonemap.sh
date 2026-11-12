#!/bin/bash
# Lane VKFRAME1 (2026-11-12, P6): the cell view's tone map (tonemap.hlsl) on Vulkan against the GL program (cell_hdr.frag,
# whole) on the same inputs, every mode, per sample. bash tests/vkframe/tonemap.sh <out dir> [<WW_CELL_HDR_DUMP .hdr>]
# Needs MSYS2 UCRT64 g++ (Vulkan headers), DXC (DXC= to override) and python with numpy.
# Each job runs the compute kernel and the fragment pass (tonemap_raster.hlsl, the way GL draws it).
# Green: every job's TONEMATCH EXACT or BOUND, compute and fragment pass both (tonemapmatch.cpp's return code gates both;
# TONEMATCH_VKVS=1 also diffs the two Vulkan routes float by float). Red TONEMAP_RED (no LUT) must FAIL job real_A
# both ways. Then the floor: TONEMAP_NO_WORK (load + store only) both ways on real_A, timed (its compare fails by design).
# Last line: `VKFRAME TONEMAP GATE OK` or `... FAILED`.
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
g++ -O1 -std=c++17 "$ROOT/tests/vkframe/tonemapmatch.cpp" -o "$OUT/tonemapmatch.exe" || { echo "VKFRAME TONEMAP GATE FAILED (g++)"; exit 1; }
sp() { "$DXC" -spirv -fspv-target-env=vulkan1.2 "$@" > /dev/null || { echo "VKFRAME TONEMAP GATE FAILED (dxc $*)"; exit 1; }; }
sp -T cs_6_0 -E main -Fo "$(W "$OUT/tonemap.spv")" "$(W "$H/tonemap.hlsl")"
sp -T cs_6_0 -E main -D TONEMAP_RED -Fo "$(W "$OUT/tonemap_red.spv")" "$(W "$H/tonemap.hlsl")"
sp -T cs_6_0 -E main -D TONEMAP_NO_WORK -Fo "$(W "$OUT/tonemap_nowork.spv")" "$(W "$H/tonemap.hlsl")"
sp -T vs_6_0 -E vs -Fo "$(W "$OUT/raster_vs.spv")" "$(W "$H/tonemap_raster.hlsl")"
sp -T ps_6_0 -E main -Fo "$(W "$OUT/raster_ps.spv")" "$(W "$H/tonemap_raster.hlsl")"
sp -T ps_6_0 -E main -D TONEMAP_RED -Fo "$(W "$OUT/raster_red.spv")" "$(W "$H/tonemap_raster.hlsl")"
sp -T ps_6_0 -E main -D TONEMAP_NO_WORK -Fo "$(W "$OUT/raster_nowork.spv")" "$(W "$H/tonemap_raster.hlsl")"
sp -T cs_6_0 -E main -Fo "$(W "$OUT/unpack.spv")" "$(W "$ROOT/tests/vkframe/tonemap_unpack.hlsl")"
sp -T vs_6_0 -E vs -Fo "$(W "$OUT/gen_vs.spv")" "$(W "$ROOT/tests/vkframe/srccomb_gen.hlsl")"
sp -T ps_6_0 -E ps -Fo "$(W "$OUT/gen_ps.spv")" "$(W "$ROOT/tests/vkframe/srccomb_gen.hlsl")"
if [ ! -f "$OUT/jobs/real_A/params.txt" ] || [ "${TONEMAP_REF:-1}" = 1 ]; then
	"$PY" -I "$ROOT/tests/vkframe/tonemap_gl_ref.py" "$(W "$OUT/jobs")" "$HDR" < /dev/null 2>/dev/null | grep -E "^(job|TONEMAP)" || true
fi
[ -f "$OUT/jobs/real_A/params.txt" ] || { echo "VKFRAME TONEMAP GATE FAILED (gl reference)"; exit 1; }
rc=0
cd "$OUT"
for j in real_A real_B real_C syn_A syn_B syn_C syn_D syn_E syn_F; do
	./tonemapmatch.exe "jobs/$j" tonemap.spv gen_vs.spv gen_ps.spv raster_vs.spv raster_ps.spv unpack.spv > "green_$j.txt"; g=$?
	grep -E "^(time|tone |TONEMATCH|no VK)" "green_$j.txt"
	[ $g -eq 0 ] || { echo "GREEN $j FAILED"; tail -8 "green_$j.txt"; rc=1; }
done
echo "raster route: $(cat green_*.txt | grep -cE "raster (EXACT|BOUND)") of 9 jobs match"
[ -f jobs/real_A/gl_ms.txt ] && echo "GL real_A: $(tr '\n' ' ' < jobs/real_A/gl_ms.txt)"
./tonemapmatch.exe jobs/real_A tonemap_red.spv gen_vs.spv gen_ps.spv raster_vs.spv raster_red.spv unpack.spv > red.txt
echo "red TONEMAP_RED: $(grep TONEMATCH red.txt | tr '\n' ' ')"
grep -q "TONEMATCH real_A FAIL" red.txt || { echo "RED DID NOT FAIL (compute)"; rc=1; }
grep -q "TONEMATCH real_A raster FAIL" red.txt || { echo "RED DID NOT FAIL (raster)"; rc=1; }
./tonemapmatch.exe jobs/real_A tonemap_nowork.spv gen_vs.spv gen_ps.spv raster_vs.spv raster_nowork.spv unpack.spv > nowork.txt
echo "floor TONEMAP_NO_WORK:"; grep -E "^time" nowork.txt
if [ $rc -eq 0 ]; then echo "VKFRAME TONEMAP GATE OK"; else echo "VKFRAME TONEMAP GATE FAILED"; exit 1; fi
