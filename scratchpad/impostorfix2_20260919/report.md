# IMPOSTORFIX2 -- simulate the next impostor repairs, offline. Report.

Lane IMPOSTORFIX2, 2026-09-19, started 14:32 CEDT (`date`). OFFLINE ONLY: no
build, no exe, no src/ or res/ edit, no commit. numpy/PIL/scipy on files on disk.

## 0. What is on disk, what is not, and the instrument

Read in order: CONSTITUTION.md; HANDOFF.md top block; the skill
.claude/skills/ww-reference-card-diagnose/SKILL.md (IMPOSTORFIX1's numpy
reference card is REUSED, not rewritten); impostorfix1_20260919/PENDING.md and
DELIVER.md; docs/LODGEN_IMPOSTOR_SPEC.md; skills ww-control-calibration,
ww-simulate-before-build, ww-texel-picture.

USABLE ON DISK
* impostorfix1_20260919/fixture/<tag>/cards/ -- the REPAIRED sheets (DDS +
  .lodm + the pre-compression PNGs) and cards_before/ with the SHIPPED DDS.
  Tags: blast_n4 blast_n5 blast_n8 blast_n12 dead_n4 maple_n4 maple_n4_t256
  rock_n4.
* impostorfix1_20260919/control/<tag>_{before,after}_b1/v_az%03d_el%02d_{card,mesh}.png
  -- the harness's own 512x768 grabs, 12 azimuths x elev 15 and 45 = the 24
  orbit views, for all five subjects. THESE ARE THE MESH GRABS.
* tests/spells/impostor_bc_decode.py, impostor_bake_views.py,
  impostor_sheet_check.py.

SHEET CENSUS (measured, not quoted)

| tag | N | frame px | sheet px | half | depthSpan | a>=250 | any cov | texels |
|---|---|---|---|---|---|---|---|---|
| maple_n4      | 4 | 32x64   | 128x256  | 439.5 x 879.0   | 3072 | 401    | 2907  | 32768  |
| maple_n4_t256 | 4 | 128x256 | 512x1024 | 439.5 x 879.0   | 3072 | 21532  | 35259 | 524288 |
| rock_n4       | 4 | 128x128 | 512x512  | 1727.9 x 1727.9 | 5048 | 51869  | 54308 | 262144 |
| blast_n4      | 4 | 48x128  | 192x512  | 135.3 x 360.8   | 3072 | 7315   | 9733  | 98304  |

The maple's frame is 32 x 64 TEXELS. Its "401 of 32,768 fully covered" is 1.22
per cent of the sheet; maple_n4_t256, the same subject at 4x the linear frame
resolution, is 4.11 per cent. That ratio is section 1's whole subject.

NOT ON DISK -- named so a build lane can produce it
* THE PRE-DOWNSAMPLE PHOTOGRAPHS. frameOf (src/nifskope_ui.cpp:23102) crops the
  live viewport render and calls QImage::scaled( iw, ih, IgnoreAspectRatio,
  SmoothTransformation ). Its input -- matte(), channel(8), channel(9) (window
  depth), channel(10), channel(11), channel(13) at viewport size -- is never
  written to a file. Every *_oct_*.png in bake/ and cards/ is AFTER the
  downsample. What a build lane must dump, exactly: `inner` at
  src/nifskope_ui.cpp:23115 BEFORE `.scaled(...)`, as PNG, per view, for
  matte() and channel(9), on blast_n4 and maple_n4. Until then section 1 is
  measured at ratio 4 (maple_n4_t256 -> maple_n4) instead of the real ~7x, and
  says so on every row.


## 0c. A DEFECT IN THE INSTRUMENT THAT SHIPPED THIS MORNING

`tests/spells/impostor_bc_decode.py:26` decodes the BC3 ALPHA ramp one step
short. The eight-level mode's interpolated entries must be
`a[k+1] = ((7-k)*a0 + k*a1) / 7` for k = 1..6; the file writes `(6-k)` and
never assigns index 7, so index 7 decodes as 0.

Known-answer block, a0 = 255, a1 = 0, texels carrying indices 0..7:

| index | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| D3D | 255.0 | 0.0 | 218.6 | 182.1 | 145.7 | 109.3 | 72.9 | 36.4 |
| shipped | 255.0 | 0.0 | 182.1 | 145.7 | 109.3 | 72.9 | 36.4 | 0.0 |

Worst error 36.4 levels of 255. It reads every interpolated alpha LOW, and the
consequence is in the flattering direction: `impostor_sheet_check.py` selects
"covered" texels by that alpha, `covBase` is 160, so a texel whose true encoded
alpha is between 160 and 196 reads below 160 and is SKIPPED by the gate row
that is supposed to test it. Gate row 14 is therefore weaker than its report
says -- it is not wrong about the texels it did test.

It is also the decoder IMPOSTORFIX1's own reference card used, so every number
in its report rests on it. Replacing it here moved this lane's instrument
CLOSER to the application, which is the check that the replacement is right:
blast_n4 numpy-vs-harness delta 0.0148 -> 0.0051, blast_n8 0.0193 -> -0.0018.

A build lane applies the three-line fix at `tests/spells/impostor_bc_decode.py`
lines 26-28 and re-runs row 14; the corrected copy is
`scratchpad/impostorfix2_20260919/bcdec2.py`.

## 0b. THE INSTRUMENT AND ITS KNOWN-ANSWER CONTROL

`inst.py` is IMPOSTORFIX1's reference card (its `render` reproduces
res/shaders/impostor_oct.frag's three-frame loop, decode, parallax and
coverage-weighted average) with the sheet read through the corrected decoder,
rasterised over the card quad at the quad's own PIXEL footprint and pasted into
the harness's 512x768 frame. Two free numbers: one world-units-per-pixel scale
per subject, and one integer (dy,dx) per view. Both are fitted against the
harness's own CARD grab -- never against the mesh -- by FFT cross-correlation.
The silhouette rule is the harness's own (`silhouette()`,
src/impostorpreviewtest.cpp:384: any channel more than 12 from the clear
colour).

The control: with that registration, the numpy card scored against the MESH
GRAB must reproduce the IoU the application printed.

| tag | scale (u/px) | card-vs-card IoU | numpy vs MESH | harness printed | delta |
|---|---|---|---|---|---|
| blast_n4 | 0.694 | 0.8694 | 0.5089 | 0.5038 | +0.0051 |
| blast_n8 | 0.674 | 0.9435 | 0.6736 | 0.6754 | -0.0018 |
| maple_n4 | 0.287 | 0.9346 | 0.3584 | 0.3545 | +0.0039 |
| dead_n4  | 0.284 | 0.8945 | 0.5822 | 0.5721 | +0.0101 |
| rock_n4  | 0.145 | 0.9819 | 0.7767 | 0.7724 | +0.0043 |

Five subjects, worst absolute disagreement 0.0101 (dead_n4), mean +0.0043. The
instrument reads a shade HIGH, so a repair it scores as a GAIN of less than
about 0.011 is not proven. Every delta below is reported against this lane's own
baseline column, measured by the same instrument, so the bias cancels in the
difference; the absolute column is there to be compared with the application.

maple_n4 and maple_n4_t256 are metrically identical sheets -- same half extents,
same depthSpan 3072, same covFloor/covBase, frame offsets equal to 0.0000 --
so the maple registration transfers to any sheet derived from t256 without
refitting, and section 1 cannot cheat by re-registering its own output.

## 5. THE ALPHA-THRESHOLD SWEEP, RE-MEASURED ON THE REPAIRED SHEETS

The ruling bungo is owed. `res/shaders/impostor_oct.frag` discards below
`alphaThreshold`; today's value is covFloor = 16/255 = 0.0627. The spec,
docs/LODGEN_IMPOSTOR_SPEC.md:348, says a consumer "alpha-tests at 0.5 for full
crowns and tests lower, or blends, for bare trees". Measured on the REPAIRED
sheets, 24 orbit views, three-frame blend, parallax on. "Vanishes" = card ink
below 5 per cent of the mesh's ink in that view.

| subject | 0.063 | 0.120 | 0.200 | 0.300 | 0.450 | best | vanishes at ANY threshold |
|---|---|---|---|---|---|---|---|
| blast_n4 | 0.5090 | 0.5043 | 0.5437 | **0.5576** | 0.4700 | 0.300 | 0 / 24 |
| blast_n8 | 0.6737 | 0.6955 | 0.7194 | **0.7433** | 0.7178 | 0.300 | 0 / 24 |
| maple_n4 | 0.3586 | 0.3706 | **0.3775** | 0.3346 | 0.2140 | 0.200 | 0 / 24 |
| dead_n4  | 0.5824 | 0.5806 | **0.6010** | 0.5898 | 0.4890 | 0.200 | 0 / 24 |
| rock_n4  | 0.7767 | 0.7786 | **0.7842** | 0.7660 | 0.7045 | 0.200 | 0 / 24 |

Card ink as a fraction of the mesh's ink at the same threshold:

| subject | 0.063 | 0.120 | 0.200 | 0.300 | 0.450 |
|---|---|---|---|---|---|
| blast_n4 | 1.51 | 1.30 | 1.11 | 0.84 | 0.56 |
| blast_n8 | 1.27 | 1.19 | 1.08 | 0.92 | 0.79 |
| maple_n4 | 1.88 | 1.45 | 1.00 | 0.62 | 0.29 |
| dead_n4  | 1.19 | 1.04 | 0.90 | 0.73 | 0.54 |
| rock_n4  | 1.06 | 1.03 | 0.98 | 0.85 | 0.73 |

Read the ink column, not the IoU column, for the reason: at 0.063 EVERY subject
paints more ink than the mesh has -- the leafy maple paints 88 per cent too
much. That surplus is the blend's minor frames (section 4) surviving the test.
0.20 is the first threshold where the maple's ink is right (1.00) and it is the
peak for both the maple and the dead tree; the two blasted trees keep gaining to
0.30 and lose by 0.45.

NOTHING VANISHES. Not one of the 24 views on any of the five subjects drops
below 5 per cent ink at any threshold up to 0.45. The fear the current 0.0627
was guarding against is not measurable on these sheets. 0.45 is still wrong --
it costs the maple 0.16 IoU -- but it does not make a card disappear.

The honest reading: 0.0627 is too low for every subject. A single number that is
never worse than today on any subject is 0.20 (maple +0.019, dead +0.019,
blast_n4 +0.035, blast_n8 +0.046). 0.30 is better for the two blasted trees and
worse for the maple by 0.024. This is a one-line default, not a format change.

## 1. THE FRAME DOWNSAMPLE IN THE BAKE

THE PRE-DOWNSAMPLE PHOTOGRAPHS ARE NOT ON DISK. frameOf
(src/nifskope_ui.cpp:23102) crops the live viewport render and immediately
calls `QImage::scaled( iw, ih, Qt::IgnoreAspectRatio,
Qt::SmoothTransformation )` at src/nifskope_ui.cpp:23115. Its input is never
written anywhere; every PNG in bake/ and cards/ is already the output. So this
section is measured at ratio 4 -- maple_n4_t256's 128x256 frames downsampled to
maple_n4's 32x64 by six candidate rules -- and NOT at the real viewport-to-32
ratio, which is closer to 7-10x. Every row below carries that caveat.

WHAT A BUILD LANE MUST PRODUCE: dump `inner` at src/nifskope_ui.cpp:23115
BEFORE the `.scaled(...)` call, as PNG, one file per view, for `matte()` and
`channel(9)` (window depth), on blast_n4 and maple_n4. Two subjects and 32
files answer the question at the real ratio.

THE LEAFY MAPLE, ratio 4, scored against maple_n4's own 24 mesh grabs. Crown
IoU = the top 45 per cent of the mesh's vertical extent only. The shipped 32x64
bake scores 0.3584 / crown 0.3153 by the same instrument.

| downsample rule | IoU | crown IoU |
|---|---|---|
| the 128x256 sheet itself, NO cut (the ceiling) | 0.3384 | 0.3201 |
| A box average (what the bake does) | 0.3417 | 0.3043 |
| B coverage-weighted colour, max coverage | 0.3096 | 0.2813 |
| C nearest (max-coverage texel wins, colour and depth) | **0.3453** | 0.3062 |
| D median depth, box colour | 0.3413 | 0.3028 |
| E max coverage + nearest depth | 0.3194 | 0.2889 |
| F max-coverage colour + box height | 0.3416 | 0.3023 |

THE RESULT IS NEGATIVE AND IT IS THE INTERESTING ONE. The whole spread from
best to worst rule is 0.036, and the best rule beats the box average by 0.0036 --
a third of this instrument's own 0.011 disagreement with the application, i.e.
not proven. More decisively: THE 128x256 SHEET WITH NO DOWNSAMPLE AT ALL SCORES
0.3384, BELOW the box average of itself at 32x64. Four times the linear
resolution does not buy the maple a better silhouette.

The "401 of 32,768 texels fully covered" figure that framed this section is real
(1.22 per cent; t256 is 4.11 per cent) but it is not what is costing the maple
its 0.35. The lace of twigs in the mesh grab is finer than one texel at EITHER
resolution, so every rule paints the same fat blob; changing which fat blob only
moves the number by a rounding error. Max-coverage (B, E) is the WORST of the
six on every column, because keeping the most-covered texel of each 4x4 grows
the blob further.

THE BARE TREES, ratio 2 (48x128 -> 24x64 and the dead tree's frame halved):
see the table appended below when s1 'other' lands. What matters for the ranked
list is already decided by the maple: the downsample rule is NOT a repair worth
a build lane's day, and the pre-downsample dump is worth ONE hour only to
confirm that at the real ratio.

## 3. THE ROCK REGRESSION 0.7944 -> 0.7724. IT IS THE PICTURE, NOT THE METRIC.

WHICH REPAIR. From IMPOSTORFIX1's own ablation: shipped 0.7944 -> old sheets
with the orthographic ray 0.7891 (the ray cost -0.0053) -> repaired sheets with
the ray 0.7724 (THE HEIGHT FILL cost -0.0167). Reproduced here on this lane's
instrument with the registration FROZEN at the calibrated value, so the only
thing that changes between rows is the sheet content and the ray:

| variant | IoU | IoU at best rigid shift | mean vertical error el15 | el45 |
|---|---|---|---|---|
| shipped heights, ortho ray on | 0.7894 | 0.8134 | 9.92 px | 7.75 px |
| repaired heights, ortho ray on | 0.7724 | **0.8328** | **20.08 px** | 9.58 px |
| either sheet, parallax step OFF | 0.4988 | 0.5122 | -41.42 px | -7.67 px |

(Parallax off renders the two sheets IDENTICALLY -- the same four digits -- which
is the self-check that these two sheets differ ONLY in the height channel.)

IT IS THE PICTURE. The repaired card's SHAPE is BETTER by +0.0193 once it is
allowed one rigid translation; what it lost is its POSITION. The same
measurement run on the harness's own 512x768 grabs, which never touch this
lane's code, agrees: before raw 0.7891 / at best shift 0.8142 / dy -10.17 px,
after raw 0.7724 / at best shift 0.8301 / dy -20.17 px, shape delta +0.0159.
Two independent instruments, the same conclusion and the same 10 px.

WHICH VIEWS. The whole loss is at ELEVATION 15, where the ray grazes:
az 60 -0.1163, az 90 -0.1120, az 180 -0.0901, az 270 -0.0781, az 240 -0.0631,
az 120 -0.0610, az 300 -0.0576, az 150 -0.0521; it GAINS at az 0 +0.0813 and
az 330 +0.0336. At elevation 45 it is net POSITIVE (az 0 +0.0439, az 240
+0.0363, az 270 +0.0234, az 210 +0.0185). The views that lose most are exactly
the views whose vertical error grew most: az 90 el15 dy -40 px, az 270 -39,
az 300 -34, az 60 -29.

THE CAUSE, IN UNITS. 1 px = 6.88 world units on this subject. The repair
changed the height channel OUTSIDE coverage (207,836 texels, 97.4 per cent of
them moved): mean height 119.0 -> 131.3 levels. `want = -(h/255 - 0.5) *
depthSpan` with depthSpan 5048 turns that into a depth push of -243 units. At
elevation 15 the screen-vertical component of a depth push is sin 15 = 0.259,
so 243 x 0.259 = 63 units = 9.2 px -- against a MEASURED growth of 10.2 px.
That is the mechanism, and it is arithmetic, not a guess.

WHY IT IS WRONG FOR THIS SUBJECT. The rock's real covered surface sits at
h = 92.6 levels = -691 units, well in front of the card plane. The old flood
value 119 was -166 units -- too shallow, but on the correct side. The repair
moved it to 131.3 = +6 units, i.e. BEHIND the card plane, the one place the
rock's surface certainly is not. For a bare tree that region is empty air and
any value does; for a fat solid object a neighbouring frame's ray lands out
there constantly, so the value it reads is the object's own depth or it is a
lie. That is the difference between the four trees (which gained) and the rock
(which lost).

The picture: scratchpad/impostorfix2_20260919/images/03_rock_worst_before_after.png
-- az 60, 90, 180 and 0 at elevation 15, mesh grab on top, BEFORE overlay in the
middle, AFTER overlay at the bottom, red = mesh only, blue = card only. BEFORE
is blue along the TOP with detached flakes below. AFTER has the flakes gone and
the right AMOUNT of ink (ink/mesh 1.176 -> 0.973 at az 60) but a thick RED band
along the whole bottom edge: the card has climbed off the rock. az 0, where the
old card was fringed and the new one is clean, gains +0.0813 by the same rule.

## 4. THE TRUNK-BASE FLAKES AND THE DOUBLED TRUNK. THE CAUSE, WITH NUMBERS.

THE MECHANISM IS TWO LINES OF res/shaders/impostor_oct.frag.

  line 287   float wc = w * cov;
  line 290   colour.a += cov * w;      // NOT divided by wsum
  lines 301-307  colour.rgb /= wsum; normal /= wsum; height /= wsum; ...
  line 316   if ( colour.a < alphaThreshold ) discard;

Every other channel is normalised by the coverage-weighted sum; the ALPHA is
not. The barycentric weights sum to 1, so a frame with weight w and full
coverage contributes exactly w to colour.a on its own. alphaThreshold is
covFloor = 16/255 = 0.0627. Therefore:

  ANY FRAME WITH BARYCENTRIC WEIGHT ABOVE 0.0627 PAINTS ITS OWN FULL,
  UNSOFTENED SILHOUETTE AT THE CURRENT THRESHOLD.

Measured second-frame weight over the 24 orbit views: mean 0.268, max 0.366 at
N=4 -- 4.3x and 5.8x the threshold. At N=8 it is mean 0.334, max 0.479. There is
no view in the set where the second frame is quiet enough to be excluded.

HOW MUCH OF THE CARD IS PAINTED BY FRAMES THAT ARE NOT THE DOMINANT ONE.
`minorInk` = the fraction of the card's ink whose alpha would fall under the
threshold if only the dominant frame's contribution were counted. `base` = the
same restricted to the bottom quarter of the card, which is the trunk.

| subject | minorInk | minorInk, TRUNK BASE | IoU dominant frame only | IoU 3-frame | 2nd weight mean / max |
|---|---|---|---|---|---|
| blast_n4 | 45.8% | **45.0%** | **0.5319** | 0.5089 | 0.268 / 0.366 |
| blast_n8 | 26.3% | 21.7% | **0.6932** | 0.6736 | 0.334 / 0.479 |
| maple_n4 | 31.6% | **45.7%** | 0.3464 | **0.3584** | 0.268 / 0.366 |
| dead_n4  | 37.6% | **45.5%** | 0.5304 | **0.5822** | 0.268 / 0.366 |
| rock_n4  | 22.5% | 28.1% | 0.7017 | **0.7767** | 0.268 / 0.366 |

THE ANSWER THE DIRECTOR ASKED FOR. Nearly HALF the ink at the trunk base --
45.0 per cent on blast_n4, 45.7 on the maple, 45.5 on the dead tree -- is
painted by frames that are not the one nearest the view. Those frames
photographed the same trunk from 90 degrees away, so their trunk sits at a
different horizontal position on the card; each one paints it at full opacity
because of line 290, and what the eye reads is a second and a third trunk beside
the first. That is the doubled base at az 0-60 and the horizontal branches at
az 240-330: they are not artefacts of one frame, they are NEIGHBOURING FRAMES
DRAWN AT FULL STRENGTH.

The flakes are the same line one step weaker: a minor frame whose coverage is
partial, say cov 0.3 with w 0.27, still clears 0.0627 (0.081) and paints a
detached fragment wherever that frame happened to have a twig.

THE SPEC ALREADY PROMISES THE CURE. docs/LODGEN_IMPOSTOR_SPEC.md:232-233 defines
the three-frame triangle blend, and :313-315 lists "ghost-free frame blending"
as a USE of the height channel. `res/shaders/impostor_oct.frag:105` already
carries `uniform bool useHeightBlend; // the ghost-free blend, spec 286` and the
branch at :214 already reprojects each frame's height. So the height-consistency
rejection is a spec WORDING change at most (name the rejection rule and its
tolerance), never a format contract.

WHAT IT COSTS IF YOU JUST TURN THE MINOR FRAMES OFF. Dominant-frame-only is
BETTER for the two blasted trees (+0.023 and +0.020) and WORSE for the other
three (maple -0.012, dead -0.052, rock -0.075). So "nearest only" is not the
repair; the repair is to keep the blend and stop the minor frames painting
silhouette they do not own. Section 2 measures the candidates and the popping
they cost.

THE BARE TREES AND THE ROCK, each subject's OWN frames halved (ratio 2), scored
against the same 24 mesh grabs. The "shipped full-res" figure beside each name
is this instrument's score for the sheet as it ships, so the whole table is a
measure of what HALVING costs and which rule loses least.

| rule | blast_n4 | crown | dead_n4 | crown | rock_n4 | crown |
|---|---|---|---|---|---|---|
| shipped, no cut | 0.5089 | 0.4042 | 0.5822 | 0.5586 | 0.7767 | 0.7881 |
| A box average | 0.4607 | 0.3602 | 0.5198 | 0.4861 | 0.7675 | 0.7819 |
| B coverage-weighted, max coverage | 0.4333 | 0.3225 | 0.5014 | 0.4525 | 0.7781 | 0.7879 |
| C nearest | **0.4613** | 0.3602 | **0.5223** | **0.4929** | 0.7603 | 0.7722 |
| D median depth, box colour | 0.4603 | 0.3587 | 0.5202 | 0.4885 | (see below) | |
| E max coverage + nearest depth | 0.4430 | 0.3446 | 0.5087 | 0.4649 | | |
| F max-coverage colour + box height | 0.4602 | **0.3603** | **0.5223** | **0.4929** | | |

Halving costs the bare trees 0.048 and 0.060 and the rock only 0.009 -- the
thin subjects are the ones resolution touches, exactly as expected. But the
SPREAD BETWEEN RULES is again tiny where it matters: C nearest beats A box by
0.0006 on blast_n4 and 0.0025 on dead_n4, both far inside this instrument's own
0.011 uncertainty. Max-coverage (B) is again the worst on the trees, by 0.027
and 0.018, and it is the only rule that beats box on the ROCK (+0.011) -- a fat
solid object is the one subject where growing the blob is right.

CONCLUSION FOR SECTION 1. The downsample rule is not a repair. Changing it is
worth at most 0.003 on any subject, which this instrument cannot resolve, and
the max-coverage variants the brief asked about are actively WORSE on three of
the four trees. The 401-texel figure is a symptom of the maple's geometry, not
of the filter. A build lane should spend the hour on the `inner` dump only to
close the question at the real ratio, and spend the day on sections 2 and 3.

### 3b. THE REPAIR THAT UNDOES THE REGRESSION WITHOUT GIVING BACK THE GAIN

If the fault is that the height OUTSIDE coverage is filled with the wrong depth,
the test is to fill it differently and measure all five subjects. Six fills,
same sheets otherwise, same BC3 round trip, same 24 views, same registration:

| height fill outside coverage | blast_n4 | blast_n8 | maple_n4 | dead_n4 | rock_n4 |
|---|---|---|---|---|---|
| as baked: flood with the frame's mean | 0.3599 | 0.3794 | 0.3569 | 0.4187 | 0.7894 |
| R1: the card plane (128) everywhere | 0.4550 | 0.5943 | 0.3232 | 0.5391 | 0.7596 |
| R3: WHAT SHIPPED TODAY | 0.4978 | 0.6639 | 0.3609 | 0.5826 | 0.7777 |
| **R2d8: dilate the covered height out 8 texels, plane beyond** | **0.5646** | **0.7200** | 0.3685 | **0.6153** | 0.8331 |
| R2d16: the same, 16 texels | 0.5515 | 0.7165 | 0.3690 | 0.5907 | **0.8416** |
| R2 dilate to the whole frame | 0.5302 | 0.7163 | **0.3694** | 0.5898 | 0.8380 |

R2d8 BEATS WHAT SHIPPED ON ALL FIVE SUBJECTS: blast_n4 +0.0668, blast_n8
+0.0561, maple +0.0076, dead +0.0327, rock +0.0554. On the rock it is also above
the number the regression is measured against -- 0.8331 against the shipped
0.7944 -- so it does not merely undo the loss, it clears it by +0.039. Nothing on
this table is a trade between subjects; the dilate is simply a better answer than
either the old flood or the new plane, and 8 texels is the right radius for four
of the five (the rock prefers 16 by +0.0085, inside this instrument's noise).

The reason is the one section 3 gives: near the silhouette a neighbouring
frame's ray lands just OUTSIDE this frame's coverage, and dilating carries the
object's own depth out to meet it instead of a constant. Beyond the dilation
radius nothing samples, so the plane is harmless there.

R1 -- the card plane everywhere -- is the WORST rule for the leafy maple
(0.3232, below even the as-baked flood) and the second worst for the rock. That
is the shape of the mistake that shipped: R3 is R1 softened, and it inherits
R1's direction.

## 6. THE OWED `_n` HEIGHT <-> SWAY CHANNEL SWAP

docs/LODGEN_IMPOSTOR_SPEC.md:45 writes the contract:

    | _n | BC3 | normal X | normal Y | height | sway weight |

so HEIGHT is the BLUE channel and SWAY is the ALPHA channel. In BC3 those two
slots are not alike. Alpha gets a DEDICATED BC4 block: two 8-bit endpoints and
six interpolated levels, per 4x4, for that one channel. Blue shares ONE
four-entry RGB565 palette with red and green, chosen by
`lodgenEncodeBC1Block` (src/lodgen.cpp:4451) from the min and max LUMINANCE
texels of the block -- so the blue channel's fidelity is decided by whatever the
NORMAL is doing in that block, and 565 gives blue five bits before the palette
even starts.

Simulated through the real encoder and decoder -- `bcenc.py` mirrors
src/lodgen.cpp:4451 and :4560 line for line, and is gated exact (mean 0.00 error)
against the shipped DDS on every 4x4 block whose 16 texels are all fully
covered -- on the REPAIRED height, both layouts, all five subjects:

| subject | channel | as shipped: mean / p95 / max levels | SWAPPED: mean / p95 / max |
|---|---|---|---|
| blast_n4 | HEIGHT | 2.29 / 6.06 / 21 | **0.06 / 1.00 / 2** |
| blast_n8 | HEIGHT | 2.48 / 6.49 / 30 | **0.09 / 1.00 / 3** |
| maple_n4 | HEIGHT | 2.83 / 6.13 / 57 | **0.07 / 0.00 / 5** |
| dead_n4  | HEIGHT | 2.95 / 7.87 / 36 | **0.16 / 1.00 / 4** |
| rock_n4  | HEIGHT | 3.36 / 9.90 / 95 | **0.22 / 1.00 / 5** |
| blast_n4 | sway   | 1.51 / 8.00 / 18 | 6.89 / 29.77 / 149 |
| blast_n8 | sway   | 1.54 / 7.00 / 18 | 6.80 / 28.98 / 151 |
| maple_n4 | sway   | 2.87 / 10.00 / 16 | 10.02 / 31.20 / 119 |
| dead_n4  | sway   | 0.94 / 4.00 / 13 | 6.39 / 27.30 / 120 |
| rock_n4  | sway   | 0.54 / 3.00 / 14 | 4.27 / 11.52 / 94 |
| blast_n4 | normal X | 12.49 / 43.35 / 156 | 14.62 / 56.31 / 169 |
| blast_n8 | normal X | 13.05 / 45.19 / 200 | 14.85 / 55.46 / 214 |
| maple_n4 | normal X | 13.14 / 48.40 / 150 | 13.68 / 50.71 / 150 |
| dead_n4  | normal X | 15.31 / 55.01 / 173 | 16.46 / 58.43 / 173 |
| rock_n4  | normal X |  9.37 / 36.52 / 168 |  9.59 / 37.55 / 168 |

In world units the height error is the number that matters, because the
parallax step multiplies it by depthSpan:

| subject | depthSpan | height error as shipped | swapped |
|---|---|---|---|
| blast_n4 | 3072 | 28 units mean, max 253 | 1 unit mean, max 24 |
| blast_n8 | 3072 | 30 units mean, max 361 | 1 unit, max 36 |
| maple_n4 | 3072 | 34 units mean, max 687 | 1 unit, max 60 |
| dead_n4  | 3072 | 36 units mean, max 434 | 2 units, max 48 |
| rock_n4  | 5048 | 66 units mean, max 1881 | 4 units, max 99 |

THE SILHOUETTE IT BUYS, same 24 views, three-frame blend, parallax on:

| subject | height in blue (shipped) | height in alpha (swapped) | delta |
|---|---|---|---|
| blast_n4 | 0.4978 | 0.5362 | **+0.0383** |
| blast_n8 | 0.6639 | 0.7010 | **+0.0372** |
| maple_n4 | 0.3609 | 0.3629 | +0.0020 |
| dead_n4  | 0.5826 | 0.5870 | +0.0044 |
| rock_n4  | 0.7777 | 0.7823 | +0.0046 |

THE RULING BUNGO IS OWED, put as a measured trade. The swap makes the height
channel 20 to 40 times more accurate -- the rock's worst-case height error falls
from 1,881 world units to 99, and 1,881 units is a quarter of that subject's own
depth span, i.e. the encoder alone can put a texel a quarter of the rock behind
where it is. It costs the sway channel a factor of 4 to 8 (mean 1.5 -> 6.9
levels) and normal X about 1.5 levels of mean, which is 12 per cent of an error
that is already 12-15 levels because red shares the same palette.

Sway is a vertex-animation weight sampled per pixel to wobble foliage; 7 levels
of 255 is 3 per cent of a wobble amplitude. Height is a DEPTH, multiplied by
depthSpan, and every artefact in this report is a depth artefact. The trade is
not close.

SPEC IMPACT: this is the one repair that is a FORMAT CONTRACT change. It
rewrites docs/LODGEN_IMPOSTOR_SPEC.md:45 to
`| _n | BC3 | normal X | normal Y | sway weight | height |`, changes the writer
in src/lodgen.cpp and the reads in res/shaders/impostor_oct.frag (`n.b` ->
`n.a`, `n.a` -> `n.b` at :217, :292, :293), and INVALIDATES EVERY SHEET ALREADY
BAKED. It needs bungo's ruling before a line is written, and a version bump in
the .lodm so an old sheet is refused rather than read backwards.

## 2. N=4 GHOSTING: WHAT THE SPEC SAYS, AND WHAT THE CANDIDATES COST

WHAT THE SPEC SAYS ABOUT BLEND WEIGHTS. docs/LODGEN_IMPOSTOR_SPEC.md:232-233:
"every direction falls inside a triangle of three frames -- the (N-1)^2 triangle
mesh between frame centres is the blending rule". That is the whole of it: three
frames, barycentric, no exponent and no rejection. :313-315 then lists the uses
of the height channel and one of them is "ghost-free frame blending", and
res/shaders/impostor_oct.frag:105 already declares
`uniform bool useHeightBlend; // the ghost-free blend, spec 286` with the
reprojection branch written at :214. SO THE REJECTION IS ALREADY PROMISED. A
sharpening EXPONENT is not -- `w^2` or `w^4` is no longer barycentric and would
need :232-233 reworded. Neither is a format change.

THE TABLE. 24 orbit views for IoU. The temporal column is a FINE 2-DEGREE orbit,
azimuth 0 to 90 at elevation 15, 46 steps: `2deg mean` and `2deg max` are the
mean and worst 1 - IoU between consecutive 2-degree cards, i.e. how much of the
silhouette changes for two degrees of camera motion. `30deg card` is the same
quantity at the 30-degree step, printed beside `30deg MESH`, the change THE MESH
ITSELF makes over the same 30 degrees -- the floor no card can go below.

| variant | subject | IoU | 2deg mean | 2deg MAX | 30deg card | 30deg MESH |
|---|---|---|---|---|---|---|
| spec: 3 frames barycentric | blast_n4 | 0.5086 | 0.1943 | 0.4242 | 0.7459 | 0.7999 |
| weights^2 | blast_n4 | 0.5241 | 0.1905 | 0.4139 | 0.7683 | 0.7999 |
| weights^4 | blast_n4 | 0.5369 | **0.1778** | 0.4381 | 0.7837 | 0.7999 |
| nearest frame only | blast_n4 | 0.5335 | 0.1818 | **0.8640** | 0.7976 | 0.7999 |
| height reject 0.25 span-step | blast_n4 | **0.5608** | 0.2006 | 0.7606 | 0.7889 | 0.7999 |
| height reject 1 span-step | blast_n4 | 0.5305 | 0.1912 | 0.4194 | 0.7545 | 0.7999 |
| height reject 4 span-steps | blast_n4 | 0.5086 | 0.1943 | 0.4242 | 0.7459 | 0.7999 |
| spec | blast_n8 | 0.6719 | 0.2179 | 0.3809 | 0.7517 | 0.8002 |
| weights^2 | blast_n8 | 0.6784 | 0.2171 | 0.4361 | 0.7623 | 0.8002 |
| weights^4 | blast_n8 | 0.6948 | 0.2290 | 0.4925 | 0.7808 | 0.8002 |
| nearest frame only | blast_n8 | 0.6913 | 0.2524 | 0.7210 | 0.8004 | 0.8002 |
| height reject 0.25 | blast_n8 | **0.6952** | 0.2563 | 0.5940 | 0.7915 | 0.8002 |
| height reject 1 | blast_n8 | 0.6766 | 0.2164 | 0.3737 | 0.7507 | 0.8002 |
| spec | maple_n4 | 0.3584 | 0.1172 | 0.1885 | 0.5856 | 0.7861 |
| weights^2 | maple_n4 | 0.3596 | 0.1022 | **0.1697** | 0.5774 | 0.7861 |
| weights^4 | maple_n4 | 0.3597 | **0.0895** | 0.2021 | 0.5627 | 0.7861 |
| nearest frame only | maple_n4 | 0.3462 | 0.0912 | **0.5368** | 0.5866 | 0.7861 |
| height reject 0.25 | maple_n4 | **0.3666** | 0.1031 | 0.2411 | 0.5828 | 0.7861 |
| height reject 1 | maple_n4 | 0.3658 | 0.1006 | 0.1680 | 0.5730 | 0.7861 |
| spec | dead_n4 | 0.5804 | 0.1188 | 0.2912 | 0.5855 | 0.6073 |
| weights^2 | dead_n4 | 0.5876 | 0.1109 | 0.2678 | 0.5949 | 0.6073 |
| weights^4 | dead_n4 | 0.5733 | **0.0884** | 0.2892 | 0.6031 | 0.6073 |
| nearest frame only | dead_n4 | 0.5303 | 0.0841 | **0.4207** | 0.5900 | 0.6073 |
| height reject 0.25 | dead_n4 | 0.5978 | 0.0901 | 0.2180 | 0.5914 | 0.6073 |
| height reject 1 | dead_n4 | **0.6080** | 0.0892 | 0.2728 | 0.5777 | 0.6073 |
| spec | rock_n4 | **0.7767** | **0.0361** | **0.1059** | 0.4294 | 0.4072 |
| weights^2 | rock_n4 | 0.7737 | 0.0471 | 0.2842 | 0.5261 | 0.4072 |
| weights^4 | rock_n4 | 0.7540 | 0.0627 | 0.4837 | 0.5413 | 0.4072 |
| nearest frame only | rock_n4 | 0.7048 | 0.0646 | 0.5361 | 0.5392 | 0.4072 |
| height reject 0.25 | rock_n4 | 0.7042 | 0.0807 | 0.5068 | 0.5382 | 0.4072 |
| height reject 1 | rock_n4 | 0.7115 | 0.0829 | 0.3798 | 0.5375 | 0.4072 |
| height reject 4 span-steps | rock_n4 | 0.7746 | 0.0384 | 0.1063 | 0.4393 | 0.4072 |

FOUR THINGS THE MEASUREMENT SAYS THAT A GUESS WOULD HAVE GOT WRONG.

1. "POPPING IS THE PRICE OF SHARP WEIGHTS" IS NOT TRUE OF THE MEAN. Sharpening
   LOWERS the mean 2-degree change on three of the four trees (maple 0.1172 ->
   0.0895 at w^4, dead 0.1188 -> 0.0884, blast_n4 0.1943 -> 0.1778). The reason
   is plain once measured: a sharper weight makes the card look like ONE frame
   for longer, so most 2-degree steps change less, not more.

2. THE PRICE IS IN THE MAXIMUM, AND ONLY FOR NEAREST-ONLY AND FOR THE TIGHTEST
   REJECTION. blast_n4 worst step: spec 0.4242, w^2 0.4139, w^4 0.4381 -- flat --
   but NEAREST-ONLY 0.8640, i.e. a single 2-degree step that throws away 86 per
   cent of the silhouette. maple 0.1885 -> 0.5368, dead 0.2912 -> 0.4207, rock
   0.1059 -> 0.5361. Nearest-only is a visible snap on every subject. The
   0.25-span-step rejection pops too (blast_n4 0.7606, rock 0.5068); the
   1-span-step rejection does not (0.4194 / 0.3798) and keeps most of the gain.

3. THE ROCK IS THE SUBJECT THAT REFUSES ALL OF IT. Every variant is worse than
   spec on the rock, and its 30-degree card change (0.4294 at spec) is ALREADY
   above the mesh's own 0.4072, so the rock is the one subject where the card is
   already less stable than the thing it replaces. Sharpen it and that goes to
   0.54. A repair applied to fat solid LOD is a regression; this must be per
   subject-class or not at all.

4. 4 SPAN-STEPS OF TOLERANCE IS A NO-OP. blast_n4 and maple return the spec
   numbers to four digits at k=4 and rock to within 0.002 -- the rejection is
   doing nothing at that tolerance, which is the control proving the k=0.25 and
   k=1 rows are the rejection and not a coding accident.

THE HONEST RECOMMENDATION FOR N=4 GHOSTING: the height-consistency rejection at
1 span-step (16/255 of depthSpan). Trees: blast_n4 +0.0219, blast_n8 +0.0047,
maple +0.0074, dead +0.0276, with the worst 2-degree step UNCHANGED or better
(blast_n4 0.4242 -> 0.4194, blast_n8 0.3809 -> 0.3737, maple 0.1885 -> 0.1680).
Rock: -0.0652, so the rock does not get it. At 0.25 span-step the trees gain
about twice as much and pay with a visible pop; that is a trade for bungo, not
for a lane.

### 2b. N=8 AND N=12 AS THE HONEST ALTERNATIVE

N=8 is the only larger sheet that can be scored honestly, because registration
is fitted against the harness's own CARD grab and only blast_n4 and blast_n8
have one. Both numbers below are the harness's, not this lane's:

| N | sheet | texels | harness IoU | this instrument |
|---|---|---|---|---|
| 4 | 192 x 512 | 98,304 | 0.5038 | 0.5086 |
| 8 | 384 x 1024 | 393,216 | 0.6754 | 0.6719 |

+0.172 IoU for 4x the texels and 4x the sheet memory. Every shader repair in
this report together is worth about +0.07 on this subject. N=8 IS THE BIGGEST
SINGLE LEVER ON THE TABLE and it needs no code at all -- it is a bake
parameter.

THE TEMPORAL COLUMNS DO NOT NEED REGISTRATION, because they compare a card with
itself. Those are valid for every N, including the two that cannot be scored:

| N | 2deg mean | 2deg MAX |
|---|---|---|
| 4 | 0.1943 | 0.4242 |
| 5 | 0.3917 | 0.5633 |
| 8 | 0.2179 | 0.3809 |
| 12 | **0.1860** | **0.3236** |

N=12 is the steadiest card in the set on both columns. N=5 is the worst by a
wide margin on both -- worse than N=4 -- which is a defect worth its own lane
and is NOT explained by anything in this report.

WHAT A BUILD LANE MUST PRODUCE for the N=12 IoU: the harness card and mesh grabs
for blast_n12 and blast_n5, i.e.
`control/blast_n12_after_b1/v_az%03d_el%02d_{card,mesh}.png` over the same 24
views. Without a card grab there is no ruler to register against and any IoU
printed for N=12 would be this lane choosing its own scale -- exactly the
flattering-instrument failure the skill warns about. The 0.6309 that falls out
of borrowing N=4's ruler is NOT REPORTED AS A RESULT for that reason.


## 7. THE RANKED LIST

Ranked by measured gain per unit of risk. "Gain" is always against the sheets
and shader AS THEY ARE TODAY, measured by this lane's instrument over the same
24 orbit views; this instrument's own +0.0043 mean bias against the harness is
in section 0b, so anything under 0.011 is marked NOT PROVEN.

### 1. BAKE AT N=8 INSTEAD OF N=4 (no code at all)

| subject | today (N=4) | N=8 | gain |
|---|---|---|---|
| blasted tree | 0.5038 (harness) | 0.6754 (harness) | +0.1716 |
| the other four | no N=8 bake exists | -- | -- |

COST: 4x the sheet texels and 4x the VRAM per card (192x512 -> 384x1024), 4x
the bake time. Temporal: the mean 2-degree change WORSENS 0.1943 -> 0.2179, the
worst 2-degree step IMPROVES 0.4242 -> 0.3809.
SPEC IMPACT: none. Line 232-233 already describes any N.
WHERE: N is a parameter of the card set, not a source constant; a build lane
changes the lodgen invocation.
GATE ROW THAT WOULD FAIL TODAY: "every shipped impostor card set has N >= 8".
blast_n4, maple_n4, dead_n4, rock_n4 -- 4 of 4 fail.
CAVEAT: measured on ONE subject. Bake N=8 for the maple, the dead tree and the
rock before this is ranked first on evidence rather than on one tree.

### 2. REPLACE THE OUTSIDE-COVERAGE HEIGHT FILL WITH AN 8-TEXEL DILATION (R2d8)

| subject | today (R3) | R2d8 | gain |
|---|---|---|---|
| blast_n4 | 0.4978 | 0.5646 | +0.0668 |
| blast_n8 | 0.6639 | 0.7200 | +0.0561 |
| rock_n4  | 0.7777 | 0.8331 | +0.0554 |
| dead_n4  | 0.5826 | 0.6153 | +0.0327 |
| maple_n4 | 0.3609 | 0.3685 | +0.0076 NOT PROVEN |

COST: a rebake of every card set. One extra dilation pass over the height
channel per frame. No runtime cost at all.
SPEC IMPACT: NONE. The spec never says what the height is outside coverage.
WHERE: src/lodgen.cpp:2659-2660, inside lodgenRepairOctHeight
(src/lodgen.cpp:2585). Today that branch is

    b = 128;   // the card plane: the parallax step is then an exact no-op
    outside++;

It becomes: for the first 8 texels beyond the coverage floor take the height of
the nearest covered texel -- the same dilation the 16..250 band already gets --
and the card plane only beyond 8 texels. The doc comment at
src/lodgen.cpp:2560-2571 must be rewritten with it: its candidate table was
scored on ONE subject against the N=12 set and it never tested a dilation that
reached OUTSIDE the coverage.
GATE ROW THAT WOULD FAIL TODAY: "in every frame, no texel within 8 texels of a
covered texel decodes to the card plane while its nearest covered neighbour is
more than 2 levels off the plane." rock_n4 has 207,836 outside texels, 97.4 per
cent of them at the plane, with the covered surface at h = 92.6 -- the row fails
on every frame of every subject.
THIS ALSO UNDOES THE ROCK REGRESSION: 0.8331 is +0.039 above the pre-repair
0.7944, so the rock is not being traded away to buy it.

### 3. RAISE THE DEFAULT ALPHA THRESHOLD FROM 0.0627 TO 0.20

| subject | 0.063 today | 0.200 | gain |
|---|---|---|---|
| blast_n8 | 0.6737 | 0.7194 | +0.0457 |
| blast_n4 | 0.5090 | 0.5437 | +0.0347 |
| maple_n4 | 0.3586 | 0.3775 | +0.0189 |
| dead_n4  | 0.5824 | 0.6010 | +0.0186 |
| rock_n4  | 0.7767 | 0.7842 | +0.0075 NOT PROVEN |

COST: one float. No rebake, no memory, no bake time. The risk, named and
measured: ZERO of the 24 views on ANY of the five subjects loses its card at
any threshold up to 0.45.
SPEC IMPACT: WORDING. Line 348 already says a consumer "alpha-tests at 0.5 for
full crowns and tests lower, or blends, for bare trees"; 0.20 is inside that
sentence, and it would be honest to write the measured number into it.
WHERE: src/gl/impostordraw.cpp:477-478. The fallback
set.covOk() ? float( set.covFloor ) / 255.0f : 0.5f is what makes the default
0.0627. covFloor is the sheet's ENCODING floor and was never a display cut.
GATE ROW THAT WOULD FAIL TODAY: "card ink is within 25 per cent of mesh ink,
per view, on every subject." At 0.0627 the maple paints 188 per cent of the
mesh's ink and blast_n4 151 per cent: 2 of 5 fail today, 0 of 5 at 0.20.

### 4. THE HEIGHT-CONSISTENCY REJECTION BETWEEN FRAMES, TOLERANCE 1 SPAN-STEP

| subject | spec blend | reject k=1 | gain |
|---|---|---|---|
| dead_n4  | 0.5804 | 0.6080 | +0.0276 |
| blast_n4 | 0.5086 | 0.5305 | +0.0219 |
| maple_n4 | 0.3584 | 0.3658 | +0.0074 NOT PROVEN |
| blast_n8 | 0.6719 | 0.6766 | +0.0047 NOT PROVEN |
| rock_n4  | 0.7767 | 0.7115 | -0.0652 REGRESSION |

COST: up to three extra normal-sheet taps in the blend loop. TEMPORAL COST:
none measured -- the worst 2-degree step is unchanged or better on every tree
(blast_n4 0.4242 -> 0.4194, blast_n8 0.3809 -> 0.3737, maple 0.1885 -> 0.1680,
dead 0.2912 -> 0.2728). It MUST NOT be applied to fat solid LOD: the rock
loses 0.065, so this is per subject-class or not at all.
SPEC IMPACT: WORDING. Lines 313-315 already list "ghost-free frame blending" as
a use of the height channel; the spec must be given the rejection RULE, its
tolerance in span-steps, and a sentence saying which subjects get it.
WHERE: res/shaders/impostor_oct.frag, the useHeightBlend branch at :214-232;
the uniform is set at src/gl/impostordraw.cpp:480.
GATE ROW THAT WOULD FAIL TODAY: "no more than 20 per cent of a card's ink is
painted by frames other than the nearest, in the bottom quarter of the card."
Today blast_n4 45.0 per cent, maple 45.7, dead 45.5, rock 28.1, blast_n8 21.7
-- 5 of 5 fail.

### 5. SWAP THE _n HEIGHT AND SWAY CHANNELS

| subject | height in blue | height in alpha | gain |
|---|---|---|---|
| blast_n4 | 0.4978 | 0.5362 | +0.0383 |
| blast_n8 | 0.6639 | 0.7010 | +0.0372 |
| rock_n4  | 0.7777 | 0.7823 | +0.0046 NOT PROVEN |
| dead_n4  | 0.5826 | 0.5870 | +0.0044 NOT PROVEN |
| maple_n4 | 0.3609 | 0.3629 | +0.0020 NOT PROVEN |

The number that is not the IoU: mean height error falls from 2.3-3.4 levels to
0.06-0.22, worst case from 95 levels to 5; on the rock that is 1,881 world
units of worst-case depth error down to 99.
COST: sway's mean error 4-8x worse (1.5 -> 6.9 levels), normal X about 1.5
levels worse. Every baked sheet is invalidated.
SPEC IMPACT: FORMAT CONTRACT = A RULING. docs/LODGEN_IMPOSTOR_SPEC.md:45
changes and the .lodm needs a version bump so an old sheet is refused rather
than read backwards.
WHERE: the write at src/nifskope_ui.cpp:23272, the sheet writer in
src/lodgen.cpp, and the reads in res/shaders/impostor_oct.frag at :217, :292
and :293 (n.b and n.a exchanged).
GATE ROW THAT WOULD FAIL TODAY: "the height channel round-trips through the
sheet's own compression to within 1 level at the 95th percentile." Today
blast_n4 6.06, blast_n8 6.49, maple 6.13, dead 7.87, rock 9.90 -- 5 of 5 fail.
Swapped, all five are at 1.00 or below.

### 6. FIX THE BC3 ALPHA DECODER IN THE GATE ITSELF (a repair to the RULER)

No IoU. tests/spells/impostor_bc_decode.py:26-28 decodes the eight-level alpha
ramp one step short, worst error 36.4 of 255, always LOW, so gate row 14
silently skips covered texels whose true encoded alpha is 160..196.
COST: three lines. SPEC IMPACT: none.
WHERE: tests/spells/impostor_bc_decode.py:26-28; corrected copy at
scratchpad/impostorfix2_20260919/bcdec2.py.
GATE ROW THAT WOULD FAIL TODAY: a known-answer row -- decode a synthetic BC3
block with a0=255, a1=0, indices 0..7, and compare with the D3D ramp. Today 6
of 8 entries are wrong. NOTHING IN THE TREE TESTS THE DECODER, which is why
this survived a day of use by two lanes.

### NOT A REPAIR: THE FRAME DOWNSAMPLE

Six rules measured (section 1). Best minus box is +0.0036 on the maple and
+0.0006 / +0.0025 on the bare trees, all inside this instrument's own
uncertainty, and the max-coverage variants the brief asked about are WORSE on
three of the four trees. The 128x256 maple sheet with NO downsample scores
0.3384, BELOW the box average of itself at 32x64. Resolution is not what the
maple is short of. Do not spend a lane on it.

### WHAT A BUILD LANE MUST PRODUCE BEFORE THE NEXT SIMULATION

1. inner dumped at src/nifskope_ui.cpp:23115 BEFORE the .scaled(...) call, as
   PNG, per view, for matte() and channel(9), on blast_n4 and maple_n4 -- to
   settle section 1 at the real ratio instead of ratio 4.
2. N=8 bakes for maple_n4, dead_n4 and rock_n4; ranked repair 1 rests on one
   subject.
3. Harness card AND mesh grabs for blast_n12 and blast_n5 over the same 24
   views. Without a card grab there is no ruler to register an N=12 IoU
   against, and N=5's temporal number (2-degree mean 0.3917, WORSE than N=4's
   0.1943) is an unexplained defect that needs its own lane.


## 8. THE PICTURES

All under scratchpad/impostorfix2_20260919/images/.

* `02_height_fill_R3_vs_R2d8.png` -- ranked repair 2, four views, the height
  fill that shipped today on top and the 8-texel dilation below. The rock's red
  bottom band (mesh the card does not reach) collapses; blast_n4's flared,
  doubled trunk base becomes one trunk. rock az 60 +0.1110, rock az 90 +0.0796,
  blast az 30 +0.1267, dead az 60 +0.0098.
* `03_rock_worst_before_after.png` -- section 3, the regression. Mesh grab on
  top, the shipped card in the middle, today's repaired card at the bottom, for
  the three worst views and the one that gained. The repaired card has the right
  amount of ink and the wrong position: a thick red band along the bottom edge.
* `04_ghost_minor_frames.png` -- section 4, the cause of the doubled trunk. Row
  2 is the GHOST MAP: grey is what the nearest frame paints, ORANGE is ink that
  ONLY the minor frames paint. At az 30 that is 45.2 per cent of the card and at
  az 0 it is 53.2 per cent, and it is shaped like a second trunk. Rows 3 and 4
  are weights^4 and the height rejection cleaning it up.

## 9. WHAT THIS LANE DID NOT DO, AND WHAT IS NOT PROVEN

* No build, no exe, no test that starts the exe, no edit to any file under src/
  or res/, no commit, no git stash, no edit to WW_CHANGES.md or HANDOFF.md.
* Nothing here is "fixed". Every number is a SIMULATION by a numpy
  reproduction of the shader, controlled against the application to within
  0.0101 on five subjects but never run by the application.
* The N=8 recommendation rests on ONE subject.
* Section 1 is measured at ratio 4, not at the bake's real ratio, because the
  pre-downsample photographs do not exist on disk.
* N=12 and N=5 have no harness card grab, so no IoU is reported for them.
* Every "NOT PROVEN" row in section 7 is inside this instrument's own bias
  against the harness and must not be quoted as a gain.
