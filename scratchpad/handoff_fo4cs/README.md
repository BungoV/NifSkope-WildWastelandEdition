# FO4CS handoff — the LOD generator's file family

Written 2026-09-09 by lane CONTRACTS, for the FO4CS **Improved LOD** module
(bungo's ruling, 2026-09-09 15:34, verbatim: *"This will be a new module for
Fo4cs, called Improved LOD"*).

**Everything here describes files this generator writes.** The generator repo
(`E:\Projects\NifskopeWildWastelandEdition`) is **read-only to FO4CS**; the
contract documents under `docs/` are the interface, and every byte in them is
traced to a writer line in that document's own provenance footer.

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

**What an FO4CS reader must change:** the terrain loader's extension, from
`<WS>.lodt` to `<WS>.lodl`. Nothing else — not the magic, not the version, not
one byte of layout.

---

## 1. The file family, in one table

| file | purpose | status | reader in FO4CS? | contract |
|---|---|---|---|---|
| `<WS>.lodl` | whole-worldspace landscape: heights, LTEX blend alphas, water height + type, terrain colour, ground cover, a coarse AO plane, behind a progressive zlib pyramid | **SHIPPED**, v1 and v2 | **YES** — `src/FarField/FarFieldLodtFormat.h` + `FarFieldLodtSource.h` (lane LODT1, wave 71). **v1 only** | `docs/LODGEN_BTD_FORMAT.md` |
| `<WS>.VT.<dim>.lodt` | one level of the terrain virtual texture: bordered tiles, four sheets (colour, model-space normal, data, height) | **SHIPPED but GATED OFF** (`--vt`); never run whole-worldspace | no | `docs/LODGEN_TERRAIN_VT.md` |
| `<WS>.VT.lodm` | the pyramid index, `kind: "terrainVT"` | **SHIPPED**, written with the pyramid | no | `docs/LODGEN_TERRAIN_VT.md` §4 + `docs/LODGEN_LODM_FORMAT.md` §5 |
| `*.lodm` (`source`, `card`, `array`, `cardArray`) | the LOD material sidecar: family, four textures, card geometry, per-layer lists | **SHIPPED** | no | `docs/LODGEN_LODM_FORMAT.md` |
| `<chunk>.bto.manifest.txt` | per-object constants beside every `.BTO`: identity, class, bound radius, `(ref, part)`, card placements, array layers | **SHIPPED**, version 2 | no | `docs/LODGEN_MANIFEST_FORMAT.md` |
| `<id>_oct_*.DDS` + `<ws>.LodgenCards.*` | octahedral impostor card sheets, per base and packed into arrays | **SHIPPED**, never flown | no | `docs/LODGEN_CARD_SHEETS.md` |
| `<ws>.LodgenArrays*` | mesh LOD texture arrays, one per size class and family; sidecar version 5 | **SHIPPED**, never flown | no | `docs/LODGEN_TEXTURE_ARRAYS.md` |
| `<ws>.LodgenObjects*` | the atlas sheets (stock-engine path; **dropped on the native target**) | **SHIPPED** | no | `docs/LODGEN_TEXTURE_ARRAYS.md` §6 |
| `.bto` / `.btr` vertex channels | identity, AO, sway, sky visibility, array layer, ground contact, geomorph | **SHIPPED** | partly — the `.bto` channel census exists (`FarFieldLodBtoChannels.h`) | `docs/LODGEN_VERTEX_PACKING.md` |
| `<WS>.lodo` | the FO4CS-native **geometry library**: base / mesh / cluster / material tables, index and vertex blobs | **SPEC ONLY, NOT WRITTEN** | no | `docs/LODGEN_NATIVE_LODO_LODI.md` |
| `<WS>.lodi` | the FO4CS-native **instance tables**: chunk table, cell ranges, 24-byte instance records, cold records | **SPEC ONLY, NOT WRITTEN** | no | `docs/LODGEN_NATIVE_LODO_LODI.md` |
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
5. **`docs/LODGEN_BTD_FORMAT.md`** — terrain, if you are touching the far field's
   heights or its AO. **Row 0 is SOUTH here.**
6. **`docs/LODGEN_TERRAIN_VT.md`** — the terrain pyramid, if you are building a
   streamer. **Row order is NORTH-UP here.** The two conventions are both live
   and the mismatch has already cost one consumer a Y mirror.
7. **`docs/LODGEN_NATIVE_LODO_LODI.md`** — the native far field. Spec only.
8. `docs/LODGEN_IMPOSTOR_SPEC.md` is the **design record** behind 3–5: the
   rationale and the measurements, not the contract.

---

## 3. The draw, as a reader's checklist

The native spec's D3D11 section (§8.3) rewritten as the sequence a consumer
performs is **`docs/LODGEN_NATIVE_LODO_LODI.md` §7**, in full. Its ten steps in
one line each:

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

---

## 4. The one live incompatibility

**`.lodl` is at version 2 in this tree and FO4CS knows version 1.**

* Another lane added `.lodl` header version 2 on 2026-09-09 (two fields at
  0x98/0x9C: the worldspace default water height and its WATR form). The writer's
  `LodtOptions::headerVersion` **defaults to 2**.
* FO4CS's parser pins `kVersion = 1u` and refuses anything else, so a file
  generated today would be **refused**, not misread.
* **The zero-effort fallback needs no rebuild on our side:**
  `WW_LODL_VERSION=1` on the generator, or `headerVersion = 1` in the options,
  writes the exact bytes it wrote before version 2.
* **CORRECTED 2026-09-09 20:1x (lane IMAGES5): the five installed `.lodl`
  files are VERSION 2 and would be refused.** This page said they were
  version 1 and unaffected, which was true when it was written and stopped
  being true at 17:27 the same day, when lane BUILD1 re-baked and renamed
  them. Read from the files, byte 0x04:

  | file | bytes | version | written |
  |---|---:|---|---|
  | `Commonwealth.lodl` | 35,953,294 | **2** | 2026-09-09 17:27 |
  | `DLC03FarHarbor.lodl` | 9,195,933 | **2** | 2026-09-09 17:27 |
  | `NukaWorld.lodl` | 7,182,356 | **2** | 2026-09-09 17:27 |
  | `DiamondCity.lodl` | 53,148 | **2** | 2026-09-09 17:27 |
  | `NukaWorldAmphitheater.lodl` | 38,303 | **2** | 2026-09-09 17:27 |

  `Commonwealth.lodl` also carries the version-2 fields: default water
  height **450.0** at 0x98 and WATR form **0x18** at 0x9C. The version-1
  twins are the `*.lodt.bak-20260909` copies beside them (2026-09-05
  03:14, 35,953,286 / 9,195,806 / 7,182,348 / 53,220 / 38,104 bytes), which
  keep the old extension deliberately so the rollback ladder stays
  readable.
* **Owed:** either FO4CS learns version 2 (eight bytes, `ver >= 2`, plus the
  `hasDefaultWater()` / `defaultWaterHeight()` / `defaultWaterType()` accessors),
  or the generator ships `headerVersion = 1` until it has. **bungo's call.**

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

**The five `.lodl` files installed in bungo's mod folder predate that fix**
(written 2026-09-05). Far Harbor, Diamond City and NukaWorldAmphitheater are
still wrong on disk; they need a re-bake.

---

## 5. The sample-file set -- IT EXISTS NOW

**`scratchpad/handoff_fo4cs/samples/`** -- 129 files, 170,004,485 bytes
(162.1 MB), written 2026-09-09 by lane IMAGES5 on `release/NifSkope.exe`
19:35:14. **`samples/MANIFEST.md` is the authority**: it lists every file with
its size, the exact command that made it, and the contract document it obeys,
and every size in it is read from disk by `samples/make_manifest.py` rather than
typed. `samples/make_samples.sh` is the run; `samples/make_samples.log` is its
whole stdout.

**The region** is the one containing cell (0,0) -- cells 0..3 x 0..3 -- baked at
all four far levels. The sweep bakes every chunk touching that rectangle, which
here is exactly the one chunk that contains it, so the four levels are four
views of the same ground: `Commonwealth.4.0.0` (cells 0..3), `8.0.0` (0..7),
`16.0.0` (0..15), `32.0.0` (0..31).

**The profile is this section's own former sentence** -- objects + identity +
`--arrays` + `--impostors` + `--cover` + `--vt`, with the card library baked
first -- plus `--slot-fallback` at dim 16 and 32, and **no `--atlas`**, because
the atlas sheets are the stock engine's draw-call optimisation and the native
target drops them (section 1).

### What is now on disk

| was missing | now | where |
|---|---|---|
| a version-2 `.lodl` | the five INSTALLED files are version 2 -- see the correction in section 4 | `E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\` |
| a `.lodt` at any level | **five**: levels 2, 4, 8, 16, 32 | `samples/vt/Terrain/Commonwealth.VT.<dim>.lodt` |
| a `<WS>.VT.lodm` index | yes, 1,964 bytes, `kind terrainVT` | `samples/vt/Terrain/Commonwealth.VT.lodm` |
| a version-2 manifest | **four**, one per level, `# lodgen manifest 2 ...` | `samples/L<dim>/Commonwealth.<dim>.0.0.BTO.manifest.txt` |
| a `kind:"array"` `.lodm` + `LodgenArrays*` | two size classes (128x128, 256x256) at every level | `samples/L<dim>/textures/terrain/Commonwealth/Objects/` |
| a `kind:"card"` `.lodm` + `<id>_oct_*.DDS` | the 19-tree octahedral library | `scratchpad/images_20260909/gen/cards_trees19/` |
| a `kind:"cardArray"` `.lodm` + `LodgenCards.*` | four frame classes (256x256, 256x512, 768x1024, 1024x1024) at L16 and L32; one at L8 | `samples/L<dim>/.../Objects/` |
| `LodgenObjects*` atlas sheets | **still absent, deliberately** | -- |
| `.lodo` / `.lodi` | **still absent -- there is no writer** | section 6 |

Cards do not substitute at ring 0, so **L4 has no card array** and its run says
so out loud: *"card arrays: no placement in the chunks stands on an octahedral
card"*. L8 has one card set group, L16 and L32 have **18 card sets in 16 arrays
(4 groups)** -- 18 of the 19 trees; `000a7206 TreeBlasted01Lichen` baked its
sheets but no placement in the region stood on it, so `--impostors` converted no
DDS for it.

### It was read back, not just written

Every `.lodt` level was validated with `lodgen --lodt-check`, which walks every
rule of `docs/LODGEN_TERRAIN_VT.md` section 3.4 and checks **every tile's CRC**.
All five exited 0:

| level | present tiles | cover tiles | stored bytes |
|---|---:|---:|---:|
| 2 | 32 | 30 | 11,744,960 |
| 4 | 8 | 8 | 2,959,360 |
| 8 | 4 | 4 | 1,479,680 |
| 16 | 2 | 2 | 739,840 |
| 32 | 1 | 1 | 369,920 |

`lodgen --lodm-check` was run on one sidecar of each kind -- `terrainVT`,
`array`, `cardArray` -- all `lodm ok 1`, version 1, family legacy.

**The pyramid is PARTIAL and says so**: the index carries `"partial": true` and
`extent {south 0, west 0, north 3, east 3}` because `--terrain-region` was
given. A consumer must read that rather than assume a whole worldspace. The
index also states `worldUnitsPerTile` and `unitsPerTexel` per level (32 units a
texel at dim 2, doubling each level to 512 at dim 32) and
`alignedToWorldOrigin: true`.

**The height sheet is present.** `--vt-height` is off by default and was passed
deliberately: a sample set that omits a sheet cannot be used to write a reader
for it. Every level reports `role 4` at dxgi 56 (`R16_UNORM`).

### The whole-worldspace land file is NOT copied here

36 MB, and it exists already; a second copy would only go stale. Point at
`E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl`
(35,953,294 bytes, header **version 2**, 2026-09-09 17:27).

### Pictures of all of this

`scratchpad/images_20260909/handoff_contact_sheet.png` is the index: terrain
chunks at all four far levels vanilla-vs-ours, both object chunk rings, the
identity channel, the octahedral card sheets, a card beside its source model,
and 882 cards standing in a far chunk. `scratchpad/lane_images_handoff_report.md`
carries the command behind each one.

### Where FO4CS's own fixtures are

`E:\Projects\Fo4CommunityShaders\wt-fixfirst\tests\lodt_format_tests.cpp`
(1,442 lines) is the only exercised reader outside this tree. It builds its
fixtures in memory rather than reading a shipped file, which is why the
real-file comparison of section 4 had to be done separately -- and why this
sample set exists.

---

## 6. Open items

1. **`.lodo` / `.lodi` are unwritten.** No writer, no reader, no file. The spec
   is `scratchpad/specs_20260906/spec_fo4cs_native.md` and the contract page
   `docs/LODGEN_NATIVE_LODO_LODI.md` carries a **SPEC / NOT YET WRITTEN** banner.
   Nothing on that page has been checked against a writer, because there is no
   writer. The spec's own provenance tags (`[par]`, `[pri]`, `[cei]`, `[nat]`,
   `[arith]`, `[con]`, `[inv]`) must be consulted before quoting any of its
   numbers as measured.
2. **The `.lodl` v1/v2 split, §4.** bungo's call.
3. ~~The Far Harbor 62 pixels.~~ **Closed 2026-09-09**, §4 — and the cause was
   landless cells, not the cell edge. This page said the installed files still
   carried it; **that is now unverified rather than true** — all five were
   re-baked at 2026-09-09 17:27 (§4), which is after the fix landed, so they
   probably do not. Nobody has re-run the heightmap comparison against them.
   Lane IMAGES5, 2026-09-09, stating what it measured (the mtimes and the
   versions) and not what it did not.
4. **The lattice / detail thread is PARKED** by bungo, 2026-09-09, verbatim:
   *"let's save this for later"*. Owed when it resumes: whether to reuse
   vanilla's `_msn` where terrain is unchanged and/or write the sheet
   uncompressed or BC7; the shape of the material-blend mod; and the pending
   terrain gate. Cause 1 of the square lattice is fixed in `heightAt` (grid
   roughness 0.947 → 0.209 before the codec, vanilla 0.296) but **cause 2
   dominates the picture**: BC1 flattens 16–29% of our 4×4 blocks against
   vanilla's 0.1–3.3%, because our sheet carries ~10 code steps of
   high-frequency signal against vanilla's 65.
5. **The VT pyramid path `lodgenBakeVtTile` is FIXED** — 2026-09-09, the same
   day. Both 2026-09-07 defects (nearest sampling, up-in-blue) were a *copy* of
   twelve lines from the per-chunk baker; both paths now call
   `lodgenTerrainHeightAt` and `lodgenTerrainMsnPixel`. Grid-phase roughness on
   tile 4.-60.36 went 2.001 → 0.209 (vanilla 0.065) and the mean up channel
   0.288 → 0.841 (vanilla 0.770). **HANDOFF.md still says this is open; it is
   not.** What remains open is that the fix has not been through
   `tests/spells/lodgen_terrain.sh` with the game down.
6. **The `.bto` channel census will lie the moment `.bto` stops being written**
   (§3 step 10). It reports "vanilla" forever with no `.bto` in the scene.
7. **Two writer comments are wrong and one column is misnamed** —
   `WRITER_CHANGES_NEEDED.md`.
8. **Nothing in this family has ever run in a game** except the `.lodl`
   heightmap source, and its two arms (`bTerrainAoFromLodt`,
   `bLodObjectAoFromBto`) ship at 0 and change no picture by design.

---

## 7. The tree is uncommitted

Nothing in `E:\Projects\NifskopeWildWastelandEdition` has been committed since
2026-09-04, on bungo's standing *"Not yet"*. That includes every document this
package points at. **An FO4CS engineer reading these files is reading a working
tree, not a tag**, and two source files moved under this lane while it was
writing (`src/lodgen.cpp` grew by 68 lines, `src/lodtfile.cpp` by 148 and gained
header version 2). Every contract page therefore carries the **sha256 prefix and
line count** of the sources it was verified against, and quotes anchor text
beside every line number.
