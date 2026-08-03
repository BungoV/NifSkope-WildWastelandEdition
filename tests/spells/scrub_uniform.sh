#!/bin/bash
#
# Do all number fields behave like the Move field?
#
# bungo asked for the operator panel's field behaviour -- hover arrows, press
# and drag the number -- on every type-in field in NifSkope. The audit found
# FIVE implementations of that gesture, none reachable from the others because
# the original sits in an anonymous namespace inside nifskope_ui.cpp.
#
# WRITTEN BEFORE THE FIX. Every check here was run against the unfixed binary
# and its failure recorded, so the harness is known to be able to fail. A
# consistency check authored after the change cannot tell you that.
#
# Check D is the one that guards the disease rather than a symptom: it counts
# the files carrying the scrub cursor. A textual probe would not work -- the
# collision copy renames dx to delta, so grepping the threshold expression
# finds four of five -- but a copy cannot omit the cursor and still be the same
# widget.
#
# USAGE
#   bash tests/spells/scrub_uniform.sh

set -u

# opens a real window; keep it off the primary monitor. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
LOG="$ROOT/release/ww_scrub_test.log"
PORT="${PORT:-42317}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }

fail=0

# --- D: the structural anti-drift guard (static, no launch) ----------------
echo "--- D: how many copies of the scrub gesture exist? ---"
COPIES="$(grep -rl "Qt::SizeHorCursor" "$ROOT/src" | wc -l | tr -d ' ')"
grep -rl "Qt::SizeHorCursor" "$ROOT/src" | sed 's|.*/src/|  src/|'
if [ "$COPIES" = "1" ]; then
	echo "  ok   exactly one implementation carries the scrub cursor"
else
	echo "  FAIL $COPIES files carry the scrub cursor; there must be exactly 1"
	fail=1
fi

# --- A/B/C: driven in-process ----------------------------------------------
rm -f "$LOG"
WW_SCRUB_TEST=1 "$NS" --port "$PORT" > /dev/null 2>&1
if [ ! -f "$LOG" ]; then
	echo "no log at $LOG (NifSkope exited before the harness ran)"
	exit 2
fi
cat "$LOG"
grep -q '^PASS' "$LOG" || fail=1

echo
[ "$fail" -eq 0 ] && echo "PASS" || echo "FAIL"
exit "$fail"
