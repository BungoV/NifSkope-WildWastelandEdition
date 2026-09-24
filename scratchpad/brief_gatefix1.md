# Lane GATEFIX1 -- the LOD gates that were already red (BUILD lane)

Director brief 2026-09-24 22:4x. Opus 5.5, account A, in-session agent. Worktree E:\Projects\NifskopeWWE-gatefix1,
branch gatefix1-20260924 from b2f3073 (main with LOADORDER1, VTFIX1, ESMFIX1, INCRGATE1, TOOLFIX1, CARDLINK1 merged).
Read scratchpad/brief_lodnight_common.md (main tree copy) FIRST. Bring the worktree up with nifskope-ww-worktree-build.

## Why
bungo: "Finish Nifskope side completely overnight" -- the LOD generator side. Three gates were found red on the
PRE-lane exes too (not caused tonight), and a red gate cannot vouch for the final whole-Commonwealth bake:
1. tests/spells/native_open.sh -- 3 failures (CARDLINK1: identical verdict lines on its rung exe).
2. tests/spells/lodgen_btofree.sh -- 5 failures (same).
3. tests/baselines/stock_baseline.sha256 is stale (INCRGATE1: its check fails with the same 6 files on the rung;
   against a baseline written from the rung the exe matches 25/25).
Reports: E:\Projects\NifskopeWWE-cardlink1\scratchpad\cardlink1_20260924\DONE.md, E:\Projects\NifskopeWWE-incrgate1\
scratchpad\incrgate1_20260924\DONE.md.

## The work
For each: find WHY it is red -- a real defect in the generator (fix the code) or a gate whose expectation went stale
when a ruled change landed (find the commit + ruling that moved it, update the expectation, say which in DONE.md).
Never loosen a check to make it pass; a stale expectation is updated only with the commit that justifies it.
For the baseline: regenerate it only if every moved file is explained by a named landed change; list them.
Files: whatever the defect lives in, EXCEPT the card regions of src/lodgen.cpp, src/impostorcard.*, src/gl/impostordraw.*,
src/lodmfile.cpp, res/shaders/impostor_* (lane CARDFIX1 is live there). If a defect is in those, report it, do not fix.

## Gates
native_open.sh, lodgen_btofree.sh and the stock baseline check green, each with a red: the unmodified b2f3073 exe
fails them (already true -- record it). Kept green: lodgen_native, lodgen_cardlink, lodgen_incremental, lod_generation,
lodgen_loadorder.
Report per the common brief (scratchpad/gatefix1_20260924/DONE.md + DELIVERABLE_TEXT.md).
