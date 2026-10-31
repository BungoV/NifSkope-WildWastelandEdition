#!/bin/bash
# lane FIX1031B clouds: the rain fog's scale (Marshall-Palmer) and its tails, offline (no NifSkope). Green must exit 0,
# each red must exit 1.
#   bash tests/rainfogscale/run.sh <scratch dir>   (from the tree root; MSYS2 UCRT64 g++; PY=<python> overrides)
set -u
S=${1:-../rainfogscale_scratch}
PY="${PY:-$(command -v python)}"
mkdir -p "$S"
fail=0
"$PY" -I tests/rainfogscale/rainfogscale_check.py "$S" < /dev/null > "$S/green.log" 2>&1
rc=$?; echo "green rc=$rc (want 0)"; [ $rc -eq 0 ] || fail=1
# today = main 5422be6c (one drop size, no tails); mono = the tails alone; notail = the new law alone; wiring = the base file
for r in today mono notail wiring; do
	"$PY" -I tests/rainfogscale/rainfogscale_check.py "$S" --red $r < /dev/null > "$S/red_$r.log" 2>&1
	rc=$?; echo "red $r rc=$rc (want 1)"; [ $rc -eq 1 ] || fail=1
done
echo "RAINFOGSCALE-RUN $([ $fail -eq 0 ] && echo PASS || echo FAIL)"
exit $fail
