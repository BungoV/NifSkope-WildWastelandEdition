# Lane IMPOSTORLOOK1 -- "Impostors look off", said in numbers

Lane opened 2026-09-19 21:44:10 CEDT (first `date` read). OFFLINE ONLY: no
build, no exe run, no in-place edit of existing `src/` / shader / `tests/spells`
files. Lane CELLWORK1 owns the build and exe slots.

NOTE ON THE FILENAME: the harness refuses to write `report.md`. The brief's own
fallback name is taken; this file IS the report.

## 0. State at launch, and the grabs that already exist

New first priority from bungo, relayed by the director 21:45, verbatim:
"Do a comparison for imposters, compare each imposter with its original model
it was baked from, at every taken angle, side by side". That is done first,
in section A, before the shading work.

### Every bake direction, per subject, and which of them a grab exists for

Bake directions are derived from the `.lodm`'s own grid by
`tests/spells/impostor_bake_views.py`, never typed:

    subject    oct N   frames   frame texels   mesh+card grabs at bake dirs
    blast_n4     4       16        48x128      YES  impostorfix5/control/blast_n4_bake_ctl (blend 0)
    blast_n8     8       64        48x128      NO   -- nothing was ever shot at its 64 directions
    maple_n4     4       16        32x64       YES  impostorfix5/control/maple_n4_bake_on (blend 1)
                                                    and .../maple_n4_bake (blend 0)
    dead_n4      4       16        32x128      NO
    rock_n4      4       16       128x128      NO

Sidecar `oct` lines, read out of `scratchpad/impostorfix5_20260919/fixture/*/cards/*.txt`:

    blast_n4   oct 4  48 128  ... base 128
    blast_n8   oct 8  48 128  ... base 128
    dead_n4    oct 4  32 128  ... base 128
    maple_n4   oct 4  32  64  ... base  64   <- one rung DOWN the size ladder
    rock_n4    oct 4 128 128  ... base 128

Every 24-view orbit run in the tree (IMPOSTORFIX1/3/5, IMPOSTORSHOW) is at
ORBIT views, not bake directions, so none of it can fill the three NO rows.
Per the brief and the director's message, no nearby view is substituted:
those cells are marked MISSING and `shoot_missing.sh` is written (NOT run)
for a build-slot lane.

The third column of every cell IS available offline for all five subjects:
the raw baked frame cut straight out of the shipped `_oct_d.DDS`, coverage
decoded from its alpha. That needs no exe.

================================================================================
A. EVERY IMPOSTOR BESIDE THE MODEL IT WAS BAKED FROM, AT EVERY BAKED ANGLE
   (bungo's first priority, verbatim: "compare each imposter with its original
   model it was baked from, at every taken angle, side by side")
================================================================================

Five contact sheets in `images/`, one cell per BAKED frame, labelled with the
frame index and the azimuth/elevation it was photographed from. Each cell:

    the mesh from that bake direction | the card as the viewer draws it |
    the RAW BAKED FRAME cut out of the shipped `_oct_d.DDS`, coverage decoded,
    enlarged by a whole number with NEAREST so you see the stored texels

    A_maple_n4_every_bake_angle.png    16 cells, mesh+card present
    A_blast_n4_every_bake_angle.png    16 cells, mesh+card present
    A_blast_n8_every_bake_angle.png    64 cells, mesh+card MISSING, frames shown
    A_dead_n4_every_bake_angle.png     16 cells, mesh+card MISSING, frames shown
    A_rock_n4_every_bake_angle.png     16 cells, mesh+card MISSING, frames shown

96 directions were never photographed by anybody. No nearby view was put in
their place; `shoot_missing.sh` (written, `sh -n` clean, NOT RUN) shoots exactly
those 192 grabs for a build-slot lane.

THE VERDICT THESE SHEETS DELIVER, and it overturns a published reading:

  On the maple the mesh is a thin twiggy tree, the card is a cluster of BLOBS,
  and the third panel -- the sheet's own frame -- is a recognisable thin tree.
  The detail is IN THE SHEET. It is destroyed by the DRAW.

IMPOSTORFIX5 published "the disagreement is the SHEET's". That was reached by
ruling out the parallax step and never putting the sheet's own frame beside the
final picture. It is wrong for this subject, and the mistake is in MISTAKES.md.

--------------------------------------------------------------------------------
A2. THE SIMULATION, AND ITS KNOWN-ANSWER CONTROL          `sim_cut.py`, SIMULATED
--------------------------------------------------------------------------------

    images/C_sim_cut_maple_n4.png       <-- the single most telling picture
    images/C_sim_cut_blast_n4.png

MESH | CARD as shipped (a real grab) | SIMULATED: the same sheet frame with its
coverage used as OPACITY instead of as a pass/fail test. The right panel is NOT
a render. Nothing was built.

The control had to reproduce first. At a bake direction the blend weights are
(1,0,0) and parallax is a provable no-op, so the shipped card IS one frame,
thresholded; rebuilding that from the sheet and scoring it against the REAL card
grab gives

    blast_n4   IoU 0.9171   (the brief's pre-registered bar was 0.88 -- passes)
    maple_n4   IoU 0.7620   (BELOW the bar. The maple panel is therefore
                             illustrative; its NUMBERS are not leaned on.)

Ink laid down (1.000 would be the mesh's):

    subject    shipped card   coverage as opacity
    blast_n4      1.034            0.910
    maple_n4      1.369            0.740

Read that honestly: coverage-as-opacity does not land on the mesh either. It
overshoots the other way -- the maple goes from 37% too much paint to 26% too
little, because at 32x64 texels a one-texel twig at 20% coverage is a ghost.
That is the reason this repair is listed as NEEDING A RULING and not applied.

================================================================================
1. THE SIDE-BY-SIDE PICTURES (brief item 1)
================================================================================

    B_pairs_maple_n4.png           azimuth 0/90/180/270, native size
    B_pairs_blast_n8.png           azimuth 0/90/180/270, native size
    B_crop_crown_maple_n4_x3.png   the crown, x3 nearest, at a bake direction
    B_crop_trunk_blast_n4_x3.png   the trunk, x3 nearest, at a bake direction
    B_crop_fork_blast_n4_x3.png    a fork,    x3 nearest, at a bake direction

Plain sentences, one per picture:
  - maple pairs: the card is a blob at all four azimuths, and it is the same
    blob at all four -- it does not change character as you walk round it.
  - blast pairs: the trunk is right, the side twigs are simply absent.
  - crown x3: every square is one screen pixel. The card's crown has no holes.
  - trunk x3: the card's trunk is wider than the mesh's and its edge steps.
  - fork x3: the V of the fork is filled in on the card.
The crops are at BAKE directions on purpose: parallax is a no-op there, so they
cannot be blamed on the blend.

================================================================================
2. SHADING, MEASURED FOR THE FIRST TIME (brief item 2)      `shade.py` `look.py`
================================================================================

Everything ever scored on these cards was silhouette overlap. Nothing had ever
measured a colour. Inside the INTERSECTION of the two silhouettes (so a coverage
difference cannot masquerade as a brightness one), over the 24-view orbit runs:

    subject    card/mesh mean linear luma    mean Lab dE (CIE76)
    blast_n4          0.745                       10.97 - 15.70
    blast_n8          0.769                       across the five
    maple_n4          0.716
    dead_n4           0.786
    rock_n4           1.620    <-- the rock's card is 62% BRIGHTER than its mesh
                                   and its brightness swings 0.25 over the orbit
                                   against the mesh's 0.036

THE TRANSFER CURVE -- "flat and dull" in one row. Card linear luma against mesh
linear luma, blast_n4, 190,708 texels:

    mesh luma   0.02  0.06  0.10  0.14  0.18  0.22  0.27  0.35  0.47  0.78
    card luma  0.130 0.130 0.135 0.144 0.154 0.164 0.171 0.178 0.168 0.127

The mesh spans a factor of THIRTY-NINE. The card spans a factor of 1.4. And the
card is DARKEST where the mesh is BRIGHTEST -- it does not merely compress the
range, it turns over at the top. The same shape holds on all five subjects
(rock_n4 runs 0.276 -> 0.352 against a mesh 0.02 -> 0.35). Full table in
`look.txt`.

THE CANDIDATE CAUSE, named at the line. The mesh and the card are drawn into the
same framebuffer through two different colour pipelines:

    quantity     FO4 mesh path                          impostor card path
    ambient      sqrt(lightSourceAmbient.rgb)*0.375     lightSourceAmbient.rgb
    diffuse      sqrt(lightSourceDiffuse[0].rgb)        lightSourceDiffuse[i].rgb
    output       tonemap( colour )                      colour, written raw

  res/shaders/fo4_default.vert:61 and :63 take the two square roots;
  res/shaders/fo4_default.frag:238 defines the filmic curve and :538 applies it;
  res/shaders/impostor_oct.frag's whole output is its last line.

Every other shader in the tree ends in a tonemap -- f76_default,
f76_effectshader, sk_effectshader, skybox, stf_default. The impostor is the only
one that does not. This is a CANDIDATE, not a proven cause of the 0.745 ratio,
because this lane is offline and nobody has drawn a pixel with it. The
DISCRIMINATOR is written into the script's docstring: re-run `look.py`'s
transfer table; if the card's span does not widen, the colour pipeline was not
the cause and the remaining suspects are the missing specular lobe and one sheet
texel per screen pixel.

THE BRIEF'S FOUR SPECIFIC QUESTIONS, answered at the line:

  frame-space normals used as world?  NO -- they are stored in frame space and
    correctly rotated by `frameNormalModel`. But that function computes
    `z = sqrt(max(0,1-dot(xy,xy)))`, which is never negative, so a card normal
    can never face away from its own bake camera; and the light loop then takes
    `abs(dot(normal, L))`. Two independent reasons a card cannot have a dark
    side. That is the "no bright lit side that turns" complaint, mechanically.
  lighting baked into the albedo?  NO -- REFUTED, not assumed. Bucketing covered
    texels by decoded coverage and printing mean albedo luma gives 0.475 at
    coverage < 0.15 against 0.402 at full coverage: the low-coverage texels are
    BRIGHTER, which is the dilate bringing neighbours in, not shading.
  sRGB applied twice or not at all?  The FO4 path's `sqrt()` IS its gamma
    convention. The impostor omits it AND the tonemap. Not twice: not at all.
  AO and depth used, per his ruling?  YES. `src/gl/impostordraw.h:58` has
    `bool bakedAo = true;` and the depth offset drives `gl_FragDepth`. The
    missing SPECULAR highlight is a different matter and is blocked by his own
    standing ruling that material sheets stay debug-only until the FO4/PBRM
    renderer exists -- so it is listed below as blocked, not as a repair.

AN INSTRUMENT THAT DID NOT WORK, named so nobody re-derives it: the trunk
left-minus-right luma band, intended as the "lit side" signal, gives card sd
0.023 against mesh 0.030. Too close to prove anything, because silhouette
asymmetry moves that number as much as shading does. The transfer curve
replaced it.

================================================================================
3. THE OUTLINE: IS THE LUMPY EDGE BC BLOCKS, TEXELS, OR PARALLAX? (brief item 3)
================================================================================
                                                          `period.py` `bloat.py`
PARALLAX: excluded by construction. Those crops are at BAKE directions, where
the parallax step is provably a no-op, and the edge is still stepped.

BC BLOCKS: excluded by measurement. Distance of the trunk edge from a grid of
one sheet texel, and from a grid of four (a DXT block), phase searched, with the
MESH grab through the identical procedure as the control (0.25 = no grid):

    subject   edge  what   to 1 texel   to 4 texels
    blast_n4   L    mesh      0.235        0.237
    blast_n4   L    card      0.186        0.170
    maple_n4   R    mesh      0.177        0.209
    maple_n4   R    card      0.173        0.173

No grid at any multiple is imprinted on the card's edge, and four is no better
than one. Not BC. Full table in `period.txt`.

AND THE HONEST LIMIT: this test cannot separate "texel-shaped" from
"pixel-shaped" in these grabs, because one sheet texel there is 0.72-1.24 SCREEN
PIXELS -- the two grids coincide. Claiming "texel-shaped" from these numbers
would be over-reading them. So the question was moved off the screen and into
the sheet, where no camera is involved:

    subject   frame     trunk @floor  trunk @0.5   bloat/side   ramp   partial
    blast_n4  48x128        8.0          7.0         0.50 tex   2 tex    31%
    blast_n8  48x128        7.5          7.0         0.25 tex   2 tex    31%
    maple_n4  32x 64        2.0          1.0         0.50 tex   1 tex    87%
    dead_n4   32x128        6.0          5.5         0.25 tex   2 tex    37%
    rock_n4  128x128      108.5         13.5        47.50 tex   6 tex    32%

THE FAT TRUNK, EXPLAINED WITHOUT A GUESS. The bake dilates colour outward so
filtering cannot drag background in, so coverage RAMPS across about two texels
at the silhouette. The drawer keeps every texel at or above the sheet's floor
(0.0627), so the card's outline is the 0.0627 contour of that ramp -- half a
texel outside the real edge, on each side. On a trunk 7 texels wide that is 14%
fatter; on the maple, whose trunk is ONE texel at half coverage, it is DOUBLE.
And a one-texel wobble on a 7-texel trunk IS the lumpiness: the edge is coarse
because the trunk is only seven texels, not because of any block.

rock_n4's row is the extreme and is flagged rather than explained: its bottom
band spans 108 texels at the floor against 13.5 at half coverage. That subject
also carries the 1.620 luma ratio. A card drawing a large low-coverage halo at
full opacity would produce both, and this lane could not test it -- the rock has
no mesh grab at any bake direction. `shoot_missing.sh` shoots it.

WHY NOT A STANDARD DEVIATION, recorded so it is not tried again: the deviation
of the edge about a straight line gives the OPPOSITE verdict on blast_n4 -- card
1.84 texels against mesh 2.33, i.e. the card measures SMOOTHER, because the
mesh's deviation is real bark and real taper. Roughness was never the question.

================================================================================
4. THE RANKED LIST (brief item 4)
================================================================================

1. THE CROWN IS BLOBS -- and the detail is in the sheet
   bungo sees        a fine-twig tree becomes a solid clump that never changes
                     shape as he walks round it
   stage             DRAW
   repair            res/shaders/impostor_oct.frag -- coverage is tested
                     (`if ( colour.a < alphaThreshold ) discard;`) and then
                     thrown away, `fragColor = vec4( ..., 1.0 )`. A texel at 7%
                     coverage paints as solid as one at 100%, and 87-94% of this
                     subject's silhouette is partial coverage.
   simulated         images/C_sim_cut_maple_n4.png (control 0.762, below bar)
                     images/C_sim_cut_blast_n4.png (control 0.9171, passes)
   re-bake?          NO. The sheet on disk already holds what is needed.
   ruling?           YES -- this IS the owed alpha cut-off ruling, and the
                     simulation shows why it cannot be applied blind: it
                     overshoots to 26% too little paint on the maple. The
                     choices (coverage as opacity, a lower cut, or coverage to
                     alpha with a sharpen) are his.

2. FLAT AND DULL, NO TONAL RANGE, NO LIT SIDE THAT TURNS
   bungo sees        the mesh has a bright side that moves with the camera; the
                     card is evenly, flatly lit and slightly too dark
   stage             DRAW
   repair            res/shaders/impostor_oct.frag, three lines, against
                     res/shaders/fo4_default.vert:61,63 and fo4_default.frag:538
                     (plus the two-sided `abs( dot( normal, L ) )` and the
                     never-negative z in `frameNormalModel`)
   simulated         NO PICTURE. Simulating a shader's output without running it
                     would be a drawing, not a measurement. The transfer table
                     in section 2 IS the before half; the after half needs the
                     build slot and the discriminator is written down.
   re-bake?          NO
   ruling?           NO -- `hookup_cardlight.py` is written and checked, NOT
                     applied. The `abs()` and the z-clamp are NOT touched by it:
                     removing them makes half of every card black, which is a
                     look decision and therefore his.
   NOT A CLAIM that this alone makes the cards match.

3. NO SPECULAR HIGHLIGHT AT ALL
   bungo sees        the mesh's wet/waxy highlight is simply not on the card
   stage             DRAW
   repair            BLOCKED by his own standing ruling: material sheets stay
                     debug-only until the FO4/PBRM renderer exists. Listed so it
                     is not re-discovered as news.
   ruling?           already ruled -- do not touch

4. THE TRUNK IS FATTER AND ITS EDGE IS LUMPY
   bungo sees        a fat, slightly wobbly trunk where the mesh's is slim
   stage             SHEET (the coverage ramp) x FRAME SIZE
   repair            the floor cut is the same line as defect 1; the coarseness
                     is `LodGeneration/cardRes` and the frame size in the `.txt`
                     sidecar that `lodgen --impostors` reads
   simulated         the two tables in section 3
   re-bake?          YES for the size half -- the frame size lives in the bake
   ruling?           YES, both halves. Nothing applied.

5. THIN BRANCHES COME OUT THICKER AND SHORTER; SIDE TWIGS ABSENT
   bungo sees        branches that end early and are too thick
   stage             SHEET, frame size
   repair            same as 4. A twig thinner than one texel cannot survive a
                     32-texel-wide frame however the alpha is handled.
   re-bake?          YES
   ruling?           YES (frame size is owed)

THE THREE OWED RULINGS AND THEIR SHARE OF "LOOKS OFF" -- measured, none applied:

  alpha cut-off   the largest single share on the fine-twig subject: it is the
                  whole of defect 1. Card ink / mesh ink 1.369 -> 0.740 when
                  coverage is used as opacity (maple), 1.034 -> 0.910 (blast).
                  It does NOT repair the maple; it swaps the error's sign.
  frame size      a real share, and not the whole: IMPOSTORFIX5 measured 64x128
                  frames at IoU 0.5022 / ink 1.250 against the shipped 32x64's
                  0.4673 / 1.472. Doubling the long side recovers about a third
                  of the excess ink and 0.035 of IoU -- and takes the maple's
                  trunk from one texel to two at half coverage.
  `_n` channel    SHARE BOUNDED, NOT MEASURED -- and the bound is the useful
  swap            part: whatever order the channels are in, `frameNormalModel`
                  cannot produce a normal facing away from its bake camera and
                  the light loop takes `abs()`. A swap can change WHICH side of
                  a card is bright. It cannot create the missing contrast, so it
                  cannot be more than a small part of "flat and dull".

================================================================================
5. THE ONE PROPOSED SOURCE CHANGE                  `hookup_cardlight.py` NOT RUN
================================================================================

Refusing anchored script, skill `ww-anchored-hookup`, three exact-once anchors
on `res/shaders/impostor_oct.frag`. `python hookup_cardlight.py --check`, quoted:

    E:/Projects/NifskopeWildWastelandEdition/res/shaders/impostor_oct.frag
      19978 bytes, CR 0, LF 411, sha1 badf7eb982e1da3654c407c9721e9982045ab4ce
      anchor the light loop head                  1 match
      anchor the diffuse term                     1 match
      anchor the ambient term and the output      1 match
    OK: 3 anchors, 1 match each. Nothing was written.

It gives the card the mesh path's ambient, diffuse and tonemap. It does not
touch the alpha cut-off, the `_n` order or the frame size. NOT APPLIED. Nothing
in this lane has been built, run or drawn.

================================================================================
6. MISTAKES AND THE SKILL
================================================================================

MISTAKES.md (root), two entries spliced to the top on the exact-once anchor
"Newest at the top.", CR/LF asserted either side:
  APPLIED 659195 -> 661474 bytes, CR 10668 -> 10705, LF 10668 -> 10705.
  (a) "the disagreement is the SHEET's", published without ever putting the
      sheet's own frame beside the draw. Rule: a claim of the form "the defect
      is in stage X" ships with stage X's OWN output beside the final one.
  (b) the brief named the fallback filename and this lane used the refused one
      first.

Skill `ww-reference-card-diagnose`, section 9 "THE SHADING ARM: a card can be
the right SHAPE and still look wrong", appended to BOTH trees:
  20095 -> 24378 bytes, CR 0, sha1 4b7019ffa60290a852d25b9b89f3ba68e6d3d567,
  equal in `E:/Projects/Claude/.claude/skills/` and `<repo>/.claude/skills/`.

================================================================================
7. WHAT WAS NOT MEASURED -- so nobody reads more into this than it holds
================================================================================

  - Nothing was built, run or drawn. Every "after" here is arithmetic on files
    that already existed.
  - 96 of the 128 bake directions have no mesh grab. blast_n8, dead_n4 and
    rock_n4 have NO side-by-side at any baked angle at all.
  - The maple's simulation reproduced the shipped draw at IoU 0.762, below the
    0.88 control bar. Its picture is evidence; its numbers are not.
  - The shading numbers come from the 24-view ORBIT runs (blend and parallax
    ON), not from bake directions. They are what the player sees, which is what
    the complaint is about, but they mix the blend in.
  - "Texel-shaped" is NOT claimed for the outline: at 0.72-1.24 screen pixels
    per sheet texel the two grids coincide in those grabs.
  - rock_n4's 1.620 luma ratio and its 108-texel floor contour are unexplained.
  - No re-bake was run, so every frame-size number is IMPOSTORFIX5's.
  - The shading comparison uses the mesh as drawn by THIS viewer's headlight
    (`src/glview.cpp:3745-3810`, `lightSourcePosition[0] = (0,0,1)` in view
    space), not by the game's sun. The divergence measured is between two
    pipelines in one window; it is not a claim about how either looks in game.
