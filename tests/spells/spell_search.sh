#!/bin/bash
#
# The command palette — does it find things, and is it safe to type into?
#
# WHY A DIALOG AND NOT A FIELD IN THE MENU
#
# A popup QMenu holds a keyboard grab, so a hosted line edit only receives the
# keys the menu chooses to forward; QMenu::keyPressEvent otherwise spends every
# printable key on first-letter jump; click-to-focus through a QWidgetAction is
# the flakiest interaction in Qt; a filtered menu cannot expand a submenu
# inline, so hits have to be hoisted onto proxy actions; and the popup is
# anchored where it was exec'd, so it can neither grow nor re-centre.
#
# The dialog is boring code, and it is the only version that can show what is
# NOT available — which is the point below.
#
# WHAT IS MEASURED
#
# The palette walks a live SpellBook's QAction tree, so none of this can be
# checked by reading the spell registry, and the two rules that matter most are
# invisible in a diff:
#
#   1. a plain query finds its entry
#   2. the top hit is pre-selected, so Enter runs it
#   3. Enter really returns that action
#   4. the GROUP is searchable — "transform copy" narrows to one row, which is
#      what tells the four spells named "Copy" apart
#   5. a whole-file spell is still FINDABLE on a block row...
#   6. ...shown greyed as not applicable, rather than silently missing. Batch,
#      Sanitize, Optimize and Error Checking vanish from a block menu by design,
#      and "Update All Tangent Spaces — Batch — not applicable here" answers the
#      question a "no results" row cannot
#   7. ...and Enter cannot run it anyway
#   8. a destructive entry is found
#   9. ...but is NEVER auto-highlighted. Crop To Branch deletes every block
#      outside the clicked branch; two keystrokes and a reflexive Return must
#      not reach it. This is the one rule that makes typing into it safe.
#
# Checks 6 and 9 are the ones a "filter the menu instead" implementation fails
# outright: it cannot show an entry it has filtered away, and it has no notion
# of refusing to highlight one.
#
# The harness drives the palette from inside its own modal loop — the driver is
# scheduled before wwSpellPalette is called, so it fires once exec() is running.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/spell_search.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/Effects/ElectricalExplosionSmall.nif}"
PORT="${PORT:-45895}"
LOG="$ROOT/release/ww_spellsearch_test.log"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

rm -f "$LOG"
WW_SPELLSEARCH_TEST=1 "$EXE" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 60); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

if [ ! -f "$LOG" ]; then
	echo "FAIL: harness produced no log (did NifSkope start?)"
	exit 1
fi

cat "$LOG"
grep -q '^PASS$' "$LOG" && exit 0
exit 1
