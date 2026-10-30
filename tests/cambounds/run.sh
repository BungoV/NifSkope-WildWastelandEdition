#!/bin/bash
# lane CAMGATE1: the camera bounds rule's unit test. green + one red per rule (each red must FAIL).
# lane CAMGATE2: + RED_FREEZE (the state is not put back) and RED_INTERIOR (interiors not judged); with a second
# argument, every <dir>/fix_*.txt (a real interior's placements, interior_census.py --dump) runs --room on the green.
# lane CAMGATE3: + RED_LOOSEBOX (CAMGATE2's one box of the origins); --room also takes the box-set fixtures room_*.txt
# (notes/fix1030b/camgate3/tools/mk_fixtures.py): tests/cambounds/fixtures always, a second argument's dir too; each
# box-set fixture must PASS on the green and FAIL on RED_LOOSEBOX when it holds a point only the set gets right.
#   bash tests/cambounds/run.sh <out dir> [<interior fixtures dir>]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
FIX="${2:-}"
export PATH=/c/msys64/ucrt64/bin:$PATH
rc=0
for v in green RED_UNDER RED_OUTSIDE RED_FREEZE RED_INTERIOR RED_LOOSEBOX; do
	d=""; [ $v = green ] || d="-DCAMBOUNDS_$v"
	g++ -std=c++17 -O1 $d -I"$ROOT/src" "$HERE/cambounds_test.cpp" -o "$O/cambounds_$v.exe" || { echo "BUILD FAIL $v"; rc=1; continue; }
	"$O/cambounds_$v.exe" > "$O/cambounds_$v.log" 2>&1; r=$?
	tail -n 1 "$O/cambounds_$v.log" | sed "s/^/$v: /"
	if [ $v = green ]; then [ $r = 0 ] || rc=1
	else
		[ $r != 0 ] || { echo "$v: the red PASSED (it must fail)"; rc=1; }
		grep '^FAIL ' "$O/cambounds_$v.log" | sed "s/^/$v:   /"
	fi
done
for f in "$HERE"/fixtures/room_*.txt ${FIX:+"$FIX"/fix_*.txt "$FIX"/room_*.txt}; do
	[ -e "$f" ] || continue
	b=$(basename "$f" .txt)
	"$O/cambounds_green.exe" --room "$f" > "$O/room_$b.log" 2>&1; r=$?
	echo "room $b: $(head -n 1 "$O/room_$b.log" | sed 's/^room [^:]*: //') -> $(tail -n 1 "$O/room_$b.log")"
	[ $r = 0 ] || { rc=1; grep '^FAIL ' "$O/room_$b.log" | sed "s/^/  /"; }
	case "$b" in room_*)
		"$O/cambounds_RED_LOOSEBOX.exe" --room "$f" > "$O/roomred_$b.log" 2>&1; r=$?
		echo "  RED_LOOSEBOX $b: $(tail -n 1 "$O/roomred_$b.log") ($(grep -c '^FAIL ' "$O/roomred_$b.log") checks fail: $(grep '^FAIL ' "$O/roomred_$b.log" | awk '{print $2}' | sort -u | paste -sd' ' -))";;
	esac
done
echo "CAMBOUNDS UNIT $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
