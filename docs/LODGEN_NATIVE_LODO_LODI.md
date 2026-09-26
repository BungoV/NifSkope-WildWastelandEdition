# `.lodo` v6 (v4..v7) + `.lodi` v7 (v3..v11) — the FO4CS-native far field (and the v7/v11 near library)

> **NEAR LIBRARY (lane NEAR1, 2026-09-26).** `lodgen --near-library <dir>` writes
> `<ws>.near.lodo` at **version 7** (§3.9: header flag `NEAR` 16 + the material
> row's `features` byte) and `<ws>.near.lodi` on the v7 layout, at **version 11**
> when a placement is Initially Disabled (§4.15: instance flag bit 8), else 9 or 7.
> **No far-field file ever carries either:** a far bake is byte-identical to the
> one before NEAR1, version word included (gate G4, 130 of 130 files). The readers
> accept `.lodo` 4..7 and `.lodi` 3..11.

> **VERSIONS TODAY (re-read 2026-09-23 against `src/lodofile.h`,
> `src/lodifile.h`, `src/nifcli.cpp`; `.lodo` re-read 2026-09-25, lanes SEAM1 and SWAP1).**
> The library is `.lodo` **version 6** (§3.8, material-swap variant base rows);
> a version-5 file is read as a v6 file with no variant rows, and a version-4 file
> as one with no colour stream either (§3.7). A default bake writes `.lodi` **version 7** (§4.9, §4.10);
> `--scrappable` writes **version 9** (§4.12); a placement scaled above 7.99988
> writes **version 10** (§4.14, lane BAKE2 2026-09-25); the ways back (`--lodi-v6`,
> `--native-no-vertex-ao`, `--native-no-placement-ao`) step it down through 6, 5
> and 3/4 (§4.6-§4.8). Version 8 is
> retired and read only (§4.11). The reader accepts `.lodi` 3..10 and refuses 1
> and 2 by name. The default library is **authored-only** (bungo 2026-09-17,
> *"Authored LODs only"*): `--library mnam` and no ladder (§3.5.7). A default
> urban `.lodo` is **6,204,388 B** (2,970 bases, 2,982 meshes, 10,634 clusters,
> `levelMax` 0). The status block below is the 2026-09-11 NATIVE1b bake (`.lodo`
> v3, ladder on) and its numbers are history, not today's default.

> **STATUS: BUILT INTO THE EXE, WITH THE CLUSTER LADDER, THE BOUNDS AND CONES AND
> THE OCCLUDER BOXES (2026-09-11, lane NATIVE1b).**
> The emitter is hooked into `lodgenBuildObjectChunk`, the CLI carries `--native`,
> `--native-verify`, `--native-verify-corpus`, `--native-mesh-report`,
> `--native-fixture` and the two ways back `--native-no-ladder` /
> `--native-no-occluders`. `release/NifSkope.exe` has written the nine-chunk
> Sanctuary region: `Commonwealth.lodo` **9,657,316 B** (2,970 bases, 2,982
> meshes, **20,678 clusters over eight levels**, 252,268 triangles, 419,204
> vertices) and `Commonwealth.lodi` **128,256 B** for **3,526 placements**,
> which is the stock manifests' placement count exactly.
> **What is still not done:** nothing in FO4CS reads either file; there is no
> reconstruction path that turns a pair back into a renderable mesh, so parity
> is shown as coverage numbers, the decoder's own geometry and a point-set
> picture rather than a side-by-side render; the LOD Generation panel has no
> `.lodo`/`.lodi` row (lane LODUI1); and the aggregate ring-3 impostors are lane
> CARDS-AGG.
>
> **THE ONE NUMBER A READER OF THIS PAGE SHOULD CARRY AWAY.** The ladder is
> real, it is a partition, and its first step is **too coarse to be selected
> anywhere in the Commonwealth at a one-pixel tolerance**: the median level-1
> cluster deviates by **3.80 percent of its model's own diagonal**, which at a
> 1,000-unit building is 38 units, which reaches one pixel only past 52,100
> units. The reason is not the ladder — it is that **"full detail" in this file
> is already Bethesda's LOD mesh**, a mean of 47.7 triangles for a whole
> building, with almost nothing left to remove. bungo's ruling of 10:3x asks the
> ladder to serve the NEAR field too; that needs the library built from the
> base's own `MODL`, not from its `MNAM` slots. §3.5.4 states the measurement
> and the choice; it is **his call**, not this lane's.

**bungo's FINAL ruling, 2026-09-09 ~16:4x**, which SUPERSEDES the ~16:1x
*"lodg sounds better"*: the object library is **`.lodo`**, `.lodg` is retired,
and the rest of the family is `.lodl` land, `.lodt` terrain textures, `.lodi`
instances, `.lodm` materials. **`.bto` stays the stock bake** the engine reads;
these two files are the parallel native path, and the engine's own LOD is the
zero-effort fallback beneath them.

**bungo's ruling, 2026-09-09 15:34**, verbatim: *"This will be a new module for
Fo4cs, called Improved LOD"* — the consumer of everything in this folder is ONE
FO4CS module with its own master switch that ships off.

---

## 0. What v3 changed, and why v2 is refused

Three rulings of 2026-09-11 08:0x–08:2x land in the bytes. Every one of them is
a field that is WRITTEN and that MOVES, with a test and a floor (the three rules
of 2026-09-04 21:33); the tests are named beside each.

| # | his words | what it became | where |
|---|---|---|---|
| a | *"1"* — per-cluster screen error instead of four rings: *"continuous detail, no pop at ring borders, triangles spent only where they show"* | a **CLUSTER LADDER** per mesh, from full detail down. A parallel 48-byte `LodoClusterLod` row per cluster carries `geometricError` (against FULL detail, never against the parent), the parent link and the level; the cluster table now holds every level | §3.5 |
| b | *"2"* — cluster bounds and normal cones for GPU culling | the same row carries an exact **bounding sphere** and an **octahedral normal cone**, and `LodoCluster.flags` bit 2 says **CONE_OPEN** when a cluster's normals span more than a hemisphere | §3.6 |
| c | *"2 sounds good"* (second round) — *"a few boxes per cell for buildings and hills, baked from the meshes"* | **occluder boxes**: up to four oriented boxes a cell, each fitted INSIDE a watertight LOD mesh and probed at a hundred points before it is written, in two new `.lodi` tables | §4.5 |

**A v2 file is refused by both readers, by name**, and not merely
version-checked. A v2 `.lodo` has no ladder table at all (header 0xC0 was
reserved), so a v3 reader that accepted it would see `geometricError` 0 and
`parentError` 0 for every cluster and draw the whole library at full detail at
every distance — the exact opposite of what the selection exists for. A v2
`.lodi` has both occluder offsets at zero, which reads as "this worldspace
occludes nothing". Re-bake.

---

## 1. What the two files are, and what they retire

WHERE THEY LAND, 2026-09-16 (lane LAYOUT1). bungo, 19:3x: "The folder should be
called FO4CSLOD maybe, so it'd be Data/FO4CSLOD, sound fine?" -- so the pair, and
every other FO4CS-target output of a bake, sits under one root inside the output
mod folder. `--native` accordingly NAMES A MOD FOLDER on the command line, as
`--vt` and `--lodl` already did: the pair lands at
`<MODFOLDER>/FO4CSLOD/<ws>/<ws>.lodo` + `.lodi`, not in the named directory
itself. It was `<dir>/<ws>.lodo` until that day; every harness that opened it by
path was re-based in the same lane, and `src/lodgenlayout.cpp` is the only place
the folder is spelled.

| file | holds | measured, 9-chunk Sanctuary | v2, the same region |
|---|---|---|---|
| `Data\FO4CSLOD\<WS>\<WS>.lodo` | the **geometry library**: base, mesh, cluster, **cluster-ladder**, and material tables, a fixed-stride local-index blob, a vertex blob, a string blob. One row per distinct model, never per placement | **9,657,316 B** (2,970 bases, 2,982 meshes, **20,678 clusters**, 419,204 vertices, 252,268 triangles, 136 materials) | 5,692,388 B (10,634 clusters, 273,695 vertices, 142,138 triangles) |
| `Data\FO4CSLOD\<WS>\<WS>.lodi` | the **instance tables**: a dense chunk table, a cell-range blob, a 24-byte instance record per placement, a parallel 8-byte cold record, and the **occluder box table + its per-cell ranges** | **128,256 B** for 3,526 placements = **36.4 B a placement** | 126,512 B, 35.9 B a placement |

**THE LADDER'S PRICE, stated plainly: the `.lodo` grew by 69.6 percent**
(5,692,388 → 9,657,316 B) for 10,044 extra clusters and 145,328 extra vertices.
That is the whole cost of the format change on this region. `--native-no-ladder`
writes **6,204,388 B** on the same region: 512,000 bytes above v2, which is the
ladder TABLE at 48 bytes for each of the 10,634 level-0 clusters — the sphere
and the cone are written whether or not a ladder was built, because a cull
dispatch needs them either way.

The `.lodi` grew by **1,744 bytes** on a region that writes **no** occluder box
(§4.5.3): that is the two new 4,096-aligned payloads and nothing else.

Same folder convention as `.lodl` (`Data\Terrain\`, which FO4 does not use, so
nothing collides) and the same editor-ID stem, so a child worldspace with
`kUseLandData` inherits its parent's files exactly as the heightmap loader
already resolves them.

They replace, **on the native target only**: the 481 `.BTO` object chunks and
their 481 manifests, the object atlas (`--atlas` is not written at all), and the
loose per-texture copies. They do **not** replace `.lodl` (terrain), `.lodt`
(the terrain pyramid), the card sheets or the mesh texture arrays — those are
shared with the stock path.

The reason the format is a library plus an instance table and not a decimation
pass is one measured number: the 34 distinct LOD models behind 97.5% of a far
chunk's 7,626 placements are **1,088 triangles**. Instancing measured **39.1×**
on disk against 15% for mesh decimation and 3% for screen-size culling.

**The stock path is untouched.** With `--native` off the region bake is
byte-identical to the same bake on the exe before this lane, over every output
file: 25 of 25 in `tests/spells/lodgen_native.sh` leg 5, and 25 of 25 against
`tests/baselines/stock_baseline.sha256`, which was written by the 2026-09-10
03:57:46 exe and includes the dim-32 bucket-cap chunk and the array/atlas set.

---

## 2. Rules inherited from `.lodt`, and they are not optional

Little-endian throughout; **absolute 64-bit offsets**; fixed table strides;
payloads in table-index order at **4,096-aligned** offsets with **zero-filled**
pad; an absent row is **all-zero**, not merely a zero offset; CRC-32 with the
zlib polynomial `0xEDB88320`; **NORTH-UP** row order and a clear
`ROW_ORDER_NORTH_UP` bit is a refusal; a 32-byte editor ID is **refused, never
truncated**; **every reserved field is zero and a non-zero reserved field is a
refusal**.

**Table ordering is part of the format**, because two-bake byte identity cannot
see a bake that is self-consistent and differently ordered from the last one:

| table | sort key |
|---|---|
| base | `formId` ascending, over the **full worldspace ESM census**, in every bake including a one-chunk one |
| mesh | model path ascending, then MNAM slot |
| cluster | `meshId`, then `materialId`, then **`level` (v3)**, then first triangle index |
| material | `family`, `arrayClass`, `arraySet`, `layer`, then the material's string |
| instance | **chunk index** (north-up row-major), then **cell index**, then **`drawKey`**, then `refFormId`, then `scolPart` |
| occluder (v3) | the cell's own order: chunk index, then cell index, then **world volume descending**, then the instance index |

The base ordering being over the **full** census, not over the bases a region
bake happens to touch, is what makes `baseId` worldspace-stable.

**`level` joins the cluster sort between the material and the first triangle**,
not above the material: a simplification group may never cross a material (§3.5.1),
so a material's whole ladder is one contiguous run and the v2 prefix
`(meshId, materialId)` still reads exactly as it did.

### 2.1 THE ONE SORT LAW, and why the cell stays outermost (v2)

bungo, 2026-09-11 08:1x, on item 5 of the performance list: *"5 sounds good"* —
instances pre-sorted by mesh then material at bake time, which changes no draw
count and only the load-time grouping.

Mesh and material could **not** become the outermost key inside a chunk. The
cell-range blob states exactly ONE `(instanceFirst, instanceCount)` run per cell
(§4, 8 bytes), and the reader checks that a chunk's sixteen cells partition its
instances *in order*. A mesh-major order inside a chunk leaves a single cell's
instances in up to sixteen disjoint runs, which that row cannot describe; the
only way to have mesh outermost is to delete the cell ranges, and they are what
the near-field suppression reads.

So the CELL stays outermost and the mesh/material rank sorts inside it. A cell
is 4,096 units — the granularity the cell blob exists for — so a consumer
building per-bucket lists still walks one contiguous run per
(cell, mesh, material). **Measured on the region:** 41 distinct draw ranks over
3,526 instances, and **787 adjacent pairs inside one cell are ordered by the
rank where the ref alone would have ordered them the other way**, so the key is
load-bearing and not decoration.

**Open for bungo:** the reverse — mesh outermost, cell ranges dropped or widened
to a run list — is a format break and is his call, not this lane's.

`drawKey` is the **rank of the base's (primary mesh id, that mesh's first
material id) pair** among every base in the `.lodo`, ascending. The primary mesh
is `rep[0]`, or the first filled slot when `rep[0]` is empty. It is a pure
function of `baseId`, and it is stored anyway for one reason: it is what lets
the **`.lodi` reader check the sort law without opening the `.lodo`**.
`--native-verify` and the independent decoder then check the rank itself against
the library, so the redundancy is checked in both directions and a wrong rank is
a refusal that names the instance, the ref and the two numbers.

---

## 3. `.lodo` header — 256 bytes at offset 0

| off | type | field |
|---|---|---|
| 0x00 | char[4] | magic `LODO` |
| 0x04 | u32 | **version = 4** (versions 1, 2 and 3 are refused by name — v3 by §3.7) |
| 0x08 | u32 | flags — **bit0 must be 1** (vertex layout v1, stride 16); bit1 `PARTIAL`; bit2 `CACHE_ORDER` (§3.3); **bit3 `LADDER` (v3)**; **bit4 `NEAR` (v7, §3.9: set exactly when the version is 7)**; all others reserved 0 |
| 0x0C | u32 | `headerCrc32`, over 0x10…0xFF |
| 0x10 | u64 | `pluginCorpusHash` — the terrain writers' hash, carried for the object/terrain/plugin triple |
| 0x18 | u64 | `objectCorpusHash` — see §8 |
| 0x20 | u64 | `modelCorpusHash` — over every source model read (path, size, content) |
| 0x28 | u64 | `cardCorpusHash` — **PROPOSED (R19, not ruled), §4.13:** FNV-1a 64 over the linked card arrays' files (name, size, bytes), sets in lower-cased-name order; **0 = no card linked** |
| 0x30 | char[32] | worldspace editor ID, NUL-padded |
| 0x50 | u32 | `baseCount` |
| 0x54 | u32 | `meshCount` |
| 0x58 | u32 | `clusterCount` — **every level**, not level 0 alone |
| 0x5C | u32 | `materialCount` |
| 0x60 | u32 | `vertexCount` |
| 0x64 | u32 | `maxClustersPerMesh` — so a consumer can size an append buffer statically |
| 0x68 | u16 | `clusterMaxTris` = 16 |
| 0x6A | u16 | `vertexStride` = 16 |
| 0x6C | u32 | `stringBytes` |
| 0x70 | u64 | offset: base table (32 B stride) |
| 0x78 | u64 | offset: mesh table (56 B) |
| 0x80 | u64 | offset: cluster table (16 B) |
| 0x88 | u64 | offset: material table (16 B) |
| 0x90 | u64 | offset: local-index blob (48 B per cluster) |
| 0x98 | u64 | offset: vertex blob (16 B per vertex) |
| 0xA0 | u64 | offset: string blob (NUL-terminated UTF-8) |
| 0xA8 | u32 | `indexCrc32` — over the **eight payloads**, in file order (v3 adds the ladder table) |
| 0xAC | u32 | reserved, 0 |
| 0xB0 | u64 | `fileBytes` |
| 0xB8 | u64 | `loadOrderHash` (v2) — §8.1. A reader gets it from the header alone, without parsing a table |
| **0xC0** | **u64** | **offset: cluster-ladder table (48 B per cluster, PARALLEL to the cluster table) (v3)** |
| **0xC8** | **u32** | **`clusterLodStride` = 48 (v3); any other value is refused by name** |
| **0xCC** | **u8** | **`levelMax` (v3) — the deepest `level` in the file; checked against the table** |
| **0xCD** | **u8** | **`ladderGroup` (v3) — the grouping target the ladder was built at; 0 iff the LADDER flag is clear** |
| 0xCE…0xCF | — | reserved, zero |
| **0xD0** | **u32** | **`cardCount` (v4) — bases whose `cardLayer` is not `LODO_NO_CARD`. A reader sizes its card-draw pass from the header alone; `cardCount > baseCount` is refused by name, and (CARDLINK1) the reader RECOUNTS it over the base rows and refuses a mismatch by name, §4.13** |
| **0xD4** | **u32** | **`colourVertexCount` (v5) — rows in the colour blob; 0 = the file carries no colour. Never more than `vertexCount`, and zero exactly when `offColours` is zero** |
| **0xD8** | **u64** | **offset: colour blob (v5), 4 B per row, written LAST (after the strings), 4096-aligned, and in `indexCrc32` only when present** |
| 0xE0…0xFF | — | reserved, zero (on a version-4 file 0xD4…0xFF) |

The ladder table sits **between the cluster table and the material table** in
file order, which is what makes a v2 file's `indexCrc32` arithmetic different
from a v3 file's: a reader that ignored the version word still could not
mistake one for the other.

### 3.1 Library vertex — 16 bytes

| off | size | field | encoding |
|---|---|---|---|
| 0x00 | 6 | position | 3 × u16 into the **mesh's own AABB**. At a 4,096-unit model box: step 0.0625 u |
| 0x06 | 4 | UV | 2 × u16 unorm into the mesh's own UV rect (`uvMin`, `uvExtent`), so **tiling UVs are exact**, including the measured `[−1.299, 2.438]` cases |
| 0x0A | 3 | normal | octahedral **12:12** — `n = b0 \| b1<<8 \| b2<<16`, `octX = n & 0xFFF`, `octY = n >> 12`. Worst error 0.0591°, mean 0.0209°, against vanilla `ByteVector3`'s 0.384° / 0.170° |
| 0x0D | 1 | tangent | bits 0–6 roll angle around the normal (2.83° step), bit 7 handedness |
| 0x0E | 1 | `sway` | per-vertex sway weight, 0 = rigid; `h²·(0.35+0.65r)` |
| 0x0F | 1 | `selfAO` | the **model's own** self-occlusion — constant across copies, so it belongs in the library, not in the instance |

No bitangent: it is exactly `cross(normal, tangent)`, verified over 949,477 real
vertices, max component deviation 0.0134, zero vertices above 0.02.

### 3.2 The other five tables

**Mesh entry — 56 B:** `f32 aabbMin[3]`, `f32 aabbExtent[3]`, `f32 uvMin[2]`,
`f32 uvExtent[2]`, `u32 clusterFirst`, `u16 clusterCount` (**every level**),
`u16 flags` (bit0 anyAlphaTested, bit1 anySway, **bit2 WATERTIGHT (v4)**: the
level-0 surface has no boundary edge; **bit3 VERTEX_COLOUR (v5)**: the mesh has
rows in the colour blob, §3.7; **bit4 VERTEX_ALPHA (v5)**: its A is opacity, only
with bit3; bits 5–15 reserved), `u32 modelStringOffset` (the
LOD model path — the table's own sort key), and **(v3, the word v2 reserved)**
`u16 clusterCountL0`, `u8 levelCount` (≥ 1), `u8 reserved` (0). See
Deviations 1. Both v3 words are REDUNDANT on purpose and both readers recount
them from the rows rather than believe them.

**Cluster entry — 16 B:** `u32 vertexBase`, `u8 vertexCount` (≤ 48),
`u8 triangleCount` (≤ 16), `u16 materialId`, `u8 boundCentre[3]` (u8 into the
mesh AABB), `u8 boundRadius` (u8/255 of the mesh radius), `u16 meshId`,
`u16 flags` — **bits 0–1 are the draw size class** (0 = ≤ 4 tris, 1 = ≤ 8,
2 = ≤ 16), **bit 2 `CONE_OPEN` (v3)**, bits 3–15 reserved.

**Cluster-ladder entry — 48 B (v3), one row per cluster, at header 0xC0:**

| off | type | field |
|---|---|---|
| 0x00 | f32[3] | `centre` — bounding-sphere centre, mesh-local |
| 0x0C | f32 | `radius` — bounding-sphere radius, **never 0** |
| 0x10 | f32 | `geometricError` — the deviation of THIS cluster's surface from the **FULL-DETAIL** surface, in the mesh's own units at scale 1. **0 at level 0, and > 0 at every deeper level** |
| 0x14 | f32 | `parentError` — the error of the group that CONSUMED this cluster, or `FLT_MAX` at a root |
| 0x18 | u32 | `parentFirst` — first cluster of the parent group's output range, or `0xFFFFFFFF` at a root |
| 0x1C | u16 | `parentCount` — how many clusters that range holds; 0 at a root |
| 0x1E | u8 | `level` — 0 = full detail |
| 0x1F | u8 | reserved, 0 |
| 0x20 | u16[2] | `coneAxis` — octahedral 16:16; `(0, 0)` when `CONE_OPEN` |
| 0x24 | f32 | `coneCos` — cosine of the cone's half angle, in (0, 1]; `−1` when `CONE_OPEN` |
| 0x28 | u32 | `sourceTriangles` — how many FULL-DETAIL triangles this cluster's subtree covers; at level 0, its own `triangleCount` |
| 0x2C | u32 | reserved, 0 |

**Local-index blob:** a fixed **48 bytes per cluster** at `clusterIndex · 48`,
three u8 local indices per triangle, slots past `triangleCount` filled with
**0xFF** (a degenerate marker the vertex shader turns into a zero-area triangle).
Coarse clusters obey the same caps and use the same blob, so a consumer's draw
path does not branch on level at all.

**Material entry — 16 B:** `u8 arrayClass` (0 = 256², 1 = 128²), `u8 arraySet`,
`u16 layer` (**< 2048**, or 0xFFFF = no array layer assigned), `u8 family`
(0 legacy / 1 pbr), `u8 alphaThreshold` (0 = opaque; vanilla writes 128),
`u8 flags` (twoSided / emits / tree), `u8 reserved` (**v7: `features`**, §3.9;
zero below v7), `f32 emissiveScale`, `u32 lodmStringOffset`.

**Base entry — 32 B:** `u32 formId`, `u32 modelStringOffset`, `u16 rep[4]` (per
MNAM slot 0–3: a mesh index, or **0xFFFF**), `u16 cardLayer` (**low 11 bits the
layer, high 5 bits the card array set**, the set's rank in §4.13's order; 0xFFFF = no card), `u16 flags`,
`f32 boundRadius` (at scale 1, **never 0** — the reader refuses it), and **(v4)**
`u32 fullTriangles` (the base's full-detail triangle count over the DISTINCT
meshes its `rep` slots name, never 0, recounted by the reader) followed by
**(v6)** `u32 materialSwap` — the MSWP form this row is a colourway of, 0 on a
plain row (§3.8). On v4/v5 those four bytes were `u16 crossPx16[2]`, always
written 0, and a v4/v5 file is read with `materialSwap` = 0 whatever they hold.
The first four bytes of `fullTriangles` are the ones v3 spent on
`crossPx16[0..1]`, which is why a v3 file is refused by name (Deviation 14;
`src/lodofile.h` `struct LodoBase`). Base flags: bit0..2 as before, **bit3
SWAPPED (v6)**, set exactly when `materialSwap` is not 0.

**The two caps fight each other** at 1.843 vertices a triangle: a 16-triangle
patch of an open strip can need more than 48 vertices. The rule is **whichever
cap binds first closes the cluster**; `triangleCount` may end below 16.

### 3.3 GPU cache order (v2), and what it actually bought

bungo, 2026-09-11 08:2x, verbatim: *"vertex/triangle order for the GPU cache on
every .lodo mesh"*.

At emit, each shape's triangles go through `meshopt_optimizeVertexCache` and its
vertices through `meshopt_optimizeVertexFetchRemap` **before** the cluster walk,
so a cluster's sixteen triangles touch as few distinct vertices as possible —
fewer boundary copies is a smaller vertex blob — and the blob comes out in
first-use order. Header flag bit 2 says the library was written that way; a
library without it is in source order and is still valid.

**It is a PERMUTATION.** No vertex is created, none is removed, and the welded
topology is unchanged — which §3.4's boundary-edge counts prove, per mesh, on
every mesh of the corpus rather than by assertion.

**THE BETTER OF THE TWO ORDERS IS KEPT, per shape.** meshopt's cache optimiser
is a heuristic, and measured over the Commonwealth's 2,982 LOD meshes it read
**worse** than the order Bethesda shipped on 21 of them. Both orders are already
measured, so the writer keeps whichever is better and "no mesh reads worse" is
true by construction instead of by hope.

**What it bought, measured, and it is small.** ACMR at a 16-entry post-transform
cache, triangle-weighted over the 2,982 meshes: **1.8600 → 1.8585**; overfetch
1.0097 → 1.0097; the pass changed **45 of 2,982** meshes. Bethesda's shipped LOD
meshes are already close to cache-optimal, so the honest statement is that the
pass costs nothing, can no longer hurt, and buys about **0.08% of vertex-shader
invocations**. It is reported as what it is, not as a win.

*(The v2 page quoted a vertex and byte saving here. Both figures were v2's own
`.lodo`; under v3 the level-0 vertex count is unchanged at 263,876 source
vertices and the file's totals are the ladder's, §1.)*

### 3.4 The shadow-caster silhouette rule (v2, and what v3 adds)

bungo, 2026-09-11 08:3x, verbatim: *"remember, we'll have far shadows cast by
those lods, so a far shadow cast by a LOD tower behind me, will cover the area
I'm at"* — every object in the LOD set is a shadow caster, and decimation may
never open a silhouette.

The invariant is the count of **boundary edges** — edges used by exactly one
triangle — over the topology **welded by the quantised library position**.
Welding by position is what makes the two sides comparable: the source is split
into shapes and the emitted mesh into clusters, and neither split may change the
silhouette. The emitter computes both counts per mesh and **refuses**, naming
the model and both numbers, if the emitted count exceeds the source's.

**Measured on the region: 2,982 meshes, 365 of them watertight, 0 opened,
0 closed, worst delta 0.** The floor is shown red in the gate: removing one
triangle from the largest mesh (`AirportTower01_LOD.nif`) takes it from 430
boundary edges to 433, which the same rule rejects.

**v3 adds the ladder's half of the same question, and it is a different
statement.** Level 0 is a permutation and its boundary count must EQUAL the
source's, which is the refusal above. A coarse level is a decimation, so its
boundary count may only fall; the mesh report carries `boundaryCoarsest` beside
`boundaryEmitted` and the field gate refuses any mesh where the coarsest level's
count **grew**. Measured on this region: **57 groups were REFUSED** for doing
exactly that, and **18 of 1,900 laddered meshes** still show a coarsest level
whose count exceeds level 0's — see Deviation 11, which is the audit of why
that second number is not the hole it looks like.

The stronger statement, and the one that actually protects the shadow, is the
error itself: **a cluster's silhouette cannot move further than its
`geometricError`**, and the selection law keeps that under one pixel. A count
says a hole did not appear; the error says by how much the outline may have
shifted, and it is the number a shadow pass can budget against.

`--native-mesh-report <file>` writes one line per mesh (report version **4**,
**twenty-six columns**; `model` stays the line's remainder because it is the only
token that may hold a space):
`meshId triangles srcVerts emitVerts acmrBefore acmrAfter atvrBefore atvrAfter
boundarySrc boundaryEmitted levels clustersL0 clustersLadder maxError
groupsFormed refSmall refNoCut refFlat refSilhouette errExact errBounded weldedVerts
uvConflicts boundaryCoarsest casterInstances model`.

**Two of those columns arrived after this paragraph, and it did not move with
them.** `refSilhouette` is v3—s: the header on disk already wrote `report 3`
with twenty-five columns while this page still said 2 and twenty-four, so the
page was wrong rather than merely old (lane GENSMALL1, 2026-09-16).
**`casterInstances` is v4—s**: the number of instances whose base names this
mesh, each instance counted once per DISTINCT mesh its base names. That is the
per-source caster count at mesh granularity (CENSUS §5.3, bungo 2026-09-11
14:4x), and the column sums to the `native-casters:` line's `mesh-slot casters`.

### 3.5 The cluster ladder (v3)

bungo, 2026-09-11 08:0x, item 1, taken with *"1, 2"*: per-CLUSTER screen error
instead of four rings — *"continuous detail, no pop at ring borders, triangles
spent only where they show"*. And at 10:3x, for the near field as well: *"the
`.lodo` cluster hierarchy must include the NEAR levels too … the error ladder
runs from full detail down, so the inward extension has something to select."*

#### 3.5.1 How a level is built, and the four ways a group refuses

Level 0 is **exactly what v2 emitted**: the same per-shape greedy walk, the same
caps, the same flush between shapes, the same attributes straight off the source
shape. Nothing about the full-detail geometry changed.

Above it, per **(mesh, material)** — never across a material, because two
materials are two textures and an edge collapse across them drags one texture's
geometry onto the other's:

1. **Weld.** One entry per distinct QUANTISED library position over the
   material's shapes. An edge collapse cannot cross a split vertex, and the
   library stores two vertices at one quantised position as the same three
   numbers anyway. Attributes come from the first contributor — see
   Deviation 9, which carries the measured cost.
2. **Group.** `meshopt_partitionClusters` over the level's clusters, target
   `ladderGroup` = **4** clusters a group, which groups by shared vertices and
   then by proximity.
3. **Lock the border.** Every welded vertex that a triangle OUTSIDE the group
   still uses is locked. This is the whole reason a ladder is built on groups
   and not on clusters: a consumer that replaced this group and kept its
   neighbour would otherwise see a crack along the shared edge.
4. **Simplify** to half the triangles with `meshopt_simplifyWithAttributes`.
5. **Measure** (§3.5.2).
6. **Re-split** the result under the same 16-triangle / 48-vertex caps, at
   `level + 1`, and link: every cluster of the group gets `parentFirst`,
   `parentCount` and `parentError`; every output gets the group's error and its
   share of the coverage.

**A group that cannot be formed is not an error.** Its clusters simply stay
ROOTS, and the reason is counted BY NAME in the census and in the mesh report:

| refusal | rule | measured, this region |
|---|---|---|
| `refSmall` | the group holds `LODO_LADDER_MIN_TRIS` = 4 triangles or fewer | **2,476** |
| `refNoCut` | the simplifier removed no triangle (disconnected components, a locked border with nothing between it) | **34** |
| `refFlat` | the error would not GROW above the children's — the brief's own stopping rule, and what keeps every chain STRICTLY increasing | **742** |
| `refSilhouette` | the simplified group has MORE boundary edges than the surface it replaces: a hole, and bungo's far-shadow ruling forbids it (§3.4) | **57** |
| — | formed | **6,235** |

The cap on depth is `LODO_LADDER_MAX_LEVEL` = 15.

#### 3.5.2 The error, and which of the two rules measured it

`geometricError` is the deviation from **FULL DETAIL**, never from the parent.
That is what lets a consumer compare ONE stored number against ONE tolerance
without walking the chain, and it is what makes the errors monotone.

Two rules, and the file says how many rows each one served:

* **Exact**, when the group's subtree covers at most `LODO_ERROR_EXACT_TRIS`
  = 512 full-detail triangles: the two-sided vertex-sampled Hausdorff distance
  between the group's simplified triangles and the full-detail triangles under
  it. **6,973 groups.**
* **Chain-bounded**, above that: `child error + this step's deviation`, which
  the triangle inequality makes an UPPER bound on the true deviation. **4 groups.**

Then `E = max(E, max child error)`, and the group is not formed unless
`E > max child error`. So every chain's errors strictly increase, which is what
makes "tolerance 0 selects level 0 and nothing else" true rather than hopeful.

#### 3.5.3 What the ladder came out as, measured

Nine-chunk Sanctuary, the whole worldspace library:

| level | clusters | triangles | mean `geometricError` (world units at scale 1) |
|---|---|---|---|
| 0 | 10,634 | 142,138 | 0.0000 |
| 1 | 5,676 | 68,448 | 165.62 |
| 2 | 2,781 | 29,699 | 497.16 |
| 3 | 1,029 | 8,750 | 726.31 |
| 4 | 414 | 2,503 | 921.86 |
| 5 | 123 | 626 | 1,192.52 |
| 6 | 19 | 98 | 1,527.20 |
| 7 | 2 | 6 | 3,328.47 |

**1,900 of 2,982 meshes have a level above 0.** Of the 1,082 that do not,
**1,065 are 16 triangles or fewer** — one cluster, nothing to group — and the
remaining 17 named their refusal. **4,715 root clusters cover 142,138
full-detail triangles**, which is the library's level-0 triangle count exactly:
the ladder is a partition of its own surface, checked per mesh by
`--native-verify` and by the reference selector at every tolerance.

#### 3.5.4 THE FINDING, and it is bungo's call

The ladder is correct and it is barely selectable. Expressed against each
model's own size rather than in world units:

* the median level-1 cluster deviates by **3.80 percent of its model's
  diagonal**;
* over the 1,900 laddered meshes, the deepest level's error is a median
  **26.9 percent** of the model diagonal, p90 **52.8 percent**;
* the worst are objects that are a handful of triangles already —
  `HitExtStrutureMassFusion02_LOD.nif` is 54 triangles for a 7,093-unit
  building, and one halving of that costs 98 percent of its diagonal.

At the reference projection (§4.4), a 38-unit deviation on a 1,000-unit building
reaches one pixel only past **52,100 units**. So at a one-pixel tolerance the
ladder's first step is not selected anywhere inside the Commonwealth, and the
measured cut (§4.4.1) spends between 104,090 and 124,205 triangles across the
whole region whatever the distance.

**The cause is not the ladder. It is that "full detail" in this file is already
Bethesda's LOD mesh** — a mean of 47.7 triangles for a whole building, with
almost nothing left to remove. The ladder would have room, and bungo's 10:3x
ruling would be served, if the library were built from each base's own **near
`MODL`** instead of its `MNAM` slots, so that level 0 is the real mesh and the
LOD slot is one rung down. That is a different library (bigger by the ratio of
near to LOD geometry) and it is **his decision**, not this lane's. Nothing here
blocks it: the format, the writer and every gate are indifferent to what level 0
was built from.

### 3.5.5 Foliage is never laddered (v4)

A cluster whose material is **alpha-tested** — `alphaThreshold > 0`, which in this
corpus means a tree or a bush card — is refused a group and stays a root. The
refusal has its own name, `refFoliage`, and its own census number. bungo, over
lane NATIVE1b's `ladder.png`, 2026-09-11 16:1x: *"Hm, that tree LOD becomes a
stump there"*. A simplifier scores an alpha-tested leaf quad by its geometry,
and its geometry is two triangles that mean nothing: the shape is in the cutout.

Measured on the 9-chunk Sanctuary region: **2,849 clusters refused**. The way
back is `--native-ladder-foliage`, which refuses **0** and puts them back — the
same region's level-1 cluster count goes **152,577 → 154,201**. Both arms are
gated, in `tests/spells/lodgen_ladder.sh` §2.

### 3.5.6 The silhouette floor, per LEVEL (v4)

Every level the ladder keeps must hold at least `silhouetteMin` of **level 0's**
outline, seen from the worst of `LODO_SILHOUETTE_VIEWS = 8` azimuths around the
horizon, rasterised at `LODO_SILHOUETTE_GRID = 96` px over the mesh's own box.
The comparison is the **CUT**, not the level: the roots that dropped below it,
the clusters this level did not consume, and the level's own output — because
Deviation 11 already records that a ladder is PARTIAL wherever a group refuses,
so a level's own clusters are only a fragment of what a viewer sees.

Below the floor the whole level is **rolled back** — parents cleared, clusters,
rows, local indices and vertices truncated to the snapshot — and that material
stops laddering. Refusals are counted as `refLevelSilhouette`, and the worst
fraction the file kept is reported per mesh.

**The default is 0.70 and here is the reason**, not an assertion: the rule bungo
gave is *a building may shrink; a tree may never become a stump*, so the floor
has to sit between what a simplified building does and what a destroyed outline
does. `tests/spells/lodgen_silhouette.py` measures both ends independently, with
its own rasteriser over the bytes: a random-vertex-drop twin of level 0, matched
on triangle count, is the FLOOR arm, and the unsimplified soup against itself is
the CEILING arm that must read exactly 1.0. **`--native-silhouette <0..1>` is the
switch, and `0` turns the gate off entirely, which is the exact v3 ladder.**

### 3.5.7 Level 0 comes from the base's near `MODL` (v4) — REVERSED 2026-09-17

> **Not the default any more.** bungo, 2026-09-17: *"Authored LODs only"*. The
> default library is the base's `MNAM` LOD slots (`--library mnam`) and the
> ladder ships OFF (`--native-no-ladder`); `--library near` and `--native-ladder`
> are the ways back to what this section describes (`src/nifcli.cpp`,
> `bool lgNativeLadder = false` and `bool lgLibraryNear = false`). A default
> urban `.lodo` is then 6,204,388 B with `levelMax` 0: every cluster a root, so
> the cluster cut selects full detail everywhere. The rest of this section is
> the 2026-09-16 near-library design, kept because the switch still bakes it.

bungo, 2026-09-16 11:1x, ruling on plan §6 (a): *"Also do the parked"*. The
library's level 0 is now built from each base's **near model**, and the base's
`MNAM` LOD slots move one rung down the ladder. `--library mnam` is the exact
way back and bakes what v3 baked.

This is not a format change — §12 said so in v3 — but it changes what every
number in the file is ABOUT, and §5 of `docs/FO4CS_IMPROVED_LOD_PLAN.md` is the
reason it was worth doing: with `MNAM` at level 0, "full detail" was already
Bethesda's LOD mesh at a mean of 47.7 triangles for a whole building, the median
level-1 cluster deviated 3.80 percent of its model's diagonal, and the ladder's
first step was not selectable anywhere in the Commonwealth. A base with no near
`MODL` keeps its `MNAM` slots where they were and is counted by name.

### 3.6 The bounding sphere and the normal cone (v3)

bungo, 2026-09-11 08:0x, item 2: per-cluster bounding sphere and normal cone for
GPU culling.

**Both describe the STORED geometry, not the geometry that walked in.** The
library keeps positions as u16 into the mesh AABB, so a consumer's triangle is
up to half a quantum away from ours on every axis. A sphere fitted to the FLOAT
positions misses its own stored vertices, and a cone fitted to the float face
normals excludes stored faces: measured on this region before it was fixed,
**worst sphere overshoot 0.058 u and worst cone cosine deficit 0.002033**, both
pure quantisation, both gone the moment the description is computed from the
same numbers the reader will read. This is a format-level rule, not an
implementation detail: **a v3 writer computes the sphere and the cone from the
dequantised positions.**

* **The sphere** is the cluster's own box centre and the distance to its
  farthest stored vertex, widened by one relative ulp and an absolute 1e-4 so a
  containment test in float cannot sit exactly on the boundary.
* **The cone's axis** is the AREA-WEIGHTED sum of the face normals — the
  direction a cluster's biggest faces agree on, rather than the one its
  smallest slivers vote for — octahedrally packed at 16 bits an axis. The
  half-angle is then the WORST face against the axis **as the reader will
  decode it**, so the axis's own quantisation is already inside the stored
  cosine.
* **`CONE_OPEN`** (cluster flags bit 2) is the cone's REFUSAL, in a bit rather
  than as a magic cosine: the cluster's normals span more than a hemisphere and
  no cone bounds them. Leaf cards, crossed quads and two-sided shapes. A
  consumer must never backface-cull an open cluster. Measured: **14,604 of
  20,678 clusters are open**, which is what a worldspace of trees and cut-out
  fences looks like.

The gate is `tests/spells/lodgen_native_cut.py` groups A and B: every triangle
of every cluster inside its sphere (+1e-3), every face normal inside its cone,
with a shrunk sphere and a tightened cone shown red in the same run.

### 3.7 Version 5: the optional per-vertex colour stream (lane SEAM1, W4, 2026-09-25)

**What it is for.** A handful of vanilla LOD models tint their own vertices: the
Amphitheater's shell, the blasted maples, the warehouse roofs, the brick shells.
The game draws that tint because the shape has BOTH a colour channel in its
vertex descriptor (attribute bit 0x20) AND the `Vertex_Colors` shader flag
(SLSF2 bit 5). v4 had no room for it, so those models went grey in the native
far field. bungo's ruling (2026-09-25): *match the game exactly* -- carry the
colour only where both are set, apply it only there, and keep RGB and A as their
own channels, the way the game uses them.

**The law.**

* A shape contributes colour iff it has the channel AND `Vertex_Colors`. A shape
  with only one of the two contributes nothing, exactly as the game draws it.
* A mesh with at least one such shape is flagged `VERTEX_COLOUR` (mesh bit 3) and
  gets **one RGBA8 row per vertex over its whole contiguous vertex range**; its
  other shapes' vertices carry opaque white, which multiplies to no change.
* `VERTEX_ALPHA` (mesh bit 4) is set when such a shape also has SLSF1
  `Vertex_Alpha` (bit 3). A is stored as the source stores it either way; only
  this bit makes it opacity. Measured on the Boston census, 28 streamed shapes:
  all 28 carry `Vertex_Colors`, none `Vertex_Alpha` and none `Tree_Anim`, and 4
  have A below 255 (so A is kept, and ignored, on them).
* The blob: the flagged meshes in mesh order, each its rows `vertexBase..end` in
  vertex order, R G B A. It sits after the strings, 4096-aligned, and joins
  `indexCrc32` only when present.

**What stays the same.** A file with no colour differs from a v4 file in the
version word at 0x04 **and nowhere else**: 0xD4…0xDF are zero, no mesh bit 3/4,
no blob. The version word is outside `headerCrc32` (0x10…0xFF), so the CRC and
the `.lodi`'s `lodoIdentity` are unchanged. A version-4 file is read as a
version-5 file with no colour.

**The viewer.** `src/lodinative.cpp` multiplies the colour into the vertex colour
of every view (the channel views keep their own value, times the colour), sets
SLSF2 `Vertex_Colors` on a bucket holding a flagged mesh, and SLSF1
`Vertex_Alpha` only on a `VERTEX_ALPHA` mesh; elsewhere A is drawn as 1.

**The gate** (pre-registered before the build,
`scratchpad/seam1_20260925/w4_gate.py`, inputs from `w4_bakes.sh`; the
instruments' own self-test is `w4_synth.py`): **G1** the synthetic fixture is
byte-identical but 0x04, and each region's file with its colour stripped is too;
**G2** the Amphitheater and both blasted maples are `VERTEX_COLOUR`, the
Amphitheater and maple 01 carry non-white RGB, the flags agree with the source
NIFs both ways, and each flagged mesh's decoded rows are its source's rows;
**G3** the same gate on the pre-v5 exe's bakes is RED.

**The FO4CS reader is owed.** The in-game reader of the native pair reads v4;
it needs the v5 header words and the colour blob before a v5 library can ship
to it. That is the standing order (FO4CS readers come last), not news.

**History.** An earlier version 5 -- the subdivided library for the per-vertex
horizon stream (§4.11) -- was written by lane HORIZON3 on 2026-09-19 and removed
whole by lane HORIZONOUT the same day. No exe ever wrote it, so this version
number was free.

### 3.8 Version 6: material-swap variant base rows (lane SWAP1, 2026-09-25)

**Why.** bungo, 2026-09-25: *"these towers still look grey, compare the vanilla
lod towers to these"*. Lane TOWER1 measured it: the CK bakes each placement's
material swap into the vanilla LOD atlas, and lodgen ignored swaps. An `MSWP`
record is a list of `BNAM` (original material) -> `SNAM` (replacement) rows,
paths relative to `Materials\`, each with an optional `CNAM` colour-remap index.
A `REFR`'s `XMSP` names one; a base's `MODS` names one; the reader takes the
winning record of each.

**The effective swap of a placement** (`nativeEffectiveSwap`): the REFR's `XMSP`,
else the `MODS` of the REFR's `NAME` base (the SCOL itself, for a SCOL part),
else -- for a SCOL part only -- the part base's own `MODS`.

**The rows.** The library is keyed by (base, effective swap). For every pair the
whole-worldspace census names (so the library stays a pure function of the
census, and region/reuse bakes agree), when a swap row's `BNAM` names a material
of one of the base's four `MNAM` models, the base gets a **variant row**: the same
`formId`, `materialSwap` = that MSWP, flag SWAPPED, and `rep[k]` pointing at a
**variant mesh** -- the same LOD NIF loaded with those materials replaced (the
replacement BGSM and its textures resolve through the MO2 stack like any other).
A pair whose rows change nothing (no LOD material named, or a row naming itself)
gets no row and its placements keep the plain row. Two MSWPs giving the same
substitution on the same model share one variant mesh (its key carries the
lowest such MSWP). A variant row keeps the plain row's card layer (the card is
unswapped), and occluder boxes use the plain model (the geometry is identical).

**Sort law.** The base table is sorted by `(formId, materialSwap)` strictly, so a
base's plain row comes first and its variants follow it. A placement's `.lodi`
`baseId` is its variant row when one exists for (base, effective swap), else the
plain row. The mesh table keeps its order (a variant mesh's string is the model
path plus `|mswp:<8 hex>`, folded), so a worldspace with no swap is
**byte-identical to v5 apart from the version word** at 0x04 -- outside
`headerCrc32`, so `lodoIdentity` does not move and the `.lodi` is unchanged.

**CNAM is counted, not applied.** The colour-remap index selects a row of the
replacement material's grayscale-to-palette ramp; the census line reports how
many swap rows that hit a LOD material carry one. Applying it is future work.

**The census line** (`native-material-swaps:`) reports the placements read,
those carrying a swap by clause, those sent to a variant row, those whose swap
names no LOD material, TOWER1's own count (Fallout4.esm REFRs whose `XMSP`, else
`NAME` base `MODS`, names a material of their slot-0 model: 21,064 on the
Commonwealth), the census pairs, variant rows and meshes, load failures,
missing swap records, CNAM rows, and the `.lodo` version.

**The FO4CS reader is owed** (standing order: FO4CS readers come last). A v5
reader refuses a v6 file by version. Once it accepts 6, a variant row draws like
any base row (its `rep` names the swapped mesh); only a lookup BY formId must
learn that one formId can now own several rows, the plain one first.

### 3.9 Version 7: the NEAR library (lane NEAR1, 2026-09-26)

**What.** `lodgen <plugins|--mo2-profile P> --worldspace HEX [--terrain-region x0 y0
x1 y1] --near-library <dir>` bakes the FULL-DETAIL models (the base's own `MODL`,
never an MNAM LOD model) of the static placements the engine draws up close into
`<dir>/<ws>.near.lodo` + `.near.lodi`, plus three text sidecars. It is a separate
path (`src/nearlib.cpp`); the far writer is not entered.

**The layout is v6's.** Version 7 adds two meanings and no byte:

| where | v7 meaning |
|---|---|
| header 0x08 bit4 `LODO_FLAG_NEAR` (16) | this is a near library: every mesh is a base's full model, one level (`levelCount` 1, no ladder), `rep[1..3]` 0xFFFF. **Set exactly when the version is 7**; a v7 file without it, or the flag on any other version, is refused |
| material row +7 (`u8`, v6 `reserved`) `features` | bit0 `PARALLAX` (SLSF2 Multi_Layer_Parallax), bit1 `ENV_MAP`, bit2 `GREYSCALE` (greyscale to palette), bit3 `VERTEX_COLOUR`, bit4 `MODEL_SPACE_NORMALS`; **bits 5..7 refused**; non-zero below v7 refused |
| material `arraySet` / `layer` | on a NEAR file they index the DIFFUSE set of `<ws>.near.textures.txt`, not the far arrays |

The draw BUCKET is derived, never stored twice: (family, `alphaThreshold` != 0,
`TWO_SIDED`, `PARALLAX`) -- opaque, alpha-test, two-sided, alpha-test-two-sided,
parallax; legacy vs PBR is `family`.

**Clusters.** The far code's clusters are reused as they are: **at most 16
triangles and 48 vertices** each, with the §3.6 bounding sphere and normal cone.
The campaign brief's "~128 triangles" is NOT what this rung writes; a bigger
cluster is a later rung's call.

**Eligibility** (per REFR, the first failing rule is its census reason): deleted;
no base; a type other than STAT or SCOL (`type:DOOR`, `type:FURN`, `type:ACTI`,
`type:CONT`, `type:LIGH`, `type:MSTT`, trees ...); STAT Is Marker; DEST/DSTD; no
MODL; SCOL with no parts. Per placement (a STAT, or each SCOL part placement, the
part judged as a STAT with `part-` reasons): `model-missing` (not in the stack),
`no-geometry` (in the stack, no BSTriShape with vertices and triangles),
`animated` (any NiTimeController / NiSequence block), `no-drawable-shape`,
`scale-out-of-range` (> 15.99988, §4.14). A SCOL REFR with no eligible placement is
`scol-no-eligible-part`. **Shapes** excluded: `effect` (BSEffectShaderProperty /
BGEM), `alpha-blend`, `decal` (SLSF1 bits 26/27 or BGSM), `tree-anim` (SLSF2 bit 29
or BGSM bTree); EditorMarker subtrees are skipped as the far loader does. A
**BSMeshLODTriShape** keeps its first `LOD0 Size` triangles and only the vertices
they use (measured: every such shape in Boston stores LOD1+LOD2 after LOD0).

**Material key** = (base, effective material swap), the SWAP1 rule (§3.8): a swap
that renames a material of the model gets a variant base row.

**Sidecars** (text, beside the pair):
* `<ws>.near.textures.txt` -- `set <id> dxgi <code> w <px> h <px> part <n> layers
  <count>` lines, then each material's slots with their own (format, size) bucket.
  A LIST: nothing is re-encoded. The array layer reaches the shader through UV2.y
  as in the far field.
* `<ws>.near.shapes.txt` (v2) -- one row per source shape: mesh, path, variant,
  block, name, verts, tris, lod0, model-space box, `kept` or the reason, material,
  meshId, srcTris. verts/tris/box are what is DRAWN (after the LOD0 trim).
* `<ws>.near.refs.txt` -- `R ref type eligible|reason` per REFR read and `P ref part
  base swap baseRow|- eligible|reason disabled` per placement.

**Gates** (`tests/spells/near_library_check.py`, independent Python ESM + NIF +
BGSM reading; `tests/spells/near_format_selftest.py` for the refusals; the far
byte identity with the rung exe): G1 every eligible REFR in the `.lodi` exactly
once with its form id, the eligible count equal to the Python count; G2 per shape
tris + verts + box equal to the source NIF; G3 the census sums. Whole Commonwealth
(his MO2 stack, 2026-09-26): 736,214 REFRs read, 523,755 eligible, 677,390
placements, 38,622 (mesh, material) pairs, 12,160,169 triangles, 777,824 clusters;
`.lodo` 382,058,580 B, `.lodi` 26,549,276 B; 178 s.

**The FO4CS reader is owed** (standing order). A v6 reader refuses a v7 file by
version, which is the right answer: a near library is not a far field.


---

## 4. `.lodi` header — 256 bytes at offset 0, **512 on a version-7 file**

| off | type | field |
|---|---|---|
| 0x00 | char[4] | magic `LODI` |
| 0x04 | u32 | **version = 3, or 4 when the file carries aggregates (§4.6), 5 when it carries the placement-AO blob (§4.7), 6 when it carries the per-vertex AO stream (§4.8), 7 when it carries a group table (§4.9) or a per-vertex sky stream (§4.10), or 9 when it carries the workshop-scrappable bit (§4.12), or 10 when any instance carries the wide-scale bit (§4.14)**; versions 1 and 2 are refused by name. **Version 8 is RETIRED** (§4.11): no exe in this tree writes one, v9 is a superset of **v7** and not of v8, and the reader opens a v8 file met in the wild |
| 0x08 | u32 | flags — bit0 `ROW_ORDER_NORTH_UP` (**clear = refusal**), bit1 `PARTIAL`, bit2 `NOLIB` |
| 0x0C | u32 | `headerCrc32` — over `0x10 … headerBytes − 1`, so it covers **256 bytes on a v3…v6 file and 512 on a v7 one**, and a v6 file's CRC is the byte-for-byte same number it was before v7 existed |
| 0x10 | u64 | `pluginCorpusHash` — must equal the `.lodo`'s |
| 0x18 | u64 | `objectCorpusHash` — must equal the `.lodo`'s |
| 0x20 | u64 | `lodoIdentity` — **FNV-1a 64 over the `.lodo`'s `headerCrc32`, `modelCorpusHash` and `objectCorpusHash`**, never an XOR |
| 0x28 | char[32] | worldspace editor ID |
| 0x48 | i16 ×4 | `chunkWest, chunkSouth, chunkEast, chunkNorth` — **inclusive, in chunk units** (`cellX >> 2`) |
| 0x50 | u16 | `chunkCells` = 4 |
| 0x52 | u16 | `instanceStride` = 24 |
| 0x54 | u32 | `chunkCount` — the dense table length, **capped at 65,536** |
| 0x58 | u32 | `instanceCount` |
| 0x5C | u32 | `presentChunks` |
| 0x60 | u32 | `maxInstancesPerChunk` |
| 0x64 | u32 | `indexCrc32` — the chunk table, the cell ranges, **the occluder table and the occluder ranges (v3)**, in that order |
| 0x68 | u64 | offset: chunk table (32 B stride, **dense**, north-up row-major) |
| 0x70 | u64 | offset: cell-range blob (8 B per cell, **`chunkCells²` per present chunk**, in table order) |
| 0x78 | u64 | offset: instance blob (24 B) |
| 0x80 | u64 | offset: cold blob (8 B, parallel to the instance blob) |
| 0x88 | u64 | `fileBytes` |
| 0x90 | u64 | `loadOrderHash` (v2) — must equal the `.lodo`'s; a mismatch is a pairing refusal naming both files |
| **0x98** | **u64** | **offset: occluder box table (40 B stride) (v3)** |
| **0xA0** | **u64** | **offset: occluder cell-range blob (8 B per cell, PARALLEL to the cell ranges) (v3)** |
| **0xA8** | **u32** | **`occluderCount` (v3)** |
| **0xAC** | **u16** | **`occluderStride` = 40 (v3)** |
| **0xAE** | **u16** | **`maxOccludersPerCell` = 4 (v3); 0 is a refusal even when no box is written** |
| **0xB0** | **u64** | **offset: aggregate table (48 B stride) (v4)** |
| **0xB8** | **u64** | **offset: covered-instance blob (4 B per entry) (v4)** |
| **0xC0** | **u32** | **`aggregateCount` (v4); 0 on a v4 file is a refusal** |
| **0xC4** | **u32** | **`coveredCount` (v4) — the blob's length in u32s** |
| **0xC8** | **u16** | **`aggregateStride` = 48 (v4)** |
| **0xCA** | **u16** | **`aggregateViews` (v4) — azimuths a sheet, a file constant** |
| **0xCC** | **f32** | **`aggSwitchPx` (v4) — the cross-fade threshold, §4.6.5** |
| **0xD0** | **f32** | **`aggBandRatio` (v4) — 1.0 or below is a refusal** |
| **0xD4…0xE3** | **4 × u32** | **`slotInstances[4]` (v5) — instances drawn from each of the base's four `MNAM` rungs. The four must sum to `instanceCount`; a sum that does not is refused by name** |
| **0xE4** | **u64** | **offset: placement-AO blob (v5), written LAST so no existing offset moves** |
| **0xEC** | **u32** | **`placementAoCount` (v5) — must equal `instanceCount`** |
| **0xF0** | **u8** | **`placementAoStride` = 1 (v5); any other value is refused by name** |
| 0xF1…0xF3 | — | reserved, zero |
| **0xF4** | **u64** | **offset: vertex-AO stream (v6), written LAST so no existing offset moves** |
| **0xFC** | **u32** | **`vertexAoBytes` (v6) — the whole stream, offsets included; must be ≥ 4 × (`instanceCount` + 1)** |
| **0x100** | **u64** | **offset: group table (v7, §4.9), written LAST so no existing offset moves** |
| **0x108** | **u32** | **`groupCount` (v7) — the chunks' group counts SUMMED; the reader adds them up itself and refuses a header word that disagrees** |
| **0x10C** | **u16** | **`groupStride` = 2 (v7); any other value is refused by name** |
| **0x110** | **u64** | **offset: per-vertex sky stream (v7, §4.10)** |
| **0x118** | **u32** | **`vertexSkyBytes` (v7) — the whole stream, offsets included; must be ≥ 4 × (`instanceCount` + 1)** |
| 0x11C…0x1FF | — | reserved, zero (v7) |
| — | — | the pad starts at 0xF1 on a v5 file, 0xD4 on a v4 file and 0xB0 on a v3 file; a v3 or v4 file carrying anything at 0xE4…0xF0, or a v3…v5 file carrying anything at 0xF4…0xFF, is refused BY VERSION NAME. **A version-3…6 file carrying anything at 0x100…0x11F is refused by version name too: those versions have a 256-byte header and END at 0x100.** |

**THE HEADER BLOCK GREW, and that is a deviation stated out loud.** The 256-byte
block was FULL: after v6 the only free bytes were 0xF1…0xF3, three of them, where
version 7 needs twenty-four. So the block is 512 bytes **on a version-7 file
only**. Nothing moved to pay for it: 0x100…0xFFF was already zero pad ahead of
the 4096-aligned first payload, and because `headerCrc32` is defined over
`0x10 … headerBytes − 1` rather than over a literal 0x100, every version-3…6
file keeps the exact CRC and the exact bytes it had. `lodiHeaderBytes(version)`
is the one place that decision lives, and the writer's `file.resize()`, the CRC
and the reader's first payload offset all read it rather than a constant.

**Chunk table entry — 32 B:** `u32 instanceFirst`, `u32 instanceCount`,
`f32 zMin`, `f32 zExtent`, `f32 maxBoundRadius`, `u32 cellRangeOffset`,
`u32 crc32` (over this chunk's instance + cold records), `u32 reserved`.
X and Y of the chunk box are **implied by the table index**, so only Z is stated.
**An absent chunk is 32 zero bytes.**

`maxBoundRadius` is load-bearing: the chunk box is built from instance
**origins**, so an 8,224-unit tree's bound leaves the box and a per-chunk cull
against `zMin`/`zExtent` alone would pop it.

**Cell-range entry — 8 B:** `u32 instanceFirst`, `u32 instanceCount`.

### 4.1 The instance record — 24 bytes, fixed stride, 8-byte aligned

| off | size | field | encoding |
|---|---|---|---|
| 0x00 | 6 | `position` | 3 × u16 into the **chunk box**. X and Y span 16,384 world units; Z spans the chunk directory's `zMin`/`zExtent`. Step 0.250 u, worst 0.125 u |
| 0x06 | 6 | `rotation` | 2-bit selector (the dropped, largest component; w,x,y,z order) + 3 × 15-bit smallest-three quaternion over [−1/√2, 1/√2], **LSB-first over the three u16**. Measured worst **0.0073°**. This is the **DRAWN** rotation — the ESM rotation × the generator's tree yaw — see Deviations 2 |
| 0x0C | 2 | `scale` | u16, `scale = v / 8192`, range 0 … 7.99988; **with flags bit 7 (version 10), `scale = 8 + v / 8192`, range 8 … 15.99988** (§4.14). **A stored 0 without bit 7 is a refusal** (§4.1b) |
| 0x0E | 2 | `baseId` | u16 index into the `.lodo` base table |
| 0x10 | 1 | `ao` | u8 |
| 0x11 | 1 | `sky` | u8, sky visibility |
| 0x12 | 1 | `ground` | u8 ground-contact blend over the 256-unit ramp |
| 0x13 | 1 | `seed` | u8 = `treeHash & 0xFF` (0 for a non-tree): sway phase / jitter only, **not** the yaw — see §4.3 |
| 0x14 | 2 | `flags` | u16: bit0 mirrored, bit1 force-card (§4.13: the base has a card and the ring slot has no mesh, or a `C` line put it on its card), bit2 alpha-tested, bit3 emits, bit4 SCOL part, bit5 buried-cull candidate, **bit6 workshop-scrappable (version 9 only, §4.12: set in a file below version 9 it is refused by name)**; **bit7 wide scale (version 10 only, §4.14: set by the writer alone, refused by name below version 10)**; **bit8 initially disabled (version 11 only, §4.15: the near library alone)**; **bits 9–15 reserved, and a set reserved bit is a refusal** (`LODI_INST_FLAGS_KNOWN` = 0x1FF) |
| 0x16 | 2 | `drawKey` (v2) | the base's (primary mesh, that mesh's first material) rank, §2.1 |

**Cold record — 8 B, parallel to the instance blob:** `u32 refFormId`,
`i16 scolPart` (−1 when not a SCOL part), `u16 identity` (v2).

**Position decode:**

```
x = chunkX·16384 + px/65535·16384
y = chunkY·16384 + py/65535·16384
z = zMin       + pz/65535·zExtent
```

**The cell a placement belongs to is the one the cell-range table STATES, not
one re-derived from the decoded position** (`lodiCellAgrees`,
`LODI_CELL_QUANT_TOL`, `src/lodifile.h`). The writer sorts on the cell of the
FLOAT position; `lodoQuantU16` rounds to nearest, so the stored position can
sit up to half a quantisation step across a cell line from the float the writer
sorted on. The reader therefore accepts a stored cell that differs from the
derived one **by one, on ONE axis, and only while the decoded position lies
within `LODI_CELL_QUANT_TOL` of the line between them**:

```
LODI_CELL_QUANT_TOL = LODI_POS_QUANT_STEP / 2 + 16384 · 2^-22
                    = 16384/65535/2 + 16384 · 2^-22  ≈ 0.12890 u
```

(half a step, plus two float32 ulp of the chunk box for the round trip back
out). Anything else — two cells that are not one-axis neighbours, or a position
past the band — is refused by name with the distance and the band in the
message (`lodiRead`'s instance loop). The Boston instance that sits 2.999985
cells north of its chunk origin (`docs/LODGEN_CENSUS.md` §7) is exactly this
case, and it is why the independent decoder was refusing a file the writer had
written correctly. A consumer that bins placements by cell uses the stated cell,
never `floor(position / 4096)`.

**Measured on the downtown-Boston pair** (lane INCRGATE1, 2026-09-24,
`tests/spells/lodgen_native.sh` leg 13b, region (0,−12)..(11,−1) dim 4, 33,123
placements, 280 occluder boxes). The independent decoder reads the pair,
6 checks, 0 failures. **14** instances sit in the band, worst **0.062501 u** from their
cell line, all in chunk 6, the first at instance 3358. With the band set to 0 the
decoder refuses at that instance by name, which is the leg's red control. The
decoder's band is `step/2 + 16384 · 2^-23` ≈ 0.126955 u, one float ulp
narrower than the reader's constant above. Both hold the measured worst with
room to spare.

#### 4.1a The placed REFR, and why the record did NOT grow to 32 bytes

bungo, 2026-09-11 10:3x: *"the instance table must be joinable to the engine's
placed REFR (formID in the .lodi record)"*.

It already is. `cold[i].refFormId` sits at the instance's **own index**, so the
join is one array read with no search, and it is in the **load-order-mapped ID
space** — `src/esmdata.cpp`'s `ref.formID = r->formID`, the same space
`ESMFile::mapFormID` puts every other id in. A consumer that must suppress the
engine's own draw per object reads the cold blob once at load and builds its map.

Duplicating it into the hot record would have grown the stride from 24 to 32,
which is **+33% on the one buffer the per-frame cull dispatch reads** — 28 KiB
on this region, ~1.2 MiB on a full Commonwealth at the spec's placement count —
to store a number already present at the same index in a blob the draw path never
touches. The stride stayed 24. **This is stated here rather than silently done:
if bungo wants the formID in the hot record anyway it is a stride change and
the reader refuses the 24-byte stride by name.** v3 did NOT take the
opportunity; the question is still open and still his.

Gated: every instance names a REFR, the `(ref, part)` key is unique over the
file, and **the key set equals the stock manifests' set exactly — 3,526 of 3,526,
nothing only in the table and nothing only in the manifests.**

#### 4.1b The per-instance bound radius: the rule, not a stored float

bungo, 2026-09-11 08:0x, item 3: a per-instance bound radius for the screen-size
fade. **`base.boundRadius × scale` satisfies it** and is *more* accurate than a
1-unit u16 while costing zero bytes an instance — one multiply at the point the
runtime already reads both numbers.

Two things make the rule safe, and both are enforced:

* **A `scale` of 0 is a REFUSAL**, naming the instance and its ref. A zero scale
  is a zero radius, and an object with a zero radius fails the screen-size test
  at every distance and is never drawn. The base table's `boundRadius > 0` rule
  is the other half.
* **`maxBoundRadius` is computed from the QUANTISED scale.** Measured on the
  region: the v1 writer used the unquantised scale, and 4 of 3,526 instances then
  had `base.boundRadius × (scale/8192)` sitting up to **0.055 u** outside their
  own chunk's expanded box — a cull that could clip an object the consumer
  expected to keep. `LodiSrcInstance::boundRadius` is now the base radius at
  scale 1 and the writer applies the quantised scale, so the stored value is a
  true upper bound by construction.

#### 4.1c Identity: what is unique, and over what

bungo, 2026-09-11 08:4x, verbatim: *"Our shadow casters are based on the color id
right now I think, so they can only occlude other objects and terrain, never
themselves"* — the far-shadow pass keys on an identity AND excludes
self-shadowing by it, so that word must survive into the native output.

**As of v7 the word it keys on is the GROUP (§4.9), and that is a correction to
what this section said before, made by the director on 2026-09-18 17:4x.** The
exclusion is what forces it. A kit-built house is not one placement: chunk
4.4.-12 holds one that is **205** separate wall, roof, floor and garage
placements. Give each its own caster identity and the pass excludes each piece
only from ITSELF, so the house's own front wall casts a far shadow across its
own roof — which is precisely the artifact the exclusion exists to prevent.
**One house, one SCOL, one tree = one caster.** A group shared by two DIFFERENT
objects is a wrong shadow; pieces of one object sharing a group is the point.

There are now three words, and the difference matters:

* **The caster identity is the GROUP**, `u16 group[i]` (§4.9), dense per chunk.
  It is the only one of the three the far-shadow pass may key on.

* **The per-placement identity is the instance INDEX**, a u32, unique across
  the whole file by construction — and it is **NOT the shadow key**. It is what
  a picker, a manifest join and `check_manifest`'s uniqueness gate need, and v7
  leaves it exactly as it was. It replaces the 16-bit identity
  the stock path smuggles through vertex colour R+G, whose ceiling is 65,536
  placements a chunk and whose measured headroom on the two densest dim-32
  chunks was only 1.54× — it wraps silently.
* **`cold[i].identity` is the STOCK bake's index** for that placement, and it
  is **NOT the shadow key either**: the
  manifest's `index` column, R + G·256 of the `.bto` vertex colour. It is carried
  so a consumer can join our instance to the engine's own far chunk, and it is
  **unique inside the STOCK CHUNK the placement was first drawn in** — not
  inside the `.lodi`'s 16,384-unit bin.

  The two coincide at dim 4 except at a boundary. **Measured: 7 of 3,526
  Sanctuary instances collide inside a `.lodi` chunk**, because a placement whose
  position sits across the chunk line from the chunk that baked it lands in the
  neighbour's bin and meets that chunk's own index. Gated both ways: identity is
  unique inside every stock chunk (0 duplicates over the eight manifests), and
  **it equals the stock manifest's index on all 3,526 rows**.

### 4.7 The placement-AO byte (v5)

bungo, 2026-09-11 15:3x: *"is vertex AO baked into impostors too on top of the
texture AO they hold?"* The honest answer for v3 was **no**. The instance
record's `ao` at 0x10 is the mean over that placement's own LIT CHUNK-MESH
VERTICES, and a card-drawn placement has none — `litVerts == 0` — so it was
written as **255**, fully lit, which is why a tree under a bridge read the same
as a tree in a field.

v5 adds a **second, different quantity**, and keeps them apart on purpose:

| | `ao` (0x10, v1) | `placementAo` (the v5 blob) |
|---|---|---|
| what | mean of the placement's own lit chunk-mesh vertices | ONE ray cast straight up from just above the placement's drawn top |
| against | the lighting solution of the assembled chunk | the assembled chunk's geometry **and the heightfield**, exactly as the chunk's own vertices get theirs |
| a card-drawn placement | has none; written 255 | has one, like every other placement |
| absent | not representable | `0xFF` = **NOT MEASURED**, which is not AO 255 |

**Where it lives, and what was rejected.** The byte is a **parallel u8 blob**,
one entry per instance in instance order, written after every other payload:

* not in the instance record's `flags` — a set reserved bit there is a refusal,
  and 8 bits plus a meaning do not fit in the 10 that are left;
* not in the **cold record** — it is a fixed 8 bytes and the draw path never
  reads it, so growing it is a stride refusal for a table nobody loads;
* not a **per-chunk table** — the whole point is that two copies of one base in
  one chunk differ.

A blob written last means an OFF run produces the same offsets, the same
alignment and the same file length it always did. `--native-no-placement-ao` is
the way back, and on that arm the `.lodi`'s **payload is byte-identical** to the
bake before this lane.

**Not the whole file, and the gate had to say so out loud.** A `.lodi` carries
two DERIVED words — `headerCrc32` at 0x0C and `lodoIdentity` at 0x20 — and
`lodoIdentity` names the companion `.lodo`, which this lane bumps to v4
*unconditionally* (Deviation 14). A way-back `.lodi` therefore CANNOT be
byte-identical to one baked before the lane, and claiming it would be was this
lane's own error. Measured against the rung exe on the Sanctuary region:
**12 of 128,256 bytes differ**, all of them inside those two words, and
`tests/spells/lodgen_lodi_wayback.py` RECOMPUTES both from their own inputs
(crc32 over 0x10…0xFF; FNV-1a 64 over the `.lodo`'s `headerCrc32`,
`modelCorpusHash` and `objectCorpusHash`) so the difference is proved to be a
consequence of the library and not a changed instance. Gated in
`tests/spells/lodgen_ladder.sh` §6, with the comparator shown red first on a
flipped PAYLOAD byte, whose offset it names.

`0xFF` is reserved as NOT MEASURED, so the range a measured byte may take is
**0…0xFE** (`LODI_PLACEMENT_AO_MAX`). A measured byte equal to `0xFF` is a
writer defect and the field gate checks for it by name.

### 4.2 The two u16 refusals

`scale` maxes at **15.99988** (7.99988 before version 10, §4.14) and `baseId` at **65,535**. The measured corpus
reaches 1.9600 and 2,019 on this region and 4.970 / 3,400 worldwide, but a modded
load order is the only environment this ships into. **The writer computes both
maxima over the whole census before it writes a byte and refuses above range,
naming the offending ref formId or base editor ID.** A refusal, never a clamp and
never a drop.

### 4.3 The tree yaw is in the quaternion; `seed` is the hash's low byte

The generator's hash is
`treeHash = (quint32(qRound(pos[0])) * 2654435761U) ^ (quint32(qRound(pos[1])) * 40503U)`;
the stock bake rotates a tree by `treeHash % 360` degrees about Z and mirrors its
U when `(treeHash >> 8) & 1`.

The spec wanted the ESM rotation alone in the record and the yaw re-derived by
the consumer from a u8 `seed`. **That cannot work**: `% 360` needs 9 bits and the
mirror a tenth, and the consumer holds only the QUANTISED position. So, as built:
`rotation` = the **drawn** rotation; `flags` bit0 = the mirror; `seed` =
`treeHash & 0xFF`, a per-instance phase and nothing a consumer must reconstruct
the transform from.

**The ESM cross-check runs for trees.** The independent decoder recomputes
`treeHash` from the plugin's own float `DATA` position and requires the record's
rotation within 0.02° of it: measured worst **0.0046°** over the nine chunks.

### 4.4 Selection is by screen ERROR per cluster, not by ring

`rep[0..3]` are MNAM's own four positional slots. A base with **no** authored LOD
mesh in any slot has `rep[0..3]` all 0xFFFF and **must** have a `cardLayer`.
From lane CARDLINK1 such a base is written when a card array links it, and
left out of the table otherwise (§4.13, deviation 5).

**THE CUT, and it is the whole point of v3.** For a cluster of an instance,

```
screenErrorPx = geometricError × scale × projectionScale / distance
```

and the cluster is drawn when

```
screenErrorPx <= tolerance   AND   parentError × scale × projectionScale / distance > tolerance
```

Exactly one cluster of every leaf's ancestry satisfies that, so the cut is a
partition of the surface — walking up from any level-0 cluster, one group of the
chain is selected and no other. `parentError` is `FLT_MAX` at a root, so a chain
whose every error is under the tolerance terminates at its root rather than
selecting nothing.

`projectionScale` is a **reference constant, not a format constant**:
`960/tan(35°) = 1371.0`, assuming 1920 wide and `fDefaultWorldFOV = 70`; **the
consumer recomputes it from the live projection.** bungo's screen-size spec of
2026-09-11 10:4x states the runtime side in Unity's terms — projected size as a
fraction of SCREEN HEIGHT, one threshold per fade class, ~20% hysteresis — and
that is FO4CS's, not the bake's. **The global pixel tolerance for the cluster cut
defaults to 1 px** (his 10:4x wording).

**The shadow view is a second, coarser selection of the same data.** Its
tolerance is its own; nothing in the file is per-view, so a cluster culled for
the camera is still addressable for the light. bungo's 08:4x ruling makes that
safe: the far-shadow pass keys on the caster's **GROUP** (`.lodi` v7, §4.9 and
§4.1c; the per-placement identity index before v7) and excludes self-shadowing
by it, so a coarser shadow cut has no self-occlusion to get wrong.

The `crossPx16` ladder in the base row is **v1's per-BASE mesh-slot ladder**.
Since v4 only `crossPx16[2]` is left of it (its first four bytes became
`fullTriangles`, §3.2, Deviation 14), and those two words are still written
as 0 (Deviation 5). It is not the cluster cut and does not
interact with it: a consumer picks the mesh slot by base size and then cuts that
mesh's cluster tree by error.

Do not select by ring: the measured 172× spread of bound heights inside one chunk
(47.8 … 8,224.1 u, median 1,086.4) makes a per-chunk distance wrong by two orders
of magnitude for most of its contents.

#### 4.4.1 The cut, measured on the real pair

`tests/spells/lodgen_native_cut.py` is the reference selector: it reads both
files, applies the law above to every one of the 3,526 instances at a fixed
distance, asserts the partition per mesh, and prints the triangle count. The
camera is the centre of the instance cloud; the distance is applied uniformly so
the three rows of a tolerance are comparable.

| tolerance | 2,000 u | 8,000 u | 32,000 u |
|---|---|---|---|
| 0.5 px | 124,205 | 124,181 | 121,898 |
| 1 px | 124,205 | 124,121 | 115,583 |
| 4 px | 124,121 | 115,583 | 104,090 |

Level 0 alone is 142,138 triangles over the library, of which these instances
reach 124,205. **The cut therefore saves between 0 and 16 percent on this
region**, which is §3.5.4's finding expressed as triangles. The two floors are
in the same run: tolerance 0 selects exactly the level-0 clusters, and a
tolerance of 1e12 selects exactly the roots.

### 4.5 The occluder boxes (v3)

bungo, 2026-09-11 08:2x, second round, verbatim: *"2 sounds good"* — precomputed
occluders, *"a few boxes per cell for buildings and hills, baked from the
meshes"*.

#### 4.5.1 The row, and the law that shapes every constant in it

**Occluder entry — 40 B:** `f32 centre[3]` (world), `f32 halfExtent[3]` (along
the box's own axes, after scale), `u16 rot[3]` (the instance's rotation, the same
smallest-three codec as §4.1, so a consumer already has the decoder),
`u16 flags` (bit0 = fitted inside a watertight mesh; bits 1–15 reserved 0, and
bit 0 CLEAR is a refusal — the file must say what rule made the box),
`u32 instanceIndex`, `u16 meshId`, `u16 reserved`.

`meshId` is stated rather than derived from the base's `rep[]`: a placement draws
ONE slot, and only that slot's geometry is what the box was measured against. A
checker that guessed the slot would be testing the wrong mesh.

**Occluder cell-range entry — 8 B:** `u32 occluderFirst`, `u32 occluderCount`,
one per cell, PARALLEL to the cell-range blob and walked in the same order. A
cell's boxes must name instances **of that cell** — the rule with teeth, because
a box is only an occluder by virtue of sitting inside a particular object.

**The rule behind every constant: a box that sticks out of its object hides
things WRONGLY, which is worse than no occluder.** So the fit is deliberately
timid, and each step is a named refusal:

| step | rule | measured, over the 2,982 library meshes |
|---|---|---|
| watertight | `boundarySrc == 0` — ray parity means nothing on an open mesh, and the same boundary count the shadow rule reads serves here | **2,617 refused** |
| big enough | the model's AABB diagonal ≥ 256 units at scale 1 | **100 refused** |
| an interior | the largest all-interior box on a 16³ voxel grid, one ray a (y, z) row | **123 refused (no interior voxel)** |
| a whole voxel shaved off every side | the grid says only that a voxel's CENTRE is interior | — |
| worth a row | the box is at least 2 percent of the AABB's volume | **43 refused (too thin)** |
| **a hundred points** inside the box are inside the MESH | the writer's own gate, the same test the harness runs | **0 refused** |
| | **fitted** | **99** |

The writer additionally pulls the half extents in by **0.1 percent** before
writing, because the consumer builds the box's axes from the QUANTISED rotation
(worst 0.0073°) and a corner may move by that much.

#### 4.5.2 Which boxes survive, and the order

Per cell, the largest by world volume, up to `maxOccludersPerCell` = 4; ties
break on the instance index, which is not decoration — it is what makes two
writes of one set byte-identical when two boxes measure the same volume.

#### 4.5.3 Measured, and the honest half

**The nine-chunk Sanctuary region writes ZERO occluder boxes, and that is
correct.** It draws **41 distinct LOD meshes and not one of them is watertight**;
the 365 watertight meshes in the library are elsewhere in the Commonwealth. A
census that printed only "0" would read as a defect, which is why the row names
every refusal separately.

A second small region — cells (0, −12)…(11, −1), downtown Boston — is where the
rule has something to say, and it is the region the gate uses for the box
checks:

| reading | number |
|---|---|
| boxes written | **280** |
| boxes offered by instances | 599 |
| dropped by the 4-a-cell cap | 319 |
| cells with at least one box | **87 of 147 populated (59.2 percent)** |
| cells with none | 60 |
| instances in the region | 33,123 |

---

### 4.6 The aggregate ring-3 impostors (v4)

bungo, 2026-09-11 08:2x → 08:3x, verbatim: *"At ring 3 the bake takes each
cell's trees, places their cards with the same rotation and mirror the
repetition breaking would give them, photographs the whole cluster from the
horizon views, and writes one aggregate sheet per cell. The ring 3 instance list
then holds one placement per cell instead of one per tree."* — *"1 sounds
good"*.

#### 4.6.1 The version is CONDITIONAL, and that is a deviation stated out loud

A bake with **no** aggregate writes **version 3**, byte for byte what this
writer wrote before the module existed. A bake **with** aggregates writes
**version 4**. The reader accepts 3 and 4 and refuses 1 and 2 by name.

That is not the licence v3 refused v2 under, and the difference is the point: a
v3 file read by a v4 reader is UNAMBIGUOUS — zero aggregates, both new offsets
0, and the header bytes from 0xB0 are the zero pad the v3 writer already wrote.
v2 read as v3 was not: it said "this worldspace occludes nothing" and meant
"this file predates occluders". The condition exists because aggregation is a
MODULE and CONSTITUTION 10 requires its off value to be the exact way back;
making the version unconditional would have made `--aggregate` off a different
file from the bake before it, and there would then be nothing to measure byte
identity against.

**Deviation 12** in §11 records it.

#### 4.6.2 The header words (v4), at the room §12 named

| off | type | field |
|---|---|---|
| **0xB0** | **u64** | **offset: aggregate table (48 B stride)** |
| **0xB8** | **u64** | **offset: covered-instance blob (4 B per entry)** |
| **0xC0** | **u32** | **`aggregateCount`** — 0 is a refusal on a v4 file: a file with no aggregate is a v3 file |
| **0xC4** | **u32** | **`coveredCount`** — the blob's length in u32s |
| **0xC8** | **u16** | **`aggregateStride` = 48**; any other value is refused by name |
| **0xCA** | **u16** | **`aggregateViews`** — azimuths a sheet, a FILE constant; a row that disagrees is refused |
| **0xCC** | **f32** | **`aggSwitchPx`** — the cross-fade threshold, §4.6.5 |
| **0xD0** | **f32** | **`aggBandRatio`** — the band's width as a multiple of the threshold; 1.0 or below is refused |
| 0xD4…0xFF | — | reserved, zero **on a v4 file**. On a v3 file the pad still starts at 0xB0, and the same sweep is what refuses a v3 file carrying an aggregate table |

`indexCrc32` covers, in file order: the chunk table, the cell ranges, the
occluder table, the occluder ranges, **the aggregate table and the covered
blob**. An empty pair folds zero bytes in, which is why a v3 file's CRC is
exactly where it was.

The two payloads are written LAST and only when there is something to write, so
a v3 file's byte layout is untouched.

#### 4.6.3 The aggregate row — 48 B

| off | type | field |
|---|---|---|
| 0x00 | f32[3] | `centre` — the card's centre in WORLD units: the cell's centre in X and Y, the cluster's mid-height in Z |
| 0x0C | f32[2] | `half` — the quad's half extents along the view's own right and up, world units. ONE pair for every view |
| 0x14 | f32 | `depthSpan` — `units = (B − 0.5) × depthSpan`, the card law; **not positive is a refusal** |
| 0x18 | f32 | `boundRadius` — the tree cloud's radius, for the projected-size test; **not positive is a refusal** |
| 0x1C | i16[2] | `cellX`, `cellY` |
| 0x20 | u16 | `views` — must equal the header's `aggregateViews` |
| 0x22 | u16 | `flags` — bit0 `HEIGHT` (**clear is a refusal**), bit1 `MIRRORED` (at least one source tree was composited mirrored); bits 2–15 reserved, and a set reserved bit is a refusal |
| 0x24 | u32 | `identity` — **`0x80000000 \| aggregateIndex`, always**; any other value is refused by name |
| 0x28 | u32 | `coveredFirst` — into the covered blob |
| 0x2C | u32 | `coveredCount` — how many instances this aggregate stands for; **0 is a refusal** |

**There is no `.lodm` path in the row, deliberately.** The sheets live at
`Data\Textures\Lodgen\Aggregate\<EDID>\<cellX>_<cellY>_agg.*`, derived from the
header's worldspace and this row's own cell, so a reader cannot be handed a path
that disagrees with the cell (zero-authoring).

Rows are in **north-up cell order**, the same order as everything else in the
file, and a repeated cell is refused. Two writes of one set are byte-identical.

#### 4.6.4 The covered blob, and why the instances are NOT removed

bungo's words are *"the ring 3 instance list then holds one placement per cell
instead of one per tree"*. **This file has no ring-3 instance list.** It has ONE
instance table and a 4-cell chunk directory, and selection is by projected size
(§4.4) — that is the whole premise of v3. So the aggregate cannot remove
anything; it SUPPRESSES, and the covered blob is what says which.

Per aggregate, `coveredCount` u32s at `coveredFirst`, **ascending** so a
consumer can bisect, each an index into the instance blob. The rules, all
enforced by the reader:

* the aggregates **partition** the blob in order — `coveredFirst` must be the
  running sum;
* no instance is covered **twice** — a doubly covered instance would be
  suppressed twice and the count identity would be measuring a number nothing
  else agrees with;
* **every covered instance stands in the aggregate's OWN cell.** This is the
  rule with teeth: a covered instance somewhere else is a forest being hidden by
  a card that does not draw it.

A runtime reads the blob once at load and marks those instances "aggregated";
past the switch distance it draws the cell's card instead of them, and across
the band it draws both and dithers between them.

**The count identity.** `coveredCount` for a cell equals the `trees` key of that
cell's `.lodm` (§3a of `docs/LODGEN_LODM_FORMAT.md`) equals the number the bake
photographed. Three statements of one number in two files and a log line, so any
one of them can check the others; the bake prints
`count identity photographed N == file covered M == AGREE|DISAGREE` and the gate
reads it back from the bytes.

#### 4.6.5 The cross-fade band, as a projected size and not a distance

No bake can state a ring-3 distance, for the same reason §4.4 gives: a ring is
camera-relative and this file has none. The band is therefore stated the way
bungo's screen-size spec of 10:4x states everything else — as a **projected
size with a hysteresis**:

```
the aggregate is selected when the CELL's projected width falls to
aggSwitchPx, and the per-tree cards cross-fade out over
aggSwitchPx … aggBandRatio × aggSwitchPx.
```

Defaults **96 px** (three quarters of a 128-px frame, the point past which a
sheet can no longer add detail) and **1.2** (his ~20 percent hysteresis).

At the reference projection of §4.4 (`projectionScale` 1371.0) a 4,096-unit cell
is 96 px wide at **58,500 units** and 115 px wide at **48,800 units**, so the
band is about **48,800 … 58,500 units, 2.4 cells wide**, and the aggregate takes
over about **14 cells out**. Those three numbers are the reference READING of
the rule, not the rule: a consumer recomputes them from its live projection.

#### 4.6.6 The identity law (bungo's 08:4x far-shadow ruling)

**One identity per aggregate**, never the dominant tree's, and the space is
disjoint from the instance indices by construction (`0x80000000 | index`).

**v7 does NOT fold this into the group table (§4.9), and that is a decision, not
an omission.** The group table exists to say that many placements are one
caster; an aggregate is already ONE row that is already one caster, so there is
nothing for it to merge with and a group id per aggregate would be a table whose
every entry is a singleton. The two spaces stay separate and stay disjoint: the
group is a `u16` dense per chunk over the INSTANCE table, the aggregate identity
is a `u32` with the top bit set, and a consumer reads the caster identity from
whichever table it drew the caster out of. There is also no bake behind a change
here — the chunk this lane measured carries `aggregateCount` **0** — and a rule
no refuter on this lane can turn red is a rule that ships unproven.

The identity index is what the far-shadow pass keys on so a caster never shadows
itself. Once a cell's trees are one card they ARE one caster; giving the
aggregate its dominant tree's identity would make it share an identity with that
same tree's own per-tree instances, which are still drawn at the nearer rings,
and the shadow pass would exclude the wrong pixels. The top bit says which space
a consumer is holding, so an aggregate identity and an instance index can never
be confused.

#### 4.6.7 Measured, on the 9-chunk Sanctuary region

| reading | number |
|---|---|
| cells holding at least one tree | 105 |
| cells forested at the default threshold of 8 | 97 |
| aggregates written | **97** |
| trees photographed into them | **3,414** |
| trees refused, their base having no card set in the bake tree | 9 |
| trees refused, their card baked through a perspective camera | 0 |
| aggregate rows in the `.lodi` | 97 |
| covered-instance entries | 3,414 |
| `.lodi` version written | **4** |

The ESM census taken before any of this was built (`scratchpad/cards_agg_20260911`)
reads **97 forested cells holding 3,423 trees** on the same region from the
plugin alone: 3,414 photographed + 9 refused = 3,423 exactly, from two
instruments that share no code.



### Provenance of §4.6 (lane CARDS-AGG, 2026-09-11)

Source hashes taken FIRST, every line number below found again from its own
anchor text (each matched exactly once), the version constants re-read last
(`ww-contract-provenance`).

| file | sha256 (16) | lines |
|---|---|---|
| `src/lodifile.h` | `78f94b0162a68a1e` | 494 |
| `src/lodifile.cpp` | `996d02e8f7ea8214` | 1,492 |
| `src/lodgenaggregate.h` | `b24833822dad3567` | 159 |
| `src/lodgenaggregate.cpp` | `e31dd7b97fac4c4d` | 690 |
| `src/nativeemit.cpp` | `63d5f8a87ba6c73b` | 1,351 |

| claim | line | anchor |
|---|---|---|
| the aggregate row is 48 bytes | `lodifile.h:130` | `constexpr quint16 LODI_AGGREGATE_STRIDE = 48;` |
| the row's own layout | `lodifile.h:264` | `struct LodiAggregate` |
| the switch threshold's default, 96 reference pixels | `lodifile.h:139` | `constexpr float LODI_AGG_SWITCH_PX = 96.0f;` |
| the aggregate identity space is the top bit | `lodifile.h:143` | `constexpr quint32 LODI_AGG_IDENTITY_BIT = 0x80000000U;` |
| one identity per aggregate, and it is the row's index | `lodifile.cpp:512` | `row.identity = LODI_AGG_IDENTITY_BIT \| quint32( ai );` |
| **the version is CONDITIONAL** (Deviation 12) | `lodifile.cpp:555` | `h.version = aggs.empty() ? LODI_VERSION : LODI_VERSION_AGGREGATE;` |
| the two new payloads join `indexCrc32`, after the occluders | `lodifile.cpp:591` | `h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( aggs.data() )` |
| the header pad starts at 0xD4 on a v4 file and 0xB0 on a v3 one | `lodifile.cpp:764` | `const int padFrom = ( h.version == LODI_VERSION_AGGREGATE ) ? H_RESERVED_D4 : H_RESERVED_B0;` |
| the view basis: right = up x eye | `lodgenaggregate.cpp:285` | `right[v][0] = -eye[v][1]; right[v][1] = eye[v][0]; right[v][2] = 0.0f;` |
| the gap law, unchanged from the card sheets | `lodgenaggregate.cpp:42` | `int gapOf( int side )` |
| the area weight (§10.4 of the card sheets) | `lodgenaggregate.cpp:469` | `const float wgt = texelArea > 0.0f ? sampleArea / texelArea : 1.0f;` |
| the height channel re-encoded through the aggregate's own span | `lodgenaggregate.cpp:585` | `const float behind = -( D.depth * inv );` |
| the `.lodm` says the identity law in words | `lodgenaggregate.cpp:667` | `a.insert( QStringLiteral( "identity" ), QStringLiteral( "per-aggregate" ) );` |

---

### 4.8 The vertex-AO stream (v6)

bungo, 2026-09-18 04:0x, on the hotfix-6 AO-only picture: *"The AO is broken, there
is no vertex AO on the objects."* / *"The AO on the objects was from the objects
themselves, from objects amongst each other, and with the objects and terrain and
with objects on nearby chunks too."* The `.lodo` `selfAO` byte (§3) is a cast
against the model's OWN triangles, in model space, once per library vertex; it
cannot know what stands beside a placement. The `.BTO` route's colour-B AO was a
SCENE cast per chunk-mesh vertex. v6 carries that cast for the native files.

**Layout.** `offVertexAo` (0xF4) points at `u32 first[instanceCount + 1]`, in
instance (sorted table) order, followed by the bytes; `vertexAoBytes` (0xFC) is
the whole stream, offsets included. Instance *i* owns bytes `first[i]` …
`first[i+1]`, ONE A VERTEX of the mesh it draws — `bases[baseId].rep[mnamSlot]` —
in that mesh's vertex order (the union of its clusters' vertex ranges, which the
writer checks is contiguous; index = library vertex index − the mesh's first).
`first[0] == 0`, the run is monotone, and `first[n] == vertexAoBytes − 4 (n + 1)`;
anything else is refused by name. An EMPTY slice means the bake had no cast for
the placement: card-drawn, no mesh in the slot, or a chunk with no land cell to
build a heightfield from. 255 = open, 0 = fully occluded.

**The cast.** At finalize, every library mesh is decoded once (level-0 clusters).
Instances are grouped by chunk; per chunk a `LodgenAoScene` (`src/lodgenao.h`,
the one caster) is built from the ESM heightfield of the chunk plus ONE CHUNK of
apron on every side (`EsmWorld::land`, 128-unit spacing), and the triangles of
EVERY placement whose origin lies in that field — so the neighbouring chunks'
objects shade this chunk's edge, as the `.BTO` skirt placements did. The chunk's
own placements then cast `ambientOcclusion(pos, nrm, 300 × dim)` per vertex, the
reach the `.BTO` route used. Chunks run in parallel.

**Consumer.** When the slice length equals the drawn mesh's vertex count, use the
byte ALONE — it already holds what `selfAO` and the v5 placement byte
approximated; multiplying either in darkens twice. Otherwise fall back to
`selfAO × placementAo` (v5) or `selfAO` (v4). The NifSkope viewer does exactly
this under `WW_LODL_AO=1` (`src/lodinative.cpp`).

**Way back.** `--native-no-vertex-ao` writes a v5 file, byte-identical to hotfix 6
(the stream is written last; no other offset moves). v6 requires the v5 blob.

**Known.** The caster fires 8 rays, so a byte is one of 9 values (255, 228, …
38); colour B had the same steps. A finer ladder is a caster change, not a
format change. Urban region 0 -12 11 -1: 33,123 placements, 490,600 bytes, mean
174.3 against `selfAO` 242.5 over the same vertices.

### 4.9 The group table (v7)

bungo, 2026-09-18 09:4x: *"The houses should be one object each though, for
identity"*. `identity` (§4.1c) is unique per placement, which is what a picker
needs and the opposite of what an eye needs: a kit-built house is 205 separate
wall, roof and floor placements and the identity view paints it 205 colours.
v7 carries a SECOND word beside it.

**THE GROUP IS THE IDENTITY THE FAR-SHADOW PASS KEYS ON** (director, 2026-09-18
17:4x; §4.1c). That pass excludes self-shadowing by identity — bungo,
2026-09-11 08:4x: *"they can only occlude other objects and terrain, never
themselves"* — so the unit the identity names has to be the unit that must not
shadow itself. **One house, one SCOL, one tree = one caster.** A house split
into forty identities would shadow its own walls; that is the artifact the rule
below exists to prevent, and it is why a collision between two different groups
is a wrong shadow while forty pieces sharing one group is correct. The
per-placement instance index and `cold[i].identity` stay exactly as they were,
for the manifests and the stock join, and neither is the shadow key.

**`identity` is not touched.** It stays unique, `check_manifest`'s uniqueness
gate passes unchanged, and the group is a parallel table. The viewer's
`identity` channel now DRAWS the group and the new name `placement` draws what
`identity` drew before (§8).

**Layout.** `offGroup` (0x100) points at `u16 group[instanceCount]`, in instance
(sorted table) order — one word a placement, stride 2 (`groupStride`, 0x10C).
Ids are **dense per CHUNK from 0**: a chunk holding *C* groups uses exactly
{0 … C−1}, and a reader that finds a hole, or an id at or past the chunk's own
placement count, refuses by name. `groupCount` (0x108) is the chunks' counts
SUMMED, and the reader adds them up itself rather than trusting the word. Per
chunk and not globally, because a full Commonwealth can hold more than 65,536
groups and the word is a u16; the writer refuses by name if one chunk ever does.

**Who assigns what.** The emitter (`src/nativeemit.cpp`) computes a global u32
`groupKey` per placement, with the sentinel `LODI_GROUP_ALONE` for a placement
that ended up by itself — "alone" is a stated state, not a coincidence of
numbering. The WRITER turns those keys into dense per-chunk ids, because only
the writer knows the sort and the chunk partition. **A component cut by a chunk
border becomes two groups, one a side**, which is the same rule the rest of the
format lives under.

**THE DEFAULT RULE: THE PROXIMITY JOIN (bungo's ruling 2026-09-19; lane
IDENTPROX measured it, lane HORIZONOUT shipped it).** Three clauses, in order:

1. a SCOL part's group is its SCOL reference's group;
2. every placement that is **not a tree** and **has a drawn LOD mesh** joins a
   connected component with every other one whose **MESH** comes within
   `--identity-join-gap` world units — **64 by default**
   (`src/nifcli.cpp`, `float lgIdentityJoinGap = 64.0f;`). The distance is
   mesh to mesh, not box to box: the minimum distance between two sample sets,
   each the placed level-0 LOD vertices plus every level-0 triangle's three
   edge midpoints and its centroid. It never reports less than the true
   surface distance, so a pair it joins is genuinely within the gap;
3. trees and card-only placements stay singletons.

The measure changed, not only the number, because the box rule below at 16 u
already welds 286 placements across 9 Creation Kit layers into one
18,121-unit identity: the elevated highway deck's axis-aligned box hangs over
four South Boston city blocks, and no gap fixes that (`src/nativeemit.cpp`,
the `(ii) THE JOIN` comment). 128 u is the last gap at which no identity holds
two different reference buildings. **Chunk 4.4.-12: 167 groups at the default
against 588 under the legacy rule** (read from the two `.lodi` headers' 0x108
word, `scratchpad/horizonout_20260919/join/{prox,legacy}`); 2,387 grouped,
largest 206, 62 singletons. `--identity-join proximity` says the default out
loud; `--identity-join legacy` is the way back.

**THE LEGACY RULE (`--identity-join legacy`, the shipped rule until
2026-09-19, and the gate's red control: it must reproduce 588 groups on chunk
4.4.-12).** Three clauses, in order:

1. a SCOL part's group is its SCOL reference's group;
2. a placement whose BASE model path has an `architecture` component joins a
   connected component over world boxes that overlap or touch within 16 units;
3. everything else is its own group.

The box is the DRAWN mesh's local AABB (`LodoMesh.aabbMin`/`aabbExtent`), its
eight corners placed by the instance's rotation, scale and position and then
re-bounded axis-aligned in world. Not the base's bound SPHERE: `LodoBase`
carries only `boundRadius`, and a sphere of that radius around a long wall's
centre reaches across the street. **Measured on chunk 4.4.-12: the sphere puts
1,526 of 2,449 placements into ONE id; the box's largest group is 205.** The
pair search runs over a 1024-unit spatial hash, which is an accelerator only —
baking the same chunk at 256 and at 4096 units produces the BYTE-IDENTICAL
`.lodi`, and that is a gate (`tests/spells/lodi_v7.sh`).

**THE PATH TEST IS ON THE BASE'S SOURCE MODEL, and that was forced by
measurement.** Asking whether the model path *starts with* `architecture\`
catches **0 of 2,449** placements, and asking whether the DRAWN model's path has
an `architecture` component catches only **238** — because the drawn model of a
far placement is the authored LOD and Bethesda files those by NEIGHBOURHOOD, not
by kind (`LOD\Neighborhoods\Cambridge\Cambridge10_Bld01LOD.nif`). Exactly 1 of
the 314 LOD-rooted paths in this `.lodo` carries an `architecture` component.
The base's SOURCE path does carry the kind
(`Architecture\Buildings\BldgBrick7Story3x5FreeComEntA.nif`) and catches
**1,877 of 2,449** — and it is the string the `.lodo` SHIPS
(`bases[].modelStringOffset`), so a refuter reading the file judges the identical
bytes the rule judged rather than a paraphrase of them.

**The knobs** are in one `GroupKnobs` struct above the emit function, and each
is readable from the environment — `WW_LODI_GROUP_COMPONENT`,
`WW_LODI_GROUP_TOLERANCE`, `WW_LODI_GROUP_SHAPE` (`box`/`sphere`),
`WW_LODI_GROUP_GRID` — as a MEASURING surface, so the table below comes from
bakes of the shipped code rather than from a re-implementation that could be
wrong in its own way. An unset variable changes nothing.

| setting | groups | grouped | largest | singleton |
|---|---|---|---|---|
| **legacy (shipped until 2026-09-19): box, 16 u, grid 1024** | **588** | **1,981** | **205** | **468** |
| tolerance 0 u | 713 | 1,914 | 126 | 535 |
| tolerance 4 u | 632 | 1,960 | 164 | 489 |
| tolerance 64 u | 561 | 1,989 | 208 | 460 |
| tolerance 256 u | 527 | 1,997 | 288 | 452 |
| bound SPHERE, 16 u | 505 | 2,000 | **1,526** | 449 |
| grid 256 u | 588 | 1,981 | 205 | 468 (byte-identical file) |
| grid 4096 u | 588 | 1,981 | 205 | 468 (byte-identical file) |
| component `buildings` | 809 | 1,752 | 168 | 697 |

**What the tolerance cannot do.** At a tolerance of ZERO the largest group is
still 126 placements, so the big components are not an artefact of the 16-unit
slack: Bethesda's row houses physically abut, and a rule over geometry cannot
split a terrace that shares a wall. Splitting one would need the reference or
the base, not the boxes. That is the open question for the ruling, not a defect
in the table.

**Census.** `groups`, `groupedPlacements`, `largestGroup`, `singletonGroups`.
Chunk 4.4.-12 under the LEGACY rule (the default gives 167, above): 588 groups
over 2,449 placements, 1,981 grouped, largest 205
(a single kit-built house of 205 distinct refs — `DecoMainA1x1Wall01` ×43,
`DecoRoof1x1Str01` ×24, garage floors — spanning 0.7 × 0.4 of a cell), 468
singletons, 1,877 architecture placements.

**Way back.** `--lodi-v6` writes a v6 file with no group table, byte-identical
to what the writer wrote before v7 existed.

### 4.10 The per-vertex sky stream (v7)

bungo, 2026-09-18 09:4x: *"We need per vertex sky visbility too"*. §4.1's `sky`
is ONE byte a placement; a building's base stands in shadow while its roof sees
the whole sky, and one byte cannot say both.

**Layout mirrors §4.8 exactly.** `offVertexSky` (0x110) points at
`u32 first[instanceCount + 1]` in instance order, then one byte a library vertex
of the mesh the placement draws, in that mesh's vertex order;
`vertexSkyBytes` (0x118) is the whole stream, offsets included. `first[0] == 0`,
monotone, `first[n] == vertexSkyBytes − 4 (n + 1)`. **The sky stream and the AO
stream are ONE vertex population**: a placement whose two slice lengths disagree
is refused by name, by both readers.

**The cast** rides the same `place`/`perVertex` loop the v6 scene AO uses, in the
same `LodgenAoScene`: `skyVisibility(p, 300)` — 9 rays, normal-independent,
upper hemisphere, 2-unit Z offset. Same scene, same reach, same parallelism.

**How it compares with the 0x11 byte, and the honest half.** On chunk 4.4.-12,
2,449 placements:

| | median | within 2 | within 4 | within 8 | within 16 | pearson r |
|---|---|---|---|---|---|---|
| **sky** stream vs `sky` byte | 0.38 | 72.4% | 77.9% | 87.8% | 94.1% | **0.9854** |
| **AO** stream vs `ao` byte (same file, same placements) | 0.25 | 91.6% | 93.6% | 96.0% | 97.2% | 0.9878 |

The two streams track their bytes equally well — r = 0.985 against 0.988, where
the same means SHUFFLED score 0.019 — but only 72% of sky's per-placement means
land within 2 where AO manages 92%. **The reason is the two vertex populations,
and it was measured rather than assumed.** The byte is a mean over the stock
`.BTO` chunk mesh's vertices for that placement (`lodgenNativeLighting`,
`src/lodgen.cpp`); the stream is a mean over the authored LOD mesh's vertices.
Sky varies enormously ACROSS one object where AO does not, so the same
population difference moves a sky mean much further: agreement falls monotonically
with how much the slice itself spans — 90.7% within 2 where the slice spans ≤ 8,
74.5% at 16…64, 68.8% at 128…256. Two checks rule the alternatives out: the
disagreement is two-sided (53% high, 47% low, mean signed +1.09), so it is not a
sparser scene; and it is flat in placement size and vertex count. The chunk
border costs both streams on top of that (AO 62%, sky 46% within 2 inside 512
units of an edge, against 99.7% and 74.4% beyond 6,000) for the reason §4.8's
scene note already gives.

**The gate is therefore set on the median, the correlation and the flat-slice
subset** — where the two populations CANNOT disagree — and not on a 95% bar the
mechanism says is unreachable. `tests/spells/lodi_v7.sh` G3.

**Census.** `vertexSkyBytes`, `vertexSkyPlacements`, and the mean. Chunk
4.4.-12: 2,449 placements streamed, 53,396 bytes, mean 119.2, 25,098 vertices at
or above 128.

**Way back.** `--lodi-v6` writes a v6 file with no sky stream.

### 4.11 The per-vertex horizon stream (v8) -- RETIRED, reader-only

**THE BAKED HORIZON: WHAT WAS TRIED, AND WHY IT IS GONE** (lanes HORIZON1-3,
2026-09-18/19). Three lanes baked the horizon so a far shadow could be LOOKED UP
instead of cast: a per-vertex object horizon stream in the `.lodi` (version 8)
and a role-7 terrain horizon sheet in the `.lodt`. bungo ruled it out on
2026-09-19, after the first perspective picture of it against a ray-cast sun:
*"As you can see, the end result is terrible"*, *"So, for now, we revert back to
identity data per LOD object from the preauthored LODs"*, *"So yeah, horizon
goes bye bye now, we're back to identity"*. The two numbers behind the ruling:
at a low sun the baked object horizons disagreed with a ray-cast sun on **50-58 %**
of object pixels (lane SUNSIM1), while the identity far shadow map simulated at
64 u disagreed on about **9 %** (lane HORIZON4). **The route is not lost**:
`release/NifSkope.before_horizonout.exe` still bakes both streams. The shipped
exe writes neither, and both readers still open a file that carries one.

**A DEFAULT BAKE WRITES VERSION 7.** There is no switch that turns the stream
back on: `--horizon-azimuths`, `--horizon-near-skip`, `--horizon-subdivide`,
`--horizon-refute`, `--vt-horizon-texel` and `--no-terrain-horizon` are gone
from the parser and an unknown one fails by name like any other.

**What a reader must still know**, because v8 files exist. The header words
survive read-only at their v8 offsets: `offVertexHorizon` (0x11C),
`vertexHorizonBytes` (0x124), `horizonAzimuths` (0x128, the stride: bytes a
vertex), `horizonSteps` (0x12A) and `horizonReach` (0x12C, an `f32`). The
stream itself is `u32 first[instanceCount + 1]` then `A` bytes a library vertex,
one byte a bin, the bin's skyline elevation at `0.3529°` a step, `0` meaning
nothing above the horizontal. The reader validates it exactly like every other
payload -- alignment, table order, length against `fileBytes`, zero pad, and its
share of `indexCrc32` (`src/lodifile.cpp:1292`, `:1335`) -- and then does **not**
read it into the table (`src/lodifile.h:692`). A v8 file whose `0x11C` is zero is
still refused **by name**, because the stream is what version 8 IS.

**Proof it opens** (lane HORIZONOUT, 2026-09-19): `--native-verify` on lane
HORIZON1's own v8 bake returns rc 0 and prints `lodi version 8`,
`offVertexHorizon 245760`, `vertexHorizonBytes 864136`, `horizonAzimuths 16`,
`horizonSteps 22`, `horizonReach 127561.0`.

#### Way back to the data itself

`release/NifSkope.before_horizonout.exe`, with the switches above. Its bake of
chunk 4.4.-12 at `--lodi-v7 --no-terrain-horizon` is **byte-identical** to the
shipped exe's bake at `--identity-join legacy`: 132,690,554 bytes of `.lodo` and
3,109,505 of `.lodi`, `cmp` silent on both. Removing the route moved no other
byte.


### 4.12 The workshop-scrappable bit (`.lodi` v9, lane HORIZON3, kept by
lane HORIZONOUT 2026-09-19)

**This is the one piece of lane HORIZON3 that survived the ruling.** It answers
a need of its own -- a settlement the player has scrapped is gone from the save,
whatever draws it -- and it never depended on the baked horizon. **NOT FLOWN IN
GAME**; it is baked, read back by `tests/spells/lodgen_scrappable.sh` and
photographed in the viewer's `scrappable` channel, and no save has been loaded
over it.

A placement the player can scrap at a workshop is a placement that **will not be
there** — and a far field that keeps drawing it, and keeps casting its baked
shadow, is wrong about a settlement from the first hour of a save onwards. Bit 6
of the instance flags word at 0x14 says which placements those are.

**It costs no bytes.** The flags word has been in the 24-byte instance record
since v1 and bit 6 was reserved-zero; a v9 file is a **v7** file with one more
bit meaningful, and the version word is the only thing that moved. The version has
to move all the same: below v9 that bit is reserved-zero and a reader is
entitled to refuse it, which is what this one does, by name.

**The rule, three clauses, each read out of the plugin and none from memory**
(`EsmScrapIndex`, `src/esmdata.h`; ported from
`scratchpad/horizon3_20260919/scrap_rule.py`):

1. the placement's BASE is the `CNAM` of a `COBJ` whose `FNAM` category array
   contains `00106D8F WorkshopRecipeFilterScrap`. 87 of those CNAM targets are
   FormLists rather than bases — a scrap recipe may name a whole family at once
   — so `FLST` members are expanded transitively; **AND**
2. the placement's position lies inside at least one **build area**: an `XPRM`
   Box primitive on a REFR that links, by `000B91E6 WorkshopLinkedPrimitive`, to
   a workshop workbench REFR. The box is rotated by its own REFR's `DATA`
   rotation, and only about Z — every one of the 111 build areas in
   `Fallout4.esm` has X and Y within float noise of zero, and a plugin that ever
   pitches one is counted as `tilted` rather than silently mis-tested; **AND**
3. the placement's base does NOT carry `001CC46A UnscrappableObject`. This
   removes nothing on `Fallout4.esm` — 149 bases carry the keyword, 2 of them are
   in clause 1, and neither of those 2 stands in a build area — and it is in the
   rule anyway, because a DLC or a mod that sets it means it.

One clause is a **heuristic and is labelled as one**: a workshop workbench is
identified by an editor id containing both `workshop` and `workbench`. There is
no keyword on the bench side to read instead — the link goes from the primitive
to the bench, not back.

**On the measured urban region this is 14 placements of 33,123 (0.04 %).** That
number is the gate.

**The knob.** `--scrappable`, and it ships **OFF** — bungo's standing
rule of 2026-09-12 that every master ships off, and of 2026-09-17 that an owed
ruling never ships as a default. Off writes no bit, leaves the version at 7, and
the file is byte for byte the v7 file. This is a **deviation from the lane
report's §9 draft**, which said the bit "is not a knob — it is always written
from v9 on"; the standing rule outranks the draft, and the plugin walk the rule
needs is not something every bake should pay for.

**The guard in the writer, and it is why the clause is not two lines.** Version
9 is the v7 layout plus bit 6, and a v7 header is 512 bytes. A bake run with
`--lodi-v6` has a 256-byte header, so writing 9 on it would claim a layout the
file does not have.
On such a file the bit is DROPPED rather than written into a version that would
be a lie — and dropped visibly, because the census then reads 0.

**Census**, on its own `native-scrappable:` prefix, ending with the word the
gate greps for: `scrappablePlacements`.

### 4.14 The wide-scale bit (`.lodi` v10, lane BAKE2, 2026-09-25; the director's ruling (a))

**Why.** The instance record stores its scale as `u16 / 8192`, so nothing above
65535/8192 = **7.99988** fits, and the writer refused the whole file on such a ref
(§4.2). The engine and the CK allow a reference scale up to **10.0**. Nuka-World
places four LOD-carrying cliffs above the old line (0604D45A 9.97, 0604D45D 8.33,
0604DDA1 9.23, 0604DDB9 8.33 in his load order), so its `.lodi` could not be
written at all. Dropping them was ruled out: the distant view shows the game's
own data.

**The rule.** Instance flag **bit 7, `LODI_INST_SCALE_WIDE` (0x80)**:

| bit 7 | `scale` means | range | step |
|---|---|---|---|
| clear | `v / 8192` (every version) | 0 … 7.99988 | 1/8192 |
| set (v10 only) | `8 + v / 8192` | 8 … 15.99988 | 1/8192 |

`lodiScaleWord()` / `lodiScaleValue()` / `lodiScaleQuantised()` in
`src/lodifile.h` are the one encoder and the one decoder. At or below 7.99988 the
word is `lround(s × 8192)` clamped to u16, the exact arithmetic of every earlier
version, and the bit is clear: **no instance anywhere is coarser than before**.
Above **15.99988** the writer still REFUSES (never clamps), naming the ref; 16 is
60 percent headroom over the engine's 10. A caller that sets bit 7 itself is
refused: the writer alone decides it from the scale.

**The version moves only when it must.** Like v9 (§4.12), the version word is the
only thing that tells a reader which flag bits may appear. It rises to **10 only
when some instance carries bit 7**. A file whose scales all fit is the v7 (or v9)
file this writer always wrote, **byte for byte**, version word included, so every
`.lodi` already installed stays valid and readers keep accepting 7 and 9.
Version 10 is the **v9 layout** (bit 6 keeps its meaning; 512-byte header, no new
table, no header word). A `--lodi-v6` bake that meets a wide scale is REFUSED:
no pre-v7 version can say the bit, and dropping it would draw the object at an
eighth of the scale it should have or worse.

**Readers.** `lodiRead`: bit 7 below version 10 is refused by name; a stored 0 is
refused only without bit 7 (a wide 0 is 8.0). `lodinative.cpp` decodes the scale
through `lodiScaleValue`. The independent decoder
(`tests/spells/lodgen_native_decode.py`) and the fields spell (`j0`, `j0b`: bit 7
appears exactly when the version is 10) read both. **The FO4CS reader owes the
same decode** (version 10 accepted, bit 7 = +8).

**Census**, on its own `native-wide-scale:` prefix: placements above the line,
of the total, the max scale and the `.lodi` version written.

### 4.15 The Initially-Disabled bit (`.lodi` v11, lane NEAR1, 2026-09-26)

Instance flag **bit 8, `LODI_INST_INITIALLY_DISABLED` (0x100)**: the placement's
REFR carries record flag 0x800, so the engine does not draw it until a script
enables it, and a consumer keeps it hidden until told otherwise. Like v9 and v10
there is no table and no header word: **v11 is the v10 layout plus bit 8**, and
the version rises to 11 **only when an instance carries the bit** (the Sanctuary
test region has none and wrote 9). The far field drops initially-disabled
references before the writer, so **no far file can carry it**; only the near
library keeps them (50 on the whole Commonwealth). v11 implies v7's 512-byte header
block, so a `--lodi-v6` set carrying the bit is refused (dropping it would draw a
hidden object). Readers: bit 8 below version 11 is refused by name
(`tests/spells/near_format_selftest.py` relabels a v11 file 10 and requires it).
**The FO4CS reader owes the same** (version 11 accepted, bit 8 = hidden).

### 4.13 The card link (lane CARDLINK1, 2026-09-24) -- `cardLayer`, `cardCount`, `cardCorpusHash`, FORCE_CARD

**Status: the `cardCorpusHash` definition below is PROPOSED (R19).** bungo has
not ruled on it. It is what the writer does from this date, and a reader may
compare it, but it can still move before FO4CS reads it. No field was added and
the version did not move: every field below already had its place in `.lodo`
v4 and `.lodi` v7/v9, and no exe before this one ever wrote a non-zero card
value, so no file on disk means anything else by them.

**Where the numbers come from.** A `--native --impostors <dir> --arrays` bake
builds the chunks, then the card-arrays pass packs every octahedral card set a
chunk's `C` line stands on into DX10 arrays
(`<mod>/FO4CSLOD/<ws>/Objects/<ws>.LodgenCards.<family>.<WxH>` -- a
`cardArray` `.lodm` and four DDS) and appends `<array .lodm> <layer>` to each
`C` line it placed (twelve tokens; ten = a card in no array). Only then is the
pair written: `lodgenNativeLinkCards()` (`src/nativeemit.cpp`) reads the
manifests and the arrays, and the emitter writes what follows. On the CLI the
native block therefore runs after the object passes; in the panel the link is
called right after the card arrays, before the scratch teardown deletes the
manifests.

1. **The sets.** The arrays are the DISTINCT array `.lodm` files the `C` lines
   name -- never a directory listing, so an array an older bake left in the
   folder is never linked. A set's index is its rank in ascending order of the
   lower-cased file name's UTF-8 bytes. At most 32 sets (the 5 high bits of
   `cardLayer`) and 2048 layers a set (the low 11); more is refused.
2. **`cardLayer`** = `(set << 11) | layer` for the base whose formID is that
   layer's `id` (8 hex digits) in that set's `.lodm`; 0xFFFF on every other
   base. A base named by two layers, a packed value of 0xFFFF, and a `C` line
   whose (array, layer) is not the layer its base's id names are each refused
   by name, and the pair is not written.
3. **`cardCount`** = the base rows whose `cardLayer` is not 0xFFFF. The reader
   (`lodoRead`) RECOUNTS it and refuses a mismatch by name (`cardCount N but M
   base row(s) name a card layer`), and refuses rows naming a layer while
   `cardCorpusHash` is 0 (no arrays are named for them).
4. **`cardCorpusHash`** (proposed R19) = FNV-1a 64 from the offset basis
   `0xCBF29CE484222325` over, for each set in the order of 1, its five files in
   the order `.lodm`, colour, normal, mask, emissive (the names the `.lodm`'s
   `textures` object gives, last path component): the lower-cased file name's
   UTF-8 bytes, the file size as a little-endian u64, then every byte of the
   file. 0 when no set is linked. One byte of one card sheet moves it.
   `tests/spells/lodgen_cardlink.py hash <pair dir>` recomputes it outside the
   exe.
5. **FORCE_CARD** (`.lodi` instance flags bit 1) is set on a placement whose
   base has a card AND either (a) the base has no mesh in the placement's
   MNAM slot (`rep[mnamSlot]` = 0xFFFF), or (b) the chunk pass put that
   (chunk, dim, object index) on its card -- a `C` line, which is where
   `--impostors-from-level N` lands. Never on a base without a card.
6. **Card-only bases.** A base with no loadable LOD mesh in any slot but a
   linked card is now WRITTEN (it was dropped before, deviation 5):
   `rep[0..3]` all 0xFFFF, `ANY_MESH` clear, `boundRadius` =
   `|center| + sqrt(2 hw^2 + hh^2)` from its layer's `half`/`center`, which is
   §4.4's "must have a `cardLayer`" made true.
7. **Library reuse** compares `cardCorpusHash` too: a library whose arrays
   moved is rebuilt (`the card arrays moved`).

**Without `--impostors` (or without `--arrays`) nothing is linked** and the
pair is byte-identical to the one the exe before this lane wrote.

**Census**, on its own line: `native-cards: LINKED: cardCount N of M bases over
N array(s) of N layer(s), cardCorpusHash 0x...; card-only bases N; card sets
whose base is not in the table N; FORCE_CARD on N of M instances (N whose ring
slot has no mesh, N on a card by a C line); C lines N read, N linked` -- or
`native-cards: OFF (no card arrays linked); cardCount 0, cardCorpusHash
0x0000000000000000, FORCE_CARD on 0 instances`.

**For whoever makes the native bake incremental:** `--incremental --native` is
refused today (CONSTITUTION 10), so the link never meets a chunk replayed from
the `.lodj` cache. When that changes, a replayed chunk must bring its manifest
`C` lines with it, or FORCE_CARD rule (b) misses it (the link refuses a chunk
with no manifest at all, so a pair is never written half-linked).

**Gate:** `tests/spells/lodgen_cardlink.sh` -- Sanctuary, 9 chunks, the real
tree card sets: G1 cardCount, G2 every layer resolves, G3 the hash is the
contract and moves with one byte, G4 `--native-verify` refuses a cardCount off
by one, the no-cards bake byte-identical to the rung, and every one of them red
on the rung exe.

## 5. Refusal policy — hard for the generator, soft for the consumer

The generator refuses on anything wrong and names the field. **The consumer does
not**, because three of the keys are hashes of the user's data, which change the
first time any mod is installed or removed after a bake.

| class | keys | generator | consumer |
|---|---|---|---|
| **hard: both files** | magic, **version (see the per-file rows)**, `vertexStride`, `instanceStride`, **`groupStride` (v7), a group id that is not dense per chunk, a `groupCount` that disagrees with the chunks' sum, a sky slice whose length disagrees with the same placement's AO slice, a version-3…6 file carrying version-7 header words,** `clusterMaxTris`, **`clusterLodStride`**, **`occluderStride`**, a set reserved bit, `ROW_ORDER_NORTH_UP` clear, `chunkCount` over cap, a zero `lodoIdentity` without `NOLIB`, **a `scale` of 0**, **a `drawKey` out of order or not the base's rank**, **a cluster whose `geometricError` exceeds its `parentError`**, **a `CONE_OPEN` cluster carrying a cone (or the reverse)**, **an occluder naming an instance outside its own cell**, any CRC mismatch | refuse, name the field | **refuse to load, and never hide the engine's own LOD tree** |
| **hard: pairing** (between the two files) | the two files name different worldspaces; `pluginCorpusHash` or `objectCorpusHash` differs **between the `.lodo` and the `.lodi`**; `loadOrderHash` differs **between the two files** (§4 row 0x90); `lodoIdentity` does not name this `.lodo` (unless `NOLIB`) — `src/nativeemit.cpp`, every `pairing:` refusal | refuse, name the field | **refuse to load, and never hide the engine's own LOD tree** |
| **hard: `.lodo`** (`lodoRead`) | versions **1, 2 and 3 refused by name**, anything but 4, 5, 6 or 7; (v7) the `NEAR` flag without version 7 or version 7 without it, a material `features` byte with bits 5..7 set, or non-zero below v7 (§3.9); (v6) the base table not sorted by `(formId, materialSwap)` strictly, the SWAPPED flag disagreeing with `materialSwap`, a variant row with no plain row of its base before it (§3.8); the `LADDER` flag disagreeing with `ladderGroup` / `levelMax`, `levelMax` > 15; `cardCount` > `baseCount`; `cardCount` not equal to the base rows naming a card layer, or rows naming one while `cardCorpusHash` is 0 (CARDLINK1, §4.13); a base's `fullTriangles` that its own meshes do not recount to, or non-zero on a base with no mesh; mesh flags beyond ALPHA / SWAY / WATERTIGHT (plus VERTEX_COLOUR / VERTEX_ALPHA on v5); VERTEX_ALPHA without VERTEX_COLOUR; `colourVertexCount` and `offColours` not both zero or both set, a count over `vertexCount`, a flagged mesh whose vertices are not one contiguous range, or flagged rows that do not add up to the count (v5); reserved header bytes 0xCE…0xCF and 0xD4…0xFF (0xE0…0xFF on v5) | refuse, name the field | as above |
| **hard: `.lodi`** (`lodiRead`) | versions **1 and 2 refused by name**, anything outside 3…11; a version whose defining table is missing (v5 without the placement-AO blob, v6 without the vertex-AO blob, v7/v9 with neither group table nor sky stream, v8 without the horizon stream); a file carrying a LATER version's header words (v3/v4 with placement-AO words, v3–v6 with v7 words at 0x100/0x110, v7/v9 with v8 words at 0x11C); reserved header bytes by version (from 0xB0 on v3, 0xD4 on v4, 0xF1…0xFF on v5, 0xF1…0xF3 on v6 and later, plus 0x11C…0x1FF on v7/v9, 0x130…0x1FF on v8); instance flag bit 6 below v9 (§4.1); instance flag bit 7 below v10 (§4.14); instance flag bit 8 below v11 (§4.15); a stored cell outside the quantisation band (§4.1, `lodiCellAgrees`); the vertex-AO, sky and horizon offset tables and their slice lengths; the aggregate rows and their covered list (§4.6) | refuse, name the field | as above |
| **soft** (against the user's LIVE data only) | `pluginCorpusHash`, `objectCorpusHash`, `modelCorpusHash`, `cardCorpusHash`, **`loadOrderHash`** recomputed from the running load order and disagreeing with the file — a mod installed, removed or reordered since the bake | refuse, name the field and the plugin | **load anyway, log it, raise a `stale=1` census row, keep rendering** |

**The rows above are the classes, not every check.** `lodoRead` and `lodiRead` are
the complete list, and each refusal names its field in words; a consumer that
implements this table and not the readers will accept files they refuse.

---

## 6. Region bakes

A region bake sets `PARTIAL` in **both** files. Absent rows are all-zero and
indices stay **worldspace-stable**, because §2's base ordering is over the full
census. A consumer merges every `*.lodi` in the folder by chunk key, last wins.

**Note the chunk extent is the extent of the INSTANCES, not of the cells asked
for.** The nine-chunk Sanctuary region bakes 9 stock chunks and the `.lodi`
reports **10 present of 12 dense**: a handful of placements sit across a chunk
line from the chunk that drew them, so the bounding box of the instance positions
is one chunk wider than the bake. That is the same boundary effect §4.1c
measures, and it is not an error.

**The library is NOT region-scoped and the ladder does not change that.** It is
built from the full worldspace census in every bake, so two region bakes of one
worldspace carry byte-identical `.lodo` files. **Measured today (default,
authored-only library, 2026-09-19 bakes):** the urban region
(`scratchpad/horizonout_20260919/scrap`), a small region
(`scratchpad/cellview2b_20260919/lodibake`) and the DEFAULTS1 gate
(`scratchpad/defaults1_20260912/gate/e_new`) each wrote **6,204,388 bytes, md5
`89407121f640…` in all three**. (The 2026-09-11 ladder-era Sanctuary and
downtown bakes both wrote 9,710,564 bytes; that number is history, and it
disagrees with the 9,657,316 B the §1 table gives for the same era.)

---

## 7. The reader's draw checklist

Unchanged from v1 in substance — the layout is chosen so **FO4CS never creates a
vertex or index buffer**:

1. **Open and validate.** Both headers, both `headerCrc32`, both `indexCrc32`,
   the hard/soft split of §5. `lodoIdentity` must match unless `NOLIB`.
2. **Upload three StructuredBuffers** — the library, the ladder table and the
   instances — plus append and indirect-args buffers sized from
   `maxInstancesPerChunk` and `maxClustersPerMesh`.
3. **Derive `chunkIndex[instanceCount]`** by walking the chunk table once. It is
   not in the file: 636 KiB of VRAM, zero bytes of disk.
4. **Cull, one Dispatch over all instances.** Frustum-test with
   `base.boundRadius × scale`; expand each chunk box by its `maxBoundRadius`;
   reject behind the cell's occluder boxes; then **cut the cluster tree by
   §4.4's two comparisons** — a cluster's sphere and cone are in the same row as
   its error, so one fetch serves the frustum test, the backface test and the
   detail test.
5. **Draw, `DrawInstancedIndirect` per bucket**, seated at
   `DeferredPrePass_Post`. **No IA, no input layout, no index buffer.**
6. **Three size classes, not one fixed 48.** `vertexCountPerInstance` is
   **12 / 24 / 48**, chosen by the cluster's `flags` bits 0–1. A coarse cluster
   obeys the same caps, so nothing branches on level.
7. **Bucket key** = (draw size class × family × arrayClass × arraySet × alpha
   state) for clusters. Measured today that is **8–14 mesh draws plus 1–2 card
   draws** for the whole worldspace, against 343–1,795 resident engine draws.
   v2 makes those buckets contiguous per cell, so building them is a walk
   rather than a scatter.
8. **The shadow pass is a second complete consumer**, with its own tolerance, not
   a sentence. **Quote the main-view draw count and the shadow draw count
   together.**
9. **Order of operations when suppressing the engine's far field:**
   validate → build buffers → read back a **non-zero drawn-primitive count for
   one frame** → **only then** hide `spLODObjectRoot`.
10. **The census must accuse its own plumbing.** The replacement row is
    `FarField: native drawn=<n> culled=<n> overflow=<n> | not observing: no .bto in scene`.

**The whole census this checklist implies is specified in
`docs/LODGEN_CENSUS.md`** (lane CENSUS1, 2026-09-11, from bungo's ruling on gap
(4): *"We need them"*). It states, field by field, what Improved LOD prints per
frame-window — the serving arm per module, the per-ring considered / culled three
ways / drawn / triangles, the cluster cut and its pixel tolerance, cards,
residency against its budgets, the shadow view's own counts, the screen-size fade
thresholds and the cross-fades in flight — each with its unit, the section of
this page it is read from, the scene change that must MOVE it, its refusal words
and a default that accuses its own plumbing. It also carries the arithmetic that
ties a runtime number to a bake number (its §6.2), so a drawn count can be
checked against this file's own `instanceCount` rather than against an opinion,
and `tests/spells/lodgen_census_check.py` proves that bake half against the
bytes.

**Residency: read once, deliberately not streamed.** The v2 figure was ~63.8 MiB
for one worldspace; the ladder adds this region's 4.0 MiB of `.lodo` growth to
that, against 125.9–370.0 MiB of resident object geometry today.
**Textures are 76% of it.**

---

## 8. Staleness, and what each hash sees

`objectCorpusHash` exists because the terrain hash is provably blind to objects:
`EsmWorld::vhgtCorpusHash` FNV-hashes the first VHGT field of each LAND record
and nothing else, so moving a REFR, rescaling it, adding a placement or
repointing a base's MNAM does not change it.

**`objectCorpusHash` is FNV-1a 64 over exactly this, in this order:**

1. for EVERY reference the object walk reads — in ascending cell order
   (`cy` then `cx`), then the persistent cell —
   `(REFR formId, base formId, DATA position[3], DATA rotation[3], XSCL,
   a flag byte of initially-disabled | deleted)`;
2. then, over the LOD-bearing bases in **ascending formId** order,
   `(base formId, the four MNAM slot paths, case-folded)`;
3. then, over the SCOL bases in **ascending formId** order,
   `(SCOL formId, each part's base formId, each placement's position, rotation
   and scale)`.
4. **(SWAP1, v6) material swaps, each ONLY when set**, so a worldspace with no
   swap reaching a LOD placement keeps its pre-v6 hash: after a reference's flag
   byte, `'XMSP'` and its XMSP form, for a reference that can reach the library
   (enabled, not deleted, a SCOL or a LOD-bearing base); after a base's slot paths, `'MODS'` and its MODS form; after a SCOL's
   formId, `'MODS'` and the SCOL's MODS form; and last, over every distinct swap
   form named (including part MODS reached through the third clause of §3.8), in
   ascending order: `(form, row count or 0xFFFFFFFF when the record is missing,
   then per row the folded BNAM, the folded SNAM and the CNAM or -1)`.

Nothing else. Both files carry the same value at header 0x18.

### 8.1 `loadOrderHash` (v2) — bungo's *"Add it"*, 2026-09-11 10:0x

The record hash above is blind to one thing: which plugins were read. **`loadOrderHash`
is FNV-1a 64 over, for each plugin of the list `EsmWorld::load` was given, IN
THAT ORDER: the lower-cased BASE FILE NAME's UTF-8 bytes, then its byte size as a
little-endian u64.** Nothing else — not the directory, not the mtime — so moving a
mod folder or touching a file does not fire it, while adding, removing, reordering
or editing a plugin does.

It sits at `.lodo` 0xB8 and `.lodi` 0x90, and **a reader gets it from the header
alone without parsing a table**. A mismatch between the two files is a pairing
refusal naming both. Measured on this bake, from `Fallout4.esm` alone:
`0xa056a596e2bb16e7`.

`lodgen --native-verify <lodo> <lodi> --native-verify-corpus` re-reads the plugin
and recomputes all three — object, plugin and load order — and refuses a stale
pair **naming the file and which hash moved**, for example:

```
native REFUSED <path>\Commonwealth.lodo is STALE: objectCorpusHash
0x6324ed6a35cde4eb in the file, 0x6324ed6a35cde4ea from
X:\...\Fallout4.esm -- a placement, a base's MNAM or a SCOL part changed since
the bake
```

That refusal is a gated FLOOR, not a claim: leg 9 of `tests/spells/lodgen_native.sh`
doctors a written pair (flipping `objectCorpusHash`, re-deriving `lodoIdentity` so
the pairing rule cannot answer first, re-signing both header CRCs) and requires the
refusal to name it.

**Since lane BAKEREC1 (2026-09-17) the refusal also names the PLUGIN.** A
`loadOrderHash` or `pluginCorpusHash` refusal is a fold over the whole list and
cannot be un-folded, so on its own it says *something in thirty plugins moved*.
The `.lodb` bake record beside the pair (`docs/LODGEN_BAKE_RECORD.md`) carries the
list itself — index, lower-cased base name, byte size and **an FNV-1a 64 over the
plugin's own BYTES** — so `--native-verify` appends a second paragraph naming the
file and what happened to it: `was ADDED` / `was REMOVED` / `was REORDERED (it was
N at the bake)` / `was RESIZED (A bytes at the bake, B now) -- it was edited` /
`was EDITED: its bytes hash 0x... now and hashed 0x... at the bake, at the same N
bytes -- the load order cannot see this`. The last of those is the case
`loadOrderHash` is structurally blind to, because it folds only the name and the
size; the byte hash is computed **only** when name and size still agree, which is
the one case the cheap fields cannot answer.

When there is no record the message is exactly what it was, plus the sentence
`(no .lodb bake record beside this pair, so the plugin that moved cannot be named
-- re-bake once with this build and it will be)`. When the record lists the
plugins and none of them moved, it says so, which points the reader at the records
instead of the load order. Gated by leg (e) of `tests/spells/lodgen_bakerec.sh`.

---

### 8.2 Keeping the library instead of rebuilding it (lane PERF1, 2026-09-17)

`--incremental` may now KEEP the previous `.lodo` rather than rebuild it, and
the census line `native-library-build:` says which it did — `reused (…)` or
`rebuilt (<the one test that refused>)`. The preconditions are exactly the
hashes this section is about: the base census, `loadOrderHash`,
`vhgtCorpusHash` and the object corpus hash must all be unmoved against the hex
the previous bake's record wrote, the file must be there, and it must survive
`lodoRead( …, payloadCheck = true )`. The switch digest is guaranteed equal by
the driver, which refuses an incremental run outright when it moves.

Two refusals are structural rather than accidental. **Occluders**: the
per-model occluder box the library build computes lives in neither file (§4.5
keeps only the writer's per-cell selection, in world space, quantised), so a
reused library has no box to offer; occluders default ON, so the ruled pipeline
rebuilds and says so. **Meshes**: the three hashes cover the plugin corpus, not
the mesh corpus — a `.nif` edited on disk with no plugin change is invisible,
because the reused run opens no model at all (`models 0 loaded` in its own
census). `modelCorpusHash` cannot close that, since computing today's value
means loading every model, which is the stage being skipped.

`lodoRead` had never restored `loadOrderHash` into the library it returns —
every other corpus hash was carried over, and v2's addition at header `0xB8`
was missed. Nothing noticed until a library was read back and then WRITTEN
from: the `.lodi`'s header `0x90` came out zero, 12 bytes apart from the full
bake's. Fixed in `src/lodofile.cpp`; gated by leg (d) of
`tests/spells/lodgen_perf.sh`.

---

## 9. What this format deletes by construction

Today's 16-bit index cap fires in four places, and one of them **silently drops
object geometry**: the ring-3 chunk at (−32,0) has four shapes at
65,535 / 65,534 / 65,529 / 65,424 vertices and **2,628 of 42,560 placements
(6.17%) have no geometry in the file at all**; downtown (0,−32) loses 882 of
42,641 (2.07%). The loss is order-dependent, not importance-dependent.

In the native format nothing is stitched: the only index domains are a cluster's
u8 local index over ≤ 48 vertices and a u32 `vertexBase` over ~422k library
vertices, and the heaviest LOD model in the Commonwealth is 2,244 triangles.
If a model ever did exceed a cluster's addressing, **the writer refuses and names
the model**.

**On this region the census gate is exact rather than vacuous:**
`instanceCount` = 3,526 = the stock manifests' placement row count, with
**0 dropped for a base outside the table** and **0 instances with neither
geometry nor a card** (`--native-verify`). **The asymmetric drop proof on
(−32,0) dim 32 has been run** (lane INCRGATE1, 2026-09-24,
`tests/spells/lodgen_native_baseline.sh --drop-proof`). One bake of that chunk with
`--slot-fallback --identity --keep-bto --native` gives both halves. The stock `.BTO`
has no geometry for **2,628 of 42,560** placements (6.17 %, the figure above).
The `.lodi` holds **all 42,560**: every manifest row is in its table, and
`--native-verify` reads `instancesWithNeitherGeometryNorCard 0`.

---

## 10. Sample files, and the gate

**The synthetic known-answer pair**, written by the exe and checked by the
independent decoder against answers printed before the bytes:
`Synthetic.lodo`, `Synthetic.lodi` and `Synthetic.expect.txt`.
`lodgen <esm> --native-fixture <dir>` writes it.

**v3 gave the fixture a ladder, a cone that must open, and one occluder.** Its
level-0 answers stay hand-derived — 3 clusters, 32 vertices, 32 triangles — and
so do two new ones nobody has to compute: mesh 0's single level-0 cluster is a
**CLOSED CUBE**, whose twelve face normals span the whole sphere, so it MUST
carry `CONE_OPEN`, axis (0,0) and cosine −1, while mesh 1's flat strip carries
the tightest cone a cone can be; and the cube's bounding sphere is exactly
centre (0, 0, 100), radius √(50²+50²+100²) = 122.474487. The occluder is
hand-derivable too: a box of half (40, 40, 90) about the cube's centre, drawn at
scale 0.5 with no rotation, lands at world centre (1200, 2200, 350) with half
extents (19.98, 19.98, 44.955) — the 0.999 pull-in stated, not hidden.

**What the fixture does NOT claim.** The ladder's own output is a simplifier's
and no hand derives it, so the whole-file cluster and vertex counts are no longer
stated as known answers. The ladder is checked by INVARIANTS instead — errors
monotone up every chain, the roots' subtree counts summing to the level-0
triangle count, every triangle inside its sphere, every normal inside its cone —
and by one number stated as a PREDICTION made before the run
(`lodo.levelMaxAtLeast 1`). A fixture may predict; it may never copy.

The fixture library deliberately does **not** set `CACHE_ORDER`: its vertex
expectations are hand-derived from the source order and a permutation would make
them underivable. The cache order is proved on the real worldspace bake instead,
per mesh, with a floor.

**`tests/spells/lodgen_native.sh`** is the gate, **thirteen legs**: the fixture
and the decoder, two-write byte identity, the refusal set (one mutation per row
rule and one per new field, every CRC re-signed so the RULE answers), the real
region bake, the stock-path byte identity, the decoder's ESM and manifest legs on
the real pair, `--native-verify --native-verify-corpus`, the field gate
(`lodgen_native_fields.py`, one subsection a field with its floor), the staleness
floor, **the geometry gate on the fixture and on the real pair
(`lodgen_native_cut.py`: spheres, cones, the cut's partition at three tolerances
× three distances, the occluder boxes, every one with a floor that must go red in
the same run)**, **the two exact ways back**, and **the occluders on a second
small region**, because Sanctuary has no watertight LOD mesh and its box gates
would otherwise be vacuous.

---

## 11. Deviations from the spec, as built

1. **Mesh row 56 B, not 48.** The spec's row carried no model path, so the mesh
   table's sort law could not be checked in the file. `modelStringOffset` + a
   reserved word were added: +8 B × ~3,355 meshes = 26.8 KiB. v3 spent that
   reserved word on `clusterCountL0` + `levelCount`.
2. **The rotation is the drawn rotation; `seed` is the hash's low byte.**
   `treeHash % 360` needs 9 bits, the u8 holds 8. §4.3.
3. **The cell index inside a chunk is defined** (north-up row-major,
   `(3 − ly)·4 + lx`); the spec named the sort key and not the numbering.
4. **`material.layer` may be 0xFFFF** ("unassigned"); the spec's `< 2048`
   holds for every assigned layer and the reader refuses 2048..0xFFFE.
5. **The bake writes what the stock ring bakes and nothing more:** no card layer
   (`cardLayer` = 0xFFFF everywhere, `cardCorpusHash` = 0) **unless a
   `--impostors --arrays` bake links its card arrays (lane CARDLINK1,
   2026-09-24, §4.13): then cardLayer, cardCount, cardCorpusHash and
   FORCE_CARD are written, and a base with a card but no mesh is kept**; no `crossPx16` (0),
   `selfAO` = 255, `arrayClass`/`arraySet` = 0 with `layer` unassigned. A base
   with no loadable LOD model in any slot is left OUT of the base table and its
   instances are counted (`dropped for a base outside the table` in the census
   line) rather than written with neither mesh nor card. Lanes OBJM/OBJC/OBJP
   fill those fields.
6. **(v2) The mesh/material sort is inside the CELL, not inside the chunk**, and
   the reason is the 8-byte cell-range row. §2.1. **bungo's to overrule.**
7. **(v2) The placed REFR formID stayed in the cold record**, at the instance's
   own index, instead of growing the hot record to 32 bytes. §4.1a. **bungo's to
   overrule.**
8. **(v2) `cold.identity` is unique per STOCK CHUNK**, not per `.lodi` chunk; the
   per-placement identity is the instance index. §4.1c.
9. **(v3) THE LADDER SIMPLIFIES ON A POSITION WELD, so levels 1 and up carry the
   FIRST contributor's UVs where two vertices shared a quantised position.**
   An edge collapse cannot cross a split vertex, so a ladder that respected every
   UV seam would barely simplify Bethesda's LOD meshes at all — they are split
   heavily for the atlas. Measured on this region: **117,722 welds merged
   vertices whose UVs differ by more than 1/256 of a UV unit**, over 103,534
   welded positions and 263,876 source vertices. **Level 0 never uses the weld**,
   so the near view is untouched; the artefact is a texture shift on coarse
   levels only, at the distances where those levels are selected. The
   alternative — welding by (position, UV) and accepting far fewer laddered
   meshes — is measurable in one bake and is **bungo's call**.
10. **(v3) The error above 512 covered triangles is an upper BOUND, not a
    measurement.** Four groups of 6,977 took the chain bound. Stated because a
    bound is conservative in the safe direction (it over-states the error, so a
    consumer draws finer than it must) but it is not the same claim as the
    measured 6,973.
11. **(v3) `boundaryCoarsest` is NOT comparable to `boundaryEmitted`, and the
    first version of this deviation said the opposite.** The mesh report carries
    the coarsest level's boundary-edge count beside level 0's, and the obvious
    reading — a rise means a hole — is wrong, because **a ladder is PARTIAL
    wherever a group refuses**, so the coarsest level is a FRAGMENT of the mesh.
    Measured on this region: 18 of the 1,900 laddered meshes show a rise, every
    one of them has at least one refused group, and their coarsest level covers
    as little as **6.6 percent** of their own surface
    (`RockCliffGSCrater02_LOD_0.nif`, 45 of 685 full-detail triangles). A
    fragment has its own outline. The comparison was apples to oranges and the
    gate that made it has been replaced.

12. **(v4) THE VERSION WORD IS CONDITIONAL.** A bake with no aggregate writes
    version 3 and a bake with aggregates writes version 4, from the same
    writer, and the reader accepts both. Every earlier version bump in this
    format was unconditional and refused its predecessor by name, so this is a
    deviation and not a precedent. The reason is CONSTITUTION 10: aggregation
    is a MODULE and its off value has to be the EXACT way back, which means
    byte-identical, which an unconditional bump would have made impossible --
    there would have been nothing left for the byte-identity gate to measure
    against. It is safe here in a way v2-read-as-v3 was not: a v3 file read by
    a v4 reader is unambiguous (zero aggregates, both offsets 0, the pad from
    0xB0 all zero), where a v2 file read as v3 silently claimed a worldspace
    occludes nothing. **If bungo would rather the version always moved, it is
    one line and every v3 baseline is re-pinned.**
13. **(v4) The aggregate does not REMOVE the instances it covers, it
    SUPPRESSES them**, because this file has no per-ring instance list to
    remove them from (§4.4). bungo's words were *"the ring 3 instance list then
    holds one placement per cell instead of one per tree"*; what the file can
    honestly say is *these 3,414 instances are stood in for by these 97 cards
    past this projected size*, and that is what the covered blob says.

    **The rule with teeth is PER STEP and it is in the writer**: a simplified
    group whose boundary-edge count exceeds that of the surface it REPLACES —
    the same surface, both sides — is refused and its clusters stay roots. It
    fired **57 times** on this region, and before it existed two meshes went from
    a WATERTIGHT level 0 to four and eight boundary edges, which is a closed
    building with a hole in it. §3.4.

14. **(v4) THE `.lodo` VERSION WORD IS NOT CONDITIONAL, and it cannot be.**
    Deviation 12 is the opposite case and the contrast is the point. The
    aggregate could ship a conditional version because a v3 file read as v4 says
    *"no aggregates"*, which is TRUE. A v3 file read as v4 says the base's
    `crossPx16[0..1]` — four bytes v3 wrote as zeros — are the base's
    **full-detail triangle count**, and zero triangles is not true, it is a lie
    that reads as a valid number. So `.lodo` goes to **4 unconditionally** and
    the reader **refuses 3 by name**, saying what the four bytes used to be and
    what this reader takes them as. That refusal is a gated check, and it is the
    one that told this lane the fixture's own `--expect` table was stale.

15. **(v5) THE `.lodi` VERSION WORD IS CONDITIONAL, following Deviation 12.**
    The placement-AO blob adds no reinterpretation: every v5 header word it uses
    (0xD4…0xF0) is pad a v3 or v4 writer already wrote as zero, and all-zero can
    only mean "the feature is absent". So a bake with `--native-no-placement-ao`
    writes the version it would have written anyway, and the file's PAYLOAD is
    byte-identical to the bake before this lane — the two derived words, 12
    bytes, still move with the companion `.lodo`, and §4.7 shows the measurement.
    The reserved sweep moves with
    the version (`padFrom` = 0xF1 on v5, 0xD4 on v4, 0xB0 on v3), and that same
    sweep is what refuses a **v3 or v4 file that carries the AO words** — the
    case nobody remembers to cover.

16. **(v4) THE CONE MARGIN GREW BY A FACTOR OF A HUNDRED, and it is a
    measurement, not a loosened test.** `LODO_CONE_MARGIN` was `1.0e-5` and held
    for the whole of v3. With level 0 built from the near `MODL` the mesh boxes
    are the real models' — thousands of units instead of a LOD mesh's tens — so
    the writer's float32 quantised positions and an independent reader's
    **double** dequantisation of the same u16 no longer produce the same face
    normal. `tests/spells/lodgen_native_cut.py` measured the worst disagreement
    at **1.21e-4 of cosine over 1,533 coned clusters**, twelve times the old
    margin, and two harness legs went red with that number. The margin is now
    `1.0e-3`, eight times the measured worst, and the open-cone threshold moved
    to `2 × CONE_MARGIN` so a kept cone still stores a cosine above zero. **A
    wider cone culls LESS, never more**, so the safe direction and the correct
    one are the same here; a narrower one would have been a silent hole.

17. **(v4) The silhouette gate measures the CUT, not the level, and it rolls
    back the WHOLE level.** Deviation 11 already records that a ladder is PARTIAL
    wherever a group refuses, so the clusters a level produced are only a
    fragment of what a viewer at that level sees. The gate therefore rasterises
    `roots ∪ unconsumed ∪ new` against level 0, and on a failure it truncates
    clusters, rows, local indices and vertices back to a snapshot taken before
    the level began — a partial level would leave a mesh whose parent pointers
    name rows that are no longer there. On the Sanctuary region it refused
    **5,683 levels across 3,266 meshes**, and the worst fraction any kept level
    holds is exactly the floor, **0.7000**.

---

## 12. What v3 leaves for the lanes after it

| what | where it goes | free today |
|---|---|---|
| aggregate ring-3 impostors (CARDS-AGG) | **DONE, v4** — §4.6; the room this row named at `.lodi` 0xB0 is now the aggregate table, the covered blob and their six header words | — (the 44 bytes it left at 0xD4…0xFF went to v5 and v6; see the occluder row) |
| the ladder starting from the NEAR model (§3.5.4) | **DONE, v4, then REVERSED as the default 2026-09-17** (*"Authored LODs only"*) — §3.5.7; no format change; the default is now `--library mnam` with no ladder, and `--library near` / `--native-ladder` bake this row | — |
| the ladder's screen-size steps per base | **half spent, v4** — `crossPx16[0..1]` is now one u32 `fullTriangles` (§3, Deviation 14) | `crossPx16[2..3]`, 4 B a base |
| per-instance tint / light-record index | **gone** — v2 took the record's 0x16 word for `drawKey`. A field here means a 32-byte stride, and the reader must refuse 24 by name | — |
| more than four occluders a cell, or a second box shape | `maxOccludersPerCell` is in the header, so raising it is not a format break; a new shape is | `.lodi` v3–v6 header is 256 B and FULL: v4 took 0xB0…0xD3, v5 0xD4…0xF0, v6 0xF4…0xFF, leaving **0xF1…0xF3 = 3 reserved bytes**. A **v7+ header is 512 B** (§4, `lodiHeaderBytes()`): v7 took 0x100…0x10D (group table) and 0x110…0x11B (sky stream), leaving **0x11C…0x1FF = 228 reserved bytes** on v7 and v9 (retired v8 spent 0x11C…0x12F). 0x10E…0x10F sits between the two tables and the reader's reserved sweep does not cover it |
| anything else in the `.lodo` header | — | `.lodo` header **0xCE…0xCF and 0xD4…0xFF = 46 reserved bytes** (v4 took 0xD0…0xD3 for `cardCount`) |
| per-instance AO for card-drawn placements | **DONE, v5** — §4.7; the blob is written last and `--native-no-placement-ao` leaves the payload byte-identical to v4 (12 derived bytes move with the `.lodo`) | — |
| a per-base LOD-mesh / near-mesh ratio, so a reader can budget | nothing written; `fullTriangles` is now in the row and the near-model triangle count is derivable from the cluster table | `crossPx16[2..3]` |

A v5 that changes a stride or a table set bumps the version and **refuses v4 by
name**, the way v4 refuses v3 and v3 refuses v2. The rule this lane adds to that
sentence: bump UNCONDITIONALLY when old bytes are REINTERPRETED (`.lodo` v4),
and conditionally only when the old file read by the new reader is unambiguous
(`.lodi` v5) — Deviations 14 and 15.

---

## Source, as built

Rewritten 2026-09-11 by lane NATIVE1b under `ww-contract-provenance`: the source
hashes below were taken FIRST, every line number in the anchor table was then
found again from its own anchor text by
`scratchpad/native1b_20260911/anchors.py` (which exits 1 if any anchor is not
found exactly once), and the version constants were re-read last.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodofile.h` | `6b3e6de0289a3ee7` | 23,504 | 446 |
| `src/lodofile.cpp` | `b699440d82a1657e` | 79,240 | 1782 |
| `src/lodifile.h` | `b2c7d68d0cadc26e` | 17,445 | 373 |
| `src/lodifile.cpp` | `184426b8031ae4ab` | 61,418 | 1167 |
| `src/nativeemit.h` | `3607712352550482` | 5,279 | 105 |
| `src/nativeemit.cpp` | `24d4b150ddaa26f0` | 50,291 | 1158 |
| `src/esmdata.h` | `a60733e1eba326d4` | 12,652 | 300 |
| `src/esmdata.cpp` | `87a7519ec268c55a` | 29,570 | 932 |
| `src/lodgen.cpp` | `c05fd079655ac03e` | 391,673 | 8924 |
| `src/nifcli.cpp` | `5c83d4e2e554ceaf` | 278,021 | 6335 |
| `NifSkope.pro` | `00ff23679bc56fb3` | 20,362 | 768 |
| `tests/spells/lodgen_native_decode.py` | `0c483f15dbb23fb8` | 49,668 | 939 |
| `tests/spells/lodgen_native_mutate.py` | `654a69d4f9044c7d` | 14,664 | 316 |
| `tests/spells/lodgen_native_fields.py` | `d69a506f64b9a673` | 23,192 | 438 |
| `tests/spells/lodgen_native_cut.py` | `6e39184a1972bb3b` | 22,510 | 531 |
| `tests/spells/lodgen_native.sh` | `517f686142b791ef` | 13,883 | 261 |

Exe that wrote the measurements on this page: `release/NifSkope.exe`
**2026-09-11 10:14:23**, 21,101,056 B, sha256
`ab97d12c511aa59823f7b3b5cadb63b7bcf10b1b136334add01b9c056292b508`. The bakes, the fixture, the logs and the pictures are under
`scratchpad/native1b_20260911/`.

| claim | line | anchor |
|---|---|---|
| `.lodo` magic, version 3 and the header size | `src/lodofile.h:51` | `constexpr quint32 LODO_MAGIC = 0x4F444F4CU;` |
| the version constant itself | `src/lodofile.h:64` | `constexpr quint32 LODO_VERSION = 3;` |
| the LADDER flag (v3) and the way back it names | `src/lodofile.h:82` | `LODO_FLAG_LADDER = 8` |
| CONE_OPEN, the cones refusal in a bit | `src/lodofile.h:105` | `constexpr quint16 LODO_CLUSTER_CONE_OPEN = 4;` |
| the ladders constants: group, floor, depth | `src/lodofile.h:118` | `constexpr int LODO_LADDER_GROUP = 4;` |
| the exact-error budget | `src/lodofile.h:127` | `constexpr int LODO_ERROR_EXACT_TRIS = 512;` |
| the root marker | `src/lodofile.h:129` | `constexpr float LODO_ERROR_ROOT = 3.4028235e38f;` |
| the 16-byte vertex | `src/lodofile.h:140` | `struct LodoVertex` |
| the 56-byte mesh row, and the v3 words in v2s reserved slot | `src/lodofile.h:165` | `quint16 clusterCountL0;` |
| the 16-byte cluster row | `src/lodofile.h:170` | `//! Cluster entry, 16 bytes. Unchanged from v2 except flags bit 2.` |
| THE 48-BYTE LADDER ROW | `src/lodofile.h:202` | `struct LodoClusterLod` |
| the 16-byte material row | `src/lodofile.h:223` | `struct LodoMaterial` |
| the 32-byte base row | `src/lodofile.h:237` | `struct LodoBase` |
| the strides pinned at compile time | `src/lodofile.h:253` | `static_assert( sizeof( LodoClusterLod ) == 48` |
| `loadOrderHash` at .lodo header 0xB8 | `src/lodofile.h:298` | `quint64 loadOrderHash = 0;          //!< v2, header 0xB8` |
| the ladder tables header words | `src/lodofile.h:276` | `quint64 offClusterLods = 0;` |
| the per-mesh statistics the gate reads | `src/lodofile.h:358` | `struct LodoMeshStats` |
| the ladder's silhouette refusal, declared | `src/lodofile.h:394` | `quint32 groupsRefusedSilhouette = 0;` |
| `.lodo` header field offsets, and 0xC0 / 0xCE | `src/lodofile.cpp:44` | `constexpr int H_OFF_CLUSTERLODS = 0xC0, H_CLUSTERLOD_STRIDE = 0xC8;` |
| octahedral 12:12 pack | `src/lodofile.cpp:120` | `quint32 lodoPackOct12( const float n[3] )` |
| octahedral 16:16, the cone axis | `src/lodofile.cpp:152` | `void lodoPackOct16( const float n[3], quint16 out[2] )` |
| the boundary-edge count, welded by quantised position | `src/lodofile.cpp:270` | `quint32 lodoBoundaryEdges( const std::vector<quint32> & tris, KeyFn key )` |
| point-to-triangle distance | `src/lodofile.cpp:310` | `float lodoPointTriDist2( const float p[3], const float a[3], const float b[3], const float c[3] )` |
| the two-sided vertex-sampled deviation | `src/lodofile.cpp:398` | `float lodoSoupDeviation( const std::vector<quint32> & a, const std::vector<quint32> & b,` |
| ONE emitter for every level: sphere, cone, size class | `src/lodofile.cpp:412` | `quint32 lodoEmitCluster( LodoLibrary & lib, const LodoMesh & mesh, float meshRadius, quint16 meshId,` |
| the sphere and the cone describe the STORED geometry | `src/lodofile.cpp:461` | `qp[v * 3 + k] = lodoDequantU16( lodoQuantU16( verts[v].pos[k], mesh.aabbMin[k], mesh.aabbExtent[k] ),` |
| the cone axis is AREA-WEIGHTED, the cosine measured on the DECODED axis | `src/lodofile.cpp:551` | `cosMin = std::min( cosMin, dec[0] * faceN[t] + dec[1] * faceN[t + 1] + dec[2] * faceN[t + 2] );` |
| the weld the ladder simplifies on, and the UV conflicts it counts | `src/lodofile.cpp:835` | `uvConflicts++;` |
| the group partition | `src/lodofile.cpp:997` | `nParts = meshopt_partitionClusters( part.data(), flat.data(), flat.size(),` |
| the group border LOCKED so a replaced group cannot crack | `src/lodofile.cpp:1033` | `lockv[v] = 1;` |
| the simplification itself | `src/lodofile.cpp:1039` | `const size_t n = meshopt_simplifyWithAttributes( dst.data(), gTris.data(), gTris.size(),` |
| THE LADDERS SILHOUETTE REFUSAL | `src/lodofile.cpp:1059` | `if ( lodoBoundaryEdges( outSoup, identityKey ) > lodoBoundaryEdges( gTris, identityKey ) ) {` |
| the error: exact under the budget, chain-bounded above it | `src/lodofile.cpp:1076` | `E = lodoSoupDeviation( fullSoup, outSoup, wpos );` |
| the error must GROW or the group is not formed | `src/lodofile.cpp:1083` | `if ( !( E > childErr ) ) {` |
| the coverage split that makes sourceTriangles a partition | `src/lodofile.cpp:1124` | `outCover[best].push_back( t );` |
| whichever cap binds first closes the cluster | `src/lodofile.cpp:916` | `if ( idx.size() / 3 >= LODO_CLUSTER_MAX_TRIS \|\| members.size() + size_t( fresh ) > LODO_CLUSTER_MAX_VERTS )` |
| payloads 4,096-aligned, pad zeroed by hand | `src/lodofile.cpp:1251` | `const quint64 at = alignUp( start, LODO_PAYLOAD_ALIGN );` |
| the ladder table is inside indexCrc32 | `src/lodofile.cpp:1296` | `h.offClusterLods = payload( lib.clusterLods.data(), quint64( lib.clusterLods.size() ) * sizeof( LodoClusterLod ) );` |
| `.lodo` reader: version 2 refused by name | `src/lodofile.cpp:1383` | `return refuse( QStringLiteral( "version 2: a v2 library has NO cluster ladder table` |
| `.lodo` reader: reserved header bytes refused by offset | `src/lodofile.cpp:1451` | `return refuse( QString( "reserved header byte at 0x%1 is not zero" )` |
| `.lodo` reader: the base table sort law | `src/lodofile.cpp:1688` | `return refuse( QString( "base table is not sorted by formId ascending at row %1` |
| `.lodo` reader: the cluster sort law gained LEVEL | `src/lodofile.cpp:1596` | `return refuse( QString( "cluster table is not sorted by (meshId, materialId, level) at row %1" ).arg( i ) );` |
| `.lodo` reader: MONOTONICITY is a refusal | `src/lodofile.cpp:1625` | `return refuse( QString( "cluster %1: geometricError %2 is larger than its parentError %3; the "` |
| `.lodo` reader: the cone and its flag must agree | `src/lodofile.cpp:1651` | `return refuse( QString( "cluster %1 is CONE_OPEN but carries an axis (%2, %3) and cosine %4" )` |
| `.lodi` magic and version 3 | `src/lodifile.h:67` | `constexpr quint32 LODI_MAGIC = 0x49444F4CU;` |
| the version constant itself | `src/lodifile.h:79` | `constexpr quint32 LODI_VERSION = 3;` |
| THE ONE SORT LAW, stated in the header | `src/lodifile.h:36` | `*  THE ONE SORT LAW (v2, 2026-09-11, lane NATIVE1a)` |
| the 24-byte instance record, 0x16 = drawKey | `src/lodifile.h:136` | `quint16 drawKey;` |
| the cold record: the placed REFR and the stock identity | `src/lodifile.h:215` | `quint16 identity;` |
| THE 40-BYTE OCCLUDER ROW | `src/lodifile.h:170` | `/*! v3: ONE PRECOMPUTED OCCLUDER, 40 bytes.` |
| the per-cell occluder range | `src/lodifile.h:200` | `struct LodiOccluderRange` |
| the per-cell cap and the stride, as constants | `src/lodifile.h:104` | `constexpr quint16 LODI_OCCLUDERS_PER_CELL = 4;` |
| the box the emitter offers per instance | `src/lodifile.h:279` | `bool hasOccluder = false;` |
| `.lodi` header field offsets, and 0x98 / 0xB0 | `src/lodifile.cpp:39` | `constexpr int H_OFF_OCC = 0x98, H_OFF_OCCRANGE = 0xA0, H_OCCCOUNT = 0xA8;` |
| smallest-three 2 + 3 x 15, LSB-first | `src/lodifile.cpp:113` | `void lodiPackRotation( const float m[9], quint16 out[3] )` |
| the cell index inside a chunk (Deviation 3) | `src/lodifile.cpp:171` | `return ( LODI_CHUNK_CELLS - 1 - ly ) * LODI_CHUNK_CELLS + lx;` |
| the sort: chunk, cell, drawKey, ref, part | `src/lodifile.cpp:295` | `return std::make_tuple( chunkIdx[a], cellIdx[a], A.drawKey, A.refFormId, A.scolPart )` |
| maxBoundRadius from the QUANTISED scale | `src/lodifile.cpp:318` | `maxR = std::max( maxR, r.boundRadius * qs );` |
| THE OCCLUDER SELECTION: volume descending, index as the tie-break | `src/lodifile.cpp:390` | `std::sort( v.begin(), v.end(), []( const std::pair<double, quint32> & a, const std::pair<double, quint32> & b ) {` |
| the 0.999 pull-in for the quantised rotation | `src/lodifile.cpp:412` | `b.halfExtent[k] = r.occHalf[k] * qs * 0.999f;` |
| the occluders join indexCrc32 | `src/lodifile.cpp:451` | `// v3: the occluder table and its ranges join indexCrc32 (contract 4.5)` |
| `.lodi` reader: version 2 refused by name | `src/lodifile.cpp:541` | `return refuse( QStringLiteral( "version 2: a v2 instance table has NO occluder tables` |
| `.lodi` reader: a scale of 0 is a refusal | `src/lodifile.cpp:767` | `if ( r.scale == 0 )` |
| `.lodi` reader: a box must name an instance of its OWN cell | `src/lodifile.cpp:749` | `return refuse( QString( "occluder %1 is listed in chunk %2 cell %3 but names instance %4, "` |
| the synthetic known-answer fixture | `src/lodifile.cpp:884` | `bool lodNativeFixtureWrite( const QString & dir, QStringList * report, QString * error )` |
| the fixtures CONE THAT MUST OPEN | `src/lodifile.cpp:1018` | `E( QStringLiteral( "lodo.mesh0.l0cluster0.coneOpen" ), QStringLiteral( "1" ) );` |
| the fixtures one hand-derived occluder | `src/lodifile.cpp:1093` | `keyHigh.hasOccluder = true;` |
| the object census and its hash law, in one function | `src/nativeemit.cpp:301` | `bool nativeObjectCensus( const EsmWorld & world, std::vector<quint32> * baseIdsOut,` |
| THE OCCLUDER FITTER, and the rule that shapes its constants | `src/nativeemit.cpp:162` | `NativeOccRefusal fitOccluderBox( const std::vector<float> & pos, const std::vector<quint32> & tris,` |
| ray parity along +X | `src/nativeemit.cpp:147` | `bool pointInSoup( const float pt[3], const std::vector<float> & pos, const std::vector<quint32> & tris )` |
| the whole voxel shaved off every side | `src/nativeemit.cpp:259` | `lo[k] = mn[k] + float( i0[k] ) * d[k] + d[k];` |
| the writers own hundred-point gate | `src/nativeemit.cpp:280` | `if ( !pointInSoup( pt, pos, tris ) )` |
| the LADDER flag set from the CLI switch | `src/nativeemit.cpp:582` | `lib.flags \|= LODO_FLAG_LADDER;` |
| the box offered per placement, for the mesh it DREW | `src/nativeemit.cpp:797` | `r.occMeshId = mi.value().meshId;` |
| the shadow-caster refusal at emit time | `src/nativeemit.cpp:631` | `the emit opened the silhouette and this object is a shadow caster` |
| the per-mesh report, version 3 | `src/nativeemit.cpp:910` | `"# lodgen native mesh report 3 ws "` |
| the ladder census line | `src/nativeemit.cpp:964` | `QString ladderLine = QString( "native-ladder: %1` |
| the occluder census line | `src/nativeemit.cpp:981` | `QString occLine = QString( "native-occluders: %1` |
| `--native-verify`: the ladder is a partition of its own surface | `src/nativeemit.cpp:1131` | `return fail( QString( "mesh %1 (%2): its root clusters account for %3 full-detail triangles but the mesh "` |
| the load-order hash law | `src/esmdata.cpp:873` | `quint64 EsmWorld::loadOrderHash() const` |
| the REFR form id is the load-order-mapped one | `src/esmdata.cpp:379` | `ref.formID = r->formID;` |
| the CLI: --native, --native-verify, --native-fixture | `src/nifcli.cpp:8101` | `else if ( t == QLatin1String( "--native" ) ) lgNativeDir = next();` |
| the CLI: the ladder switch (OFF by default since 2026-09-17; `--native-ladder` at 8108 turns it on) and the occluders' way back (8109) | `src/nifcli.cpp:8107` | `else if ( t == QLatin1String( "--native-no-ladder" ) ) lgNativeLadder = false;` |
| the emitter armed from the region driver | `src/nifcli.cpp:3859` | `lodgenNativeBegin( &world, lodgenFo4csWorldDir( nativeDir, world.worldspaceEdid() ),` |
| the partitioner joined the build | `NifSkope.pro:489` | `lib/meshoptimizer/src/partition.cpp` |
| the decoder reads the 48-byte ladder row | `tests/spells/lodgen_native_decode.py:179` | `L['clusterLods'] = [dict(zip(('cx', 'cy', 'cz', 'radius', 'geometricError', 'parentError',` |
| the decoder refuses a non-monotone ladder | `tests/spells/lodgen_native_decode.py:258` | `raise Refusal('cluster %d: geometricError %r > parentError %r -- the ladder is not monotone'` |
| the decoder walks the occluder ranges by cell | `tests/spells/lodgen_native_decode.py:522` | `raise Refusal('chunk %d cell %d: occluders start at %d, expected %d' % (ci, k, of, occCursor))` |
| the reference selector: the cut and its partition | `tests/spells/lodgen_native_cut.py:295` | `def partition_ok(L, mi, sel):` |
| the reference projection constant | `tests/spells/lodgen_native_cut.py:54` | `PROJECTION_SCALE = 960.0 / math.tan(math.radians(35.0))` |
| the cone floor that CAN fire | `tests/spells/lodgen_native_cut.py:264` | `coneWorstNarrow = max(coneWorstNarrow, (worstDot + 1.0e-3) - worstDot)` |
| the occluder box floor, grown until it leaks | `tests/spells/lodgen_native_cut.py:450` | `GROW = (1.1, 1.25, 1.5, 2.0)` |
| the mutation set: one per new v3 field | `tests/spells/lodgen_native_mutate.py:226` | `add('v3 lodo a child deviates more than its parent', 'lodo',` |
| the field gate reads the 25-column report | `tests/spells/lodgen_native_fields.py:126` | `t = line.split(None, 24)` |
| the spell: thirteen legs | `tests/spells/lodgen_native.sh:6` | `# Thirteen legs, in the order a failure is cheapest to read:` |
| the spell: the second region, because Sanctuary has no watertight mesh | `tests/spells/lodgen_native.sh:50` | `OCCREGION="${OCCREGION:-0 -12 11 -1}"` |

bungo's `.lodo`/`.lodi` naming ruling and the *Improved LOD* module ruling are
in `HANDOFF.md` (2026-09-09) and in
`E:\Projects\Fo4CommunityShaders\Codex\HANDOFF.md` (15:34 2026-09-09).

---

## Viewer -- opening a `.lodi` as a built document

`File > Open` on a `.lodi` BUILDS a Fallout 4 document, the way a `.btd` and a
`.lodl` do. The file stores no triangles: it is a worldspace's placement table,
and the geometry those placements point at is in the `.lodo` LIBRARY beside it.
So the route reads BOTH files and welds a scene out of them.

  * **The `.lodo` is found by worldspace stem**, `<stem>.lodo` in the `.lodi`'s
    own directory. Missing, and the open is refused in words naming the file it
    wanted -- never an empty document.
  * **The readers are `src/lodifile.cpp` and `src/lodofile.cpp`.** The builder
    (`src/lodinative.cpp`) contains no second parser of either format.
  * **The scene is shaped like a `.BTO`**, on purpose: one `NiNode` root, then
    one `BSTriShape` per **(base, material)** bucket, carrying every placement of
    that base welded into it in WORLD space, with the vertex layout the `.lodo`
    stores carried over -- position, normal, tangent/bitangent, UV and the UV2
    layer -- and the same `BSLightingShaderProperty` plumbing the chunk builder
    writes (`src/lodgen.cpp`). A node per placement would be thousands of nodes
    on one region and would answer no question a bucket does not.
  * **The material is the one the `.BTO` shape carries**, not a new one. The
    `.lodo` material strings are the bake machine's own paths
    (`...\Data\Materials\LOD\<name>.BGSM`); they are passed through with
    separators normalised and NOTHING prepended, because
    `Game::GameManager::get_full_path` finds the archive folder at any `/`
    boundary and erases everything before it -- a prepended `materials\` puts
    the folder at offset 0, the search stops, and the lookup MISSES. The BGSM
    goes in the shader block's **"Name"**, which is what
    `BSShaderLightingProperty::setMaterial` keys on, with the array sheets the
    bake wrote resolved through the usual resource roots.
  * **The vertex descriptor is the full-precision `0x0041B00000650407`**, not
    the `.BTO`'s half-precision object layout: a `.BTO` is chunk-local and its
    16-bit positions cannot hold region-space coordinates.
  * **Region** `WW_LODI_REGION="x0,y0,x1,y1"` in cells, inclusive; unset takes
    the file's whole extent. **Level** `WW_LODI_LEVEL=n` picks the cluster-ladder
    level, 0 (the default) being full detail; a mesh with fewer levels draws its
    own coarsest. Both are refused in words when they do not parse.
  * **The occluder boxes are drawn only under `WW_LODI_BOXES=1`**, and then as
    wire boxes -- they are not geometry the far field shows and a solid box would
    be read as a building.
  * **What it MEASURED is printed**, not just drawn: placements read, drawn,
    outside the region, with no mesh, with no geometry; bases, buckets, shapes,
    vertices, and the time. A picture is never the only evidence that the pair
    was understood.
  * **The instance census is written, not inferred.** `WW_LODI_DUMP=<file>` gets
    one row per placed instance -- `ref part base x y z scale mesh material
    level` -- because a gate cannot count placements by looking at merged
    geometry, and a screen coordinate is not carried between builds. That file
    is the left-hand side a harness compares against the `.lodi` reader and
    against the chunk manifest of the same bake.
  * **Read-only.** Nothing on this route writes a `.lodi`, a `.lodo` or any
    other bake output; `File > Save` on such a document goes to Save As, as it
    does for the other built documents.
  * **Together with the terrain.** `WW_LODL_OBJECTS=<file.lodi>` on a `.lodl`
    open appends these same objects under the terrain's root, defaulting to the
    terrain's own region, so one document carries both halves of the native bake
    (`docs/LODGEN_BTD_FORMAT.md`, "Lit from the `.lodt` sheets").
  * Gate: `tests/spells/native_open.sh`.

## Lighting -- the terrain sheets are MODEL-space normal maps

Measured 2026-09-16, lane NATIVEVIEW2, on Bethesda's own shipped sheets and on
this tree's renderer.

* **A terrain LOD normal sheet is an `_msn`: a MODEL-space normal map.** It
  carries all three components in the model's own axes, and it says so in the
  shape: `lodgen` sets Shader Flags 1 bit 12 (`SLSF1_Model_Space_Normals`,
  `LAND_SHADER_FLAGS1 = 0x80401000`), and `src/btdterrain.cpp` sets the same bit
  on every sheet-lit `.lodl` tile it builds.
* **The channel order, measured, not assumed:** `R = EAST (+x)`,
  `G = UP (+z)`, `B = NORTH (+y)`, range 0..255 mapping to -1..+1 with no sign
  flip, alpha a constant 255 that carries nothing, and texel row 0 = NORTH. Six
  vanilla `Commonwealth.16.*_msn.DDS` tiles were decoded and correlated against
  the heights of the same cells: R against `-dh/dx` 0.364 and against `-dh/dy`
  0.001; B against `-dh/dy` 0.422 and against `-dh/dx` 0.002; G's mean 238.6 of
  255. The same statistic with the sheet's rows NOT flipped collapses to
  0.060 / -0.063, which is the refuter for the row order. Two of the six tiles
  are flat and read 0.000 everywhere -- a constant sheet has no variance to
  correlate, so it is not evidence either way. This matches our writer
  (`lodgenTerrainMsnPixel`) and `res/shaders/sk_msn.frag`'s `.rbg` swizzle.
* **The viewer has a model-space path for it, and takes it only on bit 12.**
  `res/shaders/fo4_default.frag` transforms the texel by `normalMatrix` --
  model to view -- and by nothing else. There is no tangent frame in that path
  on purpose: a `.lodl` tile's tangent frame is arbitrary
  (`src/btdterrain.cpp`, `T = n x worldUp`, `B = n x T`), so reading the sheet
  as tangent-space sends its "up" along that arbitrary bitangent. That is what
  the dark blotches on the native terrain were. `src/gl/renderer.cpp`
  (`setupProgramCE1`) sets `hasModelSpaceNormals` from the shape's own bit 12,
  gated on the same test that decides whether a real normal map was bound, so
  with lighting or normal maps switched off the branch switches off too --
  otherwise `default_n`, a flat TANGENT-space texel, would decode as "north".
* **The legacy `.BTR` does NOT use that path, and this was measured.** Its
  `Land` shape is Shader Type 18, which `res/shaders/fo4_default.prog` excludes
  by condition, and the program scan hands it to `res/shaders/sk_msn.prog` --
  a model-space path already, the Skyrim one. The census that says so is
  `WW_PROGRAM_CENSUS=<absolute path>`, which writes one row per first-sighted
  (shape, program) pair plus the view-space light direction; on chunk (-20,24)
  it reads `shape="Land" bsver=130 msn=1 lodland=1 prog=sk_msn.prog`. Whether
  the two model-space paths agree with each other on brightness is NOT
  measured here and is open.
* Gate: `tests/spells/native_lighting.sh` (14 checks).

## Viewer -- every baked channel, one switch (`WW_LODL_CHANNEL`)

Added 2026-09-18, lane CHANVIEW1, on bungo's 06:0x round ("Beyond the AO, you will also
now show me on the same chunk: Leaf sway bake, identity bake, sky visibility bake") and his
06:1x follow-up ("show me also ground contact blend, ground cover, and roughness / metallic
or specular / gloss in this case, since these are baked from legacy textures and materials,
not .pbrm").

`WW_LODL_CHANNEL=<name>` paints ONE baked channel of the native pair, on the same seam
`WW_LODL_AO` is read at. **`WW_LODL_AO=1` is unchanged**: it is still exactly the `ao`
channel, byte for byte, and the gate asserts that (below). An unknown name is refused by the
name given, lists the known names, and draws nothing different.

The name is parsed once, by `lodlChannelFromEnv()` in `src/lodinative.cpp` (declared in
`src/lodinative.h`); the object side consumes it in `nifAppendLodiObjects`, the terrain side
in `src/btdterrain.cpp` through the existing sheet sampler
(`LodtSheets::sheetChannel( role, tx, ty, channel, ... )` -- the old blue-only `maskAo()` is
now one call into it, so the AO path and the channel path read the same texels by the same
code). `src/nifskope_ui.cpp` adds one line beside the `WW_LODL_AO` one: a non-empty
`WW_LODL_CHANNEL` also sets `Scene::DoVertexColors`, without which a painted flat byte is
never shown.

| name | what it paints | the byte, and where it is stored |
|---|---|---|
| `identity` | **the GROUP**, hashed with the stock channel-1 palette -- one colour a house (v7). On a file with no group table it falls back to the per-placement identity **and the note line says so by name**, rather than drawing the fallback silently | `.lodi` group table (§4.9) |
| `placement` | every placement its own colour -- **what `identity` drew before v7** | `.lodi` instance identity (§4.1c) |
| `identityraw` | that identity's low byte as grey | `.lodi` instance identity & 0xFF |
| `sky` | sky visibility: the **per-vertex stream** on a v7 file (§4.10), the flat per-placement byte on a v6 one. The note line names WHICH served, with its own count -- `per-vertex stream, N bytes over M slices` against `placement byte, N placements` -- and both numbers are read back from what was uploaded | `.lodi` sky stream (§4.10), else instance byte 0x11 |
| `ground` | ground-contact blend -- PLACEMENTS and TERRAIN in one grey ramp | `.lodi` instance byte 0x12; the terrain is drawn at the ramp's value at the surface, which is the constant 255, and the note line says so |
| `seed` | per-placement tree seed hashed to colour; **0 = not a tree = black** | `.lodi` instance byte 0x13 (§4.3) |
| `sway` | per-vertex wind-sway weight | `.lodo` library vertex byte 0x0E (§3.1) |
| `selfao` | per-vertex self-AO | `.lodo` library vertex byte 0x0F (§3.1) |
| `ao` | **identical to `WW_LODL_AO=1`** | v6 per-instance vertex AO (§4.8) x the placement-AO byte (§4.7); terrain from the mask sheet's B |
| `mask-r` | terrain **roughness** | `.lodt` role-5 (MASK/RMAOS) sheet, R |
| `mask-g` | terrain **metallic** | role-5 sheet, G |
| `mask-b` | terrain sky AO -- the channel `WW_LODL_AO` has always sampled | role-5 sheet, B |
| `mask-a` | terrain **ground cover** | role-5 sheet, A. **A BC1 sheet has no alpha at all**; the switch then says `ABSENT on this bake -- tile x,y is BC1 (dxgi N): it carries no alpha` and draws the default view |
| `emissive` | the role-6 emissive sheet bound as the terrain's base colour, texturing ON | `.lodt` role 6. Absent containers say `emissive sheet ABSENT -- <file> carries no sheet with role 6` |
| `normal` | the role-2 MSN sheet bound as the terrain's base colour, texturing ON | `.lodt` role 2 (the model-space normal map of the section above) |

`sky`, `ground`, `sway`, `selfao` and the four `mask-*` are a single byte written into all
three colour components, so the picture is a grey ramp and byte 128 is 128 grey. `identity`
and `seed` are hashed to a colour, because a ramp cannot separate thousands of neighbours.
`normal` and `emissive` are textures and are the only two that need texturing on -- use
`WW_LOD_CHANNEL=12` (raw base colour, unlit, no tone map) with them, and `WW_RENDER_FLAT=1`
with all the others.

**Every channel writes a note line, and every number in it is read back from what was
uploaded** -- the accumulators sit on the write into the vertex colour and on the decoded
sheet texels, never on the intent (the rule in §7's checklist). A terrain channel writes
TWO lines on purpose, over two different populations:

```
WW_LODL_CHANNEL=sky: the per-placement sky visibility (.lodi 0x11) from Commonwealth.lodi, 2446 placements read; min 0, max 255, mean 129.305
WW_LODL_CHANNEL=mask-r: terrain mask-r from the MASK SHEET'S R (...), 512 texels a cell, bilinear a vertex; 16 tiles read, 257 vertices without a tile (drawn open); values 99..231, mean 181.9
WW_LODL_CHANNEL=mask-r: over the sheet's own CONTENT texels, 4194304 texels of 16 tiles, values 66..239, mean 180.413
```

the first being the bilinear resample at the 129x129 grid vertices the viewer uploads --
which is what the picture is made of -- and the second the content-texel census over the
decoded tiles with their borders excluded, which is the file's own number and the one an
independent reader reproduces. Quoting the resample where the census is meant is how a
number misses by 1.5 and still looks right.

**What the mask sheet's numbers MEAN on a legacy bake, which is bungo's question.** For a
terrain layer `lodgen` passes the literal smoothness `1.0f` into the mask law
(`src/lodgen.cpp`, the LTEX texture-set resolve), so

```
roughness = 1 - lodgenLegacyGloss( 1.0, glossGreen ) = 1 - glossGreen
```

-- the baked roughness is one minus the vanilla `_s` map's GREEN channel, and nothing else.
Metallic is 0 unless a `.pbrm` supplied it, by bungo's ruling: a legacy material carries no
metallic to read, so a chunk of legacy layers is `constant 0 (legacy materials)` and is
reported as a constant rather than as a measurement. The per-layer rule census the bake
itself prints is the provenance for any given chunk, e.g.
`maskPbrm 0 maskLegacyInverted 17 maskRoughMaps 17 maskMetalMaps 0 maskDistinctLtex 17
maskLayerRefs 322 coverTiles 0 cover 0 emissive none sheets 3`.

**Gate**: `tests/spells/lodl_channels.sh` (48 checks), which for every name asserts that it
renders, that its note line is present with a population > 0 or the word `constant` or the
word `ABSENT` **by name**, that its render DIFFERS from the default render of the same
framing (a channel whose two renders are byte-identical is not wired, however good the
picture), that the note line's mean equals the independent Python reader's mean within 1
(`tests/spells/lodl_channels_table.py` over `lodgen_native_decode.py` and
`lodgen_vt_check.py`, sharing no code with the viewer), that `WW_LODL_AO=1` with
`WW_LODL_CHANNEL` unset is byte-identical to `WW_LODL_CHANNEL=ao`, and that an unknown name
is refused by name and renders byte-identically to the default. The two names this bake does
not carry (`mask-a`, `emissive`) have the inverted floor: they MUST be identical to the
default AND must say ABSENT by name.
