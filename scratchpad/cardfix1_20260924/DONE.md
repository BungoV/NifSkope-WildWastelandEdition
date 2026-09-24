PARTIAL -- lane CARDFIX1 (LOD-D), chain of seven steps; this file grows one section per landed step.

# 1. Skills loaded
nifskope-ww-worktree-build, nifskope-ww-build-verify, nifskope-ww-lodgen, nifskope-ww-render-shot,
ww-test-harness-add, search-lean (the common rules' list, loaded with the Skill tool before any work).

# 2. What was built, step by step

## Step 1 -- IMPOSTORDEPTH2 landed (code was already in the branch point)
- IMPOSTORDEPTH2's code is in the tree at 71f96c1 (commit 85c0b14 carried it in); nothing of it was
  re-applied. This step re-ran its gates on this worktree's exe and drops the two stale comments that
  said the tree has no BC7 encoder:
  - src/lodgen.h (the `_msn` cache note): now says the encoder exists (src/lodgenbc7.h, used for the
    card `_n` sheets) and that this cache does not use it -- BC7 in the card cache stays OFF (R9 "No").
  - src/nifcli.cpp (the cache-verify comment): "BC7 (src/lodgenbc7.h) is not used here."
  Comments only; no behaviour moved.
- lodgen_defaults.sh leg (d) (director's add, VTFIX1's red "no impostor-card lines with identity off"):
  NOT a generator defect. The spell's default card dir is repo-relative and git carries only the 24 `.txt`
  sidecars of that card set (the PNGs are untracked, main tree only), so in any WORKTREE the bake finds no
  card image and places no `C` line. Measured on the rung exe with CARDS = the main tree's
  showcase1_20260912/cards: d_new manifest rows 7626, C 7210, I 35. FIXED CHEAP in the spell: leg (d) now
  refuses BY NAME ("the card directory holds no card image ... set CARDS=") instead of reading as a
  generator failure. Kept-green runs pass CARDS=<main tree>/scratchpad/showcase1_20260912/cards.

## Step 2 -- the three "empty" models (Sapling01, TreeElmUndergrowth01, ShrubGroupLarge05)
- Diagnosis: ONE cause, already repaired before this lane. Every BSMeshLODTriShape of these models ships
  its LOD0 slot empty (0/n/m); the bake used to keep LOD0 only and so drew 0 triangles. IMPOSTORSHRUB1's
  per-model rule (keep the lowest slot any such shape of the model fills, sidecar `rangekept LOD<k>`) is
  in the branch point, so this worktree's rung exe ALREADY bakes all three. The brief's red ("the rung exe
  bakes them empty") therefore cannot be the rung; the red is the exe from before SHRUB1
  (main release/NifSkope.before_impostorshrub1.exe, 09-23 03:01, copied as release/NifSkope.pre_shrub1.exe).
- No code change in this step. Evidence script: step2.sh (impostor_shrubs.sh with MATCH per model).

## Step 3 -- IMPOSTORFIX5's owed re-runs
- impostor_draw.sh: re-run in step 1 on this lane's exe, 33 steps / 1 failure (row 5, the known red).
- cardres_test.sh (FIX5's frame-resolution discriminator, re-pointed at this worktree): run on exe ff86b488.
  A measurement, not a gate (FIX5 set no bar; the ruling on card resolution is bungo's).
- SHRUB1's "texel islands at el 20" (cedar01/02, hollyshrub01prewar): NOT measured -- not cheap (needs a new
  picture-based instrument). Step 4 below takes the stipple out of the DEFAULT draw by name, which is the
  class SHRUB1 suspected; if islands remain at the crisp end they are the frame's own coverage.

## Step 4 -- R5 defaults: N8, the crisp cut, the slider at the crisp end
- Checked first: N8 (the bake driver's OCT default, the panel's Card frames default) and the slider at
  the crisp end (frameCount 1, flat) were ALREADY the shipped defaults (IMPOSTORDEPTH2). Nothing moved there.
- The crisp cut is now the default BY NAME: `Resolved::cutRule`, which is 2 (the strongest frame) whenever
  one frame is drawn, and the Options cut otherwise; the shader's `cutRule` uniform reads the resolved rule
  (src/gl/impostordraw.h/.cpp). Before, the crisp end drew under the stipple rule and was crisp only because
  one frame at weight 1 happens to cut where that frame does. No pixel moves (D5, D7).
- The preview harness names the resolved cut in its log (src/impostorpreviewtest.cpp).
- New gate tests/spells/impostor_defaults.sh, rows D1-D7.

## Step 5 -- IMPOSTORRING1: a horizon ring card set (16 views x 1 row)
- `WW_IMPOSTOR_RING=V` bakes V views evenly around the horizon at elevation 0 into a V x 1 sheet (frame v
  at x = v*tw; eye (cos p, sin p, 0), right (-sin p, cos p, 0), up z). The sidecar says `ring V tw th` and
  echoes one `ringview v az el` per frame, which the gate reads back against the law (src/nifskope_ui.cpp).
- lodgen carries a ring set as `views` V / `grid` [V,1] with NO `oct` key and frameOffset 2V
  (src/lodgen.cpp card region; docs/LODGEN_LODM_FORMAT.md 3.2). The drawer picks the nearest azimuth frame
  (src/impostorcard.*, src/gl/impostordraw.cpp). An old exe refuses a ring set by name ("oct is 0, outside").
- Why 16 x 1: a tree seen from LOD distance is seen from within a few degrees of the horizon; a ring spends
  every texel there, and 16 x 1 at tile 256 is a quarter of the N8 sheet's pixels.
- The preview harness gained WW_IMPOSTOR_ORBIT_SELECT (src/impostorpreviewtest.cpp); the bake driver
  passes WW_IMPOSTOR_RING through and defaults the tree run to 16 (tools/bake_impostor_cards.sh); the
  FO4CS reader is owed (spec text only).
- FINDING for bungo: N8 is BETTER than the 16-view ring at every elevation measured, including the horizon.
  N8 already has 28 frames near the horizon (largest step 16.2 degrees), so the ring buys pixels, not shape.
  The ring sits at its own ceiling (the mesh against itself rotated by half a step). The bake driver's
  TREE run defaults to RING=16 as ruled (bungo 2026-09-23 04:4x, "22.5 degrees per take"; RING=0 = the
  N8 grid, the empty-slot run keeps the grid); the numbers above argue for his second look.
- Owed: lodgenaggregate learning the ring (refused by name today); the panel's Card frames row and
  cardsOnDisk ignore ring sets; the FO4CS reader.

# 3. Gates (numbers; red runs)

## Step 1 (exe 97716e49, the worktree's first build = the rung, before the comment rebuild)
| gate | pre-registered | measured |
|---|---|---|
| impostor_trunk.sh | 38/3, the 3 named | 38 checks, 3 failures: 2 KNOWN RED flat-snap tear (el 0 4.68 %, el 20 10.14 %) + smooth end el 20 trunk bar -- the named three |
| impostor_draw.sh | 33/1, row 5 known | 33 steps, 1 failure: row 5 KNOWN RED (4x4 flat snap IoU 0.3920 < 0.50) |
| impostor_aa.sh | 7/0 | 7 checks, 0 failures |
| impostor_shrubs.sh | PASS | RESULT PASS |
| lodgen_octahedral.sh | PASS | RESULT PASS |
| lodgen_card_arrays.sh | PASS | RESULT PASS |
| lodgen_impostor_cards.sh | PASS | RESULT PASS |
Reds inside those gates fired in the same runs (trunk: the DXT5 sheet FAILS the sheet bar; the old drawer
FAILS the trunk bar; draw row 18: strongest-frame control breaks popping 2.06/2.09).
lodgen_defaults (d): the red is the worktree's own card dir (0 images -> now a NAMED refusal); green = C 7210.
Outputs: gates/*.new.out, gates/lodgen_defaults_d_maincards.out.

## Step 2 (exe ff86b488, N8 / TILE 512 / REF 1326.5, impostor_shrubs.sh MATCH=<model>)
| model | this exe: covered texels | pre-SHRUB1 exe (red) |
|---|---|---|
| sapling01 | 43,531 (halfW 93.83), S3 ok | 0 (halfW 1.08 = nothing measured), S3 FAIL |
| treeelmundergrowth01 | 40,952 (halfW 159.13), S3 ok | 0, S3 FAIL |
| shrubgrouplarge05 | 212,521 (halfW 405.21), S3 ok | 0, S3 FAIL |
The full 54-model impostor_shrubs.sh run of step 1 (RESULT PASS, 0 empty) covers the same three.
Outputs: gates/step2_<model>.<new|red>.out.

## Step 3 (exe ff86b488; orbit IoU vs the mesh, 16 bake directions, WW_IMPOSTOR_BLEND=0)
| subject | FIX5 (exe c529e3c1) | this exe |
|---|---|---|
| maple (TreeMapleForest2) at the full 128 long side, no ladder | 0.5022 (64x128 frames) | 0.4651 |
| maple, FIX5's ladder-rung fixture (32x64) | 0.4673 | 0.4072 |
| blast (TreeMapleblasted05) re-baked full size | 0.8701 | 0.9074 |
| blast, FIX5's fixture | 0.8701 | 0.8847 |
Same exe, full size vs ladder: maple +0.058, blast +0.023 -- FIX5's finding holds (the ladder rung is a real
part of the maple's gap, not all of it). The FIX5-era fixtures read differently on this exe (maple -0.060,
blast +0.015): the drawer moved since c529e3c1 (AA4, DEPTH2 crisp end), so only same-exe pairs compare.
Output: gates/cardres_test.out (pictures under cardres/, not committed).

## Step 4 (exe 56724fa6)
- impostor_defaults.sh: 7 checks / 0 failures (gates/impostor_defaults.new.out). D7 = the default picture is
  byte-identical to the rung's default at all 16 views. RED on the rung exe 97716e49: 6 / 1, D4 (the rung
  names no strongest-frame cut; gates/impostor_defaults.rung.out). D6 is the floor: the smooth end differs
  from the default at 8 of 16 views, so D5/D7 can see a change.
- kept green on 56724fa6: impostor_trunk 38/3 (the three named: flat-snap tear el 0 and el 20 = bungo's
  13:1x ruling, smooth end el 20), impostor_draw 33/1 (row 5, the known red), impostor_aa 7/0.
  Outputs gates/*.s4.out.

## Step 5 (exe eaa4b0b6)
- tests/spells/impostor_ring.sh 13 / 0 (gates/impostor_ring.s5.out). R1 16 view echoes, error 0.0000.
  R2 views 16, grid [16,1], albedo 1280 x 256. R3 this exe loads it ("grid: RING of 16 views"); the rung
  refuses it by name ("oct is 0, outside"); floor: the rung loads this exe's N8 set. R4 in-between azimuths
  el 0: IoU 0.4886 >= 0.4513 (0.90 x the mesh's own ceiling 0.5015); RED shuffled frames 0.0776.
  R4a at the bake directions 0.9131. R6 16 / 16 nearest-frame picks. R5 pixels ring16 62,795 vs N8 47,453
  bytes compressed (1.32); ring8 at 1.66 bites the size bar.
- RUN 1 FAILED 13 / 2 (gates/impostor_ring.run1.out): R4's bar was an absolute 0.60 pre-registered without
  measuring the subject; the mesh rotated by half a step against itself only reaches 0.5015. Re-pinned to
  0.90 x that measured ceiling (fix12), and the red filter now drops only 'EXCLUDED: mesh' lines (it was
  also dropping colour EXCLUDED lines). MISTAKES text in DELIVERABLE_TEXT.md.
- Mean IoU at the in-between azimuths, ring16 vs N8 (M, not gated):
  el 0: 0.4886 vs 0.8249 | el 5: 0.4863 vs 0.8044 | el 15: 0.4266 vs 0.6511 | el 30: 0.2849 vs 0.7947 |
  el 60: 0.2086 vs 0.3712. Full turn at el 0: ring16 0.6904 (ceiling 0.7012), N8 0.7842, ring8 0.5255.
- Kept green on eaa4b0b6 (gates/*.s5.out, *.s5b.out): impostor_trunk 38 / 3, the three named (flat-snap tear
  el 0 and el 20 = bungo's 13:1x ruling; smooth end el 20) -- re-run ALONE (s5b) because the chained run
  was refused while another lane's --port harness was up; impostor_draw 33 / 1 (row 5, the known red);
  impostor_aa 7 / 0; impostor_defaults 7 / 0 with RUNG= the rung (D7 byte-identical at all 16 views; the
  chained run without RUNG reads 6 / 0 because D7 only runs with a rung); impostor_shrubs PASS (54 of 54
  baked, 0 empty); lodgen_octahedral 116 ok / 0; lodgen_card_arrays 37 ok / 0; lodgen_impostor_cards
  12 ok / 0.
- lodgen_defaults.sh phase (d) with CARDS=<main tree>/scratchpad/showcase1_20260912/cards: 6 / 0
  (C lines 7210 = 7210, I 35, M 12, A 23, arrays 11 = 11; gates/lodgen_defaults.d.s5.out). The director's
  "no impostor-card lines with identity off" red is NOT a generator defect: a fresh worktree's card
  directory holds only the .txt sidecars git carries, so no card places and every C count reads 0. Fixed
  cheaply in step 1 (19c0347): the gate now fails BY NAME ("the card directory holds no card image ...
  set CARDS=") instead of reporting 0 C lines. Gate dir restored with git checkout afterwards.

# 4. Exe sha1 + commits
- rung / first build: release/NifSkope.exe 97716e4988e493f7b0eab6952780ac18aca0a609 (21:43:55),
  kept as release/NifSkope.before_cardfix1.exe.
- step 1 build (comments only, 10 objects recompiled): ff86b488aa769d1e453b719ee8e07e5f9ce8164f (22:24:34), same size 24,716,800.
- step 4 build: 56724fa6332362467884619efe19667e158024fc (22:31:49), 24,717,312 B.
- step 5 build: eaa4b0b60e9ff6796df846f54ebf292a94cc0aca (23:00:20), 24,729,600 B; kept as
  release/NifSkope.s5_eaa4b0b6.exe (the step-6 gates' previous exe).
- commits: step 1 19c0347; step 2 91ddd41 (evidence only); step 3 d8302c9; step 4 7896ad1;
  step 5 1303334 (code) + this DONE commit.

# 5. What the final bake needs
(filled at the end)

# 6. Skill review
(filled at the end)
