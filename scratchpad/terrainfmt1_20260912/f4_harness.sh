#!/bin/bash
# Gate F4: the nine-harness chain, at GROUND1's baselines.
#
# Run from GIT-BASH, not from the MSYS2 login shell: several of these spells
# shell out to `python`, and MSYS2's python has no numpy -- a missing module
# prints an EMPTY number where a measurement should be and reads exactly like a
# regression (lodl_open scored 23/1 that way, lane ROADS1).
#
# The exe under test and the rung are named in the log so no verdict can be
# quoted against the wrong binary.
set -u
cd /e/Projects/NifskopeWildWastelandEdition
echo "exe:  $(ls -l --time-style=+%Y-%m-%d_%H:%M:%S release/NifSkope.exe | awk '{print $6, $5" B"}')"
echo "rung: $(ls -l --time-style=+%Y-%m-%d_%H:%M:%S release/NifSkope.before_terrainfmt1.exe | awk '{print $6, $5" B"}')"
echo "python: $(which python)"
for h in lodgen_roads lodgen_native lodgen_native_baseline lodgen_terrain_vt \
         lodgen_ground_cover lod_generation lodl_open lodgen_terrain_pbrm animws; do
	echo "=== $h"
	timeout 1800 bash tests/spells/$h.sh > scratchpad/terrainfmt1_20260912/h_$h.log 2>&1
	rc=$?
	tail -6 scratchpad/terrainfmt1_20260912/h_$h.log | sed 's/^/    /'
	echo "    rc=$rc"
done
echo "=== CHAIN DONE"
