#!/bin/bash
# TERRLIVE1 chain 7: DYNAMIC with the rule (VT-only path: writes the .lodr only), then a small region bake
# off and on (the .lodb row and the region .lodr).
D=/e/Projects/NifskopeWWE-terrlive1/scratchpad/terrlive1_20260929
cd $D
bash chain6.sh whole_dyn_rule --terrain-option dynamic --outside-paint rule
bash bake_small.sh $D/run_rule $D/bakes/small_off --terrain-option hybrid
bash bake_small.sh $D/run_rule $D/bakes/small_rule --terrain-option hybrid --outside-paint rule
date
