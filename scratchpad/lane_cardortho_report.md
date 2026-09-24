# Lane CARDORTHO — the bake's camera, and the transition in a one-reference scene

**STATE: BUILD PENDING.** All code, gates, documents and measurement scripts are
on disk and compile-checked. **Nothing was built and nothing was run**: the
brief's build rule puts lanes WATER2 and CLAMP first, and the single check —
made once, after the last line of code, never polled — found
`scratchpad/clamp_20260910/DONE` absent while `tasklist | grep -i -E
"Fallout4|NifSkope"; echo rc=$?` printed `rc=1`. CLAMP is alive in
`src/lodgen.cpp`, the same file this lane changed in a different function, so a
build now would link its half-written terrain bake.

The paste-able resume is `scratchpad/cardortho_20260910/PENDING.md`; the whole
post-build chain is one script, `scratchpad/cardortho_20260910/run.sh`.

Nothing was committed (CONSTITUTION 8).

---

## 1. The measurement that was already made, and what it means for the bake

Lane HOOKCAM measured this on 2026-09-09 and recorded it as a finding for this
lane rather than acting on it (`scratchpad/lane_hookcam_report.md` §1.3):

* `restoreUi()` has `const bool isPersp = true;` and calls
  `ogl->setProjection( isPersp )`. **The only other callers of `setProjection`
  are the View menu and Numpad-5.** Nothing in the render hook, and nothing in
  the impostor bake, ever called it;
* two identical cubes 1024 units apart in depth photograph **31 px and 27 px**
  in one frame — `4000 / 3488 = 1.147`, `27 × 1.147 = 31.0`. An orthographic
  camera draws them the same size;
* the unpinned control frames the 512-cube at **231 px**, and the perspective
  reading predicts 231.0 where the orthographic reading predicts 202.6.

So every headless run is a 60-degree perspective frustum, **the impostor bake
included**.

### 1.1 Why that is fatal for a card, in the bake's own arithmetic

Every geometric number the bake writes is a world length taken off VIEWPORT
PIXELS through a single units-per-pixel constant. Pass one, at
`src/nifskope_ui.cpp` (the silhouette box loop):

```
const float upp = 2.0f * halfH0 / float( H );      // units per pixel, both axes
```

and pass two crops `2*halfW × 2*halfH` world units out of a viewport fitted to
`fitH`, then records those extents in the sidecar and the `.lodm`. That is only
true under an orthographic projection. Under perspective a point `d` in front of
the card plane is magnified by `eye / (eye − d)`, so:

| what | under the perspective bake |
|---|---|
| the SCALE | over a tree's own depth the magnification varies by tens of per cent, so the recorded `half` describes no picture and a reader's quad cannot match the mesh it replaces |
| the SHAPE | the magnification varies ACROSS a frame, so each silhouette is foreshortened — wider at the frame's near edge than its far one — and the N² frames of one set disagree with each other |
| the HEIGHT sheet | the bake puts near and far symmetric about the bound centre and calls window z 0.5 the card plane. Exact under ortho; false under perspective, where the window z of the midpoint is not 0.5 |

### 1.2 Why no check caught it, and this is the part worth keeping

`GLView::orthographicHalfHeight()` is `inline float ... { return float( Dist / Zoom ); }`
— **it returns that expression whatever the projection is**. The bake already
had a read-back beside its fit:

```
const float got = skope->ogl->orthographicHalfHeight();
if ( std::fabs( got - fitH ) > 1.0e-3f * fitH && got > 0.0f )
    skope->ogl->setDistance( fitH * fitH / got );
```

It ran, it passed, and it compared `Dist / Zoom` with `Dist / Zoom`. A check on
a quantity that cannot distinguish the two cases is not a check. **A name is not
a measurement** — the entry is in `MISTAKES.md`, and it is the rule this lane's
gate is built to obey.

---

## 2. The fix

### 2.1 `src/nifskope_ui.cpp` — the bake asserts its own camera

One statement, in the `WW_IMPOSTOR_BAKE` block before anything renders:

```
const bool bakePersp = qEnvironmentVariableIntValue( "WW_IMPOSTOR_PERSP" ) == 1;
skope->ogl->setProjection( bakePersp );
```

It covers both halves of the bake — the crossed front/side cards (whose comment
has always claimed "orthographic front and side views") and the octahedral
sheets — and it fixes pass one as well as pass two, because `halfH0` is read
after the fit and is then the true half-height.

Nothing else in the bake needed to change. The fit's read-back already corrects
for a `Zoom` other than 1 (`Dist' = fitH²·Zoom/fitH`, so `Dist'/Zoom = fitH`),
and `glProjection`'s orthographic branch uses exactly `h2 = Dist / Zoom`,
`w2 = h2 · aspect` — the same pair of numbers the crop is computed from.

**MODULE AND FALLBACK (CONSTITUTION 10).** `WW_IMPOSTOR_PERSP=1` restores the
old camera exactly. It is the way back for a behavioural change a user can see,
and it is the CONTROL the gates fail against.

### 2.2 The output names the arm that served

Read back off the live `GLView`, never off what was asked for:

| where | line / key | meaning |
|---|---|---|
| sidecar | `projection ortho` / `projection persp` | written from `isPerspectiveProjection()`, not from the environment |
| sidecar | `orthofit <asked> <achieved> <persp 0|1>` | the fit through the same accessor the frames are sized with, and the projection it was read through. The first two are true in either projection; the third is the field that MOVES |
| `.lodm` `card` | `projection` | present only when the sidecar said so |
| `.lodm` `cardArray` layer | `projection` | per LAYER, because an array can hold a metric set beside a foreshortened one |

**Absence has a defined meaning and the documents now say which:** the line
arrived in the same change that fixed the camera, so a sidecar that does not say
was baked through the perspective frustum. Absence is the older vintage, not
"unknown", and a reader may refuse such a set by name. `LODM_VERSION` does not
move — re-read last, still `1` at `src/io/lodmfile.cpp:9` — because the key is
optional and its absence is defined.

Every `.lodm` written from a sidecar with no `projection` line is byte-identical
to before, which is what protects the identity gate.

### 2.3 `src/lodgen.cpp` — card functions only

Lane CLAMP owns the terrain bake in the same file. **The functions this lane
touched, by name:** `struct LodgenCard` (one new member, `octProjection`),
`lodgenCard()` (the sidecar parse arm and the card `.lodm` emitter), and inside
the card-array packer its local `struct Layer`, its layer fill and its layer
emit. Nothing in `lodgenBakeVtTile`, `lodgenBakeTerrain`, the `.lodt`/`.lodl`
path or any terrain function was read for writing or changed.

---

## 3. The cube proof, PRE-REGISTERED before the build

`scratchpad/cardortho_20260910/prereg_cube.md`, written before a line of the
gate ran, and the gate is `tests/spells/lodgen_octahedral.sh` bake 4.

**Why a cube.** Its orthographic silhouette is arithmetic. For a box of
half-extents `h` seen with screen axes `r` and `u`,

```
halfR = hx|rx| + hy|ry| + hz|rz|        halfU = hx|ux| + hy|uy| + hz|uz|
```

and the bake's own camera law — `setRotation( -90 + elev, 0, 90 - azim )`
through `Matrix::fromEuler` with y = 0 — gives those axes exactly:

```
r  = ( sin azim, -cos azim, 0 )
u  = ( sin elev cos azim, sin elev sin azim, cos elev )
```

(their cross product is the matrix's third row, which is the check that the
derivation is the code's and not a textbook's.)

**The predicted table.** OCT=8, TILE=64, cube half-extent 256. The bake's own
ladder picks a **64×64** frame, gap 4,4, pad 2,2, inner rect 60×60, `half`
428.959, `oct`-line half-extents **457.556**, mips = log2(4) = **2**. Predicted
span = `2·halfR·tw / (2·fullHalfW)`:

| distinct predicted x spans, texels | 35.81 | 41.21 | 43.42 | 46.55 | 48.04 | 49.66 | 50.13 |
|---|---|---|---|---|---|---|---|

**a seven-rung ladder over 64 frames, not one number** — deliberately, because a
single predicted span passes just as happily on a single wrong scale factor. At
OCT=4 the same cube predicts only two distinct values (every view sits on a
symmetry plane of the octahedron); the grid was raised to 8 for the spread.

**Tolerance: 2 texels**, and where it comes from — the crop's integer rounding
plus one antialiased texel of the smooth downsample on each side of the
silhouette.

**The fixture's own size is measured through a DIFFERENT code path**, so this is
not our output judging our output (CONSTITUTION 4): `WW_RENDER_ORTHO=512
WW_RENDER_CENTER=0,0,256 WW_RENDER_SIZE=640x480` through the render hook's
pinned camera, whose own gate is `tests/spells/render_shot.sh` section 7 (27 of
27 on 2026-09-10). 512 units of cube must span 320 px there, one pixel of edge
is 1.6 units, and the check is 256 ± 3.

### 3.1 The two no-foreshortening invariants, on the same 64 frames

| invariant | why it holds under ortho | orthographic | perspective CONTROL |
|---|---|---|---|
| **central symmetry** — the coverage mask against its own 180° rotation about its box, as a fraction of covered texels | an orthographic projection of a centrally symmetric solid is centrally symmetric | ≤ 0.05 | **> 0.05** |
| **near edge vs far edge** — the widest row of the silhouette's top fifth against its bottom fifth, as a fraction of their mean | no depth gradient across the frame | ≤ 0.05 | **> 0.05** |

The second is the brief's "identical in extent at the frame's near and far edge"
in its most direct form, and it needs no model of the object at all.

And one more, so the camera is shown reaching the FORMAT and not only the
picture: the two bakes must record **different** half-extents on the same
fixture.

---

## 4. The transition measurement, in a scene with ONE reference

`scratchpad/cardortho_20260910/transition.py`.

**Why the chunk route is closed.** Lane HOOKCAM took this measurement in a
placed Sanctuary chunk and could not get it (§B.5): the most isolated instance
of each of the three trees needs ~59-61° of frame to hold the subject at the
distance it turns into a card, while its nearest neighbour sits at 25-40° — a
factor of 2.3-3.9, on all three trees, at both distances. There is no field of
view that does both.

**The scene here holds one reference by construction: the model file itself,
loaded alone.** Nothing else can be in the frame.

| arm | what it is |
|---|---|
| source | the model the bake photographed, through the pinned camera (`WW_RENDER_VIEW` + `_FOV` + `_DIST` + `_CENTER`), one instance |
| card | the card **as a reader draws it** — the frame's quad at `center + ox·right + oy·up`, spanning ±`half`, composited from the shipped sheet with the `.lodm`'s own numbers and projected through the same camera, whose `upp` is read back out of that run's census rather than assumed |
| ctl | the same composite with `frameOffset` zeroed — what a reader that ignores the key draws. **It must fail** |

**The card arm is composited, not rendered, and that is deliberate and is a
finding.** NifSkope's own chunk builder emits `lodgenCardShape` — the LEGACY
crossed quad off a front|side sheet. It has no octahedral frames and no
per-frame offsets in it at all, so rendering it would not be the thing under
test. The `.lodm` is consumed by FO4CS; an independent implementation of the
documented reader rule is also a stronger check than our renderer checking our
own bake.

**Which frames.** At OCT=8 four frames sit exactly on axis views, so no blending
and no interpolation enters the measurement. Solving the bake's `viewDir`
against `GLView::viewRotations`:

| frame (i,j) | axis view | `ViewState` |
|---|---|---|
| (0,7) | Front (−90, 0, 180) | 5 |
| (7,7) | Right (−90, 0, 90) | 4 |
| (0,0) | Left (−90, 0, −90) | 3 |
| (7,0) | Back (−90, 0, 0) | 6 |

Front and Right are both used, at both distances, so one coincidence cannot pass.

**Distances**: `scratchpad/cardfit_20260909/rend/dists.json` — mid / ring
1873.95 / 7495.80 (`0003a28b`), 1457.66 / 5830.63 (`0004a074`), 766.78 /
3067.13 (`00038599`) — the same numbers HOOKCAM used, so the two tables are
comparable.

**Gate**: centre within **one card texel at that distance** (`2·halfW/tw`
divided by that run's `upp`, printed in pixels too) and both extents within
**2%**, over 3 trees × 2 views × 2 distances = 12 rows; the zeroed-offset
control must pass **0 of 12**.

### 4.1 The trunk-width measurement against the model's own dimensions

The same script's second half, and the brief's literal ask. The source model is
photographed through `WW_RENDER_ORTHO` at the card's own scale (the half-width
given is `1.05 · max(halfW, halfH)` so a tall tree is not clipped on a square
viewport; the scale itself comes back in the census), and frame (0,7)'s own
per-row widths are compared with it at that one world scale — **every row**,
resampled to 100 bands over each silhouette's own height, and then the widest
row of the bottom fifth and of the top fifth reported separately, which is "the
trunk width at the top and the bottom of the frame against the source model's
own dimensions".

### 4.2 The pictures

One per tree, `scratchpad/cardortho_20260910/cardortho_transition_<id>.png`,
three panels — **source | card | overlay** (blue mesh, orange card) — at the
ring distance, cropped to the silhouette with the row's own numbers burned into
the caption. **Not produced: nothing has been rendered.** They are step 4 of
`run.sh` and every one of them is to be opened before it is reported
(CONSTITUTION 5).

---

## 5. The gates, and what is a floor for what

Pre-registered in `scratchpad/cardortho_20260910/PENDING.md`. All PENDING.

| gate | what is new in it | the floor beside it |
|---|---|---|
| `tests/spells/lodgen_octahedral.sh` bake 1 | the sidecar says `projection ortho`; `orthofit`'s third field is 0 and its first two agree within 0.1%; the card `.lodm` carries `projection: "ortho"` | bake 4's perspective control says `projection persp` |
| `tests/spells/lodgen_octahedral.sh` **bake 4** (new) | the cube: 64 predicted spans within 2 texels, central asymmetry ≤ 0.05, near/far width difference ≤ 0.05, the two bakes recording different extents | the `WW_IMPOSTOR_PERSP=1` control must EXCEED all three |
| `tests/spells/lodgen_card_arrays.sh` | layer `id1` carries `projection: "ortho"` | layer `id2`, whose sidecar names no camera, carries **no key at all** — one array holding both states is what proves the field is written AND moves |
| `tests/spells/lodgen_impostor_cards.sh` | untouched (the legacy crossed card has no `.lodm` and no octahedral frame) | it is a floor: 12 ok must not move |
| `tests/spells/lodgen_identity.sh` | untouched | 8 ok must not move |
| CARDFINAL's gap / mip / per-frame gates | untouched in code | expected green; an expectation is not a measurement |

**Skipped, with the reason:** everything else in `tests/spells`. Nothing in this
change reaches the block viewer, the panels, the collision, the NIF writers, the
terrain bake or the `.lodl`/`.lodt` path.

Every embedded Python block of every harness edited was compile-checked —
`scratchpad/cardortho_20260910/check_harness_py.py`, 10 blocks in
`lodgen_octahedral.sh`, 3 in `lodgen_card_arrays.sh`, 1 in `run.sh`, all clean —
and every harness passed `bash -n`.

---

## 6. Documents

1. **`docs/LODGEN_CARD_SHEETS.md`** — new **§3.7**, "The camera is
   ORTHOGRAPHIC, and until 2026-09-10 it was not": the units-per-pixel identity,
   what the perspective frustum cost per consequence, the two new sidecar lines,
   the fallback switch, and the sentence a reader of a library needs — *a
   library whose sidecars carry no `projection` line is the older, foreshortened
   vintage*. Plus the two lines in §5's sidecar list and invariant **1c** in §7.
2. **`docs/LODGEN_LODM_FORMAT.md`** — `projection` in the `card` key table and
   in the `cardArray` layer shape, a paragraph in §3.1 (the "why the card does
   not move" section, which is exactly what the projection underwrites), a third
   way to get it wrong, and invariant 8.
3. **`tools/bake_impostor_cards.sh`** — a header block saying the camera is
   orthographic, that it never was, and that an old library must be re-baked.
4. **`WW_CHANGES.md`** — the entry, spliced in binary, `STATUS: NOT BUILT`.
5. **`MISTAKES.md`** — two entries (§8).

**Contract provenance** (`ww-contract-provenance`): both pages re-stamped. File
hashes and line counts re-read from disk — `src/lodgen.cpp`
`64191a7ed236ddb8` / 8,619 lines, `src/nifskope_ui.cpp` `dbf4540b51166e31` /
31,316 lines — every line number re-derived from its own anchor in one scripted
pass (`scratchpad/cardortho_20260910/anchors.py`, copied from CARDFINAL's and
re-pointed), and ten new rows added for the new code sites. Result:
**`LODGEN_CARD_SHEETS.md` 37 rows, 0 anchors not found; `LODGEN_LODM_FORMAT.md`
27 rows, 0 not found**, and a second pass after the write reports 0 moved. The
version constant was re-read LAST and does not move.

**Line endings**, Python byte counts: `src/`, `docs/`, `tools/`, `tests/`,
`MISTAKES.md` and the scripts are LF-only and are at CR = 0. `WW_CHANGES.md` is
mixed and is at **CR = 19,020**, equal to HEAD's — see mistake 2 below, which is
how it got there twice.

---

## 7. What is NOT claimed

* **Nothing was built and nothing was rendered.** Every number above is either
  another lane's measurement (§1, HOOKCAM's) or a PREDICTION written before the
  build (§3). There is no measured cube table, no re-baked library, no
  transition table and no picture.
* **The 19-tree library and the FO4CS sample set on disk are still the
  perspective bake.** They are steps 3 and 5 of `run.sh`.
* CARDFINAL's gap, mip and per-frame laws are untouched in code, and their gates
  are expected to stay green. Expected, not measured.
* The card arm of the transition test is a Python implementation of the
  documented reader rule, not our renderer. That makes it independent, and it
  also means a defect in NifSkope's own card drawing would not show up in it.

---

## 8. Mistakes

Both are in `MISTAKES.md` at the repo root, written the moment each was
recognised.

1. **Found, not made: every impostor card ever baked was measured as
   orthographic and DRAWN as perspective.** The rule: a name is not a
   measurement. `orthographicHalfHeight()` returns `Dist / Zoom` in either
   projection, so the bake's own read-back beside the fit compared two numbers
   that could not tell the cases apart — a check existed, ran, and passed on the
   wrong quantity. When code asserts a camera property, check the PROPERTY (a
   known object's span, a symmetry the projection either preserves or does not),
   never a helper named after it.

2. **Mine: one `sed -i` normalised all 19,020 CRs out of `WW_CHANGES.md`** —
   three lines after a binary splice that had asserted the count on both sides,
   to fix a single mis-typed character. `sed -i` rewrites the whole file with
   LF, and `WW_CHANGES.md` is one of the files CONSTITUTION 8 names as mixed.
   Caught by the byte count chained after it. Repaired from
   `git show HEAD:WW_CHANGES.md`: the damaged file was
   `header + <two new LF-only entries> + body`, `cur.endswith(body_lf)` held
   exactly, and the CRLF body was spliced back under the new entries — CR is
   19,020 again and equal to HEAD's, LF unchanged from the damaged file, so no
   content moved. Lane WATER2's entry, written the same hour, contributed 0 CRs
   of its own (the count matched HEAD before the accident), so nothing of theirs
   was lost. **The rule: no `sed -i`, `tr`, `>` redirect or editor on a mixed
   file, ever** — a typo inside a spliced block is fixed by redoing the splice,
   and the byte count belongs after the LAST write of a turn, not the first.

---

## 9. Skill review (CONSTITUTION 1a)

**Loaded and used.** `nifskope-ww-render-shot` — the switch table, the `upp`
arithmetic that both the cube gate and the transition composite rest on, its
instruction to verify a pin by reading `upp` rather than by comparing two files,
`WW_RENDER_SIZE` being clamped so the size must be read back from the PNG, and
its own §"the projection is PERSPECTIVE ... the impostor bake included", which
is the sentence this lane exists to make false. `nifskope-ww-resume-pending` —
the read order, the qmake-before-make rule and the exe-newer-than-EVERY-changed-
file sweep, both written into the resume; and §6, "when a gate fails, measure the
cause and STOP", which is why `run.sh` does not stop on a red gate and does not
try to cure one. `ww-contract-provenance` — the five steps, its worked anchor
script reused rather than rewritten, and re-reading the version constant last.

**The skill I broke, and it cost the round's one real mistake:** the rule that
Python carrying a backslash or a `\n` goes into a script FILE written with the
Write tool, never a bash heredoc. Two heredoc patches silently had their `\\n`
collapsed and refused to match; after that every patch went through a file. It
is in `nifskope-ww-commit` §0 and in `MISTAKES.md` six times, and the second
half of it — no line tool on a mixed file — is mistake 2 above.

**Written this session.**

* **`.claude/skills/ww-analytic-fixture-gate/SKILL.md`** (new). The procedure
  this lane derived from scratch and that CARDFIT3, CARDPAD, CARDFINAL and
  HOOKCAM each derived a partial version of: prove a pipeline's recorded numbers
  describe its picture by baking a fixture whose answer is arithmetic. It
  carries the parts that were genuinely re-worked out here — *a name is not a
  measurement*; deriving the screen axes from the code's own camera law rather
  than a textbook's; **pre-registering a LADDER rather than a number**, with the
  OCT=4-gives-two-values / OCT=8-gives-seven example that made the gate strong;
  measuring the fixture's own size through an independent instrument so the gate
  is not circular; shipping the defect behind a switch so it becomes the control;
  the two projection invariants (central symmetry, near-edge vs far-edge) that
  need no model of the object; and making the OUTPUT name the arm that served,
  with absence given a defined meaning.

**Amended.**

* **`nifskope-ww-render-shot`** — its "the projection is PERSPECTIVE in every
  headless run, the impostor bake included" is now false for the bake, and a
  skill that tells lanes to rely on behaviour that no longer exists is the most
  expensive document in the repo (`nifskope-ww-resume-pending` §7.4). A new
  subsection records the exception, the two sidecar lines, `WW_IMPOSTOR_PERSP`,
  the fact that the bake does NOT read `WW_RENDER_ORTHO` (the pin and the bake
  are two cameras and would fight), and that `isPerspectiveProjection()` — not
  `orthographicHalfHeight()` — is the accessor that answers.
* **`nifskope-ww-commit`** §3 — the no-line-tool rule and the exact repair for a
  mixed file that has already been normalised, from mistake 2.

**Both amendments are in the REPO tree only** (`<repo>/.claude/skills`), as is
the new skill. **The director has to apply all three to the live tree at
`E:\Projects\Claude\.claude\skills`** — the two trees drift and nothing syncs
them (CONSTITUTION 1a). The live and repo copies of `nifskope-ww-render-shot`
were byte-identical (16,826 bytes, both 00:00 today) before this lane's edit, so
the repo copy can be copied over wholesale; `ww-analytic-fixture-gate` is new in
the repo tree and absent from the live one.

**Declined:** a skill for the octahedral frame-to-axis-view mapping (frame (0,7)
= Front, and the rest). It is a CONTRACT, not a procedure — it belongs where a
reader of the format will find it, and it is now derivable in one line from
`docs/LODGEN_LODM_FORMAT.md` §3 plus `GLView::viewRotations`.

---

## 10. Owed, and to whom

* **To bungo:** the three transition pictures, the transition table, and the
  cube table measured against the pre-registration. All three exist as
  predictions and scripts and none of them as a measurement.
* **To the director:** the three skill files to copy into the live tree (§9);
  and the fact that **every impostor card library and every card `.lodm` in the
  tree, including the FO4CS handoff sample set, was baked through a perspective
  frustum and needs regenerating** — `scratchpad/cardfinal_20260909/cards_perframe`,
  `scratchpad/cardfit_20260909/cards_after`, `scratchpad/cardpad_20260909/cards_gap`
  and `scratchpad/images_20260909/gen/cards_trees*` are all that vintage.
* **Nothing is owed to another lane.** No file outside this lane's ownership was
  changed, so there is no `CHANGE_NEEDED.md`.

---

## Build (BUILD4)

Built and gated by lane BUILD4 on 2026-09-10, after lane CLAMP
(`scratchpad/clamp_20260910/DONE`). Appended, not rewritten. Skills loaded:
`nifskope-ww-resume-pending`, `nifskope-ww-build-verify`,
`nifskope-ww-render-shot`, `ww-texel-picture`.

### B.1 The build

`Fallout4.exe` and `NifSkope.exe` both absent (`rc=1`) before the build and
before every launch below. `qmake NifSkope.pro` rc **0**, `make -j2` rc **0**
(`Nothing to be done for 'first'`). Staleness sweep over all 15 changed files:
**0 stale**; `cmp res/style.qss release/style.qss` in step.

`release/NifSkope.exe` **2026-09-10 01:01:04** against `src/nifskope_ui.cpp`
00:38:50 and `src/lodgen.cpp` 00:40:11 -- lane WATER2's link already carried
this lane's code, and the object check says so rather than inferring it
(`nifskope_ui.o` 00:44:24, `lodgen.o` 00:44:00).

| artefact | mtime |
|---|---|
| `release/NifSkope.exe` | 2026-09-10 01:01:04 |
| `src/nifskope_ui.cpp` | 2026-09-10 00:38:50 |
| `src/lodgen.cpp` | 2026-09-10 00:40:11 |
| `tests/spells/lodgen_octahedral.sh` | 2026-09-10 00:54:21 |
| `tests/spells/lodgen_card_arrays.sh` | 2026-09-10 00:53:40 |
| `tools/bake_impostor_cards.sh` | 2026-09-10 00:54:45 |
| `docs/LODGEN_CARD_SHEETS.md`, `docs/LODGEN_LODM_FORMAT.md` | 2026-09-10 00:57:23 |
| `lodgen_octahedral.log` | 2026-09-10 01:13:15 |
| the three transition pictures | 2026-09-10 01:18 - 01:19 |

### B.2 The gate table, against section 5's pre-registration

| gate | bar | measured | verdict |
|---|---|---|---|
| `lodgen_octahedral.sh` | RESULT PASS | **100 ok, 0 FAIL** | ok |
| `lodgen_card_arrays.sh` | RESULT PASS | **35 ok, 0 FAIL** | ok |
| `lodgen_impostor_cards.sh` | 12 ok, must not move | **12 ok, 0 FAIL** | ok |
| `lodgen_identity.sh` | 8 ok, must not move | **8 ok, 0 FAIL** | ok |
| bake 1 sidecar | `projection ortho` | said orthographic | ok |
| bake 1 `orthofit` | first two within 0.1%, third field 0 | **`orthofit 972.833 972.833 0`** -- the first two agree EXACTLY | ok |
| the perspective control's line | `projection persp` | said perspective | ok |
| the cube's own size | 256 +- 3 units, through `WW_RENDER_ORTHO` -- a different code path | **256.5097** | ok |
| cube spans, run-time table | all 64 frames within 2 texels | **worst 1.78** | ok |
| cube spans, **FROZEN** `prereg_cube.md` | the same bar, against the table written BEFORE the build | **worst 1.87** | ok |
| central asymmetry | <= 0.05 | **0.000** | ok |
| near-edge vs far-edge width | <= 0.05 | **0.000** | ok |
| the `WW_IMPOSTOR_PERSP=1` control | must EXCEED all three | **18.70 texels** (23.19 against the frozen table), **0.495**, **0.850** | ok |
| card array layers | `id1` ortho, `id2` **no key at all** | `{'0004a074': 'ortho', '0004a075': None}` | ok |
| the 19-tree re-bake | 19 baked, 0 failed | **19 baked, 0 failed** | ok |
| its sidecars | 19 of 19 `projection ortho` | **19 of 19** (see B.4) | ok |
| its `orthofit` third field | 0 perspective | **0 of 19** | ok |
| its `.lodm` files | every card `projection ortho` | **18 of 18 ortho, 0 absent** | ok |
| the FO4CS sample set | every card entry ortho, `(absent)` count 0 | **38 card entries, all ortho; no `(absent)` line printed** | ok |
| the transition table | every row within 1 card texel on centre and 2% on both extents, 12 rows | **1 of 12** | **MISS** |
| the ZEROED-OFFSET control | **0 of 12** passing | **0 of 12** | ok |

**The cube proof is the round's strongest result.** It passes against the
pre-registration as PRE-REGISTERED -- not only against the table the harness
recomputes at run time from the sidecar's own `oct` line. Those two tables
differ by at most **0.10 texels**, so the recomputation did not loosen the bar;
and the perspective control fails every one of the three invariants by a factor
of nine or more. `scratchpad/build4_20260910/frozen_cube.py` is that
re-comparison.

### B.3 The transition table, in full

Three trees x two axis views x two distances, each with its zeroed-offset
control. `card` is the arm under test, `ctl` the reader that ignores
`frameOffset`.

| tree | view | distance | arm | one card texel, px | centre off, px | in texels | dx extent | dy extent |
|---|---|---|---|---|---|---|---|---|
| 0003a28b | front | mid | card | 7.68 | 18.18 | 2.37 | 21.85% | 3.33% |
| 0003a28b | front | mid | ctl | 7.68 | 80.69 | 10.51 | 21.85% | 1.83% |
| 0003a28b | front | ring | card | 1.92 | 8.25 | 4.30 | 6.67% | 0.87% |
| 0003a28b | front | ring | ctl | 1.92 | 12.04 | 6.27 | 6.67% | 0.87% |
| 0003a28b | right | mid | card | 7.68 | 69.16 | 9.01 | 4.71% | 2.07% |
| 0003a28b | right | mid | ctl | 7.68 | 85.09 | 11.08 | 4.71% | 0.87% |
| 0003a28b | right | ring | card | 1.92 | 4.74 | 2.47 | 3.32% | 1.30% |
| 0003a28b | right | ring | ctl | 1.92 | 10.61 | 5.52 | 3.32% | 1.30% |
| 0004a074 | front | mid | card | 7.78 | 27.54 | 3.54 | 6.65% | 5.84% |
| 0004a074 | front | mid | ctl | 7.78 | 42.50 | 5.46 | 6.65% | 1.91% |
| **0004a074** | **front** | **ring** | **card** | 1.94 | 1.58 | **0.81** | **0.92%** | **0.43%** |
| 0004a074 | front | ring | ctl | 1.94 | 13.44 | 6.91 | 0.92% | 0.43% |
| 0004a074 | right | mid | card | 7.78 | 40.39 | 5.19 | 8.26% | 8.29% |
| 0004a074 | right | mid | ctl | 7.78 | 3.20 | 0.41 | 8.26% | 4.04% |
| 0004a074 | right | ring | card | 1.94 | 3.35 | 1.72 | 7.50% | 2.17% |
| 0004a074 | right | ring | ctl | 1.94 | 12.54 | 6.45 | 7.50% | 2.17% |
| 00038599 | front | mid | card | 16.49 | 21.00 | 1.27 | 2.84% | 0.00% |
| 00038599 | front | mid | ctl | 16.49 | 25.00 | 1.52 | 2.84% | 0.00% |
| 00038599 | front | ring | card | 4.12 | 2.12 | 0.51 | 2.63% | 0.40% |
| 00038599 | front | ring | ctl | 4.12 | 4.30 | 1.04 | 2.63% | 0.40% |
| 00038599 | right | mid | card | 16.49 | 2.50 | **0.15** | 18.70% | 0.00% |
| 00038599 | right | mid | ctl | 16.49 | 20.50 | 1.24 | 18.70% | 0.00% |
| 00038599 | right | ring | card | 4.12 | 1.12 | **0.27** | 5.56% | 0.40% |
| 00038599 | right | ring | ctl | 4.12 | 3.35 | 0.81 | 5.56% | 0.40% |

**1 of 12** card rows meets both bars. The one that does is 0004a074 front at
the ring distance. Four more meet the centre bar and are turned away by an
extent (00038599 right mid at 0.15 texels but 18.70% on dx; 00038599 front ring
at 0.51 texels but 2.63%; 00038599 right ring at 0.27 but 5.56%; 00038599 front
mid at 1.27 texels).

**The trunk-width table**, the same run's second half:

| tree | source height px | card height px | height diff | source bottom fifth | card bottom fifth | diff | source top fifth | card top fifth | diff |
|---|---|---|---|---|---|---|---|---|---|
| 0003a28b | 941 | 941 | **0.00%** | 226 | 267 | 18.14% | 311 | 591 | 90.03% |
| 0004a074 | 941 | 941 | **0.00%** | 32 | 53 | 65.62% | 224 | 494 | 120.54% |
| 00038599 | 941 | 941 | **0.00%** | 89 | 129 | 44.94% | 66 | 91 | 37.88% |

The HEIGHT matches to **0.00% on all three trees** -- so the card's vertical
world scale is right, which is the thing the orthographic camera was changed to
fix. The WIDTHS are 18% to 121% wider than the source at both the trunk and the
crown, and the three pictures show why in one look: the card's crown is a filled
blob where the mesh is lacy. That is the shape of coverage-threshold dilation --
a twig covering a fraction of a 128-texel frame becomes a whole texel on the way
in and a solid band on the way out -- and it is NOT the projection. Named as a
candidate with its discriminator: re-run the same measurement at two frame sizes
(the dilation scales with the texel, a projection error does not).

### B.4 Two readings that look like misses and are not

1. **`19 of 20` sidecars saying ortho.** The 20th file is `cards/library.txt`,
   the run-level manifest -- `oct 8 / tile 128 / ref 1183.3 / half_aux 0 /
   candidates trees`, no camera and correctly no `projection` line. It is
   **19 of 19** card sidecars. `run.sh` line 57's denominator is
   `ls "$CARDS"/*.txt`, which counts the manifest.
2. **`run.sh` step 2 printed nothing.** Its `sed -n '/bake 4/,$p'` looks for a
   line containing `bake 4` and the harness never writes one -- the cube block
   is labelled by its content, not by a bake number. The numbers were read
   straight out of `lodgen_octahedral.log` instead and are in B.2. Both are
   reporting defects in `run.sh`, not measurement failures.

### B.5 The transition instrument's own sensitivity -- a finding

CONSTITUTION 4, rule 1 of 2026-09-04 21:33: a field must be WRITTEN and must
MOVE. Comparing each card row with its own control:

| column | rows where card and control read the SAME value |
|---|---|
| centre offset | **0 of 12** -- moves on every row |
| dy extent | 8 of 12 |
| **dx extent** | **12 of 12** |

So the control fails only through the centre column. The `2%` extent bars are in
the gate but nothing in the round demonstrates they can tell the card arm from a
reader that ignores `frameOffset` -- and four of the eleven failing rows are
failed by exactly those bars. This does not make the 1-of-12 result wrong; it
means the extent half of the gate has no floor under it yet. Not fixed and not
re-pinned; reported.

### B.6 The pictures (CONSTITUTION 5)

All three opened before being reported:
`cardortho_transition_0003a28b.png` (48,403 bytes),
`_0004a074.png` (18,186), `_00038599.png` (11,892).
Source | card | overlay, blue mesh and orange card, at the ring distance.

**A defect in the pictures themselves**, against `ww-texel-picture`'s caption
rule: the three panel titles are drawn at one y and overlap into an illegible
line across the top of every one of them, and the bottom caption is CLIPPED at
the right edge -- `0003a28b`'s ends mid-number at `ctl: centre 12.04 px = 6.27
card tex`. The numbers quoted in this report were therefore read from
`transition.log`, not from the picture, which is exactly the coupling that rule
exists to prevent. The pictures still show the thing they were made to show.

### B.7 What was NOT done

* **Nothing was committed** (CONSTITUTION 8).
* No fix was landed for the transition miss and no bar was re-pinned.
* CARDFINAL's gap, mip and per-frame gates are inside `lodgen_octahedral.sh`'s
  100 ok and did not move -- so that expectation is now a measurement.
* Skipped harnesses, with the reason: everything in `tests/spells` this change
  does not reach (block viewer, panels, collision, NIF writers).
* The three skill files section 9 owed to the LIVE tree
  (`nifskope-ww-render-shot`, `nifskope-ww-commit`, `ww-analytic-fixture-gate`)
  were checked and are already byte-identical in both trees. Nothing owed.
* bungo's open NifSkope window predates the exe and needs a restart.

### B.8 The rest of the shared build's suite

Lanes HOOKCAM and WATER2 changed `tests/spells/render_shot.sh`,
`tests/spells/lodl_water.sh` and the `.lodl` readers in the SAME build, so BUILD4
swept those too. Logs in `scratchpad/build4_20260910/logs/`.

| harness | result |
|---|---|
| `lodl_water.sh` | **33 ok, 0 FAIL, PASS** -- 346 water bodies (sea 1, river 115, lake 230), 0 of 346 disagreeing on table area vs plane count |
| `lodl_open.sh` | **23 checks, 0 failures, PASS** -- 9 planes render, 8 distinct pictures, greatest disagreement with the block pyramid 0.000 units |
| `render_shot.sh` | **82 checks, 2 failures, FAIL** |

**Both `render_shot.sh` failures are the same check, and it is the one that
cannot tell our window from anything else on the desktop:**

```
FAIL bake_lod: and the screen itself did not change  luminance range 16.119 over 45 samples (bar 15.756, desktop noise 5.252)
FAIL bake_oct: and the screen itself did not change  luminance range 83.620 over 80 samples (bar 15.756, desktop noise 5.252)
```

Every OTHER invisibility check in both groups is green, and each of those IS a
direct measurement of NifSkope's own windows: `0 of 4` / `0 of 6` records on the
primary, `0 of 7` / `0 of 19` samples from outside, `0 of 2` / `0 of 3` mapped
records at opacity > 0, `0 of 7` / `0 of 19` with layered alpha > 0. Only the
whole-screen luminance sampler moved. **Candidate, with its discriminator:** the
primary monitor was not idle -- this session's own console repainted it
throughout the run -- and the discriminator is a re-run with the desktop quiet.
Not fixed and not re-pinned; it is lane HOOKCAM's harness, not this lane's gate.

**Section 7 -- the pinned orthographic camera the cube fixture is measured
through -- is 27 of 27 green.** `ortho view 5` and `view 4` at eye 500, 1000 and
2000 all span **376.910 px** against a predicted 376.75 with `upp` constant at
1.358991, while the perspective arm scales 765 / 251 / 107 over the same three
distances. That is the independent instrument CARDORTHO's cube gate rests on, and
it holds.
