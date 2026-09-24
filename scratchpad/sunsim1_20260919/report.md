# Lane SUNSIM1 -- an offline picture of the sun casting shadows on chunk 4.4.-12

## 0. Clock, inputs found

`date` at lane open: **Sat Sep 19 02:47:07 CEDT 2026**.

Tree is READ-ONLY for this lane except `scratchpad/sunsim1_20260919/`. HORIZON3 is live in
`src/` and `tests/`; nothing there is touched, nothing is built, `release/NifSkope.exe` is never
run. Fallout4 is up. CPU Python only, <= 6 workers, no GPU.

### Inputs located on disk

| what | path | note |
|------|------|------|
| worldspace heightmap dump | `scratchpad/horizon2_20260918/land.bin` | 80,326,672 B, `--dump-land`, int16 heights in units of 8 on a 128 u node grid, whole Commonwealth |
| heightmap reader | `scratchpad/horizon2_20260918/wit.py` class `Land` | bilinear `at()`, NaN where no LAND |
| placement boxes builder | `scratchpad/horizon2_20260918/boxes.py` | world AABBs; this lane needs triangles, so boxes are a cross-check only |
| independent `.lodo`/`.lodi` decoder | `tests/spells/lodgen_native_decode.py` | read-only use |
| baked native far field | `scratchpad/horizon2_20260918/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth.lodo` (6,204,388 B) + `.lodi` (1,109,896 B) | HORIZON2 re-bake 2026-09-19 01:02 |
| baked VT sheets | `scratchpad/horizon2_20260918/dumpbake/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.*.lodt` | role-7 horizon sheets |
| role-7 sheet reader | `tests/spells/lodgen_horizon_witness.py` class `HorizonSheet` (+ `lodgen_vt_check.Lodv`) | byte order `SHIFT=(0,8,16,24)`, bin j = byte j |
| bin convention | HORIZON2 report s1.1/s1.5 | 16 bins, bin 0 = NORTH, clockwise toward EAST, `u8 = round(deg/90*255)`, Z up |


## 1. What is and is not in the "truth" cast -- READ THIS BEFORE THE PICTURES

The LEFT panel of every pair is the best cast this lane could make offline in Python. It is not
the game and it is not a reference renderer. Everything below is what it does and does not
contain. Nothing here is a claim that the bake is fixed, correct or final.

**In the truth cast**

* The **full-resolution heightmap** of the whole Commonwealth, not a crop: `land.bin`, 6145 x 6145
  nodes at 128 u spacing, read bilinearly. The shadow test reaches **150,000 units** from the
  chunk in every direction. At a 5-degree sun that is a blocker allowance of 13,100 units of
  height, and the highest ground anywhere in the dump is about 4,400 units above the lowest point
  of this chunk, so at 5 degrees the reach is roughly three times what the terrain can use. A
  5-degree sun is honest here.
* The **actual `.lodo` triangles** of every placement in `Commonwealth.lodi`, transformed to
  world: 2,449 placements, 53,396 vertices, 29,587 triangles, world Z from -1,424 to 2,949.
* Lambert `N.L` times lit/shadow plus a small sky ambient, grey albedo, terrain slightly warmer
  than the objects, no textures, a two-band sky and distance haze.

**NOT in the truth cast -- state these to anyone who reads the pictures**

1. **Neighbouring chunks' objects are absent.** `Commonwealth.lodi` from this bake carries three
   chunks only: cx=0,cy=-3 with 2 instances, **cx=1,cy=-3 with 2,446 (ours)**, cx=2,cy=-3 with 1.
   A building one chunk east casts no shadow into our chunk in the LEFT panel, because the bake
   this lane was given does not contain it. The terrain of the neighbours IS present.
2. **Trees are impostor cards.** The LOD library carries tree crowns as a handful of flat
   alpha-tested quads. Nothing here is alpha-tested: a tree card casts a solid quadrilateral
   shadow, which is wrong in the direction of too much shadow. Many of the odd "spiky star"
   shapes in the pictures are exactly these cards seen edge-on; measured against each placement's
   own diagonal (p50 triangle edge 257 u, max 2,547 u, never longer than the instance itself)
   they are the real geometry, not an index bug.
3. **The shadow ray has a 16-unit footprint.** The LEFT panel does not march one pencil ray a
   pixel. It shears the world into sun space (`s = z - u tan(elevation)`) and takes one suffix
   maximum of `s` along the sun line, which answers every shadow ray in the frame at once, on a
   16-unit near grid (terrain sampled plus every triangle rasterised) and a 256-unit far grid
   (terrain only). The price is measured in s4, control A: it agrees with exact brute-force ray
   casting **82-93%** of the time and it always errs toward MORE shadow.
4. **Primary visibility of objects is rasterised, not ray-cast.** For the first hit only. A
   z-buffered rasteriser with perspective-correct barycentrics returns the same surface point a
   primary ray would; it is a substitution of method, not of answer, and it is stated here because
   the brief asked for a ray-caster.
5. **Terrain beyond 150,000 units** of the chunk is not in the far grid at all.
6. A handful of triangles are clipped away at the near plane per camera (the count is printed in
   `times.json`'s sibling log); the street camera, which stands on the ground inside the geometry,
   drops the most.

## 2. Two faults this lane found in its own reader, and how

Both were caught by the controls in s4, not by looking at pictures. Both are recorded because the
pictures BEFORE the fixes looked perfectly plausible.

**2a. `.lodo` vertex normals are octahedral 12:12, not three signed bytes.** The vertex row's
`nrm[3]` is `n = b0 | b1<<8 | b2<<16`, `octX = n & 0xFFF`, `octY = n >> 12`
(`src/lodofile.h:222`, codec `lodoUnpackOct12` in `src/lodofile.cpp`). This lane first read them as
a signed byte triple. That gave 66% of the chunk's vertices a normal at or below the horizontal,
which is not what a town looks like, and it was invisible in the render because `N.L` on a wrong
normal still shades something. Control B found it: the bake stores a flat zero for every bin of a
vertex whose normal points DOWN (`lodgenHorizonCastAt`, `nz <= -1e-3`), and that class did not
line up with the file. After the fix, 60.7% of this chunk's vertices point down -- the exact figure
`src/lodghorizon.h` itself quotes for all-zero vertices in this chunk. Everything in this report
and every image in `images/` is from after the fix.

**2b. The far shadow grid over-shadowed because it was a max-Z mip.** The far half of the sheared
grid sampled `mip_max(level 1)`, a 256-unit block maximum, so a ray was shadowed by a hilltop it
never passed. Terrain-only, against brute force at azimuth 240 / elevation 15: the near grid alone
agreed 96.7% while the mip far grid pulled the pair down to 89.6%. Replacing it with the bilinear
surface at the cell centre -- a tap every 256 units along the sun line, then the suffix maximum,
which is the march the brute force does -- lifted terrain-only agreement to 96-98%.

## 3. What the RIGHT panel is allowed to know

Nothing but the baked bytes, at the hit point.

* **Terrain** -- `Commonwealth.VT.4.lodt`, role 7. It is the LAST `azimuths/4` = 4 sheets of the
  container, contiguous, DXGI 28, one coarse level; bin *j* is BYTE *j* of the texel, so
  `SHIFT = (0, 8, 16, 24)`. levelDim 4, content 512, stored 528, border 8, 1 x 1 tiles, 32 units a
  texel, west 4 east 7 south -12 north -9. Sampled BILINEARLY IN SPACE on each of the two nearest
  azimuth bins, then those two blended by `frac` from `lodgenHorizonBinPair`, then compared with
  the sun elevation with a 1-degree soft edge.
* **Objects** -- the `.lodi` v8 per-vertex horizon stream: `u32 first[instanceCount+1]`, then
  `azimuths` bytes per library vertex of the mesh the placement draws, in that mesh's vertex order.
  The three vertices of the hit triangle each give their own bin-pair blend and those three are
  weighted by the hit's barycentrics.
* Where the baked data has nothing to say -- ground outside the one baked sheet -- the RIGHT panel
  is drawn **lit** and wears a **teal wash**, and those pixels are EXCLUDED from every disagreement
  figure. The caption of each pair prints what fraction that was.


## 4. The controls

Scripts: `control.py` (both), numbers in `controls.json`. Run at 03:2x on 2026-09-19.

### Control A -- is the LEFT panel's shadow the shadow a ray cast gives?

The sheared suffix-maximum is scored against BRUTE FORCE: the shadow ray marched against the
bilinear heightmap at a CONSTANT 32-unit step (a quarter of the node spacing, so it cannot walk
over a ridge) out to 150,000 units, plus Moller-Trumbore against ALL 29,587 triangles with no
acceleration structure at all. 1,500 random ground points and 1,500 random points on object
triangles, per sun.

| sun | surface | grid vs brute agree | brute says lit | grid says lit |
|-----|---------|--------------------:|---------------:|--------------:|
| az 120 el 5 | terrain | 93.13% | 11.0% | 5.1% |
| az 120 el 5 | objects | 88.33% | 18.1% | 14.5% |
| az 120 el 15 | terrain | 85.80% | 28.2% | 16.3% |
| az 120 el 15 | objects | 86.40% | 23.8% | 19.9% |
| az 120 el 30 | terrain | 84.93% | 43.3% | 31.0% |
| az 120 el 30 | objects | 83.73% | 29.5% | 27.7% |
| az 240 el 15 | terrain | 82.33% | 30.3% | 15.0% |
| az 240 el 15 | objects | 85.93% | 21.6% | 19.4% |

**The disagreement is one-sided: the LEFT panel always shows MORE shadow than an exact ray.** The
cause was traced, not guessed. Terrain-only the grid agrees 96-98%; every remaining disagreement
is an object occluder. Tracing individual cases: the blocker is a real piece of geometry standing
above the ray line, a few units to the SIDE of it -- a mast, a railing, the edge of a catwalk, a
tree card seen edge-on -- and the exact ray threads past it while a ray 16 units wide does not.
Refining the near grid to 8 and then 4 units does not close the gap (86.8% -> 87.1% -> 88.0% on
objects at azimuth 240) because at that point the grid starts CATCHING small triangles it used to
miss; the geometry of this chunk is genuinely mostly thin cards and masts. So: **the LEFT panel is
a shadow ray with a 16-unit footprint, and in a chunk built of thin geometry that is a visibly
fatter shadow than a pencil ray gives.** It is stated rather than hidden, and the reader should
treat the LEFT panel as the conservative bound, not as ground truth to three digits.

### Control B -- are the object horizon bytes read from the right vertex?

If the vertex-to-16-byte-slice mapping were scrambled the RIGHT panel would still look plausible
and would still be wrong. So 800 sampled object vertices had their skyline computed independently
and the stored bytes scored against it -- with the SAME bytes SHUFFLED between vertices as the red
control. The independent skyline copies the three rules out of `src/lodghorizon.h` and
`src/nativeemit.cpp` rather than inventing its own: a 32-unit NEAR lattice of the LOD object
triangles (each triangle raising its whole XY bounding box to its own max Z, `nearHz.raiseBox`), a
128-unit FAR lattice of the land, each field skipped within one of its own cells
(`nearSkipCells` = 1, bake.log `horizonNearSkip 1.00`), a 4-unit rise, the bake's own geometric
segment ladder (32 units, x1.3, elevation taken at the segment's NEAR end), and the tangent-plane
rule -- `nz > 1e-3` gives `planeDeg = atan(-n.d / nz)`, an exactly vertical face stores 0 on its
back and takes everything on its front, and a face pointing DOWN stores 0 in all 16 bins.

| | mean abs error | Pearson r | "is there anything at all" bit agrees |
|---|---:|---:|---:|
| **stored bytes** | **2.94 deg** | **0.946** | **98.8%** |
| the same bytes shuffled between vertices | 33.81 deg | 0.008 | 55.9% |

Independent mean 20.96 deg (68.0% of bins zero) against the file's 23.40 deg (67.0% zero); the
file's own census says `mean elevation 24.91 deg, 555651 bins at 0` over the whole chunk. The
decode and the mapping stand. The residual 2.94 degrees is the bake measuring each segment's
occluder at the segment's near end, which over-states by design.

**What control B also shows about the bake itself, stated plainly:** 65.0% of all stored object
horizon bytes are zero and **1,320 of the 2,449 placements are entirely zero**. That is not a
decode failure -- it is the tangent-plane rule doing its job on a library whose LOD meshes are
largely flat double-sided cards, 60.7% of whose vertices point downward and therefore store
nothing at all. It means the RIGHT panel's objects go dark far less often than the LEFT panel's,
and that is most of the object disagreement in the table in s5.

## 5. The numbers, and the pictures they belong to

Every figure below is the percentage of **decided surface pixels** where the two panels disagree
about lit vs shadow. A pixel is decided when it is not sky and the baked data has something to
say about it; the pixels where the bake has nothing (drawn with a teal wash on the RIGHT) are
counted separately in the *no-data* column and are **excluded** from the disagreement. The
terrain / objects split is by what the camera ray hit first, so the two pixel counts add up to
the decided total for that camera.

These are disagreements, not errors on anyone's part: the LEFT panel is this lane's ray cast
(with the honesty limits in s1 and the measured error bar in s4), the RIGHT panel is what the
baked horizon data can express. Where they differ at least one of the two is wrong, and s4 says
how far each can be trusted.

### 5.1 The perspective pairs -- the main deliverable

| camera | az | el | ALL | terrain | ter px | objects | obj px | no-data | file |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| close       | 120 |  5 |  47.56% |   7.10% |   177,390 |  58.43% |   659,904 |  22.8% | `persp_close_az120_el05.png` |
| close       | 120 | 15 |  45.93% |  10.51% |   177,390 |  55.46% |   659,904 |  22.8% | `persp_close_az120_el15.png` |
| close       | 120 | 30 |  42.48% |  14.26% |   177,390 |  50.06% |   659,904 |  22.8% | `persp_close_az120_el30.png` |
| close       | 120 | 50 |  23.10% |  16.11% |   177,390 |  24.97% |   659,904 |  22.8% | `persp_close_az120_el50.png` |
| close       | 240 | 15 |  47.13% |  15.91% |   177,390 |  55.52% |   659,904 |  22.8% | `persp_close_az240_el15.png` |
| east        | 120 |  5 |  39.86% |  15.62% |   175,282 |  46.65% |   625,008 |  10.8% | `persp_east_az120_el05.png` |
| east        | 120 | 15 |  29.42% |  31.76% |   175,282 |  28.76% |   625,008 |  10.8% | `persp_east_az120_el15.png` |
| east        | 120 | 30 |  20.35% |  20.11% |   175,282 |  20.41% |   625,008 |  10.8% | `persp_east_az120_el30.png` |
| east        | 120 | 50 |  13.43% |   6.90% |   175,282 |  15.27% |   625,008 |  10.8% | `persp_east_az120_el50.png` |
| east        | 240 | 15 |  48.94% |  33.16% |   175,282 |  53.37% |   625,008 |  10.8% | `persp_east_az240_el15.png` |
| full        | 120 |  5 |  39.71% |   4.89% |   222,360 |  56.11% |   472,249 |  42.1% | `persp_full_az120_el05.png` |
| full        | 120 | 15 |  40.04% |   9.79% |   222,360 |  54.29% |   472,249 |  42.1% | `persp_full_az120_el15.png` |
| full        | 120 | 30 |  39.38% |  16.74% |   222,360 |  50.04% |   472,249 |  42.1% | `persp_full_az120_el30.png` |
| full        | 120 | 50 |  25.93% |  17.85% |   222,360 |  29.74% |   472,249 |  42.1% | `persp_full_az120_el50.png` |
| full        | 240 | 15 |  50.51% |  34.15% |   222,360 |  58.21% |   472,249 |  42.1% | `persp_full_az240_el15.png` |
| street      | 120 |  5 |  46.41% |  41.98% |   548,651 |  51.47% |   480,633 |   0.2% | `persp_street_az120_el05.png` |
| street      | 120 | 15 |  23.34% |  14.92% |   548,651 |  32.95% |   480,633 |   0.2% | `persp_street_az120_el15.png` |
| street      | 120 | 30 |  16.46% |   5.08% |   548,651 |  29.45% |   480,633 |   0.2% | `persp_street_az120_el30.png` |
| street      | 120 | 50 |  10.34% |   3.05% |   548,651 |  18.66% |   480,633 |   0.2% | `persp_street_az120_el50.png` |
| street      | 240 | 15 |  62.33% |  47.62% |   548,651 |  79.11% |   480,633 |   0.2% | `persp_street_az240_el15.png` |

Cameras (all heights are offsets read off the terrain, never typed):

- **close** -- low SW oblique over the built-up part, eye ~3,400 u above the ground (1600x900, 58 deg)
- **east** -- stands in the east looking WNW, eye ~1,600 u up (1600x900, 58 deg)
- **full** -- high SW oblique of the whole chunk, eye ~6,000 u up (1600x900, 56 deg)
- **street** -- near-ground, eye ~120 u above the ground, looking along azimuth 289 (1600x900, 62 deg)

**Why there are four cameras and not three.** The brief asked for three. Four of the five sun
positions stand at azimuth 120 (east-south-east), and the first two cameras both look
north-east from the south-west, so those four suns lit only the backs of everything in frame --
the shadows were there but they fell on surfaces the lens could not see. `east` was added to
look the other way, so the same four suns light the faces in frame and the cast shadows lie
across open ground toward the lens. It is the clearest pair in the set.

**The street camera was re-shot.** Its first bearing was azimuth 120, exactly into four of the
five suns: at elevation 5 and 15 the entire frame was a contre-jour silhouette and not one cast
shadow was legible (that render has been overwritten; its log line read `persp_street_az120_el05
all 23.86% ter 1.63% obj 71.83%`, and the terrain figure is that low only because nearly every
ground pixel was in shadow on both panels). The shipped `street` looks along azimuth 289 instead
-- 11 degrees off the anti-sun direction -- so the long shadows run down the street toward the
lens across lit ground. This is a deliberate deviation from the brief's wording ("long shadows
streak toward the camera"): shadows that point at the lens can only be seen from behind the sun,
and that is the picture that does not read. The elevation-5 frame at this new bearing is the
most informative image in the set: on the LEFT a row of pillars throws hard shadow bars down the
lit street; on the RIGHT the same street is almost entirely dark.

### 5.2 The sanity control -- the RIGHT panel read at the wrong azimuth

| camera | az | el | ALL | terrain | ter px | objects | obj px | no-data | file |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| east_ROT180 | 120 | 15 |  38.69% |  64.11% |   175,282 |  31.56% |   625,008 |  10.8% | `control_east_az120_el15_RIGHT_ROT180.png` |

Same camera and same sun as `persp_east_az120_el15`, except the RIGHT panel looks the baked bins
up at azimuth 300 instead of 120 -- the sun turned 180 degrees -- while the LEFT panel is
unchanged. The control bites on the **terrain**, where the bake has dense data: terrain
disagreement jumps from **31.76% to 64.11%**, i.e. turning the data round roughly doubles the
disagreement and overshoots the 50% a coin would give. That is the evidence that the RIGHT
panel's terrain result is actually driven by the azimuth bins and not by something that would
look the same whatever bin it read.

On the **objects** it barely moves -- 28.76% to 31.56%. That is not a failure of the control, it
is the fact s4 ends on: 65% of the stored object horizon bytes are zero, and rotating a field of
zeros by 180 degrees gives a field of zeros. The object half of the RIGHT panel is largely
insensitive to azimuth because there is largely nothing there to be sensitive with.

### 5.3 The secondary top-down pair

| camera | az | el | ALL | terrain | ter px | objects | obj px | no-data | file |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| top_full    | 120 | 15 |  30.17% |  19.19% | 2,798,132 |  53.76% | 1,302,574 |   2.2% | `top_full_az120_el15.png` |
| top_crop    | 120 | 15 |  37.33% |  19.89% | 2,386,539 |  60.35% | 1,807,765 |   0.0% | `top_crop_az120_el15.png` |

North up, a 1,000,000-unit lens 1,000,000 units above the centre (orthographic to within a few
pixels), through the same ray path as everything else. `top_full` is the whole chunk plus its
margin; `top_crop` is a 7,200-unit square over the built-up part.

### 5.4 The sweep

`images/sweep_close.gif` (14 frames, 420 ms each, loops) and `images/sweep_close_filmstrip.png`
(the same 14 frames as a 4-wide contact sheet). Camera `close`, TRUTH on the left of each frame
and BAKED on the right, sun swept from azimuth 95 to 265 with the elevation on a sine arch
peaking at 44.7 degrees. Frames are rendered at 1600x900 and downscaled by 2 for the GIF.

| # | az | el | ALL disagree | terrain | objects |
|---:|---:|---:|---:|---:|---:|
| 00 |  95.0 |  7.7 |  48.48% |   2.48% |  60.85% |
| 01 | 108.1 | 16.9 |  45.75% |   9.04% |  55.61% |
| 02 | 121.2 | 25.3 |  44.12% |  14.21% |  52.16% |
| 03 | 134.2 | 32.7 |  41.00% |  14.14% |  48.22% |
| 04 | 147.3 | 38.6 |  40.30% |  18.96% |  46.04% |
| 05 | 160.4 | 42.6 |  37.47% |  18.27% |  42.63% |
| 06 | 173.5 | 44.7 |  34.24% |  18.91% |  38.37% |
| 07 | 186.5 | 44.7 |  31.39% |  18.15% |  34.95% |
| 08 | 199.6 | 42.6 |  33.25% |  16.57% |  37.73% |
| 09 | 212.7 | 38.6 |  37.57% |  22.23% |  41.70% |
| 10 | 225.8 | 32.7 |  40.71% |  19.53% |  46.40% |
| 11 | 238.8 | 25.3 |  44.34% |  20.63% |  50.71% |
| 12 | 251.9 | 16.9 |  48.16% |  16.37% |  56.71% |
| 13 | 265.0 |  7.7 |  50.80% |   0.60% |  64.29% |

The shape of that column is the headline of this lane: the two panels agree best under a high
sun (31% at 45 degrees) and worst under a low one (48-51% at 8 degrees) -- backwards from what
you would want, because the low sun is when long shadows matter.

### 5.5 Render times

Wall clock, one process, no GPU, no multiprocessing -- plain NumPy on the CPU, with Fallout 4
running throughout.

| stage | time |
|---|---:|
| close gbuffer (1600x900) |  19.9 s |
| east gbuffer (1600x900) |  19.8 s |
| full gbuffer (1600x900) |  17.9 s |
| street gbuffer (1600x900) |  16.7 s |
| top_full gbuffer (2048x2048) |  81.2 s |
| top_crop gbuffer (2048x2048) |  84.4 s |
| one 1600x900 PAIR (both panels, all shading, PNG written) | 2.0 - 2.7 s |
| the whole set: 4 cameras x 5 suns + control + 2 top-downs + 14 sweep frames | 5.5 min |
| the street re-shoot (1 camera x 5 suns) | 0.5 min |

The primary visibility (the G-buffer) is solved **once per camera** and reused by all five suns
and by both panels, which is why a pair costs about two seconds against the twenty a camera
costs. The shadow test itself is not a per-pixel ray march: it is a suffix maximum in sheared
sun space (s1), so one pass answers every shadow ray in the frame at once.

No resolution was dropped. The brief allowed lowering resolution before dropping cameras; that
was not needed, and a camera was added rather than removed.

### 5.6 The files

In `images/`:

- `persp_<camera>_az<azimuth>_el<elevation>.png` -- 20 pairs, 3234x1050 each (two 1600x900
  panels, labels and caption burned in). Cameras `close`, `east`, `full`, `street`.
- `control_east_az120_el15_RIGHT_ROT180.png` -- the sanity control of s5.2.
- `top_full_az120_el15.png`, `top_crop_az120_el15.png` -- the secondary top-down pairs, 2048px
  panels, north up.
- `sweep_close.gif`, `sweep_close_filmstrip.png` -- the sweep of s5.4.

Beside this report: `disagreement.json` (every row of the tables above), `times.json`,
`sweep.json`, `controls.json` (the s4 control output), and the scripts `scene.py`, `render.py`,
`shade.py`, `cams.py`, `run.py`, `control.py`. Files whose names begin with `_` are scratch
(camera-candidate contact sheets and probes) and are not part of the deliverable.
