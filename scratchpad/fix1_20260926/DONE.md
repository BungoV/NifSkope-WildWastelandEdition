# FIX1 -- the small fixes (colour remap, DLC vs vanilla, floating objects, harness settings, owed calls)

Lane FIX1, branch fix1-20260926 from main 0834d4dc, worktree E:\Projects\NifskopeWWE-fix1.

## 0. Progress log
- 02:13 started (clock read). Worktree up; main's objects proven current (make -n = 0 g++); qmake OK (123 worktree
  paths, 0 main); REVISION objects dropped; first build 02:15:56 rc 0; rung exe release/NifSkope.before_fix1.exe
  sha1 612b319b.
- 02:2x fix 1 finding (before any build): the two Nuka-World CNAM hits change NOTHING in the game. Engine
  (1.10.155, Todd's treat): the MSWP lambda of BGSModelMaterialSwap::Apply (RVA 0x532a0) takes a row's colour-remap index
  (BGSMaterialSwap::GetColorRemappingIndex, 0x2d6ac0) only when it is below FLT_MAX (constant at RVA 0x2c48d68 =
  3.4028235e38), and stores it into the lighting material's fLookupScale (+0xB8, BSLightingShaderMaterialBase)
  ONLY when the property's flags word (+0x30) has bit 4 = SLSF1 Greyscale_To_PaletteColor (0x536cd
  `test byte [r13+30h],10h`); effect materials get it in fBaseColorScale (+0x84). So a CNAM on a material without
  the palette flag is ignored. Nuka-World's six LOD CNAM rows (MSWP 0102400f MacYellow->MacRed x2, 0101f5b4
  GalacticZoneSwap21_Yellow x2, 0101f5b3 GalacticZoneSwap17_Black x2) all carry CNAM 0.0 and all name
  replacement BGSMs with bGrayscaleToPaletteColor = 0 (their colour is in their own _Red/_Yellow21 diffuse, which
  SWAP1 already applies). Census of every INSTALLED library's material strings through the MO2 stack
  (fix1cnam/lod_g2p_census.py): palette-flagged LOD materials = 0 in NukaWorld (155 BGSM), DLC03FarHarbor (84),
  SanctuaryHillsWorld (24), Commonwealth (185, 1 unresolved). So the director's gate "the 2 placements' colour
  moves to the remapped row" is refuted by the engine: the correct colour does NOT move.
- Fix 1 as built (applies the game's rule, no format change): the loader reads the palette flag (NIF SLSF1 bit 4,
  the BGSM's bGrayscaleToPaletteColor wins); the swap census splits "CNAM rows hit" into game-applied (a palette
  row the library cannot carry -- would be a defect to report) / ignored by the game (no palette flag) / index
  unset (>= FLT_MAX). Build 1 02:22:49 rc 0.
- **Fix 1 gate, pre-registered 02:2x:** NukaWorld object-only bake (SWAP1 bake_ws.sh flags, BAKE2 cards) on the
  rung exe and on build 1. GREEN iff (a) build 1's census line reads `CNAM rows hit 2 (game-applied 0 ...; ignored
  by the game, no Greyscale_To_PaletteColor 2; index unset 0)`, and (b) every file of the two bakes is
  byte-identical (the library colour must NOT move -- the game does not move it), and (c) the rung's line lacks
  the split (RED on the unfixed exe: it cannot say whether the 2 rows reach the screen).
- **02:43 fix 1 GREEN.** NW_rung (612b319b) vs NW_fix (09287360): 646 of 647 files byte-identical; the one that
  differs is the .lodb (it records paths, time, exe size and census text), not a library file. Build 1's census
  splits the 2 CNAM rows as "ignored by the game, no Greyscale_To_PaletteColor 2"; the rung's line has no split.
  Committed 57c5f0c7 (lodgen.cpp, nativeemit.cpp/.h, contract §3.8). No re-bake or install is needed: the
  library does not change.
- **02:40 fix 2 (helper, offline): no systematic gap.** fix2/RESULT.md. Far Harbor: Vim! and Cliff's Edge match
  vanilla within 3% brightness and 0.01 saturation; Acadia reads BRIGHTER than vanilla, not greyer. Nuka-World:
  Space Needle, Nuka-Town and Kiddie Kingdom are all within 6% brightness and 0.035 saturation with the swap
  variants applied. Nothing to fix, nothing to install.
- **02:49 fix 8 GREEN** (director addendum). Before (fixture from fix3_b2): `--native-verify` rc 1 "fullTriangles
  0 but its distinct meshes hold 12". After (fix8_b3 e0383dbf): rc 0; the independent decoder
  (lodgen_native_decode.py) recounts [12,20,12] from the cluster table = the hand answer = the stored counts; 71
  checks / 0 failures; its mutate arm 48 / 0. Committed fda01cbc. The committed fixture expectation moved only by
  ADDING the key `lodo.bases.fullTriangles 12,20,12`; nothing existing changed. The FO4CS reader was not run
  (other repo; FO4CS readers come last).
- **Fix 3 RED measured on the installed files** (fix3/installed_before.txt, float_gate.py): pre-war
  SanctuaryHillsWorld has 101 placements in 15 no-LAND cells, every one floating more than 1500 units over the
  flat default ground; the worst height step at a LAND / no-LAND edge beside them is 8664 units. The Glowing Sea's
  8 grey cells each have LAND (the grey is the plugin's own painted texture): justified, no change.
- Fix 3 as built: `--land-fill-vanilla` (panel: tied to "Fill unpainted ground with vanilla's colour") fills a
  landless cell's height from vanilla's own terrain LOD (.BTR, read as input only) in the .lodl and in the VT
  height/normal sheets; census line "landless-cell fill: N cells filled". Build 2 7610ef6b.
- **Fix 3 gate, pre-registered before the fixed bake:** PW_rung = installed files. PW_off (build, switch off) must
  equal PW_rung byte for byte apart from the .lodb; PW_fix (switch on) must bring landless floating to 0 with the
  LAND-cell counts unchanged. 02:55 the game came up; the coordinator's order: nothing under Fallout 4 Mods, stage
  in the scratchpad, report INSTALL PENDING. Bakes write only under fix3/.
- **Fix 5a** (whole-map FORCE_CARD: NO) and **5c** (.lodj = our own per-chunk cache for `--incremental`, about
  294 bytes a placement; no game or FO4CS reader opens it; still written by default; `--no-native-cache` drops it)
  documented in 992b6fb5 (contract §4.13, ledger doc §4.1).
- **Fix 5b** as built (build 4 ae789cff): a "Ground cover" row under the FO4CS target's terrain section (xB row,
  tooltip "Command line: --cover", no description), read tick-and-visible; coverOptions() asks for cover when
  that row is ticked under FO4CS with the pyramid on. Gate, pre-registered in WW_LODGEN_TEST: "ground cover can be
  asked for under the FO4CS target" (the row is visible and flips the summary's cover request), RED on the rung
  (fix8_b3, no row), GREEN on build 4.
- **Fix 5d NOT DONE -- not a small fix.** `--native` with `--incremental` already works on ONE ring with the .lodj
  cache. The ruled FO4CS pipeline refuses for three reasons: `--dim all` is refused outright (nifcli ~3990); the
  region products (arrays, impostor cards) refuse (Verdict RegionProducts, nifcli ~4101); and a replayed chunk
  would also need its card-link C lines (contract §4.13). Proposed: its own lane (replay the card links, then
  per-ring records under --dim all, gate = one touched cell's incremental bake byte-identical to a full bake).
- **Fix 4** (helper): 11 spells now force their own scope. Red-before 03:02: unmodified impostor_draw under the
  hostile scope fails 16c and 16d (lighting rows photograph the mesh through the profile's shading). After-runs
  pending in the helper.
- **03:32 fix 3 GREEN** (fix3/float_gate.py; plugins.txt sha1 f91c6794 for all three arms). PW_rung2 (rung exe
  612b319b, re-baked 03:30 because CORE.esp left the profile at 02:53 and the first rung arm carried the old order)
  vs PW_off (build 2, switch off): 163 files, all byte-identical but the .lodb. PW_fix (switch on): landless
  placements floating 101 in 15 cells -> 0; worst edge step 8664 -> 200 units; LAND-cell placements 1249, none
  floating, unchanged; census "landless-cell fill: 1610 cells filled" (.lodl) and "cells asked 3220, filled 3220;
  dim-4 chunks read 115, missing 0" (chunks). Files that move: .lodl, VT.2, VT.4, .lodb (and .lodo/.lodi only in
  their load-order words, because of the profile change). --lodt-check VT.2/VT.4 rc 0; --native-verify rc 0.
  Commit f4eabed2 (code + LODGEN_TERRAIN_VT.md §2.6b + switch row).
- **INSTALL PENDING (fix 3):** stage/install_fix3.sh for the overseer: refuses with the game up, refuses if
  plugins.txt moved since the bake or the installed files are not the measured ones, backs the 6 files up to
  replaced/SanctuaryHillsWorld/, copies from fix3/PW_fix, reads back sha1 (stage/before.sha1, stage/expected.sha1):
  VT.2 2eec63e6->93e4e5bc, VT.4 a1dd9bf5->682d83bb, .lodb 579e8ed2->d2515d2d, .lodi b4e8d95e->39f18763,
  .lodl 8535f288->5c7b6eda, .lodo 62cb4bc3->f454c16c.
- Fix 3 pictures: fix3/pics.sh (08 camera, cells -18..-11 x 17..24, before = PW_off, after = PW_fix) -- held
  while the game is up (shot.sh refuses).
- 03:19 fix 5b first gate run was NOT a valid gate: the rung exe carries no self-test for the row (128 checks,
  the check never ran) and on build 4 the check read the row through a FOLDED section body (isVisibleTo), so it
  failed on correct code. Test fixed to the panel's own rule (row and section not hidden). Build 5 03:36:59
  (green, 4a646226) and a RED exe with the same test and the panel at HEAD (fix5_red, 1c4ed56f).
- **03:42 fix 5b GREEN** (fix5/cover_gate2.sh, scope seeded with the Game Manager state, wiped after: "scope
  wiped"): RED exe fix5_red 1c4ed56f = 129 checks, 1 failure ("ground cover can be asked for under the FO4CS
  target"); GREEN exe fix5_b5 4a646226 = 129 checks, 0 failures ("shown 1, request off 0, on 1"). The archive
  check that failed in the 03:19 runs passes here: those runs had empty Game Folders (unseeded scope). Commit 7dc3107a.
- **03:57 fix 3 pictures** (untracked): fix3/pic_PW_off.png (before: the landless cells are a flat plain, the LAND
  block stands as a cliff above it, rocks and trees hang at the hill height) and fix3/pic_PW_fix.png (after: the
  hills are there and the same objects sit on them). 08 camera over cells -18..-11 x 17..24, same exe, same scope.
  Fix 1 and fix 2 have no picture: no library byte moved.
- **04:07 fix 4 GREEN (helper, fix4/RESULT.md).** Red before: unmodified impostor_draw under the hostile scope
  fix1hostile = 33 steps, 3 failures (16c, 16d + its known step 5). After: 16c/16d pass (only step 5 left);
  cell_open, lodgen_octahedral, lodl_open, lodl_channels, lodi_v7 pass. Still red for reasons that are not settings
  (each shown red the same way before or under a copy of his profile): skeleton_overlay (j) + old rung size,
  cell_pick exe-staleness/git-HEAD rows, lod_channel_preview AO = wetness, refraction 2 reds once the normal map
  loads, render_shot one outside-sampler floor per run (timing). His settings key byte-identical 02:26 vs 04:06.
  Commit 5093f561. Follow-up NOT done: 12 other spells wipe a scope without the Game Manager seed (list in
  fix4/RESULT.md); root cause GameManager::init_settings() calls find_paths() before load().
- Branch: fda01cbc, 57c5f0c7, 992b6fb5, f4eabed2, 7dc3107a, 5093f561 (+ this report). Not pushed, not merged.

## Skills
- Loaded: nifskope-ww-worktree-build, fo4-surface-colour-census, fo4-lod-material-swap-check, fo4-pdb-struct-layout,
  fo4-terrain-lod-input-cache, ww-whole-map-lod-bake, nifskope-ww-panel-style.
- Wished: none.
- Written: a section "A WIPED SCOPE MUST BE SEEDED WITH THE GAME MANAGER TOO" in skill ww-test-harness-add (NifSkope-specific, so not copied to AISkills).
