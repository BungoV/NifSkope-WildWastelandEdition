#!/bin/bash
# Lane SKYBELOW2: build and run the reflection probes' below-horizon rule test, green and every red
# (tests/skybelow2/skybelow2_test.cpp compiles res/shaders/cell_skybelow.glsl itself, through
# tests/rtddgi_math/glsl_shim.h, against SKYBELOW1's src/rtddgimath.h).
#   tests/skybelow2/run.sh <out dir>        (from the tree root; CXX defaults to g++, UCRT64 first on PATH)
# Green must print "SKYBELOW2 PASS 6"; each red build must print "FAIL <its check>" and exit 1.
# Every run is capped by `timeout 30` (lane-hygiene: a new test exe never runs uncapped).
set -u
OUT="${1:?usage: run.sh <out dir>}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
SRC=tests/skybelow2/skybelow2_test.cpp
bad=0
"$CXX" -O2 -std=c++17 -Wall -I src "$SRC" -o "$OUT/green.exe" -static || { echo "BUILD FAIL green"; exit 2; }
timeout 30 "$OUT/green.exe" > "$OUT/green.txt"; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^SKYBELOW2 PASS 6$' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
for pair in MISS:below MISS:openprobe LIFT:above INTGROUND:interior NOSWITCH:red ALBEDO:albedo; do
	red="${pair%%:*}"; check="${pair##*:}"
	"$CXX" -O2 -std=c++17 -I src -DSKYBELOW2_RED_$red "$SRC" -o "$OUT/red_$check.exe" -static || { echo "BUILD FAIL red $red"; bad=1; continue; }
	timeout 30 "$OUT/red_$check.exe" > "$OUT/red_$check.txt"; rc=$?
	if [ $rc -eq 1 ] && grep -q "^FAIL $check:" "$OUT/red_$check.txt"; then
		echo "red $red: fails $check as it must ($(grep -c '^FAIL' "$OUT/red_$check.txt") check(s) failed): $(grep "^FAIL $check:" "$OUT/red_$check.txt")"
	else
		echo "RED $red DID NOT FAIL $check (rc $rc)"; bad=1
	fi
done
[ $bad -eq 0 ] && echo "SKYBELOW2 GATE OK (green 6 + 6 reds)" || echo "SKYBELOW2 GATE FAILED"
exit $bad
