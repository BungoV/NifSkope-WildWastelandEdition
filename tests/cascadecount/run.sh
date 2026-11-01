#!/bin/bash
# lane PERFFIX2 b (item 8b): the cascade count per tier (gl/sunshadowfit.h). green + two reds (each must FAIL):
#   RED_TODAY (3 cascades at Low, today's count), RED_RECV (the receivers still select the undrawn cascade 2).
#   bash tests/cascadecount/run.sh <out dir>
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED_TODAY RED_RECV; do
	d=""; [ $v = green ] || d="-DCASCADES_$v"
	g++ -std=c++17 -O1 -Wall $d -I"$ROOT/src" "$HERE/cascadecount_test.cpp" -o "$O/cc_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	timeout 120 "$O/cc_$v.exe" "$ROOT" > "$O/cc_$v.log" 2>&1; r=$?
	tail -n 1 "$O/cc_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || { rc=1; grep '^FAIL ' "$O/cc_$v.log" | sed "s/^/$v:   /"; }
	else
		[ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
		grep '^FAIL ' "$O/cc_$v.log" | sed "s/^/$v:   /"
	fi
done
echo "CASCADECOUNT UNIT RUN $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
