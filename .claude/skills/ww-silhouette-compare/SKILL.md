---
name: ww-silhouette-compare
description: Measure whether a GENERATED silhouette matches the SOURCE's — an impostor card frame against its mesh, a LOD chunk against its full model, a baked sheet's coverage against the render it came from — and get a number that is about the generator rather than about the viewport, the threshold or the resolution. Covers refusing a clipped mask, taking the reference from the generator's own finer pass instead of a render, separating a THRESHOLD question from a DOWNSAMPLE question by running the comparison inside the source alone, the two floors that belong under every such measurement, and the rule that a control is built in the domain of the number it floors. Use before quoting any "our silhouette is N% wider/narrower than the source" figure in the NifSkope Wild Wasteland tree.
---

# WW: comparing a generated silhouette with its source

Three lanes in a row re-derived this and two got it wrong in ways that cost a
round.

* **CARDFIT3 (2026-09-09)** carried a "fills the long axis within 2%" bar from a
  brief into a gate and it failed at 89.3% — because pass one measures in
  VIEWPORT pixels and pass two downsamples into a 64-texel frame, which is a
  resolution step, not a defect.
* **CARDORTHO (2026-09-10)** reported "the HEIGHT matches to 0.00% on all three
  trees" off masks that were both the full 941-row viewport, and "the WIDTHS run
  18–121% wide" off a band statistic taken across a resolution step. Neither
  number was about the bake.
* **CARDWIDTH (2026-09-10)** built its first extent floor by resampling a
  128-texel frame to 105% and re-thresholding; the card drawn 5% WIDER measured
  0.93% NARROWER, and the floor did not fire.

The bake in all three cases was within one texel of right.

## 1. Refuse a clipped mask before you measure it

A mask that touches the viewport edge carries the VIEWPORT's number, not the
object's, and two clipped masks agree perfectly. This is not covered by the usual
"an empty mask and a full-frame mask are both refused" floor: a clipped mask is
neither.

```python
def touches_edge(m):
    return bool(m[0, :].any() or m[-1, :].any() or m[:, 0].any() or m[:, -1].any())
```

Run it on EVERY arm of EVERY row, print the table before any width is compared,
and refuse rather than measure. And size the camera from the artefact's own
extents so it cannot happen: `WW_RENDER_ORTHO` sets the half-WIDTH, so on a
1507x941 viewport a card of half-height `H` needs `1.06 * max(halfW, halfH*W/H)`,
not `1.05 * halfW`. Probe once, read `vp` back out of the census, size, shoot
again — never assume the size the request asked for.

## 2. Take the reference from the generator's own finer pass, not from a render

A generator that measures a silhouette before it downsamples usually WRITES that
measurement. In the impostor bake it is the sidecar's
`framefit <maxDx> <maxDy> <unionDx> <unionDy>` — the widest and tallest single
view's half-box in world units, read off the 941-px matte, i.e. ~7.4x the card's
own resolution.

Comparing the artefact against that number needs no render, so it carries none of
a render's confounds — no clipping, no perspective, no alpha-tolerance on the
background — and it can be run while the game is up. Use it for the DOWNSAMPLE
question (does the crop land where it should).

It is CIRCULAR for the THRESHOLD question, because that reference was itself read
at the generator's own threshold. Section 3 is how to answer that one.

## 3. Separate a THRESHOLD question from a DOWNSAMPLE question inside the source

To decide which alpha threshold reproduces the source, put the whole comparison
inside ONE source render, with the generated artefact taken out of it entirely:

1. take the fine mask of the source at full render resolution;
2. box-filter it to the artefact's own texel pitch — this is exactly what a
   `SmoothTransformation` downsample computes, a coverage FRACTION per texel;
3. read the coarse mask back at each candidate threshold;
4. the error against the FINE mask's own box is the threshold's error and
   nothing else's.

Whatever this shows cannot be the generator's fault, because the generator is not
in it. In the card round it gave: conservative `>= 16/255` worst **0.63 texels**
over twelve half-extents, majority `>= 0.5` worst **2.07**.

The same construction answers "is our output N% wider": run the BAND statistic
the report quotes between the fine mask and the coarse mask. In the card round a
crown band read **+36% to +65%** with no card in the comparison, which is most of
the 18–121% that had been laid at the bake's door.

## 4. Two floors, always, under the pipeline itself

1. **Pitch 1.0 must return exactly 0.00** on every row and every threshold. If
   the box-filter-and-threshold path adds anything at pitch 1, every number above
   it is that instead.
2. **A known-answer shape.** A disc of stated radius or a rectangle of stated
   half-width through the same downsample must come back at its own size, and the
   residual it shows (±1 texel from box quantisation) is the instrument's noise
   floor — quote it beside any effect you claim.

A metric whose own error is comparable to the effect is not a metric.

## 5. A control is built in the DOMAIN of the number it floors

The extent floor for a gate measured on a composited pixel canvas belongs on that
canvas — 5% of a 1435-px card is 72 px against 1 px of bbox quantisation.
Applying the same 5% by resampling a 128-texel frame and re-thresholding moves
the boundary by up to a texel, which is larger than the 5%, and the control reads
backwards.

And state the floor as a RESPONSE, not as an absolute failure: "the extent column
moves by at least half the amount applied, on every row". A wide card on a row
where the artefact is already too narrow moves TOWARDS the source and passes an
absolute bar for the right reason. (CARDORTHO's control moved the dx column by
0.00 points on 12 of 12 rows — that is what a floorless bar looks like.)

## 6. Model the generator's own code, and run it BOTH ways

When a candidate is "does step X move the silhouette", re-implement step X
offline and run it in the configuration that MUST move it as well as the one
under test. `lodgenDilateFrames` keeps the alpha when `img == coverage` and
writes it otherwise: running only the first gives "0 texels differ", which is
indistinguishable from a model that never writes at all. Running both gives 0
against 838,783, and the 0 becomes a result.

## 7. What the answer usually is

In every card round so far the generator's geometry was right to under a texel
and the number in the report was one of:

* the viewport (a clipped mask),
* the resolution (a band statistic across a downsample),
* the threshold (the generator's coverage floor against the consumer's alpha
  test — different silhouettes, and the gap is the object changing size),
* the instrument (an upsample-then-threshold that invents coverage the sheet
  does not carry: threshold FIRST, then resample NEAREST).

Check those four before writing "the bake is wide".

## Worked example

`scratchpad/cardwidth_20260910/` — `recon.py` (§1), `measure.py` (§2, §4, §6),
`threshold.py` (§3 with both floors), `law.py` (a candidate law refused with
numbers), `transition2.py` (§1, §5 and the threshold-first composite), and
`scratchpad/lane_cardwidth_report.md` for how the four candidates were reported.
