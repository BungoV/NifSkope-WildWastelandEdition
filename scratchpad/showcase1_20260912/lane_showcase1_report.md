# Lane SHOWCASE1 -- the final Sanctuary chunk bake with every landed feature on

Written incrementally. Tree `E:/Projects/NifskopeWildWastelandEdition`, branch main,
**no code changes, no build, nothing committed**.

Exe: the lane's own copy `scratchpad/showcase1_20260912/ns_run/NifSkope.exe`,
22,007,808 B, sha1 `ba7585cba389c8b38f0e07c6c11063e0cec1124c` -- the size and the
hash are exactly what the brief names. Its **mtime reads 15:42:53, not the
12:58:48 the brief quotes**, because the copy did not carry the original's
timestamp; the sha1 is the byte test and it matches, so the run went ahead.
`release/NifSkope.exe` was never touched or executed (lane PANEL1 owns it).

Region: Sanctuary, cells `-20 24 -9 35`, dim 4 = 9 chunks. Game down before every
launch (`tasklist` checked; `Fallout4.exe` absent every time).

---

## 0. Settings table

| feature | switch as typed | value | source |
|---|---|---|---|
| roads | `--road-detail 1` | 1 (also the built-in default since 06:31 today) | bungo's standing rule 2026-09-12 05:0x |
| land sampling | `--land-guide aspecthex --land-guide-scale 256 --land-hex 256` | LAND1's winner | brief; LAND1 block, HANDOFF + BAKE_INSTRUCTION "the one arm worth a region bake" |
| land warp | *not passed* | `--land-sample` left at its default (off) | LAND1/TILING3 left it as **bungo's call** ("Try it on a region and look at it; the call is yours"). Not a lane's pick. |
| terrain object AO | `--terrain-object-ao` | strength left at default **0.5** | GROUND1 §2.5h: 0.5 is the largest sampled strength at which no texel clamps to black |
| erosion | `--erosion 1 --erosion-iterations 4 --erosion-seed 7` | GROUND1's own picture values | `scratchpad/ground1_20260912/work/pics_b_render.py:53` and `pics_b.py:167` |
| land detail source | *not passed* | default (vanilla `_msn` reuse, TILING3) | GROUND1's pictures also passed `--land-detail-source erosion`; that would switch OFF the vanilla reuse TILING3 landed, so it was left at the default and is named here instead |
| `_msn` cache | `--msn-cache E:/Tools/Upscale/esrgan-bat/output` | 2,304 cleaned 2K PNGs, read-only | TERRAINFMT1 block |
| sheet format | `--sheet-format legacy` | legacy = the default; typed anyway | TERRAINFMT1 |
| terrain identity | default ON, plus a whole second bake with `--no-terrain-identity` | both baked, both shown | brief: bungo has not ruled, so no decision taken |
| objects | `--cover --arrays --atlas` (`--merge` is default true) | arrays AND atlas compose in one run | `src/nifcli.cpp:4024` builds the arrays, `:4041` the atlas, `:4081` the merge -- in that order, no refusal between them |
| native object files | `--native --native-mesh-report` | on | director's `bake2.sh` |
| pyramid | `--vt` | on, levels 2 and 4 | director's `bake2.sh` |
| vanilla root | default `E:/Tools/Fallout 4/DataUnpacked/Data` | read-only | TILING3 |
| `.lodl` | own command `--lodl <dir>` | whole worldspace | it RETURNS after writing and never reaches the region |
| shadow heightmap | own command `--heightmap <dir>` | whole worldspace | same |
| cards | `CANDIDATES=trees OCT=8 TILE=512` | 23 trees | BAKE_INSTRUCTION §7 |

**`--terrain-object-ao` cannot be combined with `--lodl`** (exit 2, GROUND1's named
refusal), which is a second reason the `.lodl` is its own command here.

---

## 1. Commands and results

Every command is in a script beside this report, so the exact argv is on disk and
not retyped here: `bake_region.sh` (the 9-chunk ON and NOID bakes), `bake_aux.sh`
(`--lodl`, `--heightmap`, six one-chunk attribution bakes), `bake_far.sh`
(dim 8 / 16 / 32 with the card library), `bake_cards_lane.sh` (the card bake),
`bake_look.sh` and `bake_far_look.sh` (see the red in §3), `make_res.sh`
(the resource roots the renders read), `shot.sh` (one render), `verify.sh`.
The exact argv of each bake is also written to `logs/<name>.argv`.

### 1.1 The near bakes, cells -20 24 -9 35, dim 4

| bake | out dir | chunks | stage times | note |
|---|---|---|---|---|
| ON (every feature) | `out/on` | 17 written, 0 empty, 0 failed | landscape 0.0 s, meshes 34.2 s, textures 41.9 s | the showcase bake |
| NOID (`--no-terrain-identity`) | `out/noid` | 17, 0, 0 | same shape | terrain vertex colours off |
| LOOK (`--no-terrain-identity --no-identity`) | `out/look` | 17, 0, 0 | meshes 25.5 s, textures 37.7 s | added mid-lane, see §3 red 1 |
| OFF (every switch off) | `out/off` | 17, 0, 0 | -- | the control |

Peak working set on the ON bake: 1.79 GB (1,917,046,784 bytes), 9 chunk jobs,
1 chunk worker.

Object counts from the manifests, the three fullest near chunks:

| chunk | rows | trees | rocks | buildings | misc | BTO bytes |
|---|---|---|---|---|---|---|
| `Commonwealth.4.-20.24` | 678 | 654 | 13 | **11** | 0 | 1,276,441 |
| `Commonwealth.4.-16.24` | 696 | 678 | 16 | 0 | 2 | 1,265,958 |
| `Commonwealth.4.-20.28` | 512 | 507 | 5 | 0 | 0 | 763,896 |

`-20.24` is the chunk every near picture uses: it is the only one of the nine
with buildings in it.

### 1.2 The far rings (same region, cards from `cards/`)

| ring | chunks | merged | far-ring cut | card arrays | stage times |
|---|---|---|---|---|---|
| dim 8 | 4 | 30 shapes -> 15 | 0 cut | 2 card sets in 8 arrays (2 groups), 1,346 `C` lines | meshes 7.6 s, textures 36.0 s, impostors 2.1 s |
| dim 16 | 4 | 81 shapes -> 8 | 2 shapes, 559 -> 553 tris, worst error 502.1 units | 23 card sets in 60 arrays (15 groups), 14,564 `C` lines | meshes 441.7 s, textures 36.4 s, impostors 190.4 s |
| dim 32 | 2 | 39 shapes -> 6 | 0 cut | 23 card sets in 60 arrays (15 groups), 24,998 `C` lines | meshes 262.0 s, textures 38.0 s, impostors 201.0 s |

23 of 23 card candidates baked, 0 sets unreadable. The card list is in
`logs/cards.log`; they are all trees, e.g. `0003a28b Landscape\Trees\TreeHero01.nif`,
`000d9ca7 TreeElmFree01.nif`, `00121550 BlastedForestBurntTreeUpright02.nif`.

### 1.3 The two whole-worldspace commands

* `.lodl` -- `out/lodl/Terrain/Commonwealth.lodl`, v2, 192x192 cells, 36,864 land
  and 36,864 water, 15 WATR types, height -8,320..44,872, overview 1536^2.
  `--verify-only` read it back: **36,864 samples cross-checked against the ESM,
  0 mismatched, worst 0**; alpha words 0 of 36,864 differ, colour words 0 of
  36,864 differ.
* shadow heightmap -- `Commonwealth.HeightMap.-96.-96.95.95.-8320.44872.dds`,
  6144x6144 R16_UNORM, **37,748,736 of 37,748,736 texels covered**, corpus hash
  `d8337d022f637f22` over 36,864 VHGT, and the log says that hash **matches the
  loader's pinned Commonwealth constant**.

### 1.4 The native far field (`--native`)

`logs/native_verify.log`: pair identity ok, instancesWithGeometry 3,526,
instancesWithNeitherGeometryNorCard **0**, distinctStockIdentities 696,
drawKeyRanksChecked 3,526, ladderMeshes 1,900 of 2,982, ladderRootTriangles
142,138 (= level-0 triangles). occluderBoxes 0 -- the ON bake's occluder pass
offered 0 boxes because 2,617 source models were refused as not watertight,
100 as too small, 123 with no interior voxel, 43 as too thin; 99 models fitted
but none survived to a cell. **That is a zero worth knowing about and I am not
calling it wrong** -- it is what this region's models are.

### 1.5 Attribution: which switch moves which file

One chunk (`-20 24 -17 27`), six bakes, byte-compared. `SAME` means byte-identical
to the all-on bake.

| file | all on | all off | no `--land-guide` | no `--erosion` | no `--terrain-object-ao` | no `--msn-cache` |
|---|---|---|---|---|---|---|
| `.BTR` | -- | SAME | SAME | SAME | SAME | SAME |
| colour `.DDS` | -- | diff | **diff** | **diff** | SAME | SAME |
| `_msn.DDS` | -- | diff | SAME | SAME | SAME | **diff** |
| `_data.DDS` | -- | diff | SAME | **diff** | **diff** | SAME |

Three things fall out of that table:

1. **The `.BTR` mesh is byte-identical across all six.** Every pixel that differs
   between an ON render and an OFF render of the terrain comes out of the sheets,
   not out of the geometry. That is the render skill's first floor and it fires.
2. **With `--msn-cache` on, erosion never reaches the normal sheet.** The `noero`
   column leaves `_msn` byte-identical while moving both the colour and the `_data`
   sheet. The cached/vanilla normal wins over the erosion pass's own normals.
   Worth bungo's eye: it means the two features do not compound the way the
   names suggest.
3. `--terrain-object-ao` moves **only** `_data` -- which is right, its output is
   that sheet's R channel.

**A confound to state rather than hide:** the all-off control also drops
`--cover`, so its `_data` A channel is flat 255 and the sheet is DXT1 where ours
is DXT5. That one difference belongs to `--cover`, not to the four switches
beside it.

### 1.6 What the ON bake's own census says it did

Straight out of `logs/on.log` (the `vt:` line), the numbers behind the switches
in the table above:

* **roads** -- `roadDetail 1.000`, 91 road placements from 33 meshes, 60,077 road
  triangles, 272 road shape tiles, 91 decal shape tiles, 77 ground-paint shapes,
  2,000 alpha-rejected texels. 22 refused for no texture, 4 refused as raised,
  and the refusal list names them (two highway-overpass pieces that have their
  own LOD).
* **object AO** -- `terrainObjectAo 1`, reach 1,458 units, strength 0.500,
  5,743 placements from 75 meshes, 213,212 triangles rasterised, 59,525 squares,
  **1,565,554 texels darkened, mean darkening 71.87 of 255**. 17,951 placements
  refused because their base has no LOD mesh (607 distinct bases; the log names
  saplings, brambles, paper debris, a crow marker).
* **erosion** -- 1.000 strength, 4 iterations, seed 7, step 32.0,
  **10,036,224 cells, all 10,036,224 moved**, mean absolute move 8.40,
  worst cut -27.74, worst fill +33.36.
* **`_msn` cache** -- `msnCacheHit 9, msnCacheMiss 0, msnCacheRenorm 9`, and
  `msnCopied 9 / msnOurs 0`: **all nine chunks' normal sheets came from bungo's
  cleaned 2K set**, none was generated. Colour is the other way round --
  `colCopied 3 / colOurs 6`, six of the nine colour sheets are ours.
* **virtual texture** -- levels 2, 45 tiles, 45 present, 25 cover tiles,
  7,496,960 bytes, 3 mask sheets, 21 distinct LTEX in the masks, 813 layer refs,
  `sheetFormat 0` (legacy), 6 chunks layered / 3 layerless / 0 layerless with no
  vanilla fallback, land shade -3.242.

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

---

## 3. Owed / red / bungo's calls

### Red 1 -- our LOD files cannot be photographed in this viewport with the identity payloads on

This is the carried-forward red from HANDOFF ("NifSkope's viewport draws FO4
terrain LOD in greens/magentas over a dark-brown sheet"), and this lane can now
say what causes it and how far it goes.

**Cause.** Both bakes write a payload into VERTEX COLOURS, and NifSkope's normal
shading multiplies vertex colour into the diffuse:

* terrain, `--terrain-identity` (default on): `.BTR` vertex R = material class,
  G = wetness, B = AO, A = shore -- `src/lodgen.cpp:945-957`.
* objects, `--identity` (default on): `.BTO` vertex R+G = the 16-bit object
  index, B = the baked AO, A = tree sway -- `src/lodgen.h:594-604`.

So the viewport is not drawing the sheet wrong; it is drawing exactly what is
in the file, on top of the sheet.

**Measured, one camera, chunk (-20,24), top down, ortho halfW 12000:**

| file | background | non-background mean RGB |
|---|---|---|
| vanilla's own `.BTO` | 88.3 % | 120.2 / 118.5 / 113.9 |
| ours, `--identity` on | 88.2 % | **61.5 / 2.2 / 84.2** |
| ours, `--no-identity` | 86.4 % | 127.3 / 125.6 / 120.8 |

The green channel is crushed to 2 of 255 by the index byte. Coverage is within
2 points across all three, so this is colour and not geometry.

**How far it goes.** `--no-terrain-identity` fixes the TERRAIN only. I rendered
the `on` and the `noid` `.BTO` of the same chunk on the same camera and the two
PNGs are **byte-identical** -- the object payload is a separate switch and the
terrain one does not touch it.

**What I did about it.** I did not change any code. I added a fourth bake,
`out/look`, with the same switches as the showcase bake plus `--no-identity`
and `--no-terrain-identity`, purely so the "what does it look like" pictures
have something to be taken from, and every such picture says which bake it came
from. The showcase bake's own renders are in picture 2 as well, labelled, so
the difference is visible rather than described.

**Not my call:** whether the shipping profile should keep writing the object
index into vertex colour at all, or whether the viewport should stop multiplying
vertex colour on a `.BTO`/`.BTR`. Both are changes to landed behaviour.

### Red 2 -- there is no environment switch for an arbitrary camera rotation

The brief asked for **two** oblique views. The render hook can pin the look-at,
the eye distance, the FOV and the orthographic half-width
(`WW_RENDER_CENTER/DIST/FOV/ORTHO`), but the only thing that sets the ROTATION is
`WW_RENDER_VIEW`, and that is an enum: `ViewTop=1 .. ViewFront=5 .. ViewUser=8`
(`src/glview.h:452`). `ViewUser` is the Blender startup rotation and is the ONLY
oblique in the list; everything else is axis-aligned. I read
`GLView::wwCameraPinFromEnvironment()` (`src/glview.cpp:6352+`) in full and there
is no rotation arm.

So the pictures ship **one** oblique (VIEW=8, census `rot=-63.5593,0,133.3081`)
plus top and front, and the second oblique is refused with that reason rather
than faked by rotating the PNG.

### Red 3 -- the impostor cards' texture path does not resolve from the bake's own output

Every far-ring log carries lines like

```
File ' "textures/data/textures/lodgen/cards/0004d93b_fs.dds" ' not found in archives
```

The card `_fs.DDS` files are recorded with a `data\textures\lodgen\cards\` prefix
and the resource resolver then prefixes `textures/` again. The card ARRAYS were
still written (`23 card sets in 60 arrays, 0 sets unreadable`), so the bake is
not broken by it, but the message is real and would be a missing texture for
anyone who shipped the loose cards. I have not chased it; it is one line in the
card path writer and belongs to whoever owns the card bake.

### Red 4 -- the ON bake fitted 99 occluder models and wrote 0 boxes

`--native`'s occluder pass: 99 models fitted, 2,617 refused as not watertight,
100 too small, 123 with no interior voxel, 43 too thin -- and then
`occluderBoxes 0`, `cells 0 of 105 populated`. The 99 that fitted did not reach a
cell. That is a number worth an eye; I am not calling it a defect because I did
not read the cell-assignment code.

### Red 5 -- `--no-identity` silently takes the impostor card arrays with it

Found while building picture 3. The far LOOK bakes (dim 16 and dim 32, both
identity payloads off) came back with

```
card arrays: no placement in the chunks stands on an octahedral card
... impostors 0.1 s
```

while the identity-ON bakes of the same rings wrote `23 card sets in 60 arrays
(15 groups), 14,564 C lines` and spent 190 s on them.

**Cause, read out of the source.** The whole manifest row block is inside
`if ( opts.identity )` at `src/lodgen.cpp:3760` -- the placement row AND the
`C` line that says which octahedral card a placement stands on. With
`--no-identity` no `.manifest.txt` is written at all (measured: `out/farlook16/obj`
has the four `.BTO` files and no manifests). `lodgenBuildCardArrays` then reads
the manifests looking for `C` lines, finds none, and fails at
`src/lodgen.cpp:13046` with the message above.

So the two switches are coupled in a way their names do not say: turning the
object identity payload off also turns the card arrays off. It is not a crash
and the per-card sets are still on disk; it is a coupling worth knowing about
before anyone ships a profile with `--no-identity`.

**What it cost this picture.** Picture 3's renders are from the LOOK bakes so
the colour is honest (red 1), which means the card numbers in it -- the card
name, the `C` line count, the sheet -- are read from the identity-ON dim-16
ring instead. Both are named in the picture's own caption.

**Also worth saying plainly:** a card is not geometry inside a `.BTO`. An ASCII
scan of `Commonwealth.16.-32.16.BTO` finds exactly three texture paths, all
three the merged object atlas. A consumer learns about a card from the manifest
and draws it itself. So "a render with an impostor card in it" is not a thing
this viewport can produce from a chunk file; picture 3 shows the card's own
sheets instead, and says so.

### Bungo's calls, not taken by this lane

* `--land-sample` (the land warp) is left at its default. LAND1 and TILING3 both
  handed it to him: "try it on a region and look at it; the call is yours."
* `--land-detail-source erosion` is NOT passed, because it would switch off the
  vanilla `_msn` reuse TILING3 landed. GROUND1's own pictures did pass it. Two
  landed features that do not compose; his pick.
* `--terrain-object-ao` strength stays at the default 0.5 (GROUND1 §2.5h: the
  largest sampled strength at which no texel clamps to black).
* The identity payload question in red 1.

### Owed

* The `.lodl` and the shadow heightmap are whole-worldspace commands and were run
  as such; they are NOT region-scoped, so they carry the whole Commonwealth and
  are not a 9-chunk artefact.
* **Picture 3 does not contain a card drawn in the viewport**, for the reason in
  red 5: the card is metadata plus sheets, not geometry in the chunk file. If
  bungo wants an impostor drawn as the engine would draw it, that needs a
  consumer that reads the `C` lines -- a viewport feature nobody has written.
* The far rings render pale grey. That is the far object atlas, not a missing
  texture: its opaque texels have mean saturation 0.013, and the identical
  camera with no resource root comes back solid magenta. The gate is in the
  picture's caption.
* Nothing was committed, nothing was built, `release/NifSkope.exe` was never run.

---

## 4. Mistakes

1. **I grepped a binary with `strings` and believed the empty answer.** `strings`
   is not installed in this MSYS bash, so my first scan of a `.BTO` for texture
   paths returned nothing and I nearly wrote down that the file names no
   textures. Caught by re-running the scan in Python
   (`re.finditer(rb'[ -~]{8,}', data)`), which found
   `data\Textures\Terrain\Commonwealth\Objects\Commonwealth.LodgenObjects.DDS`
   immediately. A tool that is missing and a tool that finds nothing look the
   same on this shell; check the tool exists before trusting a negative.
2. **My first `shot.sh` waited on any `NifSkope.exe`.** That would have
   deadlocked the lane against its own headless bakes. The GUI slot is about GUI
   instances, so the wait now reads the command line through `wmic` and ignores
   anything with `-no-gui`.
3. **The first grid stretched every render into a square cell.** A 2800x1730
   render squeezed into a square changes every slope in the picture -- a
   measurement picture that lies about geometry. Fixed by letterboxing; the fix
   is in `pics/common.py:grid()` and applies to every picture.
4. **I rendered the building close-ups at ortho half-width 520** and got one wall
   across the whole frame at 98.4% coverage. Re-rendered at 1300 (oblique) and
   700 (top down) so the cluster and its surroundings are both in frame.
5. **I built picture 3's card panels before reading why the look bake had no
   cards.** The right order was source first: `--no-identity` gates the whole
   manifest (red 5), which is a finding, not an obstacle. I got there, but only
   after a 510-second bake that produced nothing usable for the picture.
6. **My DDS reader could not read our own card sheets.** It handled the
   uncompressed DX10 the `_msn` cache writes but refused DX10 BC3 (dxgi 77),
   which is what the packed card sheets are. Fixed by decoding the colour block
   in `pics/common.py:_bc_colour()` rather than by dropping the panel.

7. **Every composite keyed on the top-left pixel, and a close-up has geometry
   there.** `over()` took the background colour as `image[0, 0]`. That is true
   for a chunk-wide frame and false for the building close-ups, where the object
   render covers 97.4% of the frame -- so the mask came out inverted and the
   panel drew the BACKGROUND over the terrain. Caught by measuring instead of
   looking: the modal pixel of every clean grab is (43, 45, 49), while the
   corners of the four close-ups read (230, 230, 230), (92, 92, 92),
   (153, 157, 160) and (43, 45, 49). Fixed in `pics/common.py:clear_colour()`,
   which takes the modal pixel and checks it against the known clear colour; all
   four picture scripts now go through it and were rebuilt.

## 5. Skill review

Skills read: `lodgen-region-bake`, `nifskope-render-harness`, `lodgen-card-bake`.

* **The render skill's first floor fired and was worth the whole lane.** "Prove
  the pixels come from the file you think they do." The `.BTR` being
  byte-identical across all six attribution bakes is what lets section 1.5 say
  every visible difference comes from the sheets and not the geometry, and the
  magenta-vs-grey gate is what lets picture 3 say the pale far ring is the
  atlas rather than a missing texture. Both of those started as skill steps I
  nearly skipped.
* **The render skill has no entry for "the file draws its own payload".** It
  assumes a render shows a surface. Two of this fork's landed features write data
  into vertex colour, and NifSkope multiplies vertex colour into the diffuse, so
  the default bake cannot be photographed as it looks. The skill should say: if
  the bake writes vertex-colour payloads, either bake a look-only arm or state
  the payload; never present a payload render as a look render. That is red 1
  and it cost this lane a fourth bake.
* **The render skill should name `WW_LOD_CHANNEL` as the AO view.** `--ao-grey`
  exists in lodgen and writes baked AO into R, G and B, but it needs a REBAKE.
  `WW_LOD_CHANNEL=3` draws the same byte flat from a file already on disk, in
  one render. Nothing in either skill points at it; I found it in
  `src/lodgenmanager.cpp:1795-1803` and the shader.
* **The card skill does not say a card is not geometry.** It describes the bake
  and the sheets but never states that the chunk file names only the merged
  object atlas, so "show me an impostor" cannot mean "render the chunk and point
  at one". A line saying what a consumer has to do with the `C` lines would have
  saved this lane an hour.
* **The region-bake skill should carry the switch couplings.**
  `--terrain-object-ao` refuses to run with `--lodl` (a named exit 2),
  `--no-terrain-identity` does not touch the object payload, `--no-identity`
  takes the card arrays with it, and `--msn-cache` beats `--erosion` to the
  normal sheet. Four couplings, none of them in the skill, all of them found by
  running the thing.
* The resource-root shape (`<root>/data/Textures/...` AND `<root>/Textures/...`,
  a junction between them) is not in any skill and is needed by every render of
  a bake output. `make_res.sh` in this lane's folder is the procedure; it should
  become a skill.
