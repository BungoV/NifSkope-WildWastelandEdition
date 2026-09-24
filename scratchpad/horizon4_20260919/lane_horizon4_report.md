# Lane HORIZON4 -- what it takes to make the baked far shadows match a ray cast

## 0. State at launch

`date` at lane open: **Sat Sep 19 03:46:22 CEDT 2026**.

`tasklist | grep -i -E "Fallout4|NifSkope"` at 03:46:22 -> `Fallout4.exe 17248 Console 1 559,588 K`,
no NifSkope. **The game is up.** Therefore, for this whole lane: no `make`, no `qmake`, no
`release/NifSkope.exe`, no lodgen run, no GUI harness. Python only, plus C++ source edits gated
with `g++ -fsyntax-only`. The lane ends at `PENDING.md` headed `BUILD PENDING`.

Markers: `scratchpad/horizon4_20260919/BUILDING` touched 03:46:22. This report is written
incrementally; sections appear as they land.

### Tree state inherited

Lane HORIZON3 is parked at BUILD PENDING with UNBUILT, uncommitted edits in `src/` and `tests/`
(`lodofile`, `lodifile`, `esmdata`, `nativeemit`, `lodinative`, `nifcli` .cpp + headers). This lane
works **on top of** those edits: nothing of HORIZON3's is reverted, reformatted or tidied. Every
file this lane touches that HORIZON3 also touched is listed in s3.

### Inputs this lane reuses (all read-only unless noted)

| what | path |
|---|---|
| the simulator | `scratchpad/sunsim1_20260919/{scene,shade,render,cams,run,control}.py` |
| cached scene | `scratchpad/sunsim1_20260919/scene_objects.npz`, `scene_sheet.npz` |
| SUNSIM1 numbers | `scratchpad/sunsim1_20260919/disagreement.json`, `controls.json` |
| heightmap dump | `scratchpad/horizon2_20260918/land.bin` (+ `wit.py` class `Land`) |
| baked native far field | `scratchpad/horizon2_20260918/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth.lodo` + `.lodi` |
| baked VT horizon sheets | `scratchpad/horizon2_20260918/dumpbake/vt/.../Commonwealth.VT.*.lodt` |
| bake rules in C++ | `src/lodghorizon.h` (`lodgenHorizonCastAt`), `src/nativeemit.cpp` |

### The truth cast's standing limits (restated beside every number in this report)

1. No neighbouring-chunk objects (the bake carries cx=1,cy=-3 only, 2,446 instances).
2. Tree impostor cards are not alpha-tested -- a card casts a solid quad shadow (too much shadow).
3. The shadow ray has a **16-unit footprint** (sheared suffix-max), so it errs toward MORE shadow;
   measured agreement with exact brute force 82-93% (SUNSIM1 s4 control A).
4. Primary visibility of objects is rasterised, not ray-cast (same answer, different method).
5. Terrain beyond 150,000 u is absent.

These limits are *common to the LEFT panel in every row below*, so they cancel when rows are
compared against each other -- which is what the ceiling table in s1 does. They do NOT cancel when
an absolute disagreement percentage is read as "how wrong the bake is".

## 1. The ceiling table (step 1)

### 1.0 The instrument, and the control that says it is one

Every row below is SUNSIM1's simulator with the RIGHT panel's input replaced,
one rule at a time. The LEFT panel is never touched, so a row's number is the
effect of that one change.

**The ceiling horizon is not a better version of the bake's march.**
`lodgenHorizonCastAt` walks a geometric segment ladder over a max-Z lattice and
reads the elevation at each segment's NEAR end; that march has a measured
residual bias (+0.86 deg at growth 1.3, HORIZON2) and it is an APPROXIMATION of
a quantity. The ceiling has to be the quantity, or the table measures the march
instead of the representation. So the ceiling is computed with the LEFT panel's
own geometry -- the identical 16-unit near grid with the identical triangle
splat, the identical 256-unit far grid, the identical reach and bias -- run at a
ladder of elevations. `s = z - u tan(el)` is constant along a sun ray, so one
suffix maximum per ladder rung answers every receiver at once and a receiver's
horizon is the largest rung at which it is still blocked
(`h4core.py`, class `ShearGrid`).

Ladder: 0.5 deg from 0 to 50, then 4 deg to 90. The file's quantiser is 0.3529
deg a step. **The ceiling therefore UNDER-states the true horizon by up to half
a rung, i.e. it errs toward LESS shadow** -- the opposite side from the LEFT
panel's 16-unit footprint, which errs toward more. Both are stated, neither is
corrected for.

**Control 1, known-answer** (`p_control.py`, `p_control.log`): for 4,000 random
ground points and 4,000 random points on object triangles, the ceiling horizon
at azimuth A must predict lit/dark at elevation E exactly as
`render.SunShadow(A, E).lit` does.

| sun | terrain agree | objects agree |
|---|---:|---:|
| az 120 el 5 | 100.00% | 99.98% |
| az 120 el 15 | 100.00% | 99.95% |
| az 120 el 30 | 100.00% | 99.78% |
| az 240 el 5 | 100.00% | 99.98% |
| az 240 el 15 | 100.00% | 99.88% |
| az 240 el 30 | 99.95% | 99.85% |

**Red control**, the same receivers scored against the horizon of an azimuth 90
degrees away: **77.61%** (az 120) and **80.66%** (az 240). The instrument is the
left panel's own geometry, and it is sensitive to which direction it is asked
about.

### 1.1 The sweep

`sweep.py` -> `ceiling.npz`, log `sweep.log`: **64 azimuths** 5.625 degrees
apart. Bin centres for A = 16 are every 4th of them, A = 32 every 2nd, A = 64
all -- so one sweep feeds every bin-count row, and the maximum over the five
fine azimuths inside a 22.5-degree bin is the SECTOR maximum row T4 needs, free.
Receiver sets: the 53,396 `.lodo` vertices at their own position, the same
pushed +16 u and -16 u along the vertex normal, the midpoints of every drawn
edge over 256 u, and terrain texel centres at 64 u and 32 u over chunk 4.4.-12.
`sweep2.py` adds 16 azimuths for the receiver-height rows and the per-face
receivers.

### 1.2 Rows M1-M3 -- the runtime far shadow map with a group identity

These rows do not change what the bake stores. They throw the stored horizons
away entirely and shade the RIGHT panel from an **orthographic depth map taken
from the sun** over the chunk's LOD meshes plus terrain, each texel also holding
the caster's **shadow identity = the `.lodi` v7 group id**
(`docs/LODGEN_NATIVE_LODO_LODI.md` s4.9; SCOL = one group, architecture kit
pieces touching within 16 u = one group). A receiver pixel is dark when the map
is nearer **and** the map's identity differs from the receiver's own group.
Terrain is its own identity and is judged on depth alone with a slope bias.

Light space is the same shear as every other row in this lane (`h4map.py`), so a
map row and a horizon row cannot differ because of a coordinate convention.
Bias is a **normal offset** (1.0 texel along the receiver's own normal) plus 0.5
texel of depth, chosen before the first row was measured: at a 5-degree sun a
constant depth bias big enough to stop acne is a 730-unit peter-pan at 64 u a
texel, because a horizontal surface crossing one `s` texel spans
`texel / tan(el)` = 11.4 texels of depth.

Python only; no C++ for this route in this lane.

**Means over the 16 camera x sun cells** (`map_rows.json`, log `map_rows.log`):

| row | texel | map texels (max over the four suns) | ALL | terrain | objects | objects, identity OFF | self-shadow refused |
|---|---:|---:|---:|---:|---:|---:|---:|
| M1 | 64 u | 144,760 (0.87 MB at 4 B depth + 2 B id) | **11.41%** | 12.25% | 11.25% | 7.14% | 15.10% |
| M2 | 32 u | 570,050 (3.42 MB) | **10.98%** | 12.84% | 10.37% | 5.85% | 15.42% |
| M3 | 16 u | 2,260,541 (13.56 MB) | **10.92%** | 13.11% | 10.21% | 5.30% | 16.36% |

Against SUNSIM1's baked baseline, whose mean over the same cells is **44.1%**.

**The headline cells** (the ones the pictures are made from):

| cell | M1 ALL | M2 ALL | M3 ALL | baked O1 ALL |
|---|---:|---:|---:|---:|
| street 120/5 | **12.82%** | 11.64% | 11.47% | **46.41%** |
| street 240/15 | 27.20% | 27.60% | 27.94% | 62.33% |
| east 120/15 | 10.48% | 9.22% | 8.41% | 29.42% |
| east 240/15 | 16.42% | 15.83% | 15.42% | 48.94% |

**Three things this table says plainly.**

1. **The map route is three to four times closer to the ray cast than anything
   the baked horizons do**, in every cell, on terrain and on objects alike. That
   is not a tuning margin, it is a different order of wrongness.
2. **Texel size hardly matters.** 64 u -> 16 u, a 16-fold rise in memory, moves
   the mean from 11.41% to 10.92% -- half a point. Whatever is left over at 64 u
   is not a resolution problem, so paying M3's 13.56 MB buys nothing. If this
   route ships, it ships at 64 u.
3. **The identity test is what is costing the route its remaining error, not the
   depth test.** With identity OFF the objects column is 7.14 / 5.85 / 5.30%;
   with it ON, 11.25 / 10.37 / 10.21%. Identity roughly doubles the object error.

### 1.3 What excluding self-shadow costs, measured

"Self-shadow refused" above is the share of **truth-dark object pixels** that the
depth test alone darkens correctly but the identity test lets back to lit --
a house that never shades its own wall or its own roof. Worst cells:

| cell | M1 | M2 | M3 |
|---|---:|---:|---:|
| street 240/15 | 30.82% | 32.65% | **36.01%** |
| east 240/15 | 28.17% | 29.43% | 30.04% |
| east 120/15 | 20.74% | 20.21% | 23.02% |
| street 120/5 | 1.08% | 1.33% | 1.57% |

At a 5-degree sun down the street the identity rule costs almost nothing,
because at that elevation almost every dark pixel is shaded by something else.
At 15 degrees from the west it costs **a third of all the correctly-dark object
pixels** -- and that is exactly the cell where the M rows look worst
(objects 37.5-41.4% with identity, 9.9-11.7% without).

**So the identity rule is not a free way to kill acne. It is a trade: it removes
the self-shadow acne and it removes a third of the real self-shadowing with it.**
The number to hold on to is that column: 9.90% vs 41.44% on street 240/15, M3.

### 1.4 Is a group "one thing"? -- bungo's doubt, counted

He said identity "relied on each lod object being one thing, so a house, a single
thing, a tower, a single thing." Chunk 4.4.-12 carries 2,449 placements in
**588 groups** (468 singletons, 77 of size 2-9, 32 of 10-49, 11 of 50 or more,
largest 205). Measured three ways (`groups.py`, `groups.log`, `groups.json`),
using the same world boxes the grouping rule itself uses:

**F1 -- a group that is several separate solids.** Re-run the connected-component
join at tolerance 0 instead of 16 u: **50 of the 120 multi-placement groups fall
into more than one piece.** Five named:

| group | placements | falls into | widest gap | a base model in it |
|---|---:|---:|---:|---|
| 100277 | 205 | 48 pieces | 2,549 u | `PGarageInFloor1x2Str01.nif` |
| 100022 | 58 | 36 pieces | 1,808 u | `PGarageInFloor2x2Str01.nif` |
| 100100 | 71 | 8 pieces | 1,789 u | `DecoMainA1x1DoorSingle02.nif` |
| 100545 | 168 | 12 pieces | 1,664 u | `ChurchRoofA1x1Str01Full01.nif` |
| 100284 | 29 | 8 pieces | 1,304 u | `DecoLobbyC1x1WinB01Full03.nif` |

**F2 -- a group far wider than a building.** Longest footprint side: p50 443 u,
p90 1,198, p99 3,203, max 4,096 (a whole chunk). 27 groups are wider than
2,048 u. Group 100306 holds 129 placements spread over 3,200 u while its widest
single member is 720 u -- a terrace that joined up into one identity, so no part
of it can shadow any other part.

**F3 -- one placement that is several things.** Weld each drawn LOD mesh at 0.01
mesh units and count lumps more than 256 u apart: **56 of 2,449 placements are
themselves two or more separate solids.** Five named:

| placement | lumps | spread | base model |
|---:|---:|---:|---|
| 281 | 9 | 3,004 u | `HWDoubleStrExit01.nif` |
| 1664 | 10 | 1,937 u | `HWDoubleEndCapL03.nif` |
| 280, 881 | 8 | 1,613 u | `HWDoubleStr01Damaged01.nif` |
| 2112 | 25 | 1,488 u | `SouthBostonMonument01.nif` |
| 97 | 9 | 1,280 u | `HWDoubleStr01.nif` |

**bungo's doubt is correct and it is the biggest single failure in the M rows.**
The highway pieces are the clearest case: one placement is nine separate deck
segments strung over 3,000 units, and with identity on, no segment can ever
shadow the segment beside it, even though in the ray cast it plainly does. The
architecture join at 16 u is the other half: a 205-placement garage that is
really 48 buildings is one identity, so those 48 buildings are mutually
transparent to the sun.

### 1.5 Row M4 -- the same map with the identity bungo MEANT

s1.4 counts how often a v7 group is not one thing. M4 answers the next question:
**if every lod object really were one thing, how much of the map route's error
would that recover?** Identity here is a maximal set of drawn LOD triangles that
are physically connected, welded in **world space across placement boundaries at
8 units** -- so two kit pieces that touch are one solid, and nine highway deck
segments strung over 3,000 units are nine solids. Nothing in the file carries
this id today; M4 is the ceiling of the identity idea, not a proposal.
`m4.py`, log `m4.log`, 64 u a texel so it is directly comparable with M1.

**Census**: 588 v7 groups -> **1,508 welded solids** over 2,449 placements.
**234 of the 588 groups hold more than one solid** (largest holds 68), and --
the other direction, which s1.4 did not look at -- **112 solids span more than
one group** (largest spans 47), i.e. the group table also *splits* things that
are physically one piece, which makes a wall shadow-fight with the wall welded
to it.

| row (64 u) | ALL | terrain | objects |
|---|---:|---:|---:|
| M1, identity = the shipped v7 group | 11.41% | 12.25% | 11.25% |
| **M4, identity = one welded solid** | **10.44%** | 12.25% | **9.82%** |
| M1 with the identity test switched OFF entirely | -- | -- | 7.14% |

**Read that carefully, because it cuts both ways.**

* Fixing identity is worth **1.4 points of object error** (11.25 -> 9.82).
  bungo's doubt is real and it is worth money, but it is not the main cost.
* The identity idea as a whole costs **4.1 points** (7.14 -> 11.25). A *perfect*
  "one thing" identity recovers only a third of that. **The remaining two thirds
  is genuine self-shadowing inside a single building** -- one wall of a house
  shading another wall of the same house, a parapet shading its own roof. No
  identity scheme, however clean, can give that back, because excluding
  self-shadow is precisely what identity is for.
* Worst cell is unchanged: street 240/15 objects 33.72% (M4) against 37.52%
  (M1) and 11.71% with no identity at all. The west-lit street is where a
  building shading itself dominates the picture.

So the honest ranking of the identity rule is: it is a cheap, robust way to kill
self-shadow acne in a far shadow map, it is measurably harmed by the v7 group
table not being "one thing", and even repaired it throws away about a third of
the real object shadowing at a low western sun. Whether that trade is better
than the alternative (no identity, a real normal-offset + slope-scaled bias
doing the acne work) is **a ruling for bungo, not a default** -- it is in
s6, ROWS FOR BUNGO, with its pictures.

### 1.6 The baseline control -- O1 and T1 reproduce SUNSIM1 exactly

Row O1 is the stored `.lodi` v8 bytes and row T1 is the stored role-7 terrain
sheet, both read the way the bake intends. They must reproduce SUNSIM1 s5.1 to
the digit or the instrument has drifted. They do, on all six published cells:

| cell | O1 / T1 ALL / ter / obj | SUNSIM1 s5.1 |
|---|---|---|
| street 120/5 | 46.41 / 41.98 / 51.47 | 46.41 / 41.98 / 51.47 |
| street 240/15 | 62.33 / 47.62 / 79.11 | 62.33 / 47.62 / 79.11 |
| east 120/15 | 29.42 / 31.76 / 28.76 | 29.42 / 31.76 / 28.76 |
| east 240/15 | 48.94 / 33.16 / 53.37 | 48.94 / 33.16 / 53.37 |
| close 120/5 | 47.56 / 7.10 / 58.43 | 47.56 / 7.10 / 58.43 |
| full 120/5 | 39.71 / 4.89 / 56.11 | 39.71 / 4.89 / 56.11 |

### 1.7 THE CEILING TABLE

Percentage of **decided** pixels where the two panels disagree (`rows.json`,
`extra.json`, `extra2.json`; logs `rows.log`, `extra.log`, `extra2.log`).
"mean" is over all 16 camera x sun cells. Object rows keep the shipped terrain
sheet; terrain rows keep the shipped object bytes; so a row's column is its own
change and nothing else. Lower is closer to the ray cast.

| row | what changed | mean ALL | mean ter | mean obj | st 120/5 obj | st 240/15 obj | ea 120/15 obj | ea 240/15 obj | bytes on the region |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| **O1** | **the stored bytes (baseline)** | **39.99** | 20.23 | **48.77** | **51.47** | **79.11** | 28.76 | 53.37 | 0 (7,982,096 B today) |
| O2z | ceiling per vertex, rule KEPT | 31.07 | 20.23 | 35.88 | 47.07 | 68.61 | 15.77 | 46.13 | 0 |
| O2 | ceiling per vertex, rule GONE, no offset | 36.80 | 20.23 | 43.50 | 38.48 | 8.46 | 32.97 | 39.51 | 0 |
| O2zt | no offset, rule at a byte-safe -0.02 | 34.97 | 20.23 | 40.96 | 36.51 | 12.79 | 30.44 | 40.00 | 0 |
| O2p | +16 u along the normal, rule gone | 32.64 | 20.23 | 37.41 | 32.80 | 11.38 | 27.12 | 38.77 | 0 |
| **O2m** | **two-sided, the LOWER skyline, rule gone** | 31.46 | 20.23 | 35.50 | 32.19 | 13.11 | 26.28 | 36.94 | **0** |
| O2mz | two-sided + the shipped rule | 27.97 | 20.23 | 31.66 | 47.43 | 68.38 | 15.31 | 45.04 | 0 |
| **O2mt** | **two-sided + the rule at -0.02** | 29.88 | 20.23 | 33.36 | **30.52** | 17.26 | 24.27 | 37.46 | **0** |
| O5a | 32 azimuth bins | 36.08 | 20.23 | 42.60 | 36.33 | 14.95 | 31.81 | 38.77 | +7,982,096 B (+100%) |
| O5b | 64 azimuth bins | 36.16 | 20.23 | 42.97 | 37.77 | 23.06 | 30.49 | 38.61 | +23,946,288 B (+300%) |
| O5n | 16 bins, read NEAREST not blended | 36.49 | 20.23 | 43.22 | 37.12 | 14.87 | 30.42 | 40.08 | 0 |
| O5m | 16 bins, read as the pair MAX | 39.82 | 20.23 | 47.95 | 44.65 | 8.74 | 40.09 | 41.84 | 0 |
| O3a | + a vertex on every edge over 512 u | 37.78 | 20.23 | 44.84 | 38.31 | 8.49 | 33.44 | 40.12 | +1.94 MB (+24.2%) |
| O3b | + a vertex on every edge over 256 u | 37.53 | 20.23 | 44.43 | 36.47 | 8.44 | 31.90 | 40.38 | +7.81 MB (+97.5%) |
| **O4** | **a horizon per 64 u FACE TEXEL (tier 3)** | **10.63** | 20.23 | **5.53** | 17.38 | 7.35 | 5.66 | 7.02 | +21,835,024 B (+273%) |
| O4x | the same at 16 u texels | 10.50 | 20.23 | 5.36 | 17.05 | 7.55 | 5.30 | 6.77 | ~350 MB -- the instrument floor, not a proposal |
| **T1** | **the stored terrain sheet (baseline)** | 39.99 | **20.23** | 48.77 | -- | -- | -- | -- | 262,144 B a 4,096 u cell |
| T2a | ceiling sheet, 32 u a texel (shipped size) | 38.06 | 12.99 | 48.77 | -- | -- | -- | -- | same, 262,144 B |
| T2b | ceiling sheet, 64 u a texel | 38.08 | 13.06 | 48.77 | -- | -- | -- | -- | 65,536 B (-75%) |
| T3a | ceiling sheet, 32 bins | 37.70 | 11.22 | 48.77 | -- | -- | -- | -- | 524,288 B (+100%) |
| **T3b** | **ceiling sheet, 64 bins** | 36.02 | **7.35** | 48.77 | -- | -- | -- | -- | 1,048,576 B (+300%) |
| T4 | bin = MAX over its 22.5 deg sector | 39.03 | 16.71 | 48.77 | -- | -- | -- | -- | same |
| T5 | no spatial interpolation at read | 38.10 | 13.10 | 48.77 | -- | -- | -- | -- | same |
| T6a | ray starts AT the ground (rise 0) | 38.17 | 13.29 | 48.77 | -- | -- | -- | -- | same |
| T6c | ray starts 48 u up | 38.16 | 13.35 | 48.77 | -- | -- | -- | -- | same |
| **B1t** | **O2mt objects + T3b terrain** | **25.91** | 7.35 | 33.36 | 30.52 | 17.26 | 24.27 | 37.46 | +1,048,576 B a cell, 0 object bytes |
| **B2** | **O4 objects + T3b terrain** | **6.66** | 7.35 | 5.53 | 17.38 | 7.35 | 5.66 | 7.02 | +21.8 MB objects + 1,048,576 B a cell |
| M1 | runtime shadow map, 64 u, group identity | 11.41 | 12.25 | 11.25 | 11.99 | 37.52 | 10.95 | 19.13 | **0 file bytes**, 0.87 MB of runtime memory |
| M4 | the same with a true one-solid identity | 10.44 | 12.25 | 9.82 | 11.83 | 33.72 | 8.34 | 13.73 | 0 file bytes, 0.87 MB |

Byte model: object stream = A bytes a placement vertex over the region's 498,881
placement vertices (HORIZON3 s1: 16 bins x 498,881 = the 7,982,096 B the v8
stream costs today); O3 and O4 costs are HORIZON3 s1(a)/s1(b) verbatim. Terrain
sheet = 1 byte a bin a texel, quoted per 4,096-unit cell (128 x 128 texels at the
shipped 32 u); the shipped `.lodt` container adds its own mips and borders, by
the same factor for every T row, so the ratios hold even though the file bytes
are larger than the payload.

**T4, and which one the bake does today.** `lodgenHorizonCastAt` casts along the
bin's CENTRE direction (`lodgenHorizonBinDir`) and stores that one ray's
elevation. T4 replaces it with the maximum skyline anywhere in the bin's 22.5
degrees. It is **worse**: terrain 16.71% against 12.99% for the centre at the
same size and bin count, and on the street at 120/5 it is 41.98% either way --
no change at all, because at that sun the street's own walls dominate and the
terrain barely decides anything. So the centre convention is right and there is
nothing to repair in T4.

### 1.8 The ROT180 control -- the red proof

The RIGHT panel reads its own data 180 degrees away while the LEFT panel keeps
the real sun. A representation that carries real directional information must
get much worse. Mean over all 16 cells, and over the eight street/east cells the
pictures are made from (`extra.log`, `extra2.log`):

| row | objects, 16 cells | objects, the 8 street/east cells |
|---|---|---|
| **O1, the bake today** | 48.77% -> 48.51%, **moves -0.26 points** | **-3.43 points** |
| B1t (the shipping row) | 33.36% -> 49.52%, moves +16.16 | **+28.02 points** |
| B2 (tier 3) | 5.53% -> 34.24%, moves +28.70 | +39.4 points |

**Today's object horizons move the WRONG WAY.** On every street cell, rotating
the sun 180 degrees makes the stored bytes *agree better* with the ray cast:
-4.86, -10.30, -7.47 and -9.33 points. That is the plainest possible statement
that the stored object horizon stream carries no usable directional information
at all -- and s1.9 says why.

### 1.9 WHY: 60.68% of the stored object horizons are sixteen zero bytes

| measurement | value |
|---|---|
| `.lodo` vertices on chunk 4.4.-12 | 53,396 |
| vertices whose 16 stored bins are ALL zero | **32,403 (60.68%)** |
| vertices whose unpacked normal has `nz <= -1e-3` | 32,402 (60.68%) |
| the two sets differ by | **5 vertices out of 53,396** |

`src/lodghorizon.h` (`lodgenHorizonCastAt`): a face pointing DOWN
(`nz <= -1e-3`) stores 0 in all 16 bins. `src/nativeemit.cpp` line 2386 feeds
that test the normal it got from **`lodoUnpackOct12`** (line 2104) -- the normal
after it has been through the file's own quantiser. Break the down set up by how
far down it actually points:

| `nz` band | vertices | what it is |
|---|---:|---|
| below -0.05 | 9,358 | a genuinely down-facing face |
| -0.05 to -0.02 | 829 | a nearly-flat soffit |
| **-0.02 to -0.001** | **22,215 (68.56% of the down set, 41.60% of ALL vertices)** | **a VERTICAL WALL whose normal rounded the wrong side of zero** |

and inside that band **20,682 vertices sit in a single spike at nz = -0.0037**.
That is the signature of a byte-quantised normal: a FO4 NIF stores a vertex
normal as unsigned bytes, so a face that is exactly vertical comes back as
`127/255*2 - 1 = -0.00392`, four times below the `-1e-3` threshold.

**So the rule does not fire on down-facing faces. It fires on walls.** Four out
of every ten LOD vertices in the chunk -- and the great majority of the vertical
surfaces the camera sees down a street -- store sixteen zero bytes, which the
reader means as "nothing is above me", which paints them fully lit at every sun
in the sky. That is bungo's terrible picture, and it is one comparison in one
line of `src/lodghorizon.h`.

**The refuter.** If this were the whole story, simply removing the rule would
repair the picture. It does not: row O2 (rule gone, receiver left sitting on its
own surface) is *worse* on the mean than O2z (rule kept), 43.50% against 35.88%,
because the rule was also masking a second defect -- a receiver sitting on a wall
sees its own wall and reads a huge horizon. The two have to be repaired
together, which is what row O2mt does. **And a second refuter, against myself:**
O2mz -- the good offset with the *shipped* rule -- has the best mean objects
column in the whole vertex family, 31.66%. It wins the mean by blanking 60% of
the vertices to "lit", which is right wherever the truth is mostly lit. It is
also 68.38% wrong on the street at 240/15 and it is the row the ROT180 control
destroys. **The mean column alone would have picked the bug.** That is why the
street cells and the ROT180 control are in the gate and the mean is not.


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


## 3. The identity screenshots (deliverable A)

> *"send me screenshots of the chunks with our identity"* -- bungo, 04:3x.

`ident_pics.py`, log `ident_pics.log`, 13 pictures in `images/ident_*.png`.
Every picture is a PAIR over the same G-buffer, the same geometry and the same
shading, so the only difference between the panels is **which id decides the
colour**:

| | LEFT | RIGHT |
|---|---|---|
| `ident_<camera>.png` | one flat colour per **GROUP** -- our shadow identity | one flat colour per **PLACEMENT** -- every reference its own thing |
| `ident_east_layermerge.png`, `ident_full_layermerge.png` | the **PROPOSAL**: groups merged under a Bld/Building-named CK layer | the groups we ship today |

**How to read them.** Where the two panels look the same, the grouping did
nothing. Where the LEFT panel is **one colour spread over several buildings**,
the file has told the shadow map that all of them are one thing, so none of them
may shadow another. Where the LEFT panel is **many colours over one building**,
the opposite mistake: that building will shadow itself at the seams.

**What the pictures are not.** No shadows are drawn -- this is the identity, not
its consequence. Shading is flat identity colour times N.L at a plain high sun
(azimuth 120, elevation 35) purely so the shapes read. Terrain is flat grey
because it is its own identity by rule. The 149 tree-like placements are
desaturated to a green-grey so they cannot be mistaken for architecture; they
are still one colour each, and each tree is its own group.

**The caption every picture carries:** `588 groups | 2,449 placements |
468 singletons | largest group 205 placements`, plus the in-frame counts for
that camera.

### The files

| picture | what it shows |
|---|---|
| `ident_east.png` | the east camera, 146 groups / 719 placements in frame |
| `ident_street.png` | the street camera -- the one bungo judges by |
| `ident_close.png` | the low oblique from the south-west |
| `ident_full.png` | the whole chunk obliquely |
| `ident_wide.png` | **the wider view**: a million units up on a long lens, north-up, the whole chunk and its margin |
| `ident_east_layermerge.png`, `ident_full_layermerge.png` | the layer-merge PROPOSAL, labelled as a proposal in its own title strip |
| `ident_cu_DN135_GwinnettExt.png` | **the worst case**: 260 placements in ONE CK layer, shattered into **205** shadow identities |
| `ident_cu_AndrewStation.png` | 230 placements in one layer, **7** identities -- the opposite failure |
| `ident_cu_Theater47_Bld01.png` | the clean case: 56 placements, **one** layer, **one** identity |
| `ident_cu_SouthBostonBlock34.png` | 110 placements -> 95 identities |
| `ident_cu_SouthBostonBlock16.png` | 45 -> 33 |
| `ident_cu_SouthBostonBlock02.png` | 73 -> 21 |

Theater47_Bld01 IS on this chunk, so the clean case is the real one and not a
substitute. The last three are the chunk's own worst groups-vs-layer cases,
found by counting v7 groups inside each CK layer (`ident_pics.log`).

### What the close-ups say, in one line each

**DN135_GwinnettExt is the shattering case and AndrewStation is the fusing
case, and they are the same defect seen from two sides.** The v7 rule joins
architecture placements whose world boxes touch within 16 units. Gwinnett is a
long ruined exterior of many small separated pieces, so the rule finds 205
things where the artist meant one building; Andrew Station is a dense
interlocking block, so the rule finds 7 things where a street-level eye sees
many. Neither number comes from anything that knows what a building is -- and
that is the argument for a better identity, not against identity.

**The proposal panel is nearly a no-op here and the picture says so.** Only 3 of
this chunk's 30 layers are Bld-named, so 588 identities become 586 and the two
panels are all but identical. South Boston was layered by CITY BLOCK. That is
the picture's finding: the layer route would work in the Theater district and do
nothing here, which is why it is labelled a proposal in its own title strip and
is not ruled (numbers in s1.13).

## 4. The identity route against the ray cast (deliverable B)

`mpics.py`, log `mpics.log`, table `mpics.json`. The runtime far shadow map
keyed on the `.lodi` group identity, **64 u a texel**, terrain carried in the
SAME map and judged by depth alone. Six cells: the east and street cameras at
120/5, 120/15 and 240/15.

`images/row_M1_east_az120_el05.png`, `row_M1_street_az120_el05.png`,
`row_M1_east_az120_el15.png`, `row_M1_street_az120_el15.png`,
`row_M1_east_az240_el15.png`, `row_M1_street_az240_el15.png`.
LEFT is the ray-cast truth, RIGHT is the map; nothing else differs.

| cell | ALL | terrain | objects | **objects N.L>0** | of px | self-shadow refused | no data |
|---|---:|---:|---:|---:|---:|---:|---:|
| east 120/5 | 12.48% | 10.21% | 13.20% | **13.06%** | 605,890 | 8.93% | 8.42% |
| street 120/5 | 12.82% | 13.54% | 11.99% | **11.45%** | 452,455 | 1.08% | 0.36% |
| east 120/15 | 10.48% | 8.96% | 10.95% | **11.08%** | 611,075 | 20.74% | 9.05% |
| street 120/15 | 6.82% | 5.89% | 7.88% | **6.53%** | 453,433 | 5.88% | 0.25% |
| east 240/15 | 16.42% | 7.64% | 19.13% | **7.64%** | 206,868 | 28.17% | 8.82% |
| street 240/15 | 27.20% | 18.17% | 37.52% | **3.47%** | 70,507 | 30.82% | 0.22% |
| **mean** | **14.37%** | **10.74%** | **16.78%** | **8.87%** | | **15.94%** | |

Against the same six cells the bake that ships today (row O1/T1) is 39.99% on
the 16-cell mean with a 54.53% objects N.L>0 column (s1.7), so this is not a
close comparison.

**The two numbers bungo asked for by name.**

**(1) The share of truth-dark object pixels lost to self-shadow exclusion.** Of
the object pixels the ray cast calls dark, the identity rule hands this share
back to the light because the caster carried the receiver's own id:

| cell | truth-dark object px | refused | share |
|---|---:|---:|---:|
| east 120/5 | 245,042 | 21,893 | 8.93% |
| street 120/5 | 226,149 | 2,451 | **1.08%** |
| east 120/15 | 104,046 | 21,582 | 20.74% |
| street 120/15 | 66,103 | 3,884 | 5.88% |
| east 240/15 | 283,084 | 79,731 | 28.17% |
| street 240/15 | 435,377 | 134,167 | **30.82%** |

The 30.82% at street 240/15 is the whole cost of the identity rule in one
number, and the row beside it is why the cost is survivable: that same cell is
**3.47% wrong on the N.L > 0 pixels**, because self-shadow at a western sun
falls overwhelmingly on faces the sun is already behind, where N.L blacks the
pixel anyway. **The identity rule's damage is concentrated exactly where the eye
cannot see it.** That is an argument for the route, and it is also the thing to
re-test first if the shipped shader ever stops multiplying N.L in.

**(2) The count of groups that are not one thing.** From s1.4 and s1.5, on this
chunk: **588 groups over 2,449 placements, 468 of them singletons, the largest
holding 205 placements.** Of the 120 groups that hold more than one placement,
**50 are several physically separate solids** at weld tolerance 0. Welding in
world space at 8 units (row M4) turns the 588 groups into **1,508 solids**:
**234 groups hold more than one solid** (the worst holds 68), and **112 solids
span more than one group** (the worst spans 47). So the group is not one thing
in both directions at once, which is what the Gwinnett and Andrew Station
close-ups show with no numbers at all.

**And the ceiling on repairing it.** Row M4 -- the identity replaced by a
perfect one-welded-solid identity -- scores 10.44% mean ALL against M1's 11.41%.
**A perfect identity recovers 1.4 of the 4.1 points the identity rule costs;
the other two thirds is genuine self-shadowing that no identity scheme can hand
back.** Repairing the identity is worth doing and it is not where the picture
is won.

### What would have to be true for section 4 to be wrong

The map is rebuilt when the sun moves, over the LOD meshes of every loaded
chunk, and **this lane has not costed that draw** -- every number above is a
simulator's, taken on one chunk, with the truth cast's standing limits (s0)
attached. The "no data" column is receivers outside the map's (v, s) box and
runs to 9.05% on the east camera, which overshoots the chunk; those pixels are
excluded from every percentage rather than counted as agreement. And the texel
size barely matters (64 u 11.41% -> 16 u 10.92% on the 16-cell mean), so a
future disagreement about resolution is not a disagreement about this table.

## 5. src and tests

**src untouched by this lane.** `git status --porcelain src/ tests/` reports 216
entries, modified or untracked; all of them are pre-existing or lane HORIZON3's
uncommitted work, none is mine. The two files I read are untracked -- `??
src/lodghorizon.h`, `?? src/nativeemit.cpp` -- they arrived with HORIZON3 and
they leave this lane byte-identical. I opened them to read the three-way normal
rule and the octahedral normal unpack, and wrote nothing. `tests/spells/lodgen_sunsim.sh` does not exist and was not created --
the gate was dropped by the redirect. No build was attempted: Fallout 4 is up
(pid 17248). Everything in this lane is Python under
`scratchpad/horizon4_20260919/`.
