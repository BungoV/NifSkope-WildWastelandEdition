---
name: ww-chunk-model-export
description: Export one Fallout 4 object-LOD chunk (or any cell rectangle) as a single glTF/OBJ model -- every placed object at full detail, or the authored LOD models our bake uses -- with the ground as its own part, then render it headless in Blender from fixed cameras and measure a simplified copy against it (triangles, bounds, two-way surface distance, open edges, outline kept). Use when a LOD question needs the chunk as one model for an outside tool, a side-by-side picture of full detail against LOD, or a number for how much of the shape a LOD keeps.
---

# Exporting a chunk as one model (tools/cellexport)

Everything here is Python + Blender; no NifSkope build, no headless NifSkope turn needed.
Game data is read in place and every output goes to scratch -- never commit a .glb/.obj/.png
(the repo .gitignore does NOT ignore .glb/.obj).

## 1. Export

```
python tools/cellexport/cell_export.py <Fallout4.esm> Commonwealth x0 y0 x1 y1 <DataUnpacked/Data> <scratch>/chunk_full \
    --library near --ground Textures/Terrain/Commonwealth/Commonwealth.4.<x0>.<y0>.DDS --tex-dir <scratch>/textures --cache <scratch>/fo4.pkl [--obj]
```
- A level-4 chunk (x0,y0) covers cells x0..x0+3, y0..y0+3.
- `--library near` = full-detail models; `--library lod0` = the level-4 authored LOD slot
  (empty slot = object dropped, as the bake does); `lod1..lod3` = levels 8/16/32.
- `--library none` = ground only; `--origin=X,Y` (with the `=`: negative values) makes several
  exports line up, e.g. the 8 neighbouring chunks' ground as context for a far view.
- `--ground` may be relative to the data root; the census line "ground texture missing" must be 0
  (a missing texture renders the ground white).
- `--cache` pickles the ESM walk (first run about a minute, then seconds).
- Read `<stem>.census.txt`: placements drawn/dropped, missing/unreadable models, material swaps,
  triangles/vertices/materials, textures missing. Treat every nonzero "missing" as a finding.
- Cross-check an authored-LOD export against the vanilla .BTO of the same chunk
  (`nif_mesh.read(...)` triangle count): chunk -4,-8 gave 24,996 vs 24,929.

Known limits: grayscale-to-palette materials are written grey (palette row not read); effect-shader
shapes are dropped; BC5 textures are not decoded by Pillow 10.

## 2. Render (Blender 4.5 headless)

```
"E:/Tools/3D/Blender 4.5/blender.exe" -b --factory-startup -P tools/cellexport/bl_shots.py -- spec.json
```
spec: `W, H, out_dir, shade (texture|clay|mask), models[{label, files, drop:["Ground"]}], cams[{name, spec}], context[files]`.
- Camera specs are game units relative to the export origin, z absolute:
  `persp:ex,ey,ez:tx,ty,tz:fovDeg` or `ortho:...:widthUnits`. Put far-view cameras above the
  terrain at that spot (read LAND heights from the cache) -- Boston's south ring is 2,500-3,000 high.
- EEVEE_NEXT (Workbench ignores the vertex colours FO4 tints bricks with).
- `clay` assigns one grey material to every slot (the view-layer override is not honoured);
  `mask` writes alpha-only outlines, context not drawn.
- Blender merges faces that repeat the same three vertices: its triangle count is lower than the
  census by exactly that number (2,286 in chunk -4,-8). Quote the census, not Blender.

## 3. Measure a simplified copy

```
python tools/cellexport/compare_glb.py <full objects-only .glb> <candidate.glb> ...
```
Nodes named Ground are skipped. Prints triangles, materials, UVs kept, bounds shift, area, two-way
surface distance in metres (new->original = strays, original->new = lost) with the sample-spacing
floor, and open edges (the reference is open by design; only the change means anything).
Outline kept: render `mask` shots of the reference and the candidate from the same cameras and
compare alpha (kept / extra / overlap); repeated runs vary by about 0.5 percentage points.
