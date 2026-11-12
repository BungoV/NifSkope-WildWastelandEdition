#!/bin/bash
# Lane VKFRAME1 (2026-11-12, round 8): the TAA copy-back, motion vectors, MS resolve and RCAS, the clouds' front composite and
# the lens-flare sprites on Vulkan (res/shaders/hlsl/post/taa_*, cloud_front, lens_flare) against the GL .frag/.vert on the
# same inputs (tests/vkframe/post2_gl_ref.py).
# bash tests/vkframe/post2.sh <out dir> [<WW_CELL_HDR_DUMP .hdr>]      TIME=1 also times (warm, against GL) and runs tm_*
# Needs MSYS2 UCRT64 g++ (Vulkan headers), DXC (DXC= to override) and python with numpy.
# Green: every job POSTFXMATCH EXACT or BOUND as compute kernels and as fragment passes (POSTFXMATCH_RASTER=1); raster_only
# jobs (blended draws, the flare's vertex shader) as fragment passes only. Every kernel's PFX_RED build FAILS its job.
# Last line: `VKFRAME POST2 GATE OK` or `... FAILED`.
set -u
OUT="${1:?out dir}"
HDR="${2:-X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH: its python has no numpy
TIME="${TIME:-0}"
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p "$OUT/spv"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
H="$ROOT/res/shaders/hlsl/post"
g++ -O1 -std=c++17 "$ROOT/tests/vkframe/postfxmatch.cpp" -o "$OUT/postfxmatch.exe" || { echo "VKFRAME POST2 GATE FAILED (g++)"; exit 1; }
spr() { "$DXC" -spirv -fspv-target-env=vulkan1.2 "$@" > /dev/null || { echo "VKFRAME POST2 GATE FAILED (dxc $*)"; exit 1; }; }
# kernel name, source, defines (a comma between defines)
K='taa_copy:taa_copy.hlsl: taa_copy8:taa_copy.hlsl:-DPFX_OUT0_FORMAT="rgba8" taa_mv:taa_mv.hlsl: taa_msr:taa_msresolve.hlsl:
taa_rcas:taa_rcas.hlsl: taa_rcas8:taa_rcas.hlsl:-DPFX_OUT0_FORMAT="rgba8" cloud_front:cloud_front.hlsl:'
for k in $K; do
	n="${k%%:*}"; r="${k#*:}"; f="${r%%:*}"; d="${r#*:}"
	for red in "" _red; do
		rd=""; [ -n "$red" ] && rd="-D PFX_RED"
		spr -T cs_6_0 -E main $rd ${d//,/ } -Fo "$(W "$OUT/spv/$n$red.spv")" "$(W "$H/$f")"
		spr -T ps_6_0 -E ps -D PFX_RASTER $rd ${d//,/ } -Fo "$(W "$OUT/spv/$n${red}_ps.spv")" "$(W "$H/$f")"
	done
done
# the flare: its own vertex and fragment entries (no compute form); the harness names the fragment entry ps
for red in "" _red; do
	rd=""; [ -n "$red" ] && rd="-D PFX_RED"
	spr -T vs_6_0 -E vs $rd -Fo "$(W "$OUT/spv/lens_flare${red}_vs.spv")" "$(W "$H/lens_flare.hlsl")"
	spr -T ps_6_0 -E main -fspv-entrypoint-name=ps $rd -Fo "$(W "$OUT/spv/lens_flare${red}_ps.spv")" "$(W "$H/lens_flare.hlsl")"
done
spr -T vs_6_0 -E vs -D PFX_RASTER -Fo "$(W "$OUT/spv/pfx_vs.spv")" "$(W "$H/taa_copy.hlsl")"
spr -T ps_6_0 -E ps -Fo "$(W "$OUT/spv/ms_expand_ps.spv")" "$(W "$ROOT/tests/vkframe/ms_expand.hlsl")"
targ=""; [ "$TIME" = 1 ] && targ=time
"$PY" -I "$ROOT/tests/vkframe/post2_gl_ref.py" "$(W "$OUT/jobs")" "$HDR" $targ < /dev/null 2>&1 | grep -E "^(real|job|POST2|COMPILE|Traceback|  |[A-Za-z]+Error)" || true
[ -f "$OUT/jobs/flare_A/chain.txt" ] || { echo "VKFRAME POST2 GATE FAILED (gl reference)"; exit 1; }
rc=0
cd "$OUT"
jobs_=""
for d in jobs/*/; do
	j="$(basename "$d")"
	case "$j" in tm_*) continue ;; esac
	[ -f "jobs/$j/chain.txt" ] || continue	# in_real: the inputs
	jobs_="$jobs_ $j"
done
nj=0
for j in $jobs_; do
	nj=$((nj + 1))
	t=""; [ "$TIME" = 1 ] && t=time
	if [ ! -f "jobs/$j/raster_only" ]; then
		./postfxmatch.exe "jobs/$j/chain.txt" spv $t > "green_$j.txt"; g=$?
		grep -vE "^device" "green_$j.txt"
		[ $g -eq 0 ] || { echo "GREEN $j FAILED"; rc=1; }
	fi
	POSTFXMATCH_RASTER=1 ./postfxmatch.exe "jobs/$j/chain.txt" spv $t > "raster_$j.txt"; g=$?
	grep -vE "^device" "raster_$j.txt"
	[ $g -eq 0 ] || { echo "RASTER $j FAILED"; rc=1; }
done
if [ "$TIME" = 1 ]; then
	for d in jobs/tm_*/; do
		j="$(basename "$d")"
		POSTFXMATCH_RASTER=1 ./postfxmatch.exe "jobs/$j/chain.txt" spv time > "raster_$j.txt"
		grep -E "^time" "raster_$j.txt"
	done
fi
nred=0
for r in jobs/*/red_*.txt; do
	nred=$((nred + 1))
	k="$(basename "$r" .txt)"; j="$(basename "$(dirname "$r")")"
	if [ ! -f "jobs/$j/raster_only" ]; then
		./postfxmatch.exe "$r" spv > "$j.$k.txt"
		echo "$k on $j: $(grep POSTFXMATCH "$j.$k.txt")"
		grep -q "POSTFXMATCH .* FAIL" "$j.$k.txt" || { echo "RED $k DID NOT FAIL"; rc=1; }
	fi
	POSTFXMATCH_RASTER=1 ./postfxmatch.exe "$r" spv > "rasterred_$j.$k.txt"
	echo "raster $k on $j: $(grep POSTFXMATCH "rasterred_$j.$k.txt")"
	grep -q "POSTFXMATCH .* FAIL" "rasterred_$j.$k.txt" || { echo "RASTER RED $k DID NOT FAIL"; rc=1; }
done
echo "jobs run: $nj; reds run: $nred (8 kernels, cloud_front twice)"
[ "$nred" -eq 9 ] || rc=1
if [ $rc -eq 0 ]; then echo "VKFRAME POST2 GATE OK"; else echo "VKFRAME POST2 GATE FAILED"; exit 1; fi
