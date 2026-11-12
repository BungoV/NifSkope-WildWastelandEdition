#!/bin/bash
# Lane VKFRAME1 (2026-11-12, P7): depth of field (the game's, the thin lens), motion blur (alone and fused) and object blur
# on Vulkan (res/shaders/hlsl/post/dof_*, mblur, doflens_*, objblur*) against the GL .frag chains on the same inputs.
# bash tests/vkframe/postfx.sh <out dir> [<WW_CELL_HDR_DUMP .hdr>]
# Needs MSYS2 UCRT64 g++ (Vulkan headers), DXC (DXC= to override) and python with numpy.
# Green: every job POSTFXMATCH EXACT or BOUND (postfxmatch.cpp). Each kernel's PFX_RED build must FAIL its job.
# Last line: `VKFRAME POSTFX GATE OK` or `... FAILED`.
set -u
OUT="${1:?out dir}"
HDR="${2:-X:/ClaudeOverflow/fogmatch1/m1/mem_fm_def.hdr}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH: its python has no numpy
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p "$OUT/spv"
W() { cygpath -m "$1" 2>/dev/null || echo "$1"; }
H="$ROOT/res/shaders/hlsl/post"
g++ -O1 -std=c++17 "$ROOT/tests/vkframe/postfxmatch.cpp" -o "$OUT/postfxmatch.exe" || { echo "VKFRAME POSTFX GATE FAILED (g++)"; exit 1; }
sp() { "$DXC" -spirv -fspv-target-env=vulkan1.2 -T cs_6_0 -E main "$@" > /dev/null || { echo "VKFRAME POSTFX GATE FAILED (dxc $*)"; exit 1; }; }
# kernel name, source, defines
K="dof_down:dof_down.hlsl: dof_blur:dof_blur.hlsl: dof_comp:dof_comp.hlsl: lens_comp:dof_comp.hlsl:-DDOF_LENS mblur:mblur.hlsl:
dofmblur:mblur.hlsl:-DMBLUR_FUSE lensmblur:mblur.hlsl:-DMBLUR_FUSE,-DDOF_LENS lens_prep:doflens_prep.hlsl: lens_tile:doflens_tile.hlsl:
lens_gather:doflens_gather.hlsl: ob_vec:objblur_vec.hlsl: ob_tile:objblur_tile.hlsl: ob:objblur.hlsl:"
for k in $K; do
	n="${k%%:*}"; r="${k#*:}"; f="${r%%:*}"; d="${r#*:}"
	sp ${d//,/ } -Fo "$(W "$OUT/spv/$n.spv")" "$(W "$H/$f")"
	sp ${d//,/ } -D PFX_RED -Fo "$(W "$OUT/spv/${n}_red.spv")" "$(W "$H/$f")"
done
"$PY" -I "$ROOT/tests/vkframe/postfx_gl_ref.py" "$(W "$OUT/jobs")" "$HDR" < /dev/null 2>&1 | grep -E "^(real|job|POSTFX|COMPILE|Traceback|  |[A-Za-z]+Error)" || true
[ -f "$OUT/jobs/ob_A/chain.txt" ] || { echo "VKFRAME POSTFX GATE FAILED (gl reference)"; exit 1; }
rc=0
cd "$OUT"
for j in dof_A dof_F dof_S mb_A dmb_A lens_A lens_F lens_S lmb_A ob_A; do
	t=""; [ -f "jobs/$j/gl_ms.txt" ] && t=time
	./postfxmatch.exe "jobs/$j/chain.txt" spv $t > "green_$j.txt"; g=$?
	grep -vE "^device" "green_$j.txt"
	[ $g -eq 0 ] || { echo "GREEN $j FAILED"; rc=1; }
done
for r in jobs/*/red_*.txt; do
	k="$(basename "$r" .txt)"; j="$(basename "$(dirname "$r")")"
	./postfxmatch.exe "$r" spv > "$j.$k.txt"
	echo "$k on $j: $(grep POSTFXMATCH "$j.$k.txt")"
	grep -q "POSTFXMATCH .* FAIL" "$j.$k.txt" || { echo "RED $k DID NOT FAIL"; rc=1; }
done
nred=$(ls jobs/*/red_*.txt | wc -l)
echo "reds run: $nred (13 kernels)"
[ "$nred" -eq 13 ] || rc=1
if [ $rc -eq 0 ]; then echo "VKFRAME POSTFX GATE OK"; else echo "VKFRAME POSTFX GATE FAILED"; exit 1; fi
