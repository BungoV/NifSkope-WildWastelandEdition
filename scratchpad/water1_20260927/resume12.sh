#!/bin/bash
# WATER1 RESUME 2 then 1 (DONE.md): the depth-bake removal chunk gates, then the depth view pictures.
# One NifSkope run per turn (bake_btr.sh / shot.sh take and release WATER1 themselves). Stops at the first
# missing output; never relaunches. usage: resume12.sh [btr|depth|all]
ME=/e/Projects/NifskopeWWE-water1/scratchpad/water1_20260927
# one WATER1 pass at a time (2026-09-28: two chain4 copies ran at once): a second pass exits BUSY
if [ -z "${WATER1_PASS_HELD:-}" ]; then
  mkdir /e/Projects/NifskopeWWE-water1/scratchpad/water1_20260927/.water1_pass 2>/dev/null || { echo "BUSY: another WATER1 pass runs"; exit 3; }
  export WATER1_PASS_HELD=1; trap 'rmdir /e/Projects/NifskopeWWE-water1/scratchpad/water1_20260927/.water1_pass' EXIT
fi
cd $ME || exit 1
G=${1:-all}
stop() { echo "STOPPED: $1"; exit 2; }
if [ $G = btr ] || [ $G = all ]; then
  for pair in "run_nodepth bk_btr_nd" "run_new bk_btr_cur_id --terrain-identity" "run_nodepth bk_btr_nd_id --terrain-identity"; do
    set -- $pair; exe=$1; out=$2; shift 2
    if [ -n "$(find $ME/$out -iname '*.btr' 2>/dev/null | head -1)" ]; then echo "SKIP $out"; continue; fi
    echo "$(date +%H:%M:%S) bake $out ($exe $*)"
    bash $ME/bake_btr.sh $ME/$exe/NifSkope.exe $ME/$out "$@" | tail -2
    [ -n "$(find $ME/$out -iname '*.btr' 2>/dev/null | head -1)" ] || stop "no .btr in $out"
  done
  echo "== btr_cmp cur vs nd";       python $ME/btr_cmp.py $ME/bk_btr_cur $ME/bk_btr_nd | tail -6; echo "rc=${PIPESTATUS[0]}"
  echo "== btr_cmp --floor";         python $ME/btr_cmp.py $ME/bk_btr_cur $ME/bk_btr_nd --floor | tail -3; echo "rc=${PIPESTATUS[0]} (must be FAIL)"
  echo "== btr_cmp identity pair";   python $ME/btr_cmp.py $ME/bk_btr_cur_id $ME/bk_btr_nd_id | tail -6; echo "rc=${PIPESTATUS[0]}"
fi
if [ $G = depth ] || [ $G = all ]; then
  [ -f $ME/run_depth/NifSkope.exe ] || stop "run_depth/NifSkope.exe absent (still .paused?)"
  P=$(python -c "import json;print(json.load(open('agree_pick.json'))['probe'])")
  GR=$(python -c "print(';'.join('%d,%d'%(x,y) for x in range(-20480,12289,1024) for y in range(-40960,-8191,1024)))")
  WW_LODL_DEPTH_PROBE="$P;$GR" DEPTHNS=$ME/run_depth/NifSkope.exe PORTBASE=${PORTBASE:-46300} bash $ME/render_all.sh depth
fi
echo "RESUME12 END $G"
