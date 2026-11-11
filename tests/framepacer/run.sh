#!/bin/bash
# lane EMPTY240 ITEM 0: the frame pacer's tick-rate test. Green must pass; the red (build 71's coarse timer route)
# must fail. Usage: bash tests/framepacer/run.sh <out dir>   (from the tree's root; MSYS2 ucrt64 g++)
set -u
out=${1:?out dir}
mkdir -p "$out"
GXX=${GXX:-C:/msys64/ucrt64/bin/g++.exe}
export PATH="/c/msys64/ucrt64/bin:$PATH"
"$GXX" -O2 -std=c++17 -I src tests/framepacer/framepacer.cpp -o "$out/fp.exe" -lwinmm || { echo "BUILD FAIL green"; exit 2; }
"$GXX" -O2 -std=c++17 -DFRAMEPACER_RED -I src tests/framepacer/framepacer.cpp -o "$out/fp_red.exe" -lwinmm || { echo "BUILD FAIL red"; exit 2; }
"$out/fp.exe"; g=$?
FRAMEPACER_NO_PC=1 "$out/fp_red.exe"; r=$?
if [ $g -eq 0 ] && [ $r -ne 0 ]; then echo "FRAMEPACER OK (green passes, red fails)"; exit 0; fi
echo "FRAMEPACER BAD (green rc=$g, red rc=$r)"; exit 1
