#!/bin/bash
# lane MATLOOK3: tests/haireye green (rc 0) and red (-DWWHAIREYE_RED, rc != 0) -> GATE PASS.
# usage: bash tests/haireye/haireye.sh <out dir>
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:?out dir}"
mkdir -p "$OUT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -std=c++17 -O1 -I"$ROOT/src" "$ROOT/tests/haireye/haireye.cpp" -o "$OUT/haireye.exe" || { echo "BUILD FAIL green"; exit 2; }
g++ -std=c++17 -O1 -DWWHAIREYE_RED -I"$ROOT/src" "$ROOT/tests/haireye/haireye.cpp" -o "$OUT/haireye_red.exe" || { echo "BUILD FAIL red"; exit 2; }
timeout 60 "$OUT/haireye.exe" "$ROOT" > "$OUT/green.txt"; G=$?
timeout 60 "$OUT/haireye_red.exe" "$ROOT" > "$OUT/red.txt"; R=$?
tail -1 "$OUT/green.txt"; tail -1 "$OUT/red.txt"
echo "green rc=$G red rc=$R"
if [ "$G" -eq 0 ] && [ "$R" -ne 0 ]; then echo "GATE PASS"; exit 0; fi
echo "GATE FAIL"; exit 1
