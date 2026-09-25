# GREY1 -- why our far-field buildings read greyer than in game (measure only)

Lane GREY1, branch grey1-20260925 from f506a0cc, worktree E:\Projects\NifskopeWWE-grey1. Started 2026-09-25 21:10
(clock read). No code changed, nothing re-baked, nothing written under E:\Projects\Fallout 4 Mods\.

## 0. Progress log

- 21:10 worktree made (no build needed). BAKE2's lodgen (-no-gui, PID 11240) is running; no render until it ends.
- 21:20 candidate 1 first cut (top 10 pairs, 16,969 placements): the LOD atlas is MORE saturated than the
  full-detail textures, not less. Whole run started (top 400 pairs).
- 21:27 candidate 1 whole census done (census2.py, palette + material swaps applied; 72,035 of 75,831 Commonwealth
  building-LOD placements measured). Result in section 1.
- 21:30 candidate 3 done (weather_light.py) and candidate 2 numeric half done (viewer_shade_model.py). The render
  half waits: BAKE2's NifSkope (PID 3888) is still running.
- 21:42 BAKE2 exited; 12 renders done 21:42 (all rc 0). 21:44 section 2 written.

## 1. Candidate 1 -- the LOD atlas colour vs the full-detail building, as the game colours it

Tool: census2.py (helpers in atlas_vs_full.py, ESM reads in esm_swaps.py). Every model and texture is read in
place through his MO2 stack (BAKE1's resources.txt; loose first, then the BA2s). Unit = one (base, material swap)
pair as the Commonwealth REFRs of vanilla Fallout4.esm place it; weight = REFR count. The full model is coloured
the way the game colours it: 41% of its area is grayscale-to-palette (BGSM bGrayscaleToPaletteColor), so the colour
is the palette (texture slot 3) at U = diffuse green, V = the BGSM row or the swap's Colour Remapping Index (MSWP
CNAM); a swap's replacement material (MSWP SNAM) is used where it names one. The first cut (atlas_vs_full.py main,
raw diffuse, no palette, no swaps) was WRONG for that reason and is not quoted.

Coverage: 72,035 of 75,831 placements of building-LOD bases (3,234 no model found, 562 unmeasurable).

| | LOD atlas | full detail | full / LOD |
|---|---|---|---|
| linear luminance | 0.1926 | 0.1981 | +3% |
| saturation of the mean colour | 0.158 | 0.172 | +9% |
| mean per-texel saturation | 0.171 | 0.191 | +12% |

Split by where the tint comes from (the three palette-row readings A/B/C agree to 3 decimals, so vertex colour
plays no part on the full models):

| placements | refs | LOD S / mean S | full S / mean S |
|---|---|---|---|
| no swap | 37,925 | 0.165 / 0.179 | 0.159 / 0.169 (LOD is NOT greyer) |
| swap on the placement (REFR XMSP) | 33,504 | 0.151 / 0.160 | 0.189 / 0.214 (+25% / +34%) |
| swap on the base (STAT MODS) | 606 | 0.080 | 0.096 |

Example: DecoMainA1x1Wall01 with swap 000A3224 (1,099 placements): full S 0.341, sRGB (0.557,0.460,0.367);
its LOD atlas S 0.182, (0.553,0.508,0.452).

Verdict: REAL, but it is vanilla's own trait. A building LOD mesh has one shared atlas; the per-placement
material swap (44% of building placements) that tints the full model cannot reach it. The vanilla game's own far
LOD uses the same atlases, so this explains "far LOD is greyer than the building up close", not "our LOD is greyer
than the game's LOD". Unswapped buildings are no greyer in LOD than in full detail.

Mip averaging (mip_sat.py, 4d below): mean per-texel S of the atlases 0.150 at mip 0, 0.139 at mip 4, 0.138 at
mip 6 (-8%). Common to the game and the viewer; not a cause.

## 3. Candidate 3 -- the in-game weather light at midday, as a multiplier

Tools: esm_weather.py (vanilla Fallout4.esm WRLD 0x3C -> CLMT DefaultClimate -> WTHR/IMGS -> weathers.json),
weather_light.py. CommonwealthClear (chance 51 of 102 in the climate), Day slot:
- Sun (225,225,225) = neutral; x IMGS Sunlight Scale 4.5 -> 3.42 linear.
- DALC (directional ambient) X+ (97,113,130) X- (75,93,111) Y+ (82,96,111) Y- (93,113,132) Z+ (42,52,62)
  Z- (101,133,169): every axis is BLUE. Flat NAM0 ambient (93,93,93).
- IMGS CW_ClearDAY_MAY19: Cinematic saturation 1, brightness 1, contrast 1; Tint amount 0. The other Commonwealth
  weathers also have saturation 1 and tint 0. Vanilla post-processing adds NO saturation.

Light colour on a face, normalised (sun 60 deg high from the south; 40 and 75 deg give the same picture):

| face | light R:G:B | the light's own S | LOD-atlas mean colour S 0.17 -> | a neutral grey S 0.00 -> |
|---|---|---|---|---|
| roof | 0.92 : 0.95 : 1 | 0.04 | 0.13 | 0.04 |
| wall in sun | 0.96 : 0.98 : 1 | 0.02 | 0.15 | 0.02 |
| wall side-on (east) | 0.42 : 0.68 : 1 | 0.32 | 0.20 | 0.33 |
| wall in shade (north) | 0.46 : 0.71 : 1 | 0.29 | 0.16 | 0.30 |

Verdict: PART of it, and a different kind of colour. The game lights sunlit faces almost white and SHADED faces
strongly blue (DALC), so in game a neutral grey building is never grey on its shaded sides -- it reads cool blue
-- and warm walls in shade swing toward neutral-blue. The viewer's Legacy light is pure white everywhere, so a
neutral atlas stays exactly neutral. Assumption flagged: the DALC axis sense (src/esmweather.cpp calls it an
assumption); flipped, the up/down faces swap colours but every axis is still blue, so the conclusion holds.

### 3b. What he actually plays with (read 21:35, his live files, read only)
- **FO4CS Physical Weathers** (FO4CSPhysicalWeathers.esp) overrides CommonwealthClear: sun (220,220,220), NAM0
  ambient (55,60,68), DALC X+ (62,72,83) X- (48,60,71) Y+ (52,61,71) Y- (60,72,84) Z+ (27,33,40) Z- (65,85,108)
  -- blue on every axis again. weather_light.py on it (weather_light_pw.out): roof light S 0.02, sunlit wall 0.01,
  side-on wall 0.32, shaded wall 0.28. Same conclusion as vanilla.
- **Side finding, not GREY1's to fix:** that override's IMSP (the eight imagespaces) holds 0x01002665..0x0100266C
  and 0x010044DE. With the esp's masters (Fallout4, DLCCoast, DLCNukaWorld) index 01 = DLCCoast, where those IDs
  are REFR/STAT records, not imagespaces (pw_imgs_probe.py; DLCNukaWorld has none of them either). So in game
  the Clear weather's imagespace likely does not resolve. Refuter: xEdit shows the IMSP entries resolving to IMGS.
- **FO4CS.ini [Post]:** bGradingEnabled 0, fGradeSaturation 1.0, fGradeVibrance 0.0; tone curve Physical with a
  chroma fade only at 0.98..1.16 of white. In game no pass adds saturation either.

## 4. Candidate 4 -- everything else, each with its discriminator

| # | candidate | measured | verdict | discriminator |
|---|---|---|---|---|
| 4a | per-placement material-swap tint cannot reach a shared LOD atlas | full S 0.189 vs LOD 0.151 on the 33,504 swapped placements (section 1) | REAL, vanilla's own | the swap-kind split: unswapped placements show no gap |
| 4b | LOD atlas textures fail to resolve and draw grey | 0 of the atlases used by the measured building LODs unresolved through his MO2 stack (mip_sat.py); on screen, base colour S 0.165 vs atlas 0.158 | NOT a cause: the raw base colour on screen equals the atlas (section 2) | raw base-colour render (WW_LOD_CHANNEL=12) mean colour vs the atlas mean |
| 4c | the viewer's palette rule (row x vertex-colour red) differs from the engine | readings A/B/C of section 1 agree to 3 decimals; TINT1: 0 of 2,848 building-LOD BGSMs use the palette | NOT a cause | a building LOD BGSM with bGrayscaleToPaletteColor set |
| 4d | mip averaging | mean per-texel S 0.150 at mip 0 -> 0.138 at mip 6 (-8%) | common to game and viewer; NOT a cause | the same number from a game capture of the same atlas at that distance |
| 4e | FO4CS in-game grading adds saturation | grading off, saturation 1.0, vibrance 0 (section 3b) | NOT a cause | a game capture with FO4CS [Post] off |
| 4f | vanilla imagespace saturation/tint | CNAM saturation 1, tint 0 on every Commonwealth weather (section 3) | NOT a cause | -- |

## 2. Candidate 2 -- the viewer's own shading (read 21:43)

Code (f506a0cc): the Legacy scene mode, which every session starts in, lights with a white headlight (frontalLight,
lightColor 0 -> white, brightness 1), white ambient 1 (A = sqrt(1) x 0.375), and a per-channel Uncharted2 filmic
tone map; GGX specular is untinted. The texel is used as stored (UNORM, the shader's sqrt-linear space); no sRGB
mix-up, no desaturation pass; fog exists only in Lookdev. Lookdev feeds legacy shapes a FLAT NAM0 ambient, so the
weather's blue DALC never reaches the far-field objects (src/gl/lookdevstage.h).

Numeric model (viewer_shade_model.py): the tone curve's log-slope is 0.98 at input 0.3 and 0.80 at 0.9, so a
face turned to the headlight is pushed into the shoulder: the atlas mean colour S 0.167 -> 0.140 at N.L = 1,
0.162 at N.L = 0.

Renders (run/release/NifSkope.exe = main 21:09 exe, sha1 375b42b3; installed FO4CSLOD Commonwealth set; BAKE1's
Boston window cells -5,-10..2,-3, oblique; frames read back 1600 wide x 1624 high; one NifSkope at a time,
after BAKE2's had exited; pics/, pics_run.out, contact strip pics/close_strip.png). Building pixels = non-tree placements (seed channel black): 732,768
(wide) and 2,103,256 (close). Same pixels in every shot (measure_pics.py, measure_pics.out):

| shot | S of mean | mean S | luma | median per-pixel S / S(raw base colour) |
|---|---|---|---|---|
| raw base colour (WW_LOD_CHANNEL=12) | 0.165 / 0.162 | 0.173 / 0.171 | 0.425 / 0.417 | 1 |
| Legacy lit (default) | 0.156 / 0.150 | 0.165 / 0.163 | 0.496 / 0.487 | 0.948 / 0.948 |
| Legacy lit, vertex colour (AO) forced on | 0.156 / 0.152 | 0.168 / 0.166 | 0.396 / 0.379 | 0.966 / 0.967 |
| Lookdev, CommonwealthClear 12:00 | 0.146 / 0.135 | 0.169 / 0.169 | 0.256 / 0.248 | 0.987 / 0.989 |

(wide / close.) The raw base colour on screen (S 0.165, mean S 0.173) equals the atlas census (0.158 / 0.171):
**the textures resolve**; nothing draws grey from a missing texture (4b closed). Lit vs Lookdev differ on 49% /
96% of pixels, so the Lookdev pin took.

Verdict: SMALL. The viewer's own shading takes ~5% of the saturation (tone-map shoulder under the headlight) and
adds no colour cast; in Lookdev it takes ~1% but makes the scene half as bright (luma 0.25) and still cast-free.

## 5. Ranked verdict

1. **The per-placement material swap (the "tint") never reaches the LOD.** 44% of Commonwealth building
   placements carry a material swap that recolours the full model through the palette. Their full-detail colour
   is 25% more saturated than the one shared LOD atlas (S 0.189 vs 0.151), and the swap moves the colour a lot
   both ways: full/LOD colour ratio p10 0.7, p90 1.6 over 4,049 (base, swap) variants. In game neighbouring
   buildings of one kit are many colours; in our far field they are all the one beige-grey atlas. Unswapped
   buildings show no gap. This is vanilla's own LOD trait -- the game's own far LOD shares it.
2. **The game's light has a colour; the viewer's does not.** Clear midday (vanilla and his Physical Weathers):
   sunlit faces lit near-white, shaded faces lit blue (light S 0.28..0.32), so in game no building side reads
   neutral grey. The viewer's Legacy light is pure white, and Lookdev hands far-field objects a flat grey ambient
   (the blue DALC never reaches them): the Lookdev render adds no cast (per-pixel S ratio 0.99).
3. **The viewer's tone map under the headlight: -5%** (render, per-pixel median 0.948). Small.
- Not causes (measured): missing textures, the viewer's palette rule, mips (-8%, common to both), FO4CS grading
  (off), imagespace saturation/tint (1 / 0).

Which of 1 or 2 dominates what he sees depends on what he compared against. Discriminator: were the in-game
buildings inside the loaded cells (full detail, uGridsToLoad) or in the game's own far LOD? Full detail -> 1
leads; the game's own far LOD -> 1 is shared and 2 leads.

**Smallest change that would close the gap (proposal only, nothing changed):** in the object bake, give each
placement an RGB multiplier = the census's full-detail mean colour of its (base, swap) / the LOD atlas mean colour
of its base (census2.py already computes both, REFR by REFR), and multiply it into the albedo like the TINT1
library colour. Second, for 2: let Lookdev feed legacy far-field shapes the DALC ambient cube instead of the flat
NAM0 ambient.

**The test that would refute it:** take the buildings he points at as "grey" and read their REFR XMSP and base
MODS. If they carry no swap, verdict 1 does not explain them. After the change: the WW_LOD_CHANNEL=12 render of the
Boston window must rise from S 0.165 toward the census's full-detail 0.172 overall, and each swapped placement
must land within 0.03 of its census full-detail S. If it does not, the multiplier is wrong.

## 6. Skills

- Loaded: search-lean, nifskope-ww-render-shot.
- Wished for: a skill for the "LOD vs in-game colour" question. None existed, and the first cut (raw diffuse)
  gave the wrong sign.
- Written: fo4-surface-colour-census, at E:\Projects\Claude\.claude\skills\fo4-surface-colour-census\SKILL.md
  and E:\Tools\AISkills (commit 4babe6e). It covers the palette/material-swap trap, the ESM reads, REFR
  weighting, the swap-kind split, the weather light step, and the pixel-for-pixel viewer A/B.
