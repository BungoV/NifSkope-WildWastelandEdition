---
name: ww-population-refuter
description: Turn "this change must not have touched X" into a measurement, by classifying the WHOLE domain by what the code under test can see of each unit and comparing the provably-unchanged population against the changed one in the same run. Covers conservative classification (union over sample positions, worst-case reference value), reading the noise floor off the unchanged group instead of assuming one, what a lossy container (BC1, mip chains) does to a texel that did not change, and how to tell a broken classifier from a real leak. Use when a single hand-picked refuter rectangle fails, or before quoting "unchanged" about a compressed output.
---

# WW: the population refuter — measure the floor, do not assume it

Lane SLAB1 (2026-09-18) had a pre-registered gate of the form "ground beside an
occluder that reaches the ground must not brighten by more than 2 of 255". The
one hand-picked rectangle brightened by 24.1, and the honest answer took a
population, not an argument.

## 1. One rectangle is an anecdote; the domain is the measurement

A single refuter rectangle can only fail in two ways, and from the number alone
you cannot tell them apart: the code leaked, or **the rectangle was not what you
thought it was**. Classify every unit of the domain instead — every 128-unit
square of the chunk, every vertex, every block — into:

* **provably unchanged**: the code under test cannot see anything here, so the
  output must be identical by construction;
* **provably in the old branch**: the new path is ALGEBRAICALLY the old one for
  these units;
* **fed by the new branch**: everything else.

Then print the mean and the worst move of each group. Three numbers, one run.

## 2. Classify CONSERVATIVELY, or you will measure your own classifier

SLAB1's first classifier sampled each square's CENTRE against the MEAN terrain
of the square, and reported "nothing visible" squares moving by up to 16.8 —
which would have been a genuine defect if the classification had been right. It
was not: the code under test samples many positions inside a square, and it
compares against each texel's OWN ground height.

The rule: **a unit belongs to the "provably unchanged" group only if it is
unchanged for every sample the code could take inside it.**

* take the UNION over a grid of sample positions inside the unit (SLAB1 used 16
  positions × 8 directions × 7 steps, and restricted "visited" to the squares
  the march actually reaches, not a bounding ring);
* use the WORST-CASE reference value — the lowest of the four land nodes, the
  earliest time, the smallest threshold — so the claim holds for every texel in
  the unit;
* when the group is small, say so. SLAB1's pure groups were 30 and 131 squares
  of 16,384, and that smallness is itself a finding worth reporting.

**Symptom to recognise: the "cannot have changed" group is moving. Suspect the
CLASSIFIER before the code under test.**

## 3. Read the noise floor off the unchanged group

A lossy container makes "unchanged" a statistical claim. BC1 encodes 4×4 blocks
with two endpoints: a texel whose own value did not change still moves when its
block's NEIGHBOURS change, because the block is re-fitted. SLAB1 measured that
floor at up to **2.488 of 255** on texels that provably could not have changed —
and the pre-registered bar had been 2, chosen by eye before anyone measured.

So:

* the floor is **the worst move in the provably-unchanged group**, measured in
  the same run, on the same file, with the same encoder;
* the result to report is the COMPARISON: SLAB1's wall-only group moved mean
  +0.153 / worst 1.559 against the unchanged group's +0.387 / 2.488 — **the
  supposedly-untouched branch moved LESS than the provably-untouched one**,
  which is a stronger statement than any absolute bar;
* the same floor then belongs in the shipped gate's comment, with the date and
  the number, so the next lane does not re-derive it (see `ww-test-harness-add`
  §5c for the framebuffer version of the same rule).

Corollary: **a mean over many texels is a result; a single texel of a
5-bit channel is one of 32 steps and is not.** Say which one you are quoting.

## 4. The signal is the control's floor

A control that reads "did not move" is green under a broken build too — a build
that ignored the flag entirely writes the old file and every "did not move"
check passes. Its floor is the SIGNAL measured in the same run: SLAB1's
ceiling-fed population moved +24.166, which the same 2.0 bar refuses twelve
times over. Print the pair.

## 5. When you re-implement the law to predict the population

An independent re-implementation is the right instrument (`ww-clearance-instrument`
§6), with one trap: it will not be bit-exact. Different summation order and
different precision agree to six decimals and differ in the seventh, which is
enough to make an `==` comparison lie. Either compare with a tolerance you
justify, or reproduce the ORDER and PRECISION of the arithmetic exactly — for a
claim of bit-exactness, the second, and it is worth writing out the loop.
