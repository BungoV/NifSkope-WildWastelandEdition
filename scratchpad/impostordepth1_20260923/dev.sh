#!/bin/sh
# IMPOSTORDEPTH1 dev sweeps with NO build: run_dev = release exe 8d87c155 + release shaders, its
# impostor_oct.frag = res/shaders/impostor_oct.frag with the uniform depthSearchSteps pinned to a const.
#   sh dev.sh <N steps> <variant S|A|snap> <az|az20|az3|list:...> [extra ENV=VAL ...]
# coverage (ch 2) + mesh ch 8 unless CH is set (e.g. CH="WW_IMPOSTOR_CHANNEL=20 WW_IMPOSTOR_LIGHT=45,45").
R=E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostor16_20260923
L=E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostordepth1_20260923
N=$1; V=$2; SW=$3; shift 3
SRC=${SHADER:-E:/Projects/NifskopeWildWastelandEdition/res/shaders/impostor_oct.frag}
sed "s/^uniform int   depthSearchSteps;/const int depthSearchSteps = $N;/" "$SRC" > $L/run_dev/shaders/impostor_oct.frag
grep -q "^const int depthSearchSteps = $N;" $L/run_dev/shaders/impostor_oct.frag || { echo "PIN FAILED"; exit 1; }
if [ -n "${COVF:-}" ]; then sed -i "s/^uniform bool  coverageDecodedFilter;/const bool coverageDecodedFilter = true;/" $L/run_dev/shaders/impostor_oct.frag; grep -q "^const bool coverageDecodedFilter = true;" $L/run_dev/shaders/impostor_oct.frag || { echo "COVF PIN FAILED"; exit 1; }; fi
case $V in S) E="";; A) E="WW_IMPOSTOR_CUT=mean";; snap) E="WW_IMPOSTOR_BLEND=0";; esac
CH=${CH:-"WW_IMPOSTOR_CHANNEL=2 WW_IMPOSTOR_MESH_CHANNEL=8"}
OUT=${OUT:-$L/sw/$SW/n$N$V/n8_2k}
BAKES=${BK:-$R/n8/bakes} PORT=$((29800 + RANDOM % 150)) sh $R/orb.sh $L/run_dev n8_2k $OUT $SW $CH $E "$@"
