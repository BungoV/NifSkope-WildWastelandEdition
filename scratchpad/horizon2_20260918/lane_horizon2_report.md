# Lane HORIZON2 -- terrain horizon sheet over-occludes

## 0. Exe at launch + rung

Read back at 2026-09-18 22:57:50 CEDT (`date` in the same turn):

- `release/NifSkope.exe` -- mtime 2026-09-18 21:59:46.760044700 +0200, 22,952,448 B,
  sha1 `e578b76f9d7a2d011363e4300a2c94967e14d605`. Matches the brief's HORIZON1 final exactly.
- Rung taken once, before any build: `release/NifSkope.before_horizon2.exe`,
  sha1 `e578b76f9d7a2d011363e4300a2c94967e14d605` (byte-identical to the launch exe).
- Process check at launch (`tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?`): rc=1, no match.
  Fallout4 DOWN, no NifSkope window live. (`release/NifSkope_inuse_2000.exe` is an older renamed-aside exe on disk,
  not a running process.)
- Markers: `scratchpad/horizon2_20260918/BUILDING` touched first.

## 1. The third witness -- ten receivers on chunk 4.4.-12

Every number in this section is in `scratchpad/horizon2_20260918/table_out.txt`, written by
`table.py`; the receivers are in `receivers.json`, written by `pick10.py`.

### 1.1 What the third witness is

`wit.true_skyline` casts a **pencil ray per 1 degree of azimuth**, 360 of them, from the receiver
out to the full 127,561 u reach at a constant 32 u step, and reads at every step:

* the **BTD heightmap** through the existing reader (`--dump-land` -> `land.bin`, the whole
  worldspace as a 6145x6145 node grid), bilinear at the ray's own position -- no 128 u lattice, no
  mip chain, no `maxAlong`, no 2x2 tap, no near-skip, no segment maximum;
* the **placements as exact world boxes**, built by `boxes.py` from the shipped `Commonwealth.lodo`
  + `Commonwealth.lodi` through `tests/spells/lodgen_native_decode.py` -- an independent decoder
  written from the byte tables, which links none of the bake's C++. A box is the world AABB of the
  mesh's own local AABB (`.lodo` mesh row `aabbMin`/`aabbExtent`) carried through the instance
  rotation, scale and position: **2,449 boxes, tops 301..2,949 u**, footprint p50 256 u, p99 1,511 u.
  A box is **larger** than the mesh inside it, so this witness is an UPPER bound on the true
  skyline, never a lower one.

The 360 rays are folded to the 16 stored bins as the **maximum over each bin's own 22.5 degree
sector**, which is the quantity the stored byte claims to hold.

### 1.2 Two instrument bugs the controls caught before any conclusion was drawn

1. **My sheet reader had the channel order backwards.** The bake packs a Qt `0xAARRGGBB` u32 with
   `static const int shift[4] = { 16, 8, 0, 24 }` (`src/lodgen.cpp:11110`) and the sheet writer
   hands that plane out as R8G8B8A8, so **in the file bin *j* is byte *j*** -- shifts
   `{0,8,16,24}`. Reading the file's little-endian u32 with the C++ shifts swaps bins 0 and 2 of
   every four. The control that caught it: `|STORED - PORT|` over the ten receivers fell from
   **43.44 deg** to **0.17 deg** when the order was corrected, and the permutation recovered from
   the data was exactly `[2,1,0,3]` within each group of four.
   Note what this says about the in-bake refuter: it reads its bytes back out of the in-memory
   `planes` (`src/lodgen.cpp:11152`), never out of the file, so **no in-bake check can see a
   file-order defect at all**. The viewer reads byte *j* (`src/btdterrain.cpp:1762` ->
   `LodtSheets::sheetChannel( LODV_ROLE_HORIZON, tx, ty, bin % 4, ... )`), so the shipped file and
   the shipped viewer agree with each other. The mistake was mine, not the bake's -- but the gap in
   the refuter is real and is written up in section 11.
2. **My box slab test passed every box in the world on an axis-aligned ray.** For `dx == 0` exactly
   (azimuth 0) the outside-the-slab branch wrote `(+inf, -inf)`, which makes `min()` -inf and
   `max()` +inf and therefore every box a hit at distance 1 u. The tell was that bin 0 read exactly
   `90.0` at all ten receivers at once. Fixed in `wit._box_elev`: outside the slab the branch now
   writes `(+inf, +inf)`, so `tnear` is +inf and nothing is hit.

### 1.3 The control that makes the rest of the lane readable

`PORT` is a Python transcription of `lodgenHorizonCastAt` run over the bake's own two lattices,
rebuilt by `fields.py` exactly as `LodgenVtHorizon::build()` builds them -- LAND nodes raised at
128 u over the 70x70 cell rectangle, then raised again by the object height field's tops
(`objfield.bin`, 24,729 occupied squares). Over the ten receivers x 16 bins:

```
CONTROL   |STORED - PORT|    mean 0.14 deg    max 3.67 deg
```

The quantisation step is 0.353 deg and each receiver is evaluated at the **texel centre the bake
cast from** (upt = 32 u here, so a receiver named by world position sits up to 16 u from the sample
that was stored, and beside a building 16 u is tens of degrees). This control says the Python
witness chain is reading the same bake the C++ wrote. Every later claim rests on it.

### 1.4 Divergence from the brief, stated up front

The brief asks for *three receivers on open shoreline facing water*. **Chunk 4.4.-12 has no water
and no shore.** Of its 16,641 LAND nodes, **6** are at or below z = 0; the height distribution is
min -8, p5 240, p50 680, p95 1,032, max 1,184 (`survey.py`). It is dense downtown: of the 16,384
object lattice squares inside the chunk **7,562 (46.2%)** carry a placement, and of the candidate
receiver squares **57.2% stand inside a placement box**, the distance to the nearest box running
p75 126 u, p95 504 u, **max 1,183 u** (`pick10.py`).

So the brief's prediction -- *a texel on the shore looking out over water must read a skyline near
0 in that bin* -- has no receiver on this chunk to stand on. The ten were taken from the data
instead:

| id | class | world (x, y) | ground | nearest placement box |
|----|-------|--------------|--------|-----------------------|
| LOW1 | lowest open ground | 25,984  -45,056 | 528 | 270 u |
| LOW2 | lowest open ground | 23,296  -44,288 | 536 | 278 u |
| LOW3 | lowest open ground | 21,888  -44,160 | 592 | 305 u |
| FLAT1 | flattest open ground | 27,136  -41,088 | 680 | 491 u |
| FLAT2 | flattest open ground | 27,392  -38,144 | 688 | 538 u |
| FLAT3 | flattest open ground | 23,808  -41,984 | 672 | 474 u |
| ST1 | street, boxes both sides | 26,368  -38,528 | 680 | 257 u |
| ST2 | street, boxes both sides | 25,856  -39,168 | 680 | 251 u |
| FOOT1 | at a building's foot | 22,144  -39,680 | 880 | 9 u |
| FOOT2 | at a building's foot | 25,600  -41,216 | 656 | 9 u |

All ten sit a full cell (4,096 u) inside the chunk, so the `.lodi`'s own extent is not what decides
what they can see. The most a placement missing beyond that rim could add is
atan((2,949 - 700) / 4,096) = **28.8 deg**, which bounds the witness's possible under-report and is
still far below the 60..85 deg the sheet holds.

### 1.5 The table

Rows per receiver, in degrees, bin 0 = north and clockwise:
`TRUE` third witness with every box; `TRUEX` the same with boxes whose footprint contains the
receiver left out (an AABB containing the receiver reads ~90 deg in every bin, which is what the box
says and not what the mesh says); `TER` third witness, terrain only, placements left out;
`STORED` the shipped sheet; `PORT` the control above; `REF` the in-bake reference cone
(`lodgenHorizonReferenceElev`, 9 pencils across the bin, 32 u step, the same lattice).
The full ten tables are in `table_out.txt`. Two of them, and then the summary that decides the lane:

```
=== FLAT1 FLAT (27152, -41104)  lattice ground 680.0  true ground 680.0
  bin      0      1      2      3      4      5      6      7      8      9     10     11     12     13     14     15
 TRUE   21.7   21.0   12.1   12.3   13.5   16.9   28.5   30.7   31.1   44.4   56.1   60.4   60.9   35.8   30.7   21.4
  TER    6.9    5.5    3.2    1.7    1.5    5.2    5.7    6.1    7.0    5.2    8.1    7.9    4.5    4.3    4.3    3.9
STORED   21.5   30.4   30.4   13.4   16.6   14.8   33.9   35.6   35.6   25.1   64.6   59.3   44.5   33.2   22.9   26.1
 PORT    21.4   30.3   30.3   13.3   16.7   14.7   33.7   35.8   35.8   25.0   64.4   59.4   44.3   33.1   22.0   26.2
  REF    20.3   23.0   24.5   11.9   12.5   19.6   26.3   27.8   28.5   51.4   57.8   56.7   56.7   40.3   20.1   20.3

=== ST2   ST   (25872, -39184)  lattice ground 706.9  true ground 685.1
  bin      0      1      2      3      4      5      6      7      8      9     10     11     12     13     14     15
 TRUE   15.7   28.8   39.9   43.9   39.2   15.8   39.0   43.7   44.3   52.4   49.9   54.9   55.5   54.9   50.3   15.9
  TER    1.4    0.8    0.2    1.0    1.9    1.9    2.8    5.2   11.1   10.4    8.0    6.5    9.2    5.4    3.3    2.1
STORED   20.1   30.0   31.1   45.5   49.4   16.9   24.4   63.2   63.2   63.2   63.2   65.3   65.3   65.3   55.4   55.4
 PORT    16.4   29.9   31.1   45.4   49.3   16.9   24.5   63.1   63.1   63.1   63.1   65.3   65.3   65.3   55.4   55.4
  REF    14.9   34.5   47.7   52.0   52.0   52.0   42.4   59.0   59.0   59.0   64.2   67.0   67.0   67.0   64.2   54.0
```

```
over the ten receivers x 16 bins (degrees)
  CONTROL  |STORED - PORT|        mean  0.14   max  3.67
  SHEET    |STORED - TRUE|        mean  8.51   max 67.77    over 2 deg 81.9%   over 10 deg 24.4%
  REFER    |REF    - TRUE|        mean  8.19   max 65.53    over 2 deg 73.8%   over 10 deg 24.4%
  SHEET vs REFERENCE              mean  5.59   max 40.46
  terrain alone (TER)             mean 10.86   max 62.43
  the sheet stands more than 2 deg OVER the third witness in 48.8% of bins, UNDER in 33.1%
```

### 1.6 What the table says

**The brief's substance is confirmed and its detail is refuted.**

Confirmed: *the reference shares the bug, and a 97% agreement floor between two wrong witnesses
would be worthless.* The sheet misses the third witness by **8.51 deg** on average and the in-bake
reference misses it by **8.19 deg** -- the reference is no better. And the two of them agree with
**each other** (5.59 deg) considerably better than either agrees with the truth, because they share
the lattice, the 2x2 tap, the near-skip and the segment rule. The 97% balanced-agreement floor was
never measuring whether the sheet is right.

Refuted: the mean skyline over 60 deg is **not** an artefact of the bake's data handling. `TER`, the
true terrain-only skyline, is **0.2 to 15.7 deg** at nine of the ten receivers -- the land really
does leave the sky open, and the brief's instinct about that was right. What stands in the way is
the **placements**, and on this chunk they really are that tall: at ST2 the third witness reads
15.7 .. 55.5 deg across the sixteen bins and the sheet reads 16.9 .. 65.3. The viewer's note line
for this chunk (terrain horizon mean 64.37, 0.0% lit at a 15 degree sun) is therefore reporting a
real downtown skyline that is roughly 8 degrees too steep in the wrong places, not a fabricated one.

And the defect is **not a uniform inflation**: it is per bin, and it runs both ways -- more than
2 deg OVER the truth in 48.8% of bins, more than 2 deg UNDER in 33.1%. The rows show the shape
directly. Look at ST2 bins 7..10: the sheet holds `63.2 63.2 63.2 63.2` where the third witness
reads `43.7 44.3 52.4 49.9`, and bins 11..13 hold `65.3 65.3 65.3` against `54.9 55.5 54.9`. Four
adjacent azimuth bins carrying one identical byte is not a skyline; it is one maximum smeared
across a quarter turn. Section 2 names what does the smearing.

## 2. The cause, by elimination

Logs: `variants_out.txt` (`variants.py`), `variants2_out.txt` (`variants2.py`),
`lattice_out.txt` (`lattice.py`), `lit_out.txt` (`lit.py`), `candidate_out.txt` (`candidate.py`).

### 2.0 The first elimination pass found nothing, and that was the finding

`variants.py` changed one march rule at a time -- mip on/off, 2x2 tap vs 1x1, the tap's addressing,
the elevation measured at the near vs the far end of the segment, the growth ladder at 1.5 / 1.10 /
1.02, the near-skip at 0 / 1 / 4, the reach at 127,561 / 16,384 / 4,096 -- and scored each against
the third witness. **Not one of them improved the score**; the best was 8.46 deg against the shipped
rule's 8.50, and most were worse.

That result is only puzzling until you notice what it was scored against: the maximum over each
bin's 22.5 degree sector. Which is what the shipped march already computes, on purpose. Every rule
was being measured against its own convention.

The viewer does not read a sector maximum. `src/btdterrain.cpp:1750` takes the sun's azimuth, finds
the two bins either side of it, and **blends them** -- an operation that is only meaningful if each
stored byte is the skyline in *its own direction*. So the quantity the stored byte must be scored
against is the third witness's skyline at the bin's own azimuth, one pencil, and `variants2.py`
scores it that way. On that footing the numbers separate immediately.

### 2.1 The measurement that names the cause

```
third witness, ten receivers x 16 bins
  directional (one pencil at the bin's azimuth)   mean 43.34 deg
  maximum over the bin's 22.5 degree sector       mean 50.31 deg
  the sector convention alone costs               mean  6.97 deg  (max 50.87)

score = |march - DIRECTIONAL third witness|                    mean     max    bias
  the SHIPPED sheet                                           11.90   74.26   +8.59
  V0 shipped rule (control, reproduces the sheet to 0.14)     11.83   74.22   +8.52
  V1  mip off (wantCell 1, always level 0)                    11.23   74.22   +7.85
  V2  tap 1x1 instead of 2x2                                   9.93   57.54   +2.17
  V3  tap 1x1 addressed by the containing square                8.67   63.23   +2.21
  V4  mip off + tap 1x1                                         9.77   57.54   +1.83
  V6  mip off + tap 1x1 + growth 1.02                           9.96   65.07   -1.25
```

**The 2x2 tap is the cause, and the mip chosen against the bin width is its partner.** Dropping the
2x2 block to the one square the sample lands in takes the systematic bias from **+8.52 deg to
+2.17 deg** in one step. Nothing else in the march moves the bias at all: the mip switch is worth
0.67 deg, the growth ladder over-corrects past zero, the near-skip and the reach are worth nothing.

The mechanism is written down in the source itself, as an intention rather than a defect
(`src/lodghorizon.h:56-64`): *"HOW WIDE IT MAY BE is the design's one resolution invariant: the
footprint never exceeds the AZIMUTH BIN'S OWN width at that distance ... and it errs toward MORE
occlusion, never less, which is the safe side for a shadow."* The level is picked so that
`2c <= binWidthFraction * d`, and then each tap reads a 2x2 block, so the footprint is deliberately
as wide as the whole bin. The march is therefore a **maximum over the bin's sector**, by design.
What was never measured is the price: the third witness puts it at **6.97 deg of the skyline**, and
the viewer then interpolates two such maxima as if they were directional samples, so the price is
paid twice.

### 2.2 What that costs in the thing the gate actually asks

`lit.py` puts the gate's own question -- *is this texel lit by a sun at (azimuth, elevation)* -- to
the third witness over **576 real texels** of chunk 4.4.-12, and samples the sheet the way the
viewer samples it:

```
sun               TRUE    SHEET     REF
az 120 el  5      3.8%     0.0%     0.0%
az 120 el 15     10.8%     1.2%     5.6%
az 120 el 30     22.7%    12.3%    17.7%
az 120 el 60     52.6%    36.1%    38.7%
az 240 el  5      2.8%     0.0%     0.0%
az 240 el 15     10.2%     0.0%     3.8%
az 240 el 30     23.6%     7.3%    15.5%
az 240 el 60     52.4%    35.6%    37.7%

azimuth 120  TRUE mean 54.5 deg | SHEET 63.7 (err 16.55, bias +9.27) | REF 60.6 (err 15.90)
azimuth 240  TRUE mean 53.5 deg | SHEET 64.9 (err 17.07, bias +11.40) | REF 61.6 (err 16.43)
```

So the viewer note line's **"0.0% lit at a 15-degree sun"** on this chunk is wrong, and by a
measurable amount: **10.2% of the chunk's texels should be lit at azimuth 240, elevation 15.** The
in-bake reference says 3.8% -- closer, and still wrong, because it shares the lattice and the
near-skip even though it does not share the mip. Balanced agreement with the third witness at that
sun is **50.0% for the sheet and 62.0% for the reference**: the 97% floor HORIZON1 scored the sheet
against was a comparison between two witnesses that are 11.4 and 8.1 degrees off the truth in the
same direction.

### 2.3 The six candidates the brief named, each with the row that kills or keeps it

**(a) Terrain outside the chunk / outside the loaded cells treated as occluding.** REFUTED.
`variants.py` runs the march over a terrain-only lattice and compares it to the third witness's
terrain-only pencil: mean 5.94 deg, and the two means are 10.09 (march) against 10.86 (truth) --
the march reads terrain *lower* than the truth on average, not higher. The lattice rectangle is
70x70 cells filled from the whole worldspace, and a square with no LAND holds the NONE sentinel,
which `top > SENTINEL_TEST` rejects. Trimming the reach to 16,384 u changes the answer by 0.00 deg
and to 4,096 u by 0.03 deg (V10, V11), so nothing beyond the loaded data is contributing at all.

**(b) The max-Z lattice's mip choice in `maxAlong`.** KEPT, as the junior partner. Forcing level 0
everywhere (V1) moves the bias from +8.52 to +7.85 -- real, and small beside the tap. It matters
because the two are coupled: the level is chosen so that `2c <= binWidthFraction * d` precisely so
the 2x2 tap fills the bin. With a 1x1 tap the mip is worth another 0.34 deg (V2 +2.17 -> V4 +1.83).

**(c) The receiver's own square / the tangent-plane rule.** REFUTED, by my own control.
`probe3.py` builds a flat world at z = 0 with a single square raised to 1,500 u and casts from the
origin: the bins pointing AWAY from that square read exactly **0.0**, at every distance from 128 to
4,096 u. A march that read its own receiver square could not do that. The near-skip sweep agrees --
0.0 / 1.0 / 4.0 cells move the bias by 0.1 deg (V7, V8).

**(d) The reach of 127,561 u reading beyond the data.** REFUTED. See (a): 127,561 -> 16,384 u is
identical to two decimals, 127,561 -> 4,096 u costs 0.03 deg. Whatever is making the skyline steep
is inside one cell of the receiver.

**(e) Unit or axis confusion.** REFUTED. Rotating the stored bins against the third witness by -2,
-1, 0, +1, +2 gives mean errors 14.22 / 11.39 / **8.51** / 11.76 / 14.35, and mirroring the bin
order gives 15.75. Bin 0 is where the witness's bin 0 is -- north, clockwise toward east -- and no
other alignment is close. (One unit bug WAS found, in my own reader, and it is section 1.2.)

**(f) The placements' boxes inflated.** REFUTED, in the direction that matters. The third witness's
boxes are `.lodo` AABBs, which are *larger* than the meshes inside them, so the witness over-states
the skyline and the sheet's excess over it is a lower bound on the defect. Against the bake's own
object height field: `lattice.py` builds a lattice from the exact `.lodi` boxes rasterised at 128 u
and marches it -- **6.49 deg** against the bake's own `sky` lattice at **7.33 deg**. So
`LodgenObjectHeightField` is within 0.84 deg of an exact box rasterisation; it is not inflating
anything.

**(g) The SLAB wall/ceiling law, which I expected to be the answer.** REFUTED by measurement. The
horizon march reads only `topAt` (the MAX plane) and never `spanAt`'s MIN plane, so an elevated deck
whose lowest surface stands 1,000 u above the receiver is entered as a wall from the ground up --
exactly the defect lane SLAB1 fixed for the AO march. The object field's dumped MIN plane says the
opportunity is real (span max-min over occupied squares: p50 150 u, p75 616 u, p95 1,601 u).
Applying SLAB1's law to the horizon anyway -- a square occludes only where its lowest object surface
reaches down to the receiver -- makes the answer **worse, 7.33 -> 10.90 deg**. Ceilings in this
chunk are mostly the tops of buildings the receiver genuinely cannot see past, and dropping them
lets light through walls. The hypothesis is dead and is written up in MISTAKES (section 11).

### 2.4 Where the rest of the error lives, since the fix does not remove all of it

`lattice.py` splits the chain at the lattice: it runs the third witness's own 1-degree pencil,
32 u step, one square per sample, no tap, no mip, no growth, no near-skip, over each candidate field.
Whatever that pencil reads is the **ceiling on any march over that field**:

```
  W_sky     the bake's own lattice (LAND + object field MAX plane)        7.33 deg
  W_boxlat  LAND lattice + the .lodi boxes rasterised at 128 u            6.49 deg
  W_slab    the bake's lattice with SLAB1's wall/ceiling law             10.90 deg
  W_land    LAND only, no objects at all                                 41.10 deg
  the SHIPPED sheet, same truth                                           8.51 deg
```

So of the sheet's 8.51 deg against the sector truth, **7.33 comes from the 128 u lattice** and only
1.18 from the march rule. Against the *directional* truth the same split puts the tap-and-mip
dilation at 6.4 deg of bias and leaves the lattice holding the rest. **A 128 u max-Z square holds
the top of everything over it**, so a building's silhouette is dilated by up to a square of ground
plan in every direction; seen from the 250 u that a street is wide, that is tens of degrees of
azimuth. The brief's step-4 gate -- *the ten receivers within 2 deg of the third witness in every
bin* -- is **not reachable on this lattice**, and this lane says so with the number rather than
chasing it: the floor is 7.33 deg and the fix below reaches 8.14.

### 2.5 The fix, measured before a line of C++ was touched

`candidate.py` re-runs the march with three changes and nothing else:

1. `maxAlong` reads **one square per tap** -- the square the sample lands in -- instead of a 2x2
   block, with taps spaced half a cell instead of a whole one so nothing is stepped over.
2. `wantCell` comes from **the segment and the tap budget** (`(dEnd - d) / 64`) instead of from the
   azimuth bin's width, so the mip stays fine enough to be a line sample and the tap count stays
   bounded at long range.
3. `growth` 1.5 -> **1.3**, because the elevation is measured at the segment's NEAR end and a x1.5
   segment can over-state `tan(elevation)` by half.

```
ten receivers x 16 bins, against the DIRECTIONAL third witness
  shipped                          mean 11.83   max 74.22   bias  +8.52
  changes 1+2                      mean  8.52   max 64.87   bias  +2.74
  changes 1+2+3 (growth 1.30)      mean  8.14   max 67.89   bias  +0.86
  (for scale: growth 1.20 gives 7.79 / -0.46 at 46 steps a bin instead of 32)

576 texels, the lit question
sun               TRUE    SHEET      REF     FIXED     balanced agreement with TRUE
az 120 el 15     10.8%     1.2%     5.6%      5.4%     sheet 55.6%  ->  fixed 70.5%
az 120 el 30     22.7%    12.3%    17.7%     20.7%     sheet 70.2%  ->  fixed 77.1%
az 120 el 60     52.6%    36.1%    38.7%     47.2%     sheet 75.3%  ->  fixed 75.4%
az 240 el 15     10.2%     0.0%     3.8%      3.3%     sheet 50.0%  ->  fixed 59.5%
az 240 el 30     23.6%     7.3%    15.5%     17.7%     sheet 63.5%  ->  fixed 73.5%
az 240 el 60     52.4%    35.6%    37.7%     49.7%     sheet 71.8%  ->  fixed 73.0%

azimuth 120 mean elevation: TRUE 54.5 | sheet 63.7 (bias +9.27) -> fixed 56.8 (bias +2.31)
azimuth 240 mean elevation: TRUE 53.5 | sheet 64.9 (bias +11.40) -> fixed 56.6 (bias +3.16)
```

The systematic bias is cut by about three quarters and the chunk stops being uniformly dark at a low
sun. What remains is the lattice, quantified in 2.4, and it is the next lane's work, not this one's.



## 3. The fix

**One source file: `src/lodghorizon.h`.** Nothing else in `src/` was touched. The file is
UNTRACKED in git (lane HORIZON1 created it and nothing in this tree is committed), so there is no
`git diff` to quote; the patch is `scratchpad/horizon2_20260918/patch_horizon.py`, five
unique-match byte splices, and it reported `line endings before: CRLF 0 of 521 LF` ->
`after: CRLF 0 of 558 LF`, `bytes 22649 -> 25134`. A sixth, hand-applied splice moved the new
constant above its first use (below).

Three functional changes, each one measured in section 2 BEFORE it was written:

**(1) `LodgenHorizonField::maxAlong` reads ONE SQUARE a tap, not a 2x2 block** -- `src/lodghorizon.h`
lines 274-307.

```
-		const int gx = int( std::floor( ( px - ox ) / c - 0.5f ) );
-		const int gy = int( std::floor( ( py - oy ) / c - 0.5f ) );
-		for ( int j = 0; j < 2; j++ )
-			for ( int i = 0; i < 2; i++ ) {
-				const float v = at( level, gx + i, gy + j );
+		const int gx = int( std::floor( ( px - ox ) / c ) );
+		const int gy = int( std::floor( ( py - oy ) / c ) );
+		const float v = at( level, gx, gy );
```

A 2x2 block is a `2c`-wide dilation of every occluder on the segment. To keep the guarantee that
nothing is stepped over, the taps are now spaced HALF a square instead of a whole one
(`int taps = int( 2.0f * len / c )`, line 293) -- a segment that crosses a square enters and leaves
it more than half a square apart, so at least one tap lands inside it. The tap cap moved with it:
`if ( taps > 2 * LODGEN_HORIZON_MAX_TAPS ) taps = 2 * LODGEN_HORIZON_MAX_TAPS;`.

**(2) `lodgenHorizonCastAt` picks the mip level from the SEGMENT, not from the AZIMUTH BIN** --
line 527.

```
-			const float wantCell = std::max( k.binWidthFraction * d, 1.0f );
+			const float wantCell = std::max( ( dEnd - d ) / float( LODGEN_HORIZON_MAX_TAPS ), 1.0f );
```

This is the cause named in section 2. `binWidthFraction * d` is 0.390 d at A = 16, i.e. exactly the
bin's own width, so the level was chosen so that the footprint equalled the whole 22.5-degree
sector and the stored byte was a maximum over that sector. The viewer
(`src/btdterrain.cpp`, `BtdTerrain`) takes the sun's azimuth, finds the two bins either side of it
and BLENDS them, which is only meaningful if each stored byte is the skyline in ITS OWN direction.
The level now only has to keep the segment inside the tap budget.

**(3) `LodgenHorizonCast::growth` 1.5 -> 1.3** -- line 338. The elevation is taken at the segment's
NEAR end, so a segment whose occluder stands at its FAR end over-states `tan(elevation)` by up to
`growth`. `candidate.py` swept it on the fixed rule: 1.50 -> mean 8.52 bias +2.74 (21 steps),
**1.30 -> mean 8.14 bias +0.86 (32 steps)**, 1.20 -> 7.79 / -0.46 (46 steps), 1.10 -> 8.22 / -1.79
(87 steps). 1.30 is the knee: it is the coarsest ladder whose residual bias is under a degree, and
going finer buys 0.35 deg of mean for 44% more steps while overshooting the bias to the other side.

**New constant:** `constexpr int LODGEN_HORIZON_MAX_TAPS = 64;`, line 104. It replaces the bare
`64` the old code used as its tap cap, because it is now load-bearing twice -- it bounds the walk
AND it sets the mip level. It is declared immediately above `struct LodgenHorizonField` because
`maxAlong` uses it; the patch script first placed it next to `LODGEN_HORIZON_NEAR_CELL` at line 317,
which is BELOW the struct, and the build failed with
`src/lodghorizon.h:287:33: error: 'LODGEN_HORIZON_MAX_TAPS' was not declared in this scope`
(`scratchpad/horizon2_20260918/build2.log`). That is build 2 of 3 and it is in the log.

**`binWidthFraction` is left in the struct** (line 344). It is still set by `src/lodgen.cpp:11002`,
`src/nativeemit.cpp:2048` and `src/lodghorizonrefute.h:584`, and still read by the refuter's own
bound at `src/lodghorizonrefute.h:616`. The march no longer reads it. Removing it would change
three call sites and the refuter's arithmetic in the same landing as the fix, which is the thing
the brief says not to do; it is named here as owed cleanup.

**The doc block at the top of the file was rewritten** (lines 50-82) to say what the rule is now and,
explicitly, what it used to be and why that was wrong -- including the sentence the old block
carried, that the wide footprint "errs toward MORE occlusion, never less, which is the safe side
for a shadow", with the measured price of that safety next to it.

### 3.1 The object stream moves too, and it was not optional

`lodgenHorizonCastAt` is ONE entry point with TWO callers: the terrain sheet
(`src/lodgen.cpp:11002`) and the per-placement object sky/AO cast (`src/nativeemit.cpp:2048`).
There is no way to fix the terrain sheet's march without moving the object stream's numbers, and
this lane did not try to fork it -- a second copy of a cast is the defect this lane exists to
clean up, not a fix. Section 5 carries the object refuter re-run and the before/after numbers.

### 3.2 Build

Three builds; the first two are failures and both are in the lane folder.

- **build 1** (`build1.log`): `mingw32-make: command not found`, `make rc=127`. The PATH in the
  brief (`/ucrt64/bin`) is an MSYS2-shell path; this lane's Bash tool is Git-Bash, where MSYS2's
  root is `/c/msys64`. Corrected to `export PATH=/c/msys64/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`,
  which is what `scratchpad/horizon1_20260918/lane_horizon1_report.md:1034` uses.
- **build 1, second attempt**: `mingw32-make: Nothing to be done for 'first'.` and **`make rc=0`**.
  Make's exit code is the gate the brief names, and it said 0 on a build that compiled nothing:
  `Makefile.Release` does not name `src/lodghorizon.h` as a dependency of ANY object
  (`grep -c lodghorizon Makefile.Release` -> 0), because the Makefile was generated by qmake before
  that header existed. `stat release/NifSkope.exe` was unchanged at 21:59:46. Repaired by deleting
  the five objects of the translation units that include it and re-running make. MISTAKES entry
  written (section 11).
- **build 2** (`build2.log`): the declaration-order error above, `make rc=2`, 5 `error:` lines.
- **build 3** (`build3.log`): `make rc=0`, zero `error:` lines, exe relinked.


## 4. The proof

Every number below is from a named log in `scratchpad/horizon2_20260918/`.

### 4.0 The re-bake, and what it moved

`bash scratchpad/horizon1_20260918/bake.sh <exe> <out> --horizon-refute 4100` -- the HORIZON1 recipe
verbatim, one script, three callers -- run on the fixed exe at 2026-09-18 23:49:22, `bake rc=0 50s`
(`scratchpad/horizon2_20260918/v8/bake.log`, copied to `refute.log` the way HORIZON1 copied its own).
The way back ran second, `--lodi-v7 --no-terrain-horizon`, `bake rc=0 26s`.

**The way back is unchanged.** Every one of the 23 payload files of `wayback/` is byte-identical to
HORIZON1's `wayback/`. The 24th file, the bake record `Commonwealth.lodb`, differs in 33 bytes and
they are the run's TIMESTAMP and its own output paths (`horizon1_20260918` -> `horizon2_20260918`)
and nothing else -- checked byte by byte, not by eye. The gate's own G1 says the same thing against
the RUNG exe's bake: `.lodi` and `.lodo` byte-identical, `--no-terrain-horizon` 3 containers
compared 0 differ, and the old reader still refuses v8 by name.

**Of the 24 files the main bake writes, exactly three differ from HORIZON1's:** the terrain sheet
`Commonwealth.VT.4.lodt`, the object file `Commonwealth.lodi`, and the bake record. Everything
else -- the `.lodo`, the `.BTR`-side containers, every other level of the pyramid -- is byte for
byte what it was.

### 4.1 The census, before and after

From the two `bake.log` census lines, `horizon*` tokens only:

```
                       HORIZON1 (shipped)      HORIZON2 (fixed)
horizonSteps                   22                     33          growth 1.5 -> 1.3
horizonMeanElev             63.22                  55.02          degrees, all texels x all bins
horizonZeroBins                 0                    776          bins that are EXACTLY zero
horizonNearSkip              1.00                   1.00          unchanged
horizonTexels              278784                 278784          unchanged
horizonLatticeSquare          128                    128          unchanged
```

`horizonZeroBins` moving off 0 is the first sign in the census that the march stopped filling every
bin with something: 776 of 4,460,544 bins now see clear sky, where before not one did.

### 4.2 G3, the in-bake refuter -- what moved and what it is worth

```
                         HORIZON1    HORIZON2
horizonRefuteWorst          51.69%      89.01%   worst of the eight suns, balanced
horizonRefuteA120E5        100.00%      99.98%
horizonRefuteA120E15        75.65%      94.16%
horizonRefuteA120E30        91.40%      95.01%
horizonRefuteA120E60        94.60%      93.79%
horizonRefuteA240E5        100.00%     100.00%
horizonRefuteA240E15        51.69%      89.01%   <- the number the brief opened on
horizonRefuteA240E30        83.76%      93.66%
horizonRefuteA240E60        93.12%      94.57%
horizonRefuteControlWorst   50.00%      50.79%   the 90-degree control stays RED
horizonRefuteMeanErrDeg      3.651       4.113
horizonRefuteMeanRefDeg     63.565      58.995   the REFERENCE's own mean
horizonRefuteMeanRefPencilDeg 60.041     54.226   the REFERENCE's own pencil mean
```

**51.69% -> 89.01%, and the floor is 97%, so G3 is still RED.** It is red for two reasons and both
are stated rather than argued away.

*The first is that this score does not measure what the brief thought it measured.* Look at the last
two rows: **the reference moved too**, by 4.6 and 5.8 degrees. It moved because
`lodgenHorizonReferenceElev` calls `LodgenHorizonField::maxAlong` -- `src/lodghorizonrefute.h:107`
and `:112`, `wantCell` 1.0, mip 0 -- which is the very function this lane changed. The in-bake
reference is not independent of the march and never was. That is the MISTAKES entry this lane owes
(section 11), and it is why the brief was right to ask for a third witness before asking for a
number: a 97% floor between two witnesses that share a function is worth nothing, and this is
exactly how a sheet that called a downtown chunk 0.0% lit passed for a day.

*The second is that the reference's move is TOWARD the third witness.* Its pencil mean is now 54.226
degrees; the third witness's mean over 576 real texels of the same chunk is 54.5 at azimuth 120 and
53.5 at azimuth 240 (`lit_out.txt`, section 1). Before the fix it read 60.041 against those same
numbers. So the change made the in-bake reference a better instrument as well as the sheet, which is
what one expects when the defect was in a function they share -- and it is the reason G3's score can
rise 37 points while its own floor stays out of reach.

### 4.3 G6, the third witness -- the new check, and the red control

The check the brief asked for is now in the tree as `tests/spells/lodgen_horizon_witness.py` with its
fixture `tests/spells/lodgen_horizon_witness.json` (10 receivers x 16 bins, the skylines computed
from the BTD heightmap and 2,449 exact placement boxes, no line of `src/lodghorizon.h` involved),
wired into `tests/spells/lodgen_horizon.sh` as **G6**.

```
NEW sheet   10 receivers x 16 bins = 160; mean |sheet - witness|  8.15 deg (bar 9.00);
            bias +0.87 deg (bar +-2.00); over 2 deg 78.8%; worst 67.7 deg at FOOT2 bin 4   PASS
OLD sheet   10 receivers x 16 bins = 160; mean |sheet - witness| 11.90 deg (bar 9.00);
            bias +8.59 deg (bar +-2.00); over 2 deg 95.0%; worst 74.3 deg at FOOT2 bin 3   FAIL (the red control)
```

**The brief's "within 2 degrees in every bin" gate is not reachable and this lane says so rather
than quietly scoring something else.** `lattice_out.txt` measured the ceiling before any rule was
changed: the same 1-degree pencil run over the 128-unit lattice the march reads -- one square a
sample, no mip, no tap, no growth, the best any march over that field could do -- still misses the
third witness by **7.33 degrees** in the mean. The lattice owns that error, not the march. The
shipped sheet is at 8.15, so the march's own remaining contribution is 0.8 degrees. G6's bars are
therefore set at 9.0 degrees of mean error and, the one that bites, **2.0 degrees of signed BIAS** --
because a lattice of maxima read by a pencil is noisy in BOTH directions, while a march that dilates
every occluder to the width of its own bin is wrong in ONE, always up, always more shadow. That is
the shape of defect G6 catches, and the pre-fix sheet fails it on the bias alone.

G6 also carries a fixture-rot check: the witness's mean skyline must stand more than 20 degrees
above its own terrain-only column, or the placements have fallen out of the fixture and it is
scoring bare ground.

### 4.4 The control the C++ owes: it implements the rule that was measured

`candidate.py` measured the candidate rule in Python over 576 real texels before a line of C++ was
written, and `control13.py` re-ran it with BOTH changes at the shipped knobs (footprint AND growth
1.3, which `candidate.py` itself had left at 1.5 while its own sweep was still choosing):

```
all 576 texels x 16 bins: |C++ - python| mean 0.244 deg  max 21.746  over one stored step 2.3%
                          (one stored step is 0.353 deg)
  azimuth 120: python mean 56.78  C++ mean 56.79  |diff| mean 0.079  max 2.528
  azimuth 240: python mean 56.63  C++ mean 56.63  |diff| mean 0.071  max 0.176
```

and on the ten receivers the prediction was `mean 8.14 bias +0.86`; the shipped bake measures
`8.15 / +0.87`. The residual per-bin maximum of 21.7 degrees is my Python's object lattice, which is
a reconstruction of `LodgenObjectHeightField` from `--dump-object-ao`, not the bake's own array; it
affects 2.3% of bins and none of the aggregates. What this control rules out is the thing that
actually goes wrong between a measured proposal and a shipped diff: that the C++ does something
adjacent to the rule that was scored.

### 4.5 The lit question, the one the gate really asks

576 texels of chunk 4.4.-12, "is this texel lit by a sun at (azimuth, elevation)", put to the third
witness and to each sheet (`after_out.txt`; the sheet is sampled the way `BtdTerrain` samples it,
the two bins either side of the sun blended):

```
sun              TRUE     OLD     REF     NEW     balanced agreement with the third witness
az 120 el  5      3.8%    0.0%    0.0%    0.2%     old  50.0%   ref 50.0%   NEW  52.3%
az 120 el 15     10.8%    1.2%    5.6%    5.4%     old  55.6%   ref 68.6%   NEW  70.5%
az 120 el 30     22.7%   12.3%   17.7%   20.7%     old  70.2%   ref 75.6%   NEW  77.1%
az 120 el 60     52.6%   36.1%   38.7%   47.2%     old  75.3%   ref 75.7%   NEW  75.4%
az 240 el  5      2.8%    0.0%    0.0%    0.0%     old  50.0%   ref 50.0%   NEW  50.0%
az 240 el 15     10.2%    0.0%    3.8%    3.3%     old  50.0%   ref 62.0%   NEW  59.5%
az 240 el 30     23.6%    7.3%   15.5%   17.5%     old  63.5%   ref 70.7%   NEW  73.7%
az 240 el 60     52.4%   35.6%   37.7%   49.5%     old  71.8%   ref 71.3%   NEW  72.8%

az 120 mean elevation: TRUE 54.5 | OLD 63.7 (err 16.55, bias  +9.27) | NEW 56.8 (err 15.61, bias +2.33)
az 240 mean elevation: TRUE 53.5 | OLD 64.9 (err 17.07, bias +11.40) | NEW 56.6 (err 15.04, bias +3.16)
```

The systematic bias is cut by about three quarters at both azimuths. **Against the third witness the
worst balanced agreement is still 50.0%, at elevation 5, before AND after**, and that number is not
a defect the fix failed to reach: at a 5-degree sun the witness itself says only 2.8-3.8% of the
chunk is lit, and a balanced score at a 3% base rate is a coin flip for any field that says
"shadowed". A gate that scores elevation 5 on this chunk is scoring nothing; that belongs in G6's
successor, not in a claim here.

### 4.6 The viewer, on the chunk the brief named

`WW_LODL_CHANNEL=horizon`, chunk 4.4.-12, from the gate's own shots
(`scratchpad/horizon2_20260918/gate_horizon/hz_e15_a120.log`, `hz_e05_a240.log`):

```
sun 240,15  BEFORE  terrain horizon 14.47..87.18 deg, mean 64.37, 0.0% lit   <- the brief's line
sun 240,15  AFTER   terrain horizon  8.91..86.82 deg, mean 56.95, 2.7% lit
sun 120,15  BEFORE  terrain horizon 11.41..87.18 deg, mean 63.85, 0.9% lit
sun 120,15  AFTER   terrain horizon  4.32..86.82 deg, mean 56.44, 5.3% lit
sun 240,05  AFTER   terrain horizon  8.91..86.82 deg, mean 56.95, 0.0% lit
```

The BEFORE rows are read out of lane HORIZON1's OWN render logs, copied into
`images/before/chunk_horizon_close_e15_a{240,120}.log`, and the AFTER rows out of this lane's
`images/chunk_horizon_close_e15_a{240,120}.log` -- the same camera, the same scene, the same
channel. The mean differs between the two azimuths because the note line reports the blended
elevation AT THE SUN'S AZIMUTH, which is the whole point of the bins.

The brief's step-4 requirement -- the viewer note line for this chunk showing terrain lit above 0%
-- is met at the sun the brief quoted (**0.0% -> 2.7%** at 240,15) and at the other azimuth
(0.9% -> 5.3% at 120,15). At azimuth 240 elevation **5** it is still 0.0%, and the third witness
agrees that almost nothing is lit there (2.8%), so that one is not evidence of the old defect.

### 4.7 The object stream moved, because the cast is shared

`objmove.py` decodes both `.lodi` with `tests/spells/lodgen_native_decode.py` and subtracts:

```
vertexHorizon    854336 bytes  changed 18.07%  mean +74.92 -> +70.58  |delta| mean 4.39 max 252  signed -4.34
vertexSky         53396 bytes  changed  0.00%  mean +119.23 -> +119.23
vertexAo          53396 bytes  changed  0.00%  mean +177.07 -> +177.07
placement byte sky              changed  0.00%  mean +129.37 -> +129.37
placement byte ao               changed  0.00%  mean +183.66 -> +183.66
```

Exactly one stream moved: the per-vertex 16-bin horizon, 18.07% of its bytes, mean 74.92 -> 70.58
bytes = 26.44 -> 24.91 degrees, signed -1.53 degrees. The per-vertex sky, the per-vertex AO and both
placement bytes are byte-identical, which is the answer one wants: the shared function is
`lodgenHorizonCastAt` and nothing else leaked.

**Its refuter was re-run in the same bake and it went DOWN.**

```
                        HORIZON1   HORIZON2
vhorRefuteWorst           97.73%     95.96%   <- below the 97% floor: RED, and new
vhorRefuteA120E60         97.73%     95.96%   the only position that carries the worst
vhorRefuteMeanErrDeg       0.673      0.816
vhorRefuteMeanRefDeg      18.875     17.738   the reference moved here too
vhorRefuteControlDirWorst 60.59%     59.81%   the control stays RED
```

So the object stream's agreement with its own in-bake reference falls 1.77 points at one of eight
suns and crosses the pre-registered floor. **That is left red with its number, not re-tuned.** Two
things about it, both measured rather than asserted. The reference here moved as well (18.875 ->
17.738), for the same shared-function reason as the terrain's, so this is again two instruments
moving relative to each other rather than a measured loss of accuracy. And the object stream has no
third witness in this lane: `lodgen_horizon_witness.json` covers the terrain sheet only. Building the
raw-input witness for 53,396 LOD-mesh vertices is the honest way to settle whether the object stream
got better or worse, and it is not this lane's work -- it is named in section 10 as owed.

## 5. The neighbours

All on the final exe (23:47:33, sha1 `b349f807…`), Fallout4 checked DOWN as its
own command before each batch.

| harness | standing | this lane | log |
|---|---|---|---|
| `lodgen_horizon.sh` | 22 ok / 1 failed | **24 ok / 2 failed / 0 skipped** | `gate_horizon_after2.log` |
| `lodi_v7.sh` | 12 / 0 | **12 ok, 0 failed, 0 skipped** | `nb_lodi_v7.log` |
| `lodl_channels.sh` | PASS | **54 checks, 0 failures -- PASS** | `nb_lodl_channels.log` |
| `lodgen_slab.sh` | 16 / 0 | **16 checks, 0 failures -- RESULT PASS** | `nb_lodgen_slab.log` |
| `native_open.sh` | 17 / 0 / 2 | **17 checks, 0 failures, 2 skipped -- PASS** | `nb_native_open.log` |
| `render_shot.sh` | 82 / 0 | **82 checks, 0 failures -- PASS** | `nb_render_shot.log` |
| `lodl_open.sh` | 23 / 0 | **23 checks, 0 failures -- PASS** | `nb_lodl_open.log` |
| `lodgen_native.sh` | PASS | **RESULT PASS** (rc=0) | `nb_lodgen_native.log` |

Seven of the eight are exactly their standing counts. The eighth is the lane's
own gate and it is the one that moved.

### 5.1 `lodgen_horizon.sh` 22/1 -> 24/2, and the brief asked for 23+/0

G6 adds the two ok lines and the red control:

```
ok   G6 the sheet stands up to the raw-input witness -- 10 receivers x 16 bins = 160;
     mean |sheet - witness| 8.15 deg (bar 9.00); bias +0.87 deg (bar +-2.00);
     over 2 deg 78.8%; worst 67.7 deg at FOOT2 bin 4
red  G6 control: the PRE-FIX sheet  FAIL -- mean 11.90 > 9.00; bias +8.59 outside +-2.00 (EXPECTED)
ok   G6 the fixture still carries the placements -- witness mean 43.34 deg, terrain-only mean 10.86 deg
```

The two failures are the two G3 lines, and they are the reds this lane chose to
leave standing with their numbers rather than re-tune (section 4.2):

```
FAIL G3 horizonRefute agrees with the reference cast -- worst of the eight sun positions
     89.01% over 4161 samples, floor 97%            (51.69% before this lane)
FAIL G3 vhorRefute agrees with the reference cast -- worst of the eight sun positions
     95.96% over 2449 samples, floor 97%            (97.73% before this lane)
```

Both controls are still red, which is what makes those numbers readable at all:
`horizonRefute` control 50.70%, `vhorRefute` control 59.81%.

So the brief's target -- **23+ ok and 0 failed** -- is met on the ok side and
NOT on the failed side, and the honest reading is in 4.2: G3's reference calls
`LodgenHorizonField::maxAlong` (`src/lodghorizonrefute.h:107,112`), so it is not
a floor between the march and the truth, it is a floor between the march and
itself. Moving it to 97% is a matter of making the two agree, which this lane
can do at any time by tuning growth, and which would mean nothing. What the
brief wanted from that number -- evidence the sheet is right -- is in G6 and in
the lit ground of section 7, both of which are new and both of which can fail.

### 5.2 Two neighbours FAILED inside G5 and PASS standing alone -- the cause, measured

The first full run reported `lodl_channels.sh rc=1` and `native_open.sh rc=1`.
Neither is a regression, and it is not a matter of opinion:

| run | `lodl_channels.sh` | `native_open.sh` |
|---|---|---|
| inside G5 (the gate's env) | rc=1 | rc=1 |
| standalone, gate variables unset, private `OUT` | **rc=0, 54/0** | **rc=0, 17/0/2** |
| standalone, the gate's `OUT` + `SHEETS`/`V8DIR`/`V8VT`/`WAYBACK`/`REFLOG` re-exported | **rc=1, 54 checks 3 failures** | **rc=1, 17 checks 3 failures** |

Same exe, same working tree, three runs: the failure follows the ENVIRONMENT,
not the build. The failing checks name the cause themselves --

```
FAIL (a) mask-a: the note line says ABSENT by name -- ... terrain mask-a from the MASK SHEET'S A ...
FAIL (b) mask-a: absent, so its render is IDENTICAL to the default (380174 px differ, must be 0)
FAIL the lit terrain looks like the .BTR of the SAME cells (NCC 0.1387 >= 0.45)
```

`lodl_channels.sh` expects its OWN fixture, one with no mask sheet, and `G5`
hands it `SHEETS=` this lane's v8 containers, which do carry one; `native_open.sh`
is handed this lane's bake in place of the one it builds. `G5` exports `OUT`,
`SHEETS`, `V8DIR`, `V8VT`, `WAYBACK` and `REFLOG` to every neighbour it runs, and
those are the names the neighbours read for their own fixtures.

The same export does something worse and quieter: the neighbours use `OUT` as
their own working directory, so running G5 **deletes the gate's own evidence** --
`witness.log`, `witness_control.log`, `hz_*.png`, every per-neighbour log the
loop redirects into `$OUT/<script>.log`. That is why the G5 summary lines in
`gate_horizon_after2.log` carry an rc and an empty tail. The numbers in the
table above therefore come from the standalone re-runs, each with its own `OUT`,
and every one of them is named beside its row.

**Owed, named, not fixed here** (it is another lane's file and outside this
lane's one-file scope): G5 should run each neighbour with `env -u` on the
fixture names and a per-neighbour `OUT`, or it will keep reporting other
people's harnesses as broken and eating its own evidence while doing it.

## 6. The build

Every timestamp here is from `date` or `stat` in the shell that ran the command.
Fallout4 was re-checked as its own command before each of the three builds and
before every exe run in this lane; it was DOWN every time
(`tasklist | grep -i -E "Fallout4|NifSkope"` -> no `Fallout4.exe` row).

**The exe that ships this lane** -- `release/NifSkope.exe`:

| | |
|---|---|
| mtime | **2026-09-18 23:47:33** (+0200) |
| size | **22,949,376 bytes** |
| sha1 | **`b349f807426be700ed2ff9b54ee23e4fab3ba037`** |

**The rung**, taken once before the first build and never deleted --
`release/NifSkope.before_horizon2.exe`, 2026-09-18 22:57, 22,952,448 bytes, sha1
`e578b76f9d7a2d011363e4300a2c94967e14d605`: byte-for-byte the exe this lane was
handed (section 0). No `release/NifSkope.before_*.exe`,
`NifSkope.archlock1_rung.exe`, `NifSkope.at_0117.exe` or `NifSkope_inuse_*.exe`
was deleted or renamed by this lane, and no NifSkope had to be renamed aside at
link time -- there was no window holding the exe at any of the three link steps.

**`find src tests res -newer release/NifSkope.exe`** at 2026-09-19 00:08 returns
four paths and they are all test files, not sources:

```
tests/spells                            (the directory's mtime)
tests/spells/lodgen_horizon.sh          the gate, +G6
tests/spells/lodgen_horizon_witness.json the frozen fixture
tests/spells/lodgen_horizon_witness.py   the checker
```

Nothing under `src/` and nothing under `res/` is newer than the exe. The three
test files are interpreted at run time by `bash` and `python`; none of them is
compiled into anything, so the exe is still the exact build of the sources as
they stand.

**The object-vs-header check** (`nifskope-ww-build-verify`, the LODIV7 lesson):
`src/lodghorizon.h` is the header this lane edited, at **23:46:09**. Every `.cpp`
that includes it has an object NEWER than that:

```
btdterrain.o   23:46:30      lodgen.o      23:47:04
lodinative.o   23:46:30      nativeemit.o  23:46:39
nifcli.o       23:46:48      src/lodghorizon.h  23:46:09
```

Five objects, five includers, none stale. That check is not decoration in this
lane: **the first build of this lane was a stale-object build of the worst kind**
and `find -newer` could not have seen it. `grep -c lodghorizon Makefile.Release`
returns **0** -- qmake froze that Makefile's dependency lists before the header
existed -- so `mingw32-make -f Makefile.Release -j8` printed `Nothing to be done
for 'first'.` and **exited 0** while the exe stayed at 21:59:46
(`scratchpad/horizon2_20260918/build1.log`, 57 bytes, which is how small a build
log gets when nothing is built). The exe's mtime is the second half of the gate,
and a MISTAKES entry is written for it (section 11). A `qmake` run is owed --
this hand repair does not survive one, and the next lane that edits
`src/lodghorizon.h` will hit the same silence.

**The three builds**, all `mingw32-make -f Makefile.Release -j8` under
`PATH=/c/msys64/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`:

| # | log | make rc | outcome |
|---|---|---|---|
| 1 | `build1.log` | **0** | `Nothing to be done for 'first'.` -- compiled nothing, exe unmoved. Not a build. |
| 2 | `build2.log` | **2** | 5 error lines: `src/lodghorizon.h:287:33: error: 'LODGEN_HORIZON_MAX_TAPS' was not declared in this scope` -- the constant had been placed below the struct that uses it. |
| 3 | `build3.log` | **0** | 18,870 bytes; compiled exactly `btdterrain`, `lodgen`, `lodinative`, `nativeemit`, `nifcli` and linked. Exe 21:59:46 -> **23:47:33**. |

Build 3's log names five translation units and no others, which is also the
check that the shipped exe is the handed exe plus this lane's diff and nothing
else.

**One source file changed**, `src/lodghorizon.h`, 25,134 bytes, CRLF 0 of 558 LF
(Python byte count, before and after). It is untracked -- HORIZON1 created it and
nothing in this tree is committed -- so there is no `git diff` to quote; the
three functional edits are quoted in full in section 3. Nothing was committed,
nothing was stashed, and `WW_CHANGES.md` and `HANDOFF.md` were not touched
(section 9 hands the director their text instead).

## 7. The pictures, and their captions

All in `scratchpad/horizon2_20260918/images/`. Every render is a headless
`--port 12091` run with `WW_WINDOW_AT=1960,40`, one at a time, every path
absolute, the scene passed positionally, and `tasklist | grep -i -E
"Fallout4|NifSkope"` checked as its own command before the batch and again
inside the script before every single shot (`shots.sh`, this lane's copy of
HORIZON1's with three lines changed: `LANE`, `PORT`, and the object/sheet
sources). 39 renders, all `exit=0`, 01:02:25 -> 01:06:37 on 2026-09-19.

**The BEFORE panels are HORIZON1's own renders**, copied byte-for-byte into
`images/before/` with their logs. They are not re-renders of the old bytes by
the new exe: that would differ in two ways at once.

**Every caption number is read out of the render's own log** by `pairs.py`
(the viewer's note line for the role-7 sheet), not typed in, so a caption cannot
drift from its picture. The numbers in these captions are the numbers in
sections 4.5 and 4.6.

### 7.1 The twelve framings, BEFORE beside AFTER

`before_after_{close,full}_e{05,15,30}_a{120,240}.png`, 2824x1204 each, two
panels on one page, same camera in both. The caption under each panel is its own
log's line; the page's numbers:

| page | BEFORE mean / lit | AFTER mean / lit |
|---|---|---|
| `_close_e05_a120` / `_full_e05_a120` | 63.85 deg / 0.0% | 56.44 deg / 0.0% |
| `_close_e15_a120` / `_full_e15_a120` | 63.85 deg / **0.9%** | 56.44 deg / **5.3%** |
| `_close_e30_a120` / `_full_e30_a120` | 63.85 deg / 11.8% | 56.44 deg / **20.5%** |
| `_close_e05_a240` / `_full_e05_a240` | 64.37 deg / 0.0% | 56.95 deg / 0.0% |
| `_close_e15_a240` / `_full_e15_a240` | 64.37 deg / **0.0%** | 56.95 deg / **2.7%** |
| `_close_e30_a240` / `_full_e30_a240` | 64.37 deg / 7.9% | 56.95 deg / **18.5%** |

The caption on the left panel of every page:
> BEFORE -- exe 21:59:46, the sector-max footprint. terrain horizon
> 11.41..87.18 deg, mean 63.85, 0.9% lit. The stored byte is the MAXIMUM over
> the bin's whole 22.5 deg sector.

and on the right:
> AFTER -- exe 23:47:33, one square a tap. terrain horizon 4.32..86.82 deg,
> mean 56.44, 5.3% lit. Each stored byte is the skyline in its own direction,
> which is what the viewer blends.

What a reader sees, and it is the only claim these pictures make: in the BEFORE
panel the ground is one flat black field between the buildings; in the AFTER
panel the same ground carries lit patches along the streets and the open lots,
with the roofs unchanged. The roofs are the object stream and the ground is the
terrain sheet, so a change that touched only the terrain has to look like that
-- and it does.

The elevation-5 pages are in the set on purpose although neither side is lit:
**a picture that does not move is evidence too.** The third witness says only
2.8-3.8% of this chunk can be lit at 5 degrees, so a fix that lit it up there
would be the thing to distrust.

### 7.2 The control, beside the real picture

`control_beside_real.png`: the AFTER bake at sun 120,15, and beside it the SAME
bytes read four bins (90 degrees) away from the sun (`WW_HORIZON_BIN_ROT=4`).

> REAL -- the sun's own azimuth (120 deg): terrain horizon 4.32..86.82 deg,
> mean 56.44, 5.3% lit.
> CONTROL -- the same bytes a quarter turn away: terrain horizon 8.56..86.82
> deg, mean 56.17, 2.9% lit. If this were hard to tell from its neighbour, the
> azimuth is not being read and every picture here is decoration.

It is easy to tell: 5.3% lit against 2.9%, and the lit patches are in different
places. The renderer announces the control in its own note line
(`WW_HORIZON_BIN_ROT=4: THE CONTROL IS ON`), which the gate checks separately as
G3b.

### 7.3 The raycast overlay, before and after

`before_after_raycast_{close,full}_e{05,15,30}_a{120,240}.png` -- twelve pages,
each a BEFORE panel beside an AFTER one, every dot a sampled receiver where the
stored bins and the in-bake reference answered THAT sun differently. RED = a
terrain texel, ORANGE = a LOD vertex. The AFTER dumps come from a bake of the
same recipe with `WW_HORIZON_REFUTE_DUMP` set (`bake rc=0 34s`, 4,164 terrain
rows + 2,452 object rows, 01:01:51).

**The camera is measured, not assumed.** `compose.py` translates the camera by
three known world vectors, reads the image translation off by phase correlation,
and then predicts a FOURTH translation it never saw: close framing predicted
(-133.00, -86.80) against a measured (-133.00, -86.00), **0.80 px apart**; full
framing predicted (-168.70, -109.20) against (-169.00, -109.00), **0.36 px
apart**. The script stops itself above 1.5 px, because a red dot on the wrong
pixel is a claim about WHERE the two instruments disagree, invented.

Marks on the AFTER pages: close framing 3 / 33 / 48 (az 120, el 5/15/30) and
1 / 1 / 21 (az 240); full framing 19 / 121 / 217 and 5 / 54 / 221.

**The caption says what this page is worth, and it is less than it looks.** The
in-bake reference calls the same `maxAlong` the march calls, so it moved with
the fix; these pages show where two instruments that share a function disagree,
not where the sheet is wrong. The page that answers "is the sheet right" is
7.1's lit ground and the G6 numbers in 4.3.

### 7.4 The sixteen bins

`horizon_bins_close.png`, 2830x2246: one render a bin, laid out as a compass
rose -- bin 0 is NORTH (+Y) and the numbering runs CLOCKWISE toward EAST, each
tile captioned with its own azimuth. It is the picture that makes the
directional claim visible: the lit ground walks around the chunk as the bin
index advances, which is what a per-direction skyline has to do and what a
sector maximum blurs away.

## 8. Documents corrected, and the skill deltas

### 8.1 The nine edits, applied

All four files were LF-only before and are LF-only after (Python byte counts,
printed by the splice script itself, `scratchpad/horizon2_20260918/patch_docs.py`,
which refuses on a non-unique anchor):

```
docs/LODGEN_NATIVE_LODO_LODI.md        2 edit(s)  CRLF 0 of 2367 LF  149692 -> 152078 bytes
docs/LODGEN_TERRAIN_VT.md              2 edit(s)  CRLF 0 of 3097 LF  200192 -> 202035 bytes
docs/LODGEN_CENSUS.md                  2 edit(s)  CRLF 0 of  508 LF   54735 ->  55844 bytes
docs/FO4CS_IMPROVED_LOD_PLAN.md        3 edit(s)  CRLF 0 of 1538 LF   96353 ->  97751 bytes
```

**`docs/LODGEN_NATIVE_LODO_LODI.md`** -- the march's own chapter.
* §4.11's defaults line now reads the shipped ladder: growth **1.3**, about
  **33** steps to 4096 units, not ×1.5 / 22 steps, with the sweep that chose it
  (1.2 / 1.3 / 1.5 against the third witness: mean 8.09 / **8.14** / 9.77, bake
  50 s vs 47 s) written beside it so the next lane can see the knee.
* The "**Two deliberate biases**" block is now ONE bias. The second -- the
  sector-max footprint, `wantCell = binWidthFraction * d` plus a 2×2 tap, sold as
  a resolution invariant that "errs toward MORE occlusion, never less, which is
  the safe side for a shadow" -- is gone from the code and the paragraph now says
  what it cost: +8.52 degrees of one-sided bias and a downtown chunk reported
  0.0% lit at a 15-degree sun.
* The bullet "the footprint is the bin, not twice the bin" is marked
  **SUPERSEDED**, with its own numbers kept. It was a real improvement measured
  inside a convention nobody had measured, which is the more dangerous half of
  the story and is why it stays on the page instead of being deleted.
* The paragraph that called the refuter's reference "a CONE, and independent" is
  replaced by **THE REFUTER'S REFERENCE IS NOT INDEPENDENT OF THE MARCH**, with
  the two line numbers (`src/lodghorizonrefute.h:107`, `:112`) and the rule that
  G3 measures agreement, G6 measures correctness.

**`docs/LODGEN_TERRAIN_VT.md`** -- the sheet's chapter.
* "How high a terrain horizon really is" said the shipped sheet's 60-plus-degree
  means were what a dense city skyline looks like from the ground. It now says
  that reading was wrong, that a raw-input pencil over the same ten receivers
  gives a mean of 43.34 degrees, and that the difference was the march's
  footprint, not the city.
* The own-square sweep table is annotated as **a fact about the 21:59:46 build**
  and given its new default point (55.02).

**`docs/LODGEN_CENSUS.md`** -- the census field list.
* `…MeanRefPencilDeg` / `…ConeRays` now carry the warning that these are the
  IN-BAKE reference's own numbers and move when the march moves -- which makes
  them a regression signal on the march, not a score of it.
* The near-skip sweep clause is re-stated against the directional truth.

**`docs/FO4CS_IMPROVED_LOD_PLAN.md` §9**
* A new ruling-(i) bullet: **the `lerp` in the viewer is a constraint on the
  BAKE.** `BtdTerrain` blends the two bins either side of the sun's azimuth, and
  a blend of two sector maxima is not a skyline. Whatever a future lane does to
  the bin count, each stored byte has to be the skyline in its own direction.
* The bake-time table row for the horizon pass: 22 -> **33** steps, 47 s -> 50 s
  per chunk.
* The provenance block now names **G6** and both lane reports as the standing
  evidence for the terrain horizon.

### 8.2 Skill deltas, as text for the director

These are proposed edits, not applied -- the lane does not edit skills.

**`nifskope-ww-build-verify`** -- add, next to the existing stale-object bullet
(which is about an object older than a header it lists):

> **A header that NO object lists at all is the silent version, and `make` exits
> 0 on it.** `Makefile.Release` is generated by qmake and its dependency lists
> are frozen at generation time, so a header added to `src/` after the last qmake
> run appears in no `.o`'s prerequisites. Editing it produces `Nothing to be done
> for 'first'.` and **rc=0**, and `find src tests res -newer release/NifSkope.exe`
> stays empty because the exe is newer than everything -- it is the OLD exe.
> The tell is the one thing neither check looks at: **the exe's mtime did not
> move.** `stat -c %y release/NifSkope.exe` before and after, every build, and
> `grep -c <newheader> Makefile.Release` after adding any header (0 = the
> Makefile has never heard of it; delete the includers' objects by hand and note
> that a qmake run is owed, because the hand repair does not survive one).
> Lane HORIZON2, 2026-09-18, root `MISTAKES.md`.

Also worth a line in the same skill: **a brief that hands over a `PATH` is
handing over a shell assumption.** `/ucrt64/bin` is an MSYS2-shell path and
resolves to nothing under Git-Bash, where MSYS2 is at `/c/msys64`. The working
line for this tree's Bash tool is
`export PATH=/c/msys64/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`.

**`ww-test-harness-add`** -- add to §5 ("Every check gets a floor, and the floor
has to be able to fire"):

> **A floor between the code and a reference that CALLS IT is not a floor.**
> Before pre-registering any agreement threshold, `grep -n "<producer class>::"
> <the refuter source>` and name every function the two share. If the list is not
> empty the number measures agreement, and the gate's own comment must say so --
> it is still useful as a regression signal, it is simply not a check. The check
> that CAN fail is built from the RAW INPUTS and shares no line of code with the
> thing it scores; freeze it as DATA next to the script (this lane:
> `tests/spells/lodgen_horizon_witness.json`, ten receivers computed from the BTD
> heightmap and the placements as exact world boxes), give it a fixture-rot check
> so a fixture that silently loses its content fails instead of passing, and
> **show it failing on a real pre-fix artefact, not on a mutation** (`--expect-fail`
> fed the sheet the previous exe baked). Lane HORIZON2, 2026-09-18.

**`nifskope-ww-lodgen`** -- add to the editing traps:

> **A `.lodt` sheet's channel order in the FILE is not the packing order in the
> C++.** The bake builds a Qt `0xAARRGGBB` u32 (`shift[4] = {16,8,0,24}`) and the
> writer hands the plane out as **R8G8B8A8**, so on disk bin *j* of each group of
> four is **byte j**, shifts `{0,8,16,24}`. Any new reader validates against the
> shipped CONSUMER (`src/btdterrain.cpp` -> `LodtSheets::sheetChannel(role, tx,
> ty, bin % 4, …)`), never against the writer's constants; reading the file with
> the writer's shifts swaps bins 0 and 2 of every four and looks exactly like a
> bake defect. And note what the in-bake refuter cannot do: it reads its bytes
> back out of the in-memory `planes`, never out of the file, so no gate in the
> tree can currently catch a wrong swizzle.

**`ww-control-calibration`** -- add a short section:

> **A control is a comparison at IDENTICAL settings, and a sweep script leaves
> its artefacts at whichever value the sweep ended on.** This lane's Python
> prediction was frozen by a script whose `cast()` default was `growth=1.5` while
> the sweep in the same file had chosen 1.3; comparing the shipped exe to it gave
> `mean 2.325 deg, max 35.048` and read as "the C++ does not implement what was
> measured". Re-cast at the shipped configuration: **mean 0.244 deg, 97.7% of
> bins within one stored step.** Either regenerate the reference artefact at the
> shipped settings, or record the settings INSIDE the artefact so a mismatch
> refuses instead of reporting a number.

## 9. Text for the two files this lane does not touch

`WW_CHANGES.md` and `HANDOFF.md` were not edited. Here is their text.

### 9.1 A paragraph for `WW_CHANGES.md`

> **Far-LOD terrain shadows: the horizon march no longer smears every occluder
> across its azimuth bin.** `LodgenHorizonField::maxAlong` picked its mip level
> against the width of the 22.5-degree bin and took a 2×2 block at every tap, so
> each stored byte was the *maximum over the whole sector* rather than the
> skyline in that direction -- while the viewer blends the two bins either side
> of the sun, which is only meaningful for a directional sample. The march now
> takes one square a tap, spaced half a square, with the footprint derived from
> the segment rather than the bin, and the step ladder grows 1.3× instead of 1.5×
> (about 33 steps to 4096 units, 50 s vs 47 s per chunk). Measured against a new
> third witness built from the raw inputs -- the BTD heightmap read along a
> 1-degree pencil plus every placement as an exact world box, ten receivers on
> chunk 4.4.-12 -- the sheet's one-sided bias falls from **+8.59 to +0.87
> degrees** and its mean error from 11.90 to 8.15 (the 128-unit lattice itself
> owns 7.33 of that and no march over it can do better). In the viewer, chunk
> 4.4.-12's terrain horizon drops from 14.47..87.18 deg (mean 64.37) to
> 4.32..86.82 (mean 56.44), and the chunk that reported **0.0% lit** at a
> 15-degree sun now reports 5.3%. The way back (`--no-terrain-horizon`,
> `--lodi-v7`) is byte-identical in all 23 payload files. `tests/spells/
> lodgen_horizon.sh` gains **G6**, which scores the sheet against that witness and
> carries the pre-fix sheet as a red control that must fail on a real file --
> because G3's in-bake reference calls the same `maxAlong` the march calls and
> moved with it, which is why a 97% agreement floor sat at 51.69% for a day
> without anyone being able to say which of the two was wrong.

### 9.2 A LANDED block for `HANDOFF.md`

> **LANDED 2026-09-19 -- HORIZON2 -- the terrain horizon's sector-max footprint**
> * Exe `release/NifSkope.exe` 2026-09-18 23:47:33, 22,949,376 B, sha1
>   `b349f807426be700ed2ff9b54ee23e4fab3ba037`. Rung:
>   `release/NifSkope.before_horizon2.exe` (the 21:59:46 exe, sha1
>   `e578b76f9d7a2d011363e4300a2c94967e14d605`).
> * One source file: `src/lodghorizon.h` (untracked, 25,134 B). Three edits: one
>   square a tap at half-square spacing; footprint from the SEGMENT
>   (`(dEnd - d) / LODGEN_HORIZON_MAX_TAPS`, 64 taps) instead of
>   `binWidthFraction * d`; `growth` 1.5 -> 1.3.
> * Two tracked test files added: `tests/spells/lodgen_horizon_witness.py` and
>   `…_witness.json`; `tests/spells/lodgen_horizon.sh` gains G6 (427 -> 486 LF).
> * Nine documentation corrections across `LODGEN_NATIVE_LODO_LODI.md`,
>   `LODGEN_TERRAIN_VT.md`, `LODGEN_CENSUS.md`, `FO4CS_IMPROVED_LOD_PLAN.md` §9.
>   Seven `MISTAKES.md` entries.
> * Report: `scratchpad/horizon2_20260918/lane_horizon2_report.md`.
> * **Still red, with their numbers, not re-tuned:** G3 terrain
>   `horizonRefuteWorst` 51.69% -> **89.01%** against a 97% floor -- but the
>   in-bake reference calls `maxAlong` (`src/lodghorizonrefute.h:107,112`) and
>   moved too (`MeanRefDeg` 63.565 -> 58.995, toward the third witness), so that
>   floor scores agreement between two instruments, not correctness. G3 objects
>   `vhorRefuteWorst` 97.73% -> **95.96%** (all at A120E60) is a NEW red against a
>   floor the object stream used to pass; there is no `--horizon-growth` switch,
>   so separating growth from footprint needs a code change.
> * **Owed, found while running G5:** `tests/spells/lodgen_horizon.sh`'s
>   neighbour loop exports `OUT`, `SHEETS`, `V8DIR`, `V8VT`, `WAYBACK` and
>   `REFLOG` to every harness it runs. Those are the names the neighbours read
>   for their OWN fixtures, so `lodl_channels.sh` and `native_open.sh` fail
>   inside G5 and pass standing alone on the same exe (report 5.2, reproduced
>   three ways); and because the neighbours use `OUT` as their working
>   directory, a G5 run deletes the gate's own logs. Give each neighbour a
>   private `OUT` and unset the fixture names.
> * **Owed:** a `qmake` run (`Makefile.Release` carries no dependency on
>   `src/lodghorizon.h`, so make exits 0 having compiled nothing); a third witness
>   for the OBJECT stream's per-vertex horizon, which has none; cleanup of
>   `binWidthFraction`, now set by three callers and read only by the refuter's
>   bound; and the ruling on whether 16 bins is still the right count now that
>   each byte is directional.

## 10. Rows for bungo

| what he sees | before | after |
|---|---|---|
| Downtown chunk at a low sun (15 deg) | the whole chunk in shadow -- **0.0%** of it lit | **5.3%** lit; the streets facing the sun open up |
| How high the sheet thinks the skyline is | mean **64.4 deg** -- as if every direction had a tower in it | mean **56.4 deg**, and a raw measurement of the same ground says 43.3 |
| Which way the error pointed | always **too much** shadow, +8.6 deg everywhere | +0.9 deg, and it now falls on both sides |
| A gate that could catch this | none -- the checker shared its code with the thing it checked | **G6**, built from the heightmap and the building placements, with the old sheet kept as the proof it can fail |
| Bake cost | 47 s a chunk | **50 s** a chunk |
| Turning it off | `--no-terrain-horizon` / `--lodi-v7` | identical bytes to before, all 23 payload files |

## 11. MISTAKES entries written

Seven, at the top of the root `MISTAKES.md` (8,894 -> 9,080 LF, 541,310 ->
553,785 bytes, LF-only before and after, byte splice on a unique anchor):

1. **The refuter's "independent reference" calls the same function the march
   does** -- `src/lodghorizonrefute.h:107,112` call
   `LodgenHorizonField::maxAlong`. This is the brief's "a MISTAKES entry for
   whichever witness shared the bug", and the witness is named: the in-bake
   reference. The rule: name every function a producer and its reference share
   BEFORE pre-registering a floor between them, and read a reference whose own
   aggregates move when the producer is edited as a regression signal, not a
   score.
2. **A "resolution invariant" that errs on the safe side, with its price never
   measured** -- "errs on the safe side" is a claim with units; a one-sided bias
   does not average out.
3. **I read the shipped sheet with the WRITER's channel shifts, and blamed the
   bake** -- my instrument was wrong, not the bake; a decoder is validated
   against the shipped consumer. Carries the second finding: the in-bake refuter
   reads back from in-memory planes and can never verify the file.
4. **My leading hypothesis was refuted by my own measurement** -- SLAB1's
   wall/ceiling law made the error worse, 7.33 -> 10.90 deg. Write the
   measurement that refutes your favourite first.
5. **An elimination pass that scored every candidate against the defect's own
   convention** -- the first sweep found nothing because it graded candidates on
   how well they reproduced the sector maximum. The yardstick comes from the
   CONSUMER.
6. **`make rc=0` on a build that compiled nothing** -- `Makefile.Release` carries
   no dependency on a header added after qmake ran; the tell is the exe's mtime.
7. **A control that mixed the two changes it was controlling for** -- the frozen
   prediction was cast at growth 1.5 while the exe shipped 1.3.

## 12. The finished-work skill review

**Skills that carried weight, unchanged.** `nifskope-ww-lodgen` (the build
incantation, the exe lock, the `.lodt` reader traps), `ww-texel-picture` (every
framing and caption in section 7), `ww-sheet-diff` (the way-back byte comparison
in 4.7), `ww-population-refuter` (the 576-texel population in 4.4 rather than a
hand-picked rectangle -- the mean over a population is what made the +8.5-degree
bias visible where any single receiver looked merely noisy), and
`feedback_measure_dont_eyeball`, which is the whole lane: the defect had been
looked at and reasoned about for a day, and it moved the moment someone built an
instrument that could disagree.

**Skills that needed a delta, written out in 8.2.**
`nifskope-ww-build-verify` (a header with no Makefile dependency at all),
`ww-test-harness-add` (a floor between a producer and a reference that calls it),
`nifskope-ww-lodgen` (the file's channel order is not the packing order),
`ww-control-calibration` (a control is a comparison at identical settings).

**The skill this lane wanted and did not have -- proposed, for the director.**
`ww-third-witness`: building a raw-input witness for a baked field. Its shape,
from this lane: (1) pick the receivers from the OUTPUT's own addressing -- texel
centres, not round world numbers, so the comparison needs no interpolation;
(2) compute the truth from the SOURCE assets by a different mechanism than the
bake uses -- here a 1-degree pencil over the BTD heightmap plus placements as
exact world boxes, sharing no line with `src/lodghorizon.h`; (3) measure the
FLOOR the bake's own data structure imposes before setting any bar (the 128-unit
lattice costs 7.33 deg and no march can beat it), and put that number in the
script's docstring next to the bar it justifies; (4) freeze it as JSON with
`what`/`how`/`why` provenance beside the checker; (5) add a fixture-rot check so
a fixture that quietly loses its content fails; (6) require the checker to FAIL
on a real pre-fix artefact via `--expect-fail`, never on a mutation; (7) carry
the alternative truth column (`trueSectorMaxDeg`) so a future lane can argue the
convention without regenerating anything. That recipe is why this lane has a
gate that can fail, and it is worth having before the next field is baked.

**What this lane could not do and is owed:** the object stream's per-vertex
horizon has no third witness, and building one for 53,396 vertices is the same
recipe at a different scale. Until it exists, `vhorRefuteWorst` is the same kind
of number `horizonRefuteWorst` was.

## DONE

`horizon2` -- lane complete at 2026-09-19 01:28 (`date`). `BUILDING` removed,
`DONE` written. Nothing committed, nothing stashed, `WW_CHANGES.md` and
`HANDOFF.md` untouched (their text is section 9). Exe on disk:
`release/NifSkope.exe` 2026-09-18 23:47:33, 22,949,376 B, sha1
`b349f807426be700ed2ff9b54ee23e4fab3ba037`; the rung
`release/NifSkope.before_horizon2.exe` is the 21:59:46 exe, untouched.

### Five plain sentences for bungo

1. The far-LOD terrain shadow map was smearing every building across a
   22-and-a-half-degree wedge of sky, so the ground thought it had a tower in
   every direction and a whole downtown chunk rendered with **nothing lit at a
   low sun**; the march now looks along the actual direction it is storing, and
   that chunk goes from 0.0% lit to 5.3% at a 15-degree sun.
2. The reason nobody caught it is that the checker that was supposed to catch it
   calls the same function as the thing it checks -- so the two agreed with each
   other while both stood about eight and a half degrees too high -- and that is
   now written down in `MISTAKES.md` in plain words.
3. There is a new check, G6, that measures the sheet against the raw heightmap
   and the actual building placements instead, it shares no code with the bake,
   and it is shipped with the old broken sheet wired in beside it as proof that
   it can fail.
4. The way out is unchanged and free: `--no-terrain-horizon` / `--lodi-v7`
   produce byte-identical files to before, every other harness in the tree
   reports exactly the counts it reported yesterday, and the bake costs three
   seconds more a chunk.
5. **His open window needs a restart.**
