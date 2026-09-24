# Lane ROADS2 -- feathered, blended roads; raised highways excluded; the tree filename clause scoped

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, working tree.
Nothing committed, nothing stashed. Lane directory
`scratchpad/roads2_20260911/`.

Exe at launch: `release/NifSkope.exe` 2026-09-11 19:08:42, 21,435,904 B
(RESUME3's DONE exe), sha1 `89065512abfd1fed11ab4f934c943f5972bd106d`.
Rung taken ONCE at 19:38: `release/NifSkope.before_roads2.exe`, same sha1,
same size, same mtime.

---

## 0. Pre-registered gates

Copied from `scratchpad/brief_roads2.md` BEFORE any measurement or code, so
nothing below is a gate invented after the numbers came in.

| id | gate |
|---|---|
| S1 | The seam metric is measured BEFORE any code. After the change: ours within a stated factor of vanilla on feathered piece boundaries; the solid-boundary control unchanged; the displaced-boundary floor red. |
| S2 | `--road-composite max-z` byte-identical to the rung's bake, every file; `--no-roads` byte-identical to the rung's bake, every file. |
| S3 | Raised highways no longer painted: on chunk (-8,8) the elevated family's projected texels in our sheet equal the no-roads bake within tolerance, while flat road still clears its floor (0.716 grey vs 0.629, FLAGSCAN1's numbers). |
| S4 | The four `SetDressing\` tree false positives gone from the Sanctuary candidate list; the 19 trees intact. |
| S5 | Sidewalks decided by a number measured on a tile with >= 5,000 sidewalk texels. |
| S6 | Exe newer than every changed file; dependent objects rebuilt; the rung equals the launch bytes; no NifSkope left running. |

Rules this lane works under, from the brief: ONE build plus counted relinks;
markers `scratchpad/roads2_20260911/BUILDING` -> `DONE`; one NifSkope instance
ever; `Fallout4.exe` checked before the link; bungo's window renamed aside,
never killed; region bakes only, into this lane's own out-dir, never his
installed `Data\Terrain`; no tiling change (RESUME3's 341.3333 stands), no
tone/grading change (GRADE1's territory); never `git stash`, never a commit.

**Inherited red that is this lane's to close or explain** (director, from
RESUME3): `lodgen_roads.sh` went 11/0 -> 11/1 because bar 2 of R5 is defined as
0.8 x the surrounding ground's own agreement with vanilla, and the corrected
tiling raised the ground from 0.3442 to 0.4024 (17 percent better) while the
road term stood still at 0.3065 -> 0.3061. The road raster is now the
worst-matching part of the sheet.

### 0.1 The verdicts, all in one place

Written last, read off the logs named in section 4. Every count comes from a
file on disk.

| id | verdict | the number |
|---|---|---|
| **S1** | **GREEN, and it moved further than promised** | feathered boundaries 54 texels: vanilla 4.242, before 11.837, **after 3.988**; solid-boundary control 2,886 texels: vanilla 5.291, **after 5.750**; displaced floors 3.215..4.876. Section 4.6. |
| **S2** | **GREEN, 9 of 9 files, three times** | `--roads-legacy` = the rung's `--roads` bake, `--no-roads` = the rung's `--no-roads` bake, and `--roads` alone = `--roads` with all four knobs spelled out. Section 4.2. The brief spelled the first `--road-composite max-z`; this lane moved the way back to `--roads-legacy` and says why. |
| **S3** | **GREEN** | chunk (-8,8) elevated-family clearance: vanilla -0.009, before +0.314, **after +0.001**; flat road still clears (+0.068 against vanilla's +0.011). Section 4.4. The gate's own "0.716 grey" could not be reproduced and is a defect in the instrument, not in the change -- section 7 mistake 1. |
| **S4** | **REFUSED AS WRITTEN, substituted, and the substitute is GREEN** | the four props are absent from the candidate list on the RUNG too, because the lister is `hasLod`-gated; the gate could not fail. Substituted a flip table over the whole placed corpus: 137 tree bases under both rules, **7 flip out (82 placements, 0 with a distant LOD mesh), 0 flip in**, and the 36 of 137 kept bases with a distant LOD mesh match the exe's own 36-line list. Section 7 mistake 2. |
| **S5** | **GREEN, decided with a number** | 17,801 sidewalk texels on chunk (-8,8), over the 5,000 required; 15,696 of them pure. Vanilla's clearance there **-0.102**, ours +0.284, mean luminance 86.5 against 128.4. Decision: refused by default. |
| **S6** | **GREEN** | `release/NifSkope.exe` 20:46:44 is newer than `GeneratedFiles/.obj/lodgen.o` (20:46:42) and `nifcli.o` (20:45:37), which are newer than `src/lodgen.cpp` (20:46:21), `src/lodgen.h` (20:43:29) and `src/nifcli.cpp` (20:45:02); the rung still hashes `89065512abfd1fed11ab4f934c943f5972bd106d`; no `NifSkope.exe` and no `Fallout4.exe` running at 21:04. |
| **the inherited red** | **CLOSED** | `lodgen_roads.sh` **11/0**, R5 at 0.3431 against bars 0.2694 and 0.3231. Section 4.5. |

---

## 1. The seam, measured

Measured before a line of C++ was written, on the chunk bungo pointed at:
**(-20,20)**, dim 4, cells -20..-17 x 20..23. That is vanilla's own
`Commonwealth.4.-20.20.DDS` grid -- 512 texels a side, 32 world units a texel
-- so ours and vanilla are compared texel for texel with no resampling.

The instrument is the 1-texel gradient magnitude of luminance, averaged over a
named set of texels. The sets come from re-projecting the same road geometry
the baker projects (ROADS1's `rasterlib`), keeping the winning piece's id per
texel, so a "piece boundary" is a texel whose 4-neighbourhood contains two
different road pieces. Two controls, both required by `ww-control-calibration`:

* the **solid-boundary control** -- boundaries where neither side is a
  feathered (alpha-ramped) shape. If a change only helps feathered joints, this
  row must not move;
* the **displaced-boundary floor** -- the same boundary mask rolled by five
  offsets. It keeps the mask's area, shape and spectrum and destroys only its
  registration with the sheet, so it says what the metric reads off an
  unregistered mask of the same shape.

| set | texels | vanilla | rung (ours, before) | ratio |
|---|---|---|---|---|
| all piece boundaries | 2,940 | 5.271 | 9.708 | **1.84x** |
| FEATHERED boundaries | 54 | 4.242 | 13.957 | 3.29x |
| SOLID boundaries (control) | 2,886 | 5.291 | 9.629 | 1.82x |
| all road texels | 23,321 | 5.533 | 8.612 | 1.56x |
| off-road ground | 238,823 | 4.547 | 4.315 | 0.95x |
| displaced floors (5) | 2,940 | 4.99..5.58 | 5.40..6.87 | -- |

The seam is real and it is not confined to the joints: the solid-boundary
control is as bad as the boundary row, and the whole road interior is 1.56x
vanilla's texel-to-texel variation. The floor rows are well below the live
rows in both sheets, so the metric is reading registration and not shape.

### 1a. Three candidate mechanisms, all three refuted with numbers

**The brief's own hypothesis -- feathered joint pieces overwritten by max-z --
is wrong.** `roads2lib.read_material_full()` parses the flag `matinfo.py`
discards: **0 of 474** road materials set `bAlphaBlend`. Only **3.8 percent**
of road texels are touched by an alpha-ramped shape at all, and **0 texels** are
covered ONLY by feathered geometry, so there is no texel whose value a blend of
ramps could change on its own. Vanilla's sheet does not follow the ramp either:
correlation of vanilla's road luminance with the coverage ramp is **0.001**.
The 54 feathered-boundary texels are 1.8 percent of the boundary set; they
cannot be what bungo sees.

**Coplanar z-fighting is wrong.** Median z spread among the pieces covering a
multi-piece boundary texel is **12.299 world units** -- not coplanar. And the
gradient ratio is the same either side of a 1-unit spread threshold (1.68 vs
1.72), while the worst boundaries are the SINGLE-piece ones (2.30). Whatever
hardens the sheet does not need two pieces.

**A clamped mip is wrong.** `mipcheck.py` reads every road diffuse: all 14 are
2048x2048 with 12 mips, and the mip the generator asks for
(`0.5*log2(uvArea/pixelArea)`, 6.99..8.82 here) is never clamped short --
**0 of 474** shapes.

### 1b. What the seam actually is

Our bake prints the road diffuse's **own pattern at footprint scale**; vanilla
does not.

`flat.py` builds two fields per road texel from the same material the baker
chose: **T8**, the diffuse sampled at the footprint with the code's own mip
(median 7.20), and **T1**, the whole-texture average. Then it correlates each
sheet against each field **with the material held fixed**, so only the
texture's internal pattern can explain anything:

| sheet | corr vs T8, materials pooled | corr vs T8, within material |
|---|---|---|
| vanilla | 0.140 | **0.016** (22,192 texels, 5 materials) |
| ours, max-z (the rung) | 0.958 | **0.852** |
| ours, `--no-roads` (floor) | -0.006 | 0.006 |

Vanilla's road texels know which material they are (0.14 pooled) and nothing
about where inside it they landed (0.016). Ours are a photograph of the
texture (0.852). Two independent readings agree:

* the diffuse's UV repeat measures **256 world units = 8.01 bake texels**, and
  an 8-texel period is exactly the band spacing in bungo's picture. A 16x16
  phase table explains R2 = **0.1396** of our road luminance, **0.0133** of
  vanilla's, against a `--no-roads` floor of 0.0223 -- ours is ten times the
  floor, vanilla is below it;
* local 5x5 luminance SD on the road: ours **10.326**, vanilla **6.624**; off
  the road 5.522 vs 5.376. The excess is inside the road only.

At 32 world units a texel a road texel is 32x32 units of asphalt. Sampling one
point of a 2048-square tile there is a point sample of a high-frequency
pattern, and the pattern aliases into bands at the UV period. Vanilla's
far-terrain road reads as one flat colour per material. That is the
compositing rule vanilla's result implies, and it is what the change below
implements.

---

## 2. What the road meshes carry

`meshflags.py` walks every road NIF projected onto the two chunks this lane
measured -- **119 models, 512 shapes** -- and reads the shader flags, the
`NiAlphaProperty` if there is one, the BGSM the shape resolves through
`lodgenRoadMaterialPath()`'s last-`materials/` rule, and the vertex colours
(its own reader, because `gltf_nifread.py` walks past them).
`scratchpad/roads2_20260911/meshflags.json` is the table.

| what | count of 512 shapes |
|---|---|
| material resolved (BGSM) | 474 (38 resolve to no file) |
| `bAlphaBlend` in the material | **0 of 474** |
| material alpha | 1.0 on all 474 |
| `bAlphaTest` in the material | 177 (thresholds 128 x380, 50 x51, 40 x39, 122 x4 by shape) |
| `bDecal` in the material | 126 |
| `bTwoSided` in the material | **0 of 474** |
| shader flag Decal / Dynamic Decal | 13 / 13 |
| shader flag Vertex Alpha | 52 |
| shader flag Landscape | 0 |
| `NiAlphaProperty` present | 18 |
| ... of those, BLENDING (bit 0 set) | **6** (`0x10ed` x5, `0x12ed` x1) |
| ... of those, testing only | 12 (`0x12ec`) |
| vertex colours present | 118 |
| ... carrying a real alpha ramp (min < 0.99) | 76 (min 0.0; median shape has half its vertices below 0.9) |

So the feathering is real but it lives in **vertex alpha on 76 shapes**, not in
the materials: the six blending shapes are all `RoadASkirt*` / `RoadAFree01`
decal pieces. Every road material is opaque with alpha 1.0, and 177 use alpha
TEST, which is a binary keep-or-drop, not a blend.

**The compositing rule vanilla's result implies.** Vanilla's road texels
correlate 0.016 with the diffuse's footprint pattern within a material and
0.001 with the coverage ramp, while carrying a clear per-material identity
(section 1b). That is the signature of a road plane built as *coverage x the
material's own average colour*, composited over the ground -- not a
re-rendering of the road texture at 32 units a texel. Two consequences for the
rasteriser:

1. the colour written for a texel should not depend on WHERE inside the tile
   the texel's footprint fell (the detail control, work item 3);
2. where two pieces share a texel the result should be a weighted mix in a
   stated order, not "whichever piece happened to be highest" (the composite,
   work item 3). Under max-z the order does not exist at all: the z test
   decides, and 12.299 world units of spread between covering pieces means it
   decides by deck height, which is why raised decks print over the ground
   (work item 4).

---

## 3. The composite and the exclusions

One build. Three new CLI options, all under the existing `--roads`; with
`--roads` absent nothing below can run.

| option | values | default | what it does |
|---|---|---|---|
| `--road-composite` | `max-z` \| `blend` | `blend` | `max-z` is the rung's rasteriser, unchanged. `blend` sorts the pieces covering a texel by mean world Z ascending, decals last, and composites them `dst = lerp(dst, src, srcAlpha)`, un-premultiplying at the end. |
| `--road-detail` | 0..1 | `0` | lerps the sampled diffuse colour toward the diffuse's own whole-texture average: `c = texAvg + (c - texAvg) * detail`. RGB only; the coverage alpha is never flattened. 1 = the rung's sampling exactly. |
| `--road-raised` / `--no-road-raised` | flag | excluded | include the raised road family (highway decks, bridges) in the ground paint. |

**The way back is one token.** `--road-composite max-z` alone also sets
`--road-detail 1` and `--road-raised` unless those were passed explicitly
(`nifcli.cpp`, after the argument loop), so a single word restores ROADS1's
bytes. Gate S2 below is that promise, measured.

### 3a. Why blend and detail are two controls and not one

Measured separately on chunk (-20,20) (`logs/after.txt`):

| | vanilla | rung | max-z + detail 0 | blend + detail 1 | blend + detail 0 (default) | `--no-roads` |
|---|---|---|---|---|---|---|
| all piece boundaries | 5.271 | 9.708 | 5.996 | 10.232 | **6.204** | 3.275 |
| SOLID boundaries (control) | 5.291 | 9.629 | 5.887 | 10.153 | **6.124** | 3.247 |
| FEATHERED boundaries (54) | 4.242 | 13.957 | 11.837 | 14.472 | **10.492** | 4.788 |
| all road texels | 5.533 | 8.612 | 4.457 | 9.140 | **4.278** | 3.361 |
| off-road ground | 4.547 | 4.315 | 4.308 | 4.313 | **4.306** | 4.230 |
| local 5x5 SD on the road | 6.624 | 10.326 | 6.820 | 10.778 | **6.542** | 4.211 |
| phase R2 | 0.0133 | 0.1396 | 0.0521 | 0.1116 | **0.0330** | 0.0223 |
| mean \|luminance error\| on the road | 0 | 16.017 | 14.089 | 15.016 | **12.499** | 33.879 |

* **The detail control does the seam work.** It alone takes the boundary row
  from 1.84x vanilla to 1.14x and the phase R2 from 0.140 to 0.052.
* **The composite alone makes the gradient slightly WORSE** (10.232) and the
  colour slightly better (15.016 against 16.017), which is what a real blend
  does: it introduces intermediate values at partial coverage.
* **Together they beat either**: boundaries 1.18x, local SD 6.542 against
  vanilla's 6.624, phase R2 0.0330 against a floor of 0.0223, road colour error
  12.499 -- the best of the five columns on every row.
* The off-road ground moves by 0.007 out of 4.3 across all four road settings.
  This lane did not touch the ground.

### 3b. Raised roads: why `hasLod` and not only the folder

The rule as shipped: refuse a road placement when `lb.hasLod` is true **or**
`lodgenIsRaisedRoadModel()` matches the folder
(`Landscape\Roads\HighwayOverpass\` or `\Bridge\`).

`hasLod` first, because a base with its own distant-LOD mesh **is already drawn
at distance as an object**; painting its footprint into the ground draws it
twice, once in the wrong place -- on the ground under the deck. `EsmLodBase::hasLod`
is `MNAM` present, which is perfectly correlated with header bit 15 on the road
family (FLAGSCAN1; re-measured here on chunk (-8,8): 28 bases with both, 123
with neither, **0 disagreeing**).

The folder clause second, because it closes the gap `hasLod` leaves: 20
`HighwayOverpass` and 2 `Bridge` bases ship with no `MNAM` at all.

`Landscape\Roads\Raised\` is deliberately NOT in the clause. Those six bases are
raised CURBS (`RoadRaised*`) -- ground-level kerbstones, zero `hasLod` -- and
they belong in the paint.

### 3c. The tree filename clause, scoped

`lodgenIsTreeModel()` now drops a leading `meshes` and a leading `lod` and then
requires the first remaining component to be `landscape` before it will trust a
file name that starts with `tree`. The `\trees\` folder clause is untouched.
`lod\landscape\` had to be allowed because the 167 far models the clause exists
for are `LOD\Landscape\Tree*.nif`.

`treescope.py` classifies every placed base in the Commonwealth under both
rules (`logs/treescope.txt`):

| | bases |
|---|---|
| tree under both rules | 137 (135 `landscape\trees`, 2 `landscape\plants`) |
| **tree under the old rule only -- the flip** | **7** |
| tree under the new rule only | **0** (the clause only narrows) |
| neither | 13,744 |

The seven, every row: `SetDressing\TreeSwing_RopePile01.nif` (45 placements),
`TreeSwing01` (13, MSTT), `TreeNoose01_HangingMannequin` (8, MSTT),
`TreeSwing02` (5, MSTT), `TreeNoose01_Branch` (4), `TreeSwing_Grounded01` (4),
`TreeSwing03_NoSwing` (3) -- 82 placements, and **not one of them carries a
distant LOD mesh**. FLAGSCAN1 named four because it counted STATs only; three
more are MSTTs.

The kept set cross-checks against the exe: of the 137 kept bases, **36** carry a
distant LOD mesh, and the exe's own whole-worldspace
`--list-impostor-candidates --candidates trees` prints **36 lines**. The python
re-typing and the shipped C++ agree on the number exactly.

### 3d. A DEVIATION FROM THE BRIEF, with the number that forced it

The brief specifies `blend` as the default. **Measurement says `max-z`, and the
brief's own work item 7 is why.** Both composites, baked at `--road-detail 0` on
chunk (-20,20) and scored by the pre-registered
`tests/spells/lodgen_roads_metric.py` -- the fraction of vanilla's own road
centreline where our RGB lands within 16/255 of Bethesda's, per channel:

| bake | road metric | bar 1 (2 x floor) | bar 2 (0.8 x the ground) | verdict |
|---|---|---|---|---|
| rung (max-z, detail 1) | 0.3061 | 0.2694 ok | 0.3219 **FAIL** | RESUME3's inherited red |
| **max-z + detail 0** | **0.3404** | 0.2694 ok | 0.3228 **ok** | **both bars cleared** |
| blend + detail 1 | 0.2481 | 0.2694 FAIL | 0.3265 FAIL | worse than the rung |
| blend + detail 0 | 0.2669 | 0.2694 FAIL | 0.3287 FAIL | worse than the rung |

`max-z` + `--road-detail 0` **closes the red the director handed this lane**;
`blend` deepens it and takes bar 1 down with it. On the seam metric the two are
close and max-z is ahead where the seam actually is: piece boundaries 5.996
against blend's 6.204 (vanilla 5.271). Blend wins three smaller rows -- local
5x5 SD on the road 6.542 against 6.820, texture phase R2 0.033 against 0.052,
mean road colour error 12.50 against 14.09 -- which is why it is kept and
offered rather than deleted.

So the shipped defaults are **`max-z`, `--road-detail 0`, raised excluded,
sidewalks excluded**, and the way back is one token, `--roads-legacy`.

### 3e. Sidewalks, decided with a number (work item 6)

Chunk (-8,8) downtown carries **17,801** projected sidewalk texels, over the
5,000 the brief asks for. 15,696 of them are more than two texels from any flat
road, so the two families cannot be confused (`logs/sidewalk.txt`,
`logs/sidewalk_pure.txt`). Brightness AUC against the displaced-mask floor,
tie-averaged:

| pure sidewalk texels (15,696) | vanilla | rung | max-z + detail 0 | blend |
|---|---|---|---|---|
| AUC brightness | 0.518 | 0.764 | 0.857 | 0.849 |
| clearance above its own floor | **-0.102** | +0.187 | **+0.284** | +0.274 |
| mean luminance | **86.5** | 128.4 | 128.4 | 127.8 |
| mean absolute error vs vanilla | 0 | 42.5 | 42.4 | 41.9 |

| the flat ROAD family on the same tile | vanilla | rung | max-z + detail 0 | blend |
|---|---|---|---|---|
| clearance above its own floor | **+0.100** | -0.045 | **+0.101** | +0.115 |
| mean absolute error vs vanilla | 0 | 20.0 | **16.7** | 16.7 |

Read it in one line: **vanilla paints no pale pavement there and we paint a
bright one 42 luminance units too light -- the worst family error in the sheet,
against 18.5 for the tile as a whole -- while the flat road under the new
default reproduces vanilla's own floor clearance to 0.001.** So the roads stay
and the pavements come out: `roadSidewalks` defaults off, `--road-sidewalks`
(and `--roads-legacy`) put them back, and the census counts every refusal
(`roadRefusedSidewalk`, `roadSidewalkBases`).

## 4. Build and gates

### 4.1 The exe on disk

One build, then two relinks, which is what the header allows (one build plus
counted relinks). The game check ran immediately before each: no `Fallout4.exe`,
no `NifSkope` process, and no NifSkope GUI was launched at any point in this
lane.

| | when | size | note |
|---|---|---|---|
| the rung, kept | `release/NifSkope.before_roads2.exe` 19:08:42 | 21,435,904 B | sha1 `89065512abfd1fed11ab4f934c943f5972bd106d`, untouched |
| build 1 | 20:13:46 | 21,455,872 B | the composite + detail + raised work |
| relink 1 | -- | -- | **failed to compile**, no exe written: `src/lodgen.cpp:9037:65: error: 'coverOpts' was not declared in this scope`, the telemetry block reads `opts.cover`, not `coverOpts` |
| relink 2 | **`release/NifSkope.exe` 20:46:44, 21,458,944 bytes** | | the shipped exe: sidewalk refusal + the `--roads-legacy` rename |

`tools/ww_build.sh src/lodgen.cpp src/nifcli.cpp src/lodgen.h` each time; the
log's own last lines read `exe not held by a window`, `BUILD-RC=0`, `exe newer
than the sources`, `copies in step`. Nothing is committed and `git stash` was
never run.

The five flags are on the shipped exe's own usage page
(`NifSkope -no-gui help lodgen`, lines 384-420): `--road-composite max-z|blend`,
`--road-detail 0..1`, `--road-raised` / `--no-road-raised`, `--road-sidewalks` /
`--no-road-sidewalks`, `--roads-legacy`.

### 4.2 Gate S2 -- the two byte-identity gates

Region `-20 20 -17 23 --dim 4`, nine files a bake (three textures, two objects,
one manifest, three VT files), everything under this lane's own out-dir. `diff
-rq` over the whole tree, and a content hash that does not include the path so
the two trees can be compared as sets of bytes.

| gate | command | against | result |
|---|---|---|---|
| **S2a** | `--roads --roads-legacy` (`out/v2_legacy`) | the rung's own `--roads` bake (`out/rung_on`) | **byte-identical, 9 of 9 files**, content hash `32f88dc8df56092a` both sides |
| **S2b** | `--no-roads` (`out/v2_off`) | the rung's `--no-roads` bake (`out/rung_off`) | **byte-identical, 9 of 9 files**, content hash `72cc5e098dfa7d4e` both sides |
| **S2c** | `--roads` with nothing else named (`out/v2_def`) | `--roads --road-composite max-z --road-detail 0 --no-road-raised --no-road-sidewalks` (`out/v2_def_x`) | **byte-identical, 9 of 9 files** -- the shipped defaults are exactly the four knobs, not a fifth hidden one |

The brief wrote S2a as `--road-composite max-z`. This lane moved the way back
to `--roads-legacy` because `max-z` became the DEFAULT composite (section 3d),
so `max-z` alone can no longer mean "all of ROADS1". The gate is the same gate
with the new spelling, and it is green on every byte.

### 4.3 Gate S1 -- the census moves, and the switches are real

Read back off the shipped exe's own telemetry line, not off intent.

| | Sanctuary (-20,20) | downtown (-8,8) |
|---|---|---|
| `--roads-legacy` roadTexels | 27,695 | 163,586 |
| **shipped defaults** roadTexels | **27,509** | **40,436** |
| `roadComposite` / `roadDetail` | `max-z` / `0.000` | `max-z` / `0.000` |
| `roadBlendTexels` (defaults) | 0 | 0 |
| `roadRefusedRaised` / `roadRaisedBases` | 0 / 0 | **110 / 35** |
| `roadRefusedSidewalk` / `roadSidewalkBases` | **111 / 27** | **414 / 64** |
| `--road-sidewalks` roadTexels | **27,695 -- the legacy number exactly** | -- |

Three things to read off that: Sanctuary has no raised road at all (0 refused),
which is why the highway gate had to move to a downtown tile; the sidewalk
refusal is the whole difference between 27,509 and 27,695 on Sanctuary, and
`--road-sidewalks` puts every one of those 186 texels back; and downtown loses
75 percent of its painted road area, almost all of it highway deck.

Every refusal is named in the census's own `roadRefusals` list, e.g.
`raised-haslod Landscape\Roads\HighwayOverpass\HWDoubleEndCapL03.nif`,
`sidewalk Landscape\Sidewalks\SWCurb4x1Str01.nif`.

### 4.4 Gate S3 re-read on the shipped defaults

`gate4b.py gate4_masks.npz vanilla=... legacy=out/hw2_legacy/tex
shipped=out/hw2_def/tex`, log `logs/gate4b_shipped.txt`. Clearance above the top
of the displaced-mask floor, tie-averaged brightness -- positive means the
family is visible in the sheet beyond what an unregistered mask of its own shape
would score.

| family | vanilla | `--roads-legacy` | **shipped defaults** |
|---|---|---|---|
| `road_elevated` (72,264 texels) | -0.009 | +0.314 | **+0.001** |
| `road_ground` (27,988) | +0.011 | +0.124 | +0.068 |
| `nonroad` control (145,001) | -0.028 | -0.005 | -0.094 |

**Gate S3 is green**: the elevated family's clearance lands 0.010 from
vanilla's, inside the noise of the five displaced floors. The flat road keeps a
positive clearance, which is the right sign -- vanilla does paint flat road into
the far sheet, it just does not paint highway decks.

### 4.5 The harness chain, every count against RESUME3's baseline

`scratchpad/roads2_20260911/logs/h2_*.log`, one NifSkope process at a time,
20:51:xx to 20:55, ten harnesses, 383 checks, 6 failures -- all six on checks that are red on the rung as well.

| harness | this exe | RESUME3's baseline | verdict |
|---|---|---|---|
| `lodgen_roads.sh` | **11 / 0 PASS** | 11 / 1 (RESUME3), 11 / 0 before it | **THE RED IS CLOSED** |
| `lodgen_terrain.sh` | 26 / 0 PASS | 26 / 0 | at baseline |
| `lodgen_terrain_vt.sh` | 41 / 1 | 41 / 1 (`V9b`, red on the rung too) | at baseline |
| `lodgen_ground_cover.sh` | 29 / 5 | 29 / 5 (red on the rung too) | at baseline |
| `lodgen_terrain_pbrm.sh` | 14 / 0 PASS | 14 / 0 | at baseline |
| `lodgen_native.sh` | 18 / 0 PASS | 18 / 0 | at baseline |
| `lodgen_panel_run.sh` | 125 / 0 PASS | 125 / 0 | at baseline |
| `lod_generation.sh` | 116 / 0 PASS | 116 / 0 | at baseline |
| `ui_align.sh` | 11 / 0 PASS | 11 / 0 | at baseline |
| `water_ui.sh` | 82 / 0 PASS, 0 skips | 82 / 0 | at baseline |

`lodgen_roads.sh`'s own R5 numbers on the shipped exe, which is the check that
was red when this lane was handed over:

```
floor    ours --no-roads        0.1347
after    ours --roads           0.3431
reference the ground around it  0.4038
bar 1  after >= 2 x floor       0.3431 >= 0.2694  ok
bar 2  after >= 0.8 x reference 0.3431 >= 0.3231  ok
mean |colour error| vs vanilla, --no-roads  whole tile 22.45  centreline 38.11
mean |colour error| vs vanilla, --roads     whole tile 20.80  centreline 22.37
```

0.3431 is a shade better than the 0.3404 section 3d measured, because the
sidewalk refusal landed after that measurement and takes pale kerb out of the
texels the metric samples. The centreline colour error drops from 38.11 with no
road to 22.37 with one, and the whole-tile error from 22.45 to 20.80 -- adding
our road now makes the sheet MORE like vanilla on both, which was not true of
the blend default.

### 4.6 The seam row re-measured on the shipped defaults

`seam.py out/v2_def/... out/v2_off/... shipped`, log `logs/seam_shipped.txt`,
numbers `seam_shipped.json`. Mean luminance gradient magnitude, same texel sets
as section 1.

| texel set | texels | vanilla | rung (ROADS1) | **shipped** | `--no-roads` floor |
|---|---|---|---|---|---|
| all piece boundaries | 2,940 | 5.271 | 9.752 | **5.718** | 3.275 |
| FEATHERED boundaries | 54 | 4.242 | 11.837 | **3.988** | 4.788 |
| SOLID boundaries (control) | 2,886 | 5.291 | -- | **5.750** | 3.247 |
| all road texels (local 5x5 SD) | 23,321 | 5.533 | 10.375 | **4.369** | 3.361 |
| off-road ground (control, not this lane) | 238,823 | 4.547 | -- | 4.302 | 4.230 |

**The feathered row is green too, and I did not expect that.** Section 1 left it
red at 10.492 under blend and 11.837 under max-z + detail 0. The sidewalk
refusal, which landed after that measurement, closed it: those 54 texels are
where a kerb piece meets a road piece, so taking the kerb out of the paint
removes the step. 3.988 against vanilla's 4.242, with the five displaced floors
at 3.215..4.876 -- indistinguishable from vanilla and from the floor, which is
the honest reading: at 54 texels this row cannot resolve better than that.

What is still not vanilla, in the same table: **the road interior is now too
smooth** -- 4.369 local SD against vanilla's 5.533, below vanilla instead of
2x above it. We went from twice vanilla's roughness to four fifths of it. That
is the price of `--road-detail 0` and it is the right side of the error to be
on, but it is a real difference and section 6 carries it.

The skirt row also moved without closing. Binning the 676 skirt-covered road
texels by the mesh's own vertex alpha:

| correlation of sheet luminance with the mesh vertex alpha | value |
|---|---|
| vanilla | **+0.001** (vanilla's far sheet knows nothing of the skirt) |
| ours, shipped | **-0.791** |
| ours, `--no-roads` floor | -0.325 |

The correlation is still strong and still the wrong sign, but read the sizes,
not the correlation: across the whole alpha range our luminance moves 104.41
-> 100.68, a 3.7-unit swing, and vanilla's moves 96.74 -> 98.74, a 2.0-unit
swing the other way. So this is a 5.7-unit disagreement spread over 676 texels
of a 262,144-texel sheet. It is real, it is named, and it is not the seam bungo
saw.

## 5. Pictures

Four, all in `scratchpad/roads2_20260911/images/`, all built by
`make_pics.py` / `make_pic_sidewalk.py`. Every panel in every picture is the
SAME 512-texel grid at 32 world units a texel -- vanilla's own
`Commonwealth.4.<x>.<y>.DDS` grid -- so nothing is resampled on any side and
the zooms are nearest-neighbour. The red box on each whole-sheet panel is the
zoom window; the blue outline in a zoom is the projected footprint of the
family being discussed, drawn as an outline (not a fill) so the terrain under
it is still visible. **I looked at all four before writing a word about them.**

### 5.1 `cmp_seam.png` -- the thing bungo pointed at

Three columns: vanilla, ours before this lane (`--roads-legacy`), ours at the
shipped defaults. Zoom 6x on texels (53,378)-(101,426), the window the
before/after diagnostic picked earlier in the lane, kept fixed so the crop was
not chosen after the numbers.

What is in it: vanilla's Sanctuary sheet is a dim brown chunk with the road as a
faint trace; the 6x zoom is a smooth brown gradient with no stripes in it at
all. The BEFORE column shows the road as a bright grey ribbon and the zoom is
covered in strong diagonal light/dark bands running along the road at a fixed
spacing -- **that is the seam, and it is the road diffuse's own pattern printed
at footprint scale**, exactly what section 1 measured. The AFTER column's ribbon
is one flat grey and the zoom has no bands left, only a couple of soft steps
where pieces join. The burned-in numbers agree: boundary gradient 5.302 vanilla
/ 9.752 before / **5.712 after**; local 5x5 SD on the road 6.653 / 10.375 /
**6.678**.

The same picture shows what is NOT fixed: at whole-sheet scale our road still
reads as a far more prominent pale ribbon than vanilla's, and the ground around
it is visibly darker than vanilla's. Mean road luminance 92.18 vanilla vs
101.59 ours -- 9 units too bright.

### 5.2 `cmp_highway.png` -- the raised-road exclusion

Chunk (-8,8) downtown; zoom 4x on texels (200,180)-(296,276) with the elevated
road family outlined in blue.

What is in it: vanilla's downtown sheet has **no bright highway in it at all** --
only faint pale surface streets -- and inside the blue outline the ground is
plain brown. The BEFORE column paints an enormous bright cream interchange
straight into the far texture, guard rails and all, and the blue outline sits on
top of a pale printed deck. The AFTER column's interchange is gone; inside the
outline is the same brown/grey ground vanilla has. A few small pale specks
remain, which are flat road pieces, not raised ones, and are meant to stay.
Burned in: elevated-family mean luminance 86.28 vanilla / 129.37 before /
**101.27 after**, and clearance above its own floor -0.009 / +0.314 / **+0.001**.

### 5.3 `cmp_sidewalk.png` -- the change bungo has not seen yet

Same tile, zoom 4x on texels (176,240)-(272,336), the projected pavement
footprint outlined in blue. This picture exists because the pavement exclusion
is a visible change nobody has approved, so it needs to be looked at, not
just tabulated.

What is in it: the blue outline traces a thin kerb ribbon down and around a
corner. In vanilla there is nothing pale along it -- the kerb is simply not in
the far sheet. In the BEFORE column a bright cream band runs exactly along the
outline. In the AFTER column that band is gone and the ground inside the
outline matches vanilla's tone far better. Burned in: pavement mean luminance
86.71 vanilla / 128.14 before / **102.10 after** (error 41.4 -> 15.4), and the
flat road on the same tile 94.78 / 112.44 / 106.98.

### 5.4 `cmp_sanctuary_road_v2.png` -- ROADS1's picture re-taken

Vanilla, ours `--no-roads`, ours `--roads` at the shipped defaults, at ROADS1's
own crop (150,120)-(300,270), zoom 3x, so it can be laid beside ROADS1's.

What is in it, and it is the most useful thing in this lane's pictures:
**vanilla's Sanctuary cul-de-sac is a flat desaturated BLUE-GREY patch, not a
pale asphalt ribbon.** Our ring lands in exactly the right place and the right
shape -- the geometry is right -- but it is drawn as a crisp warm pale-grey
ribbon with a distinct darker centre and visible kerb lines, where vanilla has a
soft cool blob with no internal structure. Whole-sheet mean luminance: vanilla
83.84, ours `--no-roads` 67.45, ours `--roads` 71.16.

So there are two separate errors left in that panel and they belong to different
lanes: the road's HUE is too warm and its edges too crisp (this lane's family,
section 6), and the ground under everything is about 16 luminance units too dark
(TILING2 and GRADE1's, not mine to touch).

## 6. Owed / red / bungo's calls

### 6.1 Bungo's calls -- three things changed that he has not approved

Each is behind a flag with a one-token way back, each was decided by a number,
and each is a visible change to the far terrain.

1. **The road is now one flat colour a material** (`--road-detail 0`). This is
   the fix for the seam he reported and it is the least controversial of the
   three, but it also made the road interior *smoother than vanilla's* (local
   5x5 SD 4.369 against vanilla's 5.533, from 10.375). If he wants some of the
   grain back, `--road-detail 0.3` or so is the dial, and it should be
   re-measured on the harness metric before shipping -- detail 1 fails both
   bars.
2. **Highway decks and bridges are no longer painted** (`--no-road-raised`).
   Downtown road texels fall 163,586 -> 40,436, which is 75 percent of the
   painted area on that tile. `cmp_highway.png` shows exactly what disappears.
   The measurement says vanilla paints none of it (clearance -0.009 against our
   +0.314), but this is the biggest visible change in the lane.
3. **Pavements are no longer painted** (`--no-road-sidewalks`). 15,696 pure kerb
   texels on the test tile, where vanilla reads -0.102 clearance -- nothing --
   and we read +0.284 at 42 luminance units too bright. `cmp_sidewalk.png` shows
   it. Sanctuary loses only 186 texels to this, downtown loses a lot more.

`--roads-legacy` puts all four knobs back, byte for byte, in one token, on any
command line.

### 6.2 Still red, with the number

| what | number | whose |
|---|---|---|
| our road interior is too SMOOTH | local 5x5 SD 4.369 against vanilla's 5.533 on 23,321 road texels (it was 10.375) | **this lane's**, the cost of detail 0 |
| our road is too BRIGHT | mean road luminance 101.59 against vanilla's 92.18 on chunk (-20,20) | **this lane's family** |
| our road hue is too WARM and its edges too CRISP | seen in `cmp_sanctuary_road_v2.png`: vanilla's cul-de-sac is a soft desaturated blue-grey blob, ours a crisp warm pale-grey ribbon with a visible kerb line. Not reduced to a single number -- the honest statement is that it is a hue and edge difference nobody has measured yet | **this lane's family, NOT measured** |
| the skirt still darkens with the mesh's vertex alpha where vanilla's does not | correlation ours -0.791, vanilla +0.001, floor -0.325; but the SIZE is small: ours moves 104.41 -> 100.68 across the alpha range (3.7 units), vanilla 96.74 -> 98.74 (2.0 the other way), over 676 of 262,144 texels | **this lane's**, small |
| the off-road ground is far too dark | whole-sheet mean luminance vanilla 83.84, ours 71.16; off-road ground 68.69 against 83.52 | **TILING2 and GRADE1's** -- explicitly not this lane |
| `lodgen_terrain_vt.sh` `V9b` | 41/1, the assembled and direct `_msn` sheets are not byte-identical | at baseline, red on the rung too, not this lane |
| `lodgen_ground_cover.sh` | 29/5 | at baseline, red on the rung too, not this lane |

### 6.3 Owed

* **bungo's open NifSkope window needs a restart.** `release/NifSkope.exe`
  changed at 20:46:44; a window opened before that is running the old code and
  will keep doing so.
* **The three document blocks are written but NOT spliced** -- this lane does not
  edit the shared documents. `scratchpad/roads2_20260911/WW_CHANGES_ENTRY.md`
  for `WW_CHANGES.md`, `HANDOFF_BLOCK.md` for `HANDOFF.md`,
  `MISTAKES_ENTRIES.md` (five entries, each starting `## `) for the root
  `MISTAKES.md`. The director splices.
* **The contract IS amended in place**, because the brief asked for it directly:
  `docs/LODGEN_TERRAIN_VT.md` section 1a gained a provenance note in 1a.1, a
  rewritten 1a.3, a new 1a.3b (which families are painted and the numbers), a
  new 1a.5b (the composite alternative and why it lost), a new 1a.5c (why the
  diffuse is flattened), the new census fields in 1a.7 and a rewritten 1a.8.
  LF-only throughout, matching the file.
* **Nothing is committed** and `git stash` was never run by this lane. For the
  record, because someone will check: `git stash list` shows exactly one entry,
  `stash@{...} On main: wip-tooltips`, dated **2026-08-11 07:00:41** -- a month
  old and not this lane's. `HEAD` is still `720762a`.
* **The rung is intact**: `release/NifSkope.before_roads2.exe`, 19:08:42,
  21,435,904 B, sha1 `89065512abfd1fed11ab4f934c943f5972bd106d`.
* **A road hue/edge measurement is owed to a later lane.** This lane measured
  the seam (pattern), the families (which are painted) and the brightness, and
  did not measure chroma or edge sharpness against vanilla. `cmp_sanctuary_road_v2.png`
  shows both differences plainly, and neither has a number.
* **Lane TILING2 is queued after this one** for the land-texture repeat, blend
  edges and detail. None of its work was done here. The one thing it should know:
  every road measurement in this lane was taken on top of the current ground, so
  when TILING2 changes the ground, `lodgen_roads.sh`'s R5 and the four-variant
  composite ranking in 1a.5b both need re-running before anyone assumes max-z
  still wins.

## 7. Mistakes

Five, written up in full at `scratchpad/roads2_20260911/MISTAKES_ENTRIES.md`,
each entry starting with `## ` so it can be spliced into the root `MISTAKES.md`
unedited. In one line each:

1. **An AUC recorded to three decimals when its own ties made the third
   meaningless.** FLAGSCAN1's 0.716 re-measures as 0.757 from its own script;
   one saturation value covers 53.5 percent of the tile and `np.argsort` breaks
   ties by raster index. Tie-averaged it is 0.731. Found by trying to reproduce
   the gate before using it.
2. **A pre-registered gate that could not fail.** Gate S4's candidate list is
   `hasLod`-gated, so the four props were absent on the rung too. Found by
   running the gate on the OLD exe first. Substituted a flip table over the
   whole corpus.
3. **A tile-scoped reference dump used without checking its own coordinate
   range.** `hw_refs.json` is chunk (-20,-12), not (-8,8); it produced a
   complete, plausible 108-base table for the wrong tile, and an all-zero mask
   set half an hour earlier that was wrongly blamed on the script. Found by
   printing each dump's own position extent.
4. **The heredoc apostrophe trap, again.** A `nifcli.cpp` patch with apostrophes
   in its comment prose died on `unexpected EOF while looking for matching '''`.
   Every source patch in the lane is now a file written with the Write tool.
5. **A material reader that silently dropped the field the measurement needed.**
   `matinfo.read_material` reported 0 of 474 road materials blending, and never
   parses `bAlphaBlend` at all; the real blending is 6 of 512 shapes, at the
   `NiAlphaProperty` level.

Two of the five produced skill amendments -- section 8.

## 8. Finished-work skill review

The brief named ten skills. What each was worth, and what changed.

| skill | used | verdict |
|---|---|---|
| `ww-spec-gate-audit` | yes, first thing, on "0.716 vs 0.629" | **earned its place twice and is AMENDED.** Its "read the source that produced the number, not the number" is exactly what found the tie problem. It had no rule for a gate that cannot fail. |
| `ww-control-calibration` | yes, throughout -- every family number in this lane is a clearance above five displaced floors plus a phase-randomised twin | **load-bearing and AMENDED.** Its displaced-floor construction is what makes "vanilla paints no highway" a measurement instead of an opinion. It had nothing about rank statistics and ties. |
| `nifskope-ww-vanilla-compare` | yes | held. The "same grid, same mip, no resampling" discipline is why every picture in section 5 is comparable. |
| `nifskope-ww-lodgen` | yes | held. The bake-order line (splat -> cover -> VCLR -> ROAD -> tint) and the census-echo rule were both used unchanged. |
| `nifskope-ww-build-verify` | yes, three times | held. It is the reason relink 1's compile failure was caught as a failure instead of being mistaken for a rebuilt exe -- the exe mtime did not move. |
| `ww-contract-provenance` | yes | held. Section 1a's amendments follow its shape: the number, the script that produced it, and what would refute it. |
| `ww-texel-picture` | yes | held, and it is the reason the seam crop was fixed BEFORE the numbers were read off it. |
| `ww-sheet-diff` | yes | held. |
| `fo4cs-census-field` | yes, for the four new telemetry fields | held. |
| `ww-anchored-hookup` | not used | no C++/engine anchor work in this lane. |

### 8.1 The two amendments, named

Both were written to the project tree `E:\Projects\NifskopeWildWastelandEdition\.claude\skills`
**and** the live mirror `E:\Projects\Claude\.claude\skills`, LF-only, verified by
byte count on both copies.

**`ww-spec-gate-audit`** gained *"Run the gate on the OLD binary FIRST"*: before
running a pre-registered gate on the new binary, run it on the one you are
replacing, because a gate that is already green is measuring something else. It
names gate S4 as the case, explains that the lister is `hasLod`-gated two levels
away from the clause under test, points out that this is the mirror image of
`ww-control-calibration`'s "a floor with a fixed step cannot fire", and gives the
five-row flip table to substitute when a gate turns out to be vacuous.

**`ww-control-calibration`** gained *"A rank statistic with a dominant tie block
is not reproducible"*: count the distinct values of the score being ranked and
the size of its largest tie block before quoting a rank statistic; if one value
covers a large fraction of the sample, the ranking is decided by the loop order
and the digits are noise. It carries the 281-values / 140,305-texel numbers, the
three random-tie seeds, a fifteen-line `rankdata` / `auc_tie` (there is no
`scipy` on this machine), and four reporting rules -- print both columns, gate on
a tie-free column, quote at most two decimals, and prefer a clearance over a raw
score because the tie bias is common to both and largely cancels.

### 8.2 What no skill covered

**How to decide between two implementations when the harness disagrees with the
plan.** This lane built the blend composite expecting it to fix the seam, and it
lost to the max-z path it was meant to replace -- on the two gated measures,
while winning three ungated ones. Nothing in the skill set says what to do with
that. What was done: make the gated metric the default, keep the loser behind a
flag rather than deleting it, record all five measures on both sides in the
contract, and write down the condition under which the ranking must be re-run
(section 1a.5b's last line: if a later lane changes the road colour or the
grading, re-run all four variants). That is offered as a pattern, not written as
a skill, because one instance is not enough to generalise from -- if a second
lane hits the same shape, it is worth a skill of its own.
