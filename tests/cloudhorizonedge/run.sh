#!/bin/bash
# Lane BUGS1102H: no straight hard edge across the sky where the 2D cloud deck meets the horizon band (bungo 11-02).
#   tests/cloudhorizonedge/run.sh <out dir> [<green png> <red png>]     (from the tree root)
# Without pictures: the synthetic self-test (a hard cut must FAIL, a hazed fade and a plain skyline must PASS).
# With a hold's pair (green = as shipped, red = ^WW_LOOKDEV_RED=cloudnoap at the same camera): green PASS, red FAIL.
set -u
OUT="${1:?usage: run.sh <out dir> [<green png> <red png>]}"
PY="${PY:-$(command -v python)}"
mkdir -p "$OUT"
if [ $# -ge 3 ]; then
	timeout 120 "$PY" -I tests/cloudhorizonedge/edge_check.py --pair "$2" "$3" < /dev/null > "$OUT/pair.txt"; rc=$?
	cat "$OUT/pair.txt"
else
	timeout 120 "$PY" -I tests/cloudhorizonedge/edge_check.py --selftest < /dev/null > "$OUT/selftest.txt"; rc=$?
	cat "$OUT/selftest.txt"
fi
[ $rc -eq 0 ] && echo "CLOUDHORIZONEDGE GATE OK" || echo "CLOUDHORIZONEDGE GATE FAILED (rc $rc)"
exit $rc
