21:44:55 step1 gates chain start (exe 97716e49)
22:22:57 step1 gates on 97716e49: trunk 38/3 named, draw 33/1 row5, aa 7/0, shrubs/octahedral/card_arrays/impostor_cards PASS; defaults(d) = fixture (C=7210 with main cards)
22:24:55 step1 build ff86b488 BUILD-RC=0; committing
22:25:01 step1 COMMITTED 19c0347; step2 start
22:27:14 step2 new 3/3 covered (43531/40952/212521), red pre-SHRUB1 0/0/0
22:27:20 step3 cardres_test start
22:29:36 step3 cardres: maple_full 0.4651 blast_full 0.9074 | same-exe ladder fixtures maple_n4 0.4072 blast_n4 0.8847; step4 patch applied, building
22:58:11 step4 gates on 56724fa6: trunk 38/3 named, draw 33/1 row5, aa 7/0; committing step 4 (step-5 hunks held out of the index)
23:06:32 step5 build eaa4b0b6 BUILD-RC=0; impostor_ring.sh start
23:11:24 ring run1 13/2 (R4 bar vs ceiling 0.5015; red count filter) -> re-pinned; full chain s5 start
23:57:54 step 5 gates read (trunk 38/3 alone, defaults 7/0 RUNG, shrubs/oct/arrays/cards PASS, defaults(d) 6/0); DONE step 5 written; step 6 patches applied, build next
00:26:18 step 6 wind gate run3 26/1 (G4 red vs pre-registered bar, R/G reference measured); array name fix24; kept-green chain s6 start on 309f3aa9
00:30:13 correction: wind run3 read 27 checks / 1 failure (G4), not 26/1; pbr_shade_ab s6 REFUSED (no OLD arm) -> OLD arm release/before_pbrr0 built from s5 exe + HEAD shaders, re-run after the chain
01:15:59 s6 chain: ring 13/0, trunk 38/3 named, draw 1 known red (row 5), aa 7/0, shrubs PASS, card_arrays PASS, impostor_cards PASS, defaults 7/0 RUNG; octahedral 115/1 on its stale lodm-1 premise -> fix27; rerun + pbr_shade_ab + GIF
01:24:11 octahedral s6b PASS (fix27); GIF run 2 OK (run 1 blank: no textures\ tree beside the .lodm, fixed in wind_gif.sh); pbr_shade_ab s6b 10 cases 11 fail = OLD arm missing DLLs (my copy, rc 127), fixed, s6c running
01:28:37 pbr_shade_ab s6c 10/0 PASS; director relays bungo 2026-09-25 RULING 'Yes, 8x8 is the default choice for a bake' -> fix29 (driver RING=0 default, docs, ring gate R7); ring gate s6n run
01:34:06 ring s6n 17/0 (R7 green, red = step-5 driver ring 16); step 6 committed 24e7835; DONE 6b + sections 5/6 written; committing 6b
01:36:05 DIRECTOR DECISION G4 (a): bar = codec floor of the sheet's other channels x1.25 (fix31), red = next frame + 4-bit sway; preview on existing elm set: 3.573/13 vs bar 4.083/15, 4-bit 6.702 fails; wind gate run4 start
2026-09-25 01:38 step 6c: wind gate run4 28/0 PASS (G4 3.573/13 <= 4.082/15; next frame 51.099, 4-bit 6.702 fail the bar); DONE 6c + DELIVERABLE written (fix32); committing
2026-09-25 01:49 step 7 job 1: maple .pbrm fixture made (none on disk for a tree); exe 309f3aa9 bakes it family legacy, _gsaos vanilla (gloss 80.8/80.4), no _s -- the .pbrm is never opened. Design written to DONE (fix33): _s = sqrt(F0') RGB + weight A, BC7 aux, no version bump; mixed = legacy; CPU texture-space law, no shader edit
2026-09-25 02:4x step 7 built (exe 45719ad4): impostor_pbrm run3 8/10 -- RED colour bar=0 (floor 0 after fix37; 0.32/0.35 measured) and RED non-aa arm 14/15 (unregistered row, partial-alpha edge law); reds bite; kept-green gates green; native_lighting 2 fails reproduce on s5 exe; DONE/DELIVERABLE written; committing
2026-09-25 02:52 DIRECTOR DECISIONS step 7 (1) colour bar max(1.25xfloor,0.5) (2) non-aa judged coverage==1 -> fix39; run4 13/14: aa 15/15, reds bite both arms (add colour 4.14/4.00), non-aa 4 FAIL MapleAtlas02_Tree = material-boundary texels (98% beside class 0; 0.997 away); reported, not re-pinned; committing and stopping
