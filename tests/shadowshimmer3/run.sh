#!/bin/bash
# lane SHADOWSHIMMER3: FARSWAYLEGACY / AIRTAILSLIDE unit (gl/sunshadowfit.h + the shipped sources).
# green + one red per rule (each red must FAIL).   bash tests/shadowshimmer3/run.sh <out dir>
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED_TREEANIM RED_AIRSLIDE; do
	d=""; [ $v = green ] || d="-DSS3_$v"
	g++ -std=c++17 -O1 -Wall $d -I"$ROOT/src" "$HERE/shadowshimmer3_test.cpp" -o "$O/ss3_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	timeout 120 "$O/ss3_$v.exe" "$ROOT" > "$O/ss3_$v.log" 2>&1; r=$?
	tail -n 1 "$O/ss3_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || rc=1
	else
		[ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
		grep '^FAIL ' "$O/ss3_$v.log" | sed "s/^/$v:   /"
	fi
done
echo "SHADOWSHIMMER3 UNIT RUN $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
