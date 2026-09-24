#!/bin/bash
# GATEFIX1: re-bake the stock baseline set with ONE exe into a kept directory, so two
# rungs' outputs can be diffed file by file. usage: bake_pair.sh <exe-name-in-release> <outdir>
set -u
W=/e/Projects/NifskopeWWE-gatefix1
NS="$W/release/$1"; O="$2"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
OW="$(mkdir -p "$O" && cd "$O" && { pwd -W 2>/dev/null || pwd; })"
mkdir -p "$O/chunks" "$O/region"
b() { "$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects "$1" "$2" --dim "$3" --identity $4 \
	--data-root "$DATA" -o "$OW/chunks/Commonwealth.$3.$1.$2.bto" > "$O/bake_$1_$2_$3.log" 2>&1; echo "($1,$2)d$3 rc=$?"; }
b -20 24 4 ""
b -24 24 8 ""
b -32 16 16 "--slot-fallback"
b -32 0 32 ""
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 --identity \
	--arrays --atlas --data-root "$DATA" --out-dir "$OW/region" > "$O/bake_region.log" 2>&1; echo "region rc=$?"
