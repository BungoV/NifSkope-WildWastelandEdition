#!/bin/bash
# FLAT2: Nuka-World's north edge (installed bake: every dim-2 tile north of cell y 33 has all four sheets one value).
# Box -8 24 3 32 + the installed bake's --vt-fill-vanilla; the pyramid pads north past 33, into the one-value tiles.
cd /e/Projects/NifskopeWWE-flat2/scratchpad/flat2_20260927
export WS=0600290F REGION="-8 24 3 32" LEAN=1
bash bake.sh "$PWD/run_new/NifSkope.exe" "$PWD/bakes/nw_off" --vt-fill-vanilla --no-collapse-uniform > nw_off.out 2>&1; echo "nw_off $?" >> nw_off.out
bash bake.sh "$PWD/run_new/NifSkope.exe" "$PWD/bakes/nw_on" --vt-fill-vanilla > nw_on.out 2>&1; echo "nw_on $?" >> nw_on.out
bash bake.sh "$PWD/run_rung/NifSkope.exe" "$PWD/bakes/nw_rung" --vt-fill-vanilla > nw_rung.out 2>&1; echo "nw_rung $?" >> nw_rung.out
cat nw_off.out nw_on.out nw_rung.out | grep -v "waiting on"
