#!/bin/bash
#
# The Block List in both modes: can anything in it be clicked?
#
# WHY THIS EXISTS
#
# The flat list mode was filed as "blocks inserted while it is showing are not
# addressable", and that was the second-worst reading of it. NO row was
# addressable -- not the new one, not the ones that had been there since the file
# opened -- so in that mode a click, a drop and a right-click all landed on
# nothing, everywhere, all the time.
#
# The original measurement asked only about the row it had just inserted, which
# cannot tell "this row is broken" from "every row is broken". This one asks
# about all of them, which is how the real shape of it turned up in one run.
#
# THE MECHANISM, because the invariant below is otherwise a strange thing to
# assert. QHeaderView keeps `length` as the total of its sections, maintained by
# adding and subtracting rather than re-derived. Hiding a section subtracts its
# width and remembers it; changing the model gives every remembered width back
# WITHOUT adding it to `length`. Hide them again and each width comes off twice.
# The Block List hides 9 of the NifModel's 12 columns, so after a file load
# `length` was NEGATIVE -- and QHeaderView::visualIndexAt answers -1 for any
# position past `length`, so QTreeView::indexAt found no column, and with no
# column it returns no index at all.
#
# Two things put it right, and this covers both: the columns are released before
# any model change and applied after (nothing hidden at the moment of the change,
# nothing to hand back), and each mode keeps its own saved header blob, since
# restoring one saved against the 3-column proxy onto the 12-column NifModel
# desyncs the total in exactly the same way.
#
# WHAT IS MEASURED, per mode
#
#   1. the app started in the mode under test                   <- not vacuous
#   2. the header's total matches the sections it totals        <- the invariant
#   3. every row resolves back to itself through indexAt        <- what it costs
#   4. a block inserted now is addressable, and is the one inserted
#   5. an even number of switches ends in the mode it started in
#   6. the header's total still matches after switching modes
#   7. every row still resolves after switching modes
#   8. the model's row signals agree with its own row counts
#
# Checks 6 and 7 are there because nothing re-derives that total on a switch: a
# mode that breaks it leaves the OTHER mode broken too, which would make this
# flat mode's fault only in where it starts.
#
# Against the code before the fix, in list mode, checks 2, 3, 4 and 7 fail and
# the log shows every row of the file resolving to INVALID.
#
# ON THE CRASH THAT IS FILED WITH THIS: the flat list was also reported to take
# the process down, one run in three. It does not reproduce -- 12 runs of
# block_rename.sh in list mode on the code before this fix, 12 after, plus 12
# runs of this harness's switching loop -- and the session that filed it lost an
# hour the same night to heap corruption that turned out to be a stale
# incremental build. Not proven either way; not reproducible today.
#
# FIXTURE: the CLI's cube fixture (`new --cube`). No game
# corpus.
#
# USAGE
#   bash tests/spells/block_list_modes.sh
#   MODES=list CYCLES=10 bash tests/spells/block_list_modes.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45902}"
CYCLES="${CYCLES:-4}"
LOG="$ROOT/release/ww_flatlist_test.log"
TMP="$(mktemp -d)"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }

# The list mode is read while the window is being built, so it has to be in the
# registry BEFORE launch -- and it is the user's own setting, so it goes back.
# PowerShell, not reg.exe: MSYS2 mangles a "HKCU\..." argument.
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

"$EXE" -no-gui new --cube -o "$(winpath "$TMP/fixture.nif")" >/dev/null 2>&1
[ -s "$TMP/fixture.nif" ] || { echo "FAIL: could not write the cube fixture"; exit 1; }
echo "fixture: cube fixture ($(stat -c%s "$TMP/fixture.nif") bytes)"

fails=0
for mode in ${MODES:-hierarchy list}; do
	echo "--- $mode mode"
	seed "$mode"
	[ "$(ps "(Get-ItemProperty -Path '$KEY' -Name 'List Mode').'List Mode'")" = "$mode" ] \
		|| { echo "FAIL: could not seed the list mode"; exit 2; }
	rm -f "$LOG"
	WW_FLATLIST_TEST="$CYCLES" WW_FLATLIST_TEST_MODE="$mode" \
		"$EXE" --port "$PORT" "$(winpath "$TMP/fixture.nif")" >/dev/null 2>&1 &
	pid=$!
	for _ in $(seq 1 60); do
		[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
		kill -0 "$pid" 2>/dev/null || break
		sleep 1
	done
	kill "$pid" 2>/dev/null
	wait "$pid" 2>/dev/null

	if [ ! -f "$LOG" ]; then
		echo "FAIL: the harness wrote no log"; fails=$((fails + 1)); continue
	fi
	cat "$LOG"
	# a process that died half way leaves a log with no verdict in it
	grep -q '^PASS$' "$LOG" || fails=$((fails + 1))
done

[ "$fails" = "0" ] || exit 1
exit 0
