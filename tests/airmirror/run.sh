#!/bin/bash
# lane PERFFIX3: the air pass's uniform mirror + wiring + grid presets. bash tests/airmirror/run.sh <scratch> [red]
# green: every row passes; red: the forget-less mirror (-DAIRMIRROR_RED) and the base sources (4c9ea88e) must FAIL
T=$(cd "$(dirname "$0")/../.." && pwd)
O=${1:?scratch dir}
mkdir -p "$O"
PY="${PY:-$(command -v python)}"
export PATH=/c/msys64/ucrt64/bin:$PATH
if [ "$2" = red ]; then
	g++ -std=c++17 -O1 -DAIRMIRROR_RED -I "$T/src" "$T/tests/airmirror/airmirror_test.cpp" -o "$O/airmirror_red.exe" || exit 2
	"$O/airmirror_red.exe"; a=$?
	"$PY" -I "$T/tests/airmirror/wiring_check.py" --base 4c9ea88e; b=$?
	echo "RED airmirror rc=$a wiring rc=$b (both must be 1)"
	[ $a = 1 ] && [ $b = 1 ]
	exit $?
fi
g++ -std=c++17 -O1 -I "$T/src" "$T/tests/airmirror/airmirror_test.cpp" -o "$O/airmirror.exe" || exit 2
"$O/airmirror.exe" || exit 1
"$PY" -I "$T/tests/airmirror/wiring_check.py"
