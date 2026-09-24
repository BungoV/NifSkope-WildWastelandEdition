#!/bin/bash
#
# A loaded Havok animation (.hkx) plays on the open NIF's bones.
#
# WHY THIS EXISTS
#
# bungo, 2026-09-10: "in animation workspace, add an option to load a hkx file
# with animation, then they get added to the animations list, and if there's
# rigged geometry with nodes / bone names that match, they play". Lane HKX1
# built the reader and proved the DECODER against an independent one. Nothing
# there touches a NiNode, so nothing there can say whether the pose the decoder
# produced is the pose the rig is actually standing in. That is what this asks,
# from the other end: read the transform back OFF THE SCENE GRAPH and hold it
# against the decoder.
#
# THE FIXTURES ARE THE POINT, twice over.
#
#   skeleton.nif is not a clean match for skeleton.hkx and never was. 17
#   Weapon* bones of the animation skeleton have no node in it, and four names
#   (Head, Spine1, Spine2, Weapon) differ from the hkx only in CASE. A matcher
#   that is case-sensitive gets 74; one that ignores the difference between
#   "absent" and "present" gets 95. Only 78 / 17 / 4 is the truth, and those
#   three numbers were measured by lane HKX1 before this code was written.
#
#   35CourtSign01.nif is a road sign. It has geometry and nodes and not one
#   bone the clip names, which is bungo's "no match" case: it must refuse in
#   WORDS and leave every transform in the file exactly where it found it.
#
# WHAT IS MEASURED
#
#   (c)  the mapping report is 78 / 17 / 4, and the unmatched bones are NAMED
#   (c') FLOOR: 95 invented names match nothing        <- the matcher is not
#                                                         matching everything
#   (a)  at frames 0, N/2 and N-1 every matched NiNode's local transform equals
#        the decoder's output for that frame: translation and scale <= 1e-4,
#        rotation <= 0.01 degrees
#   (a') FLOOR: held against the WRONG frame the same test goes red
#   (b') FLOOR: the clip really moved the rig
#   (b)  unloading puts every Transform in the scene back BYTE for byte
#   (d)  on a NIF with no matching bones: it still loads, it refuses to bind, it
#        says so in words, and nothing in the file changes
#
# Checks (a) and (b) read Node::localTrans(), not the playback's own record of
# what it wrote: telemetry echoes truth, never intent.
#
# NOTE ON PORTS: the GUI needs a free IPC port and NifSkope EXITS SILENTLY if
# it cannot bind one. Keep the number below ~49152.
#
# USAGE
#   bash tests/spells/hkxanim_play.sh

set -u

# Real window; keep it off the primary monitor. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/character/characterassets/skeleton.nif}"
CLIP="${CLIP:-$ROOT/scratchpad/hkx1_20260910/clips/jog.hkx}"
SKEL="${SKEL:-$ROOT/scratchpad/hkx1_20260910/clips/skeleton.hkx}"
NOMATCH="${NOMATCH:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
EXPECT="${EXPECT:-78,17,4}"
LOG="$ROOT/release/ww_hkxanim_test.log"
PORT="${PORT:-42291}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no rigged NIF at $SRC"; exit 2; }
[ -f "$CLIP" ] || { echo "no clip at $CLIP (pull one with tools/ba2get.py, see the ww-hkx-animation skill)"; exit 2; }
[ -f "$SKEL" ] || { echo "no skeleton.hkx at $SKEL"; exit 2; }
[ -f "$NOMATCH" ] || { echo "no no-match NIF at $NOMATCH"; exit 2; }

rm -f "$LOG"
WW_HKXANIM_TEST=1 \
WW_HKXANIM_CLIP="$(winpath "$CLIP")" \
WW_HKXANIM_SKEL="$(winpath "$SKEL")" \
WW_HKXANIM_NOMATCH="$(winpath "$NOMATCH")" \
WW_HKXANIM_EXPECT="$EXPECT" \
	"$NS" --port "$PORT" "$SRC" >/dev/null 2>&1

[ -f "$LOG" ] || { echo "FAIL: no log — did the app exit before the harness ran?"; exit 1; }
cat "$LOG"
grep -q "^PASS" "$LOG" && exit 0
exit 1
