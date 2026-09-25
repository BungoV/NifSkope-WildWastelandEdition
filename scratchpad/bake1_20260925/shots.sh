#!/bin/bash
# Step 7: the labelled chunk views, the overview and the contact sheet (sequential: one NifSkope at a time).
L=/e/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925
D="/e/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"
P=$L/pics; R=$P/work; mkdir -p $R
shot() { # name title x0 y0 x1 y1 view port lodl-level
	bash $L/shot.sh $R/$1.png "$D" $3 $4 $5 $6 $7 $8 ${9:-2}
	[ -s $R/$1.png ] && python $L/label.py $R/$1.png $P/$1.png "$2" $(( $5 - $3 + 1 ))
}
shot 01_sanctuary "Sanctuary Hills + Red Rocket, cells -22,19..-17,24 (top)" -22 19 -17 24 1 43721
shot 02_concord "Concord + Museum of Freedom, cells -17,14..-12,19 (top)" -17 14 -12 19 1 43722
shot 03_downtown_boston "Downtown Boston: Diamond City to Boston Common, cells -5,-10..2,-3 (top)" -5 -10 2 -3 1 43723
shot 04_glowing_sea_edge "Glowing Sea edge, cells -18,-24..-11,-17 (top)" -18 -24 -11 -17 1 43724
shot 05_bns_forest "BNS-dense forest (densest dim-4 chunks), cells -4,-28..3,-21 (top)" -4 -28 3 -21 1 43725
shot 06_grass_block "Grass-densest block, cells -24,-8..-17,-1 (top)" -24 -8 -17 -1 1 43726
shot 08_boston_oblique "Downtown Boston, cells -5,-10..2,-3 (oblique)" -5 -10 2 -3 8 43727
shot 09_overview "Playable Commonwealth with objects, cells -64,-48..31,47 (top)" -64 -48 31 47 1 43728 3
