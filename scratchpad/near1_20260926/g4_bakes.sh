#!/bin/bash
# G4 (pre-registered): a far bake with the NEAR1 exe is byte-identical to the rung's, bar the version word.
# usage: g4_bakes.sh <exe> <tag>  -> scratchpad/near1_20260926/g4/<tag>/{fx,shw,boston}
#   fx      the synthetic known-answer pair (--native-fixture)
#   shw     SanctuaryHillsWorld A7FF4, cells -28,-12..2,25 (the SWAP1 real-worldspace leg)
#   boston  Commonwealth cells -4,-9..0,-7 (the NEAR1 test region)
# Vanilla Fallout4.esm + the unpacked data root, shipped defaults, --native into the scratchpad only.
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi
NS="$1"; TAG="$2"
O=E:/Projects/NifskopeWWE-near1/scratchpad/near1_20260926/g4/$TAG
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
mkdir -p "$O/fx" "$O/shw/Native" "$O/boston/Native"
"$NS" -no-gui lodgen "$ESM" --native-fixture "$O/fx" > "$O/fx.log" 2>&1; echo "fx rc=$?"
for r in "shw:A7FF4:-28 -12 2 25" "boston:3C:-4 -9 0 -7"; do
  k=${r%%:*}; rest=${r#*:}; WS=${rest%%:*}; REG=${rest#*:}; t0=$(date +%s)
  # shellcheck disable=SC2086
  "$NS" -no-gui lodgen "$ESM" --worldspace $WS --terrain-region $REG --dim 4 --data-root "$DATA" \
    --out-dir "$O/$k" --native "$O/$k/Native" > "$O/$k.log" 2>&1
  echo "$k rc=$? $(( $(date +%s) - t0 )) s"
done
