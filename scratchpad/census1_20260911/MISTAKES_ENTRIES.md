# MISTAKES.md entries — lane CENSUS1, 2026-09-11

(For the director to splice into the root `MISTAKES.md`. Both are this lane's own
mistakes; the Boston-pair decoder defect in the report's §4.2 is a FINDING about
another lane's file, not a mistake of ours, and belongs in the handoff ledger
rather than here.)

---

**2026-09-11, lane CENSUS1 — a gate that reported rot that was not there.**
What was done: the first run of `scratchpad/census1_20260911/page_gate.py` (gate
C3, "every read-from citation resolves to a section that exists") reported **49 of
50 citations MISSING**, including `NATIVE 7`, `NATIVE 4`, `VT 4` and `CARDS 2` —
every one of which exists and is a section anyone can see.
What was true instead: the page's citations were correct from the first draft. The
fault was the script's own heading regex. Headings in these pages are
`## 4.1 Title` at the second level but `## 4. Title` at the first, **with a
period**, and the regex demanded whitespace immediately after the number, so every
top-level section was invisible to it.
How it was found: the report was implausible on its face — a page cannot be wrong
about `NATIVE 7` — so the extractor was suspected before the page was, and the
heading regex was tested against the real headings.
The rule that prevents it: `ww-contract-provenance` already says, in as many
words, that a first-run MISSING report is usually the script and not the tree
(11 of lane DOCS2's 13 first-run MISSING rows were the script), and it names four
extractor rules each learned by a wrong report. Those rules were re-derived from
scratch here instead of being read first. **Cost: one round.** The period case is
now written into the new skill `ww-census-contract` §2 so the next lane gets it
free.

**2026-09-11, lane CENSUS1 — believed a decoder's return shape instead of reading
it.** What was done: `tests/spells/lodgen_census_check.py` indexed
`T['cellRanges'][k]['instanceCount']`, assuming the independent decoder returned
dicts there as it does for the instance and cluster tables.
What was true instead: `lodgen_native_decode.py` returns plain
`(instanceFirst, instanceCount)` tuples for both `cellRanges` and
`occluderRanges`; the run died with `TypeError: tuple indices must be integers`.
How it was found: the first run crashed loudly, which is the harmless case.
The rule that prevents it: CONSTITUTION rule 4's third rule of 2026-09-04 21:33 —
check our own tree before quoting it. The shapes were three greps away and were
assumed instead of read. Recorded because the harmless version of this class is
the one that teaches it cheaply; the harmful version is a shape that silently
reads the wrong field.
