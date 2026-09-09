# Lane IMAGES — the two mountain pictures owed to bungo

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, uncommitted.
Exe used: `release/NifSkope.exe`, mtime **2026-09-07 12:48:07**; newest file
under `src/` is `src/lodgen.cpp` at **2026-09-07 12:47:37** — the exe is newer
than every source file. `Fallout4.exe` was down for every run (`tasklist`
checked before the first exe launch); one NifSkope instance at a time; every
window placed by `WW_WINDOW_AT=1960,40` (second monitor).

Both images are **real renders through the render hook**, not texture-sheet
crops. The fallback in the brief was not needed.

---

## 1. Region chosen, and why

**Distance tile: `Commonwealth.16.-64.32`** — the dim-16 LOD chunk whose cells
are x −64..−49, y 32..47. That is the **north-west corner**, wholly outside the
painted box (x −36..32, y −41..32) on both axes.

**Close-up tile: `Commonwealth.4.-60.36`** — the dim-4 chunk at cells
x −60..−57, y 36..39, which sits inside the dim-16 tile above.

Both were picked by measurement on **vanilla's own shipped sheets only**
(`E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth`), never
on anything we generated — reading our own output to choose the tile would be
circular. Script: `scratchpad/mountains_20260907/images/pick_region.py`.

Two numbers per tile:

* `colour` = mean of (max(RGB) − min(RGB)) over mip 2 of the diffuse. A grey or
  blank tile scores ~0; painted rock and dirt score high.
* `tiltDeg` = mean angle of the `_msn` model-space normal away from up (up is
  GREEN in FO4), over mip 2. This is the relief.

Ranked by `colour × tiltDeg` over all **108** dim-16 chunks lying wholly
outside the painted box (`pick16.txt`):

| tile | score | colour | lumSD | lum | tiltDeg |
|---|---|---|---|---|---|
| **Commonwealth.16.-64.32** | **735.3** | 17.32 | 13.45 | 89.1 | **42.44** |
| Commonwealth.16.-64.48 | 716.8 | 17.65 | 13.17 | 86.0 | 40.61 |
| Commonwealth.16.-64.64 | 690.8 | 16.51 | 17.09 | 88.8 | 41.83 |
| … | | | | | |
| Commonwealth.16.48.-16 (last) | 174.5 | 12.63 | 3.50 | 65.7 | 13.82 |

`-64.32` is the top of that list: the most colour and the most relief of any
far chunk, and 4.2× the score of the flattest. Its 16 dim-4 children were then
ranked the same way (`pick4.txt`); `Commonwealth.4.-60.36` won at 711.9
(colour 18.69, tilt 38.09).

---

## 2. How OURS was generated

Two runs of this tree's own generator, one per tile. Exe as above; ESM is the
game's own master.

```
release/NifSkope.exe -no-gui lodgen \
  "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" \
  --worldspace 3C --terrain-region -64 32 -49 47 --dim 16 \
  --out-dir  scratchpad/mountains_20260907/images/ours16 \
  --tex-dir  scratchpad/mountains_20260907/images/ours16/textures/terrain/Commonwealth \
  --data-root "E:/Tools/Fallout 4/DataUnpacked/Data"

release/NifSkope.exe -no-gui lodgen \
  "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" \
  --worldspace 3C --terrain-region -60 36 -57 39 --dim 4 \
  --out-dir  scratchpad/mountains_20260907/images/ours4 \
  --tex-dir  scratchpad/mountains_20260907/images/ours4/textures/terrain/Commonwealth \
  --data-root "E:/Tools/Fallout 4/DataUnpacked/Data"
```

Both exited 0. Logs `gen16.log`, `gen4.log`. Each printed the same refusal in
words, which is the generator naming its own fallback rather than failing
silently:

```
bake -64,32: 0 px no land, 262144 px no base tex, 0 samples unresolvable LTEX []
bake -60,36: 0 px no land, 262144 px no base tex, 0 samples unresolvable LTEX []
```

All 262,144 texels of each 512×512 sheet have **no base texture**: the cells
carry height but no painted material, exactly the HANDOFF finding, and the
heights are all present (`0 px no land`).

### How the renderer is made to serve OUR sheets and not the game's

A `.btr` names its textures as `Data\Textures\Terrain\Commonwealth\<tile>.DDS`,
and ours names the same path vanilla's does. `NifModel::load` adds the folder
the `.btr` sits in as a resource path (`getNIFDataPath` → `addNIFResourcePath`,
`src/model/nifmodel.cpp:2147`) and `GameResources::find_file` consults that
path **before** the game's archives. So each side was staged as its own tiny
data root and rendered from there:

```
images/van16/Commonwealth.16.-64.32.BTR   + textures/terrain/Commonwealth/{,_msn}
images/ours16/Commonwealth.16.-64.32.BTR  + textures/terrain/Commonwealth/{,_msn,_data}
images/van4/Commonwealth.4.-60.36.BTR     + …
images/ours4/Commonwealth.4.-60.36.BTR    + …
```

### What the sheets measure before anything is rendered

Decoded from the DDS files themselves (`dds.py` + `bcnp.py`, mip 0):

| tile | sheet | side | format | mean RGB | luminance SD |
|---|---|---|---|---|---|
| 16.-64.32 | vanilla diffuse | 512² | BC3 | 97.3, 90.2, 79.8 | **16.01** |
| 16.-64.32 | ours diffuse | 512² | BC1 | 132.0, 130.0, 132.0 | **0.00** |
| 16.-64.32 | vanilla `_msn` | 512² | BC3 | 143.4, 220.2, 120.6 | 21.35 |
| 16.-64.32 | ours `_msn` | 512² | BC1 | 148.7, 238.5, 120.6 | 21.58 |
| 4.-60.36 | vanilla diffuse | 512² | BC3 | 101.7, 93.9, 83.0 | **14.87** |
| 4.-60.36 | ours diffuse | 512² | BC1 | 132.0, 130.0, 132.0 | **0.00** |
| 4.-60.36 | vanilla `_msn` | 512² | BC3 | 154.3, 226.5, 164.2 | **19.75** |
| 4.-60.36 | ours `_msn` | 512² | BC1 | 158.0, 236.1, 165.2 | **11.76** |

Our diffuse is a single flat grey on both tiles — blank, not dark. That flat
grey is also the **control that proves the staging works**: if the renderer had
fallen through to the game's own archives, our half would come back with
vanilla's colour in it, and if the texture were missing altogether it would
come back white.

### One material difference, stated and not explained

`lodgen --dump` on the two `BSLightingShaderProperty` blocks: ours ships
Smoothness 1, Specular Color #000000, Texture Clamp Mode 3; vanilla's ships
Smoothness 0, Specular Color #ffffff, Clamp Mode 0. Both frames were rendered
with each file's own material as it sits, so this difference is inside the
comparison rather than controlled out of it. No verdict on it here.

---

## 3. Image 1 — the distance comparison

**`scratchpad/mountains_20260907/images/mountain_distance_compare.png`**
(1414 × 486, 297,475 bytes; both halves labelled "vanilla" / "ours" burned in.)

Camera, identical for both halves and pinned rather than auto-fitted:

| | |
|---|---|
| view | `WW_RENDER_VIEW=8` (ViewUser, the oblique startup view) |
| look-at | `WW_RENDER_CENTER=32768,32768,27000` — the chunk's own centre in scene units (16 cells × 4096 = 65536 across) |
| distance | `WW_RENDER_DIST=55000` |
| framebuffer | `WW_RENDER_SIZE=1400x900`, which the window resolves to **1507 × 841** |
| crop | `(420, 290) – (1110, 620)`, the same box on both |
| scene time | `WW_RENDER_TIME=1` |
| window | `WW_WINDOW_AT=1960,40`, second monitor |

Driver: `images/shoot.sh <btr> <png> 8 32768,32768,27000 55000 1400x900 <port>`
— it re-checks `Fallout4.exe` and any other `NifSkope.exe` before every launch
and refuses rather than running. Composition: `images/compose.py`.

Frames: `shot_van16.png`, `shot_ours16.png`.

**The camera pin was verified, not assumed.** Two runs of the vanilla tile at
the same look-at and two distances give two different pictures and two
different file sizes — `ctl_dist90000.png` (63,321 bytes) and
`ctl_dist60000.png` (145,546 bytes). `WW_RENDER_DIST` takes.

## 4. Image 2 — the close-up

**`scratchpad/mountains_20260907/images/mountain_peak_closeup.png`**
(1474 × 647, 523,006 bytes; same labelling.)

The peak: **cell (-60,36), centre height 38,840 units** — the highest single
sample in the 8 × 8 block of cells x −64..−57, y 32..39, and higher than all
four of its own corners, so it is a summit and not a shoulder. Read from the
ESM with `lodgen --worldspace 3C --cell X Y` on each of those 64 cells (the
authoritative source, not a regenerated map). In the dim-4 chunk's own scene
units (4 cells × 4096 = 16384 across) that summit sits at **(2048, 2048)**.

| | |
|---|---|
| view | `WW_RENDER_VIEW=8` |
| look-at | `WW_RENDER_CENTER=2048,2048,38840` — the summit |
| distance | `WW_RENDER_DIST=11000` |
| framebuffer | 1507 × 841 |
| crop | `(180, 350) – (900, 841)`, the same box on both |

Frames: `shot_van4.png`, `shot_ours4.png`.

## 5. What the pictures show

At distance, vanilla's chunk carries brown-and-grey rock colour over the whole
tile while ours is a single flat grey with no colour anywhere, and the two
silhouettes and their shading agree, so the shape is there and only the colour
is missing. Close up on the peak, vanilla's surface carries fine crisp
striations and rubble across the slope while ours resolves the same slopes as
smooth rounded folds, with a faint square lattice on the quieter faces where
its normal map is being interpolated between far fewer distinct values.

No verdict here on why.

### One thing found while taking the pictures, stated as a measurement

Our terrain chunks ship **vertex colours** by default (`lgTerrainIdentity` is
`true` at `src/nifcli.cpp:5144`); vanilla's do not. Measured:

* ours, default: `Vertex Desc` = 686095322853893, `Vertex Data/0/Vertex Colors`
  = `#007ecd00`, `/1` = `#0000de00`, `/2` = `#0000ea00`.
* vanilla: `Vertex Desc` = 52776558133763, and the field `Vertex Colors` does
  not exist in the block at all.
* ours with `--no-terrain-identity`: `Vertex Desc` = **52776558133763**, the
  same number as vanilla, and no `Vertex Colors` field.

NifSkope multiplies that vertex colour into the diffuse, so the default build
of our chunk photographs deep blue rather than grey:
`ctl_ours16_vertexcolour.png` and `ctl_ours4_vertexcolour.png` are those
frames, same camera. **Both delivered images were therefore made with
`--no-terrain-identity`**, so the comparison is terrain against terrain at
vanilla's own vertex layout and the channel is not sitting inside the picture.
What that channel does under Fallout 4's own LOD shader was not measured here
and is not claimed either way.

## 6. Mistakes

1. **Wrote outside the folder the brief allowed.** The regeneration commands
   passed `--out-dir`/`--tex-dir` as paths relative to a `cd`-ed shell, but the
   exe resolves a relative output path against **its own directory**, so eight
   files landed under `release/scratchpad/…`. Found by the next command failing
   with `no such file` on the path the generator had just reported writing.
   Moved into `images/` and `release/scratchpad` deleted; `release/` is back to
   what it was. Rule: **every path handed to `release/NifSkope.exe` is
   absolute**, and the file is listed on disk before the run is believed.
2. **Used a decoder I had not calibrated, and nearly acted on its answer.** To
   find the summit I wrote a reader for
   `Commonwealth_fine.HeightMap.-96.-96.95.95.-8320.44872.dds` assuming R16
   linear over −8320..44872. It returned 19,954..22,240 over a region whose ESM
   heights are 28,464..38,840, and a global maximum of 22,828 — so the
   assumption is wrong somewhere (offset, orientation or scale). Found by the
   cross-check against three known ESM cell corners, which is the only reason
   it did not silently pick the wrong peak. The script was deleted rather than
   left in the folder as a trap, and the summit was taken from `lodgen --cell`
   instead. Rule: **a decoder prints its cross-check against the master before
   its answer is used**, and one that fails the cross-check is deleted, not
   parked.
3. **Started a second `NifSkope.exe` job while a render batch was queued.** The
   64-cell `lodgen --cell` sweep and the render chain were launched in the same
   turn; `shoot.sh`'s own one-instance guard caught it and refused all four
   renders (`REFUSED: a NifSkope is already running`), costing one round.
   Nothing was corrupted — the guard did its job — but a guard is not a
   scheduler. Rule: **one NifSkope-launching job in flight at a time**, CLI
   sweeps included.

Not a mistake, recorded because it interrupted the lane: `Fallout4.exe` came up
at about 12:59 while the renders were queued. The gate in `shoot.sh` refused
every launch until it went down again at 13:03:45, and no exe of ours ran
beside the game.

## 7. Finished-work skill review

**Skills loaded and used.** `nifskope-ww-render-shot` — mandatory here, and it
carried the whole render: the `WW_RENDER_*` switch table, `WW_WINDOW_AT`, the
unused-port rule, the one-instance rule, and specifically its warning to
*verify `WW_RENDER_CENTER`/`WW_RENDER_DIST` actually took by rendering two
distances*, which is section 3's control. `nifskope-ww-lodgen` — the CLI table
(`--terrain-region`, `--dim`, `--tex-dir`, `--data-root`), the worldspace id
`3C`, the FO4 corpus paths, and the reminder that `-no-gui` never initialises
the game manager so a CLI bake needs `--data-root`. Both saved re-deriving from
source.

**Procedures re-derived from scratch that a skill would have carried.** Three:

1. *How to put OUR generated assets in front of the renderer without touching
   the game's data.* This took reading `gltex.cpp`, `nifmodel.cpp:3387` and
   `gamemanager.cpp:519` to establish that a `.btr`'s own folder becomes a
   resource path consulted before the game archives, and that the staging
   layout is `<dir>/<chunk>.btr` + `<dir>/textures/terrain/<ws>/`. Every future
   vanilla-vs-ours picture needs it.
2. *Choosing the tile to photograph, defensibly.* Ranking vanilla's own sheets
   by colour spread × `_msn` tilt, on the master only, with the painted-box
   filter — the same question ("which tile shows this best, and why that one")
   comes up for every comparison.
3. *Pinning two renders to one camera and burning the labels in.* Look-at from
   the chunk's own scene extent, the two-distance control, the identical crop,
   and the side-by-side with a hairline and a caption that names the camera.

**Skill written.**
`E:\Projects\Claude\.claude\skills\nifskope-ww-vanilla-compare\SKILL.md` —
"photograph vanilla's shipped asset against ours, same camera" — carrying all
three of the above plus the two traps this lane hit (absolute paths to the exe;
the default terrain vertex-colour channel that tints our half). Written to the
LIVE tree; per CONSTITUTION 1a the director must mirror it into
`<repo>/.claude/skills`, because the two trees do not sync.

**Not written, and why.** Nothing was written for "find the summit in a cell
rectangle": the honest route is 64 `lodgen --cell` calls, which is a one-line
loop already quoted in section 4, and the fast route needs a heightmap decode
that mistake 2 shows is not established. A skill built on an uncalibrated
decoder would spread the error rather than save work.

---

## Files

Under `scratchpad/mountains_20260907/images/` (nothing outside it was written,
and no `src/` file, build or commit was touched):

| file | what |
|---|---|
| `mountain_distance_compare.png` | **image 1, the deliverable** |
| `mountain_peak_closeup.png` | **image 2, the deliverable** |
| `shot_van16/ours16/van4/ours4.png` | the four raw frames the two images crop |
| `ctl_dist60000.png`, `ctl_dist90000.png` | the control that `WW_RENDER_DIST` takes |
| `ctl_ours16_vertexcolour.png`, `ctl_ours4_vertexcolour.png` | our chunk with the default vertex-colour channel on |
| `pick_region.py`, `pick16.txt`, `pick4.txt` | tile selection and its numbers |
| `shoot.sh`, `compose.py` | the render driver and the compositor |
| `gen16.log`, `gen4.log` | the two generation runs |
| `van16/`, `van4/`, `ours16/`, `ours4/` | the four staged data roots |


---

## 8. Normal-only remake (2026-09-09)

bungo's words: *"Can you show me the vanilla normal map on the left, without
the diffuse? For a better comparison"*.

Four new images, same tiles, same pinned cameras, same crops as sections 3
and 4:

| file | what |
|---|---|
| `images/mountain_distance_normal_only.png` | the distance pair, diffuse removed on both halves |
| `images/mountain_peak_normal_only.png` | the close-up pair, diffuse removed on both halves |
| `images/msn_sheet_distance_crop.png` | the two `_msn` sheets themselves, dim 16 |
| `images/msn_sheet_peak_crop.png` | the two `_msn` sheets themselves, dim 4 |

Exe `release/NifSkope.exe`, mtime **2026-09-07 12:48:07**, still newer than
every file under `src/` (newest `src/lodgen.cpp` **2026-09-07 12:47:37**).
`Fallout4.exe` absent and no other `NifSkope.exe` running before every launch
(`shoot.sh` re-checks both and refuses); one NifSkope-launching job at a time,
each render waited out before the next; `WW_WINDOW_AT=1960,40`, second
monitor. Nothing outside `scratchpad/mountains_20260907/images/` and this
report was written; no `src/` edit, no build, no commit.

### 8.1 There is no normal-only view in the render hook — so the diffuse was replaced, not switched off

Checked in the source rather than assumed. The hook's switches are
`WW_RENDER_SHOT/SIZE/VIEW/TIME/FLAT/CENTER/DIST/REFRACTION/SEQ/SS` and
`WW_LOD_CHANNEL` (`src/nifskope_ui.cpp:21251-21410`), and the channels
themselves are `1..13` in `res/shaders/fo4_default.frag:267-345`. None of them
lights the surface from the normal map:

* channel **8** is the *geometric* normal, `normalize(btnMatrix_norm[2])` — the
  interpolated vertex normal, not the sheet;
* channel **10** is the material channel (gloss/specular), not the shading;
* `WW_RENDER_FLAT=1` is *"vertex colours only, no textures, no lighting"*,
  which removes the normal map as well.

So the brief's substitute route was used: **the same flat mid-grey sheet stands
in for the diffuse on both halves**, and the lit path runs untouched, so the
only thing left that can shade the surface is the `_msn`.

`images/make_grey.py` writes it — a BC1 512x512 sheet with the full 8-mip chain,
every block `colour0 = 0x8410`, `colour1 = 0x0000`, all indices 0, which is the
4-colour opaque mode selecting `colour0` for every texel. The 128-byte header is
copied from a sheet the renderer already accepts rather than hand-written; the
payload is regenerated in full. Decoded back with `dds.py` + `bcnp.py`: mips 0,
2 and 5 all have **min == max == (132, 130, 132)**, i.e. per-channel SD 0. That
is the nearest 565 value to 128 grey.

```
python images/make_grey.py <abs ours16 diffuse .DDS> <abs images/flat_grey.DDS>
  -> 512x512  8 mips  174888 bytes
```

### 8.2 Staging, and the three controls that say the substitution is honest

Same trick as section 2 — each side is its own miniature data root, and the
`.btr`'s own folder is searched before the game's archives — but now with the
grey sheet in the diffuse slot:

```
images/nrm_van16/   Commonwealth.16.-64.32.BTR   (vanilla)  + grey diffuse + VANILLA _msn
images/nrm_ours16/  Commonwealth.16.-64.32.BTR   (ours)     + grey diffuse + OURS    _msn
images/nrm_van4/    Commonwealth.4.-60.36.BTR    (vanilla)  + grey diffuse + VANILLA _msn
images/nrm_ours4/   Commonwealth.4.-60.36.BTR    (ours)     + grey diffuse + OURS    _msn
images/nrm_swap16/  Commonwealth.16.-64.32.BTR   (VANILLA)  + grey diffuse + OURS    _msn
```

The grey sheet is **md5 `ebab29655be312f8c1887c5a0645eeae` in all five
folders**, so it cannot be the thing that differs. Both `.BTR`s on both tiles
report `Vertex Desc` **52776558133763** — vanilla's number, no `Vertex Colors`
field — so ours are the `--no-terrain-identity` builds and the vertex-colour
channel of section 5 is not in these pictures either.

Neither chunk names a specular map (`dump -b 3`: only slots 0 and 1 are filled,
the diffuse and the `_msn`), so `hasSpecularMap` is false on both sides and the
`_data.DDS` our generator also writes is not bound.

Three controls, all pre-registered before the renders:

1. **Noise floor.** `nrm_van16` rendered twice, independently
   (`shot_nrm_van16.png`, `ctl_nrm_repeat16.png`): **byte-identical, md5
   `18e7ce6c…`, mean absolute difference 0.000000**. Any difference below is
   real.
2. **The substitution is neutral on our half.** Our own diffuse already *was*
   this exact flat grey (section 2: mean 132, 130, 132 at luminance SD 0.00), so
   replacing it must change nothing — and it does not:
   `shot_nrm_ours16.png` is byte-identical to section 3's `shot_ours16.png`
   (`02b59cfe…`), and `shot_nrm_ours4.png` to `shot_ours4.png` (`96df4d38…`).
   The same substitution on vanilla's half *does* change the frame
   (`shot_van16.png` `54d86c99…` vs `shot_nrm_van16.png` `18e7ce6c…`), which is
   the diffuse leaving the picture.
3. **The sheet alone, isolated from the mesh and the material.**
   `ctl_nrm_swap16.png` is vanilla's own `.btr`, vanilla's own material and the
   grey diffuse, with **only** the `_msn` swapped for ours. Over the 77,323
   terrain pixels inside the delivered crop box:

   | pair | mean abs difference (0-255) | max |
   |---|---|---|
   | repeat render (noise floor) | **0.000** | 0 |
   | vanilla vs vanilla-with-our-`_msn` (sheet only) | **12.198** | 129.3 |
   | vanilla vs ours (the delivered halves) | **20.280** | 143.3 |

### 8.3 The two lit images

```
images/shoot.sh <abs .BTR> <abs .png> 8 32768,32768,27000 55000 1400x900 <port>   # dim 16
images/shoot.sh <abs .BTR> <abs .png> 8 2048,2048,38840   11000 1400x900 <port>   # dim 4
```

Ports 42401-42406, one at a time. Framebuffer resolves to 1507 x 841 as before;
crops `(420,290)-(1110,620)` and `(180,350)-(900,841)`, the same box on both
halves, nothing scaled. Labels **"vanilla (normal only)"** / **"ours (normal
only)"** burned in by `images/compose.py`, same caption style, and the caption
names the tile, the cells, the camera, the grey substitution and the generator
command.

```
python images/compose.py shot_nrm_van16.png shot_nrm_ours16.png \
   mountain_distance_normal_only.png "vanilla (normal only)" "ours (normal only)" \
   "<caption>" 420 290 1110 620
python images/compose.py shot_nrm_van4.png shot_nrm_ours4.png \
   mountain_peak_normal_only.png  "vanilla (normal only)" "ours (normal only)" \
   "<caption>" 180 350 900 841
```

**Named, not controlled out, and said in the caption:** each chunk keeps its own
material — vanilla Smoothness 0 / Specular `#ffffff` / Clamp 0, ours Smoothness
1 / Specular `#000000` / Clamp 3 (section 2). Control 3 above is what separates
that from the sheet.

### 8.4 The two raw sheet crops

`images/msn_crop.py` decodes the sheet with `dds.py` + `bcnp.py`, takes the
**same centred 160 x 160 texel region of mip 0** from both files (crop at
176,176), and scales it x4 with nearest-neighbour, so one screen block is one
texel and nothing is filtered, lit or resampled.

```
python images/msn_crop.py <abs _msn .DDS> <abs out.png> 0 160 4
python images/compose.py crop_msn_van16.png crop_msn_ours16.png msn_sheet_distance_crop.png \
   "vanilla _msn (raw sheet)" "ours _msn (raw sheet)" "<caption>"
python images/compose.py crop_msn_van4.png crop_msn_ours4.png msn_sheet_peak_crop.png \
   "vanilla _msn (raw sheet)" "ours _msn (raw sheet)" "<caption>"
```

Measured over exactly those 25,600 texels:

| tile | side | format | mean RGB | per-channel SD | distinct colours |
|---|---|---|---|---|---|
| 16.-64.32 | vanilla | BC3 | 158.5, 216.5, 144.1 | 34.34, 21.66, 42.88 | **5,559** |
| 16.-64.32 | ours | BC1 | 167.0, 234.9, 148.7 | 27.54, 11.17, 47.01 | **3,178** |
| 4.-60.36 | vanilla | BC3 | 135.4, 230.1, 186.0 | 45.81, 13.85, 29.09 | **4,919** |
| 4.-60.36 | ours | BC1 | 142.9, 238.5, 181.6 | 24.87, 10.05, 21.39 | **2,160** |

No claim is made about which cell a texel belongs to: the sheet-to-cell mapping
was not calibrated against the ESM in this lane, so the crop is simply the
centre of both sheets, identically.

One thing worth having in writing, because it bounds what any `_msn` comparison
can mean here: the renderer **discards the stored blue** and recomputes it,
`normal.b = sqrt(max(1 - dot(normal.rg, normal.rg), 0))`
(`res/shaders/fo4_default.frag:368-369`), treating the model-space sheet as a
tangent-space one. Two sheets differing only in blue would render identically.
Blue is shown in the raw crops; it does not reach the lit pictures.

### 8.5 What the pictures show

With the colour gone, vanilla's slope carries dense fine striations and rubble
right across the face, and its far chunk keeps sharp ridge lines and gully
shadows. Ours resolves the same landform as smooth rounded folds: the
silhouettes and the large shapes agree, the fine detail does not, and on the
quieter faces of the close-up a faint square lattice shows where the normal map
is being interpolated between far fewer distinct values — which is the same
thing the raw sheets say numerically, 2,160 distinct colours against vanilla's
4,919 in the same 25,600 texels.

No verdict here on why, and nothing here is called fixed or final.

### 8.6 Mistakes

4. **Shipped a caption that ran off the right edge of the picture.** The first
   compose of `mountain_distance_normal_only.png` used `compose.py` as it stood,
   which draws each `|`-separated caption line as one unwrapped run; four of the
   five lines were wider than the image and lost their tails, including the line
   naming the material difference that the caption exists to disclose. Found by
   opening the file that had just been written instead of trusting the
   compositor's own "written, 1414x534" line. Fixed in `compose.py` by wrapping
   on the measured text width (`ImageFont.getlength`), and both images
   recomposed. Rule: **an image deliverable is opened and read after it is
   written** — a compositor reports that it wrote a file, never that the file
   says what was passed to it. (This is the picture form of the standing rule
   that telemetry echoes bytes, not intent.) The earlier images of sections 3
   and 4 were composed by the same unwrapped code and may have the same clipping;
   they were not recomposed here because this lane was not to touch them.

### 8.7 Finished-work skill review

**Skills loaded and used.** `nifskope-ww-vanilla-compare` — it carried the whole
job: the absolute-path rule, the staging layout that puts our files in front of
the renderer, the `--no-terrain-identity` check with the exact `Vertex Desc`
number to compare against, the "prove the staging works before you trust the
picture" rule (which is what controls 1 and 2 above are), the "stage a third
folder to isolate sheets from mesh" line (control 3), and the ban on a verdict.
`nifskope-ww-render-shot` — the switch table, the one-instance and second-monitor
rules, and specifically the channel list, which is what made "is there a
normal-only view" a five-minute source check instead of a guess.

**Procedures re-derived that a skill did not carry.** One, and it is now
written: how to remove one texture from the comparison without touching the
renderer — build a constant BC1 sheet, prove it is constant, use the same bytes
on both halves, and prove the substitution is neutral where it must be.

**Skill amended.**
`E:\Projects\Claude\.claude\skills\nifskope-ww-vanilla-compare\SKILL.md` (the
LIVE tree) gained a new section **"7. Take a texture OUT of the comparison"**
carrying: the finding that no render-hook switch gives a normal-only lit view
and why (channel 8 is the geometric normal); the constant-BC1 recipe and the
`make_grey.py` reference; the three controls (noise floor by repeat render,
neutrality on the side whose sheet already was that value, and the swap folder
that isolates the sheet from the mesh and material) with the numbers this lane
measured; and the caption-wrapping trap from 8.6. Per CONSTITUTION 1a the
director must mirror it into `<repo>/.claude/skills`, because the two trees do
not sync.

**Declined.** No skill was written for "flat-grey a texture slot" as a separate
procedure — it is three paragraphs inside the comparison skill and has no life
of its own outside it.

### 8.8 Files added under `scratchpad/mountains_20260907/images/`

| file | what |
|---|---|
| `mountain_distance_normal_only.png` | **deliverable 1** |
| `mountain_peak_normal_only.png` | **deliverable 2** |
| `msn_sheet_distance_crop.png` | **deliverable 3** |
| `msn_sheet_peak_crop.png` | **deliverable 4** |
| `flat_grey.DDS`, `make_grey.py` | the neutral diffuse and the script that writes it |
| `msn_crop.py` | the raw-sheet cropper |
| `compose.py` | amended: captions now wrap on measured width |
| `shot_nrm_van16/ours16/van4/ours4.png` | the four raw frames the two lit images crop |
| `ctl_nrm_repeat16.png` | the noise floor: byte-identical to `shot_nrm_van16.png` |
| `ctl_nrm_swap16.png` | vanilla mesh + vanilla material + OUR `_msn` |
| `crop_msn_van16/ours16/van4/ours4.png` | the four raw sheet crops |
| `nrm_van16/`, `nrm_ours16/`, `nrm_van4/`, `nrm_ours4/`, `nrm_swap16/` | the five staged data roots |
