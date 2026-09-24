# Lane CENSUS1 — the performance census page for FO4CS's Improved LOD module

Brief: `scratchpad/brief_census1.md`, plus the director's ONE AMENDMENT at launch
(lane BAKEPERF1 owns `src/` and `NifSkope.pro` concurrently, so item 2's
"header-only field … then do it" clause is VOID: this lane touches no writer, no
`src/` file, no build, no exe. A missing generator census word is NAMED as owed,
nothing more).

Files this lane owns: `docs/LODGEN_CENSUS.md` (new),
`tests/spells/lodgen_census_check.py` (new), one pointer paragraph in
`docs/LODGEN_NATIVE_LODO_LODI.md` section 7, and `scratchpad/census1_20260911/`.

## 0. Pre-registered gates

Written before any number was measured, from the brief's own Gates section.

| gate | what it asserts | how it is run | its floor |
|---|---|---|---|
| **C1** | every field row of `docs/LODGEN_CENSUS.md` has all six gate columns filled (name / unit / read-from / moves / refusal / default); blanks = 0 | `scratchpad/census1_20260911/page_gate.py` | a deliberately blank row is appended to a copy of the page and the count must rise above 0 |
| **C2** | `tests/spells/lodgen_census_check.py` green on the real Sanctuary `.lodo`/`.lodi` pair | the checker, `--self-floor` | a doctored census line shown red — three claims doctored one at a time, each must be caught by name |
| **C3** | every "read from" citation in the page resolves to a contract-page section that exists | `scratchpad/census1_20260911/page_gate.py` | a citation to a section number that does not exist is added to a copy and must be reported MISSING |

Rules pre-registered with them: no build; no `src/` file touched; no FO4CS file
written; never `git stash`; never commit.

### The pair this lane gated on

The brief says the real Sanctuary pair is under `scratchpad/native1b_20260911/`
and asks which pair a lane baked most recently. Measured (sha256 first 16, size,
mtime):

| pair | `.lodo` | `.lodi` | baked by | when |
|---|---|---|---|---|
| `scratchpad/native1b_20260911/gate/final/native/Native/` | `1efe74130a6b85f5` 9,657,316 B | `7c42d7265b7ce686` 128,256 B (3,526 placements) | NATIVE1b | 2026-09-11 10:17:44 |
| `scratchpad/native1b_20260911/gate/final/occ/Native/` | `1efe74130a6b85f5` 9,657,316 B | `c12e494b73641b70` 1,091,072 B (33,123 placements, 280 occluder boxes) | NATIVE1b | 2026-09-11 10:18:21 |
| `scratchpad/lodui1_20260911/stage_times/region/` | `1efe74130a6b85f5` 9,657,316 B | `276ec262709c76b7` 37,248 B (678 placements) | **LODUI1 — the newest pair on disk** | 2026-09-11 13:25:22 |

**The three `.lodo` files are byte-identical.** That is not a coincidence and it
is worth stating: `docs/LODGEN_NATIVE_LODO_LODI.md` §6 says the library is built
from the full worldspace census in every bake and is not region-scoped, and three
independent bakes three hours apart, over three different cell sets, produced the
same 9,657,316 bytes. The `.lodi` is the region-scoped half and all three differ.

The newest pair is **LODUI1's**, 13:25:22, from its stage-times leg. It is a
four-cell region, so it exercises the instance half thinly; the **9-chunk
Sanctuary pair (NATIVE1b 10:17:44) is what C2 gates on**, and the Boston pair is
run beside it because it is the only one that writes a non-zero occluder count.

## 1. The field table

`docs/LODGEN_CENSUS.md` sections 2 to 5. **60 field names**, in seven groups, one
group per row kind, every one with all seven columns filled (name, unit, what it
counts, read from, how it MOVES, refusal words, default).

| group | row | fields | what it answers |
|---|---|---|---|
| 1 | `[ImprovedLOD]` | 11 | the serving arm per module (objects / terrain / cards / arrays), the worldspace, the two container versions, the pairing rule, staleness with the file and the hash named, and `frames` / `windowMs` as the denominator for everything below |
| 2 | `[ImprovedLOD.Ring]` | 9 | per distance bin: instances considered, culled by frustum, culled by occluder, culled by screen size, drawn, triangles — plus `ringEdges` and the two totals |
| 3 | `[ImprovedLOD.Cluster]` | 9 | the pixel tolerance in force, the live projection scale, `levelMax`, clusters selected per level, the two partition faults, triangles, draw calls, buffer overflow |
| 4 | `[ImprovedLOD.Cards]` | 5 | cards drawn per bin and in total, bases on a card, view frames sampled, aggregate cell impostors |
| 5 | `[ImprovedLOD.Residency]` | 10 | resident pyramid tiles and bytes per level with their denominators, library / instance / card / array bytes, the budget, and evictions |
| 6 | `[ImprovedLOD.Shadow]` | 5 | the shadow view's own tolerance, clusters, triangles and draws, and whether identity was unique |
| 7 | `[ImprovedLOD.Fade]` | 11 | the four fade-class thresholds as a fraction of screen height, the hysteresis, cross-fades in flight and the window's peak, the grid-edge band, suppressed engine draws, the primitive read-back and whether the engine's far tree is actually hidden |

Of the 60, **7 are per-bin families** (four numbers each: `ringInstances`,
`ringCulledFrustum`, `ringCulledOccluder`, `ringCulledScreen`, `ringDrawn`,
`ringTriangles`, `cardsDrawn`) and **4 are per-level families**
(`clusterSelected`, `tilesResident`, `tilesPresent`, `tileBytes`). The remaining
49 are scalars.

Three things in the table are worth naming because they are decisions, not
transcription:

* **A "ring" is a census BIN, not a selection rule.** NATIVE 4.4 forbids selecting
  by ring — the measured 172x spread of bound heights inside one chunk makes a
  per-chunk distance wrong by two orders of magnitude. So the rings survive only
  as the bins these rows are reported in, `ringEdges` states the four distances in
  force, and a per-ring number printed without it refuses `no_bins`.
* **Every per-bin row obeys one arithmetic law**, stated in the page:
  `considered == frustum + occluder + screen + drawn`, per bin. A row where that
  fails has lost instances between two counters and says so.
* **The defaults are five words that all read as a fault** — `uncounted`,
  `unread`, `unset`, `unchecked`, `unwired` — and `0` is a default nowhere except
  `frames` and `engineFarHidden`, where zero is a measurement. That is the
  `TrueAimCensus{}` lesson from `fo4cs-census-field`: a quiet zero composes into a
  row that looks like a healthy quiet build.
* **17 cells say "no refusal"** with an em dash rather than being left blank, and
  **11 cells say `runtime`** in the read-from column rather than being left blank.
  Both are deliberate: the gate cannot tell "we could not trace this" from "there
  is nothing to trace" unless the page says which.

## 2. What the generator promises, and what is owed

### 2.1 What it already prints (page §6.1, §6.2)

Eleven census outputs exist today and are inventoried in the page from the frozen
logs, not from memory: the three `native:` / `native-ladder:` / `native-occluders:`
lines, the `vt:` line (including the twelve road fields and the mask-rule census),
the per-chunk `cover ...` and `roads ...` lines, `arrays written:`, `merged:` /
`far rings:`, `stage times:`, the per-chunk manifest, and the per-card sidecar.

Page §6.2 turns those into **14 cross-checks** between a runtime field and a bake
number, each checkable live with the file open — for example `instancesTotal <=`
the `.lodi`'s `instanceCount`; `clusterTriangles` at tolerance 0 equals the drawn
set's level-0 triangle count; `tilesResident <= tilesPresent` at every level;
`stale` may only be raised by the five SOFT keys of NATIVE 5 and never by a hard
one.

The sharpest of them is the occluder one: **a worldspace whose `.lodi` writes zero
occluder boxes forces `ringCulledOccluder` to refuse `no_boxes`**, never to print
0. Sanctuary is exactly that case — 41 distinct LOD meshes, not one watertight —
so a silent 0 there would be indistinguishable from a broken occluder test.

### 2.2 What is owed (page §6.3)

Named, **not added**: the director's amendment voided the "header-only … then do
it" clause, because lane BAKEPERF1 owned `src/` concurrently. No writer, no
`src/` file, no build, no exe was touched by this lane.

1. **A per-slot instance total in the `.lodi` header** (four `u32` in the reserved
   0xB0..0xFF). The brief's example was "per-ring instance totals"; the honest
   version is that a *ring* is camera-relative and no file can state one, while
   *how many instances draw from each of the four MNAM slots* is static and is the
   thing the bins approximate. Without it `ringInstances` has no bake-side
   denominator at all.
2. **A per-base full-detail triangle count.** The base row is 32 bytes and full,
   but `crossPx16[4]` is eight of them and is written as all zeros today
   (NATIVE 11, Deviation 5), so the room exists without a stride change. Re-using
   it is a format decision, not a lane's.
3. **A card count in the `.lodo` header** (one `u32` in the reserved 0xCE..0xFF);
   `cardBases` has no denominator without it.
4. **The aggregate ring-3 impostors do not exist** — `cardsAggregate` refuses
   `not_baked` until lane CARDS-AGG bakes them — and **the count of forested cells
   bungo asked to see before anything is built (2026-09-11 08:3x) is printed
   nowhere.** Owed to CARDS-AGG.
5. **A watertight bit in the `.lodo` mesh row's free flags.** `no_boxes` can say
   *that* a worldspace has no occluders but not *why*; the generator knows (2,617
   of 2,982 meshes refused for not being watertight) and that fact lives only in a
   census line and a sidecar.
6. Stated as known holes rather than owed items: `crossPx16` is all zeros, so no
   per-base screen-size ladder can be read from the file (the cluster cut replaced
   it); and **nothing names an object's engine fade class** — that is the engine's
   own record, read at runtime, so it is not owed to the generator, and the page
   says so in place so a later reader does not go looking for it in a file.

**One thing the page had to say out loud about cards.** NATIVE 11, Deviation 5:
the bake writes what the stock ring bakes and nothing more, so `cardLayer` is
0xFFFF on every base and `cardCorpusHash` is 0. Until lanes OBJM / OBJC / OBJP
fill those fields, `arm.cards` reads `no_layer`, `cardBases` reads 0 and
`cardsAggregate` reads `not_baked`. Four zeros there would have looked like a
healthy treeless worldspace.

## 3. The checker

`tests/spells/lodgen_census_check.py <out-dir> [--census <log>] [--doctor f=v] [--self-floor]`

Read-only, no exe, no build. It decodes the pair with
`tests/spells/lodgen_native_decode.py` — the independent decoder, written from the
contract rather than from the emitter — and compares every number the bake's
census line printed against the number read out of the bytes.

| run | pair | result |
|---|---|---|
| **C2, the gate** | `scratchpad/native1b_20260911/gate/final/native` (9-chunk Sanctuary, 3,526 placements) | **59 checks, 0 failures, 31 census words the files cannot carry**; `--self-floor` caught all three doctored claims; rc 0 |
| the newest pair | `scratchpad/lodui1_20260911/stage_times/region` (LODUI1 13:25:22, 678 placements) | 59 checks, 0 failures, the same 31; floor caught all three; rc 0 |
| the dense pair | `scratchpad/native1b_20260911/gate/final/occ` (downtown Boston, 33,123 placements, 280 occluder boxes) | **REFUSED, rc 2** — see §4 |

Logs: `scratchpad/census1_20260911/logs/check_sanctuary.log`,
`check_newest_lodui1.log`, `check_boston_occ.log`, `page_gate.log`.

**The 59 checks.** Both file sizes and both header `fileBytes`; base, mesh,
cluster, material and vertex counts; the triangle total summed out of the cluster
table; `levelMax`, `ladderGroup` and the LADDER flag; `coneOpen`; roots and the
full-detail triangles they cover; `maxError`; meshes with and without a ladder and
how many of the flat ones are 16 triangles or fewer; **eight levels x (clusters,
triangles, meanError)**; instances, present chunks of dense, max instances a
chunk, max scale, max baseId, `PARTIAL`; occluder count, the per-cell cap, cells
populated, cells with a box, the percentage and cells without; and bytes a
placement.

**The 31 it refuses to claim.** Every bake-time fact the container does not carry
— the five ladder refusal reasons, the five occluder fit refusals, models loaded
and failed, arrivals, census refs, drops, the welded-vertex and UV-conflict
intermediates, the ACMR before/after, the silhouette comparisons against the
source. Each is printed **by name with its reason**. They are counted separately
and never as passes, because a checker that skipped them silently would report a
coverage it does not have.

**The floor.** `--self-floor` doctors `lodo.clusterCount`, `lodi.instanceCount`
and `lodo.level1.triangles` one at a time and requires each to be caught by name;
all three were caught on both green pairs. `--doctor field=value` does one by
hand and inverts the exit code, so the floor can be shown on demand.

**Two refusals it makes rather than guessing**, both exercised:

* two logs under the out-dir disagreeing about a census line → refuse and name
  both (`check_refuse_twologs.log`), because choosing the log that agrees would
  make the check circular;
* the decoder refusing the pair → exit 2 with the decoder's own words, so "the
  files are unreadable" is never reported as "the census is wrong".

### The page gate (C1 and C3)

`scratchpad/census1_20260911/page_gate.py`, run on the finished page:

```
60 field rows, 109 citations (36 distinct), 11 `runtime` cells, 17 cells that state "no refusal"
C1 blanks 0, C3 missing 0
C1 PASS (0 blanks, floor caught 5)
C3 PASS (0 missing, floor caught 2)
```

Both floors run on a copy of the text in memory, never on the file: a blank row
appended raises the blank count to 5, and a row citing `NATIVE 99.99` raises the
missing count to 2.

## 4. Owed

### 4.1 To the next generator lane (the page's §6.3)

The five items of §2.2 above, in the page, with the exact bytes each would take.

### 4.2 A defect this lane found, and did not fix

**The independent decoder refuses the Boston pair.**
`tests/spells/lodgen_native_decode.py` raises *"instance 3359 out of
(cell, drawKey, ref, part) order"* on
`scratchpad/native1b_20260911/gate/final/occ/Native/Commonwealth.lodi`
(33,123 placements, the only pair on disk that writes occluder boxes).

Measured rather than asserted — instance 3358 and its neighbours, decoded by hand:

| instance | recomputed cell | drawKey | ref | position, in cells from the chunk origin |
|---|---|---|---|---|
| 3357 | 0 | 1316 | `0011f01b` | fx 0.656565, fy 3.041184 |
| **3358** | **4** | 1320 | `0011ed8f` | fx 0.828138, **fy 2.999985** (py 49151) |
| 3359 | 0 | 1320 | `00150d44` | fx 0.828138, fy 3.062486 |

`fy` 2.999985 is one quantisation step below the cell line at 3.0, so the decoder
re-derives cell 4 from the STORED position while the writer sorted on the FLOAT
position, which was on or above the line. It is the **cell-level twin of the
chunk-boundary effect NATIVE 4.1c and NATIVE 6 already document** (7 of 3,526
Sanctuary instances land in a neighbour's bin for exactly this reason), and it is
a decoder rule that is stricter than the format, not a writer defect: the `.lodi`'s
own cell ranges are authoritative and the decoder already checks separately that
they partition the instances in order.

Not fixed here — `lodgen_native_decode.py` is not this lane's file, and the brief
fences this lane to the census page, the checker and one paragraph. **Owed to a
generator lane:** the decoder's sort check should take an instance's cell from the
cell RANGE it falls in, not re-derive it from the quantised position.

It also means **no gate has ever decoded the Boston pair** — NATIVE1b ran
`lodgen_native_cut.py` on it (bounds, cones, the cut, the 280 boxes) and
`lodgen_native_decode.py` only on Sanctuary. That is in §5.

### 4.3 To bungo

Nothing is owed to him by this lane: it produced documents and a read-only
checker, no picture and no decision. The two questions already standing in the
handoff — the `.lodo` built from `MODL` instead of `MNAM` (NATIVE 3.5.4) and the
REFR formID in the hot record (NATIVE 4.1a) — are unchanged by this page, which
only records their consequences for the census.

## 5. Mistakes

Text for `MISTAKES.md`, also in `scratchpad/census1_20260911/MISTAKES_ENTRIES.md`
for the director to splice.

**2026-09-11, lane CENSUS1 — a gate that reported rot that was not there.**
The first run of `scratchpad/census1_20260911/page_gate.py` reported **49 of 50
citations MISSING**, including `NATIVE 7` and `VT 4`, every one of which exists.
The cause was the script's own heading regex: sections in these pages are
`## 4.1 Title` at the second level but `## 4. Title` at the first, with a period,
and the regex demanded whitespace straight after the number. What was true
instead: the page's citations were correct from the start. How it was found: the
report was implausible — a page cannot be wrong about `NATIVE 7` — so the
extractor was suspected before the page was. The rule that prevents it:
`ww-contract-provenance` already says in as many words that a first-run MISSING
report is usually the script (11 of lane DOCS2's 13 were), and the extractor's
rules were re-derived from scratch anyway instead of being read first. **Cost:
one round.** Written into the new skill `ww-census-contract` §2 so the next lane
gets the period for free.

**2026-09-11, lane CENSUS1 — believed a decoder's return shape without reading
it.** The checker indexed `T['cellRanges'][k]['instanceCount']`; the decoder
returns plain `(first, count)` tuples there, not dicts, and the run crashed.
Harmless because it crashed loudly on the first run, but it is the same class as
CONSTITUTION rule 4's third rule — check our own tree before quoting it. The
shapes were three greps away and were assumed instead.

Nothing was written to `src/`, to a writer, to FO4CS, or to any file another lane
owns; nothing was committed; `git stash` was never used.

## 6. Finished-work skill review

**Skills loaded, and what each was actually used for.**

* `fo4cs-census-field` — the whole shape of the field table came from it: the
  written-and-moves rule, one key one meaning, the buffer rule (which is why the
  census is seven rows rather than one), a refusal naming its reason, and a
  default that accuses its plumbing. The five default words and the "0 is never a
  default except where zero is a measurement" rule in §1.2 of the page are that
  skill's `TrueAimCensus{}` lesson written forward. Its "make the row falsifiable
  from itself" section is why `frames`, `windowMs`, `ringEdges`, `tilesPresent`,
  `libraryBudgetBytes` and `crossFadePeak` exist at all.
* `ww-contract-provenance` — steps 1 and 5 (hash and line-count before, re-stamp
  after), and its rule that a page with no writer carries a status banner rather
  than a footer pointing at a design document. It is also why this page cites no
  `src/` line numbers: BAKEPERF1 owned `src/` throughout, and the skill's own
  failure case is a contract written against a file another lane was growing.
* `fo4cs-log-read` — read-only, for the row shape: `[Tag] key=value` on one
  physical line, parsed by keyword and never by field position; and its rules 5
  (a counter's unit is a fact about the instrument) and 11 (never quote an absence
  without the census interval) are the reason `frames=` and `windowMs=` are
  fields.

**The skill that should have existed, and now does.**
`.claude/skills/ww-census-contract/SKILL.md` (new, this lane). Nothing covered
"write the census a CONSUMER project will print, from this side, before its
runtime exists". Every step below was worked out from first principles and will
be wanted again for the terrain module and the water module:

* the seven-column table and why no cell may be blank (two different words for the
  two different "not applicable"s);
* the citation gate, its two in-memory floors, and **the heading-period trap that
  cost this lane a round**;
* the bake half and the cross-check table, including how to look for the check
  where zero is ambiguous and the file resolves it;
* the checker's three verdicts, the `not-derivable` class, and the two refusals
  it must make instead of guessing (never pick the log that agrees; unreadable is
  not wrong);
* run the checker on every pair a lane has baked, not only the one the brief
  names — that is what found §4.2;
* provenance when another lane owns `src/`, and re-stamping after your own edit
  to a page you cite.

It is in the REPO skill tree. Per CONSTITUTION 1a the two trees drift and nothing
syncs them: **the director should mirror it to
`E:\Projects\Claude\.claude\skills\ww-census-contract\SKILL.md`.**

**Declined, with the reason.** No new skill for "read the HANDOFF top block for a
lane's rulings" — that is CONSTITUTION 3's read order and adding a skill beside it
would be a second copy of a rule that already binds. No new skill for the decoder
finding in §4.2 either: fixing a sort rule is ordinary work, and the general
lesson (run the extra pair, then measure before accusing) is §5 of the new skill
rather than a skill of its own.

---

## Final state on disk (for the director's verify-before-believe step)

| file | sha256 (16) | bytes | lines | CR |
|---|---|---|---|---|
| `docs/LODGEN_CENSUS.md` (new) | `521d0a5fa0de05f8` | 35,284 | 448 | 0 |
| `tests/spells/lodgen_census_check.py` (new) | `28acb98bf0fb...` | 22,708 | 478 | 0 |
| `.claude/skills/ww-census-contract/SKILL.md` (new) | `12089da940b0...` | 9,018 | 151 | 0 |
| `docs/LODGEN_NATIVE_LODO_LODI.md` (edited, §7 paragraph) | `067eeaa70e2924fe` | 79,504 | 1,253 | 0 |

All four LF-only, measured with Python byte counts. Re-run from the repo root:

```
python scratchpad/census1_20260911/page_gate.py                          # rc 0
python tests/spells/lodgen_census_check.py \
  scratchpad/native1b_20260911/gate/final/native \
  --census scratchpad/native1b_20260911/gate/final/bake_native.log \
  --self-floor                                                            # rc 0
python tests/spells/lodgen_census_check.py \
  scratchpad/lodui1_20260911/stage_times/region \
  --census scratchpad/lodui1_20260911/stage_times/logs/region.log \
  --self-floor                                                            # rc 0
python tests/spells/lodgen_census_check.py \
  scratchpad/native1b_20260911/gate/final/occ \
  --census scratchpad/native1b_20260911/gate/final/bake_occ.log           # rc 2, the §4.2 finding
python tests/spells/lodgen_census_check.py \
  scratchpad/native1b_20260911/gate/final/native                          # rc 1, the two-log refusal
```

No build, no `src/` file, no `NifSkope.pro`, no exe launch, no FO4CS file, no
commit, no `git stash`. Marker `scratchpad/census1_20260911/DONE`.
