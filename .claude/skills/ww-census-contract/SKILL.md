---
name: ww-census-contract
description: Write the census a CONSUMER project will print, from the NifSkope side, as a contract page — the seven-column field table that makes every counter written-and-moves before a line of runtime exists, the read-from column that is script-checked against real contract sections, the bake half that gives each runtime number something to be checked against, and the checker that proves the bake half from the bytes. Use whenever a generator here produces files another project's runtime will consume and somebody asks for counters, telemetry or "how will we know it works"; also for any page whose gate is "every row is filled in".
---

# Writing a census spec for a runtime that does not exist yet

Lane CENSUS1 (2026-09-11) wrote `docs/LODGEN_CENSUS.md` for FO4CS's Improved LOD
module from bungo's ruling *"We need them"*. The module had no runtime at all, so
every field had to be specified before anything could be measured — which is the
point: **defining them now means the runtime is measurable on its first flight
instead of argued about.** The same shape will be wanted for the terrain module,
the water module and every later consumer, and none of the steps below were
obvious the first time.

`fo4cs-census-field` is the rule for ONE word in an EXISTING row. This is the
page for a whole module's rows, written before the row exists.

## 1. The field table is seven columns and none of them may be blank

`name | unit | what it counts | read from | how it MOVES | refusal words | default`

* **`how it MOVES` is the test**, written as the scene change the consumer must
  drive — "raise the threshold: this rises and `drawn` falls by the same number",
  not "it changes when the thing changes". A lane implementing the field owes
  that case red with the assignment removed. Writing the moves column IS writing
  rule 1 of 2026-09-04 21:33 in advance.
* **`refusal words` is the COMPLETE list** for that field. Anything the runtime
  emits that is not on the list is a defect, and saying so in the page is what
  makes the list worth having.
* **`default` must accuse the plumbing.** Pick a small closed vocabulary and use
  it everywhere — CENSUS1 used `uncounted`, `unread`, `unset`, `unchecked`,
  `unwired` — and state in the page which fields are allowed a literal `0`
  (only those where zero is a measurement). A quiet zero composes into a row that
  looks like a healthy quiet build; that is the `TrueAimCensus{}` defect.
* **Never leave a cell blank to mean "not applicable".** Two words carry the two
  different meanings: an em dash in `refusal words` means *this field never
  refuses*, and the literal `runtime` in `read from` means *this is not read from
  any baked file*. Blank cannot distinguish those from "we did not trace it", and
  the gate below counts blanks.

## 2. The `read from` column is a citation, and it is script-checked

Use short page keys (`NATIVE 4.1`, `VT 2.4`, `CARDS 1.2`) and define them once in
the page. Then a ~100-line gate script:

* parses every table whose header row is exactly the seven columns;
* counts blanks over the six gate columns — must be 0;
* extracts every `KEY n.n` citation from the WHOLE page (table cells and prose)
  and checks the section exists in the page it names.

**Two floors, both on a COPY of the text in memory, never on the file:** append a
row with blank cells (the blank count must rise) and append a row citing a
section that does not exist (the missing count must rise). A gate with no floor
is not a gate, and copying the page to disk to sabotage it risks leaving the
tree poisoned when the turn ends early.

**The heading regex is where this goes wrong.** Markdown sections here are
`## 4.1 Title` but top-level ones are `## 4. Title` — with a period. A regex that
demands whitespace after the number reports every top-level citation MISSING and
you will spend a round "fixing" citations that were correct. Allow an optional
trailing period. Also teach it the non-numbered headings a page really has
(`## 11. Deviations from the spec, as built` is section **11**, so cite it as
`NATIVE 11, Deviation 5`, never as `NATIVE, Deviations 5` — the second form is
invisible to the checker and therefore unchecked).

## 2a. When the page cites more than one sibling

`docs/FO4CS_IMPROVED_LOD_PLAN.md` (lane PLAN-FO4CS, 2026-09-11) cites eight
contract pages and carries 73 numbered citations plus 4 named ones. Four things
go wrong at that width that do not go wrong at one page, and two of them make the
gate report a defect that is not there — which is worse than no gate, because a
lane then edits a correct document to satisfy a broken checker.

1. **Resolve NAMED headings as well as numbered ones.** Half of
   `docs/LODGEN_BTD_FORMAT.md`'s sections have no number at all (`## Header`,
   `## Tables`, `## Blocks`, `## Height encoding`, `## Ambient occlusion`), so a
   page citing it has to write `BTD Header` and the checker has to resolve that.
   Collect named headings beside numbered ones, lower-case both sides, and match
   on the **longest prefix** of the cited phrase that is a real heading — a
   citation reads `BTD Height encoding` inside a sentence and the sentence keeps
   going, so an exact-string match finds nothing.

2. **A numbered LIST inside a section is not a subsection.** `CARDS 7.6` and
   `LODM 7.8` name nothing: section 7 of both pages is ONE section holding a
   numbered list of invariants. The citable forms are `CARDS 7, invariant 6` and
   `LODM 7, invariant 8`. Put this in the citing page's own "how to read a
   citation" block, or the next writer invents the dotted form again — it is the
   obvious thing to write and it is silently unresolvable.

3. **When the vocabulary gate accuses a token you can read in the source page
   with your own eyes, fix the EXTRACTOR, never the document.** This lane's
   splitter ran `split('.')` before `split('[')`, so `rep[0..3]` — which
   `docs/LODGEN_NATIVE_LODO_LODI.md` §4.4 uses verbatim — reduced to `3]` and was
   reported as invented vocabulary. §2 already records the same failure in the
   heading regex ("you will spend a round fixing citations that were correct");
   it is a family, not an instance, and the family is **operator order and
   normalisation inside the extractor**. The tell is that the accused token is a
   phrase you have just read in the contract.

4. **A real word that belongs to no contract of ours goes in a NAMED allowlist
   with its source file, never into the corpus.** `fBlockLevel0Distance` is the
   engine's own `[TerrainManager]` key, quoted with bungo's live values in
   `E:\Projects\Fo4CommunityShaders\Codex\lod-fo4-vs-fo76-comparison.md`, and it
   is in none of the eight contract pages. The quick fix — add that file to the
   corpus — admits **every identifier in it at once** and records nothing about
   why any of them is allowed. Write instead:

   ```python
   # Established ENGINE vocabulary: not ours, not in our contracts, not invented.
   # Each entry names where it is quoted, so the allowlist cannot grow silently.
   ENGINE_VOCAB = {
       'fBlockLevel0Distance',   # [TerrainManager], quoted in Codex/lod-fo4-vs-fo76-comparison.md
   }
   ```

   One entry, one comment, one source. A gate whose allowlist can grow without a
   sentence beside each entry is a gate that stops gating on its second bad day.

**The third gate this width needs, beyond §2's two.** A page that plans work
rather than specifying fields has no seven-column field table for §2's blank
check to run on, so the structural gate becomes **shape**: every section that is
one unit of work carries every part it is required to carry. Write the part
labels as a literal list in the script and report which unit is missing which
label by name. Its floor is one label deleted from a copy in memory. On this
page it found a rung genuinely short of two of its nine parts, which prose alone
had hidden.

**Every floor still runs on a copy in memory, never on the file** (§2's rule).
Three floors here: a citation to a section that does not exist, a deleted part
label, and an invented token. All three must fire in the same run as the pass, or
the pass is not evidence.

## 3. The bake half is what makes the runtime half falsifiable

A census page that only describes the runtime is a wish list. Add two sections:

* **an inventory of the census the generator ALREADY prints** — one row per census
  line, read off frozen log artefacts, never retyped from memory;
* **the cross-checks**: one table of `runtime field | bake number | the check`.
  `drawn <= instanceCount`. `tilesResident <= tilesPresent`. `triangles at
  tolerance 0 == the level-0 count of the drawn set`. Each is arithmetic a reader
  can run with the file open.

The best cross-check is one where **zero is ambiguous and the file resolves it**:
if the container says a worldspace has no occluder boxes, the runtime's
`culledByOccluder` must REFUSE by name rather than print 0, because a silent 0
cannot be told from a broken test. Look for that shape deliberately — there is
usually one per module.

## 4. The checker: three verdicts, and only one is a pass

Write `tests/spells/<thing>_census_check.py <out-dir> [--census <log>]`. It
decodes the output files with the project's INDEPENDENT decoder (never the
writer) and compares each census-line number against the number in the bytes.

* `ok` — they agree.
* `RED` — they disagree.
* **`not-derivable`** — the census word is a bake-time fact the container does not
  carry: a refusal reason, a corpus read, a before/after measurement. **Name each
  one with its reason and count them separately.** CENSUS1 had 31 of these
  against 59 real checks; a checker that quietly skipped them would have claimed
  a coverage it did not have.

Two refusals it must make instead of guessing:

* **Never pick the log that agrees.** If more than one log under the out-dir
  carries the census line and they differ, REFUSE and name both. Auto-selecting
  the matching one makes the whole check circular (CONSTITUTION 4).
* **"Unreadable" is not "wrong".** If the decoder refuses the files, exit with a
  distinct code and the decoder's own words, so a broken pair is never reported
  as a census disagreement.

The floor is a `--doctor field=value` switch that overwrites one parsed CLAIM
before comparing, plus a `--self-floor` that drives three of them one at a time
and requires each to be caught by name.

## 5. Run it on more than one output, and expect the extra one to find something

Gate on the pair the brief names, then run the same checker on **every other pair
a lane has baked**. CENSUS1 ran three. Two were green. The third — the only dense
region anyone had produced — made the independent decoder refuse at instance
3359, on a rule nobody had exercised because no earlier gate had decoded that
pair. That is the whole value of the extra run, and it costs one command.

When it happens: **measure it before naming it a defect.** Decode the offending
record and its neighbours by hand and print the quantity the rule keys on. In
that case the neighbour sat 2.999985 cells from its chunk origin *after*
quantisation, so the decoder re-derived a different cell than the writer sorted
on — a known boundary effect one level down, not a writer defect. Report it with
the number, and do not fix a file you do not own.

## 6. Provenance when another lane owns `src/`

If a build lane is live in `src/` while you write, **cite no `src/` line numbers
at all** and say in the page why. Anchor every bake-census claim to the census
line's own leading word (`native:`, `vt:`, `arrays written:`) — which survives
edits — and to a frozen log file that cannot change. Stamp the contract pages you
cite with sha256, bytes and lines, and **re-stamp after your own last edit**: a
pointer paragraph you add to a page you also cite changes that page's hash, and a
footer written before it is wrong by your own hand (`ww-contract-provenance`
step 5).

## 7. What the page must say out loud

* **Which of its own fields must refuse TODAY.** CENSUS1's card fields all refuse,
  because the bake writes `cardLayer` 0xFFFF on every base (a documented
  deviation). Four zeros would have read as a healthy treeless worldspace.
* **Which counters are BINS rather than rules.** If the format forbids selecting
  by a quantity the reader will recognise (rings, chunks, levels), but the census
  still reports in it, say so in its own subsection and make the bin edges a
  field, or the first reader will think the runtime selects that way.
* **What is owed, and to whom.** A missing generator word is named as owed to the
  next generator lane with the exact bytes it would take, and NOT added when
  another lane owns the writer.
* **What is deliberately not owed.** "Nothing names an object's engine fade class,
  and that is the engine's record, not a bake product" stops the next reader
  searching a file for it.
