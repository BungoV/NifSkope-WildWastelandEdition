#!/bin/bash
# AUDIT1 step 3: the refuters for the .lodt and .lodb invariants, on copies of
# real product-written files. Each mutation must turn its reader red.
#
# R2 is RETARGETED from its first draft, and the first draft is the lesson: it
# changed the first 40-hex digest in the record, which is the `switches` digest,
# and `lodgen_bakerec_gate.py hashes` does not read that line -- it reads the
# five `hash <name> <16 hex>` rows against the .lodo header. The refuter said
# NOT CAUGHT about a reader that was working. A refuter has to be aimed at a
# field the reader it refutes actually reads, or it measures nothing.
#
#   usage: bash refute_rest.sh <file.lodt> <file.lodb> <bake tree> <bake log>
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
PY="${PY:-/c/Users/bungo/AppData/Local/Programs/Python/Python39/python}"
SC="$ROOT/scratchpad/audit1_20260916"
T="$(mktemp -d)"; missed=0
VT="$1"; LODB="$2"; TREE="$3"; LOG="$4"

# R1: one byte flipped deep inside a tile payload -- the tile CRC must stop
# recomputing, and nothing else in the file can notice.
cp "$VT" "$T/a.lodt"
"$PY" "$SC/mutate_lodt.py" "$T/a.lodt"
if "$PY" "$ROOT/tests/spells/lodgen_vt_check.py" tiles "$T/a.lodt" >/dev/null 2>&1
then echo "R1 lodt one payload byte flipped      NOT CAUGHT"; missed=$((missed+1))
else echo "R1 lodt one payload byte flipped      CAUGHT"; fi

# R2: one hex digit of objectCorpusHash -- `hashes` must go red.
cp "$LODB" "$T/a.lodb"
"$PY" "$SC/mutate_lodb.py" hash "$T/a.lodb"
O="$(find "$TREE" -name '*.lodo'|head -1)"; I="$(find "$TREE" -name '*.lodi'|head -1)"
if "$PY" "$ROOT/tests/spells/lodgen_bakerec_gate.py" hashes "$T/a.lodb" "$O" "$I" >/dev/null 2>&1
then echo "R2 lodb objectCorpusHash one digit    NOT CAUGHT"; missed=$((missed+1))
else echo "R2 lodb objectCorpusHash one digit    CAUGHT"; fi

# R3: one `out` row deleted -- `sections` must go red.
cp "$LODB" "$T/b.lodb"
"$PY" "$SC/mutate_lodb.py" dropout "$T/b.lodb"
if "$PY" "$ROOT/tests/spells/lodgen_bakerec_gate.py" sections "$T/b.lodb" "$TREE" "$LOG" >/dev/null 2>&1
then echo "R3 lodb one out row deleted           NOT CAUGHT"; missed=$((missed+1))
else echo "R3 lodb one out row deleted           CAUGHT"; fi

rm -rf "$T"
echo "$missed refuter(s) that did NOT catch their mutation"
