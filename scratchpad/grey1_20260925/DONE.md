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
