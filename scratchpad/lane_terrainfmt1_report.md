# Lane TERRAINFMT1 — far-terrain sheet format, the `_msn` detail term, and bungo's cleaned 2K cache

Main tree `E:/Projects/NifskopeWildWastelandEdition`, branch `main`, launched
2026-09-12 12:33. Nothing committed, no `git stash`. Report written
incrementally, a section per gate as it lands.

## 0. Pre-registered gates

Written 12:4x, before any code and before any number was taken. These are the
brief's F1–F4 plus the ADDED ITEM 8 gate F5, restated here with the floor each
one is judged against, so that no gate below can have been invented after its
numbers arrived.

| gate | what must be true | floor / control |
|---|---|---|
| **F1** | vanilla's format law is measured over the shipped corpus BEFORE any code is written: fourCC and mip count over all 6,120 Commonwealth sheets, and the ALPHA channel's content (constant? mask? height?) histogrammed over a sample of at least 50 sheets, colour and `_msn` separately. | the count of files examined is reported; a claim of "constant" must be backed by the per-file min/max over the sample, not by one file |
| **F2** | `--sheet-format legacy` (and no flag at all) writes **byte-identical** bytes to the rung exe's bake, every file of every chunk (`cmp`). With `--sheet-format vanilla`: fourCC `DXT5`, mip count 10, and the render no longer reads blue-purple. | off-value identity is `cmp` == 0 on every file, not a tolerance. The colour-distance number for the render has the BEFORE render as its floor. |
| **F3** | the `_msn` texel-scale detail metric is reported against a **floor** (a phase-randomised twin of vanilla's own sheet) and a **ceiling** (vanilla against itself / vanilla's own sheet-to-sheet spread). A detail term ships only if it beats the floor; otherwise it is REFUSED with the numbers. | twin floor per sheet; ceiling = vanilla vs vanilla |
| **F4** | one build; `make -n` quiet afterwards; the exe newer than every changed source AND no stale `.o` against every header touched; the nine harness baselines from GROUND1's chain reproduced (lodgen_roads 11/0, lodgen_native 18/0, lodgen_native_baseline 25/25 0 differ, lodgen_terrain_vt 41/1, lodgen_ground_cover 29/5 by check NAME, lod_generation 116/0, lodl_open 23/0, lodgen_terrain_pbrm 14/0, animws 236/0/1 skip); rung `release/NifSkope.before_terrainfmt1.exe` == launch bytes; no NifSkope left running. | each harness's own count; the rung is `cmp`-equal to the launch exe |
| **F5** | bungo's cleaned 2K `_msn` cache: measured before anything ships — per-channel means, unit length after renormalising, which channel is derived, coarse agreement with vanilla's own 512 sheet above a phase-randomised twin floor per channel, fine energy at the 4-texel scale, and the BC1 4-texel block-grid spectral line BEFORE and AFTER. `--msn-cache` off == rung bytes; on: grid line gone (number vs before), unit length within 1/255, coarse correlation above the twin floor per channel. | phase-randomised twin per channel; the block-grid line measured on vanilla's own 512 `_msn` as the "grid present" reference and on the cleaned 2K as the "grid gone" claim |

Every picture is baked with `--road-detail 1`. Region bakes only. bungo's
installed `Data\Terrain` is never written; `E:/Tools/Upscale/esrgan-bat/` is a
read-only input.

### 0.1 A premise of the brief that is stale, and was checked before any code

`ww-spec-gate-audit` says to test the pre-registered number before building to
it. Two of the brief's premises come from lanes ROADS1 (2026-09-11 12:19) and
PIC-CHUNK (16:3x), and **lane TILING3 landed at 23:26 the same night and
changed what a chunk's `_msn` is.** On the shipped default
(`--land-detail-source vanilla`) `lodgenVanillaChunkSheets`
(`src/lodgen.cpp:6946`) sets `msnCopy = vmsn` — the output `_msn` **is
Bethesda's file, byte for byte**, for every chunk that has one, and TILING3
measured that Bethesda ships a complete 48×48 dim-4 grid over cells −96..95, so
on Commonwealth that is every chunk.

What follows, and is measured rather than asserted, in sections 2 and 3:

* the "our `_msn` is DXT1 / 8 mips" and "roughness 0.93 vs 10.15" findings are
  true of the ROADS1 bake they were taken on, and are **not** what the default
  writes today on a Commonwealth chunk;
* the sheets this lane can still change are the **colour** sheet (ours whenever
  the chunk has land paint) and the `_msn` on any chunk with no vanilla sheet or
  with a non-default `--land-detail-source`;
* ADDED ITEM 8's cache is the case that matters most, because it **replaces**
  the byte-for-byte vanilla copy on exactly those chunks.

## 1. Vanilla's far-terrain sheet format law (gate F1)

Measured 12:4x by `scratchpad/terrainfmt1_20260912/f1_corpus.py` over
`E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth`, before a
line of C++ was written. Raw numbers in `f1_corpus.json`.

### 1.1 fourCC and mip count — the whole corpus, not a sample

| family | fourCC | mips | size | files |
|---|---|---|---|---|
| colour | `DXT5` | 10 | 512x512 | 3,060 |
| `_msn` | `DXT5` | 10 | 512x512 | 3,060 |

6,120 files, **no exceptions** — there is no second row in that table. By
dim, each family: 2,304 at dim 4, 576 at dim 8, 144 at dim 16, 36 at dim 32.

Two checks that the header is not lying:

* **declared vs present**: for the first 200 files the mip count in the header
  is the mip count the file's bytes actually hold — 0 mismatches.
* **`dwReserved1`**: 0 files out of 6,120 carry a non-zero `dwReserved1`
  (offsets 32..75).

10 mips at 512 means the chain runs 512, 256, ..., 1 — **past the 4x4 block
floor our writer stops at**, which is why ours has 8. A BC block is 4x4
whatever the level says, so levels 2x2 and 1x1 each still cost one block; the
arithmetic is exact: BC3 512 with 10 mips = 128 + 16 x (16384+4096+1024+256+64+
16+4+1+1+1) blocks = **349,680 B**, which is the size every vanilla sheet on
disk has. Ours today: BC1 512 with 8 mips = **174,888 B**.

### 1.2 The ALPHA channel of both families

Sampled 50 dim-4 `_msn` sheets and 50 dim-4 colour sheets (seeded draw), mip 0
decoded whole, 13,107,200 texels per family:

| family | alpha min | alpha max | mean | fraction == 255 | distinct values |
|---|---|---|---|---|---|
| `_msn` | 255 | 255 | 255.000 | 1.000000 | **1** |
| colour | 255 | 255 | 255.000 | 1.000000 | **1** |

The whole histogram is one bin: `255 -> 13,107,200` on each side. Per-file
min and max are 255 and 255 on all 100 files, so this is not a mean hiding a
tail.

**What the engine can read from each alpha: nothing.** A channel with one value
over a 50-sheet sample carries no mask, no height, no cover, no cut-out. It is
there because BC3 has an alpha block, and Bethesda's encoder filled it with the
only constant it could.

I could NOT cite the FO4 LOD terrain shader's `_msn` sampling: FO4CS
(`E:/Projects/Fo4CommunityShaders/wt-fixfirst`, read-only) has **no terrain-LOD
shader replacement at all**. The only landscape shader in that tree is the near
prepass, `res/Lighting/deferred_prepass_landscape.hlsl`, which samples its
layer normals as `.xy` and reconstructs z — that is the NEAR path, a different
shader on a different geometry, and quoting it about the LOD sheet would be
inventing a citation. So: the alpha is measured to be constant; what the engine
does with it is **not measured** and I am not guessing.

### 1.3 The `WWCV` cover stamp against vanilla's alpha semantics

They do not collide, and the reason is not a tolerance:

* the stamp is not in an alpha channel at all. It is `hdr[8]`/`hdr[9]`, i.e.
  `dwReserved1[0..1]` at file offsets 32..39 (`src/lodgen.cpp`, the header
  block of `lodgenWriteDds`), and vanilla writes zero there on all 6,120 files;
* the stamp is written on `_data.DDS` (`docs/LODGEN_TERRAIN_VT.md` 1.4), and
  **vanilla ships no `_data.DDS`** — it is our sheet, in a family whose name
  does not occur in the vanilla corpus;
* the cover BYTE rides in the `_data` sheet's alpha, again not in the colour or
  `_msn` alpha that this section measured.

So there is nothing for the stamp to decide over. If a future change ever does
want the colour alpha, the stamp decides and this paragraph is where it was
said.

### 1.4 What this means for the writer

`--sheet-format vanilla` writes DXT5 with the chain to 1x1 and a constant 255
alpha — Bethesda's measured law and nothing added to it. `legacy` is the
default and is the rung's bytes. A sheet COPIED from vanilla is untouched by
either value: re-encoding a copied BC block is what "byte for byte" forbids.

## 2. The writer, and gate F2 (the sheet format)

Exe built 12:58:48, 22,007,808 B. `make -n` afterwards: `Nothing to be done for
'first'`. Every object that includes `src/lodgen.h` is newer than it (7 TUs
checked by name; the build log shows 7 objects rebuilt, which is the same set).

### 2.1 What was written

* `lodgenWriteDds` gains `bool mipsToOne = false`. The mip loop's floor moves
  from 4x4 to 1x1 **only when it is true**, and the per-level dimensions are now
  the ones the loop produced (`mipW`/`mipH`) instead of a second, independently
  halved copy further down — the old `mw = qMax( 4, mw / 2 )` in the write loop
  could not be reached, and now cannot exist to be wrong.
* `--sheet-format vanilla|legacy`, default `legacy`. Applied in
  `lodgenWriteChunkSheets` — the ONE function both writers (the stock per-chunk
  bake and the pyramid/VT assembly) come through, which is where the reuse
  decision already lives, so the two cannot drift.
* `--msn-cache <dir>`, default empty, in the same function (section 5).
* Census words `sheetFormat`, `msnCacheDir`, `msnCacheHit`, `msnCacheMiss`,
  `msnCacheRenorm`, written unconditionally. **Said plainly because it is a
  trap:** that census LINE is printed only on the `--vt` path, while the
  counters increment on BOTH paths, because `lodgenWriteChunkSheets` is shared.
  A stock per-chunk bake moves these numbers and prints none of them. The
  comment at the emit site says so too.

Files: `src/lodgen.cpp`, `src/lodgen.h`, `src/nifcli.cpp`. All three LF-only
before and after.

### 2.2 The bakes

One chunk, cells −20 24 −17 27, dim 4, `--vt --cover --road-detail 1`, the same
command four times, everything absolute:

| bake | exe | extra flags |
|---|---|---|
| `rung` | `release/NifSkope.before_terrainfmt1.exe` | — |
| `off` | the new exe | — |
| `vanfmt` | the new exe | `--sheet-format vanilla` |
| `cache` | the new exe | `--msn-cache E:/Tools/Upscale/esrgan-bat/output` |

All four exited 0 and wrote the same 10 files (same name set, md5 of the sorted
listing identical).

### 2.3 Off is the rung's bytes

`cmp` on **every file of the bake**, not on the sheets alone:

> **files compared 10, differ 0**

That covers `Commonwealth.VT.2.lodt`, `Commonwealth.VT.4.lodt`,
`Commonwealth.VT.lodm`, the BTO, its manifest, the BTR, the `.lodb`, and all
three texture sheets. This is `cmp`, not a tolerance.

### 2.4 On: the format is vanilla's, measured on the file

| sheet | fourCC | mips present/declared | bytes | alpha |
|---|---|---|---|---|
| rung colour | `DXT1` | 8 / 8 | 174,888 | 255..255, 1 distinct |
| **`--sheet-format vanilla` colour** | **`DXT5`** | **10 / 10** | **349,680** | 255..255, 1 distinct |
| vanilla's own colour sheet | `DXT5` | 10 / 10 | 349,680 | 255..255, 1 distinct |

Byte count, fourCC and mip count all land on vanilla's. No trailing bytes after
the last mip in either of ours; `dwReserved1` zero, as vanilla's is.

What changed and what did not, mip by mip: the eight levels the two share
decode **bit-identically** (maxdiff 0 on every one of mips 0..7), and the new
file adds exactly the two levels vanilla has and ours lacked (2x2 and 1x1).
That is expected and worth stating — with the alpha constant 255 the BC1
punch-through mode never triggers, so a BC1 colour block and a BC3 colour block
of the same texels are the same eight bytes. **`--sheet-format vanilla` changes
the container and the length of the chain; it does not change a colour.**

### 2.5 The `_msn` under this switch

On this chunk the `_msn` is Bethesda's file byte for byte (the shipped default
`--land-detail-source vanilla`), and it stays so: `cmp` says the rung's `_msn`
IS vanilla's file, and so is `--sheet-format vanilla`'s. **A copied sheet is not
re-encoded by the format switch** — that is the rule the code states and this is
the check that it holds.

So on a Commonwealth chunk with a shipped `_msn`, `--sheet-format vanilla`
changes the COLOUR sheet only. On a chunk with no vanilla sheet — and on any
worldspace Bethesda did not bake — it changes both.

### 2.6 The `WWCV` stamp

Untouched and uncollided, for the reasons measured in 1.3: it lives in
`dwReserved1` of `_data.DDS`, the `_data` sheet is unchanged by both new
switches (`cmp` clean above), and vanilla ships no `_data` sheet to collide
with.

## 3. The render proof (item 3) -- and what the blue-purple actually is

**The premise this lane was given is refuted, and the cause is named with a
number.** ROADS1's picture reads blue-purple; the brief attributes it to our
colour sheet's DXT1 container. It is not the container, it is not the colour
sheet and it is not the `_msn`. It is **our terrain mesh's vertex colours**.

### 3.1 How the arms were built so the answer could not be an accident

* The reference is **vanilla's own shipped `.BTR`** --
  `E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Terrain/Commonwealth/Commonwealth.4.-20.24.BTR`,
  27,565 bytes, dated 2025-10-08 -- drawn with vanilla's own shipped sheets. Not
  a reconstruction of Bethesda's chunk: Bethesda's chunk.
* Every other arm is OUR `.BTR` from the same one-chunk bake. The `.BTR` is
  **byte-identical across all six bakes** (rung / off / vanfmt / cache / ourleg /
  ourvan, sha1 `dbe3328b06c9...`), so between those arms every differing pixel
  came out of the sheets and nothing else.
* The camera is PINNED, not auto-fitted, because the two meshes have different
  bounds and an auto-fit would have framed them differently: orthographic,
  look-at `8192,8192,8938`, half-width 8600, frame **1358x1024**,
  **12.665685 world units per pixel** -- all read back out of
  `release/ww_camera_pin.log` at the grab, never quoted from the request.
* Resource roots are shaped `<root>/Textures/Terrain/Commonwealth/<sheet>`.
  **The refuter that the roots are read at all**: an arm `mix` carrying
  vanilla's COLOUR with OUR `_msn` renders 3.34 levels away from the pure
  vanilla-sheet arm and differs in 168,211 pixels. A root that missed would have
  produced byte-identical PNGs.
* The mask is the pixels covered in EVERY arm (1,325,055 px, 95.3 % of the
  frame), so no arm can win by covering less.

### 3.2 The numbers

Mean Euclidean RGB distance, in levels, to the vanilla reference render:

| arm | median | mean | mean off the mesh disagreement | terrain RGB mean |
|---|---|---|---|---|
| vanilla `.BTR` + vanilla sheets (reference) | 0.00 | 0.00 | 0.00 | 131.8 / 121.3 / 106.8 |
| **BEFORE**: our `.BTR` + vanilla sheets | 138.47 | 155.70 | **137.77** | 23.5 / 19.6 / 74.9 |
| our `.BTR` + our sheets, legacy container | 133.88 | 150.25 | 131.95 | 28.1 / 23.4 / 87.1 |
| our `.BTR` + our sheets, `--sheet-format vanilla` | 133.88 | 150.25 | **131.95** | 28.1 / 23.4 / 87.1 |
| our `.BTR` + our sheets, `--msn-cache` | 136.34 | 152.89 | 134.77 | 26.3 / 21.7 / 81.6 |
| **AFTER**: our `.BTR` `--no-terrain-identity` + vanilla sheets | **0.00** | 26.95 | **0.48** | 116.9 / 105.4 / 90.9 |

6.22 % of the masked pixels (82,388) sit at a mean distance of 426.3 in EVERY
arm identically: that is the water shape vanilla's mesh carries at this chunk
and ours does not. It is a mesh coverage difference, not a colour one, so the
fourth column excludes it and says so. The `p95` and `max` of every arm
(428.27 / 441.67) are that same region and are identical across arms for the
same reason.

**`--sheet-format vanilla` moved the render by 0.00 levels and 0 pixels.** That
is measured on the pinned frame and separately on the auto-fit frame: our
legacy arm and our vanilla-format arm are the same picture, pixel for pixel.
It is exactly what section 2.4 predicted from the block bytes -- with alpha
constant 255 the BC1 punch-through mode never triggers, so a DXT5 colour block
and a DXT1 colour block of the same texels are the same bytes. **The switch
changes the container and the length of the mip chain. It cannot change a
colour, and it did not.**

### 3.3 Where the blue-purple comes from

`WW_RENDER_FLAT=1` draws vertex colours only -- no texture, no lighting:

| mesh | vertex-colour RGB mean | SD |
|---|---|---|
| vanilla's `.BTR` | 237.3 / 237.3 / 237.3 | 60.9 / 60.9 / 60.9 |
| ours, shipped default | **50.9 / 49.3 / 202.1** | 29.3 / 39.9 / 55.0 |
| ours, `--no-terrain-identity` | 237.2 / 237.2 / 237.2 | 61.1 / 61.1 / 61.1 |

Vanilla's terrain vertices are neutral. Ours are dark blue. The site is
`src/lodgen.cpp` around line 952, inside `if ( opts.terrainIdentity )`:

```cpp
// A = shore proximity. It was the terrain profile's last free slot.
nif->set<ByteColor4>( row, "Vertex Colors", ByteColor4( FloatVector4(
    float( tMat[s] ) / 255.0f, float( tWet[s] ) / 255.0f,
    float( tAo[s] ) / 255.0f, float( tShore[s] ) / 255.0f ) ) );
```

R = the land-texture material CLASS id, G = wetness, B = ambient occlusion,
A = shore proximity. A class id is a small hashed integer and wetness is mostly
zero, so R and G land near 50 while AO sits near 202 -- which is the measured
50.9 / 49.3 / 202.1 to within a level. Any consumer that multiplies the albedo
by the vertex colour, this viewer included, then renders the terrain
blue-purple. Two independent instruments agree on the multiplier: a synthetic
flat GREY colour sheet (128,128,128) renders 31.0 / 28.7 / 131.1 and a
synthetic flat RED sheet (255,0,0) renders 59.8 / 0.2 / 0.2, i.e. the colour
sheet decides the hue completely and the vertex colour is what darkens R and G.

`opts.terrainIdentity` defaults to **true** (`src/lodgen.h:578`) and the CLI's
`lgTerrainIdentity` defaults to **true** (`src/nifcli.cpp:6442`). The switch
that turns it off is `--no-terrain-identity`; `--no-identity` is a DIFFERENT
switch (it controls the OBJECT identity channels) and a bake with it produced a
`.BTR` byte-identical to the default, which is how the two were told apart.

`docs/LODGEN_PARITY.md` says of the identity channels: *"Default output carries
none of it."* That is not what the CLI does today. Whether it should is not a
measurement I get to make.

**This is not a recommendation about how the terrain should look.** The identity
channels are a deliberate feature -- the LOD channel preview, the material
class, wetness and AO views all read them, and the whole `WW_LOD_CHANNEL` route
exists to show them. What is measured here is only that they are written into
the slot a renderer multiplies the albedo by, that vanilla leaves that slot
neutral, and that switching them off puts our chunk 0.48 levels from vanilla's
own render. What to do about it is bungo's call.

### 3.4 What this says about items 2 and 8

Neither `--sheet-format vanilla` (0.00 levels) nor `--msn-cache` (2.82 levels
away from the legacy arm, in the direction of vanilla) can affect the
blue-purple, because the blue-purple is not in a sheet. Both switches remain
what section 2 and section 5 measured them to be; neither is the cure for the
picture that motivated this lane, and neither was ever going to be.

Picture: `scratchpad/terrainfmt1_20260912/images/cmp_render_msn.png` (six tiles,
the three lit arms above the three vertex-colour arms, the camera in the
caption). Cache pair: `images/cmp_msn_cache.png`. Raw numbers:
`f3_render_pinned.json` and `f3_render.json`; the arms are built by
`f3_render.sh` / `mkroots.sh` / `f3_synth.py`, the metric by `f3_metric2.py`.

## 4. The `_msn` detail term (item 4, gate F3) -- REFUSED, with the numbers

**No `--msn-detail` switch was written.** The measurement was taken first, as
the brief requires, and the candidate fails three separate tests. The refusal
is the deliverable.

### 4.1 The gap it would have to close

Texel-scale roughness = mean |1-texel neighbour difference| over the east (R)
and north (B) channels of the `_msn` at mip 0, in levels of 255. Two tiles,
both baked with `--land-detail-source none` so the sheet is OUR bake and not a
byte copy of Bethesda's (the shipped default copies vanilla's file, and would
have measured vanilla twice):

| tile | vanilla | ours | ours / vanilla |
|---|---|---|---|
| `Commonwealth.4.-20.24` | 19.825 | 2.221 | 11.2 % |
| `Commonwealth.4.-16.24` | 20.302 | 2.326 | 11.5 % |

Vanilla's number is not compression noise: section 5.2 measured that bungo's
cleaned 2K cache, which has the BC1 block grid removed, keeps 92.9 % of
vanilla's fine energy in the 512 band. About a fourteenth of it is the blocks;
the rest is real.

### 4.2 The candidate, built and fitted

The candidate is the one item 4 names: each land texture's own `_n` sampled at
the FOOTPRINT mip with the same 341.333 tiling and the same weights, its
departure from the repeat average scaled by a strength and added to the height
normal's east and north -- which is exactly what `--land-detail` already does
on the diffuse (`src/lodgen.cpp` ~9186).

The geometry is fixed and leaves no room for choice: a dim-4 chunk is 16384
world units over 512 texels = 32 units a texel; 32 / 341.333 = 0.09375 of a
repeat a texel; on a 1024x1024 `_n` that is 96 source texels, so the footprint
mip is log2(96) = **6.585** and two adjacent sheet texels sample 1.5 texels of
mip 6 apart. Every Fallout 4 landscape `_n` in the sample is BC5U 1024x1024
with 11 mips (22 of 22 read); the BC5 decoder is cross-checked against
`dds_np`'s BC3 alpha decode on a real sheet and is bit-equal.

Built on the sheet in the BEST case the generator could ever hit -- ONE land
texture covering the whole chunk at full weight, no layer blend to average the
term down:

| | tile -20.24 | tile -16.24 |
|---|---|---|
| roughness at strength 1, mean over 22 textures | 7.085 (**35.7 %** of vanilla) | 7.149 (**35.2 %**) |
| best single texture at strength 1 (`DriedGrassObj02_n`) | 19.068 (96 %) | 19.093 (94 %) |
| median strength needed to reach vanilla | **5.19** | **5.31** |

A real chunk blends one to four layers per quadrant, which can only move the
first row down. Two of the twenty-two textures reach vanilla near strength 1;
neither is what covers Sanctuary.

The strength is not physically absurd on its own: at the fitted value the
tangential length of (east, north) exceeds 1 on 0.1--0.3 % of texels for most
textures (worst `AsphTrim01_n`, 11.3 %). So a fit EXISTS. The next two tests
are why it does not mean anything.

### 4.3 Test 1: is the detail in the same places as vanilla's?

Both sheets high-passed (texel minus its 3x3 box mean -- exactly the scale the
roughness statistic measures), then correlated over the whole 512x512:

| | tile -20.24 | tile -16.24 |
|---|---|---|
| candidate's fine detail vs vanilla's, mean \|r\| over 22 textures | **0.0010** | **0.0013** |
| best single texture | 0.0032 | 0.0028 |
| **floor**: the candidate's own phase-randomised twin (same power spectrum, structure destroyed) | **0.0011** | **0.0010** |
| our own coarse bake's fine detail vs vanilla's | 0.0116 | 0.0144 |

**The candidate scores its own noise floor.** Fitting the strength makes the
amplitude match; the detail lands nowhere near vanilla's. Our own bake, which
carries a ninth of the amplitude, correlates with vanilla ten times better than
the candidate does.

### 4.4 Test 2: what the candidate adds that vanilla does not have

341.333 world units into 16384 is exactly **48 repeats across the sheet**, so a
blended land `_n` term must appear as a spectral line at k = 48. Ratio of the
k=48 row and column of \|F\| to the median of k in 40..56 excluding 48 (1.0 = no
line):

| | vanilla `_msn` | our bake | candidate s=1 | candidate at the fitted strength |
|---|---|---|---|---|
| tile -20.24 | 0.988 | 0.991 | **6.089** | **10.094** |
| tile -16.24 | 1.027 | 0.986 | **5.810** | **9.627** |

Vanilla's sheet carries no repeat at the tiling frequency and neither does
ours. The candidate carries one at six times its neighbours at strength 1, and
ten times at the strength needed to match vanilla's roughness. The picture
shows it plainly: vanilla's high-pass is directional, flow-like erosion; the
candidate's is a regular lattice, and its phase twin looks the same.

### 4.5 The refusal

The candidate reaches about a third of vanilla's texel-scale roughness at
strength 1; reaching all of it needs the term multiplied by roughly five; and
at either strength it correlates with vanilla's actual detail no better than
its own phase-randomised noise, while adding a periodic repeat vanilla does not
have. **It does not reproduce vanilla's detail, so it was not shipped.** No
code was written, no `--msn-detail` switch exists, and the rung's bytes are
therefore untouched by this item by construction -- there is nothing to prove
off, because there is nothing on.

What this does NOT say: it does not say vanilla's detail cannot be reproduced,
only that this source cannot reproduce it. Vanilla's `_msn` fine structure is
directional and flow-like, which is what an erosion term produces and what
`--land-detail-source erosion` in this tree already aims at; that path was not
measured here and this lane makes no claim about it. It also does not say
anything about how the terrain should LOOK.

Scripts: `f3_detail.py`, `f3_ceiling.py`, `f3_fit.py`, `f3_struct.py`,
`f3_repeat.py`, all on the shared statistics in `f3_common.py`, with the BC5
reader `bc5_np.py`. Numbers: `f3_detail.json`, `f3_ceiling.json`,
`f3_fit.json`, `f3_struct.json`, `f3_repeat.json`. Picture:
`images/cmp_msn_detail.png`.

## 5. bungo's cleaned 2K `_msn` cache (ADDED ITEM 8, gate F5)

`E:/Tools/Upscale/esrgan-bat/output` is a **read-only input**. Nothing in this
lane wrote to it, and nothing read it except `f5_cache.py` and the bake.

### 5.1 (a) What the cache is — measured before anything shipped

Inventory: **2,304 PNG files, every one 2048x2048 RGB**, 16,631,440,862 bytes
(15.49 GiB) as PNG. Names cover **exactly** the 2,304 dim-4 vanilla `_msn`
sheets on disk — 2,304 of 2,304, with 2,304 vanilla dim-4 `_msn` files to match.
Full coverage of the Commonwealth's finest terrain level, nothing else.

Sample: 16 chunks, drawn seeded, with `Commonwealth.4.-20.24` forced in.

**The channel layout is not vanilla's, and this was measured rather than taken
from the note.** Cross-correlation of every cache channel against every vanilla
channel, on the box-downsampled 2K sheet (median over 16):

| | vanilla R (east) | vanilla G (up) | vanilla B (north) |
|---|---|---|---|
| cache R | **0.956** | 0.004 | 0.034 |
| cache G | 0.030 | 0.007 | **0.794** |
| cache B | 0.000 | 0.000 | 0.000 |

Cache B is **identically zero** on 14 of the 16 sheets; the other two have a
maximum of 1 at a nonzero fraction of about 1e-6. So the cache is a
**two-channel tangential normal, and UP is the channel that is gone** — not
blue, and not "G recomputed from R and B". The handoff note's "recompute the
missing channel" is what the bytes support; the bat's description of which
channel was rebuilt is not.

**Up is recoverable, but only by renormalising.** A median **0.11 %** of texels
per sheet have east² + north² > 1 (the worst sampled chunk,
`Commonwealth.4.-40.72`, has **26.28 %**, and the largest tangential length seen
is 1.4142 — a texel at full extent on both channels). Clamping those leaves a
normal that is not unit length, so the ingest renormalises. Recomputed up, mean
level 243.81, against vanilla's own up mean of 246.11.

**Agreement with vanilla, every channel against its own phase-randomised twin**
(median r, and the count of sheets that beat the twin):

| channel | r vs vanilla | twin floor | beats the floor |
|---|---|---|---|
| east | 0.9558 | −0.0006 | **16 of 16** |
| north | 0.7942 | −0.0010 | **16 of 16** |
| up (recomputed) | 0.4691 | 0.0017 | **16 of 16** |

**Fine energy.** At the 512 sheet's own 4-texel band (cache downsampled to 512):
cache 22.63, vanilla 24.35 — the cleaned sheet is **92.9 % of vanilla's fine
energy there, i.e. slightly smoother, not sharper.** The phase twin reads 24.39
and is **not a floor for this statistic** (it shares vanilla's amplitude
spectrum by construction); it is quoted only so nobody mistakes it for one.
Below vanilla's resolution, at the 2K sheet's own 4-texel band: cache 13.58
against a bicubic 4x upscale of vanilla at 6.76 — **about 2.0x**, which is
detail that vanilla's 512 sheet does not contain. Whether that invented detail
is wanted is not a measurement and is not mine to say.

**The BC1 block grid** (mean |neighbour difference| on the lines where
index % period == 0, over the mean elsewhere; 1.0 = no grid):

| what | period | grid line |
|---|---|---|
| vanilla's own 512 `_msn` | 4 | **1.0975** |
| the cleaned 2K | 16 | **1.0259** |
| a bicubic 4x upscale of vanilla — cleans nothing, on purpose | 16 | **1.3460** |

A first attempt used a nearest/`kron` upscale as the control and read 5.49: that
is the control manufacturing a step at every 4th texel, not a measurement, and
it was thrown away. Against a control that behaves, **his chain removed the
grid.**

### 5.2 (b) The ingest, and why it is uncompressed

`--msn-cache <dir>`; a chunk whose `<ws>.<dim>.<x>.<y>.png` is in that directory
gets its `_msn` from it. East = cache R, north = cache G, up recomputed, the
triple **renormalised**, alpha 255, written as **uncompressed B8G8R8A8 through a
DX10 header (DXGI 87) with a full mip chain**.

Measured on the sheet the bake wrote for chunk (−20,24):

| | rung `_msn` (= vanilla's file) | `--msn-cache` `_msn` |
|---|---|---|
| format | `DXT5` | `B8G8R8A8_UNORM` (DX10, array size 1) |
| size | 512x512, 10/10 mips | 2048x2048, **12/12 mips** |
| bytes | 349,680 | 22,369,768 |
| alpha | 255..255 | 255..255 |
| unit length, mean | 1.0215 | **1.0000** |
| unit length, worst texel | 77.67 levels off | **0.68 levels off** |
| block-grid line at period 4 | **1.1210** | **1.0029** |

Vanilla's own sheet is not unit length after a block decode — 77.67 levels off
at its worst texel — so "within 1/255" is a bar the cached sheet passes (0.68
levels) and the sheet it replaces does not.

**The refuter for "uncompressed, because a DXT re-encode brings the blocks
back", on one sheet:** the cleaned 2K sheet was put through the tree's own
colour-block rule (`lodgenEncodeBC1Block`: min/max-luminance endpoints, RGB565,
four-entry palette — and a BC3 colour block is that same block) and measured
again.

> block-grid line at period 4: cleaned **1.0029** → re-encoded **1.9849**

The grid comes back at nearly twice the surrounding gradient. Note honestly that
this is OUR encoder: 1.985 is harsher than the 1.121 vanilla's own sheet reads,
because our endpoint rule is min/max luminance rather than a least-squares fit.
The direction is not in doubt — a block format re-imposes the block — but the
magnitude is a property of this encoder, not of BC generally.

**BC7 is not implemented.** There is no BC7 encoder anywhere in this tree, and
writing one was not this lane's job. What it would cost is in 5.4.

### 5.3 Off is the rung's bytes

The `off` bake in 2.3 was run with no `--msn-cache` at all: 10 files, 0 differ
against the rung. With the flag present, **two** files change, and the second
one is not a surprise once read:

* `tex/Commonwealth.4.-20.24_msn.DDS` — the sheet, which is the point;
* `obj/Commonwealth.lodb` — the bake ledger, in **one field**: `switches`, the
  hash of the switch set the bake ran with (35 bytes differ, all inside that
  hex string; `inputs`, `loadOrder`, the region, and the sha1 of every emitted
  BTO/BTR/manifest are unchanged). `--sheet-format vanilla` moves the same
  field the same way. A ledger whose switch hash did NOT move when a switch
  moved would be the defect.

Everything else — the colour sheet, `_data`, both `.lodt` containers, the
`.lodm`, the BTO, the BTR and the manifest — is byte-identical to the rung's.

### 5.4 (d) What it costs — the numbers, for bungo's call

The `_msn` cache covers the 2,304 dim-4 chunks of the Commonwealth. Every
figure below is a full mip chain to 1x1 plus the 148-byte header, computed from
the format, and the 22,369,768 B row is confirmed against the sheet the bake
actually wrote.

| what the `_msn` would be | per sheet | x 2,304 |
|---|---|---|
| vanilla today (DXT5 512, 10 mips) | 349,680 B | **0.75 GiB** |
| **uncompressed 2048 B8G8R8A8 — what ships behind the flag now** | 22,369,768 B (21.33 MiB) | **48.00 GiB** |
| uncompressed 1024 (mip 0 dropped to 1K) | 5,592,552 B (5.33 MiB) | **12.00 GiB** |
| BC7 2048, full chain — **not implemented** | 5,592,580 B | 12.00 GiB |
| BC7 2048, mip 0 only (the brief's figure) | 4.00 MiB | 9.00 GiB |
| BC7 1024, full chain — **not implemented** | 1,398,276 B | 3.00 GiB |

For scale: **all 6,120 shipped Commonwealth terrain sheets together are
2,140,041,600 B = 1.99 GiB.** The uncompressed 2K cache is 24x that whole set;
at 1K it is 6x; BC7 at 2K would be 6x and at 1K 1.5x.

**Dropping mip 0 to 1K costs a factor of four in size and half the linear
resolution.** What it costs in appearance is not something a number here can
answer, and I am not going to pretend otherwise: what IS measured is that the
cleaned sheet's extra detail lives below vanilla's own resolution (5.1: 2.0x the
fine energy of a bicubic upscale at the 2K band, against 92.9 % of vanilla's at
the 512 band), so a 1K mip 0 keeps a 2x linear gain over vanilla and throws away
the half of the invented detail that is finest. **This is bungo's call and this
lane does not make it.**

Load cost was **not measured**. No timing of the bake with and without the flag
was taken beyond the bakes' own wall clock, and nothing was measured in the
game — the sheets were not installed. Saying so rather than quoting the bake's
seconds as a load cost.


## 6. Gate F4 -- the build, and the nine harnesses

### 6.1 The build, and what it links

| | |
|---|---|
| exe | `release/NifSkope.exe` 2026-09-12 12:58:48, 22,007,808 B, sha1 `ba7585cba389c8b38f0e07c6c11063e0cec1124c` |
| rung | `release/NifSkope.before_terrainfmt1.exe` 2026-09-12 11:51:35, 22,000,128 B, sha1 `ecf5ecab537f70a3409f2df3da4f4bbda2b08708` |
| sources | `src/lodgen.cpp` 12:55:40, `src/lodgen.h` 12:53:09, `src/nifcli.cpp` 12:56:25 -- all older than the exe |

ONE build was run this lane, which is what the brief allows. Gate F3 was
refused, so no `--msn-detail` code exists and no second build is owed.

`make -n` in the MSYS2 UCRT64 shell after the build emits **0** compile or link
commands (`grep -cE "g\+\+|gcc"` over its output = 0): nothing is left to
compile.

Object staleness against the one header touched, `src/lodgen.h` (12:53:09) --
every translation unit that includes it, checked by mtime WITHOUT touching any
source:

    ok  GeneratedFiles/.obj/lodgen.o
    ok  GeneratedFiles/.obj/lodgenmanager.o
    ok  GeneratedFiles/.obj/main.o
    ok  GeneratedFiles/.obj/nativeemit.o
    ok  GeneratedFiles/.obj/nifcli.o
    ok  GeneratedFiles/.obj/nifskope_ui.o

Six of six newer than the header. No stale object.

### 6.2 Both switches at off == the rung's bytes

`bake/rung` (the rung exe, no new flags) against `bake/off` (the new exe, no new
flags), the same one-chunk region, every file compared with `cmp`:

    rung vs off: 10 files, 0 differ

    mod/Terrain/Commonwealth.VT.2.lodt      mod/Terrain/Commonwealth.VT.4.lodt
    mod/Terrain/Commonwealth.VT.lodm        obj/Commonwealth.4.-20.24.BTO
    obj/Commonwealth.4.-20.24.BTO.manifest.txt
    obj/Commonwealth.4.-20.24.BTR           obj/Commonwealth.lodb
    tex/Commonwealth.4.-20.24.DDS           tex/Commonwealth.4.-20.24_data.DDS
    tex/Commonwealth.4.-20.24_msn.DDS

That is the whole output tree, not a sample, and it includes the `.lodb` ledger
whose `switches` hash would move if either default had moved.

### 6.3 The nine harnesses, against GROUND1's baselines

Run from Git-Bash (whose `python` has numpy), exe 12:58:48, driver
`scratchpad/terrainfmt1_20260912/f4_harness.sh`, one log per harness
(`h_<name>.log`).

| harness | GROUND1 baseline | this run | verdict |
|---|---|---|---|
| `lodgen_roads` | 11 checks, 0 failures | **11 / 0** | matches |
| `lodgen_native` | 18 checks, 0 failures | **18 / 0** | matches |
| `lodgen_native_baseline` | 25 files, 25 baked, 0 differ | **25 / 25 / 0 differ** | matches |
| `lodgen_terrain_vt` | 41 checks, 1 failure | **41 / 1** | matches, same check |
| `lodgen_ground_cover` | 29 checks, 5 failures, by NAME | **29 / 5**, names below | matches |
| `lod_generation` | 116 checks, 0 failures | **116 / 0** (floor 116) | matches |
| `lodl_open` | 23 checks, 0 failures | **23 / 0** | matches |
| `lodgen_terrain_pbrm` | 14 checks, 0 failures | **14 / 0** | matches |
| `animws` | 236 checks, 0 failures, 1 skip | **236 / 0 / 1 skip** | matches |

`lodgen_native_baseline` also records the two exes it compared:
baseline exe `664e0de4...` 2026-09-10T03:57:46, this exe `6009233c...`
2026-09-12T12:58:48 -- 25 stock files byte-identical across a two-day gap and
this lane's build.

**The one `lodgen_terrain_vt` failure is the baseline's own**, named:

    FAIL V9c the direct sheets are continuous ACROSS a chunk seam (the block above)
         the _data sheets differ (expected: the wetness domains are not the same grid)

**`lodgen_ground_cover`'s five, by name, and the floor that proves they are not
mine.** The brief asks for a NAME comparison and GROUND1's deliverable records
only the count, so the comparison was made INSIDE this lane instead: the same
harness was run a second time against the RUNG exe (`EXE=` is honoured at
`tests/spells/lodgen_ground_cover.sh:36`), and the two name lists set side by side.

| check | new exe | rung |
|---|---|---|
| C0 the exe is newer than every source | ok | **FAIL** |
| C1 a frozen baseline exists to compare against | FAIL | FAIL |
| C2 this ground has no cover (coverMax=69) | FAIL | FAIL |
| C2 all three grass-free sheets are byte-identical with `--cover` | FAIL | FAIL |
| C2 a grass-free chunk stays DXT1 (fourCC DXT5) | FAIL | FAIL |
| C6a the model and the bake agree on the composite | FAIL | FAIL |
| C9 the slope gate is there and it bites | FAIL | FAIL |
| C16 the cover survives the mip chain | FAIL | FAIL |
| C11b one grass colour explains both halves | FAIL | FAIL |
| C3..C17 the measurements on the files (roll-up) | FAIL | FAIL |
| totals | **29 / 5** | 29 / 6 |

Every named failure is identical between the two exes. The rung's ONE extra
failure is `C0 the exe is newer than every source this answer depends on`, which
fails because the rung is an 11:51:35 copy and the sources were edited after it
-- an artefact of running a frozen exe, not a difference in behaviour. So: the
five failures of the new exe are, by name, the five the rung already had, and
none of them is this lane's. The rung's log is `h_ground_cover_RUNG.log`.

### 6.4 Processes

`tasklist` immediately after the chain and again after the rung run:
**Fallout4.exe 0 lines, NifSkope.exe 0 lines.** Nothing was left running, and
bungo's own window was not open at any point during the chain (every instance
this lane launched carried `--port`).

### 6.5 The rung IS the launch exe, and there is an outside witness

The rung `release/NifSkope.before_terrainfmt1.exe` is 2026-09-12 11:51:35,
22,000,128 B, sha1 `ecf5ecab537f70a3409f2df3da4f4bbda2b08708`.

The lane before this one, GROUND1, closed at 12:28 -- five minutes before this
lane launched at 12:33 -- and its own handoff block records the exe it left
behind as *2026-09-12 11:51:35, 22,000,128 B, sha1
`ecf5ecab537f70a3409f2df3da4f4bbda2b08708`*. Same timestamp, same size, same
sha1, written down by a different lane before this one existed. So the rung is
the exe as it stood at launch, on a record this lane did not author, not merely
on its own timestamp.

### 6.6 What gate F4 does NOT cover

* The harnesses were run once each, not repeated, so nothing here bounds their
  run-to-run variation.
* `lodgen_native_baseline` compares 25 stock files. It does not exercise either
  new switch -- that is what 6.2 is for.
* No harness renders with `--sheet-format vanilla` or `--msn-cache` on. Both
  switches are off in every harness, by design, because the gate being tested is
  that off is the rung's bytes.


## 7. The pictures

All under `scratchpad/terrainfmt1_20260912/images/`. Every size READ BACK with
PIL, never the size requested. Every bake behind them used `--road-detail 1`.

### 7.1 The three the brief asks for

| file | size | what it shows |
|---|---|---|
| `cmp_render_msn.png` | 1608x923 | item 3. Five panels on one camera: vanilla's own `.BTR` + vanilla sheets (reference, 0.00), our `.BTR` + vanilla sheets (BEFORE, 137.77), our `.BTR` + our legacy sheets (131.95), our `.BTR` + `--sheet-format vanilla` (131.95, 0 px differ from the panel left of it), our `.BTR` with `--no-terrain-identity` (AFTER, 0.48). The number is in every label |
| `cmp_msn_detail.png` | 1572x756 | gate F3, the REFUSAL. Ten tiles: the raw `_msn` east channel and its high-pass, for vanilla, our bake, the candidate at strength 1, the candidate at the fitted strength, and the phase-randomised twin floor |
| `cmp_msn_cache.png` | 1608x489 | gate F5 / item 8(c). The render pair on chunk -20,24 with and without `--msn-cache`, beside the vanilla reference |

The page-level caption on each names the camera with the number the application
logged, not the one asked for: orthographic, look-at 8192,8192,8938, half-width
8600, frame 1358x1024, `upp` 12.665685 out of `release/ww_camera_pin.log`.

### 7.2 The working renders the pages are cut from

`p_vanmesh_van.png`, `p_ourmesh_van.png`, `p_ourmesh_ourleg.png`,
`p_ourmesh_ourvan.png`, `p_ourmesh_cache.png` -- the five lit arms;
`p_lit_notid.png`, `p_lit_noid.png` -- the two identity switches;
`p_flat_vanmesh.png`, `p_flat_ourmesh.png`, `p_flat_notid.png`,
`p_flat_noid.png` -- `WW_RENDER_FLAT=1`, vertex colours only, which is the
picture that carries the finding (vanilla 237.3/237.3/237.3 against ours
50.9/49.3/202.1).

`r_top_*.png` / `r_flat_*.png` are the un-cropped originals of every arm
including the refuters: `r_top_mix.png` (vanilla colour + our `_msn`),
`r_top_syn_red.png` / `r_top_syn_grey.png` / `r_top_syn_flatn.png` (the
synthetic flat sheets that measure the renderer's own response),
`r_top_van_nodata.png` / `r_top_syn_grey_nodata.png` (the `_data` sheet
removed, which changes nothing).

`d_raw0..4.png` / `d_hp0..4.png` are gate F3's ten tiles before the page was
assembled, so the page can be rebuilt without re-running the fit.

## 8. The documents

| path | what changed |
|---|---|
| `docs/LODGEN_TERRAIN_VT.md` | 176,501 B / 2,715 LF -> **185,143 B, CR 0, LF 2,841**. Two CLI rows (`--sheet-format vanilla\|legacy`, `--msn-cache DIR`); a new `## 7a. The far sheet format, and the _msn cache` section (7a.1 what vanilla ships, 7a.2 `--sheet-format vanilla`, 7a.3 `--msn-cache`, 7a.4 what these switches are NOT); a provenance block with the three source hashes and 10 anchors each asserted to appear exactly once |
| `scratchpad/lane_terrainfmt1_report.md` | this report, written a section per gate as it landed |

Deliverables for the director to splice, all in
`scratchpad/terrainfmt1_20260912/`: `WW_CHANGES_ENTRY.md`, `HANDOFF_BLOCK.md`,
`MISTAKES_ENTRIES.md`, `CHANGED_FILES.txt`. This lane did not edit
`WW_CHANGES.md`, `MISTAKES.md` or `HANDOFF.md`.

**One document contradiction found and NOT fixed by this lane**:
`docs/LODGEN_PARITY.md` says of the terrain identity channels *"Default output
carries none of it"*, and the CLI's default is `lgTerrainIdentity = true`
(`src/nifcli.cpp:6442`), so the default DOES carry it -- which is section 3's
whole finding. Reported, not edited, because the fix is a decision about what
the default should be and that is bungo's.

## 9. Skills

Both in THIS tree's `.claude/skills`, and both listed in `CHANGED_FILES.txt`.

**New: `.claude/skills/ww-render-arm-isolate/SKILL.md` (5,296 B, CR 0, LF 106).**
The procedure that turned this lane around -- finding which INPUT a rendered
defect comes from before writing code to fix it. Eight sections: one resource
root per arm and the `<root>/Textures/...` vs `<root>/Data/Textures/...` trap
that misses silently; the MIX arm that proves a root is read at all; holding the
mesh byte-identical so only the sheets can differ; pinning the camera when the
meshes differ; the SYNTHETIC flat input that measures the renderer's own
response; the `WW_RENDER_FLAT` vertex-colour probe; the masked distance number
and its floor; and switch names that are not synonyms. It opens with this lane's
own number, that the switch the brief named moved the render by 0 pixels.

It is a new skill rather than an amendment because the tree's
`nifskope-ww-render-shot` is about GETTING a render out of the application and
this is about deciding what a render MEANS -- and because
`ww-texel-picture`'s own description already routes geometry pictures away to
`nifskope-ww-render-shot`.

**Amended: `.claude/skills/ww-texel-picture/SKILL.md` (6,682 -> 8,300 B, CR 0,
LF 146).** A new `## 8. The ARM page`: every arm on the page including the
reference, the BEFORE and the refuter, each with its own number in its own
label; the PAGE caption wraps and grows the canvas where section 3's PANEL
caption is handled by the fixed cell; and the caption names the camera with the
application's logged `upp`, not the requested framing. That last one is the
defect the working script had -- the caption ran off the right edge and only
opening the PNG showed it, which is section 5 of that same skill catching its
own author.

---

Lane closed 2026-09-12 13:45. `DONE` written, `BUILDING` removed. 0 Fallout4.exe
and 0 NifSkope.exe left running.
