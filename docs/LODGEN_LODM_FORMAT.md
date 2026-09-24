# `.lodm` v1 — the LOD material sidecar

**Contract version: `lodm` payload version 1, envelope version 1.**
**Status: SHIPPED.** Writer `src/io/lodmfile.cpp` (envelope) plus four payload
builders in `src/lodgen.cpp`; reader `lodmParse()` in the same file. Consumers:
the FO4CS *Improved LOD* module, and this generator itself when it reads a
source override.

This page is **the contract**: what the bytes and keys are, what a reader may
assume, what is refused. `docs/LODGEN_IMPOSTOR_SPEC.md` is the design record —
why the channels are what they are, and the measurements behind them. Where the
two disagree, **this page wins**, because every statement here is traced to a
writer line in the provenance footer.

---

## 1. Envelope — 12 bytes, then JSON

| off | type | value |
|---|---|---|
| 0x00 | char[4] | magic `LODM` (`'L','O','D','M'`, no NUL) |
| 0x04 | u32 LE | envelope version, **1** |
| 0x08 | u32 LE | payload size in bytes |
| 0x0C | u8[n] | the payload: **compact** UTF-8 JSON, one object |

`fileBytes == 12 + payloadSize` **exactly** — a reader refuses on any other
size rather than parsing a prefix. The payload cap is **4 MiB**; a larger
declared size is refused before any allocation. There is no CRC and no
alignment: the file is a few hundred bytes and the JSON parse is its own
integrity check.

The envelope deliberately mirrors PBRM's, so a four-byte sniff tells the two
apart without a parse.

### 1.1 Reader refusals, by name

A reader refuses — naming the field — on:

1. fewer than 12 bytes;
2. magic != `LODM`;
3. envelope version != 1;
4. declared payload > 4,194,304;
5. declared payload != `fileSize − 12`;
6. the payload is not well-formed JSON, or is not a JSON **object**;
7. `lodm` is neither 1 nor 2; and `lodm` 2 on a kind outside the card family (card, cardArray,
   aggregate), refused naming the version and the kind (§3.3);
8. `family` is neither `"legacy"` nor `"pbr"` — **a third family word is a hard
   refusal, not a fallback.** The terrain-VT index used to say `legacy` for that
   reason alone; since 2026-09-11 it says `pbr` and means it (see §5).

Everything else is defaulted, never refused: an absent `kind` reads as
`"source"`, an absent `emissiveScale` as **1.0**, an absent `heightInBlue` as
false, an absent texture key as the empty string ("keep whatever the source
had").

---

## 2. The payload object — every key

One flat object. Keys a reader does not know are ignored, and the writer never
emits a key whose value is the default (that is what keeps the file compact).

| key | type | present on | meaning |
|---|---|---|---|
| `lodm` | int | every kind | payload version, **1** |
| `family` | string | every kind | `"legacy"` or `"pbr"` — decides the texture key names and what the third texture's channels mean |
| `kind` | string | every kind | `"source"` \| `"card"` \| `"array"` \| `"cardArray"` \| `"aggregate"` \| `"terrainVT"`; **absent means `"source"`** |
| `textures` | object | every kind but `terrainVT` | see §2.1 |
| `emissiveScale` | number | `source`, `card`, `aggregate` | the multiple a consumer scales the emissive **sheet** by; absent = 1; **0 means this set emits nothing**, and an `aggregate` always writes 0 because it has no emissive sheet at all (§3a) |
| `heightInBlue` | bool | `source` only | the source normal's blue channel carries height, not Z |
| `card` | object | `card` | §3 |
| `aggregate` | object | `aggregate` | §3a |
| `array` | object | `array`, `cardArray` | §4 |
| `terrain` | object | `terrainVT` | §5 — defined by `docs/LODGEN_TERRAIN_VT.md`, not here |

### 2.1 `textures` — the key names are family-dependent

| slot | legacy key | pbr key | file suffix (legacy / pbr) | format | channels |
|---|---|---|---|---|---|
| colour | `diffuse` | `baseColor` | `_d` / `_bc` | BC3 | RGB colour (unlit, sRGB), **A = coverage** |
| normal | `normal` | `normal` | `_n` / `_n` | **BC7** (a `DX10` header, `BC7_UNORM`; since 2026-09-23, lane IMPOSTORDEPTH2: BC3 carried the height in the 5:6:5 colour block) | R = normal X, G = normal Y, B = height, A = sway weight |
| mask | `gsaos` | `rmaos` | `_gsaos` / `_rmaos` | BC3 | R gloss / roughness, G specular / metallic, B AO, A subsurface mask |
| emissive | `emissive` | `emissive` | `_g` / `_e` | BC1 | RGB emissive colour, no alpha |

Reader rules a consumer may rely on:

* Normal **Z is not stored**; rebuild it as `sqrt(1 − x² − y²)`.
* **Coverage lives only on the colour sheet's alpha.** That is why the emissive
  can be BC1.
* Everything is **linear except the colour sheet**, which is sRGB.
* The emissive key is `emissive` under **both** families; only the file suffix
  differs, so a directory listing tells you the family.
* Values are **game paths**, backslash-separated, with the leading `Data\` kept
  as the writer emitted it. Compare case-insensitively (the engine's own string
  pool is).
* An **empty or absent** slot on a `source` file means *keep the vanilla
  texture for that slot* — it is not "bind black".

### 2.2 `emissiveScale`, and where it is not

`emissiveScale` is a **top-level** float on a `source` or a `card` file: one
set, one multiple. On an `array` or a `cardArray` it is instead
`array.emissiveScale`, **a list parallel to `array.layers`** — two layers of one
array are two materials and do not share a multiple. A reader that looks only at
the top level of an array file gets 1.0 and is wrong for every layer.

It cannot be folded into the sheet: a multiple may exceed 1 and the sheet is
eight bits a channel.

---

## 3. `kind: "card"` — one octahedral impostor set

Written beside the sheets as `<formid8hex>_oct.lodm`; game path
`Data\FO4CSLOD\Cards\<formid8hex>_oct.lodm`. The four sheets are the same
stem with the family's suffixes and `.DDS`.

`card` object:

| key | type | meaning |
|---|---|---|
| `oct` | int | **frames per side, N. ABSENT on a horizon-RING card (§3.2), which says `views` and `grid` instead.** The sheet is **N × N frames = N² views** — `OCT=8` is 64 views, **not 81**. The sheet is `N·frameW` by `N·frameH` pixels |
| `frame` | int[2] | `[frameW, frameH]` in pixels — the size class, longer side = the run's tile rung, shorter side a multiple of 16 |
| `base` | int | the run's chosen resolution before the size ladder. `frame` at or below it is a **rung, not a different run** |
| `half` | float[2] | `[halfW, halfH]`, the quad's half extents in **model units**, spanning the WHOLE frame including its padding |
| `pad` | int[2] | `[padX, padY]`, **the margin in texels on EACH side** of every frame, per axis. The silhouette occupies the inner rect `frame - 2*pad`. Absent on a set written before 2026-09-09: read `max(4, max(frame)/16)` on both axes, which is what those sheets carry |
| `gap` | int[2] | `[gapX, gapY]`, the **distance between two neighbouring silhouettes** across a frame border, per axis, in texels -- exactly `2*pad`, and the quantity bungo's number names (*"8 pixels of distance between two rendered objects"*, 2026-09-09). `mips` is `log2(min(gapX,gapY))` by construction. Absent with `pad` present = a set from earlier the same day whose `pad` was written as the whole spacing; absent with `pad` absent = older still. Both of those wrote a per-side number, so their gap is twice it and the same expression gives the chain they were built for |
| `center` | float[3] | **the offset from the object's PIVOT to the card's centre**, in model units. The pivot is the NIF root, i.e. the reference's own placement origin, so a reader places the quad at `pivot + center`. The bake points its camera at this one point in every one of the N-squared views, so it is the projection of the frame's centre in all of them -- which is what makes the model-to-card transition still (3.1 below) |
| `depthSpan` | float | world units the height channel spans: `units = (B − 0.5) × depthSpan`, 0.5 = the card plane |
| `mips` | int | stored mips, `max(1, log2(min(gapX,gapY)))`: the chain stops at the last level where **each of the two frames meeting on an interior border still keeps a whole texel of margin**, because at the next level that margin is half a texel and a border tap reaches across (bungo, 2026-09-09 evening: ship one mip fewer -- a 128 frame at gap 8 ships 128/64/32) |
| `frameOffset` | float[2·oct²] | (`2·views` on a ring card, §3.2) **per-frame positioning.** Where each frame's quad sits relative to `center`, in model units, along that view's own right and up axes: frame `(i,j)` at index `j·oct + i`, so `[2·(j·oct+i)]` is its right offset and the next its up offset. Every frame shifts its own silhouette to its own centre, so the frame holds the widest SINGLE view rather than the union of all of them; `half` is still ONE size for the whole card. Absent = a set from before 2026-09-09 evening, whose frames were all centred on `center`. A reader that ignores it draws every quad at `center`, and the tree steps sideways by the offset when the mesh hands over |
| `auxDiv` | int | **present only when > 1.** The normal, mask and emissive sheets were written at `1/auxDiv` of each side; the colour sheet never divides. Sampling is unaffected (normalised UV); a consumer needs this only to size its own allocation |
| `projection` | string | **the camera the sheet was photographed through**: `ortho`, or `persp` for a set deliberately baked the old way. `half`, `center` and `frameOffset` are world measurements taken off viewport pixels through ONE units-per-pixel constant, which only an orthographic camera makes true; this is what says they describe the sheet beside them. **Absent = the bake did not say, and every bake that did not say drew a 60-degree perspective frustum** -- absence is the older, foreshortened vintage, not "unknown". See `docs/LODGEN_CARD_SHEETS.md` §3.7 |
| `coverage` | object | **the coverage contract of the base-colour sheet**: `{ floor, test, base }`. `floor` is the coverage at which the bake counted a texel covered and measured `half` and every `frameOffset`; `test` is the alpha a consumer must ALPHA-TEST at to select that same set (`128`, i.e. 0.5, on a sheet written under the contract); `base` is the alpha the floor was written at, so the coverage FRACTION is `floor + (a - base) * (255 - floor) / (255 - base)`. **Absent = the sheet's alpha is the raw fraction and its declared extents describe the silhouette at 16/255, which is what such a set must be tested at** -- reading an older set at 0.5 draws a tree up to 5.41 texels of half-width narrower than `half` declares. See `docs/LODGEN_CARD_SHEETS.md` §4 |
| `conv` | string | **the view convention the frames were photographed under** (2026-09-19, the azimuth repair). `spec1` = frame `(i,j)` is the view from direction `(i,j)`, the spec's own law, and every bake from that exe on writes it. **Absent = a set baked before the repair, whose azimuth is turned by 180 degrees: it must be re-baked**; a viewer opens it only under the diagnostic `AsBaked` convention and says so. An unrecognised word is carried through verbatim, never read as `spec1` (`src/lodgen.cpp` `octConv`; `src/impostorcard.h` `legacyBake()`; `src/impostoroct.h` `Convention`) |
| `source` | string | the model file photographed; absent when unknown |

**Frame addressing.** Frame `(i, j)`, `i` and `j` in `0 … N−1`, occupies pixels
`[i·frameW, (i+1)·frameW) × [j·frameH, (j+1)·frameH)`. Its view direction is

```
u = i/(N−1)·2 − 1        v = j/(N−1)·2 − 1
x = (u+v)/2              y = (u−v)/2         z = 1 − |x| − |y|      normalise
```

so the four corners are exact horizon directions and the centre of the grid is
the exact top (a frame only when N is odd). A direction always falls inside a
triangle of three frame centres; that `(N−1)²`-cell triangle mesh **is** the
blending rule, with the diagonal and weights fixed in
`docs/LODGEN_CARD_SHEETS.md` §2. Frames are rectangular and
one size for every view.

Every frame carries a transparent **margin** of `pad[0]` texels on its left and
right and `pad[1]` on its top and bottom -- so two neighbouring silhouettes are
`gap[0]` / `gap[1]` texels apart across the border they share, and the sheet's
outer border, which has no neighbour and is sampled clamped, carries half of
that. Every channel under a transparent
texel is dilated out from the silhouette and then flooded with the frame's
average, so filtering and mips never pull black into an edge. `halfW`/`halfH`
include the padding: **the quad is the frame.**

### 3.1 Why the card does not move when the mesh becomes it

bungo's requirement, 2026-09-09: *"the tree must be positioned correctly, so that
when a 3d tree transitions to an imposter, the tree won't change position"*.

The bake photographs every view with the camera pointed at ONE model-space point,
and `center` is that point expressed as an offset from the object's pivot. Each
frame is the crop of that view about that point, plus and minus `halfW` by
`halfH` in model units -- **shifted, per frame, by `frameOffset`**, which the
same bake measured and wrote. So a reader that draws the quad at

```
pivot + center + frameOffset[2k] * right(i,j) + frameOffset[2k+1] * up(i,j)
```

with `k = j*oct + i`, spanning plus/minus `half`, with the frame's own UV rect,
reproduces the model's silhouette in the same place at the same size, from any of
the N-squared directions. `right` and `up` are that view's own screen axes, the
basis the frame was photographed in; `half` is one pair for the whole card, so
the tree is the same SIZE in every frame and only its PLACE moves.

**All of this rests on the bake's camera being ORTHOGRAPHIC**, because `half`
and `frameOffset` are world lengths read off viewport pixels through one
units-per-pixel constant. Under a perspective camera that constant is not one --
a point `d` in front of the card plane is magnified by `eye / (eye - d)` -- so
the quad drawn from those numbers is the wrong size AND the silhouette inside it
is foreshortened, wider at the frame's near edge than at its far one. Sets baked
before 2026-09-10 carry no `projection` key and were photographed that way; a
consumer that cares about the transition should treat their numbers as
approximate and ask for a re-bake.

Three ways to get it wrong, and what each costs:

* treating `center` as zero draws the card at the pivot instead, which for a
  tree is the trunk's base: TreeHero01's `center` is `[-12.81, -5.23, 1070.19]`,
  so the card would sit **1,070 units low**;
* honouring `center` but ignoring `frameOffset` draws every frame centred, which
  is a set from before the law and makes the tree step sideways by that frame's
  offset as the mesh hands over;
* drawing a set whose `projection` is absent or `persp` as if it were metric: the
  quad is sized from numbers taken through the wrong projection, so it neither
  matches the mesh's silhouette nor holds still across the N-squared views.

The first two are controls the generator's transition gate runs and requires to
FAIL; the third is refused by name rather than measured, because a set that does
not say what camera made it cannot be corrected after the fact.

---

---

### 3.2 A horizon-RING card (2026-09-24, lane CARDFIX1 step 5)

bungo, 2026-09-23 04:4x, RULED: *"for fo4cs use the convention was 22.5 degrees
per take"*. A ring card is photographed at 16 azimuths, 22.5 degrees apart, at
elevation 0 -- not over the hemi-octahedral grid. **It is an option, not the
default:** bungo RULED 2026-09-25, *"Yes, 8x8 is the default choice for a bake"*,
after step 5 measured the N8 grid better than the ring at every elevation, the
horizon included. The card bake driver defaults every run, trees included, to the
grid (`RING=0`); `RING=16` bakes the ring (`tests/spells/impostor_ring.sh` R7).

A ring card uses **the aggregate's own layout keys (section 3a)**, not a third
layout:

| key | ring card | grid card |
|---|---|---|
| `oct` | **ABSENT** | N |
| `views` | V (16) | absent |
| `grid` | `[V, 1]` | absent |
| `frameOffset` | `2·V` numbers, view `v` at `[2v]`, `[2v+1]` | `2·N²` |

Every other key (`frame`, `half`, `pad`, `gap`, `mips`, `center`, `depthSpan`,
`coverage`, `projection`, `conv`, `auxDiv`) means exactly what it means on a grid
card. Frame `v` occupies pixels `[v·frameW, (v+1)·frameW) × [0, frameH)` and was
photographed from

```
eye(v)   = ( cos φ, sin φ, 0 )          φ = 2π·v / V
right(v) = ( −sin φ, cos φ, 0 )         up(v) = ( 0, 0, 1 )
```

under the `spec1` convention. **A reader blends the TWO frames that bracket the
camera's azimuth**, `f = φ_cam / (2π) · V`, `v0 = floor(f) mod V`, `v1 = v0 + 1
mod V`, weights `1 − t` and `t` with `t = f − floor(f)`; elevation selects
nothing (there are no frames above the horizon; what that costs is measured in
`tests/spells/impostor_ring.sh`, row M). At the slider's crisp end the stronger
of the two is drawn alone.

**Why there is no `oct` key, and why the sheet is one row.** A reader that knows
only the grid then finds no grid and refuses the set BY THAT KEY'S NAME (the
NifSkope reader before this change: *"oct is 0, outside the bake's own 2..16"*).
A 4 × 4 packing of the same 16 frames would carry a square sheet and an `oct 4`
that every existing reader would accept -- and draw hemisphere views from
horizon photographs without a word. The one-row sheet cannot be taken for any
N × N grid. Its width is `V·frameW`: 4096 at a 256 tile, 8192 at 512, 16384 at
1024, which is the Direct3D 11 texture limit, so a 16-view ring above a 1024
tile would not load (nothing enforces this yet; the driver's TILE default is 256).
At one tile it holds a quarter of an N8 sheet's pixels.

**Card arrays** (section 4) carry the same keys on the array object: a ring set
groups only with ring sets of its own sheet size (the group key gains `|ring`,
and the file name gains `.ring` after its `WxH`:
`<ws>.LodgenCards.legacy.2304x256.ring_d.DDS`; until 2026-09-25 the name took the
key's `|` and the array could not be written at all).
**The aggregate** (section 3a) composites from grid cards only; a ring set given
to it is refused by name and counted (`aggregate cards: refused N horizon-ring
set(s) by name`). Teaching the aggregate the ring is owed.

**A ring alone does not move the version.** A reader that does not know `views` on a card
refuses the set; nothing is misread. Version 2 is the model sway (§3.3).

### 3.3 `lodm` 2 -- the model's OWN sway (2026-09-24, lane CARDFIX1 step 6)

bungo, 2026-09-23: *"sway from the tree's model's own wind weights would be neat"*; RULED 2026-09-24
21:1x as **sway A**. A card baked from a model that has at least one tree-animation shape (a
`BSLightingShaderProperty` with the vertex-alpha-animation flag -- the same test that makes the mask
"tree") writes, per texel,

    _n.A = W x h

`W` the model's own vertex-alpha wind weight at that texel (the only wind input the game's tree vertex
shader reads; 0 on a shape without the flag, which the game never moves), `h` the linear height up from
the view's own coverage bottom row. A model with NO tree-animation shape keeps the synthetic
`h^2 x (0.35 + 0.65 r)`, byte for byte. The bake's sidecar says which: `sway model` or `sway synthetic`.

Because `_n.A` then MEANS something else, the payload version moves, on the card family only:

| key | on | type | meaning |
|---|---|---|---|
| `lodm` | card, cardArray | int | **2** when the set (any layer of an array) carries model sway; 1 otherwise, with none of the keys below |
| `sway` | card | string | `"model"` -- `_n.A` is `W x h` |
| `leafAmplitude`, `leafFrequency` | card | number | the placed base's own leaf numbers (STAT DNAM / TREE CNAM); both 0 in the record reads as 1 / 1 |
| `array.sway`, `array.leafAmplitude`, `array.leafFrequency` | cardArray | list | one per layer, parallel to `layers`; a synthetic layer says `"synthetic"` and 1 / 1 |

* **An old reader refuses a v2 card by name** (`payload is not a lodm 1 object`), which is the point:
  it would otherwise draw a real wind weight under the synthetic law's assumptions.
* **A SOURCE (or any kind outside card, cardArray, aggregate) claiming 2 is refused by name**:
  `lodm 2 is the card family's version (...); a "<kind>" .lodm is lodm 1`.
* The envelope version stays 1.
* **Owed:** the aggregate (§3a) composites the model weight texel by texel already (§3a.3), but writes
  no `sway` key and stays 1; teaching it v2 is owed with the ring (lodgenaggregate.cpp is not this
  lane's). FO4CS's reader is owed (`docs/LODGEN_IMPOSTOR_SPEC.md`, the owed paragraph).

## 3a. `kind: "aggregate"` — one forested CELL's whole tree cluster on one card set

bungo, 2026-09-11 08:2x → 08:3x, verbatim: *"At ring 3 the bake takes each
cell's trees, places their cards with the same rotation and mirror the
repetition breaking would give them, photographs the whole cluster from the
horizon views, and writes one aggregate sheet per cell. The ring 3 instance list
then holds one placement per cell instead of one per tree. At the ring 2 to 3
border the per-tree cards cross-fade into the cell card. Same sheet format, same
sway rule from the height channel."* — and *"1 sounds good"*.

Written beside the sheets as
`Data\FO4CSLOD\<worldspace>\Aggregate\<cellX>_<cellY>_agg.lodm`; the
three sheets are the same stem with the family's suffixes and `.DDS`. **The path
is DERIVED from the worldspace and the cell and is not stored in the `.lodi`
row**, so a reader can never be handed a path that disagrees with the cell it
came from (zero-authoring, CONSTITUTION 10).

**THREE sheets, not four.** Colour + coverage, normal + height + sway, and the
mask. There is **no emissive sheet**: a forest emits nothing, and
`emissiveScale` is written as **0**, which is exactly how a set says so in one
number (§2.2). A reader binds black. This is the one place an `aggregate`
differs from a `card` in its texture set, and it is stated here rather than left
to a missing file.

`aggregate` object:

| key | type | meaning |
|---|---|---|
| `cell` | int[2] | `[cellX, cellY]`, the exterior cell this set stands for. With the worldspace it is the set's whole identity: the file's own path is derived from these two numbers |
| `views` | int | **azimuths photographed at the HORIZON**, one elevation band. 8 by default. This is NOT `oct`: a card's `oct` is frames per side of a hemi-octahedral grid, and an aggregate has no grid — it has a ring |
| `grid` | int[2] | `[views, 1]`, the sheet's frame layout: the frames sit in ONE ROW, frame `v` at pixels `[v·frameW, (v+1)·frameW) × [0, frameH)`. Stated rather than implied, so a later elevation band is a `grid` change and not a reinterpretation |
| `frame` | int[2] | `[frameW, frameH]` in texels. The long side is the run's aggregate tile, the short side the smallest multiple of 16 whose inner rect is not narrower than the measured silhouette — `docs/LODGEN_CARD_SHEETS.md` §3.2, unchanged |
| `pad`, `gap` | int[2] | the margin on each side of a frame, and twice it — the distance between two neighbouring silhouettes across a frame border. The card law, unchanged |
| `mips` | int | `max(1, log2(min(gap)))`, the card law, unchanged |
| `half` | float[2] | the quad's half extents in **world units**, spanning the WHOLE frame. ONE pair for every view, exactly as a card's is |
| `center` | float[3] | the card's centre in **world** coordinates — the cell's own centre in X and Y, and the mid-height of the cluster in Z. Unlike a card's `center`, which is an offset from a pivot, an aggregate has no pivot: it is a placement in the world |
| `depthSpan` | float | world units the height channel spans: `units = (B − 0.5) × depthSpan`, 0.5 = the card plane. Same law, same decoder |
| `boundRadius` | float | the tree cloud's radius about `center`, for the projected-size test that selects the aggregate |
| `trees` | int | **how many tree placements were photographed into this sheet.** It must equal the `coveredCount` of this cell's row in the `.lodi` — that equality is the count-identity gate, and it is stated in two files on purpose so one can check the other |
| `frameOffset` | float[2·views] | where each view's quad sits relative to `center`, along that view's own right and up axes, in world units: view `v` at `[2v]` and `[2v+1]`. The card law of `docs/LODGEN_CARD_SHEETS.md` §3.6, applied per azimuth |
| `identity` | string | **`"per-aggregate"`, always.** The far-shadow pass keys on the identity index (bungo 08:4x), and once a cell's trees are one card they are ONE caster: the aggregate carries its own index, `0x80000000 \| aggregateIndex` in the `.lodi` row, and NOT the dominant tree's. The sheet therefore carries no identity channel, and this word says that is deliberate rather than missing |
| `projection` | string | `"ortho"`. An aggregate is composited from orthographic card sheets by an orthographic resample, so its `half`, `center` and `frameOffset` are metric. A value other than `ortho` is not written by any generator |
| `coverage` | object | `{floor, test, base}`, the colour sheet's coverage contract, identical in meaning to a card's (`docs/LODGEN_CARD_SHEETS.md` §4). This bake writes `16 / 128 / 160` |

### 3a.1 What a reader does with one

```
quadCentre = center + frameOffset[2v] * right(v) + frameOffset[2v+1] * up(v)
```

spanning ±`half`, with `right(v) = (−sin φ, cos φ, 0)`, `up(v) = (0, 0, 1)` and
`φ = 2π·v/views` — the azimuth of the view the frame was photographed from, with
the eye direction `(cos φ, sin φ, 0)`. Between two azimuths a reader blends the
two neighbouring frames; the frames are a RING, so view `views−1` blends back
into view 0.

**When to draw it instead of the trees.** The `.lodi` header carries
`aggSwitchPx` and `aggBandRatio`
(`docs/LODGEN_NATIVE_LODO_LODI.md` §4.6): the aggregate takes over when the
CELL's projected width falls to `aggSwitchPx`, and the per-tree cards cross-fade
out over `aggSwitchPx … aggBandRatio × aggSwitchPx`. Which instances to stop
drawing is not a guess either — the `.lodi` row names them, one u32 each.

### 3a.2 The three things an aggregate does NOT inherit from a card

1. **`oct` does not apply.** An aggregate has `views` and `grid`, and a reader
   that looks for `oct` on an aggregate finds nothing. The hemi-octahedral
   mapping is a hemisphere's worth of directions; an aggregate is photographed
   at the horizon only, because that is where ring 3 is.
2. **`center` is WORLD, not an offset from a pivot.** A card stands in for a
   placed object and is drawn at `pivot + center`; an aggregate stands in for a
   CELL and is drawn at `center`.
3. **There is no emissive sheet and no `textures.emissive` key.** See above.

### 3a.3 What the sway channel carries, and why it is not re-derived

bungo asked for *"the same sway rule from the height channel"*. The rule
(`h²·(0.35 + 0.65·r)`, `h` up from the view's own bottom row) is measured
**per tree, on the tree's own card**, and carried through the composite
unchanged — it is not recomputed from the aggregate frame's bottom row. Deriving
it from the aggregate would make a tree standing on a hill sway at its trunk,
because the aggregate's bottom row is the lowest point in the whole cell and not
the base of that tree. Each texel keeps the sway weight of the tree it came
from, which is the same rule applied where the rule means something.


## 4. `kind: "array"` and `kind: "cardArray"`

`array` object:

| key | on | type | meaning |
|---|---|---|---|
| `class` | both | int[2] | `array`: the **per-layer texture size**. `cardArray`: the **whole sheet size** (`oct·frameW` × `oct·frameH`). The `<WxH>` in the file name is this same pair |
| `layers` | `array` | string[] | one entry per layer: the **source colour texture path** that layer was built from |
| `layers` | `cardArray` | object[] | one per layer: `{ id, half[2], center[3], depthSpan, frameOffset?, projection?, conv?, coverage?, source }` — `id` is the base's form ID, and the geometry is per layer because two trees of one sheet size are not the same size in the world. `frameOffset` is per layer for the same reason: two sets in one array have different per-frame shifts, and a layer from a set baked before the law carries no key at all. `projection` is per layer for the same reason again: an array can hold a metric set beside a foreshortened one, and only the layer knows which it is. `conv` (§3) is per layer for the same reason: a library part-way through the re-bake holds both vintages in one size class |
| `emissiveScale` | both | number[] | one per layer, **parallel to `layers`** |
| `sway`, `leafAmplitude`, `leafFrequency` | `cardArray`, only when a layer carries model sway (the file is then `lodm` 2) | string[], number[], number[] | one per layer, parallel to `layers` (§3.3) |
| `oct` | `cardArray` | int | frames per side, shared by every layer of the array |
| `frame` | `cardArray` | int[2] | frame size, shared by every layer |
| `pad` | `cardArray` | int[2] | the margin in texels on each side of a frame, per axis, shared. An array is built from the same PNGs and the same dilation as the per-card sets, so it inherits their spacing and their clean mip depth |
| `gap` | `cardArray` | int[2] | the distance between two neighbouring silhouettes across a frame border, per axis, shared -- `2*pad` |
| `mips` | `cardArray` | int | mip cap, shared -- `max(1, log2(min(gap)))` |
| `auxDiv`, `auxClass`, `auxMips` | `cardArray` only, when `auxDiv > 1` | int, int[2], int | the half-resolution auxiliary sheets' divisor, size and mip count |

**A `kind: "array"` file carries no `aux*` keys**, because `--card-half-aux`
applies to card sheets only. A reader must not infer aux sizing on a mesh array.

**Grouping is part of the contract.** An array holds only sets that share
`family` **and** `class`; a card array additionally shares `oct` and `frame`.
That is why frame sizes are quantised into classes at bake time — without it
nearly every base would be alone in its own array, which is the one thing arrays
exist to avoid.

A card set baked before the emissive sheet existed contributes a **black**
emissive layer rather than being dropped, so a layer index always means what the
manifest's `C` line says it means.

---

## 5. `kind: "terrainVT"` — the pyramid index

The one kind that carries **no `textures` object at all**. It names a set of
`.lodt` tile containers and is defined by `docs/LODGEN_TERRAIN_VT.md` §4; the
whole payload lives under `terrain`.

**`family` IS NO LONGER VESTIGIAL HERE** (2026-09-11, bungo: *"you can mirror
how it's set up for the .lodm"*, *"we just add the coverage for whatever's
missing in terrain textures that lod objects have in the texture department"*).

Until container version 2 the pyramid's third sheet was terrain's own invention
— AO, wetness, shore proximity and ground cover — and `family` wrote `"legacy"`
because a tile pyramid was neither family and §1.1 rule 8 hard-refuses a third
word. Version 2 gave the pyramid **this page's own texture family**:

| §2.1 slot | what the pyramid stores | note |
|---|---|---|
| colour | the `color` sheet, RGB albedo with the grass tint folded in | its alpha is FREE, and is the other candidate for the ground cover — see the terrain page §2.2a |
| normal | the `msn` sheet | MODEL-space, not tangent-space: terrain has one basis and needs no tangents. Z IS stored, so the `sqrt(1 - x^2 - y^2)` rebuild does **not** apply |
| mask | the `mask` sheet, **`rmaos` exactly**: R roughness, G metallic, B AO, **A ground cover in place of subsurface** | the one substitution, and the index names it in the sheet's `channels` string |
| emissive | the `emissive` sheet, RGB, BC1 | written only when a layer supplies one; **absent** otherwise, and `terrain.emissive` says `"none"` in words |

Two sheets have no slot on this page and are terrain's own: **height** (R16, the
shadow heightmap's encoding) and nothing else.

So `family` is **`"pbr"`** and it describes the bytes: a legacy landscape
material is CONVERTED at bake — its gloss inverted into roughness, its metallic
0, never guessed from its specular colour — and `terrain.maskRules` counts how
many landscape textures came by which road, so a reader can audit the word
instead of trusting it. `kind` is still the discriminator for WHICH payload
object to read.

* **Its path is deliberately unreachable from a source lookup.** The index lives
  at `Data\Terrain\<EDID>.VT.lodm`, and `lodmSourceCandidate()` always prepends
  `materials\`, so no shape can ever resolve to it by accident.

---

## 6. Where a SOURCE `.lodm` is looked for

`lodmSourceCandidate( material, diffuse )`, in this order:

1. **The shape names a material** — take that path, normalise `/` to `\`, strip
   a leading `data\`, replace the extension with `.lodm`.
   `materials\lod\foo.bgsm` → `materials\lod\foo.lodm`.
2. **The shape names no material** — take the diffuse, normalise, strip a
   leading `data\` **and then a leading `textures\`**, prepend `materials\`,
   replace the extension. `textures\lod\foo_d.dds` → `materials\lod\foo_d.lodm`.
3. Empty input → no candidate.

The extension is replaced only when the last `.` is after the last `\`; a
path with no extension simply gains `.lodm`.

**Search order for the bytes**, once the candidate path is known: the resource
stack (mod folders and archives in Mod Organizer order, **last entry wins**, a
loose file beating an archive wherever the archive sits), then the loose data
root (`--data-root` / `WW_LODGEN_DATA_ROOT`), then the game's own resources.
`lodgen --probe <relpath>` reports which entry actually supplied a file.

**What a source `.lodm` does:** its textures replace the source shape's for that
slot (an empty slot keeps the vanilla texture), its third texture is taken
**raw**, and its `family` names the set. A set's family is decided per source for
mesh arrays, and **per base** for cards — a card set is `pbr` only when *every*
textured shape of the model carries a pbr `.lodm`, else legacy.

---

## 7. Invariants a reader may assume

1. `lodm == 1` and `family ∈ {legacy, pbr}` on every file that parsed.
2. `array.emissiveScale.size() == array.layers.size()` on `array` and
   `cardArray`.
3. On a `cardArray`, every layer's sheet is `oct·frame[0]` × `oct·frame[1]`, and
   `class` equals that pair.
4. On a `card`, `frame[0] ≤ base` and `frame[1] ≤ base`; a frame **below** the
   base is a size-ladder rung and is correct, not a mismatch.
5. `mips == max(1, log2(min(gap[0], gap[1])))`, so **no shipped mip bleeds across
   a frame border**: a reader sampling on a frame's own UV border reaches half a
   texel into the neighbour, and the margin at the deepest shipped level is
   still a whole texel on both axes -- measured 0 of 19 sheets bleeding, against
   13 of 19 (worst 64/255) under the cap one level deeper. On a set with no `pad` key the
   invariant does not hold -- those sheets shipped one bleeding level whenever
   their frame was 96 texels or more (measured 26/255 of a neighbour's alpha
   across 16 of 28 borders at mip 4 of a 128x128 frame).
6. `depthSpan > 0` on any set with a usable height channel.
7. Texture paths, where non-empty, are Data-relative game paths with
   backslashes.
8a. `card.coverage` / a `cardArray` layer's `coverage`, when present, is
   `{floor, test, base}` with `1 <= floor < test <= base <= 255`, and it is the
   ONLY statement of which alpha selects the silhouette `half` and `frameOffset`
   describe. A reader that ignores it and tests at 0.5 on a set that carries no
   key draws a smaller tree than the mesh it replaced; a reader that ignores it
   on a set that DOES carry one is right by accident, because the contract this
   generator writes puts `test` at 0.5 exactly.

8. `card.projection` / a `cardArray` layer's `projection`, when present, names
   the camera the sheet was photographed through. `ortho` is the only value a
   generator writes for a metric set; ABSENT means the bake did not say, and
   every bake that did not say drew a 60-degree perspective frustum, so the
   set's `half`, `center` and `frameOffset` are approximate and its frames are
   foreshortened. A reader may refuse such a set by name; it must not silently
   treat absence as `ortho`.

## 8. Sample files

`kind: "card"` files for 19 Sanctuary trees exist twice on disk:
`scratchpad/cardfit_20260909/cards_after` (this contract, with `pad`) and
`scratchpad/images_20260909/gen/cards_trees19` (the same trees before
2026-09-09, without it -- the fallback path's own fixture). For `cardArray` the
fixture is `tests/spells/lodgen_card_arrays.sh`, which builds two synthetic card
sets and decodes every field of the resulting `.lodm`.

---

## Provenance

Re-read 2026-09-09 after lane CARDPAD added `card.gap` / `array.gap` and made
`card.pad` the PER-SIDE half of it (bungo's correction of lane CARDFIT3 the same
day). Anchor text is quoted beside every line number.

| file | sha256 (16) | lines |
|---|---|---|
| `src/io/lodmfile.cpp` | `f3d9a99b7a12677b` | 115 |
| `src/lodgen.cpp` | `c05fd079655ac03e` | 8,924 |
| `src/nifskope_ui.cpp` | `1073ddef14f8562e` | 31,495 |
| `src/gl/glmesh.cpp` | `2352b01248694327` | 1,118 |

**Re-derived 2026-09-10 by lane DOCS2** (`ww-contract-provenance` step 3, script
`scratchpad/docs2_20260910/anchors.py`): every line number below was found again
from its own anchor text against the sources stamped above, never shifted by a
delta. 22 of 34 rows moved, 12 were already right; the coverage-sidecar row's anchor
had a literal newline pasted into it, which had broken that markdown row in two.
Then `src/nifskope_ui.cpp` moved TWICE MORE under a concurrent lane while this
page was being written, and 5 of those rows were re-derived again each time; the
stamp above is the state the pass finally settled on, with the source's hash
unchanged across the last run. **That file is under live edit: check its sha256
before trusting a number here.**

| claim | line | anchor |
|---|---|---|
| magic, envelope version, 4 MiB cap | `lodmfile.cpp:10` | `static const qsizetype LODM_PAYLOAD_CAP` |
| 12-byte envelope, exact payload size | `lodmfile.cpp:16-38` | `bytes.size() < 12`, `declared != bytes.size() - 12` |
| `lodm` 1 or 2 refusals | `lodmfile.cpp:46-63` | `payload is not a lodm 1 object`, `lodm 2 is the card family's version` |
| family hard refusal | `lodmfile.cpp:52-54` | `family must be legacy or pbr` |
| `kind` defaults to `source` | `lodmfile.cpp:56` | `.toString( QStringLiteral( "source" ) )` |
| `emissiveScale` defaults to 1 | `lodmfile.cpp:63` | `.toDouble( 1.0 )` |
| family-dependent key and suffix names | `lodmfile.h:113-118` | `lodmColorKey`, `lodmMaskKey`, `lodmColorSuffix` |
| `lodmSourceCandidate` two-branch rule | `lodmfile.cpp:96-118` | `c.prepend( QStringLiteral( "materials\\" ) )` |
| `kind: "card"` key set | `lodgen.cpp:2835-2869` | `root.insert( QStringLiteral( "kind" ), QStringLiteral( "card" ) )` |
| `card.pad`, per axis, in texels a side | `lodgen.cpp:2863` | `oc.insert( QStringLiteral( "pad" ), QJsonArray{ padX, padY } );` |
| `card.gap`, per axis, twice the padding | `lodgen.cpp:2864` | `oc.insert( QStringLiteral( "gap" ), QJsonArray{ gapX, gapY } );` |
| the three vintages all reduce to a GAP, and the cap divides it | `lodgen.cpp:8711-8720` | `const int mipUnit = qMin( gapX, gapY );` |
| `card.center` is the bake's own look-at point | `lodgen.cpp:2866` | `oc.insert( QStringLiteral( "center" ), QJsonArray{ double( card.octCenter[0] )` |
| that point is the scene's bound centre in MODEL space | `nifskope_ui.cpp:22706` | `<< bs.center[0] << " " << bs.center[1] << " " << bs.center[2] << " " << depthSpan` |
| the renderer recomputes a shape's bound FROM VERTICES | `gl/glmesh.cpp:732` | `boundSphere = BoundSphere( verts );` |
| `mips` is derived from the gap, one level shallower than before | `lodgen.cpp:2780-2781` | `frameMips = qMax( 1, frameMips );` |
| `card.frameOffset`, one pair a frame in sheet order | `lodgen.cpp:2882` | `oc.insert( QStringLiteral( "frameOffset" ), fo );` |
| a `cardArray` layer's own `frameOffset` | `lodgen.cpp:8870` | `o.insert( QStringLiteral( "frameOffset" ), L.frameOff );` |
| card `auxDiv` written only above 1 | `lodgen.cpp:2853` | `oc.insert( QStringLiteral( "auxDiv" ), auxDiv );` |
| `kind: "array"` key set, no aux keys | `lodgen.cpp:4599-4618` | `root.insert( QStringLiteral( "kind" ), QStringLiteral( "array" ) )` |
| `kind: "cardArray"` key set incl. `pad` and `gap` | `lodgen.cpp:8841-8886` | `root.insert( QStringLiteral( "kind" ), QStringLiteral( "cardArray" ) )` |
| `array.gap`, shared | `lodgen.cpp:8852` | `arr.insert( QStringLiteral( "gap" ), QJsonArray{ g.gapX, g.gapY } );` |
| `kind: "terrainVT"` payload | `lodgen.cpp:7343` | `QStringLiteral( "terrainVT" )` |
| the `u = i/(N−1)·2 − 1` mapping | `nifskope_ui.cpp:22145-22147` | `auto viewDir = [octN]` |
| `card.projection`, the camera the sheet was photographed through | `lodgen.cpp:2898` | `oc.insert( QStringLiteral( "projection" ), card.octProjection );` |
| a `cardArray` layer's own `projection` | `lodgen.cpp:8872` | `o.insert( QStringLiteral( "projection" ), L.projection );` |
| the layer reads it off its set's own `.lodm` | `lodgen.cpp:8796` | `l.projection = card.value( QStringLiteral( "projection" ) ).toString();` |
| `card.coverage`, the contract | `lodgen.cpp:2919` | `oc.insert( QStringLiteral( "coverage" ), cov );` |
| a `cardArray` layer's own `coverage` | `lodgen.cpp:8874` | `o.insert( QStringLiteral( "coverage" ), L.coverage );` |
| the layer reads it off its set's own `.lodm` | `lodgen.cpp:8803` | `l.coverage = card.value( QStringLiteral( "coverage" ) ).toObject();` |
| the sidecar's three numbers | `lodgen.cpp:2629-2631` | `card.octCovFloor = qBound( 1, line[1].toInt(), 255 );` |
| the bake writes the word after asserting the projection | `nifskope_ui.cpp:21845` | `ms << "projection "` |
| the bake states the coverage contract on the sidecar | `nifskope_ui.cpp:22698` | `ms << "coverage " << covFloor << " " << covTest << " " << covBase` |
| the bake re-encodes the coverage into the sheet | `nifskope_ui.cpp:22561-22569` | `auto coverageEncode = [covFloor, covBase]( int a ) {` |
