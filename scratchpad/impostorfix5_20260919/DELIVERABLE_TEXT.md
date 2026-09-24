# IMPOSTORFIX5 -- IMPOSTORFIX4's simulations, made real

Lane IMPOSTORFIX5, 2026-09-19, launched 19:30 CEDT, first `date` read
19:29:29 CEDT. This lane OWNED the build slot and the exe slot.

NOTE ON THE FILENAME: the harness refuses to write `report.md`. The brief's
own fallback is taken; this file IS the report.

## 0. State at launch, and the exe this lane leaves

Process check, its own command, before every build and every exe run:
`tasklist | grep -i -E "Fallout4|NifSkope"` -> `rc=1`, no match. Never once up.

    at launch   release/NifSkope.exe  23,625,216 B  2026-09-19 18:58:55
                sha1 ee87eb9ee6bfe45a199cd5c85820c333c6572256   (== the brief)
    rung ONCE   release/NifSkope.before_impostorfix5.exe, that exact file
    build 1     release/NifSkope.exe  23,625,216 B  2026-09-19 19:32:42
                sha1 38a1745d0a70aa3c2a6bc4a06cc09543201ec979
                the ramp. Every sheet in this report was baked by this exe.
    SHIPPED     release/NifSkope.exe  23,625,728 B  2026-09-19 19:49:03
                sha1 c529e3c12fe4216a3c33cc14631e3c811169fbb0
                build 1 plus the corrected census wording

Two builds, because the ramp hook-up left the census line saying "within 8
rings" on an exe that ramps over sixteen (section 1, and `MISTAKES.md`). Both
recompiled ONE translation unit -- `GeneratedFiles/.obj/lodgen.o` -- so the
shipped exe is the launch exe plus this lane's diff and nothing else.
`BUILD-RC=0` both times, `release/NifSkope.exe -nt src/lodgen.cpp`,
`res/style.qss` byte-equal to `release/style.qss`. Every `src/cell*.cpp` and
`src/cell*.h` was md5-identical across the second build, so nothing of lane
CELLVIEW4's live work was compiled into it.

**Does the second build invalidate the tables?** No, and it was checked rather
than argued: blast_n4 was re-baked from the same PNGs on the SHIPPED exe and
all five of its DDS came back BYTE-IDENTICAL to the ones every number here was
measured on, with the corrected census line printed. A string is a string.

**bungo's open NifSkope window needs a restart** -- his title bar shows the last
COMMIT, so the exe timestamp is the tell.

## 1. THE RAMP IS APPLIED, AND THE SIMULATION HELD

`hookup_ramp.py --check` -> 2 anchors, 1 match each, OK. `--apply` -> CR 0
before and after, LF 14438 -> 14450. `kOutRings` has no remaining reader.

One thing the hook-up script did not carry, and this lane fixed: the census
line still said *"outside within 8 rings"* on an exe that ramps over sixteen.
A census that describes code that no longer exists is the kind of instrument
this project has been burned by, so the line now names the ramp and its
length. The four counts are unchanged in meaning and still add up exactly:
blast_n8 249,445 near + 102,858 far = 352,303, which is IMPOSTORFIX1's
single `outside` total to the texel.

### All five re-baked into this lane's folder, and only the height sheet moved

`rebake5.sh`. ONE DEFECT FOUND AND FIXED ON THE WAY: IMPOSTORFIX3's
`rebake_all.sh` never passed `--no-trees-only`, so the rock's bake failed with
*"no LOD-bearing refs in chunk"* and its log is that one line. This lane's
script passes it and CHECKS every subject's log for the height-repair census
line, so a silent no-op cannot pass as a bake. All five printed it.

Of the five sheets each subject writes, `_oct_n.DDS` differs and
`fs / _oct_d / _oct_g / _oct_gsaos` are byte-identical to IMPOSTORFIX3's,
on all five subjects. That is the control that the change reaches the height
sheet and nothing else -- and, incidentally, the proof that the rock's
"before" sheets really were the 8-ring ones.

### Texels more than 12 levels from the encoder's input (`decode_err.py`)

The reference is reconstructed by porting `lodgenRepairOctHeight` to Python
and is NOT trusted on its word: the port recomputes the four integers the exe
itself printed (whole / partial / outsideNear / outside) and is refused unless
all four match. They matched on all five subjects on both sheet sets. The
rock's old log holds no census line (see above), and its port counts
(51,389 / 4,692 / 40,542 / 165,521) are the ones IMPOSTORFIX3's own report
quotes for it, which is the independent citation that stands in for it.

    subject   IF3 REAL   IF4 SIMULATED     IF5 REAL   mean 4x4 block range
              (8-ring)   today -> ramp     (ramp)     before -> after
    blast_n4    0.663%   0.69 -> 0.01      0.011%     1.77 -> 1.55
    blast_n8    0.886%   not simulated     0.035%     2.03 -> 1.70
    maple_n4    2.252%   2.26 -> 1.15      1.089%     3.91 -> 3.26
    dead_n4     1.581%   1.62 -> 0.38      0.362%     3.36 -> 2.65
    rock_n4     1.382%   1.41 -> 0.72      0.699%     4.31 -> 3.98

**The simulation held on every subject it made, to 0.06 of a percentage point
or better, and the block ranges match to two decimals.** Between four and
sixty times fewer texels are more than twelve levels wrong.

### The 24 views, on the real exe, both sheet sets, one renderer

`measure5.sh`. Both sides measured on the NEW exe with the SAME 24 views
IMPOSTORFIX1 froze: the only thing varied is the bytes of `_oct_n.DDS`. The
rung exe was never run with a GUI and did not need to be. The "before"
column reproduces IMPOSTORFIX3's published numbers to four digits
(0.5736 / 0.7483 / 0.3674 / 0.6073 / 0.8305), which is the route control.

    subject   IoU before  IoU after   delta     IF4 simulated   worst view
    blast_n4    0.5736     0.5733    -0.0002      -0.0003       0.3895 -> 0.3705
    blast_n8    0.7483     0.7466    -0.0017    not simulated   0.6554 -> 0.6343
    maple_n4    0.3674     0.3656    -0.0018      -0.0021       0.2621 -> 0.2751
    dead_n4     0.6073     0.6182    **+0.0109**  +0.0134       0.4770 -> 0.5424
    rock_n4     0.8305     0.8230    -0.0075      -0.0063       0.6774 -> 0.6642

**THE HONEST READING, AND IT IS NOT THE FLATTERING ONE. The ramp fixes the
chip and does not move the silhouette.** Every delta agrees with
IMPOSTORFIX4's simulation in sign and to about 0.002 in size. dead_n4's gain
is the only one anybody could call a gain and it lands at +0.0109 -- just
UNDER IMPOSTORFIX4's own +0.011 bias bar, so on this lane's instrument
**not one subject clears it**. Its worst view does improve by a clear +0.065
(0.4770 -> 0.5424), and the rock pays 0.0075.

So: this is applied for the visible artefact, on IMPOSTORFIX4's own reasoning,
and the metric does not endorse it. Anyone who wants the metric to endorse it
should read the picture instead.

REFUTER, and it was run: `images/01_trunk_crop_blast_n4.png` is the SAME
48x48-texel crop of the decoded height before and after. 134 texels in it are
more than twelve levels wrong before; 0 after. The left panel's hard grey step
IS the cliff. If that step had survived the ramp, the cliff was not the source
of the large-range blocks -- it did not.

## 2. THE SHOT IMPOSTORFIX4 COULD NOT TAKE, AND IT OVERTURNS THE QUESTION

`run5b.sh`, part A/B. The maple's card AND mesh drawn at the SIXTEEN DIRECTIONS
ITS OWN BAKE USED (`impostor_bake_views.py` reads them out of the .lodm, they
are not retyped). At a bake direction a correct parallax ray is an exact no-op,
so the card is the bake's own photograph of the mesh from that very camera, and
whatever disagreement survives is upstream of every blend, threshold and
parallax question in the spec.

    maple_n4, blend 0   IoU 0.4673   16 of 16 views
    maple_n4, blend 1   IoU 0.4673   16 of 16 views   <- identical: parallax IS a
                                                         no-op, so the view ray
                                                         is right and the
                                                         disagreement is the
                                                         SHEET's
    blast_n4, blend 0   IoU 0.8701   16 of 16 views   <- the control on the
                                                         control; IMPOSTORFIX1
                                                         published 0.8823 for
                                                         this and this run
                                                         lands there

Card ink / mesh ink at those same directions: maple **1.472**, blast 1.038.
**At the camera the frame was photographed from, the maple's card is half again
FATTER than the mesh.**

### The verdict IMPOSTORFIX4 s3 left open: all three of its candidates are wrong

The brief names three suspects for the alpha-tested leaves -- two-sidedness, a
bake-time alpha test, depth written by discarded texels. I went and LOOKED at
the bake's own frames, which is what settles it:

**THE MAPLE HAS NO LEAVES.** `0004a074_front.png` out of its own bake is a bare
twiggy tree, and so is the mesh row of `00_before_after_maple_n4.png`. The leafy
crown in the CARD row is not leaves -- it is that fine twig mass rasterised onto
a frame 32x64 texels across. So:

* two-sided leaves: there is no leaf geometry to be one- or two-sided.
* a bake-time alpha test discarding texels: nothing is discarded. Coverage is
  STORED as a value in the albedo sheet's alpha and cut at draw time.
* depth written by discarded texels: parallax is a measured no-op at a bake
  direction, so depth cannot move the silhouette there at all.

### What is actually happening, measured: sub-texel geometry

Whole-texel share of each subject's covered silhouette -- the fraction of the
bake's own coverage that is a FULL texel (alpha >= 250) rather than a partial:

    rock_n4    91.6%      dead_n4   55.8%
    blast_n4   61.7%      blast_n8  60.6%
    maple_n4    5.9%   (297 whole against 4762 partial)

**94 per cent of the maple's silhouette is partial coverage**, and the drawer's
cut is the sheet's floor, 16/255 = 0.0627, which admits essentially all of it --
so the crown draws as its solid hull. That is the 1.472 ink ratio, and it is
also why the maple is the one subject where raising the cut to 0.50 DELETES the
crown in two views (section 3): when every value is small, one threshold is
either "all of it" or "none of it".

Sub-texel geometry at the frame's resolution is not a bug in a shader. It is the
card being too small for this model.

### The lever, and why this lane PREPARED rather than applied

The frame resolution comes from `LodGeneration/cardRes` (QSettings, default 128)
read in `src/lodgenmanager.cpp:1085`. It is the long side for the LARGEST base
in the run; smaller bases halve by WORLD SIZE, three rungs at most, and a thin
tree narrows again from its own silhouette. The maple is small in world size and
fine in geometry, so the ladder serves it worst of the five.

**A CORRECTION TO THIS LANE'S OWN FIRST READING, made before the report was
finished and written here rather than quietly dropped.** I looked for a
`--card-res` switch on `lodgen`, found only `--card-half-aux`, and concluded
that the decisive experiment could not be run from a script. **That was wrong.**
The resolution does not live in `lodgen` at all: `lodgen --impostors` is the
COMPRESSION half and reads the frame size out of the bake's `.txt` sidecar
(`oct N tileW tileH ... base`). The PHOTOGRAPHY half is the GUI hook, and it
takes `WW_IMPOSTOR_TILE` (the long side, 64/128/256/512) and `WW_IMPOSTOR_REF`
(the run's largest base in world units; **UNSET means no size ladder, every base
at full size**) -- `src/nifskope_ui.cpp:22766`, driven by
`tools/bake_impostor_cards.sh`. So the experiment IS runnable, and this lane ran
it; the result is below. The mistake is in `MISTAKES.md`.

The numbers that set it up: the maple's sidecar says `oct 4 32 64 ... 64`, so
its frames are 32x64 texels on a run whose largest base was 128 -- it is already
one rung DOWN the ladder. blast_n4's says `oct 4 48 128 ... 128`: full size.
That is the ladder doing exactly what it is documented to do, and it is why the
maple is the worst-served subject of the five.

### THE EXPERIMENT, RUN (`cardres_test.sh`)

The maple photographed again with `WW_IMPOSTOR_TILE=128` and **`WW_IMPOSTOR_REF`
UNSET** -- the size ladder switched off, so this base gets the full long side.
Its sidecar goes `oct 4 32 64 ... 64` -> `oct 4 64 128 ... 128`: frames of
64x128 instead of 32x64, **four times the texels, nothing else changed**. Same
exe, same compression, same 16 bake directions, same known-answer control.

THE ROUTE CONTROL FIRST, because without it the rest is worthless. blast_n4 was
pushed through the identical new route; it is already at full size, so nothing
about it should move:

    blast_n4   sidecar    oct 4 48 128 ... 128   -> IDENTICAL
               census     6879 / 4271 / 62169 / 24985 -> IDENTICAL to its own bake
               IoU        0.8701 -> 0.8701
               ink ratio  1.038 -> 1.038

Four ways of saying the re-bake route changes nothing. Now the maple:

    quantity (16 bake directions)      32x64 frames    64x128 frames
    IoU, card against its own mesh        0.4673          0.5022
    card ink / mesh ink                   1.472           1.250
    mesh ink (the control: unmoved)       0.0365          0.0365
    whole-texel share of the silhouette    5.9%           14.3%
                                      (297 / 4762)    (2417 / 14508)

**The refuter did not fire: frame resolution IS a cause.** +0.0349 of IoU is
thirty times the bias bar this corpus has been using, and the card lost nearly
half of its excess ink (1.472 -> 1.250, where 1.000 is the mesh). The mesh side
is identical to four decimals, so the instrument did not move.

**And it does not close the gap.** At four times the texels the maple's card is
still a quarter fatter than its mesh and still sits at 0.50 where blast_n4 sits
at 0.87, and 86 per cent of its silhouette is STILL partial coverage. So the
ladder rung is a real part of this subject's problem and not the whole of it.
The next step for whoever takes it up is the obvious one -- `TILE=256`, which
this lane did not run -- and the question it answers is whether the curve is
still climbing or has flattened.

### What is NOT this lane's to decide

The shape of the ruling:

* This is NOT a plain defect, so the brief's "repair and re-measure" branch does
  not apply. There is no broken line of code here: the size ladder halves a
  base by WORLD SIZE and the maple is genuinely small in world size. It is
  doing what it was written to do.
* It is the brief's other branch -- a change of a number -- and a number that
  costs. The bake's cost goes as `OCT^2 * TILE^2`, so the maple's card cost
  four times more to bake and its sheets are four times the bytes. Whether a
  forest of these is worth it is a shipping decision and bungo's, not a
  measurement.
* **NOTHING WAS APPLIED.** `cardres_test.sh` writes only into this lane's
  folder. No default moved, no QSettings key was touched, no switch was added.
  The ladder still ships exactly as it was.
* What the tree could use, and what this lane did NOT add because the brief
  does not authorise it: the bake's `TILE`/`REF` knobs are reachable only
  through `tools/bake_impostor_cards.sh`'s environment. A base whose geometry
  is fine relative to its world size has no way to say so. That is the shape
  of a future lane, not of a patch today.
* The refuter, and it was run rather than left as a sentence: see above. It did
  not fire.

### Grab size: the harness log REFUSES and the pixels do not

The brief asks the mesh grabs' size to be proved from
`release/ww_harness_window.log`. That log will not prove it, and this is worth
the director's attention:

    harness-window settled asked=1024x1024 window=569x216 viewport=1024x1024 ... FLOORED
    REFUSED: the window size asked for was 1024x1024 and the window came out
             569x216 -- the request was floored, so this run measures the
             machine and not the code

All four entries REFUSE. The orbit harness in the same run prints
`viewport 1024x1024`. Both numbers are honest and they measure different things:
the window guard measures the TOP-LEVEL WINDOW, and the orbit grab is an
offscreen buffer whose size is read back off the image. The decisive evidence is
the bytes on disk: all 16 maple mesh grabs and all 16 card grabs are
**1024x1024**, and the 24-view runs are **512x768**, read out of the PNG headers.
So the measurements stand -- but a guard that refuses every run it sees is a
guard nobody will read. It needs to either check the buffer or stop reporting on
runs that do not use the window.

## 3. THE ALPHA ROW, MEASURED ON THE REAL EXE, NOTHING APPLIED

The brief forbids applying the cut-off change, the `_n` channel swap, any
combine-rule change and any mip cap, and asks for the 0.20 row measured for
real. Nothing on that list was applied. The tree is shared and carries other lanes'
uncommitted work, so the honest proof is mtime, not `git diff`: everything under
`src/ res/ tests/ tools/` touched since this lane opened at 19:29:29 is

    src/lodgen.cpp            the ramp + the census wording
    tests/spells/impostor_draw.sh          row 14c
    tests/spells/impostor_height_ref.py    NEW, row 14c's reference

and nothing else.

**Yes, the cut can be forced from the harness.** `WW_IMPOSTOR_ALPHA` sets it in
the DECODED fraction domain and the log prints the cut it used --
`coverage cut: 0.2000 (forced by WW_IMPOSTOR_ALPHA)` against the default
`coverage cut: from the set -- floor 16/255 = 0.0627` -- so these are
measurements and not simulations. 24 views, the ramped sheets:

    subject    set's 0.0627    0.20      0.50
    blast_n4      0.5733      0.6087    0.5406
    blast_n8      0.7466      0.7827    0.7651
    maple_n4      0.3656      0.3907    0.2138  <- only 22 of 24 views counted
    dead_n4       0.6182      0.6347    0.5361
    rock_n4       0.8230      0.8307    0.7706

and on the 8-ring sheets at 0.20: 0.6090 / 0.7844 / 0.4004 / 0.6251 / 0.8457
(so the cut and the height fill are close to independent, as they should be --
the cut reads the albedo sheet's alpha and the fill writes the normal sheet's
blue).

**0.20 is better than the set's own floor on all five subjects. 0.50 is worse
than both on all five**, and on the maple it DELETES THE CROWN OUTRIGHT in two
of the 24 views -- the renderer found nothing to measure and the harness
counted 22. 0.50 is the spec's crown number, so the spec's number is the worst
of the three on this corpus.

That is a result, not a recommendation. What it does NOT tell anybody is which
value to ship: 24 orbit views of five static models at one distance is not the
same instrument as a person walking past a forest, IoU does not see popping
between the card and the mesh at the swap distance, and the maple's collapse at
0.50 is a symptom of the resolution problem in section 2 rather than an
argument about thresholds. **The number is for the director; the change is
not this lane's to make.**

## 4. THE GATES

All run on the new exe (19:49:03, sha1 c529e3c1), each after its own
`tasklist | grep -i -E "Fallout4|NifSkope"` -- `rc=1` every time, Fallout 4
never up, this lane's rung `NifSkope.before_impostorfix5.exe` never executed.

The BEFORE column is not a run of the rung exe: all five of these gates open a
window, and the brief forbids running a `before_*` rung with a GUI. It is the
count each gate last published, from lane CELLVIEW2B's control sweep on exe
`ef4dab1f` (2026-09-19 18:14:53) -- five hours old, same tree, same day.

    gate                      before (CELLVIEW2B, ef4dab1f)   after (c529e3c1)
    impostor_draw.sh          24 checks, 0 failures           24 steps, 0 failures
    lodgen_octahedral.sh      116 ok, PASS                    116 ok, RESULT PASS
    render_shot.sh            82 checks, 0 failures           82 checks, 0 failures
    harness_window.sh         15 checks, 0 failures, 0 skips  15 checks, 0 failures, 0 skips
    cell_open.sh              8 rows, PASS                    13 rows, 0 failures, PASS

Two rows need a word rather than a tick:

**`impostor_draw.sh` reports 24 either side, and the numbers being equal is a
coincidence worth naming rather than a tick.** The row SET is not the same: the
run on this exe prints `0 1 2 3 3 3 3 3 3 3 4 4b 5 6 7 8 10 13 13b 14 14a 14b
14c 15`, which contains the new row and, on this fixture, omits the per-N row 9
the baseline included. So read it as "no row regressed and 14c is present", not
as "the count is unchanged".

The floor for 14c is IMPOSTORFIX4's 0.5 per cent and was NOT lowered to fit.
Proof that it can fail, run on this same exe with only the sheet bytes changed:

    ramped sheets (this lane's bake)   done 24 steps, 0 failures
      PASS 14c ... 0.011% of 98304 texels ... (max 0.5%, mean err 2.74,
           worst 22, mean 4x4 block range 1.55, fill = 16-ring ramp)
    8-ring sheets (exe ee87eb9e's bake)   done 24 steps, 1 failures
      FAIL 14c ... 0.663% of 98304 texels ... (mean err 3.16, worst 33,
           mean 4x4 block range 1.77, fill = 8-ring cliff)
           -- the height fill ends on a cliff and BC1 cannot carry it

The comment block above the row carries the five-subject table and says in
words that the maple still fails at 1.089 deliberately, and that a lane wanting
it green has to find the other source rather than move the bar.

**`cell_open.sh` reports 13 rows where CELLVIEW2B saw 8.** That gate is CELLVIEW
lane territory and CELLVIEW4 is live in this tree while I write; the gate has
grown rows since the baseline was taken. 0 failures either way. I did not touch
`src/cell*` and the objects prove it: every `src/cell*.cpp` and `.h` was
md5-identical before and after my build, and the build recompiled exactly ONE
translation unit, `lodgen.o`.

### A mistake in this section's own first run, reported rather than re-run quietly

The first pass of the five-gate batch called `impostor_draw.sh` without the
mesh argument. It refused six steps BY NAME ("no mesh: ... REFUSED rather than
scoring a card against an empty scene") and reported `25 steps, 6 failures`.
That is the gate behaving correctly and me driving it wrong. Notably row 14c
PASSED in that run, because it reads the sheets off disk and needs no scene --
which is a property worth knowing about it. The re-run with the mesh is the
number in the table.

## 5. THE PICTURES, AND WHAT IS STILL WRONG IN THEM

All under `scratchpad/impostorfix5_20260919/images/`. Nothing was written to the
repo-root `images/`.

    00_before_after_blast_n8.png    card before / card after / mesh, 12 azimuths at el 15
    00_before_after_maple_n4.png    the same three rows for the leafy maple
    01_trunk_crop_blast_n4.png      the SAME 48x48-texel crop of the decoded height
    01_trunk_crop_maple_n4.png      ... and for the other four subjects
    01_trunk_crop_dead_n4.png
    01_trunk_crop_rock_n4.png
    01_trunk_crop_blast_n8.png

Ringed texels in the crop (more than twelve levels from the encoder's input),
before -> after, counted off the drawn crop and not off the sheet:

    blast_n4  134 -> 0      maple_n4  272 -> 117    dead_n4  192 -> 72
    rock_n4   173 -> 101    blast_n8  180 -> 46

I LOOKED at all seven. In plain words:

**The chip is gone and you can see it go.** In `01_trunk_crop_blast_n4.png` the
left panel has a hard grey step running through it -- one row of texels at the
object's height, the next at 128 -- and blocks along it come back flat. The
right panel is a smooth gradient over the same texels and nothing is ringed.
That is the whole of what this lane's code change does.

**The silhouettes are not visibly different, and the strips say so honestly.**
Put rows 1 and 2 of `00_before_after_blast_n8.png` side by side and I cannot
tell them apart at these sizes, which is what a mean IoU delta of -0.0017
should look like. Anyone hoping the ramp would make the cards look better is
going to be disappointed by this picture, and that is the correct impression.

**The maple's strip is the one worth the director's minute.** Row 3 is a BARE
TWIGGY TREE. Rows 1 and 2 are a solid dark blob with a vaguely tree-shaped
outline. They are not the same object seen twice; the card has filled in every
gap the mesh has. That is the 1.472 ink ratio in section 2 made visible, and it
is why the ramp cannot help it -- there is nothing wrong with the height sheet
in the crown, there is nothing IN the crown but partial coverage.

**What is still wrong, listed plainly:**

1. The maple's card is a filled hull where the mesh is open twigs. Section 2.
2. Three subjects' crops still ring texels after the ramp (maple 117, rock 101,
   dead 72). The ramp removed the cliff; these are the OTHER source, and this
   lane did not identify it. Row 14c fails the maple at 1.089 per cent on
   purpose so that stays visible.
3. `blast_n4`'s worst view got slightly worse (0.3895 -> 0.3705) while its chip
   count went to zero. A silhouette metric and a height metric are not the same
   metric, and on this subject they point opposite ways.

## 6. FOR THE DIRECTOR

### WW_CHANGES.md text (this lane does not edit that file)

    ### Octahedral impostor height sheet: the fill ends on a ramp, not a cliff

    `lodgenRepairOctHeight` filled the `_n` sheet's height channel outside the
    silhouette by dilating the object's height eight rings out and then stepping
    straight to the card plane (128). A step that size inside one 4x4 BC1 block
    gives the block a height range its single colour line cannot carry, and the
    block decodes as a flat chip -- visible on tree trunks.

    The fill now fades linearly to the card plane over sixteen rings. The census
    line names the ramp and its length, and reports the two buckets separately
    (inside the ramp, and set to the card plane) so a later change of the number
    is visible in every bake log.

    Measured on five fixture subjects, real bakes, real draws: texels more than
    twelve levels from what the encoder was handed drop from 0.663 / 0.886 /
    2.252 / 1.581 / 1.382 per cent to 0.011 / 0.035 / 1.089 / 0.362 / 0.699
    (blasted maple N=4 and N=8, leafy maple, dead tree, rock cliff). The mean
    4x4 block range falls with them.

    The 24-view silhouette IoU does NOT improve: the deltas are -0.0002,
    -0.0017, -0.0018, +0.0109 and -0.0075, and the only positive one is under
    the +0.011 bias bar its own simulation set. The change is for the chip a
    person can see on a trunk; the silhouette is unmoved and the maple's is
    still wrong for a different reason (see HANDOFF).

    `tests/spells/impostor_draw.sh` gains row 14c, which decodes the shipped
    sheet and scores it against a controlled reconstruction of the encoder's
    input (`tests/spells/impostor_height_ref.py`, which refuses unless it
    reproduces the four-integer census the bake itself printed). 24 steps 0
    failures on the new sheets; 1 failure on the sheets the previous exe baked.

### HANDOFF text

    IMPOSTORFIX5 (2026-09-19 19:30-20:0x) applied IMPOSTORFIX4's simulated ramp
    for real and re-measured everything on the exe. The simulation held on every
    subject it had made, to 0.06 of a percentage point. The chip is gone and
    the silhouettes did not move -- see WW_CHANGES.

    THE OPEN QUESTION IS NOT THE ONE WE HAD. The lane took the shot
    IMPOSTORFIX4 could not take offline: the leafy maple's card AND mesh at the
    SIXTEEN DIRECTIONS ITS OWN BAKE USED, where parallax is provably a no-op.
    IoU 0.4673 at blend 0 and 0.4673 at blend 1 (identical -- so the view ray
    is right); card ink / mesh ink 1.472. blast_n4 on the same run: 0.8701,
    reproducing IMPOSTORFIX1's published number, so the instrument is sound.

    All three candidates the brief named for "alpha-tested leaves" are refuted:
    THE MAPLE HAS NO LEAVES. Its own bake frame is a bare twiggy tree. What
    reads as a crown on the card is fine twig geometry rasterised onto a
    32x64-texel frame. 94 per cent of its silhouette is PARTIAL coverage
    (297 whole texels against 4762 partial; the rock is 91.6 per cent whole,
    blast_n4 61.7), and the drawer's floor cut admits all of it, so the crown
    fills in solid.

    The maple's sidecar says `oct 4 32 64 ... 64`: it is already one rung down
    the card size ladder, on a run whose largest base was 128. The lane re-baked
    it with the ladder OFF (`WW_IMPOSTOR_TILE` / `WW_IMPOSTOR_REF` on the GUI
    bake hook) rather than filing a ruling request for an experiment it could
    run. At 64x128 frames -- four times the texels, nothing else changed --
    IoU goes 0.4673 -> 0.5022 and card-ink/mesh-ink 1.472 -> 1.250, while
    blast_n4 pushed through the identical route reproduces its sidecar, its
    census, its 0.8701 and its 1.038 exactly. So the ladder rung is a real part
    of this subject's problem AND NOT ALL OF IT: 86 per cent of the crown is
    still partial coverage at four times the texels. TILE=256 is the obvious
    next rung and was not run.

    NOTHING WAS APPLIED for this. No default moved, no switch was added, and
    the bake cost goes as OCT^2 * TILE^2 -- a forest of quadrupled cards is a
    shipping decision, not a measurement.

    ALSO MEASURED, NOTHING APPLIED: the coverage cut. 24 views, five subjects,
    real. The set's own floor (0.0627) is beaten by 0.20 on ALL FIVE subjects,
    and the spec's 0.50 is worse than both on all five -- and on the maple it
    deletes the crown outright in two of the 24 views. That is a number for a
    ruling, not a change to make.

    The `release/ww_harness_window.log` guard REFUSES every run it saw
    (`asked=1024x1024 ... window=569x216 ... FLOORED`) while the same run's
    orbit log reads `viewport 1024x1024` and the grabs on disk really are
    1024x1024. The guard measures the top-level window; the grab is an
    offscreen buffer. A guard that refuses everything will stop being read.

## 7. MISTAKES AND THE SKILL

Three entries spliced to the top of root `MISTAKES.md` by byte splice
(`splice_mistakes.py`, one anchor, exact-once, the file is CRLF throughout and
the CR count is asserted to move by exactly the lines added).
**APPLIED: 646,222 -> 650,178 bytes, CR 10,452 -> 10,517, LF 10,452 -> 10,517,
+65 lines, no bare LF.** The entries:

1. **"there is no CLI switch for it" was a search, not a fact.** Mine, this
   lane, corrected in place twenty minutes later. Grepping `lodgen`'s argument
   parser answered a question about the COMPRESSION half of a two-process
   pipeline; the resolution belongs to the PHOTOGRAPHY half and has been
   settable by environment variable all along.
2. **A census line that describes code that was deleted.** The ramp hook-up
   left `lodgenRepairOctHeight` saying "within 8 rings" on an exe that ramps
   over sixteen. Nothing failed -- every check reads the numbers, and the
   numbers were right -- which is exactly why it is worth an entry.
3. **A re-bake that did nothing, and a log that said so in one line.**
   IMPOSTORFIX3's rock never re-baked (`rebake_all.sh` did not pass
   `--no-trees-only`) and the lane published its row anyway. The bytes were
   fine by luck. `rebake5.sh` now greps every subject's log for the census line
   the repair prints, because a step that can succeed while producing nothing
   needs a positive artefact and not a return code.

**Skill, written to BOTH trees with equal sha1
`5c0be41bfdab55c55478b5dcc6f0bb1ebed97bdd`:**

    .claude/skills/ww-knob-owning-stage/SKILL.md          (NifSkope tree)
    E:/Projects/Claude/.claude/skills/ww-knob-owning-stage/SKILL.md

"Which stage owns the knob?" -- the table of how each half of a WW split
pipeline is configured (GUI hook = `WW_*` environment, `-no-gui` verb =
arguments, panel = QSettings, driver = `UPPERCASE=` in `tools/*.sh`), the
sidecar that carries a decided value forward so the later stage cannot choose
it, the four-place grep procedure, the control the experiment still owes (re-bake
a subject already AT the new setting and check it reproduces its own number),
and the rule that running the experiment beats filing a ruling request for it.
It does not exist to justify flipping a default: an owed ruling still never
ships as one.

## 8. WHAT THIS LANE WROTE, AND WHERE

In the repo, outside the scratchpad -- three files, and nothing else under
`src/ res/ tests/ tools/` was touched since 19:29:29:

    src/lodgen.cpp                        the 16-ring ramp + the census wording
    tests/spells/impostor_draw.sh         row 14c (51,183 -> 53,938 bytes, CR 0 -> 0,
                                          `sh -n` clean, backup in the lane folder)
    tests/spells/impostor_height_ref.py   NEW: row 14c's controlled reference
    MISTAKES.md                           three entries at the top, byte splice
    .claude/skills/ww-knob-owning-stage/SKILL.md      NEW

and the same skill file, byte-for-byte, at
`E:/Projects/Claude/.claude/skills/ww-knob-owning-stage/SKILL.md`.

`WW_CHANGES.md` and `HANDOFF.md` were NOT edited; their text is section 6, for
the director to splice.

In `scratchpad/impostorfix5_20260919/`:

    DELIVERABLE_TEXT.md   this report (the harness refuses `report.md`)
    PENDING.md            the resume state, written past half context
    BUILDING / DONE       the lane marker
    rebake5.sh            the five re-bakes, each log checked for its census line
    measure5.sh           24 views, both sheet sets, both on the new exe
    run5b.sh              the 16-direction control + the alpha 0.20 row
    run5c.sh              the alpha 0.50 row
    cardres_test.sh       the section 2 discriminator
    decode_err.py         the standalone decode-error driver for the tables
    pics5.py              the two pictures
    fix01_row14c.py       the refusing splice for row 14c
    splice_mistakes.py    the refusing splice for MISTAKES.md
    mistakes_entries.txt  their text, authored LF
    sheet_hashes.txt      sha1 of every baked DDS, before and after
    impostor_draw.sh.bak  the gate as it was before row 14c
    gate_*.txt            each gate's full output
    fixture/              the five re-baked subjects (two roots are required:
                          `registerLooseSheets` walks UP to the nearest ancestor
                          holding `textures/`, so BEFORE stays in IMPOSTORFIX3's
                          tree and AFTER lives here)
    control/ cardres/     the orbit and bake-direction runs, with their logs
    images/               the seven pictures

## 9. THE SHORT VERSION

The ramp is in, it does what its simulation said it would do to the height
sheet, and it does not move any subject's silhouette past the bias bar. The
chip a person sees on a trunk is gone and you can see it go in
`01_trunk_crop_blast_n4.png`.

The maple was never an alpha-tested-leaves problem. It has no leaves. It is a
model whose geometry is finer than the texels it is being photographed onto,
and it sits one rung DOWN a size ladder that is working exactly as designed.
Giving it four times the texels buys a third of the gap back (IoU 0.4673 ->
0.5022) and leaves the rest, which is the honest answer and not a tidy one.

Nothing on the brief's do-not-apply list was applied. The 0.20 coverage cut
beats the shipped floor on all five subjects and the spec's 0.50 loses on all
five; that is a number for a ruling and not a change this lane should make.

## 10. STATE AT CLOSE

`date` at close is in the lane folder's `DONE` marker. Last process check before
the last exe run: `rc=1`, nothing running. The leftover harness instance this
lane created by its own driving mistake (pid 44416, `--port 28401`, wedged on a
path that does not exist) was ended with the documented UDP `NifSkope::open`
unwedge followed by `CloseMainWindow()`; `taskkill` is classifier-denied in this
session. `release/NifSkope.before_impostorfix5.exe` was never executed.

Not done, and named rather than left to be discovered:

* `TILE=256` on the maple -- the next rung, which would say whether section 2's
  curve is still climbing. One bake and one control run, about ten minutes.
* The OTHER source of the maple's 1.089 per cent decode error. The ramp
  removed the cliff; something else puts large ranges in its blocks, and this
  lane did not name it. Row 14c fails the maple on purpose so it stays visible.
* The `ww_harness_window.log` guard refuses every run it sees. Worth a lane.
