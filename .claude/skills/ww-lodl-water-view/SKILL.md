---
name: ww-lodl-water-view
description: Render and gate the far-LOD viewer's water (flat water surfaces from a .lodl, v3 bodies or v2 cells) - which switches draw it, how to prove it is flat, over the right ground, and that its legend matches the picture. Use for any water picture or water-view change in NifSkope WW.
---

# WW: the .lodl water view (lane WATER1, 2026-09-27)

## What draws water
`nifCreateLodtTerrainScene` (src/btdterrain.cpp) calls `addLodlWater()` after the terrain when:
- the plane is the default Height view, with no `WW_LODL_CHANNEL` and no `WW_LODL_AO=1` -> plain water
  (0.16,0.36,0.50) at alpha 0.60 (NiAlphaProperty 4333);
- the plane is `waterheight | watertype | bodyid | flow | shore | cellflags` -> the plane's colour ON the
  water (opaque) and the ground goes back to the Height view (lit when sheets are found).
`WW_LODL_WATER=0` draws none: the pre-WATER1 picture, byte for byte. Harnesses that measure terrain or
objects export it (lodl_open, native_open, native_lighting, lodl_channels, lodi_v7).

v3 file: one quad run per wet body-ID texel at the body table's height (rate halved until the region is
<= 4M texels). v2 file: one sheet per cell whose water height is above the cell's minimum; body/flow/shore
say ABSENT.

## Read the notes, not the picture
The render log carries, in this order: `water (<view> view): N bodies of M in this region ...`,
`water bodies drawn, biggest first: ...`, `water flatness: largest height spread inside one body X units ...
largest distance from the body table's height Y` (read back from the built vertices: must be 0 / 0),
`ground above water: A of W wet texels ...` (file's full rate, then the view's mesh), and
`water legend (<view>): label = r,g,b; ...`.

## Gates
1. Identity: rung exe vs new exe with `WW_LODL_WATER=0`, same camera -> PNG sha1 equal.
2. Legend: render with `WW_RENDER_FLAT=1` (vertex colour = pixel); water pixels = diff against the same
   frame with `WW_LODL_WATER=0`; categorical views -> share within 3/255 of a swatch; ramps -> on the
   segment first..last swatch; flow -> on the hue wheel at the legend's brightness; default -> blend
   0.60*water + 0.40*ground. Floor: another view's legend must fail. Script:
   scratchpad/water1_20260927/legend_check.py.
3. Bake: `--no-water-bodies` == rung v2 sha1; default v3 == rung `--water-bodies` except flag bit 0
   cleared (scratchpad/water1_20260927/bake_cmp.py, red on a doctored copy first).

## Traps
- `turn.sh` with no arguments is `acquire anon`, not a status query. Read `.ns_turn/who` instead.
- The shot scripts need `LODL=` pointing at the v3 bake; the maps1 terrain folder's own .lodl is v2.
