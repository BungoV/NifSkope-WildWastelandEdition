---
name: ww-measure-before-you-parallelise
description: Use before parallelising or speeding up any generator stage: measure the launch exe on two regions that differ in one property, split the dominant stage, and let the table decide where the work goes. PERF1 found the texture pass had nothing to parallelise and the prize was the serial library build.
---

# ww-measure-before-you-parallelise

Written by lane PERF1, 2026-09-17; placed by the director at landing.

*The brief said "parallel object and texture passes". The measurement said the
texture pass had nothing to parallelise and the prize was somewhere else. This
is how to find that out before writing the code, and how to ship the finding.*

## The steps

1. **Measure the exe AT LAUNCH, before one source line changes.** Two regions
   that differ in exactly one property (PERF1: 9 chunks and 16, same worldspace,
   same origin), each arm repeated warm, wall clock and the stage line and the
   peak working set and the census's bound-by word. Anything measured after a
   change is a measurement of a different program.
2. **Split the stage that dominates, and SHIP the split.** Put wall-clock reads
   at the internal boundaries and append them to the census line that already
   exists. A scratch build measures a binary nobody will ever run; a shipped
   split is read by every lane after you for free. Ride INSIDE an existing
   volatile field so the count of volatile things does not move, and check BOTH
   normalisers mask it.
3. **Count the workers, do not ask for them.** Record the distinct thread ids
   that actually ran a job, per fan-out, and put the counts in the census. This
   is what lets a gate call a run VACUOUS: a bake that permitted 16 threads and
   ran 1 reports 1, and a floor that only checks wall clock cannot tell those
   apart.
4. **Let the split choose the work.** PERF1's split said: models 30 %, ladder
   45 %, textures 0.0 s inside the library, region-dependence 4 % of a bake. So
   the lane spent itself on the object library, wrote step 3 up as a measured
   statement rather than a rewrite, and found the real prize (not rebuilding the
   library at all) in the same table.
5. **Sweep the worker count per fan-out, not per program.** PERF1 measured the
   model stage getting SLOWER past four workers while the ladder stage over the
   same bakes wanted all sixteen. One number for both throws one of them away;
   a per-fan-out ceiling is a ceiling, never a floor and never a way past
   `--threads`.
6. **Write the negative result up with all its numbers.** "There is nothing to
   parallelise here" is a deliverable when it carries the table that says so.
   The next lane must not have to re-measure it to disbelieve it.

## The trap

Never trust a wall clock without looking at what else is running. Two runs in
PERF1's sweep came back three to twelve times slow because an unrelated
`find.exe /` from another session was walking the filesystem. They were re-run
and named as outliers in the report rather than averaged in.
