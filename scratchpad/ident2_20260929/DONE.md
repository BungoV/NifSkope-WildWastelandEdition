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
