
---

## 3. Pictures

Six panel sheets, written to
`scratchpad/nativeview2_20260912/images/`. Every one was composed by
`work/compose.py`, every label is BURNED INTO the image (the panel's title bar
and its one-line caption, plus the sheet's own title and note), every source
frame was taken with `WW_RENDER_CLEAN=1` so the background is the flat
(43,45,49) and not the viewer's gradient, and **every size below was read back
from the file with PIL** after it was written, not quoted from the request.

| path (under `scratchpad/nativeview2_20260912/images/`) | size, read back | bytes |
|---|---|---|
| `i_native_top_rung_vs_new.png` | 2164 x 718 | 1,647,084 |
| `i_native_obl_rung_vs_new.png` | 2164 x 718 | 692,137 |
| `ii_terrain_own_vs_flat_top.png` | 2164 x 718 | 1,567,773 |
| `ii_terrain_own_vs_flat_obl.png` | 2164 x 718 | 546,442 |
| `iii_slope_known_answer_oblique.png` | 2164 x 718 | 529,021 |
| `iv_slope_top_control.png` | 2164 x 718 | 1,063,183 |

**(i) NATIVEVIEW1's picture re-shot, four panels per view.** Native terrain and
objects together, at the top view and at the oblique, chunk (-20,24) dim 4,
orthographic half-width 8192. The four panels are, left to right: the LEGACY
`.BTR` control, the LEGACY `.BTO` control, the NATIVE scene on the rung exe, the
NATIVE scene on the new exe. The two control panels are byte-identical on both
exes, so they are shown once. The two native panels draw the SAME `.lodl`
terrain and the SAME `.lodi` objects from the same bake with the same camera —
only the exe differs.

**(ii) Terrain alone, its own normal tiles against FLAT tiles.** Four panels:
rung/own, rung/flat, new/own, new/flat, at each view. This is the picture of
the refuter. On the rung the first two panels are the same picture, which is the
defect stated as a photograph: a normal map that cannot change the render is a
normal map nobody is reading. On the new exe they differ, and the flat arm is
lit evenly.

**(iii) and (iv) the slope test.** (iii) is the oblique: the west tilt, the flat
sheet and the east tilt on the new exe, then the rung's east tilt as the
refuter, with each panel's mean luma burned in beside the `N.L` the arithmetic
predicted the ORDER from. (iv) is the top-view control, where the east and west
tilts must be the same picture and on the new exe are byte-identical, against
the rung where they differ by a mean of 47.06 luma.

**What the pictures are not.** They are 1024-wide frames scaled to 512 in the
panels, so they are for judging shape and shading, not for reading a pixel
value; every number in this report comes from the full-size PNGs in `work/`,
never from a panel. Per the charter, **nothing was sent to bungo from this
lane** — the director sends.
