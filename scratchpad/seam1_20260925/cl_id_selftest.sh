#!/bin/bash
# The re-pinned lodgen_cardlink ID must still fail: another AO byte moved (other), an attributed byte
# holding a third value (third), one byte of a non-.lodo file (dds). Each on a copy of the kept new bake.
cd /e/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925
PY=C:/Users/bungo/AppData/Local/Programs/Python/Python39/python; K=cardlink_keep; C=$K/idn_mut; L=FO4CSLOD/Commonwealth/Commonwealth.lodo
for m in other third dds; do
  rm -rf $C; cp -r $K/idn_new $C
  if [ $m = dds ]; then f=$(cd $C && find . -name '*.dds' -o -name '*.DDS' -o -name '*.BTR' | head -1); printf '\x5a' | dd of="$C/$f" bs=1 seek=200 conv=notrunc 2>/dev/null; echo "planted byte 200 of $f"
  else $PY cl_mutate.py $C/$L $m; fi
  $PY ../../tests/spells/lodgen_cardlink_id.py $C $K/idn_rung > $K/o.txt; r=$?; tail -2 $K/o.txt; echo "$m rc=$r  (must be 1)"
done
rm -rf $C
