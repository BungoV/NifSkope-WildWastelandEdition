#!/bin/bash
#
# The starter cube: it builds, it renders, and Reload gives it back.
#
# WHY THIS EXISTS
#
# NifSkope opens on a cube for the same reason Blender does -- an empty document
# gives you nowhere to click, and Add Primitive itself refuses without an
# existing BSTriShape to clone a vertex layout from.
#
# That document has no file behind it, and Reload assumed there always is one.
# QFileInfo("") resolves to the working directory, loadFromFile fails on it, and
# the window was left blank: Reload was the one command in the program that could
# destroy the starter scene without touching anything on disk.
#
# WHAT IS MEASURED
#
#   1. the starter document is built at all
#   2. it is the scene it is supposed to be (NiNode / BSTriShape / shader
#      property / texture set), not just some blocks
#   3. an untouched window is CLEAN, so closing it does not ask about saving
#   4. it renders -- the framebuffer grab is not empty
#   5. reload puts the block count back after an edit
#   6. ...to the cube, not to an empty document        <- the defect
#   7. ...and clean again, as a freshly opened window is
#
# Check 5 edits the document first. A reload that did nothing whatsoever would
# pass a bare "the cube is still there", which is exactly the state this
# replaced -- the blank window came from the load FAILING, so "did the count
# change" has to be asked against a count that was deliberately made wrong.
#
# NOTE ON WW_ VARIABLES: startupCubeWanted() is off for any WW_* environment, so
# every other harness keeps the empty document it was written against.
# WW_STARTER_SHOT is the exception, because it is the harness FOR this path.
#
# USAGE
#   bash tests/spells/starter_reload.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45899}"
LOG="$ROOT/release/ww_starter_test.log"
SHOT="$ROOT/release/ww_starter_shot.png"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

rm -f "$LOG" "$SHOT"
# no file argument: this is the startup path, which is what is being tested
WW_STARTER_SHOT="$(winpath "$SHOT")" "$EXE" --port "$PORT" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 45); do
	[ -f "$LOG" ] && grep -q '^reload clean' "$LOG" 2>/dev/null && break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log"; exit 1; }
cat "$LOG"

checks=0; fails=0
check() {
	checks=$((checks+1))
	if [ "$1" = "1" ]; then echo "  ok   $2"; else echo "  FAIL $2"; fails=$((fails+1)); fi
}

blocks=$(sed -n 's/^blocks \([0-9]*\).*/\1/p' "$LOG" | head -1)
check "$([ "${blocks:-0}" -ge 4 ] && echo 1 || echo 0)" \
	"the starter document is built ($blocks blocks)"
for want in NiNode BSTriShape BSLightingShaderProperty BSShaderTextureSet; do
	check "$(grep -q "\] $want" "$LOG" && echo 1 || echo 0)" "...and holds a $want"
done
check "$(grep -q '^clean 1' "$LOG" && echo 1 || echo 0)" \
	"an untouched starter window is clean, so closing it will not ask"
# a black cube on a dark viewport still differs from an unwritten file
if [ -s "$SHOT" ]; then
	check 1 "it renders ($(stat -c%s "$SHOT") bytes of framebuffer)"
else
	check 0 "it renders"
fi

line=$(grep '^reload: ' "$LOG")
base=$(printf '%s' "$line" | sed -n 's/^reload: \([0-9]*\) blocks.*/\1/p')
edit=$(printf '%s' "$line" | sed -n 's/.*edited to \([0-9]*\).*/\1/p')
after=$(printf '%s' "$line" | sed -n 's/.*after reload \([0-9]*\).*/\1/p')
check "$([ "${edit:-0}" -gt "${base:-0}" ] && echo 1 || echo 0)" \
	"the document was actually edited before reloading ($base -> $edit)"
check "$([ "${after:-0}" = "${base:-x}" ] && echo 1 || echo 0)" \
	"reload puts it back to $base blocks (got ${after:-none})"
check "$([ "${after:-0}" -ge 4 ] && echo 1 || echo 0)" \
	"...to the cube, not to an empty document"
check "$(grep -q '^reload clean 1' "$LOG" && echo 1 || echo 0)" \
	"...and clean, like a freshly opened window"

echo "$checks checks, $fails failures"
[ "$fails" = "0" ] || { echo FAIL; exit 1; }
echo PASS
exit 0
