DONE -- GATEFIX1, 2026-09-25 01:08. Branch gatefix1-20260924, worktree E:\Projects\NifskopeWWE-gatefix1.
Exe release/NifSkope.exe sha1 dca43d83 (b2f3073, unmodified; no source code changed). Commits 3e343a8, d0a8e55, 1d2c769.

## 1. Skills loaded
nifskope-ww-worktree-build (brought the tree up: stash, objects from main at b2f3073, REVISION objects
deleted), nifskope-ww-lodgen (bake flags, the two targets, --keep-bto, gate list). New skill written:
ww-stale-gate-attribution.

## 2. What was built
No generator code changed. All three reds were stale expectations or harness defects, not generator
defects. One build (b2f3073 as is), kept as release/NifSkope.before_gatefix1.exe = release/NifSkope.exe.
- tests/baselines/stock_baseline.sha256: regenerated after both moves were named (3e343a8).
- tests/spells/native_open.sh: every window in its own wiped settings scope (d0a8e55).
- tests/spells/lodgen_btofree.sh + lodgen_btofree_ledger.py: re-pinned, three harness defects fixed (1d2c769).

## 3. Gates, with the red runs
### stock_baseline.sha256: RED -> GREEN (stale expectation)
Red: `--check` on dca43d83 = 6 CHANGED (baseline_check_rung.log). A sweep of all 47 rungs shows the list moved twice:
- before_msncache1 -> before_slab1 (09-18): 4.-20.24, 8.-24.24 and 16.-32.16 .bto, plus the region BTO
  and its manifest. Only NiAlphaProperty Threshold moved, from 128 to the BGSM's alpha ref. Cause: bungo
  2026-09-18 05:2x, "Alpha test 80 looks better" (src/lodgen.cpp ~2314).
- before_cellview2 -> before_cellview3 (09-19): 16.-32.16 .bto and its manifest. Two absolute build-path
  BGSMs now resolve their textures. Cause: lane CELLVIEW2b's material-path fix.
Both moves are in squashed commit 85c0b14. The new list equals the before_cellview3 sweep list exactly.
Green: `--check` 0 failures, `--selftest` PASS.

### native_open.sh: RED -> GREEN (harness defect)
Red: 16 checks, 2 failures. (d) NCC was -0.2094 < 0.45, and its refuter also failed (native_open_rung.log).
The same -0.2094 appears on every rung back to before_btofree1, which passed 17/0 on 09-16, so the exe is
not the cause. The harness inherited bungo's saved settings, and the .BTR water drew white.
Fix: every window runs in its own WW_SETTINGS_SCOPE, wiped before each window and at exit. The lodl_open
sub-gate is called with `env -u LODL -u PORT`.
Green: 17 checks, 0 failures, 2 skipped. COVER 0.9927, NCC 0.8411 (the other chunk 0.2284, flip -0.047),
lodl_open 23/0.

### lodgen_btofree.sh: RED -> GREEN (stale pin, plus three harness defects the new pin exposed)
Red: b2f3073 script and helper, pin before_btofree1: 27 checks, 5 failures (btofree_red_b2f3073.log).
- (a) 3 files differ: .lodo, .lodi and the colour DDS.
- (a) .lodo and (a) .lodi.
- (b) 4 files differ: the same three plus .BTO.
- (c) 2 files differ: .BTO and the colour DDS.

A recursive bisection over the 47 rungs (btofree_bisect.sh, sw/*.sig) found nine moves. Each is named by a measurement:
| rung window | moved | cause |
|---|---|---|
| viewfix -> authored | .lodo .lodi | EditorMarker exclusion, bungo 2026-09-17 ("magenta squares on the roofs"). Meshes 5567 -> 5562; the 5 new "failed" models are exactly the HitExt*DummyLOD.nif |
| msncache1 -> slab1 | .BTO .lodo .lodi | alpha-ref ruling (above), plus .lodi v5 -> v6 vertex-AO stream, bungo 2026-09-18 04:0x (41638 -> 526571 B) |
| lodiv7 -> btdterrain_rebuild | .lodi | v7 group table + sky stream (LODIV7, bungo 09-18) |
| horizon1 -> horizon2, horizon2 -> horizonout | .lodi | v8 horizon stream (HORIZON1/2) |
| horizon3 -> gltfexport1 | .lodi | horizon retired, v8 -> v7 (8679308 -> 1013995 B), bungo 2026-09-19 "horizon goes bye bye" |
| cellview2 -> cellview3 | .lodo .lodi | CELLVIEW2b material paths |
| defaults2 -> blendseam1 | colour DDS | DEFAULTS2: --blend-edges quadrant is the default (bungo 2026-09-23) |
| blendseam1 -> vtnormal1 | colour DDS | BLENDSEAM1: the stock chunk colour sheet cross-fades with the blend on |

Re-pin: RUNG defaults to release/NifSkope.before_gatefix1.exe. bake() tries the rung's flags, newest first.
The first run on the pin gave 30 checks, 4 failures, all of them harness defects (btofree_run1.log):
1. (a) "the rung wrote none .lodj". The pin writes the native cache too. The exclusion now applies only to
   a rung with no .lodj; otherwise the cache is compared byte for byte, which is tighter.
2. (a) Ledger drop mode crashed with an AttributeError (strip_digests() was given shape()'s text). It has
   been broken since AUDIT1 (09-17) but was never reached: the old rung wrote a v1 record and the leg
   skipped. It now checks the bto scratch row, the bto clause, the layout count and the end line against
   the out rows that entered the record folder. Everything else must be equal.
3. (b)(c) Ledger same mode compared census rows raw. Stage times, the peak working set and the out-dir
   path failed it. Those are masked through lodb_read.normalise(), the record's own documented rule, and
   each record's own --out-dir becomes <out-dir>. A change anywhere else in the census still fails.
Plus --generators-differ for a rung with different bytes (the VTFIX1 generator word). It checks that every
chunk's inputs moved, then leaves them out.

Refuter: ledger_refute.py runs 8 mutated-record cases, 8/0. Changed content, digest, end count, layout
count or a dropped row fails; a volatile value passes.
Green on the pin (same exe): 30 checks, 0 failures (btofree_green.log). Green against a byte-padded copy
(sha1 72459c94, the generators-differ path): 30 checks, 0 failures, inputs moved 1 of 1 in every leg
(btofree_genword.log).

### Kept green (exe dca43d83)
- lodgen_native: 32/0, run before and again after the ledger change.
- lodgen_cardlink: PASS 0 failures (RUNG/CARDS from the cardlink1 worktree; it refuses rc=2 without them).
- lodgen_incremental: FAILURES 0.
- lodgen_loadorder: 24/0.
- lod_generation: PASS, 128 checks (floor 121).

## 4. Exe sha1 and commits
- dca43d83aac6e306fb5fffc1f7c4dda65e8a1bd9 (release/NifSkope.exe == release/NifSkope.before_gatefix1.exe).
- 3e343a8: tests/baselines: stock baseline re-pinned; both moves named.
- d0a8e55: tests/spells/native_open: every window in its own wiped settings scope.
- 1d2c769: tests/spells/lodgen_btofree: re-pinned to before_gatefix1; three harness defects fixed.

## 5. What the final bake needs
- **Director:** copy this worktree's release/NifSkope.before_gatefix1.exe (sha1 dca43d83) into main's
  release/. Without it, lodgen_btofree skips its byte legs ("no rung exe on disk"). Once main's exe is
  rebuilt, the harness takes the --generators-differ path automatically.
- native_open needs main's fixtures by absolute path (LODL/NATIVE/SHEETS/OBJ/RES under
  scratchpad/showcase1_20260912 and nativeview1_20260912). See progress.md.
- lodgen_incremental rewrites the tracked scratchpad/incr_gate_work/* files when it runs. Never commit them.
- No generator defect was found, so the bake itself is unaffected. No defect was seen in CARDFIX1's regions.

## 6. Skill review
- nifskope-ww-worktree-build: worked as written, with one gap. Its runtime copy list omits the historical
  release/NifSkope.before_*.exe rungs, and a rung exe only starts beside the DLLs. Worth one line.
- nifskope-ww-lodgen: the btofree paragraph still names before_btofree1 as the pin. It should say
  before_gatefix1, and that a rung writing a .lodj is swept rather than excluded.
- New: E:\Projects\Claude\.claude\skills\ww-stale-gate-attribution\SKILL.md covers:
  - the rung sweep and bisection when git is squashed;
  - naming each move with a dump diff, the decoder census and the .lodi version word;
  - the failed-models list diff;
  - re-pinning without loosening, including the generator word;
  - per-window settings scopes for GUI gates.
  It is not added to E:\Tools\AISkills: that repo is FO4 modding skills, and this is WW harness practice.
