---
name: ww-reference-card-diagnose
description: Diagnose a baked impostor/LOD sheet by rebuilding the consumer in numpy — an offline reference card that decodes the shipped DDS itself, reproduces the drawer's blend and parallax, and scores the result against the mesh, so a defect can be localised to the BAKE, the SHEET, or the DRAW without a build. Covers the known-answer control (score a single un-blended frame from its own bake direction), the candidate table that picks a repair by measurement, the instrument-contamination traps (viewer chrome in the grab, a control that compares a thing to itself, a PNG dumped before the dilate), and the rule that a repair is proved by a gate row that fails on the old exe. Use whenever a baked card or sheet "looks wrong" and nobody can say which stage broke it.
---

# WW: diagnose a baked sheet with a numpy reference card

Repo `E:\Projects\NifskopeWildWastelandEdition`. Written 2026-09-19 by lane
IMPOSTORFIX1, which took an octahedral impostor from IoU 0.35 ("a spray of
fragments") to 0.50 and named the two defects that made it. Working code:
`scratchpad/impostorfix1_20260919/refcard.py` (the reference card),
`bcdec.py` / `tests/spells/impostor_bc_decode.py` (the decoder),
`repairpick.py` (the candidate table), `tests/spells/impostor_sheet_check.py`
and `tests/spells/impostor_bake_views.py` (the two gate rows that came out of
it).

Related: `ww-sheet-diff` diffs two sheets against each other; this one asks
whether ONE sheet is right at all. `ww-silhouette-compare` is the picture;
this is the model behind it.

## 0. The claim to refuse first

"The card does not match the mesh" is not a defect report, because three
independent stages can produce it: the photography (the bake), the encoding
(the sheet on disk), and the drawer (the shader). A lane that starts
repairing before it has split those three spends its build slots on the wrong
one. The previous lane's report blamed thin twigs and an alpha threshold; both
were true statements about the subject and neither was the defect.

Split them with a reference card: a numpy program that reads the SHIPPED DDS,
does what the shader does, and scores against the mesh. It has no window, no
driver and no build, so it can run a hundred variants in the time one build
takes -- and, more importantly, when it and the application disagree, the
disagreement is itself the finding.

## 1. Decode the sheet yourself, from the format

Never read a generated sheet through the writer that made it, and never
through the PNG the bake dumps beside it. Two traps, both paid for:

* **The `*_oct_normal.png` is NOT the pre-compression twin of the `_n` DDS.**
  It is dumped BEFORE `lodgenDilateFrames`, so it lacks the dilate and the
  flood entirely. Comparing DDS to PNG and calling the difference "BC3 error"
  charges compression for a bug in the dilate. It is a valid twin on FULLY
  COVERED texels only, where the later passes do not write -- say which
  texels you compared.
* BC3 is two different things in one block: an 8-bit interpolated ALPHA block,
  and a BC1-style RGB565 palette of four entries SHARED by R, G and B. So a
  value in `.a` has roughly a byte of precision and a value in `.b` has a
  quarter of a shared palette. Which channel a quantity sits in is a
  measurable design decision, not a detail.

`tests/spells/impostor_bc_decode.py` is ~80 lines and decodes both block
kinds. Keep such a decoder in `tests/spells/`, not in a scratchpad, because
the gate rows that come out of this work will import it.

## 2. The known-answer control comes FIRST, and its truth comes from outside

Before measuring any repair, find the input whose correct output is known
independently, and measure that.

For an octahedral card it is the bake directions themselves: a frame IS an
orthographic photograph of the mesh along one direction, so from that
direction the card must reproduce it -- the neighbours carry barycentric
weight zero, and under an orthographic camera the parallax moves the sample
along a ray the frame's own UV projection discards. **A photograph must score
high. If it does not, that is the next defect, and it is chased before
anything else.**

The directions are derived from the `.lodm`'s own grid size by the bake's
hemi-octahedral map (`impostor_bake_views.py`), never typed or stored -- a
stored list is a constant somebody can edit to pass.

Lane IMPOSTORFIX1's decomposition, same exe, same sheets, same mesh:

| views | blend + parallax | IoU |
|---|---|---|
| 16 bake directions | OFF | 0.8823 |
| 16 bake directions | ON | 0.6507 |
| 24 orbit views | OFF | 0.4401 |
| 24 orbit views | ON | 0.3546 |

Four numbers, and the whole diagnosis is in them: the draw is sound (0.88),
the parallax step costs 0.23 where it is provably a no-op, and the bottom-right
cell is the shipped complaint. Nothing about twigs.

## 3. The instrument contaminates in the direction that flatters

The first run of that control gave 0.7455 with four views at 0.31. The cause
was in the harness: `Scene::drawGrid` returns early in ortho mode UNLESS the
view is axis-aligned, so at azimuth 0/90/180/270 elevation 0 -- and nowhere
else -- a screen-plane lattice was painted into the MESH grab and not the card
grab. It entered the union of every comparison.

The checks that catch this class:

* **Look at the grabs.** The number said "four views are bad"; only the
  picture said "there is a grid in them".
* **Watch the coverage, not just the score.** Mesh coverage 0.0746 against a
  card's 0.0250 is the tell before the IoU is.
* Anything the viewer paints over the framebuffer -- navigation gizmo, 3D
  cursor, grid, axes -- is turned OFF in the harness that measures, and the
  suppression is a line in its log.
* **A control that compares a thing to itself convicts the innocent.** An
  earlier lane "proved" `frameOffset` was wrong with a control whose reference
  was built from the same offsets; it agreed with itself perfectly and the
  conclusion was backwards.

## 4. Pick the repair from a table, not from the first idea

When the reference card localises the defect, enumerate the candidate repairs
and score ALL of them against the same views before writing any C++. Lane
IMPOSTORFIX1's height-channel table had five rows (un-premultiply, dilate from
covered neighbours, background at the card plane, and two span fits); the
winner scored within 0.0015 of the best possible and, unlike the best, needed
no projection change, no format change and no spec change.

Write the table into the code comment beside the repair, with the losers. The
next lane's first question is "why not the obvious one", and the answer is a
measurement or it is an opinion.

Two standing constraints that decide such tables here:

* **A parameter that is a consequence is not a free parameter.** `depthSpan`
  looks tunable and is not: `GLView::glProjection` sets the ortho clip range to
  `|bs.center[2]| +/- 1.5 x bounds`, `gl_FragCoord.z` is linear across exactly
  that, and `depthSpan` is that range written down. Fitting it means narrowing
  the projection. Before tuning any baked constant, find the code that
  produces it.
* **A format contract is not changed silently.** If the best repair needs the
  sheet's channel assignment or bit depth to move, ship the in-spec option,
  quote the spec line, state the byte cost of the change, and mark the ruling
  OWED to the director.

## 5. A repair is proved by a row that FAILS on the old build

Every defect this finds gets a gate row that is red on the exe that shipped
the defect, and the report says which row catches which defect -- including
the ones no row catches.

The two shapes that came out of this lane, both in
`tests/spells/impostor_draw.sh`:

* **A row with no application in it at all.** `impostor_sheet_check.py` reads
  the two DDS and asks whether every covered texel's height lies inside the
  band that frame's OWN fully covered texels occupy. The band comes from the
  file, so the row cannot be satisfied by editing a constant. It needs no
  window, no driver and no scene, and it is the only row that survives a
  broken build.
* **The known-answer row.** Score parallax ON and OFF at the bake directions
  and assert they are EQUAL (the algebra says the step is a no-op there) and
  that both are high. Assert the view COUNT as well, so an exe that ignores
  the variable that selects the directions fails loudly instead of quietly
  scoring some other set of views.

A floor raised after a repair is raised to sit above the PRE-repair number,
with the pre- and post-numbers written into the comment, and never lowered.

## 6. Say what is still broken

The report names the residuals the pictures show, in the same voice as the
wins. Lane IMPOSTORFIX1 shipped with a maple whose crown is still a blob on a
stick (401 of 32,768 texels fully covered -- a resolution and coverage-cut
problem the height repair cannot reach), and a rock that regressed 2.8%
because the old flood average happened to sit near a solid object's real
depth. Both are in the report with the number and the mechanism.

No sentence like "follows the tree's shape" unless the picture shows it.

## 7. The decoder is an instrument too (added by IMPOSTORFIX2, 2026-09-19)

Section 2's known-answer control applies to the DECODER before it applies to
anything the decoder reads. `tests/spells/impostor_bc_decode.py:26-28` shipped
with its BC3 alpha ramp one step short -- `((6-k)*a0 + k*a1)/7` where D3D says
`((7-k)*a0 + k*a1)/7`, and index 7 never assigned -- worst error 36.4 of 255,
always LOW, so a gate that selects covered texels by alpha silently skips the
ones it was written to test. It was used as ground truth for a whole day by two
lanes because nothing in the tree decodes a SYNTHETIC block with a known answer.

The three-line control, before any sheet is opened:

    a0, a1 = 255, 0  (a0 > a1, so the eight-level mode)
    expect 255, 0, 218.6, 182.1, 145.7, 109.3, 72.9, 36.4
    and for a0 < a1 the six-level mode plus 0 and 255

The same rule for an ENCODER: mirror `lodgenEncodeBC1Block` (src/lodgen.cpp:4451)
and `lodgenWriteDds` (src/lodgen.cpp:4560) and gate it against the shipped DDS.
Gate it only on 4x4 blocks whose 16 texels are ALL fully covered -- a block with
a partial neighbour has different endpoints because the PNG twin on disk is
pre-dilate and pre-repair, and chasing that difference wastes an hour. On an
all-covered block the agreement must be EXACT, mean 0.00.

## 8. One subject is not a table (added by IMPOSTORFIX2, 2026-09-19)

IMPOSTORFIX1's truth was the N=12 frame of ONE subject, so its repair table had
one column and its chosen height fill lost 0.017 on a subject it never scored.
The five-subject truth is the harness's own MESH grab,
`control/<tag>_after_b1/v_az%03d_el%02d_mesh.png`, with the numpy card pasted
into the same 512x768 frame.

Registration: ONE world-units-per-pixel scale per subject and ONE integer
(dy,dx) per view, fitted by FFT cross-correlation against the harness's own
CARD grab and NEVER against the mesh. Derive the scale's search range from each
subject's OWN quad extent (`min(512/2*halfW, 768/2*halfH)`); a range carried
over from the first subject that worked will simply find nothing for a subject
three times the size.

The control is then: the numpy card scored against the MESH must reproduce the
IoU the application printed, on every subject. IMPOSTORFIX2's five rows agreed
to within 0.0101 with a mean bias of +0.0043, and every gain smaller than that
bias was marked NOT PROVEN rather than reported.

A repair is ranked only when it has been scored on a FAT SOLID subject as well
as on thin ones. Of the four candidate blend repairs measured in IMPOSTORFIX2,
every one that helped the trees hurt the rock, the worst by -0.065.

## 9. A temporal number beside every silhouette number

A silhouette IoU cannot see popping. Measure it: render a FINE orbit -- 2-degree
steps, 46 of them -- and report the mean AND the maximum `1 - IoU` between
consecutive cards, beside the change THE MESH ITSELF makes over the same step,
which is the floor no card can beat. The measurement overturns the obvious
guess: sharpening the blend weights LOWERS the mean change (the card looks like
one frame for longer) and the price is paid entirely in the MAXIMUM, and only
for nearest-frame-only and for the tightest rejection tolerance.

Always include a tolerance so loose that the repair is a no-op, and check it
reproduces the baseline to four digits. That row is the proof the other rows
are the repair and not a coding accident.

## 10. Cost a sweep before launching it

IMPOSTORFIX2 launched a 60-scale x 24-view x 5-subject registration sweep with
no estimate and `print` through a redirect; after fifteen minutes the output
file was still empty and it had to be killed with nothing on disk. Time ONE
step, multiply, print with `flush=True`, and write results to a file as they
land so a kill costs the remaining steps and not the finished ones. The same
sweep at a 448-pixel reference raster instead of 1024 costs at most 0.0013 IoU
and runs 5x faster -- measure that trade once and then take it.

## 11. Make the simulation real, then publish both columns (IMPOSTORFIX3, 2026-09-19)

Sections 4 and 8 get you a candidate table: six fills x five subjects, scored
offline, a winner. That table is a PREDICTION. The lane that builds the winner
owes the same table with a second pair of columns, because the reference card
and the application are two implementations and the whole value of the method
is that their disagreement is a finding.

    subject   R3 sim  R2d8 sim  sim gain | R3 real  8-ring real  real gain
    blast_n4  0.4978   0.5646    +0.0668 | 0.5038     0.5736       +0.0698
    blast_n8  0.6639   0.7200    +0.0561 | 0.6754     0.7483       +0.0729
    maple_n4  0.3609   0.3685    +0.0076 | 0.3545     0.3674       +0.0129
    dead_n4   0.5826   0.6153    +0.0327 | 0.5721     0.6073       +0.0352
    rock_n4   0.7777   0.8331    +0.0554 | 0.7724     0.8305       +0.0581

Absolute IoUs within 0.006..0.011 and every gain the same sign and order: that
is the instrument passing its own audit. A simulated gain that does not appear
is not a disappointment, it is the most interesting result of the lane.

**The metric will not be the same on both sides, so say which.** The offline
fill used `scipy.ndimage.distance_transform_edt`, a EUCLIDEAN disc of radius 8.
The C++ grows by 8-connected ring passes, a CHEBYSHEV square of radius 8. The
two sets differ by the corners. Write the divergence into the code comment, and
show the answer does not turn on it -- here both 8 and 16 texels beat the
shipped fill on all five subjects, so the radius and therefore the metric is
not what is being decided.

**Two sheet sets need two fixture ROOTS.** `registerLooseSheets` walks UP from
a `.lodm` to the nearest ancestor holding a `textures/` tree, so `cards/` and
`cards_old/` side by side in one fixture resolve to the SAME DDS and the A/B
measures nothing -- while still printing two slightly different numbers,
because the harness's IoU has jitter. Build a separate root, check the two
`_oct_n.DDS` differ, and check the old root reproduces the previous lane's
published number to four digits before believing the new one.

**Count the views the repair LOSES and name them.** A mean over 24 views hides
its own minority. blast_n4 improved on 21 of 24 and lost three, worst
-0.0675 at azimuth 300 elevation 15; the rock improved on 21 of 24 and lost
three at elevation 45, all under 0.009. Put the worst LOSING view in the
picture strip, not the best winning one -- and if the close-up you chose at
random turns out to be one of the losers, publish that one.

**The band that contains the plane by construction.** A clause of the form
"the texels outside must lie in the band the covered texels occupy" cannot
convict a sheet whose outside is the CARD PLANE, because the band is the
object's full depth range and an object centred on its own card straddles the
plane. Measured: 0 frames of 16 failed on four of five subjects. Continuity
across the silhouette -- an uncovered texel touching an inked one carries a
height within SLACK of the mean of its inked neighbours -- fails on all five.
Probe a proposed clause against the OLD state before writing it into the gate;
a throwaway script that scores two candidate clauses side by side costs ten
minutes and is the only thing that tells them apart.

## What lane IMPOSTORFIX4 had to re-derive (2026-09-19)

**The bake's PNG is not the encoder's input, and a control tells you so.** The
`<id>_oct_{albedo,normal}.png` beside the DDS are written BEFORE the bake tail:
`src/lodgen.cpp:3079-3088` dilates three images and then runs
`lodgenRepairOctHeight`, and only after that does the writer convert "from the
bake's PNGs". Encode the PNG and you are measuring the dilate and the height
repair, not the compressor -- IMPOSTORFIX4 got a mean height error of 5-7
levels and worst blocks of 206 that way, and every one of those numbers was
withdrawn. Rebuild the encoder's real input by re-running that tail
(`dilate(nrm, alb, deep)`, `dilate(alb, alb, deep)` -- which preserves alpha
because `isCoverage` -- then the height repair, `deep = max(8, max(w,h)/8)`),
and CONTROL IT ON THE SHIPPED BYTES: a correct model reproduces 100 per cent of
the BC1 endpoint words and 99.9 per cent of the index words. Below that, do not
quote a compression number.

**Reproduce the previous lane's in-application numbers first, and let the worst
delta BE the bar.** An offline instrument that scores the shipped sheets over
24 views will sit a few thousandths off the harness. Score every subject and
every sheet set the lane before published, take the worst signed delta (here
+0.0113 on dead_n4, mean +0.0044), and write into the report that no simulated
gain smaller than that is a gain. It kills arguments before they start, and it
killed three of this lane's own candidate repairs.

**One mesh-free number explains most of a subject's fat ink: `texels painted /
summed coverage`.** Coverage is a fraction; the honest area of a frame is the
SUM of the fractions; what the card paints is the COUNT over the threshold. The
ratio needs no mesh, no registration and no render. blast 1.21, dead 1.26, rock
1.04 -- and the leafy maple 2.36, which is its whole defect. Compute it before
building any view-based ablation, because if it is large the answer is the
sheet plus the threshold and no drawing change can reach it.

**For an apparent-size curve, RENDER at the size; never downsample a big
render.** The defect below the knee IS the aliasing, so a card rendered at 1024
and shrunk to 16 does not have it. Two confounds come with it and both belong
in the report: an integer registration offset scaled down carries up to half a
pixel, which at 16 px is three per cent of the card (it shows as a
non-monotonic dip -- IMPOSTORFIX4's 32 px rows read below its own 24 px rows);
and a mesh mask area-averaged and cut at half coverage is biased THIN for a
bare tree. Comparisons between two arms carrying the SAME reference and the
SAME offset are safe from both; absolute values below about 48 px are not.

**A mip is not automatically the repair for a card that is too small.** Box mip
plus a coverage-floor cut deletes every branch narrower than the new texel:
measured, it took blast_n4 from 0.3615 to 0.2124 at 16 px with ink collapsing
to 0.255, while HELPING the over-covered crown (0.1526 to 0.2798). A repair
that swaps one subject's failure for another's is not ranked. The version that
could win on both -- a chain whose every level's alpha is rescaled so the
level's covered AREA matches mip 0's -- is unmeasured, and naming it is
cheaper than proposing it.

**Raising N does not move the apparent-size knee.** N divides the same sheet
into more and therefore smaller frames, so texels-per-screen-pixel is unchanged
at best. Measured, N=8 beat N=4 by roughly the same margin at 16 px as at 256,
and both collapsed below about 48 px. N is worth having above the knee and is
never the answer below it.

## 9. THE SHADING ARM: a card can be the right SHAPE and still look wrong

Everything above scores COVERAGE. Lane IMPOSTORLOOK1 (2026-09-19) was sent at
"impostors look off" and found that the silhouette work had never touched the
half of the complaint that is about colour and light. Run this arm before
accepting that a card's problem is its shape.

**The measurement is a TRANSFER CURVE, not a mean.** Inside the INTERSECTION of
the two silhouettes (so a coverage difference cannot masquerade as a brightness
one), bucket the mesh's linear luma and print the card's mean luma per bucket.
A mean hides the finding; the curve is the finding. Measured on the shipped
cards, 24 orbit views, blast_n4, 190,708 texels:

    mesh luma   0.02  0.06  0.10  0.14  0.18  0.22  0.27  0.35  0.47  0.78
    card luma  0.130 0.130 0.135 0.144 0.154 0.164 0.171 0.178 0.168 0.127

The mesh spans a factor of thirty-nine and the card a factor of 1.4, and the
card is DARKEST where the mesh is brightest. "Flat and dull" is that row.

**Read the light terms out of BOTH shaders before blaming the sheet.** The two
paths can disagree about the colour space itself, and the divergence is short
enough to tabulate. In this tree, `fo4_default.vert:61,63` hands its fragment
stage `sqrt(lightSourceAmbient.rgb) * 0.375` and `sqrt(lightSourceDiffuse[0].rgb)`
and ends at `fo4_default.frag:538` with `tonemap(...)`; `impostor_oct.frag` used
both uniforms RAW and was the only shader in the tree with no tonemap at all.
Make that three-row table -- ambient, diffuse, output -- for any two paths that
share a framebuffer.

**Four questions, each with the line that answers it:**

* *Is the card lit at all, or lit from a frozen direction?* Look for the light
  uniform in the card's shader. A HEADLIGHT (`lightSourcePosition[0] =
  (0,0,1)` in view space, `glview.cpp:3780`) means neither mesh nor card has a
  turning lit side, so "the card looks evenly lit" is not evidence of a frozen
  bake until the light setup is read.
* *Are the normals stored in frame space and used as world space?* Here they
  are stored in frame space and correctly rotated
  (`frameNormalModel`), so the frame is right -- but `z = sqrt(max(0, 1-|xy|^2))`
  is ALWAYS non-negative, so a baked normal can never face away from its own
  bake camera, and `abs(dot(normal, L))` makes the shading two-sided on top of
  that. Two independent reasons a card cannot have a dark side.
* *Is lighting already baked into the albedo (double lighting)?* Test it on the
  sheet, offline: bucket the covered texels by decoded coverage and print the
  mean albedo luma per bucket. Premultiplied or light-baked albedo goes DARK as
  coverage falls. Measured here it goes BRIGHTER (0.475 at coverage < 0.15
  against 0.402 at full), which is the dilate/flood bringing neighbours in --
  the hypothesis was refuted in one command and never reached a build.
* *Is the specular missing?* A card with no specular lobe cannot have the
  highlight that turns with azimuth, and in this tree that is not a bug to fix
  but bungo's standing ruling (material sheets stay debug-only until the
  FO4/PBRM renderer exists). Name the ruling, do not propose the lobe.

**The lit-side number that does NOT work, so nobody re-derives it:** left-half
minus right-half luma of the trunk band, spread over the orbit. It came out at
sd 0.023 for the card against 0.030 for the mesh -- close enough to prove
nothing, because a silhouette that is asymmetric moves that number as much as
shading does. The transfer curve is the instrument; this one is a confound.

**The outline question is a PERIOD, not a size.** "Is the lumpy edge BC blocks
or texels" is answered by the run lengths of the edge's x position, converted
to sheet texels with the object's own height in texels -- not by its standard
deviation, which mixes amplitude and frequency. Measured: the card's trunk edge
steps every 1.8 to 2.1 texels on blast_n4 and about 1.0 on the maple, nowhere
near the 4 a BC block boundary would give. The edge is texel-shaped, so it is
frame RESOLUTION and not compression, and the deviation figure would have said
the opposite on blast_n4 (card 1.84 texels against the mesh's 2.33 -- the
card's edge is SMOOTHER by that measure, because the mesh's edge is real bark).
