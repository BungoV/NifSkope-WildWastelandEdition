#!/bin/bash
# Lane GROUND1 Part B, gate F4: the harness chain, SEQUENTIALLY, one GUI
# instance at a time. Baselines are Part A's (report section A4).
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W="$ROOT/scratchpad/ground1_20260912/work/f4logs"
mkdir -p "$W"
cd "$ROOT"
for h in lodgen_roads lodgen_native lodgen_native_baseline lodgen_terrain_vt \
         lodgen_ground_cover lod_generation lodl_open lodgen_terrain_pbrm animws; do
	n=$(tasklist 2>/dev/null | grep -c "^NifSkope.exe")
	echo "=== $h  (NifSkope.exe instances before: $n) ==="
	start=$(date +%s)
	timeout 3600 bash "tests/spells/$h.sh" > "$W/$h.log" 2>&1
	rc=$?
	echo "  rc=$rc  $(( $(date +%s) - start ))s"
	grep -iE "^[0-9]+ checks|checks.*failure|PASS$|FAIL$|^(PASS|FAIL)\b" "$W/$h.log" | tail -4
	tail -3 "$W/$h.log"
done
echo "=== F4 chain done ==="
