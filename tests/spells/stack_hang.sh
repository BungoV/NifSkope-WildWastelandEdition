#!/bin/bash
#
# Catch the list-mode rename hang in the act and take a stack of every thread.
#
# WHY THIS EXISTS
#
# block_rename.sh in list mode hangs some runs and not others (see
# docs/TO_BE_IMPLEMENTED.md). Everything cheap has been ruled out: no crash, no
# heap corruption, not the animation setting, not a stale build, not the port,
# not a repaint storm, and not a nested event loop — a watchdog timer armed
# before the pump logs nothing at all across a 60-second hang, which says one
# handler is stuck rather than a modal being up.
#
# What was never taken is a stack. That is this.
#
# HOW IT WORKS
#
# Run the harness; if NifSkope is still alive after the wait, that run has hung —
# a passing one takes 4-7 seconds — so attach gdb and dump every thread before
# the harness's own deadline kills it. Repeats until it catches one.
#
# The binary is linked with -Wl,-s, so expect Qt DLL frames rather than ours.
# Still enough to name the widget and the call.
#
# USAGE
#   bash tests/spells/stack_hang.sh [attempts] [out-file]
#
# Windows go on the SECOND MONITOR like every other harness here: a dozen
# launches on the primary is not something to do to somebody who is working.

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ATTEMPTS="${1:-12}"
OUT="${2:-$ROOT/release/stack_hang.txt}"
WAIT="${WAIT:-25}"
GDB="${GDB:-/c/msys64/ucrt64/bin/gdb.exe}"

[ -x "$GDB" ] || { echo "no gdb at $GDB"; exit 2; }
: > "$OUT"

for attempt in $(seq 1 "$ATTEMPTS"); do
	# a port of its own per attempt: a leftover instance from the one before would
	# otherwise take the file over IPC and the harness would measure nothing
	MODES=list WW_WINDOW_AT="${WW_WINDOW_AT:-1920,0}" PORT=$(( 45800 + attempt )) \
		bash "$ROOT/tests/spells/block_rename.sh" >/dev/null 2>&1 &
	runner=$!
	sleep "$WAIT"
	pid=$(powershell.exe -NoProfile -NonInteractive -Command \
		"(Get-Process NifSkope -EA SilentlyContinue | Select-Object -First 1).Id" 2>/dev/null | tr -d '\r ')
	if [ -n "$pid" ]; then
		echo "=== attempt $attempt: NifSkope $pid still alive at ${WAIT}s — hung, taking a stack" >> "$OUT"
		# what it is DOING matters more than whether it faulted, which is what the
		# earlier gdb work asked and why it came back with nothing
		"$GDB" -p "$pid" -batch -ex "set pagination off" \
			-ex "thread apply all bt 25" >> "$OUT" 2>&1
		wait "$runner" 2>/dev/null
		echo "=== done" >> "$OUT"
		echo "caught on attempt $attempt; stack in $OUT"
		exit 0
	fi
	echo "attempt $attempt: finished before ${WAIT}s, retrying" >> "$OUT"
	wait "$runner" 2>/dev/null
done
echo "never hung in $ATTEMPTS attempts; see $OUT"
