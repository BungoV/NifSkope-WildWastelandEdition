#!/bin/bash
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
OUT=scratchpad/build6_20260910; NS=release/NifSkope.exe
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
ABS=E:/Projects/NifskopeWildWastelandEdition/$OUT/native
DEC=tests/spells/lodgen_native_decode.py
run () { local label="$1"; shift; local secs="$1"; shift
  echo "### $label start $(date +%H:%M:%S)"
  timeout "$secs" "$@" > "$OUT/logs/$label.log" 2>&1
  echo "### $label rc=$?  $(date +%H:%M:%S)"
  grep -E "checks, [0-9]+ failures|^RESULT|REFUSED|MISMATCH|identity|accepted|^native|error" "$OUT/logs/$label.log" | tail -6; echo; }
tasklist | grep -i -E "Fallout4|NifSkope"; echo "gamecheck rc=$? (must be 1)"
run c1_fixture 300 $NS -no-gui lodgen "$ESM" --native-fixture "$ABS/fixture"
ls -la $OUT/native/fixture | awk '{print "   ", $5, $9}'
run c1_decode 300 python $DEC $OUT/native/fixture/Synthetic.lodo $OUT/native/fixture/Synthetic.lodi --expect $OUT/native/fixture/Synthetic.expect.txt
run c1_verify 300 $NS -no-gui lodgen "$ESM" --native-verify "$ABS/fixture/Synthetic.lodo" "$ABS/fixture/Synthetic.lodi"
cmp scratchpad/native0_20260910/fixture/Synthetic.lodo $OUT/native/fixture/Synthetic.lodo && echo "fixture .lodo byte-identical to the standalone tool's" || echo "fixture .lodo DIFFERS from the standalone tool's"
cmp scratchpad/native0_20260910/fixture/Synthetic.lodi $OUT/native/fixture/Synthetic.lodi && echo "fixture .lodi byte-identical to the standalone tool's" || echo "fixture .lodi DIFFERS from the standalone tool's"
for dim in 4 8 16 32; do
  run c3_verify_d$dim 600 $NS -no-gui lodgen "$ESM" --native-verify "$ABS/d$dim/Native/Commonwealth.lodo" "$ABS/d$dim/Native/Commonwealth.lodi"
done
echo "### CLAMP2 gates 1+2 re-run on the 03:57:46 exe"
run r1_ringcontrol 600 bash scratchpad/clamp2_20260910/ringcontrol.sh
run r2_terrain_vt 1800 bash tests/spells/lodgen_terrain_vt.sh
grep -n "msn differs\|FAIL" $OUT/logs/r2_terrain_vt.log | head -5
echo "### CHAIN3 DONE $(date +%H:%M:%S)"
