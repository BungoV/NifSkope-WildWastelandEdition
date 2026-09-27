#!/bin/bash
# FLAT2: the sea-edge region's three arms, run after the Boston chain has finished (never two bakes of ours at once).
# Box 32 -12 43 -1: the Commonwealth's LAND records end near x = 38 in these rows (installed bake, dim 2).
cd /e/Projects/NifskopeWWE-flat2/scratchpad/flat2_20260927
until grep -q "^on [0-9]" bakes_on.out 2>/dev/null; do sleep 20; done
export REGION="32 -12 43 -1" LEAN=1
bash bake.sh "$PWD/run_new/NifSkope.exe" "$PWD/bakes/sea_off" --no-collapse-uniform > sea_off.out 2>&1; echo "sea_off $?" >> sea_off.out
bash bake.sh "$PWD/run_new/NifSkope.exe" "$PWD/bakes/sea_on" > sea_on.out 2>&1; echo "sea_on $?" >> sea_on.out
bash bake.sh "$PWD/run_rung/NifSkope.exe" "$PWD/bakes/sea_rung" > sea_rung.out 2>&1; echo "sea_rung $?" >> sea_rung.out
cat sea_off.out sea_on.out sea_rung.out | grep -v "waiting on"
