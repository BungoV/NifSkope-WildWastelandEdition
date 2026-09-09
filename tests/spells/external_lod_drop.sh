#!/bin/bash
#
# Operating-system drops of LOD chunks (.btr/.bto), not just .nif.
#
# Three separate filters gate an external drop -- the application event filter
# (NifSkope::eventFilter), the viewport's own GLView::dragEnterEvent, and
# validExternalNifPaths behind the choice menu. Each one used to hardcode the
# suffix "nif", so File > Open loaded a .btr happily while dragging the same
# file from Explorer did nothing at all, with no error to explain why. All
# three now read NifSkope::fileExtensions(), the same table the Open dialog
# builds its filter from; this proves a non-.nif extension survives the gate
# and actually opens.
#
# The fixture is a cube written under a .btr name: this exercises the
# EXTENSION filter, which is what regressed, without needing an ESM to
# generate a real chunk.

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45933}"
LOG="$ROOT/release/ww_external_drop_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

winpath() { cygpath -w "$1"; }

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }

# first.btr is the one under test; the other two keep the shared door's
# three-file policy checks meaningful.
"$EXE" -no-gui new --cube -o "$(winpath "$TMP/first.btr")" >/dev/null 2>&1
[ -s "$TMP/first.btr" ] || { echo "FAIL: could not build first.btr"; exit 1; }
for name in second third; do
	"$EXE" -no-gui new --cube -o "$(winpath "$TMP/$name.nif")" >/dev/null 2>&1
	[ -s "$TMP/$name.nif" ] || { echo "FAIL: could not build $name.nif"; exit 1; }
done

rm -f "$LOG"
WW_EXTERNAL_DROP_TEST="$(winpath "$TMP/first.btr");$(winpath "$TMP/second.nif");$(winpath "$TMP/third.nif")" \
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
# The decisive line: the .btr was file 0, so this only passes if a non-.nif
# extension cleared every filter and became the open document.
grep -q 'ok   adaptive drop opens the first NIF over the starter' "$LOG" \
	|| { echo "FAIL: the .btr did not open -- an extension filter rejected it"; exit 1; }
grep -q '^PASS$' "$LOG" || exit 1
exit 0
