#!/bin/bash
# lane SHADOWSHIMMER2: FARSWAYSHADOW / TERRPENSTABLE / CSMOFFBIAS unit (gl/sunshadowfit.h + the shipped sources).
# green + one red per rule (each red must FAIL).   bash tests/shadowshimmer2/run.sh <out dir>
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
PY="${PY:-$(command -v python)}"	# the Windows python with numpy, taken before ucrt64 goes first on PATH
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
# CSM1GATEFIT: tests/spells/pbr_csm1_gates.py's fit vs the code's (green), and its red (the code's old fit must fail)
g++ -std=c++17 -O1 -Wall -I"$ROOT/src" "$HERE/fitecho.cpp" -o "$O/fitecho.exe" || { echo "BUILD FAIL fitecho"; rc=1; }
timeout 120 "$O/fitecho.exe" > "$O/fitecho.txt" 2>&1 || rc=1
for v in green red; do
	a=""; [ $v = red ] && a="--red"
	timeout 300 "$PY" -I "$HERE/csm1fit_selftest.py" "$ROOT" "$O/fitecho.txt" $a > "$O/csm1fit_$v.log" 2>&1 < /dev/null; r=$?
	tail -n 1 "$O/csm1fit_$v.log" | sed "s/^/csm1fit $v: /"
	if [ $v = green ]; then [ $r = 0 ] || rc=1
	else
		[ $r != 0 ] || { echo "csm1fit red: PASSED (it must fail)"; rc=1; }
		grep '^FAIL ' "$O/csm1fit_$v.log" | sed "s/^/csm1fit red:   /"
	fi
done
for v in green RED_TERR RED_OFFBIAS RED_SWAY; do
	d=""; [ $v = green ] || d="-DSS2_$v"
	g++ -std=c++17 -O1 -Wall $d -I"$ROOT/src" "$HERE/shadowshimmer2_test.cpp" -o "$O/ss2_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	timeout 120 "$O/ss2_$v.exe" "$ROOT" > "$O/ss2_$v.log" 2>&1; r=$?
	tail -n 1 "$O/ss2_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || rc=1
	else
		[ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
		grep '^FAIL ' "$O/ss2_$v.log" | sed "s/^/$v:   /"
	fi
done
echo "SHADOWSHIMMER2 UNIT RUN $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
