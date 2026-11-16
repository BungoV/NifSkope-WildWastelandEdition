#!/bin/bash
# lane CRASH1116: the cascades' mesh pool capacity (features/vkframe/csmpoolcap.h). green + one red (must FAIL):
#   RED_POSONLY (the law before the fix: positions alone). bash tests/csmpoolcap/run.sh <out dir>
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED_POSONLY; do
	d=""; [ $v = green ] || d="-DCSMPOOL_$v"
	g++ -std=c++17 -O2 -Wall $d -I"$ROOT/src" "$HERE/csmpoolcap_test.cpp" -o "$O/cp_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	timeout 120 "$O/cp_$v.exe" > "$O/cp_$v.log" 2>&1; r=$?
	grep -E '^(sweep|flight|FAIL)' "$O/cp_$v.log" | sed "s/^/$v: /"
	tail -n 1 "$O/cp_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || rc=1
	else [ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
	fi
done
echo "CSMPOOLCAP UNIT RUN $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
