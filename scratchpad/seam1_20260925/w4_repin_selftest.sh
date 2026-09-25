#!/bin/bash
# The re-pinned G1 must still refuse: (1) any OTHER byte moved (row 270125 byte 15 +1), (2) a listed byte holding a
# third value (row 270124 byte 15 = 150). Mutated copies of w4/final (w4_mutate.py).
cd /e/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925; PY=C:/Users/bungo/AppData/Local/Programs/Python/Python39/python
for m in other third; do
  rm -rf w4/mut_$m; cp -r w4/final w4/mut_$m; $PY w4_mutate.py mut_$m $m
  PYTHONUTF8=1 $PY w4_gate.py w4/old w4/mut_$m > w4/gate_mut_$m.txt 2>&1
  echo "== mut_$m"; grep -E '^G1 (boston|sanc)|REFUSED|^G1 (GREEN|RED)' w4/gate_mut_$m.txt | cut -c1-160
  rm -rf w4/mut_$m
done
