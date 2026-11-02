#!/bin/bash
# lane STREAMTEX5 unit (STREAMTEX4-WWCOVKEY): a TREELEAF1 coverage copy is pooled under its noted file (g++ only).
# Usage: bash tests/wwcovkey/run.sh [outdir]     PASS = the green passes and the red fails.
#   green  keys stripped of "|wwcov<ref>": the copy of a noted file is rule Keep (kTierKeep), 0 unnoted
#   red    WW_CELL_STREAMTEX_RED=rawkey (the raw cache name): the copy is unnoted and rule Timer -> FAIL
T=$(cd "$(dirname "$0")/../.." && pwd)
O=${1:-$T/tests/wwcovkey/out}
mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
CXX=${CXX:-g++}
$CXX -std=c++17 -O2 -I"$T/src" "$T/tests/wwcovkey/wwcovkey_test.cpp" -o "$O/wwcovkey_test.exe" || { echo "BUILD FAIL"; exit 2; }
fails=0
run() {	# name expected-rc args...
	local n=$1 want=$2
	shift 2
	timeout 60 "$O/wwcovkey_test.exe" "$@" > "$O/$n.txt" 2>&1 < /dev/null
	local rc=$?
	echo "== $n rc=$rc (want $want): $(tail -1 "$O/$n.txt")"
	[ "$rc" = "$want" ] || fails=$((fails + 1))
}
run green 0
run red 1 --red
if [ "$fails" = 0 ]; then
	echo "WWCOVKEY UNIT PASS"
	exit 0
fi
echo "WWCOVKEY UNIT FAIL ($fails)"
exit 1
