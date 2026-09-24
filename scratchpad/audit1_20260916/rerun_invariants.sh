#!/bin/bash
# AUDIT1 step 6: step 3's invariants, re-run after the build.
#
# The point is not to decode again for its own sake: it is that every number
# section 3 recorded must still read the same on output the FIXED exe wrote,
# and that the refuters must still go red. bake/sanctuary_after is region (a)
# baked by the fixed exe; the other trees are the ones section 3 measured.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
S="$ROOT/scratchpad/audit1_20260916"
PY="/c/Users/bungo/AppData/Local/Programs/Python/Python39/python"
cd "$ROOT" || exit 1

echo "=== decode_all over the fixed exe's own bake, and the three fixtures"
bash "$S/decode_all.sh" "$S/bake/sanctuary_after" "$S/bake/sanctuary_fo4cs" \
	"$S/bake/coast_fo4cs" "$S/bake/urban_fo4cs" "$S/bake/aggreal" 2>&1 | \
	grep -Ei 'FAIL|violation|checks|rc=' | tail -40

echo
echo "=== the .lodj sweep"
"$PY" "$S/lodj_sweep.py" "$S/bake/sanctuary_after" "$S/bake/sanctuary_fo4cs" \
	"$S/bake/coast_fo4cs" "$S/bake/urban_fo4cs" 2>&1 | tail -8

echo
echo "=== the .lodm card reader"
"$PY" "$ROOT/tests/spells/lodgen_lodm_check.py" "$S/bake/aggreal" 2>&1 | tail -4
"$PY" "$ROOT/tests/spells/lodgen_lodm_check.py" --refute "$S/bake/aggreal" 2>&1 | tail -3

echo
echo "=== the refuters"
O="$(find "$S/bake/meshrep" -name '*.lodo' | head -1)"
I="$(find "$S/bake/meshrep" -name '*.lodi' | head -1)"
bash "$S/refute_native.sh" "$O" "$I" 2>&1 | tail -8
VTC="$S/bake/everything/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt"
LODB="$S/bake/sanctuary_after/FO4CSLOD/Commonwealth/Commonwealth.lodb"
bash "$S/refute_rest.sh" "$VTC" "$LODB" "$S/bake/sanctuary_after" "$S/bake/sanctuary_after.log" 2>&1 | tail -6
