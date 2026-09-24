# Impostor card sheets v1 — per-card sets and card arrays

**Contract version: card sets carry `kind: "card"` `.lodm` v1; card arrays carry
`kind: "cardArray"` `.lodm` v1.**
**Status: SHIPPED, gated, never flown in a game.** Bake:
`WW_IMPOSTOR_OCT` / `WW_IMPOSTOR_TILE` in `src/nifskope_ui.cpp` driven by
`tools/bake_impostor_cards.sh` (default 8 x 8 frames at 256 px = a 2048-texel
sheet for the largest base since 2026-09-23, bungo's "8x8 at 2k"; was 128 px); conversion to DDS + `.lodm` in
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
Data\FO4CSLOD\Cards\<formid8hex>_oct_d.DDS      legacy   BC3
                    <formid8hex>_oct_n.DDS               BC7 (DX10, DXGI 98)
                    <formid8hex>_oct_gsaos.DDS           BC3
                    <formid8hex>_oct_g.DDS               BC1
                    <formid8hex>_oct.lodm                kind "card"

                    <formid8hex>_oct_bc.DDS    pbr      BC3
                    <formid8hex>_oct_n.DDS               BC7 (DX10, DXGI 98)
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
Data\FO4CSLOD\<ws>\Objects\<ws>.LodgenCards.legacy.<SW>x<SH>_d.DDS
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

The `_n` array is **BC7** (DXGI 98), like the per-card `_n`; every other array
keeps its format (§6).

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
directions and the centre of the grid is the exact top — **a frame only when N
is odd.** At N = 4, 6 or 8 (every grid the panel offers) `(N−1)/2` is not an
integer, there is no centre frame, and straight up falls inside a triangle like
any other direction. Every direction falls inside a triangle of three frame
centres; that `(N−1)²`-cell triangle mesh **is** the blending rule — pick the
three frames, blend with the height channel for parallax, write depth for the
true silhouette.

**Which triangle, and with what weights** (`src/impostoroct.h` `pickFrames`,
SPEC GAP #2, implemented in `src/impostoroct.cpp` `if ( a + b <= 1.0f )`). A
consumer maps a direction to the continuous grid coordinate by the inverse of
the mapping above (`dirToGrid`), takes the cell `(i, j)` it falls in and the
fractional position `(a, b)` inside it. Each cell is split by the diagonal from
`(i+1, j)` to `(i, j+1)`:

```
a + b <= 1   frames (i,j), (i+1,j), (i,j+1)       weights (1-a-b, a, b)
a + b >  1   frames (i+1,j+1), (i,j+1), (i+1,j)   weights (a+b-1, 1-a, 1-b)
```

The weights are barycentric **in the grid**, not in direction space, and sum to
one; the two triangles agree on the shared diagonal. The other diagonal is
equally standard, which is why it is written down: a bake and a consumer that
split the cell differently disagree everywhere and no test can say who is
wrong.

**Frames are rectangular and one size for every view.** The bake makes two
passes: the first photographs every view at the bound-sphere fit and takes the
widest and tallest silhouette extent from the centre over all of them; the second
bakes at that fit.

### 2.1 The tree RING (2026-09-24)

Tree cards are not on this grid any more: they are 16 azimuths at elevation 0 in
ONE row, `views × 1` -- the aggregate's ring (§10.2) at 22.5 degrees, by bungo's
ruling of 2026-09-23. `docs/LODGEN_LODM_FORMAT.md` §3.2 is the contract: `views`
and `grid` instead of `oct`, frame `v` at `[v·frameW, (v+1)·frameW) × [0, frameH)`,
two neighbours blended by angle. Everything in §3 onward -- the gap, the padding,
the size ladders, per-frame positioning, the orthographic camera, the channels --
is the grid's, frame for frame. The bake: `WW_IMPOSTOR_RING=16` (it wins over
`WW_IMPOSTOR_OCT`); the driver: `RING=16`, the default for `CANDIDATES=trees`.

Measured against the N8 grid on the same model at the same tile: the grid's
horizon is 28 frames whose azimuths bunch toward the diagonals (per quadrant 0,
9.5, 21.8, 36.9, 53.1, 68.2, 80.5, 90 degrees; largest step 16.2), the ring's is
16 at a uniform 22.5. `tests/spells/impostor_ring.sh` prints both at the
in-between azimuths and at elevations 0/5/15/30/60.

Measured 2026-09-24 (exe eaa4b0b6, TreeMapleblasted05, 256 tile, the default
crisp draw, card vs mesh silhouette IoU):

| | ring16 | N8 | ring8 |
|---|---|---|---|
| full turn at el 0, 1-degree steps | 0.690 | 0.784 | 0.526 |
| worst 1-degree popping step (px) | 62,795 | 47,453 | 78,686 |
| in-between azimuths, el 0 | 0.489 | 0.825 | |
| el 15 / 30 / 60 | 0.427 / 0.285 / 0.209 | 0.651 / 0.795 / 0.371 | |

The ring draws within 2 percent of what a perfect photograph from its nearest
frame could score (the mesh against itself 0-11.25 degrees away: 0.701 over the
full turn). What it gives up against N8 is the frame count -- 16 horizon frames
to 28 -- and every frame above the horizon; it costs a quarter of the pixels.

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
mips = log2( min( gapX, gapY ) )               // >= 1
```

The inner rect is **15/16 of the frame** wherever a side is a multiple of 32 --
120 of 128, 240 of 256 -- which is his 8-on-1024 and 16-on-2k exactly. A side of
48, 80 or 112 rounds the gap up to even and gives back at most one texel.

**Where the mip count comes from.** Mip CONSTRUCTION never mixes frames -- the
box filter halves an even frame into an even frame -- so the bleed is at SAMPLE
time: a tap on a frame's own UV border reads half of that frame's last texel and
half of the neighbour's first. What that tap picks up OF THE NEIGHBOUR is
therefore decided by the MARGIN INSIDE EACH FRAME, `gap / 2^(k+1)` at level `k`,
and the sheet ships every level where that margin is still a whole texel:

```
mips = log2( gap )        // the deepest level shipped has >= 1 texel of margin per side
```

A 128-texel frame at gap 8 gets **3** levels: 128, 64, 32. bungo, 2026-09-09
evening, verbatim: *"SHIP ONE MIP FEWER: mips = log2(gap) so the deepest shipped
level still has a full texel of margin per side (128 frame, gap 8 -> 3 levels
128/64/32)"* -- which is also what his earlier *"8 pixels = 3 clean mips"*
meant.

**Why one fewer, with the number that decided it.** Shipping while the whole
`gap` is a texel means shipping the level where each margin is HALF a texel, and
a bilinear tap taken exactly on a frame border reaches half a texel. Measured on
the 19-tree Sanctuary library under that cap: **13 of 19 sheets** let a border
tap pick up some of the neighbour's edge, worst **64/255**. Under `log2(gap)` it
is **0 of 19**, on every border of every shipped mip, at the same spacing. The
level is bought back by the mip chain's own arithmetic, not by more padding.

**The two older vintages do not move.** A sheet whose sidecar wrote a PER-SIDE
number (lane CARDFIT3's `pad`, or the older `max(4, longSide/16)` fallback) has a
gap of twice that number, and `log2(2*pad) = 1 + log2(pad)` is exactly the chain
those sheets were built for. The law changed; their counts did not.

**The sheet's outer border needs only half a gap.** There is no neighbouring
frame beyond it, so padding every frame by `gap/2` is already correct at the
edge and no special case exists: an interior border carries `gap/2` from each of
the two frames that meet on it, an outer border carries `gap/2` and faces the
sheet edge. That holds because the sheet is sampled **clamped** -- a card quad's
UV rect is a sub-rect of the sheet, and neither the DDS nor the `.lodm` asks for
wrapping. Under WRAP the outer border would face the opposite edge's frames and
would need a whole gap; nothing in this format does that.

The gap is floored at 2 and rounded up to even, so it always splits into two
whole texels of margin. On the smallest frames that floor makes the chain a
SINGLE level -- margin 1 at mip 0, half a texel at mip 1 -- and the format says
so rather than bleeding quietly.

| frame | gapX, gapY | padX, padY | inner rect | mips | margin per side at the last mip |
|---|---|---|---|---|---|
| 16x64 | 2, 4 | 1, 2 | 14x60 | 1 | 1 texel |
| 32x32 | 2, 2 | 1, 1 | 30x30 | 1 | 1 texel |
| 32x64 | 2, 4 | 1, 2 | 30x60 | 1 | 1 texel |
| 48x64 | 4, 4 | 2, 2 | 44x60 | 2 | 1 texel |
| 96x128 | 6, 8 | 3, 4 | 90x120 | 2 | 1.5 texels |
| 128x128 | 8, 8 | 4, 4 | 120x120 | 3 | 1 texel |
| 256x256 | 16, 16 | 8, 8 | 240x240 | 4 | 1 texel |

**What this replaced.** Twice, both on 2026-09-09. Before either: one gutter
`max(4, tileLong/16)` on all four sides and a chain that ran *while a frame's
shorter side spanned eight texels* -- two unrelated rules, which shipped a
BLEEDING level on every card of 96 texels or more (measured on the 19-tree
Sanctuary library: mip 4, gutter half a texel, 26/255 of a neighbouring frame's
alpha across 16 of 28 frame borders). Then lane CARDFIT3 read the number as the
PER-SIDE margin, `pad = max(2, side/16)`, which spent twice the texels bungo
asked for and left the inner rect at 7/8 of the frame instead of 15/16. Both of
those spelt the cap `1 + log2(gap)`, one level deeper than the version above.

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

`mips = log2(min(gapX, gapY))`, at least 1 -- **the gap decides it**, and §3.1 is
the derivation. The count is in the DDS header and in the `.lodm`'s `mips`. It is
a cap on the whole sheet's chain: a sheet mip halves every frame at once, and the
chain stops at the last level where each of the two frames meeting on a border
still keeps a WHOLE TEXEL of margin, because at the next one that margin is half
a texel and a border tap reaches across.

### 3.5 `--card-half-aux`

Writes the normal, mask and emissive sheets at **half of each side** and leaves
the base colour alone. Measured across the two-layer array set the card-array
gate builds (`gap 4`, a 16x32 sheet of 8x16 frames), the payload falls from
**4,480 to 1,920 bytes, 42.9%**. It read 46.4% on 2026-09-06 and the difference
is the mip law, not the saving: the halved sheets' chain comes down with their
halved gap, and since 2026-09-09 evening the base colour keeps a level they do
not. The gate now asserts the exact byte counts the class, the gap and `auxDiv`
account for, rather than a percentage that goes stale under it.

The base colour never divides: its **alpha is the coverage**, so it is the
silhouette, and that is what an impostor is judged on.

The divide happens **after** frame dilation, and the gap is rounded up to an
EVEN number of texels precisely so a halving lands its margins on whole texels;
frames are multiples of 16, so a halved frame is still even and nothing mixes
across a frame border. The aux sheets' own mip count comes down with their gap:
`auxMips = log2(min(gapX,gapY)/auxDiv)`, floored at 1. On the smallest frames --
16 and 32 texels, where the gap is already at its floor of 2 -- that division
reaches 1 and the aux sheets ship a SINGLE level: a fallback naming itself rather
than a silent bleed. Sampling is unaffected — normalised UV reads a
smaller sheet with the same coordinates. A consumer needs `card.auxDiv` /
`array.auxDiv`, `array.auxClass` and `array.auxMips` only to size its own
allocation.

### 3.6 Per-frame positioning — one scale, N² places

bungo, 2026-09-09, verbatim: *"nifskope needs to position the tree in each view,
so that the tree is equal in size on each one, and with minimal pixels wasted,
and end up with no mipmap bleeding to nearby frames"*, and *"then the tree must
be positioned correctly, so that when a 3d tree transitions to an imposter, the
tree won't change position"*. Then, the evening of the same day: *"each frame
shifts its own silhouette to its own centre for minimal waste, with the per-frame
offset written into the `.lodm` so a reader places every frame exactly."*

**The waste this removes.** Every view is photographed at ONE scale about ONE
model-space centre. A tree is not symmetric about that centre, so the silhouette
box of each view sits at its own offset inside the frame -- leaning left from the
front, right from the side. At a fixed centre the frame had to hold the UNION of
all N² boxes, which is wider than any single one of them; the difference is air
that only one view ever uses.

**The rule.** Each frame is cropped around ITS OWN silhouette centre, so the
frame only has to hold the WIDEST SINGLE VIEW. What does not change:

* **the scale** -- `half` is still one pair of half extents for the whole card,
  so the tree is the same size in every frame;
* **the shape** -- the quad is still the whole frame and every frame is the same
  rect, so blending three neighbouring frames still blends three quads of one
  shape;
* **the centre** -- `center` is still the single pivot-to-card-centre offset the
  transition rule rests on.

What changes is WHERE each frame's quad sits, and the `.lodm` says so:

```
card.frameOffset : [ ox, oy ] x oct²      model units, frame (i,j) at index j*oct + i
```

A reader draws frame `(i,j)` at

```
quadCentre = pivot + center + ox * right(i,j) + oy * up(i,j)
```

with the card's one pair of half extents, where `right`/`up` are that view's own
screen axes -- the same basis the frame was photographed in. **Ignoring the key
puts every quad at `center`, which is exactly a set from before this law, and
makes the tree step sideways by `|offset|` when the mesh hands over to the
card.** On a `cardArray` the same key is **per layer**, beside `half` and
`center`, because two sets in one array have different offsets. Absent means
absent, never zeros.

**The transition rule still holds, per frame.** Whatever the view basis, a
frame's quad centre is within `|offset|` of `center`, so the whole set of quads
lies in a sphere of `max|offset|` about it. The gate is the same one `center`
answers to -- the model's own declared bound spheres, read independently of the
bake -- extended to the most displaced frame, plus the zeroed-offset control that
must fail.

**Two side effects, both named.** The bake's viewport fit is widened to
`maxOffset + half` on each axis, because a crop can only take what was
photographed; the model is therefore drawn slightly smaller in the viewport and
the downsample into the frame starts from marginally fewer pixels. And the SIZE
LADDER (§3.3) is deliberately still fed the UNION half-extent, not the per-view
one, because it compares this base against a MODEL extent from
`--list-impostor-candidates` and the smaller number would drop trees a rung for a
reason unrelated to how big they are.

---

### 3.7 The camera is ORTHOGRAPHIC, and until 2026-09-10 it was not

Every geometric number in this document -- `half`, `center`, `frameOffset`, the
`front`/`side` extents, the aspect the frame is chosen from -- is a WORLD
measurement taken off VIEWPORT PIXELS through a single units-per-pixel constant:

```
units per pixel = 2 * orthographicHalfHeight() / viewportHeight
```

That is a statement about an orthographic camera, and it is the only projection
under which it is true. In a perspective projection a point `d` in front of the
card plane is magnified by `eye / (eye - d)`, so the constant is not a constant
and the numbers describe no picture at all.

**Every card baked before 2026-09-10 was drawn through a 60-degree PERSPECTIVE
frustum while being measured as if it were not.** `restoreUi()` hard-codes
`isPersp = true` and the only other callers of `setProjection` are the View menu
and Numpad-5, so nothing headless ever set one; `orthographicHalfHeight()`
returns `Dist / Zoom` whatever the projection is, which is why the read-back
beside the bake's own fit -- comparing two numbers that are both `Dist / Zoom` --
could not see it. Lane HOOKCAM measured the frustum on 2026-09-09 (two identical
cubes 1024 units apart in depth photograph 31 px and 27 px; an orthographic
camera draws them the same size).

What it cost, and what asserting the projection buys:

* **the SCALE.** Over a tree's own depth the magnification is tens of per cent,
  so the recorded half-extents did not describe the sheet and a reader's quad
  could not match the mesh it replaces;
* **the SHAPE.** The magnification varies across a frame, so each silhouette was
  foreshortened -- wider at the frame's near edge than at its far one -- and the
  frames of one set disagreed with each other;
* **the HEIGHT sheet.** The bake puts near and far symmetric about the bound
  centre and calls window z 0.5 the card plane (§4). That is exactly true in an
  orthographic projection and false in a perspective one, where the window z of
  the midpoint is not 0.5.

The bake now asserts the projection before pass one and the sidecar NAMES the arm
that served, read back off the live viewport rather than off what was asked for:
`projection ortho` or `projection persp`, plus `orthofit <asked> <achieved>
<persp 0|1>` beside the fit. The word travels into the `.lodm`, per card and per
card-array layer, so a consumer can refuse a non-metric set by name.

`WW_IMPOSTOR_PERSP=1` restores the old camera exactly (CONSTITUTION 10, modules
and fallbacks: a behavioural change a user can see carries an exact way back),
and is the CONTROL the gates fail against.

**A library whose sidecars carry no `projection` line is the older,
foreshortened vintage.** Absence is not "unknown": the line arrived in the same
change that fixed the camera, so every sidecar that does not say was baked
through the perspective frustum. Re-bake it.

---

## 4. What each channel holds

Formats and channel roles are the `.lodm` family contract
(`docs/LODGEN_LODM_FORMAT.md` §2.1). What is specific to a card:

| channel | law |
|---|---|
| colour RGB | the base colour × the vertex colour and **nothing else** — the unlit path, not the lit path with lighting off (that one tone-maps before it writes). The consumer lights the card |
| colour A | **coverage**, from a two-pass matte over black and white: `coverage = 1 − (passes' difference)` |
| normal R, G | the geometric normal in the **VIEW's** space, back faces flipped, half-packed. Opposite views differ; top and horizon agree |
| normal B (height) | window depth of the orthographic bake. **0.5 is the card plane**; `units = (B − 0.5) × depthSpan`, with `depthSpan = 3 × max(boundRadius, 1024)`. Drives pixel depth offset, ghost-free frame blending, shadows and the model-to-card transition. Stored **BC7** since 2026-09-23 (§6.1) |
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

**THE COVERAGE CONTRACT** (lane CARDWIDTH, 2026-09-10). The bake measures every
extent it writes -- `half`, and every `frameOffset` -- at the coverage FLOOR, and
a consumer alpha-tests at 0.5. Those were two different silhouettes, and the gap
between them is the tree changing size at the transition: measured over three of
the Sanctuary trees, the set a consumer drew at 0.5 was up to **5.41 texels of
half-width short** of the extents the `.lodm` declared -- TreeHero01 handing over
to a card **9.1 % narrower and 3.2 % shorter than its own mesh**. Which of the two
silhouettes is the right one was decided inside the source render alone, with no
bake in the comparison: the 941-px silhouette box-filtered to the card's own texel
pitch reproduces its own bounding box to **0.63 texels** worst of twelve
half-extents at 16/255 and to **2.07** at 0.5. The floor is right; the test had to
be made to agree with it.

So the base-colour sheet's alpha is now written so that **the consumer's own test
selects exactly the coverage the bake measured**:

```
a' = 0                                                       coverage <  floor
a' = base + round( (coverage - floor) * (255 - base) / (255 - floor) )
```

with `floor = 16`, `test = 128` (0.5) and `base = 160`, stated by the bake on a
`coverage <floor> <test> <base>` line and carried into the `.lodm`'s `coverage`
object. Then `{ a' >= test } == { coverage >= floor }` exactly (measured: 0
disagreeing texels of 209,793 / 64,709 / 13,510 on three sheets), 255 stays 255 so
a solid silhouette does not move, an empty texel is a clean 0, and the FRACTION is
still there, monotone and invertible to one alpha step:

```
coverage = floor + (a' - base) * (255 - floor) / (255 - base)
```

**The base is 160 and not 128** because the sheet ships as BC3, whose alpha block
is endpoints max/min with an eight-step ramp and nearest-palette indices: a
texel's alpha moves by at most `(aMax - aMin) / 14 <= 255/14 = 18.2`, so a covered
texel written at 128 can round BELOW the test (measured: 5,250 texels of one
sheet) and one written at 160 cannot, since `160 - 18.2 = 141.8`. That leaves 96
of the 256 levels for the fraction, against the eight a BC3 block resolves at all.

**A set with no `coverage` line or key** is from before this and its alpha is the
raw fraction: its declared extents describe the silhouette at **16/255**, and that
is the value a consumer must test such a set at. Reading an older set at 0.5 is
exactly the defect above.

**Still a fraction, not a cut-out.** Above the floor the value remains the
coverage the matte measured, so a consumer that blends a bare crown rather than
testing it recovers it with the inverse above.

---

## 5. The bake sidecar `<id>.txt`

One line per fact, written by the bake, consumed by `lodgenCard`. Not a shipped
format, but a reader of a bake tree needs it:

```
model <file>                 the model actually photographed
coverage <floor> <test> <base>
                             THE COVERAGE CONTRACT of the base-colour sheet (§4):
                             the coverage at which a texel counted as covered and
                             every extent on this sidecar was measured, the alpha a
                             consumer must TEST at to select that same set, and the
                             alpha the floor was written at. `16 128 160` from this
                             build. ABSENT = a sheet whose alpha is the raw
                             fraction, to be tested at 16/255
projection <ortho|persp>     the camera the sheet was photographed through, read
                             back off the live viewport (§3.7). Absent = a bake
                             from before 2026-09-10, i.e. perspective
hidden <shape>               per detail step (_L1.._L9) set aside
ranges …                     a BSMeshLODTriShape reduced to its first range
front … / side …             the crossed-quad cards
class <w> <h>                the frame size class
gap <x> <y>                  the distance in texels between two neighbouring
                             silhouettes across a frame border, per axis; the
                             margin on each side of a frame is half of it
emissive <scale> shapes <n>  the largest emissive multiple over the model's shapes
oct N frameW frameH halfW halfH cx cy cz depthSpan family base
ring V frameW frameH ...     INSTEAD of the `oct` line on a horizon-ring bake
                             (§2.1): the same fields, V frames in one row. An
                             old lodgen finds no `oct` line and makes no set,
                             rather than reading V as N
frameoff <i> <j> <ox> <oy>   one per frame: where THAT frame's quad sits relative
                             to `center`, in model units, along that view's own
                             right and up axes (§3.6)
frameclamped <n>             frames whose crop had to be pulled back inside the
                             photograph; 0 in a healthy bake
framefit <sx> <sy> <ux> <uy> what the frame was sized from -- the widest single
                             view's half box -- beside the union half-extent a
                             fixed-centre bake would have needed, in units
orthofit <asked> <achieved> <persp 0|1>
                             the ortho half-height the fit asked for, what the
                             viewport gave back, and whether it was read through a
                             perspective camera. The first two are true in either
                             projection -- both are Dist/Zoom -- so the third is
                             the field that MOVES (§3.7)
lodm <candidate> <family|none|rejected> <diffuse>    one per textured shape
ringview <v> <azim> <elev>   ring bakes only, one per frame: the camera the
                             renderer HELD for frame v, in degrees, read back
                             from the view at the moment it was drawn -- the
                             echo impostor_ring.sh R1 checks against v x 360/V
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
a `DXT1`/`DXT5` fourCC where the format allows it, or the DX10 extension. The
card `_n` is always the DX10 extension: `dxgiFormat` **98** (`BC7_UNORM`),
`resourceDimension` 3, `arraySize` 1, the same 124-byte header as the table
below, and the payload 20 bytes further on. Every other card sheet keeps its
fourCC.

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
| DXT10 `dxgiFormat` | **98** (`BC7_UNORM`) for the normal sheet (`_n`); **77** (`BC3_UNORM`) for the colour and mask sheets; **71** (`BC1_UNORM`) for the emissive |
| DXT10 `resourceDimension` | 3 (2D) |
| DXT10 `arraySize` | the layer count |

**Payload order: layer 0's whole mip chain, then layer 1's, …** Rows are tightly
packed at `ceil(w/4)·blockBytes`.

**The mip filter is a 2×2 box, rounded half-up, and it stops while
`w > 4 && h > 4`** (or earlier at the card path's frame cap). On a **BC1** sheet
the filter forces alpha to 0xFF down the chain — harmless for the emissive, which
is opaque by contract, and the reason the single-sheet writer needed an explicit
`bc1Alpha` opt-in for punch-through atlases. The BC7 `_n` keeps its alpha (sway)
down the chain, as BC3 does.

### 6.1 The `_n` sheet is BC7 (2026-09-23)

bungo's ruling, on lane IMPOSTORDEPTH1's measurement: under DXT5 the height
sat in the 5:6:5 colour block with the normal's X and Y, and came back 2.81
levels wrong on average (34 units), p95 8, with 27 of the bake's 59 heights
surviving the decode. That is the doubled, thinned trunk on the 8x8 maple.

**The encoder is in-tree**: `src/lodgenbc7.h`, header-only, no library, no
download. It is deterministic -- the same pixels give the same bytes on any
thread count; two bakes of the maple are byte-identical. It tries mode 6, modes
5 and 4 under every channel rotation (4 with both index selections), mode 7 on
the four best partitions, and modes 3 and 1 on opaque blocks, and keeps the
least WEIGHTED squared error: R 1, G 1, **B (height) 32**, A (sway) 1. The
weight is the measured knee: 16 left p95 at 3, 64 began to lose heights.
Every block was decoded with the vendored detex decoder (`lib/detex`) and
its error equals the encoder's claim; the gate decodes with Pillow.

Measured on the maple (1920x2048, 12 mips, the 1,037,765 covered texels),
against the height lodgen was GIVEN to encode (the bake PNG after
`lodgenRepairOctHeight`):

| | DXT5 (before) | BC7 (now) |
|---|---|---|
| height error, mean | 2.64 levels | **0.57** |
| height error, p95 | 7 | **2** |
| heights surviving | 27 of 58 | **57 of 58** (the lost one is the extreme, 5 texels) |
| normal X / Y error, mean | 11.3 / 8.8 | 3.3 / 3.3 |
| sway (A) error, mean / p95 | 0.02 / 0 | **1.34 / 4** -- the cost |
| file | 5,222,528 B | 5,222,548 B (the DX10 header) |
| lodgen compress, whole card | 12.2 s | 9.6-10.1 s (single runs; the encode is not slower) |

Against the raw bake PNG (the repair included) the height reads 2.81 / p95 8 /
27 of 59 before and 1.00 / p95 4 / 58 of 59 now. Gate:
`tests/spells/impostor_sheetbar.py`, run by `tests/spells/impostor_trunk.sh`.

Not moved, and named here so nobody assumes otherwise: the aggregate `_n`
(§10) and every mesh `_n` (the source arrays, the atlas) stay BC3.

---

## 7. Invariants a reader may assume

1. Every sheet of a set has identical dimensions **except** the three auxiliary
   sheets under `--card-half-aux`, whose sides are `1/auxDiv`.
1a. `mips == max(1, log2(min(gap[0], gap[1])))`, and therefore **no shipped mip of
   a card sheet lets a tap on an interior frame border pick up any of the
   neighbouring frame**: each of the two frames keeps a whole texel of margin at
   every level shipped. The silhouette lives in `frame - gap`, which is
   `frame - 2*pad`; `half` still spans the whole frame. The sheet's OUTER border
   carries half a gap and needs no more, because it is sampled clamped and has no
   neighbour beyond it.
1c. `card.projection` (per layer on an array), when present, is the camera the
   sheet was photographed through. `ortho` means the set's `half`, `center` and
   `frameOffset` are metric -- they describe the sheet beside them. ABSENT means
   the sidecar did not say, and every sidecar that did not say came from a bake
   that drew a 60-degree perspective frustum, so absence is the older vintage
   and not "unknown" (§3.7).
1b. `card.frameOffset` (per layer on an array), when present, is `2 * oct²`
   numbers in the frames' own sheet order, in model units. Absent = every frame
   centred on `center`. A reader that honours it puts the card exactly where the
   model was, in every view; one that ignores it is a set from before §3.6.
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
7. The card `_n` -- per-card set AND array -- is **BC7_UNORM** (DXGI 98, linear,
   not sRGB) under the DX10 header: `arraySize` 1 on a per-card set, the layer
   count on an array. A set baked before 2026-09-23 has a **DXT5** `_n` (legacy
   header) with the same channel meaning. **A reader picks the decoder from
   the header, never from the file name.** The `.lodm` names files, not
   formats, and is unchanged; no version moved.

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
  BC1 ones, and each layer's own `emissiveScale` out of the list. The sets state
  their own `gap` so the mip cap is checked as the law and not as a constant, and
  the `--card-half-aux` saving is asserted as the EXACT bytes the class, the gap
  and `auxDiv` account for.

What the octahedral gate adds for §3.1 and §3.6, each with the control that must
fail on the other side:

| statement | control |
|---|---|
| every shipped mip keeps a whole texel of gap between the two silhouettes on a border | the sheet re-tiled with its margins stripped: 0.44 texels |
| no shipped mip lets a border tap pick up ANY of the neighbouring frame | the same stripped sheet: 255/255 |
| every frame's silhouette is centred in its own frame within 1.5 texels | putting the recorded offsets back moves it more than a texel further off |
| the short side is the smallest rung that does not crop | one rung narrower would have cropped |
| every frame's offset plus its own silhouette stays inside the extent a fixed-centre card spanned | at least one frame REACHES that extent, which zero offsets could not |

## 9. Sample files

Four libraries of the same 19 Sanctuary trees, OCT=8 TILE=128, are on disk:
`scratchpad/images_20260909/gen/cards_trees19` (the five-rung aspect ladder, exe
19:35:14), `scratchpad/cardfit_20260909/cards_after` (this aspect law with the
spacing read as a PER-SIDE margin, exe 20:49:01),
`scratchpad/cardpad_20260909/cards_gap` (the spacing read as the GAP, one centre
for every view, exe 22:04:35) and
**`scratchpad/cardfinal_20260909/cards_perframe`** (this contract: `log2(gap)`
mips and per-frame positioning, exe 23:41:00). The measurement scripts that read
them are in `scratchpad/cardfit_20260909/`, `scratchpad/cardpad_20260909/` and
`scratchpad/cardfinal_20260909/`; `measure_perframe.py` takes the mip law as an
argument, because nothing in a sidecar distinguishes a library baked under one
cap from one baked under the other.

The FO4CS sample set (`scratchpad/handoff_fo4cs/samples/`) stands on the last of
those four.

---

## 10. Aggregate sheets — one card set per forested CELL

**Contract version: aggregate sets carry `kind: "aggregate"` `.lodm` v1.**
**Status: SHIPPED, gated, never flown in a game.** Composite:
`lodgenAggregateBuild()` in `src/lodgenaggregate.cpp`; the card library is read
through `lodgenAggregateCards()` and the sheets are written by
`lodgenAggregateWrite()`, both in `src/lodgen.cpp`; the `.lodi` rows are
`docs/LODGEN_NATIVE_LODO_LODI.md` §4.6. Switch: `--aggregate` (off by default,
and off is byte-identical).

bungo, 2026-09-11 08:3x, *"1 sounds good"*, over his own description: *"At ring
3 the bake takes each cell's trees, places their cards with the same rotation
and mirror the repetition breaking would give them, photographs the whole
cluster from the horizon views, and writes one aggregate sheet per cell."*

### 10.1 On disk

```
Data\FO4CSLOD\<ws>\Aggregate\<cellX>_<cellY>_agg_d.DDS      legacy  BC3
                             <cellX>_<cellY>_agg_n.DDS              BC3
                             <cellX>_<cellY>_agg_gsaos.DDS          BC3
                             <cellX>_<cellY>_agg.lodm       kind "aggregate"

                             <cellX>_<cellY>_agg_bc.DDS     pbr     BC3
                             <cellX>_<cellY>_agg_n.DDS              BC3
                             <cellX>_<cellY>_agg_rmaos.DDS          BC3
```

**THREE sheets, not four: there is no emissive.** A forest emits nothing, and
the `.lodm` says so with `emissiveScale` 0 (§4 of
`docs/LODGEN_LODM_FORMAT.md`). A reader binds black.

### 10.2 The grid is a RING, not a hemisphere

A card's `oct` is frames per side of a hemi-octahedral grid over a whole
hemisphere of directions. **An aggregate is photographed at the HORIZON only**,
because that is where ring 3 is, so its grid is `views × 1`: `views` azimuths in
ONE ROW, frame `v` at pixels `[v·frameW, (v+1)·frameW) × [0, frameH)`, with

```
eye(v)   = ( cos φ, sin φ, 0 )          φ = 2π·v / views
right(v) = ( −sin φ, cos φ, 0 )
up(v)    = ( 0, 0, 1 )
```

**8 azimuths by default, and the reason is the cell's own shape.** A cell is a
square footprint: its projected width swings from 4,096 units face-on to 5,793
corner-on — 41 percent, with a period of 90°. Eight azimuths at 0/45/90/… sample
both extremes exactly, so the two hardest views are photographed rather than
interpolated. The alternative considered and refused was the hemi-octahedral
grid's outer RING, which is the whole horizon at `4N−4` frames (28 at N = 8):
3.5× the sheet, and its azimuths are not uniform — `z = 1 − max(|u|,|v|)`, so
the boundary is walked in equal steps of `x+y` rather than of angle and the
frames bunch toward the diagonals. The cost of the coarse step is that a reader
blending two neighbouring frames is up to 22.5° from either; §10.6 is what that
costs, measured.

### 10.3 The frame law is the card law, unchanged

Long side = the run's aggregate tile (`--aggregate-tile`, default 64); short
side = the smallest multiple of 16 whose INNER rect is not narrower than the
measured silhouette; `gap(side) = max(2, side/16)` rounded up to even;
`pad = gap/2`; `mips = max(1, log2(min(gap)))`; `half` spans the WHOLE frame,
padding included, and every channel is dilated out from the silhouette frame by
frame. §3.1–§3.4 are the derivations and none of them moved.

What the aggregate measures instead of a model's silhouette is the CELL's:

```
the silhouette box of view v = the union of the cell's tree QUADS in that view
```

computed analytically from each tree's own `half`, `center` and `frameOffset` —
no render, because a card quad is a rectangle of known world size at a known
place. The frame then holds the WIDEST SINGLE VIEW and each view shifts its own
silhouette to its own centre, which is §3.6's law applied per azimuth and is
what `aggregate.frameOffset` carries.

### 10.4 The photograph is an orthographic COMPOSITE, and here is why

The obvious implementation is to put the cell's cards in a scene and photograph
them through the same two-pass matte hook the per-tree cards use. That was
refused, with numbers:

1. **It cannot bake the worldspace.** The hook photographs one model per process
   and sleeps 1,200 ms a card. 2,631 forested cells × 8 views = 21,048
   photographs, over seven hours of sleep alone, against an object stage that
   measures 4.1 s on a nine-chunk region.
2. **There is nothing left to photograph.** Since 2026-09-10 every card sheet is
   orthographic and metric (§3.7): `half`, `center` and `frameOffset` are world
   lengths that describe the sheet beside them. An orthographic composite of
   orthographic sheets is a RESAMPLE, exact up to the resample; a re-render adds
   a second round of pixel quantisation on top of the first.
3. **It is deterministic.** Two runs of one region give byte-identical sheets,
   which is what `--aggregate` off being byte-identical is measured against, and
   it holds no GL context, no window and no instance slot.

A card whose sidecar does not say `projection ortho` is **refused by name** and
its trees stay per-tree: a foreshortened set's extents describe no picture, so
compositing it would put the tree in the wrong place at the wrong size.

**The resample, exactly.** For each tree and each view, the frame whose own
direction best matches `R^T · eye(v)` is chosen (`R` the DRAWN rotation, tree
yaw folded in), the mirror flips the frame and the sign of its right offset, and
every source texel is splatted into the aggregate frame with the weight
`sampleArea / texelArea`. Trees are composited **back to front** by depth along
the eye axis, straight `over` compositing on every channel.

**The area weight is not a detail.** The first run normalised each tree's layer
by the NUMBER of samples that landed in a texel, so a tree covering a tenth of a
coarse texel composited as if it covered all of it, and the aggregate's
silhouette carried **94 percent more coverage mass** than the same cluster
composited finely — against a measured ceiling of 1.4 percent. Gate A3's own
CEILING arm is what found it.

### 10.5 The channels

The card channel contract of §4, with three statements of its own:

| channel | law |
|---|---|
| colour RGB, A | the trees' own colour and coverage, `over`-composited back to front. The sheet is written under THE COVERAGE CONTRACT of §4 — floor 16, test 128, base 160 — so a consumer's own 0.5 test selects exactly the coverage the composite measured |
| normal R, G | the trees' own view-space normals, `over`-composited. The aggregate's azimuth is within half a frame step of each source frame's own, so the two view spaces are the same basis to within that; it is an approximation and it is named |
| normal B (height) | **the composite's own depth, re-encoded.** Each source texel's world depth is `treeDepth − (B_src − 0.5)·depthSpan_src·scale`, and the aggregate writes `0.5 + behind / depthSpan_agg`, with `depthSpan_agg = 3 × max(boundRadius, 1024)`. Same decoder, same meaning, and it is what the far shadows are cast from (bungo, 08:2x) |
| normal A (sway) | **each texel keeps the sway weight of the tree it came from.** It is NOT re-derived from the aggregate frame's own bottom row: the aggregate's bottom row is the lowest point in the whole cell, so a tree standing on a hill would sway at its trunk. Same rule, applied where the rule means something |
| mask RGBA | the trees' own, `over`-composited |
| emissive | **absent.** `emissiveScale` 0 |

### 10.6 Gates

`scratchpad/cards_agg_20260911/aggpicture.py` is the calibrated picture gate and
`aggfixture_gate.sh` the standalone layout gate. The measure of the picture gate
is the silhouette **MASS** — the integral of coverage over the card, in world
units squared — because it is threshold-free and resolution-free, which a
thresholded mask is not: a canopy at 64 texels is mostly partial coverage, so a
hard alpha test measures the test (`ww-silhouette-compare` §3).

| arm | what it is | what it must read |
|---|---|---|
| known answer | the reference against itself | 0 |
| known answer 2 | the reference box-filtered to the subject's own pitch | ~0, because a box filter preserves mass |
| CEILING | the same, read back on the common grid | the best a frame of that size can do |
| SUBJECT | the shipped tile-64 aggregate | at or near the ceiling |
| FLOOR | a DIFFERENT cell's aggregate, same view, same grid | far outside the tolerance |

The reference is the SAME cluster of the SAME cards composited at
`--aggregate-tile 1024` — 8.2 units a texel against 23 units a texel inside a
tree's own card frame, so it resolves the cards better than the cards resolve
themselves.


---

## Provenance

Re-read 2026-09-09 (evening) after lane CARDFINAL took a level off the mip chain
(`mips = log2(gap)`) and shipped per-frame positioning, on top of lane CARDPAD's
move of the spacing from a per-side margin to the GAP between two neighbouring
silhouettes. Anchor text is quoted beside every line number because both sources
move.

| file | sha256 (16) | lines |
|---|---|---|
| `src/lodgen.cpp` | `c05fd079655ac03e` | 8,924 |
| `src/nifskope_ui.cpp` | `1073ddef14f8562e` | 31,495 |

**Re-derived 2026-09-10 by lane DOCS2** (`ww-contract-provenance` step 3, script
`scratchpad/docs2_20260910/anchors.py`): every line number below was found again
from its own anchor text against the sources stamped above, never shifted by a
delta. 24 of 36 rows moved, 12 were already right, 0 anchors missing or ambiguous.
Then `src/nifskope_ui.cpp` moved TWICE MORE under a concurrent lane while this
page was being written, and 17 of those rows were re-derived again each time; the
stamp above is the state the pass finally settled on, verified by re-running it
to `0 moved` with the source's hash unchanged across the run. **That
file is under live edit: check its sha256 before trusting a number here.**

| claim | line | anchor |
|---|---|---|
| `oct = N` frames per side, N² views | `nifskope_ui.cpp:22145` | `auto viewDir = [octN]( int i, int j, float & rx, float & rz ) {` |
| the gap law `max(2, side/16)` rounded up to even | `nifskope_ui.cpp:22354-22357` | `auto gapOf = []( int side ) {` |
| the margin on each side is half the gap | `nifskope_ui.cpp:22358` | `auto padOf = [gapOf]( int side ) { return gapOf( side ) / 2; };` |
| the short side, smallest multiple of 16 that does not crop | `nifskope_ui.cpp:22379-22385` | `for ( int s = 16; s <= tileLong; s += 16 ) {` |
| the inner rect is `frame - 2*pad` = `frame - gap`, per axis | `nifskope_ui.cpp:22391-22392` | `const int padX = padOf( tw ), padY = padOf( th );` |
| the gap the sidecar records | `nifskope_ui.cpp:22393` | `const int gapX = gapOf( tw ), gapY = gapOf( th );` |
| the measurement margin is 1%, not 4% | `nifskope_ui.cpp:22277` | `float halfW = maxDx * 1.01f, halfH = maxDy * 1.01f;` |
| the frame is sized from the WIDEST SINGLE view, not the union | `nifskope_ui.cpp:22264` | `maxDx = qMax( maxDx, 0.5f * ( x1 - x0 ) );` |
| the size ladder is still fed the UNION half-extent | `nifskope_ui.cpp:22310` | `const float myExtent = qMax( unionDx, unionDy );` |
| each channel is cropped around THAT view's own centre | `nifskope_ui.cpp:22575` | `const QImage tA = frameOf( matte(), ox, oy );` |
| the viewport fit is widened by the largest offset | `nifskope_ui.cpp:22429` | `const float fitH = qMax( maxOffY + halfH, ( maxOffX + halfW ) * viewH / viewW );` |
| the sidecar's own `gap` line | `nifskope_ui.cpp:22683` | `ms << "gap " << gapX << " " << gapY` |
| the sidecar's per-frame offset lines, up-positive | `nifskope_ui.cpp:22728` | `ms << "frameoff " << i << " " << j << " "` |
| the frames that had to be clamped, and the fit the gain is read from | `nifskope_ui.cpp:22740` | `ms << "framefit " << maxDx << " " << maxDy << " " << unionDx << " " << unionDy` |
| the reader takes `gap`, and an older `pad` as HALF a gap | `lodgen.cpp:2583-2602` | `card.octPadX = card.octGapX / 2;` |
| per-frame offsets read off the sidecar, placed once the grid is known | `lodgen.cpp:2651-2670` | `card.octFrameOff[2 * ( fj * card.oct + fi )] = rawFrameOff[k + 2];` |
| `mips = log2(min(gapX,gapY))`, floored at 1 | `lodgen.cpp:2778-2779` | `for ( int g = mipUnit; g >= 2; g /= 2 )` |
| `auxMips` comes down with the halved gap | `lodgen.cpp:2806-2808` | `for ( int g = mipUnit / auxDiv; g >= 2; g /= 2 )` |
| the card `.lodm` carries `pad` (per side) and `gap` | `lodgen.cpp:2864-2865` | `oc.insert( QStringLiteral( "gap" ), QJsonArray{ gapX, gapY } );` |
| the card `.lodm` carries `frameOffset`, absent when the bake wrote none | `lodgen.cpp:2882-2883` | `oc.insert( QStringLiteral( "frameOffset" ), fo );` |
| a `cardArray` layer carries its own `frameOffset` | `lodgen.cpp:8870-8872` | `o.insert( QStringLiteral( "frameOffset" ), L.frameOff );` |
| the `cardArray` `.lodm` carries `pad` and `gap` | `lodgen.cpp:8852-8854` | `arr.insert( QStringLiteral( "gap" ), QJsonArray{ g.gapX, g.gapY } );` |
| `_fs.DDS` is BC3/DXT5 with its alpha | `lodgen.cpp:2695` | `lodgenWriteDds( dds, 2 * w, h, px, true );` |
| per-card game path stem | `lodgen.cpp:2829` | `QStringLiteral( "Data\\Textures\\Lodgen\\Cards\\" ) + id + QStringLiteral( "_oct" )` |
| emissive sheet written BC1, no alpha | `lodgen.cpp:2823-2827` | `// the emissive sheet is BC1: RGB only, no alpha to carry` |
| dilation depth `max(8, max(fw,fh)/8)`, per card | `lodgen.cpp:2734` | `const int deep = qMax( 8, qMax( card.octTileW, card.octTileH ) / 8 );` |
| the same depth in the card-array path | `lodgen.cpp:8737` | `const int deep = qMax( 8, qMax( fw, fh ) / 8 );` |
| card-array group key = family + sheet size | `lodgen.cpp:8446` | `const QString key = QString( "%1\|%2x%3" )` |
| DX10 array header fields | `lodgen.cpp:4306-4336` | `const quint32 dx10[5] = { bc3 ? 77U : 71U, 3U, 0U, quint32( layers.size() ), 0U };` |
| mip filter box + round-half-up, single sheet | `lodgen.cpp:4046` | `// texels, or neighbouring views blend into one another.` |
| the same filter in the ARRAY writer | `lodgen.cpp:4198` | `std::vector<quint8> & out, int maxMips = 0 )` |
| the bake asserts the projection, and `WW_IMPOSTOR_PERSP` is the way back | `nifskope_ui.cpp:21814` | `skope->ogl->setProjection( bakePersp );` |
| the sidecar's `projection` line, read back off the live viewport | `nifskope_ui.cpp:21845` | `ms << "projection "` |
| the `orthofit` line, and the field of it that moves | `nifskope_ui.cpp:22741` | `ms << "orthofit " << fitH << " " << orthoFitGot` |
| the reader takes the sidecar's `projection` word verbatim | `lodgen.cpp:2620` | `card.octProjection = line[1];` |
| the card `.lodm` carries `projection`, and only when the sidecar said so | `lodgen.cpp:2898` | `oc.insert( QStringLiteral( "projection" ), card.octProjection );` |
| a `cardArray` layer carries its own `projection` | `lodgen.cpp:8872` | `o.insert( QStringLiteral( "projection" ), L.projection );` |
