#!/bin/bash
# FIX1101A unit: the ray-traced GI volume's build key holds the soup's albedo (a WW_SOUPALB_RED reopen rebuilds the volume).
#   bash tests/rtddgikey/run.sh <out dir>        rc 0 = green AND the red failed
# Green: rtddgikey_src.py on this tree's src/rtddgicell.cpp. Red (must FAIL): the same rules on main 9b443dcf's file.
# PY = a python (the Windows one; no numpy needed).
set -u
OUT=${1:-rtddgikey_out}
mkdir -p "$OUT"
PY="${PY:-python}"
D=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$D/../.." && pwd)
"$PY" -I "$D/rtddgikey_src.py" "$R/src/rtddgicell.cpp" > "$OUT/green.txt"; g=$?
r=77
if git -C "$R" show 9b443dcf:src/rtddgicell.cpp > "$OUT/main_rtddgicell.cpp" 2> /dev/null; then
	"$PY" -I "$D/rtddgikey_src.py" "$OUT/main_rtddgicell.cpp" > "$OUT/red.txt"; r=$?
fi
cat "$OUT/green.txt"
[ -f "$OUT/red.txt" ] && cat "$OUT/red.txt"
echo "green rc=$g; red (must be 1) rc=$r"
if [ $g -eq 0 ] && [ $r -eq 1 ]; then
	echo "RTDDGIKEY GATE OK"
	exit 0
fi
echo "RTDDGIKEY GATE FAIL"
exit 1
