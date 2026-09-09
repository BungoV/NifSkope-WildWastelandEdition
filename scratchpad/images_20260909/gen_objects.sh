#!/bin/bash
# The two OBJECT chunks, regenerated with the current exe.
#   ring 0  dim 4  chunk (-20,24)  Sanctuary -- vanilla ships this one
#   far     dim 16 chunk (16,16)   the same ground as the terrain pictures;
#           EMPTY without --slot-fallback (0 of its refs fill the ring-16 MNAM
#           slot), so the merged + simplified proxies only exist with it.
# Absolute paths on every exe argument (MISTAKES.md 2026-09-09).
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
GEN=$REPO/scratchpad/images_20260909/gen
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"

if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
if tasklist | grep -qi NifSkope.exe; then echo "REFUSED: a NifSkope is already running"; exit 2; fi

run() {   # run <outname> <dim> <x0> <y0> <x1> <y1> [extra...]
  local name=$1 d=$2 x0=$3 y0=$4 x1=$5 y1=$6; shift 6
  local out=$GEN/$name
  local tex=$out/textures/terrain/Commonwealth
  rm -rf "$out"; mkdir -p "$tex"
  echo "--- $name  dim $d  cells $x0,$y0 .. $x1,$y1  extra: $*"
  "$REPO/release/NifSkope.exe" -no-gui lodgen "$ESM" \
    --worldspace 3C --terrain-region "$x0" "$y0" "$x1" "$y1" --dim "$d" \
    --out-dir "$out" --tex-dir "$tex" --data-root "$DATA" "$@" 2>&1 | tail -6
  echo "rc=${PIPESTATUS[0]}"
  ls -l "$out"/*.BTO 2>/dev/null | awk '{print $5, $NF}'
}

run obj_ring0 4  -20 24 -17 27
run obj_far16 16  16 16  31 31 --slot-fallback
echo GEN-OBJ-DONE
