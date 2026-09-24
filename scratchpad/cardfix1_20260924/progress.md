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
