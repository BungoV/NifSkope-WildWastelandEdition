# Lane NATIVEVIEW2 — model-space normals for the FO4 program

2026-09-16. Renderer only: `res/shaders/fo4_default.frag` and `src/gl/renderer.cpp`.
No writer change, no bake-output change, no defaults change.

**The launch exe, checked with `ls` before the rung copy:**
`release/NifSkope.exe` — 2026-09-12 21:52 (`Sep 12 21:52`), 22,280,192 bytes. That is
DEFAULTS1's exe, exactly as the brief said. The rung copy
`release/NifSkope.before_nativeview2.exe` is the same 22,280,192 bytes,
md5 `d3deb7cefa7999fb918cc81e12912937`, and every BEFORE number below was
measured on it.

---

## 0. The convention

### What the sheet's channels mean

`scratchpad/nativeview2_20260912/work/msn_convention.py` decodes six of Bethesda's
own shipped sheets —
`E:/Tools/Fallout 4/DataUnpacked/Data/textures/terrain/Commonwealth/Commonwealth.16.*_msn.DDS`,
512x512 DXT5, 10 mips, 144 such tiles on disk — with a BC1/BC3 decoder re-typed
from the format (it shares no code with our writer), and correlates each channel
against the heights of the SAME cells, read out of
`Commonwealth_fine.HeightMap.-96.-96.95.95.-8320.44872.dds` (R16_UNORM 6144x6144,
pixel = height/8 + 32767, row 0 = north, 32 samples a cell = one texel per
128-unit LAND sample, so the sheet lines up with the heightfield 1:1).

A heightfield's unit normal is `n ~ ( -dh/dx, -dh/dy, 1 )` normalised with +x east
and +y north, so a channel that carries "east" must correlate with `-dh/dx` and
not with `-dh/dy`. All six (channel, axis) pairings were printed, so the wrong
ones refute the right one.

```
--- per-channel means, vanilla sheets (0..255) ---
Commonwealth.16.-16.-16_msn.DDS   127.7   238.7   127.9   255.0   DXT5 512x512 mips 10
Commonwealth.16.-32.48_msn.DDS    125.0   232.1   122.3   255.0
Commonwealth.16.-64.16_msn.DDS    142.0   224.4   126.4   255.0
Commonwealth.16.-96.-96_msn.DDS   126.6   251.8   126.6   255.0
Commonwealth.16.16.-64_msn.DDS    123.8   232.8   127.4   255.0
Commonwealth.16.48.-32_msn.DDS    126.6   251.8   126.6   255.0
MEAN OF THE ABOVE                 128.6   238.6   126.2   255.0

--- correlation against the heightfield of the SAME cells ---
tile                   R:-dh/dx R:-dh/dy B:-dh/dx B:-dh/dy     G:nz     A:nz
16 -16,-16                0.488   -0.002    0.010    0.559    0.151    0.000
16 -32,48                 0.586   -0.026   -0.025    0.592    0.044    0.000
16 -64,16                 0.722    0.089    0.106    0.837    0.399    0.000
16 -96,-96                0.000    0.000    0.000    0.000    0.000    0.000
16 16,-64                 0.391   -0.054   -0.081    0.541    0.049    0.000
16 48,-32                 0.000    0.000    0.000    0.000    0.000    0.000
MEAN                      0.364    0.001    0.002    0.422    0.107    0.000

ROW-ORDER CONTROL (sheet NOT flipped; must be markedly worse):
  R:-dh/dx 0.060   B:-dh/dy -0.063
```

Read plainly:

* **R is EAST (+x)** — 0.364 against `-dh/dx`, 0.001 against `-dh/dy`.
* **B is NORTH (+y)** — 0.422 against `-dh/dy`, 0.002 against `-dh/dx`.
* **G is UP (+z)** — it is not a gradient, so it has no signed partner to
  correlate against; its evidence is its mean, 238.6 of 255, i.e. decoded
  +0.87, which is what an almost-vertical terrain normal looks like. Its 0.107
  against nz is weak because nz is near 1 everywhere and barely varies.
* **Alpha is a constant 255 on every tile** and carries nothing.
* **Texel row 0 is NORTH.** The unflipped control collapses to 0.060 / -0.063,
  a tenth of the flipped numbers, which is the refuter for the row order.
* Range is 0..255 mapping to -1..+1 (`v * 2 - 1`); no sign flip on any channel.

Two of the six tiles (`-96,-96` and `48,-32`) read exactly 0.000 across the board
and share identical means 126.6 / 251.8 / 126.6. Those are flat or empty tiles —
a constant sheet has no variance to correlate, so they contribute 0 to the mean
rather than evidence. The four tiles that carry terrain all agree.

This matches our own writer (`lodgenTerrainMsnPixel`, `src/lodgen.cpp:5620-5626`,
packs R east, G up, B north) and it matches `res/shaders/sk_msn.frag`, the
Skyrim-era model-space path in this tree, whose comment says "Swizzled G/B
values!" and which does `normal.rbg` — the same reordering to (x, y, z) = (R, B, G).

### What FO4CS's shader says — it does not say anything

The brief asked for the convention read off FO4 Community Shaders' shader as
well. **That citation is not available and this is the honest statement of it.**
`E:/Projects/Fo4CommunityShaders/fallout4-community-shaders` contains exactly one
mention of `_msn` in the whole tree — `docs/RE/far-field-terrain-lod.md:968`,
which only counts the files — and no shader anywhere in it decodes their
channels. So section 0 rests on the vanilla measurement above plus this tree's
own `sk_msn.frag`, and on nothing from FO4CS.

### Why a tangent frame is the wrong thing to do with it

`src/btdterrain.cpp:300-344` builds each sheet-lit tile's tangent frame as
`T = n x worldUp` (falling back to `(1,0,0)`), `B = n x T` — an arbitrary frame
chosen for convenience, not one that matches any texture. Read as a tangent-space
map, the sheet's stored R and G become offsets along that arbitrary tangent and
bitangent: with G near 255 the decoded `normal.g` is ~+1.0, `dot(normal.rg,
normal.rg)` exceeds 1, the recomputed blue channel clamps to 0, and the shading
normal ends up lying flat along the mesh's bitangent — pointing sideways, not up.
That is the mechanism behind the blotches, and the FLAT-tile control in gate (b)
is what proves it: a sheet that says "straight up" everywhere produced the same
blotches as the real one.

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

The spell was RUN, not just written. On `release/NifSkope.exe` 12:15 it returns
**14 checks, 0 failures, 0 skips, PASS** (`release/ww_native_lighting.log`, copy
kept at `work/native_lighting_new2.txt`). Its first run FAILED all four of gate
(a)'s legacy frames, and the cause was a defect in the spell rather than in the
code under test — section 5 carries it.

---

## 2. Build and chain

### The game check

`tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` was run as its own
command and read before every build and before every exe launch in this lane.
It returned **rc=1** every time — no Fallout 4, no NifSkope, including none of
bungo's own. Nothing was ever killed. The exe was renamed aside at link time
(`release/NifSkope.before_nativeview2.exe`) and is still on disk. **No build
mutex was created and none was left behind**, as the charter instructed.

### The exe at launch, verified before the rung copy

The charter said the exe on disk would be DEFAULTS1's, 2026-09-12 21:52:25,
22,280,192 B. `ls` before the rung copy read exactly that, and the rung copy
still carries those bytes:

```
-rwxr-xr-x 22280192 Sep 16 11:40 release/NifSkope.before_nativeview2.exe
```

(the mtime is the copy's own; the size is DEFAULTS1's to the byte).

### The chain, each link with its evidence

| link | evidence |
|---|---|
| game check its own command, before build and before launch | rc=1 every time |
| rename aside, never kill | `release/NifSkope.before_nativeview2.exe` on disk, 22,280,192 B |
| `make -j2` gated on **make's own rc** | `BUILD-RC=0` (the skill's one-liner: rc is captured before any grep) |
| exe newer than EVERY changed file | 172 files under `src res tools tests` from `git status --porcelain`; the exe is newer than **170**. The two exceptions are `tests/spells/native_lighting.sh` (12:18) and `tests/spells/native_lighting_check.py` (12:03) — the gate itself, never compiled and never linked. Every file under `src/` and `res/` is older than the exe. |
| stale-object check for every header touched | **this lane touched no header.** `src/gl/renderer.cpp` and `res/shaders/fo4_default.frag` only. As a belt-and-braces run, `GeneratedFiles/.obj/renderer.o` (11:54:43) was checked against every header `renderer.cpp` includes: **0 stale**. |
| `make -n` zero compile lines | `make -n` rc=0, 4 lines of output, `Nothing to be done for 'first'`; **0** lines matching `g++`/`gcc`/`moc`/`uic`/`rcc` |
| `cmp` the stylesheet copy after the link | `cmp res/style.qss release/style.qss` — **identical**, 11,097 B |
| `cmp` the shader copy after the link | `cmp res/shaders/fo4_default.frag release/shaders/fo4_default.frag` — **identical**, 21,646 B |

**Exactly one object recompiled** — `GeneratedFiles/.obj/renderer.o`, at
11:54:43, from `src/gl/renderer.cpp` at 11:54:26. So the exe on disk is the
exe that was there at launch plus this lane's diff and nothing else.

### The exe on disk now

```
-rwxr-xr-x 22288896 2026-09-16 11:54:47.914509800 +0200 release/NifSkope.exe
```

**22,288,896 B, 2026-09-16 11:54:47** — 8,704 B larger than the rung.

### The existing harnesses

| harness | before (rung exe) | after (new exe) | verdict |
|---|---|---|---|
| `tests/spells/render_shot.sh` | 82 checks, 0 failures | **82 checks, 0 failures** | count kept, PASS |
| `tests/spells/lodl_open.sh` | 23 checks, 0 failures | **23 checks, 0 failures** | count kept, PASS |
| `tests/spells/native_open.sh` | **14 checks, 1 failure, 2 skipped, FAIL** | **14 checks, 1 failure, 2 skipped, FAIL** | count kept; the failure is **pre-existing** |
| `tests/spells/native_lighting.sh` (new) | — | **14 checks, 0 failures, 0 skips, PASS** | floor 14, measured |

**`native_open.sh`'s failure, run on both exes and settled by measurement.**
It first came back FAIL on the new exe, which is the sort of thing that should
be treated as red until proved otherwise. It was proved otherwise: the rung exe
was run against the same spell, with the pre-change fragment shader restored in
`release/shaders/` so the rung ran as it did at 11:40 (that restored file is
`work/fo4_default.rung.frag`, and it comes back at **20,045 B / 547 LF**, the
before-size to the byte, so the reconstruction is exact). The rung fails the
**same check with the same number**:

```
RUNG  IOU 0.8179   FAIL the .lodi scene covers the same pixels as the .BTO (IoU 0.8179 >= 0.95)
NEW   IOU 0.8179   FAIL the .lodi scene covers the same pixels as the .BTO (IoU 0.8179 >= 0.95)
```

It is a silhouette-coverage check on OBJECTS — the `.lodi` scene against the
chunk's own `.BTO`. It has nothing to do with normals: both frames are drawn by
shapes the census reports as `msn=0`, and gate (a) already showed those frames
byte-identical across the two exes. **It is somebody else's red, it was red
before this lane started, and this lane did not touch it.** It belongs to
whoever owns native object placement; section 4 lists it as owed.

The same run turned up something worth keeping, in `native_open`'s own gate (d),
which correlates the lit `.lodl` against the `.BTR` of the same cells:

| `native_open.sh` (d) | rung | new |
|---|---|---|
| NCC, lit terrain vs the `.BTR` of the SAME cells (bar 0.45) | 0.6008 | **0.8583** |
| mean abs colour difference (bar 48) | 35.821 | **21.132** |
| NCC against a DIFFERENT chunk's `.BTR` (refuter) | 0.2023 | 0.2303 |
| NCC against the same `.BTR` mirrored in Y (floor, bar 0.10) | -0.0242 | -0.0477 |

That gate was written by an earlier lane, with its bar and its two refuters
already fixed, and it was not part of this lane's registered set. It says the
native terrain now looks substantially MORE like Bethesda's own `.BTR` of the
same ground than it did — 0.60 to 0.86 — while both refuters stay where they
were. It is corroboration from a gate this lane did not design, which is the
kind worth more than the kind you design yourself.

---

## 3. Pictures

Six panel sheets, written to
`scratchpad/nativeview2_20260912/images/`. Every one was composed by
`work/compose.py`, every label is BURNED INTO the image (the panel's title bar
and its one-line caption, plus the sheet's own title and note), every source
frame was taken with `WW_RENDER_CLEAN=1` so the background is the flat
(43,45,49) and not the viewer's gradient, and **every size below was read back
from the file with PIL** after it was written, not quoted from the request.

| path (under `scratchpad/nativeview2_20260912/images/`) | size, read back | bytes |
|---|---|---|
| `i_native_top_rung_vs_new.png` | 2164 x 718 | 1,647,084 |
| `i_native_obl_rung_vs_new.png` | 2164 x 718 | 692,137 |
| `ii_terrain_own_vs_flat_top.png` | 2164 x 718 | 1,567,773 |
| `ii_terrain_own_vs_flat_obl.png` | 2164 x 718 | 546,442 |
| `iii_slope_known_answer_oblique.png` | 2164 x 718 | 529,021 |
| `iv_slope_top_control.png` | 2164 x 718 | 1,063,183 |

**(i) NATIVEVIEW1's picture re-shot, four panels per view.** Native terrain and
objects together, at the top view and at the oblique, chunk (-20,24) dim 4,
orthographic half-width 8192. The four panels are, left to right: the LEGACY
`.BTR` control, the LEGACY `.BTO` control, the NATIVE scene on the rung exe, the
NATIVE scene on the new exe. The two control panels are byte-identical on both
exes, so they are shown once. The two native panels draw the SAME `.lodl`
terrain and the SAME `.lodi` objects from the same bake with the same camera —
only the exe differs.

**(ii) Terrain alone, its own normal tiles against FLAT tiles.** Four panels:
rung/own, rung/flat, new/own, new/flat, at each view. This is the picture of
the refuter. On the rung the first two panels are the same picture, which is the
defect stated as a photograph: a normal map that cannot change the render is a
normal map nobody is reading. On the new exe they differ, and the flat arm is
lit evenly.

**(iii) and (iv) the slope test.** (iii) is the oblique: the west tilt, the flat
sheet and the east tilt on the new exe, then the rung's east tilt as the
refuter, with each panel's mean luma burned in beside the `N.L` the arithmetic
predicted the ORDER from. (iv) is the top-view control, where the east and west
tilts must be the same picture and on the new exe are byte-identical, against
the rung where they differ by a mean of 47.06 luma.

**What the pictures are not.** They are 1024-wide frames scaled to 512 in the
panels, so they are for judging shape and shading, not for reading a pixel
value; every number in this report comes from the full-size PNGs in `work/`,
never from a panel. Per the charter, **nothing was sent to bungo from this
lane** — the director sends.

---

## 4. Owed / red / bungo's calls

### RED — nothing this lane can close

1. **`native_open.sh` fails one check, and failed it before this lane started.**
   `the .lodi scene covers the same pixels as the .BTO (IoU 0.8179 >= 0.95)`.
   Run on both exes, same number to four decimals. It is an OBJECT placement or
   object-culling question, not a lighting one. This lane leaves it exactly as
   it found it and does not claim it. It needs an owner.

### BUNGO'S CALLS — measured, deliberately not acted on

2. **The legacy `.BTR` is on a different program, and the HANDOFF's explanation
   for it was wrong.** The HANDOFF said the `.BTR`'s darkness was "one fixed
   frame". The census says otherwise:

   ```
   # WW_PROGRAM_CENSUS  light(view) = 0 0 1
   shape="Land"             bsver=130 msn=1 lodland=1 prog=sk_msn.prog
   shape="Terrain -20,24"   bsver=130 msn=1 lodland=0 prog=fo4_default.prog
   shape="obj-at" / "obj"   bsver=130 msn=0 lodland=0 prog=fo4_default.prog
   ```

   The `.BTR`'s `Land` shape is Shader Type 18 (`ST_WorldMap4`), which
   `res/shaders/fo4_default.prog` excludes by condition, so the program scan
   hands it to `res/shaders/sk_msn.prog` — **a model-space path already, the
   Skyrim one.** It was never on the broken path. That is why its dark fraction
   is 16.44% before and 16.44% after, to the hundredth, in both views.

   So the brief's gate (c) asked for something that cannot happen: "the same
   view of the legacy `.BTR` moves too". **It does not move, and the reason is
   measured rather than guessed.** I did not make it move. Making it move means
   routing Shader Type 18 to `fo4_default.prog`, or changing `sk_msn.prog`, and
   either is a renderer routing decision with a blast radius well outside this
   lane's brief — every Skyrim model-space shape in the tree rides the same
   program. **This is bungo's call, and it is the one thing in this lane I would
   ask him about first.** The open question underneath it is whether the two
   model-space paths agree with each other on brightness at all; that was NOT
   measured here.

3. **The FO4CS citation the brief asked for does not exist.** The brief asked me
   to read the channel convention off FO4CS's shader and cite it with numbers.
   `E:/Projects/Fo4CommunityShaders/fallout4-community-shaders` contains exactly
   **one** mention of `_msn` — `docs/RE/far-field-terrain-lod.md:968`, which is a
   count of files, not a decode — and no shader anywhere in the tree that reads
   those channels. There is nothing there to cite. The convention in section 0
   therefore rests entirely on the other leg the brief asked for: six of
   Bethesda's own shipped sheets, decoded and correlated against the heights of
   the same cells, with a row-order refuter that collapses the statistic. That
   leg is strong on its own, but the brief asked for two and got one, and I am
   naming the gap rather than dressing the one up as two.

### OWED

4. **Nothing is committed.** Per the charter: no commit, no `git stash`. The
   working tree carries this lane's four modified files and four new ones, listed
   in `CHANGED_FILES.txt` with byte counts before and after. Committing is
   bungo's word, by explicit path list.

5. **The change has not been seen in bungo's own window.** Everything here is
   headless render-shot evidence and harness counts. If his NifSkope is open it
   is running the old exe and needs a restart to show any of this.

6. **The `.BTR`/`.lodl` brightness comparison is not a like-for-like one and is
   not made.** They are different programs (point 2). Where a picture puts them
   side by side it is labelled a CONTROL, not a comparison.

7. **Gate (b)'s dark-mask IoU went degenerate and its replacement is honest
   about that.** After the change neither arm has a pixel under luma 40, so the
   IoU that the brief registered (0.86 on the rung) has nothing left to measure
   and reads 0.000. That number is not evidence of anything by itself. The
   threshold-free companion — darkest-fifth IoU, 0.858 to 0.705 — is what the
   gate actually tests, together with the block SD of the own-minus-flat
   difference, and both are in `native_lighting.sh` with their rung values beside
   their floors.

8. **The terrain in this chunk is not steep, so the absolute effect is modest.**
   Cells (-20,24)..(-17,27) are gentle ground: the real normals there sit close
   to "up", so a flat sheet is a fair approximation of them and the two arms
   differ by a mean of only 6.14 luma. What the rung got wrong was the
   DIRECTION, not the magnitude, and gate (d) is where that is shown on a
   fixture with a known answer. **A steep chunk was not tested.** If bungo wants
   the effect shown at its largest, the lane to run is the same one on
   mountainous cells.

### NOT MEASURED, stated plainly

9. No PBR renderer work was done and none was looked at (standing order).
10. No writer changed, no bake output changed, no default changed. `lodgen`'s
    own gates were not re-run because nothing this lane touched can reach them —
    `src/btdterrain.cpp` was READ, not modified, in the end: the flag it already
    sets turned out to be correct and the defect was entirely on the viewer side.
11. Nothing was measured in the game. This is a viewer lane.
12. The tone map was not modelled. Every gate is built on ORDER and EQUALITY,
    never on a ratio; section 1's gate (d) says why and gives the number that
    would have made a ratio wrong (the flat arm's top/oblique luma ratio is
    0.877 where `N.L` alone would say 0.444).

---

## 5. Mistakes

Both are in `MISTAKES_ENTRIES.md` and were spliced to the TOP of the root
`MISTAKES.md` the moment they were recognised, not at the end of the lane.

1. **I wrote a gate that fed the exe paths it could not open, then read the
   resulting picture as a failure of the code under test.**
   `tests/spells/native_lighting.sh` built `WW_LODGEN_RESOURCES` from
   `ROOT="$(cd "$(dirname "$0")/../.." && pwd)"`, which is `/e/Projects/...`, and
   joined two of those with a semicolon. MSYS2 rescues a single argv or env path
   automatically but never a semicolon-joined LIST — and that is written out in
   full, with the reason, at the top of `tests/spells/_harness.sh`, which this
   spell sources on its first line. The object textures never loaded and gate
   (a) reported all four legacy frames as differing from their baselines:
   `legacy_bto_top.png` at 390,854 B against the baseline's 537,794 B, mean
   |dColour| 30.93 over 1,012,736 pixels.

   What found it was not inspection. I rendered the same frame three times and
   got the same wrong bytes each time — so the render was deterministic and the
   difference had to be in the INPUT — and the census file showed the same two
   shapes on the same program in both runs, which ruled out the change under
   test. After converting each half with `_harness.sh`'s own `winpath`: 14
   checks, 0 failures.

   THE RULE, and it is the one I will carry out of this lane: a harness that
   fails on its FIRST run has two suspects and the harness is the one you wrote
   five minutes ago. Render it twice before blaming the exe. And when a file you
   source carries a warning about exactly the shape of value you are building,
   that warning is addressed to you.

2. **I wrote a file-scope helper as if it were a class member.** The first build
   failed: `error: 'BSLightingShaderProperty* Shape::bslsp' is protected within
   this context`, `src/gl/renderer.cpp:136-138`. `Shape` befriends `Renderer`'s
   MEMBERS, not every function in `renderer.cpp`. Cost one build. Fixed by
   reading the two flags inside `Renderer::setupProgram` — which is a member —
   and passing them to the helper as ints.

Two near-misses worth recording even though they did not become mistakes:

3. **A Bash heredoc halved my backslashes, twice**, writing `fix01.py` and a
   report section, both failing with `unexpected EOF while looking for matching
   quote`. `ww-anchored-hookup` §5a says exactly this in advance: anything
   carrying a backslash goes through the Write or Edit tool. I read that after
   the second failure instead of before the first.

4. **I nearly reported `native_open.sh` as this lane's red.** It failed on the
   new exe, and the truncated log did not name the failing check. The honest
   move was the one the brief's own gate list demanded — run the rung and
   compare — and it took reconstructing the pre-change fragment shader to do it
   properly. The reconstruction came back at 20,045 B / 547 LF, the recorded
   before-size to the byte, which is what made the comparison trustworthy rather
   than approximate.

---

## 6. Skill review

### Skills loaded, and which tree served them

**Every skill this lane used came from the repo tree**,
`E:/Projects/NifskopeWildWastelandEdition/.claude/skills/<name>/SKILL.md`. None
had to be fetched from `E:/Projects/Claude/.claude/skills/`.

| skill | what it was used for |
|---|---|
| `nifskope-ww-build-verify` | the whole build chain: game check as its own command, rename aside, `make`'s own rc, exe-newer-than-sources, the stale-object rule, `make -n`, the `cmp` of the link-time copies |
| `nifskope-ww-render-shot` | every frame in this lane; the absolute-path rule for every `WW_*`, `WW_RENDER_CLEAN`, `WW_WINDOW_AT`, `--port <unused>` |
| `ww-test-harness-add` | §5c, the count floor is the MEASURED green count and never a prediction; the exit-77 SKIP that is never a pass |
| `ww-anchored-hookup` | §5a, the Write-tool rule for backslashes (see mistake 3) |
| `ww-render-arm-isolate` | the own/flat/tilt/tiltw arm structure — one input changed per arm, everything else byte-identical |
| `ww-analytic-fixture-gate` | gate (d): a fixture whose answer is arithmetic before the render |
| `ww-module-off-is-identical` | gate (a) and the `forced == &emptyString` condition on the uniform |
| `ww-silhouette-compare` | the IoU statistics and the reason a coverage mask must be read back per frame |
| `ww-toggle-lit-gate` | the lit-vs-data-view distinction `native_open` (d) rests on |
| `nifskope-ww-lodgen` | the bake layout, `.lodl`/`.lodi`/`.BTR`/`.BTO`, the sheet cache |

### The skill that should have existed and did not

**`ww-msys-path-list`** — nothing in the tree says, as a skill, "a
semicolon-joined path list handed to a Windows exe from an MSYS shell is not
converted; convert each element first". The warning exists as a COMMENT inside
`tests/spells/_harness.sh`, where it is found only by someone already reading
that file top to bottom. I sourced that file and still made the mistake. A
comment in a sourced file is not a skill; it is a note to whoever is already
looking.

### The skill I wrote

I did **not** write a new skill file this lane. Instead the facts went where
they will be read at the moment they are needed, which the charter's amendment
asked for directly:

* `.claude/skills/nifskope-ww-render-shot/SKILL.md` gained
  **"The lighting a built LOD document is photographed under"** at the end of
  "Photographing a BUILT document" — the headlight moves with the camera, the
  two view-to-world light vectors with their numbers, why `diffuse = A + D *
  max(N.L, eps)` plus a tone map means a known-answer gate must be built on
  order and equality and never on a ratio, that terrain LOD is model-space-lit
  so a flat sheet cache is the control arm and not the subject, and to take
  `WW_PROGRAM_CENSUS` with every lighting picture.
* `docs/LODGEN_NATIVE_LODO_LODI.md` gained a **`## Lighting`** section in its
  Viewer part with the measured channel order, the bit-12 rule, the viewer path,
  and the measured fact that the legacy `.BTR` goes to `sk_msn.prog`.

**On reflection that was the wrong call for the path-list lesson**, and it is
the one thing I would do differently: the render-shot skill is about pictures,
and the MSYS list rule is about every harness in the tree. If bungo wants it, the
skill to write is `ww-msys-path-list`, short, with the measured before/after
byte counts from mistake 1 as its worked example.

### The tool this lane leaves behind

`WW_PROGRAM_CENSUS=<absolute path>` in `src/gl/renderer.cpp` is small and
general: one deduped row per `(shape, program)` pair — `shape`, `bsver`, `msn`,
`lodland`, `prog` — with the view-space light on the header line. It is what
turned "the terrain looks wrong" into "the terrain is on the program I thought
it was, and the `.BTR` is not", which is the finding in section 4 point 2 and
was not reachable by looking at pictures. It costs nothing when the variable is
unset.
