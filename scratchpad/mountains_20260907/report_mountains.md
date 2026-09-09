# Lane MOUNTAINS — why rebaked Fallout 4 terrain LOD goes dark

Read-only forensic lane. Everything below is measured on this machine unless
labelled UNVERIFIED or RECALLED. Sections 1-6 hold the evidence; the two
sections here hold the conclusion.

---

## THE ANSWER

Fallout 4's Commonwealth worldspace is 192x192 cells, but **only the playable
middle of it — cells x -36..32, y -41..32, 10.7 % of the total — has any
landscape texture painted into the ESM at all**; the mountains bungo is looking
at north of Sanctuary have height data and *nothing else*, no base texture, no
layers, and no worldspace default to fall back on. Bethesda nevertheless shipped
hand-made, per-cell-distinct terrain LOD textures out there which are measurably
**more** saturated than the playable area (meanSat 0.191 vs 0.155), so a tool
that regenerates terrain LOD from the ESM has literally nothing to sample in the
outer region and must substitute a single flat default — **that accounts for the
DESATURATION and most of the FLATNESS outright, and for the DARKENING if the
substitute is darker than vanilla's mean of lum 71.0**. Two things compound it:
xLODGen ships a documented "Default diffuse size / Default normal size" control
whose stated purpose is to shrink the textures of exactly these "outer regions
without landscape textures", and because it shrinks the **normal** map too it
removes the only thing that shades terrain at that distance — a level-32 mesh
carries **2.86 triangles per cell**, so the `_msn` is doing all the work.
What I could **not** measure: there is no rebaked LOD anywhere on this machine,
so the exact colour his tool substituted is unverified, and so is whether
xLODGen transposes the normal map's green and blue channels — a mistake his own
NifSkope fork *does* make (`src/lodgen.cpp:5026`), and which would cost **68 %
of the light and 92 % of the shading variation** on its own.

---

## WHAT TO CHECK OR CHANGE

1. **First, rule out the free explanations.** His load order has
   `F76Weathers.esp` and `UltraExteriorLighting.esp`; if the two screenshots
   were taken under different weather or time of day, part of the difference
   is not LOD. And check whether *mid-distance terrain inside the playable
   box* also went dark. If it did, the cause is global (normal map / vertex
   colour intensity / gamma). If only the outer region changed, it is the
   missing-source-data cause below. **This one observation discriminates
   between the two candidates and costs nothing.**
2. **Do not regenerate terrain LOD for the outer region at all.** Vanilla's
   outer tiles cannot be reproduced from `Fallout4.esm` — the data is not
   there (section 2.4). Generate only over roughly cells -36..32 / -41..32
   using xLODGen's Chunk options and keep Bethesda's shipped tiles outside it.
   This is the single highest-value change. Mind the documented skip rule: to
   re-do a level-4 tile you must also delete the level 8/16/32 tiles covering
   it, or generation is silently skipped.
3. **Set "Default diffuse size" and "Default normal size" to `None`.** These
   are the controls that, by their own documentation, minimise "outer regions
   without landscape textures" — the exact cells in question.
4. **Set Normal Size for LOD4 to 512, not the 256 the "native" hint suggests.**
   Vanilla FO4 ships every terrain LOD texture at 512x512 (measured); accepting
   256 gives a quarter of the texels for the map that carries all the distant
   shading. Consider enabling "Bake normal-maps" as the readme suggests.
5. **Verify the generated normal map's channel order before shipping the bake:**
   `python msn_updecide.py <out>\Textures\Terrain\Commonwealth\Commonwealth.4.<x>.<y>_msn.DDS`.
   Green must be the up channel — small residual, and zero pixels below 128.
   If it is not, that is a second, independent and much larger problem.
6. **Leave Diffuse Brightness / Contrast / Gamma alone until 1-5 are done.**
   They compensate for a bad bake rather than fixing one, and they cannot
   restore saturation or relief that was never generated.
7. **If he is using this fork's own `lodgen`, its `_msn` is wrong.**
   `src/lodgen.cpp:5021-5027` (and the same packing at `:5994-5999`) writes
   `R = east, G = north, B = up`; FO4 wants `R = east, G = up, B = north`.
   Its shader flags, by contrast, are byte-for-byte vanilla (section 3.2), so
   this is a one-line class of fix, not a redesign.

---
---

# THE MEASUREMENTS

## 0. TOOLING BUILT AND VERIFIED (overseer, personally)

`C:\Users\bungo\AppData\Local\Temp\claude\laneb\dds.py` — honest DDS reader.
Parses the 128-byte header plus the 20-byte `DX10` extension, decodes BC1
(incl. punch-through), BC3 (BC4 alpha block + BC1 colour block), BC5, and
computes means from a **fully decoded mip**, not from endpoints and not over
the file bytes.

**Self-check that it is not lying:**

    python dds.py info <file>

prints `fileSize` and `expect` (dataOff + sum of mip sizes). For every
Commonwealth terrain LOD texture tested these are **equal** — 349680 bytes for
512x512 BC3 with 10 mips — so the mip walk is byte-exact and no header or mip
tail is being folded into a mean.

    Commonwealth.4.-20.24      512x512 BC3_UNORM mips=10 fourcc=b'DXT5' off=128 size=349680 expect=349680
    Commonwealth.4.-20.24_msn  512x512 BC3_UNORM mips=10 fourcc=b'DXT5' off=128 size=349680 expect=349680
    Commonwealth.32.-96.-96    512x512 BC3_UNORM mips=10 fourcc=b'DXT5' off=128 size=349680 expect=349680
    Commonwealth.16.-16.0      512x512 BC3_UNORM mips=10 fourcc=b'DXT5' off=128 size=349680 expect=349680

Colour sanity: a terrain diffuse decodes **earthy** (R > G > B, e.g. 87.8 /
79.7 / 68.7), not neon. Channels are not swapped.

### The overseer's 4x4-mip shortcut, checked

`lodmean.py` averages the two RGB565 endpoints of the single 4x4-mip block.
Against a full decode of mip 0:

| cell (level.x.y) | FULL mip0 rgb | lum | 4x4 shortcut rgb | lum | err |
|---|---|---|---|---|---|
| 4.-20.24 | 87.8 79.7 68.7 | 80.6 | 86.0 80.5 69.5 | 80.9 | +0.3 |
| 4.-20.40 | 87.4 79.4 68.3 | 80.3 | 90.0 80.5 65.5 | 81.4 | +1.1 |
| 4.-20.60 | 86.9 79.5 69.0 | 80.3 | 90.0 80.5 73.5 | 82.0 | +1.7 |
| 4.-20.80 | 74.9 68.7 60.7 | 69.5 | 74.0 70.0 61.0 | 70.2 | +0.7 |
| 4.0.0    | 85.4 82.3 74.9 | 82.4 | 77.5 74.5 65.5 | 74.5 | **-7.9** |
| 4.-60.60 | 88.1 81.1 70.7 | 81.8 | 90.0 80.5 73.5 | 82.0 | +0.2 |

**Verdict: the shortcut is good to about ±2 lum, but can be off by 8** (cell
0.0). Its *saturation* numbers are much worse — it reported sat 0.155 for
4.0.0 where the true saturation-of-mean is 0.124 and the mean per-pixel
saturation is 0.132; and 0.272 for 4.-20.40 where the truth is 0.218. **Do not
quote the shortcut's saturation.** The overseer's headline conclusion
survives: see below.

### Overseer item 2 re-measured properly — IT HOLDS

Vanilla's far LOD diffuse textures are **not** brighter or more saturated than
the playable ones. Full mip-0 decode:

    4.0.0    (playable)   lum 82.4  meanSat 0.132  lumStd 10.6
    4.-20.24 (playable)   lum 80.6  meanSat 0.219  lumStd  9.4
    4.-20.60 (far)        lum 80.3  meanSat 0.206  lumStd 10.2
    4.-20.80 (far)        lum 69.5  meanSat 0.189  lumStd  4.5
    4.-60.60 (far)        lum 81.8  meanSat 0.205  lumStd 15.5

produced by `python dds.py mean 0 <file>`.

So the shipped diffuse is not where the far/near difference lives.

### The LOD pyramid is complete everywhere — a first result for lens 1

    ls | grep -v _msn | awk -F. '{print $2}' | sort -n | uniq -c
       2304  level 4     (48x48 tiles of 4 cells   -> cells -96..95)
        576  level 8     (24x24 tiles of 8 cells)
        144  level 16    (12x12)
         36  level 32    (6x6)

Every level is a **complete, gapless square** over the same -96..+96 cell
extent. There is no region that exists only at 16/32. So "far cells only have
coarse LOD" is **false for vanilla** — but note that is exactly the kind of
thing a rebake can change (see lens 4).

### _msn channel convention — measured, all four channels

`python msn.py` (full mip-0 decode, per channel mean/std/min/max):

    Commonwealth.4.-20.24_msn   R 123.0 sd42.3   G 242.4 sd 8.9   B  99.4 sd33.5   A 255 sd0
    Commonwealth.4.-20.60_msn   R 108.7 sd49.9   G 240.3 sd 7.8   B 103.9 sd36.8   A 255 sd0
    Commonwealth.4.-20.80_msn   R 123.6 sd23.6   G 254.1 sd 1.4   B 123.5 sd11.5   A 255 sd0
    Commonwealth.4.0.0_msn      R 123.1 sd36.4   G 247.3 sd 8.2   B 119.5 sd31.3   A 255 sd0
    Commonwealth.4.-60.60_msn   R 172.8 sd38.4   G 232.7 sd12.7   B 126.4 sd49.1   A 255 sd0
    Commonwealth.32.-96.-96_msn R 123.8 sd19.1   G 247.9 sd11.2   B 124.7 sd14.4   A 255 sd0
    Commonwealth.16.-16.0_msn   R 129.9 sd29.7   G 237.6 sd16.2   B 122.4 sd22.5   A 255 sd0
    Commonwealth.8.-24.24_msn   R 118.0 sd40.3   G 237.0 sd12.7   B 121.7 sd36.8   A 255 sd0

R and B centre on ~123-128 (signed zero) with large spread; **G is pinned high
(232-254) and never dips below 121**; alpha is a constant 255 and carries
nothing. That is a model-space normal with **up in GREEN**, not the usual
tangent-space up-in-blue, and not DXT5nm (alpha is dead).

Note `4.-20.80_msn` G mean 254.1 with std 1.4 — that tile is geometrically
**flat** (it is also the tile with lumStd 4.5). Flat far tiles exist in vanilla.

---

(lens sections appended below as they land)

---

## 1. OVERSEER'S OWN MEASUREMENTS (not delegated — I reproduced every one)

### 1.1 The `_msn` up-axis, settled

The brief guessed "up appears to be GREEN" from a single mean. That guess is
**correct**, but the reasoning offered for it was not sufficient, and my own
first attempt to confirm it was worthless — see REFUTED below.

The test that decides it (`msn_updecide.py`) uses two properties a terrain
normal must have and a horizontal component cannot:

* **Determinism.** The up component is not free: `up = +sqrt(1 - x^2 - y^2)`.
  Predict each channel from the other two and measure the residual. The true up
  channel has a small residual; a horizontal channel does not, because its sign
  is unrecoverable.
* **One-sidedness.** Terrain never faces downward, so the up channel must never
  encode a negative number — never below 128.

`python msn_updecide.py`, mip 2 (128x128), full decode:

    tile                          mean |predicted-actual| (bytes)  fraction of pixels < 128
                                     R      G      B                R      G      B
    Commonwealth.4.-20.24_msn       40.7    5.4   71.2            0.543  0.000  0.839
    Commonwealth.4.-20.60_msn       62.3    5.2   57.5            0.655  0.000  0.785
    Commonwealth.4.-60.60_msn       15.3    7.2   48.0            0.042  0.000  0.481
    Commonwealth.4.0.0_msn          38.8    5.6   42.5            0.615  0.000  0.696
    Commonwealth.4.-20.80_msn       14.0    1.0   13.4            0.779  0.000  0.820
    Commonwealth.8.-24.24_msn       64.1   10.7   61.0            0.618  0.000  0.597
    Commonwealth.16.-16.0_msn       56.5   15.8   64.0            0.398  0.000  0.676
    Commonwealth.32.-96.-96_msn     40.7    6.1   39.1            0.608  0.000  0.653

Green wins both tests on **every tile, at every LOD level**: residual 1.0-15.8
bytes against 13-71 for red and blue, and **exactly zero pixels below 128 in
green** while red and blue fall below 128 for 4%-84% of pixels.

**SETTLED: vanilla FO4 terrain LOD `_msn` is `R = X (east), G = UP, B = Y
(north)`, all three channels 0.5+0.5 encoded.** Not "z direct" — see REFUTED.
Note this also puts the up axis in the 6-bit channel of RGB565, the only one
with extra precision, which is what a careful encoder does.

### 1.2 The repo's own writer puts UP IN BLUE — a real, first-order mismatch

Traced through the code, not guessed:

* `src/lodgen.cpp:5021-5027` — `Vector3 nrm( -dzdx, -dzdy, 1.0f ); nrm.normalize();`
  so `nrm[0]` is east, `nrm[1]` is north, `nrm[2]` is **up**. Then
  `msn[...] = 0xFF000000U | quint32( nr << 16 ) | quint32( ng << 8 ) | quint32( nb );`
  with `nr` from `nrm[0]`, `ng` from `nrm[1]`, `nb` from `nrm[2]`.
* `src/lodgen.cpp:3591-3595` — `lodgenPack565( quint32 bgra )` reads
  `r = (bgra >> 16) & 0xFF`, `g = (bgra >> 8) & 0xFF`, `b = bgra & 0xFF` and
  packs `(r>>3)<<11 | (g>>2)<<5 | (b>>3)`, so **bits 16-23 land in RED5**.
  Corroborated at `src/lodgen.cpp:3621-3622`, where that same `>>16` field is
  the one multiplied by the 0.299 luma coefficient — red's.
* `src/lodgen.cpp:3812` routes every colour block through that packer and
  `src/lodgen.cpp:5285` writes the result as `<tile>_msn.DDS`.

So this fork emits **R = east, G = north, B = up**, where FO4 wants
**R = east, G = up, B = north**. Green and blue are transposed.

UNVERIFIED: whether xLODGen or DynDOLOD make the same transposition. That is
lens 4's question and I have not measured their output. **This finding is about
bungo's own fork** and must not be dressed up as the answer to his DynDOLOD
question unless lens 4 finds matching output.

### 1.3 What that swap costs, simulated on real vanilla data

`python swap_sim.py` — decode each vanilla `_msn`, recover the true surface
normal with the settled convention, re-encode it the way a swapping writer
would, and shade both with the same sun. Sun azimuth 225, elevation 45,
`light = 0.25 ambient + 0.75 * max(0, n.L)`:

    tile              CORRECT mean/std     SWAPPED mean/std     light      variation
    4.-20.24           0.830 / 0.111        0.250 / 0.005       -69.8%      -95.2%
    4.-20.60           0.837 / 0.135        0.251 / 0.009       -70.0%      -93.2%
    4.-60.60           0.562 / 0.175        0.251 / 0.007       -55.4%      -96.1%
    4.0.0              0.792 / 0.110        0.252 / 0.022       -68.2%      -79.7%
    4.-20.40           0.833 / 0.117        0.251 / 0.010       -69.9%      -91.6%
    16.-16.0           0.781 / 0.065        0.250 / 0.002       -68.0%      -96.4%
    32.-96.-96         0.794 / 0.041        0.250 / 0.005       -68.5%      -88.4%
    MEAN               0.776 / 0.108        0.251 / 0.009       -67.7%      -92.0%

The swapped result is **0.250-0.252 on every tile — the ambient constant to
three decimals**. The direct sun term is gone entirely. This is not sensitive to
my choice of sun angle: the mis-read normal points nearly horizontally north, so
`n.L` is at or below zero for essentially any sun above the horizon.

**That accounts for DARKER (-68% light) and FLATTER (-92% of shading variation)
directly, with numbers.** It accounts for DESATURATED only **indirectly, and I
label that step UNVERIFIED**: with the direct term gone the surface is lit by
ambient sky alone, which in FO4 is a cool blue-grey, so the warm light carrying
the scene's colour is removed and what is left tends toward one cool hue — which
is what bungo describes. I measured the collapse of the direct term. I did
**not** measure FO4's ambient colour or its fog curve, so the last step from
"ambient only" to "reads blue-grey" is reasoning, not measurement.

### 1.4 The vanilla diffuse is plain albedo — no sun, no AO baked in

This decides whether the `_msn` carries all the shading. `python baked_light.py`
pairs each texel of the diffuse with the same texel of the `_msn` (both 512x512,
same footprint) and hunts for a baked directional term:

    tile        alpha mean/min/max   corr(lum, up)   best n.L corr   at (az, el)
    4.-20.24      255.0 255 255         -0.374          -0.321        (75, 60)
    4.-20.60      255.0 255 255         -0.262          -0.281        (315, 60)
    4.-60.60      255.0 255 255         -0.631          -0.503        (180, 60)
    4.0.0         255.0 255 255         +0.067          -0.132        (315, 20)
    4.-20.40      255.0 255 255         -0.408          -0.332        (30, 60)
    16.-16.0      255.0 255 255         +0.071          +0.125        (330, 40)

**No sun is baked in.** The best-fitting direction is weak (|r| <= 0.50) and its
azimuth is incoherent across tiles — 75, 315, 180, 315, 30, 330 degrees. A baked
sun would agree on one azimuth across the whole worldspace. These do not.

The negative `corr(lum, up)` says steeper ground is *brighter*. That is a
material correlation, not a lighting one: cliff and rock textures are paler than
the dark vegetated flats. It is **not** baked ambient occlusion, which would
need concavity rather than slope and would darken the steep.

Also: **the diffuse's alpha is a constant 255 on every tile tested** — it
carries nothing. No mask, gloss or specular hides there, so a rebake that writes
opaque alpha loses nothing.

**Consequence: at distance essentially all terrain shading comes from the
`_msn`.** A correct diffuse cannot rescue a wrong normal map — which is exactly
why the corpus result in section 0 (far diffuse no brighter or more saturated
than near) is consistent with the mountains nonetheless looking completely
different.

---

## 2. LENS 2 — SOURCE. **This is the finding that answers bungo's question.**

The CLI (`--dump-land`) is refused by this sandbox, so I wrote my own read-only
ESM walker instead: `esmland.py`, built from the subrecord layouts in
`src/esmdata.cpp` (BTXT/ATXT/VTXT at :305-333, VHGT at :334, VCLR at :351).
It handles GRUP nesting, the 24-byte record header, zlib-compressed records
(flag `0x00040000`) and the `XXXX` oversize subrecord.

### 2.1 Every cell has height. Almost none has texture.

`python esmland.py` over `Fallout4.esm`, worldspace `0000003C`:

    worldspace 0000003C group found: True
    LAND records seen:   36864
    cells with a LAND:   36864
    cell x range -96..95   y range -96..95
    with VHGT 36864   with VCLR 2362   with BTXT 3517   with ATXT(layers) 3937

36864 = **192 x 192**, cells **-96..95** in both axes — *exactly* the extent of
the shipped LOD texture pyramid measured in section 0, and independently
corroborated by the heightmap filename found in 4.2
(`Commonwealth.HeightMap.-96.-96.95.95.-8316.44862.dds`).

So the source geometry is complete: **every one of the 36864 cells carries VHGT.**
But:

| subrecord | cells carrying it | share |
|---|---|---|
| VHGT (height) | 36864 | **100 %** |
| ATXT/VTXT (texture layers) | 3937 | 10.7 % |
| BTXT (base texture) | 3517 | 9.5 % |
| VCLR (hand-painted vertex colour) | 2362 | **6.4 %** |

`python landmap.py`:

    cells with ANY texture assignment: 3955 of 36864 (10.7%)
    their bounding box: x -36..32   y -41..32

The ASCII maps (in `landmap_out.txt`) show one compact, solid block and nothing
else anywhere. **The textured region is the playable Commonwealth and only the
playable Commonwealth.**

Sanctuary Hills sits near cell (-20, 24) — inside that box. **North is +Y in a
Bethesda worldspace, so the mountains bungo is looking at, north over Concord,
are at y > 32: outside the box, with no BTXT, no ATXT and no VTXT at all.**

### 2.2 Yet Bethesda shipped rich, distinct, *more saturated* LOD out there

`python landmap.py` classified all 2304 level-4 tiles by whether all 16 of their
cells are textured or none are, then decoded 40 random tiles of each class at
mip 3 (64x64):

    tiles fully inside the textured region:  214
    tiles fully outside:                    2023
    mixed:                                    67

    TEXTURED (playable)  n=40  lum 66.4+-15.6  meanSat 0.155+-0.031  lumStd 9.35+-1.97  msn relief 38.0+- 8.4
    UNTEXTURED (outer)   n=40  lum 71.0+-11.5  meanSat 0.191+-0.023  lumStd 7.37+-4.51  msn relief 23.4+-18.5

Read that carefully. In the outer region — where the ESM contains **no landscape
texture data whatsoever** — vanilla's shipped LOD is *brighter* (71.0 vs 66.4)
and **markedly more saturated (0.191 vs 0.155)** than in the playable area. It
still carries real tonal variation (lumStd 7.37) and real normal-map relief
(23.4). And the brief's item 3 already established the tiles are md5-distinct
per cell, so they are not one repeated fill.

**88 % of the worldspace's terrain LOD (2023 of 2304 level-4 tiles) was shipped
with colour that cannot be derived from the ESM, because the ESM has nothing
there to derive it from.**

### 2.3 Why that is the answer

Join 2.1 and 2.2 to xLODGen's own documentation, quoted in 4.3(a):

> "Default diffuse size - size of diffuse texture **in case there are no texture
> layers for the LOD level**, e.g the entire terrain LOD texture is **the default
> landscape texture**. This is to minimize terrain textures of **outer regions
> without landscape textures**."
> "Default normal size - ... **This is to minimize terrain texture of outer
> regions were no terrain textures have been defined.**"

The tool is describing exactly the region measured in 2.1. Out there a
regenerator has nothing to sample, so it emits **one flat default landscape
texture** — optionally shrunk to a few texels — for a region where Bethesda
shipped the most saturated, per-cell-distinct terrain in the game. That is:

* **DESATURATED** — measured directly. The outer region is where vanilla's
  saturation is *highest* (0.191); one default texture replaces it with a single
  hue. This is the symptom the diffuse explains best, and it is the one a
  normal-map theory does **not** explain.
* **FLATTER** — vanilla's outer lumStd 7.37 and msn relief 23.4 both go to
  roughly zero when the tile becomes one flat colour, and the "Default normal
  size" clause shrinks the normal map too — which section 1.4 proved is where
  *all* the shading at distance comes from.
* **DARKER** — only if the default landscape texture is darker than what
  Bethesda baked. **I could not test this**: see UNVERIFIED. The measured
  outer-region vanilla mean is lum 71.0, so the default would have to come in
  below that.

The 54 distinct BTXT base textures counted (`000AB72E` on 879 cells, `00021336`
on 732, `0014BF47` on 536, ...) are all inside the playable box. **There is no
"far mountain base texture" to look up** — that was one of the brief's
hypotheses and 2.1 kills it.

### 2.4 The "not reproducible from landscape textures" claim — MEASURED, and stronger than claimed

The brief asked me to test the widely-repeated claim that Bethesda's Commonwealth
terrain LOD textures are not reproducible by sampling the landscape diffuse
textures alone. For the outer region the claim is true in the strongest possible
form: **there are no landscape textures assigned to sample.** No weighted mean
can be computed because the weights do not exist. Bethesda's outer LOD was
authored by some other route that does not ship in `Fallout4.esm`.

---

## 3. LENS 3 — SHADING

*(Done personally; the lens agent was killed before it reported.)*

Tool: `nifpeek.py` (from the IDENTITY lane, read-only) plus `btrsurvey.py`.
`nifpeek` self-checks — it prints `datasize=N(calc N)` per shape and
`size=N (walk ends N)` per file; both agree on every BTR opened here, so the
block walk and the vertex-descriptor decode are byte-exact.

### 3.1 Vanilla terrain LOD carries NO vertex colours — measured

`python btrsurvey.py`, 100 BTRs sampled across all four levels:

    vertex attribute sets seen: {'VERTEX+UVs': 100, 'VERTEX': 79}
    shader flag pairs seen    : {('0x80401000','0x00000003'): 100,
                                 ('0x80000000','0x00000001'):  79}

Every file has a `BSTriShape` named `Land` with `desc=0x300000000203`,
attributes **VERTEX + UVs and nothing else**. `COLORS` (vertex-descriptor bit
0x0020) is **never** present, on any tile, at any level, inside or outside the
playable region. The second shape (79 of 100 files) has attributes `VERTEX`
only and a `BSEffectShaderProperty` — it is the water plane, not terrain.

**So the "terrain LOD vertex colour multiplies into the picture" premise in the
brief is false for FO4.** There is no vertex colour on these meshes to change.
Whatever VCLR contributes is baked into the *texture* — which is exactly what
xLODGen's readme says its slider does: "Vertex Color Intensity ... controls how
strong the vertex color overlay is painted **on top of the terrain LOD
textures**."

### 3.2 Shader flags — and the fork matches vanilla exactly

Decoded against the repo's own `build/nif.xml`,
`Fallout4ShaderPropertyFlags1` at line 7000 and `...Flags2` at line 7036:

`flags1 = 0x80401000` — bits 12, 22, 31:

| bit | name | note |
|---|---|---|
| 12 | **F4SF1_Model_Space_Normals** | confirms the `_msn` is model-space, as section 1.1 measured |
| 22 | F4SF1_Own_Emit | |
| 31 | F4SF1_ZBuffer_Test | |

`flags2 = 0x00000003` — bits 0, 1:

| bit | name |
|---|---|
| 0 | F4SF2_ZBuffer_Write |
| 1 | **F4SF2_LOD_Landscape** |

Bit 5, `F4SF2_Vertex_Colors`, is **clear** — consistent with 3.1.

Now compare with what this fork writes, `src/lodgen.cpp:59-60`:

    LAND_SHADER_FLAGS1 = 2151682048  = 0x80401000   -> IDENTICAL to vanilla
    LAND_SHADER_FLAGS2 = 3           = 0x00000003   -> IDENTICAL to vanilla

**The fork's shader flags are byte-for-byte vanilla.** Its only terrain-normal
defect is the channel order found in section 1.2. Worth stating plainly so that
finding is not over-read.

### 3.3 At distance the mesh carries nothing — the `_msn` carries everything

Triangle counts, 25 tiles sampled per level:

    level    mean tris  mean verts   tris per CELL
      4        1021        560          63.84
      8        1096        711          17.13
     16        1524       1956           5.95
     32        2932       3608           2.86

A level-32 tile spends **2.86 triangles per cell** — a cell is 4096 game units
across. At that density the interpolated mesh normal describes essentially
nothing, so all surface shading must come from the `_msn` texture. This is the
mesh-side confirmation of section 1.4's texture-side result, reached
independently. **A bad or absent `_msn` is catastrophic at distance and nearly
invisible up close** — which matches bungo's screenshots, where near ground is
fine and the far range is a silhouette.

### 3.4 Bethesda's own outer meshes are simpler, but they are all there

    fully textured tiles with a BTR: 214 ; fully untextured tiles with a BTR: 2023
    TEXTURED     n=30  mean tris 2087  mean verts 1437
    UNTEXTURED   n=30  mean tris  891  mean verts  493

Outer tiles get ~43% of the triangles of playable ones. But the mesh pyramid is
**complete and gapless at every level**, exactly like the textures:

    BTR files: 3060
    per level: {4: 2304, 8: 576, 16: 144, 32: 36}
      level  4: 2304 tiles, x -96..92  y -96..92, full grid would be 2304
      level  8:  576 tiles, x -96..88  y -96..88, full grid would be  576
      level 16:  144 tiles, x -96..80  y -96..80, full grid would be  144
      level 32:   36 tiles, x -96..64  y -96..64, full grid would be   36

So the "far cells only exist at coarse levels" hypothesis is **dead for both
meshes and textures**.

### 3.5 The `noise.dds` darkening story does not hold for FO4 — measured

xLODGen's readme blames "darkening caused by `textures\terrain\noise.dds`".
FO4's copy is at `E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Noise.dds`
(there is also a `DetailNormals.dds`):

    Noise.dds  1024x1024  BC1_UNORM  mips 11
    mean rgb 241.1 236.7 237.2   lum 237.7   lumStd 38.2

Mean level 237.7/255 = **0.932**. A multiply by this costs about **7 %**
brightness, uniformly and achromatically. That cannot produce the change bungo
is describing, and it cannot desaturate anything. **Candidate rejected for FO4**
— see REFUTED. (The readme's sentence is aimed mainly at Skyrim SE, whose
`noise.dds` is a different texture.)

### 3.6 Atmospheric perspective — honestly, not decidable from files here

Shot one is hazier and warmer, shot two crisper and darker. FO4 fades distant
terrain toward a fog colour, so a rebake that changed the mesh bounds or the LOD
level assignment would move the same mountains along the fog curve. I measured
that the **mesh pyramid and the texture pyramid are both complete and
identical in extent** (3.4, section 0), so a *vanilla* install has no level
assignment to get wrong. Beyond that: the fog colour lives in WTHR weather
records and the fade distances in INI settings, and I have not read either, nor
can I compare against a rebake that does not exist on this machine.
**UNVERIFIABLE from files alone in this lane** — stated rather than
manufactured. Note also that the load order includes `F76Weathers.esp` and
`UltraExteriorLighting.esp` (from the TexGen log), so his weather is modded;
if the two screenshots were taken under different weather or time of day, some
of the difference is not LOD at all.

---

## 4. LENS 4 — TOOLS

*(Done personally: all four lens agents were killed mid-flight by the session limit.)*

### 4.1 What is actually installed on this machine — MEASURED

| thing | path | evidence |
|---|---|---|
| xLODGen 3.0.22.0 | `E:\Tools\xLODGen\` | `xLODGen.exe`, `xLODGenx64.exe`, `Edit Scripts\LODGen.exe`; version string in `LODGen_log.txt` header |
| DynDOLOD 3.0 Alpha-210 | `E:\Tools\DynDOLOD\` | `DynDOLODx64.exe`, `TexGenx64.exe` 3.0.0.210 |
| MO2 2.5.2, **Fallout 4 instance** | `C:\Users\bungo\AppData\Local\ModOrganizer\Fallout 4`, mods at `E:\Projects\Fallout 4 Mods` | `ModOrganizer.ini`: `gameName=Fallout 4`, `base_directory=E:/Projects/Fallout 4 Mods` |
| TexGen + DynDOLOD wired into that instance | same ini, tool entries 11 and 12 | `11\binary=E:/Tools/DynDOLOD/TexGenx64.exe`, `12\binary=E:/Tools/DynDOLOD/DynDOLODx64.exe` |
| a pre-DynDOLOD MO2 ini backup | `ModOrganizer.ini.bak-20260827-pre-dyndolod` | filename |

Game is **Fallout 4 1.11.221** (from the TexGen log's `Game: ... Version: 1.11.221`).

### 4.2 Did any of it ever produce FO4 terrain LOD? NO — measured

* `E:\Tools\DynDOLOD\Logs\TexGen_FO4_log.txt`, session **2026-08-29 00:04:23**,
  TexGen 3.0 Alpha-210 for Fallout4. It loaded the whole load order and then
  ended on `[00:00] Exit TexGen, check log or restart?` — it **exited without
  generating anything**. `E:\Tools\DynDOLOD\TexGen_Output\` is **empty**.
* `E:\Tools\xLODGen\LODGen_log.txt` (1.79 MB) contains only **FNV/FO3** runs —
  `Game Mode: TerrainFO3`, `Game Mode: FO3`, worldspaces `WastelandNV`,
  `adwDryWellsReloaded`, `DCworld*`, `DLC01*`, `DLC02*`. There is **no FO4
  terrain LOD run in any log on this machine.** Its output path was
  `C:\Output\`, which **no longer exists**.
* An FO4 *object* LOD run did happen at some point — `E:\Tools\xLODGen\Edit
  Scripts\FO4-AtlasMap-Commonwealth.txt` (18838 bytes) and
  `FO4-AtlasMap-SanctuaryHillsWorld.txt` exist — but object LOD is not terrain
  LOD and no terrain artefacts accompany them.
* `python scan_lod.py` walked `E:\Projects\Fallout 4 Mods` (the whole FO4 mod
  stack, ~90 mods, including `overwrite`), `E:\Tools\DynDOLOD`,
  `E:\Tools\xLODGen`, and the repo's `tests`, `scratchpad`, `scratch_water`,
  `heightmaps`, matching `<ws>.<4|8|16|32|64|128>.<x>.<y>[_msn].(dds|btr|bto)`:

        SCANNED  E:\Projects\Fallout 4 Mods                   0 LOD-named files
        SCANNED  E:\Tools\DynDOLOD                            0 LOD-named files
        SCANNED  E:\Tools\xLODGen                             0 LOD-named files
        SCANNED  ...\NifskopeWildWastelandEdition\tests       0 LOD-named files
        SCANNED  ...\scratchpad / scratch_water / heightmaps  0 LOD-named files
        === directories holding LOD-named files (0) ===

  The only `Textures\Terrain\<worldspace>` trees found hold **heightmaps**, not
  LOD tiles — e.g. `...\heightmaps\Textures\Terrain\Commonwealth\
  Commonwealth.HeightMap.-96.-96.95.95.-8316.44862.dds` (33.5 MB). That
  filename independently corroborates section 0's cell extent: **-96..95**.
* `X:\...\Fallout 4\Data` has **no loose `Textures\` or `Meshes\` directory** at
  all, so nothing was installed over the game either.

> **THEREFORE: the decisive comparison this brief asked for — measure a real
> rebake tile against vanilla — CANNOT BE MADE ON THIS MACHINE.** No rebaked
> FO4 terrain LOD exists here. Everything below about what xLODGen/DynDOLOD do
> is from their shipped documentation or is reasoning, and is labelled as such.
> **Section 1.2's green/blue transposition is a finding about bungo's own fork
> and is NOT evidence about xLODGen.**

### 4.3 What xLODGen's OWN documentation says — MEASURED (it is on this disk)

Source: `E:\Tools\xLODGen\Terrain-LOD-Readme.txt`. These are quotations, not
recollection.

**(a) The knob that targets exactly the region bungo is asking about.**

> "**Default diffuse size** - size of diffuse texture in case there are no
> texture layers for the LOD level, e.g the entire terrain LOD texture is the
> default landscape texture. **This is to minimize terrain textures of outer
> regions without landscape textures.** Setting None means no change from the
> Diffuse Size for the LOD level. Applies to all LOD levels."

> "**Default normal size** - size of normal texture in case there is no normal
> data for the LOD level. **This is to minimize terrain texture of outer regions
> were no terrain textures have been defined.** Setting None means no change
> from the Diffuse Size for the LOD level. Applies to all LOD levels."

This is a documented, region-specific down-sizing of **both** the diffuse and
the normal map for "outer regions" — the mountains outside the playable area,
by name. A tile reduced to a handful of texels is:

* **flatter** — no tonal variation left to have;
* **desaturated** — averaging many hues collapses toward grey;
* **darker** iff the default landscape texture is darker than the layer mix
  Bethesda baked (untested — see UNVERIFIED);

and, because it shrinks the **normal** map too, it removes precisely the thing
section 1.4 proved carries *all* the shading at distance. **This is my leading
candidate for the answer to the question bungo actually asked**, and unlike the
transposition it is region-specific by construction, which matches his
screenshots.

**(b) The tool documents its own output as darker than you want.**

> "**Diffuse Brightness, Contrast, Gamma** - modify intensity levels. **Might be
> required to counter darkening caused by textures\terrain\noise.dds** or the
> broken 'improved' snow shader of Skyrim SE. It would be better to adjust the
> average levels (best values seem to be different depending on game) of the
> used noise.dds texture instead..."

A documented darkening mechanism with a documented compensating slider. Note the
sentence names Skyrim SE for the snow half; whether FO4's terrain shader applies
`noise.dds` identically is **UNVERIFIED** here.

**(c) A direct knob on the VCLR multiply.**

> "**Vertex Color Intensity** - 1.00 = 100%, controls how 'strong' the vertex
> color overlay is painted on top of the terrain LOD textures."

Given VCLR's neutral is 255 and it multiplies in, this slider sits exactly where
a brightness regression would live.

**(d) The stated "native" normal size is HALF what Bethesda shipped.**

> "**Normal Size** - ... see the hint message for the size that is native to the
> data (**typically 256x256 for LOD4, 512x512 for LOD8** etc.)"

Measured, section 0: **vanilla FO4 ships 512x512 for LOD4** (and for LOD8, 16 and
32 — every terrain LOD texture in `Textures\Terrain\Commonwealth` is 512x512 BC3
with 10 mips, 349680 bytes). So the readme's "native for LOD4" is half vanilla's
resolution per axis, a quarter of the texels. A user who accepts the native hint
gets a coarser normal map than Bethesda shipped, hence less relief, hence
flatter. **Caveat, and it matters:** that sentence is generic across the nine
games xLODGen supports and may be quoting Skyrim's numbers; that it applies to
FO4 is **UNVERIFIED**. The vanilla 512x512 measurement is solid.

**(e) Other relevant controls, quoted.**

* "**Normal Rise steepness** - increase steepness for each mipmap level."
* "**Normal Bake normal-maps** - bake landscape normal map textures onto terrain
  normal. Use with higher normal resolutions for LOD4, 1024x1024 for example."
* Meshes: "**Quality**: higher settings equal less quality, less terrain detail."
  and "**Max Vertices**: can be used to limit max files size".
* "**Optimize Unseen**", "**Protect Cell Borders**", "**Hide Quads**".

**(f) A skip trap that silently mixes vanilla and rebaked tiles.**

> "If terrain LOD textures already exist in the output folder, their generation
> will be skipped. ... **If the diffuse or normal texture for a higher LOD level
> already exists, then all lower LOD levels that are covered by the higher LOD
> level are skipped as well.** So in order to re-generate a LOD level 4 texture,
> it is not enough to just delete the LOD level 4 files ... the higher LOD level
> diffuse and normal textures files need to be deleted as well."

A partial rebake therefore leaves a mixture, which is a plausible source of
"some of it changed and some did not".

**(g) The readme says NOTHING about normal-map channel order.** I have not
measured what xLODGen writes into `_msn`. See UNVERIFIED for the exact procedure
that would settle it in about ten minutes.

### 4.4 The whole-machine hunt, for completeness

`python hunt.py` walked `E:\`, `X:\`, `C:\Users\bungo`, `C:\Program Files`,
`C:\Program Files (x86)`, `C:\ProgramData` — **50004 directories in 900 s** —
looking for any directory holding `<ws>.<level>.<x>.<y>[_msn].(dds|btr|bto)`
outside `E:\Tools\Fallout 4\DataUnpacked`:

    === C. directories holding GENERATED-LOOKING terrain LOD, outside the vanilla unpack (0) ===

**Honest caveat:** that walk hit its 900 s budget and is therefore *not*
exhaustive. The targeted scan in 4.2, which did run to completion over the
actual FO4 mod stack and both tool trees, is the stronger negative.

---

## 5. REFUTED — claims that were made in this lane and killed

**R1. My own first `_msn` test was worthless.** `msn_convention.py` tried to
pick the encoding by which decoding produced unit-length vectors. Four of its
six hypotheses (H1, H2, H4, H5) are **permutations of the same three
components** and therefore have identical magnitude by construction — the test
could never separate them, and it printed "<== UNIT" against several at once.
Killed and replaced by `msn_updecide.py` (section 1.1), which uses determinism
and one-sidedness instead. **Lesson kept: a test that cannot distinguish its
hypotheses is not evidence, however confident its output looks.**

**R2. "Vanilla's far LOD textures are brighter/more colourful than the playable
ones."** Not the mechanism. Full mip-0 decodes (section 0) and the 40-vs-40
tile comparison (section 2.2) show far tiles at lum 71.0 vs 66.4 playable —
*slightly* brighter, and notably more saturated (0.191 vs 0.155), but nothing
like the difference in the screenshots. The shipped diffuse is not where the
vanilla-vs-rebake gap lives; **the gap is that a rebake cannot produce that
diffuse at all** (section 2).

**R3. "Terrain LOD vertex colour multiplies into the picture, so a change there
is a direct brightness change."** False for FO4. Measured in section 3.1:
**no** vanilla Commonwealth `.BTR` at any level carries a `COLORS` vertex
attribute, and `F4SF2_Vertex_Colors` is clear in the shader flags on all 100
sampled. There is no mesh vertex colour to change.

**R4. "Far cells only have coarse LOD (16/32), which is why they look flat."**
Dead for both textures and meshes. Section 0 and section 3.4: all four levels
are complete, gapless squares over the identical extent — textures
2304/576/144/36, meshes 2304/576/144/36.

**R5. "The darkening is `textures\terrain\noise.dds`."** Rejected for FO4.
Measured in section 3.5: FO4's `Noise.dds` has mean lum **237.7** (0.932 of
white) with `lumStd 38.2`. A multiply by it costs about 7 %, achromatically.
It cannot desaturate and it cannot flatten.

**R6. "The worldspace supplies a default land texture that the outer region
falls back to."** I raised this myself as the strongest challenge to my own
section 2, and killed it. `wrlddump.py` dumps every subrecord of WRLD
`0000003C`: there is **no LTEX reference anywhere on the record**. `DNAM` is
`(0.0, 450.0)` — two *floats*, default land height and default water height,
not formids. (My first attempt, `defaulttex.py`, misread `DNAM` as two formids
and reported a "default water texture 43E10000"; `43E10000` is the float 450.0.
Corrected.) So the outer region has no texture assignment **and** no worldspace
default to inherit — which makes section 2's conclusion stronger, not weaker.

**R7. The overseer's leading hypothesis — that a tool writing up-in-blue is why
bungo's mountains went dark — is NOT established, and the evidence points
elsewhere for *his* question.** The mechanism is real and I quantified it
(section 1.3: -68 % light, -92 % shading variation), and the fork really does
write up in blue (section 1.2). But:
* it is measured **only for this repo's generator**, never for xLODGen or
  DynDOLOD, and no rebake output exists on this machine to test (section 4.2);
* a channel transposition would break **all** rebaked terrain equally, near and
  far. bungo reports a change confined to distant terrain outside the playable
  area. The missing-source-data explanation is region-specific *by
  construction* and matches that;
* **fairness caveat, stated because it weakens my own argument:** ground within
  the loaded-cell radius is full-resolution landscape, not LOD, so his
  screenshots do not cleanly test mid-distance terrain LOD *inside* the playable
  area. If that region is also darker in shot two, R7's reasoning weakens and
  the normal-map theory rises. **That is the single cheapest thing he can check
  and it is item 1 of section "WHAT TO CHECK".**

**R8. My own tooling had a real bug, found by the lens-2 agent before it died:**
`dds.py`'s BC4/BC3 alpha interpolator used weights `(7-i):(1+i)` where the
format specifies `(6-i):(1+i)`, which can also exceed 255. Fixed, palettes
re-verified in range and monotone, and every alpha claim re-measured afterwards
(`alpha_recheck.py`): diffuse and `_msn` alpha are a constant 255, min and max,
on all six tiles retested. **RGB was never affected**, so no colour number in
this report moves.

---

## 6. UNVERIFIED — what I could not check here, and exactly what would settle it

**U1. What xLODGen and DynDOLOD actually write into `_msn` — which channel is
up.** *This is the one measurement that would answer bungo's literal question
and I could not make it.* No rebaked FO4 terrain LOD exists anywhere on this
machine (section 4.2, and a 50004-directory sweep in 4.4). **To settle it:** run
xLODGen (`E:\Tools\xLODGen\xLODGenx64.exe -fo4 -o:"C:\LODTest\"`), Terrain LOD,
worldspace Commonwealth, Chunk = level 4 at W/S of one *outer* tile and one
*playable* tile, then
`python msn_updecide.py "C:\LODTest\Textures\Terrain\Commonwealth\Commonwealth.4.<x>.<y>_msn.DDS"`.
Green residual small and zero pixels below 128 -> xLODGen is correct. Otherwise
the transposition is real in xLODGen too and R7 flips.

**U2. What colour xLODGen substitutes for an untextured cell.** Its readme says
"the entire terrain LOD texture is the default landscape texture", but I proved
in R6 that Commonwealth defines no default land texture, so the substitute is
whatever is hardcoded in the tool. **This is the missing half of the DARKER
symptom.** Measurable by the same test run as U1: decode the generated outer
tile and compare its lum against vanilla's **71.0** and its meanSat against
**0.191**.

**U3. What xLODGen's "Default diffuse size" / "Default normal size" default
to.** The readme documents the controls, not their defaults, and the tool's UI
cannot be opened in this read-only lane. If "None" is the default, the outer
tiles are not shrunk and only the flat-default-colour half of section 2.3
applies.

**U4. Whether FO4's terrain shader applies `Noise.dds` at all.** I measured the
texture (3.5) but not its use. `Fallout4 - Shaders.ba2` is on disk and would
settle it.

**U5. Fog, weather and LOD fade.** Section 3.6. Fog colour lives in WTHR records
and fade distances in the INI; I read neither. Note his load order contains
`F76Weathers.esp` and `UltraExteriorLighting.esp`, so **if the two screenshots
were taken under different weather or time of day, part of the difference is not
LOD at all.** Worth ruling out before anything else is changed.

**U6. How Bethesda actually authored the outer-region LOD.** Section 2.4 proves
it is not derivable from `Fallout4.esm`'s LAND layers, because there are none.
By what route they made it — hand painting, a source landscape that was later
stripped, an internal tool — is not answerable from shipped files.

**U7. The repo CLI.** `./release/NifSkope.exe -no-gui lodgen ... --dump-land`
is refused by this sandbox, so lens 2 was done with my own ESM walker instead.
The walker self-checks (36864 LAND records = exactly 192x192, every one with
VHGT, extent matching both the LOD pyramid and the worldspace `NAM0`/`NAM9`
bounds of +-393216 units = +-96 cells), but an independent `--dump-land` run by
the overseer would be a free confirmation.

---

## 7. LENS 1 — CORPUS (done personally; the lens agent was killed before it reported)

### 7.1 Where Sanctuary actually is — measured, not inherited from the brief

The brief asserted that `Commonwealth.4.-20.24` is "near Sanctuary". I checked
rather than assumed. `python findcell.py` scans every Commonwealth exterior CELL
for an EDID and prints its `XCLC` grid coordinates:

    Commonwealth exterior CELLs: 36865 (with an EDID: 755)
       Vault111Ext             cell ( -22,  22)
       SanctuaryExt10          cell ( -21,  20)
       SanctuaryExt03          cell ( -21,  21)
       SanctuaryExt07          cell ( -21,  22)
       SanctuaryExt04          cell ( -20,  20)
       SanctuaryExt            cell ( -20,  21)
       SanctuaryExt02          cell ( -20,  22)
       SanctuaryExt06          cell ( -19,  21)
       SanctuaryExt05          cell ( -19,  22)
       RedRocketExt            cell ( -17,  19)
       ConcordExt              cell ( -15,  17)
       ConcordMuseumExt        cell ( -14,  17)

**Sanctuary Hills is cells (-21..-18, 20..22).** (36865 cells against 36864 LAND
records: the extra one is the worldspace's persistent cell, which carries no
`XCLC` and no LAND — a clean account of the difference, not a parser gap.)

Against the textured box measured in section 2.1 (x -36..32, y -41..32), the
distance from Sanctuary to the edge of the region that has *any* landscape
texture is:

| direction from Sanctuary (-20, 21) | edge | cells away | approx. distance |
|---|---|---|---|
| **north (+Y)** | y = 32 | **11** | ~45 000 units |
| west  (-X) | x = -36 | 16 | ~66 000 units |
| east  (+X) | x = 32 | 52 | ~213 000 units |
| south (-Y) | y = -41 | 62 | ~254 000 units |

**North is the nearest edge by a wide margin.** Standing in Sanctuary and
looking north, the untextured region starts about 11 cells out and then runs a
further 63 cells to the map edge at y = 95. That is exactly the band of distant
mountains in bungo's screenshots, and it is measured, not assumed.

(Aside: Concord is at y 16-18, *south* of Sanctuary at y 20-22, so "looking
north over the Concord water tower" cannot be literally north-over-Concord.
Whichever of the two the shot is, the conclusion is unchanged — every direction
from Sanctuary reaches untextured terrain, and north reaches it soonest.)

### 7.2 The outer tiles are genuinely per-cell content, verified on the outer region itself

The brief's item 3 checked md5 distinctness on nine tiles, mixed inside and
outside. I re-ran it on **120 random level-4 tiles drawn only from the fully
untextured outer region** (classified by the ESM walk, not by eye):

    outer level-4 tiles sampled: 120
    distinct diffuse md5: 120
    distinct _msn    md5: 120

**120 of 120 distinct, in both the diffuse and the normal map.** Nothing out
there is a repeated default fill. Bethesda authored real, individual terrain LOD
for a region whose source data contains no landscape textures at all — which is
the fact the whole answer turns on.

### 7.3 Full-corpus level-4 sweep — LANDED, and it confirms section 2.2

`python corpus.py`. First it validates the coarse mip rather than assuming it,
against a full mip-0 decode on 20 random level-4 tiles:

    === mip 4 validated against mip 0 on 20 random level-4 tiles ===
       delta lum      mean -0.05   max |0.24|
       delta meanSat  mean -0.0051 max |0.0249|

**Tolerance: lum within 0.25, meanSat within 0.025.** Every number below carries
that.

Then all **2304** level-4 diffuse tiles and all 2304 `_msn` tiles were decoded
and each tile tagged with how many of its 16 cells the ESM walk found textured.
Full per-tile table in `level4.csv`; four 48x48 ASCII maps (luminance,
saturation, `_msn` relief, textured-cell count) in `corpus_out.txt`.

    === whole corpus, split by the ESM texture boundary ===
    group                       tiles          lum         meanSat        lumStd      msn relief
    TEXTURED (all 16 cells)       214   66.19+-14.92  0.1595+-0.0328   8.98+-2.17    33.5+- 7.9
    MIXED                          67   63.14+-14.13  0.1798+-0.0426   8.28+-2.07    36.5+- 9.5
    UNTEXTURED (0 cells)         2023   71.06+-11.01  0.1885+-0.0249   5.85+-3.79    18.4+-19.2

Against section 2.2's 40-vs-40 sample at mip 3 (textured lum 66.4 / sat 0.155,
untextured lum 71.0 / sat 0.191) the full corpus agrees to **0.2 lum and 0.005
saturation**. `lumStd` and `msn relief` come out lower here purely because mip 4
is one halving blurrier than mip 3; the *ratio* between the groups is what
matters and it is preserved.

Three things the full sweep adds:

1. **The boundary is real and visible in the maps.** The luminance map shows one
   compact block of high-contrast, structured terrain (characters `#*+=`) in the
   upper-left-of-centre, surrounded on all sides by a uniform `:-` field. The
   textured-cell map (map D) has the same footprint. They are the same region.
2. **The outer region is genuinely flatter even in vanilla** — `lumStd` 5.85 vs
   8.98, and `msn relief` 18.4 vs 33.5. So Bethesda's own far LOD is lower
   contrast than the playable area. But note the standard deviations: relief
   `18.4 +- 19.2` means the outer region is *wildly* varied — some outer tiles
   are genuinely flat plains and some are full mountains. A rebake that replaces
   all of it with one value destroys that variety, which is exactly the "flat
   silhouette" complaint.
3. **The outer region really is the more saturated half of the worldspace**
   (0.1885 vs 0.1595), now on 2023 tiles rather than 40. The saturation map also
   shows a distinct high-chroma band well outside the textured box on the
   eastern side — content that exists only in the shipped LOD textures and
   nowhere in the ESM.
