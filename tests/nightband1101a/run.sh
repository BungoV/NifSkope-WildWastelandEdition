#!/bin/bash
# Lane FIX1101A (atmo): the night horizon band (cell_volsky.frag rule 2). tests/nightband1101a/run.sh <out dir>
# (from the tree root; PY = a Windows python with numpy). Green must print "GATE OK"; the red apband (rule 1, the aerial
# perspective's colour alone) must FAIL step and nodark. Each run capped by `timeout 120`.
set -u
OUT="${1:?usage: run.sh <out dir>}"
PY="${PY:-$(command -v python)}"
mkdir -p "$OUT"
T=tests/nightband1101a/nightband_check.py
bad=0
timeout 120 "$PY" -I "$T" > "$OUT/green.txt" 2>&1 < /dev/null; rc=$?
cat "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^GATE OK' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
timeout 120 "$PY" -I "$T" --red apband > "$OUT/red.txt" 2>&1 < /dev/null; rc=$?
for g in step nodark; do
	if [ $rc -eq 1 ] && grep -q "^FAIL $g:" "$OUT/red.txt"; then
		echo "red apband: fails $g as it must: $(grep -m1 "^FAIL $g:" "$OUT/red.txt")"
	else
		echo "RED DID NOT FAIL $g (rc $rc)"; bad=1
	fi
done
[ $bad -eq 0 ] && echo "NIGHTBAND1101A GATE OK (green + red)" || echo "NIGHTBAND1101A GATE FAILED"
exit $bad
