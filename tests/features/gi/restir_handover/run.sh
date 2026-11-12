#!/bin/bash
# Lane AIRPERF1 round 2: the ReSTIR hand-over rule, green and its red (tests/features/gi/restir_handover).
#   bash tests/features/gi/restir_handover/run.sh <out dir>     (from the tree root; /c/msys64/ucrt64/bin first on PATH)
# Green must print "RESTIR_HANDOVER PASS 5"; the red (-DRESTIR_HANDOVER_RED, the old rule: a step always waits) must
# fail framewait and behind, and nothing else.
set -u
OUT="${1:?usage: run.sh <out dir>}"
CXX="${CXX:-g++}"
mkdir -p "$OUT"
SRC=tests/features/gi/restir_handover/restir_handover_test.cpp
bad=0
"$CXX" -O2 -std=c++17 -Wall -I src "$SRC" -o "$OUT/green.exe" -static || { echo "BUILD FAIL green"; exit 2; }
timeout 60 "$OUT/green.exe" > "$OUT/green.txt"; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^RESTIR_HANDOVER PASS 5' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
"$CXX" -O2 -std=c++17 -Wall -I src -DRESTIR_HANDOVER_RED "$SRC" -o "$OUT/red.exe" -static || { echo "BUILD FAIL red"; exit 2; }
timeout 60 "$OUT/red.exe" > "$OUT/red.txt"; rc=$?
cat "$OUT/red.txt"
fails=$(grep -c '^FAIL' "$OUT/red.txt")
if [ $rc -eq 1 ] && grep -q '^FAIL framewait:' "$OUT/red.txt" && grep -q '^FAIL behind:' "$OUT/red.txt" && [ "$fails" -eq 2 ]; then
	echo "red: fails framewait and behind as it must, and only those"
else
	echo "RED DID NOT FAIL AS NAMED (rc $rc, $fails failed)"; bad=1
fi
[ $bad -eq 0 ] && echo "RESTIR_HANDOVER GATE OK (green 5 + 1 red)" || echo "RESTIR_HANDOVER GATE FAILED"
exit $bad
