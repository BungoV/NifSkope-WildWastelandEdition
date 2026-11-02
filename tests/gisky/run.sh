#!/bin/bash
# FIX1101A LATE ITEM 15d unit: the ray-traced GI's sky from the physical sky (tests/gisky/gisky_test.cpp).
#   bash tests/gisky/run.sh <out dir>        (from the tree root; CXX defaults to g++) rc 0 = green 5 AND both reds failed
set -u
OUT="${1:?usage: run.sh <out dir>}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
SRC=tests/gisky/gisky_test.cpp
"$CXX" -O2 -std=c++17 -Wall -I src "$SRC" -o "$OUT/green.exe" -static || { echo "BUILD FAIL green"; exit 2; }
timeout 60 "$OUT/green.exe" > "$OUT/green.txt"; g=$?
"$CXX" -O2 -std=c++17 -I src -DGISKY_RED "$SRC" -o "$OUT/red.exe" -static || { echo "BUILD FAIL red"; exit 2; }
timeout 60 "$OUT/red.exe" > "$OUT/red.txt"; r=$?
# GITRUTH1: the absolute level's red (the weather-luminance anchor; the DALC as is for levelw) must fail level + levelw only
"$CXX" -O2 -std=c++17 -I src -DGISKY_RED_LEVEL "$SRC" -o "$OUT/redlevel.exe" -static || { echo "BUILD FAIL redlevel"; exit 2; }
timeout 60 "$OUT/redlevel.exe" > "$OUT/redlevel.txt"; rl=$?
cat "$OUT/green.txt" "$OUT/red.txt" "$OUT/redlevel.txt"
if [ $g -eq 0 ] && grep -q '^GISKY PASS 5$' "$OUT/green.txt" && [ $r -eq 1 ] && grep -q '^FAIL sunside:' "$OUT/red.txt" \
	&& [ $rl -eq 1 ] && grep -q '^FAIL level:' "$OUT/redlevel.txt" && grep -q '^FAIL levelw:' "$OUT/redlevel.txt" \
	&& grep -q '^GISKY FAIL 2 failed$' "$OUT/redlevel.txt"; then
	echo "GISKY GATE OK (green 5, red fails sunside, redlevel fails level + levelw)"
	exit 0
fi
echo "GISKY GATE FAIL (green rc $g, red rc $r, redlevel rc $rl)"
exit 1
