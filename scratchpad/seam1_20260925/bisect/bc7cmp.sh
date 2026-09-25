#!/bin/bash
# Compress the kept hero PNG set with each named exe and print the sha1 of its _oct_n.DDS.
# usage: bc7cmp.sh <tag>=<exe> ...
W=/e/Projects/NifskopeWWE-seam1; S=$W/scratchpad/seam1_20260925/bisect/bc7; SRC=$W/scratchpad/cardfix1_20260924/wind/hero/cards
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"; DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
for a in "$@"; do t=${a%%=*}; x=${a#*=}; O=$S/$t; rm -rf $O; mkdir -p $O/cards
  cp $SRC/*.png $SRC/000531b3.txt $O/cards/
  "$x" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao --impostors "$(cygpath -m $O/cards)" \
    --data-root "$DATA" -o "$(cygpath -m $O/chunk.bto)" > $O/lodgen.log 2>&1
  echo "$t rc=$? $(sha1sum < $O/cards/000531b3_oct_n.DDS | cut -c1-12) $(sha1sum < $O/cards/000531b3_oct_d.DDS | cut -c1-12)"
done
