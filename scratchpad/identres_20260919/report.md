# IDENTRES -- the blocks are the test's resolution, not the identity

bungo, 2026-09-19, looking at `identprox_20260919/images/hwy_today_hwydeck_az180_el10_t64.png`:
*"why is the identity again so pixelated? ... where are those large pixels coming from"*.

The claim this lane had to prove with pictures is that the blocks come from the **64 u texel and the
single nearest tap** chosen for that test, not from the `.lodi` identity data. So the identity is
held **completely fixed** -- today's shipped v7 group table, 588 groups over 2,449 placements, the
same table identprox used -- and the only thing that moves is how the occluder is looked up.

Every figure below is in `run.log` and `rows.json`. Nothing is quoted here that is not in one of them.

---

## 1. What was run

Reused, not rewritten: sunsim1's scene, camera and shading (`scene.py`, `render.py`, `shade.py`),
horizon4's `h4map.ShadowMap` and `h4core`, identprox's `hwy_render.query`, `hwycams`, `join`. New in
this lane: `identres.py` (the PCF tap loop and the identity-carrying sun cast) and `run.py` (the
driver). Nothing was written into another lane's folder; both G-buffers and both LEFT panels were
read from their existing caches.

| row | RIGHT-panel lookup | map size over the whole chunk |
|---|---|---|
| **R1** | 64 u a texel, ONE nearest tap, flat bias (normal offset 1.0 texel, depth 0.5 texel) | 323 x 133 = **42,959** texels (az 180) / 440 x 194 = **85,360** (az 120) |
| **R2** | 16 u a texel, 3x3 percentage-closer, slope-scaled depth bias | 1,283 x 507 = **650,481** / 1,751 x 750 = **1,313,250** |
| **R3** | 4 u a texel, 3x3 percentage-closer, slope-scaled depth bias | 5,123 x 2,004 = **10,266,492** / 6,997 x 2,976 = **20,823,072** |
| **R4** | **no map at all** -- a per-pixel sun cast against the same heightmap and the same 29,587 `.lodo` triangles, a blocker discarded when its triangle carries the receiver's own identity | none |

4 u was affordable: R3 costs 25-29 s a view against R1's 0.5 s, and 62-125 MB at the 4+2 byte
layout. It is the resolution band a GPU cascade actually works in.

R4 is the limit case. Its machinery is the LEFT panel's own sheared cast with one thing added: each
16 u cell carries the largest s = z - u*tan(el), the identity that achieved it, **and** the largest
s among entries carrying a *different* identity. Two scatter passes build that second plane and one
right-to-left merge scan accumulates it along the sun direction. A receiver can therefore discard
its own group and still see everything else, exactly.

### Controls (all in `run.log`)

* **R1 is identprox's code, called.** `identres.query_pcf` at one tap is asserted array-identical to
  `identprox/hwy_render.query`: `True` for both views.
* **R1 reproduces the published picture.** On identprox's own denominator (it dropped the pixels the
  map held no texel for) R1 gives **ALL 11.29% / terrain 13.13% / objects 10.21%** -- the three
  numbers printed under `hwy_today_hwydeck_az180_el10_t64.png`. Exact, to the last digit.
* **R4 differs from the truth only by the identity rule.** Re-run with the identity rule switched
  OFF, the same cast agrees with the LEFT panel on **100.00%** of 1,027,477 (hwydeck) and 896,877
  (east) decided pixels.
* **One denominator.** Every row also scored on the pixels all three maps held a texel for
  (632,493 px = 61.6% of the hwydeck frame; 811,327 px = 90.5% of east). Same ordering.
* **Only the texel.** R2 and R3 move texel, filter and bias together because that is the realistic
  combination. Each map was therefore *also* read R1's way -- one nearest tap, flat bias -- so one
  variable moves. Those numbers are the last column of the table.

---

## 2. The table

Disagree = the row calls a pixel lit where the ray-cast sun calls it shadow, or the reverse, over
**all** decided pixels of the frame (one denominator for all four rows). *false-DARK* = shadow the
row invents; *false-LIT* = shadow the row loses. **Self-shadow lost** = of the object pixels the sun
test puts in shadow behind a caster carrying the receiver's **own** group, the share the row hands
back to the light (33,657 px hwydeck, 33,479 px east).

### camera "hwydeck", sun azimuth 180, elevation 10

| row | ALL | terrain | objects | obj false-DARK | obj false-LIT | self-shadow lost | no map data | ALL, read R1's way |
|---|---|---|---|---|---|---|---|---|
| R1  64 u nearest | 9.09% | 8.50% | 9.94% | **2.16%** | 7.78% | 60.71% | 373,653 px | 9.09% |
| R2  16 u PCF | 8.47% | 9.23% | 7.39% | **0.55%** | 6.84% | 42.62% | 390,837 px | 8.42% |
| R3  4 u PCF | 8.45% | 9.37% | 7.13% | **0.59%** | 6.54% | 39.64% | 391,803 px | 8.47% |
| R4  no map | **0.58%** | **0.00%** | **1.41%** | **0.00%** | 1.41% | 17.76% | 0 | 0.58% |

### camera "east", sun azimuth 120, elevation 15

| row | ALL | terrain | objects | obj false-DARK | obj false-LIT | self-shadow lost | no map data | ALL, read R1's way |
|---|---|---|---|---|---|---|---|---|
| R1  64 u nearest | 9.66% | 6.69% | 10.95% | **3.88%** | 7.06% | 96.36% | 81,183 px | 9.66% |
| R2  16 u PCF | 7.66% | 6.03% | 8.37% | **1.09%** | 7.29% | 97.60% | 85,003 px | 7.74% |
| R3  4 u PCF | 7.68% | 6.56% | 8.17% | **0.86%** | 7.30% | 97.65% | 85,216 px | 7.71% |
| R4  no map | **3.53%** | **0.00%** | **5.07%** | **0.00%** | 5.07% | 94.55% | 0 | 3.53% |

Reading it:

* **The blocks are `objects false-DARK`** -- shadow the lookup invents on an object standing in the
  sun. It falls **2.16% -> 0.55% -> 0.59% -> 0.00%** (hwydeck) and **3.88% -> 1.09% -> 0.86% ->
  0.00%** (east). Four times smaller at 16 u, gone entirely with no map. Read R1's way (one tap,
  flat bias, only the texel moved) the same collapse is there: 2.16 -> 0.63 -> 0.59 and 3.88 ->
  1.21 -> 0.88. That is the answer to the question.
* **`objects false-LIT` barely moves with the texel** (7.78 -> 6.84 -> 6.54; 7.06 -> 7.29 -> 7.30)
  and only collapses at R4. Lost shadow is the identity **rule**, not the resolution.
* **Terrain is never touched by identity** -- today's rule judges terrain on depth alone -- and R4
  proves it: 0.00% on both views.
* **The two cameras disagree about self-shadow, and that is real.** At hwydeck the exact cast keeps
  most of it (17.76% lost); at east almost none survives (94.55% lost even with no map anywhere in
  the answer). At azimuth 120 / elevation 15 the pixels a building shades on itself have nothing
  else above them, so the identity rule removes them outright. No texel size can fix that.

### The one number that does not improve, and why I am not claiming it does

Terrain disagreement rises slightly as the texel gets finer (8.50 -> 9.23 -> 9.37 hwydeck), and it
rises **entirely on the false-LIT side** (6.02 -> 7.34 -> 7.32) while false-DARK falls. It survives
both controls: the common denominator (7.38 -> 10.68 -> 10.60) and the R1-style read (6.02 -> 7.20
-> 7.33), so it is neither the shrinking denominator nor the filter.

The leading explanation is that the **LEFT panel's own footprint is 16 u** -- four times coarser
than R3's texel -- and a 16 u max-cell casts a slightly fatter terrain shadow than the real surface.
A 64 u map over-shadows in the same direction and so agrees with it by accident; a 4 u map does not.
Under that explanation the extra disagreement is the truth panel's error, not the map's.
**This is unproven.** The refuter: rebuild the LEFT panel with a 4 u footprint and re-score. If
terrain false-LIT at 4 u then drops below the 64 u figure the explanation stands; if it does not,
something in the map's terrain representation loses shadow at fine texels and I was wrong. Either
way it does not touch the object numbers above, which are what the question was about.

### The blue tint

identprox's right panel wore a teal wash (`shade.NODATA`) on every pixel the shadow map held **no
texel at all** for -- almost all of it terrain outside the map's own extent, which stops at the
chunk plus a 2,048 u margin. It marked *"the map was never asked about this pixel"*, not *"this
pixel is in shadow"*. It is switched off in every picture here, so the two panels carry one palette
and the eye compares shadows only; the count it used to mark is printed in each caption and in the
table instead (it is 36% of the hwydeck frame, which is why that camera's terrain column is soft).

---

## 3. The pictures

All in `scratchpad/identres_20260919/images/`, 1600x900 a panel, LEFT always the ray-cast sun, same
palette both sides.

| file | what it shows |
|---|---|
| `identres_hwydeck_R1_az180_el10.png` | the baseline -- the picture bungo asked about, minus the tint |
| `identres_hwydeck_R2_az180_el10.png` | 16 u, 3x3 PCF |
| `identres_hwydeck_R3_az180_el10.png` | 4 u, 3x3 PCF -- a GPU cascade |
| `identres_hwydeck_R4_az180_el10.png` | no map at all |
| `identres_east_R1_az120_el15.png` | the same four rows, camera "east", sun az 120 el 15 |
| `identres_east_R2_az120_el15.png` | |
| `identres_east_R3_az120_el15.png` | |
| `identres_east_R4_az120_el15.png` | |
| `identres_zoom_hwydeck_R1_vs_R4.png` | **the one to look at**: the same 560 x 340 crop of the deck and the columns under its far end (x 660..1220, y 328..668; 38,207 deck pixels, 89 column pixels), magnified 2x with no smoothing, R1 beside R4 |

Working files: `run.py` (driver), `identres.py` (engine), `probe.py` + `probe.log` (what 4 u costs),
`run.log` (every number), `rows.json` (the same as data), `run_stdout.log` (map sizes).

---

## 4. For bungo

1. The big square blocks are the test's own shadow map, not the identity data -- that test stored
   one shadow sample every 64 units and read back exactly one of them, so a whole 64-unit square of
   the world got one yes-or-no answer.
2. Shrink that sample to 16 units and average nine of them and the invented blocky shadow on objects
   drops to about a quarter of what it was; go to 4 units, which is what a real game shadow map
   uses, and the blocks are gone from the picture.
3. With no map at all -- asking the geometry directly for every pixel -- the identity rule's own
   error is 1.4% of object pixels on the highway camera and 5.1% on the east camera, and it is
   always a shadow that is missing, never a blocky one that should not be there.
4. What identity still costs is self-shadowing: on the east camera 95% of the shadow a building
   casts on itself disappears even with a perfect lookup, because with nothing else above it there
   is no second caster to take over -- that is the rule, and no resolution fixes it.
