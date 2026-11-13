#!/bin/bash
# Lane VKSHADOW1 (2026-11-13, row G6): the sun cascades' queued casters on the present device (vkpresent.cpp csmRun) against
# GL's cascade program drawn the way sunshadow.cpp draws it, into D16 2D arrays, every texel compared.
# bash tests/vkframe/shadowview.sh <out dir> [map]. Needs MSYS2 UCRT64 g++.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `SHADOWVIEW GATE OK` or `SHADOWVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "SHADOWVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/shadowview.cpp" -o "$OUT/shadowview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "SHADOWVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/shadowview.exe" "$ROOT" "${2:-1024}" 2>&1 | tee "$OUT/shadowview.txt"
tail -1 "$OUT/shadowview.txt" | grep -q "SHADOWVIEW GATE OK"
