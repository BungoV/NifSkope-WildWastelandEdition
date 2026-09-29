#!/bin/bash
# TILING5 -- the four extra 12x12-cell mosaics for the macro licence (t5_band.py),
# today's default look (no new switch).  Takes and releases the turn lock itself.
#
#   usage: EXE=<exe> t5_mosaics.sh
set -u
HERE=/e/Projects/NifskopeWWE-tiling5/scratchpad/tiling5_20260927
T=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
EXE="${EXE:-$HERE/run_m3/NifSkope.exe}"
bash "$T" acquire tiling5 21600 || exit 1
for w in -20,16 -24,-28 -8,12 -20,-12; do
	IFS=, read -r X Y <<< "$w"
	bash "$HERE/t5_bake.sh" "$EXE" today "r:$X,$Y,$((X+11)),$((Y+11))" | head -1
done
bash "$T" release tiling5
echo "MOSAICS done $(date +%H:%M:%S)"
