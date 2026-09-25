#!/bin/bash
# bungo 2026-09-25: "I needed a perspective screenshot, not top down" / "just like on the pic I gave you" (08_boston_oblique).
# Same camera as 08: view 8, 8x8-cell region (ortho half-width 16384), 1600x1600, look-at 1050 u below the region's
# mean ground (Boston: Z=0 over mean 1049.6; Sanctuary mean 7738.8 -> Z=6690). Region centred on cells -22,19..-17,24.
S=/e/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/shot.sh
D="/e/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"
W=/e/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/pics/ao/work
T=01_sanctuary_oblique
env WW_LODL_AO=1 bash $S $W/${T}_ao_x_diffuse.png "$D" -23 18 -16 25 8 43841 2 0 6690
env -u WW_LODL_AO -u WW_LODL_CHANNEL bash $S $W/${T}_diffuse.png "$D" -23 18 -16 25 8 43842 2 0 6690
env WW_LODL_CHANNEL=ao WW_RENDER_FLAT=1 bash $S $W/${T}_ao_only.png "$D" -23 18 -16 25 8 43843 2 0 6690
echo ALLDONE
