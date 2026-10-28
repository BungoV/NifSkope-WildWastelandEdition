#!/bin/bash
# lane WEATHERBLEND6: the weather-change unit test on Linux (system g++ and Qt6Core, pkg-config): green, the switch red
# (WW_CELL_REGION_RED=spawnonly) and the base's red (its esmweather.cpp). Green must PASS, both reds must FAIL.
# usage: bash tests/weatherblend/run_linux.sh <out dir> [base commit]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="${1:?out dir}"
BASE="${2:-a638e35a}"	# WEATHERBLEND6's base
PY="$(command -v python3 || command -v python)"
mkdir -p "$OUT/green" "$OUT/red"
git -C "$ROOT" show "$BASE:src/esmweather.cpp" > "$OUT/red/esmweather_base.cpp" || exit 2
"$PY" -I "$HERE/cut.py" "$ROOT/src/esmweather.cpp" "$ROOT/src/esmweather.cpp" "$OUT/green/cut.inc" < /dev/null || exit 2
"$PY" -I "$HERE/cut.py" "$ROOT/src/esmweather.cpp" "$OUT/red/esmweather_base.cpp" "$OUT/red/cut.inc" < /dev/null || exit 2
for side in green red; do
	( cd "$OUT/$side" && g++ -std=c++17 -O1 -w -fPIC -I. -I"$ROOT/src" $(pkg-config --cflags Qt6Core) "$HERE/weatherblend.cpp" \
		-o weatherblend $(pkg-config --libs Qt6Core) > build.log 2>&1; echo "$side BUILD-RC=$?" )
done
"$OUT/green/weatherblend" > "$OUT/green/run.log" 2>&1; echo "green RUN-RC=$?"; tail -1 "$OUT/green/run.log"
WW_CELL_REGION_RED=spawnonly "$OUT/green/weatherblend" > "$OUT/green/red_switch.log" 2>&1; echo "red switch RUN-RC=$?"
tail -1 "$OUT/green/red_switch.log"
"$OUT/red/weatherblend" > "$OUT/red/run.log" 2>&1; echo "red base RUN-RC=$?"; tail -1 "$OUT/red/run.log"
