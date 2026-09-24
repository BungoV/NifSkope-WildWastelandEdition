# WW_CHANGES.md entry — lane CENSUS1, 2026-09-11

(For the director to splice. `WW_CHANGES.md` is MIXED line endings and stays so —
splice in binary, match the neighbouring entries' endings, and measure with Python
byte counts, never grep.)

---

## 2026-09-11 — the Improved LOD census page (lane CENSUS1)

bungo's ruling on gap (4), 2026-09-11 10:0x, verbatim: **"We need them"** — every
FO4CS module ships self-diagnosing counters and Improved LOD had none specified.
`docs/LODGEN_CENSUS.md` is now the contract FO4CS implements against, so the
module is measurable on its first flight instead of argued about.

**NO BUILD, no `src/` file, no writer and no exe were touched.** Lane BAKEPERF1
owned `src/` concurrently; the brief's "header-only field … then do it" clause was
voided by the director at launch, so every missing generator census word is NAMED
as owed rather than added.

**New: `docs/LODGEN_CENSUS.md`.** **60 field names** in seven log rows
(`[ImprovedLOD]`, `.Ring`, `.Cluster`, `.Cards`, `.Residency`, `.Shadow`,
`.Fade`), each with seven columns filled — name, unit, what it counts, the
contract section it is read from, the scene change that must MOVE it, its complete
list of refusal words, and a default that accuses its own plumbing. 7 of the 60
are per-bin families (four numbers each), 4 are per-level families, 49 are
scalars. The five defaults are `uncounted` / `unread` / `unset` / `unchecked` /
`unwired`; `0` is a default only where zero is a measurement.

It covers everything the ruling named — per ring 0-3 the instances considered,
culled by frustum, culled by occluder, culled by screen size, drawn and triangles;
clusters selected per level and the pixel tolerance in force; cards per ring and
in aggregate; resident pyramid tiles and bytes per level; resident library bytes
for the finest levels; the shadow view's own clusters, triangles and draws; the
four fade-class thresholds as a fraction of screen height plus the hysteresis;
cross-fades in flight; corpus-hash staleness per file with the file and the hash
named; and the serving arm per module (native / stock-fallback / off) for objects,
terrain, cards and arrays, per CONSTITUTION 10.

Three decisions inside it: a **ring is a census BIN, never a selection rule**
(NATIVE 4.4 forbids selecting by ring), so `ringEdges` states the bin distances
and a per-ring number without it refuses; every per-bin row obeys
`considered == frustum + occluder + screen + drawn`; and a worldspace whose
`.lodi` writes zero occluder boxes forces `ringCulledOccluder` to refuse
`no_boxes` rather than print a 0 that cannot be told from a broken test.

**The page's second half** inventories the census the generator already prints
(the three `native*` lines, `vt:`, `cover`, `roads`, `arrays written:`, `merged:`,
`far rings:`, `stage times:`, the manifests, the card sidecars) from frozen logs,
and turns them into **14 cross-checks** a reader can run with the file open.

**New: `tests/spells/lodgen_census_check.py`** — read-only, no exe. It decodes a
bake's `.lodo`/`.lodi` with the independent decoder and checks every number the
census line printed against the bytes. Measured:

* 9-chunk Sanctuary pair (NATIVE1b 10:17:44): **59 checks, 0 failures, 31 census
  words the files cannot carry**, each named with its reason and counted
  separately, never as a pass.
* LODUI1's 13:25:22 region pair: 59 checks, 0 failures, the same 31.
* Floor: `--self-floor` doctors three claims one at a time and each was caught by
  name, on both pairs.
* Two refusals exercised: two logs disagreeing about a census line (refuse and
  name both, rather than pick the one that agrees) and a pair the decoder will not
  read (exit 2 with the decoder's words, so unreadable is never reported as
  wrong).

**Page gate** `scratchpad/census1_20260911/page_gate.py`: 60 field rows, 0 blank
cells over the six gate columns, 109 citations all resolving to sections that
exist, with both floors firing (a blank row raises the count to 5; a citation to
`NATIVE 99.99` raises it to 2).

**Also:** a pointer paragraph in `docs/LODGEN_NATIVE_LODO_LODI.md` §7 (the
reader's draw checklist) to the census page; the page's provenance footer
re-stamped after that edit, so the `NATIVE` stamp is `067eeaa70e2924fe`,
79,504 bytes, 1,253 lines.

**A defect found and NOT fixed** (not this lane's file):
`tests/spells/lodgen_native_decode.py` refuses the downtown-Boston pair — the only
one on disk that writes occluder boxes — at instance 3359, *"out of (cell,
drawKey, ref, part) order"*. Measured: the neighbouring instance sits at
**fy 2.999985** cells from its chunk origin after quantisation, one step below the
cell line at 3.0, so the decoder re-derives a different cell from the stored
position than the writer sorted on. It is the cell-level twin of the
chunk-boundary effect NATIVE 4.1c and NATIVE 6 already document, and the fix is a
decoder rule (take the cell from the cell RANGE, not from the stored position),
not a format change. **No gate had ever decoded that pair.**

**New skill** `.claude/skills/ww-census-contract/SKILL.md` (repo tree) — writing a
census contract for a consumer runtime that does not exist yet. Needs mirroring to
the live tree.
