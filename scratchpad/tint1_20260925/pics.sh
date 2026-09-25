#!/bin/bash
# TINT1 pictures, one NifSkope at a time. usage: pics.sh <tag before|after> <lodi file>
T="$1"; LODI="$2"; S=/e/Projects/NifskopeWWE-tint1/scratchpad/tint1_20260925
P=E:/Projects/NifskopeWWE-tint1/scratchpad/tint1_20260925/pics; mkdir -p "$S/pics"
# 08_boston_oblique: BAKE1's camera exactly (cells -5,-10..2,-3, view 8, ortho 16384, centre = region centre, z 0, lodl 2)
bash $S/shot.sh $P/08_boston_oblique_$T.png "$LODI" -5 -10 2 -3 8 44611 2 -2048 -26624 0 16384
# the Amphitheater (one placement, 6856.8,-8607.7,576): 2x2 cells round it, oblique, half-width 3072
bash $S/shot.sh $P/amphitheater_$T.png "$LODI" 0 -4 2 -2 8 44612 1 6857 -8608 576 3072
# one blasted maple (TreeMapleblasted01_LOD_1, base 0004d93b at -32867.1,-46114.1,2155.2, dim-4 manifest): half-width 1024
bash $S/shot.sh $P/maple_$T.png "$LODI" -10 -13 -7 -10 8 44613 1 -32867 -46114 2155 1024
# the 8x8 window with the most hue placements (census: 350, cells -16,21..-9,28), oblique like 08
bash $S/shot.sh $P/hue_window_$T.png "$LODI" -16 21 -9 28 8 44614 2 -49152 102400 6690 16384
