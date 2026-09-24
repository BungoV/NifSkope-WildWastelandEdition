# IMPOSTORSHRUB1 -- deliverable text (the overseer splices; lane edited none of HANDOFF / WW_CHANGES / MISTAKES)

Lane IMPOSTORSHRUB1, 2026-09-23, 07:45-09:17. Not committed.

## Verdict
No shrub, bush, sapling, hedge or undergrowth model bakes an empty card any more: 27 of 54 did, all for one reason, and all 27 bake now. Every other model bakes byte for byte as before.

## Exe
release/NifSkope.exe: 23,831,552 B, 2026-09-23 07:54:35, sha1 8d87c1559c91b91e2a87f28477e7c65d58b74b98.
Rung: release/NifSkope.before_impostorshrub1.exe, c172ba9de608d7d9c36ac2b1f870e7a47434fe02.
The copy that was in release/ before the link is at release/NifSkope.aside_impostorshrub1.exe (same bytes as the rung). The withdrawn first build is at scratchpad/impostorshrub1_20260923/NifSkope.pershape_withdrawn.exe.
Files changed: src/nifskope_ui.cpp (the bake hook's BSMeshLODTriShape rule, +45 lines, LF-only kept). New: tests/spells/impostor_shrubs.sh.

## HANDOFF line
IMPOSTORSHRUB1 (2026-09-23, exe 8d87c155 07:54:35, NOT committed): the octahedral card bake now keeps ONE mesh-LOD level per model, the lowest slot any BSMeshLODTriShape fills, instead of always the LOD0 slot. 27 of the 54 vanilla shrub/bush/sapling/hedge/undergrowth models ship no LOD0 part at all and baked EMPTY cards (0 texels, halfW 1.077). Now 0 empty; the other 27 are byte-identical, and so is the maple gate fixture. New gate tests/spells/impostor_shrubs.sh: PASS on the new exe, FAIL on the rung. Muddy shrub: the stage is the baked AO (-7% lit luma; AO off gives +1.4%); AO is ratified behaviour, so it was not changed and a ruling is OWED. Pictures: scratchpad/impostorshrub1_20260923/shrubs_contact_sheet.png.

## WW_CHANGES entry
**Shrub and bush cards no longer bake empty (IMPOSTORSHRUB1, 2026-09-23).**
- **The level.** The card bake photographs one mesh-LOD level for the whole model: the lowest slot that any BSMeshLODTriShape fills.
- **The slots.** Vanilla's exporter merges three parts into these shapes, and the shape names show which part goes where:
  - the full part (no prefix) goes into the LOD0 slot;
  - its `L1_` copy goes into LOD1;
  - its `L2_` copy goes into LOD2.
- **What was wrong.** 27 of the 54 shrub, bush, sapling, hedge and undergrowth models in Fallout4.esm have no full part. Examples:
  - HollyShrub01 `L1_HollyShrubSmall01` 0/302/0;
  - DeadShrub04 `L2_DeadShrub04` 0/0/158;
  - every shape of ShrubGroupLarge05 is 0/n/0.

  Keeping LOD0 left zero triangles to draw, so each of those models baked an empty card.
- **What they bake now.**
  - 26 bake their `L1_` level.
  - DeadShrub04 bakes its `L2_` level.
  - The sidecar says so with a `rangekept LOD<k>` line.
- **Why per model and not per shape.** TreeMapleblasted05 carries its full tree in one shape (141/0/42) and its `L1_` copy alone in another (0/75/5). A per-shape rule would draw both on top of each other.
- **Unchanged models.** A model with any LOD0 part bakes exactly as before. This was measured:
  - all 27 such census sheets are byte-identical;
  - the IMPOSTORTEAR1 maple fixture's 12 files and sidecar are byte-identical.
- **The gate.** `tests/spells/impostor_shrubs.sh` builds its model list from the plugin, not a typed list. It bakes each model at N8 / TILE 512 / REF 1326.5 and checks four rows:
  - S1 floor: at least 50 models;
  - S2: every model baked;
  - S3: no empty card;
  - S4: `rangekept` is present exactly where it is owed.
- **Way back.** None is added on purpose. The old behaviour is empty cards, and the rung exe is the red control.

## docs/LODGEN_IMPOSTOR_SPEC.md suggestion (Source paragraph, ~line 205)
Replace "and a `BSMeshLODTriShape`, whose triangle list is [full][L1][L2] (the maple: 71 + 23 + 8) and which the viewer draws whole at its default level, is reduced to its first range for the bake" with the text below:

"A `BSMeshLODTriShape`, whose triangle list is [full][L1][L2] (the maple: 71 + 23 + 8) and which the viewer draws whole at its default level, is reduced to ONE level for the bake. That level is the lowest slot any such shape of the model fills, chosen per model, never per shape (IMPOSTORSHRUB1: 27 of 54 vanilla shrubs have no full part; the maple has its L1 copy alone in a second shape). A later level that served is named by a `rangekept LOD<k>` line."

## Census (job 1)
- **Population.** Fallout4.esm bases whose model path or editor id names shrub, bush, sapling, undergrowth or hedge, with the look-behinds excluding Ambush and TrashEdge.
  - 60 bases: 58 STAT, 1 MSTT, 1 ACTI.
  - 55 distinct models, 54 of them on disk.
  - None carries an MNAM slot. This is why none is returned by `--list-impostor-candidates`: `--candidates all` is retired, and neither `trees` nor `missing` lists a shrub.
  - 50 bases are placed in the Commonwealth, 36,672 placements in all.
  - HedgeRow05 (MSTT) is missing from the corpus and is placed nowhere.
- **Rung result.** 27 empty, 27 fine. The 27 empty models:
  - DeadShrub01-06 and DeadShrub01Obscurance;
  - HedgeRow01-04;
  - HollyShrub01-04;
  - ShrubGroupLarge04/05, ShrubGroupMedium02/03, ShrubGroupSmall01;
  - VineShrub01;
  - Sapling01-04;
  - TreeElmUndergrowth01/02.

  Together they have 29,988 placements, against 6,684 for the fine ones.
- **One named cause for the one class.** Every BSMeshLODTriShape of these models ships the LOD0 slot empty, and the bake kept only LOD0. The `_L<digit>` name rule is not the cause: it hid 0 shapes in every census model.
- **New exe.** 0 empty. The table is census_new.tsv, and the rung table is census_rung.tsv.
- **Correction to the lane brief.** The brief listed three empty models, and I first named PrivetHedgePostWar01-03's `L1_HedgeVines01` (0/442/0) as a lost part. Under the per-model rule that shape is correctly left out: those models have a full LOD0 part, and the vines exist only at the L1 level.

## Muddy (job 3), FoothillsShrubLarge01
The brief's "shrub05" was ShrubGroupLarge05, which was empty. The shrub that did bake in IMPOSTORTEAR1's sheet is FoothillsShrubLarge01. Both views, el 0 and 20, were measured on the intersection of the two silhouettes, in muddy/.

| stage | numbers | verdict |
|---|---|---|
| colour sheet | diffuse texels kept by the alpha test: luma 77.8, sat 0.28. Sheet (alpha 255): luma 88.3, sat 0.20 | clean. The texture really is brown |
| mips | DDS mip 0-3 luma 94.6 / 94.0 / 94.0 / 94.2 | clean |
| unlit render | card/mesh luma 0.977; card sat 0.194 vs mesh 0.176 | clean |
| lit render | card/mesh 0.931 | 7% dark |
| lit, AO off | card/mesh 1.014 | the AO is the whole gap |

- **The stage is the baked AO.** The gsaos B channel on drawn texels has mean 0.79, and 13% of them are below 0.5 at mip 0. The shader multiplies it into the ambient.
- **Why it was not changed.** The AO is ratified behaviour: bungo 2026-09-19, res/shaders/impostor_oct.frag:29. A ruling is OWED on whether shrubs should carry it, or carry less of it.
- **The other half of the complaint is the old picture itself.** IMPOSTORTEAR1's model column drew the untrimmed NIF, full part plus L1 copy on top. That gives 41% more ink (30,862 vs 21,883 px) and paler colour (sat 0.166). The new pictures trim the model to the level the card baked (trim_level.py).

## Gates (job 4)
- **impostor_shrubs.sh (new).**
  - New exe: S1 ok (54), S2 54/54, S3 0 empty, S4 27 of 27 with 0 wrong. RESULT PASS.
  - Rung: S3 FAIL (27 empty), S4 FAIL (0 of 27). RESULT FAIL.
  - Outputs: gate_shrubs_new.txt, gate_shrubs_rung.txt.
- **impostor_draw.sh.** Run on the maple fixture rebaked with this exe, which is byte-identical to the rung's bake. 32 steps, 0 failures:
  - row 5 IoU 0.5147;
  - row 17 torn share 0.1135 against the 0.1340 bar;
  - row 18 at 0.32 / 0.51.

  These are the same numbers IMPOSTORTEAR1 published.
- **lodgen_octahedral.sh.** 116 ok, 0 FAIL. PASS.
- **native_lighting.sh.** 21 checks, 2 failures. These are the 2 pre-existing legacy_btr byte-identity reds.

## Pictures (job 5)
E:\Projects\NifskopeWildWastelandEdition\scratchpad\impostorshrub1_20260923\shrubs_contact_sheet.png
- **Layout.** 54 rows (every census model on disk, no rocks). Four columns: 3D model | Octahedral impostor at elevation 0, then the same pair at elevation 20, all at azimuth 30.
- **Settings.** N8, TILE 512, REF 1326.5 size ladder.
- **Labels.** Rows that were empty say "was EMPTY; now bakes LOD1/LOD2".
- **Model column.** Trimmed to the level the card baked.

Residuals the sheet shows. I did not chase these; they are for bungo's eye.
- Small shrubs land on 64 px frames under the size ladder. Their cards read as soft blobs, and thin-twig dead shrubs lose their outer twigs. For example, the DeadShrub01 card is visibly narrower than its twig spray.
- At elevation 20 several card edges break into scattered texel islands (cedar01, cedar02, hollyshrub01prewar). This may be the noise or stipple class bungo named as the defect to avoid. It is unmeasured here.
- The viewer's own LOD menu semantics are open. "LOD 0" draws LOD0+LOD1+LOD2 overlapped, while the shape names say the slots are alternatives. This is out of scope and is named only as a question.

## Mistakes
1. **Built before reading the shape names.** The first fix kept the first non-empty range per SHAPE, and I built it (9862d09a) before reading the shape names. The names show the slots are alternative copies. A per-shape rule would have drawn TreeMapleblasted05's L1 copy over its full tree. It was withdrawn before any bake, at the cost of one build. Rule: read what the slots hold (names, boxes) across the gate subjects before choosing a per-shape or per-model rule.
2. **Orphan loop after TaskStop.** TaskStop killed the gate chain's shell but not its child `bash impostor_shrubs.sh`. That orphan loop kept launching wedged bakes for 40 minutes on the same ports the rerun used. One model in each of the next two runs launched into a bound port and wrote nothing. Rule: after stopping a background chain, `ps -ef | grep <spell>` and end the orphans before rerunning. Rerun on distinct PORTBASEs.
3. **CRLF model list.** The spell's model list was written in Windows text mode (CRLF). The CR rode into the mesh path, and the bake window waited 15 minutes on a missing file. Fixed in the spell (newline='\n' and `tr -d '\r'`). Rule: any Python list that a bash loop reads is written with newline='\n'.
4. **Card textures not copied.** The first muddy harness run forgot to copy the card DDS under a textures\ tree, so the card drew blank. The harness logged "draw REFUSED". I read the log before reading the numbers, so the cost was one rerun.
5. **ww_build.sh rename collision.** tools/ww_build.sh renames a held exe to NifSkope_inuse_<pid>.exe. When bungo's window already runs FROM that name, the rename would collide with a locked file. I worked around it by renaming release/NifSkope.exe aside by hand first. This is a script fix for a later lane.

## Skill review (constitution 1a)
- **nifskope-ww-lodgen (used).** Two additions for it:
  - TaskStop does not reap grandchildren, so check `ps` for orphan spell loops before rerunning.
  - A Python list read by a bash `while read` loop must be written with newline='\n' on Windows.
- **ww-reference-card-diagnose (used).** Its shading arm was the method here: albedo channel, then lit, then AO off, measured on the silhouette intersection. The AO-off control split the stage in one run. One addition: trim the MESH to the level the card baked, because a BSMeshLODTriShape model drawn untrimmed has up to 41% extra ink and paler colour.
- **nifskope-ww-build-verify.** Used through tools/ww_build.sh. The skill itself was not reloaded after compaction. Add the NifSkope_inuse_<pid>.exe collision (Mistake 5).
- **nifskope-ww-render-shot / ww-test-harness-add.** Not loaded this lane. The orbit harness pattern was taken from IMPOSTORTEAR1's tear1_run.sh, and the new spell follows the house shape: derived population, floor row, rung-red. Whether it matches ww-test-harness-add's checklist line by line is unverified.
- **New repeatable procedure.** Bake, compress and shoot any NIF as a card pair with the model trimmed to the baked level. Scripts: shrub_bake.sh + shrub_pics.sh + trim_level.py + contact.py. This is a skill candidate ("ww-card-contact-sheet"). It was not written this lane.
