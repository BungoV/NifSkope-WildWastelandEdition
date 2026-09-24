#!/bin/bash
# LAND1 gate A8 -- the harness chain on the Part A exe, run through the MSYS2
# shell with the interpreter NAMED, because MSYS2's own python has no numpy and
# an empty number reads exactly like a regression (docs/MISTAKES.md, ROADS1).
set -u
cd /e/Projects/NifskopeWildWastelandEdition
export PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
for s in lodgen_terrain lod_generation lodgen_terrain_vt lodgen_ground_cover \
         lodgen_terrain_pbrm lodgen_native lodgen_roads lodl_open animws; do
	f=tests/spells/$s.sh
	[ -f "$f" ] || { echo "== $s : NO SUCH HARNESS"; continue; }
	echo "== $s =="
	bash "$f" > scratchpad/land1_20260912/logs/h_$s.txt 2>&1
	rc=$?
	tail -3 scratchpad/land1_20260912/logs/h_$s.txt | grep -E "checks|RESULT" 
	echo "   rc=$rc  segfaults=$(grep -ci 'segmentation fault' scratchpad/land1_20260912/logs/h_$s.txt)"
done
echo "chain done $(date +%H:%M:%S)"
