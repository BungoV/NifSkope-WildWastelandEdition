#!/bin/bash
# lane WEATHERRECX3: build dump.cpp against one tree's real reader.  bash build.sh <tree with src/> <out exe> [-DWX3_OLD]
set -u
T="$1"; O="$2"; D="${3:-}"
H="$(cd "$(dirname "$0")" && pwd)"
export PATH=/c/msys64/ucrt64/bin:$PATH
Q=/c/msys64/ucrt64/include/qt6
g++ -std=c++17 -O1 -w $D -I"$T/src" -I$Q -I$Q/QtCore "$H/dump.cpp" "$T/src/features/weather/weatherrecx.cpp" \
  "$T/src/recx.cpp" "$T/src/wwswitch.cpp" -lQt6Core -o "$O" > "$O.build.log" 2>&1
RC=$?
echo "BUILD-RC=$RC $O"
grep 'error\|undefined' "$O.build.log" | head -20
exit $RC
