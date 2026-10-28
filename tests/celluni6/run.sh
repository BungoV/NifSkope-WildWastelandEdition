#!/bin/sh
# tests/celluni6 (lane CELLUNI6): builds the fake-GL test green and once per item red, runs each.
# Green must pass; each red build must fail, and only on its own item's lines.
# Usage: tests/celluni6/run.sh [g++]   (from the repository root; writes to $TMPDIR or /tmp)   exit 0 = all ok
CXX=${1:-g++}
OUT=${TMPDIR:-/tmp}/celluni6.$$
mkdir -p "$OUT" || exit 2
SRC=tests/celluni6/celluni6.cpp
FLAGS="-std=c++17 -Wall -Wextra -Werror -O1 -Isrc"
ITEMS="jitter nonlinear hdrdepth hdrbegin bsshape"
bad=0
$CXX $FLAGS "$SRC" -o "$OUT/green" || exit 2
if "$OUT/green" > "$OUT/green.txt"; then echo "green: ok"; else echo "green: FAIL"; cat "$OUT/green.txt"; bad=1; fi
for item in $ITEMS; do
	macro=CELLUNI6_RED_$(echo "$item" | tr a-z A-Z)
	$CXX $FLAGS -D"$macro" "$SRC" -o "$OUT/red_$item" || exit 2
	"$OUT/red_$item" > "$OUT/red_$item.txt"
	code=$?
	others=$(grep '^FAIL' "$OUT/red_$item.txt" | grep -v "^FAIL $item:")
	if [ $code -ne 0 ] && [ -z "$others" ]; then
		echo "red $item: fails as it must ($(grep -c '^FAIL' "$OUT/red_$item.txt") line(s), all its own)"
	else
		echo "red $item: WRONG (exit $code)"; grep '^FAIL' "$OUT/red_$item.txt"; bad=1
	fi
done
rm -rf "$OUT"
[ $bad -eq 0 ] && echo "CELLUNI6 RUN OK" || echo "CELLUNI6 RUN FAIL"
exit $bad
