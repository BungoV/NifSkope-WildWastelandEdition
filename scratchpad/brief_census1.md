# Lane CENSUS1 -- the performance census page for FO4CS's Improved LOD module (a contract, written NifSkope-side)

## Header
- Tree: `E:\Projects\NifskopeWildWastelandEdition`, branch `main`. Nothing is committed. NO BUILD in this lane: it writes documents and one generator-side census check. Exe at launch: from `HANDOFF.md`'s newest lane block.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block -- RESUME, every "RULING bungo 2026-09-11" paragraph on performance and the gap review ((4) "We need them"), the screen-size fade spec, the zoom answer, the grid-edge seam list, the FO4CS-side rulings; `docs/LODGEN_NATIVE_LODO_LODI.md` (v2/v3 as built: what a reader has to count), `docs/LODGEN_TERRAIN_VT.md`, `docs/LODGEN_CARD_SHEETS.md`, `docs/LODGEN_TEXTURE_ARRAYS.md`, `docs/LODGEN_MANIFEST_FORMAT.md` (the census lines each writer already prints); `.claude/skills/fo4cs-census-field/SKILL.md` (the 2026-09-04 21:33 rule: a field is WRITTEN and MOVES, a refusal names its reason, a default accuses its own plumbing); `.claude/skills/fo4cs-log-read/SKILL.md` if present (the shape of an FO4CS census row); `E:\Projects\Fo4CommunityShaders\Codex\` READ-ONLY for the existing census conventions of the other modules (the row format, the tag inventory) -- read, never write there.
- Skills you MUST invoke: `fo4cs-census-field`, `ww-contract-provenance` (every claim about what a file holds traced to the contract page and its writer line), `fo4cs-log-read` (read-only reference for the row shape).

## bungo's ruling (verbatim)
Gap (4), performance census: "We need them". The director's framing he accepted: "Every FO4CS module ships self-diagnosing counters. Improved LOD has none specified: draws per ring, triangles, resident tiles, card count, cull rate. Defining them now means the runtime is measurable on its first flight instead of argued about."

## The work
1. **`docs/LODGEN_CENSUS.md`**, contract style: the census the Improved LOD runtime prints, one row per frame-window, one field per line: name, unit, what it counts, which file/table it is read from (traced), how it MOVES (the scene change that must change it), its refusal words, and its default-that-accuses-plumbing. Fields at minimum: per ring 0-3: instances considered / culled by frustum / culled by occluder / culled by screen size / drawn; clusters selected per level and the pixel tolerance in force; triangles submitted per ring; cards drawn per ring and aggregate cards; resident pyramid tiles per level and bytes; resident library bytes (finest levels); shadow-view clusters selected; fade class thresholds in force (objects/actors/items/grass, fraction of screen height) and hysteresis; cross-fade count in flight; corpus-hash status per file (match / STALE with the file named); the serving arm per module (native | stock-fallback | off) per CONSTITUTION 10. Each field gets the WRITTEN-and-MOVES test description the runtime must ship.
2. **The generator's half**: the bake census the generator already prints (per-lane census lines: identity, native pair sizes, stage times, roads, cover, VT) is inventoried in the same page as "what the file promises", so the runtime census can cross-check (e.g. instances drawn <= instances in `.lodi`). Where a generator census word the runtime needs is MISSING (e.g. per-ring instance totals in the `.lodi` header), name it as owed to the next generator lane -- do not add it here unless it is header-only and `ww-standalone-writer-gate` can gate it in an hour; then do it and say so.
3. **A read-only checker** `tests/spells/lodgen_census_check.py`: given a bake out-dir, prints the generator census in the page's format and asserts each promised number is derivable from the files (counts from the decoder equal the census line). Gate: green on NATIVE1a's Sanctuary pair; floor = a doctored census line shown red.
4. **`docs/LODGEN_CENSUS.md` provenance footer**, and a pointer paragraph in `docs/LODGEN_NATIVE_LODO_LODI.md` section 7 (the reader's draw checklist) to the census page.
5. Documents: `scratchpad/census1_20260911/WW_CHANGES_ENTRY.md`, `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md`; report `scratchpad/lane_census1_report.md`.

## Gates
- C1 every field has all six columns filled (name/unit/source/moves/refusal/default); a script counts blanks = 0 with a floor (a deliberately blank row counted).
- C2 the checker green on the real pair, floor red.
- C3 every "read from" citation resolves to a contract page section that exists (script-checked).

## Rules
- No build; no changes to writers except the header-only case in item 2, gated standalone. No FO4CS files written. Never `git stash`, never commit. Plain language (the page is read by the FO4CS session's lanes; no agent-coined words -- the field names are the vocabulary).

## Report
`scratchpad/lane_census1_report.md`: `## 0. Pre-registered gates`, `## 1. The field table`, `## 2. What the generator promises and what is missing`, `## 3. The checker`, `## 4. Owed`, `## 5. Mistakes`, `## 6. Finished-work skill review`.
