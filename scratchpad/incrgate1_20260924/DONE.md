DONE -- lane INCRGATE1 (LOD-E), 2026-09-24 22:05, branch incrgate1-20260924 (not merged, not pushed).

## 1. Skills loaded
nifskope-ww-worktree-build, nifskope-ww-build-verify, nifskope-ww-lodgen, nifskope-ww-panel-style,
ww-test-harness-add, search-lean.

## 2. What was built
- INCR1's ledger code moved from nifcli.cpp to src/lodgenchunkpass.{h,cpp}, so the CLI and the panel share one implementation.
- The identity word `gen1:<sha1>` over the effective settings goes into `switches`, so a moved default now refuses.
- The panel row "Rebake only what changed" (key `incremental`), OFF by default; it never refuses and falls back to a full bake instead.
- Plan 5 rows 6/13/25/26 are closed with gates and evidence in docs/FO4CS_IMPROVED_LOD_PLAN.md.
- Census-checker fix: ladder OFF means ladderGroup is 0.

## 3. Gates (b2 exe; red runs named)
- G1 lodgen_incr_identity.py: 11/0. Red: the rung has 6 failures.
- G2 lodgen_panel_incremental.sh: rung/OFF/ON each 137/0. OFF == rung (10 files, 45,582,390 B). Red: the rung tree read as the ON tree fails.
- lodgen_incremental.sh: 12 ok / 0.
- lod_generation.sh: 128/0.
- lodgen_panel_run.sh: 137/0.
- lodgen_native.sh: 32/0 with leg 13b. 14 ambiguous instances, worst 0.062501 u. Red: band 0 refuses at 3358.
- lodgen_native_baseline.sh:
  - --check against a rung-written baseline: 25/25 identical.
  - The CHECKED-IN baseline is stale (the rung is red on the same 6 files). Re-pin owed, not done.
  - --drop-proof: 2,628 / 42,560 dropped by stock, 0 by native. Reds ok.
- lodgen_sanctuary_pair.sh: 7/0, census 38/0/32. Red: doctored ladderGroup caught; truncated lodi refused.

## 4. Exe sha1 and commits
- b2 release/NifSkope.exe 3b3808fd60c8e961b6a34e2aa54b702335e3faa4 (21:45).
- Rung b2a2b8b4f9ec913aa6b56814a15e5df477fc766d.
- Commits: 6a8bc80, e2bef40, 96a3766, 64e0d69, plus the docs/row-6 commit that carries this file.

## 5. What the final bake needs
Nothing new. The row ships OFF. The first --incremental on any pre-lane record is one full bake.

Owed rulings for bungo:
- .lodj written by default
- --native with --incremental
- .lodo reuse waits for CARDLINK1

Owed fixes:
- g_ledgerAssetDigest never cleared (lodgen.cpp)
- no gate for a second panel run
- stock_baseline re-pin

## 6. Skill review
See DELIVERABLE_TEXT.md, "Skill review". Appended to nifskope-ww-panel-style (a row wrapping a refusing flag must not refuse) and nifskope-ww-lodgen (identity, the panel row, the section-5 gates).
