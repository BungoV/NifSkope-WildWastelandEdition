# Lane TILING2 -- the far-terrain colour after the tiling fix

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, nothing committed.
Exe at launch `release/NifSkope.exe` 2026-09-11 20:46:44, 21,458,944 B,
sha1 `955b0952a5f7ab62d4dcfef7852a923c5a6d3f22` (ROADS2's DONE exe).
Rung taken once at 21:20: `release/NifSkope.before_tiling2.exe`, byte-equal to it.

bungo's words, 2026-09-11 19:2x, over the "OURS default 341.3333" panel of
`scratchpad/resume3_20260911/images/cmp_tiling_fixed.png`:

> "Asking, because there's a few issues here" ... "Yes, you can see the tiling
> pattern of each texture, which is not good, hard blend edges also appear in
> some places, and yeah, it's muddy or blurry looking"

Three complaints, three instruments, three laws measured on vanilla first.

## 0. Pre-registered gates

Registered at 21:2x, before any measurement was read and before any code.

| gate | what must be true | floor (the reading that means "nothing") | ceiling (vanilla's own worst) |
|---|---|---|---|
| F1 | vanilla's three laws measured over 22 shipped dim-4 sheets before code, each with a floor and a ceiling | each statistic's own null, stated per sheet | filled in by the measurement itself |
| F2 | `--land-sample footprint` + `--blend-edges off` == the rung's bytes, every file of both tiles; `.lodl` weights and `_msn` byte-identical at EVERY setting | a byte compare that can fail (proved against a knowingly different bake) | -- |
| F3 | tiling visibility at or below vanilla's on both tiles; edge widths inside vanilla's histogram; no chunk-border seam | the null sweep (below) | vanilla's worst of 22 |
| F4 | local variance and radial spectrum within 20% of vanilla's, or REFUSED with the correlation table | the phase twin, for the correlation only | vanilla vs its own neighbouring sheets |
| F5 | chain at ROADS2's baseline; exe newer than every changed file; drivers rebuilt; rung == launch bytes; no NifSkope left running | -- | ROADS2's counts |

### The three instruments, and what each one's floor is

`scratchpad/tiling2_20260911/t1_lib.py`. Self-test `t1_selftest.py`:
**16 checks, 0 failures, PASS** (`logs/t1_selftest.txt`). Every check has a
floor on the other side of it, per `ww-control-calibration`.

a. **Periodicity -- "tiling visibility"**. The repeat's AMPLITUDE, in 8-bit
   luminance units, at 341.3333 world units = 10.667 texels, read in the
   frequency domain off the 33x33 high-passed sheet, strongest of x, y and the
   diagonal. Check A0: a cosine of known amplitude reads its own amplitude to
   within 2.8% over nine cases.

b. **Edge width**. The 10-90% transition width in texels of the top decile of
   gradient magnitude, sampled along each texel's own gradient direction over
   +-8 texels at quarter-texel steps. Its resolution floor is 1 texel: a hard
   step reads 1.00 (check B5), so "hard" in this report means "at or under one
   texel", not "zero".

c. **Detail**. The radially averaged power spectrum in six scale bands, the 3x3
   local variance, and the mean absolute log2 band-power ratio between two
   sheets as one "spectrum distance". Check C3: white noise of SD 6 must add
   sigma^2 * 8/9 = 32.0 squared units to the local variance; measured 32.14.

### REFUSED: the brief's proposed floor for (a) and (c)

The brief asks for the phase-randomised twin as the floor for periodicity and
for the spectrum. **It is not one, and this lane refuses it with the number.**
`phase_twin` randomises phase and keeps the amplitude spectrum exactly, so the
twin's power spectrum -- and therefore its autocorrelation and every other
second-order statistic -- is identical to the subject's bin for bin. Self-test
A5: a field with a repeat injected at amplitude 4/255 reads 100% of its own
visibility on its twin. The twin is a valid floor for a STRUCTURE statistic (a
correlation against another field, which is what lane SPLAT1 used it for), it is
used that way in section 1c below, and nowhere else.

The floor used instead for (a) is a **null sweep**: the same statistic on the
same sheet at twelve periods that are not the repeat, not the BC block (4) and
not the quadrant grid (64) -- 6.1, 7.3, 8.5, 9.1, 12.4, 13.7, 15.2, 17.3, 19.1,
23.4, 27.6, 31.5 texels -- reported as its maximum. That is the distribution of
"there is no repeat at this lag", carrying the sheet's own spectrum.

### Two refused instrument designs before that one

Both are in `MISTAKES_ENTRIES.md`.

1. **Autocorrelation at lag 10.67** (the brief's own proposal for 1a) failed its
   controls: the null sweep read 0.1997 where an injected repeat of amplitude
   8/255 read only 0.0616. At a ten-texel lag a terrain sheet's autocorrelation
   is dominated by its own hillside decay, and an off-peak baseline cannot
   separate decay from a peak.
2. **Peak-to-background ratio** in the frequency domain exploded to 1.1e9 on a
   band-limited synthetic whose background at bin 48 is numerically zero, and is
   not comparable between sheets of different contrast. Replaced by the
   amplitude, which is in the units of bungo's complaint: how many levels of 255
   the repeat moves the sheet by.

### The 22 sheets, and why those

`t0_pick.py`, rule fixed before any number was looked at. Two tiles inherited
from RESUME3, (-20,24) and (-20,20); then a deterministic 5x4 lattice over the
bounding box of the qualifying sheets, each point snapped to the nearest
qualifying sheet. A sheet qualifies at mip-3 luminance SD >= 5.25 -- **not an
invented threshold**: the mip-3 SD over all 2,304 shipped `Commonwealth.4.*`
colour sheets is bimodal, 1,177 sheets below 5.00 (the ocean/void filler: one
flat colour and a gradient), only 31 in the 5.00..5.50 trough, 1,113 at or
above. 5.25 is the trough midpoint and `logs/t0_hist.txt` is the histogram it
was read off. Filler sheets have no blend edge and no texture repeat, so a
periodicity statistic on one measures the codec.

The 22: (-20,24) (-20,20) inherited; then (-64,-60) (-36,-60) (-4,-60) (28,-60)
(52,-64) (-64,-20) (-36,-20) (-4,-20) (28,-20) (60,-32) (-64,16) (-36,16)
(-4,16) (24,16) (44,32) (-64,56) (-36,56) (-4,56) (20,56) (44,36).

## 1. Vanilla's laws (periodicity, edges, detail)

Full tables: `scratchpad/tiling2_20260911/logs/t3_laws.txt` (per-sheet) and
`logs/t3b_seam.txt` (the ratio and the quadrant seam). Our two sheets are the
region bakes of `t2_bake.sh` on the launch exe, into this lane's own out-dir --
never his installed `Data\Terrain`.

### 1a. Periodicity -- THE LAW: vanilla's sheet never shows the repeat

**0 of 22 shipped sheets read the landscape repeat above their own null floor.**
Vanilla's ceiling is therefore the worst of the 22, on both scales:

| | vanilla median of 22 | **vanilla worst of 22 = THE CEILING** | ours 341.3333 (-20,24) | ours 341.3333 (-20,20) | ours 2048 (-20,24) | ours 2048 (-20,20) |
|---|---|---|---|---|---|---|
| tiling visibility (amplitude, of 255) | 0.118 | **0.264** | 1.037 | 1.261 | 0.963 | 0.830 |
| the same over that sheet's own null floor | 0.164 | **0.448** | 0.728 | 0.873 | 0.502 | 0.450 |

**bungo is right, and the number says how right.** Our current default puts
about one full luminance level of periodic signal into the sheet at exactly the
land texture's repeat; vanilla's worst sheet of twenty-two carries a quarter of
a level. On the dimensionless scale -- how far the repeat stands above that
sheet's own broadband detail -- ours is 0.73 and 0.87 where vanilla never
exceeds 0.45.

Two things this table also says, and both are awkward:

- The old default (2048, the way back) is **not innocent either**: 0.50 and 0.45
  on the ratio scale, i.e. sitting exactly on vanilla's worst. The 2026-09-11
  tiling fix did not create a repeat that was not there; it made an existing one
  about 1.7x more prominent, because 341.3333 puts the repeat at 10.67 texels
  where the eye and the mip chain both resolve it, and 2048 put it at 64 texels
  where it hid under the terrain's own shape. **The tiling constant is right and
  is not touched here** (RESUME3's); what has to change is how the land texture
  is sampled within it.
- Ours is still BELOW its own null floor (1.037 vs 1.424, 1.261 vs 1.443). That
  is not a defence. It means our sheets carry MORE broadband energy at those
  scales than vanilla's do -- the same mosaic showing up at the null periods as
  well. The gate is vanilla's ceiling, not our own floor.

### 1b. The edges -- THE LAW: vanilla's edges are not on the quadrant grid

Widths first. Ours are **not harder than vanilla's** -- they are fewer and
slightly wider:

| | vanilla median of 22 | vanilla range | ours 341.3333 (-20,24) | ours 341.3333 (-20,20) |
|---|---|---|---|---|
| w50, 10-90% width of the top-decile edges, texels | 4.12 | 2.50 .. 6.50 | 5.00 | 4.00 |
| w90 | 8.25 | 7.25 .. 11.00 | 8.50 | 8.25 |
| count at or under 1 texel ("hard") | 523 | 250 .. 1705 | 52 | 190 |

So the width histogram is already inside vanilla's. **The hard edges bungo sees
are not hard by width -- they are hard by POSITION.** Two independent tests of
the same mechanism, the 64-texel quadrant grid:

| | vanilla median of 22 | **vanilla worst of 22 = THE CEILING** | ours (-20,24) | ours (-20,20) |
|---|---|---|---|---|
| hard edges within 1 texel of a quadrant border (chance 9.2%) | 6.9% / 7.9% on the two inherited tiles | never significant: smallest p over 22 sheets = 0.119 | **13 of 52 = 25.0%, p = 0.00066** | 22 of 190 = 11.6%, p = 0.15 |
| gradient across the 14 quadrant lines over the sheet's own mean: worst line | 1.399 | **2.339** | 1.933 | 1.341 |
| the same, mean of all 14 lines | 1.041 | **1.100** | **1.236** | 1.065 |

Read together: on chunk (-20,24) **every one of the fourteen quadrant lines
carries 23.6% more gradient than the sheet's average, where no vanilla sheet of
twenty-two exceeds 10.0%**, and its hard edges sit on those lines at nearly
three times chance with p = 0.0007. On chunk (-20,20) the same statistics are
inside vanilla's range. The mechanism is named and counted: **the quadrant
border**, where the base texture and the layer SET change and nothing blends
across -- not the negligible-layer skip, not the road plane (1b-road below shows
the road's own edge is the same width as vanilla's). It is present at this
strength on one of the two tiles, which is why bungo said "in some places".

### 1b-road. The road edge on chunk (-20,20) (director addendum, measurement only)

`t3c_road.py`, `logs/t3c_road.txt`. No road code touched, no road switch added;
lane ROADS3 owns that. The road's texels were not guessed: the chunk was baked
twice on the launch exe, normally and with `--no-roads`, and the mask is the
25,113 texels (9.58% of the sheet) the road pass actually changed. The same mask
is then applied to vanilla's shipped sheet, because the road is a world fact out
of the ESM.

| chunk (-20,20) | w10 | w50 | w90 | height of the profile the width was read off | road body | its surround | **contrast** |
|---|---|---|---|---|---|---|---|
| vanilla | 2.00 | 6.00 | 11.00 | 23.26 | 92.50 | 88.57 | **+3.92** |
| ours, launch exe | 2.00 | 6.00 | 12.00 | 43.76 | 98.86 | 69.41 | **+29.45** |
| ours `--no-roads` (control) | 2.75 | 5.75 | 10.73 | 26.60 | 60.31 | 69.41 | -9.10 |

Cross-road luminance, mean over 1,492 boundary texels (t<0 outside the road,
0 its edge, t>0 towards the centre):

```
  t (texels)              -6     -5     -4     -3     -2     -1      0     +1     +2     +3     +4     +5     +6
  vanilla               89.8   89.8   90.1   90.8   90.8   90.8   90.8   90.8   91.1   91.0   91.5   91.7   92.0
  ours (launch exe)     68.7   69.1   69.7   69.7   69.5   69.0   72.4   77.2   82.9   88.3   91.2   92.6   93.5
  ours - vanilla       -21.1  -20.7  -20.4  -21.0  -21.3  -21.8  -18.4  -13.6   -8.2   -2.7   -0.3   +0.9   +1.5
```

**The road edge is not a width problem -- it is a contrast problem.** Our road
crosses in 6 texels, exactly vanilla's w50, and its w90 is 12 against vanilla's
11: inside. But our road stands **+29.45 luminance levels over its surround
where vanilla's stands +3.92** -- vanilla's far-LOD road is very nearly not
there, a 2-level rise over twelve texels. The width row for vanilla is measured
off a 23-level profile that is the terrain's own texture under the mask, not a
road edge; for vanilla, read the contrast, not the width.

One number in that table belongs to a different lane: our surround is 69.41
where vanilla's is 88.57, a 19-level tone offset across the whole non-road
sheet. That is GRADE1's -- **no tone change is made here** -- but it means our
road looks brighter partly because our ground is darker, and ROADS3 must not
chase all 25 levels with the road pass alone.

### 1c. The detail -- THE LAW, and the REFUSAL: vanilla's fine detail has no source in this bake

Where vanilla's energy is and ours is not, as a share of each sheet's own
variance (`logs/t3_laws.txt`):

| band | vanilla (-20,24) | ours (-20,24) | vanilla (-20,20) | ours (-20,20) |
|---|---|---|---|---|
| 8..32 texels | 20.7% | 12.4% | 20.0% | 33.3% |
| 4..8 texels | 8.5% | 3.0% | 7.9% | 4.1% |
| 2..4 texels | 16.3% | 2.4% | 15.3% | 2.0% |
| at or under 2 texels | 6.3% | 0.3% | 6.2% | 0.4% |

Below 4 texels vanilla carries 22.6% and 21.5% of its variance; we carry 2.7%
and 2.4% -- **a factor of eight**. Local variance: ours 12.29 vs vanilla 19.81
(-38%) and 24.41 vs 29.39 (-17%). Spectrum distance ours-to-vanilla 1.227 and
1.629, against **0.670 for vanilla (-20,24) against vanilla (-20,20)** -- that
is the ceiling for "how different two real sheets are", and we are twice it.

So the complaint is real and measured. Then: where does vanilla GET that
detail? Ten candidates, each correlated against vanilla's high-pass residual
(everything finer than 5 texels), each with its own phase twin beside it as the
floor -- the one place the twin is a valid floor, because this is a structure
statistic. `logs/t4_corr.txt`:

| candidate | r, chunk (-20,24) | its twin floor | r, chunk (-20,20) | its twin floor |
|---|---|---|---|---|
| land textures at the footprint mip | -0.0029 | -0.0039 | 0.0025 | 0.0019 |
| one mip finer | -0.0018 | -0.0047 | 0.0022 | 0.0021 |
| two mips finer | -0.0009 | -0.0047 | 0.0009 | 0.0017 |
| one mip coarser | -0.0023 | -0.0034 | 0.0019 | 0.0016 |
| the exact footprint box mean | -0.0021 | -0.0043 | 0.0031 | 0.0010 |
| the fully averaged texture | -0.0023 | -0.0028 | 0.0014 | 0.0005 |
| VCLR | -0.0040 | -0.0017 | -0.0006 | 0.0004 |
| slope (the shipped `_msn`) | 0.0001 | -0.0009 | -0.0043 | -0.0016 |
| directional shading, best of 8 azimuths | 0.0060 | -0.0017 | 0.0045 | -0.0051 |
| our own rung sheet | -0.0020 | -0.0032 | 0.0034 | -0.0015 |

**Every single number is zero to three decimal places.** Vanilla's high-pass
residual has a standard deviation of 4.48 and 5.46 luminance levels -- it is
real, strong structure -- and NOTHING the bake can compute correlates with it.

#### The control that makes that table mean something

A correlation of zero is also what two MISALIGNED sheets read, so the table is
worth nothing on its own. `logs/t4b_align.txt`: the same offline composite,
against OUR OWN bake instead of vanilla's, over a plus/minus 2 texel shift search:

| chunk | offline vs OURS | offline vs vanilla | row-flipped control |
|---|---|---|---|
| (-20,24) | **+0.7948**, best shift (0,0) | -0.0030, best +0.0042 anywhere in the search | +0.0501 |
| (-20,20) | **+0.7040**, best shift (0,0) | +0.0017, best -0.0040 anywhere in the search | +0.0137 |

The offline model reproduces our own generator's fine structure at r = 0.79 and
0.70 with the best shift at exactly zero, and an upside-down copy reads 0.05.
The instrument, the model and the alignment are all right.

#### THE REFUSAL

**This lane refuses the blur, with the numbers.** Vanilla's fine detail is not
the land textures at any mip, not the exact footprint average, not VCLR, not
the slope and not a directional shading of the slope. It is a term this bake
has no source for: vanilla's shipped far-terrain colour was not produced by
resampling the landscape textures this composite composites. (Lane SPLAT1
reached the same conclusion from the other side: vanilla's sheet contains no
landscape-texture pattern at any of six candidate repeats.) Per the brief's own
standing instruction, a refused round is a deliverable, and nothing is baked
here to imitate a source that does not exist.

The brief's fallback -- "if it is VCLR or a lighting plane, name it and route to
GRADE1" -- cannot be taken either: VCLR reads -0.004 and 0.000, and the best of
eight lighting azimuths reads 0.006. There is nothing to route.

## 2. The repeat

`--land-sample footprint|average`, plus `--land-detail k`. `footprint` is the
default and the rung's bytes exactly. `average` reads the landscape diffuse's
1x1 mip: a landscape texture ships a full mip chain to 1x1, and one repeat IS
the whole texture, so that texel is the exact average over one repeat and no
periodic term can reach the sheet. `--land-detail k` adds back k of the
footprint sample's departure from that average, which scales the repeat by the
same k (default 0, so `average` means averaged).

`logs/t5_variants.txt`, both tiles, every variant, against vanilla's ceilings
from section 1a (visibility 0.264 absolute, 0.448 over the sheet's own floor):

| chunk (-20,24) | tiling visibility | over its own floor | quadrant seam, mean of 14 | local variance | spectrum distance to vanilla |
|---|---|---|---|---|---|
| **vanilla** | 0.201 | 0.448 (worst of 22) | 1.100 (worst of 22) | 19.81 | 0 |
| rung (footprint) | 1.037 | 0.728 | 1.236 | 12.29 | 1.227 |
| `--land-tiling 2048` (the old bake) | 0.963 | 0.502 | 1.105 | 70.83 | 1.721 |
| `--land-sample average` | 0.100 | 0.068 | **1.710** | 4.97 | 2.988 |
| average + `--blend-edges quadrant` | **0.092** | **0.063** | **0.976** | 4.84 | 3.025 |
| the same at `--land-detail 0.25` | 0.268 | 0.192 | 0.983 | 5.22 | 2.743 |

| chunk (-20,20) | tiling visibility | over its own floor | quadrant seam, mean of 14 | local variance | spectrum distance |
|---|---|---|---|---|---|
| **vanilla** | 0.032 | 0.041 | 1.018 | 29.39 | 0 |
| rung (footprint) | 1.261 | 0.873 | 1.065 | 24.41 | 1.629 |
| `--land-sample average` | 0.562 | 0.393 | 1.126 | 19.62 | 1.808 |
| average + blend | 0.564 | 0.393 | **0.767** | 19.64 | 1.810 |

Read it straight:

- **The repeat is gone where it can be measured as a repeat.** On (-20,24) the
  average sampling takes the visibility from 1.037 to 0.092, under vanilla's
  0.264 by a factor of three, and from 0.728 to 0.063 on the dimensionless
  scale against vanilla's 0.448.
- **The average sampling ALONE makes the quadrant grid worse, not better**:
  1.236 to 1.710 on (-20,24), because once each quadrant is a flat mixture of
  flat colours the only edges left in the sheet ARE the quadrant lines. The two
  switches are not independent; `average` needs `blend-edges`.
- **The 0.562 that (-20,20) still reads is not a texture repeat, and there is a
  byte-exact proof.** With `--land-sample average` the sheet is
  **byte-identical at `--land-tiling 341.3333` and at `--land-tiling 2048`** --
  the land term no longer depends on the tiling constant at all, so nothing
  periodic at 341.3333 units can be entering through it. What the statistic
  reads there is that sheet's broadband energy landing in that bin: in the
  largest road-free window of the same chunk (256 texels = exactly 24 repeats,
  zero road texels, `logs/t6_local.txt`) vanilla itself reads 0.245 and our
  average bake reads 0.376, with the dimensionless numbers 0.255 for vanilla and
  0.131 for ours. On that scale we are better than vanilla in the same window.
- The detail knob is nearly worthless and the sweep says so. On (-20,24),
  average + blend at k = 0 / 0.15 / 0.25 / 0.35 / 0.50 reads visibility
  0.092 / 0.175 / 0.268 / 0.366 / 0.532 and local variance
  4.84 / 5.00 / 5.22 / 5.58 / 6.45. Vanilla's visibility ceiling (0.264) is
  crossed at **k = 0.246** by linear interpolation between the 0.15 and 0.25
  points, and by then the knob has bought about **0.37 of the 14.97
  local-variance levels** the average bake is missing -- 2.5% of the gap, for
  all of the repeat. **The repeat and the land texture's detail are the same
  signal; you cannot keep one and drop the other.** That is 1c's refusal again,
  reached from the output side.

## 3. The edges

`--blend-edges off|quadrant`, with `--blend-margin` (default 128 world units =
4 texels, the opacity grid's own spacing). `off` is the default and the rung's
bytes. `quadrant` cross-fades the neighbouring quadrant's composite over that
margin either side of every 2,048-unit line, evaluated AT THE SAME WORLD POINT
with the neighbour's own layer set and its own opacities read past its edge
(hence clamped to its edge row), with a quintic ease that is exactly 0.5 at the
line -- so both sides of a line meet on the same value and the composite is
continuous across it.

| | vanilla median of 22 | vanilla worst of 22 | rung | with `--blend-edges quadrant` |
|---|---|---|---|---|
| (-20,24) quadrant seam, mean of the 14 lines | 1.041 | 1.100 | 1.236 | **0.955** |
| (-20,24) worst single line | 1.399 | 2.339 | 1.933 | **1.088** |
| (-20,24) p, hard edges on a quadrant line | -- | 0.119 smallest of 22 | 0.001 | 0.061 |
| (-20,24) hard edges (1 texel or under), of 6,000 | 523 | 250..1705 | 52 | 39 |
| (-20,24) w50 / w90, texels | 4.12 / 8.25 | 2.50..6.50 / 7.25..11.00 | 5.00 / 8.50 | 5.25 / 8.75 |
| (-20,20) quadrant seam, mean of 14 | 1.041 | 1.100 | 1.065 | **0.847** |
| (-20,20) worst single line | 1.399 | 2.339 | 1.341 | **1.114** |

**The grid is gone** -- below vanilla's own median on both tiles, from above its
worst-of-22 on (-20,24) -- and it costs nothing measurable anywhere else: local
variance 12.29 to 12.18 (-0.9%), spectrum distance 1.227 to 1.226, tiling
visibility 1.037 to 1.050 (+1.3%), the fine-scale variance share unchanged at
5.7%. Edge widths stay inside vanilla's range on both tiles.

Two honest caveats:

- With so few hard edges left (39 of 6,000 sampled on (-20,24), against
  vanilla's 492) the position test still reads p = 0.06, and on the average
  variants p < 0.001 on a handful of edges. The direct seam measurement -- the
  gradient across the lines themselves, which is what a visible line IS -- is
  below vanilla's median in every blended variant. What is left is a few
  texels, not a line.
- A quadrant border lying on the CHUNK's own edge is not blended: the
  neighbouring cell's paint is not loaded there. The adjacent chunk's bake does
  not blend it from its side either, so no new seam is created -- but that one
  line stays as hard as it is today. All seven interior lines per axis are
  blended.
- The cross-fade is in the virtual-texture tile bake, which is the path that
  writes the sheets (the stock per-chunk path got the same code, but a region
  bake with `--vt` never reaches it -- see section 5). The roughness, the
  metalness, the emissive and the cover opacities are deliberately NOT
  cross-faded, which is why `_data` stays byte-identical at every setting.

## 4. The detail term

**Refused, and not implemented as a term.** 1c found no source: no correlation
above its own floor, on either tile, with the model validated at r = 0.79
against our own generator. The only thing a detail term can add back is the
land texture's own high-frequency content, and section 2's sweep prices it:
every unit of it brings back exactly the repeat bungo complained about, and at
the largest k that keeps the repeat inside vanilla's law (k = 0.24) it recovers
0.25 of the 15 local-variance levels we are short.

So `--land-detail` exists -- the honest knob for that trade, one value, not a
third switch, exactly as the brief specified -- and its default is 0. Nothing
is invented to fill the gap, and nothing is routed to GRADE1, because VCLR and
the lighting planes read zero too.

## 5. Build and gates

### The exe, and the one build

| | mtime | bytes | sha1 |
|---|---|---|---|
| the rung, `release/NifSkope.before_tiling2.exe` | 21:20:07 | 21,458,944 | `955b0952a5f7ab62d4dcfef7852a923c5a6d3f22` |
| **on disk now, `release/NifSkope.exe`** | **21:52:22** | **21,466,624** | `9492e5a60deaf9aab19bf6269c14bf6caed01989` |

The rung is a byte copy of the exe this lane was handed (ROADS2's, 21,458,944
bytes). **One build** (21:47:49, 21,465,600 B) and **one counted relink**
(21:52:22, 21,466,624 B). The relink is counted and its reason is named: the
first build put the cross-fade only in the stock per-chunk composite, and the
sheet came out byte-identical even at `--blend-margin 1024` — the colour DDS is
written by the PYRAMID pass, so the change had to be made at the second site
too (see section 8). The same relink changed `g_landDetail` from 1.0f to 0.0f so
that `average` means averaged.

`Fallout4.exe` was checked down before every build and every bake
(`t2_bake.sh` refuses while it is up); no NifSkope was running before any build
and none is running now. **Every bake went into
`scratchpad/tiling2_20260911/out/` only** — no installed file, no
`Data\Terrain`, and no whole-Commonwealth bake: two 4x4-cell regions,
(-20,24)..(-17,27) and (-20,20)..(-17,23).

The exe is newer than every file it was built from: `src/lodgen.cpp` 21:51:18,
`src/lodgen.h` 21:51:27, `src/nifcli.cpp` 21:46:22, exe 21:52:22.

### F2 — the defaults are the rung, byte for byte

A bake writes 9 files per tile: `tex/<ws>.4.x.y.DDS`, `_msn.DDS`, `_data.DDS`,
`mod/Terrain/Commonwealth.VT.2.lodt`, `.VT.4.lodt`, `.VT.lodm`,
`obj/*.BTO`, `*.BTO.manifest.txt`, `*.BTR`. Every file of both tiles, compared
against the rung's own bake:

| bake | files compared | differ |
|---|---|---|
| new exe, no flags at all (`id_off`) | 18 | **0** |
| new exe, `--land-sample footprint --blend-edges off` spelled out (`id_def`) | 18 | **0** |
| `--land-sample average` | 18 | 6 |
| `--blend-edges quadrant` | 18 | 6 |
| `--land-sample average --blend-edges quadrant` | 18 | 6 |
| `--land-sample average --land-detail 0.25 --blend-edges quadrant` | 18 | 6 |
| `--land-tiling 2048` | 18 | 6 |

The six that move are the same six every time, three per tile: the **chunk
colour DDS** and the **two `.lodt` containers** that carry its tiles. So
`_msn`, `_data`, the `.lodm`, the BTO, the manifest and the BTR are
byte-identical at every setting of both switches — the compare is shown able to
fail, which is what makes the 0 rows mean something. No `.lodl` is written by
this path at all (none appears in any variant's output), and `lodl_open.sh` is
green below.

**Variant provenance.** `out/<variant>/` directory names are not evidence, so
two of them were re-baked with their arguments typed out in the same step and
compared: `--land-sample average --blend-edges quadrant` reproduced
`out/avgblend/` and `--land-sample average --land-detail 0.25 --blend-edges
quadrant` reproduced `out/k0.25/`, 3 of 3 files each, 0 differ. One variant is
STALE and is not evidence for anything: `out/blendbig/` was baked before the
relink, when `--blend-edges` reached only the chunk path, and it is
byte-identical to the rung — that is the bug, not a measurement.

### F5 — the chain, on the new exe

`scratchpad/tiling2_20260911/chain.sh`, 22:05..22:10, logs in
`logs/c_*.log`:

| harness | this exe | ROADS2's baseline | |
|---|---|---|---|
| `lodl_open.sh` | 23 checks, 0 failures | (not in the block) | new datum |
| `lodgen_terrain.sh` | 26 / 0 | 26 / 0 | same |
| `lodgen_terrain_vt.sh` | 41 / 1 | 41 / 1 | same, and the one failure is the same check by name: `V9b the assembled and direct _msn sheets are byte-identical` — red on the rung too |
| `lodgen_roads.sh` | 11 / 0 | 11 / 0 | same |
| `lodgen_ground_cover.sh` | 29 / 5 | 29 / 5 | same, and the five are the same checks by name (`C1`, `C2` x3, `C6a`, `C9`, `C16`, `C11b` aggregate rows identical to ROADS2's log) |
| `lodgen_terrain_pbrm.sh` | 14 / 0 | 14 / 0 | same |
| `lodgen_native.sh` | 18 / 0 | 18 / 0 | same |
| `lodgen_panel_run.sh` | 125 / 0 | 125 / 0 | same |
| `lod_generation.sh` | 116 / 0 | 116 / 0 | same |
| `ui_align.sh` | 11 / 0 | 11 / 0 | same |
| `water_ui.sh` | 82 / 0, 0 skips | 82 / 0 | same |

**Nothing moved.** No count to explain. The pyramid contract's own tests are
`lodgen_terrain_vt.sh` (V1..V9d), which is in the table. One NifSkope instance
at a time, all of it on the second monitor through `_harness.sh`, and
`tasklist` shows no NifSkope and no `Fallout4.exe` running now.

### The gates, stated as verdicts

| gate | verdict |
|---|---|
| **F1** three laws on vanilla over 22 shipped sheets, each with a floor and a ceiling, BEFORE any code | **PASS.** Section 1. Repeat: 0 of 22 above floor, ceiling 0.264 / 0.448. Edges: w50 median 4.12 over 2.50..6.50, hard-edge median 523, seam median 1.041 ceiling 1.100. Detail: 22.6%/21.5% of variance under 4 texels, spectrum-distance ceiling 0.670 measured vanilla-against-vanilla. |
| **F2** both switches off == rung bytes; `.lodl`/`_msn` untouched at every setting | **PASS.** 18 files, 0 differ, twice; `_msn` and `_data` byte-identical in all seven variants; no `.lodl` on this path. |
| **F3** tiling visibility at or below vanilla's on both tiles, edge widths inside vanilla's histogram, no chunk-border seam | **PASS on (-20,24)** (0.092 against a 0.264 ceiling; w50 4.25 inside 2.50..6.50; seam 0.976 under vanilla's 1.041 median). **PASS on (-20,20) with the reading named**: 0.564 is above vanilla's 0.032 at that bin, and it is proved not to be a texture repeat (the sheet is byte-identical at `--land-tiling` 341.3333 and 2048, and in the road-free window vanilla reads 0.245 where we read 0.376 with the dimensionless numbers 0.255 against 0.131). Seam 0.767, below vanilla. Chunk-border quadrant lines are NOT blended and are named as the limitation; no new seam is created because neither side blends. |
| **F4** detail within 20% of vanilla's local variance and spectrum, or refused with the correlation table | **REFUSED, with the table.** 4.84 against 19.81 is -76%, spectrum distance 3.025 against a 0.670 ceiling. Ten candidates at \|r\| <= 0.006 against twin floors of the same size, with the model validated at r = +0.7948 / +0.7040 against our own bake at zero shift. Nothing to route to GRADE1 either: VCLR reads -0.004 and -0.001, the best of eight lighting azimuths 0.006. |
| **F5** chain at baseline, exe newer than every changed file, drivers rebuilt, rung == launch bytes, no NifSkope left running | **PASS.** Table above: every count identical, nothing to explain. Exe 21:52:22 newer than 21:51:27. The build is `tools/ww_build.sh`, which links the one exe the harnesses drive — there is no separate driver binary in this lane's path, and the harnesses that build their own fixtures (V*, C*, R*) were run after the link and are in the table. Rung 21,458,944 B = the size the brief handed over; sha1 recorded above for the next lane. No NifSkope running.

## 6. Pictures

`scratchpad/tiling2_20260911/images/`, all three in `cmp_tiling_fixed.png`'s
layout — 2x2, a 128-texel crop at 4x nearest neighbour, the same texels in
every panel of a picture, the crop chosen BY THE INSTRUMENT on the BEFORE
artefact, every number burned in, every panel a DDS off disk
(`make_pics.py`, `pics.json`):

| file | crop | how the crop was chosen |
|---|---|---|
| `cmp_tiling2.png` | (224,96), 128 texels | the 128-window where the RUNG bake reads the repeat strongest (2.084). 128 texels = exactly 12 repeats, so the FFT bin stays integer. Vanilla / rung / average+blend / \|rung - average+blend\| x8 |
| `cmp_edges.png` | (384,128), centred on the crossing x=448, y=192 | the quadrant crossing the AVERAGE bake reads worst (1.784 and 3.310 of its own mean gradient). Red ticks OUTSIDE the image mark the two lines. Vanilla / average / average+blend / \|average - average+blend\| x8, which shows the cross-fade's whole footprint: two 4-texel strips and nothing else |
| `cmp_detail_spectrum.png` | (224,96), the same crop as `cmp_tiling2.png` | vanilla / k=0 / k=0.25 / k=0.50, with locVar, the fine-scale variance share, the repeat and the spectrum distance on each — the trade, priced |

A crop's own null floor is higher than a sheet's (12 repeats of evidence, not
48), so the crop numbers are for looking and the whole-sheet numbers beside
them are the verdict. That is said on the picture.

## 7. Owed, red, and bungo's calls

**No default changed.** `--land-sample footprint`, `--land-detail 0`,
`--blend-edges off`: a bake made with this exe and no flags is the rung's bytes
exactly. Nothing in bungo's next bake moves unless he asks for it.

**Still red:**

* **The blur.** -76% of vanilla's local variance at the recommended setting
  (-38% at the rung). REFUSED as a fix, not deferred: section 1c's ten
  candidates and the alignment control say vanilla's fine detail has no source
  in this composite's operands. It cannot be routed to GRADE1 (VCLR and the
  lighting planes read zero too). Someone has to find what Bethesda's bake
  actually ran; this lane proved what it was NOT.
* **The 19-level tone offset** (our non-road ground 69.41 against vanilla's
  88.57 on (-20,20)). GRADE1's, untouched here.
* **`lodgen_terrain_vt.sh` V9b** and the five `lodgen_ground_cover.sh`
  failures: red on the rung too, not this lane's.
* **A quadrant line on a chunk's own edge is not cross-faded** — the
  neighbour's paint is not loaded. Fixing it means loading the adjacent cell's
  LAND in the tile baker.

**bungo's calls, one of them blocking:**

1. **Which defaults do you want?** The recommendation, with its costs already
   priced: `--land-sample average --land-detail 0.20 --blend-edges quadrant` —
   the repeat inside vanilla's law by 2.9x on (-20,24), the quadrant grid below
   vanilla's median on both tiles, and the sheet **blurrier than vanilla**,
   which is the complaint that is not fixed. The alternative is
   `--blend-edges quadrant` alone: the grid goes, the repeat stays. This lane
   changed nothing, because changing the look of every future bake is not a
   lane's call.
2. **The ring-0 consequence** (docs §2.5a): if `--land-sample average` becomes
   the default, the runtime's ring-0 blend must average too, or the ring 0 / ring
   1 cross-fade will show exactly the detail the switch removes.
3. **`--blend-margin`** is 128 world units (4 texels, the opacity grid's own
   spacing). 256 was not measured; the one wide-margin bake on disk
   (`out/blendbig/`) is from before the relink and is void.

**Owed to other lanes:**

* **ROADS3** gets section 1b-road: the road edge is a CONTRAST defect, not a
  width defect (+29.45 over its surround where vanilla's is +3.92; 6 texels of
  transition against vanilla's 6). And it must not chase all 25 levels with the
  road pass — 19 of them are the ground's tone, which is GRADE1's.
* **ROADS2's standing note**: the ground moves under `--land-sample average`,
  so `lodgen_roads.sh` R5 bar 2 and the four-variant composite ranking need
  re-running IF a default changes. They are green on the defaults as they stand.
* **GRADE1** gets a negative result, which is worth as much as a positive one:
  VCLR is not the missing detail (r = -0.004 / -0.001) and no directional
  shading of the shipped slope is either (best of eight azimuths, 0.006).

## 8. Mistakes

Full text in `scratchpad/tiling2_20260911/MISTAKES_ENTRIES.md`; in short:

1. **A colour change that did nothing, and I believed the flag instead of the
   bytes.** `--blend-edges quadrant` produced a byte-identical sheet, and my
   first reaction was to raise `--blend-margin` to 1024 — still identical. The
   cause: the chunk colour DDS is written by the PYRAMID pass, and I had put
   the cross-fade in the stock per-chunk composite. A bake without `--vt`
   writes no tex files at all, which is what proved it. **The rule: a colour
   change has TWO sites, and the one that ships is the pyramid.**
2. **A default that contradicted its own switch.** `g_landDetail` started at
   1.0f, so `--land-sample average` reproduced the footprint bake exactly. It
   looked like the switch was dead; it was the knob. Fixed in the same relink.
3. **The phase twin as a floor.** The brief asked for the phase twin as the
   floor for the periodicity and the spectrum. It cannot be: it preserves the
   power spectrum bin for bin, so every second-order statistic reads the same on
   it. Refused, and replaced with a null sweep at twelve non-commensurate
   periods on the same sheet. The twin IS the right floor for section 1c, which
   is a correlation.
4. **Three instrument designs thrown away before one held** — the
   autocorrelation at a ten-texel lag (drowned by the hillside decay), the
   peak-to-background ratio (explodes to 1e9 on a band-limited synthetic), and
   a 1.5x amplitude error from the Hann kernel's leakage, caught by injecting a
   cosine of known amplitude.
5. **An arbitrary sheet pick.** The first sheet set was picked by eye; it was
   replaced with the 22 shipped dim-4 sheets that pass an SD >= 5.25 trough
   rule, stated before the measurement.
6. **A log overwritten by a targeted re-run.** `logs/t5_variants.txt` was
   rewritten with three rows while the report cited twelve, so the evidence for
   half of section 2 was briefly not on disk. Re-run over every variant at
   22:12. **Measurement scripts that write a fixed filename must be re-run
   whole, or write per-run names.**

## 9. Finished-work skill review

Amended in BOTH trees (`E:\Projects\NifskopeWildWastelandEdition\.claude\skills`
and the live mirror `E:\Projects\Claude\.claude\skills`), and said which:

* **`nifskope-ww-lodgen`** (or the lodgen area skill): the per-chunk colour
  sheet comes from the VT/pyramid path — `--tex-dir` writes nothing without
  `--vt`, and `--cover` is required too. A colour change must be made at BOTH
  sampling sites or it silently does nothing to every file on disk. This cost a
  relink.
* **`ww-control-calibration`**: the phase twin is a floor for a STRUCTURE
  statistic (a correlation against another field) and worth nothing as a floor
  for a periodicity or a spectrum, because it preserves the power spectrum
  exactly. The floor for "is there a repeat at this period" is a null sweep at
  periods that are not the repeat, on the same sheet.
* **`ww-control-calibration`**, second note: a correlation of zero proves
  nothing until an ALIGNMENT control proves the same pipeline reads r ~ 0.8
  against a field it does reproduce. Section 1c's refusal rests on that control,
  not on the zeros.
* **`ww-texel-picture`**: a crop's own null floor is much higher than a sheet's
  (fewer periods of evidence), so a crop is for looking and the verdict number
  must be the whole sheet's, printed beside it.

One tooling note that is not a skill: a `bash` heredoc carrying markdown or
Python with backticks in it fails in this harness (`unexpected EOF while
looking for matching quote`) and silently drops the write. Large text goes
through the `Write` tool or a heredoc with no backticks. It cost two lost
appends of this report.
