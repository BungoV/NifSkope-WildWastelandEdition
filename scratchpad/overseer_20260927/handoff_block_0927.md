## TOP BLOCK -- written 2026-09-27 14:28 (`date`-read) by the overseer: PAUSED mid-campaign, pick up here

bungo 14:0x: "That's way too long, we're pausing now, make sure nothing gets lost" / "Push what you have to my repo" /
"Make a handoff doc too so that we can easily pick up where we left off". Every lane stopped at a safe point, committed
by path and wrote a RESUME section. All branches below are PUSHED to origin (branches, not main). main carries only the
ledgers up to 422881d4 plus this block. NOTHING below is merged into main, baked whole-map, installed, or flown.

### What the campaign is
bungo reviewed the 79 LOD map renders (MAPS1) on 09-26 and called most of them broken: water not on water, ground
contact a useless texture, identity cut by chunks not per building, one texture for all buildings, ground normal without
roads/rails/slabs, cell height range pixelated. His order: "we fix all the maps that are broken, overnight" / "after
you're done with fixing bakes, merge it, then render the bakes for me, this time fixed". Fault list = lane AUDIT1's
DONE.md section 4 (overseer scratch). Night rules (lane rules) = the night_rules.md each brief names.

### Branches (all from night-20260927 = main 422881d4 + AO2 51bd399b + GPU1 8f58e7db)
| branch @ head | what | state |
|---|---|---|
| night-trial @ 5b338d39 | night + water1 + tidy1(on ground1) + ident1 + terr1 merged clean at their 06:4x heads; built green (exe sha1 083227c9) | the planned merge base; REBUILD after re-merging the lanes' newer heads |
| water1-20260927 @ badd500f | .lodl v3 water bodies default; flat water drawn per body; depth view ON the water (derived: body height - ground, nothing stored); vertex-colour water-depth bake DROPPED (bungo); sloped water from placed meshes (task 3) | all v3 gates PASS + 11 pictures sent; depth view done (3 pics); SLOPED WATER BUILT, NOT GATED -> RESUME in scratchpad/water1_20260927/DONE.md |
| ground1-20260927 @ 625f0a44 | .lodi v12 per-vertex ground-contact stream | PASS (lane GATES; verify.sh needs --mo2-profile at merge), C++ reader PASS, pictures made |
| tidy1-20260927 @ 89ea5a26 (on ground1) | empty glow sheets dropped, layers merged by identical texels (114->106), labels | FAIL (lane GATES): off PASS, on 5 PASS, ONE FAIL: an all-zero 256x512 card glow sheet kept (drop test did not fire on that group; cause not found) |
| ident1-20260927 @ 86dac8c6 | identity join by contact, occluders fit per building | FAIL (lane GATES): at cap 0 the Hub towers split into 53 and 44 groups, Diamond City into 4, the city welds into one 1,507-piece group; 59 of 495 occluder boxes poke out of their building. Suggested, NOT applied: tolerance 2, cap ~4096. Pictures came out empty (Avast). File-wide ids blocked (u16 group word); hill boxes need a format change (queue) |
| terr1-20260927 @ 4c3b5730 | object normals stamped into the ground _msn; ground sky (mask B) sees objects | FAIL (lane GATES): G2 324 normal blocks change outside the roads mask; canyon sky 3-7x too dark (mask B 12 vs physical ray cast 84-92 vs object stream ~52) -> sky law must be fixed before merge. resume.sh has a CR bug (strip carriage returns with tr); 3 of 8 pictures made |
| tiling5-20260927 @ 6249465b | height-aware land blend + large-scale colour variation (bungo "so 1 and 2"), both OFF | off 27/27 PASS; macro-vs-hex PASS; transitions 7/14 -> 8/14; repeat 6/7 (no arm 7/7); grain band-shape red on 5 of 7; WW_TILING5_BETA/MACRO env overrides must become constants -> RESUME in scratchpad/tiling5_20260927/DONE.md (`EXE=$PWD/run_m3/NifSkope.exe bash t5_mosaics.sh`) |
| flat2-20260927 @ a9006455 | one-colour whole-chunk .lodt sheets stored as a 16-byte record + flag, on by default, --no-collapse-uniform = old bytes | measured: installed bake 5.96 of 19.98 GB (29.8%) terrain sheets are one value per tile (Commonwealth: normal + height sheets of outer ring and sea; Nuka-World: all four). Off gate PASS; Boston collapses 0 of 180 (vacuous); sea-region gates + renders owed -> RESUME in scratchpad/flat2_20260927/DONE.md. Picture pics/F1_commonwealth_one_value_tiles.png |
| lit1-20260927 @ b5ca755e | lit LOD view (sun/sky) | NOT for merge unless bungo says so |

Gate runs of the four fix lanes: lane GATES (GROUND1 PASS; TIDY1, IDENT1, TERR1 FAIL). Its REPORT.md could not be written; the verdicts live in each lane's DONE.md and in this table.

Leftover queued scripts that may launch ONE NifSkope run each when the lock frees: TERR1 `resume.sh pics` (shot junction_normal_after_4x) and FLAT2 sea_bakes.sh (3 bakes ~7.5 min into bakes/sea_*). Harmless; let them finish.

### Pick up in this order
1. Lock: E:\Projects\NifskopeWWE-fix1\scratchpad\fix1_20260926\.ns_turn must be absent (turn.sh release <name>, only
   the holder's name; a release of someone else's lock needs bungo's word). One lane NifSkope at a time, second monitor,
   unused --port.
2. Finish the owed lane work from each RESUME: WATER1 sloped-water gates + picture; TIDY1 the one glow sheet; TERR1 the
   sky law (compare skill ww-canyon-sky-physical-check); TILING5 constants + final arms + pictures; FLAT2 gates +
   pictures.
3. Re-merge the lanes' current heads into night-20260927 (order: water1, ground1, tidy1, ident1, terr1, then tiling5 and
   flat2 if their gates pass; a failing lane stays out). Build (skill nifskope-ww-worktree-build; worktree
   E:\Projects\NifskopeWWE-night has objects; build_trial.sh there). Boston re-gate.
4. Whole-Commonwealth bake on the GPU (objects + terrain VT + water v3), back up the installed mods\FO4CSLOD, install.
5. Render every map full size with a 60 px title bar, one picture per map, never a contact sheet (MAPS1 list with
   TIDY1's label fixes; water drawn flat). Send them.
6. Merge night into main on the work being done (bungo: "after you're done with fixing bakes, merge it"); splice every
   lane's DELIVERABLE_TEXT (AO2, GPU1, CELL1, VAN1, MAPS1, AUDIT1, WATER1, GROUND1, TIDY1, IDENT1, TERR1, TILING5, FLAT2)
   into HANDOFF / WW_CHANGES / MISTAKES; push; tell bungo to restart NifSkope.

### Machine facts learned today
- Avast auto-sandboxes fresh NifSkope exe copies: the run exits rc 0 in seconds with an empty log and no picture
  (AvastSvc.log "marked for virtualization", then "unable to add autosandbox exclusion ... error 122"). Reuse a copy
  that already ran; skill ww-gui-launch-silent-exit. An exclusion for E:\Projects\NifskopeWWE-* is bungo's to add.
- turn.sh with no name takes the lock as "anon"; a dead holder blocks every lane (05:44-10:14 today).
- A lane's run crashed with a null call (0x0) at 10:47 on --port 43742 with an empty file argument; cause not found.

### Queue (not started)
FO4CS readers for v12 / v3 water (+ sloped surface plane) / missing glow / one-value tiles (FO4CS last, by standing
order); FO4CS near-ground anti-tiling; hill-box occluders (format change); file-wide identity (wider group word);
SHADOW1 (brief written, parked); CELL1 external result pickup (its job files kept in the session scratch cell1_20260926);
next speed lane (terrain VT across all cores, hash while writing, AO tail).


### FO4CS note (bungo 14:2x)
Cloud shadows compared with 1001Bits/FO4CloudShadows: theirs = 256 cubemap of captured cloud meshes, mip 0, shell 10000 u (~140 m), opacity 2.0 clamped -> hard edges. Ours = weather cloud textures, deck 110000 u, 200000 u tiles, footprint mip, opacity 1.0, sky fraction 0.25 -> soft. bungo's screenshot of THEIR mod shows the hard edge. No action.
