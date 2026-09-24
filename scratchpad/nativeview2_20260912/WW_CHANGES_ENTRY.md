## 2026-09-16 — Native LOD terrain is lit from its normal sheet: a model-space path in the FO4 shader

The native LOD terrain came out covered in dark blotches, and the blotches were
not in the colour tiles. The measurement that found the cause replaced every
normal tile with one that says "straight up" everywhere and got the same
blotches back — dark fraction 20.0 per cent against 20.1, the two dark masks
overlapping to IoU 0.86. A normal map that cannot change the picture is a normal
map nobody is reading.

A terrain LOD normal sheet is an `_msn`: a MODEL-space normal map, carrying all
three components in the model's own axes, and the shape says so by setting
Shader Flags 1 bit 12 (`SLSF1_Model_Space_Normals`). `res/shaders/fo4_default.frag`
had no model-space branch, so it read the sheet as a tangent-space map and sent
its "up" along whatever bitangent `src/btdterrain.cpp` had built — an arbitrary
frame (`T = n x worldUp`, `B = n x T`). The sheet's "up" landed sideways and the
surface shaded in blotches.

The fix is one branch. When bit 12 is set, the fragment shader transforms the
texel by the model matrix alone and by nothing else — no tangent frame, and the
blue channel is not recomputed because it is real data. The channel order was
measured, not assumed, on six of Bethesda's own shipped sheets
(`Commonwealth.16.*_msn.DDS`) correlated against the heights of the same cells:
**R = east (+x)**, correlation 0.364 against `-dh/dx` and 0.001 against `-dh/dy`;
**B = north (+y)**, 0.422 against `-dh/dy` and 0.002 against `-dh/dx`;
**G = up (+z)**, mean 238.6 of 255. Alpha is a constant 255 and carries nothing.
The same statistic with the sheet's rows NOT flipped collapses to 0.060 / -0.063,
which is the refuter for the row order.

`src/gl/renderer.cpp` sets the new `hasModelSpaceNormals` uniform from the
shape's own bit 12, gated on the same test that decided a real normal map was
bound — so with lighting or normal maps switched off the branch switches off
too, and `default_n` (a flat TANGENT texel) is never decoded as "north".

**The tangent-space path is untouched.** Every shape without bit 12 renders
byte-identical: the object `.BTO` of chunk (-20,24) matches to the byte in both
views, and so does the legacy `.BTR`.

What moved, measured on chunk (-20,24) dim 4:

* the dark fraction (luma under 40) of the native terrain at the oblique:
  **20.01% → 0.58%**; with objects in the frame, 14.18% → 0.35%; at the top view,
  5.08% → 0.00%
* the terrain's own normal tiles and FLAT tiles now make different pictures. On
  the old exe they were the same to IoU 0.861; the flat arm's large-scale shading
  (block SD) falls 31.91 → 15.04, below the 17.57 the same flat tiles give at the
  top view where the lighting is provably equal at every texel
* a slope fixture with a known answer: at the top view an east tilt and a west
  tilt have the same `N·L` and must be the same picture — they were 47.06 luma
  apart on average before, and are byte-identical now. At the oblique the luma
  must order west > flat > east; it did so on 46.23% of pixels before (chance)
  and on 100.00% of 311,795 pixels now
* `native_open.sh`'s own correlation of the lit terrain against Bethesda's
  `.BTR` of the same cells, a gate written by an earlier lane: **NCC 0.6008 →
  0.8583**, mean colour difference 35.821 → 21.132, both refuters unmoved

The legacy `.BTR` does not move at all, and the reason is measured rather than
guessed: its `Land` shape is Shader Type 18, which `fo4_default.prog` excludes
by condition, so the program scan hands it to `sk_msn.prog` — a model-space path
already, the Skyrim one. It was never on the broken path. Whether to route
Shader Type 18 to the FO4 program instead is a separate decision and was not
taken here.

New: `WW_PROGRAM_CENSUS=<absolute path>` writes which program lit which shape for
the frame just taken, one row per first-sighted (shape, program) pair, with the
view-space light direction on the header line. It costs nothing when unset, and
it is what turned "the terrain looks wrong" into "the terrain is on the program I
thought it was, and the `.BTR` is not".

Gate: `tests/spells/native_lighting.sh` (new, 14 checks, count floor 14 measured
on this build). `render_shot.sh` 82/0 and `lodl_open.sh` 23/0 keep their counts.
`native_open.sh` keeps its count at 14 checks / 1 failure / 2 skipped — that one
failure is a `.lodi`-vs-`.BTO` object coverage check which fails identically on
the build before this change (IoU 0.8179 both times) and is not this change's.

Renderer only. No writer changed, no bake output changed, no default changed.
