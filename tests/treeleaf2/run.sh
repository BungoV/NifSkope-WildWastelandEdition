#!/bin/bash
# lane TREELEAF2: the side passes alpha-test the colour pass's texel set (src/gl/wwleafcov.h). green + reds (each must FAIL).
#   bash tests/treeleaf2/run.sh <out dir> [<folder with manifest.txt + .tlf dumps of game textures>]
# The dumps: notes/fix1030b/treeleaf2/tools/dump_few.py (8 leaf textures, game + BNS); without them only the rule runs.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
DATA="${2:-}"
PY="${PY:-$(command -v python)}"
BASE="${TREELEAF2_BASE:-d05945bb}"	# today's side passes (SHADOWSHIMMER2's head)
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED_PLAIN RED_NOSTRICT; do
	d=""; [ $v = green ] || d="-DTREELEAF2_$v"
	g++ -std=c++17 -O2 $d -I"$ROOT/src" "$HERE/treeleaf2_test.cpp" -o "$O/treeleaf2_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	timeout 600 "$O/treeleaf2_$v.exe" $DATA > "$O/treeleaf2_$v.log" 2>&1; r=$?
	grep -E '^game textures:' "$O/treeleaf2_$v.log" | sed "s/^/$v: /"
	tail -n 1 "$O/treeleaf2_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || rc=1
	else
		[ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
	fi
done
"$PY" -I "$HERE/sites.py" "$ROOT" > "$O/sites_green.log" 2>&1; r=$?
sed 's/^/sites green: /' "$O/sites_green.log"
[ $r = 0 ] || rc=1
"$PY" -I "$HERE/sites.py" "$ROOT" --rev "$BASE" > "$O/sites_red.log" 2>&1; r=$?
grep -v '^FAIL src' "$O/sites_red.log" | sed "s/^/sites red ($BASE): /"
[ $r != 0 ] || { echo "sites red: PASSED (it must fail)"; rc=1; }
echo "TREELEAF2 UNIT $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
