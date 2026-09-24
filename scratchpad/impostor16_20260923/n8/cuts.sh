#!/bin/sh
# IMPOSTOR16 N8 addendum -- the cut-variant coverage sweeps, one harness at a time.
# S = shipped stipple (no env), A = WW_IMPOSTOR_CUT=mean, F = WW_IMPOSTOR_CUT=strong,
# near = WW_IMPOSTOR_BLEND=0 (nearest single frame: the tear reference).
R=E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostor16_20260923
C="WW_IMPOSTOR_CHANNEL=2 WW_IMPOSTOR_MESH_CHANNEL=8"
run() { # set bakes sweep variant env...
	s=$1; b=$2; sw=$3; v=$4; shift 4
	BAKES=$b PORT=$((29800 + RANDOM % 150)) sh $R/orb.sh $R/run_mat $s $R/n8/cuts/$sw/$v/$s $sw $C "$@"
}
for sw in az az20; do
  for s in n8_2k n16_2k; do
    b=$R/bakes; [ $s = n8_2k ] && b=$R/n8/bakes
    [ "$sw$s" = azn16_2k ] || run $s $b $sw S
    run $s $b $sw A WW_IMPOSTOR_CUT=mean
    run $s $b $sw F WW_IMPOSTOR_CUT=strong
    run $s $b $sw near WW_IMPOSTOR_BLEND=0
  done
done
echo CUTS-DONE
