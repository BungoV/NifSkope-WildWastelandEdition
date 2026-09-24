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

## A floor with a FIXED step cannot fire on a degenerate subject (lane NATIVE1b, 2026-09-11)

Part 1 above says run the metric on inputs whose answer you know. This is the
failure that survives that rule: the floor is built by perturbing the subject by
a FIXED amount, and on some subjects that amount perturbs nothing. The floor
then reads a clean zero and the run reports a green gate that proved nothing.

Two of them in one lane, both caught only because the floor's own number was
printed beside the verdict:

* **"narrow the cone by 5 degrees".** A cluster of one triangle has a normal
  cone of zero width, and five degrees tighter than zero is still zero. The
  floor read a deficit of **0.000000** — identical to the passing case.
  **The fix is to perturb RELATIVE to the subject's own measurement:** tighten
  the cone a thousandth past that cluster's OWN worst face, which excludes that
  face at every cone width including zero.
* **"grow the occluder box by 10 percent".** The fitter already shaves a whole
  voxel off every side, so a box inside a large object has room to grow and
  stays inside. The floor read `0 of 1 grown boxes leak`.
  **The fix is to perturb until it bites and REPORT the amount:** grow by 1.1,
  1.25, 1.5, 2.0 and record the first factor that leaks, failing only if
  doubling stays inside. The reported factor is itself a measurement — "worst
  factor needed 1.5" says how much margin the fit actually has.

**The test for a floor, before the run:** name a subject in this corpus for
which the perturbation changes nothing, and if you can name one, the floor is
relative-by-construction or it is not a floor. A floor whose printed number can
equal the passing case's number is not a floor, whatever its verdict says.

## A rank statistic with a dominant tie block is not reproducible (lane ROADS2, 2026-09-11)

AUC against a displaced floor is the workhorse measurement in this tree. It is a
RANK statistic, and that has a failure mode none of the parts above catch: the
ranking can be decided by the loop order rather than by the data.

Lane FLAGSCAN1 recorded a greyness AUC of **0.716** for the road family on chunk
(-8,8), to three decimals, and it became a gate in the next lane's brief.
Re-running FLAGSCAN1's own script unchanged gives **0.757**. The brightness half
of the same table reproduces exactly. The cause:

* saturation on that sheet takes **281 distinct values** over 262,144 texels,
  and **one of them covers 140,305 -- 53.5 percent of the tile**;
* the AUC was computed by ranking with `np.argsort`, which breaks ties by array
  index, and the index runs in raster order;
* so a spatially clustered mask gets a rank that depends on WHERE its texels sit
  inside the tie block. Break the ties at random and the answer is 0.731
  repeatably (three seeds: 0.7316 / 0.7308 / 0.7308).

Brightness has 1,499 distinct values and no dominant block, which is why that
column was stable and the grey one was not.

**The check, before quoting any rank statistic:**

> Count the distinct values of the score being ranked and the size of its
> largest tie block. If one value covers a large fraction of the sample, the
> ranking is not determined by the data, and the digits are noise.

**The fix** is tie-averaged ranks, which is the AUC's own definition when ties
exist -- a tie contributes half a comparison. There is no `scipy` on this
machine, so it is fifteen lines:

```python
def rankdata(s):                      # average ranks within each tie block
    order = np.argsort(s, kind='stable'); sv = s[order]
    r = np.empty(len(s), dtype=np.float64); n = len(s); i = 0
    while i < n:
        j = i
        while j + 1 < n and sv[j + 1] == sv[i]: j += 1
        r[order[i:j + 1]] = 0.5 * (i + 1 + j + 1); i = j + 1
    return r

def auc_tie(score, mask):
    s = score.ravel().astype(np.float64); y = mask.ravel(); r = rankdata(s)
    n1 = float(y.sum()); n0 = float(len(y) - n1)
    return (r[y].sum() - n1 * (n1 + 1) / 2.0) / (n1 * n0)
```

**Reporting rules that follow:**

* print BOTH columns -- the tie-averaged score and the score as the earlier
  lane's script wrote it -- so a number in an older document can still be found;
* gate on the tie-free column when one exists (brightness here), and say in the
  record which column the gate reads;
* quote a tie-dominated rank statistic to at most two decimals, and never as a
  threshold;
* prefer a CLEARANCE -- the family's score minus the top of its own displaced
  floors -- over the raw score, because the tie bias is common to both and
  largely cancels. Lane ROADS2's whole gate was read off clearance for exactly
  that reason: vanilla -0.009, before +0.314, after +0.001.


## A phase twin is a floor for STRUCTURE, never for a periodicity or a spectrum (lane TILING2, 2026-09-11)

The phase twin — same amplitude spectrum, randomised phase — is this tree's
default null, and it is the right one for a **structure** statistic: a
correlation of one field against another, where destroying the phase destroys
the alignment being claimed. Lane SPLAT1 used it correctly that way, and lane
TILING2's ten-candidate detail table rests on it.

It is **worthless** as a floor for any second-order statistic, and a brief asked
for it as one. The twin preserves the power spectrum **bin for bin** by
construction, so the autocorrelation, every band power, and any peak prominence
at a given frequency read **exactly the same** on the twin as on the subject.
The measurement cannot fail, which means it cannot pass either.

**Name the statistic's order before choosing the null:**

| statistic | what the null must destroy | the null |
|---|---|---|
| "is this field aligned with that one" (correlation, AUC of one mask against another field) | the alignment | the phase twin, or a displaced copy |
| "is there a repeat at period P" | the repeat AT P, and nothing else about the sheet | **a null sweep**: the same prominence statistic at a dozen periods that are not P, not the codec block (4) and not the layout grid, ON THE SAME SHEET |
| "is this spectrum like that one" | nothing available -- there is no null, so use a CEILING | the same distance measured between two real subjects that are both acceptable (TILING2: vanilla against vanilla on neighbouring tiles, 0.670) |

## A zero correlation proves nothing until an ALIGNMENT control reads high (lane TILING2, 2026-09-11)

Lane TILING2 refused a whole work item on a table of ten candidate sources that
all read |r| <= 0.006 against vanilla's fine detail. A table of zeros is also
exactly what a **misaligned** comparison reads, or a broken decoder, or an
off-by-one in the sampling grid — so on its own it is not evidence of absence,
it is evidence of nothing.

What made it a result was one more run: the **same model, the same code path,
against a field it is known to reproduce** — our own bake instead of vanilla's —
over a shift search. It read **r = +0.79 and +0.70 with the best shift at exactly
(0,0)**, with a row-flipped copy of the same field at 0.05. The instrument, the
model and the alignment are therefore all sound, and the zeros mean what they
say.

**Before reporting an absence, run the positive control through the identical
pipeline** — a subject the model DOES explain — and print its correlation and
its best shift beside the zeros. If the positive control does not read high, the
absence is a bug report about your own script.

## A floor that returns NaN is not a weak floor, it is NO floor (lane ROADS3, 2026-09-12)

Lane ROADS3 asked whether vanilla keeps any of the road diffuse's own detail.
The measurement was a correlation between the residual after the best wash and
our full-detail bake's departure from its flat average, and the floor was the
same signal TRANSLATED by a large random shift.

The signal is mask-shaped: it is zero everywhere off the road. Translate it and
it is zero everywhere ON the road, so it has no variance there and
`np.corrcoef` returns **NaN** for every draw. Printed into a table beside the
real number, NaN reads like a floor that was beaten. It is a floor that never
ran.

**Check every floor for NaN and for zero variance before you believe it**, and
print the floor's own standard deviation next to its value. When the signal is
confined to a mask, the honest floor keeps its amplitude and breaks only its
phase -- `splatlib.phase_twin(sig, seed=...)`, several seeds, report mean and
max. ROADS3's real numbers then came out as +0.0275 against a phase-twin floor
of 0.0270 mean / 0.0644 max, i.e. the correlation IS the floor, which is a
result. The NaN version of the same table was an empty claim.

## Added by lane HORIZON2 (2026-09-18)

**A control is a comparison at IDENTICAL settings, and a sweep script leaves
its artefacts at whichever value the sweep ended on.** This lane's Python
prediction was frozen by a script whose `cast()` default was `growth=1.5` while
the sweep in the same file had chosen 1.3; comparing the shipped exe to it gave
`mean 2.325 deg, max 35.048` and read as "the C++ does not implement what was
measured". Re-cast at the shipped configuration: **mean 0.244 deg, 97.7% of
bins within one stored step.** Either regenerate the reference artefact at the
shipped settings, or record the settings INSIDE the artefact so a mismatch
refuses instead of reporting a number.

## Set the bar BETWEEN two populations, and build the broken one first (lane GATEFIX2, 2026-09-19)

Everything above is about making a floor honest.  This is the failure one step
earlier: a bar set from the HEALTHY reading alone, with nobody having measured
what the defect reads.

`tests/spells/native_lighting.sh` had two such bars, both pinned from one run on
one untracked container.  Measured against the defects they exist to catch:

| state | darkest-fifth IoU | blockSD |
|---|---|---|
| the sheet is not read at all | 1.000 | 0.00 |
| the sheet's UP and NORTH channels swapped | 0.887 | 1.54 |
| **the healthy state** | **0.850** | **2.28** |
| the right normals in the wrong places (three seeds) | 0.821 / 0.822 / 0.821 | 1.00 / 1.07 / 1.04 |

The healthy IoU is INTERLEAVED with the broken values, so no threshold of that
shape exists at all.  The row had never been a gate, and its greenness had only
ever been a property of the artefact it was pinned to.

**Three broken states that need no rebuild.** The defect is usually described as
a code fault ("the shader read the model-space sheet as a tangent-space one"),
and emulating a code fault means building.  Realise it as an INPUT fixture
instead, from the subject's own bytes, and the whole calibration costs one
render each:

| the defect in words | the fixture | what it proves |
|---|---|---|
| the input is not read | an exact COPY of the subject's own cache | the statistic's value when the picture cannot move -- and, free, that the renderer is deterministic (our two frames were byte-identical) |
| the input's axes are wrong | exchange two channels of every texel, re-encoding the endpoints and preserving the block mode | catches a channel-order defect, which in a normal sheet is measured rather than declared and is therefore live |
| right values, wrong places | permute the codec's own blocks within each tile, fixed seed | the phase twin with the amplitude, histogram and codec matched exactly (part 2 above), and the only one of the three that a UV or indexing bug looks like |

Run several seeds for the shuffle and quote the WORST one in the margin.

**Then prefer a bar that cannot go stale.** Two shapes, in order of preference:

1. **A ratio against a floor computed in the same run.** "The container's own
   normals must make at least 1.60x as much block-scale shading as the SAME
   normals shuffled into the wrong places" needs no re-pinning when the
   container is re-baked, because both halves move together.  Give it a small
   absolute companion floor so a total collapse cannot satisfy it as 0/0.
2. **An expectation predicted from the INPUT, calibrated on a known-answer pair
   in the same run.** Looking straight down, N.L is the normal's up component;
   two synthetic arms with exactly known up (0.9988 and 0.8751) give the luma
   response on the same frames; the container's own mean up then predicts the
   frame's mean luma before it is measured. Healthy 0.09 luma from prediction,
   channel-swapped 31.03 -- a 300x separation, and no number in the row belongs
   to any particular container.

   State the lever arm and use the row ONE-SIDED. That calibration spans 0.1237
   of up; a deviation far outside it is a failure, but the size of the deviation
   is not itself a prediction and must not be quoted as one.

**The check, before writing any threshold into a gate:** name the defect the row
exists to catch, build it, and put its number in the same column as the healthy
one. If you cannot state both numbers, you are not setting a bar, you are
recording a measurement.
