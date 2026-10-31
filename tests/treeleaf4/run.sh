#!/bin/bash
# lane TREELEAF4 (FXCOVRESID1 + TAABLENDMOVER1), wrapped by FIX1031B: the sites (text twin of the C++ half) and the
# offline GPU gate (the real fo4_effectcell vs the real taa_objvel in a hidden GL context). green + reds (each must FAIL).
#   bash tests/treeleaf4/run.sh <out dir>
# blendmover_gate.py (the policy measurement, env RIG=<taa3_gates.py>) and a2c_gate.py (A2CCOLOUR1: REFUSE) are
# measurements, run by hand; their results are in WW_CHANGES.md.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
PY="${PY:-$(command -v python)}"
BASE="${TREELEAF4_BASE:-563dcf97}"	# the lane's base: TREELEAF3's side rule
rc=0
"$PY" -I "$HERE/sites.py" "$ROOT" > "$O/sites_green.txt" 2>&1 || rc=1
tail -n 1 "$O/sites_green.txt" | sed "s/^/sites green: /"
"$PY" -I "$HERE/sites.py" "$ROOT" --rev "$BASE" > "$O/sites_red.txt" 2>&1 && { echo "sites red ($BASE): PASSED (it must fail)"; rc=1; }
tail -n 1 "$O/sites_red.txt" | sed "s/^/sites red ($BASE): /"
"$PY" -I "$HERE/fxcov_gate.py" --out "$O/fxcov_green.txt" > "$O/fxcov_green.log" 2>&1 || rc=1
tail -n 1 "$O/fxcov_green.log" | sed "s/^/fxcov green: /"
for red in base resid; do
	"$PY" -I "$HERE/fxcov_gate.py" --red $red --base "$BASE" --out "$O/fxcov_red_$red.txt" > "$O/fxcov_red_$red.log" 2>&1 \
		&& { echo "fxcov red $red: PASSED (it must fail)"; rc=1; }
	tail -n 1 "$O/fxcov_red_$red.log" | sed "s/^/fxcov red $red: /"
done
echo "TREELEAF4 UNIT $([ $rc = 0 ] && echo PASS || echo FAIL)"
exit $rc
