#!/bin/bash
# Picture (b) of the brief: a CARD PLACED IN A CHUNK, from three view angles.
#
# The far ring is where a card actually stands in for a mesh: 0 of the ring-16
# chunk's refs fill their ring-16 MNAM slot, so without --impostors the chunk is
# empty of them. Generated here with --impostors over the card library baked in
# this lane, then framed on ONE card's own world position (the manifest's
# `C index cx cy cz halfW halfH N depthspan <lodm>` line) from three directions.
#
#   bash gen_and_shoot_cardchunk.sh <cards dir>
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
B=$REPO/scratchpad/images_20260909
NS=$REPO/release/NifSkope.exe
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
CARDS=${1:?cards dir}
OUT=$B/gen/cardchunk
export WW_WINDOW_AT=1960,40
mkdir -p "$B/img"

if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
if tasklist | grep -qi NifSkope.exe; then echo "REFUSED: a NifSkope is already running"; exit 2; fi

rm -rf "$OUT"; mkdir -p "$OUT/textures/terrain/Commonwealth"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region 16 16 31 31 --dim 16 \
  --out-dir "$OUT" --tex-dir "$OUT/textures/terrain/Commonwealth" --data-root "$DATA" \
  --impostors "$CARDS" 2>&1 | tail -8
echo "rc=${PIPESTATUS[0]}"

BTO="$OUT/Commonwealth.16.16.16.BTO"
MAN="$BTO.manifest.txt"
ls -l "$BTO" | awk '{print $5, $NF}'
echo "C lines (cards placed): $(grep -c '^C ' "$MAN" 2>/dev/null || echo 0)"
grep '^C ' "$MAN" 2>/dev/null | head -3
