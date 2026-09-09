#!/bin/bash
# Regenerate ONE nested terrain region at every far level with the CURRENT exe.
# Nested on the winner of pick_handoff_region.py (dim-4 tile 28,24): each coarser
# level is the chunk that CONTAINS it, so all four pictures are the same ground.
#
#   4  -> cells 28..31 / 24..27
#   8  -> cells 24..31 / 24..31
#   16 -> cells 16..31 / 16..31
#   32 -> cells  0..31 /  0..31
#
# Every path handed to the exe is ABSOLUTE (a relative --out-dir resolves
# against release/, MISTAKES.md 2026-09-09).
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
GEN=$REPO/scratchpad/images_20260909/gen
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"

if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
if tasklist | grep -qi NifSkope.exe; then echo "REFUSED: a NifSkope is already running"; exit 2; fi

run() {   # run <dim> <x0> <y0> <x1> <y1>
  d=$1; x0=$2; y0=$3; x1=$4; y1=$5
  out=$GEN/ours$d
  tex=$out/textures/terrain/Commonwealth
  mkdir -p "$tex"
  echo "--- dim $d  cells $x0,$y0 .. $x1,$y1"
  "$REPO/release/NifSkope.exe" -no-gui lodgen "$ESM" \
    --worldspace 3C --terrain-region "$x0" "$y0" "$x1" "$y1" --dim "$d" \
    --no-terrain-identity \
    --out-dir "$out" --tex-dir "$tex" --data-root "$DATA" 2>&1 | tail -5
  echo "rc=${PIPESTATUS[0]}"
  ls -l "$out"/*.BTR "$tex"/*.DDS 2>/dev/null | awk '{print $5, $NF}'
}

run 4  28 24 31 27
run 8  24 24 31 31
run 16 16 16 31 31
run 32  0  0 31 31
echo GEN-DONE
