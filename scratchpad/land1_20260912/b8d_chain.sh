#!/bin/bash
# LAND1 gate B8, final sweep on the SHIPPING exe (09:32:37) -- the two harnesses
# this lane's fix touches, the frozen stock guard whose exclusion list it edits,
# the identity floor, and animws, whose harness and sources arrived from another
# lane mid-build and whose objects were stale until 09:31:37.
set -u
cd /e/Projects/NifskopeWildWastelandEdition
export PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
for s in lodgen_identity lodgen_roads lodgen_native lodgen_native_baseline animws; do
	echo "== $s =="
	bash tests/spells/$s.sh > scratchpad/land1_20260912/logs/he_$s.txt 2>&1
	rc=$?
	tail -4 scratchpad/land1_20260912/logs/he_$s.txt | grep -E "checks|RESULT|failures"
	echo "   rc=$rc  segfaults=$(grep -ci 'segmentation fault' scratchpad/land1_20260912/logs/he_$s.txt)"
done
echo "final sweep done $(date +%H:%M:%S)"
