# Lane AUDIT1

## HANDOFF
A read-only audit of the MAPS1 bake (lane AO2 decal round, branch ao2-20260926, E:\Projects\NifskopeWWE-bake2) against the plan, judging every baked LOD map on what it must be vs what it is now, answering 8 named points (ground contact, identity map, identityraw, placement, ground normal, cell height range, water, ground cover). Verdict: the big problems are water is never drawn as water and the default v2 bake cannot tell water from land (has-water flag set on all 36,864 cells; v3 bodies correct but opt-in), ground contact stored as one grey per piece instead of per vertex (mean diff 0.14, corr 0.99993 over 11,416 pieces), and building identity cut at chunk lines and welded into streets. Files: `scratchpad\audit1\pics\A1_ground_contact_as_stored.png`, `A2_ground_contact_per_vertex.png`, `A3_water_as_flat_surfaces.png`, plus aud_ground.json / aud_occ.json / aud_arrays.json / aud_cam.json. Left: none of the fixes were made; they are handed off as view changes, bake switches, bake code, FO4CS-side, and format changes.

## WW_CHANGES
- none (research/audit only)

## MISTAKES
- 2026-09-27 AUDIT1: ran one Bash heredoc (`python - <<'EOF'`) for a camera check that hung, then redid it as aud_cam2.py; later ran two empty/no-op heredoc or `python -c` lines by mistake. Rule: patch scripts through the Write tool, never a heredoc or `python -c`.
- 2026-09-27 AUDIT1: the brief pointed the FO4CS reader at wt-fixfirst, but READER1 lives in wt-telem1 (deviation 2, not a lane error).

# Lane MAPS1

## HANDOFF
Rendered one picture of every map the LOD bake produces (79 panels, 11 sections) into a contact sheet at the same camera as 08_roads_AO_decal, plus offline top-down and sheet-space decodes for channels the renderer cannot isolate. Verdict: most channels RIGHT; key numbers - sky 844,791 px, ao 1,158,680 px (mean 172.4), ground mean 29.852 over 11,416 placements, terrain mask R 172.271 / G 0 / B 220.781 / A 31.504 over 6,553,600 texels, msn slope corr 0.80 / 0.83; emissive, scrappable, groundcover, and v3 water planes on the v2 .lodl are EMPTY/ABSENT (0 px vs default). Files: `maps1\contact_sheet.png` (3676x14155), `maps1\contact_sheet_half.png`, every panel in `maps1\pics\`, `maps1\water\`. Left: DONE 2026-09-27; fixes (view changes, bake code) handed off.

## WW_CHANGES
- none (research/audit only)

## MISTAKES
- 2026-09-27 MAPS1: patched make_sheet.py once through a python heredoc (the brief says Write/Edit only); a caption and legend-label change. Rule: patch scripts through the Write tool, never a heredoc or `python -c`.
- 2026-09-27 MAPS1: refute_terrain_sky.py and orient_check.py took the object-panel background from pixel [2,2], which is a roof (127,128,255), so "covered" read 95% instead of 37.7% and the first sky caption was an artefact. Rule: sample the intended background colour (BG 40,40,44), not an object pixel.

# Lane VAN1

## HANDOFF
Rendered vanilla's shipped far LOD at the camera of bungo's 08 Boston pictures by merging the 9 vanilla dim-4 chunks covering cells -8..3 x -12..-1 into one NIF (18 .BTR shapes + .BTO), then clipped vanilla to our ground (Sutherland-Hodgman on the terrain, owner-placement rule on objects) to make equal ground. Verdict: same camera, `.cam.log` byte-identical to 08_roads_AO_decal.cam.log; mask match IoU 98.46% (vanilla inside ours 99.56%, ours inside vanilla 98.89%; the first unclipped render scored 67.26%); measured lum vanilla 106.2 vs ours AO on 98.4 vs ours AO off 117.1 over 1,899,746 px. Files: `pics\vanilla_08.png`, `pics\vanilla_08_nowater.png`, `pics\ours_AO_repro.png` (byte-identical control), `pics\ours_AO_wide.png`, `pics\ours_AOoff_wide.png`, compositions `VAN1_*.png`, `pics\mask_clip_vs_08.png`. Left: named differences (AO, water, terrain, objects) documented, not neutralised.

## WW_CHANGES
- none (research/audit only)

## MISTAKES
- 2026-09-27 VAN1: to match areas, grew OURS (re-rendered with the region widened to vanilla's 9 chunks) instead of clipping vanilla to our area; bungo wanted equal ground with his pictures unchanged. Rule now in skill nifskope-ww-vanilla-compare s8 step 6: clip the reference to the subject's area, read the subject's true drawn box from its log, and prove it with a covered-pixel mask match.

# Lane CELL1

## HANDOFF
No report exists - only `CELL1_brief.md` and `CELL1_folder_list.txt`. The brief asked for: exporting one downtown Boston unit (object-LOD chunk holding the ballpark or two towers, Commonwealth level 4, cells -5..2 x -10..-3) as one model at full detail with ground as its own named part, plus the same chunk as the current bake draws it (authored LOD), with triangle/vertex/material counts and file sizes; preparing a scratch-only InstaLOD job folder (.glb + .obj, self-written profile JSONs for 20k/5k/1k triangle Optimize and a Remesh, a HOWTO.txt) for bungo to run in Studio himself; measuring Fallout 76's far chunk geometry; and screenshots. The folder list shows the export and job folders exist: `export\chunk_full.glb`, `chunk_lod4.glb`, and their `census.txt` files, and `instalod_job\` with `HOWTO.txt`, `chunk.glb`, `chunk.obj`, `chunk.mtl`, plus `profiles` and `results` subfolders (their contents are not listed). The external result - bungo's InstaLOD Studio runs and their import/validation - is still owed; no results are invented here.

## WW_CHANGES
- none (research/audit only)

## MISTAKES
- none recorded

--- STATS --- 20260929-002350-ef52 | opencode-go/deepseek-v4-pro | 66s | in 22913 (cache 9216) out 3713 | $0.0227 | 6 tool calls | E:\Tools\deepseek-bridge\jobs\20260929-002350-ef52
