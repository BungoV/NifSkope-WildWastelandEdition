# Lane BLENDSEAM1 -- make the two terrain colour writers agree with edge blend on

Director brief, 2026-09-23 21:5x. Model: Opus 5.5. Folder: scratchpad/blendseam1_<date>/. Write progress.md
INCREMENTALLY, one line per step.

## Why
- DEFAULTS2 made `--blend-edges quadrant` the default (exe 90e8a57e, NOT committed).
- Since then, `lodgen_terrain_vt.sh` V9a-1/-2 have been red. With the blend on, the terrain colour writer with `--vt`
  and the one without it disagree on 1.06% of texels, all near quadrant lines at the chunk edge. The difference already
  existed; the flag hid it.
- bungo RULED 21:5x: fix it now, before VTNORMAL1. The gate goes green WITHOUT loosening it.
- Read first: scratchpad/defaults2_20260923/DELIVERABLE_TEXT.md (findings + gates), progress.md.

## Jobs
1. Find the exact cause: which writer blends differently, and where (file:line for both paths). Measure it.
2. Make both paths produce the same bytes with the blend on. Decide which one is right from the blend contract in
   docs/; if the docs are silent, pick the one that matches the chunk-seam intent, and say why.
   - `--blend-edges off` must stay byte-identical to before.
3. Gates:
   - `lodgen_terrain_vt.sh` fully green, V9a included.
   - Seam score reported (DEFAULTS2: 0.977).
   - The DEFAULTS2 byte gates re-run.
   - `lodgen_panel_run`.
   - Prove V9a fails on the 90e8a57e rung.

## Rules
- Read CONSTITUTION.md first.
- Skills: search-lean before any search, nifskope-ww-build-verify, nifskope-ww-lodgen, ww-test-harness-add.
- The game must be down before any build.
- bungo's NifSkope window: check for a running one; never kill it, rename it aside. Before renaming, check the existing
  `NifSkope_inuse_<pid>.exe` copies.
- Rung release/NifSkope.before_blendseam1.exe first.
- One harness NifSkope at a time, on the second monitor.
- Do not commit. Do not edit HANDOFF, WW_CHANGES or MISTAKES; put the text in DELIVERABLE_TEXT.md.
- The DONE marker's first word is DONE, PARTIAL or PENDING.
- Final report under 300 words, in plain words.
