#!/bin/bash
# GATEFIX1: write the stock-baseline hash list with every rung exe, oldest first,
# so each moved file can be pinned to the lane whose rung it first moved on.
# Output: scratchpad/gatefix1_20260924/bl/<exe>.sha256, then a summary of what moved between neighbours.
W=/e/Projects/NifskopeWWE-gatefix1
D=$W/scratchpad/gatefix1_20260924/bl
mkdir -p $D
cd $W/release
for f in $(cat $W/scratchpad/gatefix1_20260924/rung_order.txt) NifSkope.before_gatefix1.exe; do
	n=${f%.exe}
	[ -s "$D/$n.sha256" ] && continue
	EXE=$W/release/$f BASE=$D/$n.sha256 bash $W/tests/spells/lodgen_native_baseline.sh --write > $D/$n.log 2>&1
	echo "$f rc=$? $(grep -c '  ' $D/$n.sha256 2>/dev/null)"
done
echo SWEEP-DONE
