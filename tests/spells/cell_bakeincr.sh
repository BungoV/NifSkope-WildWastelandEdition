#!/bin/bash
#
# INCREMENTAL PROBE BAKE (lane BAKEINCR1, 2026-10-04; src/probeincr.cpp, tools/ww_bake_world.py).
#
# Every exterior cell bake writes bakeincr.txt beside its .tbk files: a fingerprint of everything it read
# (the 5x5 block's ESM records, every model/material/texture by content, the far LOD ring, the exe and its
# knobs). A world rebake checks the fingerprints and rebakes only the cells whose fingerprint moved.
#
# The gate (tests/spells/cell_bakeincr_gate.py; its header holds every leg and bar):
#   GREEN  one moved reference rebakes EXACTLY its 25 cells, the rest stay byte-identical, and the incremental
#          world equals a full bake of the edited world byte for byte; a model override dirties exactly the
#          cells whose block holds it; a far-LOD edit dirties exactly the cells whose ring reaches it; a light
#          record + light reference edit and a time-of-day change dirty nothing.
#   RED    WW_CELL_BAKEINCR_RED=nonbr (neighbour rule off) and =nomodels (models ignored): both keep stale cells,
#          and the gate shows which.
# The edits (two plugins, one loose model) are generated from the user's own master and unpacked data into the
# output folder by cell_bakeincr_esp.py; nothing of the game is in the repo.
#
# USAGE  bash tests/spells/cell_bakeincr.sh [<out dir>] [--from <step>]   (about 90 bakes; ~2 h; peak ~6.5 GB)
# Run under the nifskope lock (withlock.sh nifskope ...).

set -u
. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
OUT="${1:-$REPO/scratchpad/bakeincr_gate}"
shift || true
mkdir -p "$OUT"
echo "cell_bakeincr $(date '+%F %T') exe $(winpath "$EXE")"
python "$REPO/tests/spells/cell_bakeincr_gate.py" "$(winpath "$EXE")" "$(winpath "$OUT")" "$@"
rc=$?
echo "cell_bakeincr END $(date '+%F %T') rc=$rc"
exit $rc
