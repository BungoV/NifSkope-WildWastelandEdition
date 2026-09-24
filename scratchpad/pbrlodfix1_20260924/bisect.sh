#!/bin/bash
cd /e/Projects/NifskopeWildWastelandEdition
D=scratchpad/pbrlodfix1_20260924
for r in "$@"; do
  EXE=release/NifSkope.before_$r.exe bash tests/spells/lodgen_terrain_pbrm.sh > $D/bisect_$r.txt 2>&1
  echo "$r: $(grep -a -E 'checks,' $D/bisect_$r.txt) $(grep -a -o 'maskPbrm [0-9]*' $D/bisect_$r.txt | head -1)" >> $D/bisect_summary.txt
done
echo BISECT_DONE >> $D/bisect_summary.txt
