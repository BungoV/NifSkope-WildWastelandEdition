# Night plan 2026-09-27 (bungo asleep; his words: "after you're done with fixing bakes, merge it, then render the bakes for me, this time fixed" / "I leave you to do it all overnight")

Integration: E:\Projects\NifskopeWWE-night, branch night-20260927 = main 422881d4 + ao2 (51bd399b) + gpu1 (8f58e7db).
Lanes launched 04:3x from 8f58e7db (in-session agents):
- WATER1  E:\Projects\NifskopeWWE-water1  brief_water1.md  (v3 water default + flat water view)
- GROUND1 E:\Projects\NifskopeWWE-ground1 brief_ground1.md (per-vertex ground contact stream; owns .lodi version bump)
- IDENT1  E:\Projects\NifskopeWWE-ident1  brief_ident1.md  (file-wide building ids + occluders; no version bump)
- TERR1   E:\Projects\NifskopeWWE-terr1   brief_terr1.md   (ground sky AO with objects + normal stamping)
Queued: TIDY1 (brief_tidy1.md) after GROUND1 merges into night (it builds on GROUND1's version).
Parked: SHADOW1 (brief_shadow1.md) -- bungo "but forgot the real render", ambiguous; not launched. LIT1 not merged.

## Status 05:58
- BLOCKER: turn.sh lock held by "anon" since 05:44:55 (WATER1 ran turn.sh with no args at 05:10; pid 51060 dead).
  WATER1 was refused release; NOT released by overseer (permission laundering rule) -- surfaced to bungo, awaits his OK.
- WATER1 code done (d658f922..3df91f7f), gates/pictures owed: resume per its DONE.md section 4.
- GROUND1 code done (8da99315..da045481, .lodi v12), gates/pictures owed: resume per its DONE.md section 7.
  Brief errors of mine: ">=250 within 16 u" contradicts the ramp formula (239 at 16 u); per-object mean gate
  fails on 761 tall trees because the old byte averaged .BTO verts, stream uses library mesh (corr 0.99992).
- TIDY1 launched ~06:00 from ground1-20260927 @ da045481 (worktree NifskopeWWE-tidy1).
- IDENT1 stopped blocked (b06fac32, c8d73984, f022b71d): contact join + per-building occluders coded; file-wide ids
  blocked (u16 group word too small); hill boxes need a format change (proposal: occluder flags bit1 terrain box,
  instanceIndex 0xFFFFFFFF) -> queue after GROUND1's v12. Its waiter pid 5592 (`acquire ident1`) gives up ~07:02; if
  the anon lock is cleared before then it holds the lock as ident1 -> also run `turn.sh release ident1`.
  WATER1's queued bake.sh waiter gives up 07:05 (it would run its bakes if it gets the lock -- fine).
- TERR1 stopped blocked (f78c574c code, a95f6f38 skill): normal stamping + sky march hits objects (128 u grid).
  Resume: E:\Projects\NifskopeWWE-terr1\scratchpad\terr1_20260927\resume.sh (on off noroads noflat / gates / pics / clean).
  CHECK at landing: predicted canyons 224 -> 16 (6% sky) looks too dark -- compare with the object sky stream on the
  same street walls and a real ray cast before accepting.

- TIDY1 stopped blocked (eb5dfa38, 8741678f, 9263048d): empty glow sheets dropped (0/121 LOD materials glow;
  aspen cards keep), duplicate layers merged by identical texels only (114 -> 106 Boston), labels fixed; resume
  `bash /e/Projects/NifskopeWWE-tidy1/scratchpad/tidy1_20260927/run_gates.sh`. maps1 fixed labeller in its maps1_fix\.
- Skills copied to AISkills: ww-lod-lit-view, ww-lodi-add-vertex-stream, ww-merge-by-texels, ww-lodl-water-view,
  ww-sky-raycast-check. nifskope-ww-worktree-build amended (live tree) with TIDY1's object-copy trap.
- ALL FIVE LANES: code committed, gates/pictures owed, blocked only on the anon lock (+ ident1 waiter).
  After release: resume each lane's scripts (one NifSkope at a time), then merge order into night:
  water1, ground1 (v12), tidy1 (on ground1), ident1, terr1; build; Boston re-gate; whole bake; renders.

- 06:46 night-trial (5b338d39 = night + water1 + tidy1(ground1) + ident1 + terr1, all merged clean) BUILT in
  E:\Projects\NifskopeWWE-night (worktree now ON night-trial; night-20260927 branch itself untouched at 8f58e7db):
  exe sha1 083227c9, 288 objects, every lane's marker string present. Script scratchpad\night_20260927\build_trial.sh.
  Lanes' gates still owed on their own exes first; then fast-forward night-20260927 to night-trial.

Per lane landing: verify on disk (git log, DONE.md, gates), merge-tree pretest, merge into night-20260927.
After all: build night, whole-Commonwealth bake (objects + terrain VT + water v3) on GPU, install to mods\FO4CSLOD
(backup previous), render every map full size with title bars (maps1 set, fixed), send, merge night into main,
splice ledgers (lane DELIVERABLE_TEXTs incl. AO2, GPU1, CELL1, VAN1, MAPS1, AUDIT1), tell him to restart NifSkope.
Copy new skills to E:\Tools\AISkills.

- 10:15 bungo "Reelease it": anon lock released 10:14:27. WATER1 resumed (SendMessage). Lane GATES launched for GROUND1, TIDY1, IDENT1, TERR1 owed gates (report scratchpad/gates/REPORT.md). Next: merge night-trial -> night-20260927, whole bake, install, renders.
- 11:50 TILING5 launched (bungo "so 1 and 2" + "terrain blending on lods / lod terrain patterning could use an improvement"): height-aware blend (hex joins + land layer transitions) + macro variation; worktree NifskopeWWE-tiling5 @ night-trial 5b338d39; brief_tiling5.md. Default OFF, his call. Queue: FO4CS near-ground anti-tiling (FO4CS last).
- 12:19 WATER1 DONE 22395e47 (verified on disk): all gates pass, 11 labelled pics sent to bungo. Silent render exits = Avast auto-sandbox (AvastSvc.log, 52 virtualization lines) -> his call. Open: coarse mesh pokes through at Charles shore spots; sea ring outside S/E cliffs; one v2 sheet past terrain edge. Splice its DELIVERABLE_TEXT.md.
- 12:43 WATER1 follow-ups sent: water depth view ON the water (derived, nothing stored); then drop vertex-colour water depth from code (bungo "so we drop the depth bake for water from code"). FLAT2 launched (brief_flat2.md, worktree NifskopeWWE-flat2 @ 5b338d39): one-colour whole-chunk terrain sheets -> flag + value; measure first, decline under 1%. Merge set before whole bake: gates-passed lanes + TILING5 + FLAT2 (a failing lane stays out).
- 12:45 QUEUE (future FO4CS feature, not tonight): sloped/authored non-flat water -- FO4CS draws water itself + own authoring format (mesh or river height curve) + bake: per-point water height plane in the v3 plane container (flat bodies = uniform tiles). After the FO4CS water reader.
- 12:45 bungo "we only need support for it on nifskope side": sloped water moved OUT of the FO4CS queue -> WATER1 task 3 (reads placed sloped water meshes, per-point surface plane in v3 before it ships, viewer draws it, synthetic sloped-river gate). FO4CS reader for it stays last.

## PAUSED 14:12 (bungo: "That's way too long, we're pausing now, make sure nothing gets lost")
All four live lanes told to stop at a safe point, commit by path, write RESUME in their report, release the lock.
OVERSEER RESUME ORDER:
1. Verify each lane on disk (git log, RESUME section, clean tree): GATES (scratchpad/gates/REPORT.md + ground1/tidy1/ident1/terr1
   DONE.md), WATER1 (NifskopeWWE-water1 DONE.md), TILING5 (NifskopeWWE-tiling5 scratchpad/tiling5_20260927/DONE.md),
   FLAT2 (NifskopeWWE-flat2 scratchpad/flat2_20260927/DONE.md). Resume lanes via SendMessage with the same agent ids.
2. Lock: .ns_turn must be absent before anything restarts.
3. Then: finish owed pics -> merge (water1, ground1, tidy1, ident1, terr1, + tiling5/flat2 if gates pass) into
   night-20260927 -> build -> Boston re-gate -> whole bake GPU -> install FO4CSLOD (backup) -> full-size renders -> merge main
   -> splice ledgers -> tell bungo restart NifSkope.
Known state at pause: 4 fix lanes' gates measured (ground1 625f0a44, tidy1 89ea5a26, ident1 2a2823a1, terr1 18216350);
water1 dc67e5c8 (depth bake dropped; depth view + sloped water task 3 in progress); tiling5 295d3988; flat2 7e160882.
