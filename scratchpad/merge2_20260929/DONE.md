# MERGE2 -- IDENT2 + TERRLIVE1 into main (started 2026-09-29 12:14, date-read)

## 1. Merges (2026-09-29 12:14)
- Worktree E:\Projects\NifskopeWWE-merge2, branch merge2-20260929 from origin/main bc8f6f7e (bungo's rulings d07c52e4..bc8f6f7e kept).
- merge ident2-20260929 @ 9597d868 -> 4c418ff6. CLEAN (no conflicts). Shared with main: MISTAKES.md (main appended at the end, IDENT2 at the top: auto-merged).
- merge terrlive1-20260929 @ d2dad00b -> 75a7fb01. CLEAN. Only shared source file between the lanes: src/nifcli.cpp; hunks do not touch (IDENT2: --landmarks at 4120/6527/7387/8133; TERRLIVE1: terrain-option/outside-paint/decal-check/rule-check elsewhere).
  The brief expected lodgen/lodgenmanager/docs overlap: IDENT2 does not touch lodgen.cpp, lodgenmanager.cpp or LODGEN_TERRAIN_VT.md (it touches nativeemit/cellidentity/lodifile/lodgenchunkpass + LODGEN_NATIVE_LODO_LODI.md), so there was none.
- Pushed origin/merge2-20260929.

## 2. Build (12:18)
- Objects from the TERRLIVE1 worktree (its head d2dad00b, make -n = 0 g++ lines there), .qmake.stash + runtime from it; qmake in merge2 (125 own paths, 0 foreign).
  Touched the 12 files that differ from d2dad00b (IDENT2's) plus every includer of its 4 changed headers; deleted the REVISION objects (lodbfile, main, about_dialog + lodbfile.h includers).
- tools/ww_build.sh under the turn (12:16:15-12:18:09): BUILD-RC=0, exe newer than every changed source, copies in step. Recompiled: about_dialog main nifcli lodgen lodgenmanager lodgenchunkpass lodbfile lodofile lodifile cellidentity cellview lodinative nativeemit nearlib qrc_nifskope + 21 moc. Merged exe sha1 1e961ba5bdb6; holds both lanes' strings (WW_LODI_GROUPS_PER_CHUNK, lodgen_landmarks, outside-paint, rule-check, decal-check, terrain-option).
- Run copy: scratchpad/merge2_20260929/run_m2 (same exe). TERRLIVE1 head exe for comparisons = its run_rule (df981d25 = its release exe, make -n 0).

## 3. IDENT2 gates on the merged exe (12:25) -- reached because the merge carries IDENT2's emitter/reader code
LIGHT Boston bake bakes/m_light (IDENT2's recipe, bake.sh LIGHT=1, 257 s), group dump dump_m2.txt:
- **Whole LIGHT tree = IDENT2's b_v13, byte for byte: SAME, 107 files** (cmp_trees.sh, exe digest masked); .lodi 745fbd484866 and .lodo 9fcb9e6b11c9 equal the lane's.
- Pixel gate (pixgate.py): **PASS, 0 wrong pixels**, all 4 landmarks (g_pix.out). Lane: 0.
- Group dump (lmchunk.py): **Diamond City FILE-WIDE 1 id (1178) over 2 chunks, 404 pieces, 0 others [ONE ID]**; Hub east 1419:749, west 1336:758, Trinity 1420:25, each ONE ID. Lane: the same ids.
- Poke gate (lodi_occluder_building.py --gate): **ok, 517 boxes, 0 over 1 %, worst 0.0041**; floor grown 1.25x -> 511 of 517 over. Lane: same.
- Coverage at the lane's three eyes (Trinity 2121,-21470; brick 677,-27222; Diamond City -13838,-24517): **0.5877** (0.5866 / 0.7602 / 0.4164), Hi-Z 0.7529. Lane 0.5877. >= 0.588 as the brief words it: 0.5877 rounds to 0.588 -- the same number the lane reported, not below it.
  (My first run used coverage.py's built-in default eyes, which are IDENT1's older points: 0.5176. Not the lane's run; rerun with its eyes. The skill warns about exactly this.)

## 4. Boston box byte gate, TERRLIVE1 head vs merged (12:45) -- reached because both lanes' code writes into one bake
Both with IDENT2's way back so the identity files can be compared too: TERRLIVE1 head exe (its run_rule, df981d25 = d2dad00b)
`--identity-join proximity --occluder-fit piece` -> bakes/t_off (597 s); merged exe the same + WW_LODI_GROUPS_PER_CHUNK=1 -> bakes/m_off (523 s).
Outside paint left at its default (vanilla), terrain option default (hybrid): this IS the outside-paint OFF byte gate on the Boston box.
- **cmp_trees.sh: SAME, 232 files** (full recipe: native pair, VT, cover, cards, arrays, decals; exe digest masked in .key/.lodb, override path masked in the flat report).
- Red controls: one flipped byte in the .lodg -> DIFF 1 of 232 (named); the .lodo moved aside -> MISSING; restored -> SAME 232.
- .lodb read in full (the comparator reads only its product/chunk lines): both carry `terrain hybrid`; the other differences are the revision (9d2996b8 vs 75a7fb01) and exe size, run paths, the flat report's digest (its path row), and wall times (decal gather ms, AO cast s, stage times).

## 5. TERRLIVE1 gates on the merged exe, whole map (13:30) -- reached because the merge carries TERRLIVE1's terrain/decal/rule code
TERRLIVE1's whole-map VT-only recipe (its chain6.sh, wbake.sh here) with the merged exe: w_off (defaults: HYBRID, outside vanilla) 1,293 s; w_rule (`--outside-paint rule`) 1,380 s. (Lane: 1,299 / 1,513 s.)
- **OFF byte gate vs TERRLIVE1's head bake** (gate_off.py whole_off vs w_off): **GREEN, all 7 files sha1-identical** (VT.8/16/32, VT.lodm, flat report, .lodd, .lodg), no new file.
- **RULE bake vs TERRLIVE1's head rule bake** (whole_rule vs w_rule): **GREEN, all 8 files identical** incl. the .lodr (2,687,771 B, 7626cd4ff9ff = the lane's).
- Red half: w_off vs w_rule --expect-red -> RED as expected (VT.8/16/32 differ, a new .lodr).
- **Edge gate, vanilla** (gate_law2.py VT.8): north steps outside 1.39, dip 0.00, step 0.89; west outline outside 1.48, dip 0.27, step 0.02 -> **PASS** (lane: 1.39 / 1.48, dip 0.00 / 0.27).
- **Edge gate, rule** (--rule): north dip 0.00 step 0.78; west dip 0.54 step 0.10 -> **PASS** (lane: same); drift reported 3.35 / 7.02 (lane: same).
- **--decal-check** (w_off): .lodd 2,818 pieces (roads 642, flat 1,627, flat-over 549), 161,076,176 B, 2,774 with a normal stamp, **0 CRC mismatches**; .lodg 80,577 placements, 2,857 cells, busiest 311, all bad counts 0; pair matched (f34c4cd6); 0 decode failures. = the lane's numbers.
- **--rule-check** (w_rule .lodr): OK, cells -96,-96 + 192x192, grid 1536x1536 at 512 u, 2,359,296 samples (pairs 1,941,958), palette 36 (unused 10), band 8192 u. Also OK on the lane's own .lodr (the exec check at the chain's start).

## 6. lodgen_native.sh on the merged exe (13:29-14:03; written 14:04) -- reached because both lanes change the native bake (.lodi v13 / the .lodb rows)
- **34 checks, 1 failure = the known leg-5 .lodb ledger line, the SAME line as IDENT2's v13 run** (h_lodgen_native.log lines 81 + 84 = IDENT2's lines 81 + 84, word for word: "unaccounted: products" and the follow-on "two ledgers differ ONLY in ..."). The native record's row kinds are identical to IDENT2's run (alg 1, baked 1, census 11, chunk 9, hash 5, out 34, switch 24, ...), so TERRLIVE1's new .lodb rows add nothing there. Owner: INCR2's .lodb rows need a group in tests/spells/lodgen_btofree_ledger.py (a decision, left red).
- Leg 4 ok; leg 6 decoder 87 checks 0 failures; leg 13: 280 of 280 boxes hold; leg 13c: 312 boxes, 0 over 1 %, worst 0.0041 (= IDENT2's).

## 7. main fast-forwarded and its exe rebuilt (14:08)
- main bc8f6f7e -> 75a7fb01 (fast-forward to the merge), pushed. No collisions with untracked files in the main tree; its 6 locally modified scratchpad/incr_gate_work files were left alone.
- build_main.sh under the turn (14:05:54-14:08:28): qmake (NifSkope.pro changed) RC 0; touched the 22 changed sources + includers of changed headers (38 files); deleted the REVISION objects; tools/ww_build.sh BUILD-RC=0, exe newer than the sources, copies in step. Recompiled 19 objects incl. loddecal, terrainpreview, lodgen, nifcli, lodifile, nativeemit, qrc_nifskope. make -n afterwards: 0 g++ lines. No NifSkope was running from main (nothing renamed).
- **Deployed exe: E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe, sha1 6eb5d495919ae350db50a313a9f71ebab215ddcd** (release/build_rev.txt 75a7fb01). Holds both lanes' strings.

## 8. Ledgers spliced (14:09; written 14:10)
- splice.py (beside this file, drafts splice_*.md): WW_CHANGES.md gets the TERRLIVE1 entry at the top and IDENT2's heading now says merged (+1,185 B; CRLF lines 19,020 -> 19,020, LF 6,722 -> 6,735: the inserted text takes the ending of the line below it, no other byte rewritten). MISTAKES.md gets a new top section: TERRLIVE1's items + MERGE2's own (the coverage default-eyes slip) (+3,380 B, all LF kept). HANDOFF.md: new top block (options HYBRID default + DYNAMIC, FULL ditched; outside paint Vanilla default / Rule; .lodi v13 file-wide ids; new files .lodd/.lodg/.lodr; gates; main's exe; the installed bake is the ditched FULL law-1 bake and is owed a re-bake; rulings carried word for word from MERGE1's block, with a status line; owed list); MERGE1's block renamed to HISTORY (+12,763 B, all LF kept).
- Committed on merge2-20260929 by explicit paths (the three ledgers + this folder's small records; not run_m2/, bakes/, w_off/, w_rule/, h_native/, dump_m2.txt), pushed; main fast-forwarded to that commit and pushed. main's deployed exe is still the 14:08 build (6eb5d495...): the ledger commit changes no source.

## 9. Skills
- Loaded: nifskope-ww-campaign-merge, nifskope-ww-worktree-build.
- Wished for: a per-lane gate-arguments manifest (each lane writes the exact arguments of its gates -- coverage eyes, bake env, recipe -- into one file the merge lane runs from), so a merge never falls back to a tool's built-in defaults.
- Written: none.
