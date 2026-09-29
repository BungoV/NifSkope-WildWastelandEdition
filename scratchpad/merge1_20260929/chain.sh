#!/bin/bash
D=/e/Projects/NifskopeWWE-night/scratchpad/merge1_20260929
until grep -q "== on end" $D/gate_bakes.log 2>/dev/null; do sleep 20; done
bash $D/gate_bakes2.sh > $D/gate_bakes2.log 2>&1
bash $D/whole.sh > $D/whole.log 2>&1
echo "CHAIN DONE $(date +%H:%M:%S)"
