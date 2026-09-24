# GATEFIX1 progress

- 22:4x worktree up: objects+stash from main (main at b2f3073 sources, make -n 0), REVISION objects deleted, qmake, tools/ww_build.sh RC 0. Rung = release/NifSkope.before_gatefix1.exe sha1 dca43d83 (b2f3073 unmodified).
- copied 47 historical rung exes from main release/ into release/rungs/ (read-only copy) for bisecting.
- rungs moved from release/rungs/ into release/ (an exe needs its DLLs beside it); order from main's mtimes in rung_order.txt.
- STOCK BASELINE red: rung (dca43d83) --check = 6 CHANGED (baseline_check_rung.log). Swept all 47 rungs (sweep_baseline.sh, bl/): the 09-10 list held byte-exact through before_msncache1 and moved TWICE, nowhere else.
  - move 1, before_msncache1 -> before_slab1 (09-18 02:34 -> 05:18): 4.-20.24.bto, 8.-24.24.bto, 16.-32.16.bto, region BTO + its manifest. Dump diff: ONLY NiAlphaProperty Threshold 128 -> the BGSM's own alpha ref (127/65/80); the region manifest regroups because Threshold is in the merge key. Ruling in code, src/lodgen.cpp ~2314: bungo 2026-09-18 05:2x "Alpha test 80 looks better" (director VIEWFIX session builds, 09-18). Git: squashed into 85c0b14.
  - move 2, before_cellview2 -> before_cellview3 (09-19 16:48 -> 18:56): 16.-32.16.bto + manifest. Same block/tri/vert counts, same manifest rows; two shapes whose BGSM is an absolute Bethesda build path (C:\projects\Fallout4\Build\PC\Data\materials\LOD\DecoMainBLOD / SidingKitALOD) now resolve their textures + specular (was empty), and sort to their normalised place. Lane CELLVIEW2b (09-19): "lodgenReadAsset callers prepended materials/ to absolute build paths" fix, scratchpad/cellview2b_20260919/DELIVERABLE_TEXT.md. Git: squashed into 85c0b14.
  - regenerated with --write (exe dca43d83): equals the before_cellview3 sweep list exactly; --check 0 failures PASS; --selftest 0 failures PASS.
- native_open: shots now run in a scratch settings scope (SCOPE=nativeopen, wiped before and after).
- NATIVE_OPEN red (unmodified b2f3073 script, rung exe dca43d83, main's fixtures, RUN_LODL_OPEN=0): 16 checks, 2 failures -- (d) NCC -0.2094 < 0.45 and its refuter (native_open_rung.log). Same -0.2094 on every rung back to before_btofree1 (passed 17/0 on 09-16), so not the exe: the harness inherited bungo's persisted settings and the .BTR's water shape drew pure white.
  Fix (harness only): every shot in its own scratch WW_SETTINGS_SCOPE=nativeopen, wiped before EACH window (one wipe per run left a saved layout that moved the viewport 991->989 rows and (c) refused) and at exit; lodl_open.sh sub-gate runs with env -u LODL -u PORT so our LODL= never re-points it.
  Green: 17 checks, 0 failures, 2 skipped (nativeview1 rung absent; GBAKE unset) -- COVER 0.9927 FAT 1.83, NCC 0.8411 (other chunk 0.2284, flip -0.047), MAD 20.8, lodl_open 23/0. Scope key gone after the run; stray gatefix1iso key deleted.

## btofree moves named (native bake decoded, npairs/ + logs)
- viewfix->authored (lane VIEWFIX window, 09-17): .lodo meshes 5567->5562; the 5 new "failed" models are
  exactly the Hightech HitExt*DummyLOD.nif (all-EditorMarker shapes) = EditorMarker exclusion, bungo 2026-09-17
  "magenta squares on the roofs" (src/lodgen.cpp ~2149). .lodi follows (pairs to the .lodo identity).
- msncache1->slab1 (09-18): .lodi v5->v6 (41638->526571 B) = vertex-AO stream, bungo 2026-09-18 04:0x
  "there is no vertex AO on the objects" (doc 4.8); .lodo + BTO = alpha ref ruling "Alpha test 80 looks better" 05:2x.
- lodiv7->btdterrain_rebuild: .lodi v7 group table + sky stream (LODIV7, bungo 2026-09-18 09:4x, doc 4.9/4.10).
- horizon1->horizon2, horizon2->horizonout: .lodi v8 horizon stream (HORIZON1/2).
- horizon3->gltfexport1: .lodi v8->v7 (8679308->1013995 B), horizon retired, bungo 2026-09-19
  "horizon goes bye bye now, we're back to identity" (doc 4.11).
- cellview2->cellview3: .lodo/.lodi = CELLVIEW2b material-path fix (same as the stock baseline move 2).
- DDS defaults2->blendseam1 = DEFAULTS2 --blend-edges quadrant default (bungo 2026-09-23 09:3x);
  blendseam1->? second DDS move pending bisect (BLENDSEAM1 says the stock colour sheet moved with the blend on).
- BISECT-DONE: 9 moves. Last one blendseam1->vtnormal1: chunk colour DDS = BLENDSEAM1 (with --blend-edges
  quadrant the stock chunk colour sheet now cross-fades like the VT sheet; its DELIVERABLE_TEXT "stock on: the 4
  colour DDS"). Every btofree moved file is named; no generator defect found.
- BTOFREE harness, first run on the re-pin (bt1, btofree_run1.log): 30 checks, 4 failures, all HARNESS:
  (a) ".lodj: the rung wrote none" -- the pin now writes the native cache too; the exclusion would hide it.
      Fix: exclusion only for a rung with no .lodj; otherwise the .lodj is swept byte for byte (tighter).
  (a) ledger drop mode: AttributeError (strip_digests() on shape()'s TEXT) -- dead since AUDIT1 09-17, never
      reached because the old rung wrote a v1 record and the leg skipped. Rewritten: bto scratch row, bto clause,
      layout count and end line are each CHECKED against the out rows that entered the record folder; rest ==.
  (b)/(c) ledger same mode: census compared raw, so stage times + peak working set (the record's own documented
      volatile things, LODGEN_BAKE_RECORD.md section 3) and the out-dir path failed it. Now masked through
      lodb_read.normalise() + own --out-dir -> <out-dir>; a census content change still fails.
  ledger_refute.py: 8 mutated-record cases (content/digest/end/layout -> FAIL; volatile -> PASS): 8/0.
- GREEN: default pin (same exe) 30 checks 0 failures (btofree_green.log); padded-exe rung (sha 72459c94,
  --generators-differ path) 30 checks 0 failures (btofree_genword.log), inputs moved 1 of 1 in all three legs.
- lodgen_native rerun after the ledger change: 32 checks, 0 failures, RESULT PASS (kg_lodgen_native_after.log). Old-script red recorded: b2f3073 script + helper, pin before_btofree1: 27 checks, 5 failures (btofree_red_b2f3073.log) -- (a) 3 differ (.lodo .lodi colour DDS), (a) .lodo, (a) .lodi, (b) 4 differ (+ .BTO), (c) 2 differ (.BTO + colour DDS).
