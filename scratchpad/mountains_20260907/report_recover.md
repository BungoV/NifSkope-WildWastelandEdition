# Lane RECOVER — materials for the out-of-bounds cells

Everything below is measured unless labelled `reasoned`.

---

## 1. DOES IT WORK — no, and I would not ship it

**No. Recovering material identity from vanilla's LOD colour does not work at
the accuracy this mod needs, and the deliverables should not be published as-is.**
Colour matching scores 28.4% top-1 on LTEX identity in a spatially-honest
holdout, but that number is almost entirely spatial autocorrelation: broken down
by distance from painted terrain it runs ~40% at 0–2 cells and collapses to
**0.6%–16.1% beyond 16 cells** across six independent spatial splits (0.8% on
the primary deep holdout). Band-matched against an honestly-trained constant
guess, the model's edge shrinks from 41.7% vs 15.8% at 0–2 cells to **6.7% vs
5.0% beyond 16** — still nominally ahead, but wrong 93% of the time and barely
distinguishable from answering the same material every time. And **the median real target sits 42.5
cells from the nearest painted cell** (p90 67, max 97), so essentially the whole
file lives in that regime. Simply copying the nearest painted cell's material,
using no colour at all, beats the whole colour model (29.4% vs 28.4%).

The reason is measured and structural, not a modelling failure I could tune away:
Commonwealth's 58 well-sampled landscape materials sit a **median 2.4 RGB units
apart** while the measurement noise is **11.9**, so the single most common
out-of-bounds colour — 14.7% of all far terrain — is consistent with **28
different materials** at 3.67 bits of entropy. The colours really are all present
in the painted area (96.95% of far quadrants land in an occupied bin, confirming
the brief's feasibility probe); they just do not mean anything specific.

**The dangerous part, and the reason I am recommending against shipping rather
than shipping with caveats:** the recovery is wrong in exactly the way the brief
said is worse than nothing. Predicted-material colour error is **10.8 when the
material is wrong and 9.0 when it is right** — statistically the same. You cannot
see the failure. A mod built on this would look completely convincing in a
screenshot, be wrong about seven times in ten, and silently move every landscape
replacer user's distant terrain toward the wrong material.

The deliverables the brief asked for are all produced (`recovered.txt`,
`recovered_blend.txt`, `recovered_map.png`, `training.txt`) with honest
per-assignment confidence, so the judgement can be made against the artefact
rather than against my summary. The calibrated confidence is **median 0.000,
mean 0.040**; only 5.21% of quadrants reach 0.30, and those are the ones hugging
the painted boundary. `recovered_conf.png` shows that as a thin ring.

**What I would do instead** is in §10 — there is a defensible, much less ambitious
version of this mod that still delivers the thing bungo actually wants most.

All paths are relative to `C:\Users\bungo\AppData\Local\Temp\claude\laneb\`.
Nothing in the repo was touched, nothing was built, `release/NifSkope.exe` was
never invoked.

---

## 2. Gates passed before any modelling (measured)

**The vectorised decoder is exact.** `bcnp.py` decodes a whole BC1/BC3 mip in
numpy so 2,304 tiles cost seconds instead of hours. It is asserted against
`dds.py` texel-for-texel on six tiles across four mip levels:

    python rec_decode_check.py
    -> mismatching channels 0 on every case, PASS
    -> alpha min 255 max 255 on every case (corroborates the brief)

**The tile orientation is corroborated by a second, independent test.** The
brief settled it against `_msn` normals. I re-tested it with an argument that
shares no assumptions with that one: terrain LOD is continuous across a cell
boundary, so a wrong sub-block mapping leaves a step at the 32-texel cell seams
of the reassembled world image. Scoring mean |Δ| across within-tile cell seams
over interior rows, on 200 random level-4 tiles:

    python rec_orient_check.py
    row0=NORTH col0=WEST  (brief)   ratio 1.318
    row0=SOUTH col0=WEST            ratio 2.647
    row0=NORTH col0=EAST            ratio 2.629
    row0=SOUTH col0=EAST            ratio 3.958

The brief's orientation wins by a factor of two. (It is not 1.000 because cell
seams genuinely carry a little extra variation in the source bake.) **Settled
fact confirmed, not merely assumed.**

**VCLR is very nearly a no-op, and I nearly did not need to divide it out.**
The brief flagged it as a trap. Measured, over all 2,362 cells that carry VCLR:

    python rec_vclr_check.py
    81.77% of VCLR texels are exactly (255,255,255)
    multiplier mean 0.9956, median 1.0000, 1st percentile 0.8627
    VCLR & painted 2298 | VCLR & unpainted 64 | painted & no VCLR 1657

So the artist's hand shading biases a quadrant mean by ~0.4% on average, and
only 64 of the 32,909 recovery targets carry VCLR at all. **I divide it out
anyway** — it is free and it is correct — but it is not the load-bearing step
the brief feared, and a lane that skipped it would not have been much wrong.
`reasoned`: the 1st-percentile 0.8627 says a handful of individual cells *are*
shaded hard, so the correction still earns its place for those.

**Commonwealth defines no default land texture.** `WRLD 0000003C` DNAM's default
land texture form id is `00000000`.

    python defaulttex.py
    -> default land is NULL: the worldspace defines NO default land texture

This is the mechanical reason our generator writes flat grey out there: there is
genuinely nothing under an unpainted quadrant to sample. It also matters for the
model — see the NULL pseudo-material below.

---

## 3. The training set (measured)

    python rec_sweep.py      # 2,304 tiles -> cells.npz, 1.7 s
    python rec_train.py      # -> training.txt, training.npz

Every level-4 diffuse tile is decoded at **mip 2**, so a cell is 32×32 texels
and a quadrant is 16×16 = 256 texels — plenty for a mean, and the whole
worldspace fits in one array (`cells.npz`, 566 MB: `pix (192,192,32,32,3)`,
`have`, `vclr`).

    painted cells: 3955
    painted quadrants with a blend: 15382
    empty quadrants inside painted cells: 438
    distinct materials in training (incl. NULL pseudo): 101

**Composite weights, not normalised opacities.** `layers.txt` gives mean opacity
per layer. The engine composites layer *k* over what is below it, so I convert
to weights that sum to exactly 1:

    w_base = prod_i (1 - a_i)        w_k = a_k * prod_{j>k} (1 - a_j)

For a single layer this is exact (the composite is linear in alpha, so the mean
of the composite is the composite of the means). For several layers it assumes
the layers' alphas are independent, because `layers.txt` does not carry
E[a_i·a_j]. `reasoned`, and flagged as the model's main structural approximation.
Verified: weights sum to 1.000000 on every one of the 15,382 rows.

**The NULL pseudo-material.** 12.42% of all painted-quadrant weight belongs to
quadrants with ATXT layers but no BTXT underneath — the brief's own example cell
`-26 -41` is one. Since the worldspace has no default land texture, I do not
guess what shows through; I give it form id `00000000` as a column in the
regression and let the fit estimate its colour. Top of the painted-area budget:

| form id | EDID | share of painted weight | texture |
|---|---|---|---|
| `00000000` | *(no BTXT under layers)* | 12.42% | — |
| `000ab72e` | LCoastSandWet01 | 11.67% | `Landscape\Ground\CoastSandWet01_d.dds` |
| `0014bf47` | LOceanFloor01 | 7.10% | `Landscape\Ground\OceanFloor01_d.DDS` |
| `00021336` | LDirtGravel01 | 6.41% | `Landscape\Ground\DirtGravel01_d.dds` |
| `0001d1d3` | LDriedGrass01 | 4.21% | `Landscape\Ground\DriedGrass01_D.dds` |
| `00125c6c` | LBlastedForestDirt01 | 3.67% | `Landscape\Ground\BlastedForestDirt01_d.dds` |
| `000dedc7` | LGlowingSeaRubble01 | 3.64% | `Landscape\Ground\GlowingSeaRubble01_d.dds` |
| `00109041` | LMarshMudFloor01 | 3.62% | `Landscape\Ground\MarshMudFloor01_d.DDS` |

---

## 4. THE AMBIGUITY FINDING — colour does not identify material (measured)

This is the brief's open question, and the answer is the most important thing in
this report.

    python rec_ambiguity.py     # -> ambiguity.txt

**Colour → material is not unique, and it is not close to unique.**

    far-area-weighted mean top-1 share over the bins the far terrain uses: 27.3%
    far quadrants fall in 356 distinct 6-bit bins; painted quadrants in 268
    far quadrant area whose exact bin also occurs in the painted set: 96.95%

The brief's feasibility probe (99.97% of far texels land on an occupied painted
bin) is confirmed at quadrant granularity — 96.95% — so **the colours are all
present in the painted area. That was never the problem.** The problem is that
each of those colours means many different things:

| 6-bit bin | share of far area | painted samples | top-1 | distinct materials | entropy |
|---|---|---|---|---|---|
| (18,16,14) | 14.71% | 164 | 20.1% | **28** | 3.67 bits |
| (17,15,14) | 11.48% | 132 | 19.7% | **29** | 4.15 bits |
| (18,17,15) | 7.45% | 446 | 16.1% | **38** | 4.24 bits |
| (17,16,14) | 5.22% | 375 | 14.1% | **39** | 4.26 bits |
| (19,17,15) | 3.85% | 130 | 28.5% | 23 | 3.76 bits |

The single most common far colour — 14.7% of all out-of-bounds area — is
consistent with 28 different materials, and the best guess within it is right
one time in five. The first three bins alone are a third of the far terrain.

The reason is visible in the fitted palette. Over the 58 materials with at least
20 quadrants of painted area:

    nearest-other-material colour distance: median 2.4, p25 1.6, p10 1.1
    fit residual: 11.9
    materials whose nearest neighbour is closer than the residual: 55 of 58

**The materials are about five times closer together than the measurement noise.**
Commonwealth's landscape palette is 58 shades of warm brown packed into a tiny
volume of colour space. Some genuinely indistinguishable pairs:

    LCoastGrassSand01      <-> LMuddyLeaves01Wet          0.5
    LGlowingSeaCorrosion01 <-> LGlowingSeaMud01           0.8
    LBlastedForestDirt01   <-> LRiverbedSilt01Wet         1.0
    LMarshMudFloor01       <-> LGlowingSeaCorrosion02     1.1

Where it *is* unique, it is strongly unique — `(14,12,10)` and `(15,13,11)` are
71% LMarshMudFloor01. So the mapping is not uniformly hopeless; it is bimodal, a
few dark bins are near-deterministic and the dominant warm-brown bins are not.

### Does the texture-replacer framing rescue it? Barely.

The end product is a mod that lets a **texture replacer** rebake far LOD, and a
replacer replaces a *texture file*, not an LTEX record. Several LTEX share one
diffuse, so confusing them costs a replacer user nothing:

    python rec_labels.py
    -> 106 LTEX resolve to 78 distinct diffuse groups; 23 groups hold >1 LTEX
       e.g. LDriedGrass01 / ...NoGrass / LDriedGrass02Weeds / LDriedGrass03
            all -> Landscape\Ground\DriedGrass01_D.dds

This is the right accuracy metric for the stated goal, and I report it
throughout. **Measured, it buys almost nothing** (+0.5 percentage points, §5):
the LTEX that share a texture are not the ones getting confused.

---

## 5. HOLDOUT VALIDATION — the number that decides it (measured)

    python rec_holdout.py

Two spatial schemes, no random holdout anywhere (the brief is right that random
leaks — a cell's neighbour is nearly itself):

* **BLOCKED** — the painted area is cut into 12×12-cell blocks dealt round-robin
  into 5 folds. A cell and its neighbours always share a fold, so nothing leaks,
  but each fold still spans every terrain type. The fair generalisation estimate.
* **OUTWARD** — train on the inner core of the painted blob, test on its outer
  30% rim. **This is the honest analogue of the real task**, which is
  extrapolation away from the painted region entirely.

Null models: `prior` = always assign the single most common material.
Chance = 1.35% (74 distinct dominant materials, 58 texture groups).

| scheme | predictor | LTEX top-1 | texture-group top-1 |
|---|---|---|---|
| BLOCKED | colour3 (mean RGB, the brief's baseline) | 34.0% | 34.4% |
| BLOCKED | **colour+ (14 transferable features)** | **39.3%** | 39.8% |
| BLOCKED | palette (nearest fitted material colour) | 8.1% | 9.7% |
| BLOCKED | prior (null) | 16.4% | — |
| OUTWARD | colour3 | 23.8% | 24.5% |
| OUTWARD | **colour+** | **28.4%** | 29.0% |
| OUTWARD | palette | 7.9% | 10.7% |
| OUTWARD | prior (oracle constant, see §7) | 18.5% | — |

**On the test that resembles the actual job, the model is right 28.4% of the
time against an oracle constant of 18.5%.** Roughly seven assignments in ten
would be wrong. (That 18.5% is *not* an honest null — it picks the commonest
label using the test set and allows NULL as an answer. §7 replaces it with a
constant fitted on training data and scored band-by-band, which is the
comparison that governs.)

Note also that the **`palette` predictor — nearest fitted material colour — is
worse than the constant baseline** (7.9% vs 18.5%). Solving for each material's colour and
matching against it is an appealing idea and it does not work, for the reason in
§4: the fitted colours are 2.4 units apart and the noise is 11.9.

---

## 6. Which features are even allowed (measured)

    python rec_shift.py

A feature may only be used if the far cells occupy the same range of it as the
painted cells; otherwise the model extrapolates blind and invents confident
nonsense — precisely the failure the brief warns is worse than no recovery.
Criterion: ≥90% of far quadrants inside the painted 1–99 percentile band, and
mean shift ≤0.60 painted standard deviations.

**Terrain shape fails badly and is excluded.**

| feature | overlap | shift/sd |
|---|---|---|
| hMean | 83.8% | +0.66 |
| hStd | **39.6%** | −0.37 |
| slopeMean | **39.6%** | −0.55 |
| slopeMax | **39.8%** | −0.68 |
| cellSlope | **38.8%** | −0.66 |

The out-of-bounds heightfield is structurally a different animal — height std
282 out there against 138 inside, slope overlap under 40%. It is the procedural
border terrain, not more of the Commonwealth. Elevation and slope are the most
tempting extra features available and **they are unusable**; see REFUTED.

Two colour features also failed and were dropped (`R-G` shift 0.83, `r_norm`
0.71, `R-B` 0.65) — the far terrain is measurably *warmer* than the painted
terrain.

One shift that survived the cut but is worth flagging: the far terrain is
**smoother** than the painted terrain (`gradH` 3.50 vs 4.63, `laplace` 12.0 vs
16.0, both about −0.42 sd, overlap 99.8%). `reasoned`: most likely because
painted quadrants carry multi-layer alpha blending that creates variation the
uniform far region does not have. It biases detail-based matching slightly
toward smoother materials, and it is one reason OUTWARD scores below BLOCKED.

---

## 7. The result that ends the argument: accuracy decays with distance (measured)

    python rec_alt.py
    python rec_decay.py

First, a control the brief did not ask for. **Throw the colour away entirely** and
give each held-out quadrant the material of the nearest painted quadrant:

| method (OUTWARD holdout) | LTEX top-1 | family top-1 |
|---|---|---|
| colour+ (14 features, the whole model) | 28.4% | 33.4% |
| **geographic extension, no colour at all** | **29.4%** | **51.8%** |
| colour+ restricted to materials occurring within 12 cells | 34.6% | 50.5% |

**Copying the nearest neighbour beats the colour model.** At that point the
premise is already in trouble: the vanilla LOD pixels are carrying no more
material information than proximity does.

The confirmation is the decay test. If colour genuinely identified material, its
accuracy would be **flat** in distance-to-nearest-training-cell — colour does not
know how far away it is. Deep holdout, inner half trains, outer half tests:

| distance to nearest painted cell | n | colour LTEX | colour family | geography LTEX | geography family |
|---|---|---|---|---|---|
| 1–2 cells | 808 | 43.9% | 47.8% | 53.7% | 62.1% |
| 2–4 | 1,433 | 43.3% | 47.8% | 45.3% | 59.1% |
| 4–8 | 2,406 | 38.3% | 40.6% | 36.8% | 47.5% |
| 8–16 | 2,526 | 23.6% | 25.5% | 21.3% | 40.9% |
| **16+** | 515 | **11.3%** | 12.2% | 6.4% | 62.7% |

**Colour accuracy falls from 43.9% to 11.3% as you move away from the painted
region.** It is not flat. The 28–34% headline figures were mostly *spatial
autocorrelation leaking through the colour feature* — the k-NN was retrieving
cells that had a similar colour because they were nearby, not because the colour
identified the material.

**The real recovery targets sit up to ~60 cells from any painted cell.** The
honest extrapolation of that table to the actual job is **at or below 11%**,
— see the band-matched null table below, where the model's long-range edge over
a trained constant guess falls to 6.7% against 5.0%.

### Reconciling 11.3% with the 0.8% quoted in §1 and §9

Both are the same split and the same 515 quadrants; they differ in one deliberate
choice, and the stricter one is the honest one.

The table above lets the model score a hit for predicting the **NULL
pseudo-material** — "ATXT layers with no BTXT underneath" — which is the
dominant label in 16.4% of painted quadrants (16.9% in the outer band). NULL is
easy to predict in bulk, so it inflates the figure.

But **NULL is not a legal answer for this mod.** The entire purpose is to give
unpainted cells a base texture; writing "no base texture" back into them
achieves nothing. So `rec_apply.py` and `rec_calib.py` relabel every
NULL-dominant training quadrant to its best *real* material and score against
that. On the task the mod actually requires, the same 515 quadrants score
**0.8%**.

11.3% is the accuracy of a classifier allowed to say "nothing"; 0.8% is the
accuracy of one required to name a texture. **0.8% is the number that governs.**

### The decay is not an artefact of how the holdout was cut (measured)

    python rec_robust.py

The deep holdout confounds two things — its test cells are far from training data
*and* sit at the extremes of the painted blob, where terrain may just be odd. So
the measurement is repeated under five further splits in which the test cells are
not selected for being extreme. NULL excluded as an answer throughout.

| split | n test | overall | 0–2 | 2–4 | 4–8 | 8–16 | **16+** |
|---|---|---|---|---|---|---|---|
| west trains / east tests | 7,488 | 13.1% | 44.2% | 31.6% | 30.1% | 19.5% | **1.5%** |
| east trains / west tests | 7,892 | 11.1% | 44.1% | 37.4% | 27.1% | 12.2% | **1.9%** |
| south trains / north tests | 7,666 | 22.4% | 36.5% | 32.4% | 38.0% | 30.3% | **13.3%** |
| north trains / south tests | 7,714 | 26.1% | 44.3% | 43.4% | 39.4% | 33.8% | **16.1%** |
| inner trains / outer tests | 7,688 | 28.2% | 39.6% | 39.3% | 32.7% | 19.6% | **0.6%** |
| 16-cell block checkerboard | 7,543 | 33.0% | 36.8% | 34.0% | 29.7% | 38.0% | n/a |

**Every split shows the same collapse**: 36–44% within two cells, 0.6%–16.1%
beyond sixteen. The spread at long range is wide and I am not going to pretend
otherwise — the north/south cuts (13.3%, 16.1%) are far gentler than the
east/west ones (1.5%, 1.9%). `reasoned`: Commonwealth terrain varies much more
from coast to inland than it does north to south, so a west/east split demands
genuine extrapolation while a north/south one does not. **But at long range every split lands in single or low double
digits, and the margin over a trained constant guess is 1.7 percentage points
(6.7% vs 5.0%, table below).** The conclusion does not depend on which cut you
believe.

### The honest null, band-matched — and a correction to my own earlier figure

    python rec_null.py

I quoted "an 18.5% null" through several drafts of this report. **That figure was
wrong for the comparison I was making**, in two ways: it picked the commonest
label using the *test* set (an oracle, not a null), and it allowed NULL as a
legal answer when the governing metric forbids it. A null has to be fitted on the
training half like any other model, and scored in the *same distance band* as the
number it is compared against — otherwise a model scoring 1.5% at long range is
being measured against a baseline computed mostly on short-range cells.

Redone properly, model / trained-constant, averaged over the five spatial splits:

| distance | colour+ model | honest null | ratio |
|---|---|---|---|
| 0–2 cells | 41.7% | 15.8% | 2.6× |
| 2–4 | 36.8% | 13.6% | 2.7× |
| 4–8 | 33.5% | 12.4% | 2.7× |
| 8–16 | 23.1% | 10.4% | 2.2× |
| **16+** | **6.7%** | **5.0%** | **1.3×** |

**This changes one of my claims and not the verdict.** I had said the model was
*below* the null at long range; it is not — it keeps a small edge all the way
out. What actually happens is that the edge decays from 2.6× to 1.3× while the
absolute accuracy falls to 6.7%, i.e. **93% of long-range assignments are wrong**
and the model is only marginally better than answering the same material for
every cell in the Commonwealth. That is still disqualifying for a shipped mod,
for the reasons in §8, but it is disqualifying on *absolute* accuracy rather than
on losing to a constant, and the report should say the true thing.

Note also the per-split spread in the raw table (`rec_null.py`): on the
east-trains/west-tests split the trained constant scores **0.0%** in every band,
because the commonest material in the eastern half is essentially absent in the
west. That is a direct measure of how little the Commonwealth's material
distribution transfers across itself.

One thing does survive: geography keeps the coarse **family** roughly right even
far out (40–63%), while its LTEX accuracy collapses to 6.4%. `reasoned`: the
Commonwealth's broad terrain zones are large and smooth, so "this is the marsh /
this is the Glowing Sea" extends outward even when the specific record does not.
That is the one usable signal I found, and it is not a colour signal.

---

## 8. Where the confidence and colour numbers land (measured)

    python rec_eval.py

**Confidence is weakly informative and never gets good.** Even the most
confident 5% of assignments are only 43.9% correct on the rim holdout (and the
rim is the easy case, per §7):

| confidence band | n | LTEX top-1 |
|---|---|---|
| 0.00–0.20 | 87 | 10.3% |
| 0.30–0.40 | 872 | 19.2% |
| 0.50–0.70 | 1,555 | 36.9% |
| 0.70–1.00 | 712 | 41.4% |

There is no threshold that isolates a shippable subset.

**And the colour-is-right-material-is-wrong trap is real, exactly as the brief
predicted.** Comparing the predicted material's colour against the cell's true
LOD colour:

    within 10 RGB units: 49.1% of quadrants     median colour error 10.2
    within 20 RGB units: 82.7% of quadrants
    ... while LTEX identity is right 28.4% of the time

    median colour error among WRONG-material calls:   10.8
    median colour error among CORRECT-material calls:  9.0

**10.8 versus 9.0.** The colour error is essentially the same whether the
material is right or wrong, so *you cannot tell from the picture whether the
recovery worked*. A shipped mod built this way would look completely convincing
in a screenshot and be wrong about seven times in ten — and would move a texture
replacer's distant terrain in the wrong direction, silently. This is the precise
failure mode the brief said is worse than doing nothing.

**Failure cases by material** (OUTWARD holdout). Many materials are never
predicted at all — they get absorbed into the commoner ones:

| true material | n | recall | most common wrong answer |
|---|---|---|---|
| LBlastedForestDirt01 | 98 | 69.4% | LCoastSandWet01 7% |
| LOceanFloor01 | 571 | 56.7% | LCoastSandWet01 36% |
| *(no BTXT under layers)* | 855 | 46.2% | LCoastSandWet01 9% |
| LCoastSandWet01 | 725 | 42.2% | LOceanFloor01 18% |
| LGlowingSeaRubble01 | 449 | 19.6% | LBlastedForestDirt01 34% |
| LRubbleRock01 | 134 | 11.2% | *(no BTXT)* 61% |
| LCoastSandDry01 | 175 | **1.1%** | *(no BTXT)* 73% |
| LGlowingSeaRubble01NoGrass | 326 | **0.3%** | LCoastSandWet01 18% |
| LDriedGrass01 | 83 | **0.0%** | *(no BTXT)* 58% |
| LGlowingSeaCorrosion01 | 93 | **0.0%** | LBlastedForestDirt01 24% |
| LRootsEroded01 | 51 | **0.0%** | *(no BTXT)* 82% |
| LCoastGrassSand01 | 49 | **0.0%** | *(no BTXT)* 47% |

Note how often the wrong answer is *(no BTXT)* — the model dumps uncertain cells
into the NULL pseudo-material, i.e. into "no base texture at all", which is the
single worst thing to write into a mod whose entire purpose is to give these
cells a base texture.

**Spatial smoothing does not rescue any of it** (`rec_smooth.py`): voting over
±1/±2/±3/±5 cell windows moves 28.4% to 28.4%/28.3%/28.2%/28.3%. The errors are
systematic, not independent noise. And the ceiling is low anyway — the truth
itself is only 45.1% coherent east-west and 45.4% north-south, two quadrants of
the *same cell* agree only 55.7% of the time, and an **oracle** allowed one
material per 2×2 cell block would still only score 61.0%.

---

## 9. The confidence number was a trap, and is now fixed (measured)

    python rec_calib.py

`recovered_conf.png` in its first form showed the model *most* confident in the
far outer margin. Measured, on the deep holdout:

| distance to painted | n | mean vote share | accuracy |
|---|---|---|---|
| 0–2 cells | 808 | 0.509 | 40.2% |
| 2–4 | 1,433 | 0.495 | 39.2% |
| 4–8 | 2,406 | 0.483 | 34.0% |
| 8–16 | 2,526 | 0.475 | 20.6% |
| **16+** | 515 | **0.402** | **0.8%** |

    corr(distance, vote share) = -0.115
    corr(distance, correct)    = -0.253

The raw k-NN vote share **barely moves** across a range over which accuracy falls
from 40% to under 1%. Far from painted terrain the ground is uniform, so all 15
neighbours agree and the vote share stays high — but unanimity among neighbours
is not evidence of correctness when every neighbour is wrong the same way. A
consumer filtering on "only use high-confidence cells" would have been handed the
worst assignments in the file.

Fixed by calibrating **jointly on (distance to painted terrain, vote share)**
rather than vote share alone. Distance is known exactly for every target, so this
costs nothing. The resulting table is the honest summary of the whole lane:

| distance | vote 0.00–0.40 | 0.40–0.55 | 0.55–0.70 | 0.70–1.01 |
|---|---|---|---|---|
| 0–2 | 15.2% | 39.4% | 51.5% | **76.2%** |
| 2–4 | 18.4% | 34.7% | 56.8% | 72.5% |
| 4–8 | 17.0% | 31.6% | 48.1% | 64.1% |
| 8–16 | 10.8% | 21.0% | 32.8% | 33.4% |
| **16+** | **1.3%** | **0.0%** | **0.0%** | **0.0%** |

Read the bottom row against `median target distance 42.5 cells`. Note the top-left
of this table is genuinely good — 76.2% within two cells of painted terrain — which
is why the method is seductive and why only the distance breakdown exposes it.

`recovered.txt` and `recovered_blend.txt` carry this joint confidence, and both
files begin with a header stating the measured accuracy so they cannot be
consumed blind.

---

## 10. The recovered palette, and what I would actually ship

    python rec_map.py     # -> recovered_map.png, recovered_conf.png,
                          #    recovered_legend.txt

What the model assigns out of bounds (full table in `recovered_legend.txt`):

| form id | EDID | share of far area | texture |
|---|---|---|---|
| `000ab72e` | LCoastSandWet01 | **49.33%** | `Landscape\Ground\CoastSandWet01_d.dds` |
| `0001f78c` | LRubbleRock01 | 12.33% | `Landscape\Ground\RubbleRock01_D.dds` |
| `000ecbdf` | LGlowingSeaRubble01NoGrass | 6.48% | `Landscape\Ground\GlowingSeaRubble01_d.dds` |
| `0014bf47` | LOceanFloor01 | 4.73% | `Landscape\Ground\OceanFloor01_d.DDS` |
| `00109041` | LMarshMudFloor01 | 4.54% | `Landscape\Ground\MarshMudFloor01_d.DDS` |
| `0001d1d3` | LDriedGrass01 | 4.35% | `Landscape\Ground\DriedGrass01_D.dds` |
| `0003343c` | LForestFloor01 | 4.29% | `Landscape\Ground\ForestFloor01_D.dds` |
| `00021336` | LDirtGravel01 | 2.64% | `Landscape\Ground\DirtGravel01_d.dds` |
| `00125c6c` | LBlastedForestDirt01 | 2.60% | `Landscape\Ground\BlastedForestDirt01_d.dds` |
| `00085b4f` | LQuaryMarble01 | 1.62% | *(TXST has no TX00)* |

**Half the Commonwealth's out-of-bounds area collapses onto one material.** That
is not a recovered palette, it is the model finding the centroid of the far
colour cloud and defaulting to it. 47 distinct materials appear at all, against
101 in the painted area.

`recovered_map.png` (4 px a cell, quadrants visible, painted region outlined in
white, `+y` north up) makes the same point visually: inside the outline the true
materials form coherent geographic regions — the Glowing Sea, the coastline, the
marsh — and outside it the assignment is a flat field speckled with noise, with
believable structure surviving only in a band hugging the boundary.

### What I would ship instead

`reasoned`, from the measurements above. Re-read the goal: *"anyone running a
landscape texture replacer gets the far terrain rebaked from their new textures
too, instead of frozen in vanilla's pixels."*

**That goal does not need accurate material identity.** It needs the far cells to
carry *some* sensible landscape LTEX so a replacer's textures propagate. Detail
recovery is the part that fails; propagation works with any plausible assignment.
So the defensible mod is the deliberately unambitious one:

1. **A coarse, honest base assignment.** Geographic extension keeps the broad
   *family* right far better than it keeps the record right — 40–63% at family
   level even past 16 cells, against 6.4% for LTEX identity there (§7). Assign a
   small number of regional base materials by extending the painted zones
   outward, and do not pretend to per-quadrant detail. It is defensible because
   it claims only what was measured.
2. **Use the high-confidence ring where it is real.** The 5.21% of quadrants at
   calibrated confidence ≥0.30 — the band within roughly 8 cells of painted
   terrain — genuinely are 33–76% accurate. That ring is worth doing properly and
   is exactly where a seam between painted and unpainted LOD would otherwise be
   visible.
3. **Ship the confidence with the mod**, so a downstream LOD generator can fall
   back rather than trust a 0.8% assignment.

That is a smaller product than the brief imagined, and it is the one the data
supports.

---

## 11. Method, with the command for each measurement

| step | script | what it establishes |
|---|---|---|
| decoder gate | `python rec_decode_check.py` | vectorised BC1/BC3 decode is texel-exact vs `dds.py`; alpha constant 255 |
| orientation gate | `python rec_orient_check.py` | brief's tile orientation confirmed independently, 1.318 vs 2.6–4.0 |
| VCLR | `python rec_vclr_check.py` | VCLR is 99.56% neutral; only 64 unpainted cells carry it |
| default texture | `python defaulttex.py` | Commonwealth WRLD DNAM default land texture is NULL |
| pixel sweep | `python rec_sweep.py` | 2,304 level-4 tiles → `cells.npz`, per-cell mip-2 pixels, 1.7 s |
| training set | `python rec_train.py` | `training.txt` / `training.npz`, 15,382 painted quadrants, 101 materials |
| palette fit | `python rec_model.py` | ridge fit R²=0.6934, residual 11.9 vs noise floor 12.0; `palette.npz` |
| features | `python rec_features.py` | 22 per-quadrant features at mip 0 (64×64 texels/quadrant) |
| feature admissibility | `python rec_shift.py` | terrain shape rejected (overlap <40%); 14 features admitted |
| ambiguity | `python rec_ambiguity.py` | far-weighted top-1 27.3%; `ambiguity.txt` |
| label spaces | `python rec_labels.py` | 106 LTEX → 78 diffuse groups |
| holdout | `python rec_holdout.py` | BLOCKED 39.3%, OUTWARD 28.4% |
| honest null | `python rec_null.py` | band-matched trained constant: model 6.7% vs null 5.0% at 16+ |
| msn verification | `python rec_msn_check.py` | msn is flatter in distance than colour but tiny: 4.1% vs 0.6% at 16+ |
| smoothing / oracle | `python rec_smooth.py` | smoothing gains 0.0; 2×2-block oracle only 61.0% |
| evaluation | `python rec_eval.py` | confidence bands, colour-vs-identity trap, per-LTEX failures |
| alternatives | `python rec_alt.py` | geographic extension 29.4% beats colour 28.4% |
| decay | `python rec_decay.py` | accuracy 43.9% → 11.3% with distance |
| decay robustness | `python rec_robust.py` | same collapse under six independent spatial splits |
| albedo asterisk | `python rec_water.py` | submerged terrain is darkened; R² 0.6934 → 0.7257 |
| apply | `python rec_apply.py` | `recovered.txt`, `recovered_blend.txt`, `recovered.npz` |
| calibration | `python rec_calib.py` | joint (distance, vote) calibration; rewrites both text files |
| map | `python rec_map.py` | `recovered_map.png`, `recovered_conf.png`, `recovered_legend.txt` |

Modelling choices worth stating:

* **Composite weights, not normalised opacities.** `w_base = Π(1-a_i)`,
  `w_k = a_k·Π_{j>k}(1-a_j)`. Exact for one layer; assumes independent alphas for
  several, because `layers.txt` does not carry `E[a_i·a_j]`. Verified to sum to
  1.000000 on all 15,382 rows. This is the model's main structural approximation.
* **A NULL pseudo-material** (`00000000`) for quadrants with ATXT layers and no
  BTXT, which is 12.42% of painted weight. Its colour is *fitted* rather than
  guessed, since the worldspace defines no default land texture. It is never
  emitted into `recovered.txt` — writing "no base texture" into a mod whose
  purpose is to supply one would be actively harmful — so those labels fall back
  to the quadrant's best real material.
* **Flat BTXT-only vs blended.** Both are produced. The brief asked me to say if
  flat is materially worse; it is not the binding constraint here — at 0.8%
  identity accuracy past 16 cells the difference between one material and a
  four-way blend of the wrong materials is not meaningful. Do not read
  `recovered_blend.txt` as the better file.

---

## 12. REFUTED — what did not survive

1. **Colour → material identity, the lane's whole premise.** 27.3% far-weighted
   top-1 by bin; 28.4% by model; 0.8% past 16 cells where the targets actually
   are. Refuted by `rec_decay.py`.
2. **"The far colours exist in the painted area, therefore the materials are
   recoverable."** The first half is true and I re-confirmed it at quadrant
   granularity (96.95%). The implication is false. Colour presence was never the
   bottleneck; colour *specificity* is, and nobody had measured it.
3. **Solving for each material's colour and matching against it** (the `palette`
   predictor, and the most elegant idea I had). 7.9–8.1% — **worse than the
   18.5% constant baseline.** The ridge fit itself is fine (R² 0.69, residual at the noise
   floor) and its palette agrees with the real texture files to ~2–8 units for
   the best-sampled materials. It still cannot classify, because the fitted
   colours are 2.4 units apart and the noise is 11.9.
4. **Elevation, slope and terrain roughness as features.** The most tempting
   extra signal available. Rejected on covariate shift before use — painted vs
   far overlap is **under 40%** on every slope statistic (height std 138 inside
   vs 282 outside). The out-of-bounds heightfield is procedural border terrain,
   not more Commonwealth. Using it would have produced confident nonsense.
5. **Spatial smoothing of the predictions.** 28.4% → 28.4/28.3/28.2/28.3% at
   ±1/±2/±3/±5 cells. The errors are systematic, not independent noise.
6. **Spatial coherence as a way to beat the per-quadrant limit at all.** The
   *truth* is only 45.1%/45.4% coherent between adjacent cells, and two quadrants
   of the same cell agree just 55.7% of the time. An **oracle** allowed one
   material per 2×2 cell block scores 61.0%; per 8×8 block, 45.1%.
7. **The texture-replacer regrouping as a rescue.** Correct framing — a replacer
   replaces a file, not an LTEX — and 106 LTEX do collapse to 78 diffuse groups.
   Worth only **+0.5 percentage points**. The LTEX that share a texture are not
   the ones being confused.
8. **Raw k-NN vote share as a confidence.** Nearly flat in distance
   (corr −0.115) while accuracy collapses (corr −0.253). Replaced with a joint
   (distance, vote share) calibration.
9. **The `_msn` normal maps as the rescue.** Confirmed in kind — normal detail
   is genuinely flatter in distance than colour and beats it beyond 16 cells
   (4.1% vs 0.6%), the only material-like descriptor found in the lane — but it
   moves long-range accuracy to 4.1% against a 5.0% trained-constant null, and
   11 of its 16 admissible columns turn out to correlate >=0.45 with the terrain
   height I had already rejected. Full treatment in §14.
10. **My own "18.5% null", quoted through several drafts.** It chose the
   commonest label using the test set and allowed NULL as an answer. Refitted on
   training data and scored band-by-band, the real null is 15.8% at 0-2 cells
   and 5.0% beyond 16 -- which means my claim that the model was *worse than a
   constant guess* at long range was wrong. It stays marginally ahead (6.7% vs
   5.0%) and is disqualified on absolute accuracy instead. Corrected in §7.
11. **My own first reading of `recovered_conf.png`.** I described the confidence
   as rising with distance. Measured, it *falls* slightly (−0.115) — it just
   falls far more slowly than accuracy. The conclusion (the number is a trap) was
   right, the mechanism I first stated was not; both script and report were
   corrected.

Not refuted, and worth keeping: the brief's tile orientation (independently
re-confirmed), the constant-255 alpha, the near-neutrality of VCLR, and the
finding that Commonwealth defines no default land texture.

---

## 13. One settled fact needs an asterisk: "plain albedo" (measured)

    python rec_water.py

The brief lists *"the vanilla diffuse is plain albedo — no baked sun, no baked
AO"* as settled. The evidence for it (weak per-tile light fits, azimuths
scattering 75/315/180/315/30/330°) is sound and I did not contradict it. But the
palette fit turned up something it does not cover: **submerged terrain is
darkened.**

Splitting painted quadrants by terrain height and looking at the fit residual:

| quadrant height below | n | mean residual luminance | (above) |
|---|---|---|---|
| −2000 | 1,670 | **−5.21** | +1.00 |
| −1000 | 2,244 | −3.19 | +0.93 |
| −500 | 2,612 | −2.52 | +0.91 |
| 0 | 3,227 | −1.58 | +0.84 |

    corr(quadrant height, residual luminance) = +0.109
    a separate palette for quadrants below -1000 lifts R^2 from 0.6934 to 0.7257

So there *is* a height-dependent darkening of about 5–6 luminance units below the
waterline, which "plain albedo" does not predict. It is small, and it does not
change any conclusion in this report — the recovery fails on discriminability,
not on absolute colour — but anyone matching colours on submerged cells is
matching a tinted colour, and should know.

**A second, larger discrepancy I could not explain, stated as an open question.**
The fitted palette matches the real texture files well for most well-sampled
materials (LRubbleRock01 2.0, LGlowingSeaRubble01 3.9, LBlastedForestDirt01 7.8,
LCoastSandWet01 8.0 RGB units apart) but is off by a factor of two for a few —
LOceanFloor01 fits to 39.7 against the file's 82.4, and its two kelp variants
land in the same place. The submerged darkening above is far too small to account
for that. Candidate explanations I did **not** establish: the LOD bake may sample
a different mip or a LOD-specific texture, or the whole-file mean may simply be
the wrong statistic for a texture the terrain shader tiles and tints. Flagged
rather than guessed.

---

## 14. The `_msn` channel — real material signal, far too little of it

One avenue is not yet closed. Every conclusion above rests on the **diffuse**
tiles. The `_msn` normal maps are a second, independent channel that nobody in
this lane or the previous ones has mined for material identity, and there is a
real reason to expect signal in them: the *geometric* normal comes from the
33×33 VHGT grid, which is 4 texels per vertex at mip 0, so geometric detail is
band-limited to ~4 texels while **material** normal detail (gravel speckle
against smooth sand) is higher frequency. A 3×3 laplacian of the `_msn` should
therefore be nearly pure material signal, and it is information the diffuse mean
simply does not contain.

A sub-lane extracted 31 such features (`msn_features.py` → `msn_features.npz`,
36,864/36,864 cells; its decode gated exact against `dds.py` at mip 0, and the
brief's channel convention confirmed emphatically — `mean(n_up) = 0.9363`,
`frac(n_up > 0) = 0.999998` over 604M texels, so green really is up).

**The hypothesis is confirmed in kind and it does not rescue the recovery.**

I re-scored it myself under this report's governing metric (NULL excluded), since
the sub-lane scored with NULL allowed:

    python rec_msn_check.py

| features | overall | 0–2 | 2–4 | 4–8 | 8–16 | **16+** |
|---|---|---|---|---|---|---|
| colour+ (14) | 28.2% | 39.6% | 39.3% | 32.7% | 19.6% | **0.6%** |
| msn transferable (16) | 20.3% | 24.9% | 23.0% | 24.1% | 16.9% | **4.1%** |
| msn clean-independent (4) | 14.4% | 19.1% | 17.5% | 16.3% | 11.7% | 3.5% |
| colour+ **&** msn | 28.3% | 39.4% | 36.1% | 34.4% | 19.6% | **2.9%** |

The interesting part is the shape, not the level. **The `_msn` is much flatter in
distance than the colour is, and it beats colour outright beyond 16 cells (4.1%
vs 0.6%)** — which is exactly the signature of a feature describing the *material*
rather than its *location*, and the thing the diffuse conspicuously failed to
show. The sub-lane's F-ratio screen backs it up physically: the up-channel
high-pass orders the materials sensibly, LOceanFloor01 0.021 → LCoastSandWet01
0.031 → LDirtGravel01 0.054 → LGlowingSeaRubble01 0.088. There is real material
information in the normal maps.

It is just far too little. 4.1% is 4.1%, and adding colour to it makes long range
*worse* (2.9%), so the colour is not merely uninformative out there — it is
actively misleading. Two further caveats, both raised by the sub-lane against its
own result and both verified here:

* **The column that best identifies material is the one that fails the
  transferability gate.** The up-channel high-pass `lap_ny` ranks first on
  F-ratio but has 85.3% painted/far overlap, under the 90% bar. It misses on
  overlap, not shift.
* **Most of the "transferable" msn set is smuggled elevation.** I rejected
  terrain height in §6; **11 of the 16 admitted msn columns correlate ≥0.45 with
  mean height** (`hpTilt` +0.642, `hp9_nx` +0.634, `hp5_nz` +0.631, `dh_nz`
  +0.623, `lapVec` +0.616…). Only `mean_nx`, `mean_nz`, `aniso` and `hpCorr_xz`
  are genuinely independent, and those four alone score 14.4% / 3.5%.

**§1's verdict stands unchanged.** The `_msn` is the most interesting negative
result in the lane — it is the only feature I found that behaves like a material
descriptor — but it moves long-range accuracy from 0.6% to 4.1% against a 5.0%
trained-constant null. `reasoned`: normal-map detail is simply too attenuated by
the time terrain is baked to 128 texels a cell to carry a 74-way classification.

---

## 15. NEEDS-OVERSEER

Nothing in this lane needed `release/NifSkope.exe`, and it was never invoked.
These are the things I would want run or decided that I could not do myself:

1. **Tell the ESPWRITE lane.** A peer session is building the .esp writer that
   consumes `recovered.txt`. Its **format is fine and unchanged** — 32,909 `R`
   lines, one a cell, four quadrants, plus `conf` — so that lane is not blocked
   and its work is not wasted. But it should know the content is measured at
   ~0.8% identity accuracy over most of its extent, and that the sensible
   product is the §10 version rather than a full per-quadrant recovery. I have not
   messaged them; coordinating lanes is yours.

2. **A cross-worldspace replication, if you want the conclusion hardened.**
   Re-running `rec_train.py` → `rec_decay.py` against Far Harbor or Nuka-World
   would show whether "colour does not identify material" is a property of
   Bethesda's landscape palettes generally or a quirk of the Commonwealth's
   brown-on-brown one. Pure Python, no exe; I simply ran out of lane on it. The
   worldspace form id and a `layers.txt` for it are the only new inputs needed.

3. **Ground-truth the linear blend model against our own generator.** My whole
   palette fit assumes a quadrant's LOD colour is the composite-weighted mix of
   its materials. That could be checked directly by baking a level-4 tile for a
   few painted cells with our own LODGen and comparing against vanilla:

       release/NifSkope.exe --lodgen ... (whatever the level-4 diffuse bake
       invocation is; I did not want to guess a command into a report)

   If our bake reproduces vanilla's colour for painted cells, the model is sound
   and the negative result is about information content, not about my algebra.
   This is the single check that would most strengthen §1.

4. **A decision from you on §10.** Whether to build the smaller, defensible mod
   (coarse regional families + the high-confidence ring + shipped confidence) or
   to drop the idea. I have not built it, because it is a different product from
   the one the brief specified and that call is yours.

---

## 16. Files produced

All in `C:\Users\bungo\AppData\Local\Temp\claude\laneb\`.

**Deliverables the brief asked for**

| file | contents |
|---|---|
| `report_recover.md` | this report |
| `training.txt` | 15,382 painted quadrants: VCLR-corrected LOD colour + known blend |
| `recovered.txt` | 32,909 unpainted cells, BTXT-only, one material a quadrant, joint-calibrated `conf` |
| `recovered_blend.txt` | same cells, up to 4 blended layers a quadrant |
| `recovered_map.png` | 768×768 material map, painted cells in their true material, painted region outlined |

**Supporting data and pictures**

`recovered_conf.png` (calibrated confidence — the honest picture of where this
works), `recovered_legend.txt`, `ambiguity.txt`, `cells.npz` (566 MB per-cell
mip-2 pixels), `features.npz`, `training.npz`, `palette.npz`, `recovered.npz`,
`recovered_conf.npz`.

`msn_features.npz` (31 normal-map features a quadrant) and
`msn_features_notes.txt` from the `_msn` sub-lane.

**Scripts** — `bcnp.py` (vectorised BC1/BC3 decoder), `rec_common.py`,
`rec_labels.py`, `rec_*.py` for each step in the §11 table, and the sub-lane's
`msn_*.py`.

Nothing in `E:\Projects\NifskopeWildWastelandEdition` was read for anything but
reference, and nothing there was modified.
