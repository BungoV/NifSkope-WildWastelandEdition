#!/bin/bash
# RESUME3 gate R3's identity half: every output file of the 16-thread bake is
# byte-identical to the serial one, on BOTH regions -- and the comparator is
# SHOWN RED FIRST on a flipped byte and on a deleted file, because a check
# nobody has seen fail is not a check (CONSTITUTION 4).
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
O="$ROOT/scratchpad/resume3_20260911"
D="$ROOT/scratchpad/bakeperf1_20260911/bake_diff.py"
TMP="$O/refuter"

echo "=== THE REFUTER, first ==="
rm -rf "$TMP"; mkdir -p "$TMP"
cp -r "$O/serial_sanctuary" "$TMP/a"
cp -r "$O/serial_sanctuary" "$TMP/b"
# one flipped byte in one file
V=$(find "$TMP/b" -name "*.BTR" | head -1)
python - "$V" <<'PY'
import sys
p = sys.argv[1]
b = bytearray(open(p, 'rb').read())
b[len(b) // 2] ^= 0x01
open(p, 'wb').write(bytes(b))
print('flipped one byte in', p)
PY
python "$D" "$TMP/a" "$TMP/b" --label "REFUTER 1: one flipped byte"
echo "   (must be non-zero above) rc=$?"

rm -rf "$TMP/b"; cp -r "$O/serial_sanctuary" "$TMP/b"
W=$(find "$TMP/b" -name "*.BTO" | head -1); rm -f "$W"
echo "deleted $W"
python "$D" "$TMP/a" "$TMP/b" --label "REFUTER 2: one file missing"
echo "   (must be non-zero above) rc=$?"
rm -rf "$TMP"

echo
echo "=== THE GATE ==="
python "$D" "$O/serial_sanctuary" "$ROOT/scratchpad/nifparse1_20260911/loop_r1_keep" \
	--label "Sanctuary: --chunk-threads 1 vs --chunk-threads 16"
echo "rc=$?"
python "$D" "$O/serial_boston" "$ROOT/scratchpad/nifparse1_20260911/loop_r2_keep" \
	--label "Boston: --chunk-threads 1 vs --chunk-threads 16"
echo "rc=$?"
