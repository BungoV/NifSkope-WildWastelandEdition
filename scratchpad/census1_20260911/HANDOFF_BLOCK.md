# HANDOFF.md block — lane CENSUS1

(Lane text, verbatim, for the director to splice into the top block's lane ledger.)

---

**CENSUS1 LANDED, DOCUMENTS ONLY, NO BUILD, EXE UNTOUCHED.** `release/NifSkope.exe`
is still LODUI1's **2026-09-11 13:24:12**; this lane compiled nothing, launched
nothing and wrote no `src/` file — lane BAKEPERF1 held `src/` and `NifSkope.pro`
throughout, and the director's amendment at launch voided the brief's
"header-only field … then do it" clause, so every missing generator census word is
NAMED as owed instead. Report `scratchpad/lane_census1_report.md`; markers
`scratchpad/census1_20260911/`.

**Shipped:**
* `docs/LODGEN_CENSUS.md` — **60 field names** in seven log rows
  (`[ImprovedLOD]`, `.Ring`, `.Cluster`, `.Cards`, `.Residency`, `.Shadow`,
  `.Fade`), each with name / unit / what it counts / the contract section it is
  read from / the scene change that must MOVE it / its complete refusal list /
  a default that accuses its plumbing. Covers every item of bungo's "We need
  them" (2026-09-11 10:0x) plus his screen-size fade spec (10:4x), his zoom answer
  (10:5x, the finest-level residency budget) and the grid-edge seam band. Its
  second half inventories the census the generator already prints and turns it
  into 14 cross-checks a runtime number can be tested against.
* `tests/spells/lodgen_census_check.py` — read-only, no exe, decodes a pair with
  the independent decoder and checks the census line against the bytes.
* one pointer paragraph in `docs/LODGEN_NATIVE_LODO_LODI.md` §7.
* new skill `.claude/skills/ww-census-contract/SKILL.md` (repo tree).

**Gates, all green, all with floors that fired:**
* **C2** on the 9-chunk Sanctuary pair (`scratchpad/native1b_20260911/gate/final/native`,
  NATIVE1b 10:17:44): **59 checks, 0 failures, 31 census words the files cannot
  carry**; `--self-floor` caught all three doctored claims. Same numbers on the
  NEWEST pair on disk, LODUI1's `scratchpad/lodui1_20260911/stage_times/region`
  (13:25:22).
* **C1 / C3** (`scratchpad/census1_20260911/page_gate.py`): 60 field rows, 0 blank
  cells over the six gate columns, 109 citations all resolving; floors caught 5
  blanks and 2 bad citations on in-memory copies.
* Logs `scratchpad/census1_20260911/logs/`.

**FOR THE DIRECTOR, three things:**

1. **A defect, found and not fixed** (not this lane's file).
   `tests/spells/lodgen_native_decode.py` REFUSES the downtown-Boston pair
   `scratchpad/native1b_20260911/gate/final/occ` — 33,123 placements, the only
   pair on disk with occluder boxes — at instance 3359, *"out of (cell, drawKey,
   ref, part) order"*. Measured by hand: its neighbour sits at **fy 2.999985**
   cells from the chunk origin after quantisation, one step below the cell line at
   3.0, so the decoder re-derives cell 4 while the writer sorted the float position
   into cell 0. Cell-level twin of the chunk-boundary effect NATIVE 4.1c / NATIVE 6
   already document; the fix is a DECODER rule — take an instance's cell from the
   cell RANGE it falls in — not a format change. **No gate has ever decoded that
   pair**: NATIVE1b ran `lodgen_native_cut.py` on it and `lodgen_native_decode.py`
   only on Sanctuary.
2. **Five owed generator words**, in the page's §6.3 with the exact bytes each
   would take: per-MNAM-slot instance totals in the `.lodi` header (the honest
   version of "per-ring totals" — a ring is camera-relative and no file can state
   one); a per-base full-detail triangle count (the room exists: `crossPx16[4]` is
   eight bytes written as all zeros, NATIVE 11 Deviation 5); a card count in the
   `.lodo` header; the aggregate ring-3 impostors AND the forested-cell count bungo
   asked for at 08:3x (both owed to CARDS-AGG); and a watertight bit in the mesh
   row's free flags.
3. **Mirror the new skill** `ww-census-contract` to
   `E:\Projects\Claude\.claude\skills\` (CONSTITUTION 1a — the two trees drift and
   nothing syncs them).

**RESTART:** not needed. No exe changed; bungo's window is unaffected.
**Uncommitted:** this lane adds 3 new files under `docs/`, `tests/spells/` and
`.claude/skills/`, edits `docs/LODGEN_NATIVE_LODO_LODI.md`, and writes its own
`scratchpad/census1_20260911/`. Nothing committed (CONSTITUTION 8).
