#!/bin/bash
# GRASSCOL gate bakes: the Sanctuary dim-4 chunk and the -24,-8 chunk, his load order, cover ON, --grass-tint 1.0 so a
# full-cover texel's colour IS the tint Ttex. VT only, into this scratchpad (NEVER FO4CSLOD).
# usage: [TINT=0] grass_gate.sh <exe> <tag>   (tag <t>0 = the same exe at TINT=0, the control)   then   grass_gate.py grass/<tag>
NS="$1"; TAG="$2"
O=E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/grass/$TAG
P="E:/Projects/Fallout 4 Mods/profiles/Default"
for R in "sanc -20 20 -17 23" "g248 -24 -8 -21 -5"; do
  set -- $R; k=$1; shift
  mkdir -p "$O/$k/vt" "$O/$k/scr"
  "$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region "$@" --dim 4 --out-dir "$O/$k/scr" \
    --tex-dir "$O/$k/scr/textures" --vt "$O/$k/vt" --vt-height --vt-density 16 --cover --grass-tint ${TINT:-1.0} > "$O/$k/log.txt" 2>&1
  echo "$TAG/$k rc=$? $(ls "$O/$k/vt" | tr '\n' ' ')"
done
