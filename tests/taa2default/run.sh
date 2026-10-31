#!/bin/bash
# FIX1101A TAA2 default unit: Modern temporal AA ships High under a new settings key; harness default stays Off.
#   bash tests/taa2default/run.sh <out dir>        rc 0 = green AND the red failed
# Green: taa2default_src.py on this tree's src/gl/gametaa.cpp. Red (must FAIL): main 9b443dcf's gametaa.cpp (ships Off).
set -u
OUT=${1:-taa2default_out}
mkdir -p "$OUT"
PY="${PY:-python}"
D=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$D/../.." && pwd)
"$PY" -I "$D/taa2default_src.py" "$R/src/gl/gametaa.cpp" > "$OUT/green.txt"; g=$?
r=77
if git -C "$R" show 9b443dcf:src/gl/gametaa.cpp > "$OUT/main_gametaa.cpp" 2> /dev/null; then
	"$PY" -I "$D/taa2default_src.py" "$OUT/main_gametaa.cpp" > "$OUT/red.txt"; r=$?
fi
cat "$OUT/green.txt"
[ -f "$OUT/red.txt" ] && cat "$OUT/red.txt"
echo "green rc=$g; red (must be 1) rc=$r"
if [ $g -eq 0 ] && [ $r -eq 1 ]; then
	echo "TAA2DEFAULT GATE OK"
	exit 0
fi
echo "TAA2DEFAULT GATE FAIL"
exit 1
