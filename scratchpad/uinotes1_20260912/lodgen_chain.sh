#!/bin/bash
# Lane UINOTES1b -- ROADS3's gate F4 chain, re-run row for row on THIS lane's
# exe, so the lodgen side is measured and not assumed. Same harness order and
# the same eight names as scratchpad/roads3_20260911/r3_f4.sh; only the log
# directory moves, into this lane's own scratchpad.
set -u
R=/e/Projects/NifskopeWildWastelandEdition
cd "$R" || exit 2
L=$R/scratchpad/uinotes1_20260912/logs/lodgen
mkdir -p "$L"
mkdir "$R/scratchpad/uinotes1_20260912/.lock_lodgen" 2>/dev/null || { echo "REFUSED: a lodgen chain is already running"; exit 8; }
trap 'rmdir "$R/scratchpad/uinotes1_20260912/.lock_lodgen" 2>/dev/null' EXIT

echo "exe: $(ls -la --time-style=+%Y-%m-%d_%H:%M:%S release/NifSkope.exe | awk '{print $6, $5}')"
echo "start $(date +%H:%M:%S)"
for h in lodgen_roads lodgen_terrain lodgen_terrain_vt lodgen_ground_cover \
         lodgen_terrain_pbrm lodgen_native lodl_open lod_generation; do
	if [ ! -f "tests/spells/$h.sh" ]; then echo "$h: NO SUCH HARNESS"; continue; fi
	bad=$(tasklist | grep -icE "Fallout4\.exe")
	[ "$bad" = "0" ] || { echo "$h SKIPPED: Fallout4.exe is up"; continue; }
	bad=$(tasklist | grep -icE "NifSkope\.exe")
	[ "$bad" = "0" ] || { echo "$h SKIPPED: a NifSkope is already running"; continue; }
	S=$(date +%s)
	timeout 1800 bash "tests/spells/$h.sh" > "$L/f4_$h.txt" 2>&1
	rc=$?
	echo "$h rc=$rc $(( $(date +%s) - S ))s : $(grep -iE '^[0-9]+ (passed|pass)|passed.*failed|PASS[: ]|FAIL[: ]|[0-9]+/[0-9]+' "$L/f4_$h.txt" | tail -2 | tr '\n' ' ')"
done
echo "end $(date +%H:%M:%S)"
echo "left running: $(tasklist | grep -icE 'NifSkope\.exe') NifSkope process(es)"
