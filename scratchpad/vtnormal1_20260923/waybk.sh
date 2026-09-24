#!/bin/bash
# VTNORMAL1: lodgen_perf leg (c) with the SAME switches on both sides. The
# harness gives the exe under test "--library near --native-ladder" to match a
# rung that predates 2026-09-17; this lane's rung (add1bf84) already has the
# new defaults, so leg (c) compared two different bakes. Here: rung and new,
# default switches, --threads 1 --chunk-threads 1, region A, same out path.
L=/e/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923
W=$L/wb; WW=E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923/wb
R=/e/Projects/NifskopeWildWastelandEdition/release
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
REG="$(grep -m1 '^REGION_A=' /e/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_perf.sh | sed 's/^REGION_A=//; s/"//g; s/\${[A-Z_]*:-//; s/}$//')"
echo "region A: $REG"
rm -rf "$W"; mkdir -p "$W"
for side in rung new; do
	x=$R/NifSkope.exe; [ $side = rung ] && x=$R/NifSkope.before_vtnormal1.exe
	mkdir -p "$W/root/out" "$W/root/tex" "$W/root/nat"
	"$x" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REG --dim 4 --data-root "$DATA" \
		--out-dir "$WW/root/out" --tex-dir "$WW/root/tex" --native "$WW/root/nat" \
		--cover --roads --road-detail 1 --threads 1 --chunk-threads 1 > "$W/$side.log" 2>&1
	echo "$side rc=$?"
	mv "$W/root" "$W/$side"
done
n=$(find "$W/rung" -type f ! -name '*.lodb' | wc -l)
d=$(diff -rq -x '*.lodb' "$W/rung" "$W/new" | wc -l)
echo "waybk: $n file(s) compared (the .lodb excluded), $d differ"
diff -rq -x '*.lodb' "$W/rung" "$W/new" | head -5
