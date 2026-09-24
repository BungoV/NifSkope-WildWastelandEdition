---
name: ww-analytic-fixture-gate
description: Prove that a pipeline's recorded NUMBERS describe the picture it actually produced — an impostor bake's extents, a camera's scale, an atlas's UV rect, a chunk's placement — by baking a fixture whose answer is arithmetic, pre-registering the WHOLE table before the build, measuring the fixture's own size through an independent code path, and shipping the broken behaviour behind a switch so it becomes the control. Use whenever a routine asserts a geometric property of a camera or a projection, whenever a helper NAMED after a property is the only thing checking it, and before pinning any "the output is metric" claim.
---

# The analytic-fixture gate

The shape of a whole class of defects in this tree: a pipeline measures the
world off pixels, writes the numbers into a format, and **nothing ever checks
that the numbers describe the picture**. Every internal check compares two
quantities that come from the same wrong place, so it passes.

The worst case so far: the impostor bake sized every frame with
`orthographicHalfHeight()` and drew through a 60-degree perspective frustum for
weeks (2026-09-10, `MISTAKES.md`). It even had a read-back beside the fit — and
the read-back compared `Dist / Zoom` against `Dist / Zoom`, which is true in
either projection.

## 1. A NAME IS NOT A MEASUREMENT

Before writing any gate, list the accessors the code uses to justify the
property, and ask of each: *would this return a different value if the property
were false?*

`orthographicHalfHeight()` returns `Dist / Zoom` whatever the projection is.
`isPerspectiveProjection()` is the one that answers. A check on the first is
decoration; a check on the second is a check.

Then go one better and check the PROPERTY itself, not the flag — a flag says
what the code intended, a span says what it drew (CONSTITUTION 4, telemetry
echoes truth).

## 2. Pick a fixture whose answer is arithmetic

Not the real asset. A tree's silhouette is not predictable; a **box** is.

* an axis-aligned box of half-extents `h`, seen with screen axes `r` and `u`,
  has orthographic half-extents `hx|rx| + hy|ry| + hz|rz|` and the same on `u`.
  That is the whole prediction, in one line, for every view;
* the app can write its own: `-no-gui new --cube --size 512` (a 512-unit cube
  centred at `(0,0,256)` — it spans z 0..512, NOT -256..256; that caught a lane
  once);
* derive the screen axes from the code's OWN camera law rather than guessing.
  For this tree, `setRotation( -90 + elev, 0, 90 - azim )` through
  `Matrix::fromEuler` with y = 0 gives `r = (sin azim, -cos azim, 0)` and
  `u = (sin elev cos azim, sin elev sin azim, cos elev)`, and the rows of that
  matrix are right / up / depth in that order.

## 3. Pre-register the TABLE, not the number

Write the prediction to a file under `scratchpad/<lane>_<date>/` BEFORE the
build, with the arithmetic that produced it (CONSTITUTION 1: a gate invented
after the numbers are in is not a gate).

**A single predicted number is a weak gate**: one wrong scale factor passes it
as easily as correct code. Choose the fixture's sampling so the prediction is a
LADDER. The 512-cube at OCT=4 predicts only two distinct spans (every view sits
on a symmetry plane of the octahedron); at OCT=8 it predicts seven, from 35.81
to 50.13 texels. Same cost class, a far stronger gate — check the spread before
settling on the grid.

State the tolerance and where it comes from, in the same file. "2 texels: the
crop's integer rounding plus one antialiased texel of the smooth downsample on
each side" is a tolerance; "±2" is a guess.

## 4. Measure the fixture's own size through a DIFFERENT code path

The prediction is `predicted = f(fixture size)`. If the fixture's size is read
back through the pipeline under test, the gate is circular.

Use a second instrument that has its own gate. Here: the render hook's pinned
orthographic camera (`WW_RENDER_ORTHO`, proven by `tests/spells/render_shot.sh`
section 7) measures the cube, and the bake is what is being tested. If no second
instrument exists, the fixture's size must be an INPUT the harness supplies and
that fact is stated in the report.

## 5. Ship the defect behind a switch, and make it the control

The old behaviour becomes an environment switch (`WW_IMPOSTOR_PERSP=1`), which
buys three things at once:

1. CONSTITUTION 10's exact way back for a change a user can see;
2. a control that must FAIL every check the fix passes — run it in the SAME
   harness, in the same run, on the same fixture;
3. proof the gate is sensitive at all. A gate with no red side measures nothing.

Beside the span check, add invariants the broken projection cannot satisfy:

* **central symmetry** — an orthographic projection of a centrally symmetric
  solid is centrally symmetric; a perspective one magnifies the near half.
  Measure the mask's disagreement with its own 180-degree rotation about its
  own box, as a fraction of covered texels;
* **near edge against far edge** — the widest row of a silhouette's top fifth
  against its bottom fifth. This is "no foreshortening" in its most direct form
  and needs no model of the object at all.

## 6. Make the output NAME the arm that served

A gate is a moment; a set of files outlives it. Have the writer record, read
back off the live object and not off what was asked for, which arm produced the
data — `projection ortho` / `projection persp` in the sidecar, carried into the
shipped format. Then:

* **absence has a defined meaning and the document says which.** Every sidecar
  written before the line was baked the broken way, so absent means "the older
  vintage", NOT "unknown" — write that down or a reader will default it to the
  good case;
* the field has to MOVE. Prove it in one artefact: the card-array gate writes
  the word on one of its two synthetic sets and omits it on the other, so one
  array carries both states (CONSTITUTION 4, rule 1 of 2026-09-04 21:33);
* the format version does not move if the key is optional and its absence is
  defined. Re-read the version constant LAST (`ww-contract-provenance`).

## 7. Report the cost, not just the fix

Say what the wrong arm did to the OUTPUT, quantitatively, per consequence —
scale, shape, and any channel whose encoding assumed the property (the bake's
height channel calls window z 0.5 the card plane, which is exact under ortho and
false under perspective). Then say plainly that every artefact produced before
the fix carries it and must be regenerated.
