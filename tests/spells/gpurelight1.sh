#!/bin/bash
# Lane GPURELIGHT1: the whole gate in one call (Git Bash on Windows; run it under the machine's nifskope lock).
#   1. `NifSkope -no-gui gpurelight --out <dir>`: the synthetic three-door scene; OFF, P (CPU vs the ray relight),
#      G (GPU vs CPU), D1 (closed doors vs a merged-door ray relight), D2 (rays through each door kind) and the reds
#   2. tests/spells/gpurelight1_cells.py <dir>: the numpy twin (direct light re-traced through the doors' real
#      triangles, masks and panes) and the shared light record read back
#   3. tests/spells/gpurelight1_sheet.py: the before|after sheet
# usage: bash tests/spells/gpurelight1.sh <out dir>        exit 0 = every gate held
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:?usage: gpurelight1.sh <out dir>}"
rm -rf "$OUT"
"$ROOT/release/NifSkope.exe" -no-gui gpurelight --out "$OUT" > "$OUT.log" 2>&1
rc=$?
grep -E "^(OK|FAIL)|^timings" "$OUT.log"
[ $rc -eq 0 ] || { echo "gpurelight1: the exe's gates FAILED (rc $rc)"; exit 1; }
python "$ROOT/tests/spells/gpurelight1_cells.py" "$OUT" || exit 1
python "$ROOT/tests/spells/gpurelight1_sheet.py" "$OUT" "$OUT/sheet_doors.png" || exit 1
echo "gpurelight1: PASS"
