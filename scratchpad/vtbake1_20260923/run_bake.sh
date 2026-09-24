#!/bin/bash
cd /e/Projects/NifskopeWildWastelandEdition/scratchpad/vtbake1_20260923
echo "start $(date '+%Y-%m-%d %H:%M:%S')" > bake.log
s=$(date +%s)
./ns_run/NifSkope.exe -no-gui lodgen "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" --worldspace 3C --vt E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtbake1_20260923/bake --vt-height --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" >> bake.log 2>&1
rc=$?
e=$(date +%s)
echo "end $(date '+%Y-%m-%d %H:%M:%S') rc=$rc wall_s=$((e-s))" >> bake.log
