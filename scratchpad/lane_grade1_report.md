# Lane GRADE1 -- the tone of the far terrain

Brief `scratchpad/brief_grade1.md`. Lane dir `scratchpad/grade1_20260911/`.
Exe at launch `release/NifSkope.exe` 2026-09-12 02:08:57, 21,487,616 B
(TILING4's DONE exe). Rung taken ONCE: `release/NifSkope.before_grade1.exe`.
Report written incrementally, appended, never rewritten.

---

## 0. Pre-registered gates

Written 02:43, before a single number was measured.

| gate | what it demands |
|---|---|
| **G1** | the known-answer controls recover a synthetic gamma and a synthetic gain to **3 decimals** before any verdict is stated. A synthetic tile is built from vanilla's own sheet with a known slip applied, run through the same fitter. |
| **G2** | the fingerprint is named with residuals for **all three models on both tiles** -- constant gain, gamma, and the position-dependent residual map correlated against height, slope, AO and VCLR. Chroma (saturation) reported the same way. |
| **G3** | the switch OFF == the rung's bytes, `cmp` on every file of both test tiles; with it ON the whole-tile colour error against vanilla is **reduced on both tiles**, with ground / cover>0 / road texels reported separately, and the road's own colour error inside the ground's tolerance. Copied vanilla sheets (`_msn` everywhere, colour on layerless chunks) stay **byte-identical** through the tone change. |
| **G4** | the grass tint strength is decided by a **fitted number**, not a preference: ship the fit only if it beats 0.35 by more than the floor, else keep 0.35 and say so. |
| **G5** | exe newer than every changed file; drivers rebuilt; the rung == the launch exe's bytes; no NifSkope left running. |

### The instruments and their floors, registered now

* Matched texels: both sheets are dim-4 chunk sheets of the SAME chunk on the
  same 512x512 grid at 32 world units a texel, so texel (i,j) is the same
  world point on both. PIC-CHUNK already proved the registration (identity
  beats all six flips/rotations, cross-correlation peak within one texel); it
  is re-asserted here as a control, not assumed.
* The first pass excludes **road texels** (vanilla paints roads into its sheet
  and the reference bakes carry none) and **cover>0 texels** (the grass tint
  is a separate term with its own gate, G4), so the curve is fitted on bare
  ground blends only.
* Fit floors: a fit is only believed if the same fitter, run on a synthetic
  tile carrying a KNOWN slip, returns that slip to 3 decimals (G1), and if the
  residual of the winning model is compared against the residual of a
  deliberately wrong model on the same texels.
* Correlation floors: every residual-vs-field correlation is reported beside
  a **phase twin** of the same field (same amplitude and spectrum, phase
  randomised) as the floor. TILING2's ruling is honoured -- a phase twin is a
  floor for a STRUCTURE statistic only and never for a periodicity.

### Rules this lane operates under

One build plus counted relinks; `Fallout4.exe` checked before the link;
bungo's own NifSkope (no `--port`) renamed aside, never killed; lane UINOTES1's
NifSkope from `E:/Projects/NifskopeWWE_ui` waited for before any GUI launch and
never touched; region bakes into this lane's own out-dir only; his installed
`Data\Terrain` never written; nothing committed, nothing stashed.

---

## 0a. The ship thresholds, pre-registered

Section 0 registered the lane's gates before any measurement. These five are the
SHIP gates for whatever change came out of section 2, and they were written into
`g6_gates.py`'s docstring before the first graded bake was read back -- that file
is the record, and the thresholds below are copied from it, not fitted to it.

| id | the threshold, as registered | why that number |
|---|---|---|
| S1 | with no flag, and with the switch's off value stated explicitly, EVERY file of both test tiles is byte-identical to the rung's bake | the brief's "off == the rung's bytes". Byte equality has no tolerance to fit, which is why it is the first gate |
| S2 | the binary reproduces the Python prediction for a given k to within **1.0 level** of RGB RMS | two gap sources are named IN ADVANCE and neither is a bug: the sheet is re-encoded to BC1 after grading (measured floor 0.497 and 0.527) and the crevice term runs after the grade in the binary but is already inside the sheet Python scales, so the two differ by `crevice*(1-k)`. 1.0 is about twice the codec floor and an order below the effect being measured (~2.5 levels) |
| S3 | `--grade k_opt` strictly reduces the whole-tile RGB RMS on the tile whose optimum it is, on BOTH tiles, each with its OWN k | the weakest claim that still discriminates: if the knob cannot help a tile with that tile's own best number, it does nothing at all |
| S4 | the REFUSED gate, as an assertion and not as prose: **no single k reduces the error on both tiles**, checked against the binary's own output with the pooled k = 0.8403 | `gate-refused-with-numbers` step 5. A later lane that wants to claim a global grade has to delete an assertion that says why it is not there |
| S5 | the flag is new: the rung exe exits 2 with `unknown option --grade` | proves the old binary could not have produced the graded arms, so S2/S3 are not measuring the same exe twice |

S3 and S4 are deliberately in tension, and that tension IS the finding: S3 says
the mechanism works, S4 says no constant can be shipped on. Both are asserted
against the compiled binary in `g6_gates.py`.

## 1. The curve

### 1.0 Gate G1 -- the known-answer controls, green before any verdict

`g0_controls.py`, log `logs/g0_controls.txt`, **24 checks, 0 failures**, both
tiles.

| control | (-20,24) | (-20,20) |
|---|---|---|
| C0 shapes | 512x512x3 both sides | same |
| C1a identity beats all six flips/rotations (MAE) | 16.205, next best flipud 16.232 | 18.215, next 20.649 |
| C1b cross-correlation peak within one texel of (0,0) | peak (0,0), **r = 0.0335** | peak (0,0), r = 0.3140 |
| C4 BC1 codec floor (our own sheet re-encoded) | RMS 0.497, MAE 0.176 | RMS 0.527, MAE 0.197 |
| C2 synthetic gamma 1.250 / 0.800 / 1.350 recovered | 1.250000 / 0.800000 / 1.350000, a = 1.000000, 0 dropped | same to six decimals |
| C3 synthetic gain 0.820 / 1.200 / 0.950 recovered | exact to six decimals, affine agrees (c = 0.000) | same |
| C5a a gamma fed to the GAIN fitter must not be absorbed | gain residual RMS 3.591 vs gamma 0.0000, floor 0.497 | 4.205 vs 0.0000, floor 0.527 |
| C5b a pure gain fed to the GAMMA fitter returns g = 1 | g = 1.000000, a = 0.820000 | same |

The gate asked for three decimals; the fitters return six. Two numbers in that
table are reported as caveats and not as passes:

* **C1a's margin on (-20,24) is 0.027 MAE** (16.205 against 16.232). The
  registration is right -- the two sheets are the same chunk on the same grid by
  construction, and the cross-correlation peak is at (0,0) -- but on that tile
  the test barely discriminates, because
* **C1b's peak correlation is r = 0.0335.** Our sheet and vanilla's agree on
  almost nothing texel to texel on (-20,24). On (-20,20) it is r = 0.3140.
  That single number turns out to be the whole finding, and 1.3 makes it
  quantitative.

### 1.1 The three models, ground texels only

Road texels are excluded (mask = the shipped default bake differenced against a
`--no-roads` bake of the same command line; see section 7 -- the first road mask
was wrong). Cover>0 texels would be excluded too, but **there are none**: the
ground-cover plane is empty on every tile in this lane's census (1.6).

`g1_curve.py`, log `logs/g1_curve.txt`. Residuals in 8-bit levels on luminance,
beside each sheet's own BC1 codec floor.

**(-20,24)** -- ours mean 87.606, vanilla mean 80.819, n = 262,144 (no roads on
this tile at all)

| model | parameters | RMS | MAE | bias |
|---|---|---|---|---|
| identity (do nothing) | -- | 19.937 | 16.205 | +6.787 |
| constant gain | k = 0.8916 | 17.439 | 13.755 | -2.709 |
| affine | k = 0.0193, c = 79.131 | 9.472 | 7.371 | 0.000 |
| gamma | a = 0.3281, g = 0.0379 | 9.483 | 7.332 | -0.532 |
| sRGB slip, ours read linear -> written sRGB | fixed | 78.511 | 76.720 | +76.718 |
| sRGB slip, ours read sRGB -> written linear | fixed | 56.635 | 55.085 | -55.085 |
| *BC1 codec floor* | -- | *0.497* | *0.176* | -- |

**(-20,20)** -- ours mean 68.767, vanilla mean 83.464, n = 236,955 ground texels
(25,189 road texels, 9.61 %, excluded)

| model | parameters | RMS | MAE | bias |
|---|---|---|---|---|
| identity | -- | 22.652 | 18.533 | -14.697 |
| constant gain | k = 1.1612 | 19.587 | 15.783 | -3.612 |
| affine | k = 0.1848, c = 70.756 | 11.317 | 9.025 | 0.000 |
| gamma | a = 0.3995, g = 0.1565 | 11.361 | 9.042 | -0.763 |
| sRGB slip (linear -> sRGB) | fixed | 59.645 | 57.372 | +57.364 |
| sRGB slip (sRGB -> linear) | fixed | 68.545 | 67.436 | -67.436 |
| *BC1 codec floor* | -- | *0.527* | *0.197* | -- |

Per channel the shape is the same; R/G/B gains are 0.8697 / 0.8972 / 0.9048 on
(-20,24) and 1.1233 / 1.1716 / 1.1786 on (-20,20).

Three things are already settled by that pair of tables.

1. **Both sRGB slips are refuted by two orders of magnitude.** 56 to 78 levels
   of error against an identity of 20 to 23. Not a near miss to be tuned: the
   colour path has no conversion in it to slip (contract 2.5, and
   `src/lodgen.cpp` 7952-7975 / 9258-9270 -- bytes in, bytes out), and the
   measurement agrees with the code.
2. **The gain's sign flips between the two tiles.** (-20,24) wants k = 0.892 (we
   are too BRIGHT), (-20,20) wants k = 1.161 (we are too DARK).
3. **The affine and gamma "wins" are degenerate and must not be quoted as
   models.** The affine slope is 0.019 and 0.185; the gamma exponent 0.038 and
   0.157. Both collapsed onto "predict vanilla's mean, ignore ours" -- their RMS
   of 9.5 / 11.3 is vanilla's own standard deviation on those texels (9.478 and
   11.899). A model that ignores its input is not a transfer curve, and the
   reason it wins is C1b: there is almost nothing to transfer.

### 1.2 Saturation, the discriminator meant to separate gain from gamma

Measured on the same texels (HSV S):

| tile | ours | vanilla | a gain predicts | the fitted gamma predicts |
|---|---|---|---|---|
| (-20,24) | 0.2453 | 0.2215 | 0.2453 (unchanged, exactly) | 0.0107 |
| (-20,20) | 0.2763 | 0.2206 | 0.2763 (unchanged, exactly) | 0.0498 |

Ours is more saturated than vanilla on both tiles and on all 25 of the census,
by a ratio of about 1.11. A scalar gain leaves S untouched, so it explains none
of it; the fitted gamma would destroy it, overshooting by a factor of twenty.
Neither model is the mechanism. 1.5 asks whether a chroma pull on its own is
worth shipping; the answer is no, by number.

### 1.3 The position-dependent term, and a mistake in how it was first read

The brief asks for the best global model's residual mapped and correlated. The
best global model BY RMS is the affine one -- and its residual is, by
construction, `constant - vanilla`, because its slope is ~0. Correlating that
against height or slope measures **vanilla's** structure, not ours. The first
run of `g1_curve.py` reported exactly that; section 7 records it as a mistake
caught by its own control -- the `ours lum` row read r = -0.0000, which is the
tell.

The honest version correlates the residual of the constant-gain model, and the
raw difference `ours - vanilla`, against each field, each beside a phase twin
(`g2_position.py`, log `logs/g2_position.txt`, all texels):

| field | (-20,24) r (twin) | (-20,20) r (twin) |
|---|---|---|
| height (VHGT, parsed in this lane) | -0.287 (-0.325) | +0.005 (+0.092) |
| slope (from the `_msn` UP channel) | -0.166 (-0.001) | -0.050 (+0.002) |
| AO (our `_data` R) | +0.218 (-0.080) | -0.045 (+0.025) |
| VCLR luminance | +0.011 (+0.048) | -0.018 (-0.021) |
| **our own luminance** | **+0.835** (-0.056) | **+0.827** (+0.012) |
| vanilla's luminance | -0.522 (+0.022) | -0.274 (-0.026) |

* **height is AT its phase-twin floor** on both tiles. It is not a hill-shading
  term.
* slope and AO clear their floors on (-20,24) and do not on (-20,20), with
  opposite signs. Nothing consistent.
* the residual's one strong, sign-consistent partner on both tiles is **our own
  brightness**, r = +0.83: after the best gain we are still too bright where we
  are bright and too dark where we are dark. Our sheet's dynamic range differs
  from vanilla's. That is a content statistic, not a tone one.

### 1.4 VCLR is ruled out for tone, by its own value

We multiply the composite by `VCLR/255` (`src/lodgen.cpp` 7877-7893, 9187-9199).
Earlier lanes ruled it out for local variance and left it open for tone. It is
now closed, because the multiplier itself was measured rather than the record:

* the per-texel VCLR field's mean luminance is **254.96, 254.88, 254.89, 254.97,
  254.97, 254.97** on the six first tiles -- the multiply is 0.9998 of unity;
* dividing it back out of our sheet and refitting moves the gain by **0.0003**
  (0.8916 -> 0.8915 and 1.1180 -> 1.1173) and the RMS by 0.01 levels;
* saturation over the census is 0.2428 with the multiply and 0.2429 without;
* the natural experiment on (-20,24), where 31 % of texels sit in cells with no
  VCLR record at all, gives k = 0.9242 with a record and 0.8339 without -- a
  difference in the direction OPPOSITE to the one the multiply could cause,
  which is another way of saying those are simply different cells.

VCLR is not the grading it looks like, and that is now a measured statement
about the multiplier rather than an inference from the records.

### 1.5 The census: 25 tiles, and what one global constant can do

Two tiles cannot carry a verdict about a map, so the lane baked a 5x5 grid of
dim-4 chunks at cells (-24..-8, 12..28) with the launch exe at the shipped
defaults -- 25 tiles, `g4_census.py` / `g5_decide.py`, logs `logs/g4_census.txt`
and `logs/g5_decide.txt`.

| statistic | value |
|---|---|
| per-tile optimum gain k | **0.615 .. 1.241**, mean 0.892, sd 0.144 |
| tiles wanting k < 1 (we are too bright) | 21 of 25 |
| tiles wanting k > 1 (we are too dark) | 4 of 25 |
| per-CELL optimum k over the 96 cells of the first six tiles | 0.699 .. 1.388, mean **1.004**, sd 0.149 |
| chroma pull s fitted per tile (`c' = L + s(c-L)`) | -0.256 .. 1.301, mean 0.885, sd 0.314 |
| one global chroma pull s = 0.9156 | reduces RGB RMS on 16 of 27 rows, by at most 0.15 levels |
| one global gain k = 0.8403 (the pooled optimum) | pooled RGB RMS 24.724 -> 19.644 (-20.5 %), better on **19 of 25**, worse on 6 |

The chroma pull is not shippable: its per-tile scatter includes a negative value
(a tile whose best "desaturation" is a hue inversion), and its best global
setting moves the error by a tenth of a level -- a fifth of the BC1 codec floor.

The exposure is a real effect in the aggregate -- 20.5 % off the pooled error --
and still cannot pass this lane's own gate G3. Section 2 and 3 are why.

### 1.6 The grass tint (gate G4), answered by a number that is zero

The cover plane is **empty on all 25 tiles**. Every `_data` sheet carries the
writer's own honest stamp: `dwReserved1 = 0`, which `src/lodgen.cpp` 8196 and
9749 set exactly when `coverMax == 0`, and the decoded DXT5 alpha confirms it
(`cover>0` on 0.000 % of texels, mean byte 0.00, on every tile).

So the tint term `colour += (tint - colour) * cover/255 * tintStrength` never
fires anywhere in the census: at cover = 0 the branch is not taken at all. There
is no texel on which to fit a strength, and any number this lane "fitted" would
be fitted to nothing. **0.35 stands, unchanged, and the reason is that the term
is inert on every sheet measured, not that 0.35 won a comparison.** That the
cover plane is empty across a 5x5 chunk grid that includes Sanctuary is a red
handed on in section 6; it invalidates nothing above, since a zero cover byte
also means zero tint in the bytes being compared.

---

## 2. The cause, named

**The fingerprint is: no global tone transfer exists. The far-terrain colour
error against vanilla is a per-cell CONTENT difference with a near-zero mean --
not an exposure, a gamma, a colour-space slip, or a lighting term.**

The evidence, in the order that kills each candidate:

1. **Colour-space slip -- refuted by the code and by 56 levels of measurement.**
   The whole colour path is bytes/255, blended in that space and quantised back
   (`src/lodgen.cpp` 7952-7975 and 9258-9270): there is no conversion to slip.
   Both fixed slip candidates score 56 to 78 levels of RMS against an identity
   of 20. The skill `fo4-engine-constant-from-ini-setting` was NOT invoked,
   because its own precondition -- "the fingerprint says position-independent
   gain AND our code shows no slip" -- fails on its first clause.
2. **Constant gain -- refuted by its own sign.** k = 0.892 on (-20,24) and 1.161
   on (-20,20) (ground texels; 0.892 / 1.118 whole-tile). Over 25 tiles k runs
   0.615..1.241 with sd 0.144, and over 96 individual land cells 0.699..1.388
   with **mean 1.004**. On average our exposure is already vanilla's; the error
   is that individual cells are wrong in both directions.
3. **Gamma -- refuted by degeneracy and by saturation.** The fitted exponents
   (0.038, 0.157) are not gammas, they are the regression collapsing onto
   vanilla's mean; and a gamma that size would drop saturation from 0.25 to 0.01
   where the measured difference is 0.25 -> 0.22.
4. **VCLR -- refuted by its own value**, 254.9 of 255 (1.4).
5. **A lighting term -- refuted at its floor.** The residual's correlation with
   height sits AT its phase twin on both tiles; slope and AO clear their floors
   on one tile each, with opposite signs.
6. **What is left, and what it points at.** The residual's only consistent
   partner is our own brightness (r = +0.83 on both tiles); the per-cell gains
   scatter with sd 0.15 about 1.00; and the per-cell pattern is blocky -- on
   (-20,24) one row of four cells fits k = 1.04..1.24 while the other twelve fit
   0.80..0.93, and four of those fit 0.813..0.824 within 1 %. Cell-quantised
   structure with a mean of one is the signature of **which textures we blend in
   which cell**, not of how the result is graded. The multi-scale table makes
   the same point from the other side: on (-20,20) the correlation with vanilla
   climbs 0.31 -> 0.80 as the box average grows from 1 to 128 texels (coarse
   structure agrees, fine detail does not), while on (-20,24) it never rises
   above 0.041 at any scale -- that tile's composite disagrees with vanilla's at
   every scale, which is a splat/base-selection question and belongs to the lane
   that owns the composite.

So there is no code line of ours to name as the bug and no engine constant to
read out of `Fallout4.exe`. The line the brief expected to find -- an sRGB
conversion on our side -- does not exist, and this lane's contribution is to
close that door with numbers.

### 2.1 The correction this lane owes the project

The belief on record (ROADS2, ROADS3, and the 16:3x ruling) is that "the
off-road ground is ~16 luminance units too dark" and that ~19 levels of the
road's over-contrast is a whole-sheet tone offset that GRADE1 would remove. On
the FIXED tiling and composite that is true only of the Sanctuary tile:

| region | ours | vanilla | ours - vanilla |
|---|---|---|---|
| (-20,20) ground -- the tile that observation came from | 68.767 | 83.464 | **-14.70** |
| (-20,20) road footprint (25,189 texels) | 98.753 | 92.485 | **+6.27** |
| (-20,24) ground | 87.606 | 80.819 | **+6.79** |
| census, 25 tiles, mean per-tile optimum k | -- | -- | **0.892 -- we are too BRIGHT on 21 of 25** |

The road does stand ~30 levels over its surround in ours against ~9 in vanilla
on (-20,20) -- but of that 21-level discrepancy, **6.3 levels are the road being
too bright and 14.7 are the ground being too dark**, and the ground half
reverses sign one chunk to the north. A whole-sheet grade was the wrong owner
for it. The road pass needs no separate treatment from this lane: the grade
below is applied after the road composite, so a road is graded with the ground
it sits in, by construction.

---

## 3. The change

`--grade K`, one multiply, in both colour writers.

```cpp
/* THE GRADE, last before quantisation and after the road and the
 * tint, so the road is graded with the ground it sits in (lane
 * GRADE1). The crevice term runs LATER still, on the finished
 * 8-bit sheet, and is deliberately not scaled: -3.242 was fitted
 * in levels against vanilla's own residual. At 1.0 this branch is
 * not taken at all, which is what makes the off value the previous
 * bake's bytes rather than a float argument about 1.0f. */
if ( g_landGrade != 1.0f )
    for ( int k = 0; k < 3; k++ )
        color[k] *= g_landGrade;
```

| where | line | what |
|---|---|---|
| the state | `src/lodgen.cpp:6260` | `static float g_landGrade = 1.0f;` |
| the accessors | `src/lodgen.cpp:6300, 6305` | setter clamps to 0..4 |
| the stock chunk writer | `src/lodgen.cpp:7962` | the two lines above |
| the pyramid writer (the one that ships) | `src/lodgen.cpp:9258` | the same two lines |
| the census, JSON and text | `src/lodgen.cpp:8035, 10110` | `landGrade` |
| the declarations | `src/lodgen.h:270` | |
| the flag | `src/nifcli.cpp:6166` | `--grade` |

Four decisions inside that, each with its reason:

**The off value is a branch, not an argument about floats.** `x * 1.0f` is
exact in IEEE for every finite `x`, so a multiply would also have been
byte-identical -- but proving that requires an argument about rounding modes and
about what the compiler does with `-ffast-math` on some future build, and the
brief asked for "byte-identical to the rung" as a property of the change, not of
the toolchain. Skipping the branch makes it structural. It was still MEASURED
(gate S1, below), because a property one reasons about is not a property one has
checked.

**Where in the order.** After the layer blend, after the VCLR multiply, after
the road composite and after the grass tint; before quantisation to bytes.
Grading after the road means a road is graded with the ground it sits in, so the
brief's "road colour within the ground's tolerance" holds by construction at any
k rather than needing its own number. Grading before quantisation means the
rounding happens once.

**The crevice term is deliberately NOT graded.** `dL = -3.242 * div(n)` runs
after the per-texel loop, on the finished 8-bit sheet, in LEVELS -- it was fitted
against vanilla's own residual in levels, so scaling it by k would move a
constant that was measured against the thing we are grading towards. This is the
one term the grade skips, and it is the larger half of the S2 gap.

**Both writers, not just the pyramid.** The pyramid is what ships, but the stock
per-chunk sheets are what the unmodified engine reads, and a knob that made the
two disagree would be a trap for the next lane. The anchor `if ( g_landGrade !=
1.0f )` occurs exactly twice in `lodgen.cpp` and that count is the claim.

**And the value that ships is 1.0** -- see gate S4. The knob exists so the
question can be asked again on other worldspaces with one flag and no build; it
does not exist because a value was found.

## 4. Build and gates

### 4.1 The build

One build, no relinks.

```
bash tools/ww_build.sh src/lodgen.cpp src/lodgen.h src/nifcli.cpp
  exe not held by a window          (no NifSkope process at all; Fallout4.exe absent)
  BUILD-RC=0
  exe newer than the sources
  copies in step                    (res/style.qss == release/style.qss)
```

| | path | time | bytes | md5 |
|---|---|---|---|---|
| the rung (TILING4's DONE exe, = this lane's launch exe) | `release/NifSkope.before_grade1.exe` | 2026-09-12 02:08:57 | 21,487,616 | `8a1a1e718c6d6822ad0d60b90803fd69` |
| this lane's | `release/NifSkope.exe` | 2026-09-12 03:06:21 | 21,489,152 | `6af74b4b4667ce50c4506a2d42a04fdf` |

The rung was preserved by copy before the build and is byte-for-byte the exe the
brief names, so every "the rung's bytes" claim below is against the launch
binary and not against a rebuild of it.

### 4.2 The ship gates, measured through the binary

`g6_gates.py`, log `logs/g6_gates.txt`: **8 checks, 0 failures.**

**S1 -- off == the rung's bytes.** Both test tiles baked by the new exe with no
flag, and again with `--grade 1.0`, against the rung's bakes: colour, `_data`,
`_msn`, the two `.lodt` levels and the `.lodm`, twice per tile per arm --
**24 of 24 files byte-identical**. Not "no visible difference": `cmp`.

**S2 -- the binary agrees with the model.** Tolerance 1.0 level of RGB RMS,
registered in 0a with both gap sources named in advance.

| tile | k | before | Python predicts | the binary gives | gap |
|---|---|---|---|---|---|
| (-20,24) | 0.8916 | 20.127 | 17.524 | 17.555 | +0.031 |
| (-20,20) | 1.1180 | 21.897 | 20.410 | 20.544 | +0.134 |
| (-20,24) | 0.8403 | 20.127 | 18.000 | 18.108 | +0.108 |
| (-20,20) | 0.8403 | 21.897 | 28.316 | 28.488 | +0.173 |

Every gap is positive and grows with |1-k|, which is the ungraded crevice term
behaving exactly as predicted; the BC1 floor (0.497 / 0.527) accounts for the
rest. The model in section 1 therefore describes the shipping code, not a
prototype.

**S3 -- the mechanism works.** (-20,24) 20.127 -> 17.555 (-12.8 %); (-20,20)
21.897 -> 20.544 (-6.2 %). Each with its OWN k.

**S4 -- the refusal, as an assertion.** The pooled census optimum k = 0.8403,
through the binary: (-20,24) 20.127 -> 18.108 (better) and (-20,20) 21.897 ->
**28.488** (worse by 30 %). The assertion is that exactly one of the two
improves, and it passes. This is the gate the brief asked for -- "after, whole-
tile error vs vanilla reduced on both tiles" -- and it is **refused, with the
arithmetic in section 2 and the number here**, per `gate-refused-with-numbers`:
the error in k is a parabola whose vertex is the tile's own k_opt, the two
vertices straddle 1, so no constant can satisfy it and 1 is the off value.

**S5 -- the flag is new.** The rung exe exits 2 with `error: unknown option
--grade`, so no arm above could have come from it.

### 4.3 The brief's gates

| | gate | result |
|---|---|---|
| G1 | known-answer controls recover a synthetic gamma and gain to 3 decimals before any verdict | **PASS** -- 24 checks, 0 failures, both tiles, `logs/g0_controls.txt`, run before section 1 was written |
| G2 | the fingerprint named, with residuals for all three models on both tiles | **PASS** -- section 1.1's table and section 2 |
| G3 | switch off == rung bytes; then whole-tile error reduced on both tiles, ground/cover/road separately, road within the ground's tolerance | **HALF PASS, HALF REFUSED**. Off == rung bytes: 24/24. Ground/cover/road: reported in 1.5 and 2.1 (cover is empty, n=0, see G4). Road within the ground's tolerance: by construction -- the grade is applied after the road composite, so the road's ratio to its ground is invariant in k. "Reduced on both tiles": **refused with arithmetic and asserted as S4** |
| G4 | tint strength decided by a number | **ANSWERED BY A ZERO** -- the cover plane is empty on all 25 census tiles, the tint branch never executes, so no texel exists on which a strength could be fitted. 0.35 stands and the empty plane is a red (section 6) |
| G5 | exe newer than every changed file; drivers rebuilt; rung == launch bytes; no NifSkope left running | **PASS** -- see below |

G5 in detail. `test release/NifSkope.exe -nt <f>` is true for all three changed
sources (the doc and this report were written after the link and are not compiled
inputs). **Gate drivers: there are none to rebuild** -- every one of the eight
harnesses executes `release/NifSkope.exe` and nothing else, checked by grepping
the eight scripts for `release/*.exe`. The rung is byte-identical to the launch
exe (md5 above). `Get-CimInstance Win32_Process` for `NifSkope.exe` and
`Fallout4.exe` returns **nothing** -- checked immediately before the link and
again at the end of the lane. Lane UINOTES1's tree (`E:\Projects\NifskopeWWE_ui`)
was never touched and no GUI was launched by this lane at all: every instrument
here is `-no-gui`.

### 4.4 The harness chain

All eight at ROADS2's baselines, exactly:

| harness | this lane | ROADS2's baseline |
|---|---|---|
| lodgen_terrain | 26 checks, 0 failures | 26/0 |
| lodgen_terrain_vt | 41 checks, 1 failure | 41/1 |
| lodgen_roads | 11 checks, 0 failures | 11/0 |
| lodgen_ground_cover | 29 checks, 5 failures | 29/5 |
| lodl_open | 23 checks, 0 failures | 23/0 |
| lod_generation | 116 checks, 0 failures (floor 116) | 116/0 |
| lodgen_native | 18 checks, 0 failures | 18/0 |
| lodgen_terrain_pbrm | 14 checks, 0 failures | 14/0 |

The two reds are pre-existing and are not this lane's: they were red at the same
counts on the rung. Why these eight and not others: the change is in the two
colour writers and in the CLI, so every harness that bakes terrain colour, roads,
cover or the VT pyramid is in scope, plus the two that open the products. The
change is inert at the default, so a green chain is the expected result and the
gate that discriminates is S1's byte equality, not these counts -- which is why
S1 is a gate and this is a regression check.

## 5. Pictures

Both were specified in `g7_pictures.py`'s docstring before either was drawn, and
both were opened and read afterwards -- which is how two defects were caught
(section 7).

### `images/cmp_tone.png` (1444x946)

**What it must show:** that the two reference chunks want grades in OPPOSITE
directions, on the sheets themselves, with the numbers burned in so no caption
can drift from them. Two rows, one per chunk; four panels a row -- vanilla as
shipped, ours at the rung, ours at that chunk's own best grade, and the
luminance difference of the graded sheet against vanilla at +-40 levels, blue
where we are darker and red where we are brighter.

**What it does show.** Row 1, chunk (-20,24): vanilla mean luminance 80.82,
ours 87.61 -- we are the brighter sheet, and `--grade 0.8916` brings it to 78.13
with RGB RMS 20.127 -> 17.555. Row 2, chunk (-20,20): vanilla 84.33, ours 71.65
-- we are the DARKER sheet, and the correction runs the other way,
`--grade 1.1180` to 79.99, 21.897 -> 20.544. The two difference maps are the
finding: row 1 is red over most of its area with a blue band along the northern
ridge, row 2 is blue over most of its area -- and in row 2 the road traces
through it in solid red, which is the +6.27-level road bias of section 2.1 made
visible. Neither map is a flat tint, which is what a grade error would look like.
Saturation is printed on every panel and barely moves between the rung and the
graded sheet (0.2453 -> 0.2461, 0.2655 -> 0.2645), because a gain cannot move it
-- the chroma finding of section 1.2, visible.

### `images/curve.png` (1232x1058)

**What it must show:** why the affine and gamma fits are not transfer curves,
and why no constant ships. Three panels a row. Left: the log-density scatter of
ours (x) against vanilla (y) over the ground texels with all three fitted curves
drawn over the identity. Middle: the histogram of per-region optimal gains --
the 96 land cells on row 1, the 25 census tiles on row 2 -- with 1.0 marked.
Right: the spatial map of the residual after that tile's best gain.

**What it does show.** In both scatters the affine (blue) and gamma (green)
curves are nearly FLAT -- they leave the identity at the left edge and run
horizontally through the cloud's centre, which is a regression predicting the
mean of its target and nothing else (slopes 0.019 and 0.185; their RMS, 9.47 and
11.32, is vanilla's own standard deviation, 9.478 and 11.899). The gain (red)
is the only line that stays a line through the origin. The cloud itself is a
blob, not a ridge: r = 0.034 and 0.314. The middle panels are the refusal drawn
-- both histograms are broad and straddle the 1.0 rule, cells 0.699..1.388 mean
1.004, tiles 0.615..1.241 mean 0.892, with the two reference tiles marked on
opposite sides and the pooled optimum 0.8403 marked as a line that sits outside
most of the mass. The right-hand maps are blotchy at a scale of tens of texels,
not smooth gradients -- cell-shaped, which is the fingerprint -- and their
captions carry the correlation with our own luminance, +0.835 and +0.827 against
phase twins of -0.056 and +0.012.

## 6. Owed / red / bungo's calls

**Red 1, and the biggest: the ground-cover plane is EMPTY on all 25 census
tiles, Sanctuary included.** Every `_data` sheet carries `dwReserved1 = 0`, which
`lodgen.cpp:8196` and `:9749` set exactly when `coverMax == 0`, and the decoded
DXT5 alpha is zero on 100 % of texels. So the grass tint branch never executes
anywhere in the Commonwealth -- the `tintStrength 0.35` constant has never
affected a shipped byte. This lane could not fit it (gate G4 answered by a zero)
and did not try to fix it: the defect is in whatever fills the cover channel,
upstream of the colour law. **It needs a lane.** Vanilla's own sheets should be
checked for the same emptiness first -- if vanilla's cover is also zero, the
feature is cosmetic in the engine too and 0.35 never mattered; if vanilla's is
populated, we are dropping a ground-cover source.

**Red 2: two census tiles are near-achromatic and over-bright.** (-12,28)
saturation 0.054, mean luminance 124.3; (-16,28) saturation 0.018, luminance
126.8 -- against a census mean of 0.243 and about 85. Their RGB RMS against
vanilla is 51.2 and 48.6 where about 20 is typical. A saturation of 0.018 is
grey, and grey at luminance 127 looks like a base texture that failed to resolve
and fell back to a mid-grey. They are the two worst tiles in the census by a
factor of two and they drag the pooled optimum down; excluding them would raise
it. Worth one lane's morning.

**Red 3: the road materials do not resolve from the unpacked data root.** The
bake asks for `materials/c:/projects/fallout4/...` -- a Bethesda absolute path
baked into a record -- and finds it in no archive, so road colour is a fallback.
That is a plausible part of the road's +6.27-level brightness bias and this lane
did not chase it.

**Bungo's call 1: the pooled grade.** If a single number is ever wanted for the
Commonwealth, it is **0.8403**: pooled RGB RMS 24.724 -> 19.644 (-20.5 %),
better on 19 of 25 tiles, worse on 6 -- and one of the 6 is a reference tile,
which is why this lane does not ship it as a default. `--grade 0.8403` is one
flag away and needs no build. **My recommendation is not to**, because the mean
over land cells is 1.004: the average is already right and a global grade trades
one half of the map against the other.

**Bungo's call 2: what the saturation gap deserves.** We are more colourful than
vanilla on every tile measured (0.243 vs 0.219). No brightness model explains it
and the chroma pull that would correct it is worth 0.15 of a level, a fifth of
the codec floor. It is real, it is small, and it is the same CONTENT question as
the tone: which textures we blend where. It belongs with red 4.

**Red 4 / the owed lane: the per-cell content difference.** This is the actual
cause named in section 2 and it is not a colour bug. The residual after the best
gain correlates with our own luminance at +0.83, the per-cell gains are blocky,
and the multi-scale correlation on (-20,20) climbs from 0.31 at one texel to
0.80 at 128-texel boxes while on (-20,24) it never exceeds 0.041 at ANY scale --
two chunks that differ in different ways. The next lane is a LAYER BLEND lane:
which base texture wins a quadrant when BTXT is 0, how the 17x17 opacities are
bilinearly resolved, and whether our dominant-base choice matches the engine's.
`fo4-engine-constant-from-ini-setting` is the wrong tool for it and was not
invoked here -- its precondition is a position-INDEPENDENT gain, and the gain is
position-dependent by 0.6 of its own value.

**The record correction owed to the project.** "The off-road ground is about 16
luminance units too dark" is in the record from an earlier lane. It is true of
ONE chunk. On (-20,20): ground 68.767 vs vanilla 83.464 (-14.70), road 98.753 vs
92.485 (+6.27). On (-20,24), one chunk north, the ground is +6.79 -- brighter,
not darker. Of the road's roughly 21 levels of over-contrast against its ground,
6.3 are the road and 14.7 are the ground, and the ground half reverses sign one
chunk away. The `--grade` documentation in the contract now carries this and so
does the handoff block.

**Not done, deliberately:** no change to `tintStrength`, no chroma pull, no
default grade, no touch to the crevice constant, and no second build. The lane's
one change is inert until asked for.

## 7. Mistakes

Four, all written to `scratchpad/grade1_20260911/MISTAKES_ENTRIES.md` for the
root ledger, in full. In brief:

1. **I read a flag's name instead of the census and measured an empty road
   mask.** `--roads` is the DEFAULT, not opt-in, so my "roads on" arm was
   byte-identical to the no-flag arm and the difference between them was zero
   road texels -- which I nearly wrote down as a fact about the test chunks. The
   bake's own census had printed `roads 1 roadPlacements 142 roadTexels 27509` in
   both arms before I ran the difference. A mask of exactly zero is evidence
   about the instrument, never about the data. Fixed by differencing a
   `--no-roads` bake instead; (-20,20) has 25,189 road texels, 9.61 %.
2. **I correlated the residual of a model that ignores its input.** I took the
   best-by-RMS model (the affine fit, slope 0.019) and correlated its residual --
   which is `constant - vanilla`, i.e. vanilla's structure. The tell was `ours
   lum` reading exactly r = -0.0000, which is a definition (least squares
   orthogonalises the residual against its own regressor) and not a measurement.
   Redone on the constant-gain residual it reads +0.835 and +0.827: the strongest
   signal in the lane was showing as its own negation. Lowest RMS does not mean
   "the model to interpret" -- check the fit's RMS against the target's own
   standard deviation first.
3. **The pictures were wrong in two ways that only looking could catch.** The
   difference map clipped at +-22 levels because of a 1.8x multiplier left in the
   ramp from a sanity test, turning a structured field into two flat tones; and
   the residual panel cited the affine model's r (mistake 2) and printed -0.000
   under a scatter that visibly slopes. Both scripts exited 0 and both JSONs were
   right. A picture is a measurement: say what it must show, then open it and
   check it shows that.
4. **The heredoc trap, again, on a file the skill already covers.** `cat > file
   <<'PYEOF'` died on CRLF line endings while creating a new instrument. It is
   written down in two places and I did it anyway because the file was "a script,
   not a patch". The rule has no exception: new files go through the Write tool.

A fifth, smaller, not worth a ledger entry: `fit_affine` crashed with
`LinAlgError: 3-dimensional array given` because it was handed a 2-D luminance
map. Fixed inside all three fitters (`ravel` on entry) rather than at each call
site, so the next caller cannot reintroduce it.

## 8. Finished-work skill review

**`gate-refused-with-numbers` -- used, and it is the reason this lane is DONE
rather than PARTIAL.** The brief's G3 asked for "error reduced on both tiles",
and without this skill the honest outcomes available were to ship 0.8403 and
report a 20 % pooled win while hiding that a reference tile got 30 % worse, or to
report PARTIAL on finished work. The procedure's steps map cleanly: the
impossibility was proved from the parabola BEFORE the graded bakes existed (step
1); the constant that would have to move was named and it is ours, not the
engine's (step 2); the shipped threshold was registered in `g6_gates.py`'s
docstring before the first graded bake was read (step 3); both arms were measured
through the compiled binary and agreed with the Python to +-0.17 of a 1.0
tolerance (step 4); the refusal is assertion S4 and not a paragraph (step 5); and
it went into the CONTRACT, section 2.5f, which is what the next lane reads, not
only into this report (step 6).

One place it is thin, worth adding if it is ever revised: **the skill assumes the
refused gate has a single scalar to refuse.** Here G3 was a compound -- "off ==
rung bytes" AND "error reduced on both tiles" -- and the first half passes
perfectly while the second is impossible. Reporting "G3 refused" would have been
false and "G3 passed" worse. The lane split it (4.3 reports HALF PASS, HALF
REFUSED with the two halves separately evidenced), but the skill gives no
guidance for that and a less careful lane would have picked one word for both.
Suggested addition: *before refusing, decompose the gate into its clauses and
rule on each; a compound gate is refused clause by clause or not at all.*

**`nifskope-ww-build-verify` -- used as written, and it held.** `tools/ww_build.sh`
with the three sources; the process check is inside the chain and printed "exe not
held by a window" rather than being echoed at the top of the lane; make's own exit
code gated; `-nt` on all three sources. Two of its specific warnings were live
here: the gate-driver rule (I checked the eight harnesses for standalone
`release/*.exe` drivers and found none -- had `lodgen_*` used one, it would have
linked the old `lodgen.cpp`), and the header rule (`src/lodgen.h` changed, so I
checked the objects of the TUs that include it rather than trusting make).
Nothing to add.

**A skill this lane wanted and did not have.** The largest time sink was mistake
2 -- interpreting a degenerate regression -- and it is a general trap, not a
terrain one: any lane that fits several models and reads the residual of the best
one can fall into it, and the failure is silent and looks like a clean result. It
would be a short skill: *compare every fit's RMS against the target's own
standard deviation before ranking them; a fit that scores the trivial predictor's
error has no residual worth reading; an r of exactly 0.000 against a variable the
model was fitted on is arithmetic, not evidence; interpret the residual of the
model whose FORM you are testing, not the one with the lowest number.* The two
brief tiles would be its test case: the affine fit "wins" on both and means
nothing on either. Offered to the director as
`fit-residual-is-not-the-best-fits-residual`; not written this session because
the lane's own reds are worth more.

---

**DONE**
