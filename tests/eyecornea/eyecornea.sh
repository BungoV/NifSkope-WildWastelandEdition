#!/bin/bash
# lane SKINSSS2: tests/eyecornea green (rc 0), red (-DWWFACEGENEYE_RED: eyes keep type 1, rc != 0) and the base text
# (the green exe on the branch point's files: no eye type carried, rc != 0) -> GATE PASS.
# usage: bash tests/eyecornea/eyecornea.sh <out dir> [base commit]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:?out dir}"
BASE="${2:-}"
mkdir -p "$OUT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -std=c++17 -O1 -I"$ROOT/src" "$ROOT/tests/eyecornea/eyecornea.cpp" -o "$OUT/eyecornea.exe" || { echo "BUILD FAIL green"; exit 2; }
g++ -std=c++17 -O1 -DWWFACEGENEYE_RED -I"$ROOT/src" "$ROOT/tests/eyecornea/eyecornea.cpp" -o "$OUT/eyecornea_red.exe" || { echo "BUILD FAIL red"; exit 2; }
timeout 60 "$OUT/eyecornea.exe" "$ROOT" > "$OUT/green.txt"; G=$?
timeout 60 "$OUT/eyecornea_red.exe" "$ROOT" > "$OUT/red.txt"; R=$?
tail -1 "$OUT/green.txt"; tail -1 "$OUT/red.txt"
B=1
if [ -n "$BASE" ]; then
	mkdir -p "$OUT/base"
	git -C "$ROOT" archive "$BASE" src/gl src/lodgen.cpp src/cellview.cpp | tar -x -C "$OUT/base"
	timeout 60 "$OUT/eyecornea.exe" "$OUT/base" > "$OUT/base.txt"; B=$?
	tail -1 "$OUT/base.txt"
fi
echo "green rc=$G red rc=$R base rc=$B"
if [ "$G" -eq 0 ] && [ "$R" -ne 0 ] && [ "$B" -ne 0 ]; then echo "GATE PASS"; exit 0; fi
echo "GATE FAIL"; exit 1
