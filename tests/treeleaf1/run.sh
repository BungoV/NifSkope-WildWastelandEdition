#!/bin/bash
# lane TREELEAF1: coverage-preserving alpha mips (src/gl/wwalphacov.h). green + one red per rule (each red must FAIL).
#   bash tests/treeleaf1/run.sh <out dir> [<folder with manifest.txt + .tlf dumps of game textures>]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
DATA="${2:-}"
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED_NOSCALE RED_ENDPOINTS RED_NEAREST; do
	d=""; [ $v = green ] || d="-DTREELEAF1_$v"
	g++ -std=c++17 -O2 $d -I"$ROOT/src" "$HERE/treeleaf1_test.cpp" -o "$O/treeleaf1_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	timeout 600 "$O/treeleaf1_$v.exe" $DATA > "$O/treeleaf1_$v.log" 2>&1; r=$?
	grep -E '^(synthetic|game textures):' "$O/treeleaf1_$v.log" | sed "s/^/$v: /"
	tail -n 1 "$O/treeleaf1_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || rc=1
	else
		[ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
		grep '^FAIL ' "$O/treeleaf1_$v.log" | cut -d: -f1 | sort | uniq -c | sed "s/^/$v:   /"
	fi
done
echo "TREELEAF1 UNIT $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
