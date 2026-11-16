#!/bin/bash
# Lane VKSMALL1/VKSMALL2 (2026-11-16, census row 40): the frame's farthest depth per pixel on the present device
# (vkdepthmax.inc depthMaxRun) against GL's own cell_depthmax.prog (cell_hdr.vert + cell_depthmax.frag), per texel
# (tests/vkframe/depthmaxview.cpp has the bar and reds). bash tests/vkframe/depthmaxview.sh <out dir>. Needs MSYS2 UCRT64
# g++ and python. Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `DEPTHMAXVIEW GATE OK` or `DEPTHMAXVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "DEPTHMAXVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
# cell_hdr.vert as the app links it (cell_underwater.prog shares it with cell_depthmax.prog)
"$PY" -I "$ROOT/tests/vkframe/waterdraw_prep.py" "$OUT/uw" cell_underwater || { echo "DEPTHMAXVIEW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/depthmaxview.cpp" -o "$OUT/depthmaxview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "DEPTHMAXVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/depthmaxview.exe" "$OUT" "$ROOT" 2>&1 | tee "$OUT/depthmaxview.txt"
tail -1 "$OUT/depthmaxview.txt" | grep -q "DEPTHMAXVIEW GATE OK"
