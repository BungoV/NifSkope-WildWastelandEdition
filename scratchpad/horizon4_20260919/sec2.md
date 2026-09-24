
### 1.10 A correction the whole table needs: N.L is not in the statistic

SUNSIM1's disagreement compares the truth SHADOW RAY against the baked HORIZON
only (`render.baked_lit`). Neither side carries N.L -- the shipped shader
multiplies it in afterwards (`shade.py` line 40). So on a surface whose normal
faces AWAY from the sun the truth cast says "dark" (its ray walks into its own
wall) and a bake that stores 0 says "lit", they disagree, **and the rendered
pixel is black either way**. That disagreement cannot reach bungo's picture.

It is not a small share. Street camera, decided object pixels with N.L > 0:

| cell | decided object px | of which N.L > 0 |
|---|---:|---:|
| street 120/5 | 479,413 | 452,455 (94.4%) |
| street 240/15 | 480,633 | **70,507 (14.7%)** |

Every row is therefore re-scored over the N.L > 0 pixels as well
(`extra4.py`/`extra4.log`, `extra5.py`/`extra5.log`). Both columns are kept; the
**N.L > 0 column is the one that answers "does the right panel look like the
left panel"**, and it is the column the ranking uses.

### 1.11 The baked object horizon: which branch of the normal rule does the damage

**This section is for the record, not for a repair.** bungo's ruling of 04:3x
sent object far shadows to the identity route, and his redirect a few minutes
later took the baked horizon off the table altogether -- *"we're not doing the
horizon thing"* -- objects **and** terrain. No C++ in this lane touches
`lodgenHorizonCastAt`, and nothing below is a proposal. What follows is only the
measurement, so the decision is on paper and can be re-opened with numbers
instead of from scratch.

**One honesty note about the pictures.** `images/row_SHIP_*.png` were rendered
before the redirect arrived and their title strip still reads *"the repair this
lane writes in C++"*. **No such C++ was written.** The strip is wrong and the
run is not repeated (it costs a bake sweep); the pictures are kept because their
panels are still the measurement they were made for.

Widening the vertical band is not the whole story: row O2v (band 0.02, every
branch kept) still scores 78.63% on the street at 240/15 where O2m (no rule at
all) scores 13.11%. The branches, isolated on the same two-sided ceiling data
with the band at 0.02 (`extra5.log`):

| variant | mean obj | mean obj N.L>0 | st120/5 obj (N.L) | st240/15 obj (N.L) | ea240/15 obj (N.L) |
|---|---:|---:|---|---|---|
| Vfull -- every branch kept | 32.68 | 30.69 | 36.74 (36.57) | **79.35 (66.34)** | 40.62 (27.96) |
| **Vnoback -- "back of a vertical face stores 0" REMOVED** | 32.35 | **27.56** | **29.43 (29.79)** | **17.91 (5.36)** | 37.47 (26.31) |
| Vnodown -- "pointing down stores 0" removed | 34.77 | 32.33 | 38.39 (38.10) | 74.33 (65.57) | 40.33 (28.03) |
| Vnone -- both removed | 34.48 | 29.20 | 31.07 (31.32) | 13.36 (4.59) | 37.07 (26.39) |
| Vbare -- both removed and no tangent clamp | 35.50 | 29.61 | 32.19 (32.49) | 13.11 (4.53) | 36.94 (27.38) |

**The back-of-a-vertical-face branch is the catastrophic one**: 79.35% against
17.91% on the street at a western sun, and 66.34% against 5.36% on the pixels
the eye actually sees. The down branch is worth keeping *once the band is wide
enough for it to fire only on faces that really point down* (Vnoback 27.56
against Vnone 29.20). The tangent clamp is worth 0.4 points.

**The best baked-object row this lane reached**, for the record, is
**SHIP** = byte-safe band 0.02, no back-of-vertical zero, down rule and tangent
clamp kept, a two-sided +/-16 u cast storing the lower skyline, with the 64-bin
terrain sheet beside it:

| | mean ALL | mean ter | mean obj | mean obj N.L>0 | st120/5 obj | st240/15 obj (N.L) |
|---|---:|---:|---:|---:|---:|---|
| O1, the bake today | 39.99 | 20.23 | 48.77 | 54.53 | 51.47 | 79.11 (87.05) |
| SHIP, the best baked row | 25.20 | 7.35 | 32.35 | 27.56 | 29.43 | 17.91 (5.36) |
| B2, tier 3 (+21.8 MB) | 6.66 | 7.35 | 5.53 | 5.15 | 17.38 | 7.35 (4.63) |
| **M1, the runtime shadow map** | **11.41** | 12.25 | 11.25 | **7.89** | **11.99** | 37.52 (**3.47**) |

`images/row_SHIP_street_az120_el05.png`, `images/row_SHIP_east_az240_el15.png`.

**Even repaired, the baked object horizon loses to the shadow map on the street
camera bungo judges by** (29.4% against 12.0%), and the only baked row that
beats the map costs 2.7 times the whole existing object stream. The ruling and
the measurement point the same way.

### 1.13 Row M5 -- the CK layer as the building id, and why it buys nothing here

The IDENT lane found Fallout 4's one mechanism for "this is one building": the
Creation Kit layer `XLYR -> LAYR`. M5 merges two v7 groups into one identity
when their placements point at the same layer AND that layer's editor id names a
building. **The name rule, stated so it can be argued with**: the EDID contains
"bld", "building" or "tower", case-insensitively. It is a string test on an
authoring label that nothing checks for geometric truth, which is why this is a
ceiling and not a proposal (`m5.py`, `m5.log`, `m5.json`).

**On chunk 4.4.-12 it changes almost nothing: 588 identities become 586.**
2,133 of the 2,449 placements carry an XLYR, in **30 distinct layers, of which
only 3 are building-named**. The chunk is South Boston and its layers are
DISTRICT names:

| placements | layer |
|---:|---|
| 260 | `DN135_GwinnettExt` |
| 230 | `AndrewStation` |
| 174 | `SouthBostonBlock19` |
| 152 | `SouthBostonBlock14` |
| 145 | `SouthBostonBlock03` |
| ... | ... 13 more `SouthBostonBlockNN` ... |
| 56 | `Theater47_Bld01` |
| 33 | `Theater46_Bld01` |
| 3 | `Theater_Buildings` |

M5's numbers are therefore M1's to the second decimal: mean ALL 11.41%,
terrain 12.25%, objects 11.25%, objects N.L>0 7.89%, self-shadow refused 15.10%.

**What that means, plainly.** The layer route is not wrong -- the IDENT lane
measured 102 of 225 downtown layers as genuinely per-building, with a median
0.970 of their references in one touching blob. It is simply **absent from this
chunk**, where the artists layered by city block instead. So the layer cannot be
the identity on its own: it would work in the Theater district and do nothing in
South Boston. A merge that fires only where a name matches gives two different
shadow behaviours in two neighbourhoods, which is worse than either.
`images/row_M5_street_az120_el05.png`, `images/row_M5_east_az240_el15.png`.

## 2. The causes, ranked by how much of the picture each repairs

Ranked on the **N.L > 0** column, because that is the column that can reach the
eye.

**Read this whole section as a record, not a plan.** The redirect of 04:3x
takes the baked horizon off the table for objects and terrain both, so every
cause below that names a baked repair is closed. It is written down because the
measurements exist and because a closed route with numbers beside it can be
re-opened cheaply; nothing here is offered as a default.

### 2a. OBJECTS -- ranked, and what the rulings do to each

**1. One horizon per receiver POINT cannot represent a shadow boundary that
crosses a face.** This is the cause that outlives every repair: even with every
rule fixed (SHIP) the street at 120/5 is still 29.4% wrong, and even the whole
tier-3 stream at +21.8 MB (B2) is still 17.4% wrong. A horizon stored at a point
can say *when* that point goes dark, never *where* the line falls.
**The representation that does not have this limit is a shadow map** -- M1 gets
the same cell to 12.0% (11.5% lit-side) for zero file bytes. *This is the cause
bungo's ruling acts on, and the measurement agrees with the ruling.*

**2. The "back of a vertical face stores 0" branch fires on the face you are
looking at.** 66.3 -> 5.4 lit-side on the street at 240/15. **No C++ this lane**
(ruling). It stays a fact on the record in s1.11.

**3. The vertical band (1e-3) is narrower than the normal quantiser's step, so
41.6% of all vertices are walls filed as "pointing down" and store sixteen
zeros.** Causes 2 and 3 are one edit; together they are 54.53 -> 27.56 mean
lit-side objects. **No C++ this lane** (ruling).

**4. The receiver sits on its own surface** -- a single cast from a vertex reads
its own wall as a 60-degree skyline. Worth 34.48 -> 32.35. **No C++** (ruling).

**5. Vertex density.** Inserting vertices on long edges (HORIZON3 tier 2) is
*worse* than the row it refines (44.84 and 44.43 against 43.50) at 1.94 MB and
7.81 MB. Ruled out as a cause, which confirms HORIZON3 s1(b) from the picture
side: the missing detail is in the face INTERIOR, not on its edges.

### 2b. TERRAIN -- measured first, then ruled out of scope

**These numbers were taken while terrain was still in scope, and the redirect
arrived after they were on disk.** They are kept, and none of them is a
proposal: no `growth` change, no 64-bin sheet, no gate. Read this as "what the
baked terrain sheet had available, if anyone ever asks again".

**1. The march is approximate.** The ceiling at the SAME texel size and the SAME
16 bins scores **12.99% against the shipped sheet's 20.23%**: 7.2 of the 20.2
points are the segment ladder, not the format. Row T1 -> T2a. No ruling, no
bytes -- it is bake time (`growth` 1.3, elevation read at the segment's NEAR
end). **This lane does not change `growth`**, because the ladder ceiling says
how much is available and not which growth buys it; that measurement is named in
s5 as owed.

**2. Sixteen azimuth bins are too coarse.** T2a 12.99 -> T3a 11.22 (32 bins) ->
**T3b 7.35 (64 bins)**, at +100% and +300% sheet bytes
(262,144 -> 524,288 -> 1,048,576 B a 4,096 u cell). **Not proposed** -- the
redirect closed the route before it could be ruled on.

**3. The street case.** Terrain at street 120/5 is 41.98% wrong and at
street 240/15 47.62% -- the two worst terrain cells in the table, and 64 bins
only takes them to 25.04% and 31.50%. A low sun along a street is a long,
grazing terrain ray, and 22.5 degrees of azimuth is hundreds of units of lateral
error at that reach. This is the terrain half of cause 2a-1 and 64 bins is a
partial answer, not a cure.

### Measured and ruled OUT as terrain causes

* **Texel size.** 32 u 12.99% vs 64 u 13.06%. Nothing there -- and 64 u would
  save 75% of the sheet.
* **Spatial interpolation at read.** Turning it off costs 0.11 points (T5 13.10).
* **Receiver height.** Ground 13.29, +48 u 13.35, shipped +12 u 12.99.
* **Bin = sector MAX instead of bin CENTRE.** The bake does CENTRE and centre is
  right: 12.99 against 16.71 for max.

### 2c. The identity route, ranked against the best baked row

| | file bytes on the region | runtime memory | mean ALL | mean obj N.L>0 | street 120/5 ALL |
|---|---|---|---:|---:|---:|
| the bake today (O1/T1) | 7,982,096 B + 262,144 B a cell | none | 39.99% | 54.53% | 46.41% |
| SHIP, the best baked object row | unchanged | none | 25.20% | 27.56% | 27.60% |
| B2, tier 3 | **+21.8 MB** + a 64-bin sheet | none | 6.66% | 5.15% | 21.46% |
| **M1, runtime far shadow map at 64 u** | **none** | 0.87 MB | 11.41% | **7.89%** | **12.82%** |
| M3, the same at 16 u | none | 13.56 MB | 10.92% | -- | 11.47% |
| M4, identity = one welded solid | none | 0.87 MB | 10.44% | -- | 12.74% |
| M5, identity merged by Bld-named layer | none | 0.87 MB | 11.41% | 7.89% | 12.82% |

**Plainly: the identity route wins on the picture and it wins on cost.** For
zero file bytes and 0.87 MB of runtime memory it beats every baked row that fits
in today's budget, and it beats tier 3 on the street camera bungo is judging by.
Its worst-looking cell -- a western sun on a street, 37.52% on all object pixels
-- is **3.47% on the pixels N.L lets you see**, because self-shadow is
overwhelmingly on faces the sun is already behind. Texel size barely matters:
64 u -> 16 u is 11.41% -> 10.92%, so it should ship at 64 u.

**What would have to be true for it to be wrong.** The map has to be rebuilt
when the sun moves, over the LOD meshes of every loaded chunk, and this lane has
not costed that draw. The identity has to be per texel. s1.4 shows the shipped
v7 group is not "one thing" in 50 of 120 multi-placement cases; s1.5 shows
repairing that is worth 1.4 points of the 4.1 the identity rule costs, so two
thirds of the cost is real self-shadowing that no identity scheme can return.
And every number here is from a simulator, never from the engine.
