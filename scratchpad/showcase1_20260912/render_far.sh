#!/bin/bash
# Lane SHOWCASE1 -- picture 3, the far rings.
# dim 16 chunk (-32,16): origin (-131072, 65536), 65536 units square, centre
# (-98304, 98304).  dim 32 chunk (-32,0): origin (-131072, 0), 131072 square,
# centre (-65536, 65536).  Both from the LOOK bakes (identity payload off) so
# the pixels are the baked sheets, not the object-index colours.
set -u
L="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912"
S="$L/shots"; export SIZE=1400x900
sh () { bash "$L/shot.sh" "$@"; }
F16="$L/out/farlook16/obj/Commonwealth.16.-32.16.BTO"
F32="$L/out/farlook32/obj/Commonwealth.32.-32.0.BTO"
sh "$S/p3_far16_top.png"  "$F16" "$L/res_farlook16" 1 "-98304,98304,9000"  34000 WW_RENDER_SS=1
sh "$S/p3_far16_obl.png"  "$F16" "$L/res_farlook16" 8 "-98304,98304,9000"  34000 WW_RENDER_SS=1
sh "$S/p3_far16_near.png" "$F16" "$L/res_farlook16" 8 "-77600,102290,8500"  4000 WW_RENDER_SS=1
sh "$S/p3_far32_top.png"  "$F32" "$L/res_farlook32" 1 "-65536,65536,9000"   68000 WW_RENDER_SS=1
sh "$S/p3_far32_obl.png"  "$F32" "$L/res_farlook32" 8 "-65536,65536,9000"   68000 WW_RENDER_SS=1
