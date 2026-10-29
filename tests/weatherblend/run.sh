#!/bin/bash
# lane WEATHERBLEND1: build and run the weather-change unit test, green (this tree) and red (BASE's blends).
# usage: bash tests/weatherblend/run.sh <out dir> [base commit]   (green must PASS, red must FAIL)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="${1:?out dir}"
BASE="${2:-a638e35a}"	# WEATHERBLEND6's base (5: 940baf4a, 4: 5dd180a5, 3: ddf6483e, 2: 50023dfa, 1: 0db18b4b)
mkdir -p "$OUT/green" "$OUT/red"
git -C "$ROOT" show "$BASE:src/esmweather.cpp" > "$OUT/red/esmweather_base.cpp" || exit 2
python -I "$HERE/cut.py" "$ROOT/src/esmweather.cpp" "$ROOT/src/esmweather.cpp" "$OUT/green/cut.inc" < /dev/null || exit 2
python -I "$HERE/cut.py" "$ROOT/src/esmweather.cpp" "$OUT/red/esmweather_base.cpp" "$OUT/red/cut.inc" < /dev/null || exit 2
for side in green red; do
	MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc "cd '$OUT/$side' && g++ -std=c++17 -O1 -w -I. -I'$ROOT/src' -I/ucrt64/include/qt6 -I/ucrt64/include/qt6/QtCore '$HERE/weatherblend.cpp' -o weatherblend.exe -lQt6Core > build.log 2>&1; echo BUILD-RC=\$?" < /dev/null
	( export PATH="/c/msys64/ucrt64/bin:$PATH"; "$OUT/$side/weatherblend.exe" > "$OUT/$side/run.log" 2>&1; echo "$side RUN-RC=$?" )
	tail -1 "$OUT/$side/run.log"
done
# lane WEATHERBLEND6: the switch red on the green build: the region walk's crossing must fail (Linux: run_linux.sh)
( export PATH="/c/msys64/ucrt64/bin:$PATH" WW_CELL_REGION_RED=spawnonly; "$OUT/green/weatherblend.exe" > "$OUT/green/red_switch.log" 2>&1; echo "red switch RUN-RC=$?" )
tail -1 "$OUT/green/red_switch.log"
