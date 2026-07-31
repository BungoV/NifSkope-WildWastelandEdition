#!/bin/bash
#
# Sequence binding — does a Controlled Block resolve through the FILE's object
# palette, or through a name search of the scene graph?
#
# WHY THIS EXISTS
#
# The engine resolves every Controlled Block with
# NiDefaultAVObjectPalette::GetAVObject (1.10.155 0x1bc1d20) on the palette the
# manager was loaded with, and never searches the tree. NifSkope used to call
# Node::findChild and take the first name match. On a file where each node name
# is unique the two agree on every row, so nothing observable separates the old
# behaviour from the new one — which is exactly the trap: a change that cannot
# fail a test is a change nobody can trust.
#
# So this test builds the disagreement. The palette is the authority, so a file
# whose palette maps a name to a DIFFERENT node than the tree's first match is
# a legal file, and the only correct answer is the palette's. That file is made
# here from a vanilla one by rewiring a single Ptr.
#
# THE MEASUREMENT
#
#   differs  rows where the palette and a findChild name search picked
#            different nodes.
#
# It is 0 for the old code by construction — a name search cannot disagree with
# itself — so:
#
#   the vanilla file       must read differs = 0   (control: no false positives)
#   the rewired file       must read differs = 1   (only the palette can see it)
#
# Drop the palette lookup and the second reading goes to 0 and this fails.
#
# NOTE ON PORTS: NifSkope binds a UDP port for its single-instance IPC and exits
# silently if it cannot. On this machine UDP above ~49152 is refused, so --port
# must stay below that or every run looks like an instantly passing no-op.
#
# USAGE
#   bash tests/anim/sequence_binding.sh
# Needs release/NifSkope.exe and the unpacked FO4 data.

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/effects/Ambient/FluorescentLightBeam.nif}"
LOG="$ROOT/release/ww_seqbind_test.log"
PORT="${PORT:-45871}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $*"; }

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no source NIF at $SRC"; exit 2; }

# Windows path, because the exe is native and resolves the argument itself: a
# /e/... path silently does not exist, the window opens empty, and the harness
# never fires.
winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

# run the harness on one file and echo "rows palette differs unresolved"
run() {
	rm -f "$LOG"
	WW_SEQBIND_TEST=1 "$EXE" --port "$PORT" "$(winpath "$1")" > /dev/null 2>&1
	[ -f "$LOG" ] || { echo "MISSING"; return; }
	sed -n 's/^stats rows=\([0-9]*\) palette=\([0-9]*\) differs=\([0-9]*\) unresolved=\([0-9]*\)$/\1 \2 \3 \4/p' "$LOG"
}
verdict() { grep -qx "PASS" "$LOG"; }

# --- the file as shipped ----------------------------------------------------
cp "$SRC" "$TMP/plain.nif"
read -r rows pal diff unres <<< "$(run "$TMP/plain.nif")"
echo "vanilla:  rows=${rows:-?} palette=${pal:-?} differs=${diff:-?} unresolved=${unres:-?}"

if [ "${rows:-0}" -gt 0 ] 2>/dev/null; then ok
else bad "the harness bound nothing in the vanilla file (rows=${rows:-no reading})"; fi

if [ "${pal:-0}" = "${rows:-x}" ]; then ok
else bad "vanilla: ${pal:-0} of ${rows:-?} rows went through the palette; this file's palette names them all"; fi

# The control. A name search and the palette agree here, so a non-zero reading
# would mean the comparison itself is broken and the second half proves nothing.
if [ "${diff:-1}" = "0" ]; then ok
else bad "vanilla: palette and name search already disagree on ${diff} row(s); the control is not clean"; fi

if verdict; then ok; else bad "vanilla: harness reported FAIL"; fi

# --- the same file with the palette pointing elsewhere ----------------------
# Objs/1 is "GlowMesh:0", which the tree's first match resolves to block 11.
# Point it at block 14 (EditorMarker:0's node) instead: same name, different
# node, and only a palette read can tell.
if ! "$EXE" -no-gui set "$TMP/plain.nif" -b 10 -f "Objs/1/AV Object" -v 14 -o "$TMP/rewired.nif" > "$TMP/set.log" 2>&1; then
	echo "could not rewire the palette:"; cat "$TMP/set.log"; exit 2
fi
was="$("$EXE" -no-gui get "$TMP/plain.nif" -b 10 -f "Objs/1/AV Object" 2>/dev/null | tr -d '\r')"
now="$("$EXE" -no-gui get "$TMP/rewired.nif" -b 10 -f "Objs/1/AV Object" 2>/dev/null | tr -d '\r')"
echo "palette row 1 'GlowMesh:0': block $was -> block $now"
if [ "$was" != "$now" ]; then ok
else bad "the rewire did not take: palette still points at $was"; fi

read -r rows2 pal2 diff2 unres2 <<< "$(run "$TMP/rewired.nif")"
echo "rewired:  rows=${rows2:-?} palette=${pal2:-?} differs=${diff2:-?} unresolved=${unres2:-?}"

if [ "${rows2:-0}" = "${rows:-x}" ]; then ok
else bad "rewiring one Ptr changed the row count (${rows:-?} -> ${rows2:-?})"; fi

# THE measurement.
if [ "${diff2:-0}" -ge 1 ] 2>/dev/null; then ok
else bad "the palette was not consulted: differs=${diff2:-no reading}, so the bind still came from the name search"; fi

# ...and it still bound something, rather than "disagreeing" by finding nothing.
if [ "${unres2:-1}" = "0" ]; then ok
else bad "rewired: ${unres2} row(s) resolved to no node at all"; fi

if verdict; then ok; else bad "rewired: harness reported FAIL"; fi

echo
echo "checks passed: $pass   failed: $fail"
[ "$fail" = "0" ] || exit 1
