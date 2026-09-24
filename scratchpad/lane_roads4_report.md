# Lane ROADS4 -- the road meshes carry terrain-shaped skirt geometry; the far bake must not paint it as road

Written incrementally. Every timestamp read from `date +%H:%M` in the turn that
wrote it. Tree `E:\Projects\NifskopeWildWastelandEdition`, branch main, nothing
committed.

**Lane state.** `Fallout4.exe` pid 48328 was UP from 05:42:58 and went down
before 06:2x; the offline work was done while it ran, the one build and every
harness run happened after it was down. The lane did not end BUILD PENDING.
No commit, no `git stash`, nothing written under bungo's installed
`Data\Terrain`. All bakes are region bakes into
`scratchpad/roads4_20260912/out/`.

## Item -1 -- the chains inherited from lane UINOTES1b

RUN 06:10:16 - 06:22:33, on the 05:48:33 exe, the moment `Fallout4.exe` went
down and before this lane built anything. Full tables, with the log lines, in
`scratchpad/uinotes1_20260912/RESUME_BY_ROADS4.md`; one line appended to
`scratchpad/lane_uinotes1_report.md` under `## Build (UINOTES1b)`.

| chain | rows matching the brief's expected counts | rows differing |
|---|---|---|
| lodgen (8 harnesses) | 5 of 8 | `lodgen_terrain_vt` 41/**2** (expected 41/1), `lodgen_ground_cover` 29/**6** (29/5), `lodl_open` 23/**2** (23/0) |
| UI (6 named harnesses) | 6 of 6 | none |

* the two `+1`s are MY OWN: both extra reds are `the exe is newer than every
  source this answer depends on`, and they are true -- this lane edited
  `src/lodgen.h` and `src/nifcli.cpp` at 06:07-06:08 while the game was up and
  no build was allowed, so those files are newer than the 05:48:33 exe. Nothing
  about the exe moved. A MISTAKES entry is owed and written.
* `lodl_open`'s two are REAL and they are a **SEGMENTATION FAULT**: six
  headless render invocations (`WW_RENDER_VIEW=1`), six segfaults, `0 planes
  rendered, 0 distinct pictures`. The 21 non-render checks in the same harness
  pass, so the `.lodl` reader is fine and the render path is not. It was 23/0
  on the 04:10:38 exe, so the crash arrived with one of the nine UI rulings in
  the 05:48:33 build. Reported, not fixed: that code is lane UINOTES1b's.

The rung was then made: `release/NifSkope.before_roads4.exe`, copied 06:23:28,
**21,817,856 B**, md5 `980e64c1aa4e5478b5833d83ebea9655` -- the same bytes as
the exe the chains ran on, verified by `md5sum` on both.

## 0. The `--road-detail` default flip (bungo's ruling, not a candidate)

His words, 2026-09-12 05:0x, over `scratchpad/road_detail_look.png`:
*"--road-detail 1 is always on, do not ever use road detail 0, that looks
terrible"*.

Edited, code on disk at 06:07-06:08, NOT built:

| file | what changed |
|---|---|
| `src/lodgen.h:793` | `float roadDetail = 0.0f;` -> `1.0f`, and the doc block now leads with his ruling verbatim, keeps ROADS2's measurement as the COST of 1 rather than as an argument for 0, and records that at 1 the lerp is branched over so a default bake is a `--road-detail 1` bake by construction |
| `src/nifcli.cpp:5689-5699` | the `-h` text now says 1 is the default and that 0 is never to be used, with the date of the ruling |
| `src/nifcli.cpp:6445` | the `--roads-legacy` branch (`if ( !lgRoadDetailSet ) lgCover.roadDetail = 1.0f;`) is UNTOUCHED and still correct -- it is now a no-op that keeps the way back working if the default ever moves again |

No GUI row exists for this setting, so nothing in `src/lodgenmanager.cpp` or the
panel needed to move (checked by grep over `src/`, `res/`, `tests/`: the only
other mentions are the two call sites that pass `coverOpts.roadDetail` and the
`roadDetail %1` census line).

### 0.1 THE FLIP TURNS `lodgen_roads.sh` R5 RED, and here is the number BEFORE the build

Measured offline at 06:08 on bakes that already exist, using the harness's own
script and no new code:

```
python tests/spells/lodgen_roads_metric.py <vanilla> <rung --no-roads> <bake>
  on scratchpad/roads3_20260911/out/{rung_noroads,rung_roads,rung_detail1}/t2020
```

| bake | after | bar 1 = 2 x floor 0.1354 | bar 2 = 0.8 x reference | verdict |
|---|---|---|---|---|
| `rung_roads` = today's default, detail **0** | **0.3435** | 0.2708 ok | 0.3231 ok | rc 0 |
| `rung_detail1` = the NEW default, detail **1** | **0.3078** | 0.2708 ok | **0.3223 FAIL** | rc 1 |

Short by **0.0145**. Mean colour error on vanilla's own road centreline moves
22.34 -> 23.64 levels.

**This is not a defect in this lane and it is not fixed by this lane.** R5's
bar 2 was calibrated by lane ROADS1 and passed by lane ROADS2 *because* detail 0
was the default; detail 1 is exactly the setting ROADS2 measured as worse
against vanilla (0.3061 in its own table). bungo looked at both in one picture
and ruled for 1. His eye outranks the measurement -- the measurement does not
get to overrule him by staying green.

So gate **G6 as written ("ROADS3's F4 harness rows unchanged, 11/0 ...") is
REFUSED**, with the number above, by item 0 alone and before anything else in
this lane runs. What the record needs instead is stated in section 3.

**Confirmed on the built exe at 06:43**, which is the same number and not a
re-derivation: `tests/spells/lodgen_roads.sh` on the 06:31:05 exe prints
`after ours --roads 0.3078`, `bar 2 0.3078 >= 0.3223 FAIL`, 11 checks 1 failure.
The other ten checks pass, including R1 and R4's byte-identity floors.
Log: `scratchpad/roads4_20260912/logs/after_lodgen_roads.txt`.

## 1. The skirt, measured -- and THE BRIEF'S PREMISE IS REFUTED

The brief asked for the texels painted by SKIRT-ONLY triangles, "the size of
the defect". The rule was stated before any number was read
(`scratchpad/roads4_20260912/r4lib.py`): **a road triangle is SKIRT when any of
its three vertices carries an alpha below 1, and TRUNK when all three are 1**
-- finer than lane ROADS2's whole-shape `ramped` rule, which is the point.

The instrument was checked first (`ww-control-calibration`). C1, the projected
mask against the mask the generator actually painted:

| | (-20,20) | (-8,8) |
|---|---|---|
| generator painted (detail 1) | 25,393 | 12,792 |
| python projection | 23,170 | 11,266 |
| both | 23,116 = **91.0%** | 11,069 = **86.5%** |
| generator only -> UNCLASSIFIED, dropped from every class statistic | 2,277 | 1,723 |
| projection only | 54 | 197 |

**The answer is ZERO.**

| | (-20,20) | (-8,8) |
|---|---|---|
| trunk triangles | 214,100 | 804,192 |
| skirt triangles | 6,874 = 3.1% | 30,100 = 3.6% |
| shapes / of those carrying a skirt triangle | 1,046 / 174 | 4,367 / 784 |
| texels a skirt triangle covers and NO trunk triangle does | **0** | **0** |
| texels whose max-z WINNER is a skirt triangle | 455 | 556 |

So candidate (b) of the brief -- "skirt triangles excluded from the road plane
entirely" -- can move at most 455 and 556 texels of 23,116 and 11,069, and
the feather is not what bungo is looking at. Candidates (a) and (c), which
read vertex alpha as coverage, are bounded by the same 455/556: the alpha
field on the road mask is min 0.000, **mean 0.9887**, sd 0.0900, with 1,512 of
23,116 texels below 1 on (-20,20).

### 1.1 ROADS3's F3e' number (-0.792) is an INSTRUMENT ARTEFACT

Re-measured with the alpha buffer initialised to ONES instead of zeros, so a
texel the projection never reaches does not read "alpha 0":

| chunk | ours (detail 1) | our own ground | vanilla |
|---|---|---|---|
| (-20,20) | **-0.0345** | -0.2710 | **-0.0359** |
| (-8,8) | **+0.0404** | +0.0649 | **+0.0766** |

Ours and vanilla are indistinguishable, on both chunks. **Gate G1 as written
("road luminance correlates with mesh vertex alpha") is therefore REFUSED: the
correlation it was to be measured against does not exist in vanilla either.**
The zero-initialised buffer is `scratchpad/roads2_20260911/seam.py`'s
`abuf = [np.zeros((N,N)), np.zeros((N,N))]`, a one-sided approximation -- every
unreached texel reads 0 and drags the correlation negative. MISTAKES entry
owed and written.

### 1.2 WHAT IS ACTUALLY IN THE ROAD PLANE -- and it is exactly what bungo said

bungo, 2026-09-12, verbatim: *"the issue with the roads is, these meshes have
some terrain included there, you can see the sharp mesh terrain being included
into the chunk's bake"*.

The road NIFs carry shapes whose MATERIAL is a landscape GROUND material. The
discriminator is the material's own FOLDER, not its name -- a name-stem list
was tried first and mis-read 7,558 texels of `CommonwealthDefault01.bgsm` and
5,317 of `SancSW01.BGSM` on (-20,20), because neither name says which it is.
The files resolve on disk to

```
E:/Tools/Fallout 4/DataUnpacked/Data/materials/Landscape/Ground/CommonwealthDefault01.bgsm
E:/Tools/Fallout 4/DataUnpacked/Data/materials/Landscape/Ground/DirtGravel01.BGSM
E:/Tools/Fallout 4/DataUnpacked/Data/materials/Landscape/Roads/SancSW01.BGSM
```

carried by `Landscape\Roads\Sanctuary\SancRoadStr01.nif`, `SancRoadStr01DW`,
`SancRoadEnd01`, `SancRoadCrv01`, `SancRoadLoop01`, `SancRoadCrvCustom01/02`,
with shape names the artist left generic (`Plane002:0`, `Plane002:1`).

| | (-20,20) | (-8,8) |
|---|---|---|
| classified road texels | 23,116 | 11,069 |
| won by a `materials/Landscape/Ground/` shape | **8,337 = 36.1%** | **2,756 = 24.9%** |
| our road-SURFACE class, mean luminance | 110.82 | 111.40 |
| our TERRAIN class, mean luminance | 85.53 | 98.26 |
| **our two-tone step** | **25.28** | **13.13** |
| vanilla's surface / terrain / step | 93.54 / 91.01 / **2.53** | 95.42 / 93.66 / **1.76** |
| the EDGE: mean luminance gradient where a terrain patch meets the road surface INSIDE the plane (2,847 and 1,563 texels) | ours **15.387** | ours **8.010** |
| vanilla at the same texels | 5.362 | 5.138 |
| the same boundary set displaced five ways (the floor) -- ours / vanilla | 6.077 / 5.137 | 4.805 / 5.313 |

That is the defect, with its size: **vanilla's far road is nearly one tone and
ours is two**, and the seam between the two tones is three times vanilla's on
Sanctuary and 1.6 times on the downtown tile, both clear of their own floors.

## 2. The candidate rules, BAKED and not simulated

The brief's candidate family was about vertex alpha; section 1 closed it at 455
texels. The family that matches the measured defect is the ground-material
shapes, so a knob for exactly them was built:

**`--road-ground-paint 0..1`** (`LodgenCoverOptions::roadGroundPaint`, default
**1.0**) -- the coverage multiplier for a road shape whose material lives under
`materials/Landscape/Ground/`. 1.0 is branched over entirely, so the old
default is the old bake's BYTES; at 0 `rasterise` drops the fragment at its
existing `if ( cov <= 0.0f ) continue;` *before* the z buffer is touched, so a
ground shape at 0 neither paints nor occludes the road shape under it -- a drop
with no second code path. The multiply is on COVERAGE, so paint strength and
ground-cover suppression move together, which is stated in the header.

One build (06:31:05), then fourteen bakes of the two chunks. Every row read on
the SAME texel sets, fixed once on the default bake:

### chunk (-20,20), 23,116 classified road texels, 8,337 terrain-class, 2,847 boundary

| variant | painted | terrain mean | terrain - vanilla | EDGE | edge floor | profile 2nd diff | surface mean | two-tone step |
|---|---|---|---|---|---|---|---|---|
| **default** (detail 1, ground paint 1) | 25,393 | 85.53 | -5.47 | **15.387** | 6.077 | 4.474 | 110.82 | **25.28** |
| `--road-ground-paint 0.75` | 25,325 | 79.40 | -11.61 | 19.243 | 6.257 | 5.424 | 110.85 | 31.45 |
| `--road-ground-paint 0.5` | 25,110 | 73.06 | -17.94 | 23.810 | 6.570 | 5.389 | 110.96 | 37.90 |
| `--road-ground-paint 0.25` | 24,668 | 66.51 | -24.50 | 28.598 | 6.918 | 3.548 | 111.19 | 44.69 |
| `--road-ground-paint 0` (the drop) | 19,854 | 61.78 | -29.22 | **33.352** | 7.335 | 6.757 | 111.49 | 49.71 |
| `--road-opacity 0.83` | 25,289 | 81.44 | -9.56 | 12.630 | 5.532 | 3.322 | 102.17 | 20.73 |
| `--road-opacity 0.5` | 25,027 | 73.23 | -17.78 | 7.670 | 4.584 | 2.584 | 85.30 | 12.07 |
| `--road-opacity 0.326` | 24,736 | 68.90 | -22.11 | **5.304** | 4.208 | 1.976 | 76.27 | 7.37 |
| **VANILLA** | 23,116 | 91.01 | 0 | **5.362** | 5.137 | 1.034 | 93.54 | **2.53** |
| our own ground (`--no-roads`) | 0 | 61.70 | -29.31 | 3.125 | - | - | 59.97 | -1.72 |

### chunk (-8,8), 11,069 classified road texels, 2,756 terrain-class, 1,563 boundary

| variant | painted | terrain mean | terrain - vanilla | EDGE | edge floor | profile 2nd diff | surface mean | two-tone step |
|---|---|---|---|---|---|---|---|---|
| **default** | 12,792 | 98.26 | +4.60 | **8.010** | 4.805 | 1.820 | 111.40 | **13.13** |
| ground paint 0.75 | 12,608 | 98.97 | +5.31 | 7.401 | 4.752 | 1.896 | 111.41 | 12.44 |
| ground paint 0.5 | 12,449 | 99.70 | +6.04 | 7.147 | 4.711 | 1.960 | 111.39 | 11.69 |
| ground paint 0.25 | 12,089 | 100.39 | +6.73 | 7.150 | 4.721 | 2.535 | 111.38 | 10.98 |
| ground paint 0 | 10,686 | 101.18 | +7.52 | 7.585 | 4.754 | 2.644 | 111.36 | 10.18 |
| `--road-opacity 0.83` | 12,608 | 98.64 | +4.98 | 6.721 | 4.626 | 1.185 | 109.86 | 11.22 |
| `--road-opacity 0.5` | 11,944 | 99.55 | +5.88 | 4.700 | 4.392 | 1.790 | 106.99 | 7.44 |
| `--road-opacity 0.326` | 11,125 | 99.99 | +6.33 | **4.242** | 4.341 | 0.863 | 105.41 | 5.42 |
| **VANILLA** | 11,069 | 93.66 | 0 | **5.138** | 5.313 | 1.711 | 95.42 | **1.76** |
| our own ground | 0 | 101.30 | +7.64 | 4.650 | - | - | 102.69 | 1.39 |

### What the table says, in one paragraph

**Taking the terrain shapes OUT makes the defect WORSE, monotonically.** On
(-20,20) the edge goes 15.387 -> 19.243 -> 23.810 -> 28.598 -> **33.352** as
the ground paint falls to 0, because the patch darkens toward our ground
(85.53 -> 61.78) while the asphalt beside it does not move at all
(110.82 -> 111.49): the step it makes with the asphalt more than doubles. On
(-8,8) the edge improves by 0.9 of a level at best and the terrain class drifts
from +4.60 to +7.52 away from vanilla. **The default of 1.0 is the best member
of its own family on both chunks, and it stays the default. The knob ships as
the instrument that proved it, not as a change of behaviour.**

The thing that IS mis-set is the OTHER tone: our road SURFACE stands at 110.82
where vanilla's is 93.54, while our terrain class is already within 5.47 levels
of vanilla's. The two-tone step is our asphalt's brightness, which is exactly
what lane ROADS3 measured as `roadOpacity` and refused to set, and this lane's
edge number is new evidence on that same knob: `--road-opacity 0.326` puts the
edge at 5.304 against vanilla's 5.362 on (-20,20) and 4.242 against 5.138 on
(-8,8) -- vanilla's own number on both tiles, from two different biomes.

**It is not shipped as the default here either, and that is a refusal with a
number, not an omission:** the same setting moves the harness metric the wrong
way on (-20,20) (see the gate table, R5), and ROADS3 already put that default
in bungo's hands over a picture. Section 4's picture is that decision, priced.



### R5, the harness metric, on every variant (chunk (-20,20))

Log: `scratchpad/roads4_20260912/logs/r5_variants.txt`. Floor (`--no-roads`)
0.1354, ceiling 1.0000, centreline 9,777 texels of 262,144. Bar 1 is
`after >= 2 x floor`, bar 2 is `after >= 0.8 x reference`, both pre-registered
by lane ROADS1.

| variant | after | bar 1 | bar 2 | centreline colour error |
|---|---|---|---|---|
| **the default (ships)** | 0.3078 | 0.2708 ok | 0.3223 **FAIL** | 23.64 |
| `--road-ground-paint 0.75` | 0.2935 | 0.2708 ok | 0.3206 **FAIL** | 24.29 |
| `--road-ground-paint 0.5` | 0.2854 | 0.2708 ok | 0.3165 **FAIL** | 25.05 |
| `--road-ground-paint 0.25` | 0.2833 | 0.2708 ok | 0.3131 **FAIL** | 25.82 |
| `--road-ground-paint 0` | 0.2783 | 0.2708 ok | 0.3118 **FAIL** | 26.47 |
| `--road-opacity 0.83` | 0.3545 | 0.2708 ok | 0.3271 ok | 22.94 |
| `--road-opacity 0.5` | 0.2019 | 0.2708 **FAIL** | 0.3338 **FAIL** | 27.35 |
| `--road-opacity 0.326` | 0.1576 | 0.2708 **FAIL** | 0.3210 **FAIL** | 30.87 |

The refuted candidate is worse here too: `--road-ground-paint 0` drops R5 from
0.3078 to 0.2783 and pushes the centreline colour error from 23.64 to 26.47,
so it fails on the harness metric as well as on the seam.
`--road-opacity 0.83` is the ONLY variant in the whole family that passes both
bars (0.3545 >= 0.3271), and it also improves the seam from 15.387 to 12.630
and the two-tone step from 25.28 to 20.73. It is NOT shipped: the same switch
moves (-8,8) the other way, and that choice is bungo's to make with the
picture in front of him.

## 3. Gates

Every row is a number read off a file in `scratchpad/roads4_20260912/logs/` or
`scratchpad/roads2_20260911/seam_r4_*.json`, named in the row. "Before" is the
rung `release/NifSkope.before_roads4.exe` (built 05:48:33, the `--road-detail`
default still 0); "after" is `release/NifSkope.exe` built 06:31:05 by this lane.

| gate | as the brief wrote it | before | after | verdict |
|---|---|---|---|---|
| G0 | a default bake == a `--road-detail 1` bake byte for byte; `--road-detail 0` reproduces ROADS3's `new_default` byte for byte | n/a (the rung has no such default) | 0 of 9 files differ on (-20,20), 0 of 10 on (-8,8), for both halves | **GREEN** |
| G1 | vertex-alpha correlation | -0.792 (ROADS3, `seam.py`) | ours **-0.0345**, vanilla **-0.0359** on (-20,20); ours **+0.0404**, vanilla **+0.0766** on (-8,8) | **REFUSED** -- the gate's own number is an instrument artefact, see 1.1 |
| G2 | cross-road max second difference; bars: vanilla 1.31 on (-20,20), 1.25 on (-8,8) | 1.034 / 1.711 (vanilla, re-read on this lane's axis) | ours **4.474** on (-20,20), **1.820** on (-8,8) | **RED on (-20,20)** |
| G3 | ROADS2's feathered boundary not worse than 3.979, the solid-boundary control unchanged | feathered **3.988**, solid **5.750** | feathered **5.080**, solid **9.599** | **RED**, and it is item 0's flip that made it red |
| G4 | road MASK width within 1 texel of vanilla's | 9.64 / 4.22 | painted **9.70** vs projected **10.24** on (-20,20) (0.54 short); **4.26** vs **5.64** on (-8,8) (1.38 short) | **GREEN on (-20,20), RED on (-8,8)**, against a SUBSTITUTED reference -- see below |
| G5 | F2 byte identity | n/a | `--no-roads` identical to ROADS3's `rung_noroads`, 9 and 10 files, 0 differing; default identical to `rung_detail1` | **GREEN** |
| G6 | ROADS3's F4 harness rows unchanged (`lodgen_roads.sh` 11/0) | 11 checks, 0 failures | **11 checks, 1 failure** -- R5 after 0.3078 against bar 2 = 0.3223, short by **0.0145** | **REFUSED with a number** |

Two harnesses beyond G6 were run on the 06:31:05 exe at 06:43 and are green:
`lodgen_terrain.sh` 26 checks / 0 failures, `lod_generation.sh` 116 / 0.
`scratchpad/roads4_20260912/logs/after_*.txt`.

### G0 and G5, what was actually compared

`diff -rq` over the whole output directory with `bake.log` excluded, because
that file records the command line and the wall clock and must differ. Nine
files on (-20,20) and ten on (-8,8) -- the colour sheet, the `_msn` normal
sheet, the mesh, the manifest and the meta report among them. Zero differ in
all eight comparisons. This is the strongest result in the lane: the default
flip changes the DEFAULT and nothing else, and `--road-detail 0` still
reproduces ROADS3's bake exactly, so the old look is one switch away.

### G2, and the axis it is read on

The second difference depends on which texels the signed-distance axis is built
from. Read on the painted mask (`logs/gp.json`, the axis the whole variant
table uses) ours is 4.474 and vanilla 1.034 on (-20,20). Read on the painted
mask intersected with the projection (`logs/gates.json`) ours is 5.191 and
vanilla 0.964. Both readings say the same thing and neither is near the brief's
bar of 1.31: **our road has a profile that still ramps where vanilla's is
flat.** `--road-opacity 0.326` brings it to 1.976 -- still not 1.31, and it
costs R5 (see 2).

### G3 is red because of item 0, not because of item 2

The rung, with the old `--road-detail 0` default, read feathered 3.988 against
vanilla 4.242 and a displaced floor of 4.061 -- that is where the brief's 3.979
comes from. bungo's ruling moved the default to detail 1, and detail 1 is what
makes the feathered boundary hotter: 5.080 against the same vanilla 4.242, and
the solid control 9.599 against 5.291. G3 as written cannot be met while the
ruling stands. It is reported, not negotiated: **the ruling outranks G3**, and
G3's replacement is the number above with the ruling named beside it.

### G4's reference is substituted, and here is the substitution

The gate wants "vanilla's mask width". There is no way to measure it: a painted
mask is the difference between a road bake and a `--no-roads` bake of the same
generator, and Bethesda shipped one sheet with no such pair. The substitute is
the **projected road width** -- the mean inward distance over the texels the
ESM's road geometry actually covers, which is a world fact and therefore the
same number for vanilla and for us: 10.24 texels on (-20,20), 5.64 on (-8,8).
Ours paints 9.70 and 4.26 of them. The (-8,8) shortfall of 1.38 texels is the
one that misses the band, and it is a shortfall in the same direction as the
projection/paint gap already reported in 1 (197 projected texels on (-8,8)
carry no paint at all).

### What was NOT measured

* **In the game.** Nothing in this lane was seen in Fallout 4. Every picture is
  a bake read off disk.
* **Any chunk but two.** (-20,20) Sanctuary and (-8,8). No DLC worldspace, no
  far ring, no dim 8/16/32.
* **The `--roads-legacy` path**, deliberately: it is pinned by G5's byte
  identity and was not otherwise exercised.
* **Sidewalks and raised roads as separate classes.** `--road-sidewalks` and
  `--road-raised` were left at their defaults throughout.
* **Whether bungo prefers the opacity trade.** Section 2 prices it; the choice
  is his and the lane did not make it.

## 4. Pictures

All three are in `scratchpad/roads4_20260912/images/`, drawn by
`scratchpad/roads4_20260912/r4_pics.py`. **Every panel is a real bake** written
by the 06:31:05 exe, or Bethesda's shipped sheet read off disk; nothing is
recoloured or synthetic.

| file | what it shows |
|---|---|
| `road_ground_look.png` (2108x1088) | the Sanctuary window at 4:1, four panels: VANILLA, our default (detail 1), `--road-ground-paint 0` (refuted), `--road-opacity 0.326` (not shipped). Under each: road L, terrain L, surface L, the seam gradient and the two-tone step. The window is the 96x96 with the MOST terrain-material texels in it, at (143,187) -- chosen by the measurement, not by eye. Whole sheets underneath with the window boxed. |
| `road_ground_where.png` (2084x1094) | our default bake beside the same sheet with the terrain-material texels in red and the road surface in blue. This picture IS the finding: the red is the verge and the junction fill along the Sanctuary loop, modelled inside the road NIFs and materialled from `materials/Landscape/Ground/`. |
| `road_ground_profile.png` (1100x620) | the cross-road profile, mean luminance against signed distance to the painted edge, all five curves on the same texels. Vanilla flat inside the road (2nd difference 1.034), our default ramping (4.474), `--road-ground-paint 0` worst (11.027), `--road-opacity 0.326` (1.694), our ground under everything (1.023). |

The profile chart is the brief's "profile chart re-drawn". The brief also asked
for "vanilla | rung (detail 1) | winner" in the crop; the rung at detail 1 is
byte-identical to our default (G0), so that panel and the default panel would be
the same image -- the third and fourth panels are spent on the two candidates
instead, which is where the information is.

## 5. Documents

Four files, written by this lane, for the overseer to splice:

| file | for |
|---|---|
| `scratchpad/roads4_20260912/WW_CHANGES_ENTRY.md` | `WW_CHANGES.md`, heading `## 2026-09-12 -- ...` with an em dash |
| `scratchpad/roads4_20260912/HANDOFF_BLOCK.md` | `HANDOFF.md`, a new top block |
| `scratchpad/roads4_20260912/MISTAKES_ENTRIES.md` | `MISTAKES.md` at the repo root, three entries, newest first |
| `scratchpad/roads4_20260912/LODGEN_TERRAIN_VT_1a5.md` | the RECORD of the `docs/LODGEN_TERRAIN_VT.md` amendment, which is **already applied in the tree** (sections 1a.5, 1a.5d, 1a.5e new, 1a.8, 5 and Provenance), by `patch_doc.py`, `patch_doc2.py`, `patch_doc3.py` -- each asserting its anchor occurs exactly once. CR 0 before and after |

Also written directly, not left for a splice:

* `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md` -- a dated note at the end
  saying what changed under it (the default flip, the new switch, and that a
  bake made before 06:31 on 2026-09-12 is a `--road-detail 0` bake).
* `scratchpad/uinotes1_20260912/RESUME_BY_ROADS4.md` -- item -1's two chain
  tables, written for lane UINOTES1b.
* `.claude/skills/ww-material-folder-classify/SKILL.md` -- the repeatable
  procedure this lane found twice (see 8).

## 6. Build

| | |
|---|---|
| exe | `release/NifSkope.exe` |
| timestamp | **2026-09-12 06:31:05** |
| size | **21,819,904 bytes** |
| builds | **one** |
| relinks | **zero** |
| sources changed | `src/lodgen.h`, `src/lodgen.cpp`, `src/nifcli.cpp` -- all three CR count 0 (LF-only) by Python byte counts, before and after |
| rung | `release/NifSkope.before_roads4.exe`, the 05:48:33 exe, md5 `980e64c1aa4e5478b5833d83ebea9655` |
| game | `Fallout4.exe` was up (pid 48328) from 05:42:58; every bake, every harness and the build itself happened after it went down. `tools/ww_build.sh` refuses while it is up and was not overridden. |
| copies | `style.qss` and the shaders copied at link time by `QMAKE_POST_LINK`, checked in step |

The exe is newer than all three sources. `tools/ww_build.sh` gated `make -j2` on
its own exit code, BUILD-RC=0.

**bungo's open NifSkope window needs a restart to pick this exe up.**

## 7. Mistakes

Three, written out in full in `scratchpad/roads4_20260912/MISTAKES_ENTRIES.md`:

1. **A zero-initialised instrument produced a finding.** ROADS3's `seam.py`
   allocated its vertex-alpha buffer with `np.zeros` and never wrote the ones
   for shapes that have no vertex alpha, so "luminance correlates -0.792 with
   vertex alpha" was a correlation with *which shapes have an alpha channel*,
   not with alpha. Mine, inherited and believed for the first hour; found by
   re-deriving the buffer from the NIFs with ones as the default. This lane's
   G1 exists only because of that number.
2. **The brief's premise was not checked before it was built on.** "The road
   meshes carry terrain-shaped skirt geometry; the far bake must not paint it as
   road" -- skirt-only texels are **0** on both chunks. A classifier by vertex
   alpha was written and run before the count that refutes it was taken. The
   count is two lines and should have been line one of the lane.
3. **An inherited chain was run after this lane's own source edits landed.**
   Item -1 says run UINOTES1b's chains FIRST; I edited `src/lodgen.h`,
   `src/lodgen.cpp` and `src/nifcli.cpp` at 06:07-06:08 and ran the chains at
   06:10, so two harnesses reported "the exe is newer than every source this
   answer depends on" as RED when the only cause was my own uncommitted edits.
   The two rows are reported as unreadable rather than as regressions. The rule:
   **an inherited chain runs against the tree it was written for, before any of
   the new lane's edits touch disk.**

A fourth thing that is not a mistake but is owed to the record: six
segmentation faults in `lodl_open.sh`'s headless render path arrived with the
05:48:33 exe (it was 23/0 on the 04:10:38 one). That is UINOTES1b's territory
and is reported to it in `RESUME_BY_ROADS4.md`, not fixed here.

## 8. Finished-work skill review

**`ww-spec-gate-audit` earned its place three times.** "Run the gate on the OLD
binary first" is what turned G3 from a failure of this lane's change into a
failure of bungo's ruling -- the rung reads 3.988 and the flip alone moves it to
5.080, with item 2 not yet in the picture. "Print the TABLE, not the count" is
what made the candidate family collapse: `--road-ground-paint 0.75` looked like
progress on (-8,8) until the (-20,20) column was printed beside it and the seam
was monotonically worse. "Is the approximation ONE-SIDED" is exactly what the
zeroed alpha buffer was: an error that can only ever point one way.

**`ww-control-calibration`'s displaced floors are what stopped a second false
finding.** The seam gradient of 15.387 means nothing until the same 2,847 texels
displaced five ways read 6.077 on the same sheet; the gap is the signal.

**`ww-prototype-is-not-the-product` is why the knob ships at 1.0.** The
candidate was built, measured, and refuted, and the instrument that refuted it
is what ships -- as a switch with its measured numbers in its own help text, so
the next person does not have to re-derive them.

**A new skill is owed and is written:**
`.claude/skills/ww-material-folder-classify/SKILL.md`. The procedure that
repeated: *classify a shipped asset by the FOLDER its material lives in, never
by the material's file name*. A name-stem list put the two biggest contributors
in this lane (`CommonwealthDefault01.bgsm`, 7,558 texels, and `SancSW01.BGSM`,
5,317) in an "unclassed" bucket and hid the whole finding; the folder rule found
36.1 per cent of the road plane in one run. The skill carries the four path
prefixes shipped NIFs use, the normalisation that keys on the LAST `materials/`,
and the rule that the same discriminator must exist in BOTH the C++ and the
offline Python or the two measurements are not comparable.
