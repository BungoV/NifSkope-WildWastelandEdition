#!/bin/bash
# Lane VKFAR2 (2026-11-13, row G3): the far cards on the present device (vkfarcardsdraw.inc farCardsRun, one instanced draw a
# card set) against GL's impostor_instez.vert / impostor_cellinstez.frag (the FOGLEAK build the shipped SPIR-V twins), per
# sample + stencil + depth. bash tests/vkframe/farcards.sh <out dir>. Needs MSYS2 UCRT64 g++ and python with numpy (the job).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `FARCARDS GATE OK` or `FARCARDS GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FARCARDS GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/farcards_prep.py" "$OUT" || { echo "FARCARDS GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/farcards.cpp" -o "$OUT/farcards.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FARCARDS GATE FAILED (g++)"; exit 1; }
"$OUT/farcards.exe" "$OUT" 2>&1 | tee "$OUT/farcards.txt"
tail -1 "$OUT/farcards.txt" | grep -q "FARCARDS GATE OK"
