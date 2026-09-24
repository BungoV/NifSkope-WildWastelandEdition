#!/bin/bash
# ROADS1's harness chain. Sequential: one NifSkope instance ever.
cd /e/Projects/NifskopeWildWastelandEdition
L=scratchpad/roads1_20260911/logs
mkdir -p "$L"
for s in lodgen_terrain.sh lodgen_terrain_vt.sh lodgen_ground_cover.sh lodgen_terrain_pbrm.sh lodgen_texture_arrays.sh lodgen_card_arrays.sh lodgen_native.sh lodl_open.sh ui_align.sh water_ui.sh; do
	echo "=== $s ==="
	bash "tests/spells/$s" > "$L/${s%.sh}.log" 2>&1
	echo "$s rc=$?  $(grep -aoE '[0-9]+ checks?, [0-9]+ (failures?|fail)' "$L/${s%.sh}.log" | tail -1)  $(grep -acE '^RESULT (PASS|FAIL)|^PASS|^FAIL' "$L/${s%.sh}.log")"
	tail -3 "$L/${s%.sh}.log" | sed 's/^/      /'
done
echo CHAIN-DONE
