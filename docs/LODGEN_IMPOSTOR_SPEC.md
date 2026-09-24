# LOD material spec — `.lodm`, two families, one channel contract

> **This page is now the DESIGN RECORD, 2026-09-09.** The per-file **contracts**,
> each traced line by line to the writer, are:
>
> | file type | contract |
> |---|---|
> | `.lodm` (every kind) | `docs/LODGEN_LODM_FORMAT.md` |
> | impostor card sheets and card arrays | `docs/LODGEN_CARD_SHEETS.md` |
> | mesh texture arrays and the atlas sheets | `docs/LODGEN_TEXTURE_ARRAYS.md` |
> | `<chunk>.bto.manifest.txt` | `docs/LODGEN_MANIFEST_FORMAT.md` |
> | `kind: "terrainVT"` and `.lodt` | `docs/LODGEN_TERRAIN_VT.md` |
>
> **Where a contract page and this page disagree, the contract page wins**,
> because every statement there carries the writer line it was read at. Keep the
> rationale, the measurements and the history here; put bytes, keys and refusals
> there.

**Ours, for LOD.** Not PBRM's slots and not a BGSM's: a LOD set is described
by a **`.lodm`** — the LOD material, compact by design — and its textures mean
what this page says. FO4CS reads a distant maple on a card and a distant
shack on a mesh through this one contract. Decided with bungo 2026-09-06
(WW_CHANGES 2026-09-06f, 2026-09-06g).

## Two families

A set keeps the quantities its SOURCE carried. bungo: "keep it specular or
roughness, depending if source texture is vanilla or .pbrm sourced" — and
then, since `.pbrm` and `.bgsm` are limited in what they can carry for LOD,
our own format.

| family | source | textures | third texture |
|---|---|---|---|
| **legacy** | the vanilla material: diffuse, normal, the `_s` map, smoothness, specular strength | `_d`, `_n`, `_gsaos`, `_g` | **GSAOS** = gloss, specular, AO, subsurface mask |
| **pbr** | a source `.lodm` of family pbr | `_bc`, `_n`, `_rmaos`, `_e` | **RMAOS** = roughness, metallic, AO, subsurface mask |

`_gsaos` spells its channels the way `_rmaos` does and sits in the same slots
(gloss where roughness goes, specular where metallic goes), so one shader
reads either with a family switch and nobody mistakes it for a vanilla
two-channel `_s`.

| texture | format | R | G | B | A |
|---|---|---|---|---|---|
| `_d` / `_bc` | BC3 | colour (diffuse / albedo), unlit | | | coverage |
| `_n` | BC3; **BC7** on a card | normal X | normal Y | height | sway weight |
| `_gsaos` / `_rmaos` | BC3 | gloss / roughness | specular / metallic | AO | subsurface mask |
| `_g` / `_e` | BC1 | emissive colour | | | — |

Normal Z is rebuilt as √(1 − x² − y²). Coverage is the cut-out and the
alpha-test channel; it lives on the colour sheet and nowhere else, which is
why the emissive can be BC1 and cost half a block. Everything is linear
except the colour, which is sRGB like the source.

**The emissive, and why LOD has one.** bungo: "we need an emissive `_g`
texture or `_e` if it's PBRM". Vanilla has no such sheet and does not need
one, because it hides the quantity somewhere else. Measured 2026-09-06 on the
shipped atlases: a vanilla LOD chunk shape carries Own-Emit (Shader Flags 1
bit 22) with a BLACK emissive colour and no glow slot, and its atlas is DXT5
with a live alpha channel — Diamond City's has 216,521 of 262,144 alpha
blocks varying and not one fully transparent, on shapes that are opaque. The
light on a distant window is the diffuse's ALPHA, and the diffuse colour is
what it is the colour of. (The Commonwealth's own atlas is DXT1, one-bit
alpha: that worldspace spends the channel on cut-outs instead.) An
alpha-tested shape has already spent its alpha on the cut-out and emits
nothing.

But an alpha and a colour are not the whole of it. A vanilla chunk shape
own-emits with a BLACK emissive colour, and the engine multiplies that alpha
by the material's emissive COLOUR and its emissive MULTIPLE. Reading the
alpha alone left an opaque source with alpha 255 throughout yielding a `_g`
sheet equal to the full albedo — a consumer adding it unscaled would light
every wall (the doubt the fourth texture shipped with, 2026-09-06j). So the
law is completed by putting the colour in the SHEET and the multiple in the
MATERIAL. bungo, 2026-09-06: "carry the multiplier in lodm".

So the laws are:

- **legacy** — the colour times its own alpha **times the source's emissive
  colour** where the material is NOT alpha-tested; black where it is. The
  emissive colour is the BGSM's emittance colour where the shape names a
  BGSM that reads (and a BGSM that does not enable emit leaves it BLACK,
  which is the renderer's own reading), else the shader property's
  `Emissive Color`. Vanilla's is black, so vanilla's LOD emits nothing, and
  the sheet says so rather than a consumer having to know it.
- **pbr** — the source `.lodm`'s `emissive` texture, RAW; black where it names
  none. A pbr source has no vanilla quantity to fall back on.
- **`emissiveScale`**, both families — the MULTIPLE a consumer scales the
  sheet by. It cannot go in the sheet: a multiple may exceed 1 and the
  sheet is eight bits a channel, so folding it in would clip every emitter
  brighter than one. It is the source's emissive multiple where the source
  OWN-EMITS (Shader Flags 1 bit 22, or the BGSM's own flag) with a colour
  that is not black, and **0** otherwise — a set that emits nothing says so
  in one number. Where a source `.lodm` supplied the emissive picture, its
  own `emissiveScale` rides with it instead (1 when it names none): whichever
  law composed the sheet owns the multiple that goes with it.

A source `.lodm` of either family that names an `emissive` retargets it, and
it is read raw — the same treatment the third texture gets. `emissive` is the
key under both families; only the file suffix differs, `_g` where the rest of
the set is legacy and `_e` where it is pbr, so a directory listing says which
family a sheet belongs to.

**Legacy values, as the engine composes them.** Gloss = the material's
smoothness × the `_s` map's G (1 without a map). Specular = the `_s` map's R
(the normal's alpha without a map) × the specular strength, clamped. The
vanilla LOD sources DO name materials (`Materials\LOD\PreWarMapleGrLOD.BGSM`)
and carry an `_s` in slot 7; vanilla chunks carry `Commonwealth.Objects_s.DDS`
in theirs, smoothness 1, strength 1 (measured 2026-09-06). Never inverted,
never renamed: a consumer's legacy path lights it as the game does.

**Why these blocks.** A BC3 colour block quantises R, G and B together onto
one line of four colours per 4×4, so whatever shares it with the normal's X
and Y trades quality with them; the BC3 alpha block is a BC4 block, its own
eight-level ramp per 4×4, the best channel on the sheet. Height, the most
precision-hungry scalar, sits in the colour block only because the alpha was
wanted for sway (a card without sway is the thing people notice first about
LOD trees); its cross-talk with the normal is smooth against smooth. Sway is
the highest-contrast scalar and gets the ramp. AO, the smoothest, shares the
third texture's colour block with two channels that are nearly material
labels.

**The card's `_n` is BC7** (2026-09-23, bungo's ruling). On a card the
height is not a detail: it places every frame at its depth, and under BC3 it
came back 2.81 levels (34 units) wrong on average with 27 of 59 heights left,
which doubled and thinned the trunk. BC7 spends the same 16 bytes per 4x4 on
up to 16 index levels with 7-bit end points, and the in-tree encoder
(`src/lodgenbc7.h`) weights the height 32 to 1: 0.57 levels, p95 2, 57 of 58
heights. The price is the sway, which leaves its private alpha ramp (0.02 ->
1.34 levels mean). Numbers and the gate: `docs/LODGEN_CARD_SHEETS.md` §6.1.
Mesh `_n` sheets (source arrays, the atlas) and the aggregate `_n` stay BC3.

## `.lodm` — the LOD material

Envelope like PBRM's so a file is sniffable: ASCII `LODM`, uint32 version
(1), uint32 payload size, then that many bytes of **compact** UTF-8 JSON that
consume the rest of the file exactly. One flat object, only what a consumer
needs at load time:

```
lodm      1
family    "legacy" | "pbr"
kind      "source" | "card" | "array" | "cardArray" | "terrainVT"
textures  { diffuse|baseColor, normal, gsaos|rmaos,
            emissive }                                      game paths
card      { oct, frame [w,h], half [w,h], center [x,y,z], depthSpan, mips }
array     { class [w,h], layers [ source per layer ],
            emissiveScale [ one per layer ] }                      kind array
array     { class [w,h], oct, frame [w,h], mips, layers [ ... ],
            emissiveScale [ one per layer ] }                  kind cardArray
emissiveScale  the multiple for the emissive sheet; absent = 1
heightInBlue   true when a SOURCE normal's blue carries height
```

`kind: "terrainVT"` is the terrain virtual texture's INDEX and belongs to
`docs/LODGEN_TERRAIN_VT.md`, not here: it names a set of `.lodt` tile
containers and carries no textures at all. It reuses this envelope and this
parser unchanged; `family` is vestigial for it and `kind` is the
discriminator.

The generator **writes** one beside every set it makes (`kind` card or
array) and **reads** one as a source override (`kind` source, or absent):

- **Where a source `.lodm` lives** (`lodmSourceCandidate`): beside the shape's
  material with the `.lodm` extension — `materials\lod\foo.bgsm` →
  `materials\lod\foo.lodm` — or, where the shape names no material, at the
  diffuse's path under `materials\`: `textures\lod\foo_d.dds` →
  `materials\lod\foo_d.lodm`. Looked for in the RESOURCE STACK first
  (below), then loose in the data root (the CLI's `--data-root`, the bake's
  `WW_LODGEN_DATA_ROOT`), then through the game's resources (archives index
  `.lodm` like any material).
- **The resource stack** (2026-09-06k, `src/lodgen.h`): an ordered list of mod
  folders and archives in MOD ORGANIZER's order - the LAST entry overrides the
  earlier ones, and a loose file beats an archive wherever the archive sits in
  the list. Set by the panel's Source section (Specified, or Mod Organizer 2),
  by the CLI's repeatable `--resource`, or by `--mo2` from the profile's
  `plugins.txt`; the card bake takes it as `WW_LODGEN_RESOURCES` (`;`-separated)
  or `WW_LODGEN_MO2=1`, and `tools/bake_impostor_cards.sh` passes it to both the
  CLI and the GUI launch, so a mod's trees photograph with their own textures.
  `lodgen --probe <relpath>` prints which entry actually supplied a file.
- **What it does**: its textures replace the source's for that shape (an
  empty slot keeps the vanilla texture), its third texture is taken RAW, and
  its family names the set. A `pbr` source whose `.lodm` names no mask still
  photographs raw — the vanilla `_s` with R and G in their own places.
- **A set's family.** Arrays: per source, so a chunk can have both kinds.
  Cards: one per base — pbr only when EVERY textured shape of the model
  carries a pbr `.lodm`, else legacy (a legacy `.lodm` still retargets).

`emissiveScale` sits at the TOP LEVEL of a `source` or `card` file — one
set, one multiple — and as a list parallel to `layers`, `array.emissiveScale`,
on an `array` or `cardArray` one: two layers of one array are two materials
and do not share it. Absent means 1, so a hand-written source `.lodm` that
says nothing about emission behaves as it always did.

Reader/writer: `src/io/lodmfile.h`.

## Cards (octahedral impostors)

Files `Data\FO4CSLOD\Cards\<formid8hex>_oct_d.DDS`, `_oct_n.DDS`,
`_oct_gsaos.DDS`, `_oct_g.DDS` (legacy) or `_oct_bc.DDS`, `_oct_n.DDS`,
`_oct_rmaos.DDS`, `_oct_e.DDS`
(pbr), and `<formid8hex>_oct.lodm` beside them. One set per base. The crossed
quads in the mesh keep `<id>_fs.DDS` for the stock engine; a consumer reading
the manifest's `C` line opens the `.lodm` and draws the sheets instead.

**What FO4CS must decode (2026-09-23).** `_oct_n.DDS` is a DX10 DDS:
`dxgiFormat` **98** (`DXGI_FORMAT_BC7_UNORM`, linear, not sRGB),
`resourceDimension` 3, `arraySize` 1, the full mip chain; the card `_n`
ARRAY is the same with `arraySize` = the layer count. Direct3D 11 samples
BC7 in hardware, so this is a load change and no shader change: create the
texture with the format the header names. The other card sheets keep BC3 /
BC1. A set baked before this date carries a DXT5 `_n`; a loader that takes
the format from the header (DirectXTex `LoadFromDDSMemory` does) reads both.
Channel meaning is unchanged and the `.lodm` names files, not formats, so no
version moved.

**Source.** The base's own near model (the record's MODL), not a LOD
derivative: the candidate listing (`--list-impostor-candidates`, driver
`tools/bake_impostor_cards.sh`) prints it, falling back to the first filled
LOD slot only where a record names no model. bungo: "the base is more
detailed" — and it is, in the places a card can show: a Bethesda tree's near
mesh is itself a trunk plus branch cards (the maple: 279 triangles), but with
the 2048-class textures, the real `_s`, and the near materials' own alpha
thresholds (branches 150, bark 24), where the LOD mesh is a pre-baked crown
on 256 px textures with lighting burnt in. The bake hides the engine's own
in-cell detail steps, the shapes named `_L1`, `_L2`… beside the full shape,
which the engine draws one of and a bake would draw all of; and a
`BSMeshLODTriShape`, whose triangle list is [full][L1][L2] (the maple: 71 +
23 + 8) and which the viewer draws whole at its default level, is reduced
to its first range for the bake. The meta's `model` line and the `.lodm`'s
`card.source` name the file photographed; `hidden` and `ranges` lines say
what was set aside.
`--candidates missing|trees|all` picks the bases: far slots empty (the
default), every tree as well, or every LOD base; SCOL parts are walked to
their bases.

**Rings.** A card stands in where a ring's slot is empty, and, with
`--impostors-from-level N` (panel: "Cards from ring", FO4CS target only),
from MNAM level N on in place of the slot's mesh too: one quad per tree at
the near rings for a consumer that draws the sheets. One bake serves every
ring: the tile is the near ring's (128 px at ring 0 is pixel-matched for a
25 m tree at the loaded-cell edge) and the far rings read the mip chain.
Budget per tree type at 8 × 8 fitted frames: three sheets of 0.7 MB at
128 px, 2.9 MB at 256 px. The chunk report counts "placements on cards in
place of their ring's mesh"; the stock engine sees the crossed quads there.

**Frames.** N × N per sheet, frame (i, j) at pixel (i × frameW, j × frameH),
on the grid's VERTICES under the hemi-octahedral mapping:

    u = i / (N−1) × 2 − 1,  v = j / (N−1) × 2 − 1
    x = (u + v) / 2,  y = (u − v) / 2,  z = 1 − |x| − |y|,  normalised

so the four corners are exact horizon directions, the centre frame the exact
top, and every direction falls inside a triangle of three frames — the
(N−1)² triangle mesh between frame centres is the blending rule.

**The camera for a frame, and the 2026-09-19 repair.** Frame (i, j) is the
view FROM direction (i, j): a camera standing at that direction, looking at
the object's centre, orthographic. With this codebase's Euler convention
(`Matrix::fromEuler(rotX, 0, rotZ)`, whose ROWS are the camera's axes in world
space, so the camera sits at row2) that is

    elev = asin(d.z),  azim = atan2(d.y, d.x)
    rotX = −90 + elev,  rotY = 0,  rotZ = 270 − azim

because rotX = −90 + elev already gives cos X = sin(elev) and sin X =
−cos(elev), and row2 = d then requires sin Z = −cos(azim) and cos Z =
−sin(azim), which Z = 270 − azim satisfies exactly. Every bake before
2026-09-19 used `rotZ = 90 − azim`, which puts the camera at (−d.x, −d.y,
+d.z): the right elevation with the AZIMUTH TURNED BY 180 DEGREES, so frame
(i, j) held the view from the far side. **Every impostor set baked before that
date must be re-baked**; on anything not symmetric about its own axis, an old
set drawn to this spec shows the back of the object at the front.

A set says which bake made it with the CONVENTION TOKEN. The meta's `oct` line
carries it as a thirteenth token after `base` (`conv`, currently `spec1`), and
it is passed into the `.lodm` as `card.conv`, or per layer as the layer's
`conv` in a `cardArray`. It goes last, and is written only when the bake states
it, so every reader that indexes by position is untouched and every `.lodm`
produced before the token is byte-identical still. **An ABSENT token is not
"unknown": it means the set predates the repair**, because the token arrived in
the same change. A consumer may draw such a set under the old reading as a
diagnostic, and should say out loud that it is doing so, but the set is owed a
re-bake and nothing should be measured against it. Frames are
RECTANGULAR and one size for every view, fitted by
a first pass that photographs every view at the bound-sphere fit and takes
the widest and tallest extent from the centre over all of them (the sphere
fit left a maple's silhouette a third of the frame). Every frame keeps a
GUTTER of max(4, tile/16) transparent texels on each side, so no silhouette
touches a frame border; the recorded halfW/halfH span the full frame, gutter
included — the quad is the frame. Under every transparent texel every channel
of every sheet is DILATED from the silhouette's edge, frame by frame, then
flooded with the frame's average, the coverage alpha untouched, so filtering
and mips never pull black or neutral into an edge. Mips stop while a frame's
shorter side spans eight texels; the count is in the header and the `.lodm`.

**Frame SIZE CLASSES.** The longer side of a frame is the tile; the shorter
one is the silhouette's aspect quantised UP to a multiple of 16. It used to
be the aspect rounded to 4, which gave nearly every base a sheet size of its
own — and a card ARRAY can only hold sets that share a grid AND a frame, so
the array pass was grouping almost every base alone, which is the one thing
the arrays exist to avoid. In classes of 16 a worldspace's trees fall into a
handful of sheet sizes and share an array. Quantising up leaves the frame a
different shape from the silhouette, and the fix is to WIDEN THE RECORDED
EXTENTS to the frame's rather than stretch the picture into it: the
silhouette maps to the inner rect, so the extents take the inner rect's
aspect, whichever extent binds is left alone and the other grows, and the
object simply gets a little more air on one axis. `halfW`/`halfH` then carry
the frame's aspect exactly — a card quad is its frame, undistorted. The meta
says the class on a `class <w> <h>` line, and the `oct` line's frame size is
that class; the `oct` line's own shape did not move, because every reader of
it splits on spaces and indexes by position.

**Values per pixel.** Every channel render is averaged over the black
background on the way down to the frame, so a partially covered texel comes
out as its value times its coverage; the bake un-premultiplies each texel by
the coverage the matte measured and writes it wherever there is any
coverage (measured before the fix: 1.000 of the value at full coverage,
0.75 at three quarters, and nothing at all below half). The COVERAGE FLOOR
is 16/255: from there a texel counts as covered and carries its own values;
under it the un-premultiplied colour is the rounding of one or two source
pixels (±8 at 16, ±64 at 2, black as often as not) and the texel is
dilated over instead — its coverage alpha untouched, so a consumer that
tests low still gets the neighbours' colour there.


- colour: shader channel 12 — the base colour times the vertex colour and
  nothing else, through the two-pass matte over black and white,
  un-premultiplied; coverage = 1 − the passes' difference. Not the lit path
  with lighting off: that path tone-maps (a filmic curve) before it writes,
  and the first spec bakes were curved albedo; the crossed `_fs` cards were
  lit renders outright. The consumer lights the card.
- normal X, Y: the geometric normal in the VIEW's space (measured: opposite
  views differ, top and horizon agree), back faces flipped, half-packed.
- height: window depth of the orthographic bake; 0.5 is the card plane;
  units = (value − 0.5) × depthspan, with depthspan = 3 × max(bound radius,
  1024). Used for pixel depth offset, ghost-free frame blending, shadows and
  the model-to-card transition.
- sway: h² × (0.35 + 0.65 × r), h up from the view's own bottom row, r the
  radius from the silhouette's axis; 0 for rigid objects. The chunk builder's
  law, applied to the picture.
- R, G of the third sheet: shader channel 10 — the legacy pair composed
  from the vanilla material (above), or a source `.lodm`'s third texture raw.
- AO: from the height neighbourhood — the share of neighbours nearer the
  camera by more than a step, eight directions, four rings — multiplied by
  the third texture's own B when a `.lodm` supplied one.
- subsurface mask: a material label. Where any shape of the model carries
  the engine's tree-animation flag (a near tree: the branch cards do, the
  bark card does not), 1 on those shapes and 0 elsewhere; otherwise 1 where
  the shape is alpha-tested (a LOD tree's leaf cards), 0 where opaque
  (trunks, walls, rocks). A near tree is alpha-tested on every shape, bark
  included, so the alpha test alone would have called the whole tree a leaf.
  The meta says which rule ran (`mask tree|alpha`).
- the emissive sheet: shader channel 13 — the glow slot RAW where a source
  `.lodm` retargeted it (an EMPTY retarget binds black, which is how a pbr
  set that names no emissive says it emits nothing), else the vanilla glow
  rule above: `baseMap.rgb × baseMap.a × lodEmissiveColor` where the material
  is not alpha-tested, black where it is. `lodEmissiveColor` is the shape's
  own emissive colour and the renderer writes it UNCONDITIONALLY, unlike the
  sibling `glowColor`, which is only written while DoGlow and DoLighting are
  on — the bake photographs with lighting off, so a value written under those
  options would never reach the channel. Un-premultiplied and
  coverage-floored like every other channel, dilated at DDS time, written
  opaque: it ships as BC1. The MULTIPLE is not in the picture: the bake puts
  it on the meta's `emissive <scale> shapes <n>` line, the largest over the
  model's shapes, and `lodgenCard` copies it to the card `.lodm`'s
  `emissiveScale`.
- coverage is a FRACTION after the bake's downsample, not a cut-out: a twig
  thinner than a texel reads below 0.5. A consumer alpha-tests at 0.5 for
  full crowns and tests lower, or blends, for bare trees (the Commonwealth's
  maples are bare: the near model is twigs at threshold 80, best view 4-5%
  of its frame against the LOD's pre-baked crown at 14%).

**Sidecar** (`<id>.txt`, one line per photograph): `model <file>`, `hidden
<shape>` per detail step hidden, `front …`, `side …`, `class <w> <h>`, `emissive <scale> shapes <n>`,
`oct N frameW frameH halfW halfH cx cy cz depthspan family`, and one
`lodm <candidate> <family|none|rejected> <diffuse>` per textured shape, so a
bake says what it looked at and what it looked for.

**Manifest**, after the row of every placement standing on a card:
`C index cx cy cz halfW halfH N depthspan lodm` — the tenth token is the set's
`.lodm` game path. Where the card sits in a sheet array (below) two more
follow: `… lodm arrayLodm layer`. A reader that stops at the tenth token is
unaffected, which is why they go on the END and not in the middle.

### Card sheet arrays

The per-card sets are one texture bind per tree type. `--arrays` together with
`--impostors` packs every card set the chunks' `C` lines stand on into one
DX10 array per texture (BC3; BC7 for `_n`; BC1 for the emissive), grouped by FAMILY and
by SHEET SIZE (a card set's
sheet is `oct x frameW` by `oct x frameH`; sets that differ in grid or frame
cannot share an array — which is what the frame size classes above are for),
so a consumer draws a whole ring's trees as one
instanced quad per chunk:

    Textures\Terrain\<ws>\Objects\<ws>.LodgenCards.legacy.<WxH>_d.DDS, _n.DDS, _gsaos.DDS, _g.DDS
                                     <ws>.LodgenCards.pbr.<WxH>_bc.DDS, _n.DDS, _rmaos.DDS, _e.DDS
                                     <ws>.LodgenCards.<family>.<WxH>.lodm

A card set baked before the emissive existed gets a BLACK layer, so a layer
index still means what the `C` lines say it means.

The `.lodm` is `kind` **cardArray**: `family`, the four sheets, and an
`array` object with the size `class`, the `oct` grid, the `frame` size, the
`mips` cap, an `emissiveScale` list and one `layers` entry per layer — the
card's `id` (the base's
form ID), its `half` extents, `center`, `depthSpan` and `source` model. The
grid, the frame and the mip cap are shared by every layer of an array; the
geometry and the emissive multiple are per layer, because two trees of the
same sheet size are neither the same size in the world nor lit the same.

A layer is built the way `lodgenCard` builds a set's own sheets — from the
bake's PNGs, dilated frame by frame, mips stopping while a frame's shorter
side spans eight texels — so a layer holds what the per-card DDS holds, not a
re-encoding of it. The per-card sets stay beside the cards for a consumer
without arrays, and the crossed `_fs` quads stay in the mesh for the stock
engine: three readings of the same bake, none of them required.

Runs LAST, after the merge: it only reads the manifests' `C` lines and
appends to them.

Gate: `tests/spells/lodgen_card_arrays.sh` — two synthetic sets, one red and
one blue with a green emissive on both, on the Sanctuary cells; the four
array files, the `cardArray` `.lodm`,
the two new `C` tokens, each layer's own colour decoded off the BC3
endpoints, the green decoded off the BC1 ones, and each layer's own
`emissiveScale` (1 on the red set, 0 on the blue) out of the list.

Gate: `tests/spells/lodgen_octahedral.sh` — two real bakes, N=4: the game's
maple (legacy) and the same maple under pbr source `.lodm`s in a loose root
(R and G swap against the legacy bake; the albedo retargets). The frame's
size class and the exact extent aspect are checked there, and the emissive
from both sides: black on the legacy bake, because every shape of the near
maple is alpha-tested, and the colour sheet on the pbr bake, whose fixture
names the material's own diffuse as its emissive — without the second, the
first is a channel that never writes. The `emissiveScale` both ways too: 0
on the legacy bake, because no shape of the near maple own-emits with a lit
colour, and the 2.5 its own fixtures name on the pbr bake — without the
second, the first is a field that is always zero.

## Mesh LOD (texture arrays)

Files `Textures\Terrain\<ws>\Objects\<ws>.LodgenArrays.<WxH>_d.DDS`, `_n.DDS`,
`_gsaos.DDS`, `_g.DDS` (legacy) and `<ws>.LodgenArraysPBR.<WxH>_bc.DDS`,
`_n.DDS`, `_rmaos.DDS`, `_e.DDS` (pbr): one DX10 array per texture size class
AND family over
every source the chunks reference (BC3, and BC1 for the emissive), tiling or
not, real mips, the layer in
every vertex's UV2.y, a `<stem>.lodm` beside each set (its `array.layers`
lists the source colour texture per layer, and `array.emissiveScale` one
multiple per layer), an `A <shape block> <layer>
<lodm>` line per shape in the chunk's manifest, `M <shape block> <material>`
lines naming each chunk shape's source material (the chunk shape itself
names none, as vanilla's do not), and a sidecar `<ws>.LodgenArrays.txt`
(version 5: `family class layer lodm color normal mask emissive source
emissiveScale` — the new column goes on the END, so a reader that indexes
the first nine by position is unaffected). Run
BEFORE the atlas; the stock engine reads none of it.

- `_d` / `_bc`: the source's colour with its alpha.
- `_n`: the source normal's X and Y; height NEUTRAL (128) unless a source
  `.lodm` says `heightInBlue`; sway 0 — a mesh carries its sway per vertex.
- `_gsaos`: gloss and specular composed as above from the chunk shape's
  slot 7, smoothness and strength (the generator carries the source's into
  the chunk, as vanilla chunks carry theirs); AO neutral (255), the chunk
  carries it per vertex; subsurface mask 1 for a source an alpha-tested shape
  uses.
- `_rmaos`: the source `.lodm`'s third texture raw; mask as above.
- `_g` / `_e`: the emissive, by the laws above — the colour times its own
  alpha times the SOURCE'S EMISSIVE COLOUR for a legacy source that is not
  alpha-tested, black for one that is, black for one whose emissive colour is
  black (which is every measured vanilla LOD material), and a source
  `.lodm`'s `emissive` texture raw for anything that names one.
  The sidecar's `emissive` column says which happened: the emissive texture's
  path where a `.lodm` named one, the COLOUR texture's path where the glow
  rule composed it, and `-` where the layer emits nothing. The
  `emissiveScale` column and `array.emissiveScale` carry the multiple, read
  off the chunk shape the generator wrote the source's emission into: the
  chunk builder carries `Emissive Color`, `Emissive Multiple` and the
  Own-Emit bit from the source the way it carries slot 7, smoothness and the
  specular strength, and `lodgen --dump-shapes <file.BTO>` prints them back
  so a gate can check a layer against its SOURCE rather than against the pass
  that wrote it.

Gate: `tests/spells/lodgen_texture_arrays.sh` — the game's sources (legacy),
then a loose root with one pbr source `.lodm` (its `_rmaos` layer equals its
`_bc` layer block for block, its `_e` layer decodes to the same, and its
`emissiveScale` of 2.5 reaches both the sidecar column and the `.lodm`). The
emissive is decoded, not just headed: a glow-rule layer must equal its
diffuse times that diffuse's alpha times the source's emissive colour and NOT
the plain diffuse; a layer the sidecar says emits nothing must decode black;
and where an OPAQUE source's emissive colour is black, that layer must decode
black **while its own diffuse × alpha does not** — the half of the check that
fails if the colour multiply is dropped. Every layer's `emissiveScale` is
checked against `lodgen --dump-shapes` on the written chunk.

## Chunk shapes — the atlas `_s` sheet, and the merge

The generator builds one chunk shape per source material, which is one draw
call per material: Sanctuary (-20,24) at dim 4 came out with ten where the
vanilla chunk has three. Two passes bring that down, in this order.

**The atlas `_s` sheet.** `--atlas` already packs the chunks' non-tiling
diffuses and normals onto `<ws>.LodgenObjects.DDS` and `_n.DDS`. It now
composes a third, `<ws>.LodgenObjects_s.DDS`, **BC5** like vanilla's
`Commonwealth.Objects_s.DDS`, from each cell's slot-7 map with the SHAPE'S
OWN constants folded in: R = the map's R × the specular strength, G = the
map's G × the smoothness. Every atlased shape then carries slot 7 = the sheet
and smoothness 1 / strength 1, which is what vanilla's chunks carry, and — the
point — two shapes that differed only in their constants become identical to
the engine and can merge. The three sheets are written under
`<tex-dir>/Objects`, the directory the game path baked into the shapes names.

**The merge** (`lodgenMergeChunkShapes`, `--merge`, on by default in region
mode; `--no-merge` keeps one shape per source material). Runs after the atlas
and the arrays and concatenates every shape a chunk holds that the engine
cannot tell apart: same name, same ten texture slots, same alpha property,
same shader type, flags and constants, same vertex descriptor, and the same
array `.lodm`. Vertices and triangles are concatenated per SEGMENT so a
merged shape keeps its dim × dim segment grid, bounds take the union, the
merged-away branches go and the root's child list is rebuilt without holes.
A merge never crosses 65535 vertices; a group past that stays split.

**`A <block> -1 <lodm>`.** A merged shape can span array layers, so the
manifest's `A` line takes layer **−1** to mean *per vertex, in UV2.y* — the
place the layer has always been. A positive layer still means the whole shape
is on that one, so a consumer can keep its fast path.

**That −1 needs UV2, and since 2026-09-12 a default bake has none.** Object
identity is off by default now (bungo's ruling), so a default `.BTO` carries
the plain descriptor `474989027590661` — no vertex colours, no UV 2 — while the
manifest, the arrays and the cards are still written. A merged shape that
spanned layers would then write `A <block> -1 <lodm>` with nothing per vertex
to resolve it. Measured, that case does not occur: a dim-16 bake of the
Sanctuary chunk with `--arrays --impostors --slot-fallback` writes 21 `A` lines
with identity off and the same 21 with identity on, every one of them a
POSITIVE layer, because the merge key already holds the array `.lodm` and in
practice the shapes that merge sit on one layer. It is a latent hole, not a
live one: a consumer that meets `-1` in a file baked without `--identity`
should treat the shape as unresolved rather than read UV 2 that is not there,
and the durable fix — putting the layer in the merge key so a merged shape can
never span layers — is bungo's call, not a lane's.

Gate: `tests/spells/lodgen_merge.sh` — the region built twice, `--no-merge`
and `--merge`, compared file to file: fewer shapes, the same vertices and
triangles in total, dim × dim segments, every `A` line's layer matching its
vertices, the `_s` sheet on disk as a BC5, slot 7 and constants 1/1.

**Far rings — proxy meshes** (`lodgenSimplifyFarRings`, `--no-simplify` /
`--simplify8|16|32 R` / `--simplify-error UNITS`, the panel's *Far-ring
simplification*, on by default under BOTH targets). Runs LAST, after the merge,
because the merge has already made one shape per material and that shape IS the
cluster a far ring wants one simplified mesh of. Ring 0 (dim 4) is never
touched; ring 1 is 1.00 by default, ring 2 keeps 0.35 of its triangles and ring
3 keeps 0.20.

The cut is per GROUP, keyed by (object identity index, array layer). Both are
INDICES, not quantities — an interpolated identity is a different object and an
interpolated layer is a different texture — and grouping by them means no
collapse can cross either. meshoptimizer creates no vertices, so the survivors
are a SUBSET of the originals and every channel of
`docs/LODGEN_VERTEX_PACKING.md` arrives intact rather than blended; the four
that are quantities (normal, UV, sky visibility in UV2.x, AO in colour B, sway
in colour A, ground contact in Eye Data) ride as weighted attributes so the
metric keeps them meaningful too. Each group is asked for at least two
triangles and its originals are restored if the simplifier returns nothing, so
**the set of identity indices in a chunk is the same before and after**: no
object can leave a ring.

A shape with an **alpha property keeps every triangle** — a cut-out card is
four vertices that spell a silhouette, and a collapse spends the silhouette to
save nothing. That covers the impostor quads and the crossed quads inside
vanilla's own tree LOD models alike, and the impostor cards are excluded a
second time by object index off the manifest's `C` lines so the rule is
checkable. Groups of eight triangles or fewer keep every triangle.

Afterwards the segments are regrouped by the cell of each triangle's CENTROID
(the generator assigns them per placement, which stops being the unit once
triangles move), the vertex array is compacted to the survivors, and the
bounding sphere and the node's multi-bound AABB are recomputed. The manifest is
not rewritten: no row's meaning changed.

The error bound is in WORLD units at ring 0 and is scaled by the ring's dim, so
it is the same on-screen error at every ring — and because a chunk shape's
vertices are miniatures at `Scale = dim`, that scaling cancels and the bound is
a constant in the file's own units. The simplifier stops early rather than
exceed it, so the ratios are targets and the bound is the rail.

**Why a far ring is empty without help.** Measured over `Fallout4.esm`: only
456 of 28,932 LOD-bearing bases fill MNAM slot 2 and only 51 fill slot 3, and
over the chunk that covers Sanctuary at ring 2, (−32,16), **0 of 19,507
references** has a base that fills slot 2 (0 of 148,362 at ring 3). Vanilla
ships 20 dim-16 chunks and 4 dim-32 chunks for the whole Commonwealth, against
344 at dim 4, and no dim-16 chunk over Sanctuary at all. So a far ring holds
something only with `--slot-fallback` (the panel's *Use a nearer LOD slot when
the ring's is empty*, 2,518 refs at ring 2) or with impostor cards — and since
cards are excluded from the cut by design, the fallback is the case this pass
exists for: it is what makes "far chunks grow to many times vanilla's size"
affordable.

**The atlas format.** Vanilla's diffuse sheet is **DXT1**, not BC3 — measured
on the shipped `Commonwealth.Objects.DDS`: 4096×2048, 13 mips, fourCC `DXT1`,
5,592,552 bytes (its `_n` and `_s` are both `BC5U`). `--atlas-bc1`, which the
panel selects for the **stock target**, writes ours the same way: BC1 with
one-bit punch-through alpha for the cut-outs, carried down the whole mip chain,
half the memory of BC3. FO4CS keeps BC3 for its eight-bit alpha. The `_s` sheet
stays BC5 either way.

Gate: `tests/spells/lodgen_farring.sh` — rings 0, 1 and 2 built twice each,
`--no-simplify` against the pass, compared shape by shape through
`lodgen --dump-geometry`: the triangles within 10 points of the ratio, the
vertices down with them, the identity sets equal, the segment count unchanged
with no centroid in another cell or outside the chunk, every vertex inside its
bounding sphere and its node's AABB, the manifest's placement rows unchanged,
ring 0 byte-identical, and the atlas DXT1 under `--atlas-bc1` and DXT5 without
it with punch-through blocks surviving past the top mip. Ring 3 is opt-in
(`FARRING_RING3=1`): 21,498 references before SCOL expansion.

## What is not in the textures

Sway on meshes (vertex alpha), sky visibility and ground blend (UV2.x, Eye
Data), identity (R+G) and AO (B) on meshes: `docs/LODGEN_VERTEX_PACKING.md`.
Thickness for translucency, curvature for surface states, porosity: not
carried; the first is the upgrade if backlit canopies want it, the other two
derive or are invisible at ring three and four.

## The frame law: one resolution, two ladders

**The default is 8 x 8 frames at 256 px a frame -- a 2048-texel sheet for the
largest base** (bungo 2026-09-23, "8x8 at 2k"; lane DEFAULTS2). Until then it was
8 x 8 at 128 px, a 1024-texel sheet. The grid was already 8 everywhere; only the
frame moved: the driver's `TILE` default, the panel's *Card resolution* default
and the bake hook's fallback when `WW_IMPOSTOR_TILE` is unset are all 256 now.
`TILE=128` (or the panel row) is the way back. The ladders below are unchanged.

The resolution chosen in the panel (or `TILE=` on the driver) is what the run's
LARGEST base gets. Every other base comes down from it by two independent
quantisations, each picking the nearest rung in log space.

**Size**, from `WW_IMPOSTOR_REF` — the largest extent among the run's candidates,
which `--list-impostor-candidates` reports in its second column. Pure halving,
three rungs at most, floored at 32 px:

| this base against the largest | long side |
|---|---|
| 1 | the resolution |
| 1/2 | half |
| 1/4 | a quarter |
| 1/8 or smaller | an eighth |

With no reference set there is no size ladder and every base bakes at the
resolution.

**Aspect**, from the silhouette the bake has just measured over every view: the
short side is the smallest **multiple of 16** texels whose INNER rect (the frame
less its margin on both sides) is not narrower than the silhouette's ratio
(`src/nifskope_ui.cpp`, `for ( int s = 16; s <= tileLong; s += 16 ) {`; the
derivation is `docs/LODGEN_CARD_SHEETS.md` §3.2). Until 2026-09-09 it was a
five-rung ratio ladder (1, 3/4, 1/2, 3/8, 1/4, rounded to a multiple of four);
that ladder is retired, because its floor forced a SQUARE frame on a
needle-shaped tree (TreeBlasted05 filled 4 texels of a 32-texel frame, 12.5%).

Both quantisations are coarse ON PURPOSE. A card array holds only sets sharing a
grid AND a frame, so each additional frame shape is another array and another
bind. The size ladder's nearest-in-log bounds its rounding to about 15%, and
both fits GROW whichever extent is loose rather than cropping — the cost is air inside a frame, never a
cut silhouette.

A set records `card.oct`, `card.frame` and `card.base` (the run's resolution). A
frame below the base is a rung, not a different run.

## Half-resolution auxiliary sheets

`lodgen --card-half-aux` writes the normal, mask and emissive sheets at half of
each side and leaves the base colour alone. Measured across a two-layer array
set (the card-array gate's, `gap 4`, a 16x32 sheet of 8x16 frames), the payload
falls from 4,480 to 1,920 bytes: 42.9% (`docs/LODGEN_CARD_SHEETS.md` §3.5). It
read 46.4% on 2026-09-06; the difference is the mip law, not the saving.

The base colour never divides. Its alpha is the coverage, so it is the
silhouette, and that is what an impostor is judged on. The other three are
lit-appearance data at LOD distance.

The divide happens after frame dilation. The gap is rounded up to an EVEN number
of texels so a halving lands its margins on whole texels, and frames are
multiples of 16, so a halved frame is still even and nothing mixes across a
frame border. Sampling is unaffected — normalised UV reads a
smaller sheet with the same coordinates — but a consumer sizing its own
allocation reads `card.auxDiv` / `array.auxDiv`, `array.auxClass` and
`array.auxMips`.
