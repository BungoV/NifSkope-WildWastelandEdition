---
name: ww-downsample-gate
description: Gate a COARSE generated artefact against a FINE one in the NifSkope Wild Wasteland tree — an aggregate card against the cards it replaces, a mip against its level 0, a chunk sheet against the pyramid it came from, an atlas layer against its source texture. Covers the threshold-free measure that makes the comparison about the generator (coverage MASS, not a thresholded mask), the CEILING arm that is the only thing able to say "the subject is worse than this resolution allows", the ceiling's own decomposition, and the two known answers that must be printed above every verdict. Use before quoting any "our coarse version matches the fine one to N percent" figure; `ww-silhouette-compare` owns the threshold question, this owns the resolution one.
---

# WW: gating a downsample

Written from lane CARDS-AGG (2026-09-11), where the first version of this gate
reported a ceiling of 0.507 with a metric that could not distinguish the bake
from the alpha test, and the second version found a real defect that made the
artefact carry **94 percent too much coverage** — a defect the subject, the
floor and the known answer were all blind to.

## The shape

```
known answer 1   the reference against ITSELF                      must read 0
known answer 2   the reference reduced to the subject's pitch by
                 the IDEAL operator, measured IN THE REFERENCE'S
                 OWN FRAME                                          must read ~0
CEILING          that same ideal reduction, carried through every
                 step of the real comparison                        the best a
                                                                    subject of
                                                                    this size
                                                                    could do
SUBJECT          the artefact that ships                            at or near
                                                                    the ceiling
FLOOR            the WRONG artefact -- a different cell, a
                 different tile, a different layer                   far outside
                                                                    the
                                                                    tolerance
```

A verdict is reportable only when the floor is outside the tolerance AND the
subject is at the ceiling. "Inside the tolerance" alone is not enough: a
tolerance is a guess, a ceiling is a measurement.

## 1. The measure must be threshold-free, or it is about the threshold

A coarse texel of a sparse subject — a canopy, a cut-out, a fence — is mostly
PARTIAL coverage. Thresholding it at the reader's alpha test and comparing masks
measures the test:

* lane CARDS-AGG's first run: IoU at 0.5 gave subject 0.442 against a ceiling of
  **0.507**. Half the silhouette is lost by thresholding alone, and nothing
  about the bake is visible in that number.

Use an integral instead. For coverage the right one is the **MASS**:

```
mass = sum(coverage) * (world area a sample stands for)
```

It is threshold-free, it is resolution-free (a box filter preserves it exactly,
which is known answer 2), and it is in world units, so two artefacts of
different texel pitches are directly comparable. Keep the thresholded number if
you like, printed as `FOR REFERENCE ONLY, and not the gate`, with the sentence
that says why.

Decode the coverage through the artefact's own contract before summing it. On a
card or aggregate sheet the alpha is ENCODED (floor/test/base), and reading the
raw byte as a fraction makes every partly covered texel about four times too
opaque.

## 2. Build the CEILING, and make it the arm that can fail

The ceiling is the reference reduced to the subject's own texel pitch by the
IDEAL operator for the quantity — a box filter for an area integral. It answers
the only question that matters about a downsample: *is the subject as good as
its own resolution allows?*

Without it you cannot tell a correct coarse artefact from a broken one, because
both look plausible beside a fine reference and both beat a wrong-artefact
floor. With it, lane CARDS-AGG's defect was one line of output:

```
CEILING  mass error 0.0138        SUBJECT  mass error 0.9449
```

Gate on `subject <= ceiling * k` (k around 1.25 for a per-texel error) as well
as on the absolute tolerance.

## 3. Decompose the ceiling, or its own error gets blamed on the subject

The ceiling arm usually has two stages: the ideal reduction, and whatever the
comparison itself does (a resample onto a common grid, a re-quantisation, a
re-encode). Measure the first ALONE, in the reference's own frame, and print
both:

```
the box filter in the frame          0.00013 mean
after the nearest-neighbour resample 0.0138  mean
```

Now a bar set at 0.02 that the ceiling fails is visibly a bar on the COMPARISON
and not on the artefact, and nobody moves a threshold to make a gate pass.

## 4. Compare in WORLD space, never in texels

Two artefacts of different frame sizes have different extents, because the frame
law grows the loose axis. Put both on one common grid through their OWN recorded
geometry (`half`, `frameOffset`, the cell's centre) and compare there. Comparing
texel for texel measures the frame law.

## 5. The floor is the WRONG artefact, and it must be the same KIND

A different cell, a different tile, a different layer — the same generator, the
same size, the same view, the same grid. A synthetic or empty floor measures the
pipeline. Print the floor's number beside the subject's and require a factor,
not just an inequality: lane CARDS-AGG required `floor > 2 * subject` and read
0.314 against 0.053.

## 6. What to print, in this order

1. the two known answers;
2. the ceiling, decomposed;
3. the subject;
4. the floor;
5. the reference's own magnitude (the mass, the area, the count), so a relative
   error can be turned back into an absolute one;
6. the WORST N rows by the subject's error, named, so a bad cell is not averaged
   away.

## Traps that each cost a round

* **An out-of-range warning from your own decoder is a defect, not noise.** The
  BC4 alpha lut is `(6-i)*a0 + (i+1)*a1` over 7 for the eight-value mode and
  `(4-i)*a0 + (i+1)*a1` over 5 for the six-value one. Getting it wrong by one
  produces values above 255 and numpy says so. A known-answer block (a0 = a1 =
  255 decodes to 255 everywhere) is two lines.
* **The reference must be finer than the SOURCE, not merely finer than the
  subject.** Lane CARDS-AGG's reference resolves the tree cards at 6.3 units a
  texel against ~23 units a texel inside the cards themselves, so it carries no
  loss of its own. State that ratio in the gate's own header.
* **A resample weights by the share of the TARGET's area a sample stands for.**
  Normalising by the NUMBER of samples that land in a target texel is the defect
  this skill exists for.
