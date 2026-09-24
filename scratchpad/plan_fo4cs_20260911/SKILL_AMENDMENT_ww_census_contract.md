# Amendment to `ww-census-contract`, written by lane PLAN-FO4CS 2026-09-11

**NOT APPLIED.** The brief confined this lane to `docs/FO4CS_IMPROVED_LOD_PLAN.md`
and its own scratchpad, and `.claude/skills/ww-census-contract/SKILL.md` is
outside both. **The director applies it, to BOTH trees** — the repo tree
`E:\Projects\NifskopeWildWastelandEdition\.claude\skills\` and the live tree
`E:\Projects\Claude\.claude\skills\` (CONSTITUTION 1a: the two trees drift and
nothing syncs them).

**Where it goes:** a new section **2a**, immediately after §2 ("The `read from`
column is a citation, and it is script-checked").

**Why it is an amendment and not a new skill:** it is the same gate one page
wider. `ww-census-contract` §2 already owns the single-page citation checker and
its two floors; this is what changes when the page cites EIGHT siblings instead
of six, and when half of one sibling's sections have no number at all. A sibling
skill would split one procedure across two files.

**What it cost to learn:** four traps in one lane — two of which made the gate
accuse the document.

---

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
