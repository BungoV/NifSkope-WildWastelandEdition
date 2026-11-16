#!/bin/bash
# Lane VKFAR28 (2026-11-16): the far LOD rings under the RT GI keys on the present device (vkfarlodrt.inc farLodRtRun,
# twin 1 all-on RESTIR + RTDDGI + RTDDGI_CASCADES, twin 2 RTDDGI + RTDDGI_CASCADES) against GL's own fo4_cellcsm under the
# same keys, per sample, colour + depth; cellRestir / cellDdgiAtlas are the shared texel buffers (vkrtbuf.h rtBufShare).
# bash tests/vkframe/farlodrtdraw.sh <out dir>. Needs MSYS2 UCRT64 g++ and python. Bar and reds: farlodrtdraw.cpp's header.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `FARLODRTDRAW GATE OK` or `FARLODRTDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FARLODRTDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT/inc/features/vkframe"
"$PY" -I "$ROOT/tests/vkframe/farlodrtdraw_prep.py" "$OUT" || { echo "FARLODRTDRAW GATE FAILED (job)"; exit 1; }
# lane VKFAR29, the 11-16 ruling as in fargrounddraw.sh: HLSL is the reference and FO4MATCH1's terms are HLSL-only, so both
# RT pixel twins are built again with the gate-only WW_GLSL_BASE_TERMS into <out>/inc, ahead of the tree's headers
SPV="$ROOT/src/features/vkframe/vkframe_spirv.sh"
for fn in fxr fxdg; do
	body="$(sed -n "/^$fn() {/,/^}/p" "$SPV")"
	[ "$(printf '%s\n' "$body" | grep -c -- '-D WW_FRAME_UBO=1 ')" = 1 ] || { echo "FARLODRTDRAW GATE FAILED (vkframe_spirv.sh's $fn() not as expected)"; exit 1; }
	(
		cd "$OUT/inc/features/vkframe" || exit 1
		eval "$(printf '%s\n' "$body" | sed 's/-D WW_FRAME_UBO=1 /-D WW_FRAME_UBO=1 -D WW_GLSL_BASE_TERMS=1 /')"
		if [ $fn = fxr ]; then fxr farlod_view_rt_ps core/fo4_cellcsm_ps; else fxdg farlod_view_dg_ps core/fo4_cellcsm_ps; fi
	) || { echo "FARLODRTDRAW GATE FAILED (dxc $fn)"; exit 1; }
done
for h in farlod_view_rt_ps farlod_view_dg_ps; do
	[ -s "$OUT/inc/features/vkframe/$h.spv.h" ] || { echo "FARLODRTDRAW GATE FAILED (no $h.spv.h)"; exit 1; }
	if cmp -s "$OUT/inc/features/vkframe/$h.spv.h" "$ROOT/src/features/vkframe/$h.spv.h"; then
		echo "FARLODRTDRAW GATE FAILED ($h: WW_GLSL_BASE_TERMS moved nothing)"; exit 1
	fi
	echo "FARLODRTDRAW gate-only twin (WW_GLSL_BASE_TERMS) built: $h differs from the shipped header"
done
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -O2 -std=c++17 -pthread -I"$OUT/inc" -I"$ROOT/src" "$ROOT/tests/vkframe/farlodrtdraw.cpp" -o "$OUT/farlodrtdraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FARLODRTDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/farlodrtdraw.exe" "$OUT" 2>&1 | tee "$OUT/farlodrtdraw.txt"
tail -1 "$OUT/farlodrtdraw.txt" | grep -q "FARLODRTDRAW GATE OK"
