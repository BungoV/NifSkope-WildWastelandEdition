#!/bin/bash
# Two runs that had to wait for the gate to release the exe, in order, with the
# game rule checked before EACH: (1) lodgen_native.sh re-run after its j0
# version list learned 8, (2) the --horizon-near-skip 8 point of the sweep,
# re-measured on the exe that ships because the number in the docs was measured
# on a superseded build.
set -u
ROOT=E:/Projects/NifskopeWildWastelandEdition
LANE=$ROOT/scratchpad/horizon1_20260918
guard () {
	if tasklist 2>/dev/null | grep -qi -E "Fallout4|NifSkope"; then
		echo "GAME OR NIFSKOPE UP -- stopping before $1"; exit 90
	fi
}
guard lodgen_native.sh
bash "$ROOT/tests/spells/lodgen_native.sh" > "$LANE/neigh_native_after.log" 2>&1
echo "lodgen_native.sh rc=$? -- $(tail -1 "$LANE/neigh_native_after.log")"
guard nearskip8
bash "$LANE/bake.sh" "$ROOT/release/NifSkope.exe" "$LANE/ns8" --horizon-near-skip 8
grep -o "horizonNearSkip [0-9.]* horizonMeanElev [0-9.]*\|horizonMeanElev [0-9.]*" "$LANE/ns8/bake.log" | head -2
