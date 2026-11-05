#!/bin/bash
# lane PERFFIX2 b (item 8a): gl/sunshadowdyn.h unit. green + two reds (each must FAIL):
#   RED_TRUNC (footprint texels truncated, no margin), RED_NORESTORE (the old footprint never copied back).
#   bash tests/features/shadows/sunshadowdyn/run.sh <out dir>
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../../../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED_TRUNC RED_NORESTORE; do
	d=""; [ $v = green ] || d="-DSUNDYN_$v"
	g++ -std=c++17 -O2 -Wall $d -I"$ROOT/src" "$HERE/sunshadowdyn_test.cpp" -o "$O/ssd_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	timeout 120 "$O/ssd_$v.exe" > "$O/ssd_$v.log" 2>&1; r=$?
	tail -n 1 "$O/ssd_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || { rc=1; grep '^FAIL ' "$O/ssd_$v.log" | sed "s/^/$v:   /"; }
	else
		[ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
		grep '^FAIL ' "$O/ssd_$v.log" | sed "s/^/$v:   /"
	fi
done
echo "SUNSHADOWDYN UNIT RUN $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
