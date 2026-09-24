#!/bin/bash
# RESUME3 gate N4's floor: the 38k-vert torso's PARSE time, measured headlessly.
#
# `-no-gui info <file>` builds a NifModel, loads it, tallies every block and
# exits. Process startup is constant, so the SAME measurement is taken on a
# 1.3 KB cube and subtracted: what is left is the parse of the big mesh, which
# is the number a lock on the item pool could spoil.
#
#   bash torso_time.sh <exe> <tag> [reps]
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
EXE="${1:?exe}"; TAG="${2:?tag}"; REPS="${3:-7}"
TORSO='E:\Projects\Fallout 4 Mods\mods\Fo76 Mega Pack\meshes\F76\actors\powerarmor\x01tesla\x01tesla_torso.nif'
TINY="$ROOT/release/ww_render_shot/cube_lod.nif"

ms () { date +%s%3N; }
med () { printf '%s\n' "$@" | sort -n | awk '{a[NR]=$1} END{print (NR%2)? a[(NR+1)/2] : int((a[NR/2]+a[NR/2+1])/2)}'; }

bigs=(); tinys=()
for i in $(seq 1 "$REPS"); do
  t0=$(ms); "$EXE" -no-gui info "$TORSO" > /dev/null 2>&1; t1=$(ms); bigs+=($((t1-t0)))
  t0=$(ms); "$EXE" -no-gui info "$TINY"  > /dev/null 2>&1; t1=$(ms); tinys+=($((t1-t0)))
done
MB=$(med "${bigs[@]}"); MT=$(med "${tinys[@]}")
echo "TORSO $TAG  torso ${bigs[*]}  median ${MB} ms"
echo "TORSO $TAG  cube  ${tinys[*]}  median ${MT} ms"
echo "TORSO $TAG  PARSE = $((MB - MT)) ms   (median torso - median cube, $REPS reps each, alternating)"
