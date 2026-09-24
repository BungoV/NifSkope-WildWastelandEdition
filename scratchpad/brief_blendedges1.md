# Lane BLENDEDGES1 -- the hard lines between terrain blend squares, shown fixed, then made the default

Director brief, 2026-09-23. Model: Opus 5.5. LIVE. Folder scratchpad/blendedges1_20260923/, progress.md incremental.

bungo, over the VTBAKE1 Sanctuary crop (scratchpad/vtbake1_20260923/images/sanctuary_L02_vs_vanilla_chunk.png):
"Haven't you fixed that already? ... the hard lines that are not blended".
History: TILING2 (2026-09-11) wrote `--blend-edges quadrant` (+ `--blend-margin`, default 128 u), a quintic cross-fade
of the neighbouring quadrant's composite across every 2,048-unit line; measured seam 1.236 -> 0.955. It ships OFF
(src/lodgen.cpp:6294 g_blendEdges = 0). It was only ever shown BUNDLED with `--land-sample average`, which bungo
rejected by eye ("solid color blobs"); the "blend-edges alone" picture and the default question were owed to him on
2026-09-12 and were DROPPED by the director. Standing rule since 2026-09-18: a repair never ships behind a switch.

## Jobs
1. Prove what the hard lines ARE before anything: on the VTBAKE1 L02 Sanctuary tiles, measure where the colour
   steps sit (period in texels; do they fall on the 2,048-unit quadrant lines = 64 texels at 32 u/texel, on the
   opacity-grid spacing = 4 texels, or elsewhere). Say which the flag can fix and which it cannot.
2. Bake the Sanctuary region (same 4x4-cell window TILING2 used, or the VTBAKE1 crop's chunk 4.-20.24) with the
   CURRENT defaults and with ONLY `--blend-edges quadrant` added (footprint sampling, NOT average), through the CLI
   into this folder, both the stock chunk sheet and the --vt pyramid. Exe: release/NifSkope.exe as is -- no build.
3. Picture for bungo: "current" | "blend-edges" | "vanilla", colour, same crop, labelled, plus a 2x zoom on the
   worst line; the seam number both ways. If lines remain inside quadrants (job 1), show them circled and say what
   would fix them (do NOT build it).
4. Do NOT flip the default in this lane: the director relays the picture; the flip is a one-line follow-up.

Rules: CONSTITUTION.md first; skill nifskope-ww-lodgen. CLI only, no GUI NifSkope (IMPOSTORSHRUB1 holds the harness).
Game down check before bakes. Scratch output only, never his Data. Do not commit; no HANDOFF/WW_CHANGES/MISTAKES
edits -- text into DELIVERABLE_TEXT.md. DONE marker first word DONE/PARTIAL/PENDING; report under 300 words, plain.
