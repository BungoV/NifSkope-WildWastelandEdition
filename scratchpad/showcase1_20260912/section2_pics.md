
---

## 2. Pictures

Eight files, all in `E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912/images/`.
Every one is built by a script in `pics/` beside it, so a caption number and a
number in this report come out of one piece of arithmetic (`pics/common.py`).
Open `6_contact_sheet.png` first if you want the set in one frame.

| # | file | size | what it is |
|---|---|---|---|
| 1a | `1a_sheets_colour.png` | 1576 x 1230 | the far-terrain **colour** sheet for two chunks: ours ON, ours OFF, vanilla's own, side by side |
| 1b | `1b_sheets_msn.png` | 1576 x 1230 | the **normal** sheet (`_msn`) the same three ways -- the ON column is your cleaned 2K cache |
| 1c | `1c_sheets_mask.png` | 2060 x 1006 | the **mask** sheet (`_data`) and each of its four channels: R ambient occlusion, G wetness, B shore, A ground cover |
| 2 | `2_terrain_and_objects.png` | 1720 x 1962 | terrain **and** objects, three views (top, front, oblique) x three identity states (on, `--no-terrain-identity`, both off) |
| 3 | `3_far_rings_and_cards.png` | 1720 x 1962 | the far rings (dim 16, dim 32) top and oblique, a close-up, and the impostor card itself: the octahedral grid, one frame at 4x, and the packed sheet |
| 4 | `4_objects_only.png` | 1720 x 1334 | the near chunk's object file alone, three views, vanilla's own for scale, and the **eleven buildings** close up (top down and oblique) |
| 5 | `5_ao_greyscale.png` | 1600 x 1842 | the baked **AO in greyscale**, eight panels: the mask sheet's R channel, the terrain's vertex AO, the objects' vertex AO, the two together top-down and oblique, the sheet with the AO switched off for the difference, and the buildings + terrain AO close up both ways |
| 6 | `6_contact_sheet.png` | 1940 x 1080 | the index -- all seven above at thumbnail size |

**How the 3D panels were made, so nothing here is a claim you have to take on trust.**

* One render = one headless NifSkope launch through the WW render hook
  (`shot.sh`). `WW_RENDER_CLEAN=1` (no grid, no axes), `WW_RENDER_SS=1` (2x
  supersample, so a 1400x900 request writes a 2800x1730 PNG), window forced to
  the second monitor with `WW_WINDOW_AT=1960,40` and my own unused port 45973.
  Your own NifSkope window was never touched.
* Camera: `WW_RENDER_CENTER` pins the look-at and `WW_RENDER_ORTHO` the
  orthographic half-width, so every panel of a pair is the same projection.
  `$WW_CAMERA_CENSUS` is copied to `<shot>.png.cam` at the moment of the grab and
  the caption quotes **that**, not the arguments -- e.g. the oblique near panel
  reads `view=8 rot=-63.5593,0,133.3081 halfW=14000 vp=1400x865 upp=20.000000`.
* A `.BTR` and a `.BTO` are two files and the hook opens one, so terrain+objects
  panels are **composites**: the object render keyed over the terrain render off
  the viewport clear colour, same camera, look-at differing by exactly the chunk
  origin (-81920, 98304) because a `.BTR` is chunk-local and a `.BTO` worldspace.
  Top down the key is exact. Front and oblique draw every object over the terrain
  whatever the depth says, so a tree behind a ridge is still drawn -- every such
  caption says so.
* The greyscale AO panels are **not** a desaturated render. `WW_LOD_CHANNEL=3` is
  `v = C.bbb` in `res/shaders/fo4_default.frag:279`: the vertex colour B channel
  drawn flat, no texture, no lighting. The grey is the byte in the file.
* The resource-root gate: the same `.BTR`, two different `WW_LODGEN_RESOURCES`
  roots, produced different PNGs (1,218,003 B vs 926,255 B, not byte-identical),
  and the far ring with **no** root at all comes back solid magenta (243, 1.5,
  243) where the root gives grey (153.7, 151.6, 153.6). The pictures really are
  of our sheets.
