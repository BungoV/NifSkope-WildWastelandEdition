#!/bin/bash
# BUILD6: NATIVE0b gates on the 03:57:46 exe, then LANE 0 (the stock baseline bake).
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
OUT=scratchpad/build6_20260910; mkdir -p $OUT/logs $OUT/native
NS=release/NifSkope.exe
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
ABS=E:/Projects/NifskopeWildWastelandEdition/$OUT/native
DEC=tests/spells/lodgen_native_decode.py
run () { local label="$1"; shift; local secs="$1"; shift
  echo "### $label start $(date +%H:%M:%S)"
  timeout "$secs" "$@" > "$OUT/logs/$label.log" 2>&1
  echo "### $label rc=$?  $(date +%H:%M:%S)"
  grep -E "checks, [0-9]+ failures|^RESULT|^native|REFUSED|MISMATCH|identity|bake wall-clock|failures$|differ$|  ok   wrote|CHANGED|MISSING|NEW " "$OUT/logs/$label.log" | tail -8; echo; }
tasklist | grep -i -E "Fallout4|NifSkope"; echo "gamecheck rc=$? (must be 1)"
ls -la --time-style=full-iso $NS

# C1 the synthetic fixture through the exe, then the independent decoder against the written answers
mkdir -p $OUT/native/fixture
run c1_fixture 300 $NS -no-gui lodgen --native-fixture "$ABS/fixture"
ls -la $OUT/native/fixture
run c1_decode 300 python $DEC $OUT/native/fixture/Synthetic.lodo $OUT/native/fixture/Synthetic.lodi --expect $OUT/native/fixture/Synthetic.expect.txt
run c1_verify 300 $NS -no-gui lodgen --native-verify "$ABS/fixture/Synthetic.lodo" "$ABS/fixture/Synthetic.lodi"
cmp scratchpad/native0_20260910/fixture/Synthetic.lodo $OUT/native/fixture/Synthetic.lodo && echo "fixture .lodo byte-identical to the standalone tool's" || echo "fixture .lodo DIFFERS from the standalone tool's"
cmp scratchpad/native0_20260910/fixture/Synthetic.lodi $OUT/native/fixture/Synthetic.lodi && echo "fixture .lodi byte-identical to the standalone tool's" || echo "fixture .lodi DIFFERS from the standalone tool's"
echo

# C2/C3 the (0,0) 4x4-cell sample region at dim 4, the ring set at dim 8/16/32 (all --no-ao)
for dim in 4 8 16 32; do
  D=$OUT/native/d$dim; mkdir -p $D/obj $D/Native
  run c2_bake_d$dim 1800 $NS -no-gui lodgen "$ESM" --worldspace 3C --terrain-region 0 0 3 3 --dim $dim --no-ao --data-root "$DATA" --out-dir "$ABS/d$dim/obj" --native "$ABS/d$dim/Native"
  ls -la $D/Native $D/obj | grep -E "lodo|lodi|BTO|manifest" | awk '{print "   ", $5, $9}'
  run c3_verify_d$dim 600 $NS -no-gui lodgen --native-verify "$ABS/d$dim/Native/Commonwealth.lodo" "$ABS/d$dim/Native/Commonwealth.lodi"
  run c3_decode_d$dim 900 python $DEC $D/Native/Commonwealth.lodo $D/Native/Commonwealth.lodi --esm "$ESM" --worldspace 3C --chunk 0 0 $dim --manifest $D/obj/Commonwealth.$dim.0.0.BTO.manifest.txt
done

# LANE 0: the stock baseline off this named exe, timed; then selftest; then check
rm -f tests/baselines/PENDING.txt
echo "### baseline --write start $(date +%H:%M:%S)"; t0=$(date +%s)
bash tests/spells/lodgen_native_baseline.sh --write > $OUT/logs/baseline_write.log 2>&1; rc=$?; t1=$(date +%s)
echo "### baseline --write rc=$rc  wall $((t1 - t0)) s  $(date +%H:%M:%S)"
grep -E "bake wall-clock|ok|FAIL|RESULT" $OUT/logs/baseline_write.log | tail -6
head -7 tests/baselines/stock_baseline.sha256 2>/dev/null; wc -l tests/baselines/stock_baseline.sha256 2>/dev/null
echo
run l0_selftest 300 bash tests/spells/lodgen_native_baseline.sh --selftest
echo "### baseline --check start $(date +%H:%M:%S)"; t0=$(date +%s)
bash tests/spells/lodgen_native_baseline.sh --check > $OUT/logs/baseline_check.log 2>&1; rc=$?; t1=$(date +%s)
echo "### baseline --check rc=$rc  wall $((t1 - t0)) s  $(date +%H:%M:%S)"
grep -E "differ|ok|FAIL|RESULT|baseline exe" $OUT/logs/baseline_check.log | tail -8
echo "### NATIVE CHAIN DONE $(date +%H:%M:%S)"
