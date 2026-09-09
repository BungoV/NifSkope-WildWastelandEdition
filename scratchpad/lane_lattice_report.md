# Lane LATTICE — the square pattern on the mountain

bungo, on `scratchpad/mountains_20260907/images/mountain_peak_closeup.png`,
verbatim: *"You can see the square pattern on the right in the terrain, which is
not good."*

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, uncommitted
("Not yet"). `Fallout4.exe` was down for the build and for every render; it
CAME UP mid-lane (pid 27604) and is still up, which is why one gate is pending
(section 3.6). One `NifSkope.exe` at a time; every window at
`WW_WINDOW_AT=1960,40`.

**Answer.** The square pattern is in OUR `_msn` — not the mesh, not the
renderer — proved by swapping the two under one pinned camera. It has TWO
causes, and this lane found, measured and fixed the first: the plain bilinear
blend hands the encoder a gradient that is KINKED at every height sample, so
the sheet is creased on the 129-sample grid, which at dim 4 is every 4 texels
in both axes. The second is that the sheet has too little detail to fill
512x512, so the block encoder flattens 16–29% of its 4x4 blocks into literal
squares; that is the open "still blurry" thread, not a new generator bug.
**The pattern is reduced but still visible after the fix. I am not calling it
fixed.**

Scripts, all under `scratchpad/mountains_20260907/`: `lattice.py` (the
generator replicated offline + the samplers + the block codec), `lattice_run.py`
(the spectral comb, kept for the record), `lattice_phase.py` (**the**
measurement), `lattice_variants.py` (the interventions), `lattice_shade.py`,
`lattice_screen.py`, `lattice_image.py`, and `land_all.bin` (every Commonwealth
cell's 33x33 VHGT, from `lodgen --dump-land` on the MASTER).

---

## 1. Measurement — ours against vanilla

### 1.1 First: is it the sheet, the mesh, or the renderer?

Four renders, ONE camera (`WW_RENDER_VIEW=8`, centre `2048,2048,38840`,
distance 11000, 1400x900 -> 1507x841 — the close-up's own pinned camera), and
**our own flat-grey diffuse in all four**, so the only things that change are
the mesh and the `_msn`:

| frame | mesh | `_msn` | square pattern present? |
|---|---|---|---|
| A `swap_A_ourmesh_ourmsn.png` | ours | **ours** | **YES** |
| B `swap_B_ourmesh_vanmsn.png` | ours | vanilla | no |
| C `swap_C_vanmesh_ourmsn.png` | **vanilla** | **ours** | **YES** |
| D `swap_D_vanmesh_vanmsn.png` | vanilla | vanilla | no |

Picture: `images/lattice_swap_2x2.png`. As a number, the autocorrelation of the
high-passed frame on one fixed patch of the near face (250,560)-(700,810), the
strongest peak past lag 2:

| frame | x profile | y profile |
|---|---|---|
| A ours mesh + OURS msn | lag 33, **0.067** | lag 22, **0.133** |
| B ours mesh + VANILLA msn | lag 23, 0.039 | lag 20, 0.061 |
| C van mesh + OURS msn | lag 30, **0.059** | lag 23, **0.121** |
| D van mesh + VANILLA msn | lag 24, 0.030 | lag 20, 0.043 |

The peak follows the `_msn` and not the mesh: swapping the mesh under our sheet
keeps it (A -> C), swapping the sheet under our mesh removes it (A -> B).
**It is our data, not NifSkope's tangent basis and not the geometry**, so
paragraph 4 of the brief does not apply. Control that the staging is honest:
frame A is byte-size identical to the `shot_ours4.png` of the delivered
close-up (178,814 bytes both).

### 1.2 The instrument, and why the obvious one is the wrong one

A **spectral comb** — energy at k = RES/p and its harmonics — is what one
reaches for, and it does not work here, which is worth keeping. The crease sits
at every grid line but its STRENGTH follows the terrain, so the amplitudes are
independent line to line, and a train of impulses with independent amplitudes
has a FLAT spectrum. Measured: the known-blocky pre-fix sheet reads comb
prominence **0.19** at period 4 — *below* a broadband field — because a
zero-order hold has spectral zeros exactly at those bins. `lattice_run.py` has
the whole run; nothing is concluded from it.

The instrument that works is **phase-conditional**, the same shape as the
x-mod-4 table in WW_CHANGES 2026-09-07. On the slope field the sheet encodes
(`P = -n_east/n_up`, `R = n_north/n_up`), minus its own 9x9 box mean, 8 texels
cropped:

    c(x) = mean_y | F(x+1,y) - 2 F(x,y) + F(x-1,y) |
    MOD  = ( max over residue classes of x mod 4  -  min ) / mean

### 1.3 Controls (ww-control-calibration)

| control | reads | what it proves |
|---|---|---|
| known answer, fractal field, no crease | MOD **0.005** | the metric does not fire on broadband content |
| known answer, same field creased every 4th row and column, random amplitude | MOD **0.218** | it fires on the artefact's actual shape |
| FLOOR — vanilla's shipped sheet, same tile, size and mip | MOD 0.296 / 0.245 | a sheet not built on our grid |
| CEILING — the PRE-FIX nearest sheet, rebuilt from the same VHGT through the same encoder and the same 4x4 block codec | MOD **2.002** | what "a lattice" reads on this data |
| **ceiling / floor** | **6.76x** and **8.17x** | PASS the pre-registered 5x gate |

The replica is checked against the shipped file before anything is read off it:
mean |difference| **1.85** and **1.62** bytes, 99th percentile 9 and 8 — i.e.
the offline model reproduces the generator to within the block codec.

### 1.4 The table — `Commonwealth.4.-60.36` and `Commonwealth.4.-12.44`, mip 0, 512x512

`hf` is the 6.6–8.2x metric of this thread (mean absolute deviation from a 9x9
blur, in bytes). `classes` are the four residue means of `c(x)`; the height
samples fall between class 3 and class 0.

| sheet | MOD | hf | classes (x mod 4) |
|---|---|---|---|
| **4.-60.36** vanilla shipped (BC3) | 0.296 | 16.406 | 0.6000 0.5689 0.5678 0.6057 |
| ours shipped (bilinear, BC1) | **0.809** | 2.922 | 0.0487 0.0230 0.0230 0.0489 |
| replica bilinear, no codec | **0.947** | 2.681 | 0.0240 0.0086 0.0086 0.0240 |
| **4.-12.44** vanilla shipped (BC3) | 0.245 | 14.457 | 0.5013 0.4761 0.4776 0.5014 |
| ours shipped (bilinear, BC1) | **0.878** | 2.254 | 0.0411 0.0163 0.0163 0.0415 |
| replica bilinear, no codec | **0.830** | 1.977 | 0.0174 0.0072 0.0073 0.0174 |

Two things to read here. Ours is **2.7x and 3.6x** vanilla's MOD. And the
raised classes are **0 and 3 on both tiles** — precisely the pair straddling a
height-sample line, which sits at texel `4k - 0.5`. Vanilla's classes are flat
to 6%; its 0.24–0.30 is its own block codec, which is also on a 4-texel grid.

### 1.5 Geometry: measured, and it is not the mesh

`lodgen dump -b 1` on both `.BTR` files for the same tile: ours 1,237 vertices /
2,341 triangles, vanilla's 1,086 / 2,084, and **both carry Vertex Desc
52776558133763 — VERTEX and UVs only, no vertex normals at all**, so neither
mesh can put a normal-space lattice into the picture. The swap test (1.1)
settles it independently: our sheet on vanilla's mesh keeps the pattern.

### 1.6 At coarser mips

At mip 1 the sample grid is 2 texels and the residue test has only two classes,
so it cannot separate anything; at mip 2 it is 1 texel and the test is
degenerate. The lattice is a mip-0 phenomenon and is measured there. Numbers in
the run log.

---

## 2. Which candidate — by intervention, one change per variant

Every row below is the same tile, the same encoder and the same emulated block
codec; only the named thing differs.

### (a) the bilinear C0 crease at every sample line — **THIS ONE**

| basis | MOD (BC1) | rms vs bilinear | hf vs bilinear |
|---|---|---|---|
| bilinear (shipped) | 0.736 / 0.680 | — | — |
| **quintic ease** | **0.630 / 0.530** | **+20.9% / +20.0%** | **+16.7% / +16.7%** |
| cubic smoothstep | 0.717 / 0.600 | +14.2% / +13.4% | +11.0% / +11.1% |
| cubic B-spline | 0.767 / 0.786 | −25.2% / −23.6% | −23.5% / −21.8% |
| Catmull-Rom (the 2026-09-07 reject) | 0.763 / 0.683 | +18.2% / +17.9% | +18.4% / +17.5% |

and, on the generator's own output **before** the block codec, which is what
the generator actually controls:

| basis | MOD, no codec | hf, no codec |
|---|---|---|
| bilinear | **0.947 / 0.830** | 2.681 / 1.977 |
| **quintic ease** | **0.209 / 0.197** | **3.365 / 2.501 (+25.5% / +26.5%)** |

A fall of **78% and 76%**, to **below vanilla's own 0.296 and 0.245**, with the
high-frequency energy going UP a quarter. No smoothness was bought with blur —
the two candidates that did buy it (B-spline −23.5%, pre-blur −24% to −57%) are
the ones that failed.

Why an ease is enough, and why it cannot ring: the central difference spans a
full grid step either side, so for ANY blend `w` of the four corner samples,

    H(g+1) - H(g-1) = (1-w)(H[i+1]-H[i-1]) + w(H[i+2]-H[i])

— algebra, not approximation. With `w = t` the encoder is handed a gradient
that is continuous but KINKED at every sample. An ease whose first two
derivatives vanish at 0 and 1 removes the kink while still passing through every
VHGT sample exactly and staying between the four it sits among, so it is
monotone and cannot overshoot the way Catmull-Rom did. Quintic
(`6t^5-15t^4+10t^3`) rather than the cubic smoothstep because the cubic's
SECOND derivative still steps at each sample and this sheet is a normal map —
measured, 0.717 against 0.630 on the same tile.

### (b) the 8-unit VHGT staircase — **NOT the cause**

Isolated on a smooth synthetic height field on the same 129-sample grid, no
codec, no terrain in the way:

| heights | basis | MOD |
|---|---|---|
| exact | bilinear | **1.254** |
| **quantised to 8 units** | bilinear | **1.254** |
| exact | quintic | **0.217** |
| **quantised to 8 units** | quintic | **0.218** |

The staircase moves the number by **0.000–0.001**. The reconstruction moves it
by 1.04. And on the real tiles, dequantising by pre-blurring the height grid
does not help either — sigma 0.25 leaves MOD unchanged (0.736 -> 0.736), and
the sigmas that do move it (0.5, 1.0) move it the WRONG way while destroying
24% and 57% of the high-frequency energy.

### (c) the texel/sample phase from the x-mod-4 table — the SAME cause, seen from the side

The raised classes are 0 and 3, which is where the sample line falls
(`ngx = (px+0.5)/4`, so an integer grid coordinate sits at texel `4k - 0.5`).
Shifting the texel->grid mapping by a quarter of a grid step moves the class
pattern (`0.0462 0.0236 0.0234 0.0461` -> `0.0428 0.0204 0.0241 0.0507`) and
raises MOD to 0.959, so the roughness is locked to the SAMPLE LINES, not to the
texel grid. It is not a separate defect: it is the same crease, and it is what
the 2026-09-07 table was already looking at.

**Verdict: (a). (b) is dead with a number. (c) is (a) restated.**

---

## 3. The fix, the after-numbers, and what it does NOT do

### 3.1 What changed

`src/lodgen.cpp`, the `heightAt` lambda inside `lodgenBakeTerrainTextures`'s
`_msn` loop (now ~line 5098). One expression: the bilinear interpolation
parameters `tx` and `ty` are passed through the quintic ease
`t^3(t(6t-15)+10)` before the blend. Nothing else; no new key, no new argument,
no new panel row. **It changes every terrain `_msn` in every worldspace** — the
brief asked that this be said, and the comment at the site says it too.

Line endings: `src/lodgen.cpp` was 0 CR / 8,200 LF before and 0 CR / 8,254 LF
after (Python byte count, not grep). Still LF-only.

The stale instruction two comment blocks above — *"Do not 'improve' this to a
smoother basis without first removing the 8-unit quantisation"* — was amended
rather than left standing, because the staircase measurement above supersedes
it and an unamended comment is a trap.

### 3.2 Build

`make -j2` exit code **0** (the chain gates on `make`'s own `$?`, not on grep).
`release/NifSkope.exe` 2026-09-09 13:42:03, newer than `src/lodgen.cpp`;
`res/style.qss` and `release/style.qss` compare equal. `Fallout4.exe` was down
at the build. **bungo's open NifSkope window, if any, predates this: the next
launch of `release\NifSkope.exe` has it.**

### 3.3 Regenerated, and the scoping control

```
release/NifSkope.exe -no-gui lodgen \
  "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" \
  --worldspace 3C --terrain-region -60 36 -57 39 --dim 4 --no-terrain-identity \
  --out-dir  E:/.../scratchpad/mountains_20260907/images/ours4_fix \
  --tex-dir  E:/.../images/ours4_fix/textures/terrain/Commonwealth \
  --data-root "E:/Tools/Fallout 4/DataUnpacked/Data"
```

md5 against the pre-fix generation: the **`.BTR` is byte-identical**
(`6607b185a4e317658bc9d92e05917ba0`) and so is the **diffuse**
(`7a189887ce2af8d5060e38b0aa3c129a`). Only the `_msn` moved. Exactly one thing
changed, and it is the thing under test.

### 3.4 After-numbers, `Commonwealth.4.-60.36`, mip 0

| | MOD (grid-phase roughness) | hf energy | slope-residual rms |
|---|---|---|---|
| vanilla shipped | 0.296 | 16.406 | 0.508 |
| ours BEFORE | 0.809 | 2.922 | 0.0699 |
| **ours AFTER** | **0.637 (−21.3%)** | **3.387 (+15.9%)** | 0.0831 (+18.9%) |
| the generator's own bytes, BEFORE the codec | 0.947 -> **0.209** (−78%) | 2.681 -> **3.365 (+25.5%)** | |

**Both gates in the brief are met on the part the generator controls**: the
grid-phase peak falls below vanilla's (0.209 against 0.296) and the
high-frequency energy did not fall — it rose by a quarter. On the shipped,
compressed file the fall is 21%, because most of what is left is the block
codec: the amplitude-matched codec floor (vanilla's own field scaled to our rms
and put through the same BC1) reads **0.515 paired / 0.595 independent**, and
0.637 sits at it where 0.809 sat above it.

### 3.5 THE PICTURE, AND THE PART THAT IS NOT FIXED

**`scratchpad/mountains_20260907/images/mountain_peak_lattice_before_after.png`**
— our chunk both halves, the close-up's own pinned camera and crop.

**The square pattern is reduced, not gone.** I am not calling this fixed, and
the picture shows why. What remains was measured rather than guessed:

| sheet | % of 4x4 blocks that are a CONSTANT colour (R / G / B) |
|---|---|
| vanilla shipped (BC3) | 0.1 / 3.3 / 2.6 |
| ours BEFORE (BC1) | 21.5 / 36.3 / 25.1 |
| ours AFTER (BC1) | **16.0 / 28.8 / 18.0** |
| ours AFTER, before the codec | 0.1 / 0.1 / 0.1 |

A constant 4x4 block **is** a 4-texel square. The generator hands the encoder a
sheet with 0.1% flat blocks and the encoder returns one with 16–29% flat, and
it does that because there is almost nothing to encode: our high-frequency
slope residual is **9–11 8-bit code steps** against vanilla's **65** (rms 0.070
against 0.508). Two controls place the blame:

* it is **not our BC1 encoder being weak** — the same input through a plain
  principal-axis reference encoder gives rmse 2.84 bytes and 0.3% fully-flat
  blocks against `lodgenWriteDds`'s 3.70 and 1.0%; both collapse for the same
  reason;
* it is **not only the codec** — rendered with an UNCOMPRESSED `_msn` (a
  32-bit DDS written by `ddswrite.py`, staged the same way) the pattern is
  still there, and the shaded grid statistic is 0.252 against vanilla's 0.085.
  Underneath the codec the sheet still holds **129x129 samples over 512x512
  texels**: 16 texels per height sample, and a smooth field magnified 4x reads
  as cells however it is interpolated.

So the square pattern has **two** causes and this lane fixed the first:

1. the C0 crease at each sample line — **found, measured, fixed** (−78% on the
   generator's own bytes);
2. **too little detail to fill 512x512** — the codec then flattens blocks and
   the 4x magnification of a 129-sample field shows. That is the SAME open
   thread as *"Still blurry, vanilla is superior in quality"* and lane MSN's
   6.4x, and it is bungo's outstanding decision (reuse vanilla's `_msn` where
   terrain is unchanged), not a generator bug.

### 3.6 Gates: run, skipped, and PENDING

* **PENDING, with a reason: `tests/spells/lodgen_terrain.sh`** — the only gate
  this change reaches (it bakes chunk (-20,24) and decodes the `_msn`,
  including the `UPCH = G` up-channel check, 22 checks). It launches
  `release/NifSkope.exe`, and **`Fallout4.exe` (pid 27604) came up mid-lane**
  and is still up. Per CONSTITUTION 6 the lane ends with this gate pending; it
  must be run before this change is called landed. The change does not touch
  the channel order, the format or the mip count, so it is expected to pass —
  expected, not measured.
* **Skipped, and why:** `lodt_write.sh`, `lodt_btd.sh`, `lodgen_identity.sh`,
  `lodgen_merge.sh`, `lodgen_farring.sh`, `lodgen_texture_arrays.sh`,
  `lodgen_card_arrays.sh`, `lodgen_octahedral.sh`, `lodgen_impostor_cards.sh`,
  `lodgen_resources.sh`, `lodgen_water_subdiv.sh`, `lodgen_tree_sway.sh`,
  `lodgen_ground_cover.sh`, `lodgen_terrain_vt.sh`. None of them reads the
  terrain `_msn` bytes: the `.lodt`, the heightmap, the manifest, the merge, the
  atlas and the card paths are all upstream or on another writer. The
  byte-identity gates compare a rebake against a fresh rebake (determinism), so
  a changed sheet does not move them.
* **Done instead of a gate, and it is stronger for this change:** the
  regeneration itself, with the `.BTR` and diffuse md5s proving nothing but the
  sheet moved, and the replica-vs-shipped agreement (1.85 bytes mean) proving
  the offline model is the generator.

### 3.7 A SECOND `_msn` WRITER, NOT TOUCHED, AND IT HAS BOTH OLD BUGS

Found while locating the code, reported rather than changed because it is a
different path with its own gate and this lane measured nothing about it.
`lodgenBakeVtTile` (`src/lodgen.cpp` ~6104-6117, the virtual-texture pyramid
path reached from ~6692) writes its own `_msn`, and it still has **both**
defects the 2026-09-07 round fixed in the chunk path:

* it samples the height grid with `int( ngx )` — **nearest, not interpolated**;
* it writes `nrm[0] << 16 | nrm[1] << 8 | nrm[2]`, i.e. **north in green and up
  in blue** — the conventional order, which is the exact channel swap measured
  at 67.7% of the light and 92.0% of the shading variation.

Recommend a follow-up lane. I did not change it: one change per variant, and
"nothing is stated as a cause without a measurement".

---

## 4. Mistakes

All three are in `MISTAKES.md` at the repo root, written when recognised.

1. **Reached for a metric that could not see the artefact, and only found out
   from the control.** The spectral comb read the KNOWN-POSITIVE (the pre-fix
   blocky sheet) at 0.19, *below* a broadband field, because a zero-order hold
   has spectral zeros exactly at the comb bins. Cost one round. Rule: run the
   known positive through the metric before the subject; if it reads negative,
   change the metric.
2. **Edited `src/` and built before showing in a PICTURE that the candidate fix
   removed what bungo was looking at.** The sheet number was decisive and the
   edit stands on it, but the after-render still shows the pattern because a
   second cause was in the frame the whole time. The uncompressed-DDS writer
   that would have shown this in ten minutes was built AFTER the build.
   CONSTITUTION 5. Rule: localise -> replicate offline -> render the candidate
   sheet -> then edit `src/`.
3. **Quoted a screen scale before measuring it.** Estimated ~2.5 px per texel
   from the terrain's bounding box; the measurement (the same file rendered
   twice with the look-at shifted 512 world units, cross-correlated on one
   patch) gave 1.25 in that patch. Two rounds of reasoning were built on the
   wrong figure. Rule: a projection scale is measured with a known
   displacement, in the same patch.

Not mistakes, recorded so they are not read as such: the render driver refused
four renders when `Fallout4.exe` came up — its guard doing its job; and the
Catmull-Rom / B-spline / pre-blur variants were tried and rejected with numbers,
which is the method working.

## 5. Finished-work skill review

**Loaded and used:** `ww-control-calibration` (the whole shape of section 1.3 —
known answers first, a floor that carries the subject's own amplitude through
the same lossy stage, an independent phase-randomised twin, a ceiling from the
same data, a pre-registered separation gate; it is also what made the failed
comb metric *visible* rather than silently believed), `nifskope-ww-lodgen`
(the `--dump-land` layout, the build incantation, the LF-only editing trap, the
gate list), `nifskope-ww-build-verify` (the gated chain on `make`'s own exit
code, exe-newer-than-source, the stylesheet compare, and the line about bungo's
open window), `nifskope-ww-vanilla-compare` (staging each side as its own
miniature data root, the pinned camera, `--no-terrain-identity`, `compose.py`,
and — a direct hit — its section 7, which already documents that the lit path
discards the sheet's blue and recomputes it, so I did not have to rediscover
that or wrongly blame it).

**Written this lane:**
`E:\Projects\Claude\.claude\skills\ww-artefact-localise\SKILL.md` — *localising
a repeating visual artefact before touching `src/`*. It is the procedure this
lane had to invent and that mistakes 1–3 all came out of, and it will recur the
next time bungo says "ours looks wrong": the four-frame mesh/sheet substitution
matrix under one pinned camera, the uncompressed and constant-sheet controls
(with the DDS writer, `scratchpad/mountains_20260907/ddswrite.py`, which no
skill had), why a spectral comb fails on a grid-locked artefact and what the
phase-conditional statistic is, the synthetic 2x2 that separates two candidate
causes without a build, the look-at-shift calibration of screen pixels per
texel, and the rule that the candidate output is rendered from the offline
replica BEFORE the generator is edited. **Both skill trees need it** — it was
written to the live tree `E:\Projects\Claude\.claude\skills`; the director
should mirror it into `<repo>\.claude\skills` (CONSTITUTION 1a, the two trees
drift).

**Wished for and declined:** a skill for *replicating a `lodgen` writer offline
in numpy and proving the replica against the shipped bytes*. It was the single
highest-leverage thing here — every intervention in section 2 ran in seconds
instead of a four-minute build, and the 1.85-byte agreement is what licensed
them. I declined to write it as its own skill because the replica is specific
to one writer and would be re-derived anyway for a different one; the
transferable parts — prove the replica against the shipped file first, and use
it to render candidate output before editing — are section 5 of
`ww-artefact-localise`. If a second writer gets replicated this way, that
judgement was wrong and the skill should be written then.

**Not amended, deliberately:** `nifskope-ww-vanilla-compare`. Its section 7
already covers substituting a constant sheet; the new material is diagnosis
rather than comparison, and folding it in would have made a long skill longer
for readers who only want the picture.
