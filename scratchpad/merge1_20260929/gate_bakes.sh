#!/bin/bash
# MERGE1 Boston re-gate bakes, sequential, each takes the NifSkope turn (bake.sh). usage: gate_bakes.sh [rung] [off] [on]
set -u
D=/e/Projects/NifskopeWWE-night/scratchpad/merge1_20260929
RUNG=/e/Projects/NifskopeWWE-ground1/scratchpad/ground1_20260927/run_rung   # night-20260927 exe, sha1 da128947
G=$D/g; mkdir -p $G
for s in ${*:-rung off on}; do
  echo "== $s start $(date +%H:%M:%S)"
  case $s in
    rung) bash $D/bake.sh $RUNG $G/rung ;;
    off)  env WW_LODGEN_NO_VERTEX_GROUND=1 WW_LODGEN_KEEP_BLACK_EMISSIVE=1 WW_LODGEN_NO_LAYER_DEDUPE=1 LODL_ARGS=--no-water-bodies \
            bash $D/bake.sh $D/run $G/off --identity-join proximity --occluder-fit piece --no-stamp-normals --no-sky-objects --no-collapse-uniform ;;
    on)   mkdir -p $G/on; env WW_LODI_GROUP_DUMP="$(cygpath -am $G/on)/groupdump.txt" \
            bash $D/bake.sh $D/run $G/on --dump-object-ao "$(cygpath -am $G/on)/objh_on.bin" ;;
  esac
  echo "== $s end $(date +%H:%M:%S)"
done
