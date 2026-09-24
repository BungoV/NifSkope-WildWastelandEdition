#!/bin/sh
# IMPOSTOR16 N8 addendum -- the lit look-test runs (channel 20, light 45,45), one harness at a time.
R=E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostor16_20260923
M="WW_IMPOSTOR_CHANNEL=20 WW_IMPOSTOR_LIGHT=45,45"
run() { b=$1; s=$2; out=$3; sw=$4; shift 4
	BAKES=$b PORT=$((29800 + RANDOM % 150)) sh $R/orb.sh $R/run_mat $s $R/n8/$out/$s $sw $M "$@"; }
run $R/n8/bakes n8_2k lit3 az3
run $R/n8/bakes n8_2k lit3_F az3 WW_IMPOSTOR_CUT=strong
run $R/bakes    n16_2k lit3_F az3 WW_IMPOSTOR_CUT=strong
run $R/n8/bakes n8_2k lit3_A az3 WW_IMPOSTOR_CUT=mean
run $R/bakes    n16_2k lit3_A az3 WW_IMPOSTOR_CUT=mean
run $R/n8/bakes n8_2k lit az2
run $R/n8/bakes n8_2k lit_noao az2 WW_IMPOSTOR_AO=0
run $R/bakes    n4_512 lit_noao az2 WW_IMPOSTOR_AO=0
run $R/bakes    n16_1k lit_noao az2 WW_IMPOSTOR_AO=0
echo LITS-DONE
