# TILING5 -- text for the ledgers (HANDOFF / WW_CHANGES / MISTAKES)

## WW_CHANGES
* lodgen: two new land switches, both OFF by default (bungo's call), off = the rung's bytes (measured, Boston box,
  27 of 27 sheets identical, on BOTH final exes 34feb02e and 8026c602):
  * `--land-height-blend on` -- every land texture's relief is integrated out of its own normal map
    (Frankot-Chellappa, <= 256 texels, unit SD per mip); the hex joins weight their taps by exp(beta h), and between
    LTEX layers the texture DETAIL takes the height opacity sigma(logit a + beta dh) while the repeat averages keep
    the painted crossfade. beta = 2.0 (code constant `LODGEN_LAND_HEIGHT_BETA`). Mean-bias correction: per texture
    and relief level the colour-vs-relief slope G is measured once, and the relief-PREDICTED part of the colour
    (G h) blends with the painted weights, so choosing by relief no longer brightens the sheet
    (median shift +2.23 -> +0.009 of 255 over 14 chunks). Costs about +13 % of a Boston bake (100 s -> 113 s).
  * `--land-macro on` -- a world-space brightness / hue / saturation field (58 / 234 / 936 m fBm), applied last.
    Its amplitudes are 0 by measurement (`LODGEN_MACRO_AMP`): vanilla's LOD sheets leave no room for it, so today it
    stores the same colour as off and only adds its ledger key.
  * `--land-sample relief` turns both on.
  * `.lodb` ledger: keys `land.heightBlend` and `land.macro`, written only when ON.
* No format change (no .lodt/.lodm/.BTR field touched). Nothing for the FO4CS reader.

## HANDOFF (top block)
TILING5 (branch tiling5-20260927): height-aware land blend + macro variation, both OFF. The corrected height blend
(beta 2.0 + mean-bias correction) keeps every grain gate green (G1 +14.9 % / -7.7 %, G2 7/7 + 7/7, swirl 7/7) and no
longer shifts brightness, but it does NOT reach its own pre-registered bars: transitions 7 of 14 (= today),
TILING4 band shape 3/7 + 7/7 (4 reds, three of them within +0.012 of today), repeat 5/7 + 6/7 (= today). Most of
the transition gap is today's hex sampler's interior grain (6.70 vs vanilla 5.53), not the layer blend. The switch
changes little on screen (Boston flat colour: 41 % of pixels move, mean |d| 0.94 of 255). Recommendation:
keep OFF; the next lever is the interior grain, not beta. Pictures: `scratchpad/tiling5_20260927/pics/`.

## MISTAKES
* TILING5: the first height arm blended whole colours by relief and turned the two materials' mean difference into
  a per-texel dither (grain +115 % on one sheet). Lesson: any per-texel SELECTION between two samples must leave
  the samples' means alone -- split mean from detail first. The second-order form of the same mistake (relief
  correlates with brightness, so selecting by relief still moves the mean) was found only by measuring the mean
  shift directly; check the mean of an arm against today before reading any spectral gate.
* TILING5 2026-09-28: `turn.sh status` is not a verb -- anything but `release` ACQUIRES (as `anon`). Caught and
  stopped before it took the turn. Read the lock with `ls .ns_turn/who`.
* TILING5 2026-09-28: a picture script checked `[ -s out.png ]` with the previous run's PNG still on disk, so a
  failed run reported OK. Delete the output before each run. Also: a shot pointed at the wrong sheet folder draws
  the fallback view and the before/after come out byte-identical -- compare the pair before captioning.
