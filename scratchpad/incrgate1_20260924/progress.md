# INCRGATE1 (LOD-E) progress -- worktree E:\Projects\NifskopeWWE-incrgate1, branch incrgate1-20260924 from 47b2cad

## Step 0 -- setup (2026-09-24 21:2x)
- Skills loaded: nifskope-ww-lodgen, nifskope-ww-build-verify, search-lean, nifskope-ww-panel-style,
  ww-test-harness-add, nifskope-ww-worktree-build (written by VTFIX1 minutes before; my path matched it).
- Read: common brief, brief_incrgate1.md, CONSTITUTION 1 + 1a, lodaudit1 PLAN s2-3, helper_reports A + C,
  incr1_20260917 DONE + report s2/s6/s8/s12.
- First build: main tree clean at 47b2cad sources (only tests/spells/pbr_csm1_gates.sh differs; `make -q` rc 0),
  so its GeneratedFiles/ + release runtime were COPIED (read-only from main): .qmake.stash, GeneratedFiles/,
  release/{NifSkope.exe, *.dll, qt.conf, nif.xml, kfm.xml, style.qss, hkclasses_fo4.json, imageformats,
  platforms, styles, shaders, nifskope-cli.cmd, build_rev.txt}. qmake rc 0. make -n: 35 moc objects + link.
- Finding: the worktree qmake run carries no -DNIFSKOPE_REVISION (main's does); harmless (build_rev.txt wins).
- Where the digest lives: the SWITCH digest (`lodgenSwitchDigestOf`) is in src/nifcli.cpp, NOT lodgen.cpp; the
  per-chunk INPUT digest (`lodgenChunkInputDigest`) is in lodgen.cpp. The identity word belongs to the switch
  side, so this lane owns it (it moves into lodgenchunkpass.cpp with the rest of the ledger code).

## Step 1 -- rung + INCR1 re-run (2026-09-24 21:1x-21:3x)
- Rung: first worktree build, BUILD-RC=0, 35 moc objects + link; saved as release/NifSkope.before_incrgate1.exe,
  sha1 b2a2b8b4f9ec913aa6b56814a15e5df477fc766d (24,716,800 bytes).
- tests/spells/lodgen_incremental.sh on the rung (WW_INCR_WORK under this lane folder): 11 ok, FAILURES: 0, RC=0
  (09-17 recorded 10/0; leg (g) "--incremental with no value refuses" is the eleventh). Log work/incr_rung.log.

## Step 2 -- ledger moved + identity word (build b1, commit 6a8bc80)
- src/lodgenchunkpass.{h,cpp} now own the ledger (diff, refusals, .lodj cache hooks, record, lodbRecordPath,
  lodbFindRecord, lodgenSwitchDigestOf moved verbatim); nifcli.cpp keeps only calls. Refusal texts unchanged
  except the Switches refusal gained one sentence about the identity word.
- Identity word gen1:<sha1> over key=value lines of every EFFECTIVE setting (terrain/object/cover options, the
  land/blend getters, the extras); record switches = sha1(argv digest 0x1f word). CLI prints
  `identity: <word>, N setting(s)`; WW_LODGEN_IDENTITY_DUMP=<file> writes the lines.
- Build b1: RC 0, 3 objects (lodgenchunkpass, nifcli, + moc none), exe sha1 b2d7622e11d920c57439697966d4d026d386b8f5.
- lodgen_incremental.sh on b1: 11 ok, FAILURES 0 (work/incr_b1.log).
- Consequence (stated, not hidden): every pre-lane record refuses ONCE (a one-time full rebake).

## Step 3 -- G1 identity spell (tests/spells/lodgen_incr_identity.py)
- Legs (a) identity words/dump diff, (b) real flip via release/NifSkope.before_defaults2.exe (gitignored copy,
  sha1 6b8ed793...), (c) forged record with null control + digest self-check. Running on b1 exe.
- G1 on b1 exe: 11 checks, 0 failures, PASS (work/g1_exe.log). bare word gen1:e9ad123d... (112 settings) ==
  `--blend-edges quadrant`; `off` gen1:3cba401a...; dumps differ in blend.edges only; before_defaults2 record
  refused (switches); forged record refused after a 0-dirty null run; digest replica reproduces the record.
- G1 RED on the rung: 11 checks, 6 failures (work/g1_rung.log) -- no identity line; the before_defaults2 record
  ACCEPTED with "0 of 1 chunks dirty" (the defect: a moved default kept yesterday's chunks); forged == real.

## Step 4 -- the panel row (b2, 2026-09-24 21:45, exe sha1 3b3808fd60c8e961b6a34e2aa54b702335e3faa4)
- src/lodgenmanager.cpp: Run-section row "Rebake only what changed" (LodgenIncrementalCheck, key
  `incremental`, default OFF, env hook WW_LODGEN_INCREMENTAL=0|1). Row ON = INCR1's ledger through
  lodgenIncrementalBegin/ArmCache/NoteRetired/CacheRefusal/OfferReuse/WriteRecord, switches `--panel`
  + the identity word. No record = full bake + record; any refusal verdict = full bake + record
  rewritten, census says why (a refusing standing row would have no way back). Mixed chunk sizes =
  row off for that run, said in the census. Cancel or a refused native pair = no record.
- Default consequence: panel arrays are ON by default, so regionProducts holds and the row full-bakes
  every run until arrays/atlas/cards are unticked. Stated in the census.
- G2 tests/spells/lodgen_panel_incremental.sh: rung 137/0, OFF 137/0, ON 137/0; (a) rung == OFF,
  10 files 45,582,390 bytes sha1 1b16fc05; (b) ON = OFF + 1 record + 1 .lodj, record 1 chunk,
  `--panel`, first-run census; RED: the rung tree as ON fails b2 and b3. RESULT PASS.
- lod_generation.sh b2 128/0 (rung 128/0; check boxes 39 -> 40, dash 0, untipped 0).
- lodgen_panel_run.sh b2 137/0 (floor 130).

## Step 5 -- plan 5 rows 13 / 25 / 26 (commit 64e0d69)
- Row 13 `lodgen_native_baseline.sh --drop-proof`: one bake of (-32,0) dim 32 (84 s). Stock drops
  2,628 of 42,560 (6.17 %); native instanceCount 42,560, decoder 12/0, verify neither=0. Both reds ok.
- Rows 25/26 `lodgen_sanctuary_pair.sh` 7/0 (20 s): lodo v4 6,204,388 B, lodi v7 527,989 B, 3,526
  placements / 10 chunks, decoder 54/0, truncated lodi refused. Census checker 38/0/32 after the
  ladder-OFF fix (was 37/1/31); red leg: doctored ladderGroup 4 caught.
- Finding: the checked-in tests/baselines/stock_baseline.sha256 is STALE. --check is red with the same
  6 files on the RUNG and on b2. Against a baseline written from the rung (work/rung_baseline.sha256),
  b2 is 25/25 byte-identical, PASS. Re-pinning the checked-in file is the director's call; it was not
  touched here.

## Step 6 -- plan 5 row 6 + docs (22:05)
- lodgen_native.sh leg 13b: the decoder reads the downtown pair (33,123 inst, 280 occ),
  cellQuantAmbiguous 14, worst 0.062501 u (band 0.126955). Red: band 0 refuses at instance 3358.
  Whole spell b2: 32 checks, 0 failures, RESULT PASS (was 29 before the leg).
- Docs: LEDGER_FORMAT (identity word, panel row), BAKE_RECORD 5, PLAN rows 6/13/25/26 + lodi size,
  CENSUS re-run, NATIVE 9 drop proof + 4.1 measurement.
