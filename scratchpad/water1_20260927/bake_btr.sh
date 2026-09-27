#!/bin/bash
# WATER1: terrain chunks (.btr) of the Boston box, for the water-depth agreement check.
# usage: bake_btr.sh <exe> <out dir> [extra lodgen args...]
EXE="$1"; OUT="$2"; shift 2
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
mkdir -p "$OUT"
wp() { echo "$1" | sed -E 's#^/([a-zA-Z])/#\U\1:/#'; }
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh acquire water1 21600 || exit 1
t0=$(date +%s)
"$EXE" -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --worldspace 3C \
    --terrain-region -5 -10 2 -3 --dim 4 --out-dir "$(wp "$OUT")" "$@" > "$OUT/bake.log" 2>&1
rc=$?
t1=$(date +%s)
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh release water1
echo "rc=$rc wall=$((t1-t0))s btr=$(find "$OUT" -iname '*.btr' | wc -l) files=$(find "$OUT" -type f | wc -l)"
