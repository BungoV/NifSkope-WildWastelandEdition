#!/bin/bash
#
# The destructive confirmation — is it on the right spells, does it say what is
# lost, and can it still be switched off?
#
# WHY THIS EXISTS
#
# SpellBook::cast used to pop one generic question — "This action cannot
# currently be undone. Do you want to continue?" — for nearly every write, since
# `undoable()` is overridden true by six spells in the entire tree. It carried a
# "Do not ask me again" checkbox, so the first time anyone ticked it to get past
# a routine edit, Crop To Branch, Remove Branch, Remove and Apply Transformation
# all went silent too, permanently, in the registry.
#
# It was loudest where it mattered least and gone where it mattered most. The
# replacement inverts it: `Spell::destructive()` marks the handful that destroy
# something, those ask in their own words with the counts filled in, and the
# question has no off switch. Everything else no longer asks at all.
#
# WHAT IS MEASURED
#
# Reading the diff cannot tell you any of this holds. A `destructive()` nobody
# consults, a Cancel that still lets the cast through, a text that forgot to
# substitute its counts — all three look right on the page. So the harness casts
# real spells and answers the real modal from inside its own event loop:
#
#   1. Crop To Branch asks before it runs
#   2. Cancel leaves the file alone
#   3. the question names how much of the file goes  <- the whole difference from
#      "cannot currently be undone"; matched as "Delete N of the M blocks", with
#      M checked against the file's actual block count
#   4. setting the old suppression key does not disarm it
#   5. Move Up — non-destructive — runs with NO dialog at all
#   6. and still ran
#   7. the go-ahead button really does cast
#
# Check 5 is the one that fails on the previous version: the old code prompted
# for Move Up like it prompted for everything else.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/destructive_confirm.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/Effects/ElectricalExplosionSmall.nif}"
PORT="${PORT:-45884}"
LOG="$ROOT/release/ww_destructive_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

# a copy, because check 7 lets the crop through and the model is modified
cp "$SRC" "$TMP/subject.nif"

rm -f "$LOG"
WW_DESTRUCTIVE_TEST=1 "$EXE" --port "$PORT" "$(winpath "$TMP/subject.nif")" \
	> /dev/null 2>&1

[ -f "$LOG" ] || { echo "harness produced no log"; exit 1; }
cat "$LOG"

grep -q '^PASS' "$LOG"
