#!/bin/bash
# FIX1031B SOUPALB1 unit: the live ray-traced GI soup carries per-surface albedo, so covered shade < open shade.
#   bash tests/soupalb/run.sh <out dir>        rc 0 = green AND every red failed
# Green: soupalb_twin.py (per-surface albedo) and soupalb_src.py on this tree's src/cellview.cpp.
# Reds (must FAIL): the twin with today's grey 0.5 soup; the source rule on main 5422be6c's cellview.cpp.
# PY = a python with numpy (the Windows one; MSYS2's has none).
set -u
OUT=${1:-soupalb_out}
mkdir -p "$OUT"
PY="${PY:-python}"
D=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$D/../.." && pwd)
"$PY" -I "$D/soupalb_twin.py" > "$OUT/twin_green.txt"; g1=$?
"$PY" -I "$D/soupalb_twin.py" grey > "$OUT/twin_red.txt"; r1=$?
"$PY" -I "$D/soupalb_src.py" "$R/src/cellview.cpp" > "$OUT/src_green.txt"; g2=$?
r2=77
if git -C "$R" show 5422be6c:src/cellview.cpp > "$OUT/main_cellview.cpp" 2> /dev/null; then
	"$PY" -I "$D/soupalb_src.py" "$OUT/main_cellview.cpp" > "$OUT/src_red.txt"; r2=$?
fi
cat "$OUT/twin_green.txt" "$OUT/twin_red.txt" "$OUT/src_green.txt"
[ -f "$OUT/src_red.txt" ] && cat "$OUT/src_red.txt"
echo "green twin rc=$g1 src rc=$g2; reds (must be 1) twin rc=$r1 src rc=$r2"
if [ $g1 -eq 0 ] && [ $g2 -eq 0 ] && [ $r1 -eq 1 ] && [ $r2 -eq 1 ]; then
	echo "SOUPALB GATE OK"
	exit 0
fi
echo "SOUPALB GATE FAIL"
exit 1
