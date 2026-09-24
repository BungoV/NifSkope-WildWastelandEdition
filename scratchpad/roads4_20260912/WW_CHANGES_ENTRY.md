## 2026-09-12 — Road detail is on by default, and the terrain inside the road models is measured

`--road-detail` now defaults to **1.0**. bungo's ruling, over a side-by-side
picture: *"--road-detail 1 is always on, do not ever use road detail 0, that
looks terrible"*. The old look is one switch away — `--road-detail 0` still
reproduces the previous default bake **byte for byte**, every file, on both
test chunks.

The flip costs one harness bar and that is reported, not hidden:
`tests/spells/lodgen_roads.sh` R5 reads `after 0.3078` against `bar 2 = 0.3223`
and fails by 0.0145, where the old default read 0.3435. R5's bar was calibrated
when detail 0 was the default; detail 1 is measurably further from vanilla's
road-presence number and nearer to what bungo wants to see. The eye wins; the
bar is re-stated rather than met.

**What bungo saw and where it comes from.** He said: *"the issue with the roads
is, these meshes have some terrain included there, you can see the sharp mesh
terrain being included into the chunk's bake"*. He is right, and it is not a
skirt and not a shading bug. Fallout 4's road models carry shapes whose
material lives under `materials\Landscape\Ground\` — `CommonwealthDefault01`,
`DirtGravel01` — the verge and the junction fill, modelled inside
`Landscape\Roads\Sanctuary\SancRoadStr01.nif` and its siblings. In our bake
those shapes win **8,337 of 23,116** road texels on chunk (-20,20) (36.1 per
cent) and **2,756 of 11,069** on (-8,8) (24.9 per cent). Where such a patch
meets the asphalt the luminance step reads **25.28** and **13.13** levels
against vanilla's **2.53** and **1.76**, and the gradient across that boundary
reads **15.387** and **8.010** against vanilla's **5.362** and **5.138** (the
same texel sets displaced five ways read 6.077 and 4.805, which is the floor).

New question on the CLI: **`--road-ground-paint 0..1`**, how much such a shape
paints the sheet. It defaults to **1.0**, which is the existing bake, because
the candidate it was built to test was **refuted by baking it**: taking those
shapes out does not flatten the seam, it doubles it — 15.387 → 19.243 → 23.810
→ 28.598 → 33.352 as the knob goes 1 → 0.75 → 0.5 → 0.25 → 0 — and moves the
terrain class further from vanilla's colour at every step (−5.47 → −29.22
levels). The switch ships as the instrument that proved it, with those numbers
in its own `--help` text. The multiply is on **coverage**, so 0 also stops such
a shape suppressing ground cover, which is the behaviour a consumer would
expect if it is not painting.

What the numbers point at instead is **road opacity**, and it does not have a
single answer: `--road-opacity 0.326` puts the seam at 5.304 against vanilla's
5.362 on (-20,20) and 4.242 against 5.138 on (-8,8) — but it drops R5 to 0.1576
and pushes the centreline colour error from 23.64 to 30.87. `--road-opacity
0.83` is the only value that keeps both R5 bars green (0.3545 ≥ 0.3271) while
improving the seam to 12.630. The two chunks want opposite directions, which is
lane ROADS3's "no single opacity meets all gates" refusal a second time.
Nothing about opacity was changed.

Also in this batch: the road census now counts the ground-material shapes and
their texels (`ground_shapes`, `ground_texels` in the census line;
`roadGroundPaint`, `roadGroundShapes`, `roadGroundTexels` in the meta report),
so the thing above is a number the generator prints rather than something that
has to be re-derived offline.

Files: `src/lodgen.h`, `src/lodgen.cpp`, `src/nifcli.cpp`. `--roads-legacy` is
untouched and pinned by a byte-identity check.
