#!/bin/bash
# Lane VKLIGHT1 (2026-11-12): the lighting passes on Vulkan compute (res/shaders/hlsl/vklight/*.hlsl) against the GL .frag
# chains on the same inputs (tests/vklight/light_gl_ref.py, a hidden GL context; tests/vklight/lightmatch.cpp, a headless
# Vulkan device). Copied from VKFRAME1's tests/vkframe/postfx.sh.
# bash tests/vklight/light.sh <out dir> [<WW_CELL_HDR_DUMP .hdr>] [<pass> ...]     (passes: ao; default all)
# VKLIGHT_NOTIME=1 skips every timing (gate only, while another job loads the GPU).
# Needs MSYS2 UCRT64 g++ (Vulkan headers), DXC (DXC= to override) and python with numpy.
# Green: every job LIGHTMATCH EXACT or BOUND; each kernel's red build must FAIL its job. Last line `VKLIGHT GATE OK` / `FAILED`.
set -u
OUT="${1:?out dir}"
HDR="${2:-X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr}"
shift; [ $# -gt 0 ] && shift
PASSES="${*:-ao}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH: its python has no numpy
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p "$OUT/spv"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
H="$ROOT/res/shaders/hlsl/vklight"
g++ -O1 -std=c++17 "$ROOT/tests/vklight/lightmatch.cpp" -o "$OUT/lightmatch.exe" || { echo "VKLIGHT GATE FAILED (g++)"; exit 1; }
sp() { "$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main -O0 "$@" > /dev/null || { echo "VKLIGHT GATE FAILED (dxc $*)"; exit 1; }; }
# kernel name : source : defines (comma separated) : red define
K=""
JOBS=""
NRED=0
for p in $PASSES; do
	case $p in
	ao) K="$K ao_gbuf:ao_vk.hlsl:-DAO_STAGE=5:AO_GBUF_RED ao_mip:ao_vk.hlsl:-DAO_STAGE=1:AO_MIP_RED ao_raw:ao_vk.hlsl:-DAO_STAGE=2:AO_RAW_RED
		ao_blur:ao_vk.hlsl:-DAO_STAGE=3:AO_BLUR_RED ao_blurf:ao_vk.hlsl:-DAO_STAGE=4:AO_BLUR_RED"
		JOBS="$JOBS ao_A ao_L ao_N ao_S ao_SL"; NRED=$((NRED + 5)) ;;
	*) echo "VKLIGHT GATE FAILED (unknown pass $p)"; exit 1 ;;
	esac
done
for k in $K; do
	n="${k%%:*}"; r="${k#*:}"; f="${r%%:*}"; r="${r#*:}"; d="${r%%:*}"; red="${r#*:}"
	sp ${d//,/ } -Fo "$(W "$OUT/spv/$n.spv")" "$(W "$H/$f")"
	sp ${d//,/ } -D "$red" -Fo "$(W "$OUT/spv/${n}_red.spv")" "$(W "$H/$f")"
done
"$PY" -I "$ROOT/tests/vklight/light_gl_ref.py" "$(W "$OUT/jobs")" "$HDR" $PASSES < /dev/null 2>&1 | grep -E "^(real|job|LIGHT|COMPILE|Traceback|  |[A-Za-z]+Error)" || true
rc=0
cd "$OUT"
for j in $JOBS; do
	[ -f "jobs/$j/chain.txt" ] || { echo "VKLIGHT GATE FAILED (gl reference $j)"; exit 1; }
	t=""; [ -f "jobs/$j/gl_ms.txt" ] && [ -z "${VKLIGHT_NOTIME:-}" ] && t=time
	./lightmatch.exe "jobs/$j/chain.txt" spv $t > "green_$j.txt"; g=$?
	grep -vE "^device" "green_$j.txt"
	[ $g -eq 0 ] || { echo "GREEN $j FAILED"; rc=1; }
done
nred=0
for j in $JOBS; do
	for r in jobs/$j/red_*.txt; do
		[ -f "$r" ] || continue
		k="$(basename "$r" .txt)"
		./lightmatch.exe "$r" spv > "$j.$k.txt"
		echo "$k on $j: $(grep LIGHTMATCH "$j.$k.txt")"
		grep -q "LIGHTMATCH .* FAIL" "$j.$k.txt" || { echo "RED $k DID NOT FAIL"; rc=1; }
		nred=$((nred + 1))
	done
done
echo "reds run: $nred ($NRED kernels)"
[ "$nred" -eq "$NRED" ] || rc=1
if [ $rc -eq 0 ]; then echo "VKLIGHT GATE OK"; else echo "VKLIGHT GATE FAILED"; exit 1; fi
