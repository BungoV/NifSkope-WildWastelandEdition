#!/bin/bash
# Picture (c) of the brief: the SOURCE tree model beside its card, same angle.
#
# The source is rendered by the same renderer, from the three viewer axis views
# that the bake's octahedral law puts EXACTLY on a frame vertex (cardframe.py):
#   ViewLeft  WW_RENDER_VIEW=3  ->  frame (0, 0)
#   ViewFront WW_RENDER_VIEW=5  ->  frame (0, N-1)
#   ViewRight WW_RENDER_VIEW=4  ->  frame (N-1, N-1)
#
# Before rendering, the model's IN-MESH LOD ranges are zeroed the way the bake
# zeroes them (src/nifskope_ui.cpp:21487): `LOD1 Size` and `LOD2 Size` to 0 on
# every BSMeshLODTriShape, so the source picture draws the same triangles the
# card was photographed from and not the branch-card steps stacked on top. Done
# through the CLI on a COPY; nothing in the game folder is written.
#
#   bash shoot_source_vs_card.sh <cards dir> <formid> <model path in meshes\>
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
B=$REPO/scratchpad/images_20260909
NS=$REPO/release/NifSkope.exe
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
CARDS=${1:?cards dir}; FID=${2:?formid}; MODEL=${3:?model}
export WW_WINDOW_AT=1960,40
mkdir -p "$B/img" "$B/work"

if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
if tasklist | grep -qi NifSkope.exe; then echo "REFUSED: a NifSkope is already running"; exit 2; fi

src="$DATA/meshes/${MODEL//\\//}"
[ -f "$src" ] || { echo "MISSING $src"; exit 3; }
cur="$B/work/${FID}_src.nif"
cp "$src" "$cur"

# zero every in-mesh LOD range, one CLI call per block that has one
blocks=$("$NS" -no-gui list "$cur" -t BSMeshLODTriShape 2>/dev/null | tr -d '\r' \
         | grep -oE '^ *[0-9]+' | tr -d ' ')
echo "BSMeshLODTriShape blocks: ${blocks:-none}"
for b in $blocks; do
  for f in "LOD1 Size" "LOD2 Size"; do
    v=$("$NS" -no-gui get "$cur" -b "$b" -f "$f" 2>/dev/null | tr -d '\r')
    [ "${v:-0}" = "0" ] && continue
    "$NS" -no-gui set "$cur" -b "$b" -f "$f" -v 0 -o "$B/work/tmp.nif" >/dev/null 2>&1 \
      && mv "$B/work/tmp.nif" "$cur"
    echo "  block $b  $f  $v -> 0"
  done
done

PORT=44100
shot() {  # shot <view> <out>
  PORT=$((PORT + 1))
  WW_RENDER_SHOT="$2" WW_RENDER_SIZE=900x1100 WW_RENDER_VIEW="$1" WW_RENDER_TIME=1 \
    timeout 300 "$NS" --port "$PORT" "$cur" >/dev/null 2>&1
  echo "rc=$?  ${2##*/}  $(ls -l "$2" 2>/dev/null | awk '{print $5}' || echo 'NO FILE') bytes"
}
shot 3 "$B/img/src_${FID}_left.png"
shot 5 "$B/img/src_${FID}_front.png"
shot 4 "$B/img/src_${FID}_right.png"

N=$(awk '/^oct /{print $2}' "$CARDS/$FID.txt")
python "$B/cardvs.py" "$CARDS" "$FID" "$B/handoff_card_vs_source_${FID}.png" \
  "ViewLeft:0:0:$B/img/src_${FID}_left.png" \
  "ViewFront:0:$((N-1)):$B/img/src_${FID}_front.png" \
  "ViewRight:$((N-1)):$((N-1)):$B/img/src_${FID}_right.png"
echo SRC-VS-CARD-DONE
