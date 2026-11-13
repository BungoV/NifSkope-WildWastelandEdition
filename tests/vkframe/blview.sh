#!/bin/bash
# Lane VKOBJ2 (2026-11-13, row G4 slice 2, the bindless half): GL's bindless maps (resident handles in the row) against the
# Vulkan descriptor-indexed table (vkbindless.inc) read through the shipped include (ww_bindless_vk.hlsli), EXACT, with reds.
# bash tests/vkframe/blview.sh <out dir>. Needs MSYS2 UCRT64 g++ and DXC (E:/Tools/dxc, or DXC=<dxc.exe>).
# Also compiles both bl twins with -D WW_VK_BINDLESS (they must build) and prints their set-1 bindings.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `BLVIEW GATE OK` or `BLVIEW GATE FAILED`.
# Lane VKOBJ3: `bash tests/vkframe/blview.sh <out dir> bc` runs the texture cache's BC twins (vkobjtex.inc) instead.
# Lane VKOBJ4: also compiles the bl twins' pipeline modules (*.pipe.spv, the vertex stage's red `redshift`) for objBlPipe.
set -u
OUT="${1:?out dir}"
MODE="${2:-}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DXC="${DXC:-E:/Tools/dxc/bin/x64/dxc.exe}"
PY="${PY:-$(command -v python)}"  # the caller's Python (build_all.py), before UCRT64 leads PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "BLVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
SPV=(-spirv -fspv-target-env=vulkan1.2 -HV 2021)
# the stage shifts as src/features/vkframe/vkframe_spirv.sh's PS_SHIFT / VS_SHIFT (the shipped objbl_*.spv.h)
PSHIFT="-fvk-b-shift 0 0 -fvk-t-shift 64 0 -fvk-s-shift 128 0 -fvk-u-shift 192 0"
VSHIFT="-fvk-b-shift 256 0 -fvk-t-shift 320 0 -fvk-s-shift 384 0 -fvk-u-shift 448 0"
for v in PS_SHIFT:"$PSHIFT" VS_SHIFT:"$VSHIFT"; do
	grep -qF "${v%%:*}=\"${v#*:}\"" "$ROOT/src/features/vkframe/vkframe_spirv.sh" || { echo "BLVIEW GATE FAILED (${v%%:*} differs from vkframe_spirv.sh)"; exit 1; }
done
"$DXC" "${SPV[@]}" -T vs_6_0 -E main "$ROOT/tests/vkframe/blview_vs.hlsl" -Fo "$OUT/blview_vs.spv" > "$OUT/dxc.txt" 2>&1 \
	&& "$DXC" "${SPV[@]}" -T ps_6_0 -E main -D WW_VK_BINDLESS "$ROOT/tests/vkframe/blview_ps.hlsl" -Fo "$OUT/blview_ps.spv" >> "$OUT/dxc.txt" 2>&1 \
	&& "$DXC" "${SPV[@]}" -T ps_6_0 -E main -D WW_VK_BINDLESS -D WW_VK_BINDLESS_RED_SLOT "$ROOT/tests/vkframe/blview_ps.hlsl" \
		-Fo "$OUT/blview_ps_redslot.spv" >> "$OUT/dxc.txt" 2>&1 \
	|| { tail -20 "$OUT/dxc.txt"; echo "BLVIEW GATE FAILED (dxc, the fixture)"; exit 1; }
for twin in fo4_cellbl_ps fo4_cellcsmbl_ps; do
	"$DXC" "${SPV[@]}" -T ps_6_0 -E main -D WW_VK_BINDLESS "$ROOT/res/shaders/hlsl/near/$twin.hlsl" -Fo "$OUT/$twin.vk.spv" \
		> "$OUT/$twin.dxc.txt" 2>&1 || { tail -20 "$OUT/$twin.dxc.txt"; echo "BLVIEW GATE FAILED (dxc, $twin with WW_VK_BINDLESS)"; exit 1; }
	"$DXC" "${SPV[@]}" -T ps_6_0 -E main -D WW_VK_BINDLESS "$ROOT/res/shaders/hlsl/near/$twin.hlsl" -Fc "$OUT/$twin.vk.asm" > /dev/null 2>&1
	echo "twin $twin (Vulkan form): $(grep -cE 'wwBl(Images|Samplers) (DescriptorSet 1|Binding [01])' "$OUT/$twin.vk.asm") set-1 decorations"
	# lane VKOBJ4: the pipeline's form (fx()'s shifts, every binding kept), read by objBlPipe
	"$DXC" "${SPV[@]}" -T ps_6_0 -E main -D WW_VK_BINDLESS $PSHIFT -fspv-preserve-bindings "$ROOT/res/shaders/hlsl/near/$twin.hlsl" \
		-Fo "$OUT/$twin.pipe.spv" >> "$OUT/$twin.dxc.txt" 2>&1 || { tail -20 "$OUT/$twin.dxc.txt"; echo "BLVIEW GATE FAILED (dxc, $twin pipe)"; exit 1; }
done
# lane VKOBJ4: the bl vertex stage on its own shifts, and the red `shift` on the pixel stage's (b0 skinning on b0 global)
for vs in "pipe:$VSHIFT" "redshift:$PSHIFT"; do
	"$DXC" "${SPV[@]}" -T vs_6_0 -E main -D WW_VK_BINDLESS ${vs#*:} -fspv-preserve-bindings "$ROOT/res/shaders/hlsl/near/fo4_defaultbl_vs.hlsl" \
		-Fo "$OUT/fo4_defaultbl_vs.${vs%%:*}.spv" > "$OUT/fo4_defaultbl_vs.dxc.txt" 2>&1 \
		|| { tail -20 "$OUT/fo4_defaultbl_vs.dxc.txt"; echo "BLVIEW GATE FAILED (dxc, fo4_defaultbl_vs ${vs%%:*})"; exit 1; }
done
# lane VKOBJ4 open item 2: GL's fo4_cellbl from the HLSL SPIR-V set (and its location registry), as the app links it
"$PY" -I -X utf8 "$ROOT/tests/hlsllive/build_all.py" "$ROOT" "$OUT/spvset" --only fo4_cellbl --jobs 4 > "$OUT/spvset.txt" 2>&1 \
	|| { tail -8 "$OUT/spvset.txt"; echo "BLVIEW GATE FAILED (build_all.py, the fo4_cellbl SPIR-V set)"; exit 1; }
# lane VKOBJ4 gap 1: the 4-sample arm's fill pass (GL from SPIR-V; blview_fill_*.hlsl -> $OUT/blview_fill_*.spv)
"$PY" -I -X utf8 "$ROOT/tests/hlsllive/build_spv.py" "$OUT" "$ROOT/tests/vkframe/blview_fill_vs.hlsl" "$ROOT/tests/vkframe/blview_fill_ps.hlsl" \
	> "$OUT/fill.txt" 2>&1 && ! grep -q "^FAIL" "$OUT/fill.txt" || { tail -8 "$OUT/fill.txt"; echo "BLVIEW GATE FAILED (build_spv.py, the fill pass)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/blview.cpp" -o "$OUT/blview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "BLVIEW GATE FAILED (g++)"; exit 1; }
TXT="$OUT/blview${MODE:+_$MODE}.txt"
"$OUT/blview.exe" "$ROOT" "$OUT" $MODE 2>&1 | tee "$TXT"
tail -1 "$TXT" | grep -q "BLVIEW GATE OK"
