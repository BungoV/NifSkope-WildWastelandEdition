#!/bin/bash
#
# Lane SKEL2 -- the harness chain, on the built exe, one instance at a time.
#
# The two pose harnesses first, because they are the ones this lane's change
# could break: Pose Mode no longer draws its own bones. Both were RED ON THE
# RUNG with one named failure each (measured before any code was written,
# scratchpad/skel2_20260910/logs/base_posedraw.log and base_poseextras.log);
# the gate is that they are red with the SAME failure, not that they are green.
set -u
. "$(dirname "$0")/../../tests/spells/_harness.sh"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="$ROOT/release/NifSkope.exe"
SRC="$ROOT/fixtures/human_male_vanilla.nif"
OUT="$ROOT/scratchpad/skel2_20260910/logs"
mkdir -p "$OUT"

one () {   # one <env name> <log basename> <port>
	local var="$1" base="$2" port="$3"
	rm -f "$ROOT/release/$base"
	env "$var=1" "$NS" --port "$port" "$(winpath "$SRC")" >/dev/null 2>&1
	if [ -f "$ROOT/release/$base" ]; then
		cp "$ROOT/release/$base" "$OUT/after_$base"
		echo "--- $var"
		cat "$OUT/after_$base"
	else
		echo "--- $var: NO LOG"
	fi
}

echo "===== WW_POSEDRAW_TEST ====="
one WW_POSEDRAW_TEST ww_posedraw_test.log 42361
echo
echo "===== WW_POSEEXTRAS_TEST ====="
one WW_POSEEXTRAS_TEST ww_poseextras_test.log 42362
echo
echo "===== WW_SKELETON_TEST ====="
one WW_SKELETON_TEST ww_skeleton_test.log 42363

echo
echo "===== the neighbour spells ====="
for s in animws hkxanim_ui water_ui ui_align loaded_nifs top_bar files_tab; do
	f="$ROOT/tests/spells/$s.sh"
	if [ ! -f "$f" ]; then
		echo "-- $s.sh: NOT IN THE TREE"
		continue
	fi
	echo "-- $s.sh"
	timeout 600 bash "$f" > "$OUT/after_$s.txt" 2>&1
	rc=$?
	grep -E "checks,|PASS|FAIL|RESULT" "$OUT/after_$s.txt" | tail -6 | sed 's/^/     /'
	echo "     rc=$rc"
done
echo "NEIGHBOURS DONE"
