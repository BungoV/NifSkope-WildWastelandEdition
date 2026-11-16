#!/bin/bash
# Lane VKGROUND3 (2026-11-16): far ground mode 2 (Show > Far ground: Live, cached pages) drawn on the present device
# (vkfarground.inc farGroundLiveRun: fargnd_view_vs + fargndp_view_ps) against GL's own fo4_cellcsmfgp program
# (fo4_default.vert + far-ground/fo4_cellcsmfgp.frag, FOGLEAK + FRAME_UBO), per sample, colour + depth.
# Reference ruling (11-16): HLSL is the reference; FO4MATCH1's terms are HLSL-only, and GL here is the legacy .frag. So the
# gate compiles the far ground's pixel twins again with the gate-only WW_GLSL_BASE_TERMS (core/fo4_default_ps.hlsl: those
# terms out, the GLSL base's in) into <out>/inc, ahead of the tree's headers on -I; the shipped headers never carry it.
# bash tests/vkframe/fgpagesdraw.sh <out dir>. Needs MSYS2 UCRT64 g++, python and dxc (DXC, default /e/Tools/dxc). Not
# while NifSkope or the game runs. Last line: `FGPAGESDRAW GATE OK` or `FGPAGESDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
DXC="${DXC:-/e/Tools/dxc/bin/x64/dxc.exe}"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FGPAGESDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT/inc/features/vkframe"
"$PY" -I "$ROOT/tests/vkframe/fgpagesdraw_prep.py" "$OUT" || { echo "FGPAGESDRAW GATE FAILED (job)"; exit 1; }
body="$(sed -n '/^fxg() {/,/^}/p' "$ROOT/src/features/vkframe/vkframe_spirv.sh")"
[ "$(printf '%s\n' "$body" | grep -c -- '-D WW_FRAME_UBO=1 ')" = 1 ] || { echo "FGPAGESDRAW GATE FAILED (vkframe_spirv.sh's fxg() not as expected)"; exit 1; }
(
	cd "$OUT/inc/features/vkframe" || exit 1
	eval "$(printf '%s\n' "$body" | sed 's/-D WW_FRAME_UBO=1 /-D WW_FRAME_UBO=1 -D WW_GLSL_BASE_TERMS=1 /')"
	fxg fargnd_view_ps far-ground/fo4_cellcsmfgl_ps
	fxg fargndp_view_ps far-ground/fo4_cellcsmfgp_ps
) || { echo "FGPAGESDRAW GATE FAILED (dxc)"; exit 1; }
for h in fargnd_view_ps fargndp_view_ps; do
	[ -s "$OUT/inc/features/vkframe/$h.spv.h" ] || { echo "FGPAGESDRAW GATE FAILED (no $h.spv.h)"; exit 1; }
	if cmp -s "$OUT/inc/features/vkframe/$h.spv.h" "$ROOT/src/features/vkframe/$h.spv.h"; then
		echo "FGPAGESDRAW GATE FAILED ($h: WW_GLSL_BASE_TERMS moved nothing)"; exit 1
	fi
done
echo "FGPAGESDRAW gate-only twins (WW_GLSL_BASE_TERMS) built: fargnd_view_ps, fargndp_view_ps differ from the shipped headers"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -O2 -std=c++17 -pthread -I"$OUT/inc" -I"$ROOT/src" "$ROOT/tests/vkframe/fgpagesdraw.cpp" -o "$OUT/fgpagesdraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FGPAGESDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/fgpagesdraw.exe" "$OUT" > "$OUT/fgpagesdraw.txt" 2>&1
tail -1 "$OUT/fgpagesdraw.txt"
tail -1 "$OUT/fgpagesdraw.txt" | grep -q "FGPAGESDRAW GATE OK"
