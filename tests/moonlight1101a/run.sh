#!/bin/bash
# Lane FIX1101A (moon): the cell view's night key light (gl/moonlight.h + lookdevstage.cpp computeLight) on its twin.
# tests/moonlight1101a/run.sh <out dir>   (from the tree root; PY = a Windows python). Green must print "GATE OK"; the red
# handover (the old switch at the engine's D + the ghost sun) must FAIL handover, ghost and engine; the red phase8 (the
# vanilla 8-step phase) must FAIL phase; the red suncolumn (the moon through the whole fog column) must FAIL moon-fog.
# Each run is capped by `timeout 120`.
set -u
OUT="${1:?usage: run.sh <out dir>}"
PY="${PY:-$(command -v python)}"
mkdir -p "$OUT"
T=tests/moonlight1101a/moon_check.py
bad=0
# the C++ header against the twin (UCRT64 g++), when a compiler is there
DUMP=()
if [ -x /c/msys64/ucrt64/bin/g++.exe ]; then
	PATH=/c/msys64/ucrt64/bin:$PATH g++ -std=c++17 -O1 -I src tests/moonlight1101a/moonlight_dump.cpp -o "$OUT/moonlight_dump.exe" \
		&& DUMP=( --dump "$OUT/moonlight_dump.exe" ) || { echo "DUMP BUILD FAILED"; bad=1; }
fi
timeout 120 "$PY" -I "$T" --tables "${DUMP[@]}" > "$OUT/green.txt" 2>&1 < /dev/null; rc=$?
grep -E '^(PASS|FAIL|GATE)' "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^GATE OK' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
for spec in "handover:handover ghost engine" "phase8:phase" "suncolumn:moon-fog"; do
	red="${spec%%:*}"
	timeout 120 "$PY" -I "$T" --red "$red" > "$OUT/red_$red.txt" 2>&1 < /dev/null; rc=$?
	for g in ${spec#*:}; do
		if [ $rc -eq 1 ] && grep -q "^FAIL $g:" "$OUT/red_$red.txt"; then
			echo "red $red: fails $g as it must: $(grep -m1 "^FAIL $g:" "$OUT/red_$red.txt")"
		else
			echo "RED $red DID NOT FAIL $g (rc $rc)"; bad=1
		fi
	done
done
[ $bad -eq 0 ] && echo "MOONLIGHT1101A GATE OK (green + 3 reds)" || echo "MOONLIGHT1101A GATE FAILED"
exit $bad
