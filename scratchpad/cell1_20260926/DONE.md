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

## 4. Fallout 76 measurements
(in progress)

## 5. Pictures
(pending)

## 6. What a bridge would need
(pending)

## 7. Skills review
(pending)
