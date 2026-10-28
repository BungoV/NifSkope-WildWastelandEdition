#!/bin/bash
#
# ROOM BOXES AND THRESHOLDS AT THE REAL WALLS (lane VOLPLACE2, 2026-10-28; src/probevolumes.cpp).
# Judged by tests/spells/cell_volplace_check.py against the soup's own triangles (not the fitter's grid).
#
#   gate    the Concord hardware store window's soup (cell -15,17, rebuilt with --like-cell): no room<->room threshold
#           away from a real opening (ring of walls beside its jambs), at most 3 boxes per room
#   red     the same with --red volplace1 (the rule before the lane): must FAIL
#
# USAGE  bash tests/spells/cell_volplace.sh [gate|red]     OUT= the run folder, SOUP= the window soup.psp
#        (default: the VOLVIEW2 hold's dump; it needs <soup>.spacein beside it)

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
PHASE="${1:-gate}"
OUT="${OUT:-$REPO/scratchpad/volplace2/gate}"
SOUP="${SOUP:-/e/Projects/Claude/notes/volview2/place/live/soup.psp}"
CHECK="$REPO/tests/spells/cell_volplace_check.py"

[ -s "$SOUP" ] && [ -s "$SOUP.spacein" ] || { echo "CELL_VOLPLACE: no soup $SOUP (+ .spacein): SKIP"; exit 2; }
red=""
[ "$PHASE" = red ] && red="--red volplace1"
W="$OUT/$PHASE"
rm -rf "$W"
mkdir -p "$OUT"
"$EXE" -no-gui volumes --soup "$(winpath "$SOUP")" --like-cell "$(winpath "$SOUP.spacein")" --out "$(winpath "$W")" $red \
	> "$OUT/volumes_$PHASE.log" 2>&1 < /dev/null || { echo "CELL_VOLPLACE $PHASE: volumes rc $?"; exit 1; }
python -I "$CHECK" "$W" "$SOUP" --cells -15_17 --bar 1 > "$OUT/cell_volplace_$PHASE.log" 2>&1
rc=$?
tail -2 "$OUT/cell_volplace_$PHASE.log"
if [ "$PHASE" = red ]; then
	[ $rc = 1 ] && { echo "CELL_VOLPLACE red: FAILS as it must"; exit 0; }
	echo "CELL_VOLPLACE red: DID NOT FAIL"; exit 1
fi
[ $rc = 0 ] && echo "CELL_VOLPLACE gate: PASS" || echo "CELL_VOLPLACE gate: FAIL"
exit $rc
