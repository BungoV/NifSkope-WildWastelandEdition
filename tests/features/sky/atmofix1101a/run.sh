#!/bin/bash
# Lane FIX1101A (atmo): the physical sky's MS LUT rows and the night floor on the tree's real shaders (hidden GL context,
# no NifSkope). tests/features/sky/atmofix1101a/run.sh <out dir>   (from the tree root; PY = a Windows python with numpy)
# Green must print "GATE OK"; red msold must FAIL ms-horizon-mean and ms-horizon-worst; red nofloor must FAIL
# floor-zenith and floor-anti30. Each run is capped by `timeout 300`.
set -u
OUT="${1:?usage: run.sh <out dir>}"
PY="${PY:-$(command -v python)}"
mkdir -p "$OUT"
T=tests/features/sky/atmofix1101a/atmo_check.py
bad=0
timeout 300 "$PY" -I "$T" > "$OUT/green.txt" 2>&1 < /dev/null; rc=$?
tail -9 "$OUT/green.txt"
if [ $rc -ne 0 ] || ! grep -q '^GATE OK' "$OUT/green.txt"; then echo "GREEN FAILED (rc $rc)"; bad=1; fi
for spec in "msold:ms-horizon-mean ms-horizon-worst" "nofloor:floor-zenith floor-anti30"; do
	red="${spec%%:*}"
	timeout 300 "$PY" -I "$T" --red "$red" > "$OUT/red_$red.txt" 2>&1 < /dev/null; rc=$?
	for g in ${spec#*:}; do
		if [ $rc -eq 1 ] && grep -q "^FAIL $g:" "$OUT/red_$red.txt"; then
			echo "red $red: fails $g as it must: $(grep -m1 "^FAIL $g:" "$OUT/red_$red.txt")"
		else
			echo "RED $red DID NOT FAIL $g (rc $rc)"; bad=1
		fi
	done
done
[ $bad -eq 0 ] && echo "ATMOFIX1101A GATE OK (green + 2 reds)" || echo "ATMOFIX1101A GATE FAILED"
exit $bad
