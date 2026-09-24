#!/bin/bash
# Lane SHOWCASE1 -- the 11 buildings in chunk (-20,24), close up.
# Their manifest rows put them between x -77836..-77310 and y 102213..102378,
# z 8103..8231, so the cluster is ~530 x 165 x 130 units around
# world (-77600, 102290, 8300) = chunk-local (4320, 3986, 8300).
set -u
L="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912"
S="$L/shots"; export SIZE=1400x900
W="-77600,102290,8300"; C="4320,3986,8300"
sh () { bash "$L/shot.sh" "$@"; }
sh "$S/p4_bld_bto_top.png" "$L/out/look/obj/Commonwealth.4.-20.24.BTO" "$L/res_look" 1 "$W" 520 WW_RENDER_SS=1
sh "$S/p4_bld_btr_top.png" "$L/out/look/obj/Commonwealth.4.-20.24.BTR" "$L/res_look" 1 "$C" 520 WW_RENDER_SS=1
sh "$S/p4_bld_bto_obl.png" "$L/out/look/obj/Commonwealth.4.-20.24.BTO" "$L/res_look" 8 "$W" 520 WW_RENDER_SS=1
sh "$S/p4_bld_btr_obl.png" "$L/out/look/obj/Commonwealth.4.-20.24.BTR" "$L/res_look" 8 "$C" 520 WW_RENDER_SS=1
sh "$S/p5_bld_bto_ao.png"  "$L/out/on/obj/Commonwealth.4.-20.24.BTO"   "$L/res_on"   8 "$W" 520 WW_LOD_CHANNEL=3 WW_RENDER_SS=1
sh "$S/p5_bld_btr_ao.png"  "$L/out/on/obj/Commonwealth.4.-20.24.BTR"   "$L/res_on"   8 "$C" 520 WW_LOD_CHANNEL=3 WW_RENDER_SS=1
# A second, wider pass: half-width 520 puts a wall across the whole frame, which
# shows the texture but not the cluster.  1300 units holds all eleven buildings
# plus the trees around them.
sh "$S/p4_bld_bto_obl.png" "$L/out/look/obj/Commonwealth.4.-20.24.BTO" "$L/res_look" 8 "$W" 1300 WW_RENDER_SS=1
sh "$S/p4_bld_btr_obl.png" "$L/out/look/obj/Commonwealth.4.-20.24.BTR" "$L/res_look" 8 "$C" 1300 WW_RENDER_SS=1
sh "$S/p5_bld_bto_ao.png"  "$L/out/on/obj/Commonwealth.4.-20.24.BTO"   "$L/res_on"   8 "$W" 1300 WW_LOD_CHANNEL=3 WW_RENDER_SS=1
sh "$S/p5_bld_btr_ao.png"  "$L/out/on/obj/Commonwealth.4.-20.24.BTR"   "$L/res_on"   8 "$C" 1300 WW_LOD_CHANNEL=3 WW_RENDER_SS=1
