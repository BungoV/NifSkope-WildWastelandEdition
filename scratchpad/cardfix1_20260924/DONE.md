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

# 4. Exe sha1 + commits
- rung / first build: release/NifSkope.exe 97716e4988e493f7b0eab6952780ac18aca0a609 (21:43:55),
  kept as release/NifSkope.before_cardfix1.exe.
- step 1 build (comments only, 10 objects recompiled): ff86b488aa769d1e453b719ee8e07e5f9ce8164f (22:24:34), same size 24,716,800.

# 5. What the final bake needs
(filled at the end)

# 6. Skill review
(filled at the end)
