#!/bin/bash
# lanes WX1 / WEATHERRECX1: build weatherrecx_gate.cpp against the tree's reader and run green + the two old reds.
#   PATH=/c/msys64/ucrt64/bin:$PATH bash tests/features/weather/weatherrecx/build.sh <fixture dir> <out dir>
# The fixtures (full, merge, prec, bad-values, case-free, each a Data root) are lane WX1's notes/wx1/fixture.
set -u
T="$(cd "$(dirname "$0")/../../../.." && pwd)"
FX="${1:?fixture dir}"; O="${2:?out dir}"; mkdir -p "$O"
Q=/c/msys64/ucrt64/include/qt6
g++ -std=c++17 -O1 -w -I"$T/src" -I$Q -I$Q/QtCore \
  "$T/tests/features/weather/weatherrecx/weatherrecx_gate.cpp" "$T/src/features/weather/weatherrecx.cpp" "$T/src/recx.cpp" \
  "$T/src/wwswitch.cpp" "$T/src/features/weather/wxoverride.cpp" "$T/src/recxmodroots.cpp" \
  "$T/tests/features/weather/weatherrecx/stub_modsource.cpp" -lQt6Core -o "$O/weatherrecx_gate.exe" > "$O/build.log" 2>&1
echo "BUILD-RC=$?"
"$O/weatherrecx_gate.exe" "$FX" > "$O/green.out" 2> "$O/green.err"; echo "GREEN-RC=$?"
WW_WEATHER_RECX_RED=ignore "$O/weatherrecx_gate.exe" "$FX" > "$O/red_ignore.out" 2> "$O/red_ignore.err"; echo "RED-IGNORE-RC=$?"
WW_WEATHER_RECX_RED=recordfirst "$O/weatherrecx_gate.exe" "$FX" > "$O/red_recordfirst.out" 2> "$O/red_recordfirst.err"
echo "RED-RECORDFIRST-RC=$?"
tail -1 "$O/green.out"; tail -1 "$O/red_ignore.out"; tail -1 "$O/red_recordfirst.out"
