#!/bin/bash
# Lane HORIZONBAND1: the sky's horizon band (res/shaders/atmo_skyband.glsl) on bungo's defect frame, green and red.
#   tests/horizonband1/run.sh <out dir>        (from the tree root; CXX defaults to g++, UCRT64 first on PATH)
# Green must print "HORIZONBAND1 PASS"; the red build (-DHORIZONBAND1_RED: rule 0 = today's clamp, the switch
# WW_ATMO_AP_RED=skyclamp) must exit 1 with FAIL step, FAIL cyan and FAIL meet. Every run is capped by `timeout 30`.
set -u
OUT="${1:?usage: run.sh <out dir>}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
SRC=tests/horizonband1/horizonband1_test.cpp
FX=tests/horizonband1/c3_column.txt
bad=0
"$CXX" -O2 -std=c++17 -Wall -I tests/horizonband1 "$SRC" -o "$OUT/green.exe" -static || { echo "BUILD FAIL green"; exit 2; }
timeout 30 "$OUT/green.exe" "$FX" > "$OUT/green.txt"; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^HORIZONBAND1 PASS$' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
"$CXX" -O2 -std=c++17 -I tests/horizonband1 -DHORIZONBAND1_RED "$SRC" -o "$OUT/red.exe" -static || { echo "BUILD FAIL red"; exit 2; }
timeout 30 "$OUT/red.exe" "$FX" > "$OUT/red.txt"; rc=$?
for c in step cyan meet; do
	if [ $rc -eq 1 ] && grep -q "^FAIL $c:" "$OUT/red.txt"; then
		echo "red: fails $c as it must: $(grep -m1 "^FAIL $c:" "$OUT/red.txt")"
	else
		echo "RED DID NOT FAIL $c (rc $rc)"; bad=1
	fi
done
[ $bad -eq 0 ] && echo "HORIZONBAND1 GATE OK (green + red)" || echo "HORIZONBAND1 GATE FAILED"
exit $bad
