# NifSkope WW lanes: every ask and its state (living list)

Rule (bungo 10-21): keep this current at every lane start, finish, merge and ruling. Every ask gets a row the moment
bungo says it. Tiers: WORKING ON -> BUILT, NOT LANDED -> LANDED; APPROVED (his yes, not started); PLANNED (future, no yes
yet). A row leaves only as LANDED or SUPERSEDED, and then moves to LANES_DONE.md (keeps this file short). Read this before saying anything is "done" or "covered".
Main now: 9c624f47 (10-21 night, MERGEALL1 landing). Detail of old audits: overnight_1021\AUDIT.md, AUDIT_repo.md.

## 0. WAITING FOR YOU (only bungo can do these; top of the file, bungo 10-21)
- RTBAKE1 picture pair: CPU bake vs GPU bake, museum indoor (+ x16 difference); merge waits on your OK (coming)
- SSGI2 picture pairs (SSGI ships off): Vault 111 v_old_ultra -> v_ultra (+ v_off), Rexford night e_old -> e_on (+ e_off), in NifskopeWWE-ssgi2/scratchpad/ssgi2/g2/ + Goodneighbor sunlit glass wall off/on (bungo 10-21, coming from BATCH1)
- FOGTRICUBIC1 picture pair: smoother fog-grid read, Memory Den night (coming from VOLAIRSKY1)
- FXCHECK1 glass pair: notes/fxcheck1/pairs/glass_off.png vs glass_on.png (envmap mask red channel; ships off)
- PBRALBEDO2 pair: vanilla vs .pbrm-resolved Solomon's house (vanilla meshes name materials by a C:Projects path, so .pbrm never matched; coming from BATCH1)
- Game flights: AUDIT1 REF1 capture, LLFCHECK1, PRTP4 (save names), MOTION1/TERRBLEND1/WATER2 RenderDoc captures
- About 20 open questions (AUDIT_ledgers 49-54): one at a time

## 1. WORKING ON NOW
| Lane | What | State |
|---|---|---|
| BATCH1 | one combined build + one gate session for PERF2, SSGI2, SHADOWPAR1, VOLAIRSKY1, FXCHECK1 (bungo 10-21; batch1/README.md); also runs RTBAKE1 pics + PBRALBEDO2 gates on their own exes, + Goodneighbor sunlit glass SSGI pair (bungo's camera) | built 23:47 (sha1 44523481); 4 holds queued (concord, memden, vault, time) |
| MERGEALL1 | merge every built branch into main, clean merged worktrees (bungo 10-21 "merge everything") | 17 landed, 3 held; end check batch2 queued; FOUND: giblack red (march) fails to link fo4_cellcsm on main (NVIDIA too many instructions) |
| PERF2 | time the frame; depth reuse, per-tile lights, fewer driver calls, temporal reuse (+ fog history reprojection, Division-style, bungo 10-21), fog integrate O(S^2) -> O(S) | BATCH ready (0ded45ca; perf2/BATCH.md); SSR prev-frame, AO temporal, per-draw CPU wait for batch timing |
| VOLAIRSKY1 | + FOGTRICUBIC1 smoother fog-grid read, PNG pair for bungo (asked 10-21); VOLAIR1 sky gate fail (0.266 % vs 0.20 %) + placed lights in the fog (audit claim: 0 clusters; lane finds 1309-1419 filled on steady frames, 0 only the first ~8 frames after load) | BATCH ready (head 8e5dc845, notes/volairsky1/BATCH.md); rev1 F open: next-batch lines D2b + D3 |
| SSGI2 | SSGI gate (SSGI1 landed ungated) + blue noise, depth intervals, emissive, pyramid | BATCH ready (548fdbf1): 4 rows coded; cell_ssgi + nohiz PASS; double count FAILS near geometry (under-count 0.80); timing in BATCH1 |
| SHADOWPAR1 | shadows to FO4CS level (TERRSHADOW1 WEAKER list) + fog far-tail leak | BATCH ready 10-21: code at shadowpar1-20261021 694b4f7a, gates unrun (shadowpar1/BATCH.md) |
| FXCHECK1 | BGEM glass milky; no fire-barrel particles | BATCH ready (8ad3aadf): glass fix gated, ships off; barrel plume still open |
| VOLPART1R | rebase VOLPART1 onto main (steam/fog cards lit as volumes, ships off) | BATCH ready (6e8c92de; volpart1r/BATCH.md); gates unrun |
| TESTDRIVER1 | live test switches: load a scene once, flip WW_* switches over the port, no relaunch per row (bungo 10-22) | started 10-22 |
| BAKESPEED1 | far-LOD bake speed (chunks 2864 -> 1191 s mid region) | resume/determinism gates |
| PBRALBEDO2 | .pbrm albedo in the bake, fix of PBRALBEDO1 gate | BATCH ready (8abbbd83; pbralbedo2/BATCH.md): cause = vanilla NIFs name BGSMs by absolute C:\Projects\... path, resolver refused it; gates unrun |
| RTBAKE1 | GPU ray-traced bake (gate cell 52.5 -> 43.0 s) | BATCH ready (rt3 6cabd09; rtbake1/BATCH.md): museum CPU-vs-RT pair runs in BATCH1's timing hold |

Next batch (BATCH2): VOLPART1R, TESTDRIVER1, VOLAIRSKY1 D2b/D3, SSGI3 once built.
Queued tonight (his 10-21 go): FARBAKE2 whole-map rebake, every channel (after BAKESPEED1); VTGPU1 + PROFILE1 (after
BAKESPEED1); ROOMS-RT room enclosure by rays (after RTBAKE1).

## 2. BUILT, NOT LANDED (MERGEALL1 holds these)
| Lane | Branch | What it gives | Why held |
|---|---|---|---|
| PBRALBEDO1 | fixpack1 (superseded by PBRALBEDO2, BATCH ready) | bake reads .pbrm base color x (1 - metallic) | its own gate fails: 0/26 fixture materials resolved |
| VOLPART1 | volpart1-20261014 (superseded by VOLPART1R, BATCH ready) | steam/fog cards lit as volumes (ships off) | built on old VOLAIR1: 9 files conflict; needs a rebase lane |
| LEANCOMMENTS1 | origin/cloud-leancomments1 | lean comments pass 1 (-2758 lines) | 16 conflicts; RERUN approved 10-21: starts on main after MERGEALL1 + BATCH1 land |

## 3. APPROVED, NOT STARTED (his yes on record)
- SSGI3 (defect, queued at once): SSGI on under-counts near geometry vs tracer (0.80 vs off 0.89, bar +-10%); SSGI1 fill law (visibility removes more probe light than the march adds); diag reds in SSGI2 BATCH lines
- PERFRESEARCH1 DONE (notes in perfresearch1/): research notes, then lanes GPUDRIVEN1 (meshlets + GPU-driven drawing, the CPU-bound fix), VISBUF1 (visibility buffer, tiny far triangles), VRS1 (variable rate shading, pair if any look change), FXRES1 (per-effect resolution + OIT)
- PERF lanes (yes 10-22, order in perfresearch1/ORDER.md): FARCARD1 far cards batched; TONEMAP1+SYNC1 one lane; GPUDRIVEN1 A then B; SHADOWCACHE1 (after GPUDRIVEN1 A); LIGHTCLUSTER1; VRS1; FXRES1; STREAM1; AAFORMAT1 (MSAA off = pair); TEXSTREAM1/VTXPACK1/LOAD1; VISBUF1 last if it still pays. Running now: PERF3 + FARCARD1
- PERF3 (started 10-22): AO stays FULL res (bungo 10-21 "full res and optimize where it counts for real"); drop AO + SSR second scene submissions by reusing the main pass depth/normals (+ per-material SSR flag from main pass, SSR from previous frame); CPU per-draw setup cuts; far-LOD proposal
- LODPRTP L2-L7: distant probes (yes 10-15; L8/L9, P1/P2 still need a yes)
- PGI1 HORIZON chunk + GI port to FO4CS (PGI2-7)
- Hybrid rebake of Far Harbor, Nuka-World, pre-war Sanctuary (FARBAKE2 is Commonwealth only)
- BAKEALL1 phase 1 (2 rulings open: seam ring, dense-tile cap); BAKEVAR1 bake variants per enable parent
- PREVIEWLIGHT1 FO4 lighting as default model preview; NEARBOUNDS1; BRICK2 1 m bricks + presets
- FRAMERELIGHT1 per-frame GPU relight (settlement edits, elevators); LIGHTLAYER1; MOVEGI1 (carousels, vertibirds, lifts)
- THRESH1 remainder (door layers, swing blend, sky past closed doors, 7 leak cases)
- SSAO2 AO method row (Vanilla / GTAO / HBAO+ / XeGTAO); PROBEHINT1 .recx probe hint boxes
- FOGTOP1; FOGCLEAR1 (.nifx air volumes); NIFLIGHT1 / LAMPAIR1 / FOGSUN1; VOLPART2 (Frostbite unified)
- ROOM0SKY1; SKYVIS2 (particles, water, impostors read sky visibility); HOLEGATE1 general see-through gate
- WATER3 (ask before start); FARSKY1; snow PRT bounce
- UIPARITY1 lanes 2-8 (CAMERA1, TIMEWEATHER1, BAKE1, DEBUGVIEW1, SHOWREST1, MEASURE1, LODVIEW1)
- PROBEEXT1 rename .tbk -> .probe; REPOAUDIT1 history rewrite (yes 10-13)
- Needs his flight: AUDIT1 REF1 game capture, LLFCHECK1, PRTP4 (needs save names), MOTION1/TERRBLEND1/WATER2 captures
- FO4CS readers for new LOD formats (built last); VOLAIR1 + FARGRID1 port into FO4CS
- ATMORES1 follow-ons (ATMO2, SKYGI1, NIGHT1, EXPO1, CLOUD1-3, GLOW1) on his yes per lane

## 4. PLANNED, NO YES YET
In LANES_FUTURE.md (add rows there when he floats an idea; read it when he asks what is next or section 3 runs dry).

## 5. OWED CHECKS ON LANDED WORK (detail AUDIT_repo.md)
- GI: Goodneighbor recess2 black; GIBLACK2 frame gate 38.3 % vs 0.10 %; Museum of Freedom 2 spots + 5 roof voids;
  NEONGI1 near-field 0.10; GIAMB1 DALC halls darker. Re-measure now PERF1 + VOLFIT1 room volumes are in.
- ROOMSLOPE1 / ROOFCULL1 / PODPROBE1 / RETRACE1 / TOWERTOP1 queued defects
- MODPICK1 drop picture; FIXPACK1 REDFIX1 bar too loose; SHOWMENU1 particles rerun; SKYSTR1 E + red
- ALLON2 trash decals; ALLON1 remainder (white far bank, shoreline smears, dark top band, SSR)
- LOD water-marking tool: re-bake discards user marks (sharpest); HKX writer never loaded by FO4
- lodgen leftovers, impostor/VT rulings (FARBAKE2 may cover the card re-bake)
- 35 shot scripts force WW_CELL_BAKE_REFRACT_RED=keep: ruling owed
- About 20 questions to bungo unanswered (AUDIT_ledgers 49-54): ask one at a time

## LANDED / SUPERSEDED
Moved to LANES_DONE.md (append there at each landing; not read by default).
