#!/bin/bash
# MERGE1 second gate queue: TERR1's mask bakes (Boston, merged defaults) and FLAT2's sea-edge trio.
set -u
D=/e/Projects/NifskopeWWE-night/scratchpad/merge1_20260929
RUNG=/e/Projects/NifskopeWWE-ground1/scratchpad/ground1_20260927/run_rung
G=$D/g
BACK=( --identity-join proximity --occluder-fit piece --no-stamp-normals --no-sky-objects )
BACKENV=( WW_LODGEN_NO_VERTEX_GROUND=1 WW_LODGEN_KEEP_BLACK_EMISSIVE=1 WW_LODGEN_NO_LAYER_DEDUPE=1 )
for s in ${*:-noroads noflat sea_rung sea_off sea_on}; do
  echo "== $s start $(date +%H:%M:%S)"
  case $s in
    noroads) bash $D/bake.sh $D/run $G/noroads --no-roads ;;
    noflat)  bash $D/bake.sh $D/run $G/noflat --no-flat-objects ;;
    sea_rung) REGION="32 -12 43 -1" LEAN=1 bash $D/bake_flat2.sh $RUNG/NifSkope.exe $G/sea_rung ;;
    sea_off)  env "${BACKENV[@]}" REGION="32 -12 43 -1" LEAN=1 bash $D/bake_flat2.sh $D/run/NifSkope.exe $G/sea_off "${BACK[@]}" --no-collapse-uniform ;;
    sea_on)   env "${BACKENV[@]}" REGION="32 -12 43 -1" LEAN=1 bash $D/bake_flat2.sh $D/run/NifSkope.exe $G/sea_on "${BACK[@]}" ;;
  esac
  echo "== $s end $(date +%H:%M:%S)"
done
