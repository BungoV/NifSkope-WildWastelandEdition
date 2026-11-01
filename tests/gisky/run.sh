#!/bin/bash
# FIX1101A LATE ITEM 15d unit: the ray-traced GI's sky from the physical sky (tests/gisky/gisky_test.cpp).
#   bash tests/gisky/run.sh <out dir>        (from the tree root; CXX defaults to g++) rc 0 = green 3 AND the red failed
set -u
OUT="${1:?usage: run.sh <out dir>}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
SRC=tests/gisky/gisky_test.cpp
"$CXX" -O2 -std=c++17 -Wall -I src "$SRC" -o "$OUT/green.exe" -static || { echo "BUILD FAIL green"; exit 2; }
timeout 60 "$OUT/green.exe" > "$OUT/green.txt"; g=$?
"$CXX" -O2 -std=c++17 -I src -DGISKY_RED "$SRC" -o "$OUT/red.exe" -static || { echo "BUILD FAIL red"; exit 2; }
timeout 60 "$OUT/red.exe" > "$OUT/red.txt"; r=$?
cat "$OUT/green.txt" "$OUT/red.txt"
if [ $g -eq 0 ] && grep -q '^GISKY PASS 3$' "$OUT/green.txt" && [ $r -eq 1 ] && grep -q '^FAIL sunside:' "$OUT/red.txt"; then
	echo "GISKY GATE OK (green 3, red fails sunside)"
	exit 0
fi
echo "GISKY GATE FAIL (green rc $g, red rc $r)"
exit 1
