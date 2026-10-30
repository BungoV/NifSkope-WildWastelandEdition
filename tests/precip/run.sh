#!/bin/bash
# lane WEATHERLOOK1 phase B: build and run the precipitation unit (src/gl/precipmath.h + precipspec.h), green and one
# red per rule. usage: bash tests/precip/run.sh <out dir>
# green must print PRECIP PASS; each red (PRECIP_RED=1 constdensity, 2 nowrap, 4 onespeed, 8 nowind) must print
# PRECIP FAIL with its own rule failing (rule2+4, rule3, rule1, rule5).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="${1:?out dir}"
mkdir -p "$OUT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -std=c++17 -O2 -w -I"$ROOT/src" -I/c/msys64/ucrt64/include/qt6 -I/c/msys64/ucrt64/include/qt6/QtCore \
	"$HERE/precip_test.cpp" -o "$OUT/precip_test.exe" -L/c/msys64/ucrt64/lib -lQt6Core > "$OUT/build.log" 2>&1
echo "BUILD-RC=$?"
for red in 0 1 2 4 8; do
	PRECIP_RED=$red timeout 120 "$OUT/precip_test.exe" > "$OUT/run_red$red.log" 2>&1
	echo "red=$red RUN-RC=$? $(tail -2 "$OUT/run_red$red.log" | head -1)"
done
