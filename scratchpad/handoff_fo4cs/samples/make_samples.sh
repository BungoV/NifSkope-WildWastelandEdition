#!/bin/bash
#
# THE FO4CS SAMPLE SET -- one Commonwealth region, four far levels, everything
# the generator writes for it, under the FINAL FILE NAMES (bungo 2026-09-09).
#
# The region is the one containing cell (0,0): cells 0..3 x 0..3.  At each far
# level the sweep bakes every chunk TOUCHING that rectangle, which for a
# dim-aligned worldspace is exactly the one chunk that CONTAINS it -- so the
# four levels are four views of the same ground, nested:
#
#   dim  4  -> Commonwealth.4.0.0    cells  0..3  x  0..3
#   dim  8  -> Commonwealth.8.0.0    cells  0..7  x  0..7
#   dim 16  -> Commonwealth.16.0.0   cells  0..15 x  0..15
#   dim 32  -> Commonwealth.32.0.0   cells  0..31 x  0..31
#
# "The FO4CS profile" is scratchpad/handoff_fo4cs/README.md section 5's own
# sentence: objects + identity + --arrays + --impostors + --cover + --vt, with
# the card library baked first.  NOT --atlas: the atlas sheets are the stock
# engine's draw-call optimisation and the native target drops them (README
# section 1).  --slot-fallback at the far levels, because 0 of the far chunks'
# refs fill their own MNAM slot and the chunk is otherwise empty of them.
#
# The whole-worldspace land file is NOT copied here: it is 36 MB and it already
# exists at
#   E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl
#
# Every path handed to the exe is ABSOLUTE (a relative --out-dir resolves
# against release/, MISTAKES.md 2026-09-09).
#
#   bash make_samples.sh <cards dir>
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
NS=$REPO/release/NifSkope.exe
S=$REPO/scratchpad/handoff_fo4cs/samples
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
CARDS=${1:?cards dir}
# HALF_AUX=1 writes the normal, mask and emissive sheets at half of each side
# (`lodgen --card-half-aux`). OFF by default, which is the 2026-09-06 ruling: the
# base colour carries the coverage and is the silhouette, so it never divides.
# The sample set shipped 2026-09-09 was made WITHOUT it -- all four sheets full
# size -- and the MANIFEST says so.
HALFAUX=${HALF_AUX:+1}

if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
if tasklist | grep -qi NifSkope.exe; then echo "REFUSED: a NifSkope is already running"; exit 2; fi

chunks() {   # chunks <dim> [extra...]
  local d=$1; shift
  local out=$S/L$d
  local tex=$out/textures/terrain/Commonwealth
  rm -rf "$out"; mkdir -p "$tex"
  echo "=== dim $d  cells 0,0..3,3  extra: $*"
  "$NS" -no-gui lodgen "$ESM" --worldspace 3C \
    --terrain-region 0 0 3 3 --dim "$d" \
    --out-dir "$out" --tex-dir "$tex" --data-root "$DATA" \
    --arrays --impostors "$CARDS" --cover ${HALFAUX:+--card-half-aux} "$@" 2>&1 | tail -12
  echo "rc=${PIPESTATUS[0]}"
}

chunks 4
chunks 8
chunks 16 --slot-fallback
chunks 32 --slot-fallback

# The terrain virtual texture: one .lodt container per pyramid level under
# <MODFOLDER>/Terrain/, indexed by Commonwealth.VT.lodm.  --vt-height adds the
# fourth R16 sheet per tile (off by default, +133% on a tile) because a sample
# set that omits a sheet cannot be used to write a reader for it.
VT=$S/vt
rm -rf "$VT"; mkdir -p "$VT/Terrain" "$VT/tex/textures/terrain/Commonwealth"
echo "=== VT pyramid  cells 0,0..3,3"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region 0 0 3 3 \
  --vt "$VT" --vt-height --cover \
  --out-dir "$VT/tex" --tex-dir "$VT/tex/textures/terrain/Commonwealth" \
  --data-root "$DATA" 2>&1 | tail -20
echo "rc=${PIPESTATUS[0]}"
echo SAMPLES-DONE
