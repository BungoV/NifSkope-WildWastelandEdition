# Lane IMPOSTORDEPTH1 -- snap GIF first, then a per-pixel depth search so the trunk stops doubling/vanishing

Director brief, 2026-09-23. Model: Opus 5.5. QUEUED behind IMPOSTORSHRUB1 (build slot + the one harness
NifSkope). Folder: scratchpad/impostordepth1_<date>/ . Progress lines to progress.md INCREMENTALLY.

## bungo's words
- Asked if he'd been shown the card SNAPPING (one angle, no blend): no; offered a snap GIF: "awesome".
- "sometimes part of the trunk lag behind" in scratchpad/impostor16_20260923/n8/gifs/
  maple_3dmodel_vs_8x8_stipple_vs_crisp.gif, the crisp panel. Offered the per-pixel depth search: "Yes, queue".
- Standing ruling 2026-09-23: crisp over smooth (choppy is fine for LOD; noise is the defect; tear too).
- Slider ruling (docs/FO4CS_IMPROVED_LOD_PLAN.md R4 "TRANSITION SMOOTHNESS SLIDER"): 0 snap .. 1 smooth.

## The director's measurements (source renders, not the GIF; session scratchpad trunk_lag.py)
- Stipple (n8/lit3/n8_2k): a DOUBLED trunk between baked angles; trunk-band pixels card/mesh median 1.16,
  >1.2 at 48 of 120 azimuths; az 345 = two trunk runs 22 px wide vs the mesh's one of 14.
- Crisp A (n8/lit3_A/n8_2k): the trunk VANISHES mid-way between angles (az 315: 35 trunk px vs the mesh's
  1,586), comes back broken and shifted at 318; card trunk moves up to 13.4 px per 3-degree step vs the
  mesh's 2.9. Cause: the two frames' trunks do not overlap, each averages ~half, under the 128 cut.
- Root: the one-step height parallax aligns the crown shell, not the trunk (another depth). Epic's article
  (shaderbits, Brucks 2018): single offset "works really well for very dense trees ... for very sparse and
  busy trees, this single offset will show lots of noisy artifacts"; the higher-quality version is a
  per-pixel parallax material.

## Jobs
1. SNAP GIF (deliver FIRST, write it to FOR_BUNGO.md the moment it exists): a snap mode in the card draw
   (nearest single frame, no blend -- slider 0) behind the harness/env switch like WW_IMPOSTOR_CUT; GIFs
   labelled "3D model" | "8x8 snap" | "8x8 crisp (A)" | "16x16 snap", same orbit/light/material as the
   IMPOSTOR16 GIFs, <= ~15 MB; plus the trunk close-up strip (mesh over card, 306..321 by 3 and 339..351).
2. PRE-REGISTER a trunk bar in progress.md before code: over the 1-degree sweep at el 0 and 20, per variant:
   trunk-band pixel ratio card/mesh (bar 0.85..1.15 at every azimuth), trunk run count (never more than the
   mesh's), trunk-centre per-step motion (bar <= 2x the mesh's), plus whole-tree IoU / missing / worst pop /
   tear share as before. Prove the bar FAILS on the shipped drawer (stipple) and on crisp A.
3. DEPTH SEARCH in res/shaders/impostor_oct.frag: each of the three frames finds its own surface along the
   view ray through its height channel (a short linear march + one refinement step; fewest taps that pass;
   report taps and cost in texture reads per pixel), then blend/cut as the slider says. Keep: vanilla 128
   cut, per-frame projection, frameOffset, one scale per card. Report whether crisp A with the search passes
   the trunk bar AND the tear bar; if so it is the candidate default for the crisp side of the slider.
4. Gates: impostor_draw.sh, impostor_aa.sh, lodgen_octahedral.sh, native_lighting.sh control (2 reds
   pre-existing); new rows = the trunk bar (fails on the rung). Never lower a bar.
5. PICTURES: the same four-panel GIF with the search on: "3D model" | "8x8 snap" | "8x8 crisp + depth
   search" | "8x8 stipple + depth search"; trunk close-up strip before/after.

## Rules
CONSTITUTION.md first; skills nifskope-ww-build-verify, nifskope-ww-render-shot, nifskope-ww-lodgen,
ww-reference-card-diagnose, ww-test-harness-add. Game down before any build. bungo's NifSkope window: never
kill; rename aside. Rung release/NifSkope.before_impostordepth1.exe first. One harness NifSkope at a time,
second monitor, no focus steal. Do not commit; do not edit HANDOFF/WW_CHANGES/MISTAKES -- text into
DELIVERABLE_TEXT.md. DONE marker first word DONE/PARTIAL/PENDING. Final report under 300 words, plain words.
