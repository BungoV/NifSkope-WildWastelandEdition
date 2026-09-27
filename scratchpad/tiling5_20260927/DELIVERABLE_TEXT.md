# TILING5 -- text for the ledgers (HANDOFF / WW_CHANGES / MISTAKES)

## WW_CHANGES
* lodgen: two new land switches, both OFF by default (bungo's call), off = the rung's bytes (measured, Boston box,
  27 of 27 sheets identical):
  * `--land-height-blend on` -- every land texture's relief is integrated out of its own normal map
    (Frankot-Chellappa, <= 256 texels, unit SD per mip); the hex joins weight their taps by exp(beta h), and between
    LTEX layers the texture DETAIL takes the height opacity sigma(logit a + beta dh) while the repeat averages keep
    the painted crossfade. beta = 2.0 (code constant `LODGEN_LAND_HEIGHT_BETA`).
  * `--land-macro on` -- a world-space brightness / hue / saturation field (58 / 234 / 936 m fBm), applied last.
    Its amplitudes are 0 by measurement (`LODGEN_MACRO_AMP`): vanilla's LOD sheets leave no room for it, so today it
    stores the same colour as off and only adds its ledger key.
  * `--land-sample relief` turns both on.
  * `.lodb` ledger: keys `land.heightBlend` and `land.macro`, written only when ON.
* No format change (no .lodt/.lodm/.BTR field touched). Nothing for the FO4CS reader.

## HANDOFF (top block)
TILING5 (branch tiling5-20260927): height-aware land blend + macro variation, both OFF. Height blend at beta 2.0
passes the grain gates (G1 +19.9 %, G2 7/7) and brings the transition zones' grain to vanilla's, but it brightens
every sheet by 0.2-3.9 of 255 (median +2.2), which reddens TILING4's band-shape gate on 6 of 14 sheets: selecting
by relief selects brighter texels. Proposed next step (not done): subtract the per-texture relief-weighted mean
bias. The macro measures to zero amplitude. Pictures and final arms were NOT made (see DONE.md C1).

## MISTAKES
* TILING5: the first height arm blended whole colours by relief and turned the two materials' mean difference into
  a per-texel dither (grain +115 % on one sheet). Lesson: any per-texel SELECTION between two samples must leave
  the samples' means alone -- split mean from detail first. The second-order form of the same mistake (relief
  correlates with brightness, so selecting by relief still moves the mean) was found only by measuring the mean
  shift directly; check the mean of an arm against today before reading any spectral gate.
