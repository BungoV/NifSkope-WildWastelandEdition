#!/bin/bash
L=/e/Projects/NifskopeWWE-ident2/scratchpad/ident2_20260929; cd $L
bash pics_fp.sh
echo "== b_off_rung $(date +%H:%M:%S)"; bash bake.sh run_rung b_off_rung --identity-join proximity --occluder-fit piece
echo "== all done $(date +%H:%M:%S)"
