# Lane INCR1 -- the incremental rebake works on the RULED pipeline, keyed on the bake record's per-chunk hashes

(The 2026-09-12 brief of the same name, executed by LAND1, is kept as `scratchpad/brief_incr1_20260912_landed_by_land1.md`.)

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch: `release/NifSkope.exe` 2026-09-17 03:56:42,
  22,477,824 B (ARCHLOCK1's; read it yourself and write it in the report's first line). Queue: ... -> BAKEREC1 -> ARCHLOCK1
  -> INCR1 (you) -> PERF1 -> AUDIT1, one lane at a time. Rung ONCE before your first build:
  `release/NifSkope.before_incr1.exe` (never delete any `release/NifSkope.before_*.exe`, nor
  `release/NifSkope.archlock1_rung.exe`). Markers `scratchpad/incr1_20260917/BUILDING` (touch FIRST) / `DONE` (first word
  `incr`). Report `scratchpad/incr1_20260917/lane_incr1_report.md`, INCREMENTAL (a section per finished step; the
  director reads it while you work); `PENDING.md` past half context. Never commit, never `git stash`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command, run and read before every build and every
  exe launch; Fallout4 up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with no `--port` is bungo's own window: rename the
  exe aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it; never rename or delete a `NifSkope_inuse_*.exe` you did
  not create. A GUI harness is `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time, second monitor only. Every path in
  argv and every WW_* path ABSOLUTE `E:/...`. Batch mode resolves a RELATIVE path against the exe's folder (nifcli.cpp:179).
- You are the ONLY lane in the tree. No mutex needed; leave none behind. ONE background waiter at a time.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (the LANDED 04:50 ARCHLOCK1 and 01:17 BAKEREC1 lines, and the
  BTOFREE1 line's "NOTE FOR INCR1's BRIEF"); `MISTAKES.md` (root, top ~15 entries); `docs/LODGEN_LEDGER_FORMAT.md` (whole);
  `docs/LODGEN_BAKE_RECORD.md` (whole, section 7 is addressed to you); `src/lodgen.h` from "THE INPUT LEDGER" to the end;
  `src/nifcli.cpp` ~2495-2640 (record path, switch digest, skip list) and ~3900-4120 (the `--incremental` driver block:
  refusals, the diff, the census line); `src/lodbfile.{h,cpp}`; `src/lodgenchunkpass.cpp` ~340-360 and
  `src/lodgenparallel.cpp` ~220-245 (the peak working set line); `docs/LODGEN_NATIVE_LODO_LODI.md` (append-only library,
  chunk table, loadOrderHash); `scratchpad/land1_20260912/b3_identity.sh` + `scratchpad/lane_land1_report.md` section B3
  (the only identity measurement ever made of the incremental path).
- Skills (repo `.claude/skills`): `nifskope-ww-lodgen`, `nifskope-ww-build-verify`, `ww-test-harness-add`,
  `ww-volatile-field-law`, `ww-one-reader-per-format`, `ww-census-contract`, `ww-spec-gate-audit`, `ww-panel-run-harness`
  (section 9: run a GUI harness from Git Bash). Write a skill for any repeatable procedure you invent, under
  `scratchpad/incr1_20260917/skills_proposed/<name>/SKILL.md`; the director places it.

## bungo's words, verbatim
- 2026-09-11 15:2x, the origin of the feature: "what if I load in some mod, and it only updates like one cell in the
  worldspace, can the lodgen regenerate only partially" and "so the new .esp gets read and diffed against the original bake
  from original plugin" -> the RESOLVED world per chunk is diffed against the record, never plugin files as files.
- 2026-09-16, "carry out your side", over the queue that reads: "INCR1 -- incremental rebake: skip chunks whose input hash
  in the bake record is unchanged."
- Standing rules that bind this lane: `--road-detail 1` always; the FO4CS target (`--native <dir>`, everything under
  `Data/FO4CSLOD/`, no legacy .BTO in the mod folder) is THE ruled default pipeline; masters ship off; no default moves; no
  format bump and no new file format without a stated divergence row for him.

## What already exists (do not build it twice)
- `--incremental <out-dir>` + the per-chunk input digest (`lodgenChunkInputDigest`, conservative by construction, one-cell
  ring), the switch digest with its skip list, five refusals, the `incremental:` census line, and the ledger rows
  (`chunk` lines: `inputs` sha1 + `out` files with digests) all landed 2026-09-12 (LAND1/INCR1) and were folded into the
  v2 bake record by BAKEREC1 (one file, `FO4CSLOD/<ws>/<ws>.lodb` on the FO4CS target). `--bake-record` is the read
  path; `tests/spells/lodb_read.py` the one Python reader. You EXTEND this. A second mechanism, a second ledger, a second
  reader, or a second digest is refused on sight.
- THE GAP, measured from the driver: `--native` is on the whole-region refusal list (nifcli.cpp ~4010, "refused: --native
  builds ONE .lodo/.lodi pair for the whole region"), and so is `--impostors`. Since BTOFREE1/LAYOUT1 the ruled pipeline IS
  `--native`. So today `--incremental` refuses every command bungo would type, and the only identity measurement (B3) was
  made on the stock target on 2026-09-12, before eight lanes changed the bake. The lane exists to close that gap.

## The work
1. **Measure first, on the exe at launch, before any source change.** (a) Run B3's arms as they stand on the STOCK target
   (small region, 9 chunks, dim 4; say which) and table them (arm, floor, verdict). If any is VACUOUS or FAIL, that is
   your first finding, not a thing to fix silently. (b) Show the refusal on the ruled pipeline: the same bake with `--native`
   + `--incremental` exits 1 with the "--native builds ONE pair" text; quote it. (c) Time the diff alone: seconds for
   `lodgenChunkInputDigest` over the 9 chunks against the whole bake's seconds; the diff must cost a small fraction of the
   bake or the feature is bookkeeping, and the number goes in the report either way.
2. **The FO4CS target becomes incremental-capable.** The chunk pass collects one `NativePlacement` per drawn reference and
   the lighting samples INSIDE the chunk pass (`lodgen.cpp` ~3784 and ~4069, per the refusal comment), and the
   region-wide `.lodo/.lodi` (and the `.lodm` card set when `--impostors`) are aggregated from them. For a skipped chunk the
   aggregate needs that chunk's contribution WITHOUT re-baking it. Two routes; choose by measurement and say why:
   (i) recover it from the PREVIOUS pair through the existing readers (the `.lodi` carries a chunk table: NATIVEVIEW1
   counted 676 placements in chunk (-20,24)'s table; the `.lodo` is an append-only library per base+material, shared by
   chunks). That is the clean route: no new file, one reader per format. (ii) If something the aggregate needs is provably
   NOT recoverable from the previous outputs (name the field), a per-chunk cache beside the record under `FO4CSLOD/<ws>/`
   is a NEW FILE = a divergence row for bungo, written up before you write it, and it must be listed in the record's `out`
   rows and the census floor like every other output. Whichever route: the aggregate built from (kept + rebaked) chunks
   must be BYTE-IDENTICAL to a full bake's: ordering of placements, mesh dedup order, identity indices, lighting samples,
   `.lodi` hashes, all of it. If the full bake's aggregate order depends on chunk iteration order, the incremental
   aggregate must reproduce that order, not sort. Take `--native` off the refusal list only when the gate in step 4
   proves it; the same for `--impostors`. `--atlas`/`--arrays` are stock-only sheets and may STAY refused (state it; not
   the ruled pipeline).
3. **Volatile fields, the open red (ARCHLOCK1's finding, BAKEREC1's file).** `Commonwealth.lodb` carries the peak working
   set (`lodgenPeakWorkingSetLine()`, printed into the chunk-pass census line, `lodgenchunkpass.cpp:352`) and that is a
   FIFTH volatile field the two normalisers (`lodbNormalise`, `lodb_read.normalise`) do not mask, so `lodgen_bakerec.sh`
   legs (e)(h), `lodgen_layout.sh` (5 of 22, all this file), `lodgen_native.sh` section 5 and `lodgen_btofree.sh` (1) are
   red with identical counts on the 01:17 and 03:56 exes. Apply `ww-volatile-field-law`: mask, never drop, the named
   field only, in BOTH readers, and add it to `docs/LODGEN_BAKE_RECORD.md` section 3's table (five, not four). Then the
   second face: `lodgen_layout.sh` leg (f) counts `census 79, on disk 80` because the record does not count itself in its
   own `end` line. Resolve it the way `LODGEN_BAKE_RECORD.md` section 2.8 states the `end` line's law (if the law says the
   record is not counted, the gate's count must exclude it; if the law is silent, the record counts itself and the doc
   says so). Any tree-compare gate that still byte-compares the `.lodb` compares it NORMALISED instead. All four gates
   green or the reason named per row. This step is small and comes FIRST after step 1, so the neighbours are green before
   your own change lands.
4. **The gate: `tests/spells/lodgen_incremental.sh`** (+ a Python helper only if the two existing readers cannot do it),
   promoted from B3, on BOTH targets, FO4CS first. Arms, each with a FLOOR (the edit moved the full bake's bytes; empty
   floor = VACUOUS, never PASS): (a) untouched: `--incremental` over an unchanged tree skips every chunk, `incremental: 0
   of N dirty` read back FROM THE RECORD (`--bake-record`), tree byte-identical to itself after normalising the record;
   (b) one edited plugin (`LODGEN_LEDGER_FORMAT.md` section 7's recipe: a genuinely edited REFR/LAND in a copy of the test
   plugin) dirties that chunk AND its one-cell-ring neighbours, nothing else, and `incr == full` byte for byte on every
   file incl. `.lodo/.lodi/.lodm/.lodl/.lodt` and the normalised record; (c) an output deleted under the record rebakes
   exactly that chunk, `incr == full`; (d) a switch change refuses (text quoted), a wrong-shape record refuses; (e) the
   FO4CS aggregate arm: a region whose dirty set is a strict subset, the `.lodi` placement count and every hash equal the
   full bake's, and `lodgen_native.sh`'s verify on the incremental tree passes; (f) refuter: a record with ONE
   `chunk.inputs` hex nibble flipped by hand makes the run rebake that chunk and only that chunk (proves the diff reads
   the record, not the outputs). Wall time incr vs full in the table (seconds), not as a pass criterion.
5. **The panel row.** PANEL1 (2026-09-12) left "--incremental row needs a driver change" as an open call. Read
   `src/lodgenmanager.cpp`'s driver: if the change is the panel passing the out-dir through (a row that spells the switch
   like the other 56), add it, palette + flat Name|Value tree as `nifskope-ww-panel-style` says, and prove it in the
   panel-vs-CLI leg of the panel gate; if it is more than that, a row for bungo with the reason, and no row.
6. **Docs.** `docs/LODGEN_LEDGER_FORMAT.md` sections 4 (the refusal table: what came off the list and why), 5, 6 (the gate
   is now `tests/spells/lodgen_incremental.sh`; B3 becomes history) and `docs/LODGEN_BAKE_RECORD.md` sections 3 and 7; a
   `docs/MISTAKES.md` lodgen entry for what let the refusal-on-the-default-pipeline gap ship on 2026-09-12 without a gate
   noticing; `MISTAKES.md` (root, top) for your own. `docs/LODGEN_CENSUS.md` if a census line's wording changes
   (`ww-census-contract`).
7. **Build and neighbours.** Rung, `BUILDING`, game check, build (`nifskope-ww-build-verify`: exe newer than every changed
   source, make -n clean). On the new exe: `lod_generation.sh`, `lodgen_defaults.sh`, `lodgen_native.sh`,
   `lodgen_bakerec.sh` (all legs), `lodgen_layout.sh`, `lodgen_btofree.sh`, your gate; table before/after (name, checks,
   failures, seconds) and say WHY each neighbour is reached (`ww-spec-gate-audit`). Byte-identical stock outputs where a
   rung exists (defaults gate).
8. **Report and pictures.** `lane_incr1_report.md`: exe line; step 1 table; the route chosen in step 2 and the measurement
   that chose it; the gate table with floors; the volatile-field fix rows; docs touched; MISTAKES; divergences stated
   (each a row for bungo); changelog text for the director to splice (WW_CHANGES entry written for a reader who never saw
   the code + HANDOFF LANDED block); you do not edit `WW_CHANGES.md` or `HANDOFF.md`. Pictures in
   `scratchpad/incr1_20260917/images/`: ONE native-view screenshot of the incremental tree beside the full tree's (same
   chunk, same camera, via the `lodgen_native.sh` harness route) and ONE terminal capture of the `incremental:` census
   line on a one-plugin edit. Then `DONE` with first word `incr` and a one-line verdict: "incremental on the FO4CS target,
   N of M chunks skipped, byte-identical" or what fell short and whose it is.
