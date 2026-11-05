#!/bin/bash
# lane PERFFIX2 b (items 5b, 5c): gl/cubefacecull.h unit. green + two reds (each must FAIL): -DCUBEFACECULL_RED (the
# centre-only sphere test) and -DCUBEFACECULL_RED_CONE (a spot keeps only the faces whose axis lies in its cone).
#   bash tests/features/lights/cubefacecull/run.sh <out dir>
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../../../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED RED_CONE; do
	d=""; [ $v = green ] || d="-DCUBEFACECULL_$v"
	g++ -std=c++17 -O2 -Wall $d -I"$ROOT/src" "$HERE/cubefacecull_test.cpp" -o "$O/cfc_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	timeout 120 "$O/cfc_$v.exe" "$ROOT" > "$O/cfc_$v.log" 2>&1; r=$?
	tail -n 1 "$O/cfc_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || { rc=1; grep '^FAIL ' "$O/cfc_$v.log" | sed "s/^/$v:   /"; }
	else
		[ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
		grep '^FAIL ' "$O/cfc_$v.log" | sed "s/^/$v:   /"
	fi
done
echo "CUBEFACECULL UNIT RUN $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
