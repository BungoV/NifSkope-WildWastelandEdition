# Lane CARDWIDTH -- the coverage-preserving law, tried on the sheet bytes

## F. The law, per frame, on the widest and tallest frame of each tree

`ref` is `framefit`'s pass-one half-extent in texels (the source silhouette,
measured at 7.4x the card's resolution). `err` is the card's half-extent at the
READER's test minus it: negative = the tree shrinks at the transition.

| tree | axis | frame | ref texels | err before | k | err after | drawn area before | after | target |
|---|---|---|---|---|---|---|---|---|---|
| 0003a28b | x | (0,1) | 59.41 | -5.41 | 1.113 | -5.41 | 1639 | 1796 | 1792 |
| 0003a28b | y | (1,1) | 58.89 | -1.89 | 1.103 | -1.89 | 1724 | 1883 | 1872 |
| 0004a074 | x | (6,0) | 27.26 | -1.26 | 1.208 | -1.26 | 445 | 520 | 528 |
| 0004a074 | y | (4,0) | 59.41 | -1.41 | 1.208 | -0.91 | 433 | 542 | 547 |
| 00038599 | x | (0,0) | 4.27 | -0.27 | 1.008 | -0.27 | 190 | 193 | 191 |
| 00038599 | y | (1,0) | 29.70 | -0.20 | 1.049 | -0.20 | 186 | 188 | 193 |

## G. The law over ALL 64 frames of each tree: the widest frame at the reader's test

The number bungo's rule turns on is the SILHOUETTE the reader draws against the
source's own. Before and after, over every frame of the sheet.

| tree | half-extent, texels | ref | before | after | before, %% of ref | after |
|---|---|---|---|---|---|---|
| 0003a28b | widest halfW | 59.41 | 54.00 | 54.00 | -9.1% | -9.1% |
| 0003a28b | tallest halfH | 58.89 | 57.00 | 58.50 | -3.2% | -0.7% |
| 0003a28b | k over 64 frames | | | | min 1.08 median 1.11 max 1.17 | |
| 0004a074 | widest halfW | 27.26 | 26.00 | 26.50 | -4.6% | -2.8% |
| 0004a074 | tallest halfH | 59.41 | 58.00 | 58.50 | -2.4% | -1.5% |
| 0004a074 | k over 64 frames | | | | min 1.09 median 1.19 max 1.27 | |
| 00038599 | widest halfW | 4.27 | 4.00 | 4.00 | -6.4% | -6.4% |
| 00038599 | tallest halfH | 29.70 | 29.50 | 29.50 | -0.7% | -0.7% |
| 00038599 | k over 64 frames | | | | min 1.00 median 1.00 max 1.16 | |

## H. Floor 1: the law asked for a target 5% too small must MISS by 5%

| tree | frame | target x1.00 drawn/target | target x0.95 drawn/target x1.00 | law is target-sensitive? |
|---|---|---|---|---|
| 0003a28b | (0,1) | 1.0023 | 0.9476 | YES |
| 0004a074 | (6,0) | 0.9841 | 0.9501 | YES |
| 00038599 | (0,0) | 1.0116 | 0.9958 | NO -- refused |

## I. Floor 2: a SOLID silhouette must not move (the cube fixture's condition)

A filled rectangle box-filtered into a 120-texel inner rect: the integral and
the drawn area already agree, so k must come back 1.000 and the extent must not
move by a texel. A law that fattens a cube is refused.

| source half-width, px | ref texels | k | halfW before | after | moved? |
|---|---|---|---|---|---|
| 44.0 | 5.946 | 1.000 | 6.00 | 6.00 | no |
| 88.3 | 11.932 | 1.000 | 12.00 | 12.00 | no |
| 200.7 | 27.122 | 5.120 | 27.00 | 28.00 | YES -- +1.00 texels |
| 300.0 | 40.541 | 1.000 | 41.00 | 41.00 | no |
| 440.0 | 59.459 | 1.174 | 59.00 | 60.00 | YES -- +1.00 texels |

