#!/bin/bash
# lane SHADOWSHIMMER5: FARTILESPLIT unit (gl/fartile.h + src/lodinative.cpp's use of it).
# green + the red (it must FAIL).   bash tests/fartilesplit/run.sh <out dir>
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED; do
	d=""; [ $v = green ] || d="-DFTS_$v"
	g++ -std=c++17 -O1 -Wall $d -I"$ROOT/src" "$HERE/fartilesplit_test.cpp" -o "$O/fts_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	timeout 120 "$O/fts_$v.exe" "$ROOT" > "$O/fts_$v.log" 2>&1; r=$?
	tail -n 1 "$O/fts_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || rc=1
	else
		[ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
		grep '^FAIL ' "$O/fts_$v.log" | sed "s/^/$v:   /"
	fi
done
echo "FARTILESPLIT UNIT RUN $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
