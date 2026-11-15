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
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FARGROUNDDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
# BATCH1117: GL's program is the HLSL-built SPIR-V pair (build_all into $FARGROUNDDRAW_SPVSET, default $OUT/spvset)
"$PY" -I "$ROOT/tests/vkframe/fargrounddraw_prep.py" "$OUT" ${FARGROUNDDRAW_SPVSET:-} ||{ echo "FARGROUNDDRAW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/fargrounddraw.cpp" -o "$OUT/fargrounddraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FARGROUNDDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/fargrounddraw.exe" "$OUT" 2>&1 | tee "$OUT/fargrounddraw.txt"
tail -1 "$OUT/fargrounddraw.txt" | grep -q "FARGROUNDDRAW GATE OK"
