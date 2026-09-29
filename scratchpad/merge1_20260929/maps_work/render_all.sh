#!/bin/bash
# MERGE1 render pass = lane TIDY1's copy of MAPS1 render_all.sh (maps1_fix/render_all.sh), pointed at the merged
# whole-Commonwealth bake. Same camera as MAPS1 (view 8 ortho, half-width 16384, cells -5 -10 .. 2 -3, 1600x1600).
# Differences from TIDY1's copy, and only these:
#   * terrain + objects come from the new bake ($CW), the exe is the merged run copy (shot.sh default);
#   * the W3 loop is gone: the bake's own .lodl is version 3 now, so P_bodyid/P_flow/P_shore ARE the v3 views;
#   * P_depth added (lane WATER1's depth view: water body height - ground, the only view that is not flat water);
#   * F_identityraw is F_placement-lowbyte (lane IDENT1 renamed the channel).
# Resumable: a picture already on disk is skipped. Delete a picture to re-shoot it.
# usage: CW=<bake>/mod/FO4CSLOD/Commonwealth render_all.sh [group]   groups: lit flat plane all (default all)
set -u
MW=/e/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/maps_work
CW=${CW:?set CW to the bake mod/FO4CSLOD/Commonwealth folder}
P=$MW/pics; mkdir -p $P
port=${PORTBASE:-47301}
shot() { # <name> <extra env...>
  local name=$1; shift
  port=$((port+1))
  if [ -s $P/$name.png ]; then echo "SKIP $name"; return; fi
  env LODI_DIR=$CW LV=2 SLOT=0 SDIM=2 "$@" \
    bash $MW/shot.sh $P/$name.png $CW Commonwealth -5 -10 2 -3 8 16384 1600 1600 $port
}
G=${1:-all}
if [ $G = lit ] || [ $G = all ]; then
  shot L00_lit_ref_aodecal WW_LODL_AO=1
  shot L01_lit_default
  shot L02_raw12_default WW_LOD_CHANNEL=12
  shot L03_normal WW_LODL_CHANNEL=normal WW_LOD_CHANNEL=12
  shot L04_emissive WW_LODL_CHANNEL=emissive WW_LOD_CHANNEL=12
  shot L05_lc8_viewnormal WW_LOD_CHANNEL=8
fi
if [ $G = flat ] || [ $G = all ]; then
  shot F00_flat_default WW_RENDER_FLAT=1
  for c in sky ao selfao sway ground seed identity placement placement-lowbyte mask-r mask-g mask-b mask-a scrappable; do
    shot F_$c WW_RENDER_FLAT=1 WW_LODL_CHANNEL=$c
  done
fi
if [ $G = plane ] || [ $G = all ]; then
  for pl in height ao blend colour groundcover waterheight watertype cellflags cellrange overview bodyid flow shore depth; do
    shot P_$pl NOOBJ=1 WW_LODL_PLANE=$pl
  done
fi
echo "RENDER PASS DONE $G"
