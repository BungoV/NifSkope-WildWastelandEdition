#!/bin/bash
# lane TIMING1: the built-in presets' table (src/ui/cellpresettable.h). Green: presettable_test (every row x preset,
# the rules) and coverage.py (every Fusion Shaders panel row has a table row) pass. Reds that must FAIL: the test
# built with -DPRESETTABLE_RED (the old rule) and coverage.py --red (a table row dropped).
# Usage: bash tests/presettable/run.sh [outdir]   (default tests/presettable/out, untracked)
T=$(cd "$(dirname "$0")/../.." && pwd)
O=${1:-$T/tests/presettable/out}
mkdir -p "$O"
PY="${PY:-$(command -v python)}"
export PATH=/c/msys64/ucrt64/bin:$PATH
fails=0
for b in green red; do
	d=; [ $b = red ] && d=-DPRESETTABLE_RED
	g++ -std=c++17 -Wall -Wextra $d -I"$T/src/ui" "$T/tests/presettable/presettable_test.cpp" -o "$O/presettable_$b.exe" > "$O/build_$b.txt" 2>&1 \
		|| { echo "BUILD FAIL $b"; head -20 "$O/build_$b.txt"; exit 2; }
done
run() {	# name want(0 pass, 1 must fail) command...
	local n=$1 want=$2; shift 2
	"$@" > "$O/$n.txt" 2>&1 < /dev/null
	local rc=$? ok=BAD
	if [ "$want" = 0 ]; then [ $rc = 0 ] && ok=OK; else [ $rc = 1 ] && ok=OK; fi
	echo "== $n rc=$rc (want $( [ $want = 0 ] && echo 0 || echo fail )) $ok: $(grep -E 'checks|COVERAGE|panel' "$O/$n.txt" | tail -1)"
	[ $ok = OK ] || fails=$((fails + 1))
}
run green 0 "$O/presettable_green.exe"
run red_oldrule 1 "$O/presettable_red.exe"
run coverage 0 "$PY" -I "$T/tests/presettable/coverage.py" "$T"
run coverage_red 1 "$PY" -I "$T/tests/presettable/coverage.py" "$T" --red
if [ "$fails" = 0 ]; then echo "PRESETTABLE RESULT PASS"; else echo "PRESETTABLE RESULT FAIL ($fails)"; exit 1; fi
