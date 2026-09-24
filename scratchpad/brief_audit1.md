# Lane AUDIT1 -- the final-build audit of the lodgen pipeline (verify; fix only confirmed bugs)

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. You run AFTER the ruled queue (NATIVEVIEW2 -> VT1 -> GENSMALL1
  -> NATIVE1c -> BTOFREE1 -> LAYOUT1 -> BAKEREC1 -> INCR1 -> PERF1) has landed; the exe on disk `release/NifSkope.exe` is the FINAL BUILD of the campaign. Read its mtime
  and size yourself and write them in the report's first line. Rung ONCE before any build of yours:
  `release/NifSkope.before_audit1.exe` (never delete any `release/NifSkope.before_*.exe`).
  Markers `scratchpad/audit1_20260916/BUILDING` (touch FIRST) / `DONE` (first word `audit`). Report
  `scratchpad/audit1_20260916/lane_audit1_report.md`, INCREMENTAL (a section per step as it finishes, never one write at
  the end); `PENDING.md` past half context. Never commit, never `git stash`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command, run and read before every build and every
  exe launch; Fallout4 up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with no `--port` is bungo's own window: rename the
  exe aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it; never rename or delete a `NifSkope_inuse_*.exe` you did
  not create. `-no-gui` bakes need no GUI slot; a GUI harness is `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time.
  Every path in argv and every WW_* path ABSOLUTE `E:/...`. Never bungo's installed Data/Terrain. Never the whole Commonwealth
  in one bake (the measured whole-grid numbers already exist in WW_CHANGES.md; do not spend an hour re-proving them).
- You are the ONLY lane in the tree. No mutex needed; leave none behind. ONE background waiter at a time: stop a waiter
  before starting the next; never stack polling loops.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (every 2026-09-16 LANDED line: what the nine lanes claim);
  `MISTAKES.md` (root, top entries) and `docs/MISTAKES.md`'s lodgen section; `docs/LODGEN_NATIVE_LODO_LODI.md` IN FULL;
  `docs/LODGEN_BTD_FORMAT.md`; `docs/LODGEN_TERRAIN_VT.md`; `docs/LODGEN_CENSUS.md` s6; `docs/FO4CS_IMPROVED_LOD_PLAN.md` s5,
  s6 and s8-8.4 (what the generator owes the runtime, and his rulings); `docs/LODGEN_IMPOSTOR_SPEC.md` s"Cards";
  `docs/LODGEN_WEATHER_SHEETS.md` (parked: NOT a gap if absent).
- Skills (repo `.claude/skills`): `nifskope-ww-lodgen` (whole), `nifskope-ww-build-verify`, `ww-module-off-is-identical`,
  `ww-census-contract`, `ww-contract-provenance` (for every doc line you change), `ww-spec-gate-audit` (before reproducing any
  number a plan row states). Skill `core-vanilla-property-census` for any "how many in the corpus" question.

## bungo's words, verbatim (2026-09-16 19:0x)
- "This will be the final build, all the stuff for lod baking will be done by then? and I'll be able to do a test bake"
- "You can run one more agent to verify it's all good, working, there's no bugs or issues, no design gaps, etc."

## The work (in this order; each step lands in the report before the next starts)
1. **Every lodgen gate on the final exe.** Run every `tests/spells/lodgen_*.sh` (list them with `ls tests/spells`) once, in
   the order the `nifskope-ww-lodgen` skill gives, and table them: name, checks, failures, skipped, seconds, and for every
   failure WHOSE it is (a known red the HANDOFF already names, or new). Known reds you may find and must not re-report as
   news: the four grass-feature ground-cover failures; the stock `.BTO` ~6 percent silent drop (his call, unfixed by order);
   anything a LANDED line names as RED. A gate that cannot run (missing fixture, missing input) is a row too, with the reason.
2. **A fresh end-to-end bake** through the FO4CS target with the defaults (no switches beyond the target, the plugin list and
   the output folder), on THREE fixture regions you choose and name with their cell ranges: (a) Sanctuary / chunk (-20,24) as
   every native lane used it; (b) a region with WATER bodies (the v3 `.lodl` water table must be non-trivial: coast or a
   lake); (c) an URBAN region (dense `.lodi`, SCOL parts, aggregates). Output to a scratch folder under
   `scratchpad/audit1_20260916/bake_<region>/`. For each: wall time, peak memory (a PowerShell `Get-Process` sample loop),
   every output file with size, the census line verbatim, the four `stage times:` (landscape / meshes / textures /
   impostors) and which stage is the long pole (bungo 19:2x asked about performance; the whole-Commonwealth bake has never
   been timed by any lane, WW_CHANGES 4770; you do NOT time it either, you report the split so the director can extrapolate). Then the same three with `--keep-bto` and with the STOCK target:
   the stock output must be what `lodgen_defaults.sh` expects (byte-identical where a rung exists).
3. **Independent decode of every output file.** With the Python readers in `tests/spells/` (`lodgen_native_decode.py`,
   `lodgen_native_fields.py`, `lodgen_vt_check.py`, `lodgen_lodi_wayback.py`, `lodgen_census_check.py`; write a new one only
   where none exists and say so) parse EVERY file of the three bakes and check the invariants the docs state, at minimum:
   `.lodl` every coarser level equals the exact subsample of level 0 (drop, never average); water table per cell within the
   cell's height range, shore distance monotone away from the body; `.lodt` tile coverage complete, no tile outside the grid,
   dim-2 texel size 32 u; `.lodo` every mesh's cluster ladder has non-decreasing geometric error, every bounding sphere
   contains its vertices, every normal cone contains its face normals, refusal on v3; `.lodi` every baseId resolves, scale
   never 0 unless the flags say refusal, identity index unique, no reserved flag bit set, aggregates' top bit consistent with
   the cold record; `.lodm` every card references an existing mesh; every FO4CS-target file sits under `FO4CSLOD/` (LAYOUT1; nothing of ours outside it except the stock target and the
   Textures/Terrain HeightMap); `.lodb` (BAKEREC1) lists every plugin and resource the bake was given, in
   order, and its per-chunk hash table covers every chunk written; INCR1's skip-and-keep leaves a byte-identical set. Every invariant is a row: name, files checked, violations,
   and a REFUTER (a mutation via `lodgen_native_mutate.py` or a hand-flipped byte that the check must catch, proven caught).
4. **Diff review of the campaign.** `git diff 720762a -- src/ tests/ tools/ docs/` is everything the nine lanes plus DEFAULTS1
   changed (about 500 paths uncommitted; `git status --porcelain` first). Read every changed `src/*.cpp|h` hunk for: writes
   past a buffer, uninitialised fields in a written struct, endianness or alignment assumptions in a file writer, off-by-one
   at grid/tile/cell edges, a refusal path that does not refuse, a `WW_*` env or panel switch with no way back, a census
   word that reports intent rather than what was written (telemetry echoes truth). Every finding: file:line, what breaks, a
   two-line reproduction, and CONFIRMED (reproduced) or SUSPECT (not). Do not review style.
5. **Design gaps against the plan.** For every row of `docs/FO4CS_IMPROVED_LOD_PLAN.md` s5 ("what the generator still owes")
   and s6, state DONE (which file/field/gate proves it), PARKED-BY-RULING (quote the ruling), or GAP (nothing in the files
   carries it and no ruling parks it). Then the same for the runtime needs listed in `docs/LODGEN_CENSUS.md` s6 and for the
   rulings in plan s8.2-8.4 (stitching, geomorph, per-asset LOD precedence: the bake side of each; e.g. does the `.lodl`
   pyramid really keep samples so the geomorph target is exact? prove it from step 3). A gap is a row, not a fix.
6. **Fix only CONFIRMED bugs** from steps 1, 3 and 4 that have a one-to-few-line fix and a gate that fails before and passes
   after (add the check to the existing gate that owns the area; new gate only if none does). Rung first, `BUILDING` first,
   game check first. After any build: rerun step 1's table on the new exe and the three bakes' invariants of step 3. Never a
   design change, never a default change, never a format bump: those are rows for bungo.
7. **Report.** `lane_audit1_report.md` sections 1-6 as above plus a top summary of at most twenty lines: exe (mtime, size,
   rebuilt or not), gates run/failed (and whose), files decoded/violations, bugs fixed (file:line each), bugs left (why),
   gaps for bungo. Pictures: for each of the three bakes ONE native-view screenshot via the `lodgen_native.sh` harness route
   (`scratchpad/audit1_20260916/images/`), plus one `.lodt` sheet render. Deliver changelog text for the director to splice
   (WW_CHANGES entry + HANDOFF LANDED block); you do not edit WW_CHANGES.md or HANDOFF.md. MISTAKES entries for your own
   mistakes at the top of `MISTAKES.md`, and a `docs/MISTAKES.md` lodgen entry for every CONFIRMED bug you fixed (what let it
   through). Then `DONE` with first word `audit` and a one-line verdict: "clean", or "N bugs fixed, M red, K gaps".

## Addendum 2026-09-17 15:4x (director, at launch): what landed after this brief was written
- The final build: `release/NifSkope.exe` 2026-09-17 12:45:27, 22,567,424 B, sha1
  a843fca68c18c2740efddcb20e9fe715b7732a22 (PERF1's). Lanes since the brief: ARCHLOCK1 (Files-tab freeze, lock fix),
  OPENHERE1 (director hotfix: "Open Here" first in the OS drop menu), INCR1 (`--incremental` on the FO4CS target, per-chunk
  `.lodj` cache, `--no-native-cache`), PERF1 (parallel library build, library reuse under `--incremental`, census keyword
  `native-library-build:`, seven-stage split on `stage times:`). Read their LANDED blocks in HANDOFF and
  `scratchpad/incr1_20260917/lane_incr1_report.md` s12, `scratchpad/perf1_20260917/lane_perf1_report.md` s8 and s12.
- Build note: MSYS2 shell needs `export PATH=/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH` before
  `mingw32-make -f Makefile.Release -j8`.
- THREE STANDING REDS on the final exe, red on PERF1's rung too. They are YOUR step-1 rows and step-6 candidates: for each,
  decide with evidence whether the GATE is stale (the product is right and the gate still expects the old world) or the
  PRODUCT is wrong, then fix the side that is wrong, with the refuter: (1) `lodgen_native.sh` section 5, 2 FAIL (claimed:
  BAKEREC1's v2 record vs BTOFREE1's expectation of the stock .BTO); (2) `lodgen_btofree.sh` 3 FAIL (legs a,b: `.lodj`
  exists on the new side only because the gate's comparison rung predates it; leg c: BAKEREC1's v1-vs-v2 record);
  (3) `lodgen_stage_times.sh` 1 FAIL (claimed: LAYOUT1 moved the pair under one root and the gate still looks for it
  flat). A stale gate is a confirmed bug of the test suite: fixing it so it tests the ruled layout, and proving it still
  goes red on a broken build, is in scope. bungo's test bake should start from a green board or a board whose every red is
  a row he has been asked about.
- Rows already owed to bungo, NOT yours to decide, do not re-report as news: `.lodj` in the default output; `--native` off
  the incremental refusal list; `--chunk-threads` default; the model fan-out cap of 4 as a switch; `modelCorpusStamp`;
  the `native-library-build:` keyword. DO audit the fence PERF1 row C describes (a `.nif` edited on disk under
  `--incremental` reuse): confirm by test that a DEFAULT full bake can never reuse a library, and say so in one line.
- Step 2 additions: on region (a) also run a null `--incremental` over your own fresh bake and confirm the pair is
  byte-identical and the census says `native-library-build:` in words; record wall clock for full vs null.
- Game: bungo plays during the day. Check before EVERY gate and every bake, not once per batch; if the game is up, stop,
  write PENDING.md with what is done and what remains cheapest-first, and end your turn saying so. The director resumes you.
- Markers and folder stay as written above (`scratchpad/audit1_20260916/`). Final message under 300 words: exe (unchanged
  or rebuilt, mtime/size/sha1, rung), the gate board in one line per gate, every red and whose, confirmed bugs fixed,
  design gaps as rows, pictures, proposed skills.
