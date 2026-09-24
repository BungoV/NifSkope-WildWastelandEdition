#!/bin/bash
# ROADS3 gate F4 -- the harness chain at GRADE1's baselines, in order, each
# verdict printed with the exe timestamp beside it.
set -u
R=/e/Projects/NifskopeWildWastelandEdition
cd "$R" || exit 2
L=$R/scratchpad/roads3_20260911/logs
mkdir -p "$L"
echo "exe: $(ls -la --time-style=+%Y-%m-%d_%H:%M:%S release/NifSkope.exe | awk '{print $6, $5}')"
for h in lodgen_roads lodgen_terrain lodgen_terrain_vt lodgen_ground_cover \
         lodgen_terrain_pbrm lodgen_native lodl_open lod_generation; do
	if [ ! -f "tests/spells/$h.sh" ]; then echo "$h: NO SUCH HARNESS"; continue; fi
	S=$(date +%s)
	timeout 1800 bash "tests/spells/$h.sh" > "$L/f4_$h.txt" 2>&1
	rc=$?
	echo "$h rc=$rc $(( $(date +%s) - S ))s : $(grep -iE '^[0-9]+ (passed|pass)|passed.*failed|PASS[: ]|FAIL[: ]|[0-9]+/[0-9]+' "$L/f4_$h.txt" | tail -2 | tr '\n' ' ')"
done
