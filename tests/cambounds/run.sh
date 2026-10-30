#!/bin/bash
# lane CAMGATE1: the camera bounds rule's unit test. green + one red per rule (each red must FAIL).
#   bash tests/cambounds/run.sh <out dir>
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED_UNDER RED_OUTSIDE; do
	d=""; [ $v = green ] || d="-DCAMBOUNDS_$v"
	g++ -std=c++17 -O1 $d -I"$ROOT/src" "$HERE/cambounds_test.cpp" -o "$O/cambounds_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	"$O/cambounds_$v.exe" > "$O/cambounds_$v.log" 2>&1; r=$?
	tail -n 1 "$O/cambounds_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || rc=1
	else
		[ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
		grep '^FAIL ' "$O/cambounds_$v.log" | sed "s/^/$v:   /"
	fi
done
echo "CAMBOUNDS UNIT $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
