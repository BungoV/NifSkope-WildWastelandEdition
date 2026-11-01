#!/usr/bin/env bash
# lane FIX1101A lodch r2 (2026-11-01, LATE ITEM 23): the vanilla far path's budget on SHOTS1101's c3 vanilla rows
# (vanbudget_test.cpp). Green must print "VANBUDGET PASS" (rc 0); the red (-DVANBUDGET_RED: every triangle a document
# row, the code before r2) must print "VANBUDGET FAIL" (rc 1) with no chunk of ring 1 built.
# Prints "VANBUDGET UNIT PASS" only when both hold. OUT= picks the scratch folder.
#   bash tests/vanbudget/run.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="${OUT:-/e/Projects/Claude/notes/fix1101a/lodch/unit_vanbudget}"
UCRT="${UCRT:-/c/msys64/ucrt64}"
export PATH="$UCRT/bin:$PATH"
CXX="${CXX:-g++}"
QT="$UCRT/include/qt6"
mkdir -p "$OUT"
for v in green red; do
	def=""
	[ "$v" = red ] && def="-DVANBUDGET_RED"
	if ! "$CXX" -std=c++17 -O2 -Wall $def -I"$ROOT/src" -I"$QT" -I"$QT/QtCore" -o "$OUT/$v.exe" "$HERE/vanbudget_test.cpp" \
		-lQt6Core > "$OUT/$v.build.txt" 2>&1; then
		cat "$OUT/$v.build.txt"
		echo "VANBUDGET UNIT FAIL (build $v)"
		exit 2
	fi
done
timeout 120 "$OUT/green.exe" > "$OUT/green.txt" 2>&1; g=$?
timeout 120 "$OUT/red.exe" > "$OUT/red.txt" 2>&1; r=$?
echo "== green (rc $g)"; cat "$OUT/green.txt"
echo "== red: every triangle a row (rc $r)"; cat "$OUT/red.txt"
ok=1
[ "$g" -eq 0 ] && grep -q '^VANBUDGET PASS' "$OUT/green.txt" || { echo "green did not pass"; ok=0; }
[ "$r" -eq 1 ] && grep -q 'no chunk of ring 1' "$OUT/red.txt" || { echo "red did not stop before ring 1"; ok=0; }
if [ "$ok" -eq 1 ]; then echo "VANBUDGET UNIT PASS"; exit 0; fi
echo "VANBUDGET UNIT FAIL"
exit 1
