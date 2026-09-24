# Lane IMPOSTORRING1 -- tree cards photographed at 22.5 degree steps around the horizon

Director brief, 2026-09-23 04:4x. Model: Opus 5.5. You own the BUILD + EXE slot (no other lane is live).
Folder: scratchpad/impostorring1_20260923/ . Progress lines to progress.md INCREMENTALLY.

## bungo's words (2026-09-23 04:4x)
He saw the maple's full _d sheet (4x4 hemi-octahedral: 12 frames on the horizon ring, unevenly spaced, and 4
middle frames looking down from above that a ground player almost never uses) and said:
"Ah yeah, that's the issue, for fo4cs use the convention was 22.5 degrees per take or something like that".
=> RULED: tree cards are photographed every 22.5 degrees of azimuth = 16 azimuths, uniform, at the horizon.
The noise he means is the blend between neighbouring baked angles (the stipple the tear repair left); a
uniform 22.5 step puts every view at most 11.25 degrees from a baked frame (today: up to ~22+ at N4, bunched
toward the diagonals). Same 16 frames as now, so the same sheet pixels -- they are just all spent where the
player looks.

## Read first
CONSTITUTION.md; HANDOFF.md lines 360-372 (IMPOSTORTEAR1 LANDED + the three before it);
scratchpad/impostortear1_20260923/DELIVERABLE_TEXT.md + FOR_BUNGO.md; docs/LODGEN_CARD_SHEETS.md §10.2
(the aggregate RING layout `views x 1`, eye/right/up per azimuth -- an existing format; reuse it, do not invent
a third); docs/LODGEN_IMPOSTOR_SPEC.md; docs/LODGEN_LODM_FORMAT.md (`views` vs `oct`). Skills:
nifskope-ww-build-verify, nifskope-ww-lodgen, nifskope-ww-render-shot, ww-reference-card-diagnose,
ww-test-harness-add, ww-downsample-gate.

## Jobs
1. MEASURE FIRST (pre-register the bars in progress.md before any code): on the current exe (c172ba9d), the
   N4 maple + 3 other trees from the IMPOSTORTEAR1 set: azimuth sweep at 1-degree steps at elevation 0, 5, 15
   degrees -- card vs mesh IoU, stipple/hole share, per-step popping. That is the baseline.
2. BAKE: tree cards default to a horizon ring of 16 azimuths (22.5 degrees), frame v at azimuth 22.5*v,
   written through the ring layout the aggregate already uses (views = 16). The .lodm says which layout a
   card carries; if the tree card's meaning changes, bump the version so an old reader refuses it. Keep: 4x
   offscreen supersample, coverage-weighted downsample, vanilla 128 cut, per-frame offsets, one scale per card,
   the size ladder. Keep the hemi-octahedral N x N path working (it is not deleted; the ring becomes the TREE
   default). Sheet shape: say what you chose (16x1 row vs 4x4 tiling of the ring) and why (texture limits,
   mips, FO4CS reader simplicity).
3. DRAW (res/shaders/impostor_oct.frag + src/gl/impostordraw.cpp): a ring card blends the TWO neighbouring
   azimuths (not three), weights by angle. Views above the horizon use the ring frames as they are (no top
   frames); REPORT the error at 5/15/30/60 degrees of elevation, and state what a second raised row (16 more
   frames at ~30 degrees) would buy, measured -- do not ship the second row, it is his call.
4. Gates: impostor_draw.sh, impostor_aa.sh, lodgen_octahedral.sh (the N x N path still green),
   native_lighting.sh as control (2 reds pre-existing). Add rows: ring azimuths uniform at 22.5 (fails on the
   rung), max popping step, IoU at the in-between angle 11.25. Each new row FAILS on the rung. Never lower a
   bar. The stipple from the tear repair: if the finer step lets the cut go back toward a plain hard cut
   without the tear returning, measure it and say so -- ship whichever passes both tear and popping bars.
5. PICTURES for bungo: the same trees as IMPOSTORTEAR1's contact sheet, vanilla model | old card | new card, at
   azimuths including 11.25 (worst in-between), elevation 0 and 15. Plus the new maple _d sheet with the
   alpha applied over dark grey. Headline the contact sheet in FOR_BUNGO.md, plain words.

## Rules
Game down before any build (check Fallout4.exe). bungo's NifSkope window may run release/NifSkope.exe directly:
never kill it; rename aside. One harness NifSkope at a time, second monitor, no focus steal. Rung
release/NifSkope.before_impostorring1.exe first. The FO4CS reader is NOT this lane (FO4CS readers are built
last); write the contract change into docs only. Do not commit; do not edit HANDOFF/WW_CHANGES/MISTAKES --
text into DELIVERABLE_TEXT.md. DONE marker first word DONE/PARTIAL/PENDING. Final report under 300 words:
verdict, before/after IoU + popping + stipple numbers per elevation, gates, picture paths, exe size/time/sha1.
