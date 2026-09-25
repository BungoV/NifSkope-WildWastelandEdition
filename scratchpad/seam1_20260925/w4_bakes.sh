#!/bin/bash
# W4 gate inputs (pre-registered 2026-09-25 before the build). usage: w4_bakes.sh <exe> <tag>
# Three --native bakes, vanilla Fallout4.esm + the unpacked data root, shipped defaults otherwise
# (no ladder, no near library), into scratchpad/seam1_20260925/w4/<tag>/ -- NEVER FO4CSLOD:
#   fx      the synthetic known-answer pair (--native-fixture): no colour anywhere, by construction
#   boston  cells -5,-10..2,-3 (the W4 census box): the Amphitheater and the blasted maples
#   sanc    the lodgen_native.sh region, cells -20,24..-9,35
# OLD = the BAKE1 run copy (27a7bb29), NEW = this worktree's build. Then: w4_gate.py <old> <new>
NS="$1"; TAG="$2"
O=E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/w4/$TAG
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
mkdir -p "$O/fx" "$O/boston/Native" "$O/sanc/Native"
"$NS" -no-gui lodgen "$ESM" --native-fixture "$O/fx" > "$O/fx.log" 2>&1; echo "fx rc=$?"
for r in "boston:-5 -10 2 -3" "sanc:-20 24 -9 35"; do
  k=${r%%:*}; REG=${r#*:}; t0=$(date +%s)
  # shellcheck disable=SC2086
  "$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $REG --dim 4 --data-root "$DATA" \
    --out-dir "$O/$k" --native "$O/$k/Native" > "$O/$k.log" 2>&1
  echo "$k rc=$? $(( $(date +%s) - t0 )) s $(ls "$O/$k/Native" | tr '\n' ' ')"
done
