#!/bin/bash
# lane TREELEAF3: an alpha-tested effect shape's TAA object-vector test keeps its colour pass's texels (src/gl/wwfxcov.h);
# the alpha-to-coverage refusal's arithmetic (a2c_refusal.cpp). green + reds (each red must FAIL).
#   bash tests/treeleaf3/run.sh <out dir>
# The census (tests/treeleaf3/fx_census.py, ~75 s over the game's BA2s) is not run here: it is a census, not a gate.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
PY="${PY:-$(command -v python)}"
BASE="${TREELEAF3_BASE:-6fa08fed}"	# the lane's base (today's TAA test)
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED_RAW RED_NOSTRICT; do
	d=""; [ $v = green ] || d="-DTREELEAF3_$v"
	g++ -std=c++17 -O2 $d -I"$ROOT/src" "$HERE/treeleaf3_test.cpp" -o "$O/treeleaf3_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	timeout 120 "$O/treeleaf3_$v.exe" > "$O/treeleaf3_$v.log" 2>&1; r=$?
	tail -n 2 "$O/treeleaf3_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || rc=1
	else
		[ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
	fi
done
"$PY" -I "$HERE/sites.py" "$ROOT" > "$O/sites_green.log" 2>&1; r=$?
sed 's/^/sites green: /' "$O/sites_green.log"
[ $r = 0 ] || rc=1
"$PY" -I "$HERE/sites.py" "$ROOT" --rev "$BASE" > "$O/sites_red.log" 2>&1; r=$?
sed "s/^/sites red ($BASE): /" "$O/sites_red.log"
[ $r != 0 ] || { echo "sites red: PASSED (it must fail)"; rc=1; }
for v in green RED; do
	d=""; [ $v = green ] || d="-DTREELEAF3_A2C_RED"
	g++ -std=c++17 -O2 $d -I"$ROOT/src" "$HERE/a2c_refusal.cpp" -o "$O/a2c_$v.exe" || { echo "BUILD FAIL a2c $v"; rc=1; continue; }
	timeout 120 "$O/a2c_$v.exe" > "$O/a2c_$v.log" 2>&1; r=$?
	if [ $v = green ]; then sed 's/^/a2c: /' "$O/a2c_$v.log"; [ $r = 0 ] || rc=1
	else
		tail -n 1 "$O/a2c_$v.log" | sed 's/^/a2c red (hard shadow): /'
		[ $r != 0 ] || { echo "a2c red: PASSED (it must fail)"; rc=1; }
	fi
done
echo "TREELEAF3 UNIT $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
