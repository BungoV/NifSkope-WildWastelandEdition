#!/bin/bash
# Lane VKOBJ1 (2026-11-13, row G4 slice 1): the objects' opaque depth prepass on the present device (vkpresent.cpp objRun)
# against GL's prepass program drawn the way cellgpucull.cpp draws it, into multisample depth targets, every sample compared.
# bash tests/vkframe/objview.sh <out dir>. Needs MSYS2 UCRT64 g++.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `OBJVIEW GATE OK` or `OBJVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "OBJVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/objview.cpp" -o "$OUT/objview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "OBJVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/objview.exe" "$ROOT" 2>&1 | tee "$OUT/objview.txt"
tail -1 "$OUT/objview.txt" | grep -q "OBJVIEW GATE OK"
