# TILING4 -- LANE COMPLETE 02:34 (this file was the resume state; it is now the closing state)

`DONE` is in this directory and `BUILDING` is gone. The authority is
`scratchpad/lane_tiling4_report.md` (960 lines, sections 0-9, appended after
every step, never rewritten, CR 0). Nothing is owed by this lane; what is
listed at the bottom is owed by the OVERSEER, who owns the three shared files.

## What gated

* **F1** PASS (first agent): split frozen 00:11, swirl law 00:27, controls
  7/7, 7/7, 5/7.
* **Parity** C++ vs prototype: **0 disagreements of 75**; every setting moves
  the sample; `--sabotage` fails 56 of 75.
* **Build** 02:08:57, `release/NifSkope.exe` 21,487,616 B, BUILD-RC=0, one
  build, no relinks. `Fallout4.exe` down, checked twice. Nothing killed.
* **F2 byte identity** PASS, six arms, comparator shown red first: off == rung
  9/9 x2, the documented way back == rung, stochastic moves exactly 3 files
  (colour DDS + **both** `.lodt` -- the second `sampleLtex` site's own
  fingerprint), `_msn` == vanilla 10/10, `warp` == TILING3's bake byte for
  byte, 1 vs 16 threads 97 files 0 differ.
* **F4 the chain** PASS, 11 of 11 at baseline (23/0, 26/0, 41/1, 11/0, 29/5,
  14/0, 18/0, 125/0, 116/0, 11/0, 82/0).

## What did not gate

**F3 NOT MET, measured on the product** (`f3_full.sh` baked all fourteen
frozen-split sheets in three arms, 42 bakes rc=0; `f3_full.py` scored them with
`h1_sweep.score` and `t4_gates.decided` imported unchanged):

| | hex 256 | warp | rung |
|---|---|---|---|
| repeat | 4/7 and 5/7 | 5/7 and 6/7 | **0/7 and 0/7** |
| swirl | 6/7 and 7/7 | 3/7 and 4/7 | 6/7 and 6/7 |
| G2 grain vs the rung | 7/7 and 7/7 | 0/7 and 1/7 | - |
| G1 median vs vanilla | -14.3 %, -36.3 % | +11.3 %, -19.5 % | -24.0 %, -42.9 % |

**So the default did not change** and no `BAKE_INSTRUCTION.md` is owed. What
the lane ships is the MEANING of `--land-sample stochastic`: on fourteen real
bakes, against the warp bungo judged, the swirl goes 7/14 -> 13/14 and
grain-vs-the-previous-build 1/14 -> 14/14, at 11/14 -> 9/14 of the repeat.

**This supersedes the earlier line in this file** ("repeat 6 of 7 and 6 of 7 /
swirl 2 of 7 to 7 of 7 at an unchanged repeat count"), which was the offline
prototype's answer. The prototype has no crevice term and no vanilla reuse and
was wrong in both directions by up to 0.27 on the repeat; section 6 of the
report withdraws the claim explicitly and mistake 1 records it.

## Still red

1. The repeat: hex 9/14, warp 11/14 -- the swap costs two sheets.
2. The blotchiness at the 256-unit cell scale is **ungated**; `images/sheet_tiling4.png` at 1:1 is the only evidence and it is bungo's judgement.
3. `scratchpad/splat1_20260911/offline_bake.py` must not be used for absolute numbers again.
4. G1 / G1-band cannot separate arms: the rung fails both.
5. (-36,-20) fails TILING2's ratio law while its amplitude falls 1.501 -> 0.073 (the warp fails it the same way); possible artefact of the law's own floor.
6. A GUI `release/NifSkope.exe` pid 59040 (02:24:09, no args) was **not** this lane's -- all 42 bakes were `-no-gui` and exited. It was left alone and had closed by itself at **02:35**; at 02:35 no NifSkope and no `Fallout4.exe` is running, so the exe is free.

## Owed by the overseer, not by this lane

`WW_CHANGES_ENTRY.md` -> `WW_CHANGES.md`; `MISTAKES_ENTRIES.md` ->
`MISTAKES.md`; `HANDOFF_BLOCK.md` -> `HANDOFF.md`. The
`docs/LODGEN_TERRAIN_VT.md` amendment is **already in the tree** (2.5e, two CLI
rows, provenance with per-claim anchors; `d0_doc.py` then `d1_doc.py` for the
product's counts; 116,939 B, sha256 `b5fff76b436404d0`, CR 0, both refuse on a
second run). Nothing was committed and nothing stashed; bungo's installed files
were never touched; any NifSkope window older than 02:08:57 needs a restart.
