#!/bin/bash
# lane BINDREOPEN1 unit: the geometry pool over a stream walk and the reopen teardown order (g++ only, no GL, no Qt).
# Also re-runs tests/bindlesspool (the arena's offsets; its red -DPOOL_RED_OFF1 must still fail).
# Usage: bash tests/bindreopen1/run.sh [outdir]     PASS = every green passes and every red fails.
T=$(cd "$(dirname "$0")/../.." && pwd)
O=${1:-$T/tests/bindreopen1/out}
mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
CXX=${CXX:-g++}
$CXX -std=c++17 -O2 -I"$T/src" "$T/tests/bindreopen1/bindreopen1_test.cpp" -o "$O/bindreopen1_test.exe" || { echo "BUILD FAIL"; exit 2; }
$CXX -std=c++17 -O2 -I"$T/src" "$T/tests/bindlesspool/bindlesspool_test.cpp" -o "$O/bindlesspool_test.exe" || { echo "BUILD FAIL"; exit 2; }
$CXX -std=c++17 -O2 -I"$T/src" -DPOOL_RED_OFF1 "$T/tests/bindlesspool/bindlesspool_test.cpp" -o "$O/bindlesspool_red.exe" || { echo "BUILD FAIL"; exit 2; }
fails=0
run() {	# name expected-rc command...
	local n=$1 want=$2
	shift 2
	timeout 120 "$@" > "$O/$n.txt" 2>&1 < /dev/null
	local rc=$?
	echo "== $n rc=$rc (want $want): $(grep -E 'PASS|FAIL|ABORT' "$O/$n.txt" | tail -1)"
	[ "$rc" = "$want" ] || fails=$((fails + 1))
}
run pool 0 "$O/bindreopen1_test.exe" pool
run pool_red_poolgrow 1 "$O/bindreopen1_test.exe" pool poolgrow
run reopen 0 "$O/bindreopen1_test.exe" reopen
run reopen_red_reopenraw 1 "$O/bindreopen1_test.exe" reopen reopenraw
run bindlesspool 0 "$O/bindlesspool_test.exe"
run bindlesspool_red_off1 1 "$O/bindlesspool_red.exe"
if [ "$fails" = 0 ]; then echo "BINDREOPEN1 PASS"; else echo "BINDREOPEN1 FAIL ($fails)"; exit 1; fi
