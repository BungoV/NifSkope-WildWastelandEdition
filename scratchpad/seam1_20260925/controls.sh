#!/bin/bash
# SEAM1 step 2: control bakes of the Sanctuary block + a ring (cells -24,16..-13,27 = 3x3 dim-4 chunks), VT only,
# into this scratchpad (NEVER FO4CSLOD). usage: controls.sh <exe> <tag> [a b c]
#  a = his load order (MO2 profile), full VT settings as BAKE1 (--vt-density 16 --vt-height --cover)
#  b = Fallout4.esm alone, the unpacked vanilla data root
#  c = as a, with --cover off
# EXTRA="..." appends to every run (GRASSCOL: EXTRA="--grass-tint 0" keeps the grass-tint fix out of G2)
NS="$1"; TAG="$2"; shift 2
O=E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/ctl/$TAG
P="E:/Projects/Fallout 4 Mods/profiles/Default"
REG="-24 16 -13 27"
for k in "$@"; do
  mkdir -p "$O/$k/vt" "$O/$k/scr"
  t0=$(date +%s)
  case $k in
    a) "$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region $REG --dim 4 --out-dir "$O/$k/scr" --tex-dir "$O/$k/scr/textures" --vt "$O/$k/vt" --vt-height --vt-density 16 --cover $EXTRA > "$O/$k/log.txt" 2>&1 ;;
    b) "$NS" -no-gui lodgen "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" --worldspace 3C --terrain-region $REG --dim 4 --out-dir "$O/$k/scr" --tex-dir "$O/$k/scr/textures" --vt "$O/$k/vt" --vt-height --vt-density 16 --cover $EXTRA > "$O/$k/log.txt" 2>&1 ;;
    c) "$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region $REG --dim 4 --out-dir "$O/$k/scr" --tex-dir "$O/$k/scr/textures" --vt "$O/$k/vt" --vt-height --vt-density 16 $EXTRA > "$O/$k/log.txt" 2>&1 ;;
  esac
  echo "$TAG/$k rc=$? $(( $(date +%s) - t0 )) s $(ls "$O/$k/vt" | tr '\n' ' ')"
done
