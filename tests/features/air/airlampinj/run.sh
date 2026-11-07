#!/bin/bash
# Lane FOGGRID22A: build and run the lamp-injection twin's unit gate (src/features/air/airlampinjmath.h), green and every
# red in one process (the 16^3 references are built once and shared).
#   tests/features/air/airlampinj/run.sh <out dir>   (from the tree root; CXX defaults to g++; /c/msys64/ucrt64/bin first on PATH)
# Must print "AIRLAMPINJ PASS": every G row ok on the default, and every red failing its own row only.
# Reads res/air/stbn_scalar_128x128x64.r8 (the shipped mask) and res/shaders/features/air/cell_volair.frag (G5 anchors).
# Capped by `timeout 3000` (lane-hygiene: a new test exe never runs uncapped).
set -u
OUT="${1:?usage: run.sh <out dir>}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
SRC=tests/features/air/airlampinj/airlampinj_test.cpp
"$CXX" -O2 -std=c++17 -Wall -I src "$SRC" -o "$OUT/airlampinj_test.exe" -static || { echo "BUILD FAIL"; exit 2; }
timeout 3000 "$OUT/airlampinj_test.exe" . > "$OUT/airlampinj.txt"; rc=$?
cat "$OUT/airlampinj.txt"
if [ $rc -ne 0 ] || ! grep -q '^AIRLAMPINJ PASS' "$OUT/airlampinj.txt"; then echo "AIRLAMPINJ GATE FAILED (rc $rc)"; exit 1; fi
echo "AIRLAMPINJ GATE OK"
