#!/bin/bash
D=/e/Projects/NifskopeWWE-terrlive1/scratchpad/terrlive1_20260929
B=$D/bakes
for spec in "rung run_rung" "full run_new --terrain-option full" "hybrid run_new --terrain-option hybrid" "dynamic run_new --terrain-option dynamic"; do
	set -- $spec; name=$1; run=$2; shift 2
	echo "== $name start $(date +%H:%M:%S)"
	bash $D/bake.sh $D/$run $B/$name "$@"
	echo "== $name end $(date +%H:%M:%S)"
done
echo "CHAIN DONE $(date +%H:%M:%S)"
