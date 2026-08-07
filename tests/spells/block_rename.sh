#!/bin/bash
#
# Block-list inline rename: F2, double-click, Escape, and what the name drags
# behind it.
#
# WHY THIS EXISTS
#
# The rename shipped in d5765c4 with no harness, and with nothing measuring it
# the backlog filed it as not started -- its own entry said "this may already be
# most of the way there, try it before building anything". A feature nothing
# tests is a feature nobody can tell is there.
#
# The half that matters is not the editor. A scene object's name is not a label:
# NiDefaultAVObjectPalette entries and NiControllerSequence controlled blocks
# address nodes BY STRING. Rename the node alone and the animation stops binding
# to it, silently, with a file that still loads and still looks right. So the
# fixture builds a palette entry pointing at the node, and a rename that forgets
# to carry it fails here rather than in somebody's game.
#
# WHAT IS MEASURED
#
#   1. the fixture's root has a name to change                  <- not vacuous
#   2. the fixture's palette entry names it                     <- not vacuous
#   3. an F2 shortcut is installed on the block list
#   4. the app started in the mode under test                   <- not vacuous
#   5. firing F2 opens an inline editor on the row
#   6. ...over the name cell, not at the origin
#   7. ...holding the current name, selected
#   8. Escape closes the editor
#   9. ...and changes nothing
#  10. double-clicking the TYPE column does not start a rename
#  11. double-clicking the NAME column does
#  12. ...and no second editor opens on top of it
#  13. ...because the block list has no edit triggers
#  14. the block list's delegate draws no instant-spell icons
#  15. ...and Block Details still does
#  16. Enter closes the editor
#  17. ...and the block carries the new name
#  18. ...and the palette entry followed it
#  19. a block with no scene-object name opens no editor
#  20. the list can scroll sideways at all                     <- not vacuous
#  21. starting a rename does not scroll the list sideways
#
# 21 is another bug report made into a gate: scrollTo() ensures visibility on
# BOTH axes and the name sits in the right-hand column, so opening the editor
# dragged the whole list across and pushed the block TYPE off the left edge --
# the one thing you need to still see while renaming. 20 widens the Name column
# first so the list genuinely has somewhere to scroll; with both columns fitting
# there is nothing to get wrong and 21 would pass on the broken code.
#
# Checks 12-15 are a bug report made into a gate. Qt's default edit triggers
# opened the DELEGATE's editor on the same double-click that starts the rename,
# so two editors landed on one cell and the delegate's was on top -- an integer
# over the raw string INDEX, which is what "I can only edit the node number"
# was. And spEditStringIndex is instant and applicable to every tStringIndex,
# so with a block row's Value cell buddying to that block's Name it drew its txt
# icon down the entire column. 15 is the other half of that: Block Details, where
# the icon marks the few fields that have one, must keep it.
#
# The rename used to return unless the index came from the proxy, so in flat list
# mode F2 and double-click did nothing at all -- silently, and on the mode a type
# filter switches you into automatically. The two modes also put the name in
# different columns (proxy 1, flat ValueCol), which is the kind of thing running
# one mode hides. Both are runnable here; see MODES below for why only one gates.
#
# Check 18 is the discriminating one. 17 passes on a rename that writes the node
# and nothing else, which is exactly the version that breaks animation.
#
# Checks 6 and 7 exist because "an editor appeared" is satisfied by an editor at
# (0,0) with an empty string in it. Check 10 holds the deliberate column
# asymmetry: column 0 is the block TYPE and double-clicking it expands.
#
# Both entry points are fired through the objects the window installs -- the
# QShortcut's own activated signal and the view's doubleClicked signal -- rather
# than by calling the rename directly, so the wiring is inside what is measured.
#
# ONE MODE PER PROCESS, seeded into QSettings before launch rather than switched
# at run time. Switching the block list's model corrupts the heap intermittently
# -- 4 runs in 5, and still with this feature's drag-and-drop wiring disabled, so
# it is neither rename's doing nor drag-and-drop's. Check 4 is what stops a seed
# that did not take from letting both runs measure the same mode.
#
# FIXTURE: the program's own starter document, written here by the CLI. The
# palette is inserted by the harness, so this needs no game corpus.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/block_rename.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45897}"
LOG="$ROOT/release/ww_blockrename_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT	# replaced below, once the settings backup exists

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

"$EXE" -no-gui new -o "$(winpath "$TMP/fixture.nif")" >/dev/null 2>&1
[ -s "$TMP/fixture.nif" ] || { echo "FAIL: could not write the starter document"; exit 1; }
echo "fixture: starter document ($(stat -c%s "$TMP/fixture.nif") bytes)"

# The list mode is a QSettings value read during window construction, so it has
# to be in the registry BEFORE launch. Saved and put back on exit: this is the
# user's own setting, and a harness that leaves the block list in a mode nobody
# chose is a harness people stop running.
# PowerShell, not reg.exe: MSYS2 mangles a "HKCU\..." argument and reg add comes
# back with "Invalid syntax" every time.
KEY="HKCU:\\Software\\NifTools\\NifSkope 2.0\\UI"
ps() { powershell.exe -NoProfile -NonInteractive -Command "$1" 2>/dev/null | tr -d '\r'; }
seed() { ps "Set-ItemProperty -Path '$KEY' -Name 'List Mode' -Value '$1' -Type String"; }
SAVED="$(ps "(Get-ItemProperty -Path '$KEY' -Name 'List Mode' -EA SilentlyContinue).'List Mode'")"
restore() {
	if [ -n "$SAVED" ]; then
		seed "$SAVED"
	else
		ps "Remove-ItemProperty -Path '$KEY' -Name 'List Mode' -EA SilentlyContinue"
	fi
}
trap 'restore; rm -rf "$TMP"' EXIT

# HIERARCHY ONLY by default. The list-mode fix below IS verified -- 15 of 15
# checks, repeatedly -- but the flat list mode intermittently takes the whole
# process down (roughly one run in three) for reasons that are neither this
# feature's nor rename's: see the backlog. A check that fails intermittently
# teaches you to re-run rather than to look, so it is not in the gate.
#
#   MODES="hierarchy list" bash tests/spells/block_rename.sh
#
# runs it anyway, which is how the list-mode half was verified and how it should
# be re-checked once the flat list is fixed.
fails=0
for mode in ${MODES:-hierarchy}; do
	echo "--- $mode mode"
	seed "$mode"
	[ "$(ps "(Get-ItemProperty -Path '$KEY' -Name 'List Mode').'List Mode'")" = "$mode" ] \
		|| { echo "FAIL: could not seed the list mode"; exit 2; }
	rm -f "$LOG"
	WW_BLOCKRENAME_TEST="$mode" "$EXE" --port "$PORT" "$(winpath "$TMP/fixture.nif")" >/dev/null 2>&1 &
	pid=$!
	for _ in $(seq 1 60); do
		[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
		sleep 1
	done
	kill "$pid" 2>/dev/null
	wait "$pid" 2>/dev/null

	if [ ! -f "$LOG" ] || ! grep -q '^done$' "$LOG" 2>/dev/null; then
		echo "FAIL: the $mode run did not finish"
		[ -f "$LOG" ] && cat "$LOG"
		fails=1
		continue
	fi
	cat "$LOG"
	grep -q '^PASS$' "$LOG" || fails=1
done

exit "$fails"
