# IMPOSTORFIX4 -- what is still wrong with the impostor card, decided OFFLINE

Lane IMPOSTORFIX4, 2026-09-19, launched 18:30 CEDT, first `date` read 18:31:45
CEDT. OFFLINE ONLY: no build, no `release/NifSkope.exe`, no rung exe, no
exe-starting spell, no in-place edit of an existing `src/`, shader or
`tests/spells` file. Lane CELLVIEW3 owns the build slot and the exe slot.

NOTE ON THE FILENAME: this lane's harness refuses to write `report.md`
(it blocks report/summary/findings .md writes). The brief's own fallback
("if you cannot write report.md, write DELIVERABLE_TEXT.md") is taken. This
file IS the report.

## THE FOUR VERDICTS

    1  FAT INK            SHEET + SPEC LIMIT   1.441 -> 1.133 simulated (blast_n4)
                          largest non-ruling share = the 3-frame combination,
                          and NO combination rule wins on all five subjects
    2  BLOCK CHIPS        SHEET                0.69% -> 0.01% of texels >12 levels
                          (blast_n4); IoU 0.5709 -> 0.5706, i.e. UNMOVED except
                          dead_n4 0.6139 -> 0.6273. Not the compressor, not the
                          silhouette edge: the block's own height RANGE.
    3  LEAFY MAPLE 0.37   BAKE                 sheet ink inflation 2.357x before
                          anything is drawn, against 1.04..1.26 for every other
                          subject. Its own control CANNOT BE SHOT OFFLINE.
    4  THIN AT 16 px      SPEC LIMIT           no simulated gain is offered: the
                          obvious repair (wire up cardMipCap) MAKES IT WORSE,
                          0.3615 -> 0.2124 at 16 px on blast_n4.

## DELIVERABLES

    the report                 scratchpad/impostorfix4_20260919/DELIVERABLE_TEXT.md
    the one refusing script    scratchpad/impostorfix4_20260919/hookup_ramp.py
    the three telling pictures scratchpad/impostorfix4_20260919/pic_chip_blast_n4.png
                               scratchpad/impostorfix4_20260919/pic_chip_rock_n4.png
                               scratchpad/impostorfix4_20260919/pic_chip_maple_n4.png
    MISTAKES.md                three entries spliced at the top (CR 10360 -> 10433,
                               +73 = the entries' own CR count, LF the same)
    skill                      .claude/skills/ww-reference-card-diagnose/SKILL.md
                               in BOTH trees, sha1 269413055a64e292aee11455b05075eb734954c8

`hookup_ramp.py --check` (the default; it writes nothing):

    src/lodgen.cpp     replace  matches 1  OK
    src/lodgen.cpp     replace  matches 1  OK
    lodgen.cpp  bytes 635514  CR 0  LF 14438
    --check only: nothing written. Pass --apply to write.

WHY THE OTHER REPAIRS HAVE NO SCRIPT: every one of them either needs a ruling
(the threshold, the frame combination) or was REFUTED by this lane's own
measurement (PCA endpoints, `cardMipCap`, the colour dilate, the BC3 alpha
ramp). The brief asks for a script per repair needing NO ruling; after the
measurements there is exactly one.

WHAT THE PICTURES SHOW. `pic_chip_blast_n4.png` is the telling one: the marked
texels sit out in the dilated field, not on the white trunk, and panel 3's
ramped field is visibly smooth where panel 2's has a step.
`pic_chip_rock_n4.png` is the counter-example kept deliberately -- the rock is
the one subject whose bad blocks ARE enriched on the silhouette (14.5 per cent
against a 6.6 base rate), and its worst block is on the edge, which is why the
page's header carries the subject's own two percentages and says the crop is
not typical. `pic_chip_maple_n4.png` is the crown, where 32.9 levels of block
range meet a 29.7 per cent edge base rate.

## 0. Read, inputs on disk, and what this lane may and may not conclude

READ IN ORDER: `CONSTITUTION.md`; `HANDOFF.md` top block;
`scratchpad/impostorfix3_20260919/report.md` (all sections);
`scratchpad/impostorfix2_20260919/report.md` s7 (the ranked list);
`scratchpad/octf1_20260919/report.md`; `docs/LODGEN_IMPOSTOR_SPEC.md`;
`res/shaders/impostor_oct.{vert,frag}`; `src/lodgen.cpp`
(`lodgenRepairOctHeight`, `lodgenDilateFrames`, frameOffset); the bake in
`src/nifskope_ui.cpp`. Skills: `ww-reference-card-diagnose`,
`ww-simulate-before-build`, `ww-anchored-hookup`, `ww-silhouette-compare`,
`ww-texel-picture`.

### State at launch, as IMPOSTORFIX3 left it (24 views, real exe, 8-ring fill)

    bare maple N=4  0.5736      bare maple N=8  0.7483
    dead tree N=4   0.6073      leafy maple N=4 0.3674
    rock N=4        0.8305

### The inputs that exist on disk

    scratchpad/impostorfix3_20260919/fixture/<subj>/         the 8-ring bake
      cards/<id>_oct.lodm, cards_before/, cards_r3/
      textures/data/fo4cslod/cards/<id>_oct_{d,n,g,gsaos}.dds, <id>_fs.dds
    scratchpad/impostorfix3_20260919/fixture_r3/<subj>/      the R3 control
    scratchpad/impostorfix3_20260919/control/<subj>_cards_b1/ harness card grabs
    scratchpad/impostorshow_20260919/fixture/<set>/          mesh + card orbits

Subjects: blast_n4 / blast_n8 (bare blasted maple, `000531b3`),
maple_n4 (leafy forest maple, `0004a074`), dead_n4 (`001236b4`),
rock_n4 (RockCliff02_Alt, `000211a3`).

### What this lane may and may not conclude

Everything below is a SIMULATION on decoded sheets and existing harness grabs,
except where a number is quoted from IMPOSTORFIX1/2/3's own in-application
runs, which is said each time. The build lane makes a simulation real. The
known-answer control (0.8823 on blast N=4) reproduces before any number of
this lane's counts. All 24 views, mean AND worst, never a picked view.

## 0b. THE KNOWN-ANSWER CONTROLS. Three of them. One of them broke a hypothesis.

### Control 1 -- the instrument reproduces IMPOSTORFIX1's 0.8823

`s0_control.py`. blast_n4, the SIXTEEN bake directions, blend OFF, numpy card
registered card-against-card and then scored against the MESH grabs in
`impostorfix1_20260919/control/blast_n4_bake/`.

    IMPOSTORFIX1, in the application   mean 0.8823   range 0.8563 .. 0.9107
    this lane, in numpy                mean 0.8785   range 0.8570 .. 0.9143

-0.0038 on the mean. The control passes.

A SECOND CHECK FALLS OUT OF IT FOR FREE. Run it on all three sheet sets
(fix1's shipped, fix1's repair, fix3's 8-ring) and the number is 0.8785 to
four digits on every one. It must be: with the blend off at a bake direction
the parallax step is a mathematical no-op, so the height channel is never
read, and those three sets differ in NOTHING BUT the height channel. An
instrument that moved here would be reading something it cannot see.

### Control 2 -- the instrument reproduces all TEN of IMPOSTORFIX3's numbers

`cal4.py`, cached in `calib4.json`. 24 orbit views, scale plus integer
per-view offset fitted card-against-card, then FROZEN for every table below.

    subject    8-ring: mine / harness      R3: mine / harness
    blast_n4    0.5763 / 0.5736  +0.0027   0.5086 / 0.5038  +0.0048
    blast_n8    0.7479 / 0.7483  -0.0004   0.6739 / 0.6754  -0.0015
    maple_n4    0.3713 / 0.3674  +0.0039   0.3585 / 0.3545  +0.0040
    dead_n4     0.6165 / 0.6073  +0.0092   0.5834 / 0.5721  +0.0113
    rock_n4     0.8325 / 0.8305  +0.0020   0.7792 / 0.7724  +0.0068

Mean bias +0.0044, worst +0.0113. **ANY SIMULATED GAIN BELOW 0.011 IN THIS
REPORT IS MARKED NOT PROVEN** and is not ranked on.

### Control 3 -- the model of the BAKE TAIL reproduces the shipped DDS BYTES

Written because its first version FAILED, and the failure was the finding
(section 2).

    reconstructed input -> my model of lodgenEncodeBC1Block -> shipped `_n` bytes
    blast_n4  endpoints 100.00%  indices 99.97%
    blast_n8  endpoints 100.00%  indices 99.98%
    maple_n4  endpoints 100.00%  indices 99.90%
    dead_n4   endpoints 100.00%  indices 99.98%
    rock_n4   endpoints 100.00%  indices 99.92%

The 0.02..0.10 per cent of indices that differ are float tie-breaks in the
nearest-palette search, worth under one level each.

## 2. BLOCK CHIPS AT THE TRUNK -- VERDICT: **SHEET**. But not the compressor,
##    and the repair this lane arrived expecting is REFUTED BY MEASUREMENT.

### What was expected, and why it was wrong

`lodgenEncodeBC1Block` (src/lodgen.cpp:4571) picks a block's two endpoints as
its MIN- and MAX-LUMINANCE texels:

    l = 0.299 R + 0.587 G + 0.114 B

On the `_n` sheet R and G are the normal and **B is the height**, so that
choice weights the height at eleven per cent. It looked like the mechanism for
a 4x4-shaped artefact, and a first pass (`s2_chips.py`) seemed to confirm it:
the worst blocks' blue error read 53..101 levels against an ideal fit's 4..10.

**THAT PASS MEASURED THE WRONG THING AND ITS NUMBERS ARE WITHDRAWN.** The
`<id>_oct_normal.png` beside the DDS is the BAKE's output, written BEFORE
`lodgenDilateFrames` and BEFORE `lodgenRepairOctHeight` (src/lodgen.cpp:3079
and 3088; the comment at 3102 says the sheets are "converted once from the
bake's PNGs"). Encoding that PNG reproduced only 0.9..17.4 per cent of the
shipped endpoint bytes -- the control that caught it. Most of what the first
pass called compression error was the dilate and the height repair doing
exactly their job. Every number in `s2_chips.json` and every "ours vs ideal"
ratio in `s2b_bound.json` is SUPERSEDED and must not be quoted.

### The measurement on the encoder's real input

`s2d_reconstruct.py` rebuilds that input by running the source's own sequence
-- dilate `nrm` against `alb` with deep = max(8, max(tileW,tileH)/8), dilate
`alb` against itself (alpha kept), then `lodgenRepairOctHeight` with
kOutRings = 8 -- and control 3 says the rebuild is exact.

    height round trip, in LEVELS, on the TRUE input
    tag        mean L  mean PCA |  p95 L  p95 PCA | worst L  worst PCA
    blast_n4     3.16      3.12 |   4.00     4.00 |      33         33
    blast_n8     3.28      3.24 |   4.00     4.00 |      49         38
    maple_n4     3.71      3.59 |   5.00     5.00 |      70         70
    dead_n4      3.43      3.38 |   5.00     5.00 |      54         54
    rock_n4      3.78      3.72 |   5.67     5.00 |      87         84

**BC1 endpoint selection is not the defect, and the principal-axis repair is
not worth building.** It moves the mean by 0.04..0.12 levels and the 95th
percentile by zero on four of five subjects -- an order of magnitude inside
the instrument's own bias. The luminance rule is not costing the height
channel. REJECTED, WITH NUMBERS; it should not be re-proposed without new
evidence. (Its refuter, if someone wants one: a subject whose bad blocks have
LOW blue range but high blue error would put the endpoint rule back on trial.)

### What the chips actually are

`s2e_localise.py`, on the validated input. Texels whose decoded height is off
by more than 12 levels (12 levels is the gate's own row-14b unit):

    tag        texels>12lv  of all   blocks>12lv    ON A SILHOUETTE    BLUE RANGE
                                                    bad / base rate    bad / base
    blast_n4        675      0.69%    122 /  6144    4.1% / 15.0%    19.1 / 1.8 lv
    blast_n8       3561      0.91%    715 / 24576    6.0% / 14.5%    17.7 / 2.0 lv
    maple_n4        742      2.26%    134 /  2048   32.1% / 29.7%    32.9 / 3.9 lv
    dead_n4        1061      1.62%    190 /  4096    8.9% / 22.0%    27.5 / 3.4 lv
    rock_n4        3688      1.41%    883 / 16384   14.5% /  6.6%    30.9 / 4.3 lv

TWO THINGS ARE SETTLED, both against the hypothesis the brief handed me.

**It is NOT the silhouette edge.** On four of five subjects a bad block is
LESS likely to straddle the silhouette than a block picked at random. The
brief's "BC3 endpoint error on the height channel across a silhouette edge"
is refuted on its own terms.

**It IS the height's own range inside one 4x4.** A bad block carries
17.7..32.9 levels of height range where a typical block carries 1.8..4.3 --
seven to ten times. A BC1 block has four levels on one line, so a 30-level
range quantises in 10-level steps; 12 levels is 145 world units on the trees
and 238 on the rock, and the worst rock block is 87 levels = 1,722 units
against half-extents of 1,728. That is a chip, and it is 4x4 shaped because
the block is.

**WHAT IS NOT SETTLED, AND I AM NOT GUESSING IT.** WHERE those large-range
blocks come from is not measured. Two candidates stand: the `kOutRings = 8`
CLIFF (ring 8 carries the object's height, ring 9 snaps to 128, nothing ramps
between them) and genuine depth discontinuities inside the object. Separating
them is one cheap cross-tab of the ring map against block range, on the
`nrm_true_<tag>.npy` this lane leaves on disk. IMPOSTORFIX2 tested ring
COUNTS (8 / 16 / whole frame) and never tested a RAMP, so the ramp is the one
untried lever and it is ranked in section 5 as UNMEASURED, not as a gain.

## 1. FAT INK -- what IS and IS NOT in the excess

Launch state, from IMPOSTORFIX3's in-application run: ink ratio 1.425
(blast_n4), 1.224 (blast_n8), 1.876 (maple_n4), 1.109 (dead_n4), 1.110
(rock_n4).

### Stage C settled without a render: THE COLOUR DILATE DOES NOT BLEED COVERAGE

`lodgenDilateFrames` computes `isCoverage = ( &img == &coverage )`
(src/lodgen.cpp:2494) and at src/lodgen.cpp:3083 it is called as
`lodgenDilateFrames( alb, alb, ... )`, so `isCoverage` is TRUE and its `put`
writes back `qAlpha( img.pixel( x, y ) )` -- the alpha the texel already had.
**This stage's share of the fat ink is ZERO**, from the source.

The corroboration, and ITS LIMIT, stated because I nearly fooled myself with
it: across all five subjects there is no `_d` alpha strictly between 0 and 16
anywhere, PNG or DDS. That is consistent with no bleed, but it is a WEAK
refuter on its own -- a dilate that bled would write the frame MEAN, which is
normally >= 16 and would have been counted as "covered" by that test and
never seen. The source is the proof; the count is corroboration only.

### Stage E settled without a render: THE DRAWER NEVER PICKS A MIP

Every sheet fetch in `res/shaders/impostor_oct.frag` is
`textureLod( ..., 0.0 )` -- lines 217, 267, 275, 276, 295, 297. The drawer
uploads `cardMipCap` (src/gl/impostordraw.cpp:466); the shader DECLARES it at
line 72 and never mentions it again. So "mip choice" contributes NOTHING to
the fat ink at the 24-view size, because there is no mip choice at all. It is
not a fat-ink stage. It is defect 4, and this is where defect 4 comes from.

### The leafy maple's coverage census, which is why it is its own defect

`_d` alpha over the whole sheet, pre-compression PNG:

    tag        texels   full (>=250)   partial (16..249)   empty (<16)
    blast_n4    98304    6879  7.0%     4271   4.3%        87154  88.7%
    blast_n8   393216   24799  6.3%    16114   4.1%       352303  89.6%
    maple_n4    32768     297  0.9%     4762  14.5%        27709  84.6%
    dead_n4     65536    4919  7.5%     3901   6.0%        56716  86.5%
    rock_n4    262144   51389 19.6%     4692   1.8%       206063  78.6%

The leafy maple is the ONLY subject where partial coverage outnumbers full,
and it does so SIXTEEN TO ONE. Every other subject has more whole texels than
partial ones. Two consequences, both load-bearing further down: the default
alpha cut IS the coverage floor, so all 14.5 per cent of those partials paint;
and `lodgenRepairOctHeight` seeds only from alpha >= 250, so the maple's
entire height field is carried outward from 297 texels in 32,768.

### The stage table -- ONE thing changed per row, same 24 views, same registration

`s1_stages.py`, `s1_stages.txt`. Each cell is `ink / IoU`; ink is card-covered
pixels over mesh-covered pixels, 1.000 is the mesh itself.

    stage                     blast_n4        blast_n8        maple_n4         dead_n4         rock_n4
    today (shipping)      1.441/0.5763    1.237/0.7479    1.894/0.3713    1.168/0.6165    1.120/0.8325
    thr 0.20 (ruling A)   1.133/0.6179    1.096/0.7885    1.059/0.4044    0.932/0.6395    1.053/0.8499
    thr 0.50 (crowns)     0.632/0.5380    0.841/0.7778    0.258/0.2089    0.582/0.5331    0.830/0.7966
    pre-BC3 alpha         1.437/0.5765    1.233/0.7493    1.876/0.3736    1.163/0.6175    1.120/0.8327
    1 frame (no union)    0.903/0.6133    1.040/0.7788    1.414/0.3659    0.832/0.5865    0.847/0.7893
    no parallax           2.387/0.2952    1.749/0.4896    3.191/0.2219    2.056/0.3425    2.254/0.4988
    mip 1 (counterfact.)  1.222/0.5933    1.054/0.7818    1.180/0.3381    0.960/0.6193    1.089/0.8366

Ink removed by each stage, against today:

    stage                 blast_n4  blast_n8  maple_n4   dead_n4   rock_n4
    threshold 0.20 RULING   -0.308    -0.141    -0.835    -0.236    -0.067
    three frames -> one     -0.538    -0.197    -0.480    -0.336    -0.273
    BC3 alpha ramp          -0.004    -0.004    -0.018    -0.005    -0.000
    parallax (ON removes)   +0.946    +0.512   +1.297     +0.888    +1.134

### What the table settles

**THE BC3 ALPHA RAMP IS NOT A CAUSE.** Taking `_d` alpha from the
pre-compression PNG instead of the decoded DDS moves ink by 0.004 and IoU by
0.0002..0.0023 -- two orders of magnitude inside the bias. The eight-level
BC4 ramp over a per-block min/max is doing its job on coverage. Off the list.

**PARALLAX IS NOT A CAUSE, IT IS THE LARGEST CURE.** Turning the height step
off does not reduce smear, it more than doubles the ink (2.387, 1.749, 3.191,
2.056, 2.254) and takes 0.25..0.34 off the IoU on every subject. Whatever the
"parallax smear" in the brief is, it is not visible as ink at 24 views, and
the height step is the single most load-bearing thing the drawer does. Off
the list as a cause; it is why defect 2's height channel matters at all.

**THE LARGEST SHARE THAT IS NOT THE OWED RULING IS THE THREE-FRAME
COMBINATION**, on four of the five subjects. The mechanism is in the shader
and not inferred: coverage is accumulated as a weighted SUM and the SUM is cut
at the coverage floor, so with three weights near a third, a pixel ONE frame
alone sees as twenty per cent covered reaches 0.067 and paints exactly as
solidly as a pixel all three see whole. The three frames OR together.

**AND "USE ONE FRAME" IS NOT ITS REPAIR.** One frame helps blast_n4 (+0.0370)
and blast_n8 (+0.0309) and hurts dead_n4 (-0.0300) and rock_n4 (-0.0432).
Two wins and two losses is not a repair; it is a different trade. The
combination rules that might be one are measured in section 5 (`s6_vote.py`),
and a rule is only proposed there if it beats today by more than the
instrument's 0.011 bias on EVERY subject.

**THE OWED RULING'S OWN SHARE, reported and NOT applied.** Cutting at 0.20
instead of at the floor takes the ink to 1.133 / 1.096 / 1.059 / 0.932 /
1.053 -- within seven per cent of the mesh on four subjects -- and RAISES IoU
on all five (+0.042, +0.041, +0.033, +0.023, +0.017). Cutting at the spec's
0.50 for crowns is a disaster everywhere, and worst on the crown it was
written for: maple_n4 0.3713 -> 0.2089, ink 0.258. **The spec clause "0.5 for
full crowns" is contradicted by measurement on the only full crown in the
fixture**, and that is a fact the ruling needs before it is made.

## 3. LEAFY MAPLE 0.37 -- VERDICT: **BAKE**, with the threshold contract
##    second. The blend is not where it goes wrong.

### The control the brief asks for CANNOT BE RUN OFFLINE. Here is the shot.

The brief wants a single-frame known-answer control: the maple from one of its
own bake directions, against the mesh from that same direction. Mesh grabs at
a subject's own bake directions exist for exactly one subject --
`scratchpad/impostorfix1_20260919/control/blast_n4_bake/` -- and there is no
maple equivalent anywhere in the tree. **A BUILD LANE MUST SHOOT IT:**

    maple_n4 (0004a074), MESH and CARD, at the 16 bake directions of N=4,
    same camera, canvas and alpha handling as impostorfix1's blast_n4_bake,
    into scratchpad/<lane>/control/maple_n4_bake/.

Nothing below is offered as a substitute for that shot.

### What CAN be measured, and what it says

**A. THE SHEET'S OWN INK INFLATION -- no mesh needed, and it is the cleanest
number in this report.** A texel's coverage is a FRACTION. The honest area of
a frame is the SUM of those fractions. What the card paints is the COUNT of
texels over the threshold. The ratio is ink the threshold invents, before any
blend, any parallax, any view (`s3_maple.py` part A):

    tag         covered area   texels painted   x@floor   x@0.20   x@0.50
    blast_n4            9107            10992     1.207    1.177    1.005
    blast_n8           33277            40236     1.209    1.184    1.010
    maple_n4            2018             4758     2.357    1.902    0.768
    dead_n4             6828             8626     1.263    1.227    0.995
    rock_n4            53757            55869     1.039    1.033    1.000

**The maple's sheet is already 2.357x too much ink before anything is drawn**,
where every other subject is 1.04..1.26. That single number explains its 1.894
in-draw ink ratio almost by itself, and it is a property of the SHEET plus the
threshold, not of the blend, the union, the parallax or the compression.

**B. THE CENSUS THAT SAYS WHY.** From section 1: maple_n4 has 297 fully
covered texels in 32,768 (0.9 per cent) against 4,762 partial (14.5 per cent)
-- the only subject where partials outnumber full texels, and by sixteen to
one. A frame 128 texels wide is being asked to hold a crown whose every leaf
is narrower than a texel, so almost nothing comes out whole. With the cut at
the coverage floor, a texel that is six per cent leaf paints as solidly as a
trunk. THAT IS THE BAKE'S RESOLUTION MEETING THE THRESHOLD CONTRACT, and it is
why a full crown is the WORST subject rather than the easy case the spec
promises.

**C. IT IS NOT THE COMPRESSION AND IT IS NOT THE BLEND.** Pre-BC3 alpha moves
the maple by +0.0023 IoU and -0.018 ink (section 1) -- nothing. Single frame
moves it by -0.0054 IoU (section 1) -- nothing, and in the wrong direction for
a blend explanation. The maple is the ONE subject where the frame combination
is NOT the biggest non-ruling share; its own threshold share (-0.835 ink) is
three times the next largest.

**D. AND THE SPEC'S OWN NUMBER MAKES IT WORSE, NOT BETTER.** `thr 0.50` is the
spec's crown clause. On the crown it takes IoU from 0.3713 to 0.2089. The
clause is written for a crown that comes out of the bake mostly-whole; this
crown comes out mostly-partial, so the clause cuts the crown away.

## 4. THIN AT 16 px AND N=4 GHOSTING -- VERDICT: **SPEC LIMIT**, and the
##    obvious repair is REFUTED by its own measurement.

### The structural finding, from the source

Every sheet fetch in `res/shaders/impostor_oct.frag` is
`textureLod( ..., 0.0 )` -- lines 217, 267, 275, 276, 295, 297. `cardMipCap`
is uploaded at `src/gl/impostordraw.cpp:466` and declared at
`impostor_oct.frag:72`, and the shader never reads it. **At every apparent
size the card takes one bilinear tap out of mip 0.** When the card is 16 px
tall and a frame is 128 texels wide, one screen pixel spans eight texels and
seven of them are never looked at.

### The curve (`s4_dist.py`, `s4_dist.txt`), 24 views, frozen registration

The card is RENDERED at the on-screen height, not downsampled from a big
render, because the aliasing is the whole point. `mip k` box-downsamples both
sheets k times first, k chosen so one pixel is about one texel.

    tag        px   mip |  IoU mip0    ink |  IoU mip-k    ink
    blast_n4    8     4 |    0.0417  2.100 |    0.0000  0.000
    blast_n4   12     3 |    0.1454  4.375 |    0.1493  0.417
    blast_n4   16     3 |    0.3615  2.627 |    0.2124  0.255
    blast_n4   24     2 |    0.4967  1.620 |    0.4343  0.849
    blast_n4   32     2 |    0.4089  1.525 |    0.3827  0.906
    blast_n4   48     1 |    0.4882  1.495 |    0.4923  1.265
    blast_n4   64     1 |    0.5268  1.415 |    0.5410  1.243
    blast_n4   96     0 |    0.5825  1.405 |         =      =
    blast_n4  256     0 |    0.5913  1.359 |         =      =
    blast_n8   16     3 |    0.4230  2.355 |    0.2955  0.341
    blast_n8   24     2 |    0.6116  1.375 |    0.5805  0.717
    blast_n8   48     1 |    0.6015  1.192 |    0.5961  1.042
    blast_n8   64     1 |    0.6532  1.227 |    0.6708  1.073
    blast_n8   96     0 |    0.7594  1.227 |         =      =
    blast_n8  256     0 |    0.7657  1.167 |         =      =
    maple_n4   12     2 |    0.0573 10.182 |    0.1729  2.727
    maple_n4   16     2 |    0.1526  7.565 |    0.2798  1.859
    maple_n4   24     1 |    0.2656  3.521 |    0.3007  2.290
    maple_n4   48     0 |    0.2964  2.600 |         =      =
    maple_n4  256     0 |    0.4158  1.616 |         =      =

(The full eleven sizes for all three subjects are in `s4_dist.txt` and
`s4.json`.)

### THREE THINGS THIS SETTLES, AND ONE CONFOUND I WILL NOT HIDE

**THE KNEE IS AT 48..96 PIXELS, AND IT IS NOT SET BY N.** Taking the knee as
the size where IoU falls below nine tenths of its asymptote: blast_n4 between
48 and 64 px, blast_n8 between 64 and 96 px, maple_n4 at about 96 px. N=8
beats N=4 by 0.06..0.18 at EVERY size, small and large alike, in the same
proportion. **Raising N does not move the knee.** The knee is set by texels
per screen pixel, and N does not change that -- a bigger N spends the same
sheet on more frames, so each frame is SMALLER and the knee if anything moves
the wrong way. Anyone reaching for N=8 to fix the 16 px card is reaching for
the wrong lever, and the same table shows N=8 is worth having for a different
reason: it is uniformly better everywhere above the knee.

**THE CARD AT 16 px IS NOT THIN, IT IS FAT.** ink 2.627 (blast_n4), 2.355
(blast_n8), 7.565 (maple_n4) against 1.36..1.62 at 256 px. The card takes one
mip-0 tap per pixel, that tap lands on a covered texel or it does not, and
with the cut at the coverage floor it paints whenever it lands on anything.
IMPOSTORFIX3's in-application "thin at 16 px, IoU 0.450" and this are not in
conflict about the IoU -- they are about what the loss is made of, and here
it is ink in the wrong places, not missing ink.

**WIRING UP `cardMipCap` WOULD MAKE BARE TREES WORSE.** This is the refuter I
wrote before running it, and it fired. At 16 px a correct mip takes blast_n4
from 0.3615 DOWN to 0.2124 and blast_n8 from 0.4230 DOWN to 0.2955, because
box-downsampling coverage and then cutting at the floor deletes every branch
narrower than the new texel -- ink collapses to 0.255 and 0.341, an almost
empty card. On the maple, which is over-covered, the same mip HELPS: 0.1526 ->
0.2798 at 16 px and 0.0573 -> 0.1729 at 12 px. **A mip is not a repair for
this defect. It trades one subject's failure for another's.** What is NOT
tested, and what I am naming rather than proposing, is a COVERAGE-PRESERVING
mip chain (each level's alpha rescaled so the level's covered AREA matches mip
0's, the alpha-to-coverage trick) -- that is the only version of this repair
that could win on both, and nobody has measured it.

**THE CONFOUND.** Below about 32 px two things are approximations. The mesh
reference is the 24-view mesh grab area-averaged down and cut at half
coverage, which is not the same as rendering the mesh at 16 px with its own
alpha test, and it is biased THIN for a bare tree. And the frozen registration
is an integer offset scaled by the size factor, so it carries up to half a
pixel of error, which at 16 px is three per cent of the card. The dip at 32 px
on both blast subjects (0.4089 and 0.4039, below their own 24 px rows) is that
quantisation showing, not the card. **Read the trend and the mip comparison,
not the absolute value of any row below 48 px.** The mip comparison is safe
from both, because both arms of it carry the identical reference and the
identical offset.

### What a consumer should do below the knee -- SPEC WORDING, not a default

Proposed for `docs/LODGEN_IMPOSTOR_SPEC.md`, as text for the overseer to rule
on, NOT applied by this lane and NOT a code default:

> **Apparent size.** An octahedral card is specified only while its on-screen
> height is at least 64 pixels for a bare subject and 96 pixels for a full
> crown. The silhouette figures in this document are measured at or above
> that size and are not claimed below it. Below it the card samples mip 0 at
> roughly one texel per eight pixels, the silhouette it paints is set by which
> texels the taps happen to land on, and its area runs 1.5x to 7x the subject.
> A consumer that would draw a card smaller than its floor must drop it rather
> than shrink it: fade it out over the octave below the floor, or hand the
> subject to the next coarser representation. Raising N does not move this
> floor -- N divides the same sheet into more and therefore smaller frames --
> so a card that is too small is too small at every N.

## 2b. THE RAMP, SIMULATED END TO END -- the defect-2 repair, with its limits

`s5_ramp.py`, `s5_ramp.txt`, `s5.json`. Only the outside-coverage fill
changes; seeding, dilation, the partial band and everything inside coverage
are identical. Each variant goes through the same rebuild, the same model of
`lodgenEncodeBC1Block`, the same decode, the same renderer, the same frozen
registration and the same 24 views. `today` is RE-RUN through that identical
path, so a difference cannot be a difference of route.

    subject   fill        IoU      worst    ink    texels>12lv  blk range
    blast_n4  today     0.5709    0.3825   1.445     0.69%        1.77
    blast_n4  ramp8_24  0.5688    0.4015   1.484     0.05%        1.93
    blast_n4  ramp1_16  0.5706    0.3647   1.421     0.01%        1.55
    maple_n4  today     0.3699    0.2643   1.894     2.26%        3.91
    maple_n4  ramp8_24  0.3690    0.2721   1.915     1.07%        3.06
    maple_n4  ramp1_16  0.3678    0.2759   1.867     1.15%        3.27
    dead_n4   today     0.6139    0.4868   1.170     1.62%        3.36
    dead_n4   ramp8_24  0.6032    0.5258   1.217     0.40%        2.83
    dead_n4   ramp1_16  0.6273    0.5470   1.141     0.38%        2.65
    rock_n4   today     0.8318    0.6763   1.121     1.41%        4.31
    rock_n4   ramp8_24  0.8361    0.6833   1.132     0.81%        4.93
    rock_n4   ramp1_16  0.8255    0.6654   1.091     0.72%        3.98

(`ramp8_24` keeps eight rings then fades over sixteen; `ramp1_16` fades from
the first ring over sixteen. Route check: `today` here reads 0.5709 against
s1's 0.5763 for blast_n4 -- the 0.005 is the 0.1 per cent of BC1 indices my
model resolves differently on float ties, and it is inside the bias.)

**THE HONEST READING, and it is not the flattering one. The ramp FIXES THE
CHIP and DOES NOT MOVE THE SILHOUETTE.** `ramp1_16` takes texels more than
twelve levels wrong from 0.69 per cent to 0.01 (sixty-nine times fewer), 2.26
to 1.15, 1.62 to 0.38, 1.41 to 0.72. The 24-view IoU moves by -0.0003,
-0.0021, **+0.0134**, -0.0063: one subject over this lane's own +0.011 bar,
nothing else proven in either direction, and dead_n4's worst view improves by
a clear 0.060 (0.4868 -> 0.5470). **Anyone applying this applies it for the
visible artefact, not for the metric.** A chip is something a person sees on a
trunk; the silhouette IoU barely knows it is there.

REFUTER: if a real re-bake with the ramp applied does not reduce the 4x4 chips
visible at the trunk in a `ww-texel-picture` panel, the ring-8 cliff was not
the source of the large-range blocks, and the remaining candidate -- genuine
depth discontinuities inside the object -- stands alone.

## 5. THE RANKED LIST

Gain is SIMULATED, 24-view mean, against today, through the frozen
registration. `nm` = not proven: inside this lane's own +0.011 instrument
bias, and therefore not a gain.

**1. RAMP THE OUTSIDE-COVERAGE HEIGHT FILL** (the chips)
  file:line       `src/lodgen.cpp:2698` (`kOutRings = 8`) and
                  `src/lodgen.cpp:2770-2781` (the outside branch)
  script          `scratchpad/impostorfix4_20260919/hookup_ramp.py`
  gain            blast_n4 -0.0003 nm | maple_n4 -0.0021 nm |
                  dead_n4 **+0.0134** (worst view +0.060) | rock_n4 -0.0063 nm
                  texels >12 levels wrong: 0.69->0.01, 2.26->1.15,
                  1.62->0.38, 1.41->0.72 per cent
  risk            low. One branch, one constant. It cannot move a texel INSIDE
                  coverage: the partial and full branches are untouched.
  re-bake         YES -- it is a bake-time fill.
  ruling          NO. The outside fill is already invented by this function
                  and already chosen by measurement; this changes how it
                  fades, not what is claimed.

**2. THE ALPHA THRESHOLD** (the fat ink, the maple, most of everything)
  file:line       the set's `coverage 16 128 160` and the default
                  `alphaThreshold` = covFloor; `docs/LODGEN_IMPOSTOR_SPEC.md`
                  ~348 for the crowns clause
  script          NONE FROM THIS LANE. IMPOSTORFIX3 left two prepared,
                  unapplied ruling scripts; this lane adds evidence, not a
                  third script.
  gain            +0.042 / +0.041 / +0.033 / +0.023 / +0.017 at 0.20, and ink
                  1.441->1.133, 1.237->1.096, 1.894->1.059, 1.168->0.932,
                  1.120->1.053. At the spec's 0.50: -0.038 / -0.030 /
                  **-0.162** / -0.083 / -0.036. The spec's own crown number is
                  the worst option on the crown.
  risk            it is a change of law, not of code.
  re-bake         no.  ruling YES -- and the ruling needs the 0.50 result.

**3. A COVERAGE-PRESERVING MIP CHAIN** (the card below the knee)
  file:line       `res/shaders/impostor_oct.frag:217,267,275,276,295,297`
                  (all `textureLod(...,0.0)`); the dead uniform at
                  `impostor_oct.frag:72` / `src/gl/impostordraw.cpp:466`
  script          NONE. **NOT PROPOSED AS CODE.** The naive version is
                  refuted below; the coverage-preserving version is UNMEASURED
                  and this lane will not rank a number it does not have.
  gain            UNMEASURED. The naive version (box mip + the same floor) is
                  measured and it LOSES: blast_n4 0.3615->0.2124 and blast_n8
                  0.4230->0.2955 at 16 px, ink collapsing to 0.255 and 0.341.
                  It WINS only on the over-covered maple (0.1526->0.2798).
  risk            high until measured; it makes bare trees vanish.
  re-bake         yes (the chain is written at bake time).  ruling no.

**4. THE THREE-FRAME COVERAGE COMBINATION** -- MEASURED AND NOT PROPOSED
  file:line       `res/shaders/impostor_oct.frag`, the `colour.a += cov * w`
                  accumulation and its cut at the floor
  script          NONE, because the measurement says no rule wins.
  gain            `s6_vote.py`, four rules, 24 views, all five subjects:

        rule      blast_n4   blast_n8   maple_n4    dead_n4    rock_n4
        vote       -0.0021     +0.0523    +0.0018    -0.0442    -0.0255
        max        -0.0083     -0.0422    -0.0350    -0.0122    -0.0024
        domsum     +0.0267     +0.0408    -0.0020    -0.0315    -0.0112

                  Every rule that thins the card wins on the two blasted
                  maples -- which are the fat ones, ink 1.44 and 1.24 -- and
                  loses on dead_n4 and rock_n4, which are already near 1.12
                  and cannot afford thinning. **The right combination rule is
                  subject-dependent, which means it is the threshold ruling
                  wearing a different hat.** Not proposed.
  re-bake         no.  ruling YES (and it should not be made separately from 2).

**REJECTED WITH NUMBERS, so nobody spends a lane on them again**
  * BC1 endpoints by PRINCIPAL AXIS instead of by luminance
    (`src/lodgen.cpp:4571`): 0.04..0.12 levels of height, and zero at the 95th
    percentile on four of five subjects. Section 2.
  * Reading `cardMipCap` in the shader as it stands: section 4, it deletes
    bare trees.
  * The colour dilate bleeding coverage: zero, from the source. Section 1.
  * The BC3 alpha ramp: 0.004 ink, 0.0003..0.0023 IoU. Section 1.
  * The parallax step as a cause of fat ink: it is the largest CURE, worth
    0.25..0.34 IoU. Section 1.
  * N=8 as a fix for the small card: N does not move the knee. Section 4.

## 6. GATE ROWS THAT WOULD FAIL ON TODAY'S EXE -- TEXT ONLY, NOT APPLIED

Offered as text for the build lane to splice; this lane applies nothing.

For `tests/spells/lodgen_octahedral.sh` (a BAKE row, fails today because the
fill is a cliff):

    # row 14c -- IMPOSTORFIX4: the outside-coverage height fill must not
    # step. Decode `_n` mip 0, take every 4x4 block, and require that no more
    # than 0.5% of the sheet's texels decode more than 12 levels from the
    # encoder's input. Today: blast_n4 0.69%, maple_n4 2.26%, dead_n4 1.62%,
    # rock_n4 1.41% -- all four FAIL. With hookup_ramp.py applied the
    # simulation says 0.01 / 1.15 / 0.38 / 0.72, so maple_n4 still fails and
    # that is deliberate: the maple's chips are not all from the cliff.
    row 14c "oct height: texels >12 levels of decode error" \
        "$(lodgen_height_decode_error --sheet "$NRM" --ref "$NRMREF" --pct)" \
        --max 0.5

For `tests/spells/impostor_draw.sh` (a DRAW row, fails today because the
shader ignores the uniform it is given):

    # row 21 -- IMPOSTORFIX4: cardMipCap is uploaded at
    # src/gl/impostordraw.cpp:466 and never read; every fetch in
    # impostor_oct.frag is textureLod(...,0.0). A uniform the drawer sets and
    # the shader ignores is a lie in the interface whichever way the mip
    # question is ruled. This row asserts that the shader TEXT either reads
    # cardMipCap or does not declare it. Today it declares it at line 72 and
    # reads it nowhere: FAIL.
    row 21 "impostor_oct.frag: no uniform declared and never read" \
        "$(grep -c 'cardMipCap' res/shaders/impostor_oct.frag)" \
        --not 1

    # row 22 -- IMPOSTORFIX4: the card's apparent-size floor. Render the
    # reference card at 16 px of on-screen height and require its covered
    # area within 1.5x of the mesh's. Today blast_n4 is 2.63x, blast_n8
    # 2.36x, maple_n4 7.57x: FAIL on all three. This row is written to fail
    # until the spec's apparent-size clause (section 4) is ruled; when it is
    # ruled the row becomes "the consumer dropped the card", not "the card
    # got better".
    row 22 "impostor card ink ratio at 16 px on-screen" \
        "$(impostor_dist_ink --tag "$TAG" --px 16)" --max 1.5

## 7. WHAT A LATER LANE MUST SHOOT OR RUN, named and not substituted

1. **maple_n4's own bake-direction control.** mesh AND card at the 16 bake
   directions of N=4, same camera and canvas as
   `scratchpad/impostorfix1_20260919/control/blast_n4_bake/`. Without it
   defect 3's single-frame known-answer control is a proxy (section 3B), and
   a proxy is what it is labelled.
2. **The ring map cross-tabbed against block height range**, on the
   `nrm_true_<tag>.npy` this lane leaves on disk. It separates the two
   surviving candidates for the large-range blocks -- the ring-8 cliff and
   genuine depth discontinuities -- and it is one cheap offline run.
3. **A coverage-preserving mip chain**, built and measured against the s4
   curve. It is the only version of the mip repair that could win on both a
   bare tree and a crown, and nobody has measured it.
4. **A real re-bake with `hookup_ramp.py` applied**, photographed with
   `ww-texel-picture` from the same crops as `pic_chip_*.png`. That is the
   refuter for section 2b.

## 8. THE SCRIPTS, and which numbers each one owns

    s0_control.py    control 1: 0.8785 against IMPOSTORFIX1's 0.8823
    cal4.py          control 2: the ten 24-view numbers; the FROZEN registration
    inst4.py         the instrument (IMPOSTORFIX2's, with ablation switches)
    s1_stages.py     defect 1: the stage table            -> s1_stages.txt/.json
    s2_chips.py      SUPERSEDED -- wrong reference. DO NOT QUOTE.
    s2b_bound.py     height numbers SUPERSEDED. Its DXT5 four-cc check and its
                     stage-C coverage counts stand.
    s2c_encoder.py   the model of lodgenEncodeBC1Block. Its own control FAILED
                     (that is the finding); its functions are reused below.
    s2d_reconstruct.py control 3, and the luminance-vs-PCA table  -> s2d.json
    s2e_localise.py  defect 2: where the surviving error is       -> s2e.json
    s3_maple.py      defect 3: ink inflation + the angle proxy    -> s3.json
    s4_dist.py       defect 4: IoU vs on-screen height            -> s4.json
    s5_ramp.py       defect 2b: the ramp, simulated end to end    -> s5.json
    s6_vote.py       defect 1: the four combination rules         -> s6.json
    pic_chip.py      the texel pages
    hookup_ramp.py   the refusing script
    splice_mistakes.py  the MISTAKES.md byte splice

END OF REPORT. Lane IMPOSTORFIX4, 2026-09-19.
