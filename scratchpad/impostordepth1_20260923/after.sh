#!/bin/sh
# IMPOSTORDEPTH1 after pictures: material look (ch 20, light 45,45), az3 el0, run_dev + matsearch.frag.
L=E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth1_20260923
D=$L/../impostor16_20260923/n8/bakes
M="WW_IMPOSTOR_CHANNEL=20 WW_IMPOSTOR_LIGHT=45,45"
r() { SHADER=$L/matsearch.frag COVF=1 CH="$M" BK=$1 OUT=$L/after/$2/n8_2k sh $L/dev.sh $3 $4 az3 2>&1 | tail -1 | cut -c1-60; }
r $L/bakes_raw raw_snap 0 snap
r $L/bakes_raw raw_a16 16 A
r $L/bakes_raw raw_s16 16 S
r $D dxt_snap 0 snap
r $D dxt_a16 16 A
r $D dxt_s16 16 S
echo AFTER-DONE
