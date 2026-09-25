# Lane GATEFIX2 -- native_lighting gate (a): two legacy BTR baselines red (BUILD lane, small)

Director brief 2026-09-25 01:4x. Opus 5.5, account A, in-session agent. Worktree E:\Projects\NifskopeWWE-gatefix2,
branch gatefix2-20260925 from main b1cd5bc. Read scratchpad/brief_lodnight_common.md (main tree copy) FIRST.
Skills: ww-stale-gate-attribution (use it -- this is exactly its case), nifskope-ww-worktree-build, nifskope-ww-lodgen.

## Why
tests/spells/native_lighting.sh on main's exe (623b26cc, and CARDFIX1's step-5 exe) reads 21 checks / 2 failures:
"gate (a): legacy_btr_top is byte-identical to its baseline" and "legacy_btr_obl ...". The final whole-Commonwealth
bake needs this gate able to vouch. GATEFIX1 (scratchpad/gatefix1_20260924/DONE.md) found the stock baseline moved by
bungo's 09-18 "Alpha test 80 looks better" ruling and CELLVIEW2b's absolute-material-path fix (squashed 85c0b14):
check those first.

## The work
Attribute each moved baseline to the landed change + ruling that moved it by sweeping the saved rung exes; if every
move is explained, re-pin the baselines (no loosening); if one is NOT explained, it is a generator defect -- fix it
(outside the card regions of src/lodgen.cpp, impostorcard.*, impostordraw.*, lodmfile.cpp: lane CARDFIX1) or report.
Gates: native_lighting.sh 21/0 with a red (a corrupted baseline or the pre-move exe fails gate (a)); kept green
lodgen_native, lodgen_btofree, lodgen_native_baseline.
Report per the common brief (scratchpad/gatefix2_20260925/DONE.md + DELIVERABLE_TEXT.md).
