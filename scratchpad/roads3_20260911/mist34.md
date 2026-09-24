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
all read 0 CRLF.
