# Lane PLAN-FO4CS — report

Deliverable: `docs/FO4CS_IMPROVED_LOD_PLAN.md`. Documents only; no build, no exe,
no write into `E:\Projects\Fo4CommunityShaders`, no commit, no stash.

## 0. What was read (with hashes)

Hashes taken FIRST (`ww-contract-provenance` step 1), before a line of the plan
was written; re-derived at the end (§0.3). sha256 truncated to 16 hex.

### 0.1 NifSkope tree, `E:\Projects\NifskopeWildWastelandEdition` (branch `main`)

| file | sha256 (16) | bytes | lines | CR |
|---|---|---|---|---|
| `CONSTITUTION.md` | (read in full) | — | 368 | 0 |
| `HANDOFF.md` | `81fdc08dfa8b7707` | 297,028 | 4,452 | 0 |
| `docs/LODGEN_NATIVE_LODO_LODI.md` | `067eeaa70e2924fe` | 79,504 | 1,253 | 0 |
| `docs/LODGEN_TERRAIN_VT.md` | `13efc51896021bae` | 75,094 | 1,211 | 0 |
| `docs/LODGEN_BTD_FORMAT.md` | `376b372c32a47dfb` | 75,795 | 1,358 | 0 |
| `docs/LODGEN_LODM_FORMAT.md` | `47f2b4ee6b4938ba` | 28,378 | 411 | 0 |
| `docs/LODGEN_CARD_SHEETS.md` | `4edb4a9c8703185d` | 42,585 | 723 | 0 |
| `docs/LODGEN_TEXTURE_ARRAYS.md` | `64f9154b71f83631` | 13,608 | 256 | 0 |
| `docs/LODGEN_MANIFEST_FORMAT.md` | `ba5ca72c83991ad5` | 12,399 | 264 | 0 |
| `docs/LODGEN_CENSUS.md` | `521d0a5fa0de05f8` | 35,284 | 448 | 0 |

The six contract hashes CENSUS1 stamped in its own provenance footer
(`067eeaa70e2924fe`, `13efc51896021bae`, `4edb4a9c8703185d`, `64f9154b71f83631`,
`ba5ca72c83991ad5`, `47f2b4ee6b4938ba`) are **equal to the ones above**, so
nothing moved under either page between 2026-09-11 13:5x and this lane.

HANDOFF.md read: the top block (lines 1–330), every `RULING bungo 2026-09-11`
paragraph (313–655), the lane blocks NATIVE1a (1070–1191), NATIVE1b (1192–1327),
TERRAIN-R (1328–1403), ROADS1 (1404–1507), LODUI1 (1508–1602), CENSUS1
(1603–1669), and the two owed lists (1683–1789) — plus, mid-lane, the shadow
rulings the director spliced in at 1684–1711 (see §0.3).

Also read in this tree: `docs/LODGEN_CENSUS.md` in full as the measurement spine;
`scratchpad/census1_20260911/page_gate.py` (the gate this lane's gate is modelled
on); the `scratchpad/specs_20260906/spec_fo4cs_native.md` section index only, to
confirm it is the design record FO4CS's ROADMAP points at and not a second
contract.

### 0.2 FO4CS tree, `E:\Projects\Fo4CommunityShaders` — READ-ONLY, nothing written

* `wt-fixfirst/CONSTITUTION.md` (261 lines, in full) — rule 1a's standing skills,
  rule 3 proof by altered capture, rule 6 INI law and deploy, rule 8 MODULES AND
  FALLBACKS and the sampling-mode duality amendment of 2026-09-01.
* `wt-fixfirst/ROADMAP.md`, the LOD campaign block (lines 75–130) — the two tiers,
  and bungo's 2026-09-09 line naming Improved LOD as a new module.
* `Codex/HANDOFF.md` — the two rulings of 2026-09-09 15:34 and 15:35 (lines
  22816–22817) and the LODT1 wave lines that record the `.lodl` reader's state.
* `Codex/lod-fo4-vs-fo76-comparison.md` (349 lines, in full).
* The module pattern, as a loader plus a draw path: `src/FeatureModule.h`,
  `src/FarField/FarFieldShadowsSettings.h` (the whole-section-at-once INI schema
  and its live-engine-bounds rule), `src/FarField/FarFieldLodBtoChannels.h` (the
  existing consumer of the packed `.bto` channels and of the identity), the
  `src/FarField/` file list, `src/Effects/DirectionalShadowsFeatureModule.{h,cpp}`
  for `mgui::MasterCheckbox` and the `Name()` / `Category()` / `LoadSettings`
  shape.

**Nothing in that tree was written, and no exe there was run.**

### 0.3 Re-derived at the end (`ww-contract-provenance` step 5)

| file | sha256 (16) at the end | verdict |
|---|---|---|
| `docs/LODGEN_NATIVE_LODO_LODI.md` | `067eeaa70e2924fe` | unchanged |
| `docs/LODGEN_TERRAIN_VT.md` | `13efc51896021bae` | unchanged |
| `docs/LODGEN_BTD_FORMAT.md` | `376b372c32a47dfb` | unchanged |
| `docs/LODGEN_LODM_FORMAT.md` | `47f2b4ee6b4938ba` | unchanged |
| `docs/LODGEN_CARD_SHEETS.md` | `4edb4a9c8703185d` | unchanged |
| `docs/LODGEN_TEXTURE_ARRAYS.md` | `64f9154b71f83631` | unchanged |
| `docs/LODGEN_MANIFEST_FORMAT.md` | `ba5ca72c83991ad5` | unchanged |
| `docs/LODGEN_CENSUS.md` | `521d0a5fa0de05f8` | unchanged |
| `HANDOFF.md` | **`8a7fce2f67c7f898`**, 302,554 B, 4,531 lines | **MOVED** — the director spliced the shadow rulings of 2026-09-11 14:4x -> 15:0x into the owed list during the lane |

**Version constants re-read LAST of all** (step 4), from the pages and not from
this lane's notes: `.lodo` version **3**, `.lodi` version **3** (both refusing 1
and 2 by name), `.lodt` **version 2** with magic `LDTX`, `.lodl` writer default
**2** with the reader accepting 1, 2 and 3 and magic `LODT`, `.lodm` envelope
**1** / payload **1**, manifest `# lodgen manifest 2`, texture-array sidecar
`# lodgen texture arrays 5`.

Step 4's second question — *does the consumer know this version?* — has a real
answer here and it is in the plan twice (§1.5 and §5 item 16): **FO4CS's shipped
`.lodl` parser pins `kVersion = 1u` and refuses 2 and 3.** The zero-effort way
back needs no rebuild on our side: `WW_LODL_VERSION=1`.

### 0.4 The deliverable

`docs/FO4CS_IMPROVED_LOD_PLAN.md` — sha256 `cded3dee1c155602` before the footer's
final edit; **72,969 bytes, 1,195 lines, CR 0, NUL 0** as finished (measured, not typed). `docs/` is
LF-only and the file was written and spliced in binary throughout.

---

## 1. Decisions the plan makes, and why

1. **Six rungs, not five.** The brief proposed R0–R5; the plan keeps that shape
   and that numbering. Nothing was added or merged, so a reader of the brief and
   a reader of the page are talking about the same rungs.

2. **The rungs are ordered by when the far field becomes MEASURABLE, not by when
   it becomes pretty.** R0 draws nothing at all and its whole flight is one log
   line. That is deliberate: every later rung's gate is a census cross-check
   (CENSUS 6.2), and a cross-check against a row nobody has seen print is not a
   gate.

3. **One END-menu row, plus four bungo asked for by name.** The standing rule is
   rows only and no new row without him. His 2026-09-11 10:4x spec says *"a menu
   row + INI key each"* for the four screen-size fade thresholds, so the plan
   proposes five rows total and flags the four as needing his confirmation rather
   than assuming it (§6 item c). Everything else is INI-only policy, and every
   key in the page is labelled a PROPOSAL.

4. **Every INI key is named against a ruling or a contract section, and none is
   named against a preference.** Where a default is not known — the shadow
   tolerance, the two residency budgets, the inner-band blend width, the far
   shadow map's resolution — the page writes "to be measured" instead of a
   plausible number. A plausible number in a plan becomes a shipped default
   nobody measured.

5. **The shadow rulings of 14:4x -> 15:0x are law in three places, not one.**
   R1 carries the "exactly one shadow representation per placement" rule and the
   cell-range drop-out (because R1 is where the cell ranges are first read); R3
   carries the height-source change and the hybrid default (because R3 is where
   the shadow pass and the resident height sheet exist); §3 item 3 carries the
   grid-edge blend band (because that is a seam and the seams have their own
   section, which is where he asked these to be noted). The two millisecond
   figures in his paragraph are reproduced **with the word "estimated" beside
   each**, and the page says in the same breath that the first FO4CS capture
   round measures them.

6. **The plan states the finding that makes R3 and R5c awkward, in the room,
   rather than letting a flight discover it.** The cluster ladder is correct and
   at a one-pixel tolerance its first step is not selected anywhere in the
   Commonwealth, because "full detail" in the library is already Bethesda's LOD
   mesh at 47.7 triangles for a whole building (NATIVE 3.5.4). The plan therefore
   (a) puts the near-`MODL` question first in §6, (b) says R5c should not be
   chartered before it is answered, and (c) writes R3's flight step 2 refuter as
   *"no change at all is the finding arriving in the game, not a bug in the cut"*.
   Shipping R5c into a library with nothing to select would read as a failure of
   the mechanism.

7. **The identity trap is stated as a trap.** NATIVE 4.1c has two identities and
   the plan says which one the far shadows key on (the instance INDEX, u32,
   unique file-wide) and which one is only a join to the engine's own chunk
   (`cold.identity`, 16-bit, unique only inside the stock chunk, with **7 of
   3,526** Sanctuary instances colliding inside a `.lodi` chunk). FO4CS's
   existing identity consumer reads 16 bits out of vertex colour R+G, so this is
   a real interop decision and it is named as an RE candidate in R1.

8. **"Ring" is kept as a census BIN and never as a selection rule**, and the
   glossary says so, because it is the vocabulary bungo and the stock bake both
   use and deleting it would make the page unreadable to him. NATIVE 4.4 forbids
   selecting by it; CENSUS 1.1 already carries that distinction and the plan
   quotes it rather than re-deciding it.

9. **A glossary, because four things are called a "level" and two are called a
   "mask"** — and because FO4CS's own `FarField` module is the far SHADOW module,
   not the far LOD module. That collision would have cost a reader of the FO4CS
   session real time, and it is the first row of §7.

10. **No `src/` line number anywhere in the page, in either repository.** Lane
    BAKEPERF1 owned `src/` here while the page was written, and the FO4CS tree is
    another project's live worktree. Every claim is anchored to a contract
    SECTION or to a ruling paragraph quoted verbatim, per `ww-census-contract` §6.

11. **Section 5 is a table with an owning lane AND a "blocks" column.** Naming
    what is owed is only half of it; an FO4CS session needs to know which of the
    seventeen items stops a rung and which merely weakens a cross-check. Six of
    the seventeen block something.

---

## 2. Gates

Script: `scratchpad/plan_fo4cs_20260911/plan_gate.py`, read-only on the tree,
every floor on a copy in memory (`ww-census-contract` §2's rule: never sabotage
the file on disk, because a turn that ends early leaves the tree poisoned).

| gate | result | floor |
|---|---|---|
| **G1** every contract citation resolves to a section that exists | **PASS** — 73 distinct numbered citations across eight pages plus 4 named `BTD` headings, **0 unresolved** | a `NATIVE 99.99` and a `BTD Nonexistent heading` appended to a copy: **2 caught** |
| **G2** every rung carries all nine parts | **PASS** — 6 rungs, **0 short** | one `**MUST NOT.**` label removed from R0 on a copy: **1 caught** |
| **G3** no invented vocabulary | **PASS** — 158 backticked identifiers, 38 keys the page declares itself, **0 found in no contract** | `frobnicatorWidgetCount` appended to a copy: **1 caught** |
| **G4** plain language, actionable without this chat | held — see below |

**G1 and G2 each found a real defect before they passed.** G1's first run was
clean only because two citations had already been corrected by hand during
writing (`CARDS 7.6` and `LODM 7.8`, neither of which is a heading — §7 of both
pages is a numbered LIST of invariants, so the correct forms are `CARDS 7,
invariant 6` and `LODM 7, invariant 8`). G2's first run reported **R5 short by
READS and DOES**, which was true: R5 had been written as three lettered
sub-waves with no reads/does block of its own. Both are fixed and the gate is
green on the fixed page.

**G3 found two defects in ITSELF and one in the page**, which is the reason a
gate gets a floor:

* `rep[0..3]` was reported invented. It is not — NATIVE 4.4 uses it verbatim.
  The token splitter took `.` before `[`, so `rep[0..3]` reduced to `3]`. Fixed;
  the comment in the script names the case.
* `fBlockLevel0Distance` was reported invented. It is the engine's own
  `[TerrainManager]` key, quoted with bungo's live values in FO4CS's
  `Codex/lod-fo4-vs-fo76-comparison.md`, which is not one of the eight contract
  pages. Rather than widening the corpus silently, the script now carries an
  `ENGINE_VOCAB` set of exactly one entry **with the file it is quoted from in a
  comment beside it**, so the allowlist cannot grow without someone writing down
  where a word came from.
* `project_hybrid_lod` was reported invented, and that one was the page's fault:
  a memory file name set in backticks reads as an identifier. Rewritten as
  prose ("the hybrid-LOD memory").

**G4, how it was held rather than scripted.** Every bungo quotation is verbatim
with its timestamp. No label was coined by this lane: the census words are
`docs/LODGEN_CENSUS.md`'s, the file and table names are the contracts', the arm
words are CONSTITUTION 10's, and the one place the page needed a word that does
not exist — the caster count per source, and the two shadow costs — it describes
them in plain English and names them as owed to the census page instead of
minting three keys (§5 item 17). Every number in the page carries the section it
came from. The two millisecond figures that are estimates say so twice.

---

## 3. Owed

### 3.1 Owed to the FO4CS side, by this plan

Nothing. The plan is the deliverable; it defines no file and asks for no change
in the FO4CS tree.

### 3.2 Owed to the generator side, collected by the plan

Seventeen items, in §5 of the page with an owning lane and a "blocks" column.
The six that BLOCK something:

| item | blocks | owner |
|---|---|---|
| the decoder's cell rule (the Boston pair refuses at instance 3359) | R3's occluder gate — it is the only pair with boxes | NATIVE1d |
| no bake writes a card layer (`cardLayer` 0xFFFF everywhere) | R4 entirely on a Commonwealth bake | OBJM / OBJC / OBJP |
| the pyramid's height sheet is opt-in (`--vt-height`), and the CLI table does not list the flag | R3's terrain shadow march beyond the inner band | a generator lane for the CLI row; bungo for the bake |
| no `.lodt` container or VT `.lodm` has ever been written to disk | R2 has nothing to develop against | a bake |
| the aggregate cards do not exist | R4's aggregate half | CARDS-AGG |
| FO4CS's `.lodl` parser pins `kVersion = 1u` | R0 on a v2/v3 file | an FO4CS lane |

**Item 12 is new and this lane found it.** The `.lodt` height sheet (role 4,
`R16_UNORM`, `height/8 + 32767`) is written only under `--vt-height`, and the
CLI table at VT 5 does not carry that flag although VT 2.2 names it. His 15:0x
ruling makes that sheet the shadow march's height source beyond the inner band,
so a bake that does not ask for it leaves R3 with no source — and it more than
doubles a tile (184,960 bytes of height against 138,720 for the three
colour-class sheets together). Both halves are in §5 and §6 item (e).

**Item 17 is also new and is owed to the census page here, not to a writer.**
His 14:4x ruling asks for a caster count per SOURCE and his 15:0x ruling makes
the march's cost and the shadow map's cost the two numbers the first capture
round has to produce. `docs/LODGEN_CENSUS.md` carries none of the three. The plan
names them in plain English rather than coining keys, so the census-page lane
that adds them owns their spelling.

### 3.3 Owed to bungo

Fourteen open rulings, §6 of the page, (a) through (n). Four are new and are this
plan's own; ten are carried from the generator lanes and are already in
HANDOFF.md's owed list, restated here so an FO4CS session has them in one place.

---

## 4. Mistakes

**1. Two citations written in a form the checker cannot resolve, and that a
reader cannot follow either.** `CARDS 7.6` and `LODM 7.8` were written as if §7
of those pages had numbered subsections. It does not: §7 in both is a numbered
LIST of invariants inside one section, so `7.6` and `7.8` name nothing. Caught by
hand while writing the gate, before the gate's first run. What is true instead:
the forms are `CARDS 7, invariant 6` and `LODM 7, invariant 8`. The rule that
prevents it: `ww-census-contract` §2 already warns that a citation form the
checker cannot see is an UNCHECKED citation — the warning is about non-numbered
headings and it applies equally to list items inside a section.

**2. R5 was written without its own READS and DOES blocks.** It was written as
three lettered sub-waves (R5a/b/c) and the nine-part shape was lost. Caught by
gate G2's first run, which is what G2 is for. The rule that prevents it: write
the nine labels into the rung as a skeleton BEFORE the prose, so a rung cannot be
finished short.

**3. A memory file name in backticks.** `project_hybrid_lod` set as code reads as
an identifier and was correctly reported as invented vocabulary by G3. Caught by
G3's first run. The rule: backticks mean "this is a name the machine uses"; a
document name is prose.

**4. The gate's own token splitter was wrong, and it accused the page.** It took
`.` before `[`, so `rep[0..3]` — which NATIVE 4.4 uses verbatim — reduced to
`3]` and was reported as invented. This is the failure `ww-census-contract` §2
names explicitly ("the heading regex is where this goes wrong … you will spend a
round fixing citations that were correct"), arriving in a different regex. Caught
because the reported token was obviously a contract word. The rule: when a gate
accuses a claim you can see in the source page with your own eyes, suspect the
gate first and check the extractor before editing the document.

**5. The ENGINE_VOCAB allowlist was very nearly a silent corpus widening.** The
quick fix for `fBlockLevel0Distance` was to add another file to the corpus set,
which would have admitted every identifier in that file at once and never said
so. What was done instead: a one-entry set with the source file named in a
comment beside it. Not a mistake that shipped, but the wrong instinct, recorded
because it is the instinct that makes a gate stop gating.

---

## 5. Finished-work skill review

**Skills loaded and used.**

* `ww-contract-provenance` — the five steps ran as written: hashes and line
  counts first, every claim anchored to a section rather than a line number
  (which is this page's variant of step 2, because the page cites contracts and
  not sources), the version constants re-read LAST, and the hashes diffed end to
  end. Step 4's second question — does the consumer know this version — produced
  a real finding that is in the page twice.
* `ww-census-contract` — §4 of the plan is its shape: the read-from citations as
  a script-checked column, the bake half as what makes the runtime half
  falsifiable, the "zero is ambiguous and the file resolves it" cross-check
  (`no_boxes`), §6's rule about citing a section that does not exist, and §7's
  "what the page must say out loud" (which fields must refuse TODAY, which
  counters are bins rather than rules, what is owed and to whom, what is
  deliberately NOT owed).
* `fo4cs-census-field` — §4.2 of the plan, the six rules and the movement case
  shown red with the assignment removed.
* `fo4cs-menu-row` — read, and it changed the page: the rows-only rule, the
  no-row-without-him rule, and §8's trap (a row that passes no key gets no
  generated restart note, is invisible to the search index and to the key-table
  generator, and lands on a list pinned by an exact count). That trap is written
  into R3's key table, where four new rows arrive at once.
* `fo4cs-ini-edit` — read, and it is why this page proposes keys and writes none.
* `fo4cs-wave-integrate` — read; it is why a rung is one wave with one build and
  one deploy, and why the plan does not propose two rungs in a wave.
* `fo4cs-flight-brief` — every flight in §2 follows its shape: restart first, the
  step that decides the most first with the reason it is first, one action and
  one observation per step, a refuter per step in plain words including the one
  that kills our own claim, exactly what to send back and what to name it, and an
  INI bisect he can run with no build from us (R3 step 4).
* `fo4cs-altered-capture` — every picture gate in the page: the control FIRST
  against a measured noise floor, §3d's rule that a failing control is the wrong
  permutation rather than a noisy method, and §4's acceptance bar that forbids a
  fix which shortens the far shadows' reach.

**The skill that should have existed, and now does not need to be re-derived.**
Building the gate for this page — a citation checker over a document that cites
EIGHT sibling pages, including non-numbered headings and numbered list items
inside a section, with three floors on in-memory copies — was re-derived from
`census1_20260911/page_gate.py` by hand, and the four traps it cost (the
heading-with-a-period case, list items that look like subsections, the token
splitter's operator order, and the temptation to widen the corpus instead of
declaring an allowlist) are exactly the shape `ww-census-contract` §2 warns
about but does not cover for a MULTI-page citation set.

**It is not written as a new skill, and the reason is named rather than
silent** (rule 1a allows declining if the procedure will not recur, and this one
will): the right home is an amendment to `ww-census-contract`, not a sibling
skill, because it is the same gate one page wider. **Proposed amendment, for the
director to apply to BOTH trees** — a new section 2a, "when the page cites more
than one sibling":

1. Resolve NAMED headings as well as numbered ones, lower-cased and matched on
   the longest prefix, because half of `docs/LODGEN_BTD_FORMAT.md`'s sections
   have no number at all.
2. A numbered LIST inside a section is not a subsection. `CARDS 7.6` and
   `LODM 7.8` name nothing; the citable forms are `CARDS 7, invariant 6` and
   `LODM 7, invariant 8`. State this in the page's own "how to read a citation"
   block so the next writer does not invent the dotted form.
3. When a vocabulary gate accuses a token you can read in the source page with
   your own eyes, **fix the extractor, never the document**. The operator order
   in the token splitter (`[` before `.`) is one instance; the heading regex's
   trailing period is the instance already in the skill.
4. A word that is real but belongs to nobody's contract — an engine INI key, a
   game-side symbol — goes in a NAMED allowlist with the file it is quoted from
   in a comment beside it. Never widen the corpus to admit it: widening admits
   every identifier in that file at once and says nothing.

**Nothing else was re-derived from memory.** The lane wrote no procedure it had
to work out twice.
