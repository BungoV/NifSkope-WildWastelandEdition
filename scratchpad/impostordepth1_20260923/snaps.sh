#!/bin/sh
# IMPOSTORDEPTH1 job 1 -- SNAP renders: nearest single frame, no blend (WW_IMPOSTOR_BLEND=0),
# same exe/shaders (run_mat, c172ba9d + ch20 material), orbit (az3, el 0), light 45,45 as the IMPOSTOR16 GIFs.
R=E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostor16_20260923
L=E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth1_20260923
M="WW_IMPOSTOR_CHANNEL=20 WW_IMPOSTOR_LIGHT=45,45 WW_IMPOSTOR_BLEND=0"
BAKES=$R/n8/bakes PORT=$((29800 + RANDOM % 150)) sh $R/orb.sh $R/run_mat n8_2k $L/snap/n8_2k az3 $M
BAKES=$R/bakes    PORT=$((29800 + RANDOM % 150)) sh $R/orb.sh $R/run_mat n16_2k $L/snap/n16_2k az3 $M
echo SNAPS-DONE
