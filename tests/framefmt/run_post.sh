#!/bin/bash
# lane PRESETPERF1 (FRAMEFMT1-B): green must pass, red (-DPOSTFMT_RED, the code before the lane) must fail. OUT = a scratch folder.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-$ROOT/tests/framefmt/out}"
mkdir -p "$OUT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -std=c++17 -I"$ROOT/src" "$ROOT/tests/framefmt/postfmt_test.cpp" -o "$OUT/postfmt_test.exe" || { echo "BUILD FAIL green"; exit 2; }
g++ -std=c++17 -DPOSTFMT_RED -I"$ROOT/src" "$ROOT/tests/framefmt/postfmt_test.cpp" -o "$OUT/postfmt_red.exe" || { echo "BUILD FAIL red"; exit 2; }
timeout 30 "$OUT/postfmt_test.exe"; G=$?
timeout 30 "$OUT/postfmt_red.exe" > "$OUT/postred.log"; R=$?
grep -c FAIL "$OUT/postred.log" | sed 's/^/red FAIL lines: /'
echo "green rc=$G red rc=$R"
[ $G -eq 0 ] && [ $R -ne 0 ] && { echo "POSTFMT UNIT PASS"; exit 0; }
echo "POSTFMT UNIT FAIL"; exit 1
