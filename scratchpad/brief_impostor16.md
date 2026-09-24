# Lane IMPOSTOR16 -- bake the green maple at 16x16 frames, show every bake, GIF orbit of the card

Director brief, 2026-09-23 04:5x. Model: Opus 5.5. You own the BUILD + EXE slot (no other lane is live;
IMPOSTORRING1 is HELD behind you, brief scratchpad/brief_impostorring1.md -- do not start it).
Folder: scratchpad/impostor16_20260923/ . Progress lines to progress.md INCREMENTALLY.

## bungo's words (2026-09-23 04:4x-04:5x)
After seeing the maple's 4x4 _d sheet: "So bake a tree, preferably the green one, with 16x16 frames, show all
the bakes to me, then make a gif showing you turning around that imposter tree". Then: "Resolution either 2
or 1k". Director's reading: the WHOLE SHEET is 2048 or 1024 (16 frames per side = 128 px or 64 px frames;
1k-per-frame would be a 16384 sheet). Bake BOTH (cardRes 128 = 2k sheet is the headline, cardRes 64 = 1k).
This is a LOOK TEST, not a new default: nothing in the shipped defaults changes.

## Subject
TreeMapleInstitute06Green (the IMPOSTORTEAR1 subject mapleinst06g, form 000531b3; its bake and scripts are in
scratchpad/impostortear1_20260923/, reuse tear1_run.sh / sweep.sh as the pattern).

## Jobs
1. Bake it hemi-octahedral N16 at cardRes 128 and 64 on the current exe (c172ba9d, 4x supersample, vanilla 128
   cut, stippled cut as shipped). If N16 is refused or broken anywhere (bake, sheet size, .lodm, drawer), fix
   the smallest thing, build per nifskope-ww-build-verify, and say what it was. Report bake time and sheet
   bytes vs the N4/512 bake.
2. SHOW ALL THE BAKES: every sheet as a PNG he can look at -- _d colour with alpha applied over dark grey, _d
   raw colour, _d alpha, _n (normal as colour, height, sway as separate greyscale), _g / gsaos -- for the 2k
   sheet, plus the 1k _d. One index image or one folder, named plainly.
3. GIF: the drawn impostor card turning a full 360 degrees at elevation 0 (2 or 3 degree steps, ~20-25 fps,
   headlight fixed to the world, not the camera, so the lit side stays put), the tree filling most of the
   frame. Make it for N16/2k. Also a side-by-side GIF: vanilla model | N4/512 card (current) | N16/2k card,
   same camera, same light. Keep each GIF under ~15 MB (crop, palette); say its size.
4. Measure over the same sweep (1-degree steps): card vs mesh IoU, hole/stipple share, worst per-step popping,
   N4/512 vs N16/2k vs N16/1k. Numbers, not adjectives.

## Rules
Read CONSTITUTION.md first; skills nifskope-ww-lodgen, nifskope-ww-render-shot, nifskope-ww-build-verify,
ww-reference-card-diagnose. Game down before any build. bungo's NifSkope window may run release/NifSkope.exe
directly: never kill it; rename aside. One harness NifSkope at a time, second monitor, no focus steal. If you
build, take the rung release/NifSkope.before_impostor16.exe first. Do not commit; do not edit
HANDOFF/WW_CHANGES/MISTAKES -- text into DELIVERABLE_TEXT.md. DONE marker first word DONE/PARTIAL/PENDING.
Final report under 300 words: verdict, the picture + GIF paths (absolute), numbers, exe if rebuilt.
