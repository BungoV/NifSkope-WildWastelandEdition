# Lane CARDWIDTH -- the width excess, separated

## A. The three arms, and whether they differ AT ALL

`dil` is `lodgenDilateFrames( alb, alb, ... )` -- img == coverage, so its
`put` writes the existing alpha back. `dds` is the shipped BC3 file's own top mip.

The FLOOR under the `dil` column is the SAME function called the other way:
`lodgenDilateFrames` writes the dilated alpha whenever `img != coverage` (the
normal, mask and emissive sheets), so running the model with `is_coverage=False`
moves thousands of alphas. A 0 in the coverage column is therefore a result and
not a function that never writes.

| tree | frame | texels a>=16 png | dil | dds | png vs dil (coverage) | png vs dil (FLOOR, aux path) | png vs dds |
|---|---|---|---|---|---|---|---|
| 0003a28b | 128x128 | 209793 | 209793 | 210294 | 0 | 838783 | 167254 |
| 0004a074 | 64x128 | 64709 | 64709 | 65125 | 0 | 459579 | 57343 |
| 00038599 | 16x64 | 13510 | 13510 | 13434 | 0 | 52026 | 7132 |

## B. The threshold sweep against the bake's OWN pass-one measurement

`ref` = `framefit`'s `maxDx` / `maxDy` converted to texels: the widest (tallest)
view's silhouette, measured by the bake at viewport resolution. `err` is the
card's half-extent minus it, in card texels -- positive = the card is WIDER.

### 0003a28b TreeHero01 -- frame 128x128, texel 17.659 x 17.659 units, ref halfW 59.41 texels, halfH 58.89 texels

| threshold | arm | widest frame | card halfW, texels | err x | tallest frame | card halfH | err y |
|---|---|---|---|---|---|---|---|
| 1 | png | (0,2) | 60.00 | +0.59 | (0,0) | 59.50 | +0.61 |
| 1 | dds | (0,2) | 60.00 | +0.59 | (0,0) | 59.50 | +0.61 |
| 8 | png | (6,5) | 60.00 | +0.59 | (0,0) | 59.50 | +0.61 |
| 8 | dds | (6,5) | 60.00 | +0.59 | (0,0) | 59.50 | +0.61 |
| 16 | png | (6,5) | 59.50 | +0.09 | (0,2) | 59.50 | +0.61 |
| 16 | dds | (6,5) | 59.50 | +0.09 | (0,2) | 59.50 | +0.61 |
| 24 | png | (0,1) | 58.50 | -0.91 | (0,2) | 59.50 | +0.61 |
| 24 | dds | (5,4) | 58.50 | -0.91 | (0,2) | 59.50 | +0.61 |
| 32 | png | (0,1) | 58.00 | -1.41 | (0,2) | 59.50 | +0.61 |
| 32 | dds | (0,1) | 58.00 | -1.41 | (0,2) | 59.50 | +0.61 |
| 48 | png | (0,1) | 58.00 | -1.41 | (0,1) | 59.00 | +0.11 |
| 48 | dds | (0,1) | 58.00 | -1.41 | (7,5) | 59.50 | +0.61 |
| 64 | png | (7,6) | 57.00 | -2.41 | (7,1) | 59.00 | +0.11 |
| 64 | dds | (0,2) | 56.00 | -3.41 | (7,2) | 59.00 | +0.11 |
| 96 | png | (0,1) | 55.50 | -3.91 | (1,0) | 58.50 | -0.39 |
| 96 | dds | (0,1) | 55.50 | -3.91 | (1,0) | 58.50 | -0.39 |
| 128 | png | (0,1) | 54.00 | -5.41 | (1,1) | 57.00 | -1.89 |
| 128 | dds | (0,1) | 54.00 | -5.41 | (0,0) | 56.50 | -2.39 |
| 160 | png | (6,5) | 54.00 | -5.41 | (7,3) | 56.50 | -2.39 |
| 160 | dds | (6,5) | 54.00 | -5.41 | (7,3) | 56.50 | -2.39 |
| 192 | png | (2,3) | 49.00 | -10.41 | (1,0) | 55.50 | -3.39 |
| 192 | dds | (2,3) | 49.00 | -10.41 | (1,0) | 55.50 | -3.39 |
| 224 | png | (1,2) | 46.50 | -12.91 | (7,2) | 55.50 | -3.39 |
| 224 | dds | (1,2) | 46.50 | -12.91 | (7,3) | 55.50 | -3.39 |

### 0004a074 TreeMapleForest2 -- frame 64x128, texel 13.913 x 13.913 units, ref halfW 27.26 texels, halfH 59.41 texels

| threshold | arm | widest frame | card halfW, texels | err x | tallest frame | card halfH | err y |
|---|---|---|---|---|---|---|---|
| 1 | png | (6,0) | 28.00 | +0.74 | (6,4) | 60.00 | +0.59 |
| 1 | dds | (6,0) | 28.00 | +0.74 | (6,4) | 60.00 | +0.59 |
| 8 | png | (6,0) | 28.00 | +0.74 | (6,4) | 60.00 | +0.59 |
| 8 | dds | (6,0) | 28.00 | +0.74 | (6,4) | 60.00 | +0.59 |
| 16 | png | (6,0) | 28.00 | +0.74 | (6,4) | 59.50 | +0.09 |
| 16 | dds | (6,0) | 28.00 | +0.74 | (6,4) | 59.50 | +0.09 |
| 24 | png | (6,0) | 28.00 | +0.74 | (6,4) | 59.50 | +0.09 |
| 24 | dds | (6,1) | 28.00 | +0.74 | (6,4) | 59.50 | +0.09 |
| 32 | png | (7,0) | 27.50 | +0.24 | (6,4) | 59.50 | +0.09 |
| 32 | dds | (7,0) | 27.50 | +0.24 | (6,4) | 59.50 | +0.09 |
| 48 | png | (7,0) | 27.50 | +0.24 | (6,4) | 59.50 | +0.09 |
| 48 | dds | (7,0) | 27.50 | +0.24 | (6,4) | 59.50 | +0.09 |
| 64 | png | (6,0) | 27.00 | -0.26 | (6,0) | 59.00 | -0.41 |
| 64 | dds | (6,0) | 27.00 | -0.26 | (6,0) | 59.00 | -0.41 |
| 96 | png | (6,0) | 26.50 | -0.76 | (4,0) | 58.50 | -0.91 |
| 96 | dds | (6,0) | 26.50 | -0.76 | (4,0) | 58.50 | -0.91 |
| 128 | png | (6,0) | 26.00 | -1.26 | (4,0) | 58.00 | -1.41 |
| 128 | dds | (6,0) | 26.00 | -1.26 | (4,0) | 58.00 | -1.41 |
| 160 | png | (6,0) | 25.50 | -1.76 | (7,2) | 58.00 | -1.41 |
| 160 | dds | (6,0) | 25.50 | -1.76 | (7,2) | 58.00 | -1.41 |
| 192 | png | (7,0) | 24.00 | -3.26 | (6,0) | 57.00 | -2.41 |
| 192 | dds | (7,0) | 24.00 | -3.26 | (7,0) | 57.50 | -1.91 |
| 224 | png | (1,6) | 22.00 | -5.26 | (4,0) | 56.00 | -3.41 |
| 224 | dds | (1,6) | 22.00 | -5.26 | (4,0) | 56.00 | -3.41 |

### 00038599 TreeBlasted01 -- frame 16x64, texel 15.515 x 15.515 units, ref halfW 4.27 texels, halfH 29.70 texels

| threshold | arm | widest frame | card halfW, texels | err x | tallest frame | card halfH | err y |
|---|---|---|---|---|---|---|---|
| 1 | png | (0,0) | 5.00 | +0.73 | (0,0) | 30.00 | +0.30 |
| 1 | dds | (0,0) | 5.00 | +0.73 | (0,0) | 30.00 | +0.30 |
| 8 | png | (0,0) | 5.00 | +0.73 | (0,0) | 30.00 | +0.30 |
| 8 | dds | (0,0) | 5.00 | +0.73 | (0,0) | 30.00 | +0.30 |
| 16 | png | (1,0) | 5.00 | +0.73 | (0,0) | 30.00 | +0.30 |
| 16 | dds | (1,0) | 5.00 | +0.73 | (0,0) | 30.00 | +0.30 |
| 24 | png | (1,1) | 5.00 | +0.73 | (0,0) | 30.00 | +0.30 |
| 24 | dds | (1,1) | 5.00 | +0.73 | (0,0) | 30.00 | +0.30 |
| 32 | png | (2,2) | 5.00 | +0.73 | (0,0) | 30.00 | +0.30 |
| 32 | dds | (2,2) | 5.00 | +0.73 | (0,0) | 30.00 | +0.30 |
| 48 | png | (3,3) | 5.00 | +0.73 | (0,0) | 30.00 | +0.30 |
| 48 | dds | (3,3) | 5.00 | +0.73 | (0,0) | 30.00 | +0.30 |
| 64 | png | (5,6) | 4.50 | +0.23 | (1,0) | 30.00 | +0.30 |
| 64 | dds | (5,6) | 4.50 | +0.23 | (0,0) | 30.00 | +0.30 |
| 96 | png | (0,0) | 4.00 | -0.27 | (0,0) | 29.50 | -0.20 |
| 96 | dds | (0,0) | 4.00 | -0.27 | (7,1) | 30.00 | +0.30 |
| 128 | png | (0,0) | 4.00 | -0.27 | (1,0) | 29.50 | -0.20 |
| 128 | dds | (0,0) | 4.00 | -0.27 | (1,0) | 29.50 | -0.20 |
| 160 | png | (0,0) | 4.00 | -0.27 | (7,0) | 29.50 | -0.20 |
| 160 | dds | (0,0) | 4.00 | -0.27 | (7,0) | 29.50 | -0.20 |
| 192 | png | (0,0) | 4.00 | -0.27 | (1,0) | 29.00 | -0.70 |
| 192 | dds | (1,0) | 4.00 | -0.27 | (1,0) | 29.00 | -0.70 |
| 224 | png | (1,1) | 4.00 | -0.27 | (5,0) | 29.00 | -0.70 |
| 224 | dds | (1,1) | 4.00 | -0.27 | (5,0) | 29.00 | -0.70 |

## C. The threshold at which the card's silhouette equals the source's

Linear interpolation of `err` through zero over the sweep above, per axis.

| tree | arm | axis | crossing threshold | err at 16 (the bake's floor) | err at 128 (the reader's test) |
|---|---|---|---|---|---|
| 0003a28b | png | x | 17 | +0.09 | -5.41 |
| 0003a28b | png | y | 71 | +0.61 | -1.89 |
| 0003a28b | dds | x | 17 | +0.09 | -5.41 |
| 0003a28b | dds | y | 71 | +0.61 | -2.39 |
| 0004a074 | png | x | 56 | +0.74 | -1.26 |
| 0004a074 | png | y | 51 | +0.09 | -1.41 |
| 0004a074 | dds | x | 56 | +0.74 | -1.26 |
| 0004a074 | dds | y | 51 | +0.09 | -1.41 |
| 00038599 | png | x | 78 | +0.73 | -0.27 |
| 00038599 | png | y | 83 | +0.30 | -0.20 |
| 00038599 | dds | x | 78 | +0.73 | -0.27 |
| 00038599 | dds | y | 115 | +0.30 | -0.20 |

## D. Control 1, known answer: a synthetic silhouette of known width

A rectangle of half-width `hw` viewport pixels put through the bake's own
downsample (a box average into the inner rect) and read back by the same
`frame_extent` at each threshold. The metric must report the rectangle's own
half-width, and must report the SAME excess at 16 that it reports on a tree's
solid trunk -- if it does not, the sweep is measuring the reader, not the bake.

| source half-width, viewport px | texels | err at 16 | err at 128 | crossing |
|---|---|---|---|---|
| 44.0 | 5.946 | +0.05 | +0.05 | none |
| 44.5 | 6.014 | +0.99 | -0.01 | 24 |
| 88.3 | 11.932 | +0.07 | +0.07 | none |
| 200.7 | 27.122 | +0.88 | -0.12 | 31 |
| 300.0 | 40.541 | +0.46 | +0.46 | 143 |

## E. Control 2, the EXTENT FLOOR: a card scaled 5% wide

Lane CARDORTHO B.5: the dx-extent column read the same value on the card arm
and its zeroed-offset control on 12 of 12 rows, so the 2% extent bar had
nothing under it. This is the floor: the same frame stretched to 105% of its
width. An extent check worth quoting must FAIL on it.

| tree | frame | true halfW texels (a>=128) | 5%-wide halfW | extent error | fails a 2% bar? |
|---|---|---|---|---|---|
| 0003a28b | (0,1) | 54.00 | 53.50 | 0.93% | NO -- floor does not fire |
| 0004a074 | (6,0) | 26.00 | 27.00 | 3.85% | YES |
| 00038599 | (0,0) | 4.00 | 4.50 | 12.50% | YES |

