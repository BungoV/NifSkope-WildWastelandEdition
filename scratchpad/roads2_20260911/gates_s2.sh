#!/bin/bash
# ROADS2 gate S2 + the defaults-match gate, all on the NEW exe.
cd /e/Projects/NifskopeWildWastelandEdition
B=scratchpad/roads2_20260911/bake.sh
E=release/NifSkope.exe
date +%H:%M
bash $B $E v2_legacy -20 20 -17 23 --roads --roads-legacy
bash $B $E v2_off     -20 20 -17 23 --no-roads
bash $B $E v2_def     -20 20 -17 23 --roads
bash $B $E v2_def_x   -20 20 -17 23 --roads --road-composite max-z --road-detail 0 --no-road-raised --no-road-sidewalks
bash $B $E v2_sw      -20 20 -17 23 --roads --road-sidewalks
bash $B $E hw2_def    -8 8 -5 11 --roads
bash $B $E hw2_legacy -8 8 -5 11 --roads --roads-legacy
date +%H:%M
