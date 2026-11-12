#!/bin/bash
# lane CLOUDRECX1: build cloudrecx14_gate.cpp against the tree's reader and run green + the red.
#   PATH=/c/msys64/ucrt64/bin:$PATH bash tests/features/clouds/cloudrecx14/build.sh <out dir>
# Green: every check PASS (exit 0). Red WW_CLOUD_RECX14_RED=1 (the 1.3 clouds block): must FAIL (exit 1).
set -u
T="$(cd "$(dirname "$0")/../../../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
Q=/c/msys64/ucrt64/include/qt6
g++ -std=c++17 -O1 -w -I"$T/src" -I$Q -I$Q/QtCore \
  "$T/tests/features/clouds/cloudrecx14/cloudrecx14_gate.cpp" "$T/src/features/weather/weatherrecx.cpp" "$T/src/recx.cpp" \
  "$T/src/wwswitch.cpp" -lQt6Core -o "$O/cloudrecx14_gate.exe" > "$O/build.log" 2>&1
echo "BUILD-RC=$?"
"$O/cloudrecx14_gate.exe" "$O" "$T" > "$O/green.out" 2> "$O/green.err"; echo "GREEN-RC=$?"
WW_CLOUD_RECX14_RED=1 "$O/cloudrecx14_gate.exe" "$O" "$T" > "$O/red.out" 2> "$O/red.err"; echo "RED-RC=$?"
# the warnings the reader must print (stderr): the legacy alias, the ignored density, the clamp
grep -c 'density is the 1.4 legacy alias' "$O/green.err"
grep -c 'density ignored (liquidWater' "$O/green.err"
grep -c 'iceFraction 1.5 clamped' "$O/green.err"
grep -m3 '^cloud recx 1.4:' "$O/green.err"
tail -1 "$O/green.out"; tail -1 "$O/red.out"
