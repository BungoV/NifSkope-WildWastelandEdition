#!/bin/bash
# lane PERFFIX1 (item 1): the cell-uniform class record (src/gl/cellunirec.h): green must PASS, the three reds must FAIL.
# Usage: bash tests/cellunirec/run.sh [out dir]   rc 0 = as expected
cd "$(dirname "$0")/../.."
OUT=${1:-tests/cellunirec/out}
mkdir -p "$OUT"
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED_NOBIND RED_BYLOC RED_ONECLASS; do
	D=""; [ "$v" != green ] && D="-D$v"
	g++ -std=c++17 -O1 $D -Isrc tests/cellunirec/cellunirec.cpp -o "$OUT/cellunirec_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	out=$(timeout 60 "$OUT/cellunirec_$v.exe")
	r=$?
	echo "$v: $(echo "$out" | tail -2 | tr '\n' ' ')"
	if [ "$v" = green ] && [ $r -ne 0 ]; then rc=1; fi
	if [ "$v" != green ] && [ $r -eq 0 ]; then echo "RED $v PASSED: the test cannot see it"; rc=1; fi
done
echo "cellunirec run rc=$rc"
exit $rc
