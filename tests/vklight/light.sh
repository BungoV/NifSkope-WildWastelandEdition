#!/bin/bash
# Lane VKLIGHT1 (2026-11-12): the lighting passes on Vulkan compute (res/shaders/hlsl/vklight/*.hlsl) against the GL .frag
# chains on the same inputs (tests/vklight/light_gl_ref.py, a hidden GL context; tests/vklight/lightmatch.cpp, a headless
# Vulkan device). Copied from VKFRAME1's tests/vkframe/postfx.sh.
# bash tests/vklight/light.sh <out dir> [<WW_CELL_HDR_DUMP .hdr>] [<pass> ...]     (passes: ao contact ssgi ssr aox contactfar; default all)
# VKLIGHT_NOTIME=1 skips every timing (gate only, while another job loads the GPU).
# Needs MSYS2 UCRT64 g++ (Vulkan headers), DXC (DXC= to override) and python with numpy.
# Green: every job LIGHTMATCH EXACT or BOUND; each kernel's red build must FAIL its job. Last line `VKLIGHT GATE OK` / `FAILED`.
set -u
OUT="${1:?out dir}"
HDR="${2:-X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr}"
shift; [ $# -gt 0 ] && shift
PASSES="${*:-ao contact ssgi ssr aox contactfar}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH: its python has no numpy
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p "$OUT/spv"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
H="$ROOT/res/shaders/hlsl/vklight"
g++ -O1 -std=c++17 "$ROOT/tests/vklight/lightmatch.cpp" -o "$OUT/lightmatch.exe" || { echo "VKLIGHT GATE FAILED (g++)"; exit 1; }
# the contact bands from the app's own header (both sides read the same bytes)
g++ -O1 -std=c++17 -I"$ROOT/src" "$ROOT/tests/vklight/contact_bands.cpp" -o "$OUT/contact_bands.exe" || { echo "VKLIGHT GATE FAILED (g++ bands)"; exit 1; }
export VKLIGHT_BANDS="$(W "$OUT/contact_bands.exe")"
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
	contact) K="$K contact:contact_vk.hlsl::CONTACT_RED"
		JOBS="$JOBS ct_A ct_B ct_C ct_S"; NRED=$((NRED + 1)) ;;
	# lane VKSHADOW3: the far tier (shadowset.cpp contactFarPass) on compute, contactfar_vk.hlsl stages 1-3. Bar, stated before
	# the first run: every job's farDepth ulp0 (EXACT: a min over texels), farMask and the composed mask ulp1 (one R8 level, as
	# the near tier's ct_ jobs). Jobs cf_A (dump, light in front, the cascades' reach D 40000 given: the dump's geometry is 31000-42000 out), cf_B (dump, light behind,
	# SHADOWPAR1 + early exit, no reach), cf_S (synthetic 641x361, D 1500 given). Reds on cf_A, each must FAIL:
	# cfar_min (the block's first texel, not its min), cfar_march (the light's w sign flipped), cfar_comp (halo: tolerance
	# 1e6), cfar_compi (inside: farFade off, the tier at every distance)
	contactfar) K="$K cfar_min:contactfar_vk.hlsl:-DCFAR_STAGE=1:CFAR_MIN_RED cfar_march:contactfar_vk.hlsl:-DCFAR_STAGE=2:CFAR_MARCH_RED
		cfar_comp:contactfar_vk.hlsl:-DCFAR_STAGE=3:CFAR_HALO_RED cfar_compi:contactfar_vk.hlsl:-DCFAR_STAGE=3:CFAR_INSIDE_RED"
		JOBS="$JOBS cf_A cf_B cf_S"; NRED=$((NRED + 4)) ;;
	ssgi) K="$K ssgi:ssgi_vk.hlsl::SSGI_RED ssgie:ssgi_vk.hlsl::SSGI_EMIT_RED"	# ssgie: lane VKFAR3, the emission subtracted
		JOBS="$JOBS sg_A sg_B sg_S sg_E"; NRED=$((NRED + 2)) ;;
	aox) K="$K aox_z:aox_vk.hlsl:-DAOX_KERNEL=1:AOX_Z_RED aox_xe:aox_vk.hlsl:-DAOX_KERNEL=2:AOX_XE_RED
		aox_r8:aox_vk.hlsl:-DAOX_KERNEL=3:AOX_R8_RED aox_hb:aox_vk.hlsl:-DAOX_KERNEL=3:AOX_HB_RED"	# lane VKFAR3: the ported AO methods
		# ax_H (HBAO) is held against the readable HLSL through SPIR-V on GL (the app's live route; the GLSL keeps the old divide)
		"$PY" -I "$ROOT/tests/hlsllive/build_spv.py" "$(W "$OUT/glspv")" "$(W "$ROOT/tests/vklight/fullscreen_vs.hlsl")" \
			"$(W "$ROOT/res/shaders/hlsl/ao/cell_aox_ps.hlsl")" | grep -v "^OK" && { echo "VKLIGHT GATE FAILED (GL SPIR-V)"; exit 1; }
		export VKLIGHT_SPV_DIR="$(W "$OUT/glspv")"
		JOBS="$JOBS ax_X ax_G ax_S ax_H"; NRED=$((NRED + 4)) ;;
	ssr) K="$K ssr_ray:ssr_vk.hlsl:-DSSR_STAGE=1:SSR_RED ssr_march:ssr_vk.hlsl:-DSSR_STAGE=2:SSR_RED
		ssr_across:ssr_vk.hlsl:-DSSR_STAGE=3:SSR_RED ssr_down:ssr_vk.hlsl:-DSSR_STAGE=4:SSR_RED"
		JOBS="$JOBS ss_A ss_W ss_R ss_S"; NRED=$((NRED + 4)) ;;
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
