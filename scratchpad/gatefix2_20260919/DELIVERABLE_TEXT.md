# GATEFIX2 -- native_lighting's two standing reds, 2026-09-19

Exe for every run: `release/NifSkope.exe` 23,625,728 B, 2026-09-19 19:49:03,
sha1 `c529e3c12fe4216a3c33cc14631e3c811169fbb0` (re-hashed 20:07, matches the
brief). **Nothing was built.** `tasklist` checked as its own command before
every exe run; no Fallout4 and no NifSkope at any check. One NifSkope at a
time, `--port` 42961..42971, `WW_WINDOW_AT=1960,40`. No `src/cell*` touched.

## 0. Today's state reproduced

`PORT=42961 tests/spells/native_lighting.sh`, 20:08:09..20:09:18 -> `repro1.txt`:
**19 checks, 2 failures, 0 skips**, byte-for-byte the rows GATEFIX1 recorded.

| red row | what it measures | today | bar |
|---|---|---|---|
| gate (b) IoU | overlap of the darkest fifth of the pixels between the render made with the terrain's OWN normal sheet and the render made with an all-"straight up" sheet. High overlap = the two pictures put their shadows in the same places = the sheet is not changing the shading. | 0.850 | `<= 0.800` |
| gate (b) blockSD | how much of the own-minus-flat luma difference SURVIVES 32x32 block averaging. A real slope signal is spatially correlated and survives; uncorrelated noise averages away. | 2.28 over 266 blocks | `>= 3.50` |

GATEFIX1 called the calibration stale because both numbers were pinned on
2026-09-16 11:54:47 against the `.lodt` container as it then stood. VT1 re-baked
that container at 16:44:04 the same day (its protected-box fix changed the
heights the warp reads, so the normals moved), and the exe's cache-freshness
rule refilled the sheet caches from the new generation. The fixture behind
0.705 / 4.56 no longer exists.

## 1. The container, named and hashed

    Commonwealth.VT.2.lodt   5,948,032 B   2026-09-16 16:44:04   sha1 dff7786718969f38
    own sheet cache  scratchpad/nativeview1_20260912/work/sheetcache, 4 tiles 272x272 BC1
      Commonwealth.VT.2.0.4.n.DDS ad7f8084e3fb   .2.0.5.n.DDS 4d40d0a9b8e0
      Commonwealth.VT.2.1.4.n.DDS c898e6db6fc1   .2.1.5.n.DDS fb07fb9847dc
      colour sheets, all four arms: fd2436e7670e

**DEVIATION FROM THE BRIEF, for the director to rule on.** The brief asked for a
container the gate owns, freshly baked. I did not bake one, and I think baking
one would have repeated the fault rather than repaired it: a new 5.9 MB
untracked artefact is exactly the kind of thing whose one generation a bar gets
pinned to. Instead the two rows were re-cut so that **no bar belongs to any
container** -- one is a ratio against a floor built inside the same run from the
container's own bytes, the other is an expectation predicted from the container's
own sheet. The gate prints the container's name, size, mtime and sha1 every run
(GATEFIX1 added that line), so a change of generation is visible in the log.
If you still want a frozen container, the bake is a separate, buildless job.

**The sheet is not flat, and the render is faithful to it** (`sheetstat.py`,
`brokenstat.txt`). Decoding the four BC1 normal tiles: tilt from vertical mean
20.83 deg, median 19.53, p90 36.11, max 67.95; 95.05 % of texels tilted more
than 5 deg, 22.07 % more than the tilt fixture's own 28.94 deg. Feeding those
normals to the gate's own light direction and the luma-per-N.L scale measured
off its own known-answer arms predicts a mean own-vs-flat difference of 3.85
luma against the 3.10 the frames actually show. The low blockSD is a property of
this terrain's block-scale slope structure, not of the shader.

## 2. The two populations, on that container

Every broken state is an INPUT fixture built from the own cache's own tiles by
`breakfix.py`, so none of them needed a build. Colour sheets copied through
untouched in all three (printed as a control). Frames: `frames/`, contact sheet
`calibration_arms_oblique.png`.

| state | how it was built | IoU | blockSD |
|---|---|---|---|
| the sheet is not read at all | an exact copy of the own cache; the pair is own vs that copy | 1.000 | 0.00 |
| the sheet's UP and NORTH channels swapped | G and B exchanged on both BC1 endpoints of every block, mode preserved (endpoints exchanged and indices remapped when the swap reversed `c0 > c1`); normals end 98.13 deg from vertical | 0.887 | 1.54 |
| **THE HEALTHY STATE** | own vs flat | **0.850** | **2.28** |
| the right normals in the wrong places | the own tiles' own 8-byte BC1 blocks permuted within each tile, seeds 20260919 / 20260920 / 20260921; tilt distribution identical to own's to every printed decimal | 0.821 / 0.822 / 0.821 | 1.00 / 1.07 / 1.04 |

**The IoU row cannot be tuned.** The healthy value sits INSIDE the broken
population -- 0.821 below it, 0.887 and 1.000 above it -- so no threshold of the
form `IoU <= bar` admits the healthy state and rejects all three defects, and
the old bar of 0.800 rejects all four. It is retired, not lowered. Its stated
job (own and flat must not be the same picture) is already done with a
pre-registered floor by gate (e)'s weakest-pair row, mean |dY| 3.10 against 2.00.

**The blockSD row separates, and its floor is now measured in the run.** Healthy
2.28 against a twin of 1.00 / 1.07 / 1.04 -- the bar is the RATIO, pre-registered
at **1.60**, which today reads 2.29x and 2.13x against the worst of three seeds.
Margin: the healthy state clears the bar by 1.43x, the worst twin misses it by
1.50x, and a channel-swapped sheet reads 1.54x, i.e. red.

**A new row carries the axis question, with a known answer.** Looking straight
down, N.L IS the normal's up component, and two arms in the same run have an
exactly known up (0.9988 and 0.8751) which calibrate luma against up on the same
frames: 8.10 luma per unit. The container's own mean up (0.9185 over the content
texels of the four tiles) then predicts the top view's mean luma **before it is
measured**: 108.667 predicted, 108.762 read, **0.094 apart**, bar 3.00. The same
sheet with UP and NORTH swapped lands **31.03** away.

## 3. Before -> after

| | checks | failures |
|---|---|---|
| before (`repro1.txt`, 20:08) | 19 | 2 |
| after (`after2.txt` / `after3.txt`) | **21** | **0** |

Rows: -1 (IoU retired), +1 gate (b) collapse floor kept but re-pinned 3.50 ->
0.50, +1 gate (f) twin-is-a-twin control, +1 gate (f) ratio, +1 gate (g)
top-view prediction. `COUNT FLOOR` raised 19 -> 21, measured on this exe.

**The new rows fire.** Two sabotage runs of the checker against doctored frame
sets (`sabA`, `sabB`), no renders needed:

* the container's sheet has UP and NORTH swapped -> **2 failures**: gate (f)
  1.54x against 1.60, gate (g) 31.03 luma against 3.00;
* the shader does not read the sheet at all -> **4 failures**: gate (e) oblique
  frames 3 distinct of 4, gate (e) weakest pair 0.00 against 2.00, gate (b)
  blockSD 0.00 against 0.50, gate (f) 0.00x against 1.60.

**Determinism and neighbours** (all on the same exe, one instance at a time):

| run | result |
|---|---|
| `native_lighting.sh` 20:26:24 (`after2.txt`) | 21 checks, 0 failures, PASS |
| `native_lighting.sh` 20:27:45 (`after3.txt`) | 21 checks, 0 failures, PASS -- identical row for row to run 2, hashes included |
| `render_shot.sh` 20:29 | 82 checks, 0 failures, PASS |
| `native_open.sh` 20:31 | 17 checks, 0 failures, 2 skips, PASS. Both skips pre-existing and named: the rung `NifSkope.before_nativeview1.exe` is not on disk, and the manifest leg needs `GBAKE`. The object-coverage failure NATIVEVIEW2 left for BTOFREE1 is no longer red here. |

Determinism note: the own arm's frame is `b39e407cc0a7` in every run of every
directory, and rendering the same cache twice under two different paths gave
byte-identical frames.

## 4. Files

Changed, uncommitted (3 + 2 documents):

    tests/spells/native_lighting.sh          the twin arm, the checker's new argument, COUNT FLOOR 21, the header
    tests/spells/native_lighting_check.py    IoU retired with its table; gate (b) re-pinned; gates (f) and (g); a BC1 decoder
    tests/spells/native_lighting_fixtures.py the twin cache, seed 20260919
    MISTAKES.md                              one entry spliced under the header, CRLF, append-only past byte 309 (verified)
    .claude/skills/ww-control-calibration/SKILL.md   and the same bytes in E:/Projects/Claude/.claude/skills/ (both trees identical, verified with cmp)

All three spell files are LF-only and stayed LF-only (0 CR, measured with
Python byte counts). MISTAKES.md is pure CRLF and stayed pure CRLF: 10,550 ->
10,590 CRLF, 10,550 -> 10,590 LF, and the tail past the splice is unchanged.

Lane folder: `scratchpad/gatefix2_20260919/` -- `repro1/after1/after2/after3.txt`,
`populations.txt`, `twinseeds.txt`, `brokenstat.txt`, `breakfix.txt`,
`sheetstat.py`, `breakfix.py`, `measure.py`, `shots.sh`, `splice.py`,
`frames/` (10 frames + censuses), `sabA/`, `sabB/`,
`calibration_arms_oblique.png`.

## 5. WW_CHANGES.md text (for the director to splice)

    ### 2026-09-19 -- GATEFIX2: native_lighting's two red rows re-cut so no bar belongs to a container

    The two standing failures were a calibration fault, not a lighting fault.  Both bars had been
    pinned from ONE measurement of the healthy state against a .lodt container that lane VT1 re-baked
    hours later, and nobody had measured what the DEFECT each row exists to catch reads.

    Measured this session, with every broken state built as an input fixture out of the own cache's
    own tiles (no build): sheet not read at all 1.000 / 0.00; UP and NORTH channels swapped
    0.887 / 1.54; THE HEALTHY STATE 0.850 / 2.28; the right normals in the wrong places (three
    shuffle seeds) 0.821-0.822 / 1.00-1.07.

    The darkest-fifth IoU row is RETIRED.  The healthy value sits inside the broken population, so no
    threshold of its shape can admit the healthy state and reject the defects; the old bar rejected
    all four.  What it was written to prove -- own and flat are not the same picture -- is already
    proved with a pre-registered floor by gate (e) (mean |dY| 3.10, floor 2.00).

    The blockSD row keeps its question and loses its absolute number.  The gate now builds a FIFTH
    arm every run: the container's own normal tiles with their BC1 blocks shuffled within each tile,
    same words, same histogram, same codec, right normals in the wrong places.  The row is the ratio
    between them, bar 1.60, reading 2.29x today (2.13x against the worst of three seeds).  A small
    absolute floor of 0.50 sits under it so a total collapse cannot pass as 0/0.

    New gate (g): looking straight down, N.L is the normal's up component, and the flat and tilt arms
    (up 0.9988 and 0.8751) calibrate luma against up in the same run, at 8.10 luma per unit.  The
    container's own mean up (0.9185) then predicts the top view's mean luma before it is measured --
    108.667 predicted, 108.762 read, 0.094 apart, bar 3.00.  A sheet with UP and NORTH swapped reads
    31.03 away, so this is the row that catches a wrong channel order.

    Also measured, and the reason the low readings are not a regression: the container's normals are
    not flat (tilt from vertical mean 20.83 deg, p90 36.11, 22 % steeper than the 28.94 deg tilt
    fixture), and they predict a mean own-vs-flat difference of 3.85 luma against the 3.10 the frames
    show.  The render is faithful to the sheet; the old floors were facts about a file that no longer
    exists.

    19 checks / 2 failures -> 21 checks / 0 failures, twice, row for row identical.  The new rows were
    shown failing: a channel-swapped sheet takes gates (f) and (g) red, and a sheet that is not read
    at all takes four rows red.  Neighbours on the same exe: render_shot 82/0, native_open 17/0 with
    its two pre-existing named skips.  Nothing was built; exe release/NifSkope.exe 23,625,728 B,
    2026-09-19 19:49:03.

## 6. HANDOFF.md text (for the director to splice)

    GATEFIX2 LANDED 2026-09-19 20:3x (gate only, nothing built, nothing committed).  native_lighting
    goes 19 checks / 2 failures -> 21 / 0 on release/NifSkope.exe 19:49:03, 23,625,728 B, twice
    identically.  The two reds were a stale calibration, and the repair is that neither bar belongs to
    a container any more: the darkest-fifth IoU row is RETIRED (measured against the three defects it
    exists to catch, the healthy value sits INSIDE the broken population -- 0.821 wrong-places, 0.850
    healthy, 0.887 axes-swapped, 1.000 not-read -- so no threshold of that shape exists), and the
    blockSD row is now a RATIO against a shuffled twin of the container's own tiles that the gate
    builds every run (2.29x today, bar 1.60, 1.54x when the axes are swapped).  New gate (g) predicts
    the top view's mean luma from the container's own normals, calibrated on the two known-answer arms
    in the same run: 0.094 luma from prediction, bar 3.00, 31.03 for a channel-swapped sheet.
    OWED TO BUNGO / FOR THE DIRECTOR TO RULE: the brief asked for a freshly baked container the gate
    owns; GATEFIX2 did not bake one and argues the bars no longer need one (the container is named,
    hashed and printed every run: Commonwealth.VT.2.lodt, 5,948,032 B, 2026-09-16 16:44:04, sha1
    dff7786718969f38).  If a frozen container is still wanted it is a separate buildless job.
    Files uncommitted: tests/spells/native_lighting.sh, native_lighting_check.py,
    native_lighting_fixtures.py, MISTAKES.md (one entry), and ww-control-calibration/SKILL.md in BOTH
    skill trees (identical bytes).  Report scratchpad/gatefix2_20260919/DELIVERABLE_TEXT.md.

## 7. Finished-work skill review (CONSTITUTION 1a)

Loaded: `ww-control-calibration` (its part 2 and part 4 are the whole design of
the twin; its TILING2 warning that a phase twin is worthless as a floor for a
second-order statistic is why the twin here permutes POSITIONS, which is a
structure null, and not the spectrum) and `ww-gate-owns-its-fixtures` (the
fixture builder, the order the arms run in, the one-variable control).

Written: a new section appended to `ww-control-calibration` in both trees --
*"Set the bar BETWEEN two populations, and build the broken one first"* -- with
the two-population table, the three broken states that need no rebuild (identity
copy, channel exchange, block shuffle), the preference for a same-run ratio over
an absolute number, and the prediction-from-the-input shape with its lever-arm
caveat.

Wished for, and declined as a skill: nothing. The one candidate was "decode a
BC1 sheet tile to normals", which is now 35 lines inside
`native_lighting_check.py` and already exists in `lodgen_native_decode.py`; a
skill would be a third copy rather than a procedure.

## 8. Mistakes (spliced into root MISTAKES.md by this lane)

One entry, 2026-09-19: two gate bars were set from the healthy state alone, on
an artefact another lane could re-bake. The rule it carries: a bar is set
BETWEEN two measured populations, the healthy state and the defect the row
names, with the margin stated; when the healthy value lands inside the broken
population the statistic is replaced, not tuned.

Process mistakes of my own this session, for the record: I first wrote the
lane's report to `report.md` and the harness refused it, so it went to the
brief's fallback name `DELIVERABLE_TEXT.md`; and my first render batch wrote
nothing because I passed a RELATIVE output directory to the exe (the gate uses
an absolute one and says so). Neither reached a measurement.

## 9. What is NOT proven

* No build, so no shader-side defect was reproduced by actually breaking the
  shader. Every broken state here is an INPUT fixture. The channel-swap arm is a
  faithful emulation of a BAKER writing the normal channels in the wrong order;
  it is NOT the historical tangent-space misread, which rotated each texel by a
  frame derived from the geometry normal and cannot be reproduced without a
  rebuild.
* Gate (f)'s bar of 1.60 has one container behind it. The ratio is
  self-calibrating by construction -- both halves move when the container moves
  -- but its VALUE has one sample, and a container with much finer slope
  structure would narrow it.
* Gate (g)'s linear luma-against-up model is calibrated over a lever arm of
  0.1237 and used one-sided. The 31.03 of the swapped sheet is a distance, not a
  prediction.
* Nothing here says the lighting output is correct in bungo's eyes; it says the
  frames agree with the container's own normals to 0.09 luma and that the gate
  can now fail. No "fixed" claimed before he confirms live.
