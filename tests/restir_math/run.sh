#!/bin/bash
# Lane RESTIRDI1: build and run the ReSTIR DI unit test + CPU reference gate, green and every red
# (tests/restir_math/restir_math_test.cpp).
#   tests/restir_math/run.sh <out dir>        (from the tree root; CXX defaults to g++; put /c/msys64/ucrt64/bin first on PATH)
# Green must print "RESTIR_MATH PASS 7"; each red build must print "FAIL <its check>:" and exit 1.
# The "mirror" check compiles src/rtddgi/restir_common.glsl itself (through tests/restir_math/restir_shim.h).
# Every run is capped by `timeout 120` (lane-hygiene: a new test exe never runs uncapped).
set -u
OUT="${1:?usage: run.sh <out dir>}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
SRC=tests/restir_math/restir_math_test.cpp
INC="-I src -I tests/restir_math"
bad=0
"$CXX" -O2 -std=c++17 -Wall $INC "$SRC" -o "$OUT/green.exe" -static || { echo "BUILD FAIL green"; exit 2; }
timeout 120 "$OUT/green.exe" > "$OUT/green.txt"; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^RESTIR_MATH PASS 7$' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
for pair in HASH:mirror VIEWPOS:viewpos NOSHAPE:shape NOVIS:vis NOPDF:pdf NOTEMPORAL:temporal NOCLAMP:clamp; do
	red="${pair%%:*}"; check="${pair##*:}"
	"$CXX" -O2 -std=c++17 $INC -DRESTIR_RED_$red "$SRC" -o "$OUT/red_$check.exe" -static || { echo "BUILD FAIL red $red"; bad=1; continue; }
	timeout 120 "$OUT/red_$check.exe" > "$OUT/red_$check.txt"; rc=$?
	if [ $rc -eq 1 ] && grep -q "^FAIL $check:" "$OUT/red_$check.txt"; then
		echo "red $red: fails $check as it must ($(grep -c '^FAIL' "$OUT/red_$check.txt") check(s) failed): $(grep "^FAIL $check:" "$OUT/red_$check.txt")"
	else
		echo "RED $red DID NOT FAIL $check (rc $rc)"; bad=1
	fi
done
[ $bad -eq 0 ] && echo "RESTIR_MATH GATE OK (green 7 + 7 reds)" || echo "RESTIR_MATH GATE FAILED"
exit $bad
