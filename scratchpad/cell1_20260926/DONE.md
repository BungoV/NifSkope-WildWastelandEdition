# Lane CELL1 -- one LOD chunk as a model, InstaLOD job folder, Fallout 76 comparison

Worktree E:\Projects\NifskopeWWE-cell1, branch cell1-20260926 (from 422881d4). Report written incrementally.
Session scratch (S below) = C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\cell1_20260926

## 1. Skills loaded
- nifskope-ww-lodgen, nifskope-ww-gltf-character-export (repo tree), CONSTITUTION + HANDOFF top block read.
- (render-shot, ww-texel-picture: see section 7)

## 2. The export

Chunk: level-4 object-LOD chunk (-4,-8) of Commonwealth = cells x -4..-1, y -8..-5, downtown Boston
with the Diamond City stadium. Its vanilla LOD files are Commonwealth.4.-4.-8.BTO / .BTR / .DDS.

Tool: tools/cellexport/ (new, generic, no game data):
- esm_cells.py -- one walk over Fallout4.esm: every REFR in the rectangle (deleted and
  initially-disabled skipped), SCOL parts expanded, MSWP swaps kept, LAND heights; markers\ and
  effects\ models skipped. Placement = lodgen.cpp's convention (fromEuler(-x,-y,-z), scale).
- nif_mesh.py -- FO4 NIF reader (vector decode of BSTriShape by the vertex descriptor, hidden
  and EditorMarker branches skipped, BSMeshLODTriShape = its LOD0 range) + BGSM/BGEM header read.
- cell_export.py -- writes .glb (and .obj/.mtl with --obj): part "Objects" (one primitive per
  texture/alpha/two-sided key) and part "Ground" (the LAND grid, vanilla terrain texture).
  --library near = full-detail models; --library lod0..lod3 = the STAT's authored LOD slot
  (level 4 = slot 0; an empty slot drops the object, as our bake does). DDS -> PNG (max 512 px).
  Y-up metres, origin = chunk centre (-8192, -24576, 0) game units, written in asset.extras.

Counts (from the tool's own census, S\export\*.census.txt):

| model | placements drawn | triangles | vertices | materials | file |
|---|---|---|---|---|---|
| full detail (chunk_full.glb) | 12,276 of 12,277 | 4,577,419 objects + 32,768 ground | 5,987,339 + 16,641 | 765 | 550,560,204 bytes |
| authored LOD, level 4 (chunk_lod4.glb) | 3,186 (9,091 have no level-4 model) | 24,996 objects + 32,768 ground | 52,735 + 16,641 | 34 | 5,303,432 bytes |
| InstaLOD input (instalod_job\chunk.glb, no ground) | 12,276 | 4,577,419 | 5,987,339 | 765 | 549,566,804 bytes |
| same as .obj fallback (instalod_job\chunk.obj) | | | | | 777,109,978 bytes + 726 PNGs (271 MB) |

- Cross-check: the vanilla Commonwealth.4.-4.-8.BTO has 24,929 triangles, 52,573 vertices, 2
  shapes. Our authored-LOD export of the same chunk: 24,996 / 52,735. Difference 67 triangles (0.3%).
- Full model: 2,219 distinct models, 0 missing, 0 unreadable; 23,333 shapes drawn; 969
  effect-shader shapes dropped (glow cards, decals-by-effect); 3,233 material swaps applied;
  17 material files missing. Textures: 726 PNGs, 1 missing (gray.dds), 1 unreadable (a BC5 normal map).
- 32 materials use "grayscale to palette" (painted bricks). Their texture's green/blue channels
  are not colour, so the raw texture renders purple; they are written as plain grey (red channel).
  The palette colour itself is not applied: bricks show grey times vertex tint, not their paint.
- Export time: about 3.5 min for the full chunk, 16 s for the authored LOD one.
- Blender 4.5 imports chunk_full.glb in 77 s and counts 4,607,901 triangles (expected 4,610,187;
  the 2,286 difference not yet explained -- Blender drops degenerate faces on import, unmeasured).

## 3. InstaLOD runs

Job folder (scratch only): S\instalod_job\
- chunk.glb (primary), chunk.obj + chunk.mtl + textures\ (fallback)
- profiles\optimize_20k.json, optimize_5k.json, optimize_1k.json (Optimize, AbsoluteTriangles
  20000/5000/1000 for the whole scene, Deterministic true, ProtectBoundaries true, boundary and
  silhouette importance High), profiles\remesh_far.json (Remesh, at most 5,000 triangles, one
  baked 1024 texture page, colour + tangent normal). Written by hand; InstaLOD's own Templates
  were read for key names only.
- HOWTO.txt (6 lines), results\ (empty until bungo exports into it).
- Validation, ready for when results appear: S\compare_glb.py <reference.glb> <result.glb>...
  prints triangles, vertices, materials, textures, UVs kept, bounds shift, area, two-way surface
  distance (mean/median/p90/p99/max, metres) and open-edge count.
- InstaLOD was not launched or driven. Results: (none yet)

Baseline for judging results -- the authored LOD models measured the same way
(compare_glb.py, reference = chunk.glb objects, 2,000,000 sample points, floor 0.53 m):

| candidate | triangles | materials | area kept | new->original mean / p90 / max | original->new mean / p90 / p99 | open edges |
|---|---|---|---|---|---|---|
| authored LOD (level 4) | 24,996 | 34 | 39% | 0.32 / 0.53 / 9.55 m | 1.45 / 4.20 / 13.51 m | 10,964 (full: 597,829) |

Outline kept against full detail (alpha masks 1280x720, pics\silhouette.txt): far view 88%,
skyline from south 90%, from west 85%, oblique 82%; pixels outside the full outline 0.3-1.1%.
Two runs of the same shots differed by up to 0.5 points.

## 4. Fallout 76 measurements

Source: X:\...\Fallout76\Data holds only 28 mod files (no SeventySix archives), so the
fallback E:\SteamLibrary\steamapps\common\Fallout 76 Playtest\Data was used. Measure-only:
six files extracted to S\fo76\ (scratch), nothing in git.
Charleston located by the CELL records' EDIDs in SeventySix.esm (78 cells named Charleston*;
Capitol at -24..-22, -15..-14). Level-4 chunks there: x0 = -30/-26/-22, y0 = -17/-13.

| FO76 chunk (level 4) | triangles | shapes | remeshed shapes (tris) | of which "loose components" | authored LOD on shared atlas (tris) | file |
|---|---|---|---|---|---|---|
| -30,-17 | 95,599 | 74 | 32 (40,247) | 32,293 | 42 (55,352) | 5.0 MB |
| -30,-13 | 125,415 | 107 | 60 (112,520) | 77,629 | 39 (12,879) | 5.5 MB |
| -26,-17 (Capitol) | 134,349 | 132 | 91 (100,750) | 37,589 | 37 (33,591) | 6.0 MB |
| -26,-13 | 106,398 | 82 | 33 (64,338) | 43,455 | 44 (42,050) | 5.2 MB |
| -22,-17 | 112,822 | 87 | 33 (65,721) | 48,971 | 46 (47,085) | 5.7 MB |
| -22,-13 | 136,112 | 90 | 46 (108,981) | 96,595 | 44 (27,131) | 6.0 MB |
| FO4 -4,-8 (vanilla, for scale) | 24,929 | 2 | -- | -- | 1 atlas | 1.2 MB |

What that says (from the shape names and texture paths inside the files):
- Not one merged mesh: 74-132 shapes per chunk, named `RemeshedShape_<building>_...`,
  `RemeshedShape_LooseComponents_Appalachia` and `GlobalAtlasShape_{NotAlphaTested,AlphaTested,Translucent}`.
- So FO76 remeshes per building (each with its own baked texture, e.g.
  `bld_a_brick_building_ruins01_<id>_lod_0_d.dds`; 57 of them in the Capitol chunk), remeshes
  all small loose objects of a chunk together into one shape, and keeps authored LOD pieces on
  a world-shared set of texture arrays (textures/lod/generated/texturearrays/..., 404 diffuse
  layers, UVs tile from -23 to 19) -- not one atlas per chunk.
- The chunk also carries 3D trees (alpha-tested, 15,798 of 134,349 triangles in the Capitol chunk).
- Budget: 4-5 times the FO4 chunk's triangles (95k-136k vs 25k).
- Silhouette kept: not measurable -- FO76's full-detail source for these chunks was not rebuilt
  (would need its REFR walk + precombined meshes). The clay pictures show the Capitol dome and
  buildings as recognisable shapes, trees as crumpled low-poly blobs.

## 5. Pictures

All in E:\Projects\NifskopeWWE-cell1\scratchpad\cell1_20260926\pics\ (gitignored). Same camera per
subject; labels carry the triangle count and outline kept.
- 1_oblique_side_by_side.png -- full detail | authored LOD, textured, from above south-west.
- 2_far_view_side_by_side.png -- from about 21,000 units (5 cells) away, north-east, 70 degree
  view, standing 2,400 high, with the 8 surrounding chunks' terrain for context (no water, no
  neighbouring buildings).
- 3_far_view_zoom3x_side_by_side.png -- the same far view, centre enlarged 3x.
- 4_skyline_from_south_side_by_side.png -- orthographic, from the south.
- 5_top_down_side_by_side.png -- orthographic, from above.
- 6_fo76_vs_fo4_oblique_clay.png -- untextured: FO4 full | FO4 authored LOD | FO4 vanilla .BTO |
  FO76 Charleston Capitol chunk -26,-17.
- 7_fo76_vs_fo4_skyline_clay.png -- the same four, from the south.
- silhouette.txt -- outline numbers.
InstaLOD panels get added to 1-5 when result_*.glb files appear (compose.py takes them as
extra labels).

## 6. What a bridge would need

1. Per-chunk export: done (tools/cellexport). A dense chunk is 4.6M triangles, 550 MB, 3.5 min.
2. InstaLOD has no command line here and must not be scripted from the repo, so a whole map is
   one Start per profile per chunk by hand. A bridge is therefore a drop folder: bungo exports
   result_<profile>.glb, our side validates (compare_glb + outline masks) and converts.
3. Converting back needs: glb -> our LOD format (the .lodl / .BTO writer takes triangles in world
   space; the glb origin is written in asset.extras, and the file name must also carry the chunk id
   because an outside tool may drop extras -- the bounds check catches a lost origin).
4. Textures: Optimize keeps the 765 source materials -- the bake needs one atlas page per chunk,
   so an Optimize result still needs our atlas pass; Remesh gives one baked page directly.
5. Seams: ProtectBoundaries holds open edges of the meshes, not the chunk cut line; neighbouring
   chunks simplified separately will not meet unless the cut edges are locked.
6. FO76's own answer is per building, not per chunk: remesh each building with its own texture,
   remesh the loose small objects together, keep authored LOD pieces as they are. That split maps
   onto our data (authored LOD slot present -> keep it; absent -> remesh that object).

Mistakes (for the director to splice into MISTAKES.md):
- Ran a grep over E:\Projects\NifskopeWWE-*\scratchpad (many worktrees at once) for a word; it
  timed out and was stopped. Search-lean rule broken. Rule: one folder per search.
- Passed the ground texture as a path relative to the data root; the tool only tried it relative
  to the working folder, drew the ground untextured, and the census said nothing. Caught by eye in
  the first picture. Fixed: the path is tried under the data root and the census now prints
  "ground texture missing".
- First far-view camera stood under the terrain (2,500 high where the ground is 2,600-3,000):
  camera heights were typed without reading LAND. Caught by eye; moved to a measured spot.
- Assumed Blender's view-layer material override works in EEVEE; the clay renders came out
  textured. Now every material slot is assigned directly.

## 7. Skills review
- Loaded: nifskope-ww-lodgen, nifskope-ww-gltf-character-export (CONSTITUTION + HANDOFF top block).
  nifskope-ww-render-shot was not needed: no NifSkope render in this lane (Blender only).
- Wished for: a skill that says where each game's archives live on this machine and that the
  X: Fallout76 folder holds only mods; a skill for reading FO76 (BS 155) NIFs (the header has an
  extra 4 bytes, BSTriShape has a 24-byte box, BSLightingShaderProperty has no shader type).
- Written: .claude/skills/ww-chunk-model-export/SKILL.md (export a chunk as one model, render it
  headless, measure a simplified copy). Should also be copied to E:\Tools\AISkills -- left to the
  overseer (outside this worktree).
