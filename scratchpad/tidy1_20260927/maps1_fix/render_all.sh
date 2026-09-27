#!/bin/bash
# MAPS1 render pass: every view the renderer has, one camera (08_roads_AO_decal: view 8 ortho, half-width 16384,
# look-at -4096,-24576,0, 1600x1624 read back), one exe (ao2/ns_x1). Skips a panel already on disk (resumable).
# usage: render_all.sh [group]   groups: lit flat plane water all (default all)
# LANE TIDY1 COPY (audit rank 9; the original in maps1 is untouched). Differences, and only these:
#   L06_ao_channel_lit is gone: WW_LODL_CHANNEL=ao IS WW_LODL_AO=1 (lodinative.cpp), so it was L00 byte for byte;
#   the W3 water loop no longer shoots height/waterheight/watertype/cellflags: the v3 bake repeats the v2 planes
#   (the P_ shots), so those four were P_height/P_waterheight/P_watertype/P_cellflags again.
#   O_sky_stream and T_msn_worldXYZ come from offline_objects.py / offline_terrain.py, not from here; the fixed
#   label_full.py beside this file skips them (and these six) and keeps the audit's numbers.
SP=/c/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad
M=$SP/maps1; P=$M/pics; mkdir -p $P
T=$SP/ao2/terr_x1
OD=$SP/ao2/reg_x7/mod/FO4CSLOD/Commonwealth
V3=$M/water/regenerated.lodl
port=42801
shot() { # <name> <extra env...>
  local name=$1; shift
  port=$((port+1))
  if [ -s $P/$name.png ]; then echo "SKIP $name"; return; fi
  env LODI_DIR=$OD LV=2 SLOT=0 SDIM=2 "$@" \
    bash $M/shot.sh $P/$name.png $T Commonwealth -5 -10 2 -3 8 16384 1600 1600 $port
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
  for c in sky ao selfao sway ground seed identity placement identityraw mask-r mask-g mask-b mask-a scrappable; do
    shot F_$c WW_RENDER_FLAT=1 WW_LODL_CHANNEL=$c
  done
fi
if [ $G = plane ] || [ $G = all ]; then
  for pl in height ao blend colour groundcover waterheight watertype cellflags cellrange overview bodyid flow shore; do
    shot P_$pl NOOBJ=1 WW_LODL_PLANE=$pl
  done
fi
if [ $G = water ] || [ $G = all ]; then
  for pl in bodyid flow shore; do
    shot W3_$pl NOOBJ=1 LODL=$V3 WW_LODL_PLANE=$pl
  done
fi
echo "RENDER PASS DONE $G"
