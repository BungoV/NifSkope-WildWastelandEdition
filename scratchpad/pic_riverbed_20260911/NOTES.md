# PIC-RIVERBED -- the grey spots in our riverbed, 2026-09-11

Read-only lane. Nothing built, nothing launched, nothing committed. Everything
written by this lane is under `scratchpad/pic_riverbed_20260911/`.

**bungo's question, pointing at a grey-spotted region of our chunk (-20,20)
colour sheet:** *"the riverbed, are the pebbles in the river this big? Or what
are those grey spots? Can you show me the vanilla diffuse texture used there?"*

**Deliverable:** `images/riverbed_texture.png`, 1106 x 2049.

---

## 1. The LTEX -> TXST -> diffuse chain

Resolved from `Fallout4.esm`
(`X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm`, 330,776,576 B)
by SPLAT1's `offline_bake.py`, whose ESM walk is
`tests/spells/lodgen_cover_model.py`'s `Esm` -- the same walk ROADS1 used.
Assets from `E:\Tools\Fallout 4\DataUnpacked\Data`. Full table in `logs/s2.log`
and `chain.json`; the layers that appear in the delivered window:

| EDID | LTEX | TXST | source | diffuse (`Data\Textures\...`) | size |
|---|---|---|---|---|---|
| `LRiverbedRocks02Wet` | `000BC13D` | `000BC13E` | TX00 | `Landscape\Ground\RiverbedRocks02Wet_d.dds` | 2048x2048, 12 mips, DXT5, 5,592,560 B |
| `LRiverbedRocks02WetGrass` | `000BC13F` | `000BC13E` | TX00 | the SAME file | " |
| `LRiverbedSilt01Wet` | `0004DDF3` | `0004DDF2` | TX00 | `Landscape\Ground\RiverbedSilt01Wet_d.dds` | 2048x2048, 12 mips, DXT5 |
| `LRiverbedSilt01` | `000227B7` | `000227B6` | TX00 | `Landscape\Ground\RiverbedSilt01_d.dds` | " |
| `LRubbleRock01` | `0001F78C` | `0001F78B` | TX00 | `Landscape\Ground\RubbleRock01_D.dds` | " |
| `LRubbleRock01Grass` | `00084765` | `0001F78B` | TX00 | the SAME file | " |
| `LRiverbedRocks01Grass` | `000BA522` | `000A7C4A` | TX00 | `Landscape\Ground\RiverbedRocks_d.dds` | " |
| `LDriedGrass01` | `0001D1D3` | `0001D1D2` | TX00 | `Landscape\Ground\DriedGrass01_D.dds` | " |

Two LTEXs in this window share one TXST and one `.dds`, so **"the dominant
texture" is grouped by the DIFFUSE FILE, not by the EDID**: separately
`LRiverbedRocks02Wet` is 31% and `LRiverbedRocks02WetGrass` 21%, and neither
alone is the largest share; together the file is **52.6%** of the window. The
first draft of this lane ranked by EDID and picked the wrong texture; that is
recorded here rather than silently corrected.

One LTEX painted elsewhere in the chunk, `LDebrisGround` (`0014EC5C`), resolves
through `MNAM -> Landscape\Ground\DebrisGroundTile.BGSM` and its diffuse is not
unpacked under `DataUnpacked`; it is 0.1% of the chunk and 0% of this window.
Named, not approximated -- the same limit SPLAT1 reported.

## 2. Where the riverbed is, and the window

`s1_locate.py` walks all 16 cells of chunk (-20,20) x 4 quadrants, reads the
base LTEX and the ATXT layer stack per quadrant, and resolves the over-chain
into a surviving weight per LTEX on the sheet's own 512x512 grid (the same
17x17 bilinear opacity blend `offline_bake.bake` does). Whole-sheet coverage:
`LRiverbedSilt01` 0.528, `LRubbleRock01` 0.110, `LDriedGrass01` 0.099,
`LRiverbedRocks02Wet` 0.065, `LForestFloor01` 0.062, ... -- **riverbed layers
total 0.647 of the chunk**, so most of this chunk is riverbed.

`s3_window.py` picks the crop. The rule was written down before the picture
(`ww-texel-picture` 1: pick by the metric, on the BEFORE artefact):

* **admissible** = riverbed LTEX weight >= 0.85 over the 128x128 window AND
  **zero** texels of ROADS1's road mask (`masks_m20_20.npz`) -- the road is a
  rasterised object, not ground paint, and a road stripe in the crop is a
  second thing to explain. 285 of 148,225 windows are admissible.
* **ranked by** the mean 3x3 local variance of luminance on OUR shipped sheet
  (`splatlib.local_var`, SPLAT1's own speckle instrument) -- the quantity the
  grey spots ARE.

Rejected candidates are printed in `logs/s3.log`: the best riverbed window
before the road rule was y=382 x=97 (local variance 121.50) and **11.0% of it
was road**, so it was dropped. The best window by variance ANYWHERE in the
sheet is that same y=382 x=97, i.e. the most speckled part of the whole chunk
is riverbed.

**Delivered window: y = 362..490, x = 136..264 of the 512x512 sheet.**
World box x -77,568 .. -73,472, y 82,624 .. 86,720 (4096 x 4096 world units,
32 units a texel). Riverbed weight 0.871, road texels 0 of 16,384.

Composition of that window, by diffuse file: `RiverbedRocks02Wet_d.dds` 52.6%,
`RiverbedSilt01Wet_d.dds` 18.7%, `RiverbedSilt01_d.dds` 9.4%,
`RubbleRock01_D.dds` 8.0%, `RiverbedRocks_d.dds` 6.4%, `DriedGrass01_D.dds`
4.9%.

Note on one number: the ranking table quotes 107.53 for this window (a box sum
over the whole-sheet local-variance map) and the picture quotes **106.85** (the
same statistic recomputed on the 128x128 crop alone, which has its own edges).
The picture's number is the one computed identically for every panel.

## 3. The panels, each described before any claim

**Panel 1, OURS.** 128 x 128 texels of
`scratchpad/roads1_20260911/out/after/tex/Commonwealth.4.-20.20.DDS` at 4x
nearest neighbour. A mid-brown ground covered edge to edge in pale grey-white
blotches two to six texels across, denser towards the left and bottom, with a
few darker brown patches between them. Local variance of luminance 106.8, mean
RGB 78.9 / 69.3 / 59.1.

**Panel 2, VANILLA.** The same 128 x 128 texels of Bethesda's shipped
`Commonwealth.4.-20.20.DDS`, same 4x. A smooth warm-brown ground with broad
soft light and dark patches tens of texels across, a pale blue-grey channel
running diagonally through the lower left, and no texel-scale spotting. Local
variance 29.0, mean RGB 91.1 / 83.3 / 72.3.

**Panel 3, the vanilla diffuse.** `Landscape\Ground\RiverbedRocks02Wet_d.dds`,
2048 x 2048, 12 mips, DXT5, 5,592,560 bytes, shown at 1/4 size. A bed of
rounded wet river pebbles, pale grey-green and white, packed across dark brown
wet silt, with the pebbles occupying roughly half the area. A yellow circle in
the lower right is one measured pebble at this panel's own scale.

**Panel 4, the other layers.** The five other diffuse files blended into the
same window, as thumbnails with their share: wet silt (19%), silt (9%), rubble
rock (8%), a second riverbed rock sheet (6%) and dried grass (5%). The two silt
sheets and the dried grass are fine brown mottle; `RubbleRock01_D` and
`RiverbedRocks_d` are pale grey gravel and would contribute grey spots of their
own at the same 6x, which is why the claim below rests on the correlation test
in section 4.3 and not on panel 3 being the only grey sheet in the window.

**Panel 5, the engine repeat.** Panel 3's texture alone, sampled onto the same
128 x 128 window at 32 world units a texel with the bake's own footprint-mip
rule, at a repeat of **341.333** world units. A near-uniform mid-brown with a
faint large-scale mottle and no spots at all. Selected mip 7.58; one repeat is
10.7 texels; local variance 8.1.

**Panel 6, the baked repeat.** The same texture, same window, same rule, at the
repeat our generator uses, **2048** world units. Dense pale grey-white spots
one to three texels across on dark brown, at the same scale and density as
panel 1's. Selected mip 5.00; one repeat is 64.0 texels; local variance 148.5.

Panels 5 and 6 are the ONE dominant texture, not the whole blend, so their
absolute colour is not panel 1's; only the texel-scale pattern is the claim.

## 4. The measurements behind the caption

### 4.1 How big a pebble is (`s4_pebble.py`, `logs/s4.log`)

Instrument: the radial autocorrelation half-width of the mean-removed
luminance, **calibrated on a known answer first** -- synthetic discs of planted
diameter 30, 60 and 90 texels read half-widths 10.0, 20.7 and 27.8, i.e.
factors 3.00, 2.89 and 3.23, mean **3.04**, and the script refuses if the
spread exceeds 1.35x. A structureless
control (white noise) reads a half-width of 0.50 texels, so the instrument
falls when there is nothing to measure.

`RiverbedRocks02Wet_d.dds` half-width 20.9 texture texels ->
**pebble diameter 63.7 texture texels**.

| | one texture texel | a pebble (63.7 texels) | the biggest pale rock (87.7 texels) |
|---|---|---|---|
| engine repeat 341.333 | 0.1667 world units | **10.6 world units = 0.33 far-sheet texels** | 14.6 units = 0.46 texels |
| baked repeat 2048 | 1.0000 world units | **63.7 world units = 1.99 far-sheet texels** | 87.7 units = 2.74 texels |

Ratio 6.0000 exactly (2048 / 341.3333).

A second, independent pass over the whole 2048x2048 for the pale desaturated
rocks alone (luminance > mean + 2 sd AND saturation < mean): 4.65% of the
texture, 787 rocks of 12 texels or more, median diameter 6.2, p90 25.8, max
87.7 texture texels. That instrument reads the bright CORES of the pebbles, not
their full width, which is why it under-reads the 63.7 above; both are
reported.

### 4.2 The grey spots as they appear on the sheets

Same definition on both sheets, over the riverbed-dominant region of the chunk
(123,238 texels): brighter than that region's mean by 1 sd AND less saturated
than its mean, then connected components.

| | grey area | spots >= 3 texels | median diameter |
|---|---|---|---|
| OURS | **12.35%** of the region | 430 | 3.19 far-sheet texels = 102 world units |
| VANILLA | 5.66% | 145 | 2.76 texels = 88 world units |

Our spots merge adjacent pebbles, which is why 3.19 exceeds the 1.99 a single
pebble occupies at the baked repeat.

The same statistic on **mip 5 of the texture itself** -- the image the bake
prints at the 2048 repeat, where one mip texel is exactly one far-sheet texel:
**11.45% pale area, 61 blobs, median 2.26 mip texels**. Our sheet's 12.35% and
3.19 sit beside the texture's own 11.45% and 2.26.

### 4.3 Is the texture's own pattern in the sheet, and at which repeat?

`s5_period.py`, `logs/s5.log`. The texture is sampled onto the same 256x256
riverbed block at each repeat, high-passed, and correlated with each sheet;
the floor is the same panel phase-randomised (`splatlib.phase_twin`, which
keeps its variance and spectrum and destroys only its registration).

| sheet | repeat | correlation | its phase-twin floor | |
|---|---|---|---|---|
| **ours** | **2048** | **+0.3179** | -0.0023 | **PRESENT** |
| vanilla | 2048 | -0.0058 | -0.0043 | not above its floor |
| ours | 341.333 | -0.0204 | -0.0066 | not above its floor |
| vanilla | 341.333 | +0.0054 | -0.0013 | not above its floor |

So this texture's own grain is in OUR sheet at the 2048 repeat, is not in it at
the engine's repeat, and is in Bethesda's sheet at neither -- the same
controlled negative SPLAT1 measured chunk-wide (+0.8754 for the full blend
against a phase-twin floor).

A note on a test that was run and DISCARDED: a lag-64 autocorrelation ("does
the sheet repeat every 64 texels") cannot discriminate here, because 64 texels
is one repeat at 2048 **and exactly six repeats at 341.333** -- both controls
read +1.0000. It is left in the script's docstring as a refuted approach, not
reported as a result.

### 4.4 The picture's own self-check

The window spans exactly two repeats at 2048 (4096 world units), so panel 6
must be periodic at 64 texels. Measured: maximum absolute difference between
its two halves **0** of 255, and the script refuses to write the page
otherwise. Panel 5 reads 1 of 255 on the same test, as it must -- 64 texels is
also six whole repeats at 341.333.

## 5. The answer, in one sentence

**Yes: those grey spots are the river pebbles of
`Data\Textures\Landscape\Ground\RiverbedRocks02Wet_d.dds`, printed six times
too large, because our bake stretches every landscape texture to 2048 world
units a repeat instead of the engine's 341.333 -- a pebble that should be 10.6
world units across (a third of one far-sheet texel, too small to draw, which is
why vanilla's ground is smooth) is drawn 63.7 world units across, two whole
texels.**

## 6. What is NOT claimed

* **Nothing is fixed.** The 6x tiling is SPLAT1's open finding
  (`src/lodgen.cpp` `constexpr float TILE = 2048.0f` against
  `fLandTextureTilingMult` = 1.5 in `Fallout4.exe` 1.10.155, giving 341.3333).
  No source was touched, nothing was rebuilt, nothing was committed.
* **The colour gap is separate.** Our window is 12 of 255 darker than vanilla's
  on every channel (78.9/69.3/59.1 against 91.1/83.3/72.3). That is the splat
  grading gap ROADS1 measured as x0.82-0.83, and correcting the tiling does not
  close it -- SPLAT1 measured 16.89 -> 15.02 of 255 whole-tile.
* **Panels 5 and 6 are one texture, not a re-bake.** They do not include the
  other five layers, the VCLR term, the road pass or BC1 compression, so their
  absolute colour and their local variance (148.5) are not our sheet's (106.8).
  The claim they carry is the texel-scale pattern and its size, nothing else.
* **Vanilla also has pale patches** (5.66% of the riverbed region, median 2.76
  texels). They are not this texture's grain -- the correlation says so -- and
  what they are was not measured here.

## 7. Files

| file | what |
|---|---|
| `images/riverbed_texture.png` | **the deliverable**, 1106 x 2049 |
| `images/overview_window.png` | both full sheets with the window boxed, this lane's own eye only |
| `images/_rocks_full512.png`, `_tex_*` | the raw textures as inspected |
| `s1_locate.py` -> `locate.json`, `riverbed.npz` | the LAND walk and the per-LTEX weight maps |
| `s2_chain.py` -> `chain.json` | LTEX -> TXST -> diffuse for the whole chunk |
| `s3_window.py` -> `window.json` | the window rule and the rejected candidates |
| `s4_pebble.py` -> `pebble.json` | the calibrated pebble size and the sheet spot sizes |
| `s5_period.py` -> `period.json` | the correlation test and the mip-5 check |
| `make_picture.py` -> `picture_facts.json` | the page |
| `logs/s1..s5.log` | every number above, as printed |

## 8. Finished-work skill review (CONSTITUTION 1a)

**Loaded and used.** `ww-texel-picture` -- the crop chosen by a metric on the
BEFORE artefact over an admissible set with the rejects printed (section 2),
nearest-neighbour magnification, a fixed cell per panel with captions asserted
narrower than their panel (the assertion fired twice in this lane, on a
subtitle line and on the panel-3 path, and both were real defects), the
caption carrying the same numbers this file quotes, and "open the picture
before reporting it", which is how the red repeat label was found sitting
unreadably on the image. `nifskope-ww-vanilla-compare` -- the same crop box on
both sides with no resampling, vanilla's shipped file untouched, and its rule
that a comparison picture may not carry a verdict on a cause (section 6).

**Declined, with the reason.** No new skill written. Every step here is either
already in `ww-texel-picture` (sections 1-5) or is SPLAT1's measured result
being re-used rather than re-derived; the one genuinely new procedure -- the
autocorrelation half-width calibrated against planted discs -- is a single
function and belongs with the next lane that needs a feature size, not as a
page of its own yet. **But one thing should be written down if a third lane
hits it:** PIC-CHUNK already asked for a "sheet pair page" helper (its section
7) and this lane wrote a third variant of the same two-panel-with-provenance
layout. That is now three lanes; the director should add it to
`ww-texel-picture` as a section.

**The mistake this lane made, for the record.** The first pass ranked the
window's layers by LTEX EDID and named `LRiverbedSilt01` the dominant texture;
two LTEXs sharing one TXST made the actual dominant file (`RiverbedRocks02Wet`,
52.6%) invisible to that ranking, and the whole pebble measurement was run on
the wrong texture before the caption assertion surfaced the name. The rule:
**group by the resolved ASSET, never by the record that points at it.**
