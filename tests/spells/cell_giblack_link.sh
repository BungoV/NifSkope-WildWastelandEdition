#!/bin/bash
# lane GIBLACKLINK1: the giblack red's programs link, offline, through the real driver (no NifSkope run).
#   bash tests/spells/cell_giblack_link.sh [REF=<commit before the fix, default 3c9b4397>]
# 1 green: with WW_CELL_GI_MARCH (WW_CELL_PV_RED=borrow and every march red) fo4_cell, fo4_cellcsm, pbrm_cell(csm) LINK.
# 2 red:   WW_CELL_GI_MARCH_COPIES (a march copy per call site, the old shader) must FAIL fo4_cellcsm's link.
# 3 off:   without the march, every fo4_*.prog's driver binary equals REF's but for the run stamp (bytes <= 80) and
#          one 8-byte window (the source hash; a byte of it may match by chance): the same machine code, so the same pixels. Tolerance: exactly that, nothing more.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
REF="${REF:-3c9b4397}"; T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
CHK="python -I $HERE/shader_link_check.py"
git -C "$REPO" archive "$REF" res/shaders | tar -x -C "$T"
fail=0
for p in fo4_cell fo4_cellcsm pbrm_cell pbrm_cellcsm; do
	v="$($CHK "$REPO/res/shaders" $p.prog --define WW_CELL_GI_MARCH | sed -n 2p)"; echo "march  $v"
	case "$v" in "LINK OK"*) ;; *) fail=1 ;; esac
done
v="$($CHK "$REPO/res/shaders" fo4_cellcsm.prog --define WW_CELL_GI_MARCH --define WW_CELL_GI_MARCH_COPIES | sed -n 2p)"; echo "red    $v"
case "$v" in "LINK FAIL"*) echo "red FAILS as designed" ;; *) echo "RED DID NOT FAIL"; fail=1 ;; esac
for f in $(cd "$REPO/res/shaders" && ls fo4_*.prog); do
	$CHK "$T/res/shaders" $f --save-binary "$T/ref.bin" > /dev/null
	$CHK "$REPO/res/shaders" $f --save-binary "$T/new.bin" > "$T/new.txt"
	n="$(cmp -l "$T/ref.bin" "$T/new.bin" 2>&1 | awk '$1 > 80 { print $1 }' | python -c "
import sys
o = [int(x) for x in sys.stdin.read().split() if x.isdigit()]
print('ok' if not o or o[-1] - o[0] < 8 else 'DIFF %d bytes over %d' % (len(o), o[-1] - o[0] + 1))")"
	s1=$(stat -c %s "$T/ref.bin" 2>/dev/null || echo 0); s2=$(stat -c %s "$T/new.bin" 2>/dev/null || echo 0)
	[ "$s1" = "$s2" ] && [ "$s1" != 0 ] || n="SIZE $s1 vs $s2"
	echo "off    $f $n"
	[ "$n" = ok ] || fail=1
done
[ $fail = 0 ] && echo "PASS giblack link gate" || echo "FAIL giblack link gate"
exit $fail
