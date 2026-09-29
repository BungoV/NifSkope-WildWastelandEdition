#!/bin/bash
# after chain.sh: harness legs, then the map render pass on the whole-map bake
D=/e/Projects/NifskopeWWE-night/scratchpad/merge1_20260929
until grep -q "CHAIN DONE" $D/chain.log 2>/dev/null; do sleep 30; done
bash $D/spells.sh > $D/spells.log 2>&1
grep -q "whole end rc=0" $D/whole.log && CW=$D/stage/mod/FO4CSLOD/Commonwealth bash $D/maps_work/render_all.sh > $D/render_all.log 2>&1
echo "CHAIN2 DONE $(date +%H:%M:%S)"
