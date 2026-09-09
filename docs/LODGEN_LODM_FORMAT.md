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
7. `lodm` != 1;
8. `family` is neither `"legacy"` nor `"pbr"` — **a third family word is a hard
   refusal, not a fallback.** This is why the terrain-VT index says `legacy`
   although it is neither (see §5).

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
| `kind` | string | every kind | `"source"` \| `"card"` \| `"array"` \| `"cardArray"` \| `"terrainVT"`; **absent means `"source"`** |
| `textures` | object | every kind but `terrainVT` | see §2.1 |
| `emissiveScale` | number | `source`, `card` | the multiple a consumer scales the emissive **sheet** by; absent = 1; **0 means this set emits nothing** |
| `heightInBlue` | bool | `source` only | the source normal's blue channel carries height, not Z |
| `card` | object | `card` | §3 |
| `array` | object | `array`, `cardArray` | §4 |
| `terrain` | object | `terrainVT` | §5 — defined by `docs/LODGEN_TERRAIN_VT.md`, not here |

### 2.1 `textures` — the key names are family-dependent

| slot | legacy key | pbr key | file suffix (legacy / pbr) | format | channels |
|---|---|---|---|---|---|
| colour | `diffuse` | `baseColor` | `_d` / `_bc` | BC3 | RGB colour (unlit, sRGB), **A = coverage** |
| normal | `normal` | `normal` | `_n` / `_n` | BC3 | R = normal X, G = normal Y, B = height, A = sway weight |
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
`Data\Textures\Lodgen\Cards\<formid8hex>_oct.lodm`. The four sheets are the same
stem with the family's suffixes and `.DDS`.

`card` object:

| key | type | meaning |
|---|---|---|
| `oct` | int | **frames per side, N.** The sheet is **N × N frames = N² views** — `OCT=8` is 64 views, **not 81**. The sheet is `N·frameW` by `N·frameH` pixels |
| `frame` | int[2] | `[frameW, frameH]` in pixels — the size class, longer side = the run's tile rung, shorter side a multiple of 16 |
| `base` | int | the run's chosen resolution before the size ladder. `frame` at or below it is a **rung, not a different run** |
| `half` | float[2] | `[halfW, halfH]`, the quad's half extents in **model units**, spanning the WHOLE frame including its padding |
| `pad` | int[2] | `[padX, padY]`, **the margin in texels on EACH side** of every frame, per axis. The silhouette occupies the inner rect `frame - 2*pad`. Absent on a set written before 2026-09-09: read `max(4, max(frame)/16)` on both axes, which is what those sheets carry |
| `gap` | int[2] | `[gapX, gapY]`, the **distance between two neighbouring silhouettes** across a frame border, per axis, in texels -- exactly `2*pad`, and the quantity bungo's number names (*"8 pixels of distance between two rendered objects"*, 2026-09-09). `mips` is `1 + log2(min(gapX,gapY))` by construction. Absent with `pad` present = a set from earlier the same day whose `pad` was written as the whole spacing and whose `mips` was `1 + log2(min(pad))`; absent with `pad` absent = older still, and the same older law applies |
| `center` | float[3] | **the offset from the object's PIVOT to the card's centre**, in model units. The pivot is the NIF root, i.e. the reference's own placement origin, so a reader places the quad at `pivot + center`. The bake points its camera at this one point in every one of the N-squared views, so it is the projection of the frame's centre in all of them -- which is what makes the model-to-card transition still (3.1 below) |
| `depthSpan` | float | world units the height channel spans: `units = (B − 0.5) × depthSpan`, 0.5 = the card plane |
| `mips` | int | stored mips, `1 + log2(min(gapX,gapY))`: the chain stops at the last level where **a whole texel of gap still separates the two silhouettes** that meet on an interior frame border, because the next one has them touching |
| `auxDiv` | int | **present only when > 1.** The normal, mask and emissive sheets were written at `1/auxDiv` of each side; the colour sheet never divides. Sampling is unaffected (normalised UV); a consumer needs this only to size its own allocation |
| `source` | string | the model file photographed; absent when unknown |

**Frame addressing.** Frame `(i, j)`, `i` and `j` in `0 … N−1`, occupies pixels
`[i·frameW, (i+1)·frameW) × [j·frameH, (j+1)·frameH)`. Its view direction is

```
u = i/(N−1)·2 − 1        v = j/(N−1)·2 − 1
x = (u+v)/2              y = (u−v)/2         z = 1 − |x| − |y|      normalise
```

so the four corners are exact horizon directions and the centre frame is the
exact top. A direction always falls inside a triangle of three frame centres;
that `(N−1)²` triangle mesh **is** the blending rule. Frames are rectangular and
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
`halfH` in model units. So a reader that draws the quad at `pivot + center`
spanning plus/minus `half`, with the frame's own UV rect, reproduces the model's
silhouette in the same place at the same size, from any of the N-squared
directions.

A reader that treats `center` as zero draws the card at the pivot instead, which
for a tree is the trunk's base: TreeHero01's `center` is
`[-12.81, -5.23, 1070.19]`, so the card would sit **1,070 units low**. That is
the control the generator's transition gate runs and requires to FAIL.

---

## 4. `kind: "array"` and `kind: "cardArray"`

`array` object:

| key | on | type | meaning |
|---|---|---|---|
| `class` | both | int[2] | `array`: the **per-layer texture size**. `cardArray`: the **whole sheet size** (`oct·frameW` × `oct·frameH`). The `<WxH>` in the file name is this same pair |
| `layers` | `array` | string[] | one entry per layer: the **source colour texture path** that layer was built from |
| `layers` | `cardArray` | object[] | one per layer: `{ id, half[2], center[3], depthSpan, source }` — `id` is the base's form ID, and the geometry is per layer because two trees of one sheet size are not the same size in the world |
| `emissiveScale` | both | number[] | one per layer, **parallel to `layers`** |
| `oct` | `cardArray` | int | frames per side, shared by every layer of the array |
| `frame` | `cardArray` | int[2] | frame size, shared by every layer |
| `pad` | `cardArray` | int[2] | the margin in texels on each side of a frame, per axis, shared. An array is built from the same PNGs and the same dilation as the per-card sets, so it inherits their spacing and their clean mip depth |
| `gap` | `cardArray` | int[2] | the distance between two neighbouring silhouettes across a frame border, per axis, shared -- `2*pad` |
| `mips` | `cardArray` | int | mip cap, shared -- `1 + log2(min(gap))` |
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

Two collisions are resolved here rather than discovered:

* **`family` is vestigial.** A tile pyramid is neither legacy nor PBR. It writes
  `"legacy"` only because §1.1 rule 8 hard-refuses a third family word.
  **`kind` is the discriminator.**
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
5. `mips == 1 + log2(min(gap[0], gap[1]))`, so **no shipped mip bleeds across a
   frame border**: a reader sampling on a frame's own UV border reaches half a
   texel into the neighbour, and the padding at the deepest shipped level is
   still at least one whole texel on both axes. On a set with no `pad` key the
   invariant does not hold -- those sheets shipped one bleeding level whenever
   their frame was 96 texels or more (measured 26/255 of a neighbour's alpha
   across 16 of 28 borders at mip 4 of a 128x128 frame).
6. `depthSpan > 0` on any set with a usable height channel.
7. Texture paths, where non-empty, are Data-relative game paths with
   backslashes.

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
| `src/lodgen.cpp` | `63f9971cf5cbb438` | 8,394 |
| `src/nifskope_ui.cpp` | `c0fc470f94d7ec16` | 31,133 |

| claim | line | anchor |
|---|---|---|
| magic, envelope version, 4 MiB cap | `lodmfile.cpp:10` | `static const qsizetype LODM_PAYLOAD_CAP` |
| 12-byte envelope, exact payload size | `lodmfile.cpp:16-38` | `bytes.size() < 12`, `declared != bytes.size() - 12` |
| `lodm != 1` refusal | `lodmfile.cpp:47` | `payload is not a lodm 1 object` |
| family hard refusal | `lodmfile.cpp:52-54` | `family must be legacy or pbr` |
| `kind` defaults to `source` | `lodmfile.cpp:56` | `.toString( QStringLiteral( "source" ) )` |
| `emissiveScale` defaults to 1 | `lodmfile.cpp:63` | `.toDouble( 1.0 )` |
| family-dependent key and suffix names | `lodmfile.h:113-118` | `lodmColorKey`, `lodmMaskKey`, `lodmColorSuffix` |
| `lodmSourceCandidate` two-branch rule | `lodmfile.cpp:96-118` | `c.prepend( QStringLiteral( "materials\\" ) )` |
| `kind: "card"` key set | `lodgen.cpp:2679-2713` | `root.insert( QStringLiteral( "kind" ), QStringLiteral( "card" ) )` |
| `card.pad`, per axis, in texels a side | `lodgen.cpp:2706` | `oc.insert( QStringLiteral( "pad" ), QJsonArray{ padX, padY } );` |
| `card.gap`, per axis, twice the padding | `lodgen.cpp:2707` | `oc.insert( QStringLiteral( "gap" ), QJsonArray{ gapX, gapY } );` |
| the three vintages read under their own laws | `lodgen.cpp:8207-8216` | `const int mipUnit = gapA.size() == 2 ? qMin( gapX, gapY ) : qMin( padX, padY );` |
| `card.center` is the bake's own look-at point | `lodgen.cpp:2709` | `oc.insert( QStringLiteral( "center" ), QJsonArray{ double( card.octCenter[0] )` |
| that point is the scene's bound centre in MODEL space | `nifskope_ui.cpp:22442` | `<< bs.center[0] << " " << bs.center[1] << " " << bs.center[2] << " " << depthSpan` |
| the renderer recomputes a shape's bound FROM VERTICES | `gl/glmesh.cpp:732` | `boundSphere = BoundSphere( verts );` |
| `mips` is derived from the gap | `lodgen.cpp:2624-2625` | `for ( int g = mipUnit; g >= 2; g /= 2 )` |
| card `auxDiv` written only above 1 | `lodgen.cpp:2697` | `oc.insert( QStringLiteral( "auxDiv" ), auxDiv );` |
| `kind: "array"` key set, no aux keys | `lodgen.cpp:4361-4380` | `root.insert( QStringLiteral( "kind" ), QStringLiteral( "array" ) )` |
| `kind: "cardArray"` key set incl. `pad` and `gap` | `lodgen.cpp:8317-8362` | `root.insert( QStringLiteral( "kind" ), QStringLiteral( "cardArray" ) )` |
| `array.gap`, shared | `lodgen.cpp:8328` | `arr.insert( QStringLiteral( "gap" ), QJsonArray{ g.gapX, g.gapY } );` |
| `kind: "terrainVT"` payload | `lodgen.cpp:6842` | `QStringLiteral( "terrainVT" )` |
| the `u = i/(N−1)·2 − 1` mapping | `nifskope_ui.cpp:22065-22067` | `auto viewDir = [octN]` |
