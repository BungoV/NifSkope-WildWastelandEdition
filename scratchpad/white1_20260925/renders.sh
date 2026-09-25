#!/bin/bash
# WHITE1 terrain-only whole-map renders, the frame of whole_top_after.png (view 1, ortho 400000, 3200x3224, cells -96..95)
D=/e/Projects/NifskopeWWE-white1/scratchpad/white1_20260925; P=$D/pics
OFF=/e/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/whole/off/vt/FO4CSLOD/Commonwealth
bash /e/Projects/NifskopeWWE-extent1/scratchpad/extent1_20260925/wait_turn.sh 1800
echo "$(date +%T) T_on (control: must match extent1 pics/top/T.png)"; NOOBJ=1 bash $D/shot.sh $P/T_on.png -96 -96 95 95 1 400000 3200 3224 45631
echo "$(date +%T) T_off (SEAM1 fill-OFF sheets)"; NOOBJ=1 SHEETS_DIR=$OFF bash $D/shot.sh $P/T_off.png -96 -96 95 95 1 400000 3200 3224 45631
echo "$(date +%T) T_on_c12 (installed, WW_LOD_CHANNEL=12 unlit base colour)"; NOOBJ=1 CHAN=12 bash $D/shot.sh $P/T_on_c12.png -96 -96 95 95 1 400000 3200 3224 45631
echo "$(date +%T) done"
