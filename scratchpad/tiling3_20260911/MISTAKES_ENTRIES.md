## A fallback was claimed in a log line that the code never had

Lane TILING3, 2026-09-11. `a6_pick.py` printed that the selection "falls back to
the next-best warp if the chosen one fails a sheet". It does not: the script picks
the first setting that gates the most sheets and stops. The sentence was written
while the fallback was still an intention and survived into a log that was read as
a record.

**What it cost:** nothing on disk, because the pick was checked by hand. What it
could have cost is worse than a wrong number -- a reader trusting a safety net
that is not there, and not asking why one sheet failed.

**The rule:** a log line describes what the code did on THIS run, never what the
design intends. If a sentence in a log cannot be traced to a branch that executed,
delete the sentence or write the branch. Before shipping any script's narrative,
re-read it against the code with the question "which line printed this, and what
did it have to have done first".

## A per-sheet gate was graded without a per-sheet floor

Lane TILING3, 2026-09-11. The repeat instrument's ceilings (absolute 0.264, ratio
0.448) were frozen by TILING2 across 22 shipped sheets, and TILING3 first graded
candidate bakes against them with only the instrument's own 12-period null sweep
as the floor. On a single 512-texel chunk that sweep is too coarse to separate
anything: every reading sits under it, **including vanilla's own** (0.201 against
a null of 0.449). A gate whose floor is above every candidate cannot fail, which
means it also cannot pass.

**The fix that was applied:** each sheet was given a floor that is provably
repeat-free on that same sheet -- the `--land-sample average` bake, where one
repeat IS the whole texture at its 1x1 mip -- and the verdicts were re-graded
against the 22-sheet absolute ceiling, with the caveat written into the log rather
than left for a reader to find.

**The rule:** a per-sheet verdict needs a per-sheet floor. A floor computed on a
corpus and a reading taken on one tile are not the same measurement, and a null
sweep that swallows the control is a broken instrument, not a strict one. Check
the control against the floor BEFORE grading anything with it.

## A brief's premise was carried into the work as if it were a finding

Lane TILING3, 2026-09-11. The brief framed hypothesis D as "geology from a finer
heightfield" and proposed shipping vanilla's residual as a detail layer if D held.
D did hold as an ORIGIN -- vanilla's colour grain lies along vanilla's own fine
relief, clear of every floor on every sheet -- and the lane nearly shipped the
brief's conclusion on the strength of that. The 22-column regression that was run
afterwards put a ceiling of R^2 0.018 on ANY per-texel law from the `_msn` to the
colour: about 98 % of vanilla's fine colour is not a function of vanilla's fine
normal. The premise "D holds, therefore D generates" was the brief's, not the
data's.

**What saved it:** running the ceiling measurement at all, when the three D
instruments had already "passed" and the brief said to proceed.

**The rule:** "X correlates with Y" and "X can generate Y" are different claims
with different instruments. When a hypothesis passes, measure how much of the
target it can actually account for before building on it -- the correlation's
strength is not the ceiling, and a brief that skips from one to the other is
stating a plan, not a result.

## A harness read a ruling working as a regression

Lane TILING3, 2026-09-11. `lodgen_ground_cover.sh` check C1 asserts that all three
of a chunk's sheets are 174,888 bytes -- DXT1 with a full mip chain. When the new
default made a chunk's `_msn` vanilla's own file, which Bethesda encodes as BC5 at
349,680 bytes, the ground-cover harness failed 29/6 against a 29/5 baseline. The
failure was real, the invariant was stale, and for a few minutes the obvious
reading was that the change had broken ground cover.

**The fix:** the ground-cover bakes now pass `--land-detail-source none`
explicitly. A harness forces the state it measures instead of inheriting whatever
the current default happens to be -- the same rule this tree already applies to
GUI harnesses and QSettings, applied to a CLI default.

**The rule:** every default a harness does not name is a hidden input. When a lane
changes a default, the harnesses that pinned the old one are the first place the
change will look like a bug -- and the fix is to pin the state, never to relax the
assertion.
