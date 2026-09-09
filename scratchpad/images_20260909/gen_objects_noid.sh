#!/bin/bash
# The same two OBJECT chunks again, this time with --no-identity.
#
# WHY: our object chunks write the FO4CS identity channel into Vertex Colors by
# default (--no-identity turns it off), and this renderer MULTIPLIES vertex
# colour into the diffuse -- so a default chunk photographs in the identity's
# hashed per-object colours, not in its textures.  Vanilla's chunks carry no
# vertex colours at all, so a default-vs-vanilla picture compares a colour hash
# against a texture.  This is exactly why the terrain half of this lane passes
# --no-terrain-identity (IMAGES3), and the object half had never had the
# equivalent.
#
# Both versions are kept: the identity one is its own picture.
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
    --no-identity \
    --out-dir "$out" --tex-dir "$tex" --data-root "$DATA" "$@" 2>&1 | tail -6
  echo "rc=${PIPESTATUS[0]}"
  ls -l "$out"/*.BTO 2>/dev/null | awk '{print $5, $NF}'
}

run obj_ring0_noid 4  -20 24 -17 27
run obj_far16_noid 16  16 16  31 31 --slot-fallback
echo GEN-OBJ-NOID-DONE
