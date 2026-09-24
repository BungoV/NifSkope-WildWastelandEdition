# Lane WATER3 — marking water direction in NifSkope

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main` at `720762a`.
**Nothing committed** (CONSTITUTION 8). **BUILD PENDING** — `Fallout4.exe` was
UP at the one gate check, so nothing was compiled and no existing `src/` file
was touched. Resume: `scratchpad/water3_20260910/PENDING.md`.

Read first: `CONSTITUTION.md`, the `HANDOFF.md` top block,
`scratchpad/specs_20260909/spec_water.md`, `scratchpad/lane_water2_report.md`,
`scratchpad/lane_lodt_open_report.md`. Skills loaded: `ww-spec-gate-audit`,
`ww-contract-provenance`, `nifskope-ww-panel-style`, `nifskope-ww-resume-pending`.

---

## 0. The headline

1. **The spec is current.** Every number lane WATER2 corrected is in it, the
   reversed stride refusal is fixed, G5 and G6 are restated with the numbers
   that actually hold, and every paragraph now says whether it is BUILT or
   DESIGN. The provenance footer was rebuilt from scratch — the old one was
   written against `src/lodtfile.cpp` at 1,706 lines and the file is now 3,638,
   so all of it was wrong — and its 38 line numbers were re-derived from their
   anchors by a script, 38 moved, 0 missing, 0 ambiguous.
2. **The pre-registered gate numbers did NOT survive the audit, and the audit
   ran before a line was written.** The spec's harness marks "body 233, the
   Charles, 25,112 texels" and refutes on "body 136". Under the rule WATER2
   shipped, ids are assigned by descending area, so **the Charles is body 3**
   (25,114 texels, the same cells) and the marsh that refutes it is **body 2**
   (29,312). Measured through the INDEPENDENT decoder, never the writer.
3. **The tool is written and compiles alone** — 3,533 lines in four NEW
   files, `g++ -fsyntax-only` rc=0 with the real `Makefile.Release` flags. It is
   not built, not run, and therefore **nothing in it is proven**.
4. **One gate WAS measured, in Python, before any of it was compiled**, because
   the whole design rests on it: the flow plane really is a pure function of the
   body-ID plane and the body table — **0 mismatches over 37,748,736 texels**,
   with the floor firing at 25,114 (`scratchpad/water3_20260910/repro_flow.py`).
   That is what makes "undo is byte-identical" a property rather than a hope.

---

## 1. The spec, corrected (task 1)

`scratchpad/specs_20260909/spec_water.md`, 557 → 735 lines. Each edit is a
script in `scratchpad/water3_20260910/` (`spec_fix01.py` … `spec_fix07.py`,
`anchors.py`), each refusing unless its anchor matches exactly once.

| section | was | now |
|---|---|---|
| banner | "SPEC / NOT YET WRITTEN" | a STATUS block: what WATER2 built, what WATER3 built, what is still design with no code (§6, the FO4CS consumer), and the three corrections by measurement |
| §1.2 | 16 forms serve **590** bodies; `ExtLakeWater` 16 lakes, `ExtOceanWater` 405 | **346** bodies, 15 and 176 — and the note that the per-form TEXEL totals did not move, which is what says this is a grouping difference |
| §1.3 | "589 of 590 carry exactly one height" | under rule D a body carries one height BY CONSTRUCTION; the fact is now a property of the classifier |
| §1.6 | 93 bodies ≥ 64 texels: 26 drain / 1 bed / 12 zero / **54 need a stroke** | **89** bodies (1 sea, 42 river, 46 lake): **29** drain, **1** bed, **12** none (the sea and outlet-less lakes, which is bungo's rule and not a gap), **47 served only by the form's `NAM0`** — those 47 are what a stroke is for |
| §2 | 804 → 781 → 792 → 590, 215 accepted / 3 refused | the two defects WATER2 measured (the decimated shore test, one-sided; the merge with no DIRECTION), the SIZE guard, and 805 → 793 → **346**, **528** accepted / **13** refused; the Charles is **body 3**, 25,114 texels |
| §2 | "288 of the 590 are noise" | **115 of the 346** |
| §3.3 | 590 bodies = 28,320 bytes | **346 = 16,608 bytes**, at 0x2249AE6 |
| §3.8 | refuse when `bodyRecordBytes` is **larger** than the reader's record | **SHORTER** — the page had it backwards and would have made the stride field useless; §3.3 always stated it correctly and the reader implements §3.3 |
| §4.1 step 6 | ids by descending area | marked BUILT as stated: id 1 is the 21,575,619-texel sea, id 2 the largest marsh, id 3 the Charles |
| §5.1, §5.2 | the 3-D water plane is the canvas | as built it is a TOP-DOWN MAP in the dock, with both reasons; the Blender table gains a `built?` column and the two kinds that are NOT built (Barrier, Merge) with the reason |
| §5.3 | a Water section in the LOD Generation panel | its own dock; the row table as built; the four rows dropped, with the reason |
| §5.4 | `WW_WATER_TEST`, body 233, refuter 136 | `WW_WATER_MARK_TEST`, body 3, refuter 2, plus the three cases the spec did not have |
| §7 G5 | 590 / 93 / 215 / 3 | RESTATED and met as restated: 346 / 89 / 528 / 13, with the invariant that crossed the rule change unmoved (13 of 15 per-form texel totals) |
| §7 G6 | 4 bodies, the river with 16 surfaces | RESTATED: 19 bodies (1 sea, 16 river steps, 1 lake, 1 puddle) — a body carries ONE plane by construction, so the stepped river IS sixteen bodies. The geometry was not changed |
| §7 P1-P6 | six gates | eight, with an `as built` column; P0 identity and P7/P8 added, P4 named as shipped-but-ungated |
| footer | 4 files, 13 claims, written against lodtfile.cpp at 1,706 lines | 7 files, 38 claims, every number re-derived from its anchor |

### 1.1 The audit that produced the body ids (`ww-spec-gate-audit`)

`scratchpad/water3_20260910/audit_spec.py`, through
`scratchpad/water2_20260909/lodl_v3_authority.py` — the decoder that shares no
code with the writer. The three checks the skill asks for:

* **read the source that produced the number** — the spec's 233/136 came from
  WATER1's 590-body census, and WATER2's shipped rule assigns ids by descending
  area over 346 bodies. The ids could not have survived, and reproducing them
  would have meant marking whatever body happened to be number 233 today;
* **is the approximation one-sided** — not applicable to an id, but the areas
  are the evidence that these are the SAME bodies: the Charles 25,112 → 25,114
  and the marsh 29,305 → 29,312, both grown by the handful of texels the exact
  shore test picks up;
* **print the table, not the count** — `census_v3.txt` carries it.

The harness does not hard-code 3 and 2 in the end. It picks **the largest river**
and **the largest other non-sea body**, so it runs on any worldspace; on the
Commonwealth those resolve to 3 and 2, and the log prints which it chose.

---

## 2. The tool, as built (task 2)

Four NEW files. Not one existing `src/` file was touched, because lane BUILD4
was compiling `src/lodgen.cpp` and `src/nifskope_ui.cpp`.

| file | lines | what |
|---|---|---|
| `src/watermark.h` | 281 | `WaterStroke` (the wire record of spec §3.7, kinds 0-6), `WaterMarkSolve`, `WaterMarkDoc` |
| `src/watermark.cpp` | 2,004 | the stroke-store codec, the body-table encoder, the plane container reader and packer, the constrained harmonic fill, the save, and the headless half of the harness |
| `src/watermarkpanel.h` | 28 | one entry point, `waterMarkInstall( QMainWindow * )` |
| `src/watermarkpanel.cpp` | 1,220 | the dock, the canvas, the rows, the summary, and the dock half of the harness |
| `tests/spells/water_mark.sh` | 104 | both halves, with floors |

### 2.1 The strokes are the source; the planes are derived

Nothing in the tool edits a plane. `solve()` builds a field per marked body and
`save()` re-derives the whole flow plane from (body-ID plane + body table +
those fields) — the same function the writer computes. The body-ID and shore
planes are copied through **verbatim, with their absolute offsets rebased**,
because nothing a marking tool does can move a body's SHAPE.

Three consequences, and each is a gate rather than a claim:

* a document nobody has marked re-derives to the writer's own bytes;
* removing a stroke and saving reproduces the file it started from, byte for
  byte — there is nothing left to remember;
* a stroke laid at one sample rate survives a re-derivation at another, because
  the points are WORLD coordinates.

### 2.2 The propagation (spec §4.3), and one stated departure

Dirichlet where a stroke passes, Neumann everywhere the mask ends — and the
Neumann half is not a choice, it falls out of averaging only the neighbours
that are IN the body, which is what "tangent to the banks" means in practice.
Averaging is on the VECTOR, never the angle. Solved by **red-black SOR**, not
Jacobi: Jacobi needs O(n²) sweeps on a domain 352 texels across and the panel
re-solves on every stroke. The iteration count and the last residual are
reported, because a solver that stopped at its cap has not converged.

**The departure.** The spec asks for confidence as *"a second harmonic fill with
15 at the constraints and 0 nowhere"*. That converges to 15 EVERYWHERE — a
harmonic function with one Dirichlet value and no other boundary condition is
that constant. What the spec WANTS is the sentence beside it: it decays with
distance so a consumer fades to the body's mean where nobody marked. Built as a
geodesic distance inside the mask, halving every stroke width, which is also
what makes it fade where a body WIDENS.

### 2.3 bungo's three sentences, and where each one lives

| his words | where |
|---|---|
| *"in nifskope, have the player mark the water direction in a smart way"* | the dock's map: drag a stroke down the water and the whole reach takes its direction |
| *"lakes have no flow if they're not connected to rivers"* | the **Still water** tick, stored as a `ZeroFlow` MARK (kind 6) and not a bit in the table, so it survives a re-derivation; it beats the form's `NAM0`, which is the only reason a still lake had a velocity at all |
| *"then rivers end up at sea"* | a gate, not a feature: the harness finds the mouth IN THE FILE (the river texel nearest a texel of the body it drains into), orients the stroke to end there, and asserts the flow plane's MEAN direction over the whole body points at it |
| *"different water colors for different bodies of water" / "or at least an ID for them"* | the **Colour** row writes the body record's RGBA (unticking clears it, A = 0); the map paints Body ID as a hashed hue, which answers the second half on sight |
| *"a source pin + outlet pin = a path"* | kinds 4 and 5: two one-point marks that only mean anything as a pair, becoming one constraint segment the harmonic fill then bends to the banks |

### 2.4 Divergences from Blender, each stated

| Blender | here | why |
|---|---|---|
| grease pencil draws in the 3-D viewport | a top-down map inside the dock | the 3-D viewport is `src/glview.cpp`, another lane's file — and a river reach is eleven cells long, so its direction is a fact about the MAP; Blender itself moves to a 2-D editor (UV, Image) whenever the thing edited is flat. The 3-D viewer still SHOWS the result, by reading the planes back |
| stroke thickness from tablet pressure | the Width row is the only source | reproducible from the file alone |
| the eraser has a radius | Erase removes the whole stroke under the cursor | a stroke is ONE constraint; half a constraint is not a smaller constraint |
| middle-drag pans, the wheel zooms about the cursor | the same | none — and the wheel over a number field still does nothing unless it has focus, which is also Blender's rule |
| a GP object holds the strokes | the `.lodl`'s stroke store holds them, in world units | the file is the document |

### 2.5 The panel (`nifskope-ww-panel-style`)

Its own dock, in the Workspaces dropdown, hiding the other manager docks the way
they hide each other. `wwHeading` for every section, `wwMakeScrubField` on both
numbers, `wwMatchFieldStyle` on all five selectors and the two line edits, one
`label | field` grid a section with one `labelW` (132) for the page, one setting
a row, whole-word labels with the explanation in the tooltip, `wwSkinColor` for
every colour (`textMuted` for the summary, `danger` for a refusal), three bands
with the settings scrolling and the map, the summary and the buttons pinned.

Rows: File, Show / Tool, Speed, Width / Class, Water form, Colour, Flow (Still
water), Name / and a folding Bake section with Flow samples per cell.
**Four rows in the spec's list were dropped** — Body IDs, Flow, Shore distance
and Bridge gap are the WRITER's switches, and a row that could not do what it
said would be worse than no row.

The **Water form** list is the forms the FILE interned plus the worldspace
default: zero-authoring, no list of water types is written down anywhere in this
tree.

---

## 3. The gates (task 3)

**NONE OF THEM HAS RUN.** They need a build and the game was up. What each is
and what settles it:

| gate | state | settled by |
|---|---|---|
| P0 identity (added) | **PRE-MEASURED in Python, PASS** | `repro_flow.py`: rebuilding every flow word from the body table alone gives 0 mismatches on 37,748,736 texels; floor 25,114. The C++ twin's own comparison is the first two checks of the model half |
| P1 isolation + floor | WRITTEN, pending | `water_mark.sh`, model half. A WHOLE-PLANE sweep, not the marked body's bbox — "only the bbox could have moved" is the claim under test |
| P2 the refuter fires | WRITTEN, pending | the neighbour is marked FIRST and the river's changed count printed as 0, so the isolation check is seen to be able to fail |
| P3 undo | WRITTEN, pending | strengthened from "the flow plane" to the WHOLE FILE, byte for byte |
| P4 survives a re-bake | **mechanism shipped, gate NOT written** | `setFlowRate` re-derives at 8 or 16 from the same world strokes; the angle comparison needs a run to set an honest tolerance. PENDING step 6 |
| P5 panel style | WRITTEN, pending | the dock half, floors: ≥ 2 scrub fields and 0 plain, 0 group boxes against ≥ 4 headings, ≥ 5 selectors and 0 unmatched, ≥ 2 check boxes with 0 dashes and 0 untipped, 6 settings on 6 distinct rows by GEOMETRY, the three bands, the fold |
| P6 the picture | half written | `SHOT=` grabs the dock; the flow before/after pair is PENDING step 5, and the "before" already exists at the exact framing (`water2_20260909/images/charles_flow.png`) |
| P7 dry land (added) | WRITTEN, pending | both halves: refused in words, stored nowhere |
| P8 round trip (added) | WRITTEN, pending | save, reopen, save is byte-identical, and the strokes come back OUT OF THE FILE |
| `lodl_water.sh` 56/0 | pending | PENDING step 4 |

---

## 4. What is NOT done

* **Nothing is built and nothing has run.** Four files, one `.pro` and one
  harness are on disk; `g++ -fsyntax-only` is the only thing that has read them.
* **The picture pair.** PENDING step 5.
* **Barrier and Merge (stroke kinds 2 and 3) do nothing.** The store carries
  them and the solve ignores them: splitting or joining a body needs the
  CLASSIFIER re-run, which needs the plugin and the heightfield, not just the
  `.lodl`. Named in the spec's §5.2 table.
* **The packer is a TWIN of `lodtPackPlane`, not a call.** The file rule made it
  so, and the identity gate is what makes a twin safe. Retiring it into a shared
  header — the "what is shared lives in the shared code" rule — is owed, and it
  is a `src/lodtfile.cpp` lane.
* **The 3-D viewport cannot be marked on.** A `glview.cpp` lane; the model takes
  world coordinates and knows nothing about either canvas.
* **The writer still does not read the stroke store.** A full re-bake from the
  ESM (`lodgen --water-bodies`) writes an EMPTY store and would discard a user's
  marks. That is the sharpest owed item in this area, and it belongs to
  `src/lodtfile.cpp`.
* **`EsmWorld` still has no `WATR` accessor** (WATER2's
  `ESMDATA_CHANGE_NEEDED.md`), untouched by this lane.

---

## 5. Mistakes

Also in `scratchpad/water3_20260910/MISTAKES_ENTRIES.md` for splicing.

1. **A placeholder check sat in a harness where a measurement belonged.** The
   first draft of `lodtWaterMarkSelfTest` had the body-table identity gate
   written as `check( "the body table re-encodes to the writer's bytes",
   doc.strokes().isEmpty() )` — a condition that is true of any freshly opened
   file and cannot fail on its input, which is the exact thing CONSTITUTION
   rule 4 forbids. Found by re-reading the draft before the syntax pass, not by
   running it, and replaced with `tableRepackMatches()`. The rule that prevents
   it: when a check is stubbed because its accessor does not exist yet, WRITE
   THE ACCESSOR — a stub that returns a true-ish expression is
   indistinguishable from a passing gate for as long as nobody reads it.
2. **The report was written at the end, not incrementally.** CONSTITUTION 1
   says a lane writes each section as it finishes it, and 1b says a lane past
   half its window writes its report and its PENDING resume first. This lane
   wrote its scripts and code incrementally to disk but kept the report to the
   last step; a death at any point would have left the work without its
   explanation. Recorded as a process error against myself.

---

## 6. Skill review (CONSTITUTION rule 1a)

**Loaded and used.**

* `ww-spec-gate-audit` — the whole of §1.1. Its "do not reproduce the artefact
  to hit the gate" is why the harness selects the largest river rather than
  hard-coding an id, and its "print the TABLE, never only the count" is why the
  audit dumped the body table before deciding anything.
* `ww-contract-provenance` — the five steps, in order. Its step 3 script is
  `anchors.py` here (38 rows moved, 0 missing, 0 ambiguous), and its step 5
  caught what matters: `src/lodtfile.cpp`'s hash was identical at the start and
  the end of the lane, so no concurrent edit invalidated a claim.
* `nifskope-ww-panel-style` — every control, and the self-test counts with their
  floors. Its "an output with settings is a folding section" gave the Bake fold;
  its "the action bar says what will happen, or the one reason it cannot" gave
  `refreshSummary()` ownership of the buttons.
* `nifskope-ww-resume-pending` — the shape of `PENDING.md`: qmake before make,
  the dependency read back BY OBJECT NAME, the exe-newer sweep over every
  changed file, the sequential harness chain with the skipped ones named.
* `nifskope-ww-lodgen` (indirectly, through WATER2's report) — the heredoc trap
  cost this lane one refused command (`spec_fix05.py` would not parse as a
  heredoc), and the fix was that skill's own rule: write the patch as a FILE.

**Wished for and WRITTEN: none.** Two candidates were considered and both
declined, with reasons:

* *"syntax-only compile with the real flags"* — it is now
  `scratchpad/water3_20260910/syn.sh`, eight lines, and the procedure is one
  sentence: copy `DEFINES`, `CXXFLAGS` and `INCPATH` out of `Makefile.Release`
  and pass `-fsyntax-only`. It belongs as a paragraph in
  `nifskope-ww-build-verify` (which already owns "a green harness is not a
  build"), not as a skill of its own. **Recommended amendment**, stated here so
  the director can apply it to both trees: build-verify should name
  `-fsyntax-only` with the real flags as the thing a lane does when it may not
  build — it is the only gate a BUILD PENDING lane has.
* *"a twin of shared code is safe only behind a byte-identity gate"* — a real
  and reusable idea, but it is one paragraph and it already has a home:
  `ww-control-calibration`'s territory (a floor and a ceiling from the same
  data). Declining rather than minting a skill for a sentence.

---

## 7. Housekeeping

Nothing committed. Files NEW: `src/watermark.h`, `src/watermark.cpp`,
`src/watermarkpanel.h`, `src/watermarkpanel.cpp`, `tests/spells/water_mark.sh`,
`scratchpad/water3_20260910/`. Files CHANGED: `NifSkope.pro` (+90 bytes, four
paths), `scratchpad/specs_20260909/spec_water.md`. **No existing `src/` file was
touched.**

Line endings, measured with Python byte counts (not grep): every new `src/` file
CR=0; `NifSkope.pro` CR=0 before and after; `spec_water.md` CR=0 throughout.

To reproduce this lane's own measurements:

```
cd scratchpad/water3_20260910
python audit_spec.py            # the body ids, through the independent decoder
python repro_flow.py            # the pure-function gate, with its floor
python anchors.py --check       # every footer line number still points at its anchor
bash syn.sh ../../src/watermark.cpp ../../src/watermarkpanel.cpp
python hookup.py --check        # the five hook-up anchors, no writes
```

---

## Build (BUILD5/BUILD5b)

Lane BUILD5 applied the three hook-ups and built, then died on an API rate limit
with its harness red and its `PENDING.md` still saying nothing had been built.
Lane BUILD5b resumed from the tree, not from the note.

### The state BUILD5b found, settled by bytes and not by the resume file

| claim in `PENDING.md` | what was on disk |
|---|---|
| "nothing was built" | `release/NifSkope.exe` 02:26:06, newer than every source in `NifSkope.pro` |
| "no existing `src/` file was touched" | all three hook-ups applied -- `src/nifskope.cpp` 433,872 -> 434,370 bytes, exactly the +498 the dry run predicted; `src/nifcli.cpp` at its predicted 261,671 |
| the harness "has never executed" | it had, at 02:32, **5 failures** |

`hookup.py --check` re-run on resume says "ok" for both `nifskope.cpp` edits,
which reads as "not applied". Its anchors are the lines the new text goes AFTER,
so they keep matching once the edit is in. Recorded in `MISTAKES.md`.

### The five failures, and what each one was

| gate | measured | cause |
|---|---|---|
| P1 floor | 11,271 of 29,312 = 38.5%, floor 60 | the harness's stroke ran down the body's BOUNDING-BOX DIAGONAL and kept every point on ANY water: 13 of its 16 points were not on the river, and a bbox diagonal is within a degree of the body's own mean (55.5 against 54.84), so the stroke asked the plane for what it already said |
| P7 dry land | refused for the wrong reason | the control was placed at the worldspace corner, which on the Commonwealth is open SEA |
| P8 round trip | 38,613,382 -> 38,679,915 bytes | a cascade of P7: the accepted sea stroke made the sea's flow plane stop being uniform on the second save |
| P3 undo | 1,021,405 bytes differ | THE ONE REAL DEFECT -- `solve()` accumulated the body table's derived fields |
| dock P7 | "that stroke has no points" | the canvas filtered dry points out before the model saw them |

Four patch scripts, each refusing unless every anchor matches exactly once and
the CR count is unchanged: `gatefix.py`, `gatefix2.py`, `gatefix3.py`,
`gatefix4.py`, plus BUILD5's own `p4_gate.py` (gate P4, written and unapplied).
Three builds, `make` exit code gating each; `g++ -fsyntax-only` with the real
`Makefile.Release` flags before every one of them.

### The gates, on `release/NifSkope.exe` 2026-09-10 03:38:56

| gate | result | the number |
|---|---|---|
| P0 repack identity | PASS | body table and flow plane both re-encode to the writer's own bytes, 0 differ |
| P1 isolation | PASS | 0 texels outside the marked body changed |
| P1 floor | PASS | 25,110 of 25,114 = 100.0% of body 3's own (was 38.5% before the centreline) |
| P2 refuter, run FIRST | PASS | a stroke on the neighbour moves 25,110 of its own and 0 of the river's |
| P3 undo | PASS | **0 bytes differ** over 38,612,038 |
| P4 re-bake | PASS | marked at 32 samples a cell, re-written at 8: mean **112.59 -> 112.38, moved 0.21 degrees**, tolerance 5. The tolerance is BUILD5's pre-registered 5 and the measured move is 24x inside it; the floor beside it asserts the header really says 8, that 32 != 8, and that the body has samples at both rates (29,309 at 32, 1,819 at 8) |
| P5 panel style | PASS | 20 dock checks: 2 scrub fields / 0 plain, 0 group boxes / 4 headings, 5 selectors / 0 unmatched, 2 check boxes / 0 dashed / 0 untipped, 6 settings on 6 distinct rows, three bands, the fold |
| P6 the pictures | PASS | `images/charles_flow_pair.png` and `dock.png`, below |
| P7 dry land | PASS | refused in words, at a dry point found in the file at (-305152, -313344) |
| P8 round trip | PASS | save, reopen, save byte-identical, 38,619,353 both times |
| "rivers end up at sea" | PASS | the mouth is found IN THE FILE; the marked plane's mean points at it, cos = **1.000** on body 3 |
| `water_mark.sh` | **PASS** | 21 model checks + 20 dock checks, 0 failures |
| `lodl_water.sh` | PASS | RESULT PASS, unmoved |
| `lodl_open.sh` | PASS | 23 checks, 0 failures |
| `lodgen_terrain.sh` | PASS | 26 checks, 0 failures |
| `lodgen_identity.sh` | PASS | RESULT PASS |
| `render_shot.sh` | PASS | **82 checks, 0 failures**; section 7 (the pinned camera) all green -- the 512-unit cube spans 376.91 px against the 376.75 the projection predicts, at eye 500, 1000 and 2000, and the census `upp` agrees with the arithmetic to six figures |

### The pictures

`scratchpad/water3_20260910/images/charles_flow_pair.png` -- the Charles (body 3,
cells -16..-6 / -21..-4), the FLOW plane, ONE framing, two files. The framing is
lane WATER2's unchanged, and the proof is that this lane's "before" render is
**byte-identical** to `water2_20260909/images/charles_flow.png`: the only thing
that differs between the halves is the file. Measured through WATER2's
independent decoder (`flow_mean.py`), over all 25,114 samples of body 3:

* **before** -- mean 115.31 degrees, **1 distinct direction**, concentration
  R = 1.000. The drain rule found the body it flows into and painted that single
  vector over every texel;
* **after one stroke toward the mouth** -- mean 111.31 degrees, **99 distinct
  directions**, R = 0.807. The hue turns with the reach.

The mean barely moves and that is the point: the stroke did not re-aim the
river, it gave it a SHAPE. Every other body in the frame -- the lake, the sea,
the puddle -- is pixel-for-pixel unchanged, which is gate P1 in a picture.

`scratchpad/water3_20260910/dock.png` -- the dock as a person sees it. It obeys
the house style: four `wwHeading` sections and no group boxes, one setting a row
with one label column, scrub fields for both numbers, matched chrome on all five
selectors, tooltips instead of dashed labels, the Bake section folding, the three
bands with the map, the summary and the buttons pinned, and the refusal sentence
in words above the map. Two divergences, both deliberate and both stated: the
map's palette is a hash of the body id rather than the skin table (346 bodies
cannot come out of a twenty-entry palette), and dry land inside the map is drawn
at a literal (24,26,30) -- that one is a genuine miss and is listed as owed.

### What is still NOT done

Everything section 4 of this report lists, minus the picture pair and gate P4,
which are now delivered. Still open: Barrier and Merge strokes do nothing; the
writer does not read the stroke store, so `lodgen --water-bodies` still discards
a user's marks; the plane packer is a TWIN of `lodtPackPlane`; the 3-D viewport
cannot be marked on; `EsmWorld` has no `WATR` accessor; and the map's dry-land
literal should be a skin token.

**Nothing is committed** (CONSTITUTION 8). **His open NifSkope window needs a
restart** -- the exe under it is from before 03:38:56.

### Skill review (BUILD5b)

**Loaded and used.** `nifskope-ww-resume-pending` (the read order, qmake before
make, the dependency read-back BY OBJECT NAME -- which is how `watermark.o`,
`watermarkpanel.o` and `nifcli.o` were each checked against the header this lane
changed -- and the exe-newer sweep over every changed file rather than one).
`nifskope-ww-build-verify` (make's own exit code as the gate, the stylesheet
`cmp`, and its "a successful build is not a consistent one" section, which is
the object-versus-header check above). `nifskope-ww-panel-style` (the judgement
of `dock.png`, and the visibility count that was missing from it).
`nifskope-ww-render-shot` (the switches, one instance at a time, and the reason
a leftover instance had to be identified by its command line before being
killed). `ww-texel-picture` (the caption arithmetic: the first sheet's number
line ran past its cell and read as the neighbour's number; the fix asserts each
caption fits the panel it belongs to).

**A skill that should have existed, and now does not need to be invented twice.**
`WW_RENDER_SHOT` silently writes NOTHING when given a RELATIVE path: the grab
runs, `release/ww_camera_pin.log` records it, the process exits 0, and
`QImage::save` fails without a word. Two renders were lost to it. That belongs
as a line in `nifskope-ww-render-shot`'s switch table -- **recommended
amendment, for the director to apply to both skill trees**: *every `WW_*` output
path is ABSOLUTE; a relative one is saved relative to the process's working
directory and fails silently, and the tell is a `grab` line in
`release/ww_camera_pin.log` with no file on disk.*

**Declined, with the reason.** "Diagnose a red gate as harness or as tool" is
the whole of this lane's work and it does not compress into a procedure: each of
the five had to be read back to its own cause. The general rule it followed is
already CONSTITUTION 4.
