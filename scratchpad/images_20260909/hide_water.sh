#!/bin/bash
# Both sides' terrain chunks carry a WATER BSMultiBoundNode whose shape has no
# source texture, so NifSkope draws it as an opaque white sheet that covers the
# land.  Hide it SYMMETRICALLY -- the same one-field edit on both halves,
# NiAVObject Flags bit 0 (hidden), 14 -> 15 -- into a *_nowater.BTR beside the
# original, so the land is what the picture shows.  Nothing else is touched and
# the game data is never written.
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
NS=$REPO/release/NifSkope.exe
GEN=$REPO/scratchpad/images_20260909/gen

hide() {   # hide <btr>
  local f=$1
  local idx
  idx=$("$NS" -no-gui list "$f" 2>/dev/null | grep -n "BSMultiBoundNode 'WATER'" | sed 's/^\[\?//' )
  idx=$("$NS" -no-gui list "$f" 2>/dev/null | grep "BSMultiBoundNode 'WATER'" | sed 's/^\[\([0-9]*\)\].*/\1/')
  if [ -z "$idx" ]; then echo "no WATER node in $f"; return; fi
  local out="${f%.BTR}_nowater.BTR"
  "$NS" -no-gui set "$f" -b "$idx" -f "Flags" -v 15 -o "$out" 2>&1 | head -1
  echo "  $(basename "$out")  block $idx  $(ls -l "$out" 2>/dev/null | awk '{print $5}') bytes"
}

for spec in "4 Commonwealth.4.28.24" "8 Commonwealth.8.24.24" \
            "16 Commonwealth.16.16.16" "32 Commonwealth.32.0.0"; do
  read -r d t <<< "$spec"
  hide "$GEN/van$d/$t.BTR"
  hide "$GEN/ours$d/$t.BTR"
done
echo HIDE-DONE
