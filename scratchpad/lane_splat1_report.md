# Lane SPLAT1 -- the speckle in our far terrain: which candidate, with the number

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, working tree.
Lane directory `scratchpad/splat1_20260911/`. Nothing committed. PHASE A is
read-only: no exe, no build, no rebake, nothing under `src/`, `res/`, `tests/`,
`tools/` or `NifSkope.pro` touched.

**bungo's question, 2026-09-11 (relayed to this lane at 15:5x by `date`; the brief's "16:5x" was a guess), verbatim:** *"the terrain textures here
for the terrain bakes do use their correct scale, right? So they'd be very tiny
repeating pixel sized patterns"*

**THE ANSWER: no, they do not, and he is right about what the correct scale
would look like.** The bake stretches every landscape texture to **2,048 world
units a repeat**. The engine's own number is **341.333** -- exactly **6x**
smaller. At the engine's scale one repeat is 10.7 texels of the far sheet,
which is the "tiny repeating pixel sized pattern" he described, and a
footprint-matched tap over it lands near the texture's own mean, which is
smooth. At 2,048 one repeat is 64 texels, the footprint-matched mip is a
64x64 image of the ground texture, and the bake reproduces that image's
blotches at full contrast. That is the speckle.

---

## 0. Method, and the controls that had to pass first

### 0.1 What is measured, and on what

| | |
|---|---|
| tiles | `Commonwealth.4.-20.24` (TERRAIN-R's) and `Commonwealth.4.-20.20` (ROADS1's), both dim 4, 512 texels, **32 world units a texel** |
| vanilla | `E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth\Commonwealth.4.-20.{24,20}.DDS`, 349,680 B, DXT5, 10 mips |
| ours (-20,24) | `scratchpad/terrain_r_20260911/out/stock_new2/tex/Commonwealth.4.-20.24.DDS`, 174,888 B, DXT1, 8 mips |
| ours (-20,20) | `scratchpad/roads1_20260911/out/after/tex/Commonwealth.4.-20.20.DDS`, same format |
| rebakes | **none.** Both bakes were already on disk from those lanes. |

Two instruments, in `scratchpad/splat1_20260911/splatlib.py`:

* **local variance** -- the per-texel variance of luminance inside a 3x3
  window, reported as the mean over the sheet, in squared 8-bit units. This is
  the number the eye reads as "speckle".
* **the radially averaged power spectrum** of the mean-removed, Hann-windowed
  luminance, normalised so the bins sum to the windowed field's variance, then
  collapsed into six scale bands. The spectrum is what separates "the codec"
  (one scale) from "the content" (broadband) from a planted period.

### 0.2 The controls, run BEFORE any verdict (`s0_selftest.py`, 12/12)

```
PASS  D1 decoder == tests/spells decoder, vanilla DXT5     maxabs 0.0000 of 255, 512x512 DXT5, mips 10
PASS  D1 decoder == tests/spells decoder, ours DXT1        maxabs 0.0000 of 255, 512x512 DXT1, mips 8
PASS  D1 decoder == tests/spells decoder, vanilla msn      maxabs 0.0000 of 255, 512x512 DXT5, mips 10
PASS  D2a local_var(constant) == 0                         mean local var 0.000e+00
PASS  D2b local_var(white noise sigma=10) == 88.9          mean local var 88.59, theory 88.89
PASS  D2c metric separates smooth from 6-texel checker     smooth 4.895 -> speckled 124.719 = 25.5x
PASS  D3 sum(radial power) == var(windowed field)          sum 46.18077 vs var 46.18071
PASS  D4 spectrum finds the planted 6-texel period         bin power 6.8740 vs local median 0.0000
PASS  D5a phase twin keeps the variance                    subject var 544.00, twin var 544.00
PASS  D5b phase twin keeps the local variance too          subject 124.72, twin 124.72
PASS  D6a BC1 emulation reproduces an already-BC field     re-encode of vanilla rms 0.27/255
PASS  D6b codec floor measured on a never-coded smooth field smooth 2.493 -> after BC1 4.009
      CODEC FLOOR (local var a BC1 codec adds to smooth data) = 1.516
```

Read D5b: **a phase-randomised twin has the same local variance as its
subject.** Local variance alone therefore cannot tell structure from noise, and
every structural claim below rests on the spectrum or on a correlation, never
on the variance alone. That is the twin doing its job.

D6b is the **codec floor**: a BC1 block codec adds **1.52** units of local
variance to data that has none of its own. Every excess below is quoted against
it.

The **known-answer pair** is a band-limited smooth field (local variance 4.9)
and the same field with a 6-texel checker on it (124.7): the metric separates
smooth from speckled by 25.5x, and the spectrum finds the planted period where
the checker's own geometry puts it.

### 0.3 A control that was WRONG first, and was fixed

The first run of D4 failed. A checker of period *p* has its fundamental at
`(1/p, 1/p)` -- radius `sqrt(2)/p` in a radially averaged spectrum, not `1/p`.
Looking for the peak at the wrong radius reported a working instrument as
broken. `splatlib.checker_radius()` now states the radius and the self-test
uses it. (Recorded in `MISTAKES_ENTRIES.md`.)

---

## 1. Spectra and variances -- ours vs vanilla vs twin vs known-answer

`s1_spectra.py`, log `logs/s1.log`. Percentages are each sheet's share of its
own spectral power, by scale.

```
BANDS, left to right:  >=128tx (>=4096u) | 32..128tx | 8..32tx | 4..8tx | 2..4tx | <=2tx

=== chunk (-20,24) ===  whole-tile mean |RGB| difference ours vs vanilla: 16.33 of 255
                                   lv       sd     var
VANILLA shipped sheet           19.81     4.45   89.82   11.1%  37.1%  20.7%   8.5%  16.3%   6.3%
OURS    baked sheet             76.22     8.73  327.49   24.0%  26.1%  20.1%  11.6%  15.6%   2.6%
  control: vanilla phase twin   19.98     4.47   89.82    7.0%  47.1%  20.7%   7.4%  12.9%   4.8%
  known-answer: smooth field     1.15     1.07   89.82    4.3%  95.5%   0.2%   0.0%   0.0%   0.0%
  known-answer: + 6-tx checker 120.98    11.00  233.83    1.6%  35.2%   0.1%  49.8%   0.0%  13.2%
    EXCESS local variance ours over vanilla: 76.22 - 19.81 = 56.41   (codec floor 1.52)

=== chunk (-20,20) ===  whole-tile mean |RGB| difference ours vs vanilla: 19.51 of 255
VANILLA shipped sheet           29.39     5.42  141.59   21.3%  29.2%  20.0%   7.9%  15.3%   6.2%
OURS    baked sheet             75.26     8.68  416.51    5.0%  45.8%  32.0%   8.2%   7.6%   1.4%
  control: vanilla phase twin   29.98     5.48  141.59   13.7%  41.9%  18.5%   7.1%  13.3%   5.4%
  known-answer: smooth field     1.82     1.35  141.59    4.3%  95.5%   0.2%   0.0%   0.0%   0.0%
  known-answer: + 6-tx checker 121.65    11.03  285.59    2.1%  45.8%   0.1%  41.1%  10.9%
    EXCESS local variance ours over vanilla: 75.26 - 29.39 = 45.87   (codec floor 1.52)
```

**Ours carries 3.8x and 2.6x vanilla's local variance.** The excess is **37x
and 30x the codec floor**, so it is not the block compressor.

**It is not a planted period either.** The two periods the candidates predict
were looked for explicitly, as a peak over the local median of the spectrum:

| period | what would put it there | vanilla | ours |
|---|---|---|---|
| 64 texels | the texture tiling at TILE=2048 | 3.59x / 2.22x | 2.42x / 2.33x |
| 4 texels | the 17x17 VTXT opacity grid (128 u) | 0.90x / 0.88x | 0.92x / 1.02x |
| 2.83 texels | the same grid as a 2-D lattice | 1.02x / 0.89x | 0.81x / 0.96x |

Nothing rings at the 17-grid's spacing in either sheet. The excess is
**broadband**, which is what a texture's own detail looks like and is not what
a blend artefact or a codec looks like.

---

## 2. The mip actually selected, and the mip a footprint match would pick

`s2_sampling.py`, log `logs/s2.log`.

### 2.1 The code

**Line numbers here were re-derived at 16:2x**, after lane CARDS-AGG edited
`src/lodgen.cpp` at 15:44 (the file went 10,137 -> 10,275 lines while this lane
was measuring, and every number below moved by about 137). The file as quoted:
450,162 bytes, 10,275 lines, sha1 `d81bfcc94016956a185867ca5f594fee46f7313c`,
CR 0 / LF 10,274. `scratchpad/splat1_20260911/anchors.txt` carries the anchor
TEXT beside every number, so a later edit shows as a mismatch rather than as a
silently wrong citation (`ww-contract-provenance`). **The file is live under
another lane: re-run the anchor pass before phase B rather than trusting these
numbers.**

`src/lodgen.cpp:6688-6692` (the stock chunk bake; the same five lines appear at
7802, 7840 and 7859 for the pyramid) -- see `anchors.txt`, the numbers moved
under this lane and are re-derived there:

```cpp
const float texelWorld = TILE / float( tex->getWidth() );
const float footprint  = span / float( RES );
const float mip = qBound( 0.0f,
    std::log2( qMax( 1.0f, footprint / texelWorld ) ),
    float( tex->getMaxMipLevel() ) );
return tex->getPixelT( u, v, mip );
```

and `src/lodgen.cpp:6332-6335`, verbatim, comment included:

```cpp
// world-space tiling of the source landscape textures; near-terrain
// repeats roughly every half cell (calibration against vanilla bakes is
// an open refinement -- the constant only affects apparent texel density)
constexpr float TILE = 2048.0f;
```

### 2.2 The mip it picks, every landscape texture in both chunks

Every one of them is **2048x2048 with 12 mips**, so with TILE = 2048 the source
is **1.000 world unit a texel** at mip 0 and the bake picks:

| | |
|---|---|
| footprint | `span/RES` = `4*4096/512` = **32 world units** |
| mip selected | **5.00** (64x64), 32.00 world units a texel |
| a footprint match | mip 5 -- **the same mip** |

**So the mip selection is not the defect.** It already picks the
footprint-matched mip, to the clamp. Twenty of twenty layer textures, both
chunks, same answer. (Four LTEXs resolve through a `.bgsm` the offline model
does not open -- `LNFoothillsDirt01`, `LNFoothillsDriedGrass01`,
`LNFoothillsDriedGrass01NoGrass`, `LDebrisGround` -- named, not approximated.)

What a footprint-matched mip does NOT do is remove the texture's variation
**at 32 units**: it hands the bake one whole texel of the 64x64 image per bake
texel. With the texture stretched to a 2,048-unit repeat, 32 units of ground is
1/64 of the texture, so that 64x64 image is the ground texture's own pattern,
and the bake prints it, tiled 8x8 across the chunk, at full contrast.

### 2.3 One texel row, sampled every way

Row j=256 of each tile, 512 texels. `lv` here is the 1-D three-sample local
variance along the row.

```
=== chunk (-20,24) ===                mean      var      lv
vanilla shipped row                  79.75    58.60   10.87
our baked row                        84.88   187.75   56.61
offline, the code's mip              84.82   178.68   51.91
offline, exact footprint box mean    85.20   174.49   49.28
offline, texture global mean         85.25   118.81    2.09
offline, code mip + 2                84.17    80.88    2.11
offline, code mip + 4                85.16   110.53    1.92
offline, base layer only             83.94   171.67   56.44
  REPRODUCTION CHECK offline vs the real bake, whole tile: mean 3.90 p95 16 max 123 of 255

=== chunk (-20,20) ===                mean      var      lv
vanilla shipped row                  85.39   114.40   25.14
our baked row                        70.15   482.13   29.07
offline, the code's mip              58.14   146.74   17.85
offline, exact footprint box mean    58.41   144.64   16.77
offline, texture global mean         58.48   128.07    4.34
offline, code mip + 2                58.20   121.98    3.98
offline, code mip + 4                58.16   128.87    4.19
offline, base layer only             50.47    24.09   12.45
  REPRODUCTION CHECK offline vs the real bake, whole tile: mean 7.06 p95 40 max 96 of 255
```

Three things this row says:

1. **The EXACT footprint box mean is no smoother than the mip** (49.28 vs
   51.91; 16.77 vs 17.85). The mip chain is doing its job. A "better footprint
   match" buys nothing.
2. **Two mips coarser kills the speckle outright** (51.91 -> 2.11), and so does
   the texture's global mean (2.09). The speckle is the texture's own content
   at the scale the bake asks for it.
3. **On (-20,24) the BASE LAYER ALONE reproduces the whole of it** (56.44
   against the full blend's 51.91). The 17-grid blend is not the source there.

The reproduction check is the offline model's own ceiling: 3.90 of 255 mean on
(-20,24), consistent with the contract's own independent ring-0 model at 3.26.
On (-20,20) it is 7.06 because that sheet carries ROADS1's rasterised roads and
a grass tint the offline model does not fold in -- stated, not hidden.

### 2.4 Where TILE = 2048 came from: nowhere

* `src/lodgen.cpp:6335` -- a bare `constexpr` whose own comment says the
  calibration is open.
* `docs/LODGEN_TERRAIN_VT.md:734` restates it (`u = frac(wx/2048)`, "the bake's
  world-space tiling") and **cites the bake**, which is circular.
* **No record field carries it.** LTEX is EDID + TNAM(TXST) + HNAM + SNAM +
  GNAM; TXST is texture paths and flags; LAND carries VHGT, VNML, VCLR, BTXT,
  ATXT, VTXT. Checked over all 20 LTEXs in the two chunks. The tiling is not in
  the data at all -- it is in the engine.

### 2c. The engine's own number (`s2c_engine_tiling.py`, log `logs/s2c.log`)

Read out of `Fallout4.exe` **1.10.155.0**, 65,319,936 B, at
Todd's treat. Every address below is
an RVA-derived VA for **that build** and the script re-derives all of it from
the file, so nothing here is quoted from memory.

1. The only landscape-tiling setting in the binary is
   **`fLandTextureTilingMult:Landscape`** (one copy, file 0x2C84DD8,
   VA 0x142C861D8).
2. Its `Setting` record (`{vtable, data, name}`, file 0x36E83A8) carries
   **data = 0x3FC00000 = 1.5f**. The record's neighbours decode to
   `bCurrentCellOnly` = 0, `iMaxGrassTypesPerTexure` = 2,
   `fTexturePctThreshold` = 0.005 -- three independently sensible values, which
   is what says the stride and the field order are right. The setting is
   **absent from `Fallout4_Default.ini`**, so 1.5 is what runs.
3. The data slot (VA 0x1436E97B0) has **exactly one** code reference:

```
0x1403A74C6  movss    xmm0, [rip -> fLandTextureTilingMult]     ; 1.5
0x1403A74D1  ucomiss  xmm0, 0 ; jne                             ; if 0, fall back to
0x1403A74D6  movss    xmm0, [0x142C4B1BC]                       ;   16.0
0x1403A74E5  movss    xmm6, [0x142C4B1B4]                       ; 4.0
0x1403A74ED  divss    xmm6, xmm0                                ; xmm6 = 4/mult
0x1403A75FD  movss    xmm1, [0x142C48D60]                       ; 1.0
0x1403A760F  divss    xmm2, xmm6                                ; xmm2 = mult/4 = 0.375
```

4. And the loop it feeds is the landscape quadrant grid itself, not an
   inference:

```
0x1403A7620  outer loop, r11w ... 0x1403A76DB  cmp r11w, 0x11 ; jl     17 rows
0x1403A7650  inner loop, r10w ... 0x1403A76CC  cmp r10w, 0x11 ; jl     17 columns
0x1403A769C  movsx eax, r10w                                    the COLUMN index 0..16
0x1403A76B2  mulss xmm0, xmm2                                    u = col * 0.375
0x1403A76B6  movss [rsp+0x180], xmm0        ([rsp+0x184] = row * 0.375)
0x1403A76BF  mov rcx, [rsp+0x180]
0x1403A76C7  mov [rax + r9*8 - 8], rcx                           the (u,v) pair, stored
```

17 vertices = 16 quads = **one quadrant = 2,048 world units**, so the spacing
the multiplier steps over is 2048/16 = **128 units**, and

```
uv per vertex step            = fLandTextureTilingMult / 4 = 0.375
WORLD UNITS PER REPEAT        = 128 / 0.375 = 341.3333
                              = 12 repeats a cell, 6 a quadrant
THE BAKE USES 2048            -> 6.0000 times too large
```

**341.333 units is 10.67 texels of the far sheet.** That is bungo's "very tiny
repeating pixel sized pattern", exactly.

### 2d. The tiling sweep, with a working instrument (`s2b_tiling.py`, `logs/s2b.log`)

Each row re-bakes the whole chunk offline at that repeat, with the bake's own
footprint-mip rule (so the mip moves with the tiling, as the code would), and
asks two questions: does it reproduce vanilla's speckle, and does its
high-passed field **correlate** with vanilla's -- i.e. is the landscape
texture's own pattern present in Bethesda's sheet at that repeat.

```
=== chunk (-20,24) ===   vanilla local var 19.81      ours 76.22
  TILE(u)   src  mip   local var   corr(van)   twin floor
  171      2048  8.58       7.92     +0.0019     +0.0041
  341      2048  7.58      12.93     +0.0003     +0.0057      <- the engine's own
  512      2048  7.00      30.11     +0.0021     +0.0042
  683      2048  6.58      18.86     +0.0060     +0.0053
  1024     2048  6.00      48.37     +0.0039     +0.0063
  2048     2048  5.00      71.95     -0.0000     +0.0077      <- what we ship
  CEILING  corr(OUR sheet, offline re-bake at TILE=2048) = +0.9135
  ceiling  corr(vanilla, vanilla) = +1.0000

=== chunk (-20,20) ===   vanilla local var 29.39      ours 75.26
  171      2048  8.58      11.27     +0.0001     +0.0111
  341      2048  7.58      14.60     +0.0009     +0.0099      <- the engine's own
  512      2048  7.00      28.76     -0.0011     +0.0074
  683      2048  6.58      20.05     -0.0047     +0.0116
  1024     2048  6.00      46.20     +0.0000     +0.0085
  2048     2048  5.00      61.71     +0.0076     +0.0073      <- what we ship
  CEILING  corr(OUR sheet, offline re-bake at TILE=2048) = +0.8754
  ceiling  corr(vanilla, vanilla) = +1.0000
```

Two readings, both controlled:

* **The speckle is a monotone function of the tiling.** At 2,048 the offline
  re-bake reads 71.95 and 61.71 -- our shipped sheets read 76.22 and 75.26, so
  the model owns the effect. At the engine's 341.333 it reads **12.93** and
  **14.60**, i.e. **below vanilla's 19.81 and 29.39**. The excess does not
  merely shrink; it goes away.
* **Bethesda's sheet contains no landscape-texture pattern at ANY of the six
  repeats.** Every correlation is inside its own phase-twin floor. And the
  instrument is not blind: pointed at OUR sheet, where a landscape texture
  certainly is, the same correlation reads **+0.91 and +0.88**. So this is a
  controlled negative, not a null result -- whatever fine detail vanilla's sheet
  has is objects, roads and paint, not the ground texture's own grain.

---

## 3. The candidate split

`s3_candidates.py`, log `logs/s3.log`. Each candidate is measured **where the
other two cannot act**.

### 3.1 The grass tint -- ruled out, and one tile rules it out by itself

The cover byte is the alpha of `_data.DDS`, and only when the sheet is DXT5 and
its `dwReserved1` carries `'WWCV'` (`src/lodgen.cpp:7070-7095`). Read from the
two files:

| tile | `_data.DDS` | what that means |
|---|---|---|
| (-20,24) | **DXT1, stamp 0x00000000** | **no cover plane was written at all** |
| (-20,20) | DXT5 `WWCV`, `coverFull` = 96 | cover present, the tint acted |

**Chunk (-20,24) has no cover plane, so the grass tint cannot have touched one
texel of it -- and it is the tile with the LARGER excess, 56.41.** That is the
whole answer on its own. On the tile that does have cover:

```
cover == 0 on 124,745 texels (47.6%): ours lv 68.11  vanilla lv 29.70  excess 38.41
cover  > 0 on 137,399 texels (52.4%): ours lv 81.75  vanilla lv 29.10  excess 52.65
```

Where the tint provably could not act the excess is still **38.41**. The most
the tint can be charged with is the 14.24 gap between the two halves, and that
gap is confounded with the different ground under them. **Upper bound 31% on
one tile, 0% on the other.**

### 3.2 VCLR -- ruled out, twice

Offline, with step 6 removed entirely:

| tile | the law as it stands | no VCLR multiply | moved by |
|---|---|---|---|
| (-20,24) | 71.95 | 71.97 | **+0.02** |
| (-20,20) | 61.71 | 61.76 | **+0.05** |

**0.04% and 0.15% of the excess.** And by region, on the tile where 5 of the 16
cells carry no VCLR record at all and therefore cannot have been multiplied:

```
VCLR cells    180,224 texels: ours lv 74.43  vanilla lv 21.46  excess 52.97
no-VCLR cells  81,920 texels: ours lv 80.16  vanilla lv 16.18  excess 63.98
```

The cells VCLR **cannot** have touched are the ones with the LARGER excess.

### 3.3 The sampling -- it is all of it

Offline re-bakes of the whole tile, luminance local variance:

| | (-20,24) | vs vanilla 19.81 | (-20,20) | vs vanilla 29.39 |
|---|---|---|---|---|
| the law as it stands | **71.95** | +52.14 | **61.71** | +32.33 |
| exact footprint box mean | 71.45 | +51.64 | 61.40 | +32.01 |
| base layer only (no 17-grid) | 71.95 | +52.14 | 51.02 | +21.64 |
| no VCLR multiply | 71.97 | +52.16 | 61.76 | +32.38 |
| code mip + 1 | 15.79 | -4.02 | 17.98 | -11.41 |
| code mip + 2 | 8.09 | -11.71 | 11.42 | -17.97 |
| code mip + 3 | 6.15 | -13.65 | 9.63 | -19.76 |
| texture global mean (no texture detail at all) | 5.68 | -14.13 | 9.61 | -19.78 |
| **the engine's own tiling, 341.333** (section 2d) | **12.93** | **-6.88** | **14.60** | **-14.79** |

### 3.4 The table the brief asked for: source of variance -> share of the excess

Excess = ours minus vanilla, in local variance. Attribution is measured on the
offline model, whose own reproduction error against the real bake is 3.90 and
7.06 of 255 (section 2.3) -- that gap is the table's uncertainty.

| source | (-20,24), excess 52.14 | (-20,20), excess 32.33 |
|---|---|---|
| **the source texture's own detail, at the shipped tiling** | 66.27 of local variance goes when it goes: **100% of the excess, and 27% more** | 52.10 goes: **100% of the excess, and 61% more** |
| of which: the 17x17 VTXT blend of DIFFERENT textures | 0.00 -- **0%** | 10.69 -- **33%** |
| of which: the base texture alone | 52.14 -- **100%** | 21.64 -- **67%** |
| the grass tint | **0%** (no cover plane on this tile) | at most 14.24 -- **31% or less**, confounded |
| the VCLR multiply | +0.02 -- **0.04%** | +0.05 -- **0.15%** |
| the BC1 block codec | 1.52 (the section 0.2 floor) -- **2.9%** | 1.52 -- **4.7%** |
| the mip SELECTION being wrong for the footprint | **0** -- the code already picks the footprint mip; the exact box mean is 0.50 SMOOTHER, not 52 | **0** -- likewise, 0.31 |

The first row exceeds 100% because removing the texture detail entirely takes
the sheet **below** vanilla: vanilla's own fine detail is objects, roads and
paint that we do not bake. Setting the tiling to the engine's own number lands
at **12.93 and 14.60**, i.e. **-6.88 and -14.79 against vanilla** -- the excess
is gone and a little of vanilla's legitimate detail is still missing, which is
TERRAIN-AO1 / ROADS1 territory, not this lane's.

### 3.5 The counterweight: the corrected tiling is SMOOTHER than vanilla, and its spectrum moves FURTHER away

`s5_bands.py`, log `logs/s5.log`. A scalar can be matched by accident; six bands
cannot, so the same four sheets are compared band by band. `L1` is the total
absolute difference of the band shares against vanilla, in percentage points.

```
=== chunk (-20,24) ===   >=128tx  32..128  8..32    4..8    2..4    <=2      L1
  VANILLA          lv 19.81   11.1%   37.1%   20.7%   8.5%   16.3%   6.3%    0.0
  OURS as shipped  lv 76.22   24.0%   26.1%   20.1%  11.6%   15.6%   2.6%   32.2
  offline 2048     lv 71.95   20.4%   31.2%   20.8%  11.2%   14.3%   2.2%   24.1
  offline 341.333  lv 12.93   30.8%   45.2%   17.3%   3.9%    2.6%   0.2%   55.7

=== chunk (-20,20) ===
  VANILLA          lv 29.39   21.3%   29.2%   20.0%   7.9%   15.3%   6.2%    0.0
  OURS as shipped  lv 75.26    5.0%   45.8%   32.0%   8.2%    7.6%   1.4%   57.9
  offline 2048     lv 61.71    7.0%   31.3%   40.0%  10.4%    9.9%   1.3%   49.3
  offline 341.333  lv 14.60    9.7%   40.6%   44.3%   3.9%    1.4%   0.1%   71.3
```

**Stated plainly against the lane's own verdict:** correcting the tiling does
NOT move our spectrum toward vanilla's. It moves it away. Vanilla keeps
**16.3% and 15.3%** of its power at 2-4 texels and **6.3% and 6.2%** below 2
texels; at the correct tiling we keep **2.6% / 1.4%** and **0.2% / 0.1%**.

That is consistent with everything else this lane measured and it is not a
contradiction: section 2d showed that vanilla's fine detail does not correlate
with the landscape texture at ANY repeat. So vanilla's 16% at 2-4 texels is
content we do not bake -- object shadows, rubble, rocks, road edges, paint --
and the right reading is:

* at TILE = 2,048 our sheet has the WRONG fine detail, at the wrong amplitude,
  from a source that is not in vanilla's sheet at all;
* at TILE = 341.333 our sheet has almost NO fine detail, which is honest for a
  sheet built only from the splat, and the missing content is a separate,
  already-open item (TERRAIN-AO1, ROADS1, objects).

Substituting one for the other would be fitting a number rather than fixing a
cause, which is why the fix is the tiling and nothing else.

---

## 4. Verdict

**ONE candidate, one number: the world-space tiling of the landscape textures.
`TILE = 2048.0f` should be `341.3333` -- the bake stretches every landscape
texture to exactly 6.0000x its size.**

* The number is the engine's, not a fit: `fLandTextureTilingMult` = 1.5 in
  `Fallout4.exe` 1.10.155, `uv = vertexIndex * mult/4 = 0.375` per landscape
  vertex, 128 world units a vertex over the 17x17 quadrant grid, so
  `128 / 0.375 = 341.3333` world units a repeat -- 6 repeats a quadrant, 12 a
  cell. Every address is in section 2c and `s2c_engine_tiling.py` re-derives all
  of it from the shipped binary.
* **bungo's reading was right.** At the correct scale one repeat is 10.7 texels
  of a 32-unit far sheet -- his "very tiny repeating pixel sized patterns" --
  and the footprint-matched tap over it lands near the texture's own mean, which
  is smooth. At 2,048 one repeat is 64 texels and the bake prints a 64x64 image
  of the ground texture at full contrast, tiled 8x8 across the chunk.
* **The mip selection is NOT the defect.** It already picks the footprint mip
  (5.00 for every one of the 20 layer textures, all 2048x2048 with 12 mips), and
  an exact box mean over the footprint is 0.50 and 0.31 units SMOOTHER --
  nothing. The first candidate in the brief is refuted with its own number.
* **Neither is the tint** (0% on the tile with no cover plane, at most 31% and
  confounded on the other), **nor VCLR** (0.04% / 0.15%), **nor the codec**
  (2.9% / 4.7%).
* **Predicted effect, offline, before any build** (`s4_predict.py`,
  `logs/s4.log`):

| tile | | local variance | whole-tile mean abs RGB vs vanilla |
|---|---|---|---|
| (-20,24) | ours as shipped | 76.22 | 16.33 |
| | offline, TILE = 2048 | 71.95 | 16.89 |
| | **offline, TILE = 341.333** | **12.93** | **15.02** |
| | vanilla | 19.81 | -- |
| (-20,20) | ours as shipped | 75.26 | 19.51 |
| | offline, TILE = 2048 | 61.71 | 21.73 |
| | **offline, TILE = 341.333** | **14.60** | **20.16** |
| | vanilla | 29.39 | -- |

  **Said plainly and said now: this removes the speckle and it does NOT close
  the colour error.** 16.89 -> 15.02 and 21.73 -> 20.16 of 255. The remaining
  16-20 is the GRADING -- ROADS1 measured vanilla darkening both the road and
  the background by the same x0.82-0.83 -- and that is still the open item
  "splat calibration vs vanilla grading". Fixing the tiling does not fix it and
  must not be reported as if it did.

  (Two definitions of the whole-tile error are in circulation: TERRAIN-R's 19.96
  was the PYRAMID tile against vanilla's south-west quadrant, ROADS1's 22.81 the
  chunk sheet under its own metric. The numbers above are this lane's own
  definition -- the mean of the per-channel absolute difference over 512x512x3 --
  computed the same way for every row, which is what makes the column comparable
  with itself.)

### 4.1 What would refute this

* If `fLandTextureTilingMult` were read somewhere else in the binary with a
  different meaning. It has exactly one code reference (section 2c, step 3).
* If the 17x17 loop at 0x1403A7620 built something other than a landscape
  quadrant. It writes an 8-byte (u,v) pair per vertex over a 17x17 grid, inside
  the same function that reads the landscape tiling setting, and the enclosing
  code builds 16-bit indices over the same stride of 17.
* If a per-LTEX or per-TXST scale existed that overrode the global. None does;
  all 20 LTEXs in the two chunks were listed field by field.
* If bungo's own `Fallout4.ini` set `fLandTextureTilingMult` to something other
  than 1.5, his world would tile differently from this bake. **CHECKED:** the
  key appears in none of `Fallout4.ini`, `Fallout4Custom.ini`,
  `Fallout4Prefs.ini` under his `Documents\My Games\Fallout4`, nor in
  `Fallout4_Default.ini` / `High` / `Medium` / `Low` / `Ultra.ini` in the game
  folder -- so the compiled 1.5 is what his game uses. Another user could set
  it, which is why the change below is a CLI value and not a new hard-coded
  number.

### 4.2 What the change is

One value in each of the two bake functions, behind a switch whose off value is
byte-identical to the rung:

* `src/lodgen.cpp:6335` and `src/lodgen.cpp:7623`, `constexpr float TILE` ->
  a field on the bake options, default **341.3333**, with
  `--land-tiling <units>` and `--land-tiling 2048` as the exact way back.
* `docs/LODGEN_TERRAIN_VT.md` 2.5's `u = frac(wx/2048)` and its mip line become
  `frac(wx/T)` with `T` stated and its provenance quoted, and the ring-0 runtime
  is told the same number -- **the runtime and the pyramid must agree on T or
  ring 0 seams**, which is exactly what 2.5 exists to prevent.
* `tests/spells/lodgen_terrain_model.py`'s own `TILE = 2048.0` moves with it, or
  the ring-0 gate measures the old law against the new bake.

**PHASE B WAS NOT REACHED** -- see section 6.

---

## 5. The reds

1. **The contract's VCLR range does not reproduce.**
   `docs/LODGEN_TERRAIN_VT.md` 2.5 says "over the Sanctuary region (cells
   -20..-17 x 24..27) every byte of every VCLR present is in **249..255**".
   Measured here over exactly those 16 cells: **11 of 16 carry a VCLR and the
   range is 203..255**; on (-20,20) 16 of 16 carry one, over **170..255**. The
   conclusion the contract draws from it (VCLR is not the grading) still holds --
   this lane measured VCLR's effect at 0.02 of 52 -- but the quoted range is
   wrong and the page should be corrected.
2. **The tiling is wrong in more places than the two the fix names.** `TILE`
   appears at `src/lodgen.cpp` 6335, 6680, 6681, 6688 (stock colour) and 7623,
   7798, 7799, 7802, 7836, 7837, 7840, 7855, 7856, 7859 (pyramid: colour, mask,
   emissive) -- fourteen sites, re-derived at 16:2x into `anchors.txt`. Every sheet
   the bake writes from a landscape texture -- colour, roughness, metallic,
   emissive -- is sampled 6x too coarse. A fix that changes only the colour path
   leaves the mask sheet describing a different patch of ground from the colour
   beside it, which 2.2's own comment forbids.
3. **`_msn` is unaffected and must stay so.** The normal sheet is computed from
   VHGT, not from a landscape texture, so no tiling term reaches it. Any change
   that moves `_msn` bytes is a bug in the change.
4. **The offline model cannot open four LTEXs** that resolve through a `.bgsm`
   (`LNFoothillsDirt01`, `LNFoothillsDriedGrass01`,
   `LNFoothillsDriedGrass01NoGrass`, `LDebrisGround`). They are named and
   skipped, never approximated, and they are part of the model's 3.90 / 7.06 of
   255 reproduction gap.
5. **Not measured: whether 341.333 holds for FO76 or Skyrim worldspaces.** Only
   Fallout 4 1.10.155 was read, and this generator also opens `.btd` regions.
6. **The whole-tile colour error is NOT closed by this** (section 4). Anyone
   reading "the terrain bake was fixed" should read that row too.

---

## 5a. The picture

`scratchpad/splat1_20260911/images/speckle_diagnosis.png`, 1078 x 1324, made by
`make_pictures.py` under `ww-texel-picture`: the crop is chosen BY THE METRIC on
the BEFORE artefact -- the 128x128 window of our shipped sheet with the worst
local variance, searched over the whole sheet, found at (216,128) reading 89.70 --
and the SAME texels appear in all four panels at 4x nearest neighbour, with the
local variance burned into each caption.

| panel | local variance on that crop |
|---|---|
| VANILLA `Commonwealth.4.-20.24.DDS` | **18.58** |
| OURS as shipped, TILE = 2048 | **89.21** (4.8x) |
| OURS re-baked offline, TILE = 341.333 | **13.29** |
| `|ours - vanilla|` x4 | -- |

The offline model's own control on the same crop, baked at the shipped 2,048,
reads 84.08 against the real sheet's 89.21: the model owns the effect and the
5-unit gap is BC1 plus the four `.bgsm` layers it will not open.

The picture was opened and read back before this line was written (the first
render clipped its own title; the header was shortened and the caption band
widened from 56 to 84 px).

---

## 6. Phase B -- NOT REACHED, BUILD PENDING

The brief gated phase B on `scratchpad/cards_agg_20260911/DONE` **and**
`scratchpad/nifparse1_20260911/DONE` existing **and** no `scratchpad/*/BUILDING`
anywhere. `scratchpad/splat1_20260911/poll_phaseb.sh` polled every 60 s and
`logs/poll2.log` records every poll: CARDS-AGG held `BUILDING` throughout and
neither `DONE` ever appeared. `Fallout4.exe` was absent at every check, so the
game was never the blocker.

**What is actually known about the gate, at 16:21 when this lane ended:** no
`DONE` on either lane, and CARDS-AGG's `BUILDING` (stamped 15:48) still up. Its
own `PENDING.md` says "THE BUILD HAS NOT RUN YET". NIFPARSE1 wrote its four
handoff documents 15:43-15:47 and has been quiet since -- that lane looks ended.

**CORRECTION, and it matters to whoever picks this up.** An earlier draft of
this section said CARDS-AGG had gone quiet at 16:02 and had ended with a stale
marker, and closed the gate on that reasoning. **That was wrong: it wrote at
16:16, 16:17 and 16:20 and is still working.** It may yet build and write
`DONE`. SPLAT1 ends BUILD PENDING because its own session ends here, not because
the blocking lane is finished. A MISTAKES entry is written: a gate-poll that
watches only for markers cannot tell "still working" from "ended with the marker
up", and must print the blocking lane's newest mtime beside every poll.

**Three lanes now share one build slot and one file.** The order that avoids a
second build is NIFPARSE1's hook-up, then CARDS-AGG's, then SPLAT1's tiling
value LAST with the anchor pass re-run -- written out in `PENDING.md` and
covered by `nifskope-ww-resume-pending`.

**NO CODE WAS WRITTEN BY THIS LANE, so nothing is half-applied and there is
nothing to revert.** The resume is `scratchpad/splat1_20260911/PENDING.md`, with gates
S1-S7 pre-registered there BEFORE any build, including the two that matter most:
`--land-tiling 2048` must be byte-identical to the rung, and `_msn` must be
byte-identical at BOTH tiling values.

Documents delivered as TEXT for the director to splice, never applied by this
lane: `WW_CHANGES_ENTRY.md`, `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md` (three
entries), `CONTRACT_AMENDMENT.md` (the `docs/LODGEN_TERRAIN_VT.md` 2.5 rewrite
with its provenance, plus the VCLR correction).

---

## 7. The finished-work skill review (CONSTITUTION 1a)

**Loaded:** `ww-control-calibration` (its five parts are section 0 -- the
known-answer inputs, the floor through the same lossy pipeline, the independent
phase twin, the ceiling from the same data; and its "a floor with a fixed step
cannot fire" rule is why the codec floor is measured on never-coded data rather
than by re-encoding vanilla). `ww-texel-picture` (section 5a: the crop by the
metric, nearest neighbour, the fixed cell, the caption carrying the report's own
number, and rule 5 -- open the PNG and look, which caught a clipped title).
`nifskope-ww-lodgen`, `nifskope-ww-vanilla-compare` and `ww-spec-gate-audit` were
consulted for the tile choice and for the rule that a pre-registered number gets
audited before it is reproduced -- which is exactly what happened to `TILE`.
`ww-anchored-hookup` and `nifskope-ww-build-verify` were NOT needed: phase B did
not run.

**Written, in the REPO tree, for the director to mirror to the live tree:**

1. `.claude/skills/fo4-engine-constant-from-ini-setting/SKILL.md` -- **new.**
   Recovering a number the engine uses and never writes down, out of
   `Fallout4.exe`'s `Setting` array: finding the name, the {vtable, data, name}
   record and the NEIGHBOUR check that proves the stride, the rip-relative xref
   sweep, resolving the paired float constants, and -- the step this lane nearly
   skipped -- reading the LOOP BOUNDS to learn what the number multiplies, since
   `uv = index * 0.375` supports three different answers until `index` is pinned.
   This lane spent most of its thinking re-deriving that procedure and it is
   certain to recur: several other generator constants ("calibration is an open
   refinement") have no source outside our tree.
2. `.claude/skills/ww-sheet-diff/SKILL.md` -- **section 9 appended**
   (append-only, LF preserved, 136 -> 177 lines). "Is it as SMOOTH as vanilla's"
   is a different question from "how far apart", and it needs the local-variance
   / band-spectrum pair, a codec floor measured on never-coded data, the
   known-answer checker with its radius COMPUTED, and a correlation ceiling
   before any negative may be reported.

**Declined:** a skill for the offline chunk-bake reproduction (`offline_bake.py`).
The tree already carries an independent terrain model in
`tests/spells/lodgen_terrain_model.py`; the right move is to give THAT model the
switches this lane needed (mip mode, tiling, layers on/off, VCLR on/off) rather
than to write a skill describing a second copy. Named here as owed, for phase B
or for the lane that next needs it.


---

## 8. Closed 18:20 -- RESUME3 owns the fix; SPLAT1 owes nothing

The phase-B gate poll ran its full **120 polls** and closed at **18:18:33**,
"GATE NEVER OPENED" (`logs/poll2.log`). What it actually recorded:

* `cards_agg_20260911/DONE` **appeared** -- that lane landed. Which is why the
  16:2x correction in section 6 mattered: the first draft of this report would
  have told the director a live lane was finished.
* `nifparse1_20260911/DONE` **never appeared**, 0 of 120 polls.
* `scratchpad/resume3_20260911/` has held `BUILDING` since **16:45:32**.

**RESUME3 has taken up this lane's fix.** Its resume file lists step **R4**,
"the tiling change -- written and `--check` green, NOT applied, NOT built", and
`scratchpad/resume3_20260911/tiling.py` implements the verdict of section 4
exactly: default **341.3333**, `--land-tiling` with `--land-tiling 2048` as the
exact way back, **all fourteen `TILE` uses** across colour, mask and emissive on
both bake paths, citing VA 0x1403A74C6 / 0x1403A7620 and this lane by name --
including section 5's red 2, that changing the colour path alone would leave the
mask sheet on different ground from the colour beside it.

SPLAT1 therefore does **not** build and is not owed a build: RESUME3 holds the
slot and owns `src/lodgen.cpp`, and two lanes in one file is what the
constitution forbids. This report stands as the MEASUREMENT behind that change,
and its pre-registered gates S1-S7 (`PENDING.md`) are what RESUME3's build
should be checked against -- above all S1 (local variance 76.22 / 75.26 ->
about 12.93 / 14.60, against vanilla 19.81 / 29.39), S3 (`--land-tiling 2048`
byte-identical to the rung) and S4 (`_msn` byte-identical at BOTH values).

Nothing was built, run, committed or changed by this lane at any point.

---

## Build (RESUME3) -- 2026-09-11, appended, this lane's own text untouched

Phase B was built and gated by lane RESUME3. Rung
`release/NifSkope.before_resume3.exe` 16:20:02, 21,419,520 B, md5
`3ebf175826feee6c6545cf0873635acc`; shipping exe `release/NifSkope.exe`
**19:08:42, 21,435,904 B**. Full detail in `scratchpad/lane_resume3_report.md`
section 3.

### The anchor pass, re-run before anything was applied

`src/lodgen.cpp` at 16:45 was **450,162 B, 10,274 LF, 0 CR, sha1
`d81bfcc94016956a185867ca5f594fee46f7313c`** -- byte for byte what
`anchors.txt` recorded at 15:44:38, with all **14** `TILE` sites at the line
numbers this report quotes. Nothing had moved. The pass was run anyway, and its
sha1 is the proof rather than the absence of news.

**One anchor in this report does not match the file**: the comment block above
`constexpr float TILE = 2048.0f` is quoted here with an ASCII `--`; the file
carries an **EM DASH (U+2014)**. The anchor built from the quotation counted 0
and had to be rebuilt from the file's bytes. Same for
`docs/LODGEN_TERRAIN_VT.md`, which carries **UNICODE MINUS (U+2212)** in its
cell ranges. Both are in `MISTAKES.md` and the rule is now in
`ww-anchored-hookup` section 5.

### The change, and why all fourteen sites moved with one edit

Both bake functions declare their own `constexpr float TILE`, and all fourteen
uses read the declaration in scope -- so changing the two declarations to
`const float TILE = lodgenLandTiling();` moved colour, mask and emissive in both
bake paths together. That is this report's red 2 closed by construction rather
than by fourteen separate edits. The value lives in `src/lodgen.h` /
`src/lodgen.cpp` with the whole binary derivation in its comment, a non-positive
argument is refused, and `--land-tiling <units>` is the switch. **No panel row**
(LODUI1 owns those). `tests/spells/lodgen_terrain_model.py`'s own `TILE` moved
too, with a `WW_LAND_TILING` override.

### The gates, against the numbers pre-registered above

| id | pre-registered | measured | verdict |
|---|---|---|---|
| **S3** | every file identical at `--land-tiling 2048` | (-20,24) **6 files / 1,904,522 B identical**; (-20,20) **6 files / 1,403,974 B identical**, against the RUNG exe | **PASS** |
| **S4** | `_msn` identical at both tilings | identical on both tiles, 174,888 B each -- and the REFUTER (the same comparison over the colour sheet) went **RED** on both, so the check is known to be able to fail | **PASS** |
| **S1** | (-20,24) 76.22 -> predicted 12.93 vs vanilla 19.81; (-20,20) 75.26 -> predicted 14.60 vs vanilla 29.39; PASS inside 20 % | (-20,24) real **12.34**, **4.6 %** -- PASS. (-20,20) real **25.83**, 77 % -- RED as written. **Discriminator run**: the offline model draws no roads, and that tile carries ROADS1's road. Against the `--no-roads` bake the prediction is inside the gate at BOTH tilings -- 68.59 vs 61.71 (+11.2 %) and **17.00 vs 14.60 (+16.4 %)**. The tiling's own effect agrees to **2.4 %**: 74.06 -> 25.83 (a 48.23 drop) against the predicted 61.71 -> 14.60 (47.11) | **PASS on the like-for-like artefact; RED on the road-bearing one, with the cause measured** |
| **S2** | 16.33 -> ~15.0 and 19.51 -> ~20.2; PASS = not worse by more than 1.0 | **16.33 -> 14.33** and **20.43 -> 18.92**: better by 2.00 and 1.50 | **PASS** |
| **S5** | the harnesses the change reaches | `lodgen_terrain.sh` 26/0, `lodgen_terrain_vt.sh` 41/1, `lodgen_ground_cover.sh` 29/5, `lodgen_terrain_pbrm.sh` 14/0, `ui_align.sh` 11/0, `water_ui.sh` 82/0 -- all at baseline. **`lodgen_roads.sh` 11/1, moved**, see below | one moved, named |
| **S6** | the picture, same texels, 4x, variance burned in | `scratchpad/resume3_20260911/images/cmp_tiling_fixed.png`, 1078 x 1346, crop chosen by the metric at (216,128) reading 89.70. **All four panels are DDS files off disk -- nothing modelled**, which is the difference from `speckle_diagnosis.png` | **PASS** |
| **S7** | exe newer than every changed source, dependent objects rebuilt, `style.qss` identical | 0 STALE over the whole working set; **0 of 246 object blocks stale** against the 95 changed files; `res/style.qss` and `release/style.qss` byte-identical after every link | **PASS** |

### Two things this lane's prediction got wrong, and one it got exactly right

* **The whole-tile colour error prediction was pessimistic in the right
  direction.** Predicted 15.02 and 20.16; measured **14.33** and **18.92**.
* **The local-variance prediction for (-20,20) was 43 % low in absolute terms**,
  because the offline model draws no roads. Its DELTA was right to 2.4 %, which
  is the part the change is responsible for. The model's residual against the
  like-for-like bake is a roughly constant +11 to +16 % at both tilings -- the
  four `.bgsm` LTEXs it cannot open, plus BC1, i.e. exactly the ceiling this
  report already named as its red 4.
* **A bonus control confirmed this report's tint verdict independently**:
  `--grass-tint 0` changed (-20,20)'s local variance by **exactly 0.00** (68.59
  and 17.00 both ways), on the tile where the tint contribution had been
  "confounded".

### The moved harness count, and it is this change's doing

`lodgen_roads.sh` R5 bar 2 -- "the road must correlate with vanilla's road at
least 80 % as well as the surrounding GROUND correlates with vanilla's ground".
Re-run on the rung exe in the same session so both sides are measured:

| | rung (2048) | shipping (341.3333) |
|---|---|---|
| the road term | **0.3065** | **0.3061** |
| the reference, the ground around it | 0.3442 | **0.4024** |
| bar 2 = 0.8 x reference | 0.2753 | 0.3219 |
| verdict | ok | **FAIL** |

**The road signal did not weaken (0.0004).** The corrected tiling made the
GROUND match vanilla 17 % better, which raised a bar defined as 80 % of it.
Nothing was changed to make it green. The red states something true and new:
now that the ground matches, **our road raster is the worse-matching part of the
sheet**, and that is a roads lane's work.

### The contract

`docs/LODGEN_TERRAIN_VT.md` 2.5 now carries `T = 341.3333` with the whole binary
derivation, says the runtime and the pyramid must use the SAME `T` or ring 0
seams, and states what the tiling does NOT fix. The VCLR sentence is corrected
and the withdrawn range is named as withdrawn: **11 of 16 Sanctuary cells carry
a VCLR, over 203..255**, not 249..255. 1,211 -> 1,256 lines, CR 0 before and
after, provenance re-stamped from the live tree at apply time.
