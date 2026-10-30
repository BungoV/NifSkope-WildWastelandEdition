#!/usr/bin/env bash
# lane GLARESB2 (2026-10-30): a live set of WW_CELL_PSYS_OBJ_RED leaves no particle of the other value behind
# (psyslive_test.cpp: twin of PSysSimController::updateTime's clock + spherical pull, the real switch store).
# Green (src/gl/psyslive.h's restart) must print "PSYSLIVE PASS" and exit 0; the red (-DPSYSLIVE_RED, today's
# updateTime: no restart) must print "PSYSLIVE FAIL" on the fresh-build check and exit 1.
# Prints "PSYSLIVE UNIT PASS" only when both hold. OUT= picks the scratch folder (default notes/code1030/glaresb2/unit).
#   bash tests/psyslive/run.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="${OUT:-/e/Projects/Claude/notes/code1030/glaresb2/unit}"
UCRT="${UCRT:-/c/msys64/ucrt64}"
export PATH="$UCRT/bin:$PATH"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
for v in green red; do
	def=""
	[ "$v" = red ] && def="-DPSYSLIVE_RED"
	if ! "$CXX" -std=c++17 -O1 -w $def -I"$ROOT/src" -I"$UCRT/include/qt6" -I"$UCRT/include/qt6/QtCore" \
		-o "$OUT/$v.exe" "$HERE/psyslive_test.cpp" "$ROOT/src/wwswitch.cpp" -lQt6Core > "$OUT/$v.build.txt" 2>&1; then
		cat "$OUT/$v.build.txt"
		echo "PSYSLIVE UNIT FAIL (build $v)"
		exit 2
	fi
done
timeout 120 "$OUT/green.exe" > "$OUT/green.txt" 2>&1; g=$?
timeout 120 "$OUT/red.exe" > "$OUT/red.txt" 2>&1; r=$?
echo "== green (rc $g)"; cat "$OUT/green.txt"
echo "== red (rc $r)"; cat "$OUT/red.txt"
ok=1
[ "$g" -eq 0 ] && grep -q '^PSYSLIVE PASS' "$OUT/green.txt" || { echo "green did not pass"; ok=0; }
[ "$r" -eq 1 ] && grep -q '^PSYSLIVE FAIL' "$OUT/red.txt" \
	&& grep -q "^FAIL after the red goes off live, the system equals a fresh build's" "$OUT/red.txt" \
	|| { echo "red did not fail on the fresh-build check"; ok=0; }
# the red changes only the restart: the stage, the red's own strands and the other-switch checks read ok in both
for k in 'stage table' 'the red draws strands' 'another switch' 'no set since the stamp'; do
	grep -q "^ok   $k\|^ok   .*$k" "$OUT/green.txt" || { echo "green lacks ok: $k"; ok=0; }
	grep -q "^ok   $k\|^ok   .*$k" "$OUT/red.txt" || { echo "red lacks ok: $k"; ok=0; }
done
if [ "$ok" -eq 1 ]; then echo "PSYSLIVE UNIT PASS"; exit 0; fi
echo "PSYSLIVE UNIT FAIL"; exit 1
