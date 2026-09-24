#!/bin/bash
# LAND1 gate B8, second sweep. The ledger is written into the out-dir of EVERY
# lodgen bake, so the regression surface is every harness that compares two
# bake trees byte for byte -- not just the ones the feature's own gate names.
# `grep -c "byte-identical|cmp -s" tests/spells/*.sh` is how this list was made.
set -u
cd /e/Projects/NifskopeWildWastelandEdition
export PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
for s in lodgen_farring lodgen_texture_arrays lodgen_native_baseline lod_channel_preview; do
	f=tests/spells/$s.sh
	[ -f "$f" ] || { echo "== $s : NO SUCH HARNESS"; continue; }
	echo "== $s =="
	bash "$f" > scratchpad/land1_20260912/logs/hd_$s.txt 2>&1
	rc=$?
	tail -4 scratchpad/land1_20260912/logs/hd_$s.txt | grep -E "checks|RESULT"
	grep -c "lodb" scratchpad/land1_20260912/logs/hd_$s.txt | sed 's/^/   lodb mentions: /'
	echo "   rc=$rc  segfaults=$(grep -ci 'segmentation fault' scratchpad/land1_20260912/logs/hd_$s.txt)"
done
echo "sweep done $(date +%H:%M:%S)"
