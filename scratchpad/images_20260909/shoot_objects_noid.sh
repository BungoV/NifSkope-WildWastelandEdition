#!/bin/bash
# The --no-identity object chunks, on the SAME pinned cameras as shoot_objects.sh.
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
B=$REPO/scratchpad/images_20260909
NS=$REPO/release/NifSkope.exe
export WW_WINDOW_AT=1960,40
PORT=43900

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
  echo "rc=$?  ${out##*/}  $(ls -l "$out" 2>/dev/null | awk '{print $5}' || echo 'NO FILE') bytes"
}

shot "$B/gen/obj_ring0_noid/Commonwealth.4.-20.24.BTO" "$B/img/obj_ours_ring0_noid.png" -73400,106170,9800 24000
shot "$B/gen/obj_far16_noid/Commonwealth.16.16.16.BTO" "$B/img/obj_ours_far16_noid.png" 91800,99000,1000 78000
echo OBJ-NOID-SHOOT-DONE
