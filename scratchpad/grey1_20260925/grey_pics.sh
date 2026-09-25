#!/bin/bash
# GREY1: the A/B set, two framings (whole Boston window; a close downtown crop), same camera per set.
S=/e/Projects/NifskopeWWE-grey1/scratchpad/grey1_20260925
p=45110
for fr in "wide -2048 -26624 0 16384" "close -2048 -26624 0 4096"; do
  set -- $fr; F=$1; C="$2 $3 $4 $5"
  bash $S/grey_shot.sh ${F}_lit        $((p++)) $C
  bash $S/grey_shot.sh ${F}_lit_ao     $((p++)) $C WW_LODL_AO=1
  bash $S/grey_shot.sh ${F}_base       $((p++)) $C WW_LOD_CHANNEL=12
  bash $S/grey_shot.sh ${F}_ident      $((p++)) $C WW_LODL_CHANNEL=identity WW_RENDER_FLAT=1
  bash $S/grey_shot.sh ${F}_seed       $((p++)) $C WW_LODL_CHANNEL=seed WW_RENDER_FLAT=1
  bash $S/grey_shot.sh ${F}_lookdev    $((p++)) $C WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR=12 \
       WW_LOOKDEV_GROUND=0 WW_LOOKDEV_PLUGINS=Fallout4.esm "WW_LOOKDEV_DATA=X:/Programs/Steam/steamapps/common/Fallout 4/Data"
done
