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

# 4. Exe sha1 + commits
- rung / first build: release/NifSkope.exe 97716e4988e493f7b0eab6952780ac18aca0a609 (21:43:55),
  kept as release/NifSkope.before_cardfix1.exe.
- step 1 build (comments only, 10 objects recompiled): ff86b488aa769d1e453b719ee8e07e5f9ce8164f (22:24:34), same size 24,716,800.
- step 4 build: 56724fa6332362467884619efe19667e158024fc (22:31:49), 24,717,312 B.
- commits: step 1 19c0347; step 2 91ddd41 (evidence only); step 3 d8302c9; step 4 see git log.

# 5. What the final bake needs
(filled at the end)

# 6. Skill review
(filled at the end)
