#!/bin/bash
# Picture (b), second attempt: the cards standing IN a chunk, from three
# directions, plus one close-up on a single named card.
#
# WHAT WAS MEASURED FIRST, because it decides the shape of this picture:
# WW_RENDER_CENTER / WW_RENDER_DIST take on WW_RENDER_VIEW=8 (ViewUser) and DO
# NOT take on the axis views 3/5/4.  Control, same file, ViewUser, centre fixed:
# DIST=3000 -> img/ctl_bto_3000.png 8,723 B, DIST=60000 -> ctl_bto_60000.png
# 10,983 B (they differ, so the pin takes).  The same file on ViewLeft/Front/
# Right with the centre AND the distance both changed produced three files whose
# sizes did not move at all (11,277 / 11,598 / 10,801 twice over).  Stated as
# measured; no cause claimed.
#
# So the three directional panels are the WHOLE chunk from the three axis views
# the bake's octahedral law puts exactly on a frame vertex, rendered large and
# cropped, and the fourth panel is the close-up on the card the manifest names.
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
B=$REPO/scratchpad/images_20260909
NS=$REPO/release/NifSkope.exe
# The MANIFEST comes from the identity-on chunk (--no-identity writes none); the
# PICTURE is of the --no-identity twin, so the cards photograph in their textures
# and not in the identity channel's hashed colours. Both chunks are the same
# generation but for that one flag.
BTO=$B/gen/cardchunk_noid/Commonwealth.16.16.16.BTO
MAN=$B/gen/cardchunk/Commonwealth.16.16.16.BTO.manifest.txt
export WW_WINDOW_AT=1960,40
mkdir -p "$B/img"

if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
if tasklist | grep -qi NifSkope.exe; then echo "REFUSED: a NifSkope is already running"; exit 2; fi
[ -f "$MAN" ] || { echo "no manifest at $MAN"; exit 3; }

read -r idx ox oy oz hw hh n span lodm <<< "$(
  awk '$1=="C" { print $2, $3, $4, $5, $6, $7, $8, $9, $10 }' "$MAN" \
  | sort -k5 -g -r | head -1 )"
read -r px py pz <<< "$( awk -v i="$idx" '$1==i && $1!="C" { print $4, $5, $6; exit }' "$MAN" )"
cx=$(python -c "print($px + $ox)"); cy=$(python -c "print($py + $oy)"); cz=$(python -c "print($pz + $oz)")
base=$(awk -v i="$idx" '$1==i && $1!="C" { print $2; exit }' "$MAN")
echo "close-up card: manifest index $idx, base $base, world $cx,$cy,$cz, half $hw x $hh, frames $n, depthspan $span"
echo "lodm $lodm"
printf '%s\n' "idx $idx base $base world $cx $cy $cz half $hw $hh frames $n span $span lodm $lodm" \
  > "$B/img/cardchunk_pick.txt"

PORT=44500
shot() {  # shot <view> <out> <size> [center dist]
  PORT=$((PORT + 1))
  env WW_RENDER_SHOT="$2" WW_RENDER_SIZE="$3" WW_RENDER_VIEW="$1" WW_RENDER_TIME=1 \
      ${4:+WW_RENDER_CENTER=$4} ${5:+WW_RENDER_DIST=$5} \
    timeout 600 "$NS" --port "$PORT" "$BTO" >/dev/null 2>&1
  echo "rc=$?  view $1  ${2##*/}  $(ls -l "$2" 2>/dev/null | awk '{print $5}' || echo 'NO FILE') bytes"
}
shot 3 "$B/img/cc_left.png"  1600x1200
shot 5 "$B/img/cc_front.png" 1600x1200
shot 4 "$B/img/cc_right.png" 1600x1200
shot 8 "$B/img/cc_close.png" 1000x1200 "$cx,$cy,$cz" 3000
echo CARDCHUNK2-DONE
