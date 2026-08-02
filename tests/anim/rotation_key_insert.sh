#!/bin/bash
#
# Can a key be inserted on a rotation lane?
#
# One of two fixes in 2ff7457 that shipped verified only by reading the diff, and
# the changelog said so at the time: "the rotation fix compiles and the sampler
# is straightforward, but nothing has yet inserted a rotation key and read it
# back." This is that.
#
# THE BUG. insertKeyAtTime skipped QuatVal outright, because Controller::
# interpolate has no Quat specialisation. So I or a double-click on a rotation
# lane produced no key AND no message, on the most-keyed channel there is, while
# writeChannelKeys had handled quaternion keys correctly all along. The fix
# SLERPs between the bracketing keys off the list the function already reads.
#
# THE DISCRIMINATING CHECK is the key COUNT. The old code silently did nothing,
# so "one more key exists afterwards" fails on it. The value assertion is
# deliberately loose -- a SLERP between two unit quaternions must itself be a
# unit quaternion, which catches an uninitialised or zeroed sample without
# pinning the test to one interpolation implementation.
#
# THE FIXTURE IS BUILT, NOT FOUND, AND THAT IS THE INTERESTING PART.
#
# Fallout 4 stores rotations as XYZ float curves (KeyType 4), which the timeline
# decomposes into three Float channels -- not a QuatVal channel. Scanning ~300
# meshes across Effects, Actors, SetDressing and Architecture turned up
# NiTransformData blocks in quantity and NOT ONE with Num Rotation Keys >= 2.
# The quaternion path is real (Oblivion, Skyrim and .kf files use it, and
# writeChannelKeys has always handled it) but no stock FO4 asset exercises it.
#
# So the script grows the Quaternion Keys array on a real NiTransformData and
# writes two distinct rotations into it. A test that silently found no lane to
# work on would otherwise report success while checking nothing -- which this
# harness treats as a failure, not a skip.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/anim/rotation_key_insert.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/Actors/Alien/CharacterAssets/skeleton.nif}"
PORT="${PORT:-46251}"
LOG="$ROOT/release/ww_rotkey_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }
W="$(winpath "$TMP")"

# --- build a file that actually has quaternion rotation keys -----------------
TD="$("$EXE" -no-gui list "$(winpath "$SRC")" 2>/dev/null \
	| sed -n 's/^\[\([0-9]*\)\] NiTransformData.*/\1/p' | head -1)"
[ -n "$TD" ] || { echo "no NiTransformData in $SRC"; exit 2; }
echo "fixture: NiTransformData block $TD, growing Quaternion Keys to 2"

"$EXE" -no-gui set "$(winpath "$SRC")" -b "$TD" -f "Num Rotation Keys" -v 2 \
	-o "$W/q1.nif" > /dev/null 2>&1
"$EXE" -no-gui set "$W/q1.nif" -b "$TD" -f "Quaternion Keys/1/Time" -v 2.0 \
	-o "$W/q2.nif" > /dev/null 2>&1
"$EXE" -no-gui set "$W/q2.nif" -b "$TD" -f "Quaternion Keys/0/Value" -v "1,0,0,0" \
	-o "$W/q3.nif" > /dev/null 2>&1
"$EXE" -no-gui set "$W/q3.nif" -b "$TD" -f "Quaternion Keys/1/Value" -v "0.7071,0,0,0.7071" \
	-o "$W/rot.nif" > /dev/null 2>&1
[ -s "$TMP/rot.nif" ] || { echo "could not build the rotation fixture"; exit 2; }

rm -f "$LOG"
WW_ROTKEY_TEST=1 "$EXE" --port "$PORT" "$W/rot.nif" > /dev/null 2>&1

[ -f "$LOG" ] || { echo "harness produced no log"; exit 1; }
tr -d '\r' < "$LOG"

grep -q '^PASS' "$LOG"
