# Lane PERF1 -- the FO4CS bake uses the machine: parallel object and texture passes, and the library is not rebuilt for nothing

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch: `release/NifSkope.exe` 2026-09-17 07:33:21,
  22,534,144 B (INCR1's; read it yourself and write it in the report's first line). Queue: ... -> BAKEREC1 -> ARCHLOCK1
  -> INCR1 -> PERF1 (you) -> AUDIT1, one lane at a time; AUDIT1 audits the exe you leave, so what you leave is the final
  build of the campaign. Rung ONCE before your first build: `release/NifSkope.before_perf1.exe` (never delete any
  `release/NifSkope.before_*.exe`, nor `release/NifSkope.archlock1_rung.exe`, nor a `NifSkope_inuse_*.exe`). Markers
  `scratchpad/perf1_20260917/BUILDING` (touch FIRST) / `DONE` (first word `perf`). Report
  `scratchpad/perf1_20260917/lane_perf1_report.md`, INCREMENTAL (a section per finished step; the director reads it
  while you work); `PENDING.md` past half context. Never commit, never `git stash`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command, run and read before every build and every
  exe launch; Fallout4 up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with no `--port` is bungo's own window: rename the
  exe aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it; never rename or delete a `NifSkope_inuse_*.exe` you did
  not create. A GUI harness is `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time, second monitor only. Every path in
  argv and every WW_* path ABSOLUTE `E:/...`. Batch mode resolves a RELATIVE path against the exe's folder (nifcli.cpp:179).
- Build: MSYS2 UCRT64 shell needs `export PATH=/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH` before
  `mingw32-make -f Makefile.Release -j8` (the link runs `git` for the version string; INCR1 report section 9).
- You are the ONLY lane in the tree. No mutex needed; leave none behind. ONE background waiter at a time.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (the LANDED 08:30 INCR1 block and its two rulings owed; the
  BAKEPERF1 and NIFPARSE1 LANDED blocks further down: what the pool already proved and what faulted); `MISTAKES.md`
  (root, top ~20 entries); `src/lodgenparallel.{h,cpp}` (whole: `--threads`, `--chunk-threads` default 1, the memory cap,
  `lodgenParallelFor`, results retired in job order); `src/lodgenchunkpass.{h,cpp}` (the chunk queue, the writer thread,
  the `stage times:` and `peak working set:` lines); `src/lodgen.cpp` ~2029 (`stage times:` text) and the definition of
  `lodgenNativeWrite` (grep it; the library build: ladder, mesh dedup, texture pass, the `.lodo`/`.lodi` write);
  `docs/LODGEN_NATIVE_LODO_LODI.md` (append-only library, chunk table, loadOrderHash); `docs/LODGEN_BAKE_RECORD.md`
  section 7 ("For the lane after INCR1": the record already stores the three corpus hashes the library is a function of);
  `docs/LODGEN_LEDGER_FORMAT.md` 4.1; `scratchpad/incr1_20260917/lane_incr1_report.md` sections 1, 2 and 12 (the
  fixed-cost measurement and the divergence rows); `tests/spells/lodgen_stage_times.sh`, `lodgen_identity.sh`,
  `lodgen_byte_gate.sh`, `lodgen_incremental.sh` (what the byte gates already do; you extend, never duplicate).
- Skills (repo `.claude/skills`): `nifskope-ww-lodgen`, `nifskope-ww-build-verify`, `ww-test-harness-add`,
  `ww-volatile-field-law`, `ww-census-contract` (any new census clause), `ww-module-off-is-identical`,
  `ww-aggregate-incremental-cache` (INCR1's, the replay contract you must not break), `ww-bash-to-windows-python`,
  `ww-spec-gate-audit`, `ww-panel-run-harness` section 9. Write a skill for any repeatable procedure you invent, under
  `scratchpad/perf1_20260917/skills_proposed/<name>/SKILL.md`; the director places it.

## bungo's words, verbatim
- 2026-09-11 10:1x, the origin: "bake time, anything we can do to speed it up? use my system to its fullest here?"
  (machine: 16 logical cores, 31 GB, RTX 5070 Ti).
- 2026-09-16, "carry out your side", over the queue that reads: "PERF1, chunk-parallel object and texture passes."
- Standing rules that bind this lane: `--road-detail 1` always; the FO4CS target (`--native <dir>`, everything under
  `Data/FO4CSLOD/`) is THE ruled default pipeline; masters ship off; NO DEFAULT MOVES (`--chunk-threads` stays 1 unless
  he rules otherwise; a row for him is how you ask); no format bump and no new file without a stated divergence row;
  every output of an N-thread bake byte-identical to the 1-thread bake, whole output tree, every file.

## What already exists (do not build it twice)
- BAKEPERF1 (2026-09-11): the chunk queue is one shared function, worker pool + writer thread in `lodgenparallel`, BC
  encoders parallel per block, results retired in job order, byte identity green. NIFPARSE1 (2026-09-11): the NIF loader
  made thread-safe for the chunk fan-out, the memory cap, the census says which bound (cores/memory) held. The chunk
  fan-out still SHIPS OFF (`g_chunkThreads = 1`, "safe at 16, but slower" -- read that comment and the NIFPARSE1 block
  before you believe either half of it).
- INCR1 (2026-09-17): `--incremental --native` replays clean chunks from `.lodj`. Its measurement is your starting line:
  on a 4-chunk FO4CS region a null incremental saved 2 s of 72, a chunk costs about 1.6 s, the FIXED cost is about 58 s
  and it is `lodgenNativeWrite()` building the `.lodo` library from the worldspace's FULL base census (3,666 meshes
  laddered over 4,400,225 triangles), chunk-independent by design. So roughly 80 % of an FO4CS bake is one serial function
  that does not care about chunks. THAT is the wall clock. Chunk parallelism alone cannot touch it.

## The work (in this order; each step lands in the report before the next starts)
1. **Measure first, on the exe at launch, before any source change.** Two regions, name them by cell range: (a) the 9-chunk
   Sanctuary region `--terrain-region -24 16 -13 27` dim 4 as INCR1 used it; (b) a bigger one, 16 to 25 chunks, that
   INCR1's census picture used or one you choose. For each, one full FO4CS bake at `--chunk-threads 1` and one at
   `--chunk-threads 8`, each ONCE more after a warm cache, and table: wall clock, the four `stage times:` values, the
   `peak working set:` line, the census `bound by` word. Then split `lodgenNativeWrite` itself with wall-clock reads around
   its internal stages (ladder, dedup/welding, texture pass, atlas/array build, the `.lodo` write, the `.lodi` write) in a
   SCRATCH instrumented build (rung first) and table those seconds. That table decides steps 2 to 4; write it before them.
   Byte identity of every output file 1 vs 8 threads on both regions on THIS exe is row zero: if it is already red, that
   is your first finding and nothing else starts until it is understood.
2. **The object pass in parallel.** Inside the library build, per-mesh work (laddering, welding, whatever step 1 showed as
   the seconds) goes through `lodgenParallelFor` with results retired in job order, so the mesh ids, the append order and
   every byte of the `.lodo` are what the serial pass produced. Shared caches read-only before the fan-out (the pattern
   BAKEPERF1 set). The `--threads 1` way back must degenerate into the loop that exists today.
3. **The texture pass in parallel.** Per-texture conversion/atlas work the same way; the BC block encoders are already
   parallel, so the win is the per-file fan-out above them and the writes off the critical path. Same identity rule.
4. **The chunk fan-out, measured honestly.** With `--chunk-threads 8` on both regions: 20 consecutive runs without a fault
   and byte identity against `--chunk-threads 1`. Report seconds and the memory bound. Do NOT move the default; give bungo
   a divergence row ("chunk-threads default 1 -> N: what it costs in RAM, what it saves in seconds, the 20-run evidence")
   in section 12 of the report. If a fault appears, the row says so and the default stays 1.
5. **The library is not rebuilt for nothing (under `--incremental` only).** `docs/LODGEN_BAKE_RECORD.md` section 7: the
   `.lodo` is a pure function of the base census, the three corpus hashes the record already stores and the switch
   digest. When an incremental run finds all of those unchanged and the previous `.lodo` present with its recorded digest,
   keep it and skip the library build; the `.lodi` is still assembled from replay + dirty chunks as INCR1 left it. Proof:
   the same two arms INCR1 used (null run; 1 rebaked + 3 replayed) produce a `.lodo`/`.lodi` byte-identical to a full
   bake. A moved plugin, a moved corpus hash or a moved switch rebuilds the library. No new file, no format change, no
   default moved (a full bake never reuses anything). The census says it: one line, `native library: reused` or `rebuilt
   (why)`, through `ww-census-contract`.
6. **The stage-time split ships.** The `stage times:` census line gains the library's split (or a second line) so a user
   reading his own bake sees where the seconds went; masked as volatile like the four that exist (`ww-volatile-field-law`,
   both normalisers, `lodb_read.py` too). No panel row unless a row already exists for stage times; if it does, the split
   goes there in the panel's own words.
7. **Gates.** New `tests/spells/lodgen_perf.sh` with floors: (a) byte identity of the whole output tree, every file,
   `--threads 1 --chunk-threads 1` vs `--threads 0 --chunk-threads 8`, both regions; (b) the census proves the parallel
   path was TAKEN (worker count printed >= 2) or the arm is VACUOUS and fails; (c) `--threads 1` output identical to the
   rung exe's output on the same region under the five-volatile mask (the way back is the bake that always ran); (d) the
   library-reuse arms of step 5; (e) the stage-time split present and masked. No wall-clock floor (it flakes); the seconds
   go in the report table instead. Then the neighbours, one at a time, on your final exe, before/after in a table with
   WHOSE every red is: `lod_generation.sh`, `lodgen_defaults.sh`, `lodgen_native.sh`, `lodgen_bakerec.sh`,
   `lodgen_layout.sh`, `lodgen_btofree.sh`, `lodgen_incremental.sh`, `lodgen_stage_times.sh`, `lodgen_identity.sh`.
   Known reds you carry, not news: `lodgen_native.sh` section 5, 2 FAIL (BTOFREE1's stock `.BTO`, widened by `.lodj`);
   `lodgen_btofree.sh` 21/3: legs (a)(b) = `.lodj` present on the new side only (rung predates it; say whether YOUR rung
   `before_perf1` = INCR1's exe clears them), leg (c) = BAKEREC1's v1-vs-v2 record. Any red you make is yours to fix.
8. **Report and hand-off.** Sections in order: 1 measured first (the two tables); 2-6 each step; 7 gate; 8 neighbour table
   before/after; 9 build (mtime, size, rung, sha1); 10 docs touched (`docs/LODGEN_NATIVE_LODO_LODI.md`,
   `docs/LODGEN_BAKE_RECORD.md` section 7 continued, `docs/LODGEN_LEDGER_FORMAT.md`, `lodgen --help`, one line each
   with provenance); 11 changelog text for the director to splice (WW_CHANGES paragraph for a reader who never saw the
   code + the HANDOFF LANDED block), you never edit WW_CHANGES.md or HANDOFF.md yourself; 12 divergence rows for bungo
   (written BEFORE the thing stands: chunk-threads default, any new census clause, anything else); 13 pictures:
   `scratchpad/perf1_20260917/images/`: one stage-time bar chart before/after per region (numbers on the bars), and one
   picture of the same library cells from the 1-thread and the 8-thread bake with the measured pixel difference (0.000 %
   or the lane is not done). MISTAKES.md (root): your own entries the moment you recognise one.

## Refuters (each must FAIL on the exe at launch or on a deliberately broken build, and say so in the report)
- Identity: flip one worker's retire order in a scratch build; gate (a) must go red.
- Vacuity: run with `--threads 1`; gate (b) must report VACUOUS, not PASS.
- Reuse: touch one byte of a plugin between two incremental runs; the library must be REBUILT and the census must say why.
