#!/bin/bash
#
# Create faceBones NIF — build a _faceBones mesh from a standard-rigged head.
#
# WHY THIS EXISTS
#
# The transformation is Transfer Bones and Weights with a faceBones donor, which
# was already tested. What is new is that it runs against a COPY and produces a
# separate document, and that has two claims a compile cannot check:
#
#   * the open document is not modified
#   * the CustomizationRemapData in the output is the SOURCE's standard-skeleton
#     skinning, not the sculpt weights the output itself now carries
#
# The second is the one worth a harness. Both blobs are the same length, both
# are structurally valid, and a file with the wrong one looks perfect in every
# view NifSkope has — it just cannot map back to the animation skeleton, which
# is the only thing RemapData is for. Ordering is what makes it right (generate
# BEFORE the transfer when the donor carries sculpt bones), and ordering is
# exactly the kind of thing that survives a refactor by luck.
#
# WHAT IS MEASURED
#
#   A. the open document is byte-identical after the spell runs
#   B0-B3. the result lands in Loaded NIFs, unsaved, and saving that row writes
#     the file the rest of this reads — the spell itself writes nothing
#   B. the faceBones file exists
#   C. it reloads
#   D. the shape kept its block number
#   E. it carries skin_bone_* face-sculpt bones      <- the transfer happened
#   F. RemapData is 12 bytes per vertex
#   G. RemapData equals the SOURCE's standard skinning    <- the ordering claim
#   H. ...and is NOT the output's own sculpt skinning     <- G alone is weaker
#   I. the spell refuses to run on the file it produced
#
# H exists because G could pass vacuously if the transfer silently did nothing:
# the output's skinning would then equal the source's and both comparisons would
# agree. Requiring them to DISAGREE is what makes G mean something.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/rigging/facebones.sh

set -u

# Opens a real window; keep it off the primary monitor. See _harness.sh.
. "$(dirname "$0")/../spells/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
ASSETS="${ASSETS:-/e/Tools/Fallout 4/DataUnpacked/Data/Meshes/Actors/Character/CharacterAssets}"
SRC="${SRC:-$ASSETS/BaseFemaleHead.nif}"
DONOR="${DONOR:-$ASSETS/BaseFemaleHead_faceBones.nif}"
PORT="${PORT:-45897}"
LOG="$ROOT/release/ww_facebones_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no source head at $SRC"; exit 2; }
[ -f "$DONOR" ] || { echo "no faceBones donor at $DONOR"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

# Work on a copy: the harness saves the generated row next to it, and the corpus
# is read-only by intent.
cp "$SRC" "$TMP/head.nif"
OUT="$TMP/head_faceBones.nif"

rm -f "$LOG"
WW_FACEBONES_TEST=1 \
WW_TEST_DONOR="$(winpath "$DONOR")" \
WW_TEST_FACEBONES_OUT="$(winpath "$OUT")" \
	"$EXE" --port "$PORT" "$(winpath "$TMP/head.nif")" >/dev/null 2>&1

if [ ! -f "$LOG" ]; then
	echo "FAIL: harness produced no log — did the window open?"
	exit 1
fi

cat "$LOG"

fails="$(sed -n 's/^\([0-9]*\) passed, \([0-9]*\) failed$/\2/p' "$LOG" | tail -1)"
[ -n "$fails" ] || { echo "FAIL: no result line in $LOG"; exit 1; }
exit $(( fails == 0 ? 0 : 1 ))
