# Lane HORIZON1 -- horizon maps for far LOD shadows

## 0. Exe at launch + rung

Read 2026-09-18 19:28:02 CEDT (`date`):

- `release/NifSkope.exe` -- mtime 2026-09-18 19:05:11.012337400 +0200, size 22,764,544 B,
  sha1 `28ac412c6da96e0707e492c175822d2076016dc5`. Matches the brief's LODIV7 line exactly.
- Rung 19:28:17 CEDT: `release/NifSkope.before_horizon1.exe`, sha1
  `28ac412c6da96e0707e492c175822d2076016dc5` (identical copy).
- Game check (`tasklist | grep -i -E "Fallout4|NifSkope"`), 19:28:0x: `Fallout4.exe 4464` UP, no NifSkope.
  Per the brief's Game rule this lane does **NO build and NO exe run**. All work below is Python readers,
  design, source/script/doc edits. `PENDING.md` headed `BUILD PENDING` closes the lane.
- Markers: `scratchpad/horizon1_20260918/BUILDING` touched 19:28.

## 1. The measurements (step 1), 19:3x-19:5x 2026-09-18

Three scripts, all in `scratchpad/horizon1_20260918/`, all reading files already
on disk through `tests/spells/lodgen_native_decode.py` (no code shared with the
C++ writers): `measure_edges.py`, `measure_tall_and_sun.py`, `measure_sizes.py`.
JSON beside each.

### 1a. The long-edge distribution -- the size of the flat-quad smear

Region `scratchpad/viewfix_20260917/urban_ao/nat/FO4CSLOD/Commonwealth/`
(`.lodi` v6, chunks -1..2 x -4..0, cells -4..11 x -16..3).
**Control fired:** the walk's `instances_walked` = 33,123 = the header's
`instanceCount`, and `slot0_walked` = 33,123 = `slotInstances[0]`.

Drawn slot-0 population: **33,123 placements, 250,320 triangles, 750,960 edges**,
each at its own placement's scale (`scale = u16 / 8192`, NATIVE 4.1; the first
run of this script read the raw u16 and reported a p50 of 1.96 million units --
the units trap of root MISTAKES 2026-09-18 05:0x, caught by the number being
absurd rather than by the control).

| | world units |
|---|---|
| edge p50 | **239.7** |
| edge p90 | **512.0** |
| edge p99 | **1,158.7** |
| edge max | **5,476.1** |
| scale range over the drawn population | 0.260 .. 2.000 |

| bar | edges over it | triangles with an edge over it | share of 250,320 |
|---|---|---|---|
| 512 u | 71,465 | **41,042** | **16.4 %** |
| 1,024 u | 15,022 | **7,926** | **3.2 %** |
| 2,048 u | 1,085 | **664** | **0.27 %** |

Top offenders by triangles with an edge over 512 u (placements / triangles /
over-512 / over-1024 / over-2048 / longest edge):

| model | pl | tri | >512 | >1024 | >2048 | max u |
|---|---|---|---|---|---|---|
| `LOD\Buildings\BldgBrick4Story1x1BlockA_LOD.nif` | 91 | 910 | 728 | 728 | 0 | 1,331 |
| `LOD\Architecture\Buildings\Hightech\HitExtAWallTall01_LOD` | 344 | 688 | 688 | 0 | 0 | 572 |
| `LOD\Architecture\Buildings\Hightech\HitExtAFloorCornerA03_` | 270 | 540 | 540 | 0 | 0 | 724 |
| `LOD\SetDressing\Signage\BillboardBldgVerticalSm03_LOD.nif` | 42 | 1,176 | 532 | 0 | 0 | 687 |
| `LOD\Architecture\Buildings\Hightech\HitExtAFloorCornerA02_` | 258 | 516 | 516 | 0 | 0 | 572 |

Full table of 20: `edges_urban.json`.

**What the number MEANS, and it is a limit of the design, not a bug.** A horizon
term is stored per VERTEX and interpolated across the triangle, so a shadow edge
that falls inside a triangle is smeared over that triangle's edge length. Half
the drawn edges are under 240 units and 84 % are under 512, so the great majority
of the far field resolves a shadow edge to within a quarter of a cell; but
**41,042 triangles (16.4 %) cannot resolve one better than 512 units, 7,926
cannot beat 1,024, and 664 cannot beat 2,048.** The worst single edge in the
region is 5,476 units -- one and a third cells. No subdivision is proposed (the
brief forbids it and the standing rule "authored LODs only" forbids decimating or
subdividing authored LOD); this table is the row for bungo in section 12.

### 1b. The tallest placements, the sun, and the reach

**The tallest, over the terrain under them** (bound = the mesh AABB's highest
corner under the stored rotation, times scale; terrain = the plugin's own LAND
VHGT, bilinear over the 128-unit nodes):

| over land | over chunk zMin | model | position | scale |
|---|---|---|---|---|
| **11,160 u** | 11,726 | `GreebTower02_LOD_0.nif` | (19120, -7712, 9376) | 1.10 |
| 10,294 | 10,857 | `GreebTower02_LOD_0.nif` | (19088, -7872, 9376) | 0.70 |
| 9,524 | 9,357 | `ShackMetalWallFlat02_LOD` | (4084, -24737, 9326) | 1.00 |
| 9,327 | 9,160 | `ShackMetalWallFlat01_LOD` | (4084, -24737, 9129) | 1.00 |
| 9,255 | 9,088 | `PW_05_Generator_LOD.nif` | (3699, -24721, 9068) | 1.00 |

**The LAND decoder's control, and the first one I chose was NOT a control.**
Rejected: "every sampled land height sits inside the `.lodi` chunk table's
`[zMin, zMin+zExtent]` band". It came back 29,262 in / 3,861 out (88.3 %) and
that is CORRECT behaviour -- the chunk band is built from instance ORIGINS, so
ground below the lowest placement is legitimately outside it. A check a right
answer fails is not a check (CONSTITUTION rule 4), so it was thrown away rather
than explained away.
Kept: **the shared cell edge.** Cell (cx,cy)'s VHGT column 32 is the same row of
world positions as cell (cx+1,cy)'s column 0, and Bethesda stores the two
independently, so they agree only if the accumulation order is right.

| | edges agreeing within 0.5 u | differing | worst |
|---|---|---|---|
| the decoder as written | **2,402,212 (99.25 %)** | 18,140 | 13,056 u |
| **CONTROL RED** -- accumulation transposed (row for column) | 1,251,413 (51.7 %) | 1,168,939 | 27,184 u |

The 0.75 % that differ are real discontinuities in Bethesda's data (worldspace
edges and cliffs), not decoder error; the control separates right from wrong by
48 points, which is what makes it a control. `--control-red` runs it.

**The sun: there is no elevation in the file, and that is the answer.**
`src/esmdata.cpp` reads no `CLMT` and no `WTHR` (grep returns nothing), so the
plugin was walked directly. Worldspace `3C`'s `CNAM` is `0000015F
DefaultClimate`; its `TNAM` is `[30, 54, 102, 126, 175, 68]` = sunrise
**05:00..09:00**, sunset **17:00..21:00**, volatility 175, moons 68.
**CLMT stores TIMES, never angles.** The elevation is the engine's own function
of the time of day, so a march reach cannot be read out of the data: it is chosen
from an elevation BAR, and the bar is a stated default.

Reach = tallest / tan(elevation), for 11,160 u:

| sun elevation | reach |
|---|---|
| 1 deg | 639,362 u |
| 2 deg | 319,584 u |
| 3 deg | 212,947 u |
| **5 deg** | **127,561 u** |
| 7.5 deg | 84,769 u |
| 10 deg | 63,292 u |
| 15 deg | 41,650 u |
| 30 deg | 19,330 u |
| 45 deg | 11,160 u |
| 60 deg | 6,443 u |

### 1c. The sizes

Measured, chunk 4.4.-12 (`scratchpad/lodiv7_20260918/v7/`, `.lodi` **v7**):
2,449 placements, 2,449 streamed, **53,396 drawn vertices, mean 21.8 a
placement, max 1,004**; `.lodi` 243,420 B = **99.4 B a placement**;
`vertexAoBytes` 63,196; `vertexSkyBytes` 63,196.
Urban region (`.lodi` v6): 33,123 placements, **490,600 drawn vertices, mean
14.8**, `.lodi` 1,753,592 B = 52.9 B a placement, `vertexAoBytes` 623,096.

**The object horizon stream, A bytes a drawn vertex + `4(n+1)` of offsets:**

| A | chunk 4.4.-12 | urban region | B a placement (urban) |
|---|---|---|---|
| 8 | 436,968 B | 4,057,296 B (3.87 MB) | 122.5 |
| **16** | **864,136 B** | **7,982,096 B (7.61 MB)** | **241.0** |
| 32 | 1,718,472 B | 15,831,696 B (15.10 MB) | 478.0 |

**The worldspace, counted from the plugin and NOT from the 192x192 rim.**
Every one of the Commonwealth's **36,864** cells has a LAND record, so that count
is the rim and says nothing about population. Cells holding at least one REFR
that is neither deleted nor initially disabled: **4,707**, holding **701,131**
REFRs. The urban region's 320 cells hold 235,552 of them -- **33.6 % of the whole
worldspace in one region** -- so the worldspace is **2.98x the urban region by
live REFR**, not the 286x a naive chunk-ratio gives (chunk 4.4.-12's 16 cells
hold 9,334 REFRs, which would project 75x; the two ratios bracket the answer and
both are stated).

| | region | projected worldspace (x2.98) |
|---|---|---|
| drawn vertices | 490,600 | ~1,461,000 |
| **object horizon stream, A=16** | 7.61 MB | **~23 MB** |
| object horizon stream, A=8 | 3.87 MB | ~12 MB |
| object horizon stream, A=32 | 15.10 MB | ~45 MB |

**The terrain sheet, 2 x BC3 RGBA = 2 bytes a texel (8 bins a sheet):**

| texel | texels a cell | bytes a cell | x 36,864 cells |
|---|---|---|---|
| 8 u (the `.lodt` finest) | 262,144 | 512.0 KB | **18.00 GB -- refused** |
| 16 u | 65,536 | 128.0 KB | 4.50 GB |
| 32 u | 16,384 | 32.0 KB | 1.12 GB |
| **64 u** | **4,096** | **8.0 KB** | **0.28 GB** |
| 128 u | 1,024 | 2.0 KB | 0.07 GB |

**So the whole feature costs about 300 MB over the Commonwealth**, 23 MB of it in
the object streams and the rest in the coarse terrain sheet. The brief's 19 GB
figure for the finest tile is confirmed at **18.00 GB**, and that is why role 7
is coarse.

**G6 is therefore pre-registered against this table**: the bake's own census must
land within 10 % of 864,136 B for chunk 4.4.-12's A=16 stream, and within 10 % of
8,192 B a cell for role 7 at 64 u.

---

## 2. The object horizon stream, `.lodi` v8 (step 2)

Written 2026-09-18 20:0x by `date`. **Not built** -- Fallout4.exe is up (PID 25256
at 19:43, a different process from the 4464 this lane launched beside, so the game
was restarted during the lane) and the Game rule forbids a build. Every claim in
this section is a claim about SOURCE, and the build gate is owed. What I did do,
because it costs no output file and no exe, is a **syntax-only compile** of the
three files with the tree's own flags:

```
g++ -fsyntax-only -std=gnu++2a <Makefile.Release's DEFINES and INCPATH>
    src/lodifile.cpp       -> clean
    src/nativeemit.cpp     -> clean (one Qt/GCC -Wsfinae-incomplete warning from qchar.h, not mine)
    src/nifcli.cpp         -> clean
    src/lodghorizon.h      -> clean, through a 12-line driver that calls every entry point
```

A syntax check is **not** a build (CONSTITUTION 4: a green harness is not a build).
It cannot see a link error or a template instantiated only from another TU. It is
here because the alternative was to hand over four files nobody had ever put in
front of a compiler.

### 2.1 The header words

| offset | type | field | meaning |
|---|---|---|---|
| 0x11C | u64 | `offVertexHorizon` | the stream's payload offset |
| 0x124 | u32 | `vertexHorizonBytes` | the whole stream, its offset words included |
| 0x128 | u16 | `horizonAzimuths` | bins a vertex = **bytes a vertex** |
| 0x12A | u16 | `horizonSteps` | far-march steps a bin, **as cast** |
| 0x12C | f32 | `horizonReach` | the march reach in WORLD units |

All five live inside the block v7 itself reserved (0x11C..0x1FF), so the header
BLOCK stays 512 bytes, `lodiHeaderBytes()` needed no change, and the crc window
is untouched. The stream is written **last** of all payloads and folded into
`indexCrc32` **last**, so no existing payload's offset moves by a byte.

**The way back is `--lodi-v7`**: `LodiSrcSet::horizon` false writes no stream, no
header word, and leaves the version word where the v7 rule put it. That is G1's
object half.

### 2.2 The layout

Exactly s4.10's shape with a stride: `u32 first[instanceCount + 1]` of BYTE
offsets, then the bytes. A reader that knows only the layout can walk it without
knowing A, which is why the offsets are bytes and not vertices.

`byte = round( elevation_deg / 90 * 255 )`, one step **0.3529 deg**. Zero means
"nothing in this bin stands above the vertex's tangent plane", which is not the
same as "nothing is there": a ray below the tangent plane is the vertex's own
surface, N.L already handles that side, and it is never cast and never stored.

**Bin 0 is centred on NORTH (+Y); bins step CLOCKWISE toward EAST (+X); bin k
covers k * 360/A plus or minus 180/A.** At A = 16: 22.5 deg a bin, bin 4 east,
bin 8 south, bin 12 west. One function owns that convention for the bake, the
viewer and the gate -- `lodgenHorizonBinDir` in `src/lodghorizon.h` -- so a
picture and a byte cannot hold different opinions about which way bin 3 points.

### 2.3 The refusals, by name

Writer (`lodiWrite`, refusing before a byte is written):

* `vertex horizon without vertex AO: version 8 is a superset of version 6`
* `vertex horizon armed with horizonAzimuths 0: the bin count is the stride...`
* `vertex horizon armed with horizonReach <v>: the march reach must be positive world units`
* `instance N (name) has H horizon bytes but A AO bytes x Z azimuths = X; the two
  streams are one vertex population`
* `vertex-horizon stream of N bytes does not fit a u32 size word`

Reader (`lodiRead`):

* `version 8 with no vertex-horizon stream (header 0x11C is 0)...`
* `vertex-horizon stream of N bytes cannot hold its own M offset words`
* `horizonAzimuths is 0 (header 0x128); the bin count is the stream's stride...`
* `horizonReach V (header 0x12C) is not positive finite world units`
* `vertex-horizon offset i (v) is below offset i-1 (w)` -- monotone
* `vertex-horizon offsets run a..b but the stream carries N horizon bytes after its M offsets`
* `instance i has H horizon bytes but A AO bytes x Z azimuths = X; ...` -- the population cross-check
* `version 7 carrying version-8 header words (horizon stream at 0x11C = ...)` --
  named, not left to the pad sweep, so nobody is sent to "reserved byte at 0x11c"
* the pad sweep itself now starts at **0x130** on a v8 file and stays at 0x11C on a v7 one

### 2.4 The cast

`src/lodghorizon.h` is new and holds the ONE implementation both bakes use (the
object stream here, the terrain sheet in s3), plus the bin direction, the
quantiser and the two-bin interpolation the viewer and the gate read.

The AO scene cannot do this job: its reach is 300 miniature units and the feature
exists to see a tower two kilometres away. So the march walks **two max-Z
lattices** instead, and casts no rays at all:

* **near**, per chunk, 32 units a square, filled from the SAME triangles the
  `LodgenAoScene` is filled from (the v6/v7 population exactly), in world units;
* **far**, once per bake, 128 units a square -- the LAND record's own sample
  spacing, so the heightfield enters it exactly and not resampled -- holding the
  terrain over the whole reach-clipped rectangle and every placement this bake
  gathered as its world-space bound's footprint at its own top.

The horizon is the max of both, never one instead of the other.

**Steps.** `d` starts at 32 u and grows x1.5 to the reach:
`ceil( log(127561/32) / log(1.5) ) + 1 = 22` steps a bin, so **352 steps a
vertex at A = 16**, logarithmic in the reach as the brief asks.

**A step covers a SEGMENT, not a point.** A point-sampled x1.5 ladder skips 0.5 d
of ground at every step and walks straight past a tower standing in it. Each step
reads the segment from d to 1.5 d through a maximum-mipmap (`maxAlong`), so the
march cannot step over an occluder.

**The one resolution invariant**: the lattice square a segment is read at is never
wider than the AZIMUTH BIN ITSELF is at that distance (2 sin(180/A) d = 0.390 d at
A = 16). The smear the lattice adds is therefore never larger than the smear the
22.5-degree bins already carry, and it errs toward MORE occlusion, never less --
the safe side for a shadow, and the refuter's disagreement table counts exactly
those pixels in its own column.

About four taps fall in a step, 4 reads each, so a vertex costs
`16 x 22 x 16 = 5,632` array reads and **no triangle intersection anywhere**.

### 2.5 The census

`LodiWriteStats` carries `vertexHorizonBytes`, `vertexHorizonPlacements`,
`horizonVertices`, `horizonBytesPerVertex`, `horizonAzimuths`, `horizonSteps`,
`horizonReach`. `horizonBytesPerVertex` is the self-check: it comes back as
`horizonAzimuths` exactly when every slice is A times its AO slice.

`lodiDescribe` prints the same numbers READ BACK from the file's own bytes --
`vertexHorizonPlacements`, `vertexHorizonBytesTotal`, `horizonVertices`,
`horizonBytesPerVertex`, `horizonMeanElev` (degrees), `horizonMinByte`,
`horizonMaxByte` -- never from the switches that asked for them.

The bake's census line (`native-ladder: ... ; vertex horizon ...`) adds the
lattice's own cost: `far lattice W x H squares of C u over N land cell(s), M MB
with its mip chain`, and `OFF (--lodi-v7)` when the module is off. `horizonZeroBins`
is the honest half: a stream that saw nothing is a number, not a clean-looking mean.

### 2.6 Switches landed

| switch | effect |
|---|---|
| `--lodi-v7` | the whole module off: no stream, no role-7 sheet, both files byte-identical |
| `--horizon-azimuths <n>` | 4..64 in steps of 4 (a non-multiple of 4 leaves a sheet channel dead) |
| `--horizon-reach <u>` | world units above 0 |
| `--no-terrain-horizon` | object stream only; the `.lodt` keeps today's roles |

Files touched in step 2: `src/lodifile.h`, `src/lodifile.cpp`, `src/lodghorizon.h`
(new), `src/nativeemit.h`, `src/nativeemit.cpp`, `src/nifcli.cpp`, `NifSkope.pro`.

---

## 3. The terrain horizon sheet, `.lodt` role 7 (2026-09-18 20:2x)

### 3.1 What the brief asked for, and the one place its arithmetic did not close

The brief's step 3 asked for *"two RGBA sheets (bins 0-7, bins 8-15), BC3,
COARSE: 64 world units a texel (64x64 texels a cell)"*, 8,192 bytes a cell,
0.28 GB a worldspace, at 16 azimuths.

Those numbers cannot all be true at once, and the gap is not a rounding
argument:

* an RGBA sheet has FOUR channels, so two sheets carry EIGHT bins, not sixteen;
* BC3 is one byte a texel for all four channels together, so two sheets are
  2 B/texel -- which is where 8,192 B a cell at 64 u comes from. That is
  **half a bit per bin per texel**.

Sixteen bins at one byte each is 16 B/texel however it is packed. The brief's
byte budget and its bin count are two different designs. I built the one that
keeps the BYTE, and paid for it in texel size; section 3.6 tables what the other
choices would have cost so the ruling is bungo's and not mine.

### 3.2 Why not BC3, measured rather than asserted

BC3's ALPHA is a good scalar channel (two 8-bit endpoints, 3-bit indices). Its
RGB is BC1: two **RGB565** endpoints, so R and B carry a 5-bit floor and G a
6-bit one, no matter how smooth the block is.

What a 5-bit floor costs on this particular number:

* one step of a 5-bit elevation channel over 0..90 degrees is **2.82 degrees**;
* a caster of height `h` at a sun elevation `e` throws its shadow edge to
  `d = h / tan(e)`, so `dd/de = -h / sin^2(e)`;
* at `h = 1000` units and `e = 10 degrees`: `dd = 1000 / 0.0301 * 2.82 deg in
  radians = 1,625 world units` of shadow-edge error -- on a shadow whose whole
  length is 5,671 units.

An 8-bit channel's step is 90/255 = **0.353 degrees**, which puts the same edge
error at **203 units**. That is the difference between a shadow that lands on
the right hill and one that does not, so the sheet is **uncompressed
R8G8B8A8_UNORM** and the format enum gained that DXGI value (28) for this role
alone. Four bins a sheet, one per channel, R,G,B,A -- 4 B/texel, exact.

### 3.3 The shape on disk

| word | value |
|---|---|
| role | `LODV_ROLE_HORIZON = 7` (`src/io/lodvfile.h`) |
| format | `LODV_DXGI_R8G8B8A8_UNORM = 28`, uncompressed, no cover variant |
| sheets | `azimuths / 4`, so **4** at the default 16; cap `LODV_HORIZON_MAX_SHEETS = 4` (64 bins) |
| placement | **last and contiguous**; bin `b` = sheet `sheetCount - sheets + b/4`, channel `b%4` |
| byte | `degrees = byte / 255 * 90`, elevation above the horizontal AT THE TEXEL; 0 = nothing stands above it in that azimuth |
| bins | bin 0 centred on NORTH (+Y), stepping CLOCKWISE toward east (+X) -- the same convention as the `.lodi` v8 stream (s2, s4.10) |
| level | ONE level, the coarsest whose texel is no wider than `--vt-horizon-texel` (default **128 u**) |
| mips | the same `(a+b+c+d+2)>>2` per channel the colour sheets use -- a mean, because a coarser texel of a smooth field is a mean. (The LATTICE mips take a max; the two are different objects and the code says so.) |

`LODV_MAX_SHEETS` went **6 -> 10**: descriptors now occupy 0xA0..0xEF on the
same 8-byte stride and the reserved tail is 0xF0..0xFF. Nothing before 0xA0
moved and the stride did not change, so a container written before today still
reads -- its slots 6..9 hold the zeroes the old writer left, which is exactly
what the "past `sheetCount` must be zero" rule already required.

**Refusals, by name** (G2 will name each):

* `refused: %1 horizon sheets (role 7); at most 4, which is 64 azimuth bins`
* `refused: the horizon sheets (role 7) are not the last %1 sheets; they start at %2 of %3`
* `refused: sheet %1 sits inside the horizon run (sheets %2..%3) and has role %4; the horizon sheets are contiguous`
* `refused: sheet %1 has dxgiFormat %2 / %3` (role 7 accepts 28 and nothing else)
* bake side: `the terrain horizon takes a multiple of 4 azimuth bins, not %1`
* bake side: `level %1 declares %2 horizon sheets and staged %3` -- the header
  and the payload are written from ONE number and a level that declared sheets
  it did not stage would write a tile shorter than its own header.

Role 7 is the **one role a container may carry more than once**; every other
role still refuses a duplicate. The contiguity and last-ness rules are what make
`sheet = first + b/4` sound, so they are checked rather than documented.

### 3.4 The cast

Two lattices, both at the LAND record's own **128-unit node spacing**, so the
terrain enters unresampled (`LodgenVtHorizon`, `src/lodgen.cpp`):

* **`sky`** -- the occluders: a max of every LAND node in the rectangle and of
  every placement's world bound, taken from `LodgenObjectHeightField` (the same
  max-Z plane and the same conservative silhouette the slab march already
  reads, so the horizon cannot disagree with the slab about what an object is).
  Mipped, and marched by `maxAlong` over each growing segment.
* **`ground`** -- terrain ALONE, read bilinear (`sampleBilinear`, new in
  `src/lodghorizon.h`), and used only as the ray's ORIGIN. Starting a ray from
  `sky` would start it on a roof.

Per texel: origin = ground height + `rise` 4 units, **no normal** -- the sheet
is the horizon of the surroundings and the ground's own slope stays the msn
sheet's job and the consumer's N.L; folding it in here would apply it twice.
Then `lodgenHorizonCastAt` -- the SAME function the per-vertex stream calls, so
the two cannot drift apart.

Reach and step count are step 2's: 127,561 units, 22 steps at x1.5 from 32
units, each step covering a segment rather than a point.

**What it sees, stated rather than implied.** Terrain to the reach or the
worldspace edge, whichever comes first. Placements only within
`objectMarginCells` (`min(reach in cells + 1, 32)` = 32 cells = 131,072 units at
the default reach, so at the default nothing is clipped), because gathering
placements is an ESM walk plus a LOD model load per base, not a lattice read.
Both are census words.

Texel-to-world uses `lodgenBakeVtTile`'s own map (`tileW + (i - border + 0.5) *
upt`, `tileN - (j - border + 0.5) * upt`) copied deliberately: the horizon has
to land on the same texel centres as the colour under it or one UV reads two
different places on the ground.

**Cast, never filtered.** A parent's colour is the mean of its children's; a
parent's horizon is not, because the level below it has none. The horizon level
is cast directly in the parent row loop from the same two lattices at its own
texel, and `lodgenVtFilterTile` leaves the planes empty on purpose.

### 3.5 The census (`vt:` line) and the index words

New `vt:` tokens, every one read back from the bytes that were written:

`horizonSheets`, `horizonAzimuths`, `horizonLevel`, `horizonTexel`,
`horizonReach`, `horizonSteps`, `horizonTexels`, `horizonBins`,
`horizonZeroBins`, `horizonMeanElev`, `horizonLandCells`,
`horizonObjectSquares`, `horizonObjectMarginCells`, `horizonGroundMisses`,
`horizonLatticeBytes`, `horizonLatticeSquare`.

`horizonZeroBins` is the self-accusing one: a bin at 0 is a bin that saw
nothing, and an all-zero sheet is a cast that failed. It has to be a number on
the line, not a clean-looking mean. `horizonGroundMisses` counts texels with no
LAND under them (the worldspace rim), which cast from sea level instead of from
minus infinity.

`.lodm` gains `terrain.horizon` -- `azimuths`, `sheets`, `level`, `levelDim`,
`texel`, `reach`, `rise`, `dxgi`, `sawObjectsWithinCells`,
`latticeSquareUnits`, plus three sentences (`bin`, `byte`, `use`) that state the
bin convention, the byte law and the compare a consumer performs. Absent is
said as `"horizon": "none"`, never as a missing key. Each level entry gains
`horizonSheets`, because only one level has them.

### 3.6 Bytes, and the rows bungo gets to rule on

Geometry: content 256, border 8 (stored 272), 2 mips. One horizon sheet is
272^2*4 + 136^2*4 = **369,920 B a tile**; four sheets are **1,479,680 B a tile**.
A dim-8 tile is 64 cells.

| texel | level dim | bins | B/cell (content, mip 0) | Commonwealth-sized worldspace (36,864 cells, with border + mips) |
|---|---|---|---|---|
| 64 u | 4 | 16 | 65,536 | 3.41 GB |
| **128 u** | **8** | **16** | **16,384** | **0.85 GB**  <- ships |
| 256 u | 16 | 16 | 4,096 | 0.21 GB |
| 128 u | 8 | 8 | 8,192 | 0.43 GB |
| 256 u | 16 | 8 | 2,048 | 0.11 GB |

The shipped row is **3.0x the step-1c projection of 0.28 GB**, and that
projection is the thing that was wrong, not the bake: it assumed 2 B/texel for
16 bins, which is half a bit a bin. `--vt-horizon-texel 256` is one switch away
and lands at 0.21 GB, under the original projection, at 3.6 m texels instead of
1.8 m -- on a field whose azimuth bins are already 22.5 degrees wide, that is
very likely invisible, and it is bungo's call, not mine.

128 u is the default for a reason that is not taste: it is the LAND record's own
node spacing, so a horizon texel sits on a terrain sample and the cast resamples
nothing.

### 3.7 The way back

`--no-terrain-horizon` sets `hzSheets` to 0 and from that line on nothing runs:
no sheet descriptor, no plane, no byte, no cast. The `.lodt` is what it was
before this lane. `--lodi-v7` turns off the object stream; the two switches are
independent and G1 pins both.

**Risk I am naming rather than hiding:** the sheet is ON by default inside
`--vt`, so the six existing gates that bake `--vt`
(`lodgen_defaults.sh`, `lodgen_roads.sh`, `lodgen_slab.sh`,
`lodgen_terrain.sh`, `lodgen_terrain_pbrm.sh`, `lodgen_terrain_vt.sh`) now bake
a horizon too. None of them pins a stored `.lodt` golden -- `lodgen_terrain_vt`
compares two runs with each other and the assembled `.btr` sheets against a
direct bake, which the horizon does not touch -- but **this is unverified until
they run**, and G5 is where it gets verified. If any of them turns red on time
rather than on content, `--no-terrain-horizon` is the one-word fix for that
script.

### 3.8 Files and the check that was run

Touched: `src/io/lodvfile.h`, `src/io/lodvfile.cpp`, `src/lodgen.h`,
`src/lodgen.cpp`, `src/lodghorizon.h`, `src/nifcli.cpp`.

`g++ -std=gnu++2a -fsyntax-only -Wall -Wextra` with the tree's own DEFINES and
INCPATH (plus `-Ilib/qhull/src -isystem lib/gli/gli -isystem lib/gli/external
-Ilib/libfo76utils/src`, which `CXXFLAGS` carries) is **clean** on
`src/io/lodvfile.cpp`, `src/lodgen.cpp`, `src/nifcli.cpp`, and on the three
neighbours that read the container -- `src/nativeemit.cpp`, `src/lodtsheets.cpp`,
`src/btdterrain.cpp`. The only warning in range is `lodgen.cpp:125 unused
parameter 'grid'`, which predates this lane.

**A syntax check is not a build** (CONSTITUTION 4): it links nothing and runs
nothing. Fallout4 is up, so the build is owed and `PENDING.md` carries it.

---

## 4. The viewer: `WW_LODL_CHANNEL=horizon` (2026-09-18 20:4x)

### 4.1 The names, and the one place a name carries an argument

| env | what it does |
|---|---|
| `WW_LODL_CHANNEL=horizon` | objects AND terrain drawn by the SHADOW COMPARE: lit where the sun's elevation clears the interpolated horizon, dark where it does not |
| `WW_LODL_CHANNEL=horizonbin=<n>` | bin `n`'s own elevation as grey on both halves; white = 90 degrees |
| `WW_SUN=<azimuth_deg>,<elevation_deg>` | the sun. Azimuth is measured THE WAY THE BINS ARE -- 0 = north, clockwise toward east. Default `120,15` |
| `WW_HORIZON_SOFT_DEG=<deg>` | the smoothstep WIDTH, centred on the horizon. Default **1.0**, clamped to 0..45; 0 is a hard edge |
| `WW_HORIZON_BIN_ROT=<n>` | **THE CONTROL**: read the horizon `n` bins away from the sun. `A/4` is 90 degrees |

`horizonbin=<n>` is the first channel name that takes an argument, so the split
happens in `lodlChannelFromEnv` and nowhere else (`src/lodinative.cpp`): one
string, one parse, both halves. A bin that will not parse, a bin outside
0..63, or an argument on a name that takes none refuses the WHOLE name -- it
never quietly becomes bin 0. `lodlChannelNames()` prints it as `horizonbin=<n>`
so the refusal line teaches the syntax.

The sun and the softening are read by `lodlSunFromEnv` / `lodlHorizonSoftDeg`,
which live beside the channel table for one reason: the object half and the
terrain half must not be able to light one picture from two different places.
`lodlHorizonLit( horizonDeg, sunElevDeg, softDeg )` is the ONE compare both
halves call, and `lodlHorizonGrey` is the one grey both draw -- never 0, so
shape survives in shadow (dark = 0.10).

### 4.2 The object half (`src/lodinative.cpp`)

`horizonPerVertex` mirrors `skyPerVertex` exactly: it is true only when the
stream was READ (`vertexHorizonFirst` and `vertexHorizon` both non-empty and
`horizonAzimuths` > 0), never from the version word. The slice lookup is the
sky lookup with ONE change that matters -- the length gate multiplies:

```
rit.value().second * horizonAz == l - f
```

A slice that is not exactly `A x vertexCount` is refused and counted
(`hzMismatch`), because indexing vertex `v` at `v * A` in a slice cast for
another mesh reads another vertex's bins and would never announce itself. That
is the one failure this format can hide, so it is the one the reader checks.

A drawn vertex with NO slice is drawn **magenta**, not black: black is a shadow
and a missing stream is not (root MISTAKES 05:0x -- never a proxy shown as the
channel). The count is on the note line.

`nb.withColour` now includes `horizonPerVertex`, so the vertex colour actually
reaches the buffer; without that line the channel would compute perfectly and
draw nothing.

### 4.3 The terrain half (`src/btdterrain.cpp` + `src/lodtsheets.{h,cpp}`)

Three gaps had to close before the terrain could read role 7, and each one was
a refusal waiting to happen rather than a missing feature:

1. **`sheetChannel` found only the LAST sheet of a role.** Role 7 repeats four
   times. It now takes an `occurrence` (default 0, so every existing call is
   byte-identical), and bin `b` is `occurrence b/4, channel b%4`. Out of range
   refuses by name: `the container carries %1 sheet(s) with role %2; there is
   no occurrence %3`.
2. **It refused anything that was not BC1 or BC3.** Role 7 is uncompressed
   `R8G8B8A8` (s3.2), so the sampler gained an uncompressed path -- in the SAME
   function, not a second reader -- and the refusal text now names all three
   formats.
3. **`open()` picks the FINEST level.** The horizon lives on one level (dim 8 at
   the default 128-unit texel). `openForRole( path, role, why )` picks the
   finest level that CARRIES the role, reading each candidate's header to
   answer the question instead of guessing, and IGNORES `WW_LODL_SHEET_DIM` --
   a dial pointing at another level would turn "here it is" into "absent".

The sampler itself is the mask block's, copied deliberately: the same bilinear
read at the same texel centres, the same `tileOfCell`, the same content-square
map. Two bins are sampled (the pair the sun's azimuth falls between) and
blended by the same fraction; because degrees are linear in the byte, the
spatial blend and the azimuth blend commute, so this is exactly the number
`lodgenHorizonElevAt` returns. To make that a fact rather than a claim, the bin
pair now comes from **`lodgenHorizonBinPair`**, which `lodgenHorizonElevAt`
itself calls -- one arithmetic, two callers.

### 4.4 The note lines, read back from what was drawn

Every line carries numbers taken from the values that went INTO the buffer:

* objects: bins a vertex, bytes over slices, the sun and where it came from
  (`WW_SUN` or `default`), the softening width, vertices drawn, horizon
  min..max and mean IN DEGREES, per cent lit, mismatched slices, and vertices
  drawn magenta;
* terrain: the container's file name, bins over sheets, the LEVEL DIM and the
  units a texel, tiles read, vertices, vertices with no tile, horizon
  min..max/mean in degrees, per cent lit, and -- for `horizon` -- which two
  bins the sun fell between and the blend;
* `horizonbin=<n>`: the bin's centre azimuth, its width, and (terrain) which
  sheet and channel it is;
* **a CONSTANT horizon over the chunk is called out by name** -- "a horizon that
  never varies is a cast that did not run";
* `WW_HORIZON_BIN_ROT` prints, on both halves: *THE CONTROL IS ON ... This
  picture is SUPPOSED to be wrong.*

Absent is always said in words: no stream in the `.lodi` (with the version and
the `horizonAzimuths` it did carry), no role-7 sheet in any level (with the
reason `openForRole` gave), no tile over the region.

## 5. The refuter: an independent cast, inside the bake (2026-09-18 20:4x)

### 5.1 Where it lives, and why not in the viewer

The brief asked for a ray-cast reference "through the same `LodgenAoScene` +
heightfield at full resolution". That scene exists in the BAKE and not in the
viewer: the viewer holds a chunk, and a horizon 127,561 units long is mostly
made of terrain and placements the viewer never loaded. A reference computed
from what the viewer has would disagree with the bake for distant occluders and
the refuter would be the thing that was wrong.

So the refuter runs where the fields are: `src/lodghorizonrefute.h`, called from
the two cast sites (`LodgenVtHorizon::castTile` for the sheet,
`nativeemit.cpp`'s per-vertex loop for the stream), armed by
**`--horizon-refute <n>`** and off by default.

### 5.2 What makes the reference independent

The bake's march trades resolution for speed in exactly three ways. The
reference gives all three back and changes nothing else -- same rise, same
tangent-plane floor, same "measure to the near end of the segment" rule, same
two lattices, same world units:

| | the bake | the reference |
|---|---|---|
| azimuth | the bin CENTRE, 16 of them | the sun's EXACT azimuth |
| step | 32 u growing x1.5, 22 steps | 32 u CONSTANT, ~3,986 steps |
| lattice | the coarsest mip the bin width allows (2-square lateral smear) | **mip 0 only** (`wantCell` 1) |

The reference is therefore finer in every direction and is expected to return a
horizon at or below the stored one. It is also ~4,000 segment reads a ray,
which is why it samples rather than sweeps.

### 5.3 What it compares, and the control

Per sampled receiver, at the two azimuths **120 and 240** and the four
elevations **5, 15, 30, 60** (the eight pre-registered sun positions): does the
stored horizon put the sun above or below the horizon, and does the reference
agree? The bins are read back OUT of the written planes/stream, so a packing
bug -- a bin in the wrong channel, a shift off by one -- is inside what the
comparison measures.

**The control** is computed in the same loop: the same compare with the bins
read 90 degrees away (`vhorRefuteControlWorst`, `horizonRefuteControlWorst`). A
run where the control also passes is a comparison that is not measuring the
horizon, and G3 fails on that.

### 5.4 The table, by cause

Every disagreeing sun position is attributed, and the four counts sum to the
total disagreement:

| token | meaning |
|---|---|
| `...CauseEdge` | the sun is within one u8 step (0.353 deg) of the reference horizon -- a coin flip the byte cannot decide |
| `...CauseAzimuth` | the two bins straddling the sun differ by more than twice the error: the 22.5-degree bin is too wide HERE |
| `...CauseCoarseOver` | the bins say HIGHER -- the coarse mip's lateral smear, or the segment max, found a top standing beside the ray |
| `...CauseGrowthUnder` | the bins say LOWER -- the x1.5 growth stepped over the occluder |

Plus `...MeanErrDeg`, `...MaxErrDeg`, `...MeanRefDeg`, `...Samples`, the eight
per-position percentages, `...Worst` and `...ControlWorst`. Two prefixes:
`horizonRefute` on the `vt:` census line, `vhorRefute` on the native ladder
line.

### 5.5 What this is NOT

It is not a picture-versus-picture diff. The brief asked for a reference IMAGE
beside each shot with disagreeing pixels in red; what is implemented is the
NUMERIC half of that comparison, at higher resolution than a screenshot can
carry, plus the rotated-bin control rendered as a picture. Stating the
deviation rather than hiding it: a reference image would have to be drawn by a
viewer that cannot see the distant occluders (5.1), so it would have compared
the bake against a weaker cast and called the bake wrong. The numeric refuter
compares it against a STRONGER one.

The G3 gate therefore reads its floor off the census line
(`horizonRefuteWorst >= 97`, `vhorRefuteWorst >= 97`) and its control off the
same line, and the pictures carry the rotated-bin control beside the real one so
the eye has the same refutation.

**Corrected later in the lane, and the correction is the point:** the control
the gate reads is `...ControlDirWorst`, NOT `...ControlWorst`. A quarter turn is
arithmetically the identity on an isotropic profile, and 72.8% of this chunk's
vertices carry one, so the whole-population control scored 91.25 by agreeing
with itself. Scored over the receivers whose bins actually vary it reads 60.59.
Both are printed, with `...DirSamples` beside them so a control over an empty
population is visible rather than silent. Sections 7.2 and 13.

### 5.6 Files touched in steps 4 and 5

`src/lodinative.h`, `src/lodinative.cpp`, `src/btdterrain.cpp`,
`src/lodtsheets.h`, `src/lodtsheets.cpp`, `src/lodghorizon.h` (the bin-pair
split), **new** `src/lodghorizonrefute.h`, `src/lodgen.h`, `src/lodgen.cpp`,
`src/nativeemit.h`, `src/nativeemit.cpp`, `src/nifcli.cpp`.

`g++ -std=gnu++2a -fsyntax-only -Wall -Wextra` with the tree's own DEFINES and
INCPATH is clean on every one of them. The only warnings in range predate this
lane (`lodgen.cpp:125 unused parameter 'grid'`, a `/*` inside a comment at
7624, `\s` in a string at 3677, the `%d`/`qsizetype` format at 13190, the
`qsnprintf` deprecation at 14383, and a Qt6 `QChar` SFINAE note in
`nativeemit.cpp`), each verified to be present in `HEAD` or in code this lane
did not touch. **A syntax check is still not a build.**

## 6. The pictures (step 5), 2026-09-18 21:5x-22:0x

`scratchpad/horizon1_20260918/images/`, all from ONE bake (`v8/`), rendered by
`shots.sh` on the 21:59:46 exe, every path absolute, one instance at a time,
`WW_WINDOW_AT=1960,40`, the scene passed positionally, and a
`tasklist | grep -i -E "Fallout4|NifSkope"` before EVERY shot rather than once at
the top.

### 6.1 The twelve, and the control beside them

`chunk_horizon_{close,full}_e{05,15,30}_a{120,240}.png` -- two framings, three
sun elevations, two azimuths.

| framing | centre | ortho | what it holds |
|---|---|---|---|
| `close` | 24900,-41300,450 | 2600 | the deck and its placements: object-dominant |
| `full` | 24576,-40960,0 | 8192 | the whole chunk: terrain-dominant |

Every note line is read back from the bytes the renderer uploaded, not from
intent: `WW_LODL_CHANNEL=horizon: the PER-VERTEX HORIZON STREAM (.lodi v8 0x124)
from Commonwealth.lodi, 16 bins a vertex, 853584 bytes over 2446 slices; sun
120.0,15.0 (WW_SUN), softening 1.00 deg; 53349 vertices drawn, horizon
0.00..89.65 deg mean 26.37, 65.1% lit`.

The **lit share barely moves with the sun** -- 64.4% at 5 degrees, 65.1% at 15,
66.9% at 30 -- and that is the class split (section 7.2) showing up in a
picture rather than a bug: 60.7% of drawn vertices have an all-zero horizon and
are lit at every sun, 12.1% are blocked at 83.4 degrees and are dark at every
sun, so only the remaining 27.2% can change, and they change by 2.5 points over
that range.

`chunk_horizon_close_e15_a120_CONTROL.png` is the same bytes read a quarter turn
away (`WW_HORIZON_BIN_ROT=4`), and the viewer says so in its own note line:
*"THE CONTROL IS ON -- the horizon was read 4 bin(s) (90.0 deg) away from the
sun's azimuth. This picture is SUPPOSED to be wrong."* It is a different picture
(G3b asserts that byte-wise), which is the eye's version of the numeric control.

### 6.2 `horizon_bins_close.png` -- the sixteen bins as a grid

One render per bin (`WW_LODL_CHANNEL=horizonbin=<n>`), grey = that bin's stored
elevation, 0 degrees black to 90 white, laid out four to a row with each tile
labelled by its azimuth. Bin 0 is +Y (north) and the numbering runs clockwise,
so the grid reads as a compass rose. The terrain gradient rotates tile by tile,
which is the azimuth being carried, visible without a number; the per-bin means
(26.20 to 26.75 degrees) are printed in the logs beside each render.

### 6.3 `horizon_vs_raycast_*.png` -- WHERE the refuter disagrees

A count cannot answer *"and with no artifacts"*, because an artifact is a
PLACE: 351 disagreements scattered one to a hectare is nothing, and 351 along
one road edge is a black seam. So `--horizon-refute` gained an optional dump
(`WW_HORIZON_REFUTE_DUMP=<dir>`, `src/lodghorizonrefute.h`): one row per sampled
receiver -- world position, one bit per sun position set when the stored bins and
the full-resolution reference cast answered that sun differently, and whether the
receiver's bins vary. The rows are written by the same `census()` call that
prints the counts, so a picture can never describe a different run from the
numbers beside it, and the census gains `…DumpRows`.

Its own self-check: the dump bake's `Commonwealth.lodi` is **byte-identical**
(`sha1 03cd16d4…`) to the fixture the pictures were rendered from, and
`DumpRows 4100` / `DumpRows 2449` equal `horizonRefuteSamples` /
`vhorRefuteSamples` exactly.

**The camera is measured, not assumed.** `WW_RENDER_VIEW=8` is `ViewUser`, an
oblique orthographic view, and a red dot on the wrong pixel would be a made-up
claim about where the disagreements are. `compose.py` therefore translates the
camera by three known world vectors, phase-correlates each render against the
base (an orthographic image translates rigidly), and reads the world -> pixel
columns off the result -- then CHECKS the map against a fourth translation it
never saw. Close framing: columns (+0.184,+0.088), (+0.196,-0.082),
(0.000,+0.242) px per world unit, check off by **0.80 px**. Wide framing:
(+0.0585,+0.0275), (+0.0620,-0.0260), (0.000,+0.0765), check off by **0.36 px**.
A check worse than 1.5 px stops the script before it draws anything.

Red = terrain texel, orange = LOD vertex. What the pictures show:

| picture | marks in frame |
|---|---|
| `close_e05_a120` / `close_e05_a240` | 4 / 1 |
| `close_e15_a120` / `close_e15_a240` | 16 / 1 |
| `close_e30_a120` / `close_e30_a240` | 17 / 17 |
| `full_e05_a120` / `full_e05_a240` | 8 / 7 |
| `full_e15_a120` / `full_e15_a240` | 52 / 35 |
| `full_e30_a120` / `full_e30_a240` | 149 / 161 |

They are **not** a seam or a patch: they sit along shadow BOUNDARIES -- the
deck's shadow edge, the tree line west of the boat -- which is where a
quantised horizon and a continuous cast must differ, and which the cause table
already calls `…CauseEdge`. That is the shape a reader should expect from the
numbers in section 7, and it is the shape the pictures have.

### 6.4 What the pictures are NOT

They are the debug channel, not the game: a hard lit/dark test with one degree
of softening, no albedo, no N.L. The engine-side rule this feeds is in the
contract (`docs/FO4CS_IMPROVED_LOD_PLAN.md` s9): the horizon term MULTIPLIES
N.L, it does not replace it, and 12.1% of vertices being permanently dark in
this channel is exactly why.

## 7. The gate (step 6): `tests/spells/lodgen_horizon.sh`

```
cd /e/Projects/NifskopeWildWastelandEdition
bash tests/spells/lodgen_horizon.sh                 # G4, G1, G2, G3, G3b
NEIGHBOURS=1 bash tests/spells/lodgen_horizon.sh    # and G5
```

Run 3, the one this section reports, is
`scratchpad/horizon1_20260918/gate_run3.log`. Every run prints the exe it is
about to use, its mtime and size, and the fixture paths, as its first four
lines -- a gate that does not say which binary it tested is a gate that can
pass on yesterday's:

```
exe   .../release/NifSkope.exe  (2026-09-18 21:59:46.760044700 +0200 22952448 B)
v8    .../horizon1_20260918/v8/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi
sheet .../horizon1_20260918/v8/vt/FO4CSLOD/Commonwealth
```

### 7.1 The counts

| block | what it asserts | green | red controls | failures |
|---|---|---|---|---|
| **G4** synthetic: one box, one receiver, `atan()` -- no ESM, no bake, no fixture | 11 claims x 3 doors (default, `WW_HORIZON_TEST=1`, 32 bins) | 14 | 2 | 0 |
| **G1** the way back: `--lodi-v7`, `--no-terrain-horizon` | 5 | 5 | -- | 0 |
| **G2** layout, refuters, refusals by name | 13 refuters + reader agreement + 4 refusals | 19 | 3 | 0 |
| **G3** the ray-cast floor (>= 97%) | object stream, terrain sheet, and a rotated control for each | 3 | 2 | **1** |
| **G3b** the viewer draws the stream and says so | 5 | 5 | 1 (inside the 5) | 0 |
| **G5** the neighbours | section 8 | | | |

**G1 is the part a reader should check first**, because it is the one that says
this lane can be switched off: `--lodi-v7` gives a `.lodi` **byte-identical** to
the v7 fixture (243,420 B, sha `4eb2fc55d5f4`) and a byte-identical `.lodo`
(6,204,388 B, sha `fa993ce1d576`); `--no-terrain-horizon` gives byte-identical
`.lodt` containers (3 compared, 0 differ); the `.lodm` differs **only** by
horizon keys that say OFF (1,950 B rung, 2,021 B way back); and the v7 reader
refuses a v8 file **by name** -- `native REFUSED Commonwealth.lodi: version 8;
this reader knows 3, 4, 5, 6 and 7` -- rather than reading it wrong.

### 7.2 Every refuter red at least once

A green refuter proves nothing on its own: it has to be shown failing on a field
that is wrong in the one way it is supposed to catch. Eight controls, each run
in the same process as the claim it guards:

| control | the perturbation | what it scored | the claim it kills |
|---|---|---|---|
| G4 "box REMOVED" | the same field, no occluder | -0.007 deg, outside [12.915, 14.894] | claim 1, the magnitude |
| G4 "off by TWO cells" | a cast displaced two lattice cells | 15.020 deg, outside the window | that the window is one cell wide and no wider |
| R2 constant field | every byte replaced by one value | hi == lo (75..75) | "the stream is not a constant" |
| R3 shuffled | the same per-vertex means, dealt to other vertices | pearson r = **-0.0111** vs the field's **-0.4070** | "horizon and sky disagree" |
| R4 isotropic | every profile replaced by its own mean | **100.00%** survive a quarter turn vs the field's **0.00%** | "a rotation is a different field" |
| G3 object control | the stored bins read 90 deg from the sun | **60.59%** over the 504 receivers whose bins vary (91.25% over all 2,449) | the object floor |
| G3 terrain control | the same, per texel | **50.00%** | the terrain floor |
| G3b viewer control | `WW_HORIZON_BIN_ROT=4` | a byte-wise DIFFERENT picture, and the viewer prints *"This picture is SUPPOSED to be wrong"* | that the viewer reads the azimuth at all |

Plus five refusals by name, which are the controls on the argument parser:
`--horizon-azimuths 7`, `--horizon-azimuths 128`, `--horizon-reach 0`,
`--horizon-refute nonsense`, and `WW_LODL_CHANNEL=horizonbin=x` -- each refused
with the offending token quoted and the legal set listed.

**Where this is thin, stated rather than hidden:** R1 (the stream is exactly
`4(n+1) + A x vertices` bytes, every AO placement has a horizon slice) and R5
(one level carries role 7, the sheets are last and contiguous, four bins a
sheet, DXGI 28) have **no red control of their own**. They are arithmetic over
the file's own header, and the nearest thing to a control they have is G1's
refusal line and the four argument refusals. A layout that satisfied R1 and R5
while carrying nonsense in the bytes is caught by R2/R3/R4/G3, not by R1/R5.

### 7.3 The one failure, unchanged and located

```
FAIL G3 horizonRefute agrees with the reference cast --
     worst of the eight sun positions 51.69% over 4100 samples, floor 97%
```

This is the **terrain** half. The object half passes: **97.73%** worst of eight
over 2,449 sampled vertices, against a control at 60.59%.

The score is a **balanced** agreement -- the mean of the lit rate and the dark
rate, with a class the reference never produces left out rather than scored --
and that is what exposes this. Unbalanced, the same field reads
`horizonRefuteRawWorst` **93.17%**, because at a low sun nearly everything is
dark and a field that says "dark" everywhere is right about nearly everything.

Where it fails, exactly:

| sun | agreement | share the reference calls lit |
|---|---|---|
| A120 E5 / A240 E5 | 100.00 / 100.00 | 0.00% / 0.00% |
| A120 E15 / A240 E15 | 75.65 / **51.69** | 1.71% / **1.44%** |
| A120 E30 / A240 E30 | 91.40 / 83.76 | 12.66% / 10.54% |
| A120 E60 / A240 E60 | 94.60 / 93.12 | 35.15% / 36.32% |

At 5 degrees there is **not one disagreement**, because the reference calls
every sampled texel dark and so do the bins. The failure is at **15 degrees**,
where the reference finds 59 lit texels of 4,100 and the stored bins agree on
about two of them: **the terrain sheet over-occludes at a low sun.** It gets
better as the sun rises, which is the signature of a horizon that is a little
too HIGH everywhere rather than one that is wrong somewhere.

The cause counters say the same thing from the other end:
`CauseCoarseOver 351`, `CauseGrowthUnder 277`, `CauseAzimuth 248`,
`CauseEdge 58`; mean error **3.651 deg**, max 51.310, mean reference elevation
63.565 deg (60.041 as a pencil). A terrain texel sits ON the ground, so its own
landscape fills most of its sky and a 3.6-degree bias moves a 15-degree sun
across the line. The object stream's vertices sit above the ground -- mean
reference elevation 18.875 deg -- and the same 0.673-degree mean error moves
almost nothing.

**It is reported, not re-derived.** Nothing about the mechanism says 97% is
unreachable for the terrain; the honest statement is that this lane ships the
terrain sheet failing its own floor at a 15-degree sun, with the position, the
population and the cause printed, and `--no-terrain-horizon` bakes without it
while the object stream (which is what bungo asked for: *"shadows for distant
LOD objects"*) passes.

## 8. The neighbours (G5)

`NEIGHBOURS=1 bash tests/spells/lodgen_horizon.sh`, run 3, on the 21:59:46 exe.
Each harness's own log is `scratchpad/lodgen_horizon_gate/<name>.log`. The
standing counts in the middle column are the owners', quoted in the brief before
this lane started; they are what "unchanged" is measured against.

| harness | standing | this exe | |
|---|---|---|---|
| `lodgen_native.sh` | RESULT PASS | **RESULT FAIL, 1 of 47** -> fixed, below | the one regression |
| `lodi_v7.sh` | PASS | 12 ok, 0 failed, 0 skipped | unchanged |
| `lodl_channels.sh` | PASS | PASS | unchanged |
| `lodgen_slab.sh` | 16/0 | 16 checks, 0 failures | unchanged |
| `native_open.sh` | 17/0/2 | 17 checks, 0 failures, 2 skipped | unchanged |
| `render_shot.sh` | 82/0 | 82 checks, 0 failures | unchanged |
| `lodl_open.sh` | 23/0 | 23 checks, 0 failures | unchanged |
| `native_lighting.sh` | **14/3 BEFORE this lane** | not run | NOT this lane's to fix, and saying so is not the same as fixing it |

Gate total for run 3: **22 ok, 1 failed, 0 skipped** -- the one failure being
G3's terrain floor (section 7.3), not a neighbour.

### 8.1 The one regression, and it is a version list

```
FAIL j0 the .lodo is at version 4 and the .lodi at 3, 4, 5, 6 or 7 (4 / 8)
```

`lodgen_native.sh` (lane NATIVE1c's) asserts which `.lodi` versions it knows.
This lane wrote version **8**, so the assertion is right to fire: it is the
"refuse a version you do not know" discipline pointed at a harness instead of a
reader, and it is the same edit LODIV7 made when it added 7.

Fixed in `tests/spells/lodgen_native_fields.py`, one tuple and its message:
`(3, 4, 5, 6, 7)` -> `(3, 4, 5, 6, 7, 8)`. Nothing else in the claim moved, and
no other assertion in that harness was touched. Re-run on the same exe:

```
lodgen_native.sh rc=0 -- RESULT PASS
  ok   j0 the .lodo is at version 4 and the .lodi at 3, 4, 5, 6, 7 or 8 (4 / 8)
  47 checks, 0 failures, 3 skips   (the leg j0 belongs to)
  scratchpad/horizon1_20260918/neigh_native_after.log, 2026-09-18 22:50
```

### 8.2 One stale thing in the same neighbour, reported and NOT fixed

`lodgen_native.sh` skips its whole `k` block on a v8 file with the reason *"this
.lodi is version 8: the vertex-AO stream is a version 6 payload and this file
carries none"*. **That reason is false.** A v8 file carries the vertex-AO stream
-- this lane's own bake streams 53,396 AO bytes over 2,449 placements, and the
horizon stream is defined as `A x` that same slice. The gate is written
`if ih['version'] == 6`, so it has been skipping on every v7 file since LODIV7
too; it is pre-existing, it is the owner's, and widening it means re-reading
four assertions against the v7/v8 offsets rather than editing a tuple.

It is left alone deliberately and named here because a SKIP that states a wrong
reason is worse than a failure: it reads like coverage. The director's call, or
whichever lane next touches that harness.

## 9. The build

| | |
|---|---|
| exe | `E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe` |
| mtime | **2026-09-18 21:59:46.760044700 +0200** |
| size | **22,952,448 B** |
| sha1 | **`e578b76f9d7a2d011363e4300a2c94967e14d605`** |
| rung (the exe as it stood before this lane) | `release/NifSkope.before_horizon1.exe`, 2026-09-18 19:05:11, 22,764,544 B, sha1 `28ac412c6da96e0707e492c175822d2076016dc5` |

**Nothing under `src/`, `tests/` or `res/` is newer than the exe.**
`find src tests res -type f -newer release/NifSkope.exe` returns nothing, which
is the only statement that makes every number above about *this* code.

```
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?     # its own command, before every build
export PATH=/c/msys64/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH
touch src/nifcli.cpp src/lodgen.cpp src/nativeemit.cpp
mingw32-make -f Makefile.Release -j8
```

**The `touch` is mandatory and this is why.** `src/lodghorizon.h` and
`src/lodghorizonrefute.h` are listed in `NifSkope.pro` (21:02) but
`Makefile.Release` was generated at 03:59 and `grep -c lodghorizon
Makefile.Release` is **0**: the generated makefile has no dependency on either
header, so editing one and running `make` links yesterday's objects and reports
success. Three translation units include them --- `nifcli.cpp`, `lodgen.cpp`,
`nativeemit.cpp` --- and those are the three that get touched. A lane that
re-runs `qmake` instead would be re-generating the makefile under LODIV7's
uncommitted work, which is not this lane's to do.

Files this lane wrote or changed:

| new | `src/lodghorizon.h`, `src/lodghorizonrefute.h`, `tests/spells/lodgen_horizon.sh`, `tests/spells/lodgen_horizon_refuters.py` |
|---|---|
| **changed, writer** | `src/lodgen.{h,cpp}`, `src/nativeemit.{h,cpp}`, `src/nifcli.cpp`, `src/lodtsheets.{h,cpp}` |
| **changed, format** | `src/lodifile.{h,cpp}` (v8), `src/io/lodvfile.{h,cpp}` (role 7) |
| **changed, viewer** | `src/lodinative.{h,cpp}`, `src/btdterrain.cpp` |
| **changed, readers** | `tests/spells/lodgen_native_decode.py`, `lodgen_native_fields.py`, `lodgen_native_mutate.py`, `lodgen_vt_check.py` |
| **changed, docs** | `docs/LODGEN_NATIVE_LODO_LODI.md`, `docs/LODGEN_TERRAIN_VT.md`, `docs/LODGEN_CENSUS.md`, `docs/FO4CS_IMPROVED_LOD_PLAN.md` |

Nothing was committed and nothing was stashed; `WW_CHANGES.md` and `HANDOFF.md`
are untouched and their text is delivered in section 11.

## 10. Docs, the consumer contract, and the skill text

### 10.1 What was written

| page | what it now says |
|---|---|
| `docs/LODGEN_NATIVE_LODO_LODI.md` §4.11 | the v8 header words (`0x11C` offVertexHorizon, `0x124` vertexHorizonBytes, `0x128` horizonAzimuths, `0x12A` horizonSteps, `0x12C` horizonReach), the stream layout (`u32 first[n+1]` BYTE offsets then `A` bytes a drawn vertex), the bin convention with the figure, and the WHO ACTUALLY GETS A FAR SHADOW table (the three classes, their shares, their sky bytes) |
| `docs/LODGEN_TERRAIN_VT.md` §3.5 | role 7: four bins an RGBA sheet, DXGI 28, role 7 last and contiguous, why **no block compression**, `--vt-horizon-texel` and the level it picks |
| `docs/LODGEN_CENSUS.md` §6.1 | every new census token, including `…A<az>E<el>` and its `…Lit` partner, `…ControlDirWorst`/`…DirSamples`, `…RawWorst`/`…RawControlWorst`, `…MeanRefPencilDeg`/`…ConeRays`, `…DumpRows`, and the `--horizon-near-skip` sweep, **re-measured on the shipping exe** (§10.4) |
| `docs/FO4CS_IMPROVED_LOD_PLAN.md` §9 | **the contract**, below |

### 10.2 The contract, in one paragraph

`docs/FO4CS_IMPROVED_LOD_PLAN.md` §9 "The far-shadow contract (2026-09-18
rulings)", four rulings with provenance per line: **(i)** a LOD receiver shades
from the horizon field and nothing else, by ONE compare --
`h = lerp(bin[k0], bin[k1], frac); lit = smoothstep(h - w/2, h + w/2, sunEl)` --
with `w` a named knob (`LODL_HORIZON_SOFT_DEG`, default 1.0 deg), the horizon
term MULTIPLYING the sun term and never touching ambient; **(ii)** the existing
far shadow map is not deleted, keeps the terrain and the near world, and its
caster set stays the cell-range table; **(iii)** the v7 group table is the
witness in the grid-edge band and nowhere else; **(iv)** four prohibitions --
a LOD receiver may not sample the far map (two answers for one pixel is the
artifact class bungo named), the field may not be recomputed at runtime, the
bins may not be re-encoded or re-ordered on upload, and a missing stream is
UNSHADOWED, never black. The cost table is in the same section and is quoted in
section 12.

### 10.3 The `nifskope-ww-render-shot` skill text (director applies to both trees)

*Two rows for the `WW_LODL_CHANNEL` table, after `normal`:*

```
| `horizon` | the horizon skyline shaded against a sun: lit / dark by ONE compare, objects and terrain in one picture | `.lodi` v8 stream (0x124) x `.lodt` role-7 sheets |
| `horizonbin=<n>` | ONE bin as grey, 0 deg black .. 90 deg white -- the compass rose, bin by bin | the same bytes, bin `n` only |
```

*And a fifth bullet under "Four things that decide whether the picture is worth
taking" (which becomes five):*

```
* **The horizon names need a SUN, and the sun has a control.** `WW_SUN=<az>,<el>`
  in degrees (default **120,15**; a malformed value is REFUSED by name, never
  silently noon), `WW_HORIZON_SOFT_DEG=<w>` the smoothstep width in degrees
  (default **1.0**, `0` = a hard edge, clamped at 45), and
  `WW_HORIZON_BIN_ROT=<k>` = **THE CONTROL**: the bins are read `k` bins away
  from the sun's azimuth, the note line says *"THE CONTROL IS ON ... This picture
  is SUPPOSED to be wrong"*, and a control render that is hard to tell from its
  neighbour means the azimuth is not being read and every other horizon picture
  is decoration. Take it beside the real one. The azimuth is measured the way
  the bins are stored -- **bin 0 = NORTH (+Y), clockwise toward EAST** -- so a
  picture that is smooth, stable and a quarter turn wrong looks perfect.
  A vertex with no slice (a v7 file, or a placement the bake skipped) is drawn
  **magenta** and named on the note line: that is "no data", not "in shadow".
  `WW_RENDER_FLAT=1` applies, as for the other flat-byte names. Gate:
  `tests/spells/lodgen_horizon.sh` (G3b), and `lodl_channels.sh` for the name.
```

*The bake-side knobs belong in `nifskope-ww-lodgen` rather than here:*
`--horizon-azimuths <4..64 step 4>` (default 16), `--horizon-reach <u>`
(default 127,561 = the tallest placement at a 5-degree sun),
`--horizon-near-skip <0..8>` (default 1: never read a max-Z lattice closer than
one of its own squares -- 0 lets a receiver's own building into its horizon and
moves the mean from 63.22 to 69.78 deg), `--vt-horizon-texel <16..4096>`
(default 128 u, picks the coarsest level no wider than that),
`--horizon-refute <n>` (an independent cast inside the bake, `n` samples),
`--horizon-selftest` / `WW_HORIZON_TEST=1` (the synthetic box, no ESM needed),
`WW_HORIZON_REFUTE_DUMP=<dir>` (one CSV row a sampled receiver, written by the
same `census()` call that prints the counts), and the two ways back,
`--lodi-v7` and `--no-terrain-horizon`.

### 10.4 One number in those pages was stale, and it is corrected

While checking section 12's figures against the shipping bake I read the
`--horizon-near-skip` sweep back out of the three pages that carry it: **69.78
deg at 0 squares, 64.79 at 1, 45.53 at 8**. Two paragraphs above it, the same
page quotes the shipped bake's mean as **63.2**. Both cannot describe the
configuration that ships, and `grep` over every bake log this lane wrote finds
69.78 and 63.22 and no 64.79 anywhere: the sweep was measured correctly and then
the march changed under it (the nine-pencil reference, and `maxAlong`'s mip
choice), and it was not re-run because it read like a property of the RULE
rather than a measurement of a BUILD.

Corrected in all three pages against logs from the exe that ships:

| `--horizon-near-skip` | mean terrain horizon, chunk 4.4.-12 | log |
|---|---|---|
| 0 squares (the way back) | **69.78 deg** | `v8/bake.nearskip0.log` |
| **1 (default)** | **63.22 deg** | `v8/bake.log` |
| 8 | **40.95 deg** | `ns8/bake.log` |

The same read caught a smaller one: the object half's cone-vs-pencil gap was
quoted as 18.843 against 18.217 where the shipping bake says **18.875** against
**18.249**. Root `MISTAKES.md` carries the rule.

## 11. The text for the two documents this lane may not edit

### 11.1 `WW_CHANGES.md` -- the paragraph

```
**Far LOD shadows, baked as horizon maps (`.lodi` v8 + `.lodt` role 7).** Every
drawn LOD vertex now carries a skyline: 16 bytes, one per compass bin, each the
elevation of the highest thing it can see in that direction over a reach of
127,561 units (the tallest placement in the Commonwealth at a 5-degree sun),
encoded `u8 = round(deg / 90 x 255)` -- one step 0.3529 degrees. The terrain
carries the same thing per texel in four new role-7 sheets. Shading a far
shadow is then one compare against the sun's elevation, with no caster list, no
second pass and no identity problem -- which is what stopped the far shadow map
from ever doing this for objects. A tower two kilometres away casts on you from
behind at a low sun because its height is in YOUR bytes, not in a map that has
to know what it is. Both halves have a way back that is byte-identical:
`--lodi-v7` and `--no-terrain-horizon`. Sizes: 864 KB for a 16-cell chunk's
objects, about 23 MB projected over the Commonwealth by live REFR count, and
0.85 GB for the terrain sheets at the default 128-unit texel (uncompressed,
because BC would quantise an angle and blend north into east). The viewer draws
it: `WW_LODL_CHANNEL=horizon` and `horizonbin=<n>`, with `WW_SUN`,
`WW_HORIZON_SOFT_DEG`, and `WW_HORIZON_BIN_ROT` as the named control. Gate
`tests/spells/lodgen_horizon.sh`. KNOWN FAILURE, reported rather than
re-derived: the terrain sheet agrees with an independent full-resolution cast
97.7% of the time on the OBJECT stream but only 51.7% on the terrain at a
15-degree sun, where it over-occludes; the object stream is the half bungo
asked for and it passes.
```

### 11.2 `HANDOFF.md` -- the LANDED block

```
### LANDED 2026-09-18 22:xx -- HORIZON1: far LOD shadows as baked horizon maps (option B)

Exe `release/NifSkope.exe` 2026-09-18 21:59:46, 22,952,448 B,
sha1 e578b76f9d7a2d011363e4300a2c94967e14d605 (rung:
`release/NifSkope.before_horizon1.exe`, 19:05:11, 28ac412c6da9). **His open
NifSkope window is running the old binary and needs a restart.**

WHAT: `.lodi` v8 carries a per-vertex horizon stream (A bytes a drawn vertex,
default A=16, bin 0 = NORTH clockwise toward EAST, u8 = deg/90x255, reach
127,561 u, 22 steps a bin); `.lodt` gains role 7, four bins an uncompressed
RGBA sheet, last and contiguous, DXGI 28. One vertex population with AO (v6)
and sky (v7): the horizon slice is exactly A x the AO slice. Viewer:
`WW_LODL_CHANNEL=horizon` / `horizonbin=<n>`, `WW_SUN`, `WW_HORIZON_SOFT_DEG`,
`WW_HORIZON_BIN_ROT` (the control). Contract for FO4CS:
`docs/FO4CS_IMPROVED_LOD_PLAN.md` §9, four rulings.

GATE: `tests/spells/lodgen_horizon.sh` (`NEIGHBOURS=1` for G5).
G1 both ways back byte-identical (.lodi 4eb2fc55d5f4, .lodo fa993ce1d576, 3
.lodt containers, .lodm differs only by keys that say OFF, old reader refuses
v8 by name). G2 13 refuters + 3 red controls + 5 refusals. G4 the synthetic box
green on three doors with 2 red controls. G3b the viewer, 5 green.
**G3 STANDING FAILURE, honest: the terrain sheet scores 51.69% balanced
agreement against an independent cast at a 15-degree sun (floor 97%), because
it over-occludes; the object stream scores 97.73% against a control at 60.59%.**
G5 the neighbours, all seven on this exe: lodi_v7 12/0, lodl_channels PASS,
lodgen_slab 16/0, native_open 17/0/2, render_shot 82/0, lodl_open 23/0 -- every
one at its standing count. lodgen_native.sh failed on ONE claim, its own list of
known .lodi versions (j0, `3,4,5,6 or 7`), which version 8 is right to break;
the tuple learned 8 and it is back to RESULT PASS. native_lighting.sh was 14/3
before this lane and is not this lane's to fix.

COSTS (bungo asked): runtime = one lerp + one smoothstep a vertex, one bilinear
a terrain texel, no second pass. Disk = 864,136 B for chunk 4.4.-12's objects,
~23 MB projected over the Commonwealth, 0.85 GB for the terrain sheets at 128 u
(0.21 GB at 256 u, half again at `--horizon-azimuths 8`).

NEXT, if the director wants it: the terrain floor. It is a bias, not noise --
mean error 3.65 deg over a mean reference elevation of 63.6 deg, best at a high
sun, worst at 15 degrees -- so the candidates are the tangent-plane rule at the
receiver's own square and the mip choice inside `maxAlong`, both of which moved
this number once already (53.57 -> 75.65 -> the current worst).
Report: `scratchpad/horizon1_20260918/lane_horizon1_report.md`.
```

## 12. The rows for bungo

**The long-edge row (step 1a), which is the honest limit of this design.** A
horizon byte lives on a VERTEX, so a shadow edge that crosses a triangle is
smeared over that triangle's longest edge. Over the urban region's 250,320
drawn triangles:

| bar | triangles that cannot resolve a shadow edge finer than this | share |
|---|---|---|
| 512 u (a quarter cell) | 41,042 | **16.4%** |
| 1,024 u | 7,926 | 3.2% |
| 2,048 u (half a cell) | 664 | 0.27% |

Half the drawn edges are under 240 u and 84% under 512, so most of the far
field is finer than a quarter cell; the worst single edge in the region is
5,476 u. **No subdivision is proposed** -- "authored LODs only" is his standing
rule and the brief forbids it -- so this table is the thing to look at if a
shadow ever looks blocky on a big flat wall.

**The azimuth count, with the bytes.**

| A | chunk 4.4.-12 | urban region | Commonwealth (x2.98 by live REFR) | what it costs in look |
|---|---|---|---|---|
| 8 | 436,968 B | 3.87 MB | **~12 MB** | 45 deg a bin: a wall's shadow direction is right to within 22 deg |
| **16 (default)** | **864,136 B** | **7.61 MB** | **~23 MB** | 22.5 deg a bin |
| 32 | 1,718,472 B | 15.10 MB | ~45 MB | 11.25 deg a bin |

**The terrain sheet, which is the big number.** Uncompressed RGBA, four bins a
sheet, 16 bytes a texel:

| texel | content bytes a cell | STORED over 36,864 cells (content + the 8-texel border + two mips, x1.41) |
|---|---|---|
| 64 u | 65,536 | 3.41 GB |
| **128 u (default)** | **16,384** | **0.85 GB** |
| 256 u | 4,096 | 0.21 GB |

Every terrain figure in this report and in the three documents is the STORED
one, because that is what lands on disk; the content-only numbers are 1.41x
smaller (0.60 GB at 128 u) and two of the pages were quoting those beside
stored ones until this section was written.

**The other two knobs.** Reach **127,561 u** = the tallest placement measured
(11,160 u over its own land, `GreebTower02_LOD_0.nif`) divided by tan(5 deg);
at 22 steps growing x1.5 from 32 u that is the whole distance a shadow can
arrive from. Softening width **1.0 deg** (`WW_HORIZON_SOFT_DEG`,
`LODL_HORIZON_SOFT_DEG`), which at a 15-degree sun is about a 7% band --
`0` gives a hard edge and is legal.

**G6, the pre-registered size gate, scored honestly.** The object stream landed
**exactly** on step 1c's projection: 864,136 B predicted, 864,136 B measured
(854,336 of stream + 9,800 of offsets), 0.0% out, inside the +/-10% line. The
terrain sheet **missed its pre-registration**: 8,192 B a cell was projected for
BC3 at 64 u, and the bake writes **16,384 B a cell at 128 u** -- 2x. The reason
is a decision taken after the projection and stated in R5: an angle may not be
block-compressed (BC3 interpolates RGB jointly, so it would blend north into
east), so the sheets are uncompressed and the answer to the size is a coarser
texel. The projection is not amended after the fact; the miss and its reason
are what this row says.

One more thing this row has to say plainly, because the fixture's own census
looks worse than the table: chunk 4.4.-12 is a ONE-chunk bake whose pyramid
stops at a 32-unit texel, so `lodgenVtHorizonLevel` -- "the coarsest level no
wider than `--vt-horizon-texel`" -- had no 128-unit level to pick and wrote role
7 at **32 u**, `horizonLevel 2 horizonTexel 32`, 278,784 texels, 4,460,544 bins.
That is 262,144 content bytes a cell, sixteen times the shipping default, and it
is a property of a 16-cell fixture rather than of the feature. A worldspace bake
reaches the coarse levels and lands on the 128-unit row. The fixture's number is
quoted here so nobody reconciles the two by guessing.

## 13. The MISTAKES entries this lane filed

Six, at the **top** of the root `MISTAKES.md` (the file's own header says
"Newest at the top" -- I first appended them at the bottom and moved them):

1. **A control scored over a population it cannot fail on is not a control.**
   72.8% of drawn vertices carry an isotropic horizon, and a quarter-turn
   rotation is arithmetically the identity on those, so the control's 91.25
   was mostly the control agreeing with itself. Scored where it can move
   something: 60.59. The rule: before reading a control's number, ask what
   fraction of the population its perturbation can even change, and PRINT that
   population's size.
2. **A pre-registered threshold the mechanism cannot meet is re-derived in
   public, or it is goalpost-moving.** R4 was a population error and got
   *tightened* (to 1%); R3 was a population error and the threshold did NOT
   move; R2 was genuinely unreachable after a later rule and was replaced by
   two claims the mechanism does imply, with the old line printed beside them;
   and the terrain's 97% floor was NOT re-derived, because nothing says the
   mechanism cannot meet it.
3. **I named a population "open sky", and its own sky bytes said otherwise.**
   The all-zero class has mean sky 104.2 against the directional class's 161.6
   -- it is the second least open of the three, not the most. A name for a
   population is a claim about it; check it against another stream that
   measures the same units before reasoning from it.
4. **A reference that measures a different quantity reports the difference as
   error.** The stored byte is a maximum over a 22.5-degree sector; the
   reference was one pencil ray. Spreading it over nine pencils and fixing a
   mip choice that blurred twice the bin took the cause counter from 1,621 to
   351 and the terrain's worst from 53.57 to 75.65. Both slips were in the
   comparison and both looked like march bugs.
5. **Two process slips**: a patch that REPLACED a check inserted the new one
   and left the old one standing, so the gate printed a FAIL under the `ok`
   that had already answered it; and a projection is measured or it is
   invented (the camera map for the pictures was measured from five renders
   and checked against a sixth).
6. **A measured sweep survived the code it measured, in three documents at
   once.** The `--horizon-near-skip` sweep read 69.78 / 64.79 / 45.53 degrees
   in three pages while the shipped bake's own census said 63.22 at the middle
   point, and `grep` over every bake log in the lane found no 64.79 at all: the
   march changed underneath the measurement and the measurement was not re-run.
   A number in a document names the log it came from, and a page that
   contradicts itself on one screen is telling you which half is stale (§10.4).
   The same read caught the terrain sheet's size quoted in TWO conventions --
   0.60 GB of content in the contract, 0.85 GB stored in this report -- neither
   wrong and together unreadable, so every terrain figure is now the STORED one
   and says so where it is quoted.

## 14. The finished-work skill review

**Invoked, and they earned it:** `nifskope-ww-build-verify` (make's exit code
is the gate, and the `touch` rule in section 9 is its lesson),
`ww-test-harness-add` (the gate's shape: fixtures named when absent, SKIP is
not PASS, the exe printed first), `ww-channel-view-refuter` (a channel whose
render is byte-identical to the default is NOT WIRED -- G3b's "the shadow moves
with the sun" is exactly that test), `ww-population-refuter` (the three-class
split is its method applied to a stream rather than a sheet),
`nifskope-ww-render-shot` + `ww-texel-picture` (the twelve and the bin grid),
`ww-control-calibration` (every control here).

**One new skill, written**, because the procedure was invented twice in this
lane (once per framing) and is reusable by any lane that has to draw a world
position onto a render:
`scratchpad/horizon1_20260918/skills_proposed/ww-measure-the-render-camera/SKILL.md`
-- measure the world-to-pixel map from four renders by phase correlation, CHECK
it against a fifth the map never saw, stop the script above 1.5 px, and write
the overlay's data from inside the same pass that prints the numbers.

**Two deltas to existing skills, delivered as text rather than applied**, since
both files are shared with the other tree:

* `ww-control-calibration` gains the section-13.1 rule: *a control must be
  scored over the population its perturbation can change, and that population's
  size is printed beside the score* (`…ControlDirWorst` / `…DirSamples` is the
  census shape for it).
* `nifskope-ww-render-shot` gains section 10.3 above.

**Considered and rejected:** a skill for "the reference must measure the same
quantity as the stored value" (mistake 4). It is a one-line rule, it already
lives in the ledger, and `ww-analytic-fixture-gate` is where a reader would
look for it -- a skill that repeats a ledger entry makes both harder to find.

---

DONE

## Five sentences for bungo

1. Every distant LOD object now carries its own little skyline -- sixteen bytes
   saying how high the tallest thing it can see is in each compass direction --
   so a tower two kilometres behind you casts its shadow on you at a low sun,
   with no shadow map, no caster list and none of the identity trouble that
   stopped the old far map from ever doing this.
2. It costs almost nothing to draw (one blend between two bytes and one
   comparison against the sun) and about 23 MB of disk for the whole
   Commonwealth's objects, which is the cheap half of your question.
3. The terrain half is bigger, about 0.85 GB at the default texel, because an angle
   cannot be block-compressed without blending north into east, and the honest
   answer to the size is a coarser texel rather than a compressor.
4. The object shadows pass their own independent ray-cast check at 97.7%, but
   the terrain sheet only manages 51.7% at a low sun because it makes the
   ground too dark -- that failure is reported, not hidden, and both halves have
   a switch that turns them off and gives byte-identical files.
5. His open window needs a restart.
