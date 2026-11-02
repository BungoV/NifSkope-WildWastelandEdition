#!/bin/bash
# lane MATLOOK1 (TRANSLUC1): build + run the translucency law test, green and red (WWTRANSL_RED must fail).
# usage: bash tests/translucency/translucency.sh [outdir]   (run from anywhere; outdir defaults to $TMPDIR)
T="$(cd "$(dirname "$0")/../.." && pwd)"
O="${1:-${TMPDIR:?set TMPDIR off C:}}"
mkdir -p "$O"
export PATH="/c/msys64/ucrt64/bin:$PATH"
cd "$T" || exit 2
g++ -std=c++17 -O1 -I"$T/src" tests/translucency/translucency.cpp -o "$O/transl_green.exe" || exit 2
g++ -std=c++17 -O1 -DWWTRANSL_RED -I"$T/src" tests/translucency/translucency.cpp -o "$O/transl_red.exe" || exit 2
timeout 30 "$O/transl_green.exe"; G=$?
echo "--- red (WWTRANSL_RED, must FAIL)"
timeout 30 "$O/transl_red.exe"; R=$?
echo "green rc=$G red rc=$R"
[ "$G" = 0 ] && [ "$R" != 0 ] && echo "GATE PASS" && exit 0
echo "GATE FAIL"; exit 1
