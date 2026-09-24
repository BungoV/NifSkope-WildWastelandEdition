
---

## 1. Gates

All four gates were pre-registered in the brief before the code was written.
Every number below is printed by
`scratchpad/nativeview2_20260912/work/gates.py` (output kept at
`work/gates_out.txt`), which reads the two shot directories `work/rung/`
(the exe before the change) and `work/new/` (after). Luma is
Rec.601 `0.299R + 0.587G + 0.114B` on the 8-bit frame; the covered mask is
every pixel that is not the `WW_RENDER_CLEAN` clear colour (43,45,49), read
back per frame. Every frame is **1024x989** — `WW_RENDER_SIZE=1024x1024` asked
for 1024 wide, and on this build the height rule takes 35 rows of chrome off.
All sizes were read back with PIL; none is quoted from the request.

### (a) every shape without Shader Flags 1 bit 12 renders byte-identical

Byte comparison of the same frames on the two exes:

| frame | result |
|---|---|
| `legacy_bto_top`, `legacy_bto_obl` (the object `.BTO` of chunk (-20,24)) | **IDENTICAL** |
| `legacy_btr_top`, `legacy_btr_obl` (the legacy terrain `.BTR`) | **IDENTICAL** |
| the 10 frames that contain a bit-12 `.lodl` tile | differ, as intended |

**4 identical, 10 differ.** The `.BTO` is the object case the brief named: its
shapes report `msn=0` in the census, so the new branch is switched off for them
and the old code runs unchanged. The `.BTR` is identical for a different
reason, which is section 4's finding.

The three existing harnesses were run on the new exe and kept their counts —
see section 2.

### (b) own normal tiles vs FLAT tiles must now DIFFER, and flat must be lit evenly

The FLAT arm replaces every `.n.DDS` texel with "straight up". The value was
read back after the 5/6/5 quantisation rather than quoted: asked 128,255,128,
stored `0x87F0`, **reads back 132,255,132** = east 0.0353, north 0.0353,
up 0.9988, i.e. 2.86 degrees from vertical.

| number, oblique, terrain alone | rung (before) | new (after) |
|---|---|---|
| dark-mask IoU, own vs flat (luma < 40) | **0.861** | 0.000 — neither arm has a dark mask left |
| darkest-fifth IoU, own vs flat (threshold-free) | 0.858 | **0.705** |
| mean abs luma difference, own vs flat | 3.69 | 6.14 |
| pixels differing by more than 8 luma | 9.00% | 26.02% |
| block SD of the own-minus-flat difference | 2.63 | **4.56** |
| flat-tile block SD (large-scale shading) | 31.91 | **15.04** |

"Block SD" is the standard deviation of 32x32 block means over the covered
mask, and it exists because plain luma SD over terrain is dominated by the
colour sheet's own detail, which no lighting change can remove. Averaging 1024
texels a block flattens the albedo and leaves the large-scale shading — which
is what a blotch is.

The dark-mask IoU reproduces the brief's registered refuter exactly: **0.861**
on the rung, against the 0.86 the HANDOFF recorded. After the change that
statistic becomes degenerate, because neither arm has any pixel under luma 40
left; the darkest-fifth IoU is its threshold-free companion and moves
0.858 -> 0.705. The own-minus-flat difference is the clearer statement: its
block SD nearly doubles (2.63 -> 4.56) while its per-pixel SD slightly falls
(9.49 -> 8.78), which is what "the difference became a real slope signal
instead of noise" looks like — a slope signal survives block averaging and
uncorrelated noise does not.

**The uniformity floor, measured and stated.** A flat sheet gives every texel
the same `N.L`, so the flat arm's remaining variation can only be the colour
sheet's. The floor is what the SAME flat tiles give at the TOP view, where the
lighting is provably identical at every texel: **block SD 17.57**. At the
oblique the flat arm gives **15.04**, below the floor with 2.53 to spare. That
comparison is conservative, because the oblique foreshortens the terrain so
each block covers more ground and therefore more albedo variation, not less.
On the rung the same oblique number was **31.91**.

The difference between the two arms is modest in absolute luma, and that is
expected rather than disappointing: the terrain in cells (-20,24)..(-17,27) is
not steep, so its real normals are close to "up" and a flat sheet is a close
approximation of them. What the rung got wrong was not the magnitude, it was
the direction — gate (d) is where that is shown with a known answer.

### (c) the dark fraction (luma < 40) of the native oblique terrain

| view | before | after |
|---|---|---|
| native terrain ALONE, oblique | **20.01%** | **0.58%** |
| native terrain + objects, oblique (NATIVEVIEW1's picture i) | 14.18% | 0.35% |
| native terrain ALONE, top | 5.08% | 0.00% |
| legacy `.BTR`, oblique | 16.44% | **16.44% — no move** |
| legacy `.BTR`, top | 9.57% | **9.57% — no move** |
| legacy `.BTO` objects, oblique | 0.54% | 0.54% — no move |

The 20.01% reproduces the 20.0% the brief registered. **The legacy `.BTR` does
not move at all, in either direction, and the reason is measured rather than
guessed** — see section 4.

### (d) a slope test with a known answer

Three synthetic sheets, each built by rewriting every BC1 block of the bake's
own `.n.DDS` tiles to one constant colour with zero indices, so nothing but the
normal texels changes. Every colour is READ BACK after quantisation:

| arm | asked | stored | read back | decoded unit normal | tilt |
|---|---|---|---|---|---|
| flat | 128,255,128 | `0x87F0` | 132,255,132 | east +0.0353, north +0.0353, up 0.9988 | 2.86 deg |
| tilt east | 191,238,128 | `0xBF70` | 189,239,132 | east +0.4827, north +0.0353, up 0.8751 | 28.94 deg EAST |
| tilt west | 64,238,128 | `0x4770` | 66,239,132 | east -0.4827, north +0.0353, up 0.8751 | 28.94 deg WEST |

**The arithmetic, stated before the render.** The default light is a headlight:
`frontalLight` is true (`src/glview.h:437-440`), so
`globalUniforms.lightSourcePosition[0] = (0,0,1)` in VIEW space — and the
census read it back from the running process as `light(view) = 0 0 1`. The
light's direction in world axes is the bottom row of `Matrix::fromEuler( Rot )`
(`src/data/niftypes.cpp:230-232`):

* `WW_RENDER_VIEW=1` (Top, rotation 0,0,0) -> `(0, 0, 1)`
* `WW_RENDER_VIEW=8` (ViewUser, the Blender startup rotation
  -63.5593, 0, 133.3081) -> `(-0.6516, +0.6142, +0.4453)`

so `N.L` is:

| arm | top view | oblique |
|---|---|---|
| flat ("up everywhere") | 0.9988 | **0.4434** |
| tilt EAST | 0.8751 | **0.0968** |
| tilt WEST | 0.8751 | **0.7258** |

Two things follow with no modelling at all, and they are the gate:

**d1 — "the same luma at the top view modulo the light's own angle."** At the
top view the east and west tilts have the SAME `N.L` (0.8751): the tilt
*direction* cannot matter there, so the two frames must be the same picture.

| | rung | new |
|---|---|---|
| `t_tilt_top` vs `t_tiltw_top` | **differ: max abs luma 81.18, mean 47.06** | **byte-identical, max 0.00** |

**d2 — "one whose sheet tilts +X darkens on the side the arithmetic says."** At
the oblique `N.L` is strictly ordered west 0.7258 > flat 0.4434 > east 0.0968,
and nothing else in the pixel changes between the three arms, so the luma must
order the same way under any monotone tone map.

| | rung | new |
|---|---|---|
| pixels where luma west > flat > east | **46.23% of 311,795 — chance** | **100.00%** |
| mean luma west / flat / east | 90.65 / 85.31 / 76.61 | 104.01 / 95.88 / 80.39 |

**What was NOT measured here, plainly.** The gate is built on order and
equality, not on a ratio, because the shader's diffuse term is
`A + D * max(N.L, eps)` with `A = sqrt(ambient) * 0.375` and a tone map after
it (`res/shaders/fo4_default.vert:62-64`). A frame's mean luma is therefore NOT
proportional to `N.L`, and no absolute brightness prediction is made or
claimed. The flat arm's mean luma is 109.26 at the top and 95.88 at the
oblique, a ratio of 0.877 where `N.L` alone would say 0.444; the difference is
the ambient term, the tone map, and the fact that the two views show different
pixels of the terrain. None of that was modelled.

### The new spell

`tests/spells/native_lighting.sh` (new) renders the eight terrain arms and the
four legacy frames itself and runs `tests/spells/native_lighting_check.py`
over them. Every floor in it carries the value measured on this exe AND the
value the rung gave, so the floor is visibly able to fire:

| check | floor | this exe | the rung |
|---|---|---|---|
| gate (a): four legacy frames byte-identical to their baselines | exact | identical | identical |
| gate (c): native oblique dark<40 | bar 3.00% | 0.58% | 20.01% |
| gate (c): flat-tile oblique dark<40 | bar 1.00% | 0.00% | 20.07% |
| gate (b): darkest-fifth IoU own vs flat | bar 0.800 | 0.705 | 0.858 |
| gate (b): own-minus-flat block SD | floor 3.50 | 4.56 | 2.63 |
| gate (b): flat oblique block SD no more than flat top | measured pair | 15.04 vs 17.57 | 31.91 vs 6.14 |
| gate (d): east-top and west-top byte-identical | exact | identical | max 81.18, mean 47.06 |
| gate (d): west > flat > east at the oblique | bar 99.00% | 100.00% | 46.23% |
| census: every `.lodl` terrain shape is msn=1 on `fo4_default.prog` | all | 4 of 4 | — |
| census: every `.BTO` object shape is msn=0 | all | 2 of 2 | — |
| census: the `.BTR` `Land` shape is on `sk_msn.prog` | exact | yes | yes |

The count floor is **14**, and it is the measured green count on this exe, not
a prediction (`ww-test-harness-add` 5c). The spell exits 77 with every missing
path named when the bake fixtures are not in the tree — a SKIP, never a pass.
