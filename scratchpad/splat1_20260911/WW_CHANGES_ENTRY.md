# WW_CHANGES.md entry text (delivered, NOT spliced by the lane)

## 2026-09-11 -- lane SPLAT1: the landscape textures are baked 6x too large (MEASUREMENT ONLY, NO CODE)

bungo asked whether the far-terrain bake samples the landscape textures at
their correct scale. It does not.

* `src/lodgen.cpp` tiles every landscape texture at **2,048 world units a
  repeat** (`TILE`, 6335 and 7623). The engine's own repeat is **341.3333** --
  `fLandTextureTilingMult` = 1.5 in Fallout4.exe 1.10.155, `uv = index * mult/4
  = 0.375` over the 17x17 quadrant grid at 128 units a vertex. The bake is
  **6.0000x** too coarse, and the constant's own comment had always said its
  calibration was open.
* The MIP SELECTION is not at fault: it already picks the footprint-matched mip
  (5.00 for all 20 layer textures, every one 2048x2048 with 12 mips), and an
  exact box mean over the footprint is 0.50 units SMOOTHER, not 52.
* Measured on `Commonwealth.4.-20.24` and `.4.-20.20`, 3x3 local variance of
  luminance: ours **76.22 / 75.26** against vanilla's **19.81 / 29.39**, a codec
  floor of 1.52. Re-baked offline at 341.3333 the same tiles read **12.93 /
  14.60** -- below vanilla.
* Ruled out with their own numbers: the grass tint (the tile with the LARGER
  excess has no cover plane at all, so the tint touched none of it), VCLR (0.02
  and 0.05 of a 52 and 32 excess), the BC1 codec (1.52), the 17x17 blend (0 on
  one tile, 33% on the other, and it goes with the tiling).
* Bethesda's sheet carries **no landscape-texture pattern at any of six
  repeats** tested -- every correlation inside its phase-twin floor, with the
  same correlation reading +0.91 / +0.88 against our own sheet, so the negative
  is controlled.
* **The tiling fix does NOT close the whole-tile colour error** (16.9 -> 15.0
  and 21.7 -> 20.2 of 255). That is the grading, still open.

Instruments, logs and the picture: `scratchpad/splat1_20260911/`;
report `scratchpad/lane_splat1_report.md`; picture
`scratchpad/splat1_20260911/images/speckle_diagnosis.png`.
**NO CODE, NO DOCUMENT AND NO BAKE WAS CHANGED** -- the fix is BUILD PENDING,
resume `scratchpad/splat1_20260911/PENDING.md`.
