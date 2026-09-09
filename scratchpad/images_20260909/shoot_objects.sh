#!/bin/bash
# The two object chunks, vanilla and ours, on one pinned camera each.
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
B=$REPO/scratchpad/images_20260909
NS=$REPO/release/NifSkope.exe
VO="E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Terrain/Commonwealth/Objects"
export WW_WINDOW_AT=1960,40
PORT=43800

gate() {
  if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
  if tasklist | grep -qi NifSkope.exe; then echo "REFUSED: a NifSkope is already running"; exit 2; fi
}

shot() {  # shot <file> <out> <center> <dist>
  local f=$1 out=$2 ctr=$3 dist=$4
  gate
  PORT=$((PORT + 1))
  WW_RENDER_SHOT="$out" WW_RENDER_SIZE=1400x900 WW_RENDER_VIEW=8 \
  WW_RENDER_CENTER="$ctr" WW_RENDER_DIST="$dist" WW_RENDER_TIME=1 \
    timeout 900 "$NS" --port "$PORT" "$f" >/dev/null 2>&1
  local rc=$?
  echo "rc=$rc  ${out##*/}  $(ls -l "$out" 2>/dev/null | awk '{print $5}' || echo NO FILE) bytes"
}

shot "$VO/Commonwealth.4.-20.24.BTO"                    "$B/img/obj_van_ring0.png"  -73400,106170,9800   24000
shot "$B/gen/obj_ring0/Commonwealth.4.-20.24.BTO"       "$B/img/obj_ours_ring0.png" -73400,106170,9800   24000
shot "$VO/Commonwealth.16.16.16.BTO"                    "$B/img/obj_van_far16.png"  91800,99000,1000 78000
shot "$B/gen/obj_far16/Commonwealth.16.16.16.BTO"       "$B/img/obj_ours_far16.png" 91800,99000,1000 78000
echo OBJ-SHOOT-DONE
