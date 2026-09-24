# IMPOSTORFIX3 -- apply the PROVEN impostor repairs. Report.

Lane IMPOSTORFIX3, 2026-09-19, started 15:51 CEDT (`date`). Build slot and exe
slot OWNED by this lane. A second code-only lane (HARNESSWIN2) may be adding NEW
files and an unapplied hook-up script at the same time; this lane does not apply
that script and does not touch harnesswindow* files.

## 0. Read, state at launch, and what this lane is for

READ IN ORDER: CONSTITUTION.md; HANDOFF.md top block;
scratchpad/impostorfix2_20260919/report.md (all nine sections incl. the ranked
list in s7) and its PENDING.md; scratchpad/impostorfix1_20260919/PENDING.md
and DELIVER.md; skills ww-reference-card-diagnose, nifskope-ww-build-verify,
ww-spec-gate-audit, ww-anchored-hookup.

THE POINT OF THIS LANE, stated so it cannot drift: IMPOSTORFIX2 produced numbers
from a numpy reproduction of the shader. NOTHING IT MEASURED HAS BEEN RUN BY THE
APPLICATION. This lane's job is to make three of its repairs real and report
whether the real numbers hold, not to re-derive the simulation.

WHAT IS APPLIED HERE (proven, no ruling needed):
1. lodgenRepairOctHeight height fill outside coverage -> dilate 8 texels
   (R2d8), src/lodgen.cpp.
2. the rock's card PLACEMENT, if section 2 names a defect.
3. tests/spells/impostor_bc_decode.py BC3 alpha ramp.

WHAT IS PREPARED BUT NOT APPLIED (bungo's rulings, brief item 4): the alpha
threshold 0.0627 -> 0.20, and the _n height<->sway channel swap. Each as a
refusing anchored --check script + spec wording + a picture pair.

WHAT IS EXPLICITLY NOT DONE: the height-consistency rejection (it costs the rock
0.065, IMPOSTORFIX2 s7 item 4).

### State at launch (measured, not quoted)

(filled in as the commands run)

## 1. REPAIR 1: the height fill outside coverage. SIMULATED GAIN vs REAL GAIN.

`lodgenRepairOctHeight` (src/lodgen.cpp:2622) now runs the same 8-connected
dilation it already ran for the partial band OUT PAST the silhouette, eight
rings, and only then falls to the card plane. `const int kOutRings = 8;`, a new
`outsideNear` counter, and the census line says so.

THE SELF-CHECK SURVIVES AND IS EXACT. For every subject `outsideNear + outside`
equals IMPOSTORFIX1's single `outside` total to the texel -- e.g. the rock,
40542 near + 165521 far = 206063, which is the old number. The repair moves
texels between two buckets and invents none.

CHEBYSHEV, NOT EUCLIDEAN, AND SAID SO IN THE CODE. IMPOSTORFIX2 simulated the
fill with `scipy.distance_transform_edt`, a disc of radius 8. The C++ grows by
8-connected ring passes, a SQUARE of radius 8. The answer does not turn on the
metric -- R2d8 and R2d16 both beat R3 on all five subjects -- but the
divergence is named in the comment rather than hidden.

### The table the brief asked for

Every number in the "real" columns is the in-application harness's own printed
`orbit iou mean`, 24 of 24 views counted, viewport 512x768, blend 1, the SAME
`orbitviews24.txt` IMPOSTORFIX1 used. The two sheet sets live in two separate
fixture roots (`fixture_r3/` and `fixture/`) because `registerLooseSheets`
walks UP to the nearest ancestor holding `textures/`, so arming both from one
tree would have resolved the same sheets twice.

| subject | R3 sim | R2d8 sim | sim gain | R3 REAL | 8-ring REAL | REAL gain |
|---|---|---|---|---|---|---|
| blast_n4 | 0.4978 | 0.5646 | +0.0668 | 0.5038 | **0.5736** | **+0.0698** |
| blast_n8 | 0.6639 | 0.7200 | +0.0561 | 0.6754 | **0.7483** | **+0.0729** |
| maple_n4 | 0.3609 | 0.3685 | +0.0076 | 0.3545 | **0.3674** | **+0.0129** |
| dead_n4  | 0.5826 | 0.6153 | +0.0327 | 0.5721 | **0.6073** | **+0.0352** |
| rock_n4  | 0.7777 | 0.8331 | +0.0554 | 0.7724 | **0.8305** | **+0.0581** |

THE SIMULATION HELD ON ALL FIVE. Real gain is larger than simulated on every
subject, by +0.0030 to +0.0168. The simulation's absolute IoUs also sit within
0.006 to 0.011 of the real ones, which is the check that IMPOSTORFIX2's
instrument was measuring the same thing the application draws.

THE CONTROL THAT SAYS THE FIXTURE IS HONEST: the `fixture_r3` run reproduces
IMPOSTORFIX1's published numbers to four digits (0.5038 / 0.6754 / 0.3545), and
those were measured by a different lane on a different day with a different exe
of the same commit.

### The rock, which the brief made the condition

The rock had to come back over **0.7944**, the number it scored before the
IMPOSTORFIX1 height repair. **It scores 0.8305**, which is +0.0361 above that
line and +0.0581 above today's 0.7724. The regression is not merely undone.

WHAT IS STILL NOT PROVEN: the maple's +0.0129. It is the smallest gain on the
table and the maple is the subject with 297 whole texels in 32,768 -- there is
almost nothing for a dilation to carry outward. It is reported as a gain
because it is measured, not because it is convincing.

## 2. THE ROCK'S ~10 PX. IT IS THE HEIGHT FILL, NOT THE PLACEMENT.

`s2_rockplace.py` re-runs IMPOSTORFIX2's attribution with the registration
FROZEN at the calibrated value and adds this lane's bake as a fifth row, so the
only thing that changes down the table is the sheet's blue channel.

### The placement terms, read out of the rock's own `.lodm`

    oct N            4      frame 128x128      sheet 512x512
    half extents     [1727.89, 1727.89]        (equal, so no axis anisotropy)
    centre           [-89.011, -65.564, -274.135]
    depthSpan        5048.10 world units   ->  1 height level = 19.796 units
    frameOffset      16 pairs, max |dx| 180.77, max |dy| 438.46
    pixel scale      1 px = 6.881 world units at the harness's 512x768 grab
    so 1 height level = 2.877 px along the frame's forward, and at elevation
    15 degrees that is 0.7446 px of VERTICAL silhouette motion per level.

### The rows

| variant | IoU | IoU@best shift | dy el15 | dy el45 |
|---|---|---|---|---|
| shipped heights (flood), ray on | 0.7894 | 0.8134 | 9.92 px | 7.75 px |
| shipped heights, ray OFF | 0.4988 | 0.5122 | -41.42 px | -7.67 px |
| R3 plane-outside, ray on | 0.7767 | 0.8328 | 20.08 px | 9.58 px |
| **8-ring dilation (this bake), ray on** | **0.8325** | **0.8566** | **7.33 px** | **8.50 px** |
| 8-ring dilation, ray OFF | 0.4988 | 0.5122 | -41.42 px | -7.67 px |

THE TWO "ray OFF" ROWS AGREE TO FOUR DIGITS IN EVERY COLUMN. That is the
self-check that these two sheet sets differ in NOTHING except the height
channel: with the parallax step off, the height is never read and the two
render identically.

### Naming the cause

It is NOT extents, centre, frameOffset or pivot. Every row above is placed from
the SAME extents, the SAME centre and the SAME 16 frameOffset pairs -- those
come from the `.lodm` and the `.lodm` did not change. A defect in any of them
would displace all five rows equally. What actually moves is the term that
reads the blue channel: the parallax step carries +48.75 px of vertical
placement on this subject (ray on minus ray off), and the question is only
whether it carries the right amount.

    R3's fill puts every uncovered texel at the card plane, 128.
    The rock's own covered surface sits at h = 92.6 levels.
    Where the ray lands outside the silhouette it therefore reads a surface
    35.4 levels too far back; weighted over the texels the ray actually
    samples, IMPOSTORFIX2 measured the mean sampled height moving
    119.0 -> 131.3 levels, i.e. +12.3 levels = 12.3 x 0.7446 = 9.2 px.

MEASURED HERE: replacing that fill with the 8-ring dilation removes **12.75 px**
of the elevation-15 error (20.08 -> 7.33). Predicted 9.2, measured 12.75, same
sign, same order, and the prediction was made from the mean over ALL outside
texels while the ray samples mostly the ones nearest the silhouette -- which
are exactly the ones furthest from the plane. The ~10 px was the height fill.

NOTHING TO REPAIR IN THE PLACEMENT, therefore, and repair 1 is the repair. The
rock also ends up BETTER placed than it was before any of this: 7.33 px against
the shipped flood's 9.92 px.

### The residual, which is not claimed to be gone

7.33 px at elevation 15 and 8.50 px at elevation 45 remain. A height error
cannot be what they are: a height error's vertical component scales with
sin(elevation), so 7.33 px at el15 would have to be 20 px at el45, and it is
8.5. An elevation-INDEPENDENT residual of 7-8 px is 50-58 world units against
half extents of 1728, 1.4 to 1.7 per cent of the card, and it is the size of
the registration fit's own bias -- that fit was made card-against-card by
IMPOSTORFIX2, not against the mesh. It is a property of this instrument, not a
measured property of the application, and the application's own harness scores
the same sheets 0.8305 over 24 views. It is NOT attributed further here.

## 3. THE BC3 ALPHA DECODER, `tests/spells/impostor_bc_decode.py`

The eight-level ramp was written `((7-k)*a0 + k*a1)/7` with k running 0..5 into
slots 2..7 instead of 1..6 -- one step short, and always LOW. The six-level
ramp had `(4-k)` where D3D says `(5-k)`, and no slot 5 at all.

### The known answer

Hand-built, both modes, every index, against the D3D BC3 rule. Red control is
the file as it stood this morning (`ka_old.py` re-creates it):

    eight-level (a0=255, a1=0)  and  six-level (a0=40, a1=200)
    BEFORE: known-answer: 10 of 16 ramp entries wrong   (worst 36.4 of 255)
    AFTER : known-answer: 0 of 16 ramp entries wrong

The six-level control is a0=40/a1=200 ON PURPOSE. The obvious choice, a0=0,
exposes only ONE of the two defects: the wrong coefficient (4-k) and the right
one (5-k) both multiply zero, so a zero endpoint hides the arithmetic error and
shows only the missing index 5. That first control was written, measured, found
weak, and replaced.

### Gate row 14, before and after the decoder repair

Row 14 is the only thing in the repo that reads this decoder (`grep`: two
scratchpad lanes and `tests/spells/impostor_sheet_check.py`, and IMPOSTORFIX2
deliberately used its own `bcdec2.py` instead). So the re-measurement the brief
asks for is row 14's, and here it is on both sheet sets, with the repaired
decoder and the repaired clause (b):

| subject | af457755's sheets | this lane's sheets |
|---|---|---|
| blast_n4 | 4 of 16 fail, worst 396 units | 0 of 16 |
| blast_n8 | 17 of 64 fail, worst 396 units | 0 of 64 |
| maple_n4 | 3 of 16 fail, worst 793 units | 2 of 16, worst 836 units |
| dead_n4 | 6 of 16 fail, worst 694 units | 0 of 16 |
| rock_n4 | 16 of 16 fail, worst 1791 units | 2 of 16, worst 1557 units |

The maple and the rock still fail frames after the repair and that is reported
rather than tuned away. The maple has 297 whole texels in 32,768: there is
almost nothing for a dilation to carry outward, which is IMPOSTORFIX1's
photography-resolution defect and not this one.

## 4. GATE `tests/spells/impostor_draw.sh` -- floors up, two new rows

`fix05_gate.py`, three anchored-once edits.

**IOU_FLOOR 0.35 -> 0.50, UP ONLY, ON MEASUREMENT.** Four subjects re-measured
by that row's own instrument at 1024x1024 on this exe: blast_n4 0.5610,
blast_n8 0.7280, dead_n4 0.6143, rock_n4 0.8536. The same row on af457755's own
sheets scores 0.4469. 0.50 is a ninth above that and a ninth below the lowest
repaired number. WHAT THE FLOOR DOES NOT ADMIT, written into the gate rather
than hidden: the bare FOREST maple scores 0.3674, was already within 0.017 of
the old 0.35 floor, and is a photography-resolution defect, not a drawing one.

**NEW ROW 14a -- the ruler's own known answer.** `impostor_bc_decode.py` run as
a program. Red control: 10 of 16 ramp entries wrong.

**NEW ROW 14b -- THE ROW THAT FAILS ON exe af457755 FOR REPAIR 1.** Over the
frames of a set, the MEAN fraction of the 8-ring band outside the silhouette
whose height is more than 12 levels off the card plane. Floor 10 per cent:

    subject    af457755's sheets   this bake
    blast_n4         2.0%            19.8%
    blast_n8         2.4%            25.8%
    maple_n4         3.4%            27.7%
    dead_n4          4.3%            28.5%
    rock_n4          5.7%            72.9%

A MEAN and not a per-frame test on purpose: frames at 0.0 per cent exist on
BOTH sheet sets, because a frame whose local surface happens to sit at the card
plane has nothing to carry outward, and a per-frame test would convict the
repair for the subject's geometry.

### Both halves run, and the red one measured

    on THIS lane's sheets    24 steps, 0 failures   (gate_green.txt)
    on af457755's sheets      23 steps, 3 failures   (gate_red_r3.txt)
      FAIL  5 silhouette IoU mean 0.4469 < floor 0.50 (16 views)
      FAIL 14 sheet check: 16 frames measured, 4 fail, worst excursion 396 units
      FAIL 14b outside-band reach: mean 2.0% of the 8-ring band off the card plane

Row 15 (the photograph row) passes on BOTH, 0.8701 vs 0.8700, and that is the
expected result, not a miss: at a bake direction a correct ray makes the
parallax a no-op, so the height sheet cannot change the picture there at all.
Row 14b exists because of exactly that blind spot.

### The harness-size question the brief asked in one line

NO -- the harness window is still floored. `release/ww_harness_window.log`,
written 15:49:42 by the concurrent lane, reads
`harness-window settled asked=1024x1024 window=1822x1024 viewport=1822x989
origin=1960,40 maximised=0 settings=not-restored FLOORED`. The impostor harness
is a separate path and DOES get what it asks for at the sizes this lane used:
its own log prints `viewport 512x768` for a 512x768 request and the gate's row
4b passed `the viewport is the size that was asked for (1024x1024)`.

## 5. THE TWO OWED RULINGS, PREPARED AND NOT APPLIED

Neither is applied. Each is a refusing anchored script that defaults to
`--check` and writes nothing; both check clean against the tree as this lane
leaves it.

| ruling | script | --check | spec wording | pictures |
|---|---|---|---|---|
| A: default alpha cut 0.0627 -> 0.20 | `hookup_ruling_alpha.py` | 2 of 2 anchors match once, CR 0 | new bullet after spec line 350 | `images/ruling_alpha_blast_n4.png`, `images/ruling_alpha_rock_n4.png` |
| B: `_n` height <-> sway | `hookup_ruling_swap.py` | 7 of 7 anchors match once, CR 0 | spec line 45 table row rewritten + a paragraph | `images/ruling_swap_blast_n4.png`, `images/ruling_swap_rock_n4.png` |

RULING B IS NOT ONLY ITS TABLE, and the script says so in its own output: the
`.lodm` version bump is `root.insert( QStringLiteral( "lodm" ), 1 )` at FIVE
sites in `src/lodgen.cpp` and one in `src/lodgenaggregate.cpp`, so it is not an
exact-once anchor and is deliberately left out; `lodgenRepairOctHeight` repairs
the BLUE channel and would have to be rewritten to repair alpha and survive
`lodgenDilateFrames`; and every baked sheet in the tree is invalidated with no
way to tell a v1 sheet from a v2 one by looking at it.

THE HEIGHT-CONSISTENCY REJECTION IS NOT PREPARED AND NOT APPLIED, per the
brief: it costs the rock 0.0652.

## 6. THE PICTURES, LOOKED AT. WHAT IS STILL WRONG.

Everything here is a grab the in-application harness wrote itself, never a
screen capture. The rows are labelled inside each image. `exe 161568a5` is the
`fixture_r3` control -- the card as the previous exe left it -- re-measured on
this lane's exe and reproducing IMPOSTORFIX1's published IoU to four digits,
which is what entitles it to stand in for that exe's picture.

    images/00_before_after_blast_n4_el15.png   bare maple N=4, 12 azimuths, three rows
    images/00_before_after_blast_n4_el45.png
    images/00_before_after_blast_n8_el15.png   bare maple N=8
    images/00_before_after_blast_n8_el45.png
    images/12_orbit_maple_n4_el15.png / _el45.png    forest maple
    images/13_orbit_dead_n4_el15.png  / _el45.png    dead upright
    images/14_orbit_rock_n4_el15.png  / _el45.png    the rock
    images/*_card.gif / *_card_before.gif / *_mesh.gif    the orbits, 12 frames
    images/30_distance_blast_n4.png                  the distance strip
    images/zz_closeup.png, zz_closeup2.png           two views at full size

### The one thing the repair plainly fixes

On exe 161568a5's row the bare maple's card carries big detached slabs of bark
floating in the air beside the trunk -- kite-shaped, a dozen texels across,
clearly not part of the tree. They are the parallax step walking a ray into the
card plane just outside the silhouette and fetching whatever colour sits there.
On the 8-ring row they are gone, on every azimuth, at both elevations. That is
what the +0.07 mean is made of, and it is visible without measuring.

### What is still wrong, in plain words

**The card is fatter than the mesh, and for two subjects the repair made it
fatter.** Card-covered pixels divided by mesh-covered pixels, meaned over the
24 views (1.00 would be the mesh itself):

    subject    exe 161568a5   8-ring
    blast_n4      1.489        1.425
    blast_n8      1.250        1.224
    maple_n4      1.857        1.876      <- fatter
    dead_n4       1.126        1.109
    rock_n4       1.052        1.110      <- fatter

The forest maple's card paints nearly twice the ink the mesh does and this
lane made that slightly worse. The rock went from 5% over to 11% over. Both
subjects' IoU still rose, so the extra ink is mostly landing on the object --
but "the silhouette got better" and "the card stopped over-painting" are two
different claims and only the first one is supported.

**The outline is gnawed.** At full size (`zz_closeup.png`) the card's edge is
not the mesh's edge with a bit of blur; it is a ragged line that eats one to
three pixels in and out along its length. A tree edge is thin twigs, so some of
this is honest, but it is not the mesh's silhouette.

**A rash of small rectangular chips hugs the trunk.** On the 8-ring row, where
the big slabs used to be, what remains are chips that are visibly RECTANGULAR
and all about the same size -- 4x4 texel blocks. That is the shape of a BC3
block, not the shape of anything on a tree. It is the picture's own evidence
for the `_n` sheet's shared palette, which is what owed ruling B is about.

**Three to six views of twenty-four get WORSE, and they are not random.**

    blast_n4   21 of 24 better   worse at az 90/270/300 el 15   worst -0.0675
    blast_n8   24 of 24 better   -                              worst gain +0.1423
    maple_n4   18 of 24 better   worse at az 90/150/240/270 el 15, az 0/90 el 45
    dead_n4    19 of 24 better   worse at az 30/120/150/270/300 el 15
    rock_n4    21 of 24 better   worse at az 210/240/270 el 45   worst -0.0086

Every one of the three trees loses only at the LOW elevation, and mostly on the
half of the orbit away from azimuth 0. Low elevation is where the parallax step
is longest, so carrying the object's height 8 texels out there displaces the
silhouette further than the plane did, and on those azimuths it displaces it
the wrong way. The rock loses only at the HIGH elevation and by under 0.009,
which is inside the harness's own run-to-run wobble. I am not claiming a cause
for the low-elevation losses; I am naming them so the next lane can start at
azimuth 300 elevation 15 on the bare maple, which is the worst one.

**Far away the card is now too THIN.** In `30_distance_blast_n4.png` the near
tiles are the clear win -- exe 161568a5's card at azimuth 45 is a shredded
three-stick bush surrounded by confetti, the 8-ring card is a single trunk. But
at the far end, 16 px tall, the 8-ring card paints 0.0010% of the frame where
the mesh paints 0.0020% and the IoU is 0.450. The card has stopped over-
painting and started under-painting before it reaches the distance it exists
to serve. Nothing in this lane addressed that, and the mean over the orbit at
one distance cannot see it.

**What I will not say.** I am not saying the card follows the shape. At 12
azimuths side by side the two card rows still read as a different plant from
the mesh row -- right height, right lean, right trunk, wrong twigs -- and the
picture does not support any stronger sentence than that.

## 8. TEXT FOR THE DIRECTOR (this lane never edits WW_CHANGES.md or HANDOFF.md)

### For WW_CHANGES.md

```
- **Impostor card height outside the silhouette (lodgen).** The octahedral bake
  used to set every texel outside a frame's silhouette to the card plane, which
  made the drawer's parallax step a no-op exactly where the ray leaves the
  object -- so the ray walked out to the plane and fetched whatever sat there,
  and the card grew detached slabs of bark beside the trunk. The bake now grows
  the object's own height 8 texels OUTWARD from the fully covered texels
  (8-connected rings, ties averaged) before falling back to the plane.
  Measured in the application over 24 orbit views per subject: bare maple N=4
  0.5038 -> 0.5736, N=8 0.6754 -> 0.7483, forest maple 0.3545 -> 0.3674, dead
  upright 0.5721 -> 0.6073, RockCliff02_Alt 0.7724 -> 0.8305. Three to six
  views of twenty-four get worse, all at the low elevation on the trees; worst
  is -0.0675 at azimuth 300 elevation 15 on the bare maple. Sheets must be
  re-baked to benefit; old sheets still draw.
- **`tests/spells/impostor_bc_decode.py`: the BC3 alpha ramp was one step
  short.** The 8-level ramp divided by 7 but stopped at index 6, and the
  6-level ramp used `(4-k)` where the format says `(5-k)` and never wrote index
  5 -- every decoded alpha came out low. Repaired against a known-answer block
  that is now checked in the file and run by the gate. 10 of 16 entries were
  wrong, 0 after.
- **`tests/spells/impostor_draw.sh`: floors raised, two rows added.** The card
  IoU floor goes 0.35 -> 0.50 (measured headroom on four of five subjects; the
  forest maple's 0.3674 is named in the gate as what the floor deliberately
  does not admit). New row 14a runs the decoder's known answer. New row 14b
  measures the 8-ring band outside the silhouette and fails when it is the card
  plane -- it is red on exe af457755's sheets (2.0%) and green on today's
  (19.8%), floor 10%.
```

### For HANDOFF.md

```
IMPOSTORFIX3 (Sat 2026-09-19 15:50..16:4x CEDT) -- landed, NOT committed.
  Built: release/NifSkope.exe 23,504,896 B, 15:57:53,
         sha1 220662f1eb1f344a8f26b0e0470b1b961976263d
  Rung : release/NifSkope.before_impostorfix3.exe 23,504,384 B, 15:52:26,
         sha1 af4577556f2b80ee71a048c637cbe218643ee8d7
  Report: scratchpad/impostorfix3_20260919/report.md (pictures in images/).

  The brief's condition was that the rock come back over 0.7944. It is 0.8305.

  TWO RULINGS ARE OWED AND ARE NOT APPLIED. Each is a prepared anchored script
  that refuses unless every anchor matches exactly once, plus the spec wording
  and one picture pair made with the numpy reference card:
    A  viewer alpha cut 0.0627 -> 0.20.  scratchpad/impostorfix3_20260919/
       hookup_ruling_alpha.py  (--check 2/2)   images/ruling_alpha_blast_n4.png,
       images/ruling_alpha_rock_n4.png.  Over the same 12 azimuths: bare maple
       0.6235 -> 0.7308, rock 0.7752 -> 0.8086.  The cost is visible in the
       picture as red at the twig tips -- the cut throws thin ink away.
    B  `_n` sheet height <-> sway.  hookup_ruling_swap.py (--check 7/7)
       images/ruling_swap_blast_n4.png, images/ruling_swap_rock_n4.png.  Bare
       maple 0.6047 -> 0.6628, rock 0.7762 -> 0.7842.  Both rows are this
       lane's 8-ring fill put through a real BC3 encode, so the only difference
       is which block carries the depth.  On the ROCK the two rows look nearly
       identical and the case for B is the round-trip error, not the
       silhouette; on the BARE MAPLE it is worth +0.058, the same order as this
       lane's own repair.  The script's --check names what is NOT in its table
       (the .lodm version bump at 6 sites, moving lodgenRepairOctHeight to the
       alpha channel past lodgenDilateFrames, and a re-bake of every set).
  The height-consistency rejection was NOT applied and should not be: -0.065 on
  the rock.

  Harness size, asked by the brief: the impostor harness DOES get the size it
  asks for (viewport 512x768; gate row 4b green at 1024x1024). The separate
  harness window does not -- release/ww_harness_window.log 15:49:42 reads
  `asked=1024x1024 window=1822x1024 viewport=1822x989 ... FLOORED`.

  Pre-existing, not this lane: tests/spells/lodgen_octahedral.sh step F1 (the
  cube's silhouette span at the reader threshold, 1.69 texels against a 1-texel
  bar) is red on BOTH exes -- 112 ok / 1 fail, byte-identical measurement.
```

## 7. DELIVERABLES

### The exe

    release/NifSkope.exe
      23,504,896 bytes   2026-09-19 15:57:53   sha1 220662f1eb1f344a8f26b0e0470b1b961976263d
    release/NifSkope.before_impostorfix3.exe        (rung ONCE, at 15:52:26, before the build)
      23,504,384 bytes   2026-09-19 15:52:26   sha1 af4577556f2b80ee71a048c637cbe218643ee8d7

Nothing is committed and nothing was stashed. WW_CHANGES.md and HANDOFF.md were
not touched by this lane; their text is section 8.

### Files this lane changed in the tree

    src/lodgen.cpp                       627,754 -> 631,168 B   CR 0 -> 0
      lodgenRepairOctHeight: kOutRings = 8, the outward ring carry, the census
      line, and the exact self-check (outsideNear + outside == the old outside)
    tests/spells/impostor_bc_decode.py     2,309 ->   5,339 B
      both alpha ramps + known_answer() + known_answer_six() + a __main__
    tests/spells/impostor_sheet_check.py   5,015 ->   8,334 B
      clause (b) continuity across the silhouette, clause (c) the far plane
    tests/spells/impostor_draw.sh         44,833 ->  51,183 B   CR 0 -> 0
      IOU_FLOOR 0.35 -> 0.50 with its measured justification, rows 14a and 14b
      (backup: scratchpad/impostorfix3_20260919/impostor_draw.sh.bak, `sh -n` ok)
    MISTAKES.md                          617,877 -> 622,574 B   CR 9,965 -> 10,043
      four entries, byte-spliced after the single `Newest at the top.` anchor
    .claude/skills/ww-reference-card-diagnose/SKILL.md   13,142 -> 16,455 B
    E:/Projects/Claude/.claude/skills/ww-reference-card-diagnose/SKILL.md
      was 9,163 B and STALE (it never received IMPOSTORFIX2's sections 7..10);
      both trees are now the same 16,455 bytes, section 11 added by this lane

Prepared and NOT applied: `hookup_ruling_alpha.py` (--check 2/2 anchors, once),
`hookup_ruling_swap.py` (--check 7/7). Neither has been run without `--check`.

### Gate counts

    impostor_draw.sh   on this exe's sheets        24 steps,   0 failures
    impostor_draw.sh   on exe af457755's sheets    23 steps,   3 failures
                       (row 5 IoU 0.4469 < 0.50; row 14 decoder 4 of 16;
                        row 14b outside-band reach 2.0% < 10%)
    render_shot.sh                                 82 checks,  0 failures
    lodgen_octahedral.sh   this exe               112 ok,      1 failure (F1)
    lodgen_octahedral.sh   EXE=the rung           112 ok,      1 failure (F1)
    harness_window.sh (RUN_NATIVE_OPEN=0)          13 checks,  1 failure, 1 skip

Both octahedral failures are the SAME step with the SAME number -- F1, the cube's
silhouette span at the reader threshold, 1.69 texels against a 1-texel bar -- on
an exe built before this lane existed and on the one built by it. F1 reads the
BASE sheet's coverage; this repair writes the `_n` sheet's blue channel only.
The gate's own sibling checks agree: the 2-texel form of the same measurement
passes, and F1b, which the gate's comment names as the one to read when F1 goes
red, reports 1.69 vs 1.69 -- the two thresholds see the same silhouette.

The harness_window failure is step (f), "the repaired exe left the recent-file
list exactly as seeded". That is the neighbour lane HARNESSWIN2's gate row,
written ahead of its repair: its own floor row in the same block shows the rung
exe doing the identical thing, so both exes are on the unrepaired side of a
check whose fix is not in the tree. (Note for the director: HARNESSWIN2 did
edit an EXISTING file -- `tests/spells/harness_window.sh`, mtime 16:01 today --
which the brief for this lane said it would not. Nothing of this lane's was
overwritten; the two lanes touch no file in common.)

The skip is (d) native_open.sh, skipped because the brief said RUN_NATIVE_OPEN=0.
A skip is not a pass and is not counted as one.

### Pictures

All under `scratchpad/impostorfix3_20260919/images/`, none in the repo's
`images/`. Listed with what each shows in section 6.
