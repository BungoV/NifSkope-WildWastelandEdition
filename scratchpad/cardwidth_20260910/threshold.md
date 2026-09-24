# Lane CARDWIDTH -- the threshold, decided inside the source render

## J. Conservative (16/255) against majority (0.5) -- the source only

The fine mask is the 941-px render's own silhouette. `pitch` is the card's texel
in fine pixels. The errors are HALF-extents in card texels: positive = the coarse
silhouette is wider than the source's.

| tree | view | pitch px | fine halfW px | ref texels | err x @16 | err y @16 | err x @128 | err y @128 |
|---|---|---|---|---|---|---|---|---|
| 0003a28b | front | 1.92 | 90.0 | 46.88 | +0.62 | +0.60 | +0.12 | +0.10 |
| 0003a28b | right | 1.92 | 105.5 | 54.95 | +0.05 | +0.34 | -0.95 | -0.16 |
| 0004a074 | front | 1.94 | 54.5 | 28.03 | +0.47 | +0.35 | -1.03 | +0.35 |
| 0004a074 | right | 1.94 | 40.0 | 20.57 | +0.43 | -0.14 | -2.07 | -0.14 |
| 00038599 | front | 4.12 | 19.0 | 4.61 | +0.39 | +0.30 | -0.11 | +0.30 |
| 00038599 | right | 4.12 | 18.0 | 4.37 | +0.63 | +0.30 | -0.37 | +0.30 |

**Worst absolute error over the twelve half-extents: 16/255 = 0.63 texels, 0.5 = 2.07 texels.**

## K. The 18-121%% width excess: what RESOLUTION alone buys, no card in it

Lane CARDORTHO B.3's trunk table read the widest row of the bottom fifth and of
the top fifth of the card against the same of the source, and got 18% to 121%.
Here the SAME statistic is taken between the fine source mask and the same mask
box-filtered to the card's texel pitch and thresholded -- the card is not in the
comparison, so whatever this shows is not the bake's.

| tree | view | band | fine widest row, px | coarse @16, px | excess | coarse @128, px | excess |
|---|---|---|---|---|---|---|---|
| 0003a28b | front | bottom fifth | 56 | 58 | +3% | 56 | -1% |
| 0003a28b | front | top fifth | 47 | 73 | +55% | 56 | +18% |
| 0003a28b | right | bottom fifth | 49 | 50 | +2% | 48 | -2% |
| 0003a28b | right | top fifth | 35 | 58 | +65% | 46 | +32% |
| 0004a074 | front | bottom fifth | 13 | 14 | +5% | 14 | +5% |
| 0004a074 | front | top fifth | 51 | 72 | +41% | 54 | +7% |
| 0004a074 | right | bottom fifth | 15 | 16 | +4% | 16 | +4% |
| 0004a074 | right | top fifth | 43 | 58 | +36% | 49 | +13% |
| 00038599 | front | bottom fifth | 33 | 37 | +12% | 33 | -0% |
| 00038599 | front | top fifth | 13 | 21 | +59% | 16 | +27% |
| 00038599 | right | bottom fifth | 36 | 41 | +15% | 33 | -8% |
| 00038599 | right | top fifth | 15 | 21 | +37% | 16 | +10% |

## L. Floor 1: pitch 1.0 -- the pipeline must add nothing

| tree | view | err x @16 | err y @16 | err x @128 | err y @128 |
|---|---|---|---|---|---|
| 0003a28b | front | +0.00 | +0.00 | +0.00 | +0.00 |
| 0003a28b | right | +0.00 | +0.00 | +0.00 | +0.00 |
| 0004a074 | front | +0.00 | +0.00 | +0.00 | +0.00 |
| 0004a074 | right | +0.00 | +0.00 | +0.00 | +0.00 |
| 00038599 | front | +0.00 | +0.00 | +0.00 | +0.00 |
| 00038599 | right | +0.00 | +0.00 | +0.00 | +0.00 |

## M. Floor 2: a disc of known radius through the same downsample

| radius px | pitch | ref texels | halfW @16 | err | halfW @128 | err |
|---|---|---|---|---|---|---|
| 60.0 | 2.0 | 30.00 | 30.00 | +0.00 | 30.00 | +0.00 |
| 60.0 | 7.4 | 8.11 | 9.00 | +0.89 | 8.00 | -0.11 |
| 137.5 | 2.0 | 68.75 | 69.00 | +0.25 | 69.00 | +0.25 |
| 137.5 | 7.4 | 18.58 | 19.00 | +0.42 | 19.00 | +0.42 |
| 300.0 | 2.0 | 150.00 | 150.00 | +0.00 | 150.00 | +0.00 |
| 300.0 | 7.4 | 40.54 | 41.00 | +0.46 | 41.00 | +0.46 |

