#!/bin/bash
# (B) runs on the INSTALLED bake (VT2 default), load-order model; then the planted refuter; then the old model.
cd /e/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925; PY=C:/Users/bungo/AppData/Local/Programs/Python/Python39/python
run() { echo "== $1"; shift; env PYTHONUTF8=1 "$@" 2>&1 | grep -E "painted set|B model|PLANTED|FILE GATE|A(file) vs"; }
run neLO3 $PY fill_model.py -4 20 16 36 512 ne
run neLO3P PLANT=auto $PY fill_model.py -4 20 16 36 512 ne
run nsancLO3 $PY fill_model.py -32 22 -10 34 512 nsanc
run nsancLO3P PLANT=auto $PY fill_model.py -32 22 -10 34 512 nsanc
run gseaLO3 $PY fill_model.py -44 -44 -24 -26 512 gsea
run gseaLO3P PLANT=auto $PY fill_model.py -44 -44 -24 -26 512 gsea
run neESM3 LAND=esm PAINTED=esm $PY fill_model.py -4 20 16 36 512 neESM
date +%H:%M:%S
