#!/bin/bash
# Lane AIRZONE1: the files gate (tests/airzone/airzone_recx_gate.cpp) -- the real src/airzone.cpp + src/recx.cpp +
# src/wwswitch.cpp with Qt6Core only (UCRT64 g++, as tests/weatherrecx), no window, no game data.
#   tests/airzone/gate.sh <out dir>          (from the tree root; fixtures go under <out dir>, never a game Data folder)
# Green must print "AIRZONE_RECX PASS 6" and exit 0; the red run (WW_CELL_AIRZONE_RED=novol: the files ignored) must
# exit 1 with roundtrip, keep and gi-* failing. Every run is capped by `timeout 60`.
set -u
OUT="${1:?usage: gate.sh <out dir>}"
CXX="${CXX:-g++}"
QTINC="${QTINC:-/ucrt64/include/qt6}"
mkdir -p "$OUT"
"$CXX" -std=c++17 -O1 -w -I src -I "$QTINC" -I "$QTINC/QtCore" tests/airzone/airzone_recx_gate.cpp src/airzone.cpp \
	src/recx.cpp src/wwswitch.cpp -lQt6Core -o "$OUT/gate.exe" || { echo "BUILD FAIL"; exit 2; }
bad=0
env -u WW_CELL_AIRZONE_RED -u WW_RECX_ROOT timeout 60 "$OUT/gate.exe" "$OUT/green" > "$OUT/green.txt" 2> "$OUT/green.err"; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^AIRZONE_RECX PASS 6$' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
WW_CELL_AIRZONE_RED=novol timeout 60 "$OUT/gate.exe" "$OUT/red" > "$OUT/red.txt" 2> "$OUT/red.err"; rc=$?
if [ $rc -eq 1 ] && grep -q '^FAIL roundtrip:' "$OUT/red.txt" && grep -q '^FAIL gi-all:' "$OUT/red.txt" && grep -q '^FAIL gi-rooms:' "$OUT/red.txt"; then
	echo "red novol: fails as it must ($(grep -c '^FAIL' "$OUT/red.txt") check(s) failed)"
else
	echo "RED novol DID NOT FAIL (rc $rc)"; cat "$OUT/red.txt"; bad=1
fi
[ $bad -eq 0 ] && echo "AIRZONE_RECX GATE OK (green 6 + red novol)" || echo "AIRZONE_RECX GATE FAILED"
exit $bad
