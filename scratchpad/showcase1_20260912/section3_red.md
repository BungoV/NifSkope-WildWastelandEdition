
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
