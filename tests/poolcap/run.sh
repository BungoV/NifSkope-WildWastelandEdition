#!/bin/bash
# lane STREAMTEX5 unit: the bindless geometry pool's budget (g++ only, no GL, no Qt).
# Usage: bash tests/poolcap/run.sh [outdir]     PASS = every green passes and the red fails.
#   concord      density 1, budget 1536: under the budget, nothing refused (the cap does not bite)
#   downtown     density 3, budget 1536: the pool stops at the budget, the rest draws unpooled
#   downtown_red density 3, budget 0 (WW_BINDLESS_RED=poolnocap): the pool passes 1536 MB -> CAP FAIL
T=$(cd "$(dirname "$0")/../.." && pwd)
O=${1:-$T/tests/poolcap/out}
mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
CXX=${CXX:-g++}
$CXX -std=c++17 -O2 -I"$T/src" "$T/tests/poolcap/poolcap_test.cpp" -o "$O/poolcap_test.exe" || { echo "BUILD FAIL"; exit 2; }
fails=0
run() {	# name expected-rc args...
	local n=$1 want=$2
	shift 2
	timeout 300 "$O/poolcap_test.exe" "$@" > "$O/$n.txt" 2>&1 < /dev/null
	local rc=$?
	echo "== $n rc=$rc (want $want): $(head -1 "$O/$n.txt")"
	[ "$rc" = "$want" ] || fails=$((fails + 1))
}
run concord 0 1 1536
grep -q 'refused 0 of' "$O/concord.txt" || { echo "   concord: the cap refused a placement under the budget"; fails=$((fails + 1)); }
run downtown 0 3 1536
grep -q 'refused 0 of' "$O/downtown.txt" && { echo "   downtown: the walk never reached the budget (no test)"; fails=$((fails + 1)); }
run downtown_red 1 3 0
if [ "$fails" = 0 ]; then echo "POOLCAP PASS"; else echo "POOLCAP FAIL ($fails)"; exit 1; fi
