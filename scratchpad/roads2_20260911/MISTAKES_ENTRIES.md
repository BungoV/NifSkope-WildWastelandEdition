## 2026-09-11 — ROADS2 — an AUC recorded to three decimals when its own ties made the third meaningless

**What was done.** Lane FLAGSCAN1 recorded, and the contract's section 1a.1
repeated, a greyness AUC of **0.716** for the road family on chunk (-8,8), to
three decimals, in a table used as a pre-registered gate: lane ROADS2's brief
said "gate on chunk (-8,8) (0.716 vs 0.629)".

**What was true instead.** Re-running FLAGSCAN1's own script, unchanged, on the
same vanilla sheet today gives **0.757**. The brightness half of the same table
reproduces exactly (0.628 against 0.629 recorded). The difference is in the
score, not in the script: saturation on that sheet takes 281 distinct values
over 262,144 texels and **one of them covers 140,305 — 53.5 percent of the
tile** — while `tile2.auc()` ranks with `np.argsort`, which breaks ties by array
index, and the index runs in raster order. So the grey AUC of any spatially
clustered mask depends on WHERE its texels happen to sit inside the tie block —
which is a property of the loop order, not of the image. Break the ties at
random and the same number is 0.731 repeatably (0.7316 / 0.7308 / 0.7308 on
three seeds); compute tie-averaged ranks, which is the AUC's own definition when
ties exist, and it is 0.731. Brightness has 1,499 distinct values and no
dominant block, which is why that half is stable.

**How it was found.** ROADS2's first act on the gate was to try to reproduce the
number it was told to gate on, before using it. It did not reproduce, and the
gap was 0.041 — too big to be a rounding difference and too small to be a
different tile. Printing the histogram of the score being ranked found the
53.5-percent tie block in one line.

**The lesson.** *Before recording an AUC (or any rank statistic) to three
decimals, count the distinct values of the score being ranked and the size of
its largest tie block. If one value covers a large fraction of the sample, the
ranking is not determined by the data and the digits are noise.* Use
tie-averaged ranks, or gate on a tie-free column, and say in the record which
one was used. `scratchpad/roads2_20260911/gate4b.py` has the tie-averaged
implementation, prints both, and the contract's 1a.1 now carries the provenance
note.

## 2026-09-11 — ROADS2 — a pre-registered gate that could not fail, and was nearly reported as a pass

**What was done.** The brief pre-registered gate S4 for the tree-filename
narrowing as: run `--list-impostor-candidates --candidates trees` over
Sanctuary, and check that the four `SetDressing\Tree*.nif` props are absent from
the list after the change. The list was produced, the four were absent, and that
is a pass on the words as written.

**What was true instead.** They are absent on the **rung** too — the exe from
before the change. Sanctuary's list is 20 lines and the whole Commonwealth's is
36 lines, and there is no `SetDressing` in either, on either exe. The reason is
two levels away from the clause being tested: the candidate lister returns early
on `!b.hasLod` (`src/nifcli.cpp:3350`), and all seven of those props have no
MNAM and header bit 15 clear, so they can never appear in that list whatever the
filename rule says. The gate measured the `hasLod` guard, not the change.

**How it was found.** By running the gate on the OLD exe first, which was the
only reason it was caught. Had it been run only on the new exe, a vacuous
green would have gone into the report as evidence.

**The lesson.** *Run every pre-registered gate on the OLD binary before running
it on the new one. A gate that is already green before the change measures
something else.* The substitute used here is a classifier table over the whole
placed corpus — 137 tree bases under both rules, 7 flipping out, 0 flipping in,
and the 36 of the 137 that carry a distant LOD mesh matching the exe's own
36-line list base for base — which fails loudly if the clause is wrong in either
direction. The MEASURE-DONT-EYEBALL rule already says to prove the invariant
fails on broken code; this adds: prove it would also have failed on the code you
are replacing.

## 2026-09-11 — ROADS2 — a tile-scoped reference dump used without checking its own coordinate range

**What was done.** The raised-road audit was run against
`scratchpad/flagscan1_20260911/hw_refs.json`, chosen because its name says
"hw" and the lane needed the highway tile. It produced a complete, plausible,
internally consistent table — 108 road bases, a clean split by folder, no
warnings — and that table was read and reasoned about before anything looked
wrong.

**What was true instead.** `hw_refs.json` is FLAGSCAN1's dump of chunk
**(-20,-12)**, not (-8,8). The (-8,8) dump is the file beside it,
`hw2_refs.json`. The real answer for the gate's tile is 151 road bases and 759
placements, not 108. The same mistake had already produced an all-zero mask set
from `hw_tile.py` half an hour earlier, and that zero was written off as a
script problem rather than an input problem.

**How it was found.** By printing the minimum and maximum position of every
reference in each dump — two lines — after the all-zero masks refused to
explain themselves.

**The lesson.** *A dump that is scoped to a region must be checked against the
region it is going to be used for, by printing its own extent, before a single
number is read off it. A file name is not a coordinate range.* Every script in
this lane that loads a refs dump now prints the dump's own cell extent first,
and an all-zero mask is treated as an input error until the input is ruled out.

## 2026-09-11 — ROADS2 — the heredoc apostrophe trap, again

**What was done.** A patch to `src/nifcli.cpp` was written as a bash heredoc
containing C++ string literals with apostrophes in the prose (`vanilla's`,
`texture's`). Bash failed with `unexpected EOF while looking for matching '''`
and the patch did not run.

**What was true instead.** This is already in the project's own trap list —
"no backslash or apostrophe through a bash heredoc" — and it was walked into
anyway, on a file full of both backslashes (Windows paths in the usage text) and
apostrophes (English).

**How it was found.** The shell said so immediately, which is the good case.
The cost was one wasted round trip, not a wrong result.

**The lesson.** *Any patch whose payload contains an apostrophe or a backslash
is written to a file with the Write tool and then executed, never passed through
a bash heredoc.* That is now how every source patch in this lane was applied
(`scratchpad/patch_nifcli.py`, `patch_contract.py`). The rule is worth stating
as an unconditional: prose in code comments always contains apostrophes, so
"source patch" and "Write tool" should be a reflex, with no case-by-case
judgement.

## 2026-09-11 — ROADS2 — a material reader that silently dropped the field a measurement needed

**What was done.** `matinfo.read_material`, the shared BGSM/BGEM reader used by
several terrain lanes, was used to answer "do any road materials blend?" It
reported **0 of 474**, and that number went into the analysis as evidence
against a compositing explanation for the seam.

**What was true instead.** The 0 is correct for the MATERIAL flag, but the
reader never parses `bAlphaBlend` at all — it reads and discards it — so the
same 0 would have come back if every material set it. The blending that does
exist on that corpus is at the SHAPE level, through `NiAlphaProperty`: **6 of
512 road shapes blend**, which the material reader cannot see by construction.
The conclusion survived, but only because a separate shape-level pass was run
for an unrelated reason.

**How it was found.** Cross-checking the material answer against the shapes'
own `NiAlphaProperty` while looking for the alpha-test refusal path.

**The lesson.** *When a reader returns zero for a field, check that the reader
parses that field before reporting the zero as a measurement.* A reader that
skips a field should either expose it or raise on a request for it; silently
returning a default that is indistinguishable from a real zero is the failure
mode. `matinfo.read_material` is a candidate for a small fix, and the number to
quote for road blending is the shape-level 6 of 512, not the material-level 0 of
474.
