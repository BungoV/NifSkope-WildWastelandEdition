# CELL1 deliverable (ledger text for the overseer)

Lane CELL1, branch cell1-20260926. The whole report is in DONE.md.

- **Chunk exported as one model.** Downtown Boston level-4 chunk -4,-8.
  - Full detail: 4,577,419 triangles and 765 materials, with the ground as its own part.
  - Authored LOD version: 24,996 triangles. The vanilla file 4.-4.-8.BTO has 24,929.
  - Tools: tools/cellexport (cell_export.py, esm_cells.py, nif_mesh.py, bl_shots.py, compare_glb.py) and the skill ww-chunk-model-export.
- **InstaLOD job folder is ready** in the session scratch at cell1_20260926\instalod_job\. It holds:
  - chunk.glb and chunk.obj with the textures;
  - four profiles: optimize_20k, optimize_5k, optimize_1k and remesh_far;
  - a 6-line HOWTO.txt.
  - Nothing launched InstaLOD or drove it. results\ is empty until bungo runs it.
- **What the authored LOD keeps** of the full-detail shape:
  - It keeps 39% of the surface area.
  - Original to LOD distance: 1.45 m on average; 90% of points are within 4.20 m.
  - Share of the outline kept: 82% from the oblique view, 85% from the west, 88% from far away, 90% from the south.
- **Fallout 76 (measured only).** Charleston level-4 LOD chunks have 95k-136k triangles in 74-132 pieces.
  - Big buildings are remeshed one by one, each with its own baked texture.
  - Small objects are merged into one remesh.
  - The rest share world-wide texture arrays.
  - Trees are included as 3D models.
- **Pictures.** 7 renders plus silhouette.txt, in scratchpad/cell1_20260926/pics/ (not committed).
- **Open items:**
  - Once bungo drops result_<profile>.glb files, run compare_glb.py on them and render them into the same pictures.
  - Brick textures that use a colour palette are shown grey, because the palette is not applied yet.
