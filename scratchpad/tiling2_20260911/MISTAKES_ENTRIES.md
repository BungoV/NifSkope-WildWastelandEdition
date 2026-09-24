## 2026-09-11 — TILING2 — a colour change that reached no file, because the colour sheet has two writers and only one of them ships

**What was done.** The quadrant cross-fade was implemented in the per-chunk
colour composite in `src/lodgen.cpp` — the obvious place: it is where the
seven-step colour law reads the 17x17 layer opacities and lerps the layers. It
built, it linked, the bake ran, and the chunk colour DDS came out
**byte-identical to the bake without the flag**. The first reaction was to
assume the margin was too small, and `--blend-margin` was raised to 1024 world
units — a third of a cell. Still byte-identical.

**What was true instead.** With `--vt` on, the chunk's colour sheet is
**assembled inside the pyramid pass** from the level-D/2 tiles
(`docs/LODGEN_TERRAIN_VT.md` §2.4) and the stock per-chunk composite is never
reached. Every sheet on disk in this lane, and every sheet a region bake writes,
comes from the pyramid's own copy of the colour law. The cross-fade had been
written into the path that produces nothing.

**How it was found.** By baking with `--tex-dir` and NO `--vt`: the run
succeeds, reports chunks written, and writes **zero** texture files. That is
also the proof that `--tex-dir` alone is inert (`--cover` is required as well).
One relink put the cross-fade, colour-only, into the pyramid path as well, and
the sheet changed on the next bake.

**The lesson.** *The colour law is implemented TWICE in `lodgen.cpp` — the stock
chunk composite and the pyramid tile composite — and the one that writes the
files bungo looks at is the PYRAMID. A colour change made at one site only
silently does nothing. Change both, and gate it by a byte compare that is shown
able to fail.* When a flag produces a byte-identical artefact, the first
hypothesis must be "the code I changed does not run", not "my parameter is too
small": raising the parameter cost a whole bake cycle and proved nothing.

## 2026-09-11 — TILING2 — a knob whose default cancelled its own switch

**What was done.** `--land-sample average` was added with a companion knob
`--land-detail k` that lerps back k of the footprint sample's departure from the
repeat average. `g_landDetail` was initialised to **1.0f**, on the reasoning
that "no detail removed" is the conservative default.

**What was true instead.** k = 1 reproduces the footprint sample exactly, so
`--land-sample average` with the default knob is the same bake as no flag at
all. The switch looked dead for the second time in one lane, this time for a
different reason.

**How it was found.** The measured repeat of the `average` bake came back equal
to the rung's, 1.037, to three decimals — a number that cannot happen by
coincidence and is the signature of "the bytes did not move".

**The lesson.** *A "no change" default on a knob that scales a term can cancel
the switch the knob belongs to. State the default in the words of the switch it
serves — `average` must mean averaged — and check that the pair of them, at
their defaults, is the behaviour you named.*

## 2026-09-11 — TILING2 — the phase twin proposed as a floor for a periodicity, which it can never be

**What was done.** The brief pre-registered the phase twin (same amplitude
spectrum, randomised phase) as the floor for the tiling-visibility statistic and
for the spectrum comparison.

**What was true instead.** The phase twin preserves the power spectrum **bin for
bin** by construction, so the autocorrelation, every band power, and any peak
prominence read at a given frequency are **identical** on the twin and on the
subject. As a floor for a periodicity it is not conservative or lenient; it is
mathematically the same number. It IS the right floor for a STRUCTURE statistic
— a correlation against another field, which is what lane SPLAT1 used it for,
and what section 1c of this lane's report uses it for.

**How it was found.** By computing it before trusting it: the twin's visibility
at the repeat matched the sheet's to the printed precision on the first sheet
tried.

**The lesson.** *A null must destroy the thing being measured. Before adopting
one, name the statistic's order: a phase randomisation destroys phase alignment
(third-order and structural information) and destroys nothing second-order. For
"is there a repeat at this period", the null is a sweep of periods that are not
the repeat, on the same sheet, so the sheet's own spectrum is carried.*

## 2026-09-11 — TILING2 — three instrument designs discarded, and the one arithmetic error that survived two of them

**What was done.** The repeat was first measured by autocorrelation at the
10.667-texel lag, then by a peak-to-background ratio in the 2-D spectrum.

**What was true instead.** At a ten-texel lag a terrain sheet's autocorrelation
is dominated by its own hillside decay, and an off-peak baseline cannot separate
the decay from a peak: the null sweep read 0.20 where an injected repeat of
amplitude 8/255 read 0.06 — the instrument ranked a real repeat BELOW the noise.
The peak-to-background ratio then exploded to order 1e9 on a band-limited
synthetic whose background at the repeat's bin is numerically zero, and it is not
comparable between sheets of different contrast. The third design — the
amplitude in 8-bit luminance units, which is the unit of bungo's complaint —
held, but its first version was **1.5x too high**: a 2-D Hann window's frequency
kernel is [-1/4, 1/2, -1/4], so a 3x3 power sum holds 1.5 x 1.5 = 2.25x the
centre bin's power, and the square root of that is the 1.5.

**How it was found.** By injecting a cosine of known amplitude into a real sheet
and requiring the instrument to report the number injected (self-test A0), and
by notching the repeat's whole frequency family out and requiring the reading to
fall into the null floor. 16 checks, 0 failures, before any verdict was read.

**The lesson.** *Build the known-answer test first and in both directions —
inject a signal of known size, and remove the signal entirely — and calibrate
the window's own leakage before quoting an amplitude. A statistic that cannot
recover a planted answer cannot be used to refuse one.*

## 2026-09-11 — TILING2 — a sheet set chosen by eye, then replaced with a stated rule

**What was done.** Vanilla's "law" was first measured over a handful of sheets
picked because they looked representative.

**What was true instead.** A ceiling is only a ceiling over a population that
was defined before the measurement. The set became **the 22 shipped dim-4
sheets** that clear a stated trough rule (mip-3 SD >= 5.25, so sheets that are
almost entirely water or featureless flat are excluded by a number and not by an
opinion), and the rule was written into the report's section 0 before section 1
was run.

**How it was found.** By asking what the sentence "vanilla never does this"
would be quantified over, and finding that the answer was "the sheets I chose".

**The lesson.** *A ceiling needs a population, and the population needs a rule
that was written down before the numbers were read. Say the rule and the count
in the same sentence as the ceiling: "0 of 22", not "vanilla does not do this".*

## 2026-09-11 — TILING2 — a measurement log overwritten by a targeted re-run, leaving half a report unbacked

**What was done.** `t5_variants.py` writes `logs/t5_variants.txt` and
`t5_variants.json` under fixed names. It was re-run late in the lane on three
variants to answer one question, which overwrote the twelve-variant run the
report's sections 2 and 3 cite.

**What was true instead.** For a while the report quoted numbers whose evidence
was no longer on disk — the numbers were right, and there was no way for a
reader to check them.

**How it was found.** By reading the log back while assembling section 2, rather
than trusting the memory of having run it.

**The lesson.** *A script that writes a fixed log filename must be re-run over
the WHOLE set, never a subset, or it must name its output per run. Before citing
a number in a report, open the log it came from and confirm the row is still
there.*
