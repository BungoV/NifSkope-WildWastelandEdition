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
