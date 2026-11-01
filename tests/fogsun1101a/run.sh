#!/bin/bash
# Lane FIX1101A (atmo): the fog-sun rule (only the derived ground layer dims the sun). tests/fogsun1101a/run.sh <out dir>
# (from the tree root; PY = a Windows python with numpy). Green must print "GATE OK"; the red suncolumn (the whole column
# to the scene top) must FAIL shaft and ground-sun. Each run capped by `timeout 120`.
set -u
OUT="${1:?usage: run.sh <out dir>}"
PY="${PY:-$(command -v python)}"
mkdir -p "$OUT"
T=tests/fogsun1101a/fogsun_check.py
bad=0
timeout 120 "$PY" -I "$T" > "$OUT/green.txt" 2>&1 < /dev/null; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^GATE OK' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
timeout 120 "$PY" -I "$T" --red suncolumn > "$OUT/red.txt" 2>&1 < /dev/null; rc=$?
for g in shaft ground-sun; do
	if [ $rc -eq 1 ] && grep -q "^FAIL $g:" "$OUT/red.txt"; then
		echo "red suncolumn: fails $g as it must: $(grep -m1 "^FAIL $g:" "$OUT/red.txt")"
	else
		echo "RED DID NOT FAIL $g (rc $rc)"; bad=1
	fi
done
[ $bad -eq 0 ] && echo "FOGSUN1101A GATE OK (green + red)" || echo "FOGSUN1101A GATE FAILED"
exit $bad
