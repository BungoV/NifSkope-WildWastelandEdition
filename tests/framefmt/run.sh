#!/bin/bash
# lane FRAMEFMT1: green must pass, red (-DFRAMEFMT_RED, the rule before the lane) must fail. OUT = a scratch folder.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-$ROOT/tests/framefmt/out}"
mkdir -p "$OUT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -std=c++17 -I"$ROOT/src" "$ROOT/tests/framefmt/framefmt_test.cpp" -o "$OUT/framefmt_test.exe" || { echo "BUILD FAIL green"; exit 2; }
g++ -std=c++17 -DFRAMEFMT_RED -I"$ROOT/src" "$ROOT/tests/framefmt/framefmt_test.cpp" -o "$OUT/framefmt_red.exe" || { echo "BUILD FAIL red"; exit 2; }
timeout 30 "$OUT/framefmt_test.exe"; G=$?
timeout 30 "$OUT/framefmt_red.exe" > "$OUT/red.log"; R=$?
grep -c FAIL "$OUT/red.log" | sed 's/^/red FAIL lines: /'
echo "green rc=$G red rc=$R"
[ $G -eq 0 ] && [ $R -ne 0 ] && { echo "FRAMEFMT UNIT PASS"; exit 0; }
echo "FRAMEFMT UNIT FAIL"; exit 1
