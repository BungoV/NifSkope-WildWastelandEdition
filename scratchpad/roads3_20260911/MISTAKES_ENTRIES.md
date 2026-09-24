## 2026-09-12 -- ROADS3 -- the ledger was read and the ledgered mistake was made anyway, in the very next lane

**What was done.** Report section 1 was composed and appended with a shell
heredoc: `cat >> scratchpad/lane_roads3_report.md <<'REPORTEOF' … REPORTEOF`.
The prose carried ordinary apostrophes (*vanilla's*, *the brief's*). Bash
answered `-c: line 134: unexpected EOF while looking for matching '''` and
**nothing was written**. The report on disk still ended at section 0 while the
lane believed section 1 was delivered, which is the worse half of the failure:
an incremental-write rule was being followed in form and not in fact.

**What was already written down.** Lane ROADS2's MISTAKES entry 4, in this same
repo, from the day before: *"A `nifcli.cpp` patch with apostrophes in its
comment prose died on `unexpected EOF while looking for matching '''`. Every
source patch in the lane is now a file written with the Write tool."* That entry
was READ by this lane, at the start, as part of the required reading. It was
read and not obeyed.

**Why it happened.** The remedy was filed in memory as being about *source
patches* — the context ROADS2 hit it in — and report prose did not look like a
source patch. The trap is not about C++; it is about **any prose containing an
apostrophe passing through a shell**.

**The rule, stated so it cannot be scoped down again.** Prose never goes through
the shell. Any text with apostrophes — source comments, report sections,
changelog entries, skill files, handoff blocks — is written with the Write tool
to a file, and if it must be appended, a small Python script does the append.
The shell gets commands, never paragraphs. Both of this lane's source patches
(`patch_road_opacity.py`, `patch_usage_synopsis.py`) and both of its report
sections were written that way afterwards, and both went in first try.

**The second-order lesson, which is the point of a ledger:** reading MISTAKES.md
at the start of a lane is necessary and is not sufficient. An entry only changes
behaviour if its remedy is applied at the moment of the action, so a remedy
should be written as a rule about the ACTION ("prose never goes through the
shell"), not as a story about the file it first bit.

## 2026-09-12 -- ROADS3 -- a floor that returned NaN, printed in a table as though it had fired

**What was done.** `r3_fit.py` asked whether vanilla's road keeps any of the
road diffuse's own detail. The statistic was a correlation between the residual
after the best-fit wash and our full-detail bake's departure from its flat
average. The floor beside it was the same signal TRANSLATED by a large random
shift, five draws — the lane's standard floor shape, borrowed without thinking
about this particular signal.

**What was true instead.** That signal is **mask-shaped**: it is zero everywhere
off the road. Translate it and it is zero everywhere ON the road, so on the
texels the correlation is taken over it has no variance at all and
`np.corrcoef` returns **NaN**. All five draws returned NaN. In the printed
table that column read as a floor the measurement had cleared, when in truth
the floor had never run.

**How it was caught.** Only because the number looked too clean and the floor
column was inspected directly rather than trusted. Nothing in the script
complained: NaN propagates silently through `mean` and `max` in that code path.

**The remedy, and what replaced it.** `r3_chroma.py` builds the floor with
`splatlib.phase_twin(sig, seed=…)` instead — the signal's own amplitude kept,
its phase broken, so it stays supported on the mask and has real variance there.
Five seeds, mean and max both reported. The honest answer came out as
**+0.0275 against a phase-twin floor of 0.0270 / 0.0644** on (-20,20) and
**+0.0162 against 0.0124 / 0.0163** on (-8,8) — the correlation IS the floor,
which is a result, where the NaN table had been an empty claim.

**The rule.** A control is not a control until it is shown able to fire.
Before believing any floor: check it for NaN, check the field it is computed on
for zero variance, and print the floor's own spread beside its value. When the
signal under test is confined to a mask, a translation is the wrong floor by
construction — translate it and it leaves the mask. Break the phase, keep the
amplitude, stay on the mask. (CONSTITUTION rule 4; the skill
`ww-control-calibration` now carries this paragraph.)

## 2026-09-12 -- ROADS3 -- an accuracy figure written into a contract document before it was measured

**What was done.** Every candidate opacity was priced offline before the build,
by applying the generator's own composite to sheets that were already baked. The
report and the amendment to `docs/LODGEN_TERRAIN_VT.md` both described those
panels as *"right to about half a level, not to the byte"*. Half a level was a
guess at what 8-bit quantisation plus BC1 would cost. It was written in the
voice of a measurement, in a document whose whole point is that numbers carry
their provenance.

**What was true instead.** `r3_f3.py` later baked the same setting and compared
it with the simulation texel by texel. The simulation is right on the
AGGREGATES -- road mean luminance within **0.286** of a level on (-20,20) and
**0.221** on (-8,8) -- and is not right per texel at all: mean absolute
difference **1.583** levels, 99th percentile **7.341**, worst **15.279**
(1.527 / 5.745 / 13.802 downtown). The guess was 3x optimistic on the average
and 30x optimistic on the tail. It happened to be conservative about the
aggregate, which is the number the gate table was read on -- so the conclusions
survived, and that is luck, not method.

**How it was caught.** Only because the lane had pre-registered that the
simulation would be held to account against the real bake, and the script
printed the comparison whether or not anyone wanted it. Nothing else would have
looked.

**The rule.** A tolerance is a measurement or it is not written down. If the
number is not yet measured, the sentence says *"the quantisation cost is not yet
measured"* and names the script that will measure it. An unmeasured tolerance in
a provenance document is worse than no tolerance, because the document's format
tells the reader it was measured. (CONSTITUTION rule 1; the skill
`ww-simulate-before-build` now carries the audited figures.)

## 2026-09-12 -- ROADS3 -- 410 CRLF lines smuggled into an LF-only report by the shell

**What was done.** Parts of `scratchpad/lane_roads3_report.md` were appended
with shell heredocs. Every one of those lines landed with `\r\n`. The finished
file read 410 CRLF against 630 LF -- mixed, in a directory where all thirty-odd
sibling lane reports are LF-only (`lane_bakeperf1_report.md` 0/852,
`lane_build11_report.md` 0/364, and so on).

**What was true instead.** `grep`, `wc` and every editor view showed nothing
wrong; the file looked identical either way. Only a byte count found it:
`d.count(b'\r\n')` against `d.count(b'\n')`.

**How it was caught.** By accident. A `str.replace` patch of report section 5
failed to match its anchor, the anchor was copied from a `sed -n` view that had
stripped the `\r`, and chasing that miss led to the byte count. Had the patch
been written against a fresh read the contamination would have shipped
invisibly.

**The rule.** This repository already has the rule -- *line endings are measured
with Python byte counts only; heredocs arrive CRLF* -- and the mechanism is the
same one entry 1 above is about: **prose through the shell.** Restating it in
the form that would have prevented both: every file this lane writes is written
by Python with `newline=''`, and every file it finishes is byte-counted for
`\r\n` before it is called done. The report, the four deliverable documents and
`docs/LODGEN_TERRAIN_VT.md` were all byte-counted at the end of this lane and
all read 0 CRLF. The last thing this entry did before being filed was bite
again: writing the phrase above through the shell put a real carriage return in
the middle of the sentence describing carriage returns, and the end-of-lane byte
count is what caught it.

## 2026-09-12 -- ROADS3 -- a measurement was allowed to carry a recommendation about how something should LOOK

**What was done.** ROADS3 measured whether vanilla's far road keeps any of the
road texture's own detail. It does not: the residual after the best wash
correlates with our full-detail bake's departure from its flat average at
+0.0275 against a phase-twin floor of 0.0270 / 0.0644, and +0.0162 against
0.0124 / 0.0163, with the best-fit strength negative. That measurement is sound
and a test that could have overturned it did not.

**What was wrong.** The lane then wrote, in the report, in the changelog entry,
in the handoff block and in the format contract, that **`--road-detail` stays
0** -- "ROADS2's default is confirmed". The measurement confirmed no such thing.
It answered *what Bethesda's sheets contain*; it was quoted as an answer to
*what our roads should look like*, which is a different question and not one a
correlation can settle.

**How it was found.** bungo looked at the two bakes side by side on 2026-09-12
and said *"--road-detail 1 is always on, do not ever use road detail 0, that
looks terrible"*. The solid-colour ribbons he had complained about in the brief
that started this lane were partly that default. So the lane's own opening
complaint and the lane's own recommendation pointed in opposite directions and
nobody noticed, because the recommendation arrived wearing a number.

**The rule.** A measurement of vanilla settles what vanilla does, and may set a
floor, a ceiling or a gate. It does not settle taste. When a lane's finding
touches how something LOOKS, the lane produces the picture and the number and
**stops there** -- the default is bungo's call, named as his call, in the
report's "bungo's calls" section and nowhere else. ROADS3 did exactly this for
`--road-opacity`, correctly, in the same report, and then failed to do it for
`--road-detail` one section earlier. The tell is the verb: "stays", "is
confirmed", "should be" in a sentence whose only evidence is a correlation.
