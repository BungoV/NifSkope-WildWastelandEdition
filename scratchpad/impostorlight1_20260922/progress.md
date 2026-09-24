# IMPOSTORLIGHT1 progress
- 23:02 start. Game down (no Fallout4.exe). bungo's NifSkope pid 8728 running release/NifSkope_inuse_8728.exe. Rung taken: release/NifSkope.before_impostorlight1.exe = bf6aa749 (23,811,072 B 22:12:21).
- Read: CONSTITUTION, HANDOFF IMPOSTORFIN1 line (370), FIN1 FOR_BUNGO/DELIVERABLE/DONE, LOOK1 s2+s5. Skills loaded: build-verify, render-shot, reference-card-diagnose, channel-view-refuter, test-harness-add, lodgen.
- 23:1x READ, CANDIDATE (not yet measured): res/shaders/impostor_oct.frag:410 `abs( dot( normal, normalize( L ) ) )` -- `normal` is MODEL space (frameNormalModel :164-169, blended :291) while `L = lightSourcePosition[i]` is VIEW space (headlight (0,0,1); fo4_default.vert:59 LightDir, mesh normals `n * normalMatrix` = view, fo4_default.vert:46). The card's modelViewMatrix is scene->view (impostordraw.cpp:490), not identity, so the dot mixes spaces: under a headlight the card lights by model-space Z (up), i.e. crown tops bright, trunk sides (the mesh's brightest, facing the camera) dark. Discriminator: normals view (job 1a) + a ladder stage that isolates the diffuse term.
- Plan: diagnosis on the CURRENT exe with shader-only instrument variants in a separate run folder (diag_run/, exe bf6aa749 + DLLs + shaders, variant shaders written by diag_shaders.py) -- the exe loads applicationDirPath()/shaders (glcontext.cpp:937), so no build is needed to diagnose. Fixture = IMPOSTORFIN1 res512 sets (five subjects, cardRes 512); views = each set's own bake directions (impostor_bake_views.py).

## 23:1x NORMALS measured (bf6aa749, bake directions, blend on; normals.py, |mesh comp|>0.15 votes)
card normal taken to VIEW space with mat3(modelViewMatrix) (diag/s1_mv) vs the mesh's geometric view normal:
  blast_n4 sign x 96.7 y 94.2 z 95.9 %, angle mean 11.1 deg (med 4.8)
  blast_n8 96.5 / 95.2 / 96.0, 11.6 ; maple_n4 86.1 / 80.3 / 96.5, 21.9 ; dead_n4 93.4 / 89.6 / 96.3, 15.1 ; rock_n4 99.1 / 98.5 / 98.6, 5.4
card normal AS THE LIGHTING USES IT (model space, no transform; diag/s1_model) vs the same:
  blast_n4 50.6 / 40.6 / 79.3, 86.0 ; blast_n8 50.8/46.9/81.0, 85.1 ; maple 47.8/45.7/66.9, 87.8 ; dead 48.3/50.4/67.4, 87.6 ; rock 49.1/45.3/83.8, 68.5
=> NO flipped axis in the sheet/decode (candidate A's second half refuted): the stored normals are right.
   The lit normal is in the WRONG SPACE: x/y agreement ~50% = coin toss. impostor_oct.frag:410 dots a
   MODEL-space normal with a VIEW-space light (lightSourcePosition, glshape uses normalMatrix=view rotation).
BAR (set before fixing): after the fix, the normal the lighting uses (in view space) must reach
  sign agreement >= 90% on x,y,z for blast/rock, >= 80% on all five, mean angle <= 25 deg on all five
  (the mv numbers are the ceiling a correct transform reaches; the geometric-vs-mapped gap is not in scope).
Side evidence for impostor_draw row 5: stage-3 (ambient only, dark) grabs drop the harness's orbit IoU
  0.9741 -> 0.4941 on rock, same geometry, same exe: the harness silhouette mask reads dark pixels as background.

## 23:2x BRIGHTNESS LADDER measured (bf6aa749; ladder.py, pixels = intersection of the stage-1 normal silhouettes, which never read as background)
card/mesh mean luma:            albedo  +ambient  +diffuse  +AO/spec(pre-tonemap)  tonemapped
  blast_n4                       0.979   0.973     0.491     0.465                 0.475
  blast_n8                       0.980   0.973     0.499     0.471                 0.481
  maple_n4                       1.018   1.010     0.590     0.534                 0.541
  dead_n4                        1.021   1.013     0.538     0.506                 0.513
  rock_n4                        1.002   1.001     0.907     0.876                 0.877
=> THE DIFFUSE RUNG halves it: impostor_oct.frag:410 (`ndl = abs( dot( normal, normalize( L ) ) )`, model-space normal vs view-space light).
   Albedo sheet, colour space, ambient scale, AO and tonemap are each within 3% (candidate B refuted except the ~3% AO/spec rung).
   Spearman rho card vs mesh luma: albedo 0.43..0.65, lit 0.11..0.28 (maple -0.02): the lit transfer is near flat / inverted on maple.
BAR (before fixing): tonemapped card/mesh 0.9..1.1 on all five; lit rho >= the albedo rung's rho minus 0.10 on every subject (the lighting must not decorrelate the card beyond what the colour already does); every decile of the transfer rising.
VANILLA TWO-SIDED FLAG (material_flags.py, BGSM bTwoSided): MapleAtlas01/02/02_Tree 1, BlastedForestDestroyedTreeAtlas 1, rockslab02 0, rockslabtrim 0. No backlight/subsurface/rim on any. => trees two-sided, rock one-sided.
   The mesh lights a two-sided face one-sided about the VIEWER-facing normal (frag:404 flip, :422 clamp) and the bake stores that flipped normal (:305), so max(N.L,0) is the mesh's own rule for all five.
23:22 FIX GENERATOR final: shaders_final = oren, roughness 1 (smoothness const 0), nView declared above debug block, channel 13 = lit normal. Verifying via diag_run s6 + s1 lit.
23:3x FINAL SHADER (shaders_final, = oren1 byte-for-byte in behaviour) verified on the rung exe, bake directions: tonemapped card/mesh 0.953 / 0.954 / 0.966 / 0.984 / 0.967 (blast_n4 / blast_n8 / maple / dead / rock); rho 0.52 / 0.50 / 0.50 / 0.57 / 0.69 (albedo rung 0.47 / 0.44 / 0.43 / 0.55 / 0.65: lit >= albedo - 0.10 MET). Lit normal (channel-13 instrument) sign x/y/z = s1_mv exactly (96.7/94.2/95.9 blast_n4 ... 99.1/98.5/98.6 rock), angle 5.4..21.9 deg: normals BAR MET.
   Dark-read-as-background (harness bg rule) old card 6.06 / 6.01 / 6.62 / 44.05 / 11.35 % -> final 0.30 / 0.30 / 0.03 / 1.12 / 0.23 % (mesh 1.4..2.5 %).
   TRANSFER BAR "every decile rising": MET on maple, dead, rock; NOT MET at the TOP decile of blast_n4 (129 -> 129) and blast_n8 (129 -> 128). Mechanism measured (top_decile.py): at that decile the MESH albedo is 156 and the CARD albedo 110, the same as its 9th decile -- the albedo sheet, not the light; card N.L 0.81 vs mesh 0.83 there. Spec (s5-s4) < 1 luma everywhere (spec_decile.py). Every lighting candidate (lambert 141/141, oren gloss 135/134) is flat there too. Bar NOT lowered; reported as missed with cause.
23:38 16c BAR RESTORED to the pre-registered relative form (lit rho >= colour-sheet rho - 0.10, rho>0, rise>0); the post-hoc absolute 0.30/10 withdrawn: the gate fixture 000531b3's colour sheet has rho 0.071 (deciles 102..105) and no lighting can beat that. Checker takes ALBDIR (mesh ch12 / card ch1); spell step 16 runs light_run alb. Existing fixture grabs: 0 failures. Running the spell on new exe then the rung (diag_run = bf6aa749 exe + its shaders, stage 0 pristine, frag sha1 198d305a).
23:5x impostor_draw.sh on the NEW exe (23:32:40): 30 steps, 0 failures (gate_impostor_draw_after2.txt). Row 5 0.5082; 16a 95.8/94.4/97.7 % 14.3 deg; 16b 0.945; 16c rho 0.194 rise 6.4 vs colour-sheet rho 0.071.
      On the RUNG (IMPOSTOR_EXE=diag_run = bf6aa749 exe + its shaders): 30 steps, 6 failures (gate_impostor_draw_rung.txt): row 5 0.4937; 16d/16e FAIL (no mesh channel switch); 16a 41.8/63.1/83.0 % 71.9 deg; 16b 0.460; 16c rho -0.091 rise -2.3.
      Clean normals proof on the rung (debug backport, diag s6 + s1_model + s2): blast_n4 3 FAIL (sign 50.6/40.6/79.3, 86.0 deg; ratio 0.475; rho 0.145 vs sheet 0.467-0.10); rock_n4 3 FAIL (49.1/45.3/83.8, 68.5 deg; 0.877; 0.141 vs 0.553).
      Fix, res512, all five, checker 0 failures each (diag/final/s6 + s1_lit + s2).
23:42 ROW 5 TAKEN APART (row5.sh/row5.py, gate fixture, 16 kViews, rung exe, shader-only A/B): old IoU(rule) 0.4937, IoU with the TRUE card silhouette (stage-1 normals) 0.5082, 3.14 % of covered card px read as bg; fix 0.5082 / 0.5082 / 0.00 %. The whole shortfall is dark pixels.
00:0x (09-23) GATES: lodgen_octahedral.sh RESULT PASS (116 ok). native_lighting.sh 21 checks 2 failures on the NEW exe AND on the RUNG (release/NifSkope.before_impostorlight1.exe): gate (a) legacy_btr_top/obl not byte-identical to baseline -- new-exe frames sha1 3de8b393/716e806e == rung frames, baselines c64dc520/8b4022f6: pre-existing baseline drift, not this lane (the .BTR is lit by sk_msn.prog).
   CUBE KNOWN-ANSWER (cube_control.sh -> cube_control.txt): fix normals x 99.8 y 99.8 z 100.0 %, 3.1 deg; brightness 0.961; bf6aa749 50.0/58.2/91.2 %, 77.8 deg, 0.516. Per face (cube_faces.py): fix card/mesh 1.18 (N.L 0.4) .. 0.87 (N.L 1.0) -- transfer COMPRESSED: the cube's material is glossy and the card lights at roughness 1 by bungo's 2026-09-19 no-gloss ruling. Card reading its own sheet gloss (the mesh's rule, shaders_oren) or roughness 0: 0.89 .. 1.02. On the five subjects the sheet gloss sits with roughness 1 (slopes 0.40/0.42/0.52/0.65 vs 0.36/0.37/0.50/0.62) => vanilla foliage + rock are rough; roughness 1 KEPT; gloss = ruling owed.
   INSTRUMENT DRIFT: the blasted maple's MESH render changed between 23:2x and 23:4x (one view: 24,905 of 37,560 albedo px differ > 12, mean unchanged; source files unchanged). Colour-sheet rho on blast fell 0.467 -> 0.065 (rock 0.653 -> 0.650), which is also why the gate fixture read 0.071. Cause NOT found (persisted QSettings from bungo's window or a harness suspected). All candidate comparisons re-done inside one session: roughness 1 now 0.943/0.948/0.964/0.984/0.967, rho 0.21/0.22/0.50/0.57/0.69, all rows green.
00:1x pictures + FOR_BUNGO.md + DELIVERABLE_TEXT.md + DONE (PARTIAL) written; side-light r card-vs-mesh before -0.58/-0.41/-0.15 after 0.99/0.99/1.00 (blast/dead/rock). Skill ww-reference-card-diagnose s13 appended.
00:1x bungo's NifSkope now pid 25584 started 23:50:13 on release/NifSkope.exe (the new exe); 8728 gone. First suspect for the mesh-render drift.
00:1x bungo's NifSkope now pid 25584 started 23:50:13 on release/NifSkope.exe (the new exe); 8728 gone. First suspect for the mesh-render drift.
00:1x bungo's NifSkope now pid 25584 started 23:50:13 on release/NifSkope.exe (the new exe); 8728 gone. First suspect for the mesh-render drift.
