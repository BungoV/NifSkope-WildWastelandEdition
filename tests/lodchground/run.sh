#!/usr/bin/env bash
# lane FIX1101A lodch r2 (2026-11-01, LATE ITEM 25): the far objects' ground contact (src/features/far-lod/lodchsky.h contactAo).
# Green must print "LODCHGROUND PASS" and exit 0; both reds must print "LODCHGROUND FAIL" and exit 1:
#   -DLODCH_GROUND_RED=1  today: the .lodi v12 ground ramp is not drawn
#   -DLODCH_GROUND_RED=2  the ramp as a product on the AO (darkens a foot the AO already darkened, twice)
# Prints "LODCHGROUND UNIT PASS" only when all three hold. OUT= picks the scratch folder.
#   bash tests/lodchground/run.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="${OUT:-/e/Projects/Claude/notes/fix1101a/lodch/unit_ground}"
UCRT="${UCRT:-/c/msys64/ucrt64}"
export PATH="$UCRT/bin:$PATH"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
for v in green red1 red2; do
	def=""
	[ "$v" = red1 ] && def="-DLODCH_GROUND_RED=1"
	[ "$v" = red2 ] && def="-DLODCH_GROUND_RED=2"
	if ! "$CXX" -std=c++17 -O2 -Wall $def -I"$ROOT/src" -o "$OUT/$v.exe" "$HERE/ground_test.cpp" > "$OUT/$v.build.txt" 2>&1; then
		cat "$OUT/$v.build.txt"
		echo "LODCHGROUND UNIT FAIL (build $v)"
		exit 2
	fi
done
timeout 120 "$OUT/green.exe" > "$OUT/green.txt" 2>&1; g=$?
timeout 120 "$OUT/red1.exe" > "$OUT/red1.txt" 2>&1; r1=$?
timeout 120 "$OUT/red2.exe" > "$OUT/red2.txt" 2>&1; r2=$?
echo "== green (rc $g)"; cat "$OUT/green.txt"
echo "== red1 today (rc $r1)"; grep -E 'FAIL' "$OUT/red1.txt"
echo "== red2 product (rc $r2)"; grep -E 'FAIL' "$OUT/red2.txt"
ok=1
[ "$g" -eq 0 ] && grep -q '^LODCHGROUND PASS' "$OUT/green.txt" || { echo "green did not pass"; ok=0; }
[ "$r1" -eq 1 ] && grep -q '^LODCHGROUND FAIL' "$OUT/red1.txt" || { echo "red1 did not fail"; ok=0; }
[ "$r2" -eq 1 ] && grep -q '^LODCHGROUND FAIL' "$OUT/red2.txt" || { echo "red2 did not fail"; ok=0; }
if [ "$ok" -eq 1 ]; then echo "LODCHGROUND UNIT PASS"; exit 0; fi
echo "LODCHGROUND UNIT FAIL"
exit 1
