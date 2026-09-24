# Lane TILING4 (RESUME, 2026-09-12 01:4x) -- pick up the dead lane from its own files; same lane dir, same report, appended

## What happened
The first TILING4 agent stopped writing at 00:40 (last file `scratchpad/tiling4_20260912/logs/g0_grain.txt`) and never
returned. Nothing it did is lost: the report `scratchpad/lane_tiling4_report.md` is complete through section 1 (Gate F1
passed: split frozen 00:11, instrument law 00:27, controls 7/7, 7/7, 5/7), and `scratchpad/tiling4_20260912/` holds
`t4_lib.py`, `pool.json`, `coverage.json`, `s1*_*.json`, `swirl_law.json`, `h0_smoke.py`, `h1_sweep.py` / `h1_sweep.json`,
`h_cand.py`, `g0_grain.py` / `g0_grain.json`, `cache/`, `logs/`. READ ALL OF IT FIRST, then `scratchpad/brief_tiling4.md`
(the original brief: every gate, rule and deliverable there still stands unless this page says otherwise), then
`CONSTITUTION.md` and the HANDOFF top block. Do not redo the instrument, the split or the H1 sweep; do not touch a frozen
law. The BUILDING marker is already there; DONE when finished.

## Where it stood (from its own logs)
- `logs/h1_sweep.txt`: H1 hex tiling (256 and 341, bias 0 to -1.25) reads NO swirl on 7 of 7 (swirl gate passed on
  every row) and repeat 5 of 7; nothing gated 7 of 7 because the grain and band columns were 0-4 of 7. Its last line:
  "H3 (the warp capped at strain 0.5 plus H1/H2) is the brief's next rung and it is what happens next."
- `logs/g0_grain.txt`: the brief's grain gate as literally written (each sheet within 20 % of the SAME chunk's vanilla)
  is not a gate on the sampler -- vanilla's grain per sheet spans a factor of 24 set by which land textures are painted,
  and neither the rung (1 of 7) nor TILING3's proposal (4 of 7) meets it. It registered two attainable gates instead:
  G1 the median grain over the seven within 20 % of vanilla's median over the same seven (TILING3's own criterion), and
  G2 per sheet within 20 % of THE RUNG's grain on that sheet (no regression from the sampler change).

## Director's decision on the grain gate (this replaces brief_tiling4.md gate F3's grain clause)
G1 AND G2 are the grain gates for the default, on the selection seven and the validation seven. The literal
per-sheet-vs-vanilla number is still reported beside them for every candidate so nothing is hidden. The band-table
clause gets the same treatment: median of the six-band shares within 20 % of vanilla's median (G1-band) and per sheet
no worse than the rung's band error (G2-band); report the literal per-sheet count beside it. Repeat and swirl stay
per sheet with their own ceilings exactly as frozen.

## The work, from here
1. Re-score H1's rows under G1/G2 (one table, all candidates, `h2_rescore.py`), then run H3 (warp capped at strain
   0.5 + H1) and H2 (per-cell rotation) only if H1 alone does not gate 7 of 7 under the decided gates. Then the frozen
   validation seven on the winner. Section 2 of the report.
2. Transcribe the winner into `sampleLtex` behind `--land-sample stochastic` (warp kept as `--land-sample warp`),
   parity C++ vs prototype at fifteen positions x five settings. Section 3.
3. Build once (game check before the link; bungo's own NifSkope window is OPEN since 01:29, pid 60820, no `--port` --
   rename the exe aside, never kill it), the chain at TILING3's baselines, gates F2 and F4. Section 4.
4. Pictures `images/cmp_tiling4.png` and the whole-sheet triptych. Section 5.
5. Sections 6, 7, 8; `WW_CHANGES_ENTRY.md` (starts with a `## 2026-09-12 — <title>` line, em dash), `HANDOFF_BLOCK.md`,
   `MISTAKES_ENTRIES.md` (the first agent's silent death is NOT your mistake; the director's is: an hour without a
   liveness check). `docs/LODGEN_TERRAIN_VT.md` amendment, `BAKE_INSTRUCTION.md` if the default changes.
6. Write to disk after every step (the last lane's work survived only because it did). PENDING.md past half context.

## Rules
As brief_tiling4.md: one build + counted relinks; no tone, tiling-constant, road, vanilla-copy or crevice changes;
never his installed files; never `git stash`, never commit; plain language; every number beside its floor or ceiling.
Do not touch `scratchpad/brief_uinotes_20260912.md` or anything under the timeline / animation workspace.
