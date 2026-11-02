#!/bin/bash
# lane SKINSSS1: tests/skinsss green (rc 0), red (-DWWSKINSSS_RED: the composite keeps the frame, rc != 0) and the
# base text (the green exe on the branch point's files: no pass, no tag, rc != 0) -> GATE PASS.
# usage: bash tests/skinsss/skinsss.sh <out dir> [base commit]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:?out dir}"
BASE="${2:-}"
mkdir -p "$OUT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -std=c++17 -O1 -I"$ROOT/src" "$ROOT/tests/skinsss/skinsss.cpp" -o "$OUT/skinsss.exe" || { echo "BUILD FAIL green"; exit 2; }
g++ -std=c++17 -O1 -DWWSKINSSS_RED -I"$ROOT/src" "$ROOT/tests/skinsss/skinsss.cpp" -o "$OUT/skinsss_red.exe" || { echo "BUILD FAIL red"; exit 2; }
timeout 60 "$OUT/skinsss.exe" "$ROOT" > "$OUT/green.txt"; G=$?
timeout 60 "$OUT/skinsss_red.exe" "$ROOT" > "$OUT/red.txt"; R=$?
tail -1 "$OUT/green.txt"; tail -1 "$OUT/red.txt"
B=1
if [ -n "$BASE" ]; then
	mkdir -p "$OUT/base"	# tar overwrites; the base has no new files, so none of the branch's can linger
	git -C "$ROOT" archive "$BASE" res/shaders src/gl src/glview.cpp src/lodgen.cpp src/cellview.cpp | tar -x -C "$OUT/base"
	timeout 60 "$OUT/skinsss.exe" "$OUT/base" > "$OUT/base.txt"; B=$?
	tail -1 "$OUT/base.txt"
fi
echo "green rc=$G red rc=$R base rc=$B"
if [ "$G" -eq 0 ] && [ "$R" -ne 0 ] && [ "$B" -ne 0 ]; then echo "GATE PASS"; exit 0; fi
echo "GATE FAIL"; exit 1
