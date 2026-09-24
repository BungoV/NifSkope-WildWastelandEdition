# FO4CS handoff — the LOD generator's file family

Written 2026-09-09 by lane CONTRACTS, brought current **2026-09-10 by lane
DOCS2**, for the FO4CS **Improved LOD** module (bungo's ruling, 2026-09-09
15:34, verbatim: *"This will be a new module for Fo4cs, called Improved LOD"*).

**This page is the single entry point.** Everything below describes files this
generator writes. The generator repo (`E:\Projects\NifskopeWildWastelandEdition`)
is **read-only to FO4CS**; the contract documents under `docs/` are the
interface, and every byte in them is traced to a writer line in that document's
own provenance footer. Those footers were re-derived from their anchor text on
2026-09-10 (`ww-contract-provenance` step 3), so a line number on any page below
points at the tree as it stands today.

**bungo's FINAL naming ruling, 2026-09-09 ~16:4x**, which supersedes the
~16:1x *"lodg sounds better"*. THE WHOLE FAMILY:

| extension | holds | was called |
|---|---|---|
| `.lodl` | **land** — heights, AO, LTEX blend, colour, water, ground cover, overview | `.lodt` |
| `.lodt` | **terrain textures** — the sheets, one file per pyramid level | `.lodv` |
| `.lodo` | **objects** — the FO4CS-native geometry library | (was to be `.lodg`) |
| `.lodi` | **instances** — the placement tables | `.lodi` |
| `.lodm` | **materials** — the LOD material sidecar | `.lodm` |

`.lodv` and `.lodg` are retired. **`.lodt` is REPURPOSED**, which is why the
texture container took its OWN new magic, `LDTX`: the landscape file keeps the
magic `LODT` (its bytes did not change), and **each reader refuses the other's
file BY NAME** — a yesterday's `.lodt` opened as a terrain texture is told it is
the landscape file and to open it as `.lodl`, and vice versa. **`.bto`/`.btr`
stay the stock bake** the engine reads.

## The two changes FO4CS must make up front

Nothing else in the shipped loader has to move — not the magic, not one byte of
version-1 layout.

1. **The terrain loader's extension: `<WS>.lodt` → `<WS>.lodl`.** The file that
   was called `.lodt` yesterday is called `.lodl` today and its bytes are
   unchanged. bungo's five installed files were renamed on 2026-09-09 17:27.
2. **Accept version 3.** FO4CS's parser pins `kVersion = 1u`
   (`docs/LODGEN_BTD_FORMAT.md`, §"Header"); this tree writes **2 by default and
   3 when the water module is on**. A file this tree writes today is *refused*,
   not misread. The version-2 addition is eight bytes (default water height +
   WATR form at 0x98/0x9C); version 3 appends whole sections, all of them behind
   section-flag bits, so a reader that accepts the version and ignores unknown
   bits keeps working. The writer's default is
   `LodtOptions::headerVersion = 2` (`src/lodtfile.h:159`), raised to 3 only when
   `--water-bodies` is given. **Zero-effort fallback, no rebuild on our side:**
   `WW_LODL_VERSION=1`.

---

## 1. The file family, in one table

| file | purpose | status | reader in FO4CS? | contract |
|---|---|---|---|---|
| `<WS>.lodl` | whole-worldspace landscape: heights, LTEX blend alphas, water height + type, terrain colour, ground cover, a coarse AO plane, behind a progressive zlib pyramid | **SHIPPED**, v1, v2 and v3 | **YES** — `src/FarField/FarFieldLodtFormat.h` + `FarFieldLodtSource.h` (lane LODT1, wave 71). **v1 only** | `docs/LODGEN_BTD_FORMAT.md` |
| `<WS>.lodl` version 3 sections | water bodies: body table, body-ID plane, flow plane, shore-distance plane, stroke store, and the dye plane | **BUILT AND GATED** (`lodl_water.sh` 56/0) for the first four; the **dye plane is BUILD PENDING** (written, syntax-checked, never compiled) | no | `docs/LODGEN_BTD_FORMAT.md` §"Water bodies (version 3)" |
| `<WS>.water.json` | the curves, pins, dye pins and per-body overrides as an editable text file beside the land file; loading it onto a re-baked `.lodl` re-derives the planes | **BUILD PENDING** (lane WATER5) | no | `scratchpad/lane_water5_report.md`; no contract page yet |
| `<WS>.VT.<dim>.lodt` | one level of the terrain virtual texture: bordered tiles, four sheets (colour, model-space normal, data, height) | **SHIPPED but GATED OFF** (`--vt`); never run whole-worldspace | no | `docs/LODGEN_TERRAIN_VT.md` |
| `<WS>.VT.lodm` | the pyramid index, `kind: "terrainVT"` | **SHIPPED**, written with the pyramid | no | `docs/LODGEN_TERRAIN_VT.md` §4 + `docs/LODGEN_LODM_FORMAT.md` §5 |
| `*.lodm` (`source`, `card`, `array`, `cardArray`) | the LOD material sidecar: family, four textures, card geometry, per-layer lists | **SHIPPED** | no | `docs/LODGEN_LODM_FORMAT.md` |
| `<chunk>.bto.manifest.txt` | per-object constants beside every `.BTO`: identity, class, bound radius, `(ref, part)`, card placements, array layers | **SHIPPED**, version 2 | no | `docs/LODGEN_MANIFEST_FORMAT.md` |
| `<id>_oct_*.DDS` + `<ws>.LodgenCards.*` | octahedral impostor card sheets, per base and packed into arrays; **orthographic**, gap-padded, `coverage 16 128 160` | **SHIPPED**, never flown | no | `docs/LODGEN_CARD_SHEETS.md` |
| `<ws>.LodgenArrays*` | mesh LOD texture arrays, one per size class and family; sidecar version 5 | **SHIPPED**, never flown | no | `docs/LODGEN_TEXTURE_ARRAYS.md` |
| `<ws>.LodgenObjects*` | the atlas sheets (stock-engine path; **dropped on the native target**) | **SHIPPED** | no | `docs/LODGEN_TEXTURE_ARRAYS.md` §6 |
| `.bto` / `.btr` vertex channels | identity, AO, sway, sky visibility, array layer, ground contact, geomorph | **SHIPPED** | partly — the `.bto` channel census exists (`FarFieldLodBtoChannels.h`) | `docs/LODGEN_VERTEX_PACKING.md` |
| `<WS>.lodo` | the FO4CS-native **geometry library**: base / mesh / cluster / material tables, index and vertex blobs | **BUILT AND HOOKED UP** 2026-09-10 (lane BUILD6): `--native <dir>` on a `--terrain-region` bake writes the pair; synthetic gate 46/46, 20 mutations refused; never run in a game | no | `docs/LODGEN_NATIVE_LODO_LODI.md` (**AS BUILT**, two deviations) |
| `<WS>.lodi` | the FO4CS-native **instance tables**: chunk table, cell ranges, 24-byte instance records, cold records | **same** — `src/lodifile.{h,cpp}`, emitter `src/nativeemit.{h,cpp}`, decoder `tests/spells/lodgen_native_decode.py` | no | `docs/LODGEN_NATIVE_LODO_LODI.md` |
| `<WS>.HeightMap.*.dds`, `_fine`, `WaterMap` | the far shadow heightmaps, with F4FX provenance | **SHIPPED** | **YES** — the existing heightmap loader | `docs/F4FX_PROVENANCE.md`, skill `fo4cs-heightmap-bake` |

---

## 2. Read order

1. **`docs/LODGEN_VERTEX_PACKING.md`** — what a `.bto`/`.btr` vertex holds, and
   **the descriptor table at the end**. Read this before anything that touches a
   chunk, because three of its descriptor constants were wrong until 2026-09-09
   and two of them are still wrong in the source comments.
2. **`docs/LODGEN_MANIFEST_FORMAT.md`** — the identity key. Nothing else in the
   family makes sense until you know that the stable key is `(ref, part)` and
   never `index` or `ref` alone.
3. **`docs/LODGEN_LODM_FORMAT.md`** — the material sidecar. Every texture set in
   the family is described by one of these.
4. **`docs/LODGEN_CARD_SHEETS.md`** and **`docs/LODGEN_TEXTURE_ARRAYS.md`** —
   the two texture shapes, in that order (cards are the harder one and the
   arrays reuse its container rules).
5. **`docs/LODGEN_BTD_FORMAT.md`** — terrain and water, if you are touching the
   far field's heights, its AO, or anything wet. **Row 0 is SOUTH here.**
6. **`docs/LODGEN_TERRAIN_VT.md`** — the terrain pyramid, if you are building a
   streamer. **Row order is NORTH-UP here.** The two conventions are both live
   and the mismatch has already cost one consumer a Y mirror.
7. **`docs/LODGEN_NATIVE_LODO_LODI.md`** — the native far field. **AS BUILT**,
   with two deviations from the spec named on the page.
8. `docs/LODGEN_IMPOSTOR_SPEC.md` is the **design record** behind 3–5: the
   rationale and the measurements, not the contract.
9. `WRITER_CHANGES_NEEDED.md`, beside this file, is the short list of things
   that are wrong in OUR source comments and would mislead a reader.

---

## 3. The reader checklists, as landed today

### 3.1 The land file and its water (`.lodl` v1 → v3)

The v1/v2 path is unchanged: per-cell water height and type, one tint per WATR
form. Everything below is version 3, is written only under `--water-bodies`, and
**every part of it is behind a section-flag bit**, so a reader tests the bit and
falls back rather than assuming.

1. **`sectionFlags & (1 << 4)`?** If not, take the version-2 path and stop.
2. **Body-ID plane: sample NEAREST, never filtered, and never mipped.** An id is
   a name; the average of two names is a third body that does not exist. ID 0 =
   no water here. This plane is the MASK for everything else.
3. **Body table lookup.** Record `i` is body id `i + 1`; ids are assigned by
   descending area, so id 1 is the largest body. The record is 48 bytes and
   `bodyRecordBytes` is a field: a reader with a SHORTER record strides by the
   file's value and reads the prefix it knows; a reader with a LONGER one
   refuses by name.
4. **Tint** comes from the body, not the cell: `colour override A != 0` wins,
   otherwise resolve the body's `WATR form` through the engine's own loaded
   form. The form is the fallback; the override is the answer.
5. **Fog and underwater** also come from the body's form, so two lakes with
   different forms fog differently at the same height.
6. **Depth = `body.waterHeight - terrainHeight(gx, gy)`** — both sides are in
   this file, and nothing is baked for it.
7. **Flow plane.** One uint16 a sample: direction in bits 0..7 (0..2π from +X
   toward +Y in row-0-SOUTH space), speed in 8..11 (× `|body.meanFlow| / 8`),
   confidence in 12..15. **Filter it only INSIDE the body mask** — a bilinear
   tap that reaches a dry sample or a neighbouring body pulls a direction from
   water that is not this water; sample the body-ID plane first and drop any tap
   whose id differs, or fall back to nearest. **No mips.** A dry sample and "no
   flow" are the same value (0) on purpose, so a consumer that forgets the mask
   draws still water rather than garbage. Where confidence is 0, cross-fade to
   `body.meanFlow`; where the plane is absent, use `body.meanFlow`; where the
   table is absent, use the form's `NAM0`. Three named floors.
8. **Dye plane** (section bit 8, offset in the reserved word at `0xF4`, four
   bytes a sample, at the FLOW plane's rate). Bits 0..15 name the SOURCE — a
   body id means "that body's water carried past its mouth", so the consumer
   takes THAT body's colour (its override if A > 0, else its form's); `0x8000|n`
   names the n-th enabled dye pin in the stroke store, which carries its own
   RGBA; 0 = no dye. Bits 16..23 are the weight, 255 = undiluted. **Blend the
   dye colour over the body's own tint by that weight**, and treat an absent
   plane as weight 0 everywhere. **BUILD PENDING: never compiled, never run.**
9. **Shore-distance plane.** uint8, 32 world units a step, saturating at 255 =
   8,160 units, measured to the nearest sample not in the SAME body (dry land,
   or another body across a seam). Absent → skip foam, never synthesise it.
   **This plane is also the winter path: freeze from the shore inward**, so ice
   grows where shore distance is small and open water stays where it is large.
   That is a consumer-side rule; nothing about ice is in the file.
10. Row 0 is SOUTH in all four planes, like everything else in this file.

### 3.2 Impostor cards

* **Orthographic.** The bake asserts the projection and writes the word it read
  back off the live viewport onto the sidecar (`projection ortho|persp`) and
  into the `.lodm`. **Absent = a bake from before 2026-09-10, i.e. perspective**,
  and a perspective sheet cannot be placed by these extents.
* **THE COVERAGE CONTRACT, `coverage 16 128 160`.** Every extent the bake writes
  — `half`, and every per-frame offset — is measured at coverage floor 16/255,
  and the base-colour alpha is re-encoded so that a consumer's own alpha test at
  **128** selects exactly that set: `a' = 160 + (coverage − 16)·95/239` above the
  floor, 0 below it. Measured 0 disagreeing texels over three sheets. The base is
  160, not 128, because BC3's alpha ramp can move a texel by up to 18.2.
  **A set with no `coverage` line or key is older and must be tested at 16/255**;
  reading such a set at 0.5 is the defect this fixed (up to 5.41 texels of
  half-width, TreeHero01 handing over 9.1 % narrower than its own mesh).
* **The gap, not the margin.** `gap(side) = max(2, side/16)`, rounded to even,
  is the distance between two neighbouring silhouettes; the margin per side is
  `gap/2`; the inner rect is `frame − gap` per axis. bungo's "8 pixels on
  1024x1024" is that gap.
* **`mips = log2(gap)`, not `1 + log2(gap)`.** One level shallower, which is
  what "8 = 3 clean mips" meant: at `1 + log2(gap)` the coarsest level has half a
  texel of margin and a border tap reads the neighbouring frame (13 of 19 sheets,
  worst 64/255). At `log2(gap)`, bleed is 0 on every border of every shipped mip,
  and the zero-padding control fails 19/19.
* **Per-frame positioning.** The frame is sized from the WIDEST SINGLE view, not
  the union of all of them, and each view is then cropped around ITS OWN centre;
  the offset that took is written per frame (`frameoff i j …`, up-positive) and
  carried into the `.lodm` as `frameOffset`, one pair per frame in sheet order,
  with a `cardArray` layer carrying its own. **A consumer that ignores
  `frameOffset` will draw every frame off-centre by up to the fit gain.**
* **`card.center` is the pivot → card-centre offset**, so a card lands where the
  3-D model stood; `half` is the half-extent pair. Both are gated against the
  model's own bound spheres (6.4×/12.3×/19.7× discrimination; a zeroed-offset
  control fails).
* `_fs.DDS` is BC3/DXT5 with real alpha; the emissive sheet is BC1, RGB only.

### 3.3 The native pair (`.lodo` / `.lodi`), as built

The native spec's D3D11 section rewritten as the sequence a consumer performs is
**`docs/LODGEN_NATIVE_LODO_LODI.md` §7**, in full. Its ten steps in one line
each:

0. **Spike the graphics state first** — the deferred prepass's RTVs, the
   depth-stencil state and stencil ref, the viewport, the rasteriser state, and
   **whether depth is reversed-Z**. That last decides
   `SV_DepthGreaterEqual` vs `SV_DepthLessEqual`, and the card pixel shader must
   use one of them, never plain `SV_Depth`.
1. Open and validate both files; hard refusals refuse, soft refusals load and
   raise `stale=1`.
2. Upload two StructuredBuffers plus append and indirect-args buffers, sized
   from `maxInstancesPerChunk` and `maxClustersPerMesh`.
3. Derive `chunkIndex[instanceCount]` by walking the chunk table — it is not in
   the file.
4. One `Dispatch` culls everything: frustum on `base.boundRadius × scale`, chunk
   boxes expanded by `maxBoundRadius`, the ladder stepped from `crossPx16`
   against the **live** projection.
5. `DrawInstancedIndirect` per bucket at `DeferredPrePass_Post`. **No IA, no
   input layout, no index buffer.** A `0xFF` local index emits a zero-area
   triangle.
6. Three size classes — `vertexCountPerInstance` 12 / 24 / 48 from the cluster's
   `flags` bits 0–1. A flat 48 is 2.66× the vertex shading.
7. Bucket key = (size class × family × arrayClass × arraySet × alpha state), and
   (family × card class × card set) for cards.
8. The far shadow map is a **second complete consumer**: same cull, same buckets,
   **once per slice**, with our own depth-only VS+PS. Quote its draw count beside
   the main view's or the comparison is not one.
9. Order when suppressing the engine: validate → build buffers → **read back a
   non-zero drawn-primitive count** → only then hide `spLODObjectRoot`.
10. Replace the `.bto` channel census the same wave, with a row that accuses its
    own plumbing: `FarField: native drawn=… culled=… overflow=… | not observing:
    no .bto in scene`.

**Two deviations from the spec that a reader must know** (the page is AS BUILT,
not as designed): the mesh row is **56 bytes**, not 48 — a model-path offset was
added; and the instance `rotation` is the **DRAWN** rotation (the ESM rotation
composed with the tree yaw), with `seed` = the low byte of the tree hash, because
the seed cannot carry the yaw. A consumer that re-derives the yaw from the seed
will double it.

### 3.4 The terrain pyramid (`.lodt`, magic `LDTX`)

* One file per level, 256-byte header, version 1, payload 4096-aligned, tiles
  bordered, four sheets per tile (colour, model-space normal, `_data`, height).
  **Row order is NORTH-UP here** and a clear `ROW_ORDER_NORTH_UP` flag is a
  refusal, not a hint — this is the opposite of the `.lodl`'s row-0-SOUTH.
* **Every tile carries its own CRC-32** and it is checked at LOAD, not at open;
  a tile offset below `payloadOffset` or not 4096-aligned is refused by name.
* The pyramid INDEX is the `<WS>.VT.lodm`, `kind: "terrainVT"`. It states
  `partial: true` on a region bake with the `extent` it covers — **read that
  rather than assuming a whole worldspace** — plus `alignedToWorldOrigin`,
  `coarseLevelsAreDownsamples`, `aniso`, and per level `worldUnitsPerTile` and
  `unitsPerTexel` (32 units a texel at dim 2, doubling to 512 at dim 32).
* Gated off behind `--vt` and never run whole-worldspace.

### 3.5 The flow PNG, and the one convention that is NOT yet built

bungo's ruling, 2026-09-10 ~05:4x, verbatim: *"why not just align it with a
normal map standard to some extent, red and green channels directx directions"*.
So the flow map exported and imported by the water window uses the **DirectX
normal-map convention**: `R = +X`, `G = +Y toward the image BOTTOM`, both centred
on 128 exactly as FO4's own `_n` maps are; `B` = speed, `A` = confidence; a still
lake is a flat normal map. The body mask rides alongside as a 16-bit PNG.

**As written, the code does the opposite and says so:** `src/watercurves.cpp:932`
refuses a flipped import with *"This tool writes +green = north"*. That lane
launched before the ruling. **The export sign is OWED and must be verified at
hook-up time**, together with the flipped-green refusal (gate W6), which is
currently pinned against the OpenGL sense.

---

## 4. The `.lodl` version ladder, and what is installed

**Version 1** is what FO4CS reads. **Version 2** (2026-09-09) adds eight bytes:
the worldspace default water height at 0x98 and its WATR form at 0x9C.
**Version 3** (2026-09-10, lane WATER2) appends the water sections of §3.1. The
writer defaults to 2 and raises to 3 only under `--water-bodies`, so a file
nobody asked new sections of is byte-identical to what this writer produced
before the module existed.

**The five installed files are VERSION 2 and would be refused today**, read from
the files at byte 0x04:

| file | bytes | version | written |
|---|---:|---|---|
| `Commonwealth.lodl` | 35,953,294 | **2** | 2026-09-09 17:27 |
| `DLC03FarHarbor.lodl` | 9,195,933 | **2** | 2026-09-09 17:27 |
| `NukaWorld.lodl` | 7,182,356 | **2** | 2026-09-09 17:27 |
| `DiamondCity.lodl` | 53,148 | **2** | 2026-09-09 17:27 |
| `NukaWorldAmphitheater.lodl` | 38,303 | **2** | 2026-09-09 17:27 |

`Commonwealth.lodl` also carries the version-2 fields: default water height
**450.0** at 0x98 and WATR form **0x18** at 0x9C. The version-1 twins are the
`*.lodt.bak-20260909` copies beside them (2026-09-05 03:14, 35,953,286 /
9,195,806 / 7,182,348 / 53,220 / 38,104 bytes), which keep the old extension
deliberately so the rollback ladder stays readable. A version-3 Commonwealth was
built and gated in this tree (38,612,038 bytes; water sections 2,658,744) but is
**not installed**.

**Owed:** either FO4CS learns versions 2 and 3, or the generator ships
`headerVersion = 1` until it has. **bungo's call.**

**The pixel-level gap that was owed is CLOSED, and by a different cause than
the one that was reported.** FO4CS reconstructed all five installed `.lodl`
files through its own parser and compared against this generator's own
heightmaps: Commonwealth **0 wrong of 37.7M**, Nuka-World **0 of 4.3M**, Diamond
City **0 of 172k**, **Far Harbor 62 of 20.2M**. FO4CS's lane read that as the
generator writing an extra edge row per cell into the heightmap. It is not: the
62 are **one landless cell**, Far Harbor (14,-6), ringed by eight that have
land. A cell with no `LAND` record was written as flat height ZERO, losing both
the rows its neighbours share with it and the worldspace's default land height.
The same defect cost DiamondCity **167,936 of 172,032** texels (164 landless
cells against a default of -2048) and NukaWorldAmphitheater 97.

**Fixed in `src/lodtfile.cpp` on 2026-09-09** by the lane that added version 2:
a landless cell now inherits its south/west/south-west seam rows under the same
maximum rule and fills the rest with the worldspace default land height, which
deliberately does **not** take part in that maximum (Far Harbor's default is 0
and its inherited row is around -250; a max against the default would have kept
all 62 wrong). Gated by `tests/spells/lodl_write.sh`, which bakes
NukaWorldAmphitheater's `.lodl` **and** its heightmap and requires all 114,688
texels to agree, having first checked that the worldspace HAS landless cells so
the check can fail.

The five installed files were re-baked at 2026-09-09 17:27, which is **after**
that fix; nobody has re-run the heightmap comparison against them, so they are
*unverified* rather than known-wrong.

---

## 5. The sample-file set

**`scratchpad/handoff_fo4cs/samples/`** — 129 files, 170,004,485 bytes
(162.1 MB), written 2026-09-09 by lane IMAGES5 and regenerated by lanes
CARDFINAL and CARDWIDTH as the card rules changed. **`samples/MANIFEST.md` is
the authority**: it lists every file with its size, the exact command that made
it, and the contract document it obeys, and every size in it is read from disk
by `samples/make_manifest.py` rather than typed. `samples/make_samples.sh` is
the run; `samples/make_samples.log` is its whole stdout. **The set is NOT in the
public repo** (162 MB); `MANIFEST.md` and `make_samples.sh` are, so it
regenerates.

**The region** is the one containing cell (0,0) — cells 0..3 × 0..3 — baked at
all four far levels. The sweep bakes every chunk touching that rectangle, which
here is exactly the one chunk that contains it, so the four levels are four
views of the same ground: `Commonwealth.4.0.0` (cells 0..3), `8.0.0` (0..7),
`16.0.0` (0..15), `32.0.0` (0..31).

**The profile**: objects + identity + `--arrays` + `--impostors` + `--cover` +
`--vt`, with the card library baked first, plus `--slot-fallback` at dim 16 and
32, and **no `--atlas`**, because the atlas sheets are the stock engine's
draw-call optimisation and the native target drops them (§1).

### What is on disk

| was missing | now | where |
|---|---|---|
| a version-2 `.lodl` | the five INSTALLED files are version 2 — see §4 | `E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\` |
| a version-3 `.lodl` | built and gated in this tree, **not installed and not sampled** | §4 |
| a `.lodt` at any level | **five**: levels 2, 4, 8, 16, 32 | `samples/vt/Terrain/Commonwealth.VT.<dim>.lodt` |
| a `<WS>.VT.lodm` index | yes, 1,964 bytes, `kind terrainVT` | `samples/vt/Terrain/Commonwealth.VT.lodm` |
| a version-2 manifest | **four**, one per level, `# lodgen manifest 2 ...` | `samples/L<dim>/Commonwealth.<dim>.0.0.BTO.manifest.txt` |
| a `kind:"array"` `.lodm` + `LodgenArrays*` | two size classes (128x128, 256x256) at every level | `samples/L<dim>/textures/terrain/Commonwealth/Objects/` |
| a `kind:"card"` `.lodm` + `<id>_oct_*.DDS` | the 19-tree octahedral library, **orthographic, `coverage 16 128 160`, 38 sidecar entries** | `scratchpad/images_20260909/gen/cards_trees19/` |
| a `kind:"cardArray"` `.lodm` + `LodgenCards.*` | four frame classes (256x256, 256x512, 768x1024, 1024x1024) at L16 and L32; one at L8 | `samples/L<dim>/.../Objects/` |
| `LodgenObjects*` atlas sheets | **still absent, deliberately** | — |
| `.lodo` / `.lodi` | the SYNTHETIC pair `scratchpad/native0_20260910/fixture/Synthetic.{lodo,lodi,expect.txt}`, **plus five real region pairs** built by BUILD6 at d4/d8/d16/d32 (`.lodo` 5,696,484 B, 2,970 bases; `.lodi` 136,992 / 279,624 / 33,064 / 16,392 B). No whole-Commonwealth pair | §6 |

Cards do not substitute at ring 0, so **L4 has no card array** and its run says
so out loud: *"card arrays: no placement in the chunks stands on an octahedral
card"*. L8 has one card set group, L16 and L32 have **18 card sets in 16 arrays
(4 groups)** — 18 of the 19 trees; `000a7206 TreeBlasted01Lichen` baked its
sheets but no placement in the region stood on it, so `--impostors` converted no
DDS for it.

### It was read back, not just written

Every `.lodt` level was validated with `lodgen --lodt-check`, which walks every
rule of `docs/LODGEN_TERRAIN_VT.md` §3.4 and checks **every tile's CRC**. All
five exited 0:

| level | present tiles | cover tiles | stored bytes |
|---|---:|---:|---:|
| 2 | 32 | 30 | 11,744,960 |
| 4 | 8 | 8 | 2,959,360 |
| 8 | 4 | 4 | 1,479,680 |
| 16 | 2 | 2 | 739,840 |
| 32 | 1 | 1 | 369,920 |

`lodgen --lodm-check` was run on one sidecar of each kind — `terrainVT`,
`array`, `cardArray` — all `lodm ok 1`, version 1, family legacy.

**The pyramid is PARTIAL and says so**: the index carries `"partial": true` and
`extent {south 0, west 0, north 3, east 3}` because `--terrain-region` was
given. **The height sheet is present**: `--vt-height` is off by default and was
passed deliberately, because a sample set that omits a sheet cannot be used to
write a reader for it. Every level reports `role 4` at dxgi 56 (`R16_UNORM`).

### The whole-worldspace land file is NOT copied here

36 MB, and it exists already; a second copy would only go stale. Point at
`E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl`
(35,953,294 bytes, header **version 2**, 2026-09-09 17:27).

### Pictures

`scratchpad/images_20260909/handoff_contact_sheet.png` is the index: terrain
chunks at all four far levels vanilla-vs-ours, both object chunk rings, the
identity channel, the octahedral card sheets, a card beside its source model,
and 882 cards standing in a far chunk. `scratchpad/lane_images_handoff_report.md`
carries the command behind each one.

### Where FO4CS's own fixtures are

`E:\Projects\Fo4CommunityShaders\wt-fixfirst\tests\lodt_format_tests.cpp`
(1,442 lines) is the only exercised reader outside this tree. It builds its
fixtures in memory rather than reading a shipped file, which is why the
real-file comparison of §4 had to be done separately — and why this sample set
exists.

---

## 6. Bake time — UNMEASURED for a whole worldspace

**No lane has ever timed a full Commonwealth bake.** The only measured number in
this tree is **8 s wall** for lane 0's baseline region set (chunks (-20,24) d4,
(-24,24) d8, (-32,16) d16 with slot fallback, (-32,0) d32 — the
42,560-placement bucket-cap chunk — and the 2×2-cell region (-20,24)..(-19,25)
at d4 with arrays + atlas, AO on) on the 2026-09-10 03:57:46 exe, and it is a
region set, not a worldspace. The `.lodl` write is **6 s** for the whole
Commonwealth with the water module on (4.7 s of reported phases at version 2).

**bungo is running the first whole-Commonwealth bake in the GUI. His four stage
times go here when he reports them:**

| stage | wall time |
|---|---|
| landscape | *(pending bungo's GUI run)* |
| meshes | *(pending bungo's GUI run)* |
| textures | *(pending bungo's GUI run)* |
| impostors | *(pending bungo's GUI run)* |

Until those four numbers exist, **no claim about bake cost belongs in an FO4CS
plan**, and the 8 s above must never be quoted as a worldspace figure.

---

## 7. Open items

1. **`.lodo` / `.lodi` are built and hooked up, but only region-baked**
   (2026-09-10, lanes NATIVE0b and BUILD6). Writers, readers, the emitter and an
   independent Python decoder exist and are gated on a hand-written synthetic
   pair (46 checks, 0 failures; two writes byte-identical; 20 mutations refused
   by name), and `--native <dir>` on a `--terrain-region` bake now writes real
   pairs that all five readers plus the decoder accept. **RED, measured:** the
   decoder's manifest leg fails at 0.125 on d4/d8/d16, which is the manifest's
   six-digit print rather than the data, and the bar has not been re-pinned. Four
   `WrhsLeanTo` LOD nifs failed to load. Lane 0's baseline
   (`tests/baselines/stock_baseline.sha256`, 25 files, `--check` 0 differ) is a
   REGION SET, not a worldspace.
2. **The `.lodl` version ladder, §4 and §"The two changes".** bungo's call.
3. **The dye plane and the water window are BUILD PENDING** — written,
   syntax-checked, never compiled, never run (lanes WATER4 and WATER5). Two
   WATER4 gates fail as pre-registered and were not moved: island-bank tangency
   12.0/22.4 deg, and p99 flow-angle jump 8.44 vs a 5 deg bar. Owed with them:
   `CHANGE_NEEDED.md` C1–C3 (the solver must consume point weights, one-point
   pins and the raster source layer).
4. **The flow PNG's green sign, §3.5.** The code says +green = north; bungo ruled
   DirectX. Verify at hook-up.
5. **The lattice / detail thread is PARKED** by bungo, 2026-09-09, verbatim:
   *"let's save this for later"*. Owed when it resumes: whether to reuse
   vanilla's `_msn` where terrain is unchanged and/or write the sheet
   uncompressed or BC7; the shape of the material-blend mod; and the pending
   terrain gate. Cause 1 of the square lattice is fixed in `heightAt` (grid
   roughness 0.947 → 0.209 before the codec, vanilla 0.296) but **cause 2
   dominates the picture**: BC1 flattens 16–29% of our 4×4 blocks against
   vanilla's 0.1–3.3%, because our sheet carries ~10 code steps of
   high-frequency signal against vanilla's 65.
6. **The terrain seam ruling is implemented but one corner is still red**
   (bungo, 2026-09-10 ~02:5x, verbatim: *"The cell owns it then"*). The ring
   fills only the texels beyond the chunk; the edge band goes to 0 beyond 4 on
   all four borders. What remains: V9b `_msn` differs on tile -20.28 only, 32
   texels at the -19|-18 cell corner on the region's OUTER edge, where there is
   no neighbour for the ring; and `_data` is 16 beyond 64 there — the wetness
   defect, left.
7. **The `.bto` channel census will lie the moment `.bto` stops being written**
   (§3.3 step 10). It reports "vanilla" forever with no `.bto` in the scene.
8. **Two writer comments are wrong and one column is misnamed** —
   `WRITER_CHANGES_NEEDED.md`.
9. **Nothing in this family has ever run in a game** except the `.lodl`
   heightmap source, and its two arms (`bTerrainAoFromLodt`,
   `bLodObjectAoFromBto`) ship at 0 and change no picture by design.

---

## 8. The state of the tree

**The tree is committed and pushed.** bungo lifted his standing *"Not yet"* on
2026-09-09 evening; `origin/main` moved to `720762a`, 26 commits (eight from
that day plus eighteen older ones nobody had pushed). Everything this package
points at is on `origin/main` except the 2026-09-10 work — the water lanes, the
card lanes, the native pair and this page — which is landed but **uncommitted**
in the working tree.

**So an FO4CS engineer reading these files is reading a working tree, not a
tag.** Every contract page therefore carries the **sha256 prefix and line count**
of the sources it was verified against, and quotes anchor text beside every line
number; those numbers were last re-derived from their anchors on **2026-09-10**
by lane DOCS2 (`scratchpad/docs2_20260910/anchors.py`, 243 rows checked, 111
moved, 0 anchors missing). If a page's stamp does not match the source you are
holding, re-run that script before believing a line number.
