#!/bin/bash
# WEATHERRECX2 part 4 gate: build lawgate.cpp against <tree>/src, make the fixture root, run green + reds 1..3.
# usage: bash tests/features/weather/preciptypes/run.sh <tree> <out>   (green PASS; reds 1..3 must FAIL)
set -u
TREE="${1:?tree}"
OUT="${2:?out}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
rm -rf "$OUT/fixture"
python -I "$HERE/mkprecip.py" "$OUT/fixture" --extra
g++ -std=c++17 -O1 -Wall -I"$TREE/src" -I/c/msys64/ucrt64/include/qt6 -I/c/msys64/ucrt64/include/qt6/QtCore \
	"$HERE/lawgate.cpp" -o "$OUT/lawgate.exe" -L/c/msys64/ucrt64/lib -lQt6Core > "$OUT/build.log" 2>&1
echo "BUILD-RC=$? warnings=$(grep -c warning "$OUT/build.log")"
FX="$(cygpath -m "$OUT/fixture")"
for red in 0 1 2 3; do
	LAWGATE_RED=$red "$OUT/lawgate.exe" "$FX" > "$OUT/run_red$red.log" 2>&1
	echo "red=$red RC=$? $(tail -2 "$OUT/run_red$red.log" | head -1) | $(tail -1 "$OUT/run_red$red.log")"
done
