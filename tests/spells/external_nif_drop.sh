#!/bin/bash
#
# Operating-system .nif drops: native container gate, adaptive workspace policy,
# independent new windows, and Cancel.
#
# Qt will not deliver a synthetic drop through its native drag manager. The
# in-process harness therefore calls the application filter immediately below
# that boundary and separately proves that both native receiving widgets have
# acceptDrops enabled. No physical mouse is moved or seized.

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45931}"
LOG="$ROOT/release/ww_external_drop_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

winpath() { cygpath -w "$1"; }

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
for name in first second third; do
	"$EXE" -no-gui new -o "$(winpath "$TMP/$name.nif")" >/dev/null 2>&1
	[ -s "$TMP/$name.nif" ] || { echo "FAIL: could not build $name.nif"; exit 1; }
done

rm -f "$LOG"
WW_EXTERNAL_DROP_TEST="$(winpath "$TMP/first.nif");$(winpath "$TMP/second.nif");$(winpath "$TMP/third.nif")" \
	WW_EXTERNAL_DROP_CHOICE=1 \
	"$EXE" --port "$PORT" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 60); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	kill -0 "$pid" 2>/dev/null || break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log"; exit 1; }
cat "$LOG"
grep -q '^PASS$' "$LOG" || exit 1
exit 0
