#!/usr/bin/env bash
# lane SIGNDROP1 (2026-10-29): the GPU cull's occlusion twin (hizcull_test.cpp, no GL, no Qt).
# Green (the new occluder gate) must print "SIGNDROP1 PASS" and exit 0; the red (-DSIGNDROP1_RED, the gate before
# the fix = WW_GPUDRIVEN_B_RED=atocc) must print "SIGNDROP1 FAIL" naming the sign and exit 1.
# Prints "SIGNDROP1 UNIT PASS" only when both hold. OUT= picks the scratch folder (default notes/signdrop1/unit).
#   bash tests/signdrop1/run.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${OUT:-/e/Projects/Claude/notes/signdrop1/unit}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
for v in green red; do
	def=""
	[ "$v" = red ] && def="-DSIGNDROP1_RED"
	if ! "$CXX" -std=c++17 -O2 -Wall -Wextra $def -o "$OUT/$v.exe" "$HERE/hizcull_test.cpp" > "$OUT/$v.build.txt" 2>&1; then
		cat "$OUT/$v.build.txt"
		echo "SIGNDROP1 UNIT FAIL (build $v)"
		exit 2
	fi
done
timeout 60 "$OUT/green.exe" > "$OUT/green.txt" 2>&1; g=$?
timeout 60 "$OUT/red.exe" > "$OUT/red.txt" 2>&1; r=$?
echo "== green (rc $g)"; cat "$OUT/green.txt"
echo "== red (rc $r)"; cat "$OUT/red.txt"
ok=1
[ "$g" -eq 0 ] && grep -q '^SIGNDROP1 PASS gate=new' "$OUT/green.txt" || { echo "green did not pass"; ok=0; }
[ "$r" -eq 1 ] && grep -q '^SIGNDROP1 FAIL' "$OUT/red.txt" && grep -q '^FAIL sign: visible in the frame but culled' "$OUT/red.txt" \
	|| { echo "red did not fail on the sign"; ok=0; }
# the red changes only the gate: every pyramid, box and control line must read the same in both
for k in 'every level-0 pixel' 'boxes read no nearer' 'control floor-halved' 'control nearest-sample' 'control y-flipped' \
	'hidden hidden and culled' 'mirror visible and kept' 'edge visible and kept'; do
	grep -q "^ok   .*$k" "$OUT/green.txt" || { echo "green lacks ok: $k"; ok=0; }
	grep -q "^ok   .*$k" "$OUT/red.txt" || { echo "red lacks ok: $k"; ok=0; }
done
if [ "$ok" -eq 1 ]; then echo "SIGNDROP1 UNIT PASS"; exit 0; fi
echo "SIGNDROP1 UNIT FAIL"; exit 1
