# Lane HORIZON3 report -- far shadows on objects (three tiers) + the scrappable bit

Written incrementally. Every number names the log it came from.

## 0. Exe at launch, rung, and the game state

Read by me at lane start (`date` = **Sat Sep 19 01:31:01 CEDT 2026**):

```
release/NifSkope.exe   2026-09-18 23:47:33.196854000 +0200   22,949,376 B
sha1 b349f807426be700ed2ff9b54ee23e4fab3ba037
```

That matches the brief's HORIZON2 final exactly (mtime, size, sha1). Rung taken before any
build, byte-identical copy:

```
release/NifSkope.before_horizon3.exe  2026-09-18 23:47:33.196854000 +0200  22,949,376 B
sha1 b349f807426be700ed2ff9b54ee23e4fab3ba037
```

### GAME UP -- this lane cannot build or run the exe

`tasklist | grep -i -E "Fallout4|NifSkope"` run as its own command at 01:31:01 returned:

```
Fallout4.exe                 11328 Console                    1  7,649,620 K
rc=0
```

Fallout4 is UP. No NifSkope process is running (no bungo window, no wedged harness).
Per the brief's Game rule this lane performs **no build and no exe run**: measurement,
source work, gate script, docs and the ledger text are done, and the lane parks with
`PENDING.md` headed `BUILD PENDING`. Everything that needs the compiler or a render run
(the build, G1..G5, all pictures) is listed in PENDING.md as the resume point.

## 1. The three measurement tables (step 1)

Script `scratchpad/horizon3_20260919/measure_tiers.py`, log
`scratchpad/horizon3_20260919/tiers_urban.log`, JSON `tiers_urban.json`.
Population: HORIZON1's urban region,
`scratchpad/viewfix_20260917/urban_ao/nat/FO4CSLOD/Commonwealth/Commonwealth.{lodo,lodi}`
(`.lodi` v6, 33,123 placements). It reads through `tests/spells/lodgen_native_decode.py`,
the byte-table decoder, and shares no code with the C++ writers.

**Control, and it is a reproduction of numbers another lane printed, not a
self-check.** The walk must reproduce HORIZON1 s1a on the same pair:

| | HORIZON1 s1a | this lane |
|---|---|---|
| `instanceCount` header / walked / slot 0 | 33,123 | **33,123 / 33,123 / 33,123** |
| drawn triangles / edges | 250,320 / 750,960 | **250,320 / 750,960** |
| edge p50 / p90 / p99 / max (u) | 239.7 / 512.0 / 1,158.7 / 5,476.1 | **239.7 / 512.0 / 1,158.7 / 5,476.1** |
| edges over 512 / 1,024 / 2,048 | 71,465 / 15,022 / 1,085 | **71,465 / 15,022 / 1,085** |
| triangles with an edge over 512 / 1,024 | 41,042 / 7,926 | **41,042 / 7,926** |

Every figure identical. The decoder is the one HORIZON1 used, so this is a
reproduction, not an independent second opinion -- it pins that this lane is
reading the same population, and nothing more.

### 1(a) Tier 2: long-edge subdivision, per candidate threshold

**The scale question the brief's arithmetic does not settle, answered before the
table.** The geometry lives ONCE in the `.lodo` library and is instanced; the
per-vertex shading streams (AO v6, sky v7, horizon v8) live PER PLACEMENT in the
`.lodi`. A world-unit edge threshold is therefore ambiguous, because one library
edge is a different number of world units under every placement's own scale
(measured range on this population 0.260 .. 2.000). Two rules were measured:

* **RULE A -- what tier 2 implements.** One subdivided library mesh per mesh, cut
  at the mesh-space threshold `t = T / max(scale over that mesh's drawn
  placements)`. Every placement of that mesh then satisfies the world-unit bound,
  and the smaller copies carry more vertices than they need.
* **RULE B -- the floor, and NOT implementable** without per-placement geometry
  (which would end instancing). Each placement cut at its own scale. The gap
  between the two columns is exactly the price of sharing one library mesh.

**Three vertex counts, and they are three different things.** `welded` is the
number of genuinely new points, which is what a T-junction refuter must find
welded. `library stored` is how many `.lodo` vertex ROWS that becomes, because
clusters duplicate the vertices on their own seams and an inserted vertex on a
cluster-boundary edge is stored in both. `placement` is the one that sizes the
`.lodi`: the per-vertex streams are one slice a drawn vertex a placement.

Triangles added is exact for ANY triangulation of the refined boundary: a polygon
with V boundary vertices and no interior vertex triangulates to V - 2 triangles,
so a triangle whose three edges gain `n0 + n1 + n2` points gains exactly
`n0 + n1 + n2` triangles, whatever the fan is anchored on.

| T (u) | edges over T | triangles with an edge over T | welded new pts | library stored verts | library tris added | clusters added | placement verts added | placement tris added | RULE B placement verts |
|---|---|---|---|---|---|---|---|---|---|
| **256** | 279,527 | 155,197 | 43,376 | 61,993 | 80,535 | 5,493 | **359,338** | 461,929 | 313,928 |
| **384** | 124,825 | 62,956 | 20,573 | 29,699 | 38,479 | 2,839 | **150,076** | 190,353 | 129,525 |
| **512** | 71,465 | 41,042 | 12,976 | 18,754 | 24,496 | 1,894 | **89,603** | 115,379 | 72,699 |
| **768** | 23,460 | 12,351 | 5,157 | 7,565 | 9,919 | 845 | **24,899** | 31,040 | 22,129 |
| **1,024** | 15,022 | 7,926 | 2,994 | 4,296 | 5,788 | 512 | **15,066** | 18,737 | 13,405 |

Bytes, on the same region. `.lodo` = 16 B a library vertex (NATIVE 3.1) + 64 B a
new cluster (a 16 B cluster entry plus its fixed 48 B local-index blob, because
the `.lodo` caps a cluster at 48 vertices and 16 triangles and its local indices
are u8, so a refined cluster must split). `.lodi` = 18 B a placement vertex
(1 AO + 1 sky + 16 horizon bins at the shipped `--horizon-azimuths 16`).

| T (u) | `.lodo` added | `.lodi` added | total added | against the v8 horizon stream (7,982,096 B) | drawn triangles after |
|---|---|---|---|---|---|
| **256** | 1,343,440 B | 6,468,084 B | **7.81 MB** | +97.5 % | 712,249 (+184 %) |
| **384** | 656,880 B | 2,701,368 B | **3.20 MB** | +40.0 % | 440,673 (+76 %) |
| **512** | 421,280 B | 1,612,854 B | **1.94 MB** | +24.2 % | 365,699 (+46 %) |
| **768** | 175,120 B | 448,182 B | **0.59 MB** | +7.4 % | 281,360 (+12 %) |
| **1,024** | 101,504 B | 271,188 B | **0.36 MB** | +4.5 % | 269,057 (+7 %) |

Projected to the Commonwealth at HORIZON1 s1c's live-REFR ratio (x2.98):
256 -> ~23 MB, 384 -> ~9.5 MB, 512 -> ~5.8 MB, 768 -> ~1.8 MB, 1,024 -> ~1.1 MB,
against the ~23 MB the v8 object stream already costs.

**The one thing this table does NOT say.** It does not say a shadow looks better
at 256 than at 512. It says what each bar costs and how many triangles stop
smearing a shadow edge over more than that bar. The look is 1(c) and the
pictures, and both of those are owed with the build.

### 1(b) Tier 3: the per-face horizon texture, per candidate area threshold

**The face rule, printed as the brief asks.** A FACE is a maximal set of level-0
triangles of ONE mesh that are

1. edge-connected through **welded** edges -- vertices quantised to 0.01 mesh
   units, so the `.lodo`'s own cluster seams (which duplicate the vertices on a
   cluster boundary) do not cut a face in two; and
2. **coplanar**: every member triangle's normal within **1.0 degree** of the
   seed triangle's, and every member vertex within **0.5 mesh units** of the seed
   triangle's plane.

Its area is the sum of its triangles' areas at the placement's own scale
(`area x scale^2`), and its texture rectangle is the bounding rectangle of its
vertices in an arbitrary orthonormal frame of its own plane (`du x dv` at scale).
Texels are `ceil(du/texel) x ceil(dv/texel)`, at least 1x1; bytes are 16 a texel,
the 16 bins packed as four RGBA as the brief specifies.

The face population of the region: **135,966 faces over 33,123 placements**
(4.1 a placement), area p50 **15,392 u^2**, p90 98,982, p99 360,498, max
**8,015,247**; **53,857 (39.6 %) are a single triangle**.

| area bar | side | faces over it | texels at 64 u | bytes at 64 u | texels at 128 u | bytes at 128 u |
|---|---|---|---|---|---|---|
| **65,536 u^2** | 256 u | 22,733 (16.7 %) | 1,364,689 | **21,835,024 B (20.8 MB)** | 388,829 | 6,221,264 B (5.9 MB) |
| **262,144 u^2** | 512 u | 3,188 (2.3 %) | 519,142 | **8,306,272 B (7.9 MB)** | 141,628 | 2,266,048 B (2.2 MB) |
| **1,048,576 u^2** | 1,024 u | 117 (0.09 %) | 92,355 | **1,477,680 B (1.4 MB)** | 23,823 | 381,168 B (0.4 MB)|

For scale: the whole v8 object horizon stream on this region is 7,982,096 B. A
64-u face sheet at the 256-u bar costs **2.7 times the entire existing stream**;
at the 512-u bar it costs roughly the same again as the stream; only the 1,024-u
bar is cheap.

**The twenty largest faces, and what they are.** This decides what 1(c) is
actually asking about, so it goes in the report rather than only in the JSON.
One placement per row and at most three faces from any one model, so the count
is not decided by one repeated mesh:

| face area (u^2) | tri | du x dv (u) | normal | model |
|---|---|---|---|---|
| 8,015,247 | 10 | 5,340 x 3,272 | (0, 0, +1) | `…Roads\HighwayOverpass\HWDoubleCurveL01_LOD_0.nif` |
| 7,882,864 | 9 | 5,340 x 3,263 | (0, 0, -1) | `HWDoubleCurveL01_LOD_0.nif` |
| 7,690,949 | 11 | 5,276 x 2,998 | (0, 0, +1) | `HWDoubleCurveL01_LOD_0.nif` |
| 7,689,272 | 11 | 5,168 x 3,091 | (0, 0, +1) | `HWDoubleCurveR01_LOD_0.nif` |
| 6,205,479 | 4 | 4,096 x 1,515 | (0, 0, +1) | `HWDoubleStrExit01_LOD_0.nif` |
| 6,057,987 | 4 | 4,096 x 1,479 | (0, 0, +1) | `HWDoubleStrExit01_LOD_0.nif` |
| 6,021,285 | 4 | 4,096 x 1,470 | (0, 0, -1) | `HWDoubleStrExit01_LOD_0.nif` |
| 5,304,257 | 2 | 2,560 x 2,072 | (0, 0, +1) | `…Airport\AirportTerminalDestroyed01_LOD.nif` |
| 5,035,600 | 5 | 3,764 x 2,796 | (0, 0, -1) | `HWDoubleCurveR01_LOD_0.nif` |
| 5,003,382 | 6 | 3,767 x 2,802 | (0, 0, +1) | `HWDoubleCurveR01_LOD_0.nif` |
| 3,762,037 | 75 | 2,104 x 3,197 | (0, 0, +1) | `…BostonCommons\BostonCommonsPond01_LOD_0.nif` |
| 3,102,701 | 4 | 2,048 x 1,515 | (0, 0, +1) | `HWDoubleStr01Damaged01_LOD_0.nif` |
| 3,102,663 | 2 | 2,048 x 1,515 | (0, 0, +1) | `HWDoubleStr01_LOD_0.nif` |
| 3,028,918 | 2 | 2,048 x 1,479 | (0, 0, +1) | `HWDoubleStr01_LOD_0.nif` |
| 3,010,614 | 2 | 2,048 x 1,470 | (0, 0, -1) | `HWDoubleStr01_LOD_0.nif` |
| 2,487,817 | 4 | 1,248 x 1,996 | (-1, 0, 0) | `LOD\Buildings\BldgBrick7Story3x5FreeComEntA_LOD.nif` |
| 2,301,463 | 2 | 2,320 x 992 | (0, +1, 0) | `…TheCastle\CastleWallOutWedge02_LOD_0.nif` |
| 2,301,463 | 2 | 2,320 x 992 | (0, +1, 0) | `CastleWallOutWedge03_LOD_0.nif` |
| 2,207,164 | 7 | 2,320 x 992 | (0, +1, 0) | `CastleWallOutWedge01_LOD_0.nif` |
| 2,097,127 | 2 | 2,114 x 992 | (0, -1, 0) | `CastleWallOutWedge02_LOD_0.nif` |

**Sixteen of the twenty are horizontal** (normal +/-Z): highway overpass decks,
road surfaces, an airport roof, the Boston Commons pond. Only four are vertical
walls, and three of those are the same Castle wall model. That shape matters for
1(c): a horizontal deck is the surface most likely to carry a shadow whose
boundary never reaches its own rim, because a tower's shadow can cross the middle
of a 5,340 x 3,272-unit deck without touching either end.

### 1(c) the shadow-interior case -- does a shadow ever land in the middle of a face?

Instrument: `scratchpad/horizon3_20260919/interior_shadow.py`. It samples a face
on a 64-unit lattice in the face's own plane, keeps the samples inside the face's
triangles, and asks at each one whether the sun is visible. Visibility is
HORIZON2's third witness: the raw BTD heightmap through `wit.Land` plus the
33,123 placements as exact world AABBs from `lodgen_native_decode.py`. It shares
no code with the march. A sample is LIT when the sun elevation is above the
skyline elevation the witness returns at that sample's azimuth.

Then it 8-connects the lit and the dark samples into components, marks every
sample that sits on the face's rim (no 4-neighbour inside the face) as a boundary
sample, and counts a component as INTERIOR when none of its samples is a rim
sample and none of its samples is 4-adjacent to a rim sample. That is the literal
reading of the brief's "a lit/dark boundary that touches NO edge of the face".

Two controls, both in `interior_urban.log`:

| control | what it would catch | result |
| --- | --- | --- |
| cast control: my visibility test vs `wit.true_skyline` over 50 casts | my vectorised transcription drifting from the witness | worst **0.0000 deg** |
| detector known-answer control (`--control-synth`) | the component/rim logic calling everything interior, or nothing | **GREEN** |

The known-answer control builds a 4,096-unit deck and one tower, and runs the
detector twice: tower at the RIM (the shadow stripe reaches the edge, expect
interior = NO) and tower in the MIDDLE at 500 units tall (866 units of shadow on
a 4,096-unit deck, ending 926 units short of the rim, expect interior = YES).
Both come back as expected. The middle case had to be re-tuned once -- see
section 12 -- which is itself the evidence the control is load-bearing.

#### The first answer was wrong, and the wider sweep is the one that counts

The brief asks for the 20 largest faces at elevations 5/15/30. I ran that first,
at azimuths 120 and 240, log `interior_urban.log`:

> `=== 0 of 20 faces carry a lit/dark boundary that touches NO edge, at some sun; 0 with a component of 3+ samples ===`

I did not trust a zero that convenient, so I widened the sweep to the 60 largest
faces and five azimuths 60/120/180/240/300 -- `interior_shadow_wide.py`, which
`diff` shows is the same file with one line changed:

```
55c55
< SUN_AZIM = [120.0, 240.0]
---
> SUN_AZIM = [60.0, 120.0, 180.0, 240.0, 300.0]
```

Log `interior_urban60.log`:

> `=== 8 of 60 faces carry a lit/dark boundary that touches NO edge, at some sun; 8 with a component of 3+ samples ===`

The two runs do not disagree. Every one of the 16 (face, sun) hits is either at
an azimuth the narrow run never tried, or on a face outside the narrow run's top
20; the overlap of the two domains contains no hit in either run. **The narrow
run was under-sampled, in azimuth and in face count, and its zero is an artifact
of my sampling, not a property of the population.** The number that stands is
8 of 60.

#### The 8 faces

| # | model | face area u2 | samples | largest interior component | as a share of the face |
| --- | --- | ---: | ---: | ---: | ---: |
| 1 | `HWDoubleCurveL01_LOD_0.nif` | 8,015,247 | 1,999 | 20 samples | 1.0 % |
| 5 | `HWDoubleStrExit01_LOD_0.nif` | 6,205,479 | 1,560 | 10 samples | 0.6 % |
| 6 | `HWDoubleStrExit01_LOD_0.nif` | 6,057,987 | 1,560 | 10 samples | 0.6 % |
| 7 | `HWDoubleStrExit01_LOD_0.nif` | 6,021,285 | 1,495 | 10 samples | 0.7 % |
| 11 | `BostonCommonsPond01_LOD_0.nif` | 3,762,037 | 948 | 24 samples | 2.5 % |
| 12 | `HWDoubleStr01Damaged01_LOD_0.nif` | 3,102,701 | 792 | 15 samples | 1.9 % |
| 48 | `HWDoubleEndCapL03_LOD_0.nif` | 1,519,304 | 408 | 74 samples | **18.1 %** |
| 52 | `HWOnRampCurveFree01_LOD_0.nif` | 1,282,371 | 332 | 22 samples | 6.6 % |

A sample is 64 x 64 units, so a 74-sample component is 303,104 square units of
deck -- a 550-unit blob of shade in the middle of an off-ramp that tier 1 cannot
represent at all.

Where the hits sit, over all 60 x 15 = 900 (face, sun) pairs, of which 16 hit:

| by sun azimuth | hits | | by sun elevation | hits |
| --- | ---: | --- | --- | ---: |
| 60 | 0 | | 5 | 2 |
| 120 | 1 | | 15 | 6 |
| 180 | 3 | | 30 | 8 |
| 240 | 3 | | | |
| 300 | 9 | | | |

**All 8 are horizontal** (normal +/-Z) and **7 of the 8 are highway overpass
pieces**; the eighth is the Boston Commons pond. Not one vertical wall in the 60
carries an interior boundary at any sun. The mechanism is the one section 1(b)
predicted: an occluder is almost always wider than the wall it shades, so the
shadow runs off the lateral rim; a deck held up in the air above everything
around it is the one shape whose shade can land in its middle without touching a
rim, and that is exactly what an overpass is.

#### What the instrument does not prove

The witness's own loader control reports `shared-edge control 16052 mismatches
worst 13056.0 u` on `land.bin` -- adjacent BTD cells store their shared edge row
twice and the two copies differ. That is HORIZON2's instrument as inherited, not
something this lane changed, and the 0.0000-degree cast control only proves my
transcription matches the witness, not that the witness's terrain is right near a
cell seam. All 8 hits are placement-cast (a box shading a deck), not
terrain-cast, so a seam error would have to invent an occluder to create one; but
the honest statement is that this measurement is only as good as HORIZON2's
terrain witness, and its terrain half carries a known 13,056-unit seam
disagreement.

## 3. Tier 3 -- the per-face horizon sheet

**The brief's escape hatch does not open.** It says to write "tier 3 not needed
on this population, and the number that says so" if 1(c) comes back at about
zero. It came back at 8 of 60, and the biggest of them is 18.1 % of an off-ramp
deck, so I am not entitled to that section and I am not writing it. Tier 3 has a
measured, named need on this population. It is a small and very concentrated
need: 8 faces, 7 of them highway overpass pieces, all 8 horizontal, 16 of 900
(face, sun) pairs.

Nothing in this section is built or run. `Fallout4.exe` pid 11328 was up at
01:31:01 and stayed up, so this lane cannot compile (section 0). What follows is
the design and the cost, which is what bungo needs to rule on the knob anyway,
and the implementation is named in `PENDING.md` as owed.

### Cost, from `tier3_cost.log`

A sheet is the face's planar bounding rectangle at T units a texel, 16 azimuth
bins stored as four RGBA texels, so 16 bytes a sample position:
`(ceil(du/T)+1) x (ceil(dv/T)+1) x 16`.

| what you buy | faces | bytes at 64 u | bytes at 128 u | vs the 7,982,096 B v8 stream |
| --- | ---: | ---: | ---: | ---: |
| every face over 1,024^2 u^2 | 117 | 1,477,680 | 381,168 | +18.5 % / +4.8 % |
| every face over 512^2 u^2 | 3,188 | 8,306,272 | 2,266,048 | +104 % / +28 % |
| every face over 256^2 u^2 | 22,733 | 21,835,024 | 6,221,264 | +274 % / +78 % |
| **only the 8 faces 1(c) names** | 8 | **205,056** | 54,624 | **+2.6 % / +0.7 %** |

Every one of the 8 measured faces is over 1,024^2 u^2, so the **1,024^2 bar is
the smallest of the brief's three that covers the whole measured need**. It costs
1.41 MB at 64 u/texel and carries 109 faces that did not need it.

### The cheaper-looking alternative, priced, so it can be ruled out on numbers

Tier 2 cannot fix this. Edge subdivision only ever puts new vertices ON the
boundary of an existing triangle, so however fine the threshold, the shading
inside a big triangle stays a linear blend of its rim -- an interior-only dark
blob is unrepresentable by tier 2 at any threshold. The nearest thing to tier 3
that needs no new stream is to insert INTERIOR vertices on a lattice over the big
faces and retriangulate ("tier 2b"). For the same 8 faces at the same 64 units:

| | bytes | triangles added |
| --- | ---: | ---: |
| tier 3 sheet, the 8 faces at 64 u | 205,056 | 0 |
| tier 2b interior vertices, the 8 faces at 64 u | 309,196 | ~18,188 |

Interior vertices cost **1.5x the bytes and 18,188 triangles** (+7.3 % of the
region's 250,320 drawn triangles) to buy the same 8 faces. The texture wins on
both axes. The only thing tier 2b has going for it is that it needs no new
stream, no new `.lodi` section and no fragment-shader path -- and that is a real
advantage while the build is blocked, so it is on the table in section 11 and not
decided here.

### Format, if bungo rules the knob on

`--horizon-face-sheet <area u^2>`, default **0 = off**, per the standing rule
that every new master ships off and an owed ruling never ships as a default.

- New `.lodo` table `FACE`: one record a face -- `u16 meshId`, `u16 faceIndex`,
  `f32 origin[3]`, `f32 axisU[3]`, `f32 axisV[3]` (unit, in mesh space),
  `f32 du`, `f32 dv`, `u16 texW`, `u16 texH`, `u32 sheetOffset` = 48 bytes.
- New `.lodi` stream `HFACE`, role 8: `texW * texH * 16` bytes a face, four RGBA
  texels a sample position, bin order and byte encoding exactly tier 1's
  (`byte = round(elevation_deg / 90 * 255)`, bin 0 = north, clockwise to east).
- A `.lodo` library vertex gains `u16 faceId` (0xFFFF = none). That is the
  version bump; a v7-era reader refuses by name, and at knob 0 no `FACE` record
  and no `HFACE` stream is written, so the file is byte-identical to today's.
- The viewer samples `HFACE` in the fragment when the interpolated `faceId` is
  not 0xFFFF, at `(dot(p - origin, axisU) / du, dot(p - origin, axisV) / dv)`,
  and falls back to the per-vertex horizon when it is.
- Census words: `horizonFaceSheets`, `horizonFaceSheetBytes`.

The sampling rule is the one line a consumer needs and it goes in
`docs/FO4CS_IMPROVED_LOD_PLAN.md` s9 -- see section 9.

## 2. Tier 2 -- vertices inserted along long edges

Not built and not run: `Fallout4.exe` pid 11328 was up throughout (section 0), so
this lane compiles nothing. This section is the design, the format and the cost;
`PENDING.md` carries it forward as the first owed item.

### The knob

`--horizon-subdivide <u>`, default **0 = off** (tier 1 only), per the standing
rule that every new master ships off and an owed ruling never ships as a default.
At 0 the bake writes today's bytes and the way-back is byte-identical.

### What it does, and the one thing that decides its shape

Before the horizon march, every `.lodo` level-0 edge longer than `<u>` gets
`n = ceil(len / u) - 1` vertices inserted at even spacing along it. Both
triangles sharing the edge are retriangulated, a shared edge is split once, and
the split is welded, so no T-junction is created -- the refuter counts
T-junctions and must read 0 (G2).

A triangle with `n0 + n1 + n2` points added on its three edges triangulates to
`(3 + n0 + n1 + n2) - 2` triangles, that is exactly `n0 + n1 + n2` more than the
one it replaces, whatever vertex the fan is anchored on. That identity is why the
triangle cost in 1(a) is a sum and not an estimate.

**The scale ambiguity is the real design decision.** Geometry lives once in the
`.lodo` library; the per-vertex shading streams live per placement in the
`.lodi`. A mesh drawn at scale 0.5 and at scale 2.0 has, in world units, edges
four times apart -- but there is only one copy of the mesh to cut. Two rules:

- **Rule A (implementable, and what the design uses):** cut the library mesh once
  at `t = T / max(scale over all placements of that mesh)`. Every placement then
  has all its long edges cut, and the small-scale placements get cut finer than
  they need.
- **Rule B (the floor, not implementable without duplicating meshes):** cut each
  placement at its own scale. This is the lower bound on cost, and 1(a) prints it
  so the waste in Rule A is visible rather than assumed.

Section 1(a) costs both. At `T = 512` Rule A is the table's headline: 12,976
welded new points, 18,754 stored library vertices, 24,496 library triangles,
1,894 new clusters, 89,603 placement vertices and 115,379 placement triangles ->
421,280 B of `.lodo` and 1,612,854 B of `.lodi`, 1.94 MB, +24.2 % of the existing
7,982,096-byte v8 stream, and drawn triangles 250,320 -> 365,699 (+46 %).

A `.lodo` cluster caps at 48 vertices and 16 triangles with u8 local indices and
a fixed 48-byte local-index blob, so a cluster that grows past either cap must
SPLIT, +64 bytes each; the 1,894 new clusters in that row are split clusters, not
an allowance.

### Format

Provenance for each line is the source it is read from today:

| where | today | after tier 2 |
| --- | --- | --- |
| `src/lodofile.h:78` `LODO_VERSION` | 4 | **5** |
| `src/lodofile.cpp:1773` version check | `!= LODO_VERSION` -> refuse | unchanged; a v4 reader refuses a v5 file BY NAME, printing "version 5; this reader knows 4" |
| library vertex, `docs/LODGEN_NATIVE_LODO_LODI.md` s3.1 | 16 B, stride 16, `LODO_FLAG_VERTEX_V1` | unchanged -- inserted vertices are ordinary library vertices |
| cluster entry + local indices, s3.2 | 16 B + 48 B | unchanged; refined clusters split |
| header census | -- | two new words `horizonSubdivTriangles`, `horizonSubdivVertices` in the v4 header's reserved tail |

Nothing about the vertex row changes, which is the point: a consumer reading tier
2 reads **nothing new -- there are simply more vertices**. That sentence is the
whole of the tier-2 half of the s9 contract (section 9).

The version bump is needed anyway, and it is needed for an honest reason: at
`--horizon-subdivide 512` the mesh vertex counts in the library no longer match
what a v4 consumer computed from the same source NIFs, so a v4 reader that
assumed it could re-derive them would be silently wrong. The doc's rule is that a
reinterpretation refuses by name rather than version-checks, and this is a growth,
so a plain version check is the right instrument -- but it still has to happen.

### Inserted vertices are shaded, not interpolated

Position, normal, UV, AO and sky on an inserted vertex are interpolated along the
edge from its two ends. Its **horizon is not**: each inserted vertex gets its own
16-bin march, because interpolating the horizon is exactly the artifact bungo
objected to. G3 checks 20 sampled inserted vertices against HORIZON2's third
witness within 2 degrees, with a rotated-bin control that must come back red.

## 4. The scrappable bit

Instrument: `scratchpad/horizon3_20260919/scrap_rule.py`, log `scrap_rule.log`,
with two earlier survey logs it is built on, `scrap_survey.log` and
`scrap_rule_stage2.log` / `scrap_rule_stage3.log`. Every keyword, record and
formID below was read out of `Fallout4.esm` in those runs and printed with its
own EDID; none of it is remembered. The xEdit definitions
(`wbDefinitionsFO4.pas`, cached) were used only to read the `XPRM` and `XLKR`
field layouts, quoted below.

### The rule

> A placement is **workshop-scrappable** when all three hold:
>
> 1. its base form is reachable from the `CNAM` (Created Object) of a `COBJ`
>    whose `FNAM` category array contains `KYWD 00106D8F
>    WorkshopRecipeFilterScrap`, following `FLST` members (`LNAM`) transitively;
> 2. its `REFR` position lies inside at least one `XPRM` primitive of Type 1
>    (Box) carried by a `REFR` that links to a workshop workbench `REFR` with
>    `KYWD 000B91E6 WorkshopLinkedPrimitive` -- that box IS the settlement build
>    area, and it is rotated by its own `REFR` `DATA` rotation;
> 3. its base does NOT carry `KYWD 001CC46A UnscrappableObject`.

The three findings that shaped it, each one a place I would have got it wrong
from memory:

- **`CNAM` is usually not a base.** 87 of the 157 scrap recipes point their
  `CNAM` at a `FLST`, not at an object, so a recipe names a whole family at once.
  Taking `CNAM` at face value gives 157 bases and **0 scrappable placements on
  the region**; expanding the form lists transitively gives **1,012 bases** and
  705. The first run of this script printed that zero, and the zero is what
  exposed the indirection.
- **The build area is not on the workshop and not on its centre.** The workshop
  `REFR` carries no `XPRM`; its `WorkshopLinkCenter 00038C0B` link points at an
  `XMarkerHeading` that carries no `XPRM` either. The boxes are on 111 separate
  refs (`DefaultDummy`, `DefaultEmptyTrigger`, `DefaultDisableSelfTrigger`) that
  link *to* the workshop with `WorkshopLinkedPrimitive 000B91E6`. That is a
  reverse link, and only a reverse-link scan finds it.
- **Clause 3 is presently a no-op and stays in the rule anyway.** 149 bases carry
  `UnscrappableObject`; exactly 2 of them are also reachable through clause 1, so
  it removes 2 bases from 1,012 on this plugin. It is in the rule so a DLC or a
  mod that sets the keyword is honoured rather than silently ignored.

### The count, on the 33,123-placement urban region

| | placements | of 33,123 |
| --- | ---: | ---: |
| clause 1 alone -- the base has a scrap recipe | 705 | 2.13 % |
| clause 2 alone -- inside a workshop build area | 26 | 0.08 % |
| **the rule (1 and 2 and 3), `scrappablePlacements`** | **14** | **0.04 %** |

Fourteen is small, and the reason is worth stating so nobody reads it as a bug:
**this region is a corner of the Commonwealth, not the Commonwealth.** Its
placements span x -70..49,065 and y -49,191..45, and only **5 of the 111 build
areas in the plugin overlap it at all** (`scrap_rule_control.log`). The
containment test is not what limits the count: switching the z test off changes
26 to 26, and switching the box rotation off changes it to 25.

The four build areas that actually contain region placements, by first match:
`001B31E6` (6), `00179614` (10), `00179615` (6), `00179613` (4).

### Five each way

Scrappable:

| REFR | base | at | build area | by recipe |
| --- | --- | --- | --- | --- |
| `001BED1A` | `00035871 TreeCluster03` | 44150 -3541 460 | `001B31E6` | `00054C86 workshop_co_ScrapTreeLarge` |
| `001BED1A` | `00035871 TreeCluster03` | 44133 -3496 372 | `001B31E6` | `00054C86 workshop_co_ScrapTreeLarge` |
| `001BED1A` | `00035871 TreeCluster03` | 44149 -3534 250 | `001B31E6` | `00054C86 workshop_co_ScrapTreeLarge` |
| `001BED1A` | `00035871 TreeCluster03` | 44156 -3390 285 | `001B31E6` | `00054C86 workshop_co_ScrapTreeLarge` |
| `00170152` | `000358D1 TreeCluster02` | 44502 -4289 255 | `001B31E6` | `00054C86 workshop_co_ScrapTreeLarge` |

One `REFR` appearing four times is not a duplicate: an SCOL is split into parts
and each part is its own placement with its own `scolPart`, which is exactly why
the bit is **per placement** and not per reference.

Not scrappable, five per failing clause:

| REFR | base | at | fails |
| --- | --- | --- | --- |
| `000BBD6F` | `00033795 ShackBalconyFloor04` | 2375 -168 1802 | no scrap recipe for the base |
| `000BC126` | `000349F5 ShackBridgeFree01Support01` | 650 -389 1166 | no scrap recipe for the base |
| `000BC1CD` | `000349F5 ShackBridgeFree01Support01` | 769 -274 1146 | no scrap recipe for the base |
| `000BBD93` | `00034730 ShackFreeColumnInt01` | 2423 -238 1787 | no scrap recipe for the base |
| `000BBF4E` | `00034730 ShackFreeColumnInt01` | 2442 -115 1789 | no scrap recipe for the base |
| `000E472B` | `0005E20C TreeClusterDead02` | 40994 45 325 | outside every build area |
| `00098005` | `000503B6 TreeMapleblasted02` | 1695 -3039 450 | outside every build area |
| `0009800E` | `000503B6 TreeMapleblasted02` | 4029 -1352 375 | outside every build area |
| `0009800D` | `0003589F TreeCluster07` | 3889 -1072 467 | outside every build area |
| `0009800D` | `0003589F TreeCluster07` | 3899 -896 326 | outside every build area |

The shack pieces are the honest surprise: player-built shack walls have no scrap
`COBJ` in `Fallout4.esm`, because in the vanilla game they are scrapped as
workshop-built items through `WorkshopItemKeyword 00054BA6`, not through a
recipe. They are also not in the far map as settlement junk to begin with.

### Where the bit lives -- a divergence from the brief

The brief says to store it "as one bit in the cold record's flags". **The cold
record has no flags word.** `docs/LODGEN_NATIVE_LODO_LODI.md` s4.1a gives it as 8
bytes exactly: `u32 refFormId`, `i16 scolPart`, `u16 identity`, and the identity
word is load-bearing (a zero identity without `NOLIB` is a refusal). There is no
spare bit in it and growing it to 12 bytes would move every cold record for one
bit.

The hot record does have one. `src/lodifile.h:326` is `quint16 flags; //!<
LodiInstanceFlags; bits 6-15 reserved 0`, with bits 0-5 taken (mirrored,
force-card, alpha-tested, emits, SCOL part, buried-cull candidate). So:

- **`LODI_INST_SCRAPPABLE = 0x0040`, bit 6 of the instance record's `flags` word
  at offset 0x14**, and the reserved-bit refusal mask narrows from bits 6-15 to
  bits 7-15.
- That change is not backward compatible in the silent direction and must not be:
  a v8 reader meets bit 6 set, sees a reserved bit, and **refuses**. So the bit
  ships behind `LODI_VERSION_SCRAPPABLE = 9` (`src/lodifile.h:222` has
  `LODI_VERSION_HORIZON = 8` today), and `src/lodifile.cpp:1038`'s accept list
  gains 9.
- Census word `scrappablePlacements`; on this region it must read 14.
- Viewer channel `scrappable`: magenta = yes, grey = no, with the note line
  "scrappable: base has a workshop scrap recipe and the placement is inside a
  build area (14 of 33,123 here)".

The bit is derived data, not a knob, so it is always written from v9 on. That is
why G1 is specified as "byte-identical **except** the flag bit and the census
words" rather than plain byte-identical -- the brief already allows for exactly
this diff, and section 5 lists it.

## 5. The gate, and the refuters shown red

`tests/spells/lodgen_horizon3.sh` is written, 204 lines, **and has never been
run** -- it carries that as a banner in its own header, because a gate file
sitting in `tests/spells/` that nobody has executed is a lie waiting to be told.
`bash -n` passes. It is PRE-REGISTERED: G2's three expected counts
(12,976 / 18,754 / 24,496) and G1's expected `scrappablePlacements` = 14 are
written into the script as constants derived from `measure_tiers.py` and
`scrap_rule.py` **before** any implementation exists, which is the only moment a
floor means anything. The script detects an exe without `--horizon-subdivide`
and exits 2 with "NOT BUILT" rather than printing five failures that read like
regressions.

| gate | what it asserts | state |
| --- | --- | --- |
| G1 | `.lodo` byte-identical at knobs 0; `.lodi` differs ONLY in the version word, the scrappable bit and the census words, every run listed; `scrappablePlacements` = 14 | written, cannot run |
| G2 | T-junctions 0; `horizonSubdivVertices` = 18,754; `horizonSubdivTriangles` = 24,496; unique inserted points = 12,976 | written, cannot run |
| G3 | 20 inserted vertices within 2 deg of HORIZON2's third witness; quarter-turn control must FAIL | written, cannot run |
| G4 | the two shots differ AND the luminance ramp spans fewer pixels | written, cannot run; **its measurement is proven** (below) |
| G5 | neighbours at standing counts, each under `env -i` | written, cannot run |

G5 fixes a named defect rather than repeating it: HORIZON2's neighbour loop
exported OUT / SHEETS / V8DIR / V8VT / WAYBACK / REFLOG into its children, so
the neighbours read that gate's fixtures instead of their own. This loop runs
each neighbour under `env -i PATH HOME SYSTEMROOT`.

### Two refuters that DID run, each shown red on purpose

The gate cannot run, but two of the instruments it depends on are pure Python
and were proven now, with their own controls, rather than left as intentions.

`tests/spells/lodgen_horizon3_diff.py` -- G1's differ. A way back that says
"close enough" is not a way back, so it finds every differing byte, groups them
into runs, and fails any run that is not one of three DECLARED differences.
`--selftest` (log `diff_selftest.log`), run against the real 33,123-placement
`.lodi`:

| case | expected | got |
| --- | --- | --- |
| a file against itself | 0 runs, exit 0 | 0 runs, exit 0 |
| one byte flipped at 0x30, undeclared | **exit 1**, the run printed | `UNDECLARED 0x30..0x31 (1 B) old 61 new 9e`, exit 1 |
| the same flip, declared as a census word | exit 0 | `declared: census word pretendCensus 1 run(s)`, exit 0 |

`tests/spells/lodgen_horizon3_ramp.py` -- G4's measurement. "The picture
changed" is not a test, because any bug changes the picture; the test is that
the 10%-90% luminance rise distance gets SHORTER. `--selftest` (log
`ramp_selftest.log`) builds synthetic images with known ramp widths:

| case | expected | got |
| --- | --- | --- |
| a 100-px ramp measured | about 81 px (the 10-90 slice) | 80 px |
| a 20-px ramp measured | about 17 px | 17 px |
| long vs short | exit 0 | `ramp 80 -> 17 px (79% shorter)`, exit 0 |
| long vs an identical copy | **exit 1** | `FAIL: the pictures are identical -- subdivision did nothing` |
| short vs long, the wrong way round | **exit 1** | `FAIL: the ramp did not get shorter (17 -> 80)` |

A flat picture cannot win: a profile with less than 4/255 of contrast returns no
span rather than a very short one, which is the trap a "smaller number is
better" test walks into when the render comes back black.

## 6. Neighbours

Not run. The neighbour set is listed in the gate's G5 and in the brief:
`lodgen_horizon.sh` at HORIZON2's count, `lodi_v7.sh` 12/0, `lodl_channels.sh`,
`lodgen_slab.sh` 16/0, `native_open.sh` 17/0/2, `render_shot.sh` 82/0,
`lodl_open.sh` 23/0, `lodgen_native.sh`. This lane changed no `src/` file, so
there is no code change for them to regress against; they are owed by whoever
resumes the implementation, not by this lane.

## 7. Build

**No code was compiled in this lane and the exe is unchanged.** `Fallout4.exe`
pid 11328 was up at 01:31:01 and stayed up through the whole of the design and
measurement work, so under CONSTITUTION rule 6 this lane ends BUILD PENDING.

```
release/NifSkope.exe   2026-09-18 23:47:33   22,949,376 B
                       sha1 b349f807426be700ed2ff9b54ee23e4fab3ba037
release/NifSkope.before_horizon3.exe  identical, the rung, taken at launch
find src -newer release/NifSkope.exe   ->  0 files
find src tests res -newer ...          ->  6 files, all of them tests:
    tests/spells/lodgen_horizon3.sh, lodgen_horizon3_diff.py,
    lodgen_horizon3_ramp.py (this lane) and three HORIZON2 left behind
```

### The owed qmake run, discharged

`Fallout4.exe` went DOWN between 02:05:15 and 02:05:58 (checked as its own
command, twice). That did not un-block the implementation -- the lane had
already spent its budget on measurement and design, and a half-written
subdivider left in a shared tree is worse than none -- but it did make it
possible to discharge the one build-side item HORIZON2 handed over:

> HORIZON2 LANDED, owed: *a qmake run, because `Makefile.Release` carries no
> dependency on `src/lodghorizon.h`, so make exits 0 having compiled nothing.*

Measured before: `grep -c lodghorizon.h Makefile.Release` = **0**, against 10
for `lodofile.h` and 7 for `lodifile.h`. The header IS listed in `NifSkope.pro`
at line 288, so the project file was right and the generated makefile was stale.

```
cp Makefile.Release scratchpad/horizon3_20260919/Makefile.Release.before_qmake
C:/msys64/ucrt64/bin/qmake.exe -o Makefile NifSkope.pro -spec win32-g++ CONFIG+=release
   rc=0   (qmake.log; the lupdate/lrelease messages are pre-existing)
grep -c lodghorizon.h Makefile.Release  ->  7
diff against the backup: 32 lines, every one a dependency line
   (src/lodghorizon.h, src/lodghorizonrefute.h, src/lodgenao.h,
    src/lodinative.h, src/io/lodvfile.h, src/data/niftypes.h)
   plus one dist-zip line
mingw32-make -f Makefile.Release -j8   ->  rc=0, Nothing to be done for first
```

The build doing nothing is the right answer and it is checked, not assumed. Five
objects now depend on the header, and every one of them was compiled AFTER the
header's last edit, so nothing was stale:

| object | compiled | vs `src/lodghorizon.h` 09-18 23:46:09 |
| --- | --- | --- |
| `btdterrain.o` | 09-18 23:46:30 | newer |
| `lodinative.o` | 09-18 23:46:30 | newer |
| `nativeemit.o` | 09-18 23:46:39 | newer |
| `nifcli.o` | 09-18 23:46:48 | newer |
| `lodgen.o` | 09-18 23:47:04 | newer |

So the exe on disk is genuinely HORIZON2's exe and genuinely up to date. **The
fix is latent: it bites on the next edit to `src/lodghorizon.h`, which before
today would have compiled nothing and still exited 0.** The backup
`Makefile.Release.before_qmake` is in the lane folder if the regenerated makefile
ever needs to be reverted.

## 8. Pictures

None. Every picture in the brief needs an exe run, and the exe could not be run
while the game was up; by the time it went down the remaining budget was better
spent closing the report honestly than starting a render pass whose subject
(subdivision) does not exist in the exe yet. The six framings are carried in
`PENDING.md` with their sun angles and the caption rule, and G4's measurement
instrument -- the part that decides whether a picture proves anything -- is
written and proven green (section 5).

## 9. Docs

### What was NOT written, and why

`docs/FO4CS_IMPROVED_LOD_PLAN.md` s9 (the far-shadow contract, the 2026-09-18
rulings) was **not** edited. Writing a format into the consumer contract before
the producer can emit it invites FO4CS to code against bytes that do not exist;
the contract is the one document where "planned" and "shipping" must not blur.
The exact text to splice when the bake lands is below, and `PENDING.md` names it.

### s9 extension, as text, for when tier 2 and the bit ship

> **Tier 2 -- vertices inserted along long edges.** A consumer reads nothing
> new. The `.lodo` is version 5 and a v4 reader refuses it by name; inside, the
> vertex row is unchanged and there are simply more vertices and more triangles.
> No sampling rule changes and no new stream is present. The census words
> `horizonSubdivVertices` and `horizonSubdivTriangles` say how many were added,
> and are 0 when the knob is 0.
>
> **Tier 3 -- the per-face horizon sheet.** Present only when a `FACE` table
> exists in the `.lodo` and an `HFACE` stream (role 8) exists in the `.lodi`. A
> library vertex carries `u16 faceId`; when it is 0xFFFF the consumer samples
> the per-vertex horizon exactly as today. Otherwise it samples the face sheet
> at `u = dot(p - origin, axisU) / du`, `v = dot(p - origin, axisV) / dv`, where
> `origin`, `axisU`, `axisV`, `du`, `dv` come from the face record; the sheet is
> `texW x texH` sample positions, each four RGBA texels, sixteen azimuth bins in
> the same order and the same byte encoding as the per-vertex field
> (`byte = round(elevation_deg / 90 * 255)`, bin 0 = north, clockwise to east).
> A consumer that does not implement tier 3 must fall back to the per-vertex
> field, which is always present, and not to "no shadow".
>
> **The scrappable bit.** Bit 6 of the instance record flags word at 0x14,
> `.lodi` version 9. Set means: the placement base has a workshop scrap recipe
> AND the placement stands inside a settlement build area, so the player can
> remove it and the far map must not treat it as permanent. The far-map consumer
> DROPS a set placement from the caster set once that workshop is loaded and
> owns it; until then it casts like anything else. The bit is derived at bake
> time from `Fallout4.esm` and is not a knob -- it is always written from v9 on.
> Census word `scrappablePlacements`.

### Skill text, as text

`nifskope-ww-lodgen`, two rows for the knob table:

> `--horizon-subdivide <u>` -- insert vertices along every `.lodo` edge longer
> than `<u>` world units before the horizon march, so a long edge carries more
> than two horizon samples. Default **0 = off**. 512 costs about 1.9 MB and 46 %
> more drawn triangles on the urban region; 256 costs 7.8 MB and 184 %. Never
> removes geometry -- the authored LOD models rule stands.
>
> `--horizon-face-sheet <area u2>` -- give every coplanar face bigger than
> `<area>` square units its own 64-u horizon texture, for the shadows that land
> in the middle of a face and never touch its rim. Default **0 = off**. 1,024^2
> covers every such face measured on the urban region (8 of the 60 largest) for
> about 1.4 MB.

`nifskope-ww-render-shot`, one row for the channel table:

> `scrappable` -- magenta where the placement is workshop-scrappable (its base
> has a scrap recipe and it stands in a build area), grey where it is not. Note
> line: "scrappable: 14 of 33,123 on the urban region; the region overlaps only
> 5 of the 111 build areas in the plugin."

### Format sections

The `.lodo` and `.lodi` changes are specified line by line with their provenance
in sections 2, 3 and 4 above -- version words and their source lines, the flag
bit and its refusal-mask change, the two new tables and the four census words.
They go into `docs/LODGEN_NATIVE_LODO_LODI.md` s3/s4 when the bake emits them,
not before, for the reason at the top of this section.

## 10. For the director to splice

### WW_CHANGES.md paragraph

> **Far shadows, the three tiers measured (2026-09-19).** bungo asked whether the
> gradient across a big LOD quad was optimal; it is not, and the answer is three
> tiers of detail rather than one. This lane measured all three on the
> 33,123-placement urban region before designing any of them. Inserting vertices
> along edges longer than 512 units costs 1.9 MB and 46 % more drawn triangles
> (256 units costs 7.8 MB and 184 %; 1,024 units costs 0.36 MB and 7 %). A
> per-face horizon texture, for faces whose shadow lands in the middle and never
> reaches a rim, is needed by 8 of the 60 largest faces -- all of them
> horizontal, 7 of them highway overpass decks -- and covering all of them costs
> 1.4 MB. Both are knobs defaulting to 0, so nothing changes until bungo rules.
> The workshop-scrappable rule was read out of `Fallout4.esm` rather than
> remembered (157 scrap recipes, 87 of which name a form list, expanding to 1,012
> scrappable base forms; build areas are box primitives that link BACK to the
> workshop) and lights 14 of the placements in the region. No code was compiled:
> the game was up. The stale makefile that let a `lodghorizon.h` edit compile
> nothing was regenerated and now carries the dependency.

### HANDOFF.md block

> **HORIZON3 PARKED BUILD PENDING -- 2026-09-19 02:0x.** Measurement, design and
> research complete; no `src/` file touched, exe unchanged at
> b349f807426be700ed2ff9b54ee23e4fab3ba037 (2026-09-18 23:47:33, 22,949,376 B).
> `Fallout4.exe` pid 11328 was up at 01:31:01 and went down at 02:05:xx, too late
> in the budget of the lane to start the implementation.
> LANDED: the three measurement tables (report s1); the tier 2 and tier 3 designs
> with format and version bumps named to their source lines (s2, s3); the
> workshop-scrappable rule with its counts and examples (s4);
> `tests/spells/lodgen_horizon3.sh` pre-registered with its floors as constants;
> `lodgen_horizon3_diff.py` and `lodgen_horizon3_ramp.py`, both proven green with
> their own red controls; the owed qmake run -- `grep -c lodghorizon.h
> Makefile.Release` 0 to 7, make rc=0 nothing to do, five dependent objects all
> newer than the header (s7).
> RULED AWAY: nothing. Step 1(c) came back 8 of 60, not about 0, so the "tier 3
> not needed on this population" section the brief allows is NOT written and
> tier 3 stays owed.
> OWED: the tier 2 subdivider, the tier 3 sheet, the scrappable bit in the bake,
> `.lodo` v5 and `.lodi` v9, the six pictures, the doc splices, and running the
> gate. Exact resume point in `scratchpad/horizon3_20260919/PENDING.md`.
> CORRECTED IN-LANE: the first 1(c) sweep (20 faces, 2 azimuths) returned 0 and
> was wrong by under-sampling; the 60-face, 5-azimuth sweep returns 8, and the
> two agree on their overlap.

## 11. ROWS FOR BUNGO

Three rulings are owed, and every number below is measured, not guessed.

**Ruling 1 -- how long an edge is allowed to be before we put a vertex in it.**
Nothing changes until you pick one; 0 is what ships.

| you say | extra file size | extra drawn triangles | what it buys |
| --- | --- | --- | --- |
| 0 (ships today) | none | none | the gradient you objected to stays |
| 1,024 units | 0.36 MB (+4.5 %) | +7 % | only the worst offenders get fixed |
| **512 units** | 1.94 MB (+24 %) | +46 % | the 41,042 triangles with an edge over 512 units all get cut |
| 256 units | 7.81 MB (+98 %) | +184 % | almost every big quad, at four times the file and four times the triangles of the 512 row |

My reading: 512 is the row that matches the complaint, and 1,024 is the row to
pick if 46 % more triangles in the far field is too much to accept untested.

**Ruling 2 -- whether the few faces with a shadow in the middle get their own
little texture.** 8 of the 60 biggest faces have one, all of them flat and
facing up, 7 of them highway overpass decks; the worst is an off-ramp where a
550-unit patch of shade sits in the middle with nothing touching the edges.

| you say | extra file size | what it buys |
| --- | --- | --- |
| 0 (ships today) | none | those 8 faces keep a shadow smeared across the whole deck |
| faces over 1,024 x 1,024 units | 1.41 MB | all 8, plus 109 faces that did not need it |
| faces over 512 x 512 units | 8.3 MB | far more than the measurement justifies |

Subdivision cannot substitute: inserting vertices along edges only ever adds
points on the rim of a triangle, so a dark patch that touches no rim stays
unrepresentable however fine the cut. The alternative -- scattering vertices
across the middle of those faces instead of a texture -- costs 1.5x the bytes
and 18,188 extra triangles for the same 8 faces, so the texture is the cheaper
of the two if you want them fixed at all.

**Ruling 3 -- what scrappable should mean.** The bit is meant to let the far map
drop settlement junk the player can remove.

| you say | lights up | what it means |
| --- | --- | --- |
| recipe AND inside a build area (what the brief says, and what is designed) | 14 of 33,123 | only junk a player can actually scrap where they stand |
| recipe alone | 705 of 33,123 | anything scrappable in principle, settlement or not |

Fourteen looks tiny because this test region is one corner of the Commonwealth
and only 5 of the 111 settlement build areas in the game reach into it -- not
because the rule is broken.

## 12. MISTAKES entries, for the root ledger

**2026-09-19 -- a zero that agreed with me, believed for ten minutes.** Step 1(c)
asked how many of the 20 largest faces carry a shadow boundary that touches no
edge. I ran it at two sun azimuths and got 0, which was exactly the answer that
made tier 3 unnecessary and saved the lane a feature. I widened it to 60 faces
and five azimuths -- one changed line, `diff`-proven -- and got 8. The two runs
agree on their overlap: every hit is at an azimuth the first run never tried, or
on a face it never looked at. The lesson is not "sample more"; it is that **the
sweep which returns the convenient answer is the one to widen first**, and that
the domain of a measurement belongs in the report beside its result, because
"0 of 20" and "0 of 20 at two azimuths" are different claims.

**2026-09-19 -- the rule I would have written from memory was wrong twice.** The
workshop-scrappable rule looked like two lookups. Both were wrong by one
indirection. The `CNAM` of a scrap recipe is usually a `FLST`, not a base -- 87
of 157 -- so reading it at face value gives 157 scrappable bases instead of
1,012 and a count on the region of 0 instead of 705. And the settlement build
area is not on the workshop and not on the `WorkshopLinkCenter` marker it links
to; it is a box primitive on a third reference that links BACK to the workshop
with `WorkshopLinkedPrimitive`, which only a reverse-link scan finds. Both were
caught by printing a count that was zero and refusing to accept it. **A
record-layout rule is not written until a count it predicts comes back non-zero
for a reason you can name.**

**2026-09-19 -- a known-answer control that was testing the wrong thing.** The
control for the interior-shadow detector builds a deck with a tower on it and
expects interior = YES when the tower is in the middle. It came back NO. The
detector was right: the tower was 3,000 units tall, so its shadow at 30 degrees
was 5,196 units long on a 4,096-unit deck and ran off the far edge. The control,
not the instrument, was broken. Fixed by parameterising the tower height and
using 500 units, which puts the end of the shadow 926 units inside the rim.
**A known-answer control needs its answer worked out in numbers before it is
run, or a green control is only evidence that two bugs agree.**

**2026-09-19 -- the contract doc I did not write in.** It was tempting to extend
`docs/FO4CS_IMPROVED_LOD_PLAN.md` s9 with the tier 2 and tier 3 formats while
they were fresh. They are not implemented. A consumer contract is the one page
where a planned format and a shipping one must not blur, so the text is in this
report and the splice is in `PENDING.md`. Recorded as a mistake NOT made, so the
next lane does not "finish the docs" by mistake.

## 13. Finished-work skill review

- `nifskope-ww-lodgen` -- needs the two knob rows in section 9 when the bake has
  them, not before.
- `nifskope-ww-render-shot` -- needs the `scrappable` channel row in section 9.
- `ww-control-calibration` -- earned its keep twice today (the detector control
  and the two self-tests). Worth adding the line the deck-and-tower failure
  taught: **the answer of a known-answer control must be computed, in units,
  before the control is run.**
- `ww-contract-provenance` -- the reason s9 was left alone. No change.
- `ww-texel-picture` -- unused this lane; no picture was rendered.
- `nifskope-ww-build-verify` -- its object-vs-header check is exactly what
  section 7 does by hand. Worth folding in the stronger form: not "is the exe
  newer than the headers" but "does `Makefile.Release` MENTION each header at
  all", which is the failure HORIZON2 hit and a newer-than check cannot see.
- A skill this lane wanted and did not find: **measuring a mesh population
  before designing a format for it**. `measure_tiers.py` (edge and face census
  from a `.lodo`, with cluster-split accounting) and `interior_shadow.py`
  (casting a witness across a face on a lattice) are both general, and both
  would otherwise be rewritten. Worth writing as `ww-population-census` if bungo
  wants it.

---

## DONE

horizon3 parked BUILD PENDING at 2026-09-19 02:1x. Measurement, design, the
scrappable rule, the gate and the doc text are landed; no `src/` file was
touched and the exe is byte-identical to the one this lane started with. The
resume point is `scratchpad/horizon3_20260919/PENDING.md`.

### Five plain sentences for bungo

1. You were right that the gradient across a big LOD quad is an artifact, and
   the fix is to put vertices in the long edges at bake time: cutting every edge
   longer than 512 units costs about 1.9 MB and 46 % more triangles in the far
   field, and cutting at 1,024 units costs almost nothing but fixes only the
   worst offenders -- that is the first thing I need you to pick.
2. The little per-face shadow texture turns out to be genuinely needed, but only
   just: 8 of the 60 biggest faces have a patch of shade sitting in the middle
   that never touches an edge, they are almost all highway overpass decks, and
   covering all of them costs 1.4 MB -- so that is the second thing to pick, and
   0 is what ships until you do.
3. I read the scrappable rule out of the game files instead of trusting my
   memory of it, which was lucky, because it was wrong in two places -- a scrap
   recipe usually names a whole list of objects rather than one, and a
   settlement build area is a box on a third object that points back at the
   workshop -- and with it right, 14 of this test region 33,123 placements light
   up.
4. Nothing was compiled: Fallout 4 was running for almost the whole lane, so
   this is a measure-and-design lane that ends BUILD PENDING, with the gate and
   its two measuring instruments written and proven against deliberately broken
   inputs so the next lane cannot fool itself.
5. I did fix the stale makefile that let a change to the horizon header compile
   nothing at all and still report success, and nothing was rebuilt today, so
   when the subdivider finally lands his open window needs a restart.

---

# RESUMED 2026-09-19 02:21 -- write the code

The director resumed the lane at 02:21 with an explicit instruction: do not park
on "game up" with `src/` untouched; write the whole implementation to disk now
and park only the BUILD. `DONE` is removed, `BUILDING` is back. My own check, as
its own command:

```
02:21:39   Fallout4.exe   17248 Console   1   7,701,064 K   -> UP
```

So: no build, no exe run, no picture. Source, docs and syntax-only checks.

## 14. The code, written 2026-09-19 02:2x-03:xx (game up, nothing built)

### 14.1 `src/lodgsubdiv.h` -- tier 2's geometry, and the bug the self-test caught

New file. It holds the knob's constants, `LodgenSubdivStats`, the per-edge point
count, the welded edge key, the triangulator and a self-test. It knows nothing
about `.lodo`: the driver that walks a library is in `src/lodofile.cpp` beside
`lodoEmitCluster`, because the cluster's sphere, cone and size class must be
computed in exactly ONE place or a subdivided cluster drifts from a coarse one.

**The first triangulator was wrong and the self-test found it in four minutes.**
I wrote the boundary polygon of a subdivided triangle and ear-clipped it, always
at a strictly convex corner, choosing the ear with the largest inradius. That is
a textbook-correct convex triangulation and it is broken here, exactly:

```
n=0,0,1  made=2  want=2  worst area = 0.000000000   P=4
```

A triangle with ONE point on one edge is a four-point polygon `v0, v1, v2, c1`.
The best ear is the original triangle `(v0, v1, v2)` -- it has by far the largest
inradius -- and clipping it strands `(v0, v2, c1)`, three points that are exactly
collinear because `c1` lies on the edge `v2 -> v0`. No epsilon fixes that; the
degeneracy is exact. Worse, thirty of the 216 cases also came out SHORT of the
identity (`n=0,0,2 made=1 want=3`), because once the remaining ring has no
strictly convex corner the clip has nowhere to go and stops.

The replacement touches no coordinate at all. Take the edge carrying the most
points, cut from its MIDDLE point to the opposite corner, recurse on the two
halves. Every triangle it makes has two corners of a non-degenerate triangle and
a third strictly between two of them, so a degenerate output is not unlikely, it
is impossible. Winding is preserved by construction (each half is a cyclic
rotation of its parent with one corner replaced), so the caller never checks it.
And the count is the identity by induction on the cut,
`T(n0,n1,n2) = T(k,0,n2) + T(n0-1-k,n1,0)` with `T(0,0,0)=1`, which is
`n0+n1+n2+1` for every split point -- so step 1(a)'s triangle column is a sum and
not an estimate, whichever edge is cut first.

### 14.2 The self-test, run

`lodgenSubdivSelfTest()` is compiled and RUN -- it is a header and a two-line
`g++` harness, not the exe, and it never touches `release/`:

```
g++ -std=c++17 -O1 -Isrc -I<qt6> /tmp/h3test.cpp -o /tmp/h3test.exe -lQt6Core
lodgenSubdivSelfTest GREEN
rc=0
```

432 cases (0..5 points on each of three edges, two windings), in a plane through
none of the three axes so a triangulator that assumed `z = 0` has nowhere to
hide. Each case asserts six things: the identity `n0+n1+n2+1`, strictly positive
area on every output triangle, every output wound with the original, no triangle
naming a point twice, **every inserted point used at least once** (a point left
out IS the T-junction), and the parts summing to the area of the whole within a
part in 100,000.

A MISTAKES entry is owed for this and it is in section 12 as entry 4.

### 14.3 The subdivision driver landed, and the splice that had to be re-done (02:3x-02:52)

The heredoc that failed at the end of the last session was not re-attempted in
the same shape. The body was written to a scratch file with the `Write` tool and
spliced in by a short Python script that reads the fragment from disk, which is
the fix the MISTAKES entry for the same failure class already prescribes.

**A second instance of the same class, caught this session and worth recording:**
a `python - <<'PY'` heredoc whose Python source contained `\\n` inside a bytes
literal did NOT arrive as backslash-n. A probe proved it: `d.find(b"own square)")`
returned 293330 and `d.find(b"own square)\\n\"")` returned -1 against the same
bytes, whose `repr` is `own square)\\n"`. The escape was collapsed in transit.
**Rule: any script containing a backslash goes through the `Write` tool, never
through a heredoc.** Every splice from here on obeys it.

What is now on disk, tier 2 complete, end to end:

| file | what landed |
|---|---|
| `src/lodgsubdiv.h` | the geometry: point count, edge key, the combinatorial bisection, the triangulator, the self-test (unchanged since 14.1) |
| `src/lodofile.h` | `LODO_VERSION_SUBDIVIDED = 5`, the two header words, the two `LodoLibrary` words, the `lodoSubdivideLibrary` declaration |
| `src/lodofile.cpp` | `#include "lodgsubdiv.h"`, the v5 header cells, the conditional-version writer, the v4/v5 reader, `lodoDescribe`, and **the whole `lodoSubdivideLibrary` body** |
| `src/nativeemit.h` | `lodgenNativeSubdivideOption( float, float )` |
| `src/nativeemit.cpp` | the two `State` knobs, the two sticky `NativeLadderOptions` knobs, the option copy, the setter, the call site, and the `native-subdivide:` census line |
| `src/nifcli.cpp` | `--horizon-subdivide <u>` and `--horizon-face-sheet <a>`, both validated, both default 0, the signature, the call site, the sticky-setter call, usage line and option doc |

Three syntax-only parses with the project's own `CXXFLAGS` and `INCPATH`
(`scratchpad/.../syn.sh`; `-fsyntax-only` writes no object, links nothing and
never touches `release/`):

```
src/lodofile.cpp    rc=0, no diagnostic naming any of our files
src/nativeemit.cpp  rc=0, clean
src/nifcli.cpp      rc=0, clean
```

**One deviation from the plan in section 13, stated rather than taken quietly.**
The plan said the driver would REFUSE the bake when an original vertex came back
from the decode-and-requantise round trip with different stored bytes. That
refusal is wrong and it is gone. A u16 position dequantised to float32 and
quantised again is not guaranteed to land on the same u16 -- the rounding is
real, it is about one unit, and a bake that stops because of it would make the
knob unusable for a reason that has nothing to do with geometry. What the driver
does instead is stronger: it writes the ORIGINAL sixteen bytes back over the
re-quantised ones, so "subdivision only ADDS and never moves" is true **by
construction** and not by an argument about rounding. `requantMismatch` survives
as a MEASUREMENT -- how often the rescue was needed -- and is printed in the
census. It is no longer a failure word.

**The call site moved too, and that removed work rather than adding it.** The
plan had the cut running after the base loop with a `LodoBase::fullTriangles`
recompute afterwards. It runs BEFORE the base loop now, so `fullTriangles` is
summed from the library the file will actually hold and there is nothing to
recompute -- and nothing to forget to recompute later.

**What is still owed on tier 2, and it is the honest gap:** not one line of this
has been through a compiler's back end, let alone a bake. `-fsyntax-only` proves
the source parses and names things that exist. It proves nothing about whether
the cut library reads back, whether the ladder still selects, or whether the
partition invariant `tests/spells/lodgen_native_fields.py` h5 checks still holds
over a cut file. Those are the gates in `PENDING.md`, and they need the game
down.

### 14.4 The docs, the channel and the skills, 03:0x-03:1x (game still up)

**`docs/LODGEN_NATIVE_LODO_LODI.md`, 2 splices, 161,611 bytes.** s3.7 *The
subdivided library (`.lodo` v5)* went in at the end of section 3, immediately
before the `## 4.` header, and s4.12 *The workshop-scrappable bit (`.lodi` v9)*
at the end of s4.11, immediately before `## 5.`. Both are headed **NOT FLOWN**
in their first line, and s3.7 says in its opening paragraph that every number in
it is a prediction from `measure_tiers.py` until a gate run says otherwise. The
doc's title line still reads `.lodo` v4 + `.lodi` v6 and was left alone: it is
already stale against v8, and editing it to say v5/v9 would be the one place in
the document that claimed a flown format.

**`docs/FO4CS_IMPROVED_LOD_PLAN.md` s9 was NOT written, and that is a decision,
not an omission.** It is the CONSUMER contract -- what the game-side reader
promises to do with the bit -- and section 9 of this report already recorded why
it must not be written before the producer has emitted a single byte: a contract
written against an unbuilt writer is a contract nobody has read the other half
of. It is named in `PENDING.md`.

**`WW_LODL_CHANNEL=scrappable`, the viewer half (5 splices in
`src/lodinative.cpp`, 1 in `src/lodinative.h`), syntax-clean.** Magenta where
bit 6 is set, grey 0.25 where it is not. **Two colours and nothing between
them**, because the bit is one bit and a gradient would invite the eye to read a
confidence the file does not carry; magenta is in no other channel's palette, so
a scrappable placement cannot be mistaken for a hash collision in `identity`.
It rides the existing `objectChannel` path, so the read-back line comes for
free -- and because the values pushed are 1 and 0, **the printed mean IS the
share** (0.0004 expected, 14 of 33,123).

The guard that matters more than the colours: on a file below version 9 the
channel prints, by name and by version number, that **the FILE says nothing** --
because an all-grey picture off a v8 file and an all-grey picture off a v9 file
with a broken rule look exactly the same, and only one of them is a defect.

**This is the refuter the census cannot be.** `scrappablePlacements 14` is a
number that can be right for the wrong reasons. A picture in which a whole
street comes back magenta says the build-area box test is wrong, in one glance,
with no further instrumentation.

**Skills.** `nifskope-ww-lodgen/SKILL.md` gained a closing section *Added by
lane HORIZON3 (2026-09-19) -- THREE KNOBS, ALL OFF, NONE FLOWN* (46,469 bytes):
the three-row knob table with each knob's exact way back, Rule A named as a
compromise rather than hidden, the clamp and cap words with the sentence that a
bake whose `capped` is not 0 did not do what was asked, the pre-registered
costs, the three-clause scrappable rule with its one labelled heuristic, the
`--lodi-v7` drop guard, the three verbatim census words, and the gate's G1/G1c
split. `nifskope-ww-render-shot/SKILL.md` (43,441 bytes) gained the
`scrappable` row in the native-channel table and a bullet, first in the list, on
what an all-grey render off a v8 file means.

### 14.5 Two further deviations, added to section 12's list

**Deviation 5 -- the scrappable bit is a KNOB.** Section 9 of this report
drafted it as "not a knob -- always written from v9 on". It ships as
`--horizon-scrappable`, default off. Two reasons, and the first outranks the
draft on its own: bungo's standing rule of 2026-09-12 that every master ships
OFF, and of 2026-09-17 that an owed ruling never ships as a default. The second
is cost -- the rule needs a full plugin walk (COBJ, FLST, every REFR's XPRM and
XLKR), and that is not something every bake should pay for to produce a number
almost nobody has asked for.

**Deviation 6 -- G1c got its own bake.** The gate was pre-registered to grep
`scrappablePlacements` out of G1's log. G1's whole purpose is that the rung exe
and the new exe write IDENTICAL bytes at the knobs' off values, and the rung exe
has no `--horizon-scrappable` to be given -- so satisfying the pre-registered
grep would have meant turning the bit on inside the one run that must not
differ. G1c now runs its own bake into `$OUT/scrap` and greps `g1c.log`. **The
floor did not move**: still 14, still from `scrap_rule.log`, still
pre-registered. This is the one place where the gate's TEXT was edited rather
than the code being made to match it, and it is written down here for that
reason.

### 14.6 The mistake of this session, for the root ledger

**A script that contains a backslash must go through the `Write` tool, never
through a heredoc.** A `python - <<'PY'` whose source held `b'...\n"'` did not
arrive at Python as a backslash followed by `n`. It was proved rather than
assumed: against bytes whose `repr()` is `own square)\n"`, `d.find(b"own
square)")` returned 293330 and `d.find(b'own square)\n"')` returned -1. The
anchor "could not be found" in a file that plainly contained it. This is the
same failure class as the heredoc EOF error that ended the previous session, and
every splice script from that point on -- `splice_doc.py`, `splice_chan.py`,
`splice_skills.py` -- was written to a file first.

### 14.7 What is NOT written, named so it can be tasked

**Tier 3, the face sheet, is NOT implemented.** `--horizon-face-sheet` parses,
clamps and reaches `NativeLadderOptions::horizonFaceSheet`, and there it stops:
nothing is written and no byte of any file changes. It needs a `.lodo` `FACE`
table (48 B a row: `u16 meshId`, `u16 faceIndex`, `f32 origin[3]`, `f32
axisU[3]`, `f32 axisV[3]`, `f32 du`, `f32 dv`, `u16 texW`, `u16 texH`, `u32
sheetOffset`), a PARALLEL `u16 faceId` table rather than a widened 16-byte
vertex, a `.lodi` role-8 `HFACE` stream of `texW*texH*16` bytes, the sampling
itself, and readers, writers, census words and gates for all of it. **A
half-written tier 3 is worse than none** -- it would be a format that a reader
must refuse and a knob that lies about what it does -- and the direction in
force for this lane says to write complete functions, not half ones. The knob
and the option field exist only so that the CLI signature and the option struct
do not have to change a second time when tier 3 is written.

What tier 3 is FOR, so the next lane does not have to re-derive it: **edge
subdivision can never fix an interior-only shadow.** A shadow that falls in the
middle of a wall face, touching none of its edges, has no vertex to land on no
matter how finely the edges are cut. That is the 8-of-60 case measured in
section 5. Tier 2 fixes the other 52.

---

## DONE (final -- this supersedes the 02:1x DONE block above, which said no
## `src/` file had been touched; six of them have been since)

horizon3 parked BUILD PENDING at 2026-09-19 03:1x. The implementation is
COMPLETE ON DISK -- tier 2's subdivider, the `.lodi` v9 scrappable bit, the CLI
knobs, the census words, the viewer channel, the gate, the format doc and both
skills -- and every translation unit it touches parses under the project's own
flags. **Nothing was built and nothing was run**: Fallout4.exe pid 17248 was up
at 03:09:18, checked as its own command. `release/NifSkope.exe` is byte for byte
the exe this lane started with. Tier 3, the face sheet, is deliberately NOT
written and is named in `PENDING.md` so it can be tasked on its own. The resume
point is `scratchpad/horizon3_20260919/PENDING.md`, headed `BUILD PENDING`, with
the build command, the gate's eight pre-registered floors and the two owed
pictures spelled out.

### Five plain sentences for bungo

1. The long-edge fix is written and on disk now, not just designed: it cuts the
   shared LOD geometry so no edge is longer than the number you give it, it only
   ever ADDS vertices and triangles and never takes any away, and at 512 units
   it costs about 1.9 MB and 46 % more triangles in the far field -- which
   number you want is still the first thing I need you to pick, and it ships at
   0, meaning off.
2. I also wrote the little bit that marks which far-field objects a player can
   scrap at a settlement workshop, because an object you scrapped in hour one is
   an object the distant view should stop drawing and stop casting a shadow
   from; on the test region that is 14 objects out of 33,123, and it ships off.
3. The per-face shadow texture -- the one thing that would fix a patch of shade
   sitting in the middle of a big flat face, which no amount of edge cutting can
   reach -- I deliberately did NOT write, because half of it would be worse than
   none of it, and it is written up as its own job with everything it needs.
4. Still nothing compiled: Fallout 4 was running the whole time, so this ends
   BUILD PENDING again, with the build command and the gate's eight floors
   written down so the next lane runs them rather than inventing them.
5. All three new switches are off by default and each one has an exact way back
   to the bytes we ship today, nothing here is fixed or final or true until you
   see it yourself, and when the build finally lands his open window needs a
   restart.
