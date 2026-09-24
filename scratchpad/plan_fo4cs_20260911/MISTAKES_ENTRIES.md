## 2026-09-11 — a citation form the checker cannot see is an UNCHECKED citation (lane PLAN-FO4CS)

**What was done.** `docs/FO4CS_IMPROVED_LOD_PLAN.md` cited `CARDS 7.6` and
`LODM 7.8`, as if section 7 of `docs/LODGEN_CARD_SHEETS.md` and
`docs/LODGEN_LODM_FORMAT.md` had numbered subsections.

**What is true instead.** Section 7 of both pages is ONE section containing a
numbered LIST of invariants. There is no heading `7.6` and no heading `7.8`, so
both citations named nothing and neither would ever resolve. The citable forms
are `CARDS 7, invariant 6` and `LODM 7, invariant 8`.

**How it was found.** By hand, while writing the page's own citation gate — the
two rows were spotted before the gate's first run, which is why G1 read 0
unresolved on that run and the count is not evidence the forms were right to
begin with.

**The rule that prevents it.** `ww-census-contract` §2 already says a citation
form the checker cannot parse is an unchecked citation, and gives the
non-numbered-heading case. The same rule covers a numbered list item inside a
section, and the skill amendment this lane wrote
(`scratchpad/plan_fo4cs_20260911/SKILL_AMENDMENT_ww_census_contract.md`, §2a
item 2) states it in those words.

## 2026-09-11 — a rung written without the nine-part skeleton comes out short (lane PLAN-FO4CS)

**What was done.** Rung R5 of the plan was written as three lettered sub-waves
(R5a / R5b / R5c) in prose, with no `**READS.**` and no `**DOES.**` block of its
own, while the other five rungs carried all nine parts.

**What is true instead.** The brief's gate G2 requires all nine parts in every
rung, and R5 had seven.

**How it was found.** Gate G2's first run, which is exactly what G2 is for:
`6 rungs, 1 short — ### R5 is missing READS., DOES.`

**The rule that prevents it.** Write the nine labels into a rung as an empty
skeleton BEFORE any prose, so the rung cannot be finished short. Prose written
first fills the shape it happens to want.

## 2026-09-11 — a vocabulary gate accused the document when the gate was wrong (lane PLAN-FO4CS)

**What was done.** The page's own gate G3 reported `rep[0..3]` as invented
vocabulary and flagged it for removal.

**What is true instead.** `rep[0..3]` is `docs/LODGEN_NATIVE_LODO_LODI.md` §4.4's
own phrase, used verbatim there. The defect was in the gate: its token splitter
ran `split('.')` before `split('[')`, so `rep[0..3]` reduced to `3]` and matched
nothing. It also reported `fBlockLevel0Distance`, which is the engine's own
`[TerrainManager]` INI key quoted with bungo's live values in FO4CS's
`Codex/lod-fo4-vs-fo76-comparison.md` — real, and in none of the eight contract
pages.

**How it was found.** The reported token was a phrase visible in the source page
with the naked eye, which is the tell.

**The rule that prevents it.** When a gate accuses a claim you can read in the
source with your own eyes, **suspect the gate first and check the extractor
before editing the document**. This is the same failure
`ww-census-contract` §2 records for a heading regex, arriving in a different
regex; the amendment generalises it.

## 2026-09-11 — the quick fix for the second of those would have made the gate stop gating (lane PLAN-FO4CS)

**What was nearly done.** `fBlockLevel0Distance` would have been admitted by
adding FO4CS's `Codex/lod-fo4-vs-fo76-comparison.md` to the gate's corpus set.

**What is true instead.** Widening the corpus to admit ONE word admits every
identifier in that file at once and records nothing about why. What was done
instead is a one-entry `ENGINE_VOCAB` set with the file it is quoted from in a
comment beside it, so the allowlist cannot grow without someone writing down
where a word came from.

**How it was found.** Before it shipped; recorded because it is the wrong
instinct and the wrong instinct is what makes a green gate meaningless.

**The rule that prevents it.** A real word that belongs to no contract of ours
goes in a NAMED allowlist with its source. Never widen the corpus.
