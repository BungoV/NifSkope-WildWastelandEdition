---
name: ww-explained-variance-ceiling
description: Decide whether a correlation you just found is strong enough to GENERATE the thing it correlates with, before any of it reaches a default. Covers the ceiling regression (the richest honest basis, fitted as an upper bound on every law of that family), the phase-twin floor and the known-answer control that must run first, reading a small R^2 as amplitude rather than as a percentage, choosing between two candidate drivers on the same residual with the same floor, and the envelope-versus-sign distinction that decides whether a term can be written into a colour at all. Use whenever a measured relationship is about to become a shipped coefficient, and whenever a brief says "if X holds, ship X".
---

# The ceiling: can this correlation actually GENERATE the target?

Written from NifSkope WW lane TILING3 (2026-09-11), which confirmed a hypothesis on
three independent instruments, nearly shipped it as a generator, and found on the
fourth measurement that no law of that family could account for more than **2 %** of
what it was supposed to produce.

The failure this prevents: **"X correlates with Y" and "X can generate Y" are
different claims, measured by different instruments.** A brief that steps from the
first to the second is stating a plan, not a result.

## 0. The order, and it is not negotiable

1. **Known answer first.** Inject a synthetic target built from your own predictor at
   a known strength, and require the fit to recover that strength before you read any
   real number off the instrument. A fit that cannot recover 0.5 as 0.5000 is not
   evidence of anything.
2. **Floor second.** Every predictor gets a phase twin -- same power spectrum, same
   histogram, structure destroyed. A correlation is real only if it beats its twin,
   and the twin must be reported next to every reading, not once at the top.
3. **Ceiling third, BEFORE any coefficient is chosen.**
4. Only then a coefficient, and only then a default.

Steps 1 and 2 tell you the relationship exists. They say nothing about whether you
can build with it. Step 3 is the one lanes skip.

## 1. The ceiling regression

Build the richest basis you can honestly justify from the SAME source, and fit the
target on all of it at once. Its R^2 is an upper bound on every law of that family,
including the clever nonlinear one you have not thought of yet.

TILING3's was 22 columns from one normal map: the three components, slope magnitude,
`nz^2`, the divergence, and the source's own high-pass at eight blur radii. The
ceiling came back at **R^2 0.018 / 0.023**.

Rules that keep it honest:

- Every column must come from data the SHIPPING code will actually have. A basis
  containing something only the analysis can see measures a fantasy.
- Fit the ceiling on more than one sample and report the spread. One sheet, one
  frame, one capture is an anecdote with a decimal point.
- The ceiling is an upper bound, not an achievement: you are proving that even
  overfitting cannot do better, so do not defend it against overfitting.

## 2. Read a small R^2 as AMPLITUDE, never as a percentage

"R^2 = 0.018" invites "1.8 %, so nearly nothing" or "1.8 %, but it is real!"
depending on what the reader wants. Convert it, always:

```
explained SD  = sqrt(R^2)      * SD(target)
remaining SD  = sqrt(1 - R^2)  * SD(target)
```

TILING3: of a residual with SD 4.476, the explainable part is SD 0.6 and the
remainder SD 4.4. Then check the remainder's moments against the whole's -- if they
match (skew +1.08 vs +1.01, kurtosis 6.4 vs 7.4), the fit removed a sliver and left
the phenomenon untouched, and the report must say so in those words.

## 3. Two candidate drivers: same target, same floor, same sheets

When someone proposes a better-motivated driver -- including when the owner does --
do not swap it in on the motivation. Put both through the identical fit on the
identical residual with the identical twin floor, on the identical sample, and print:
median r, median twin, sign agreement across samples, and how often each beats the
other. Then keep the better one and say which.

Sign agreement is the underrated column. TILING3's shipped term agreed with its own
median sign on **7 of 7** samples; the proposed replacement managed **4 of 7**, which
is chance, while its |r| was 27x smaller. Either number alone could be argued with;
together they are not arguable.

Also test the proposal's own algebra for a hiding place: if the new driver is a
monotone remap of a column you already fitted to zero (`acos(x)` of a `dUp` that read
null), say so and print both forms side by side, so nobody re-derives it in a month.

## 4. Envelope versus sign: can the term be WRITTEN at all?

A driver can predict WHERE the target's detail lives without predicting its SIGN.
Test both, because they license different code:

- **signed** -- `corr(driver, residual)`. Licenses adding a level: `out += k * driver`.
- **envelope** -- `corr(blur(|driver|), blur(|residual|))`. Licenses modulating an
  amplitude, and nothing else.

TILING3's rejected driver read 0.0039 signed and **+0.1269** as an envelope, beating
its twin 7 of 7. That is a real finding and it still could not be shipped as written,
because a colour writer needs a sign. Record it as the open lead with the number
attached, and do not let an envelope result be quoted as if it were a signed one.

## 5. What the report owes

- The ceiling, in amplitude as well as R^2.
- Every reading beside its twin.
- The known answer, with the value it recovered.
- The losing candidate's numbers in full -- a refutation printed in the log is worth
  more than the winner, because it stops the next lane refitting it.
- If a coefficient ships anyway on a small R^2 (TILING3 shipped one recovering ~1 %),
  say **what it is for**: a direction that is right and an amplitude that is small is
  worth shipping only when the alternative is nothing, and the report must not let it
  be read as the fix.

## What this does NOT do

It does not tell you the source is wrong. TILING3's hypothesis was CORRECT about
origin -- the target really does derive from the source -- and still could not be
computed from it, because the source we can read is a downsampled shadow of the one
the original author had. "Confirmed as the origin, refused as a generator" is a
legitimate and useful verdict, and it is the one this instrument exists to reach.
