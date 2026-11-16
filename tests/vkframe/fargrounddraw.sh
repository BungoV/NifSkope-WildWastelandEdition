#!/bin/bash
# Lane VKFAR3 (2026-11-14, row G3): the far ring 0's live ground (shader decals) drawn on the present device (vkfarground.inc
# farGroundRun) against GL's own fo4_cellcsmfgl program (fo4_default.vert + far-ground/fo4_cellcsmfgl.frag, the FOGLEAK +
# FRAME_UBO build the twins carry), per sample, colour + depth. bash tests/vkframe/fargrounddraw.sh <out dir>. Needs MSYS2
# UCRT64 g++ and python. Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `FARGROUNDDRAW GATE OK` or `FARGROUNDDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FARGROUNDDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT/inc/features/vkframe"
"$PY" -I "$ROOT/tests/vkframe/fargrounddraw_prep.py" "$OUT" || { echo "FARGROUNDDRAW GATE FAILED (job)"; exit 1; }
# lane VKGROUND5, the 11-16 ruling as in fgpagesdraw.sh: HLSL is the reference and FO4MATCH1's terms are HLSL-only, so the
# pixel twin is built again with the gate-only WW_GLSL_BASE_TERMS into <out>/inc, ahead of the tree's headers
body="$(sed -n '/^fxg() {/,/^}/p' "$ROOT/src/features/vkframe/vkframe_spirv.sh")"
[ "$(printf '%s\n' "$body" | grep -c -- '-D WW_FRAME_UBO=1 ')" = 1 ] || { echo "FARGROUNDDRAW GATE FAILED (vkframe_spirv.sh's fxg() not as expected)"; exit 1; }
(
	cd "$OUT/inc/features/vkframe" || exit 1
	eval "$(printf '%s\n' "$body" | sed 's/-D WW_FRAME_UBO=1 /-D WW_FRAME_UBO=1 -D WW_GLSL_BASE_TERMS=1 /')"
	fxg fargnd_view_ps far-ground/fo4_cellcsmfgl_ps
) || { echo "FARGROUNDDRAW GATE FAILED (dxc)"; exit 1; }
h=fargnd_view_ps
[ -s "$OUT/inc/features/vkframe/$h.spv.h" ] || { echo "FARGROUNDDRAW GATE FAILED (no $h.spv.h)"; exit 1; }
if cmp -s "$OUT/inc/features/vkframe/$h.spv.h" "$ROOT/src/features/vkframe/$h.spv.h"; then
	echo "FARGROUNDDRAW GATE FAILED ($h: WW_GLSL_BASE_TERMS moved nothing)"; exit 1
fi
echo "FARGROUNDDRAW gate-only twin (WW_GLSL_BASE_TERMS) built: $h differs from the shipped header"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -O2 -std=c++17 -pthread -I"$OUT/inc" -I"$ROOT/src" "$ROOT/tests/vkframe/fargrounddraw.cpp" -o "$OUT/fargrounddraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FARGROUNDDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/fargrounddraw.exe" "$OUT" 2>&1 | tee "$OUT/fargrounddraw.txt"
tail -1 "$OUT/fargrounddraw.txt" | grep -q "FARGROUNDDRAW GATE OK"
