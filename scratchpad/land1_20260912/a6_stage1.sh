#!/bin/bash
# LAND1 Part A stage 1: the five rules, each at the default macro scale 1024,
# hex OFF except where the rule requires it, on the selection seven.
# Registered BEFORE the run: the amplitudes are fractions of ONE land-texture
# repeat (341.33 world units), because that is the length the repeat this lane
# is asked to break actually has -- 683 (two repeats) is what bungo called too
# strong.
set -u
S=/e/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/a6_sweep.sh
bash $S g_drag85    --land-guide drag:85
bash $S g_drag171   --land-guide drag:171
bash $S g_drag341   --land-guide drag:341
bash $S g_asp050    --land-guide aspect:0.5
bash $S g_asp100    --land-guide aspect:1.0
bash $S g_slw100    --land-guide slopewarp:1.0 --land-warp 341
bash $S g_flw100    --land-guide flatwarp:1.0  --land-warp 341
bash $S g_asphex    --land-guide aspecthex:1.0 --land-hex 256
bash $S g_hexonly   --land-hex 256
echo "stage1 done $(date +%H:%M:%S)"
