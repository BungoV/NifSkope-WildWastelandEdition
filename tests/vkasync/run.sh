#!/bin/bash
# Lane VKCOMPUTE1: the async family pick + GL wait placement (src/features/gi/vkasyncrule.h), green and every red.
#   tests/vkasync/run.sh <out dir>   (from the tree root; put /c/msys64/ucrt64/bin first on PATH)
# Green prints "VKASYNC PASS 6"; each red must print "FAIL <its check>:" and exit 1.
set -u
OUT="${1:?usage: run.sh <out dir>}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
SRC=tests/vkasync/vkasync_test.cpp
bad=0
"$CXX" -O2 -std=c++17 -Wall -I src "$SRC" -o "$OUT/green.exe" -static || { echo "BUILD FAIL green"; exit 2; }
timeout 60 "$OUT/green.exe" > "$OUT/green.txt"; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^VKASYNC PASS 6$' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
for pair in GRAPHICS:pick NOTS:ts EAGER:overlap NOSETTLE:readsafe; do
	red="${pair%%:*}"; check="${pair##*:}"
	"$CXX" -O2 -std=c++17 -I src -DVKASYNC_RED_$red "$SRC" -o "$OUT/red_$check.exe" -static || { echo "BUILD FAIL red $red"; bad=1; continue; }
	timeout 60 "$OUT/red_$check.exe" > "$OUT/red_$check.txt"; rc=$?
	if [ $rc -eq 1 ] && grep -q "^FAIL $check:" "$OUT/red_$check.txt"; then
		echo "red $red: fails $check as it must ($(grep -c '^FAIL' "$OUT/red_$check.txt") check(s) failed)"
	else
		echo "RED $red DID NOT FAIL $check (rc $rc)"; bad=1
	fi
done
[ $bad -eq 0 ] && echo "VKASYNC GATE OK (green 6 + 4 reds)" || echo "VKASYNC GATE FAILED"
exit $bad
