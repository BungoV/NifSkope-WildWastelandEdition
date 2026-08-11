#!/bin/bash
#
# Editing a COMPILED collision body in place, without decompiling it.
#
# WHY THIS EXISTS
#
# hknpEncodeSystem reproduces 810 of 822 stock Fallout 4 packfiles byte for
# byte — bodies, compounds, primitives, constraints, the ragdoll skeleton, the
# shape-list permutation, all of it. It was reachable only from the CLI's own
# round-trip self-test.
#
# The single production write of collision Binary Data went through
# hknpEncodeCompressedMesh, which builds ONE static body with ONE compressed
# triangle mesh. So changing a compiled body's friction meant Decompile, edit,
# Compile — and losing every other body, the compounds, the primitives, the
# constraints and the ragdoll skeleton on the way, none of which the file gets
# back.
#
# Now the system is decoded, one modelled field is changed, and it is encoded
# again. Every opaque region the decoder carries verbatim goes back as it came.
#
# ONLY WHAT IS REALLY STORED IS EDITABLE. Most of the compiled display is a
# substitution: Motion System and Quality Type are derived from hasMotion,
# Penetration Depth is the literal 0.15, Keyframed and Wind are always false.
# Friction and restitution are stored; the collision filter is stored only when
# the body has one, because a layer of 0 is real and the decode substitutes 1 or
# 10 so the row reads usefully.
#
# WHAT IS MEASURED
#
#   1. the fixture really has a compiled system            <- not vacuous
#   2. a compiled row offers an editable friction at all
#   3. changing it rewrites the packfile...
#   4. ...to the same size, so nothing structural moved
#   5. ...in exactly one undo step
#   6. the new value survives a re-decode
#   7. ONE UNDO RESTORES THE BYTE-IDENTICAL PACKFILE
#
# Check 7 is the whole point. An edit path that rewrites more than it was asked
# to — requantizing a mesh, re-deriving a capsule's core box, dropping an
# undecoded region — passes 1 through 6 and fails only this one. It is a byte
# comparison against the original, not a proxy for one.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/collision_compiled_edit.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
PORT="${PORT:-45901}"
LOG="$ROOT/release/ww_collcompiled_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

# a copy, because the harness edits the file in memory and a stray save must
# not touch the game data
cp "$SRC" "$TMP/fixture.nif"

rm -f "$LOG"
WW_COLLCOMPILED_TEST=1 "$EXE" --port "$PORT" "$(winpath "$TMP/fixture.nif")" >/dev/null 2>&1 &
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
