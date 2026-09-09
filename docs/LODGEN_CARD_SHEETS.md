# Impostor card sheets v1 — per-card sets and card arrays

**Contract version: card sets carry `kind: "card"` `.lodm` v1; card arrays carry
`kind: "cardArray"` `.lodm` v1.**
**Status: SHIPPED, gated, never flown in a game.** Bake:
`WW_IMPOSTOR_OCT` / `WW_IMPOSTOR_TILE` in `src/nifskope_ui.cpp` driven by
`tools/bake_impostor_cards.sh`; conversion to DDS + `.lodm` in
`lodgenCard()`; packing into arrays in `lodgenBuildCardArrays()`, both in
`src/lodgen.cpp`.

This page is the **contract for the picture files**: names, grids, formats,
what each channel holds, what a consumer must do to sample one. The material
sidecar that describes a set is `docs/LODGEN_LODM_FORMAT.md`; the manifest lines
that point at a set are `docs/LODGEN_MANIFEST_FORMAT.md` §4. The design record
and the measurements are `docs/LODGEN_IMPOSTOR_SPEC.md`.

---

## 1. Two shapes on disk

### 1.1 Per-card sets — one per base

```
Data\Textures\Lodgen\Cards\<formid8hex>_oct_d.DDS      legacy   BC3
                           <formid8hex>_oct_n.DDS               BC3
                           <formid8hex>_oct_gsaos.DDS           BC3
                           <formid8hex>_oct_g.DDS               BC1
                           <formid8hex>_oct.lodm                kind "card"

                           <formid8hex>_oct_bc.DDS    pbr      BC3
                           <formid8hex>_oct_n.DDS               BC3
                           <formid8hex>_oct_rmaos.DDS           BC3
                           <formid8hex>_oct_e.DDS               BC1
                           <formid8hex>_oct.lodm                kind "card"
```

`<formid8hex>` is the **base** record's form ID, 8 lowercase hex digits. One set
per base, whatever ring it serves: the tile is the near ring's size and the far
rings read the mip chain. The crossed `_fs.DDS` quads stay in the mesh for the
stock engine and are not part of this contract, but they are written **BC3
(DXT5)**, header `dwFlags 0x000A1007`, `pfflags 0x4`, `caps 0x401008` -- byte for
byte the header of vanilla's own alpha-tested tree LOD textures
(`Textures/LOD/Trees/MapleBranchesLOD_d.dds`, `ElmBranchesLOD_d.dds`). They went
out as **BC1 with no alpha** until 2026-09-09, which is why every card quad in a
chunk drew as an opaque square.

The bake's own intermediate is a set of PNGs
(`<id>_oct_albedo.png`, `_oct_normal.png`, `_oct_gsaos.png` / `_oct_rmaos.png`,
`_oct_g.png` / `_oct_e.png`) plus a `<id>.txt` sidecar (§5). Those are inputs to
`lodgenCard` and to the array packer, not a shipped format.

### 1.2 Card arrays — one per (family, sheet size)

```
Data\Textures\Terrain\<ws>\Objects\<ws>.LodgenCards.legacy.<SW>x<SH>_d.DDS
                                   <ws>.LodgenCards.legacy.<SW>x<SH>_n.DDS
                                   <ws>.LodgenCards.legacy.<SW>x<SH>_gsaos.DDS
                                   <ws>.LodgenCards.legacy.<SW>x<SH>_g.DDS
                                   <ws>.LodgenCards.legacy.<SW>x<SH>.lodm
                                   <ws>.LodgenCards.pbr.<SW>x<SH>_bc.DDS  … _e.DDS, .lodm
```

**`<SW>x<SH>` is the WHOLE SHEET size** (`oct·frameW` by `oct·frameH`), not the
frame and not a per-layer texture class. That differs from the mesh texture
arrays, where `<WxH>` *is* the per-layer texture size
(`docs/LODGEN_TEXTURE_ARRAYS.md`) — the two names look alike and mean different
things.

A group holds only sets that share **family, `oct` and `frame`**, which is
exactly the sets that share a sheet size. That is what the frame size classes of
§3.2 exist to produce.

The array pass runs **last**, after the merge. It reads the chunks' `C` lines,
packs the sets they stand on, and appends two tokens to each such line. The
per-card sets stay on disk beside the arrays for a consumer without arrays.

---

## 2. The grid — N × N frames, N² views

`oct = N` is **frames per side**. The sheet is `N` frames wide and `N` frames
tall and therefore holds **N² views**: `OCT=8` is **64 views, not 81**. The
`(N+1)²` reading comes from thinking of `N` as a subdivision count; it is not.
`N` is 4, 6 or 8 in the panel, and the bake accepts 2 … 16.

Frame `(i, j)`, `i, j ∈ 0 … N−1`, occupies pixels
`[i·frameW, (i+1)·frameW) × [j·frameH, (j+1)·frameH)`.

Its view direction, under the hemi-octahedral mapping:

```
u = i/(N−1)·2 − 1
v = j/(N−1)·2 − 1
x = (u + v)/2
y = (u − v)/2
z = 1 − |x| − |y|
n = normalise( x, y, z )
```

Frames sit on the grid's **vertices**, so the four corners are exact horizon
directions and the centre frame is the exact top. Every direction falls inside a
triangle of three frame centres; that `(N−1)²` triangle mesh **is** the blending
rule — pick the three nearest frames, blend with the height channel for
parallax, write depth for the true silhouette.

**Frames are rectangular and one size for every view.** The bake makes two
passes: the first photographs every view at the bound-sphere fit and takes the
widest and tallest silhouette extent from the centre over all of them; the second
bakes at that fit.

---

## 3. Frame geometry

### 3.1 The gap, the padding, and the mips they buy

**The number is the distance between two neighbouring silhouettes**, not the
margin on one side of a frame. bungo, 2026-09-09, verbatim: *"When I say padding
8 for 1k, it's 8 pixels of distance between two rendered objects."* On a 1024
sheet of 8x8 frames, two trees are 8 texels apart across the border the two
frames share, so each frame keeps HALF of that as its own margin:

```
gap(side) = max( 2, side / 16 ), rounded UP to even
pad(side) = gap(side) / 2                       // on EACH side of a frame
gapX = gap(frameW)   gapY = gap(frameH)
padX = gapX / 2      padY = gapY / 2
inner rect = (frameW - gapX) x (frameH - gapY)
mips = 1 + log2( min( gapX, gapY ) )
```

The inner rect is **15/16 of the frame** wherever a side is a multiple of 32 --
120 of 128, 240 of 256 -- which is his 8-on-1024 and 16-on-2k exactly. A side of
48, 80 or 112 rounds the gap up to even and gives back at most one texel.

**Where the mip count comes from.** Mip CONSTRUCTION never mixes frames -- the
box filter halves an even frame into an even frame -- so the bleed is at SAMPLE
time: a tap on a frame's own UV border reads half of that frame's last texel and
half of the neighbour's first. What has to survive at level `k` is therefore the
SEPARATION between the two silhouettes that meet on the border, `gap / 2^k`, and
the sheet ships every level where that is still a whole texel: `1 + log2(gap)`
levels and never the one after. A 128-texel frame gets 4 (128, 64, 32, 16).

**The sheet's outer border needs only half a gap.** There is no neighbouring
frame beyond it, so padding every frame by `gap/2` is already correct at the
edge and no special case exists: an interior border carries `gap/2` from each of
the two frames that meet on it, an outer border carries `gap/2` and faces the
sheet edge. That holds because the sheet is sampled **clamped** -- a card quad's
UV rect is a sub-rect of the sheet, and neither the DDS nor the `.lodm` asks for
wrapping. Under WRAP the outer border would face the opposite edge's frames and
would need a whole gap; nothing in this format does that.

Floored at 2 so every card ships at least two levels; rounded up to even so the
gap splits into two whole texels of margin.

| frame | gapX, gapY | padX, padY | inner rect | mips | gap at the last mip |
|---|---|---|---|---|---|
| 16x64 | 2, 4 | 1, 2 | 14x60 | 2 | 1 texel |
| 32x32 | 2, 2 | 1, 1 | 30x30 | 2 | 1 texel |
| 32x64 | 2, 4 | 1, 2 | 30x60 | 2 | 1 texel |
| 48x64 | 4, 4 | 2, 2 | 44x60 | 3 | 1 texel |
| 96x128 | 6, 8 | 3, 4 | 90x120 | 3 | 1.5 texels |
| 128x128 | 8, 8 | 4, 4 | 120x120 | 4 | 1 texel |
| 256x256 | 16, 16 | 8, 8 | 240x240 | 5 | 1 texel |

**What this replaced.** Twice, both on 2026-09-09. Before either: one gutter
`max(4, tileLong/16)` on all four sides and a chain that ran *while a frame's
shorter side spanned eight texels* -- two unrelated rules, which shipped a
BLEEDING level on every card of 96 texels or more (measured on the 19-tree
Sanctuary library: mip 4, gutter half a texel, 26/255 of a neighbouring frame's
alpha across 16 of 28 frame borders). Then lane CARDFIT3 read the number as the
PER-SIDE margin, `pad = max(2, side/16)`, which spent twice the texels bungo
asked for and left the inner rect at 7/8 of the frame instead of 15/16. The mip
COUNT is the same under both readings, because the count was always the gap's.

`halfW`/`halfH` in the `.lodm` span the full frame, padding included -- **the
quad is the frame** -- and a consumer that insets by the padding shrinks the
object. The silhouette occupies the INNER rect, `frame - 2*pad`, and the `.lodm`
says both numbers (`card.pad` / `array.pad`, the margin on EACH side; `card.gap`
/ `array.gap`, twice it) so a reader never has to re-derive the law or guess
which quantity a single number meant.

Under every transparent texel, every channel of every sheet is **dilated** out
from the silhouette edge, frame by frame, then flooded with the frame's average;
the coverage alpha is left alone. So filtering and mips never pull black or a
neutral value into an edge, and a consumer that tests below 0.5 still gets real
colour there. The dilation is `max(8, max(frameW,frameH)/8)` passes deep, which
is at least a whole gap on every frame shape above -- so a coarse mip that
averages across the margin still averages the tree's own colour.

### 3.2 The frame's aspect, and why the extents grow

The longer side of a frame is the tile rung (§3.3). **The shorter side is the
smallest multiple of 16 texels whose INNER rect is not narrower than the measured
silhouette**, never below 16 and never above the long side. The loop grows the
frame until the silhouette fits, so the loose axis gets air and the binding one
is never cropped.

Quantising the shorter side leaves the frame a slightly different shape from the
silhouette, and the fix is to **widen the recorded extents to the frame's aspect**
rather than stretch the picture: whichever extent binds is left alone, the other
grows, and the object gets a little more air on one axis. `halfW`/`halfH`
therefore carry the frame's aspect exactly and a card quad is undistorted.

Multiples of 16 rather than a continuum because a card array can only hold sets
sharing a grid **and** a frame; every extra shape is another array and another
bind. Over the 19-tree Sanctuary library it is **seven shapes against the four**
the old ladder produced, for **2.5% less** total sheet area.

**What this replaced** (before 2026-09-09): a five-rung ratio ladder
`{1, 3/4, 1/2, 3/8, 1/4}` for the short side, clamped to
`max(32, 2G+4)`. A 1:5.5 tree asks for a rung below a quarter and there was
none, and a tree on the 32-texel rung of the size ladder had
`tileLong == shortFloor == 32` and got a SQUARE frame whatever its shape:
TreeBlasted05's silhouette filled **4 texels of 32**, 12.5%.

### 3.3 Two ladders, both coarse on purpose

**Size**, from `WW_IMPOSTOR_REF` (the largest extent among the run's candidates,
column 2 of `--list-impostor-candidates`). Pure halving, three rungs, floor
32 px: 1 → the resolution, 1/2 → half, 1/4 → a quarter, 1/8 or smaller → an
eighth. With no reference set there is **no** size ladder and every base bakes at
the resolution.

**Aspect**, from the measured silhouette: §3.2, the smallest multiple of 16
texels whose inner rect does not crop it. (It was a five-rung ratio ladder until
2026-09-09; that ladder's floor is the defect §3.2 records.)

The size ladder is nearest-in-log, which bounds its rounding to about 15%, and
both fits **grow** whichever extent is loose rather than cropping: the cost is
air inside a frame, never a cut silhouette.

`card.base` records the run's resolution. **A `frame` below `base` is a rung and
is correct** — it is not a mismatch and a reader must not refuse on it.

### 3.4 Mips

`mips = 1 + log2(min(gapX, gapY))` -- **the gap decides it**, and §3.1 is the
derivation. The count is in the DDS header and in the `.lodm`'s `mips`. It is a
cap on the whole sheet's chain: a sheet mip halves every frame at once, and the
chain stops at the last level where a whole texel of gap still separates the two
silhouettes that meet on a border, because the next one has them touching.

### 3.5 `--card-half-aux`

Writes the normal, mask and emissive sheets at **half of each side** and leaves
the base colour alone. Measured across a two-layer array set the payload falls
from 3,584 to 1,664 bytes, **46.4%**.

The base colour never divides: its **alpha is the coverage**, so it is the
silhouette, and that is what an impostor is judged on.

The divide happens **after** frame dilation, and the gap is rounded up to an
EVEN number of texels precisely so a halving lands its margins on whole texels;
frames are multiples of 16, so a halved frame is still even and nothing mixes
across a frame border. The aux sheets' own mip count comes down with their gap:
`auxMips = 1 + log2(min(gapX,gapY)/auxDiv)`. On the smallest frames -- 16 and 32
texels, where the gap is already at its floor of 2 -- that division reaches 1 and
the aux sheets ship a SINGLE level: a fallback naming itself rather than a silent
bleed. Sampling is unaffected — normalised UV reads a
smaller sheet with the same coordinates. A consumer needs `card.auxDiv` /
`array.auxDiv`, `array.auxClass` and `array.auxMips` only to size its own
allocation.

---

## 4. What each channel holds

Formats and channel roles are the `.lodm` family contract
(`docs/LODGEN_LODM_FORMAT.md` §2.1). What is specific to a card:

| channel | law |
|---|---|
| colour RGB | the base colour × the vertex colour and **nothing else** — the unlit path, not the lit path with lighting off (that one tone-maps before it writes). The consumer lights the card |
| colour A | **coverage**, from a two-pass matte over black and white: `coverage = 1 − (passes' difference)` |
| normal R, G | the geometric normal in the **VIEW's** space, back faces flipped, half-packed. Opposite views differ; top and horizon agree |
| normal B (height) | window depth of the orthographic bake. **0.5 is the card plane**; `units = (B − 0.5) × depthSpan`, with `depthSpan = 3 × max(boundRadius, 1024)`. Drives pixel depth offset, ghost-free frame blending, shadows and the model-to-card transition |
| normal A (sway) | `h² × (0.35 + 0.65·r)`, `h` up from the view's own bottom row, `r` the radius from the silhouette's axis; **explicit 0 for rigid objects** |
| mask R, G | legacy: gloss = smoothness × the `_s` map's G, specular = the `_s` map's R × specular strength, **never inverted**. pbr: the source `.lodm`'s third texture **raw** |
| mask B (AO) | from the height neighbourhood — the share of neighbours nearer the camera by more than a step, eight directions, four rings — multiplied by the third texture's own B when a `.lodm` supplied one |
| mask A (subsurface) | a material **label**. 1 where any shape of the model carries the engine's tree-animation flag, else 1 where the shape is alpha-tested and 0 where opaque. The sidecar says which rule ran (`mask tree` \| `mask alpha`) |
| emissive RGB | legacy: `baseMap.rgb × baseMap.a × lodEmissiveColor` where the material is **not** alpha-tested, black where it is. pbr: the source `.lodm`'s `emissive` raw; an **empty** retarget binds black, which is how a set says it emits nothing. Written opaque, BC1. The **multiple** is not in the picture — it is `emissiveScale` in the `.lodm` |

**Un-premultiplication and the coverage floor.** Every channel render is averaged
over a black background on the way down to the frame, so a partly covered texel
arrives as its value × its coverage. The bake divides each texel by the measured
coverage wherever there is any. The **coverage floor is 16/255**: from there a
texel counts as covered and carries its own values; under it the
un-premultiplied colour is the rounding of one or two source pixels and the texel
is dilated over instead — **its coverage alpha untouched**.

**Coverage is a fraction, not a cut-out.** After the bake's downsample a twig
thinner than a texel reads below 0.5. A consumer alpha-tests at 0.5 for full
crowns and tests lower, or blends, for bare trees.

---

## 5. The bake sidecar `<id>.txt`

One line per fact, written by the bake, consumed by `lodgenCard`. Not a shipped
format, but a reader of a bake tree needs it:

```
model <file>                 the model actually photographed
hidden <shape>               per detail step (_L1.._L9) set aside
ranges …                     a BSMeshLODTriShape reduced to its first range
front … / side …             the crossed-quad cards
class <w> <h>                the frame size class
gap <x> <y>                  the distance in texels between two neighbouring
                             silhouettes across a frame border, per axis; the
                             margin on each side of a frame is half of it
emissive <scale> shapes <n>  the largest emissive multiple over the model's shapes
oct N frameW frameH halfW halfH cx cy cz depthSpan family base
lodm <candidate> <family|none|rejected> <diffuse>    one per textured shape
```

**The `oct` line's family is NOT the last token any more** — `base` follows it.
Anything anchoring `pbr$` or `legacy$` breaks. Split on spaces and index by
position.

`--list-impostor-candidates` prints `formid extent model`; the **extent is
column two deliberately**, because the model is the only token that can hold a
space and must stay the line's remainder.

---

## 6. DDS container

Both shapes are written by the same two writers.

**Per-card sets** go through `lodgenWriteDds`: a legacy 124-byte DDS header with
a `DXT1`/`DXT5` fourCC where the format allows it, or the DX10 extension.

**Arrays** go through `lodgenWriteDdsArray` and are always **DX10**:

| field | value |
|---|---|
| `dwMagic` | `'DDS '` |
| `dwSize` | 124 |
| `dwFlags` | `0x000A1007` — caps, height, width, linear size, pixel format, mip count |
| `dwHeight`, `dwWidth` | the sheet size |
| `dwPitchOrLinearSize` | `ceil(w/4) · ceil(h/4) · blockBytes` of **mip 0 of one layer** |
| `dwMipMapCount` | the chain length |
| `ddspf.dwSize` | 32 |
| `ddspf.dwFlags` | `0x4` (fourCC) |
| `ddspf.dwFourCC` | `'DX10'` |
| `dwCaps` | `0x401008` — complex, texture, mipmap |
| DXT10 `dxgiFormat` | **77** (`BC3_UNORM`) for the colour, normal and mask sheets; **71** (`BC1_UNORM`) for the emissive |
| DXT10 `resourceDimension` | 3 (2D) |
| DXT10 `arraySize` | the layer count |

**Payload order: layer 0's whole mip chain, then layer 1's, …** Rows are tightly
packed at `ceil(w/4)·blockBytes`.

**The mip filter is a 2×2 box, rounded half-up, and it stops while
`w > 4 && h > 4`** (or earlier at the card path's frame cap). On a **BC1** sheet
the filter forces alpha to 0xFF down the chain — harmless for the emissive, which
is opaque by contract, and the reason the single-sheet writer needed an explicit
`bc1Alpha` opt-in for punch-through atlases.

---

## 7. Invariants a reader may assume

1. Every sheet of a set has identical dimensions **except** the three auxiliary
   sheets under `--card-half-aux`, whose sides are `1/auxDiv`.
1a. `mips == 1 + log2(min(gap[0], gap[1]))`, and therefore **every shipped mip of
   a card sheet keeps a whole texel between the two silhouettes that meet on an
   interior frame border**. The silhouette lives in `frame - gap`, which is
   `frame - 2*pad`; `half` still spans the whole frame. The sheet's OUTER border
   carries half a gap and needs no more, because it is sampled clamped and has no
   neighbour beyond it.
2. A layer of a card array holds exactly what the per-card DDS holds — the array
   is built from the same PNGs and the same dilation, **not** by re-encoding the
   per-card DDS.
3. A set baked before the emissive sheet existed contributes a **black** emissive
   layer, so layer indices never shift.
4. Every `C` line's tenth token names a `kind: "card"` `.lodm`; when eleven and
   twelve are present they name a `kind: "cardArray"` `.lodm` and a layer inside
   it.
5. Card geometry (`half`, `center`, `depthSpan`) is **per layer** in an array;
   the grid, the frame and the mip cap are **shared** by the array.
6. Impostor cards are **never** decimated by the far-ring simplifier, and are
   excluded from it a second time by object index off the `C` lines.

## 8. Gates

* `tests/spells/lodgen_octahedral.sh` — two **real** bakes at N=4: the game's
  maple (legacy) and the same maple under pbr source `.lodm`s in a loose root.
  Checks the size class, the exact extent aspect, R/G swapping between families,
  and the emissive from both sides (black on legacy because every shape of the
  near maple is alpha-tested; the colour sheet on pbr) plus `emissiveScale`
  0 and 2.5.
* `tests/spells/lodgen_card_arrays.sh` — two synthetic sets on the Sanctuary
  cells: the four array files, the `cardArray` `.lodm`, the two new `C` tokens,
  each layer's colour decoded off the BC3 endpoints, the green emissive off the
  BC1 ones, and each layer's own `emissiveScale` out of the list.

## 9. Sample files

Three libraries of the same 19 Sanctuary trees, OCT=8 TILE=128, are on disk:
`scratchpad/images_20260909/gen/cards_trees19` (the five-rung aspect ladder, exe
19:35:14), `scratchpad/cardfit_20260909/cards_after` (this aspect law with the
spacing read as a PER-SIDE margin, exe 20:49:01), and
`scratchpad/cardpad_20260909/cards_gap` (this contract, the spacing read as the
GAP). The measurement scripts that read them are in
`scratchpad/cardfit_20260909/` and `scratchpad/cardpad_20260909/`.

---

## Provenance

Re-read 2026-09-09 after lane CARDPAD moved the spacing from a per-side margin
to the GAP between two neighbouring silhouettes (bungo's correction of lane
CARDFIT3 the same day). Anchor text is quoted beside every line number because
both sources move.

| file | sha256 (16) | lines |
|---|---|---|
| `src/lodgen.cpp` | `63f9971cf5cbb438` | 8,394 |
| `src/nifskope_ui.cpp` | `c0fc470f94d7ec16` | 31,133 |

| claim | line | anchor |
|---|---|---|
| `oct = N` frames per side, N² views | `nifskope_ui.cpp:22065` | `auto viewDir = [octN]( int i, int j, float & rx, float & rz ) {` |
| the gap law `max(2, side/16)` rounded up to even | `nifskope_ui.cpp:22220-22223` | `auto gapOf = []( int side ) {` |
| the margin on each side is half the gap | `nifskope_ui.cpp:22224` | `auto padOf = [gapOf]( int side ) { return gapOf( side ) / 2; };` |
| the short side, smallest multiple of 16 that does not crop | `nifskope_ui.cpp:22245-22251` | `for ( int s = 16; s <= tileLong; s += 16 ) {` |
| the inner rect is `frame - 2*pad` = `frame - gap`, per axis | `nifskope_ui.cpp:22257-22258` | `const int padX = padOf( tw ), padY = padOf( th );` |
| the gap the sidecar records | `nifskope_ui.cpp:22259` | `const int gapX = gapOf( tw ), gapY = gapOf( th );` |
| the measurement margin is 1%, not 4% | `nifskope_ui.cpp:22144` | `float halfW = maxDx * 1.01f, halfH = maxDy * 1.01f;` |
| the sidecar's own `gap` line | `nifskope_ui.cpp:22434` | `ms << "gap " << gapX << " " << gapY` |
| the reader takes `gap`, and an older `pad` under its own law | `lodgen.cpp:2494-2513` | `card.octMipUnit = qMin( card.octGapX, card.octGapY );` |
| `mips = 1 + log2(min(gapX,gapY))` | `lodgen.cpp:2624-2625` | `for ( int g = mipUnit; g >= 2; g /= 2 )` |
| `auxMips` comes down with the halved gap | `lodgen.cpp:2651-2653` | `for ( int g = mipUnit / auxDiv; g >= 2; g /= 2 )` |
| the card `.lodm` carries `pad` (per side) and `gap` | `lodgen.cpp:2707-2708` | `oc.insert( QStringLiteral( "gap" ), QJsonArray{ gapX, gapY } );` |
| the `cardArray` `.lodm` carries `pad` and `gap` | `lodgen.cpp:8328-8330` | `arr.insert( QStringLiteral( "gap" ), QJsonArray{ g.gapX, g.gapY } );` |
| `_fs.DDS` is BC3/DXT5 with its alpha | `lodgen.cpp:2554` | `lodgenWriteDds( dds, 2 * w, h, px, true );` |
| per-card game path stem | `lodgen.cpp:2673` | `QStringLiteral( "Data\\Textures\\Lodgen\\Cards\\" ) + id + QStringLiteral( "_oct" )` |
| emissive sheet written BC1, no alpha | `lodgen.cpp:2667-2671` | `// the emissive sheet is BC1: RGB only, no alpha to carry` |
| dilation depth `max(8, max(fw,fh)/8)`, per card | `lodgen.cpp:2593` | `const int deep = qMax( 8, qMax( card.octTileW, card.octTileH ) / 8 );` |
| the same depth in the card-array path | `lodgen.cpp:8233` | `const int deep = qMax( 8, qMax( fw, fh ) / 8 );` |
| card-array group key = family + sheet size | `lodgen.cpp:8238` | `const QString key = QString( "%1\|%2x%3" )` |
| DX10 array header fields | `lodgen.cpp:4068-4098` | `const quint32 dx10[5] = { bc3 ? 77U : 71U, 3U, 0U, quint32( layers.size() ), 0U };` |
| mip filter box + round-half-up, single sheet | `lodgen.cpp:3808` | `// texels, or neighbouring views blend into one another.` |
| the same filter in the ARRAY writer | `lodgen.cpp:3960` | `std::vector<quint8> & out, int maxMips = 0 )` |
