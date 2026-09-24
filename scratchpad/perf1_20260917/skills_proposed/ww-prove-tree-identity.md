# ww-prove-tree-identity

**Proposed by lane PERF1, 2026-09-17. Not installed; the director decides.**

*When a change is allowed to make a bake FASTER but not DIFFERENT, this is how
you prove it did not make it different.*

## When to use it

Any lane that parallelises, caches, reuses or reorders work inside a generator
whose outputs are files. "It looks the same" and "the .lodo is the same size"
are not proofs. This skill is the proof.

## The steps

1. **Compare the WHOLE tree, not a sample.** Walk both output roots, compare the
   relative path SETS first (a missing file and an extra file are differences,
   and a sampler never sees them), then SHA-256 every file. One comparer for the
   whole tree: `tests/spells/lodgen_treecmp.py`.
2. **Never write a second parser for a format you are comparing.** The `.lodb`
   goes through the tree's ONE record reader (`tests/spells/lodb_read.py`).
   A comparer with its own parser proves that the comparer's two parsers agree.
3. **Bake both arms into the SAME out-dir and move the first tree aside.** The
   record carries absolute paths; two out-dirs means the comparison measures the
   folder name. (BAKEREC1's lesson, MISTAKES.md 2026-09-17 01:1x.)
4. **Name every concession in the comparer's own docstring, and pass no other.**
   PERF1's list: the five volatile record fields (masked by the record's own
   reader); the value line of the `--threads` / `--chunk-threads` switches and
   the `threads N, chunk threads N bound by W` / `chunk workers N` clauses of
   the census, which record WHAT WAS ASKED; `--build-mask` for the header's
   generator-exe byte size, ONLY when the two arms are two different exes;
   `--drop-record-line PREFIX` for a census line an older exe never wrote, which
   must then be asserted separately in the same leg.
5. **Run the leg against a pair you already know agrees before you trust its
   first red.** A leg that has never been seen green is not a gate yet. PERF1
   shipped a leg that compared raw record bytes (the `baked` timestamp alone
   made it unfailable-green-never) and read its own red as a product defect for
   a minute. MISTAKES.md, same day, entry 4.
6. **Say the count out loud in the gate's own words**: `57 file(s); 57
   identical; 0 difference(s)` beats `PASS`.

## The refuter this skill owes

Flip one worker's retire order in a scratch build. If the identity leg does not
go red, the leg is not testing identity and nothing above it is worth anything.
