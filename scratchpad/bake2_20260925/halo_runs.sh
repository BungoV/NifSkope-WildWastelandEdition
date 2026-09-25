#!/bin/bash
# BAKE2 halo + DLC grid, pre-war legs with the final exe (run3, 4638a958), sequential:
#   fix : pre-war fill ON  -> wsgate/prewar_fix ; halo_gate GREEN; .lodi/.lodo identical to the install
#   off : pre-war fill OFF -> wsgate/prewar_off_new ; every file identical to wsgate/prewar_off (exe 0d71d0d6) but .lodb
#   cw  : Commonwealth region -24 16 -9 31 fill ON, rung (run, 0d71d0d6) vs new: VT files identical (0 no-LAND cells)
set -u
L=E:/Projects/NifskopeWWE-bake2/scratchpad/bake2_20260925; cd $L
E3=E:/Projects/NifskopeWWE-bake2/release/NifSkope.exe   # exe 62412e83 (run3 copy of 4638a958 was blocked by Avast, rc 126; relinked)
E0=$L/run/release/NifSkope.exe
cmpdir() { local d=0 n=0; while IFS= read -r f; do n=$((n+1)); cmp -s "$1/$f" "$2/$f" || { d=$((d+1)); echo "  differs: $f"; }; done < <(cd "$1" && find . -type f | sort); echo "$3: $n files, $d differ"; }
rm -rf wsgate/prewar_fix wsgate/prewar_off_new wsgate/cwfill_rung wsgate/cwfill_new
EXE=$E3 FILL=on bash bake.sh 000A7FF4 -28 -12 2 25 wsgate/prewar_fix > /dev/null; echo "fix bake rc=$?"
python halo_gate.py land/sanct.bin wsgate/prewar_fix/mod/FO4CSLOD/SanctuaryHillsWorld SanctuaryHillsWorld -28 -12 2 25
grep -ao "vanillaFill .*" wsgate/prewar_fix/logs/chunks.log | cut -c1-400
N=wsgate/prewar_fix/mod/FO4CSLOD/SanctuaryHillsWorld; I="E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/SanctuaryHillsWorld"
for x in lodi lodo lodl; do cmp -s $N/SanctuaryHillsWorld.$x "$I/SanctuaryHillsWorld.$x" && echo "IDENTICAL .$x vs installed" || echo "DIFFER .$x vs installed"; done
cmpdir wsgate/prewar_off/mod wsgate/prewar_fix/mod "pre-war fill ON new vs fill OFF (for scale)" | tail -1
EXE=$E3 FILL=off bash bake.sh 000A7FF4 -28 -12 2 25 wsgate/prewar_off_new > /dev/null; echo "off bake rc=$?"
cmpdir wsgate/prewar_off/mod wsgate/prewar_off_new/mod "pre-war fill OFF, exe 0d71d0d6 vs 4638a958"
for e in rung new; do X=$E0; [ $e = new ] && X=$E3
  EXE=$X FILL=on DIM=4 bash bake.sh 3C -24 16 -9 31 wsgate/cwfill_$e > /dev/null; echo "cw $e rc=$?"; done
for f in wsgate/cwfill_rung/mod/FO4CSLOD/Commonwealth/Commonwealth.VT.*; do b=$(basename $f); cmp -s $f wsgate/cwfill_new/mod/FO4CSLOD/Commonwealth/$b && echo "IDENTICAL cw $b" || echo "DIFFER cw $b"; done
grep -ao "vanillaFill .*" wsgate/cwfill_new/logs/chunks.log | cut -c1-400
echo HALO RUNS DONE
