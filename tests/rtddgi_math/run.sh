#!/bin/bash
# Lane RTXGI1 M1: build and run the DDGI math unit test, green and every red (tests/rtddgi_math/rtddgi_math_test.cpp).
#   tests/rtddgi_math/run.sh <out dir>        (from the tree root; CXX defaults to g++)
# Green must print "RTDDGI_MATH PASS 14"; each red build must print "FAIL <its check>" and exit 1.
# M2: the "sample" check compiles res/shaders/cell_ddgi.glsl itself (through tests/rtddgi_math/glsl_shim.h).
# Every run is capped by `timeout 30` (lane-hygiene: a new test exe never runs uncapped).
set -u
OUT="${1:?usage: run.sh <out dir>}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
SRC=tests/rtddgi_math/rtddgi_math_test.cpp
bad=0
"$CXX" -O2 -std=c++17 -Wall -I src "$SRC" -o "$OUT/green.exe" -static || { echo "BUILD FAIL green"; exit 2; }
timeout 30 "$OUT/green.exe" > "$OUT/green.txt"; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^RTDDGI_MATH PASS 14$' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
for pair in OCT:oct FIB:fib HYST:hyst GAMMA:gamma CHEB:cheb BIAS:bias BORDER:border SKY:sky NORM:norm CLASS:class RELOC:reloc TIE:tie SKYVIS:skyvis SAMPLE:sample; do
	red="${pair%%:*}"; check="${pair##*:}"
	"$CXX" -O2 -std=c++17 -I src -DRTDDGI_RED_$red "$SRC" -o "$OUT/red_$check.exe" -static || { echo "BUILD FAIL red $red"; bad=1; continue; }
	timeout 30 "$OUT/red_$check.exe" > "$OUT/red_$check.txt"; rc=$?
	if [ $rc -eq 1 ] && grep -q "^FAIL $check:" "$OUT/red_$check.txt"; then
		echo "red $red: fails $check as it must ($(grep -c '^FAIL' "$OUT/red_$check.txt") check(s) failed)"
	else
		echo "RED $red DID NOT FAIL $check (rc $rc)"; bad=1
	fi
done
[ $bad -eq 0 ] && echo "RTDDGI_MATH GATE OK (green 14 + 14 reds)" || echo "RTDDGI_MATH GATE FAILED"
exit $bad
