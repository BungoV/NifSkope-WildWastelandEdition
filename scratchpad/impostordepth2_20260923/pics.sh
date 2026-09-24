#!/bin/sh
# IMPOSTORDEPTH2 pictures: material look (ch 20, light 45,45), az 3-degree steps, el 0.
L=E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth2_20260923
R=E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostor16_20260923
M="WW_IMPOSTOR_CHANNEL=20 WW_IMPOSTOR_LIGHT=45,45"
o() { BAKES=$1 PORT=$((29800 + RANDOM % 150)) sh $R/orb.sh $2 $3 $L/pics/$4 az3 $M $5; }
o $L $L/run_mat n8_2k_bc7 bc7_blend0 WW_IMPOSTOR_BLEND=0
o $L $L/run_mat n8_2k_bc7 bc7_crisp
o $L $L/run_mat n8_2k_bc7 bc7_smooth WW_IMPOSTOR_SLIDER=1
o $R/n8/bakes $L/run_rung n8_2k rung_shipped
echo PICS-DONE
