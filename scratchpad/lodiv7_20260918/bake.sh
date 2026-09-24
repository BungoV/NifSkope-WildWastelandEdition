#!/bin/bash
# lane LODIV7 -- one bake of chunk 4.4.-12 (cells 4..7 x -12..-9, dim 4).
# usage: bake.sh <exe> <outdir> [extra switches...]
# Every path is ABSOLUTE (root MISTAKES.md, "every lodgen invocation in a script
# takes ABSOLUTE paths"). The recipe is the urban_ao one narrowed to this chunk,
# with `--native` added because chunkD3's recipe emits no .lodi at all.
set -u
R=E:/Projects/NifskopeWildWastelandEdition
EXE="$1"; OUT="$2"; shift 2
if tasklist | grep -qi -E "Fallout4|NifSkope"; then echo "GAME OR NIFSKOPE UP -- stop"; exit 90; fi
mkdir -p "$OUT"
"$EXE" -no-gui lodgen \
  "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" \
  --worldspace 3C \
  --terrain-region 4 -12 7 -9 \
  --dim 4 \
  --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" \
  --out-dir "$OUT" \
  --tex-dir "$OUT/tex" \
  --native "$OUT/nat" \
  --cover --arrays \
  --road-detail 1 \
  "$@" > "$OUT/bake.log" 2>&1
echo "exit=$? out=$OUT"
