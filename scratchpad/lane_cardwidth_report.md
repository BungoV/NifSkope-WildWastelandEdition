# Lane CARDWIDTH — where the card's extra width came from, and what it actually was

**STATE: BUILT, RE-BAKED AND GATED — see section 15 (Build), which supersedes the
"PENDING" in sections 9–10.** `Fallout4.exe` was up (PID 12500) when the lane
started, so sections 1–14 were all produced offline; it was DOWN at the lane's
one build check (`rc=1`, 02:05) and the build and the whole chain then ran.
Headline: **110 ok / 1 FAIL** on `lodgen_octahedral` (F1 refused at 1.78 texels,
identical to the pre-fix build's 1.78 — the fix moved the cube by nothing),
three other harnesses PASS, 19 trees re-baked, **0 texels of 19 sheets carrying
an alpha the contract forbids**, and the contract in all 19 sidecars, all 18 card
`.lodm` and all 38 sample-set card entries.

Nothing was committed (CONSTITUTION 8). **bungo's open NifSkope window predates
the change: the next launch of `release\NifSkope.exe` (02:07:48) has it.**

Nothing was committed (CONSTITUTION 8).

Scripts, all runnable with the game up:
`scratchpad/cardwidth_20260910/{sheetlib,recon,measure,threshold,law,reencode,base_sweep}.py`,
outputs `{recon,measure,threshold,law,reencode}.md` beside them.

---

## 1. The headline, in one paragraph

**The bake's coverage was never wide.** At the card's own texel pitch the sheet
reproduces the source silhouette's box to **under one texel** on every tree and
both axes. The 18–121 % "width excess" of lane CARDORTHO's trunk table is bought
by RESOLUTION and by that instrument's own defects, and section 4 measures both
with the card taken out of the comparison entirely. What IS broken is a
DISAGREEMENT: the bake defines coverage at **16/255** and writes every extent in
the `.lodm` for that silhouette, while both specs tell the consumer to
**alpha-test at 0.5**. At 0.5 the drawn silhouette is up to **5.41 texels of
half-width short** — TreeHero01 hands over to a card **9.1 % narrower and 3.2 %
shorter than its own mesh**, which is bungo's rule broken in the size axis. The
fix writes the coverage so that the reader's own test selects exactly the
coverage the bake measured.

---

## 2. What the on-disk run actually contained (`recon.md`)

Read before any width was compared, because a mask that touches the viewport edge
is the viewport's number and not the object's.

| render family | clipped by the viewport? |
|---|---|
| the three `*_ortho_src.png` | **YES** — all three, 941 of 941 rows |
| the six `*_mid_src.png` | **YES** — all six |
| the six `*_ring_src.png` | no |

The card quads at the orthographic camera's own scale are 1435 px tall against a
941 px viewport on all three trees, so BOTH arms of lane CARDORTHO's trunk table
fill the frame top to bottom.

**Consequence, and it is a mistake of the earlier instrument, not a finding of
this lane: that table's `height diff 0.00 % on all three trees` is 941 = 941 —
the viewport's height twice over, not an agreement.** Its `bottom fifth` and
`top fifth` bands are fifths of the CLIPPED body, so they are not the trunk base
and not the crown; and every `dy extent` on a `mid` row of the transition table
(0.00 %, 0.87 %, 1.30 %, …) is the same artefact. Recorded in `MISTAKES.md`.

Only the six `ring` renders are usable offline, and every number in sections 3–5
that needs a source render uses those.

---

## 3. The four candidates, separated (`measure.md`)

The reference is the bake's OWN pass one: `framefit <maxDx> <maxDy> …` in the
sidecar is the widest (tallest) single view's silhouette half-box in world units,
measured on the 941-px viewport matte — **7.4× the card's resolution**. No render
is needed to compare the sheet against it, so none of a render's confounds get in.

### 3.1 Dilation — REFUTED, exactly zero

`lodgenDilateFrames( alb, alb, … )` is called with `img == coverage`, so its
`put` writes the existing alpha back. Re-implemented offline and run over all
three sheets:

| tree | texels where the dilated alpha differs from the baked alpha |
|---|---|
| 0003a28b | **0** |
| 0004a074 | **0** |
| 00038599 | **0** |

Dilation pushes colour outward and coverage not at all. Candidate dead.

### 3.2 BC compression of the alpha channel — REFUTED, ≤ 0.5 texel

The shipped `_oct_d.DDS`'s top mip decoded (BC3 alpha: two endpoints, an
eight-step ramp, three bits a texel) against the PNG's:

| tree | texels whose VALUE moved | half-extent moved at 16/255 | at 0.5 |
|---|---|---|---|
| 0003a28b | 167,254 | 0.00 texels | 0.00 x, 0.50 y |
| 0004a074 | 57,343 | 0.00 | 0.00 |
| 00038599 | 7,132 | 0.00 | 0.00 |

BC3 moves a great many alpha VALUES and the silhouette by at most half a texel.
Candidate dead.

### 3.3 The coverage floor (16/255) — NOT the excess; the sheet is right there

| tree | ref halfW, texels | sheet at 16/255 | error | ref halfH | sheet at 16/255 | error |
|---|---|---|---|---|---|---|
| 0003a28b | 59.41 | 59.50 | **+0.09** | 58.89 | 59.50 | **+0.61** |
| 0004a074 | 27.26 | 28.00 | **+0.74** | 59.41 | 59.50 | **+0.09** |
| 00038599 | 4.27 | 5.00 | **+0.73** | 29.70 | 30.00 | **+0.30** |

Under one texel on all six. The crop, the placement and the downsample are sound.

### 3.4 The reader's alpha test against the bake's coverage definition — THE CAUSE

The same six half-extents read at the consumer's own test, 0.5 = 128/255
(`docs/LODGEN_CARD_SHEETS.md` §4 and `docs/LODGEN_IMPOSTOR_SPEC.md`: *"A consumer
alpha-tests at 0.5"*):

| tree | axis | error at 16/255 | **error at 0.5** | as a fraction of the declared half-extent |
|---|---|---|---|---|
| 0003a28b | x | +0.09 | **−5.41 texels** | **−9.1 %** |
| 0003a28b | y | +0.61 | −1.89 | −3.2 % |
| 0004a074 | x | +0.74 | −1.26 | −4.6 % |
| 0004a074 | y | +0.09 | −1.41 | −2.4 % |
| 00038599 | x | +0.73 | −0.27 | −6.4 % |
| 00038599 | y | +0.30 | −0.20 | −0.7 % |

The threshold at which the card's silhouette equals the source's is **17, 71, 56,
51, 78, 83** over those six — there is no single value near 128, and the value
that works is a property of how lacy the silhouette's extremity is, not a
constant. The `.lodm` declares a quad sized to the 16/255 silhouette; the
consumer draws the 0.5 one; the difference is the tree changing size at the
transition.

Known-answer control on the metric itself (`measure.md` §D): a rectangle of
half-width 5.946 texels reads +0.05 at both thresholds; one at 6.014 reads +0.99
at 16 and −0.01 at 128. The metric's own error is the ±1-texel box quantisation,
an order below the −5.41 it is being asked to see.

---

## 4. The 18–121 % width excess: it is RESOLUTION (`threshold.md` §K)

Lane CARDORTHO's trunk table took the widest row of a band of the card against
the same band of the source and got +18 % to +121 %, and named
coverage-threshold dilation as the candidate with "re-run at two frame sizes" as
its discriminator. The measurement below is stronger than that discriminator
because **the card is not in it at all**: the fine 941-px source mask against the
SAME mask box-filtered to the card's own texel pitch — which is exactly the
quantity the bake's downsample computes — and thresholded.

| band | excess at 16/255 | excess at 0.5 |
|---|---|---|
| bottom fifth, six views | +2 % … +15 % | −8 % … +5 % |
| top fifth (crown), six views | **+36 % … +65 %** | +7 % … +32 % |

A crown of twigs thinner than a texel cannot be held by a 128-texel frame, and
the widest-row-of-a-band statistic reads that as width. The remainder between
+65 % here and +121 % there is the earlier instrument's own: it upsamples the
frame alpha ~11× with BILINEAR and then thresholds at 16, and it does it on a
clipped render.

Floors under this measurement: at pitch 1.0 the whole pipeline returns **+0.00**
on all twelve half-extents (`threshold.md` §L), and a disc of known radius comes
back to +0.00 … +0.89 texels with the same quantisation (§M).

---

## 5. Which threshold is right, decided with no bake in it (`threshold.md` §J)

Section 3.3 compares the sheet at 16/255 against a pass-one number also taken at
16/255 — fine for the downsample, circular for the threshold. This decides the
threshold inside the source render alone: the fine mask box-filtered to the
card's texel pitch, read back at each candidate, against the fine mask's own box.

| rule | worst absolute error over twelve half-extents |
|---|---|
| conservative, `fraction >= 16/255` | **0.63 texels** |
| majority, `fraction >= 0.5` | **2.07 texels** |

**16/255 is the coverage definition that reproduces the source silhouette; 0.5 is
not.** So the fix is not to move the bake's floor — it is to make the reader's
own test select the set the floor defines.

---

## 6. A law tried and REFUSED, with numbers (`law.md`)

Before the encoding below, the standard alpha-coverage-preserving downsample was
tried on the sheet bytes: a box filter conserves the alpha integral, so
`sum(alpha)/255` is the source's covered area in texel units exactly, and `k` was
bisected per frame so that `|{ k·alpha >= 128 }|` equalled it (Castaño's rule with
the target taken from the frame's own integral).

| tree | widest half-extent vs the source, before | after the law |
|---|---|---|
| 0003a28b | −9.1 % | **−9.1 %** |
| 0004a074 | −4.6 % | −2.8 % |
| 00038599 | −6.4 % | **−6.4 %** |

`k` came out 1.00–1.27 because the outermost twigs carry almost no AREA, so
preserving area barely promotes them. Worse, its own floor caught it: a solid
rectangle of half-width 27.12 texels was **fattened by a whole texel** (`k` 5.12)
where a solid silhouette must not move at all — it would fail the cube fixture it
has to pass. Rejected with numbers; it stays rejected unless new evidence is
brought (CONSTITUTION 7).

---

## 7. The fix

**Write the coverage so that the reader's own alpha test selects exactly the
coverage the bake measured.** In the bake's second pass, the base-colour sheet's
alpha is re-encoded before it is written:

```
a' = 0                                                     a <  floor
a' = base + round( (a - floor) * (255 - base) / (255 - floor) )   otherwise
```

with `floor = 16` (the spec's coverage floor, and section 5's measured winner),
`test = 128` (the alpha test both specs name) and `base = 160`.

* `{ a' >= 128 } == { a >= 16 }` **exactly**, by construction — measured, 0
  disagreeing texels of 209,793 / 64,709 / 13,510 (`reencode.md` §N);
* 255 stays 255, so a solid interior does not move and the cube fixture cannot be
  fattened;
* the fraction survives monotone and invertible in `[base, 255]`:
  `a = floor + (a' - base) * (255 - floor) / (255 - base)` recovers it to a
  **maximum error of 1 alpha step** (`reencode.md` §O), so a consumer that blends
  instead of testing loses nothing that matters;
* an empty texel is written 0 instead of 1..15, so the sheet's coverage set and
  the dilation's `filled` set are the same set by construction.

### 7.1 Why `base = 160` and not 128

The sheet ships as BC3 and `lodgenWriteDds`'s alpha block is endpoints
`max`/`min` with an eight-step ramp and nearest-palette indices, so a texel's
alpha can move by at most half a step, `(aMax − aMin)/14 ≤ 255/14 = 18.2`.
A covered texel written at 128 can therefore round BELOW the test; written at 160
it cannot, because 160 − 18.2 = 141.8 > 128. Both halves measured over the three
sheets through a faithful model of that encoder (`base_sweep.py`):

| base | disagreeing texels after a BC3 round trip, three sheets |
|---|---|
| 128 | 5,250 / 2,095 / 29 |
| 144 | 0 / 0 / 0 — but 144 − 18.2 = 125.8, under the test, so it is not PROVED |
| **160** | **0 / 0 / 0**, and proved by the bound |
| 176, 192 | 0 / 0 / 0, at more of the fraction spent |

160 keeps 96 of the 256 levels for the fraction, against the ~8 a BC3 block can
resolve anyway.

### 7.2 Where it lands

* `src/nifskope_ui.cpp`, the octahedral bake's pass two — the encode, and a
  `coverage <floor> <test> <base>` line in the sidecar.
* `src/lodgen.cpp`, the card functions ONLY (`lodgenCard`'s sidecar reader and
  the card `.lodm` writer, plus the card-array layer) — the line is parsed and
  carried into the `.lodm` as a `coverage` object. **Absent = a set from before
  this, whose coverage is a raw fraction and whose consumer must test at the
  floor**, so every `.lodm` written before the key is byte-identical still.
* Not touched: `lodgenDilateFrames` (its `>= 16` still selects exactly the
  covered set), `lodgenWriteDds` (shared with the terrain sheets — the base
  above is chosen so the writer needs no change), and the legacy crossed `_fs`
  card sheet (outside this lane's files).

---

## 8. The fixture, PRE-REGISTERED

Written here before the build, and before `tests/spells/lodgen_octahedral.sh` was
edited. The existing `bake 4` block already bakes a 512-unit cube and predicts
each frame's span from the cube's own measured half-extent; it reads the mask at
`alpha >= 16` against a 2-texel bar. Added, at the READER's threshold:

* **F1** — every frame of the cube's orthographic bake spans its predicted texels
  within **1.0 texel** with the mask taken at `alpha >= 128`.
* **F2** (floor) — the perspective control must EXCEED 1.0 texel on the same
  measurement.
* **F3** — the base-colour sheet carries **0 texels** with alpha in `1 … 159`:
  coverage is written at or above the base, or not at all.
* **F4** (floor) — the same count on the DECODED alpha must be **> 0**, so F3 is a
  check that can fail on its own input.
* **F5** — `coverage 16 128 160` in the sidecar and mirrored into the card
  `.lodm`; a `.lodm` built from a sidecar without the line carries no `coverage`
  key.

And for the transition step, the floor lane CARDORTHO B.5 found missing:

* **F6** (extent floor) — a card composited with its declared half-width
  multiplied by 1.05. Lane CARDORTHO's `dx extent` column read the same value on
  the card arm and its zeroed-offset control on 12 of 12 rows, so that bar had
  nothing under it.
  **F6 was RESTATED after the instrument's dry run and before any verdict**, and
  the restatement is in `MISTAKES.md`. It was registered as "the wide card must
  FAIL the 2 % bar", which cannot hold: on a row where the card is already too
  narrow, widening it moves it TOWARDS the source and the control passes for the
  right reason (measured: 0003a28b front went 6.11 % → 1.11 %). A floor for an
  extent bar has to show that the COLUMN RESPONDS, so the bar is **the dx extent
  moves by at least 2.5 of the 5 points applied, on every row** — met on the six
  dry-run rows at 2.77 to 5.27.

---

## 9. The pictures, made offline and opened before delivery

`scratchpad/cardwidth_20260910/make_pictures.py`, three columns per
`ww-texel-picture` §6, because this is a change of LAW and not of content: the
sheet at the bake's own floor, the SAME sheet read at the consumer's 0.5, and the
sheet after the re-encoding read at the same 0.5. The frame drawn is chosen BY
THE METRIC over all 64 — the one that loses the most half-width — not by eye.

| picture | frame | half-width, floor → 0.5 today → re-encoded, texels | texels the 0.5 reading loses |
|---|---|---|---|
| `cardwidth_coverage_0003a28b.png` | (1,0) of 8×8, 128×128 | 56.0 → **48.0** → 56.0 | **1,711** |
| `cardwidth_coverage_0004a074.png` | (4,5), 64×128 | 20.5 → **14.0** → 20.5 | **513** |
| `cardwidth_coverage_00038599.png` | (1,0), 16×64 | 5.0 → **4.0** → 5.0 | **75** |

All three were opened and read back. `0004a074`'s middle panel is the clearest
statement of the defect in the round: **the whole crown is red** — a consumer
alpha-testing at 0.5 draws that tree's trunk and almost nothing else, 32 % of its
half-width gone.

(These frames are not the ones in §3.4's table: that table reads the WIDEST frame
of the sheet, this picture reads the frame that LOSES the most. Two statistics of
the same sheet, both stated.)

**Still owed and PENDING: the source | card | overlay picture per tree.** It needs
a render, and `Fallout4.exe` was up. `transition2.py` writes it as
`cardwidth_transition_<id>.png` in the same folder when `run.sh` is run.

---

## 10. Gates — every one PENDING

Nothing was built and no harness was run: `Fallout4.exe` (PID 12500) was up at
the lane's single build check.

| gate | bar | state |
|---|---|---|
| `lodgen_octahedral.sh` | RESULT PASS with F1–F5 | **PENDING** |
| `lodgen_card_arrays.sh`, `lodgen_impostor_cards.sh`, `lodgen_identity.sh` | must not move | **PENDING** |
| the 19-tree re-bake | 19 baked, 0 failed | **PENDING** |
| the transition on the fixed instrument | 12 of 12 within 1 texel / 2 % | **PENDING** |
| the zeroed-offset control | 0 of 12 | **PENDING** |
| F6, the extent floor | dx moves ≥ 2.5 points on every row | **PENDING** (dry run: 2.77 … 5.27 on six rows) |
| the FO4CS sample set + MANIFEST rows | regenerated | **PENDING** |

What DID run, with the game up, and is not pending: everything in
`scratchpad/cardwidth_20260910/*.md`, the dry run of the transition instrument's
non-exe half (`dryrun.py`, 18 rows), the three texel pictures, `bash -n` on both
shell scripts, and `compile()` over all ten embedded Python blocks of
`tests/spells/lodgen_octahedral.sh`, plus `py_compile` over every script.

The C++ has NOT been compiled. The resume is
`scratchpad/cardwidth_20260910/PENDING.md`.

---

## 11. What is NOT claimed

* That the fix works. It is measured on the sheet BYTES — the re-encoding is
  arithmetic and its result is exact — but no bake has run under it.
* That the transition gate will pass 12 of 12. The card cannot hold a twig
  thinner than a texel whatever the threshold, and §4 measures that a crown band
  reads +36 % to +65 % wider at the card's own pitch from resolution alone. The
  2 % bar the earlier lane pre-registered is a bar on a BOX, not on a band, and
  the boxes are what §5 says agree to 0.63 texels — but that is a prediction, not
  a result.
* That the mip chain holds the contract. A box filter over the re-encoded alpha
  contracts the silhouette at every level exactly as it did before (three of four
  covered neighbours average to 191, one of four to 64), so a card sampled at
  mip 1 or deeper draws a smaller tree. True before this change, unchanged by it,
  and an owed item (§13).
* Anything about the legacy crossed `_fs` card. Not this lane's file, and its
  alpha is untouched.
* That `docs/LODGEN_IMPOSTOR_SPEC.md` is consistent with the tree. It is not;
  `CHANGE_NEEDED.md` names the two passages.

---

## 12. Mistakes

Four, all also in `MISTAKES.md` at the root:

1. **(recognised in lane CARDORTHO's artefact)** its `height diff 0.00 %` is the
   viewport's height twice — all three orthographic and all six `mid` renders are
   clipped. Rule: a mask that touches the viewport edge is refused, not measured.
2. **(mine)** the first extent floor was built in the wrong DOMAIN — resampling
   the 128-texel frame to 105 % and re-thresholding made the card measure 0.93 %
   *narrower*. A control belongs in the domain and at the resolution of the
   number it floors.
3. **(mine)** F6 was pre-registered with a statement that cannot hold, and was
   restated after the dry run, before any verdict. Recorded rather than quietly
   amended.
4. **(mine, a repeat)** the heredoc apostrophe trap from the 2026-09-09 CARDFIT3
   entry, one lane later. Caught in two seconds by that same entry's rule (b) —
   `compile()` the harness's embedded Python — instead of by a ten-minute suite.

And one caught before it became a claim rather than after: the first offline model
of `lodgenDilateFrames` **computed the growth and never wrote it**, so "0 texels
differ" would have been a tautology. It was rebuilt with the `isCoverage` branch
as an argument and run BOTH ways — the coverage path moves 0 alphas, the aux path
moves 838,783 / 459,579 / 52,026. That is what makes the 0 a result.

---

## 13. Owed

* The build, the harnesses, the re-bake, the transition table and the
  source | card | overlay pictures — `run.sh`.
* `docs/LODGEN_IMPOSTOR_SPEC.md`, two passages — `CHANGE_NEEDED.md`.
* **The mip chain under the contract.** The cap already stops while a frame's gap
  is a whole texel, but nothing has measured what the SILHOUETTE does down the
  chain: a box filter contracts it, so a card sampled at mip 1 draws a tree
  smaller than the one at mip 0 — the same class of defect this lane fixed at
  mip 0. The measurement is `measure.py`'s §B run per mip level; the candidate is
  the alpha-coverage-preserving scale per level that §6 refused for the EXTENT
  problem but which is exactly right for an AREA one. Not started. Its own lane.
* Whether FO4CS's card shader tests at 0.5 at all, or blends. The contract now
  tells it which value to use; nobody has read the consumer.

---

## 14. Skill review (CONSTITUTION 1a)

**Loaded and used.** `ww-texel-picture` — its §6 (three columns for a change of
LAW) is exactly the picture this round needed, its §1 (pick the crop by the metric
over every orientation) chose the frames, and its §5 (open the picture) caught two
layout defects the script's own output could not show: a page of empty ground
under a short frame, and captions running into their neighbours on the 430-px
cells. `ww-control-calibration` — every number in §§3–7 carries a floor, and it is
why the dilation model was rebuilt to run both ways instead of being believed.
`nifskope-ww-lodgen`'s editing traps and `nifskope-ww-build-verify`'s "compile the
harness's embedded Python" rule, which caught mistake 4 in two seconds.

**Named in the brief and NOT used, with the reason.** `ww-analytic-fixture-gate` —
the cube fixture it governs already exists in `bake 4`, and this lane extended it
rather than authoring one, so its procedure was not re-derived. `ww-sheet-diff` —
the comparison here is a silhouette SET, not a sheet's bytes.
`nifskope-ww-render-shot` and `nifskope-ww-build-verify` — the exe could not be
launched; both are named in `PENDING.md` for the resume.

**The skill that should have existed, and is now written:**
`ww-silhouette-compare`. Three lanes in a row (CARDFIT3, CARDORTHO, CARDWIDTH)
have re-derived the same procedure for "does our generated silhouette match the
source's", and two of them got it wrong in ways that cost a round: CARDORTHO
measured clipped renders and reported the viewport's height as an agreement, and
this lane built its first floor in the wrong domain. What keeps being re-derived:
refuse a clipped mask before measuring it; put the two arms at the SAME resolution
before quoting a percentage, because a band statistic across a resolution step
reads +36 % to +65 % with nothing wrong; take the reference from the generator's
own higher-resolution pass where one exists, rather than from a render; and
separate a THRESHOLD question from a DOWNSAMPLE question by running the whole
comparison inside the source alone. It carries the two floors that belong under
every such measurement (pitch 1.0 must return 0.00; a known-radius disc must
return its radius) and the rule that a control is built in the domain of the
number it floors.

**Declined:** nothing else recurs. The BC3 alpha bound
(`(aMax − aMin)/14 ≤ 18.2`) is one line of arithmetic against a ten-line encoder
and does not need a page.

---

## 15. Build — the game went down at the build step, so none of this is pending

`tasklist | grep -i -E "Fallout4|NifSkope"` printed **rc=1** at the lane's single
build check (02:05), so the brief's build rule was satisfied and the chain ran.
Sections 9–10 above were written before that and their "PENDING" is superseded
here. `scratchpad/cardwidth_20260910/run.log` is the whole chain.

### 15.1 The build

Syntax-only pass first (`-fsyntax-only`, flags read out of `Makefile.Release`):
`src/lodgen.cpp` **RC=0**, `src/nifskope_ui.cpp` **RC=0**, only pre-existing
warnings. Then `qmake NifSkope.pro` **rc 0**, `make -j2` **rc 0**.

| artefact | mtime |
|---|---|
| `release/NifSkope.exe` | **2026-09-10 02:07:48** |
| `release/style.qss` | 02:07:49, `cmp res/style.qss release/style.qss` in step |
| `src/nifskope_ui.cpp` | 01:48:30 (object 02:07:30) |
| `src/lodgen.cpp` | 01:49:32 (object 02:07:04) |
| `tests/spells/lodgen_octahedral.sh` | 02:06 |

Staleness sweep over all five changed files: **0 stale**. `qmake` was re-run
because `NifSkope.pro` had changed — **not by this lane**: lane WATER3 added
`src/watermark{,panel}.{cpp,h}` to it and had never compiled them. They built
clean, and this exe therefore carries WATER3's four new files as well as this
lane's two. Said here because a shared build is a fact about the exe, and because
WATER3's own report still says BUILD PENDING.

### 15.2 The gates, against section 8's pre-registration

| gate | bar | measured | verdict |
|---|---|---|---|
| `lodgen_octahedral.sh` | RESULT PASS | **110 ok, 1 FAIL** | **REFUSED — F1 only** |
| **F1** | cube within **1.0** texel at the reader's threshold | **1.78** | **REFUSED** |
| **F1b** | the reader's threshold and the bake's floor measure the SAME silhouette | **1.78 vs 1.78** | ok |
| **F2** (floor) | the perspective control exceeds 1.0 | **18.70** | ok |
| **F3** | 0 texels of the base sheet with alpha in 1…159 | **0** | ok |
| **F3** | the set tested at 128 == the set floored at 16 | **116,752 vs 116,752** | ok |
| **F4** (floor) | the DECODED alpha carries them, > 0 | **4,208** | ok |
| **F5** | `coverage 16 128 160` in the sidecar and in the `.lodm` | present, equal, and `160 − 255/14 = 141.8 > 128` | ok |
| `lodgen_card_arrays.sh` | must not move | **35 ok, 0 FAIL, PASS** | ok |
| `lodgen_impostor_cards.sh` | must not move | **12 ok, 0 FAIL, PASS** | ok |
| `lodgen_identity.sh` | must not move | **8 ok, 0 FAIL, PASS** | ok |
| the 19-tree re-bake | 19 baked, 0 failed | **19 / 0** | ok |
| its sidecars | 19 of 19 state the contract | **19 of 19** | ok |
| the 19 sheets | 0 texels with alpha in 1…159 | **0 on all 19** | ok |
| the card `.lodm` | every one carries the contract | **18 of 18, `{base 160, floor 16, test 128}`** | ok |
| the FO4CS sample set | every card entry carries it | **38 of 38**; MANIFEST.md 197 rows | ok |
| the transition | 12 of 12 within 1 texel / 2 % | **0 of 8** (4 rows refused) | **REFUSED** |
| the zeroed-offset control | 0 of 12 | **0 of 8** | ok |
| **F6**, the extent floor | dx moves ≥ 2.5 points on every row | **8 of 8**, 2.86 … 8.00 points | ok |

### 15.3 F1, refused, and why it is not the fix

**F1 is refused at 1.78 texels against a pre-registered 1.0, and the SAME
instrument read 1.78 on the pre-fix build** (lane CARDORTHO B.2, "cube spans,
run-time table: worst 1.78"). The re-encoding moved the cube by **0.00** texels,
which is exactly what it must do to a solid silhouette — and F1b says the same
thing from the other side: the reader's threshold and the bake's floor now measure
the same silhouette to 0.01, where on a pre-contract sheet they are 5.41 texels
apart.

The 0.78 texels the bar does not cover are the existing predictor's own: the
crop's integer rounding plus one antialiased texel each side, which is why the
pre-existing check beside it is written at 2.0. **The bar was carried from the
brief and is a hypothesis about the instrument, not about the bake** — the same
error as the 2026-09-09 CARDFIT3 entry. It is left RED rather than moved, with the
decomposition above.

### 15.4 The transition, and what its refusal is made of

The pre-registered 12 rows became **8**: `transition2.py` refuses a source mask
under 200 px, and at the `ring` distance (the tree filling 15 % of the viewport
height) the two small trees photograph 169 and 174 px. **That floor is too blunt
for a distance whose whole point is a small object** — an instrument defect, §16.

Of the 8 that ran, **0 meet both bars**. The zeroed-offset control also passes 0
and the extent floor F6 fires on 8 of 8, so both columns are live. Card against
control on the centre column, in pixels:
**4.92 / 2.24 / 4.92 / 0.71 / 1.12 / 5.00 / 2.24 / 1.58** against
**11.01 / 4.24 / 10.26 / 2.12 / 10.26 / 10.20 / 3.16 / 2.12** — the card arm is
closer on every row, and 4 of 8 fall inside `max(1 texel, 1 px)` against the
control's 1 of 8.

**The "within one card texel" bar is not measurable at these distances.** One card
texel is **0.43 px** at `ring` and **1.45 px** at `mid`, so on six of the eight
rows the bar is smaller than the bbox's own 1-px quantisation. It stays red and is
not moved; the number that can be quoted is the pixel offset above.

### 15.5 The measurement that DOES answer bungo's rule

The orthographic profile — one reference, unclipped by construction, source and
card at one world scale:

| tree | one card texel | source height px | card height px | **height diff** | source bottom fifth | card | **source AT CARD PITCH** | source top fifth | card | source AT CARD PITCH |
|---|---|---|---|---|---|---|---|---|---|---|
| 0003a28b | 5.46 px | 644 | 645 | **0.16 %** | 142 | **164** | **164** | 81 | 184 | 224 |
| 0004a074 | 5.46 px | 643 | 645 | **0.31 %** | 32 | **38** | **38** | 107 | 203 | 229 |
| 00038599 | 10.92 px | 647 | 655 | **1.24 %** | 84 | 87 | 98 | 31 | 33 | 55 |

This is the table lane CARDORTHO's was meant to be, with the clipping gone. The
heights are **real** now and agree to **1.24 % worst**. The trunk band carries the
whole story: the card is **exactly** the source measured at the card's own texel
pitch (164 = 164, 38 = 38) and 15 % / 19 % wider than the source measured at the
render's. The crown band is the same effect, and there the card is now NARROWER
than the source at card pitch, not wider (184 vs 224, 203 vs 229, 33 vs 55).

**Nothing in the 18–121 % remains laid at the bake's door.**

### 15.6 The pictures (CONSTITUTION 5), all opened

Six, all read back before this was written.

`cardwidth_transition_0003a28b.png`, `_0004a074.png`, `_00038599.png` — source
mesh | the card a reader draws at alpha test 128 | overlay, blue mesh under orange
card, one world scale. On TreeHero01 the orange covers the blue almost everywhere
and reaches the same extent in both axes; what shows blue is the mesh's sub-texel
twigs, which a 128-texel frame cannot hold at any threshold.

`cardwidth_coverage_*.png` — the three-column texel pictures of §9, made before
the build on the OLD sheets, and still the clearest statement of the defect.

Picture defects, stated: `_0004a074.png`'s second caption line is blank (its
front/ring row was one of the four refused, so the line had nothing to quote), and
on `_00038599.png` the panel captions are clipped to the narrow cell's width. Both
are `transition2.py`'s, both cosmetic, both in §16.

### 15.7 What was NOT done

* **Nothing was committed** (CONSTITUTION 8).
* F1's bar was not moved, and neither were the transition's.
* No harness this change does not reach was run.
* The `_fs` crossed card, `lodgenWriteDds` and `lodgenDilateFrames` are untouched.
* `docs/LODGEN_IMPOSTOR_SPEC.md` still states the superseded rule
  (`CHANGE_NEEDED.md`).

---

## 16. Mistakes from the build round (also in `MISTAKES.md`)

1. **A patch script asserted two anchors and wrote after both, so a failed second
   assert silently discarded the first replacement.** The extent-floor gate line
   in `transition2.py` was never actually changed; the docstring edit that failed
   beside it took the code edit down with it, and the harness ran to completion
   printing the OLD gate. Found by grepping for the string after reading the
   output. Rule: **assert EVERY anchor before the first replace**, or write after
   each one — never `assert; replace; assert; replace; write`.
2. **`run.sh` read a file that a later step produces.** Steps 5 and 6 opened the
   card `.lodm`, which exists only once `lodgen card` has run — and the thing that
   runs it is the sample-set step, which came after. The transition died with
   `FileNotFoundError` and the run had to be finished by hand. The script is
   reordered. Rule: a chain step names the artefact it consumes and the step that
   produces it, and the order is checked against that, not against the reading
   order of the report.
3. **A coverage floor blunt enough to refuse the case it exists for.**
   `transition2.py` refuses a source mask under 200 px; at the `ring` distance a
   small tree legitimately photographs 169 px, so four of the twelve pre-registered
   rows never ran. A floor for "is this render usable" must scale with what the row
   is about, not be a constant.
4. **A gate bar smaller than its instrument's quantisation.** "Within one card
   texel" is 0.43 px at the ring distance and the bbox it is measured from is
   integer. Carried from the brief without asking what a texel is worth in the
   units the comparison is made in — the CARDFIT3 lesson again, and the second time
   in this one lane (F6 was the first).

---

## 17. Owed after the build

* **F1's bar, and whose it is.** Either the predictor gains the crop's integer
  rounding (and the bar can be 1.0), or the bar becomes 2.0 like its neighbour. A
  decision, not a measurement — and F1b is the invariant that guards the contract
  meanwhile.
* **The transition gate needs bars in units it can measure**: `max(1 texel, 1 px)`
  on the centre, a distance ladder that keeps a card texel above ~1.5 px, and a
  source floor that scales.
* Everything in §13 the build did not touch: the spec doc, the mip chain under the
  contract, and reading FO4CS's own card shader.
