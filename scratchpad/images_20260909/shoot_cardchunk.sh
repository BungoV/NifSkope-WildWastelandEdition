#!/bin/bash
# Picture (b): ONE card, as it stands in a real far chunk, from three directions,
# so the octahedral frame selection is visible.
#
# The chunk comes from gen_and_shoot_cardchunk.sh (dim 16, --impostors over this
# lane's library).  The card is picked from the chunk's own manifest -- the
# `C index cx cy cz halfW halfH N depthspan <lodm>` line -- so the framing is the
# card's real world position and not a guess.
#
# The three directions are the viewer axis views the bake's octahedral law puts
# EXACTLY on a frame vertex (cardframe.py): ViewLeft 3, ViewFront 5, ViewRight 4.
#
#   bash shoot_cardchunk.sh [dist]
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
B=$REPO/scratchpad/images_20260909
NS=$REPO/release/NifSkope.exe
BTO=$B/gen/cardchunk/Commonwealth.16.16.16.BTO
MAN=$BTO.manifest.txt
DIST=${1:-2200}
export WW_WINDOW_AT=1960,40
mkdir -p "$B/img"

if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
if tasklist | grep -qi NifSkope.exe; then echo "REFUSED: a NifSkope is already running"; exit 2; fi
[ -f "$MAN" ] || { echo "no manifest at $MAN"; exit 3; }

# The card with the LARGEST half-width, so the picture is of a card and not of a
# twig.  A `C` line's cx cy cz are an OFFSET from the PLACEMENT, not a world
# position -- the placement is the object row with the same index (columns
# `index base type x y z ...`) -- so the two are added here.  Framing on the
# offset alone puts the camera near the world origin, tens of thousands of units
# from the chunk, and photographs an empty frame.
read -r idx ox oy oz hw hh n span lodm <<< "$(
  awk '$1=="C" { print $2, $3, $4, $5, $6, $7, $8, $9, $10 }' "$MAN" \
  | sort -k5 -g -r | head -1 )"
read -r px py pz <<< "$( awk -v i="$idx" '$1==i && $2!="" && $1!="C" { print $4, $5, $6; exit }' "$MAN" )"
cx=$(python -c "print($px + $ox)")
cy=$(python -c "print($py + $oy)")
cz=$(python -c "print($pz + $oz)")
echo "card index $idx placement $px,$py,$pz + offset $ox,$oy,$oz = $cx,$cy,$cz"
echo "  half $hw x $hh  frames $n  depthspan $span"
echo "lodm $lodm"
echo "$idx $cx $cy $cz $hw $hh $n $span $lodm" > "$B/img/cardchunk_pick.txt"

PORT=44300
shot() {  # shot <view> <out>
  PORT=$((PORT + 1))
  WW_RENDER_SHOT="$2" WW_RENDER_SIZE=800x1000 WW_RENDER_VIEW="$1" \
  WW_RENDER_CENTER="$cx,$cy,$cz" WW_RENDER_DIST="$DIST" WW_RENDER_TIME=1 \
    timeout 600 "$NS" --port "$PORT" "$BTO" >/dev/null 2>&1
  echo "rc=$?  view $1  ${2##*/}  $(ls -l "$2" 2>/dev/null | awk '{print $5}' || echo 'NO FILE') bytes"
}
shot 3 "$B/img/cardchunk_left.png"
shot 5 "$B/img/cardchunk_front.png"
shot 4 "$B/img/cardchunk_right.png"
echo CARDCHUNK-SHOOT-DONE
