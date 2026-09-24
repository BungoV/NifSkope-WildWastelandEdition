# Lane IMPOSTORTEAR1 -- stop the cards tearing, bake at 4x, show a range of vanilla trees

Director brief, 2026-09-23 01:3x. Model: Opus 5.5. You own the BUILD + EXE slot (no other lane is live).
Folder: scratchpad/impostortear1_20260923/ . Progress lines to progress.md INCREMENTALLY.

## bungo's words
On the torn cards: "like somebody ripped out a piece of paper". Then: "Is the issue fixed? If so send me bakes
of different trees, with their vanilla model next to them". It is not fixed yet; he wants it fixed and then
the pictures. Director rules on his behalf, per the recommendation he was given:

## Read first
CONSTITUTION.md; HANDOFF.md lines 1-15 (IMPOSTORAA1 LANDED PARTIAL line); scratchpad/impostoraa1_20260922/
DELIVERABLE_TEXT.md section "THE TEAR" + FOR_BUNGO.md + the ranked repair scripts it used. Skills:
nifskope-ww-build-verify, nifskope-ww-lodgen, nifskope-ww-render-shot, ww-reference-card-diagnose,
ww-test-harness-add, ww-downsample-gate.

## Jobs
1. TEAR REPAIR, fix (2) of IMPOSTORAA1's ranked list: the alpha cut is decided on the STRONGEST contributing
   frame's coverage, not the 3-frame average (res/shaders/impostor_oct.frag); colour/normal still blend.
   PRE-REGISTER a popping bar before measuring: an azimuth sweep at 1-degree steps (and an elevation sweep)
   per subject, the per-step change in covered pixels, shipped drawer vs repaired; bar = the repaired drawer's
   worst step no worse than the shipped drawer's worst step x 1.5 (write it in progress.md first).
   If fix (2) fails that bar, try the smallest step toward fix (1) (a short depth march, fewest taps that pass)
   and report taps and cost. Torn share, IoU at the torn views before/after.
2. BAKE SUPERSAMPLE 2x -> 4x (bungo's 2x design, raised because 2x cannot pass lodgen_octahedral F1: 1.27 vs
   bar 1.0; 4x read 0.68). Keep the ruled rules (offscreen, MSAA off, coverage-weighted average, window-size
   independent). Report bake time 2x vs 4x.
3. Gates: impostor_draw.sh (row 5 was 0.4699 red), impostor_aa.sh, lodgen_octahedral.sh (F1 must go green),
   native_lighting.sh as control (2 reds pre-existing). Add a tear row + a popping row; each FAILS on the rung.
   Never lower a bar.
4. PICTURES for bungo -- the deliverable: pick 8 or more DIFFERENT vanilla trees from
   E:\Tools\Fallout 4\DataUnpacked\Data (conifers, maples, dead/burnt, blasted, a leafy one if vanilla has one,
   a shrub-sized one; name each by its vanilla path). Bake each at cardRes 512 on the new exe. One sheet per
   tree: the vanilla model | the card, at 4 azimuths INCLUDING in-between (non-baked) angles and one raised
   elevation, same camera, same scale, same light. Plus one contact sheet of all trees side by side.
   Headline the contact sheet in FOR_BUNGO.md.

## Rules
Game down before any build. bungo's NifSkope window may run release/NifSkope.exe directly: never kill it;
rename aside. One harness NifSkope at a time, second monitor, no focus steal. Rung
release/NifSkope.before_impostortear1.exe first. Do not commit; do not edit HANDOFF/WW_CHANGES/MISTAKES --
text into DELIVERABLE_TEXT.md. DONE marker first word DONE/PARTIAL/PENDING. Final report under 300 words:
verdict, tear + popping numbers, 4x numbers, gates, picture paths, exe size/time/sha1.
