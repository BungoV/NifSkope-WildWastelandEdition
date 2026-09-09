---
name: ww-control-calibration
description: Build the floor/ceiling control pair for any "is our output as good as vanilla's" measurement in the NifSkope Wild Wasteland tree — how to construct a floor that carries the signal's own amplitude and spectrum through the same lossy pipeline, an independent phase-randomised twin beside it, a ceiling from the same data with the property removed, and the known-answer inputs that prove the metric works before any verdict is reported. Use before quoting any number that compares a generated texture, mesh channel or sheet against a shipped Bethesda one.
---

# Calibrating a measurement against vanilla

CONSTITUTION rule 4 says: use an invariant that fails on broken code, and show
it failing. This is that rule as a procedure, for the case that keeps recurring
here — a scalar computed on our sheet and on vanilla's, where the difference is
supposed to mean something.

The failure this prevents was measured on 2026-09-09 (lane MSN): the obvious
positive control — our own generated sheet, correct by construction — separated
from its own deliberately broken copy by **1.0x**. Any verdict read off that
comparison would have been noise.

## The five parts, in order

**1. Run the metric on inputs whose answer you already know.** An exact
analytic case that must read ~0, and a random case that must read ~1 (or
whatever the theory says). Print both above every real number. A metric that
cannot fail on its input is not a metric. Lane MSN: an exact gradient read
0.0000 and white noise 0.498, against a theory of 0 and 0.5.

**2. The floor must carry the SIGNAL'S OWN amplitude and spectrum through the
SAME lossy pipeline.** A floor built from a clean synthetic field, or from our
own output when our output has no energy in the band being asked about, measures
the pipeline and not the question. Build it by taking the subject's own field,
removing the property under test in float (a projection, a filter, a
re-derivation), then putting it back through every lossy stage the subject went
through: quantisation, block compression, mip filtering. Decompose and print the
floor stage by stage — `float 0.0013 -> 8-bit 0.0014 -> block codec 0.0984`
tells you instantly which stage owns it.

**3. Match on the quantity the metric reads, never on a proxy.** Byte energy,
file size and pixel variance all saturate or scale differently from the thing
being differentiated. Print the matched quantity beside every control so the
match is visible, not asserted. (Lane MSN matched on encoded byte energy first
and got a control 26% rougher than its subject, which halved the apparent
effect.)

**4. A paired floor built from the subject's own data needs an independent twin.**
Projecting vanilla's field and then measuring it with the same operator that
projected it invites the charge of self-service. Build a second floor with the
same amplitude **spectrum** and **random phase** — `abs(fft2(x)) * (fft2(noise)
/ abs(fft2(noise)))`, inverse-transformed — then apply the same construction.
Report both. If they disagree by more than the effect, the effect is not
established: say so.

**5. The ceiling comes from the same data with the property removed, not from a
different dataset.** Then `ceiling / floor` measures the TEST's discriminating
power on this data, and it is pre-registered in the brief as a gate. Under it,
no verdict. Lane MSN's gate was 5x; the Helmholtz metric gave 8.1–9.6x and the
finite-difference metric 3.7–4.0x, so only the first was allowed to speak.

## Emulating a lossy stage

If the floor needs a compressor we do not have, emulate it and then **check the
emulation on the subject's own bytes before any control uses it**: re-encode the
shipped file and report how far it moves the metric. Lane MSN's 4x4 block codec
(principal-axis endpoints, RGB565, four-entry palette, nearest index) added
1.6–2.1 bytes rms to vanilla and moved the reading under 1% — which is also the
bound on how wrong the floor could be.

## Reporting

* Every number carries its tile/chunk, size and mip.
* Floor, ceiling, separation and the subject's position between them, per tile.
* A band or region breakdown when the suspicion is "this is just the codec": a
  codec's error lives at one scale, content does not.
* The refuter for each claim, and for any refuter you actually tested, its
  number. A tested-and-defeated refuter is worth more than three untested ones.
* When two floors disagree, quote the conservative one in the headline.
