#!/bin/bash
set -u
L="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912"
S="$L/shots"; export SIZE=1400x900
W="-77600,102290,8300"; C="4320,3986,8300"
sh () { bash "$L/shot.sh" "$@"; }
sh "$S/p5_bld_bto_ao_top.png" "$L/out/on/obj/Commonwealth.4.-20.24.BTO" "$L/res_on" 1 "$W" 700 WW_LOD_CHANNEL=3 WW_RENDER_SS=1
sh "$S/p5_bld_btr_ao_top.png" "$L/out/on/obj/Commonwealth.4.-20.24.BTR" "$L/res_on" 1 "$C" 700 WW_LOD_CHANNEL=3 WW_RENDER_SS=1
