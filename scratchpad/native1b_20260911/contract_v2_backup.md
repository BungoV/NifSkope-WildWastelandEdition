# `.lodo` + `.lodi` v2 — the FO4CS-native far field

> **STATUS: BUILT INTO THE EXE, AND A REAL WORLDSPACE PAIR EXISTS (2026-09-11, lane NATIVE1a).**
> The emitter is hooked into `lodgenBuildObjectChunk`, the CLI carries `--native`,
> `--native-verify`, `--native-verify-corpus`, `--native-mesh-report` and
> `--native-fixture`, and `release/NifSkope.exe` has written the nine-chunk Sanctuary
> region: `Commonwealth.lodo` **5,692,388 B** and `Commonwealth.lodi` **126,512 B**
> for **3,526 placements**, which is the stock manifests' placement count exactly.
> The gate is `tests/spells/lodgen_native.sh` (nine legs, 13/0), and under it the
> independent decoder reads the fixture (56/0) and the real pair (87/0), 24 single
> mutations are refused by name, the 25 v2 field checks pass with their floors, and
> the stock `.BTO` set is byte-identical with `--native` off over 25 files.
> **What is still not done:** nothing in FO4CS reads either file; there is no
> reconstruction path that turns a pair back into a renderable mesh, so parity is
> shown as coverage numbers and a point-set picture rather than a side-by-side
> render; the LOD Generation panel has no `.lodo`/`.lodi` row (lane LODUI1); and the
> cluster HIERARCHY — per-cluster screen error, bounds, normal cones, occluders —
> is lane NATIVE1b and is v3.
>
> **The v1 page said "AS BUILT, NOT YET BUILT INTO THE EXE".** That was already
> wrong when it was read: lane BUILD6 applied the hook-up and built it on
> 2026-09-10 03:57 and only the report's own tail said so. The STATUS block is the
> first thing a reader trusts and it is now the thing this lane keeps current.

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

## 0. What v2 changed, and why v1 is refused

Five rulings of 2026-09-11 land in the bytes. Every one of them is a field that is
WRITTEN and that MOVES, with a test and a floor (the three rules of 2026-09-04
21:33); the tests are named beside each.

| # | his words | what it became | where |
|---|---|---|---|
| a | *"Add it"* (the staleness hash) | **`loadOrderHash`**, a u64 in BOTH headers — `.lodo` 0xB8, `.lodi` 0x90 — beside the object corpus hash that was already there. `--native-verify-corpus` recomputes all three from the plugin and refuses a stale pair, naming the file and the field | §3, §4, §8 |
| b | *"the instance table must be joinable to the engine's placed REFR (formID in the .lodi record)"* | the **cold record** already carries `refFormId` at the instance's own index, load-order-mapped; the law is stated and gated rather than duplicated into the hot record — §4.1a says why the stride stayed 24 | §4.1a |
| c | *"per-instance bound radius"* | the rule is **`base.boundRadius × scale`**, no stored float; the writer now takes the base radius and applies the **quantised** scale so `maxBoundRadius` is a true upper bound, and a `scale` of 0 is a refusal | §4.1b |
| d | *"5 sounds good"* — instances pre-sorted by mesh then material | **ONE sort law**: chunk, cell, **`drawKey`**, ref, part. `drawKey` is a u16 in the record's declared v2 growth slot | §2, §4.1 |
| e | *"vertex/triangle order for the GPU cache on every .lodo mesh"* | `meshopt_optimizeVertexCache` + `meshopt_optimizeVertexFetchRemap` at emit, **keeping whichever of the two orders reads better per shape**; header flag bit 2 says so | §3.3 |
| f | *"our shadow casters are based on the color id ... the identity channel must survive"* | the cold record's second word is the **stock bake's identity index** (the manifest's `index`, R + G·256 of the `.bto` colour) | §4.1c |
| g | *"a far shadow cast by a LOD tower behind me will cover the area I'm at"* | the emitter **refuses** a mesh whose emitted boundary-edge count exceeds its source's, and prints both per mesh | §3.4 |

**A v1 file is refused by both readers, by name**, and not merely version-checked:
v1 wrote zero into the two words v2 uses, so a v2 reader that accepted it would
read "draw rank 0, identity 0" for every placement and silently mis-key the
far-shadow pass and the draw order. Re-bake.
---

## 1. What the two files are, and what they retire

| file | holds | measured, 9-chunk Sanctuary | planned, Commonwealth |
|---|---|---|---|
| `Data\Terrain\<WS>\Objects\<WS>.lodo` | the **geometry library**: base, mesh, cluster and material tables, a fixed-stride local-index blob, a vertex blob, a string blob. One row per distinct model, never per placement | **5,692,388 B** (2,970 bases, 2,982 meshes, 10,634 clusters, 273,695 vertices, 142,138 triangles, 136 materials) | ~7.0 MiB planned. The library is built from the FULL worldspace census in every bake, so it is already near its final size at nine chunks and will not grow much with region size |
| `Data\Terrain\<WS>\Objects\<WS>.lodi` | the **instance tables**: a dense chunk table, a cell-range blob, a 24-byte instance record per placement, and a parallel 8-byte cold record | **126,512 B** for 3,526 placements = **35.9 B a placement** | ~5.1 MiB planned; 35.9 B a placement reaches 5.1 MiB at ~149,000 far placements, which is the placement count the spec's figure was taken at |

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
| cluster | `meshId`, then `materialId`, then first triangle index |
| material | `family`, `arrayClass`, `arraySet`, `layer`, then the material's string |
| instance | **chunk index** (north-up row-major), then **cell index**, then **`drawKey`**, then `refFormId`, then `scolPart` |

The base ordering being over the **full** census, not over the bases a region
bake happens to touch, is what makes `baseId` worldspace-stable.

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
to a run list — is a v3 format break and is his call, not this lane's.

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
| 0x04 | u32 | **version = 2** (version 1 is refused by name) |
| 0x08 | u32 | flags — **bit0 must be 1** (vertex layout v1, stride 16); bit1 `PARTIAL`; **bit2 `CACHE_ORDER`** (v2, §3.3); all others reserved 0 |
| 0x0C | u32 | `headerCrc32`, over 0x10…0xFF |
| 0x10 | u64 | `pluginCorpusHash` — the terrain writers' hash, carried for the object/terrain/plugin triple |
| 0x18 | u64 | `objectCorpusHash` — see §8 |
| 0x20 | u64 | `modelCorpusHash` — over every source model read (path, size, content) |
| 0x28 | u64 | `cardCorpusHash` — over the card candidate list, N, tile class and every card source model |
| 0x30 | char[32] | worldspace editor ID, NUL-padded |
| 0x50 | u32 | `baseCount` |
| 0x54 | u32 | `meshCount` |
| 0x58 | u32 | `clusterCount` |
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
| 0xA8 | u32 | `indexCrc32` — over the **six tables and the string blob**, in file order |
| 0xAC | u32 | reserved, 0 |
| 0xB0 | u64 | `fileBytes` |
| **0xB8** | **u64** | **`loadOrderHash` (v2)** — §8.1. A reader gets it from the header alone, without parsing a table |
| 0xC0…0xFF | — | reserved, zero |

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

**Mesh entry — 56 B (AS BUILT; the spec said 48):** `f32 aabbMin[3]`,
`f32 aabbExtent[3]`, `f32 uvMin[2]`, `f32 uvExtent[2]`, `u32 clusterFirst`,
`u16 clusterCount`, `u16 flags` (bit0 anyAlphaTested, bit1 anySway),
`u32 modelStringOffset` (the LOD model path — the table's own sort key),
`u32 reserved` (0). See Deviations 1.

**Cluster entry — 16 B:** `u32 vertexBase`, `u8 vertexCount` (≤ 48),
`u8 triangleCount` (≤ 16), `u16 materialId`, `u8 boundCentre[3]` (u8 into the
mesh AABB), `u8 boundRadius` (u8/255 of the mesh radius), `u16 meshId`,
`u16 flags` — **bits 0–1 are the draw size class** (0 = ≤ 4 tris, 1 = ≤ 8,
2 = ≤ 16), bits 2–15 reserved.

**Local-index blob:** a fixed **48 bytes per cluster** at `clusterIndex · 48`,
three u8 local indices per triangle, slots past `triangleCount` filled with
**0xFF** (a degenerate marker the vertex shader turns into a zero-area triangle).

**Material entry — 16 B:** `u8 arrayClass` (0 = 256², 1 = 128²), `u8 arraySet`,
`u16 layer` (**< 2048**, or 0xFFFF = no array layer assigned), `u8 family`
(0 legacy / 1 pbr), `u8 alphaThreshold` (0 = opaque; vanilla writes 128),
`u8 flags` (twoSided / emits / tree), `u8 reserved`, `f32 emissiveScale`,
`u32 lodmStringOffset`.

**Base entry — 32 B:** `u32 formId`, `u32 modelStringOffset`, `u16 rep[4]` (per
MNAM slot 0–3: a mesh index, or **0xFFFF**), `u16 cardLayer` (**low 11 bits the
layer, high 5 bits the card array set**; 0xFFFF = no card), `u16 flags`,
`f32 boundRadius` (at scale 1, **never 0** — the reader refuses it), `u16 crossPx16[4]`.

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

**What it bought, measured, and it is small.** Over the 2,982 meshes:

| number | before | after |
|---|---|---|
| ACMR at a 16-entry post-transform cache, triangle-weighted | **1.8600** | **1.8585** |
| overfetch (`meshopt_analyzeVertexFetch`) | 1.0097 | 1.0097 |
| library vertices the region's `.lodo` carries | 273,969 (v1, source order) | **273,695** |
| `.lodo` bytes | 5,696,484 | **5,692,388** |
| meshes the pass changed at all | — | **45 of 2,982** |

Bethesda's shipped LOD meshes are already close to cache-optimal, so the honest
statement is that the pass costs nothing, can no longer hurt, and buys about
**0.08% of vertex-shader invocations and 4,096 bytes** on this region. It is
reported as what it is, not as a win.

### 3.4 The shadow-caster silhouette rule (v2)

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

`--native-mesh-report <file>` writes one line per mesh:
`meshId triangles srcVerts emitVerts acmrBefore acmrAfter atvrBefore atvrAfter
boundarySrc boundaryEmitted model`.

---

## 4. `.lodi` header — 256 bytes at offset 0

| off | type | field |
|---|---|---|
| 0x00 | char[4] | magic `LODI` |
| 0x04 | u32 | **version = 2** (version 1 is refused by name) |
| 0x08 | u32 | flags — bit0 `ROW_ORDER_NORTH_UP` (**clear = refusal**), bit1 `PARTIAL`, bit2 `NOLIB` |
| 0x0C | u32 | `headerCrc32` |
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
| 0x64 | u32 | `indexCrc32` |
| 0x68 | u64 | offset: chunk table (32 B stride, **dense**, north-up row-major) |
| 0x70 | u64 | offset: cell-range blob (8 B per cell, **`chunkCells²` per present chunk**, in table order) |
| 0x78 | u64 | offset: instance blob (24 B) |
| 0x80 | u64 | offset: cold blob (8 B, parallel to the instance blob) |
| 0x88 | u64 | `fileBytes` |
| **0x90** | **u64** | **`loadOrderHash` (v2)** — must equal the `.lodo`'s; a mismatch is a pairing refusal naming both files |
| 0x98…0xFF | — | reserved, zero |

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
| 0x0C | 2 | `scale` | u16, `scale = v / 8192`, range 0 … 7.99988. **A stored 0 is a refusal** (§4.1b) |
| 0x0E | 2 | `baseId` | u16 index into the `.lodo` base table |
| 0x10 | 1 | `ao` | u8 |
| 0x11 | 1 | `sky` | u8, sky visibility |
| 0x12 | 1 | `ground` | u8 ground-contact blend over the 256-unit ramp |
| 0x13 | 1 | `seed` | u8 = `treeHash & 0xFF` (0 for a non-tree): sway phase / jitter only, **not** the yaw — see §4.3 |
| 0x14 | 2 | `flags` | u16: bit0 mirrored, bit1 force-card, bit2 alpha-tested, bit3 emits, bit4 SCOL part, bit5 buried-cull candidate; **bits 6–15 reserved, and a set reserved bit is a refusal** |
| **0x16** | **2** | **`drawKey` (v2)** | the base's (primary mesh, that mesh's first material) rank, §2.1. v1 called this word `reserved` and wrote 0 |

**Cold record — 8 B, parallel to the instance blob:** `u32 refFormId`,
`i16 scolPart` (−1 when not a SCOL part), **`u16 identity` (v2)**.

**Position decode:**

```
x = chunkX·16384 + px/65535·16384
y = chunkY·16384 + py/65535·16384
z = zMin       + pz/65535·zExtent
```

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
if bungo wants the formID in the hot record anyway, it is a v3 stride change and
the reader refuses the 24-byte stride by name.**

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
themselves"* — the far-shadow pass keys on the identity index, so it must survive
into the native output.

There are two identities and the difference matters:

* **The format's own per-placement identity is the instance INDEX**, a u32,
  unique across the whole file by construction. It replaces the 16-bit identity
  the stock path smuggles through vertex colour R+G, whose ceiling is 65,536
  placements a chunk and whose measured headroom on the two densest dim-32
  chunks was only 1.54× — it wraps silently.
* **`cold[i].identity` is the STOCK bake's index** for that placement: the
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

### 4.2 The two u16 refusals

`scale` maxes at **7.99988** and `baseId` at **65,535**. The measured corpus
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

### 4.4 Mesh selection is by screen size, not by ring

`rep[0..3]` are MNAM's own four positional slots. A base with **no** authored LOD
mesh in any slot has `rep[0..3]` all 0xFFFF and **must** have a `cardLayer`.

```
screenPxRadius = base.boundRadius × scale / (distance × 7.294e-4)
```

then walk `crossPx16` down the ladder. **`crossPx16` is a RADIUS in pixels, in
1/16 px units.** The constant is a **reference constant, not a format
constant**: `960/tan(35°) = 1371.0`, assuming 1920 wide and
`fDefaultWorldFOV = 70`; **the consumer recomputes it from the live projection.**
bungo's screen-size spec of 2026-09-11 10:4x states the runtime side of this in
Unity's terms — projected size as a fraction of SCREEN HEIGHT, one threshold per
fade class, ~20% hysteresis — and that is FO4CS's, not the bake's.

Do not select by ring: the measured 172× spread of bound heights inside one chunk
(47.8 … 8,224.1 u, median 1,086.4) makes a per-chunk distance wrong by two orders
of magnitude for most of its contents.

---

## 5. Refusal policy — hard for the generator, soft for the consumer

The generator refuses on anything wrong and names the field. **The consumer does
not**, because three of the keys are hashes of the user's data, which change the
first time any mod is installed or removed after a bake.

| class | keys | generator | consumer |
|---|---|---|---|
| **hard** | magic, **version (1 is refused by name)**, `vertexStride`, `instanceStride`, `clusterMaxTris`, a set reserved bit, `ROW_ORDER_NORTH_UP` clear, `chunkCount` over cap, a zero `lodoIdentity` without `NOLIB`, **a `scale` of 0**, **a `drawKey` out of order or not the base's rank**, any CRC mismatch | refuse, name the field | **refuse to load, and never hide the engine's own LOD tree** |
| **soft** | `pluginCorpusHash`, `objectCorpusHash`, `modelCorpusHash`, `cardCorpusHash`, **`loadOrderHash`** mismatch | refuse, name the field | **load anyway, log it, raise a `stale=1` census row, keep rendering** |

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

---

## 7. The reader's draw checklist

Unchanged from v1 in substance — the layout is chosen so **FO4CS never creates a
vertex or index buffer**:

1. **Open and validate.** Both headers, both `headerCrc32`, both `indexCrc32`,
   the hard/soft split of §5. `lodoIdentity` must match unless `NOLIB`.
2. **Upload two StructuredBuffers** — the library and the instances — plus
   append and indirect-args buffers sized from `maxInstancesPerChunk` and
   `maxClustersPerMesh`.
3. **Derive `chunkIndex[instanceCount]`** by walking the chunk table once. It is
   not in the file: 636 KiB of VRAM, zero bytes of disk.
4. **Cull, one Dispatch over all instances.** Frustum-test with
   `base.boundRadius × scale`; expand each chunk box by its `maxBoundRadius`;
   step the ladder from `crossPx16` against the live projection constant.
5. **Draw, `DrawInstancedIndirect` per bucket**, seated at
   `DeferredPrePass_Post`. **No IA, no input layout, no index buffer.**
6. **Three size classes, not one fixed 48.** `vertexCountPerInstance` is
   **12 / 24 / 48**, chosen by the cluster's `flags` bits 0–1.
7. **Bucket key** = (draw size class × family × arrayClass × arraySet × alpha
   state) for clusters. Measured today that is **8–14 mesh draws plus 1–2 card
   draws** for the whole worldspace, against 343–1,795 resident engine draws.
   **v2 makes those buckets contiguous per cell**, so building them is a walk
   rather than a scatter.
8. **The shadow pass is a second complete consumer**, not a sentence. **Quote
   the main-view draw count and the shadow draw count together.**
9. **Order of operations when suppressing the engine's far field:**
   validate → build buffers → read back a **non-zero drawn-primitive count for
   one frame** → **only then** hide `spLODObjectRoot`.
10. **The census must accuse its own plumbing.** The replacement row is
    `FarField: native drawn=<n> culled=<n> overflow=<n> | not observing: no .bto in scene`.

**Residency: read once, deliberately not streamed.** ~63.8 MiB for one
worldspace, against 125.9–370.0 MiB of resident object geometry today.
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

---

## 9. What this format deletes by construction

Today's 16-bit index cap fires in four places, and one of them **silently drops
object geometry**: the ring-3 chunk at (−32,0) has four shapes at
65,535 / 65,534 / 65,529 / 65,424 vertices and **2,628 of 42,560 placements
(6.17%) have no geometry in the file at all**; downtown (0,−32) loses 882 of
42,641 (2.07%). The loss is order-dependent, not importance-dependent.

In the native format nothing is stitched: the only index domains are a cluster's
u8 local index over ≤ 48 vertices and a u32 `vertexBase` over ~274k library
vertices, and the heaviest LOD model in the Commonwealth is 2,244 triangles.
If a model ever did exceed a cluster's addressing, **the writer refuses and names
the model**.

**On this region the census gate is exact rather than vacuous:**
`instanceCount` = 3,526 = the stock manifests' placement row count, with
**0 dropped for a base outside the table** and **0 instances with neither
geometry nor a card** (`--native-verify`). The asymmetric drop proof on
(−32,0) dim 32 is **still owed** — it needs a `--slot-fallback --native` bake of
that chunk and was not run by this lane.

---

## 10. Sample files, and the gate

**The synthetic known-answer pair**, written by the exe and checked by the
independent decoder against answers printed before the bytes:
`Synthetic.lodo` 28,903 B (3 bases, 2 meshes, 3 clusters, 2 materials,
32 vertices, 32 triangles), `Synthetic.lodi` **16,424 B** (**5 instances** in 3 of
6 dense chunks), `Synthetic.expect.txt` 2,175 B.
`lodgen <esm> --native-fixture <dir>` writes it.

**v2 gave the fixture the two instances the order rule needed.** The v1 fixture
had one instance a chunk, so the `(cell, ref, part)` rule was checked by both
readers and never fired. It now puts three instances in ONE cell of chunk (0,0),
and the one with the **smallest ref** carries the **higher draw rank** — so ref
order alone would put it first and the sort law puts it last. The fixture
discriminates the two laws instead of being consistent with both.

The fixture library deliberately does **not** set `CACHE_ORDER`: its vertex
expectations are hand-derived from the source order and a permutation would make
them underivable. The cache order is proved on the real worldspace bake instead,
per mesh, with a floor.

**`tests/spells/lodgen_native.sh`** is the gate, nine legs, `13 checks, 0
failures`: the fixture and the decoder (**56/0**), two-write byte identity, the
refusal set (**24/0**, one mutation per row rule and one per v2 field, every CRC
re-signed so the RULE answers), the real region bake, the stock-path byte
identity (**25 files, 0 differ**), the decoder's ESM and manifest legs on the
real pair (**87/0**), `--native-verify --native-verify-corpus`, the v2 field gate
(`lodgen_native_fields.py`, **25/0**, one subsection a field with its floor), and
the staleness floor.

---

## 11. Deviations from the spec, as built

1. **Mesh row 56 B, not 48.** The spec's row carried no model path, so the mesh
   table's sort law could not be checked in the file. `modelStringOffset` + a
   reserved word were added: +8 B × ~3,355 meshes = 26.8 KiB.
2. **The rotation is the drawn rotation; `seed` is the hash's low byte.**
   `treeHash % 360` needs 9 bits, the u8 holds 8. §4.3.
3. **The cell index inside a chunk is defined** (north-up row-major,
   `(3 − ly)·4 + lx`); the spec named the sort key and not the numbering.
4. **`material.layer` may be 0xFFFF** ("unassigned") in v1/v2; the spec's `< 2048`
   holds for every assigned layer and the reader refuses 2048..0xFFFE.
5. **The bake writes what the stock ring bakes and nothing more:** no card layer
   (`cardLayer` = 0xFFFF everywhere, `cardCorpusHash` = 0), no `crossPx16` (0),
   `selfAO` = 255, `arrayClass`/`arraySet` = 0 with `layer` unassigned. A base
   with no loadable LOD model in any slot is left OUT of the base table and its
   instances are counted (`dropped for a base outside the table` in the census
   line) rather than written with neither mesh nor card. Lanes OBJM/OBJC/OBJP
   fill those fields.
6. **(v2) The mesh/material sort is inside the CELL, not inside the chunk**, and
   the reason is the 8-byte cell-range row. §2.1. bungo's to overrule.
7. **(v2) The placed REFR formID stayed in the cold record**, at the instance's
   own index, instead of growing the hot record to 32 bytes. §4.1a. bungo's to
   overrule.
8. **(v2) `cold.identity` is unique per STOCK CHUNK**, not per `.lodi` chunk; the
   per-placement identity is the instance index. §4.1c.

---

## 12. Reserved room, named, for lane NATIVE1b (v3)

NATIVE1b builds the cluster hierarchy bungo took on 2026-09-11 08:0x — per-cluster
screen error with the ladder running **from full detail down**, per-cluster
bounding sphere and normal cone, precomputed occluder boxes per cell — plus the
aggregate ring-3 impostors. The room it inherits, stated so it is not re-derived:

| what | where it goes | free today |
|---|---|---|
| per-cluster geometric error, level id, parent link | the cluster row grows past 16 B, or a **parallel cluster-error table** at a new header offset | `.lodo` header **0xC0…0xFF = 64 reserved bytes**, enough for eight u64 offsets |
| cluster normal cone | `LodoCluster` has **bits 2–15 of `flags`** free (14 bits) plus the growth the same parallel table gives | — |
| the ladder's screen-size steps per base | `LodoBase.crossPx16[4]` is **written as 0 today** | 8 B a base, already in the row |
| occluder boxes per cell | a new `.lodi` table at a new header offset | `.lodi` header **0x98…0xFF = 104 reserved bytes** |
| per-instance tint / light-record index | **gone** — v2 took the record's 0x16 word for `drawKey`. A v3 field here means a 32-byte stride, and the reader must refuse 24 by name | — |
| `PARTIAL` merge and `NOLIB` | unchanged | — |

A v3 that changes a stride or a table set bumps the version and **refuses v2 by
name**, the way v2 refuses v1.

---

## Source, as built

Rewritten 2026-09-11 by lane NATIVE1a under `ww-contract-provenance`: the source
hashes below were taken FIRST, every line number in the anchor table was then
found again from its own anchor text by
`scratchpad/native1a_20260911/anchors.py` (which exits 1 if any anchor is not
found exactly once), and the version constants were re-read last. The six native
files and the three test drivers are this lane's; `src/lodgen.cpp` and
`src/nifcli.cpp` carry the hook-up lane BUILD6 applied on 2026-09-10.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodofile.h` | `dc420419c412108d` | 14,374 | 308 |
| `src/lodofile.cpp` | `dc07c412e6a750c4` | 43,300 | 966 |
| `src/lodifile.h` | `907d257a981c8d58` | 13,182 | 299 |
| `src/lodifile.cpp` | `c1fa5023696595f7` | 47,320 | 939 |
| `src/nativeemit.h` | `a0b95e7aa9b57e5b` | 4,848 | 100 |
| `src/nativeemit.cpp` | `47f672eb1c7b9b59` | 32,112 | 769 |
| `src/esmdata.h` | `a60733e1eba326d4` | 12,652 | 300 |
| `src/esmdata.cpp` | `87a7519ec268c55a` | 29,570 | 932 |
| `src/lodgen.cpp` | `c05fd079655ac03e` | 391,673 | 8,924 |
| `src/nifcli.cpp` | `fa27a52cd669ec0d` | 277,101 | 6,324 |
| `tests/spells/lodgen_native_decode.py` | `3e73ae2e3c8f25c3` | 38,034 | 760 |
| `tests/spells/lodgen_native_mutate.py` | `b9a889e4ccab5047` | 10,213 | 237 |
| `tests/spells/lodgen_native_fields.py` | `8fd7a3a05280b59e` | 15,503 | 325 |
| `tests/spells/lodgen_native.sh` | `3e786936bc69beaa` | 8,963 | 177 |

Exe that wrote the measurements on this page: `release/NifSkope.exe`
**2026-09-11 08:49:08**, 20,989,440 B, sha256
`140bb8d879cb2cf39c837bd957e56efc5991e2cd3923ba0d8d138a5f83dd01d3`.
The bake, the fixture, the logs and the coverage picture are under
`scratchpad/native1a_20260911/`.

| claim | line | anchor |
|---|---|---|
| `.lodo` magic, version 2 and the header size | `src/lodofile.h:51` | `constexpr quint32 LODO_MAGIC = 0x4F444F4CU;` |
| the cache-order flag | `src/lodofile.h:67` | `LODO_FLAG_CACHE_ORDER = 4` |
| the 16-byte vertex | `src/lodofile.h:93` | `struct LodoVertex` |
| the 56-byte mesh row (Deviation 1) | `src/lodofile.h:116` | `quint32 modelStringOffset;  //!< the LOD model path this row was built from` |
| the 16-byte cluster row | `src/lodofile.h:121` | `struct LodoCluster` |
| the 16-byte material row | `src/lodofile.h:134` | `struct LodoMaterial` |
| the 32-byte base row | `src/lodofile.h:148` | `struct LodoBase` |
| `loadOrderHash` at .lodo header 0xB8, and its law | `src/lodofile.h:203` | `quint64 loadOrderHash = 0;          //!< v2, header 0xB8` |
| the per-mesh statistics the gate reads | `src/lodofile.h:255` | `struct LodoMeshStats` |
| the strides pinned at compile time | `src/lodofile.h:161` | `static_assert( sizeof( LodoVertex ) == 16` |
| `.lodo` header field offsets, and 0xB8 / 0xC0 | `src/lodofile.cpp:40` | `constexpr int H_LOADORDER = 0xB8, H_RESERVED_C0 = 0xC0;` |
| octahedral 12:12 pack | `src/lodofile.cpp:115` | `quint32 lodoPackOct12( const float n[3] )` |
| the boundary-edge count, welded by quantised position | `src/lodofile.cpp:234` | `quint32 lodoBoundaryEdges( const std::vector<quint32> & tris, KeyFn key )` |
| the GPU cache order, and KEEPING THE BETTER of the two | `src/lodofile.cpp:400` | `if ( cacheOrder && ca.acmr > cb.acmr ) {` |
| meshopt_optimizeVertexCache then the fetch remap | `src/lodofile.cpp:368` | `meshopt_optimizeVertexFetchRemap( remap.data(), tmp.data(), tmp.size(), nv );` |
| whichever cap binds first closes the cluster | `src/lodofile.cpp:526` | `if ( idx.size() / 3 >= LODO_CLUSTER_MAX_TRIS` |
| payloads 4,096-aligned, pad zeroed by hand | `src/lodofile.cpp:602` | `const quint64 at = alignUp( start, LODO_PAYLOAD_ALIGN );` |
| `.lodo` reader: version 1 refused by name | `src/lodofile.cpp:716` | `return refuse( QStringLiteral( "version 1: the v1 vertex blob is in SOURCE order` |
| `.lodo` reader: reserved header bytes refused by offset | `src/lodofile.cpp:768` | `return refuse( QString( "reserved header byte at 0x%1 is not zero" )` |
| `.lodo` reader: the base table sort law | `src/lodofile.cpp:906` | `return refuse( QString( "base table is not sorted by formId ascending at row %1` |
| `.lodi` magic and version 2 | `src/lodifile.h:67` | `constexpr quint32 LODI_MAGIC = 0x49444F4CU;` |
| THE ONE SORT LAW, stated in the header | `src/lodifile.h:36` | `*  THE ONE SORT LAW (v2, 2026-09-11, lane NATIVE1a)` |
| the 24-byte instance record, 0x16 = drawKey | `src/lodifile.h:123` | `quint16 drawKey;` |
| the cold record: the placed REFR and the stock identity | `src/lodifile.h:166` | `quint16 identity;` |
| the bound radius is the base radius at scale 1 | `src/lodifile.h:217` | `float boundRadius = 0.0f;` |
| `.lodi` header field offsets, and 0x90 / 0x98 | `src/lodifile.cpp:35` | `constexpr int H_LOADORDER = 0x90, H_RESERVED_98 = 0x98;` |
| smallest-three 2 + 3 x 15, LSB-first | `src/lodifile.cpp:108` | `void lodiPackRotation( const float m[9], quint16 out[3] )` |
| the cell index inside a chunk (Deviation 3) | `src/lodifile.cpp:166` | `return ( LODI_CHUNK_CELLS - 1 - ly ) * LODI_CHUNK_CELLS + lx;` |
| the sort: chunk, cell, drawKey, ref, part | `src/lodifile.cpp:290` | `return std::make_tuple( chunkIdx[a], cellIdx[a], A.drawKey, A.refFormId, A.scolPart )` |
| maxBoundRadius from the QUANTISED scale | `src/lodifile.cpp:312` | `maxR = std::max( maxR, r.boundRadius * qs );` |
| the scale refusal, naming the ref, before a byte is written | `src/lodifile.cpp:226` | `return fail( QString( "ref 0x%1 part %2 (base %3): scale %4 is outside 0 ..` |
| `.lodi` reader: version 1 refused by name | `src/lodifile.cpp:447` | `return refuse( QStringLiteral( "version 1: the v1 record's word at 0x16` |
| `.lodi` reader: a scale of 0 is a refusal | `src/lodifile.cpp:606` | `if ( r.scale == 0 )` |
| `.lodi` reader: the (cell, drawKey, ref, part) order | `src/lodifile.cpp:618` | `return refuse( QString( "instance %1: out of (cell, drawKey, ref, part) order` |
| the synthetic known-answer fixture | `src/lodifile.cpp:702` | `bool lodNativeFixtureWrite( const QString & dir, QStringList * report, QString * error )` |
| the fixture's two extra instances that EXERCISE the order rule | `src/lodifile.cpp:864` | `LodiSrcInstance keyLow;      // baseId 1 -> mesh B, material 0 -> drawKey 1` |
| the object census and its hash law, in one function | `src/nativeemit.cpp:98` | `bool nativeObjectCensus( const EsmWorld & world, std::vector<quint32> * baseIdsOut,` |
| the base table is the FULL worldspace census, formId ascending | `src/nativeemit.cpp:141` | `std::sort( baseIds.begin(), baseIds.end() );` |
| the cache-order flag is set on every emitted library | `src/nativeemit.cpp:369` | `lib.flags \|= LODO_FLAG_CACHE_ORDER;` |
| the load-order hash carried into both files | `src/nativeemit.cpp:371` | `lib.loadOrderHash = world.loadOrderHash();` |
| THE DRAW RANK: one rank a distinct (mesh, material) pair | `src/nativeemit.cpp:471` | `std::vector<quint16> baseDrawKey( lib.bases.size(), 0 );` |
| the shadow-caster refusal at emit time | `src/nativeemit.cpp:418` | `the emit opened the silhouette and this object is a shadow caster` |
| the stock identity carried into the cold record | `src/nativeemit.cpp:547` | `r.identity = quint16( p.objectIndex );` |
| the per-mesh report file | `src/nativeemit.cpp:599` | `# lodgen native mesh report 1 ws` |
| the census line the writer prints | `src/nativeemit.cpp:620` | `QString line = QString( "native: %1.lodo %2 bytes` |
| `--native-verify`: the three staleness hashes recomputed | `src/nativeemit.cpp:687` | `if ( world ) {` |
| `--native-verify`: the drawKey rank checked against the library | `src/nativeemit.cpp:716` | `std::vector<quint16> wantKey( lib.bases.size(), 0 );` |
| the load-order hash law | `src/esmdata.cpp:873` | `quint64 EsmWorld::loadOrderHash() const` |
| the REFR form id is the load-order-mapped one | `src/esmdata.cpp:379` | `ref.formID = r->formID;` |
| the CLI: --native, --native-verify, --native-fixture | `src/nifcli.cpp:5782` | `else if ( t == QLatin1String( "--native" ) ) lgNativeDir = next();` |
| the CLI: --native-mesh-report and --native-verify-corpus | `src/nifcli.cpp:5785` | `else if ( t == QLatin1String( "--native-mesh-report" ) ) lgNativeMeshReport = next();` |
| the emitter armed from the region driver | `src/nifcli.cpp:3430` | `lodgenNativeBegin( &world, nativeDir, lodgenNativeLoadModel,` |
| the decoder reads the 56-byte mesh row | `tests/spells/lodgen_native_decode.py:153` | `le('ffffffffffIHHII', b, h['offMeshes'] + i * 56)` |
| the decoder reproduces the load-order hash | `tests/spells/lodgen_native_decode.py:227` | `def load_order_hash(plugin_list):` |
| the decoder recomputes the (mesh, material) rank | `tests/spells/lodgen_native_decode.py:241` | `def draw_key_ranks(L):` |
| the decoder budgets the manifest's own print step | `tests/spells/lodgen_native_decode.py:261` | `def print_step(token):` |
| the generator hash the record derives from | `src/lodgen.cpp:3324` | `treeHash = ( quint32( qRound( r.pos[0] ) ) * 2654435761U )` |
| the yaw multiplied into the drawn rotation | `src/lodgen.cpp:3329` | `xf.rotation = xf.rotation * rz;` |
| the placement handed to the emitter, one a ring | `src/lodgen.cpp:3438` | `lodgenNativeAddPlacement( np );` |
| the identity index read before AO overwrites B | `src/lodgen.cpp:3702` | `lodgenNativeLighting( chunkX, chunkY, dim,` |

bungo's `.lodo`/`.lodi` naming ruling and the *Improved LOD* module ruling are
in `HANDOFF.md` (2026-09-09) and in
`E:\Projects\Fo4CommunityShaders\Codex\HANDOFF.md` (15:34 2026-09-09).
