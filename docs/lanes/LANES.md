# NifSkope WW lanes: every ask and its state (living list)

Rule (bungo 10-21): keep this current at every lane start, finish, merge and ruling. Every ask gets a row the moment
bungo says it. Tiers: WORKING ON -> BUILT, NOT LANDED -> LANDED; APPROVED (his yes, not started); PLANNED (future, no yes
yet). A row leaves only as LANDED or SUPERSEDED, and then moves to LANES_DONE.md (keeps this file short). Read this before saying anything is "done" or "covered".
Main now: 9c624f47 (10-21 night, MERGEALL1 landing). Detail of old audits: overnight_1021\AUDIT.md, AUDIT_repo.md.

## 1. WORKING ON NOW
| Lane | What | State |
|---|---|---|
| MERGEALL1 | merge every built branch into main, clean merged worktrees (bungo 10-21 "merge everything") | 17 landed (LANES_DONE); 3 held (section 2); cleanup next |
| PERF2 | time the frame; depth reuse, per-tile lights, fewer driver calls, temporal reuse (+ fog history reprojection, Division-style, bungo 10-21), fog integrate O(S^2) -> O(S) | running |
| VOLAIRSKY1 | VOLAIR1 sky gate fail (0.266 % vs 0.20 %) + placed lights in the fog (audit claim: 0 clusters; lane finds 1309-1419 filled on steady frames, 0 only the first ~8 frames after load) | running |
| SSGI2 | SSGI gate (SSGI1 landed ungated) + blue noise, depth intervals, emissive, pyramid | running |
| SHADOWPAR1 | shadows to FO4CS level (TERRSHADOW1 WEAKER list) + fog far-tail leak | running |
| FXCHECK1 | BGEM glass milky; no fire-barrel particles | running |
| BAKESPEED1 | far-LOD bake speed (chunks 2864 -> 1191 s mid region) | resume/determinism gates |
| RTBAKE1 | GPU ray-traced bake (gate cell 52.5 -> 43.0 s) | museum gate running |

Queued tonight (his 10-21 go): FARBAKE2 whole-map rebake, every channel (after BAKESPEED1); VTGPU1 + PROFILE1 (after
BAKESPEED1); ROOMS-RT room enclosure by rays (after RTBAKE1).

## 2. BUILT, NOT LANDED (MERGEALL1 holds these)
| Lane | Branch | What it gives | Why held |
|---|---|---|---|
| PBRALBEDO1 | fixpack1 | bake reads .pbrm base color x (1 - metallic) | its own gate fails: 0/26 fixture materials resolved |
| VOLPART1 | volpart1-20261014 | steam/fog cards lit as volumes (ships off) | built on old VOLAIR1: 9 files conflict; needs a rebase lane |
| LEANCOMMENTS1 | origin/cloud-leancomments1 | lean comments pass 1 (-2758 lines) | 16 conflicts; re-run on final main |

## 3. APPROVED, NOT STARTED (his yes on record)
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
