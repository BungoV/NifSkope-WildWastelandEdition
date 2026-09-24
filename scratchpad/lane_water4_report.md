# Lane WATER4 -- potential flow inside each body, and dye

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main` at `720762a`.
**Nothing committed** (CONSTITUTION 8). Written incrementally; each section is
appended as it finishes.

Read first: `CONSTITUTION.md` (1, 1b, 4), the `HANDOFF.md` top block,
`scratchpad/lane_water3_report.md` in full (with its Build section),
`scratchpad/lane_water2_report.md`, `scratchpad/specs_20260909/spec_water.md`.
bungo's words, verbatim, on the after-picture: *"That stroke doesn't look
smooth at all, it's like overlapping circles more like."* His design:
*"Could this maybe use a bit of some simulation though?"*; dye: *"a factory
that's releasing toxic sludge into a river, or river flowing into an ocean and
the river and the ocean may have slightly different color"*.

---

## 0. The gates, PRE-REGISTERED before any code ran (CONSTITUTION 1)

Written 2026-09-10 before the first measurement script existed. Every number
below is the number the harness will print; a gate that fails is reported as
failed with its number, never re-registered.

| gate | what is measured | passes when |
|---|---|---|
| D1 the disc defect (measurement, not a gate) | on `water3_20260910/work/charles_marked.lodl` body 3: the number of constant-direction PATCHES (4-connected components of texels sharing one 8-bit direction, >= 64 texels, whose boundary with other wet texels is a SEAM -- more than half of the boundary pairs differ by > 5 degrees), the patches' equivalent radius against the stroke half-width (4096/2 = 2048 units = 16 texels at 128 units a texel), and the angle jump across each seam pair | reported, and the code that makes it named by line |
| F1 continuity | a synthetic channel 256 texels long, 32 wide for x < 128 and 16 wide for x >= 128 (a half-width narrows), constant depth, inflow across the west end, outflow across the east end: mean speed over the cross-section at x = 64 against x = 192; the FLUX through 10 cross-sections at x = 16, 40, ..., 232 | speed ratio 2.00 +- 5 percent (1.90..2.10); every flux within 3 percent of their mean |
| F2 island | a channel 256 x 128, an island = a disc of radius 8 texels at (128, 64), inflow west, outflow east: (a) the flux through the north half and the south half of the column x = 128 sum to the inflow within 1 percent and differ from each other by < 2 percent (it parts and rejoins); (b) mass balance: the discrete divergence at EVERY wet cell is 0 to 1e-6 of the inflow (which is what "zero normal flux at every bank face" means on a grid -- the wall faces carry no flux by construction and this is the check that the construction held); (c) tangency at the straight banks: at every bank texel of the channel walls outside 3 radii of the island, the cell velocity's normal component is below sin(1 degree) of its magnitude; (d) at the island's own bank texels the solved direction is compared with the analytic cylinder-in-a-stream direction evaluated at the cell centre (r = R + 1/2): mean absolute difference < 5 degrees, maximum < 15 degrees (a staircase disc of radius 8 is not a circle; the number is printed either way) | all four |
| F3 lake, no outlet | a disc lake of radius 32 texels with NO constraint | the solver is not entered (the writer's zero is kept) and, entered anyway with no sources, returns max speed = 0 exactly |
| F4 lake, one outlet | the same disc with a 3-texel outlet notch on its east rim as the only sink and a UNIFORM source over the lake (the catchment) | every streamline seeded on an 8 x 8 grid of wet texels reaches an outlet texel within 4 diameters of travel (64 of 64); the texels whose velocity points AWAY from the outlet (dot with the direction to the outlet centre < 0) number 0; the mean cosine is stated |
| F5 the Charles | `charles_marked.lodl`'s stroke (the harness's centreline stroke toward the mouth, width 4096) re-solved: D1's patch count, the 99th percentile of the angle difference between 4-adjacent wet texels, the seam fraction (pairs differing by > 10 degrees over all adjacent wet pairs), the mean direction's cosine against the centroid-to-mouth direction, R | patches 0; p99 < 5 degrees; seam fraction < 0.5 percent; cos > 0.9; R stated, not gated |
| F6 the plume | a synthetic river (a channel 16 wide, 96 long) entering a synthetic sea (128 x 128) at the middle of the sea's west side; the sea's field is the mouth as a source with the sea's cut edges as the far field; dye at the mouth with half-distance L = 32 texels. PREDICTED before the bake from the flow alone: the plume's length = the streamline distance along the centreline at which the weight falls to 1/8 of the mouth's = 3 L = 96 texels, and its direction = the mean velocity direction over the sea's texels within L of the mouth (east, 0 degrees). MEASURED after: the distance along the centreline where the weight crosses 1/8, and the weight-centroid's direction from the mouth | both within 10 percent (length 86..106 texels; direction within 9 degrees) |
| F7 the dye pin | a dye pin at x = 32 in a straight channel of length 256 flowing east, strength 1, L = 32: weight at x = 32 + 32 is 1/2 +- 10 percent, weight at x < 32 is 0 (upstream is untouched), weight at x = 32 + 96 is 1/8 +- 10 percent | all three |
| F8 solve cost | the Charles (25,114 wet texels) in the C++ solver | under 1.0 s wall, iterations and the relative residual printed; residual < 1e-8 |
| P0-P8, `water_mark.sh`, `lodl_water.sh` | unchanged, re-run on the built exe | green |
| `water_flow.sh` (new) | F1..F8 through `lodl <copy> --water-mark-selftest` with `WW_WATER_FLOW_TEST=1` | every gate above, each with its floor |

**The method, stated before the code**: on each marked body's texel mask, a
scalar potential `phi` from `div( k grad phi ) = S`, `k` = the water depth at
the texel (body height minus the terrain height from the file's own level-0
plane, floored at 8 world units) so the flux `k grad phi` prefers deep water;
no-flux at every bank face (the five-point stencil only ever reaches a
neighbour inside the mask); `S` = +Q spread over the source texels, -Q over
the sink texels; velocity `u = -grad phi` (for `k = depth` this is flux over
depth, so a narrows speeds up by its width ratio); a body with no source but a
sink gets a UNIFORM source (rain); solved by Jacobi-preconditioned conjugate
gradient on the compacted wet set (the pure-Neumann system is singular but
consistent once the sources balance, which they are made to), relative
residual 1e-9, cap 20,000; the stroke's interior enters as a conductance
boost along its path (x4 inside its half-width) -- a soft preference, never a
held value -- and its first and last points are a source and a sink only when
the body has none from a pin or from the table's own `source` / `outlet`
contacts. The flow plane's direction is `u`'s; its speed nibble is
`round( 8 |u| / mean|u| )`, saturating at 15 (1.9x the mean) as the format
already says; confidence stays the geodesic distance-to-constraint decay.

**Dye, stated before the code**: a fourth plane in the same tiled container
format at the flow plane's rate, 4 bytes a sample, `uint32 = source | weight
<< 16`, source 1..32767 = a body id (the river whose water this is), `0x8000 |
n` = the n-th dye pin in the stroke store, weight 0..255; written ONLY when the
store carries a dye mark, so an unmarked file and an undone file stay
byte-identical. It is referenced from the version-3 header's reserved word at
`0xF4` (a 32-bit offset, refused by name past 4 GB) under a new section bit
`SECT_DYE = 1 << 8`, so the version stays 3 and no offset moves. Weight is the
steady advection-decay solution `u . grad c = -|u| c / L` on the potential
field, solved exactly in one pass by visiting texels in DESCENDING potential
(every upwind neighbour has a higher potential, so it is already known); one
knob, the half-distance `L` in world units, default 8,192 (two cells), stored
as a kind-8 mark when changed.

---

## 1. The disc defect, measured (task 1)

`scratchpad/water4_20260910/disc_metric.py 3 <file>` through the independent
decoder (`lodl_np.py` wraps WATER2's `lodl_v3_authority.py`; no writer code).
On the file behind the after-picture, `water3_20260910/work/charles_marked.lodl`,
body 3 (the Charles, 25,114 wet texels, 48,574 adjacent wet pairs):

| number | before (the writer's constant) | after one stroke (the disc fill) |
|---|---|---|
| distinct 8-bit directions | 1 | 99 |
| adjacent angle difference p50 / p90 / p99 | 0 / 0 / 0 | 0.00 / 1.41 / **40.78 deg** |
| seam fraction (adjacent pairs differing by > 10 deg) | 0 | **2.97 percent** (1,442 pairs) |
| seam-bounded constant-direction patches (>= 64 texels) | 0 | **39** |
| their equivalent radius sqrt(A/pi) | -- | median **12.0**, max **15.3 texels** |
| mean jump across a patch's seam | -- | 5.2 .. **72.9 deg**, most between 20 and 45 |

The stroke's width is 4,096 world units = 32 texels, so its HALF-width is 16
texels: the patches are discs of that radius, truncated by the banks and by
their neighbours, which is the picture bungo described.

**The code that makes it** (`src/watermark.cpp`, the version BUILD5b built):

* the hold loop at 1077-1097: for every segment of the stroke, every mask
  texel within `halfW` of the segment (`if ( d > halfW ... ) continue;`) is
  written the SEGMENT's tangent and marked `held` -- a Dirichlet capsule of
  radius 16 texels per segment, and where capsules overlap the later segment
  overwrites the earlier, so each texel carries exactly one segment's tangent;
* the harness's stroke has one point a cell (`axisStroke`, "one point a cell
  is plenty"), 32 texels apart, so consecutive capsules of radius 16 just
  touch: the river is tiled by discs, and the seam between two discs is the
  angle between two consecutive segments' tangents, 20-45 degrees on a
  meandering reach;
* the fill at 1100-1146 (red-black SOR) then solves only the texels NOT held
  -- the slivers between discs and the banks -- which is why p90 is one step
  and p99 is 41 degrees: nine tenths of the pairs are inside a disc, the
  seams carry the whole jump.

Neither the sampling rate nor the width fixes it: a denser stroke makes
smaller discs with smaller jumps but the same held tiling; a wider stroke
makes bigger discs. The held disc is the mechanism, and it is replaced.

---

## 2. The method, proved on synthetic masks before the C++ was written

`scratchpad/water4_20260910/flow_proto.py` is a numpy twin of the solver the
C++ implements (same stencil, same face conductance, same PCG, same face-flux
velocity, same one-pass dye, Pollock's tracer) run on the pre-registered gates.
It exists so a red C++ gate can be told apart from a red METHOD.

| gate | prototype | as registered |
|---|---|---|
| F1 speed ratio | **2.0000** (460 CG iterations, residual 9.3e-10) | PASS |
| F1 flux constant | max deviation 1.6e-10 | PASS |
| F2 parts and rejoins | north 0.5000, south 0.5000, sum 1.0000 | PASS |
| F2 mass balance | worst 3.1e-12 | PASS |
| F2 straight-bank tangency | worst normal fraction 3.9e-4 over 416 bank texels (sin 1 deg = 0.0175) | PASS |
| F2 island bank vs the analytic cylinder | **mean 12.01, max 22.40 deg** over 48 bank texels | **FAIL** |
| F3 closed lake | max speed 0 exactly | PASS |
| F4 nothing points away | 0 of 3,225, mean cosine 0.914 | PASS |
| F4 every streamline reaches the outlet | 40 of 40 | PASS (after the instrument's own seed bug, section 7) |
| F7 dye pin | 0.5000 at L, 0 upstream, 0.1250 at 3 L | PASS (after the e-fold bug, section 7) |
| F6 plume | predicted 96 texels / 0.0 deg before the dye; measured 98.2 / 0.0 after | PASS |

**F2's island-bank gate fails as registered and the gate is not moved.** The
cell velocity at a STAIRCASE bank cell is the mean of its two face velocities
per axis with the wall face at zero, and on a diagonal staircase that halves
one component and not the other. It is R-independent (R = 8: 12.0, R = 16:
11.4, R = 32: 12.1 deg); one ring in it is 4.0 / 2.8 / 2.3; at 2 R it is 0.8 /
0.4 / 0.1 -- the SOLVE is right, the bank-cell reconstruction is what is off.
An 8-neighbour least-squares gradient halves it (5.8 mean, 15.7 max) but
breaks the streamline tracer, so the mass-consistent face average was kept for
the flow and the DIRECTION written to the plane is handled below.

**The Charles, in the prototype** (the harness's stroke out of
`charles_marked.lodl`'s own store, 47 points, width 4,096; ends = discs of the
half-width; the x4 quartic bump under it; 25,114 texels, 1,565 CG iterations,
residual 9.4e-10):

| direction plane | patches | p99 | seams | R | mean |
|---|---|---|---|---|---|
| the disc fill (WATER3, out of the file) | 39 | 40.78 | 2.97 % | 0.807 | 111.3 |
| raw solve, zero where slack or bank | 0 | 152 | 8.3 % | 0.68 | 104.0 |
| + continuation into slack and bank texels | 0 | 15.5 | 3.05 % | 0.79 | -- |
| + 8 in-mask 3x3 vector-average passes | **0** | **7.0** | **0.26 %** | 0.788 | 111.5 |
| + the breadth-first seed of the continuation (the final method) | **0** | **8.4** | **0.29 %** | 0.796 | 111.8 |
| held out: the marsh, body 2, same rule | 0 | 11.3 | 1.0 % | 0.53 | 87.7 |

Where the roughness lives (`smooth_probe.py`, by chamfer distance to the
bank): within 1-3 texels of a staircase bank the raw direction jumps 22-25 deg
at its 99th percentile and 11 % of pairs are seams; at 6+ texels it is 5.6 deg
and 0.1 %. A shallow-bank conductance taper does NOT cure it (tested at 3, 4
and 6 texels: 16.9 deg); continuing the direction from the interior and
low-passing it does. **F5's p99 gate will therefore read ~8 deg against the
registered 5 and is expected RED in the C++; patches (0 of 39) and seams
(0.29 % of 2.97) are expected green.** The 8 passes were chosen on the Charles
(the gate's own body) -- stated -- and the marsh is the held-out check. The
jumps that remain at 12 passes sit at islet tips and one-texel necks where the
flow really reverses across a texel: the metric counts physics there.

---

## 3. The solver, as built (`src/watermark.{h,cpp}`)

`WaterFlowGrid` (public, in the header; the harness uses it directly):
`build` compacts the wet texels, lists the east faces then the north faces
with the harmonic-mean conductance, and labels the connected pieces (a body is
often several: the writer's bridge joins pieces up to two texels apart);
`solve` is Jacobi-preconditioned CG, the right-hand side balanced PER PIECE (a
piece with no Dirichlet cell is singular and needs its own balance -- the
first prototype run produced NaN on the Charles for exactly this), isolated
texels get a zero preconditioner, relative residual 1e-9, cap 20,000;
`velocity` is the face average; `divergence` is the check; `dye` is the
one-pass steady advection-decay in descending potential with the mean chord
through a cell as its distance; `trace` is Pollock's.

`WaterMarkDoc::solveBody` replaces the harmonic fill: the window (the whole
bbox, or a cut of margin 256 texels round the constraints when the bbox
exceeds 2^20 texels, its cut edges Dirichlet 0); the mask; the depth from
`LodtFile::height` floored at 8 units; the stroke's quartic bump; sources and
sinks in the order pins (a disc of the width) -> the table's own `outlet` /
`source` contacts (a chamfer from the other body over the window plus 64
texels, the band within 2.5 texels or the nearest band within 64) -> the
strokes' first and last points (discs) -> uniform rain / seepage when one side
is missing -> a refusal in words when both are; the solve; the strokes'
authority (mean cosine of tangent against the flow under them < 0 -> re-solve
from the strokes' ends alone, and the note says so); the written direction
(seeded by a breadth-first walk from the moving interior texels, relaxed by
SOR, low-passed by 8 passes); the speed nibble `round( 8 |u| / mean )`
saturating at 15; the confidence chamfer from the marks; the record's derived
fields as before. `solve()` walks the bodies in id order, then `solveDye`, and
reports `solveSeconds`, `strokeAgreement`, `dyeTexels`. Cost: the numpy
prototype takes 1.5 s on the Charles in Python; the C++ F8 gate (< 1.0 s) is
unmeasured until the build.

---

## 4. Dye, as built

Three stroke kinds (7 DyePin with RGBA after its points -- the record is 24 +
8n, an old reader skips it by stride; 8 DyeKnob, the half-distance in `width`,
at most one; 9 DyeMouth, a one-point mark on a river), managed like ZeroFlow
(`setBodyDyeMouth`, `bodyDyeMouth`, `dyeHalfDistance`, `setDyeHalfDistance`,
`hasDye`). A fourth plane, `uint32 = source | weight << 16`, at the flow
plane's rate, packed by the same packer at 4 bytes a sample, referenced from
the version-3 header's reserved word at 0xF4 under `LODL_SECT_DYE = 1 << 8` --
the version stays 3, no offset moves, the container is self-describing so one
word suffices, and past 4 GB the save refuses by name. **Written only while a
dye mark exists**, so P0 and P3 stay byte-identical. `LodtFile` reads it
(`dyeWordAt`, `dyePlaneSamples`, `dyePlaneOffset`) and refuses a set bit with
an empty word.

`solveDye`: a DyePin's plume rides its body's own field (a body with no other
mark is solved from its contacts for it); a DyeMouth finds the receiver from
the table's `outlet`, the mouth as the receiver's texels within 2.5 texels of
the river, and gives the receiver a DYE-ONLY field cut round the mouth (margin
4 L + 16) -- the mouth as the source, the cut edges as the far field, a lake
gets seepage instead -- whose flow words and record are NOT written, so an
unstroked sea keeps its zero. The higher weight wins where plumes overlap.
Panel: Tool "Dye pin" (one click, the Dye colour, strength 1, the Width row as
its radius), rows "Dye colour" and "Dye fade" (the knob, read out of the file
on open), tick "Dye at mouth", Show "Dye" (the source's colour -- a body's hash
or the pin's own -- blended by weight). Ice in winter from the shore plane is
reader-side: section 6's checklist, not this lane.

---

## 5. Gates: what has run and what has not

* Run: the numpy prototype (section 2), the disc measurement (section 1).
* NOT run: everything in C++. `Fallout4.exe` was down and NifSkope was not
  running, but `scratchpad/water4_20260910/GO` did not exist at the one check
  the brief allows, so the build is PENDING: `scratchpad/water4_20260910/PENDING.md`
  is the paste-able resume (`nifskope-ww-resume-pending`), with the two gates
  the prototype predicts red named in advance.
* `tests/spells/water_flow.sh` (new) reads every F gate back by name from the
  marking tool's own selftest, floors the count at 18 green, and runs the
  independent decoder on the marked file the selftest leaves aside.
* `water_mark.sh`'s model half now runs ~30 more checks (the flow gates first,
  before any body of the file is touched; F5/F8 after the river stroke; the
  dye before section 7, so P8 round-trips a DYED file and P3 undoes it).

---

## 6. Pictures

* `scratchpad/water4_20260910/images/charles_flow_proto_pair.png` -- the
  Charles (body 3), the flow DIRECTION per texel over the body's own bounding
  box at 2x, hue = direction: LEFT the disc fill read out of
  `charles_marked.lodl` (39 patches, p99 40.78, seams 2.97 %), RIGHT the
  potential-flow solve on the same mask with the same stroke (0 patches, p99
  8.44, seams 0.29 %, brightness = speed; the narrows brighten, the reach turns
  continuously, the water parts round the lower island). Opened and read. It
  is a texel picture (ww-texel-picture), not the render-hook pair at WATER2's
  framing -- that pair, the dye plane at the mouth and the synthetic channel
  with its speed all need the exe and are steps 3.1-3.3 of `PENDING.md`.

---

## 7. Mistakes (also spliced into `MISTAKES.md`, newest first)

1. **"Half-distance" coded as an e-fold** -- F7 went red at 0.368 and said so;
   fixed to `0.5^(s/L)`; the name of a knob is its contract.
2. **A tracer seeded on dry land** -- F4 read 37 of 40 because three seeds
   started outside the cell they had tested; found by tracing each failure
   step by step; the instrument, not the flow.
3. **Six vexing parses, again** -- the entry lane WATER2 already wrote;
   `vector<T> v( n )` declares a function; repeating an entry is its own entry.
4. **A heredoc ate a 120-line script** -- the fourth time this tree has paid
   for a heredoc; every file goes through the Write tool.
5. Not in the ledger because it was caught before it shipped: the first C++
   draft balanced the solver's sources GLOBALLY; the prototype's NaN on the
   Charles (ten pieces, isolated texels) found it before the build could.

---

## 8. Skill review (CONSTITUTION 1a)

**Loaded and used.** `ww-control-calibration` -- the numpy prototype is its
known-answer control: every gate's number was seen reachable by the method
before the C++ existed, and the two that are not (F2 island bank, F5 p99) are
named as method limits with their cause measured. `ww-texel-picture` -- the
pair's crop, 2x nearest, and the caption arithmetic from the same instrument
that reports. `ww-contract-provenance` -- every document edit is a script with
an anchor that must match once and a CR count that must not move, and every
one says BUILD PENDING rather than a line number the build may move.
`nifskope-ww-resume-pending` -- the shape of `PENDING.md`.
`ww-lodl-offline-census` -- `lodl_np.py` wraps WATER2's decoder rather than
writing a third reader. `nifskope-ww-panel-style` -- rows only, every helper,
the one-row list extended. `nifskope-ww-render-shot`, `nifskope-ww-build-verify`
-- read, deferred to the resume.

**Wished for.** A `ww-anchored-splice` procedure: this lane's eight patch
scripts share `splice.py` (anchor matches exactly once, CR count unchanged,
replace / before / after), and lanes WATER2, WATER3 and BUILD5b each re-wrote
the same function. Twelve lines, recurring every lane -- recommended for the
director to lift `scratchpad/water4_20260910/splice.py` into `tools/` and name
it in `nifskope-ww-lodgen`'s editing-trap section, rather than a skill of its
own. Declined as a skill: "prototype a solver in numpy before the C++" -- one
sentence, and its value was specific to a method nobody had run.

---

## 9. Housekeeping

Nothing committed. Files changed, LF-only throughout (CR 0 before and after,
by Python byte count): `src/watermark.h` 15,060 -> 21,377 bytes,
`src/watermark.cpp` 82,104 -> 151,755, `src/watermarkpanel.cpp` 47,081 ->
51,314, `src/lodtfile.h` 20,346 -> 21,491, `src/lodtfile.cpp` 140,908 ->
141,680, `docs/LODGEN_BTD_FORMAT.md` 72,264 -> 75,132,
`scratchpad/specs_20260909/spec_water.md` 46,118 -> 52,789, `MISTAKES.md`
125,422 -> 128,636; `WW_CHANGES.md` 1,454,199 -> 1,457,729 with CR 19,020 ->
19,020 (an LF-only entry at the top); NEW `tests/spells/water_flow.sh` (114
lines), `scratchpad/water4_20260910/` (1.4 MB: the scripts, the `new_*.cpp` /
`patch_*.py` sources of every edit, two `.before` copies, one PNG,
`PENDING.md`). `NifSkope.pro`, `src/nifcli.cpp`, `src/btdterrain.*` untouched.
`g++ -fsyntax-only` with the real `Makefile.Release` flags: rc=0 on
`watermark.cpp`, `watermarkpanel.cpp`, `lodtfile.cpp`, `nifcli.cpp`.

To reproduce this lane's own measurements:

```
cd scratchpad/water4_20260910
python disc_metric.py 3 ../water3_20260910/work/charles_marked.lodl     # section 1
python flow_proto.py                                                     # section 2, the synthetic gates
python smooth_probe.py                                                   # section 2, the Charles and the marsh
python make_pair_proto.py                                                # the picture
```

---

## Build (BUILD10, 2026-09-10)

`release/NifSkope.exe` **15:52:46**, `release/style.qss` in step. `make -j2`
had **nothing to do**: an earlier build lane had already compiled this lane's
five changed sources into the running exe, and the consistency sweeps say so
rather than the exit code -- `watermark.o` 14:04:17, `watermarkpanel.o`
15:52:17, `lodtfile.o` 14:04:09, `nifcli.o` 14:34:24, `btdterrain.o` 14:03:56,
`lodvfile.o` / `lodgenmanager.o` all newer than `src/watermark.h` (04:33:22)
and `src/lodtfile.h` (04:10:41), and the exe newer than every one of them.
`Makefile.Release`'s dependency blocks name both changed headers for all seven
objects (the `awk` walk, not `grep -A`), so no hand patch and no qmake run was
owed. No new translation unit, so `NifSkope.pro` was not touched.

`tasklist` printed no `Fallout4.exe` and no `NifSkope.exe` before the build and
before every launch.

### The gates

| harness | result | notes |
|---|---|---|
| `water_flow.sh` (the Charles, `WW_WATER_MARK_BODY=3`) | **47 checks, 2 failures** | the two failures are EXACTLY the two pre-registered as expected red |
| -- F1 continuity / flux | green | ratio 2.0000 (460 it, 9.3e-10); flux deviation 1.63e-10 |
| -- F2 parts / mass / straight bank | green | 0.5000 + 0.5000; 3.05e-12; 3.93e-4 over 416 texels |
| -- **F2 island bank vs the analytic cylinder** | **RED as registered** | mean **12.01**, max **22.40** deg over 48 bank texels against 5 / 15 -- the prototype's number to two decimals |
| -- F3 / F4 | green | 0 exactly; 0 of 3,225 point away, mean cosine 0.914; 40 of 40 streamlines |
| -- F5 patches / seams / mean | green | **0 patches** (the disc fill had 39); seams 0.422 % (2.97); cos 1.000, R 0.794 |
| -- **F5 p99** | **RED as registered** | **8.44** deg against 5 (prototype predicted 8.4) |
| -- F6 / F7 | green | plume 98.2 vs 96 texels, 0.0 vs 0.0 deg; 0.5000 / 0.1250 / 0 |
| -- F8 cost | green in the log | **0.319 s**, 1,493 iterations, residual 9.26e-10 |
| -- the dye chain | green | 17,218 texels, source body 3, painted on body 1; 230.7 within L/2; 34 beyond 3 L; 8,649/8,649 read back; P8 byte-identical; P3 undo 0 bytes differ |
| -- the independent decoder | green | `disc_metric.py` on the saved file: 0 patches, p99 8.44, seams 0.422 % -- the same numbers the app printed |
| `water_mark.sh` | model **47 / 8**, dock **20 / 0 PASS** | the model half runs on **body 2**, not the Charles -- see below |
| `lodl_water.sh` | **RESULT PASS**, 33 `ok` lines | the writer did not change; it prints no count, so the count is mine |
| `lodl_open.sh` | **23 checks, 0 failures, PASS** | unmoved |

Skipped, with the reason: every harness that reads no `.lodl` and builds no
water panel (collision, block list, impostor, atlas, arrays, merge, VT,
render_shot) -- this lane changed `watermark.*`, `watermarkpanel.cpp` and the
`.lodl` reader only, and `render_shot.sh` was exercised in substance by the two
picture renders below.

### The six reds that are not the two registered ones

**Four of them are `water_mark.sh`'s model half measuring a different body.**
That run picks body **2** (the marsh, 29,312 texels) as "the river"; the F5
gates and the dye gates were registered on body **3** (the Charles). On body 2
the numbers are p99 9.84, seams 0.840 %, cos 0.763 -- close to this lane's own
held-out prediction for the marsh (11.3 / 1.0 % / R 0.53), so the method is
behaving as it was measured to behave, on a body whose gates were never
registered. Its four dye reds have ONE cause and the instrument states it
itself: *"body 2 and the body it drains into (19) do not touch, so its dye has
no mouth to leave by"* -- 0 receiving fields, therefore 0 dyed texels, therefore
four gates with nothing to measure. Not fixed here (rule: a verdict, not a
cure); the candidates for the director are (a) pin `WW_WATER_MARK_BODY=3` in
`water_mark.sh` as `water_flow.sh` already does, or (b) let the dye gates
report `n/a` by name when the receiving field is empty.

**Two are defects in `water_flow.sh` itself**, found by running it for the
first time (`nifskope-ww-resume-pending` section 9 predicts exactly this):

1. the gate loop does `grep -F "F8 the solve" | head -1`, and the harness prints
   an INFORMATIONAL line `F8 the solve: 0.319 s, ...` immediately BEFORE
   `  ok   F8 the solve of body 3 runs under 1.0 s ...`. The grep takes the
   informational line, sees no leading `  ok `, and calls a green gate red;
2. `flow gates green: 17 (floor 18)`. There are 19 `F[1-8]` gates and the lane
   pre-registered TWO of them as expected red, so 17 is the arithmetic the lane
   itself predicts. The floor and the prediction were registered inconsistently.

Neither was re-pinned and neither was repaired: both are one-line changes to
`tests/spells/water_flow.sh`, and the brief for this build says report, never
re-pin.

### The pictures (`scratchpad/water4_20260910/images/`)

| file | what it shows |
|---|---|
| `charles_flow_pair_v4.png` | the generator's single direction beside the potential-flow solve, ONE framing. The framing is proved, not asserted: the BEFORE render taken today is byte-identical to `water2_20260909/images/charles_flow.png` (30,905 bytes), and `make_pair_v4.py` refuses if it is not |
| `charles_dye_mouth.png` | the dye plane at the Charles's mouth at texel level, drawn from the file's bytes through WATER2's independent decoder (there is no `dye` plane key in `src/btdterrain.*` to render). Every dyed texel names body 3 as its source and lies on body 1 |
| `flow_channel_f1.png` | gate F1's synthetic channel with the speed as brightness: the narrows is visibly twice as bright, and the caption's ratio is re-derived by the script |

**The framing needed a correction that is worth writing down.** WATER3's
`make_pair.py` docstring says the framing is "1500x1000". It is not: the render
hook honours `WW_RENDER_SIZE`'s WIDTH exactly and takes 59 px of window chrome
off the HEIGHT, so `1500x1000` renders **1500x941** and does not reproduce the
baseline. The baseline is 1507x941, i.e. `WW_RENDER_SIZE=1507x1000`; with that,
the BEFORE render is byte-identical. Three probe renders found it.

### What is still owed

* the two registered reds are the METHOD's, measured, and stand as reported;
* the two `water_flow.sh` defects and `water_mark.sh`'s body choice are the
  director's to route;
* nothing was committed (CONSTITUTION 8).
