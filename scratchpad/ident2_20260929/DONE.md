# IDENT2 -- landmark rule for building groups; the poking occluder boxes (started 2026-09-29 03:59, `date`-read)

Worktree E:\Projects\NifskopeWWE-ident2, branch ident2-20260929 from origin/main @ 9df26ffd.
Brief: overseer scratch lane_ident2_brief.md. bungo's rulings (HANDOFF top block items 1-2): Diamond City ->
landmark rule; a row-house terrace stays ONE group.

## 1. Skills loaded
nifskope-ww-worktree-build, ww-lodi-drawn-mesh, ww-gui-launch-silent-exit, deepseek-offload (search-lean has no
SKILL.md in the live skills tree). nifskope-ww-lodgen read by section as needed.

## 2. Setup (04:02)
- Main's objects current (`make -n` 0 g++ lines, no src/res/lib newer than main's exe, only docs differ since the
  merge) -> copied .qmake.stash, GeneratedFiles, release runtime; deleted REVISION objects (lodbfile, main,
  about_dialog + includers lodgen, lodgenchunkpass, lodgenmanager, nativeemit, nifcli); qmake rc 0 (123 own paths,
  0 main paths).

## 3. What changed (code, 04:16 build; exe 112779a41cb4, rung = main's exe bde2a4ba9138)
- **Landmark rule** (src/nativeemit.cpp, contact join only): before the capped contact unions, every placement is
  matched against `res/lodgen_landmarks.txt` (built in as `:/lodgen/landmarks.txt`; `--landmarks <file|none>`).
  Match = the LOD model path (lower case, '/', leading `meshes/` then `lod/` off) STARTS WITH one of the rule's
  prefixes AND, when the rule has a centre, the placement's X/Y is within the radius. First rule wins; a
  placement matching two rules is counted as a conflict. All matched pieces of a landmark are joined with no cap.
  Census: `native-landmarks:` (source, digest, rules, pieces, conflicts) + one `native-landmark:` line per rule
  (pieces, group members, extent, chunks it sits in; a CROSSES A CHUNK LINE note). The group dump gets a
  `# landmarks` header line. The chunk-cache identity names `native.landmarks <digest>` only when the contact join
  is on, so the way back hashes as before.
- **Occluder probe** (fitBuildingBox): (a) a hit within 0.5 u of an OPEN triangle edge (an edge no other triangle
  of the group shares, matched by exact end points) is a miss; (b) the probe lattice spans the box grown 0.5 u
  every face; (c) when all symmetric half-voxel shrinks fail, ONE face comes in by 1/4 then 1/2 voxel (z first,
  from the largest round first); first pass kept and counted NUDGED; none -> refused. The gate script
  (tests/spells/lodi_occluder_building.py) is NOT touched.
- Chunk line: the file's group word is a u16 dense PER CHUNK (16,384 u grid). A landmark across a chunk line is
  one group in the emitter but gets one id in each chunk it touches. A file-wide id is a format bump: out of scope.

## 4. Bakes (started 04:21, bakes.sh -> bakes.console)
b_main (main exe, defaults, LIGHT, dump_main.txt) / b_after (new exe, defaults, LIGHT, dump_after.txt) /
b_off_rung + b_off_new (full, `--identity-join proximity --occluder-fit piece`).

## 5. Row houses: far-shadow harness NOT cheap (04:25)
No code change for row houses (bungo: a terrace stays one group). The s9.2a harness
(main scratchpad/identgap_20260919) is built on ONE chunk only: sunsim1_20260919/scene.py:31
`CHUNK = (4*4096, -12*4096, 8*4096, -8*4096)` (x 16,384..32,768, y -49,152..-32,768), its geometry cached from a
2026-09-19 dump bake (scene.py:24 BAKE), cameras hard-coded (run.py:43 hwydeck/east/street) with cached G-buffers and
truth images, and its tables asserted at 588/167 groups (run.py:236-237). The terrace at (-5975, -15112) is far
outside that chunk: running it means a new geometry + land extraction, new G-buffer/truth for a new camera and new
tables -- a lane of its own, not a cheap run. So: not measured.
DeepSeek: surveyed the identgap far-shadow harness's inputs (camera, tables, cached data) -- pro, 98s, $0.0853

## 6. Results, first build (04:25-04:32)
BEFORE (b_main, main's exe, IDENT1 numbers reproduced): towers east 1 / west 4 groups, Trinity 1, Diamond City 15
(top 56 = 33 DC + 23 other), row houses 54 pieces in 1 group; poke gate 511 boxes, 5 over 1 %, worst 0.1605 (FAIL);
coverage mean 0.560, Hi-Z 0.728 (cov_main.out). My list matches the same pieces as IDENT1's audit words:
DC 165, east 730, west 727, Trinity 25.
AFTER landmark rule (b_after, gates_after.out, lmchunk_after.out): **east 1, west 1, Trinity 1, Diamond City 1**
(165 pieces, 0 foreign, 9,137 x 10,582 u); row houses unchanged 54 in 1 group. The 2 multi-piece groups over the
4,096 cap are DC and the west tower (4,100 u), both landmarks. **Chunk line:** DC is ONE emitter group but sits in
chunks (-2,-2) [14 pieces, file id 85] and (-1,-2) [151 pieces, file id 30]; no other instance of either chunk
carries those ids. Towers and Trinity are in one chunk each (ids 53, 77, 54). The same script on the before file
shows DC as 15 groups -- the check fails on the old code.
Offline re-run (groups.py with the landmark pre-join) vs the emitter: 17 groups differ (before 20) -- the dump
prints distances rounded to 0.001 and 1,308 pairs sit at the 32 u tolerance itself; IDENT1's known limit.
Poke gate, first probe fix (group-wide open edges): 520 boxes, **3 over** (177: 0.0123, 415: 0.1111 = IDENT1's
408, 438: 0.1111), 27 nudged, 25 refused. Red controls on the same file: inflate 1.02 -> 236 over, 1.05 -> 428 over.
Cause of the misses: open edges were matched across the whole group; two stacked pieces share the seam's exact end
points, so the seam looked closed. Fix 2: open edges are per placement (04:31 patch, build pending the turn).

## 7. Way back, first build (04:47-04:48)
b_off_rung (main exe) vs b_off_new (IDENT2 build 1), both `--identity-join proximity --occluder-fit piece`, full
bake: cmp_trees.sh **SAME, 233 files** (exe digest masked in .key/.lodb as the comparator always does); .lodi
4f913d6d4b4c and .lodo b144e9aff92d in both. Red controls on hard-linked copies: one flipped byte in the .lodi ->
DIFF 1 of 233; the .lodo removed -> DIFF 1 (names it). Re-run owed on the final exe.

## 8. Probe fix 2 and 3 (04:52-05:36)
- Build 2 (open edges per placement): box 415 (IDENT1's 408) fixed; 438 (0.1111) and 177 (0.0123) still over.
  boxdiag*.py: the file's member triangles match the emitter's own bounds within 0.18 u (z step 0.05 u), so it
  was not rounding. Box 438's whole TOP lattice plane fails the level rays: three walls end 0.56 u below the box
  top. The emitter passed it because it probed only the box GROWN 0.5 u -- a different set of lattice points,
  which happened to meet taller parapets (grown lattice 0.0000 out, the file's lattice 0.1111). Box 177: a column
  of the file's lattice runs up a hairline gap between roof pieces; the grown lattice misses that line.
- Build 3 (exe 358b9abc5f93): the probe runs TWO lattices, both must pass: the box as the file stores it
  (half x 0.999, the gate's own points) and the box grown 0.5 u. Census: groups 1,246, fitted 573, too thin 647,
  refused 26, shrunk 117, **nudged (one face in) 32**.
- **Poke gate (b_after3): PASS -- 520 boxes, 0 over 1 %, worst 0.0041, volume-weighted 0.00004; floor: grown
  1.25x, 514 of 520 over.** Red controls on the same file: inflate 1.02 -> 230 over (FAIL), 1.05 -> 427 over
  (FAIL). Before (main exe): 511 boxes, 5 over, worst 0.1605.
- **Street coverage** (same three eyes): mean **0.560 -> 0.570**, Hi-Z culled 0.728 -> 0.736. Per eye: Trinity
  eye 0.578 -> 0.578, (677,-27222) 0.638 -> 0.752, Diamond City eye 0.464 -> 0.380. DC-area boxes 12 -> 10
  (volume 1.48e10 -> 1.27e10). A `--landmarks none` bake on the same exe is running to split the landmark rule's
  share from the probe's.
- Landmark gates on b_after3: unchanged from section 6 (east 1, west 1, Trinity 1, DC 1; rows 1; DC ids 85 + 30).

## 9. Landmark rule off on the final exe (b_nolm, `--landmarks none`, 05:58-06:03)
- Census: `list none (--landmarks none) ... 0 piece(s) matched`. Groups: east 1, **west 4, Trinity 1, DC 15** (the
  landmark gate FAILS with the rule off -- the red control for it, same exe).
- Poke gate: 521 boxes, 0 over, worst 0.0041 (the probe fix alone passes too).
- Coverage split: main 0.560 [0.578 / 0.638 / 0.464] -> probe fix alone 0.588 [0.579 / 0.730 / 0.454] -> probe fix +
  landmark rule 0.570 [0.578 / 0.752 / 0.380]. The landmark rule costs 0.018, nearly all at the DC eye: the
  stadium is one ring-shaped group, and one box a group fits less of a ring than 15 fragments did. Several boxes
  for one landmark group would win it back; not done (scope).
- Docs 4.5.4 + 4.9 paragraphs and WW_CHANGES.md (top entries are LF; 14 LF lines added, CRLF count unchanged)
  written 06:03.

## 10. Way back on the final exe (b_off_new3, 06:02-06:10)
- `--identity-join proximity --occluder-fit piece`, main exe (b_off_rung) vs final exe (b_off_new3):
  **SAME, 233 files**; .lodi 4f913d6d4b4c, .lodo b144e9aff92d (same digests as build 1's run).
- Red controls on the same comparator (06:12): one flipped byte in the .lodi -> DIFF naming the .lodi; the .lodo
  removed -> MISSING naming the .lodo.

## 11. Commit and push (07:04)
- 02e70e33 on ident2-20260929, pushed, not merged (source, list, docs, WW_CHANGES, MISTAKES, lane tools).
- Skill written: ww-lodgen-landmark-add (Claude skills + AISkills).
- lodgen_native.sh (legs 4/13/13c) and the pictures wait on the turn (TERRLIVE1 holds it since 06:13).

## 12. lodgen_native.sh, final exe 358b9abc5f93 (turn 07:17-07:5x; no leg selector, all legs run)
- Leg 4 (Sanctuary region bake): ok, all 10 checks.
- Leg 13 (piece fit, downtown Boston): ok; 280 boxes, 280 of 280 hold all 100 points; floor 280 of 280 leak.
- **Leg 13c (default building fit): ok (was FAIL in IDENT1: 302 boxes, 6 over, worst 0.1605). Now 313 boxes,
  0 over 1 percent, worst 0.0041; floor 1.25x -> 307 of 313 over.**
- 34 checks, 1 failure: leg 5 "every field of the record is one this comparison has decided about (unaccounted:
  products)" -- the same line fails in IDENT1's log (h_lodgen_native.log:84); a .lodb field from another lane, no
  IDENT2 change touches the .lodb writer. Leg 3 passes this run.
- Log: h_lodgen_native.log. The kept bake (h_native) deleted after reading.

## 13. Pictures (08:05; shot.sh under the turn, second monitor, ports 51871-51878, all rc=0)
- View 8, 1600x1600, LV=2 SLOT=0 SDIM=2, WW_RENDER_FLAT=1 WW_LODL_CHANNEL=identity. Before = run_rung + b_main,
  after = run_new + b_after3.
- pics/dc_before|after.png: region -5 -8 -2 -5, ortho 6144. 15 colours -> one lilac mass.
- pics/hub_before|after.png: region -2 -9 1 -5, ortho 10240 (6144 cut the tower tops off; retaken). West tower =
  the right one.
- pics/west_before|after.png: region -2 -8 -1 -8, ortho 4096. The 4 base pieces (3 extra groups before, up to
  990 u) take the tower's colour after. A sign band stays its own group: not in the hitext prefix, and the west
  group (4,100 u) is over the cap, so contact cannot add it.
- 08:06: deleted b_off_rung, b_off_new3, b_nolm and the sheet cache (digests and outputs logged above). Kept b_main + b_after3
  (114 MB each) so the pictures can be re-shot.

## 14. Follow-up from the coordinator (08:12): footprints
bungo: landmarks not whole in the after pictures (pieces inside the DC ring, the west tower sign bands and base
pieces keep their own colour). Rework: a landmark = name prefixes PLUS a footprint; every non-tree piece whose
bounds sit inside the outline joins, uncapped; a piece crossing out by more than a margin is refused and logged.
Sign band joins the tower. Gate: pixel count of non-landmark colour inside each outline (trees excluded) = 0.

## 15. Footprint, first cut: whole contact GROUPS (08:22-08:40)
- Outline call (logged): the convex hull on X/Y of the name-matched pieces' box corners, derived in the emitter.
  A box around Diamond City takes in street blocks at its corners; the hull does not.
- Tower names: the west rule also names the Prudential and backbay17 LOD shells + structures (the same tower's
  distant models, 675 u past the hitext hull); the east rule names backbay_ttowerbld01 + structure (already in its
  group).
- First build judged whole contact groups (join when every piece is within 256 u; else the whole group refused).
  Bake b_fp (dump_fp_group.txt). Census: DC 40 groups / 233 pieces joined, 2 refused; west 5 / 9, 2 refused.
- New gate pixgate.py (offline from the file: in-outline triangles point-splatted, 5 views, top + 4 obliques at
  35 deg; a pixel is wrong when its instance's (chunk, group id) is not the landmark's). Red control on b_after3:
  **FAIL, 291,974 wrong pixels** (DC 134,682; west 154,564; east 2,728; Trinity 0).
- Group rule result: **2,743 wrong** (DC 732 = 4 pieces of street block 5498 standing inside the stadium; west
  2,011 = 16 bldgshell pieces of block 7048 + a billboard). The pictures show it: the west tower's left-side
  piece bungo circled is exactly those bldgshell pieces. Refusing whole groups keeps them out.
- So back to bungo's words, PER PIECE. Offline sweep (fp_piece.py on dump_after3): pieces inside leave the hull
  by at most 319 u; the nearest refused is 1,460 u (whole-block LOD shells). Margin set to **512 u**.

## 16. Footprint per piece (build 08:43 exe 23c51c4c54f1; bakes 09:06-09:28)
- Emitter: after the named join and BEFORE the contact pass, every drawn non-tree piece whose box overlaps the
  hull and leaves it by <= 512 u joins its landmark, uncapped. Refusals: one census line each + `# landmark-refused`
  dump lines; joins: `# landmark-footprint`; the hull: `# landmark-hull`.
- b_fp2 (full bake) and b_fp3 (LIGHT, like b_after3, dump_fp3.txt) give the same census: DC 239 joined (farthest
  304 u), 1 refused (fens04_bld01lod, 3,494 u out); east 17 joined, 0 refused; west 27 joined (farthest 319 u),
  2 refused (backbay15_bld01lod 2,233 u, backbay18_bld02lod 1,460 u), 6 tree/plant placements left alone;
  Trinity 0 / 0. 283 pieces joined by the footprint in all.
- **Pixel gate (pixgate.py, 5 views, trees excluded): 0 wrong pixels in all four outlines (PASS).** Red controls:
  b_after3 -> FAIL 291,974; the group-rule build (dump_fp_group) -> FAIL 2,743 naming the 20 split pieces.
- Group counts (lmchunk, from the file): DC 1 group, 404 members, ids 85 (-2,-2) and 30 (-1,-2), no other
  instance shares either; east 1 (749, id 53); west 1 (758, id 52); Trinity 1 (25, id 54). All groups 4,576 -> 4,527.
- **Poke gate: 513 boxes, 0 over 1 percent, worst 0.0041; floor 1.25x -> 507 of 513 over. ok.**
- **Coverage: mean 0.570 -> 0.558, Hi-Z 0.736 -> 0.733** (Trinity eye 0.578 -> 0.578, Hub eye 0.752 -> 0.730,
  DC eye 0.380 -> 0.368). Boxes 520 -> 513: the footprint pieces now sit in the landmark's one box, not their own.
- **Way back (b_off_fp, full bake, `--identity-join proximity --occluder-fit piece`): .lodi 4f913d6d4b4c, .lodo
  b144e9aff92d = the recorded digests.** Whole-tree compare against a fresh rung bake: queued after the pictures.
- Chunk-row label bug in lmchunk.py/pixgate.py found and fixed (MISTAKES.md); earlier numbers unaffected.

## 17. Pictures retaken (b_fp3 LIGHT, same cameras; written 09:35)
- pics/dc_fp.png, pics/hub_fp.png, pics/west_fp.png (same cameras as the befores/afters) + pics/westclose_fp.png
  (closer west-tower view, 2,560 u half width). Befores: pics/dc_before.png, hub_before.png, west_before.png.
- West tower: the two sign bands, the pale base piece and the left-side wall now carry the tower's pink (the
  strip at hub pixel ~995,480-550 was green (90,179,132) in hub_after, pink (244,69,136) now).
- DC: one lilac mass + the violet strip on its west side = chunk (-2,-2), id 85, the same group (u16 id per chunk).
- The red strip right of the west tower in hub_fp = group root 6027 (BldgBrick6Story2x2CornerResEntA_LOD,
  BackBay21_Bld01LOD, BldgBrick5Story1x2ComA_LOD), 2,100-2,750 u outside the hull, no overlap: a separate
  building behind the tower, not a tower piece.
- Close view: non-pink colours near the base are trees or pieces wholly outside the outline. Listed from the
  file: the only non-tree pieces overlapping the west outline and not in its group are the 2 refused shells.
- **Way back, whole tree (09:47): main exe (b_off_rung, baked 09:29-09:37) vs this exe (b_off_fp), both with
  `--identity-join proximity --occluder-fit piece`: SAME, 233 files.** Red controls on copies: one flipped .lodi
  byte -> DIFF 1 of 233; the .lodo removed -> MISSING (file lists differ). Turn lock released 09:37:05.
- Skill ww-lodgen-landmark-add: footprint + pixel gate + north-first chunk rows added (both copies).

## 18. Third job: file-wide group ids + more than one box a landmark (started 09:50; written 09:59)
Ask (coordinator, bungo circled the violet strip in dc_fp.png): Diamond City must BE one group across the chunk
line, not only colour as one; group ids file-wide; and win back the coverage (0.558 -> at least 0.570).
- Measured for the width call: whole-Commonwealth bake b_fp2 holds 21,248 groups over 69,806 placements; the
  bound is the placement count (`--identity-join none`, the near library: one id a placement), already past
  65,535 here. **Call: a u32 group word, file-wide, dense from 0 (stride 4). Version 13.** 2 bytes a placement more.
- v13 = v12's layout with that one table changed; the ground stream becomes optional there. Way back:
  `WW_LODI_GROUPS_PER_CHUNK=1` writes the v7..v12 u16-per-chunk table and version, byte for byte.
- Readers: src/lodifile.cpp reads 13 (and still 3..12); a v7..v12 file's per-chunk ids are offset chunk by
  chunk into file-wide ids in memory, so every consumer sees one kind of id (viewer identity colour, cell
  identity index, dump stats). tests/spells/lodgen_native_decode.py the same. cellidentity keys on the id.
- Near library (src/nearlib.cpp): stays per chunk (groupPerChunk = true). Every one of its groups is one
  placement, no chunk line can cut one, and its bytes are another lane's gate. Call logged here.
- Occluder split (my call): a group wider than the cap (4,096 u; only a landmark can be) is cut by a 4,096 u
  world grid, by each member's box centre, and each square's members get their own box and carrier. No box is
  then wider than a group the cap allows. WW_LODI_OCC_SPLIT=<u> moves the square for measuring.
- Gate tools: pixgate.py now demands ONE file-wide id a landmark; lmchunk.py prints a FILE-WIDE verdict.
  **Red controls on the previous build (b_fp3, v12): pixel gate FAIL 49,904 wrong pixels, all Diamond City,
  all chunk (-2,-2) (the violet strip; named pieces split 14 / 151 over two ids); lmchunk: DC "2 id(s) over 2
  chunk(s) [NOT ONE ID]".**

## 19. Built and gated (build 10:00-10:02, turn lock released 10:01:59; written 10:08)
- Build: tools/ww_build.sh, BUILD-RC=0, exe newer than the 6 edited sources; the exe holds the new strings
  (WW_LODI_GROUPS_PER_CHUNK, WW_LODI_OCC_SPLIT, the split census). Run copy run_v13 (sha1 6ed1af11).
- LIGHT bake b_v13 (10:02-10:07, like b_fp3): .lodi version 13, stride 4, 4,527 groups over 46,532 placements
  (b_fp3: v12, 4,613 -- the 86 fewer are groups that crossed a chunk line and were counted once a side).
- **Pixel gate: PASS, 0 wrong pixels in all four outlines** (red control b_fp3: FAIL 49,904).
- **lmchunk: Diamond City FILE-WIDE 1 id (1178) over 2 chunks, 404 pieces, 0 other instances [ONE ID]**; both
  Hub towers and Trinity ONE ID. Census: "crosses a chunk line; the v13 file-wide group word keeps it one id".
  (red control b_fp3: 2 ids over 2 chunks, NOT ONE ID).
- **Poke gate (lodi_occluder_building.py --gate): ok, 517 boxes, 0 over 1 percent, worst 0.0041**; its floor
  (boxes grown 1.25x) goes red on 511 of 517.
- **Occluder split: 2 groups wider than 4,096 u (Diamond City, west Hub tower at 4,100 u) cut into 13 parts.
  Street coverage 0.5877** (b_fp3 0.5583, the no-split control): eyes Trinity 0.578 -> 0.587, Hub 0.730 ->
  0.760, Diamond City 0.368 -> 0.416. Target was >= 0.570: met without moving the square.
- Refusals for v13 (v13mut.py, re-signed CRCs so the rule answers): id at groupCount, an unused id,
  groupCount+1, stride 2, no group table -> all refused by the Python decoder; the re-signed control loads.
  The C++ reader on the same files: post4.sh (below).
- **Way back (10:07-10:16): proximity + piece + WW_LODI_GROUPS_PER_CHUNK=1 (b_off_v13pc) vs main exe
  (b_off_rung): SAME, 233 files** (cmp_off_v13pc.out; .lodi version 12, stride 2, 20,954 groups).
- **Way back WITHOUT the switch (b_off_v13, proximity + piece, 10:16-10:26) vs b_off_rung: DIFF 2 of 233.**
  The .lodb differs only in its `product Commonwealth.lodi` digest. The .lodi (lodidiff.py, lodidiff_off_v13.out):
  version 12 -> 13; groupStride 2 -> 4; the group table 139,612 -> 279,224 bytes (u16 -> u32); groupCount
  20,954 -> 20,818; offVertexSky and offVertexGround +139,264 (the wider table, 4096-aligned); fileBytes;
  indexCrc32 and headerCrc32. Every other table (chunks, cells, instances, cold, occluders, AO, sky, ground) is
  byte-identical. The group partition only merges: 0 old groups split, 127 new ids hold 2+ old ones (136
  folded), and every merge joins old groups of DIFFERENT chunks -- exactly the chunk-line groups.
- C++ reader (--native-verify, v13 exe, 10:26): the 5 v13mut files -> rc 1 each; the re-signed control, the
  b_v13 file and the OLD v12 file b_fp3 -> rc 0 (old files still load).
- **lodgen_native.sh (10:26-11:00, h_lodgen_native_v13.log): 34 checks, 1 failure = the same leg-5 .lodb ledger
  line that fails on IDENT1's and my earlier runs (not this change).** Leg 4 ok (the Sanctuary .lodi is v13,
  j0e: u32 stride 4, 1,997 groups). Leg 13 ok: 280 of 280 boxes hold. Leg 13c ok: 312 boxes, 0 over 1 percent,
  worst 0.0041, grown 1.25x -> 306 over.
- Pictures: waiting on the turn (TERRLIVE1 took it between my shots at 11:01).
- Pictures (11:01-11:49, the turn released after each shot; TERRLIVE1 baked in between): pics/dc_v13.png (one
  colour over both chunks, no violet strip), hub_v13.png, west_v13.png, westclose_v13.png (same cameras as the
  _fp set), and dc_v12load.png = the NEW exe drawing the OLD v12 bake b_fp3: it loads, and shows the old
  per-chunk strip, as that file's ids say.
- Commit 92feb2c4 (code, readers, doc, skill, WW_CHANGES), pushed; this section and DELIVERABLE_TEXT follow.
