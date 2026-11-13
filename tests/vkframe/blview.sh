#!/bin/bash
# Lane VKOBJ2 (2026-11-13, row G4 slice 2, the bindless half): GL's bindless maps (resident handles in the row) against the
# Vulkan descriptor-indexed table (vkbindless.inc) read through the shipped include (ww_bindless_vk.hlsli), EXACT, with reds.
# bash tests/vkframe/blview.sh <out dir>. Needs MSYS2 UCRT64 g++ and DXC (E:/Tools/dxc, or DXC=<dxc.exe>).
# Also compiles both bl twins with -D WW_VK_BINDLESS (they must build) and prints their set-1 bindings.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `BLVIEW GATE OK` or `BLVIEW GATE FAILED`.
# Lane VKOBJ3: `bash tests/vkframe/blview.sh <out dir> bc` runs the texture cache's BC twins (vkobjtex.inc) instead.
set -u
OUT="${1:?out dir}"
MODE="${2:-}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DXC="${DXC:-E:/Tools/dxc/bin/x64/dxc.exe}"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "BLVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
SPV=(-spirv -fspv-target-env=vulkan1.2 -HV 2021)
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
done
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/blview.cpp" -o "$OUT/blview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "BLVIEW GATE FAILED (g++)"; exit 1; }
TXT="$OUT/blview${MODE:+_$MODE}.txt"
"$OUT/blview.exe" "$ROOT" "$OUT" $MODE 2>&1 | tee "$TXT"
tail -1 "$TXT" | grep -q "BLVIEW GATE OK"
