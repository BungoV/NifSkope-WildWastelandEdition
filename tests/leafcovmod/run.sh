#!/bin/bash
# lane FIX1031B (TREELEAF1's fix): a shape whose colour test multiplies the texture alpha by a vertex / material alpha
# under 1 binds the plain file (the Concord church's window tracery). green + the red (main 5422be6c's colourRef, which
# must FAIL).
#   bash tests/leafcovmod/run.sh <out dir> [<folder with manifest.txt + .tlf dumps of game textures>]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
DATA="${2:-}"
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED; do
	d=""; [ $v = green ] || d="-DLEAFCOVMOD_$v"
	g++ -std=c++17 -O2 $d -I"$ROOT/src" "$HERE/leafcovmod_test.cpp" -o "$O/leafcovmod_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	timeout 600 "$O/leafcovmod_$v.exe" $DATA > "$O/leafcovmod_$v.log" 2>&1; r=$?
	grep -E '^(synthetic|file) ' "$O/leafcovmod_$v.log" | sed "s/^/$v: /"
	tail -n 1 "$O/leafcovmod_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || rc=1
	else
		[ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
		grep '^FAIL ' "$O/leafcovmod_$v.log" | cut -d: -f1 | sort | uniq -c | sed "s/^/$v:   /"
	fi
done
echo "LEAFCOVMOD UNIT $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
