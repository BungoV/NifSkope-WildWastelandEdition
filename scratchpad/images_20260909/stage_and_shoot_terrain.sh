#!/bin/bash
# Stage each side of the terrain comparison as its own miniature data root
# (nifskope-ww-vanilla-compare step 2) and shoot both halves on ONE pinned
# camera (step 3).  One NifSkope at a time; second monitor; unused port each
# launch; absolute paths everywhere.
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
BASE=$REPO/scratchpad/images_20260909
GEN=$BASE/gen
IMG=$BASE/img
VANM="E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Terrain/Commonwealth"
VANT="E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth"
NS=$REPO/release/NifSkope.exe
export WW_WINDOW_AT=1960,40
mkdir -p "$IMG"
PORT=43400

gate() {
  if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
  if tasklist | grep -qi NifSkope.exe; then echo "REFUSED: a NifSkope is already running"; exit 2; fi
}

# shot <file> <out.png> <center> <dist> <channel|0>
shot() {
  local f=$1 out=$2 ctr=$3 dist=$4 ch=$5
  gate
  PORT=$((PORT + 1))
  env ${ch:+WW_LOD_CHANNEL=$ch} \
      WW_RENDER_SHOT="$out" WW_RENDER_SIZE=1400x900 WW_RENDER_VIEW=8 \
      WW_RENDER_CENTER="$ctr" WW_RENDER_DIST="$dist" WW_RENDER_TIME=1 \
      timeout 600 "$NS" --port "$PORT" "$f" >/dev/null 2>&1
  local rc=$?
  local sz
  sz=$(ls -l "$out" 2>/dev/null | awk '{print $5}')
  echo "rc=$rc  ${out##*/}  ${sz:-NO FILE} bytes"
}

stage() {   # stage <dim> <tile>
  local d=$1 t=$2
  local v=$GEN/van$d
  mkdir -p "$v/textures/terrain/Commonwealth"
  cp "$VANM/$t.BTR" "$v/"
  cp "$VANT/$t.DDS" "$VANT/${t}_msn.DDS" "$v/textures/terrain/Commonwealth/"
}

set -- "4 Commonwealth.4.28.24 8192,8192,0 14000" \
       "8 Commonwealth.8.24.24 16384,16384,0 28000" \
       "16 Commonwealth.16.16.16 32768,32768,0 55000" \
       "32 Commonwealth.32.0.0 65536,65536,0 111000"

for spec in "$@"; do
  read -r d t ctr dist <<< "$spec"
  stage "$d" "$t"
  shot "$GEN/van$d/${t}_nowater.BTR"  "$IMG/lit_van$d.png"  "$ctr" "$dist" ""
  shot "$GEN/ours$d/${t}_nowater.BTR" "$IMG/lit_ours$d.png" "$ctr" "$dist" ""
  shot "$GEN/van$d/${t}_nowater.BTR"  "$IMG/nrm_van$d.png"  "$ctr" "$dist" 8
  shot "$GEN/ours$d/${t}_nowater.BTR" "$IMG/nrm_ours$d.png" "$ctr" "$dist" 8
done

# the camera-pin control: the SAME file at two distances must give two files
shot "$GEN/van4/Commonwealth.4.28.24_nowater.BTR" "$IMG/ctl_dist14000.png" 8192,8192,0 14000 ""
shot "$GEN/van4/Commonwealth.4.28.24_nowater.BTR" "$IMG/ctl_dist22000.png" 8192,8192,0 22000 ""
echo SHOOT-DONE
