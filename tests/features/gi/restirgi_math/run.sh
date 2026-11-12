#!/bin/bash
# Lane RTEMIS1 part 2: build and run the ReSTIR GI unit test, green and every red
# (tests/features/gi/restirgi_math/restirgi_math_test.cpp, the rules in src/features/gi/restirgimath.h).
#   tests/features/gi/restirgi_math/run.sh <out dir>        (from the tree root; CXX defaults to g++)
# Green must print "RESTIRGI_MATH PASS 0 failed"; each red build must print "FAIL <its check>:" and exit 1.
# Run with /c/msys64/ucrt64/bin first on PATH (a static exe, but the compiler's own DLLs). Every run is capped by timeout 120.
set -u
OUT="${1:?usage: run.sh <out dir>}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
SRC=tests/features/gi/restirgi_math/restirgi_math_test.cpp
bad=0
"$CXX" -O2 -std=c++17 -Wall -I src "$SRC" -o "$OUT/green.exe" -static || { echo "BUILD FAIL green"; exit 2; }
timeout 120 "$OUT/green.exe" > "$OUT/green.txt"; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^RESTIRGI_MATH PASS 0 failed$' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
for pair in NOPDF:pdf NOJACOBIAN:jacobian NOVIS:vis NOTEMPORAL:temporal MIRROR:mirror SKIPM:skip; do
	red="${pair%%:*}"; check="${pair##*:}"
	"$CXX" -O2 -std=c++17 -I src -DRESTIRGI_RED_$red "$SRC" -o "$OUT/red_$check.exe" -static || { echo "BUILD FAIL red $red"; bad=1; continue; }
	timeout 120 "$OUT/red_$check.exe" > "$OUT/red_$check.txt"; rc=$?
	if [ $rc -eq 1 ] && grep -q "^FAIL $check:" "$OUT/red_$check.txt"; then
		echo "red $red: fails $check as it must: $(grep "^FAIL $check:" "$OUT/red_$check.txt" | cut -c1-160) ($(grep -c '^FAIL' "$OUT/red_$check.txt") check(s) failed)"
	else
		echo "RED $red DID NOT FAIL $check (rc $rc)"; bad=1
	fi
done
[ $bad -eq 0 ] && echo "RESTIRGI_MATH GATE OK (green 6 + 6 reds)" || echo "RESTIRGI_MATH GATE FAILED"
exit $bad
