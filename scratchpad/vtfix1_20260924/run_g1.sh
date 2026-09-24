#!/bin/bash
# G1: whole-Commonwealth --vt bake (VTBAKE1's command) on the exe named by $1, into $2.
# usage: bash run_g1.sh <exe> <outdir-absolute-E:/...>
EXE="$1"; OUT="$2"
mkdir -p "$OUT"
echo "start $(date '+%Y-%m-%d %H:%M:%S') exe $(sha1sum "$EXE" | cut -c1-8)" > "$OUT.log"
s=$(date +%s)
"$EXE" -no-gui lodgen "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" --worldspace 3C \
	--vt "$OUT" --vt-height --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" >> "$OUT.log" 2>&1
rc=$?
e=$(date +%s)
echo "end $(date '+%Y-%m-%d %H:%M:%S') rc=$rc wall_s=$((e-s))" >> "$OUT.log"
