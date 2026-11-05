#!/usr/bin/env bash
# lane FIX1101A lodch (2026-11-01): the far field's sky term (src/features/far-lod/lodchsky.h, the baked per-vertex sky byte read as the
# GI's share) against the near field's GI at the last cascade edge, on uniform facades (lodchsky_test.cpp).
# Green must print "LODCHSKY PASS" and exit 0; both reds must print "LODCHSKY FAIL" and exit 1:
#   -DLODCH_RED=1  today's far field: no sky factor at all
#   -DLODCH_RED=2  the raw byte as the factor (an open wall reads 0.56 of the sky)
# Prints "LODCHSKY UNIT PASS" only when all three hold. OUT= picks the scratch folder.
#   bash tests/features/far-lod/lodchsky/run.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../../.." && pwd)"
OUT="${OUT:-/e/Projects/Claude/notes/fix1101a/lodch/unit_sky}"
UCRT="${UCRT:-/c/msys64/ucrt64}"
export PATH="$UCRT/bin:$PATH"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
for v in green red1 red2; do
	def=""
	[ "$v" = red1 ] && def="-DLODCH_RED=1"
	[ "$v" = red2 ] && def="-DLODCH_RED=2"
	if ! "$CXX" -std=c++17 -O2 -Wall $def -I"$ROOT/src" -o "$OUT/$v.exe" "$HERE/lodchsky_test.cpp" > "$OUT/$v.build.txt" 2>&1; then
		cat "$OUT/$v.build.txt"
		echo "LODCHSKY UNIT FAIL (build $v)"
		exit 2
	fi
done
timeout 120 "$OUT/green.exe" > "$OUT/green.txt" 2>&1; g=$?
timeout 120 "$OUT/red1.exe" > "$OUT/red1.txt" 2>&1; r1=$?
timeout 120 "$OUT/red2.exe" > "$OUT/red2.txt" 2>&1; r2=$?
echo "== green (rc $g)"; cat "$OUT/green.txt"
echo "== red1 today (rc $r1)"; tail -1 "$OUT/red1.txt"
echo "== red2 raw byte (rc $r2)"; tail -1 "$OUT/red2.txt"
ok=1
[ "$g" -eq 0 ] && grep -q '^LODCHSKY PASS' "$OUT/green.txt" || { echo "green did not pass"; ok=0; }
[ "$r1" -eq 1 ] && grep -q '^LODCHSKY FAIL' "$OUT/red1.txt" || { echo "red1 did not fail"; ok=0; }
[ "$r2" -eq 1 ] && grep -q '^LODCHSKY FAIL' "$OUT/red2.txt" || { echo "red2 did not fail"; ok=0; }
if [ "$ok" -eq 1 ]; then echo "LODCHSKY UNIT PASS"; exit 0; fi
echo "LODCHSKY UNIT FAIL"
exit 1
