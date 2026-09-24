- PLAN-FO4CS LANDED, DOCUMENTS ONLY, NO BUILD, EXE UNTOUCHED.
  `release/NifSkope.exe` is still LODUI1's **2026-09-11 13:24:12**; this lane
  compiled nothing, launched nothing and wrote no file under `src/`, `res/`,
  `tests/` or `tools/`. Lane BAKEPERF1 held `src/` and `NifSkope.pro` throughout
  and neither was touched. The FO4CS repository
  `E:\Projects\Fo4CommunityShaders` was read and **not written**. Report
  `scratchpad/lane_plan_fo4cs_report.md`; markers and gate
  `scratchpad/plan_fo4cs_20260911/`.

  **SHIPPED.** `docs/FO4CS_IMPROVED_LOD_PLAN.md` — new, **72,969 bytes, 1,195
  lines, CR 0**. The plan the FO4CS session builds Improved LOD from: one module,
  its own master switch, ships off (bungo 2026-09-09 15:34), built LAST against
  frozen contracts, defining no byte and no field — where it and a contract page
  disagree the contract wins, and the page says so in its first paragraph.

  **SIX RUNGS, each one FO4CS wave with its own flight**, each with all nine
  parts (reads / does / switch and keys / fallback and its arm word / census /
  gates / flight / must not / RE candidates): **R0** loaders + staleness, nothing
  drawn, the flight is one census line; **R1** vanilla far objects suppressed,
  native far field drawn, ONE shadow representation per placement; **R2** terrain
  — the `.lodt` pyramid beyond, the runtime blend from the `.lodl` weights in the
  inner band, cross-faded; **R3** screen-size selection, sphere/cone/occluder
  culling, the four fades, and the hybrid far shadow pass; **R4** cards; **R5**
  the three 5x5-grid rounds, after R1-R4 have flown.

  **HIS SHADOW RULINGS OF 14:4x -> 15:0x ARE IN IT AS LAW, IN THREE PLACES.**
  R1: every placement has exactly one shadow representation at a time, chosen the
  same way its draw is chosen — cells inside the loaded grid drop out of the
  far-shadow casters by the same cell-range table that drops their draws, the
  cascades never contain LOD geometry, casters counted per source, self-shadow
  excluded by identity. R3: ONE height source for draw and shadow (the `.lodl`
  finest level in the inner band, the pyramid's resident height sheet beyond,
  `HeightMap.dds` as the fallback arm), and the HYBRID as the default — march the
  terrain, render only the object cluster cut plus the cards into a far shadow
  map, combine with a max, map-only as the fallback arm. §3's seams table: the
  grid-edge band blends shadows the way it blends draws. **His two millisecond
  figures are labelled ESTIMATES in both places they appear**, with the first
  FO4CS capture round named as the thing that measures them.

  **GATES, three, all green, all with floors that FIRED**
  (`scratchpad/plan_fo4cs_20260911/plan_gate.py`; read-only on the tree, every
  floor on an in-memory copy so a turn ending early cannot poison it):
  G1 **73 numbered citations over eight contract pages + 4 named headings, 0
  unresolved** (floor caught 2); G2 **6 rungs, 0 short** (floor caught 1);
  G3 **158 backticked identifiers, 38 keys the page declares itself, 0 found in
  no contract** (floor caught 1). G2 found R5 genuinely short of READS and DOES
  and it was written. G3 found two defects in ITSELF before it found one in the
  page — a token splitter that took `.` before `[` and therefore accused
  `rep[0..3]`, a phrase NATIVE 4.4 uses verbatim.

  **PROVENANCE.** All eight contract hashes taken before reading and re-derived
  after the last edit: **none moved**. Version constants re-read last of all, and
  the second question that step asks produced a finding that is in the page
  twice: **FO4CS's shipped `.lodl` parser pins `kVersion = 1u` and refuses 2 and
  3**; `WW_LODL_VERSION=1` is the zero-effort way back and needs no rebuild on
  our side. `HANDOFF.md` MOVED during the lane
  (`81fdc08dfa8b7707`, 297,028 B -> `8a7fce2f67c7f898`, 302,554 B) because the
  director spliced the shadow rulings in mid-lane; the footer states both values.
  **No `src/` line number in the page, in either repository.**

  **FIVE THINGS FOR THE DIRECTOR.**
  1. **A NEW OWED ITEM nobody had.** The `.lodt` height sheet is opt-in
     (`--vt-height`) and `docs/LODGEN_TERRAIN_VT.md` §5's CLI table does not list
     the flag although §2.2 names it. His 15:0x ruling makes that sheet the
     shadow march's height source beyond the inner band, so **a full bake without
     it leaves R3 with no source** — and it more than doubles a pyramid tile
     (184,960 B of height against 138,720 for the three colour-class sheets
     together). Two halves: a generator lane for the CLI row, and bungo for the
     bake. This belongs in the bake instruction before he bakes.
  2. **THREE CENSUS WORDS ARE OWED HERE, not to a writer.** His 14:4x ruling asks
     for a caster count per SOURCE; his 15:0x ruling makes the march's own cost
     and the shadow map's own cost the two numbers the first capture round must
     produce. `docs/LODGEN_CENSUS.md` carries none of the three. The plan names
     them in plain English rather than coining keys, so the census-page lane owns
     their spelling.
  3. **R5c should not be chartered before the near-`MODL` call is taken.** The
     ladder is correct and at his own 1-px tolerance its first step is selected
     nowhere in the Commonwealth (median level-1 cluster 3.80 percent of the
     model diagonal, one pixel only past 52,100 units, because "full detail" is
     already Bethesda's LOD mesh at 47.7 triangles a building). Shipping the
     inward extension into that library would read as a failure of the mechanism.
     It is §6 item (a) and it is first in the list for that reason.
  4. **FOUR END-MENU ROWS need his word.** He asked for *"a menu row + INI key
     each"* for the four screen-size fade thresholds on 10:4x; the standing rule
     is no row without him, so the plan proposes and does not assume. Every other
     key in the page is INI-only policy and every one is labelled a PROPOSAL.
  5. **A SKILL AMENDMENT IS READY AND WAS NOT APPLIED**, because the brief
     confined this lane to its own two files:
     `scratchpad/plan_fo4cs_20260911/SKILL_AMENDMENT_ww_census_contract.md` — a
     new section 2a for `ww-census-contract`, "when the page cites more than one
     sibling", carrying the four traps this lane paid for (named headings as well
     as numbered ones; a numbered LIST inside a section is not a subsection;
     fix the extractor and never the document when a vocabulary gate accuses a
     word you can read in the source; a real word that belongs to no contract
     goes in a NAMED allowlist with its source file, never a silent corpus
     widening). **The director applies it to BOTH trees** (CONSTITUTION 1a).

  **UNCOMMITTED:** this lane adds `docs/FO4CS_IMPROVED_LOD_PLAN.md` and its own
  `scratchpad/plan_fo4cs_20260911/` plus `scratchpad/lane_plan_fo4cs_report.md`.
  Nothing committed (CONSTITUTION 8). **RESTART: not needed** — no exe changed
  and bungo's window is unaffected.
