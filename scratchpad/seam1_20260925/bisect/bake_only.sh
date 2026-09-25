#!/bin/bash
# re-run only the W4 bake + byte readout for an exe already built by run.sh (it copies into release/ for the DLLs)
W=/e/Projects/NifskopeWWE-seam1; S=$W/scratchpad/seam1_20260925; SHA=$1
cp $W/release/NifSkope.exe $W/release/NifSkope_bis_$SHA.exe; sha1sum $W/release/NifSkope_bis_$SHA.exe | cut -c1-8
rm -rf $S/w4/bis_$SHA; cd $S; bash w4_bakes.sh $W/release/NifSkope_bis_$SHA.exe bis_$SHA
PYTHONUTF8=1 C:/Users/bungo/AppData/Local/Programs/Python/Python39/python w4_bytes.py old bis_$SHA
