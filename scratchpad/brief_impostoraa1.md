# Lane IMPOSTORAA1 -- anti-aliased card bakes: render at 2x the frame, downscale

Director brief, 2026-09-22 23:1x. QUEUED: launches when IMPOSTORLIGHT1 lands (one build lane at a time).
Model: Opus 5.5. Folder: scratchpad/impostoraa1_20260922/ . Progress lines to progress.md INCREMENTALLY.

## bungo's words (2026-09-22 23:1x)
"Also, are the bakes for these anti aliased?" then "Preferable solution would be a 2x render then downscale
to achieve AA".

## What the director read (verify, do not trust)
- The bake photographs the LIVE VIEWPORT: grabOnce() (src/nifskope_ui.cpp ~:22795) = grabFramebuffer() of the
  window, then frameOf() (~:23240) crops the silhouette and QImage::scaled(iw, ih, SmoothTransformation) to the
  frame. So the source resolution is the WINDOW's size, and its anti-aliasing is whatever MSAA the window's
  surface format carries (src/glview.cpp:343, fmt.setSamples( 1 << aa ), a user setting).
- Consequences to MEASURE: (1) at cardRes 512 the crop may be SMALLER than the frame, i.e. the frame is
  magnified, not downsampled (report crop px vs frame px per subject at 128/256/512); (2) the result depends on
  the window size and the AA setting, so two machines bake different sheets; (3) SmoothTransformation is
  bilinear, not an area filter, at large ratios.
- grabSupersampled( shift ) exists at src/glview.cpp:21944 (offscreen FBO at 2^shift x viewport) -- a
  candidate to reuse, not a requirement.

## The ruled design
Each view renders OFFSCREEN at EXACTLY 2x the frame's own size (2*tw x 2*th, or the crop rect at 2x the
frame's inner size), independent of the window size and of the user's AA setting, then downsamples 2:1 with a
2x2 area average. Never upscale. Rules for the data channels:
- coverage: the average of the 4 samples (true 4-sample coverage).
- colour, normal, height, sway, masks: coverage-weighted average of the covered samples (premultiply, box,
  un-premultiply), keeping the existing coverage floor and dilate. Renormalise the averaged normal.
- MSAA OFF on the channel renders (MSAA resolve would blend data channels behind our back); the 2x2 average IS
  the anti-aliasing.
- Keep every existing contract (per-frame offsets, one scale per card, size ladder, the refusal when cards were
  baked at another size). If the sheet bytes change meaning at all, say so; if they only get cleaner, no
  version bump.

## Gates and proof
- A known-answer control first: an analytic shape (the cube/sphere fixtures lodgen_octahedral.sh uses) whose
  exact edge coverage is computable -- edge coverage error before vs after, and a proof the new code is
  WINDOW-SIZE INDEPENDENT (bake twice at two window sizes, sheets byte-identical or within 1 level).
- impostor_draw.sh, lodgen_octahedral.sh, native_lighting.sh as control; no bar lowered. Add rows for the two
  facts above and prove each FAILS on the rung exe.
- Rebake the five subjects at cardRes 512; bake time before/after (cost is 4x the pixels).
- Pictures for bungo: per subject, a 3x zoom crop on an edge (twigs, trunk edge) before | after, beside the
  mesh; plus the every-bake-angle sheet. Headline two in FOR_BUNGO.md.

## Rules
Read CONSTITUTION.md and HANDOFF.md lines 1-15 first; skills nifskope-ww-build-verify, nifskope-ww-lodgen,
nifskope-ww-render-shot, ww-downsample-gate, ww-test-harness-add, ww-reference-card-diagnose. Game down before
any build. bungo's NifSkope window: never kill; rename aside. One harness NifSkope at a time, second monitor.
Rung release/NifSkope.before_impostoraa1.exe first. Do not commit; do not edit HANDOFF/WW_CHANGES/MISTAKES --
text into DELIVERABLE_TEXT.md. DONE marker first word DONE/PARTIAL/PENDING. Final report under 300 words.

## Addendum 00:1x 2026-09-23 (director, after IMPOSTORLIGHT1 landed at 89574e81)
- The rung is now the IMPOSTORLIGHT1 exe (89574e81). bungo's window (pid 25584) runs release/NifSkope.exe
  DIRECTLY: rename it aside before linking.
- MEASURE, then fix only what is the bake's: in scratchpad/impostorlight1_20260922/pics/headline_before_after.png
  (both before and after) the blasted maple N4 card has lost its UPPER TRUNK, the forest maple card its MID
  TRUNK, and the cliff rock card is holed and has lost its crevice contrast. Name the stage for each (thin
  coverage under the 128 cut, the frame blend between views, parallax/height, or the bake's magnified crop),
  with numbers. If the 2x bake cures it, show it; if not, report the named cause and a ranked repair list --
  do not apply a repair outside the ruled design without the director's word.
