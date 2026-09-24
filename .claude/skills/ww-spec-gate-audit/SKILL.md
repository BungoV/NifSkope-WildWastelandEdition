---
name: ww-spec-gate-audit
description: Audit a pre-registered gate NUMBER before building to it — when a read-only measurement lane hands an implementation lane a figure to reproduce exactly ("590 bodies, 215 merges, 3 refused"), and the figure may be an artefact of how it was measured rather than of the rule it claims to state. Covers the one-sided-approximation test, the "print the table, not the count" rule, running the known-answer control FIRST so the rule's own defects surface, and how to report a refused gate with numbers instead of reproducing a bug to hit it. Use at the START of any lane whose brief says "reproduce X exactly", before writing the code that reproduces it.
---

# Auditing a pre-registered gate number

Written from lane WATER2 (NifSkope Wild Wasteland, 2026-09-10), whose brief said
*"the C++ classification reproduces WATER1's Python census exactly (590 bodies,
1 sea / 131 / 458, the 3 refused merges)"*. The right answer was 346, and
getting there took three separate measurements and about a third of the lane.

**The situation this covers.** A read-only lane measures something, writes a
spec, and its numbers become gates in the next lane's brief. The next lane's
cheapest path is to reproduce whatever the first lane's code did — including its
bugs — because that is what makes the gate go green. Do not take it. A gate is
a claim about the RULE; the first lane's script is one implementation of that
rule and may not be a faithful one.

CONSTITUTION rule 1 says gates are pre-registered before the agent starts.
That is what makes them worth auditing rather than negotiating: the audit is a
measurement, and its output is either "the number holds" or "here is the number
that holds instead, and here is what produced the difference".

## The three checks, in this order, before any implementation

### 1. Read the source that produced the number, not the number

Open the script the spec cites and find the line that implements the sentence
the spec states. Ask one question of it: **does the implementation say what the
prose says?**

Lane WATER1's prose: *"two components at the same height whose SHORES are within
2 texels"*. Its code compared point clouds thinned to at most 4,000 points a
body. For the sea that is 4,000 points out of 21,585,117.

### 2. Is the approximation ONE-SIDED?

This is the test that decides whether the number is safe. An approximation whose
error goes both ways adds noise; an approximation whose error goes **one way**
adds bias, and a gate built on it is systematically wrong.

* A subsample of a point set can only make a MINIMUM distance longer, never
  shorter. So a decimated "within 2 texels" test can only MISS merges.
* A coarse bounding-box pre-test can only ADMIT pairs, never exclude true ones,
  so it is safe.

Measured: exact 545 pairs within two texels against the decimated test's 218,
and 340 bodies against 590. Every one of the 327 missing pairs was a merge the
stated rule required.

**Write the refuter as a script, not as an argument.** A copy of the step with
the approximation removed, in the same language, on the same inputs, printing
both answers side by side. It is twenty lines and it is the whole audit.

### 3. Print the TABLE the rule produces, never only the count

A corrected count looks like progress. Lane WATER2's exact test gave 340 bodies
where the spec said 590, and 340 looked better than 590 until the body table was
printed:

```
21587443 texels  ExtMarshScumWater  height 450.0  parts 332  (5 distinct forms)
```

— the Commonwealth's entire ocean carrying a marsh's name, because the rule
being audited had no statement of WHICH SIDE of a merge is absorbed. Making the
distance test honest is what exposed it; printing the count would have hidden it
a second time.

The general form: **when a rule changes how things are GROUPED, print what the
groups became.** Ten rows and the fields a consumer will read.

## Then: the known-answer control, and run it FIRST

`ww-control-calibration` step 1 already says run the control before believing a
real number. Add one thing for this case: **the control is where the RULE's own
defects surface, not just the implementation's.** Lane WATER2's synthetic
worldspace failed on its first run and named a second instance of the same
missing direction — in a different merge, one the real worldspace happened never
to exercise, because the ocean there never TOUCHES a painted body at its own
height; it only ever passes within two texels of one.

Two rules for the control when it fails this way:

* **The expected answer belongs to the rule under test, not to the rule the
  spec's number was measured under.** WATER1's control expected 4 bodies with a
  16-surface river, measured under a rule that keyed bodies on TYPE alone. Under
  the shipped rule a body carries ONE plane by construction, so a river that
  steps sixteen times IS sixteen bodies. Restate the arithmetic; **do not change
  the geometry**, so the two controls stay comparable, and say in the report
  that the expectation was restated and why.
* **Assert only what could be written down before the run.** Anything that
  depends on which neighbour happens to be nearest is measured, not known — so
  PRINT it and assert the things the geometry fixes (a count, an area, a form,
  the one class the control exists to pin).

## Find the invariant no version of the rule can move

Every candidate rule in a family will disagree about counts. Look for the
quantity that is the same under all of them, and gate on THAT as well — it is
what separates "the classifier groups differently" from "the classifier reads
the world differently".

For water bodies it was the per-form TEXEL totals: no merging decision can
change how many texels carry a given painted type. Thirteen of fifteen forms
matched lane WATER1's read-only census to the texel across a rule change that
moved the body count by 244, and the two that moved were the defect being fixed.

## Reporting a gate you did not meet

CONSTITUTION rule 9: a refused round with numbers is a valid deliverable. What
the report owes:

1. the gate as written, and the number produced instead, in the first paragraph;
2. a table per finding — the two answers side by side, with the script that
   produced them named by path;
3. the candidate rules MEASURED, not argued, with the discriminator that chose
   between them stated as a number ("the only clause under which the two
   readings of the form rule name the same form for all 346 bodies");
4. the invariant that did NOT move, which is what says the disagreement is about
   grouping and not about reading;
5. an entry in `MISTAKES.md` for the original defect, naming the lane that made
   it and the lane that found it;
6. what the SPEC now needs, listed by section, when the spec is another lane's
   file and this lane does not own it.

## What not to do

* **Do not reproduce the artefact to hit the gate.** It ships a measurement
  shortcut as a rule, and the next consumer inherits it silently.
* **Do not quietly change the rule and report the new number as the gate.** The
  deviation is the deliverable; bury it and the next lane re-derives it.
* **Do not audit by reading alone.** Every claim here is a number from a script
  that is kept in the scratchpad folder and named in the report.

## Audit the gates you write for YOURSELF, and start with the DOMAIN (lane NATIVE1b, 2026-09-11)

Everything above is written for a number another lane handed you. The same
audit is owed to a check you invent in the same hour, and the cheapest version
of it is one question asked before the check is written:

> **Are the two quantities this check compares over the SAME DOMAIN?**

Lane NATIVE1b built a cluster ladder and wanted bungo's far-shadow rule as a
gate: a decimation may never open a silhouette, so compare the COARSEST level's
boundary-edge count against LEVEL 0's and fail a rise. It failed on fourteen
meshes. Two of them had genuinely gone from a watertight building to a holed
one — a real defect, worth the refusal that now lives in the writer. The other
twelve were an artefact of the check, and so were the eighteen that remained
after the real defect was fixed:

* **a ladder is PARTIAL wherever a group refuses to simplify**, so the coarsest
  level is a FRAGMENT of the mesh — as little as **6.6 percent** of its own
  surface on the worst row;
* a fragment has its own outline, so its boundary count is not comparable to the
  whole mesh's at all. The check was apples to oranges from its first line.

The rule with teeth was one level down: compare a simplification's output
against **the surface it replaces** — the same triangles, both sides — which is
a per-STEP question and belongs in the writer, where it can refuse.

### The three moves

1. **Name both domains out loud before writing the predicate.** "the coarsest
   level's edges" and "the whole mesh's edges" are different sets the moment any
   part of the ladder is partial; writing the two names next to each other is
   usually enough to see it.
2. **When the check fails on real data, print the ROWS AND THEIR DOMAINS**
   before believing either the check or the data. Here the extra column was
   "how much of its own surface does this level cover", and it turned a
   fourteen-row defect list into a one-row defect and thirteen artefacts in one
   run. (This is §3 above — print the table, not the count — applied to your own
   gate's failures rather than to another lane's number.)
3. **Keep the part that was real.** An audit that ends "the check was wrong" and
   drops it has thrown away the defect it found. Split it: the enforceable half
   goes into the writer as a refusal that must be seen to FIRE on real data
   ("57 groups refused"), and the unenforceable half becomes a reporting column
   the contract explicitly says is not a gate, plus a check that every anomaly
   in it is EXPLAINED (here: every mesh with a rise has a refused group; one
   without would still be red).

## Run the gate on the OLD binary FIRST (lane ROADS2, 2026-09-11)

Everything above audits a NUMBER. This audits the gate's ability to move at all,
and it costs one extra run.

> **Before running a pre-registered gate on the new binary, run it on the one
> you are replacing. A gate that is already green is measuring something else.**

Lane ROADS2's brief pre-registered: narrow the tree-filename clause, then run
`--list-impostor-candidates --candidates trees` over Sanctuary and check the
four `SetDressing\Tree*.nif` props are absent. After the change they were
absent, which is a pass on the words. They were absent on the RUNG too --
Sanctuary lists 20 lines and the whole worldspace 36, with no `SetDressing` in
either, on either exe. The cause was two levels away from the clause under test:
the candidate lister returns early on `!b.hasLod`, and all seven props have no
MNAM and header bit 15 clear, so no filename rule can ever put them in that
list. The gate measured the `hasLod` guard.

This is the mirror image of `ww-control-calibration`'s "a floor with a fixed
step cannot fire on a degenerate subject": there the floor could not fail, here
the gate could not fail. Both are found the same way -- by printing the number
the gate produces in the case where it is supposed to be RED.

### What to substitute when the gate turns out to be vacuous

Do not weaken the gate and do not report the vacuous green. Move the check onto
the whole corpus the rule runs over, and gate on the FLIP TABLE, which fails in
both directions:

* every base the rule is asked about, classified under the OLD rule and the NEW
  one;
* the count that must not move (137 tree bases under both rules);
* the flips OUT, with their placement counts and the consequence for each (7
  bases, 82 placements, **none carrying a distant LOD mesh**, so nothing
  downstream ever existed for them);
* the flips IN, which must be zero if the change is meant to be a narrowing;
* and one independent tie to the shipped binary, so the re-typed classifier is
  not the only witness -- here, the 36 of the 137 kept bases that carry a
  distant LOD mesh matching the exe's own 36-line candidate list base for base.

A table with those five rows fails loudly if the clause is wrong in either
direction, which the original gate could not do in either.

