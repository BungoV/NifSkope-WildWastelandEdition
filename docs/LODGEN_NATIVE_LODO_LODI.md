# `.lodo` + `.lodi` v1 — the FO4CS-native far field

> **STATUS: SPEC. NOT YET WRITTEN.**
> No writer, no reader, no file. Nothing in `src/` emits either extension and
> nothing in FO4CS reads one. Every byte below comes from
> `scratchpad/specs_20260906/spec_fo4cs_native.md`, which is a design document,
> **not** from a writer — so this page has no provenance footer of writer lines
> and must not be treated as one. The first lane to write the container updates
> this page against its own code and adds that footer.

**bungo's FINAL ruling, 2026-09-09 ~16:4x**, which SUPERSEDES the ~16:1x
*"lodg sounds better"*: the object library is **`.lodo`**, `.lodg` is retired,
and the rest of the family is `.lodl` land, `.lodt` terrain textures, `.lodi`
instances, `.lodm` materials. The earlier exchange, kept because it is what the
rest of this page was written against:
*"Should we rename these to .lodo?"* — **the FO4CS-native far field is `.lodo` +
`.lodi`.** There is no interim `.lodo`. **`.bto` stays the stock bake** the
engine reads; these two files are the parallel native path, and the engine's own
LOD is the zero-effort fallback beneath them.

**bungo's ruling, 2026-09-09 15:34**, verbatim: *"This will be a new module for
Fo4cs, called Improved LOD"* — the consumer of everything in this folder is ONE
FO4CS module with its own master switch that ships off.

---

## 1. What the two files are, and what they retire

| file | holds | planned size, Commonwealth |
|---|---|---|
| `Data\Terrain\<WS>\Objects\<WS>.lodo` | the **geometry library**: base, mesh, cluster and material tables, a fixed-stride local-index blob, a vertex blob, a string blob. One row per distinct model, never per placement | ~7.0 MiB |
| `Data\Terrain\<WS>\Objects\<WS>.lodi` | the **instance tables**: a dense chunk table, a cell-range blob, a 24-byte instance record per placement, and a parallel cold record | ~5.1 MiB |

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
| material | `family`, `arrayClass`, `arraySet`, `layer` |
| instance | chunk index (north-up row-major), then cell index, then `refFormId`, then `scolPart` |

The base ordering being over the **full** census, not over the bases a region
bake happens to touch, is what makes `baseId` worldspace-stable.

---

## 3. `.lodo` header — 256 bytes at offset 0

| off | type | field |
|---|---|---|
| 0x00 | char[4] | magic `LODO` |
| 0x04 | u32 | version = 1 |
| 0x08 | u32 | flags — **bit0 must be 1** (vertex layout v1, stride 16); bit1 `PARTIAL` (rows present for a subset, absent rows all-zero, indices still worldspace-stable); all others reserved 0 |
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
| 0x78 | u64 | offset: mesh table (48 B) |
| 0x80 | u64 | offset: cluster table (16 B) |
| 0x88 | u64 | offset: material table (16 B) |
| 0x90 | u64 | offset: local-index blob (48 B per cluster) |
| 0x98 | u64 | offset: vertex blob (16 B per vertex) |
| 0xA0 | u64 | offset: string blob (NUL-terminated UTF-8) |
| 0xA8 | u32 | `indexCrc32` — over the **six tables and the string blob**, in file order |
| 0xAC | u32 | reserved, 0 |
| 0xB0 | u64 | `fileBytes` |
| 0xB8…0xFF | — | reserved, zero |

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

**Mesh entry — 48 B:** `f32 aabbMin[3]`, `f32 aabbExtent[3]`, `f32 uvMin[2]`,
`f32 uvExtent[2]`, `u32 clusterFirst`, `u16 clusterCount`, `u16 flags`
(bit0 anyAlphaTested, bit1 anySway).

**Cluster entry — 16 B:** `u32 vertexBase`, `u8 vertexCount` (≤ 48),
`u8 triangleCount` (≤ 16), `u16 materialId`, `u8 boundCentre[3]` (u8 into the
mesh AABB), `u8 boundRadius` (u8/255 of the mesh radius), `u16 meshId`,
`u16 flags` — **bits 0–1 are the draw size class** (0 = ≤ 4 tris, 1 = ≤ 8,
2 = ≤ 16), bits 2–15 reserved.

**Local-index blob:** a fixed **48 bytes per cluster** at `clusterIndex · 48`,
three u8 local indices per triangle, slots past `triangleCount` filled with
**0xFF** (a degenerate marker the vertex shader turns into a zero-area triangle).

**Material entry — 16 B:** `u8 arrayClass` (0 = 256², 1 = 128²), `u8 arraySet`,
`u16 layer` (**< 2048**, the D3D11 array-axis limit), `u8 family`
(0 legacy / 1 pbr), `u8 alphaThreshold` (0 = opaque; vanilla writes 128),
`u8 flags` (twoSided / emits / tree), `u8 reserved`, `f32 emissiveScale` (copied
from the `.lodm` and pinned against it by a gate), `u32 lodmStringOffset`.

**Base entry — 32 B:** `u32 formId`, `u32 modelStringOffset`, `u16 rep[4]` (per
MNAM slot 0–3: a mesh index, or **0xFFFF**), `u16 cardLayer` (**low 11 bits the
layer, high 5 bits the card array set**; 0xFFFF = no card), `u16 flags`,
`f32 boundRadius` (at scale 1, **never 0**), `u16 crossPx16[4]`.

**The two caps fight each other** at 1.843 vertices a triangle: a 16-triangle
patch of an open strip can need more than 48 vertices. The rule is **whichever
cap binds first closes the cluster**; `triangleCount` may end below 16; a
padding-ratio gate (≤ 2.0×) keeps a pathological mesh from shredding into
3-triangle clusters.

---

## 4. `.lodi` header — 256 bytes at offset 0

| off | type | field |
|---|---|---|
| 0x00 | char[4] | magic `LODI` |
| 0x04 | u32 | version = 1 |
| 0x08 | u32 | flags — bit0 `ROW_ORDER_NORTH_UP` (**clear = refusal**), bit1 `PARTIAL`, bit2 `NOLIB` (written before a `.lodo` exists; `lodoIdentity` must then be 0, and **a zero identity without this bit is a refusal**) |
| 0x0C | u32 | `headerCrc32` |
| 0x10 | u64 | `pluginCorpusHash` — must equal the `.lodo`'s |
| 0x18 | u64 | `objectCorpusHash` — must equal the `.lodo`'s |
| 0x20 | u64 | `lodoIdentity` — **FNV-1a 64 over the `.lodo`'s `headerCrc32`, `modelCorpusHash` and `objectCorpusHash`**, never an XOR (an XOR leaves the top 32 bits unmixed) |
| 0x28 | char[32] | worldspace editor ID |
| 0x48 | i16 ×4 | `chunkWest, chunkSouth, chunkEast, chunkNorth` — **inclusive, in chunk units** (`cellX >> 2`) |
| 0x50 | u16 | `chunkCells` = 4 |
| 0x52 | u16 | `instanceStride` = 24 |
| 0x54 | u32 | `chunkCount` = `(east−west+1)·(north−south+1)`, the dense table length — **capped at 65,536; above that the writer refuses, naming the extreme chunk** |
| 0x58 | u32 | `instanceCount` |
| 0x5C | u32 | `presentChunks` |
| 0x60 | u32 | `maxInstancesPerChunk` — so a consumer can size an append buffer statically |
| 0x64 | u32 | `indexCrc32` |
| 0x68 | u64 | offset: chunk table (32 B stride, **dense**, north-up row-major) |
| 0x70 | u64 | offset: cell-range blob (8 B per cell, **`chunkCells²` per present chunk**, in table order) |
| 0x78 | u64 | offset: instance blob (24 B) |
| 0x80 | u64 | offset: cold blob (8 B, parallel to the instance blob) |
| 0x88 | u64 | `fileBytes` |
| 0x90…0xFF | — | reserved, zero |

**Chunk table entry — 32 B:** `u32 instanceFirst`, `u32 instanceCount`,
`f32 zMin`, `f32 zExtent`, `f32 maxBoundRadius`, `u32 cellRangeOffset`,
`u32 crc32` (over this chunk's instance + cold records), `u32 reserved`.
X and Y of the chunk box are **implied by the table index**
(`chunkX·16384 … +16384`), so only Z is stated. **An absent chunk is 32 zero
bytes.**

`maxBoundRadius` is load-bearing: the chunk box is built from instance
**origins**, so an 8,224-unit tree's bound leaves the box and a per-chunk cull
against `zMin`/`zExtent` alone would pop it. The consumer expands the chunk box
by this value; the gate is `z + boundRadius ≤ zMin + zExtent + maxBoundRadius`
for every instance.

**Cell-range entry — 8 B:** `u32 instanceFirst`, `u32 instanceCount`.
**Cold record — 8 B:** `u32 refFormId`, `i16 scolPart` (−1 when not a SCOL
part), `u16 flags`. The cold blob is the `(ref, part)` key the `.bto` manifest
carries; a consumer that only draws never loads it.

### 4.1 The instance record — 24 bytes, fixed stride, 8-byte aligned

| off | size | field | encoding |
|---|---|---|---|
| 0x00 | 6 | `position` | 3 × u16 into the **chunk box**. X and Y span 16,384 world units; Z spans the chunk directory's `zMin`/`zExtent`. Step 0.250 u, worst 0.125 u = 0.017 px at D = 10,240 |
| 0x06 | 6 | `rotation` | 2-bit selector + 3 × 15-bit smallest-three quaternion (48 bits, **LSB-first over the three u16**). Worst 0.0146° = 0.07 px on a 2,000-unit crown at D = 10,240 |
| 0x0C | 2 | `scale` | u16, `scale = v / 8192`, range 0 … 7.99988 |
| 0x0E | 2 | `baseId` | u16 index into the `.lodo` base table |
| 0x10 | 1 | `ao` | u8 |
| 0x11 | 1 | `sky` | u8, sky visibility |
| 0x12 | 1 | `ground` | u8 ground-contact blend over the 256-unit ramp |
| 0x13 | 1 | `seed` | u8, the generator's **existing** position-derived hash — see §4.3 |
| 0x14 | 2 | `flags` | u16: bit0 mirrored, bit1 force-card, bit2 alpha-tested, bit3 emits, bit4 SCOL part, bit5 buried-cull candidate; **bits 6–15 reserved, and a set reserved bit is a refusal** |
| 0x16 | 2 | — | **reserved, must be 0.** The declared v2 growth slot (per-instance tint, or a light-record index), kept so the stride stays 24 B and 8-byte aligned |

**Position decode:**

```
x = chunkX·16384 + px/65535·16384
y = chunkY·16384 + py/65535·16384
z = zMin       + pz/65535·zExtent
```

**There is no identity field.** The instance's own index *is* its identity, in
the u32 domain. This deletes the 16-bit identity smuggled through vertex colour
R+G, whose ceiling is 65,536 placements a chunk and whose measured headroom on
the two densest dim-32 chunks was only 1.54× (42,560 and 42,641) — it wraps
silently.

**There is no `boundRadius` field.** `base.boundRadius × scale` is *more*
accurate than a 1-unit u16 and costs nothing per instance.

**The record carries no chunk id.** The loader walks the chunk table once and
writes a parallel `u32 chunkIndex[instanceCount]` buffer: 636 KiB of VRAM, zero
bytes of disk.

### 4.2 The two u16 refusals

`scale` maxes at **7.99988** and `baseId` at **65,535**. The measured corpus
reaches 4.970 and 3,400 bases, but a modded load order is the only environment
this ships into. **The writer computes both maxima over the whole census before
it writes a byte and refuses above range, naming the offending ref formId or
base editor ID.** A refusal, never a clamp and never a drop.

The harness must synthesise an out-of-range fixture for each: without the
fixture the floor is decoration, because no Commonwealth chunk contains the case.

### 4.3 `seed` is the existing position hash, and the order of operations is
part of the format

`seed` is the generator's own position-derived hash
(`treeHash = qRound(pos[0]) * 2654435761U + …`), which today drives a tree's yaw
**and** its UV mirror. The quaternion carries **the ESM rotation only**; the
consumer applies the yaw and the mirror from `seed`.

**Hash first, quantise second.** Hashing the quantised position would flip the
hash for some trees and swing their yaw by up to 360°.

Two ways to get this wrong, and a stock-to-stock gate observes neither: bake the
yaw into the quaternion and the ESM cross-check cannot run for trees (94.8% of
the far field); replace it with a `(refFormId, part)` hash and every tree in the
Commonwealth rotates differently from the stock bake with nothing to notice.

### 4.4 Mesh selection is by screen size, not by ring

`rep[0..3]` are MNAM's own four positional slots, filled by the existing
slot-fallback pick. Slot fallback fills all four for any base with at least one
authored LOD mesh. A base with **no** authored LOD mesh in any slot has
`rep[0..3]` all 0xFFFF and **must** have a `cardLayer`.

```
screenPxRadius = base.boundRadius × scale / (distance × 7.294e-4)
```

then walk `crossPx16` down the ladder. **`crossPx16` is a RADIUS in pixels, in
1/16 px units.** The constant is a **reference constant, not a format
constant**: `960/tan(35°) = 1371.0`, `1/1371.0 = 7.2939e-4`, assuming 1920 wide
and `fDefaultWorldFOV = 70`; **the consumer recomputes it from the live
projection.**

Do not select by ring: the measured 172× spread of bound heights inside one
chunk (47.8 … 8,224.1 u, median 1,086.4) makes a per-chunk distance wrong by two
orders of magnitude for most of its contents.

---

## 5. Refusal policy — hard for the generator, soft for the consumer

The generator refuses on anything wrong and names the field. **The consumer does
not**, because one of the keys is a hash of the user's load order, which changes
the first time any mod is installed or removed after a bake.

| class | keys | generator | consumer |
|---|---|---|---|
| **hard** | magic, version, `vertexStride`, `instanceStride`, `clusterMaxTris`, a set reserved bit, `ROW_ORDER_NORTH_UP` clear, `chunkCount` over cap, a zero `lodoIdentity` without `NOLIB`, any CRC mismatch | refuse, name the field | **refuse to load, and never hide the engine's own LOD tree** |
| **soft** | `pluginCorpusHash`, `objectCorpusHash`, `modelCorpusHash`, `cardCorpusHash` mismatch | refuse, name the field | **load anyway, log it, raise a `stale=1` census row, keep rendering** |

`objectCorpusHash` exists because the terrain hash is provably blind to objects:
`EsmWorld::vhgtCorpusHash` FNV-hashes the first VHGT field of each LAND record
and nothing else, so moving a REFR, rescaling it, adding a placement or
repointing a base's MNAM does not change it. `objectCorpusHash` is FNV-1a over
exactly what the object walk reads, in §2's sort order:
`(REFR formId, base formId, DATA position + rotation, XSCL, record flags)` and
`(STAT/SCOL MNAM slot paths, SCOL part transforms)`.

---

## 6. Region bakes

A region bake sets `PARTIAL` in **both** files. Absent rows are all-zero and
indices stay **worldspace-stable**, because §2's base ordering is over the full
census. A consumer merges every `*.lodi` in the folder by chunk key, last wins.

**The "a mod ships instances for its own cells" claim is withdrawn for v1**:
`lodoIdentity` cannot be computed by a mod that has not re-baked the whole
library; `baseId` is a u16 *index* into that library, not a formId; and the
instance's own index is its identity, which a merge would reindex. What v1
supports is **incremental region re-bakes of one install's own library**.

---

## 7. The reader's draw checklist

Rewritten from the spec's §8.3 as the sequence a consumer actually performs. The
layout is chosen so **FO4CS never creates a vertex or index buffer** — the one
D3D11 primitive the tree has never used.

**Before any of this**, a state-save spike must record what the engine's deferred
prepass has bound: the MRT set (albedo / normal / specular / motion + main
depth), the depth-stencil state and stencil reference the deferred lighting pass
reads back, the viewport, the rasteriser state including LOD's own cull mode and
depth bias, and **whether the depth buffer is reversed-Z** — that last decides
`SV_DepthGreaterEqual` vs `SV_DepthLessEqual`, and the card pixel shader must use
one of them, never plain `SV_Depth`, or the single all-cards draw becomes the
most overdraw-exposed draw in the frame.

1. **Open and validate.** Both headers, both `headerCrc32`, both `indexCrc32`,
   the hard/soft split of §5. `lodoIdentity` must match unless `NOLIB`.
2. **Upload two StructuredBuffers** — the library and the instances — plus
   append and indirect-args buffers sized from `maxInstancesPerChunk` and
   `maxClustersPerMesh`.
3. **Derive `chunkIndex[instanceCount]`** by walking the chunk table once. It is
   not in the file.
4. **Cull, one Dispatch over all instances.** Frustum-test with
   `base.boundRadius × scale`; expand each chunk box by its `maxBoundRadius`;
   step the ladder from `crossPx16` against the live projection constant; append
   `(clusterId, instanceId)` pairs per bucket and one `DrawInstancedIndirect`
   args row. Needs an append/counter UAV, `CopyStructureCount` and
   `D3D11_RESOURCE_MISC_DRAWINDIRECT` — all stock D3D11, none of it present in
   FO4CS today.
5. **Draw, `DrawInstancedIndirect` per bucket**, seated at
   `DeferredPrePass_Post`. `SV_VertexID` is the local index slot, `SV_InstanceID`
   picks the pair, the vertex shader pulls the vertex from the StructuredBuffer.
   **No IA, no input layout, no index buffer.** A `0xFF` local index emits a
   zero-area triangle.
6. **Three size classes, not one fixed 48.** `vertexCountPerInstance` is
   **12 / 24 / 48**, chosen by the cluster's `flags` bits 0–1. At an average
   18.05 real vertices a cluster, a flat 48 is 2.66× the vertex shading of an
   indexed draw with no post-transform reuse by construction. The cost of the fix
   is a few more draws.
7. **Bucket key** = (draw size class × family × arrayClass × arraySet × alpha
   state) for clusters, and (family × card class × card set) for cards.
   Measured today that is **8–14 mesh draws plus 1–2 card draws** for the whole
   worldspace, against 343–1,795 resident engine draws.
8. **The shadow pass is a second complete consumer**, not a sentence: the far
   shadow map runs the same cull and the same bucket set **once per slice it
   renders**, and needs our own VS+PS for a depth-only pass matching the engine's
   depth format, constant and slope bias and cascade view-projection. **Quote
   the main-view draw count and the shadow draw count together** or the
   comparison against 343–1,795 is not a comparison.
9. **Order of operations when suppressing the engine's far field:**
   validate → build buffers → read back a **non-zero drawn-primitive count for
   one frame** → **only then** hide `spLODObjectRoot`. Nothing may hide the
   engine's far field before ours is known to work.
10. **The census must accuse its own plumbing.** With no `.bto` in the scene the
    existing packed/colours-only/vanilla census reports "vanilla" forever. The
    replacement row is
    `FarField: native drawn=<n> culled=<n> overflow=<n> | not observing: no .bto in scene`,
    and it lands in the **same wave** as the drop.

**Residency: read once, deliberately not streamed.** ~63.8 MiB for one
worldspace (12.07 buffers + 0.62 chunkIndex + 17.75 mesh arrays + 30.39 cards +
~3.0 scratch), against 125.9–370.0 MiB of resident object geometry today.
**Textures are 76% of it**; geometry is the smallest term. One worldspace
resident at a time; the previous one is released on worldspace change.

**v1 is loose-files-only by design** — FO4CS's DDS loading is two filesystem
paths and nothing in the tree reads a BA2. A `BSResourceNiBinaryStream` path is a
named v2 item.

---

## 8. What this format deletes by construction

Today's 16-bit index cap fires in four places, and one of them **silently drops
object geometry**: the ring-3 chunk at (−32,0) has four shapes sitting at
65,535 / 65,534 / 65,529 / 65,424 vertices and **2,628 of 42,560 placements
(6.17%) have no geometry in the file at all**; downtown (0,−32) loses 882 of
42,641 (2.07%). The loss is order-dependent, not importance-dependent.

In the native format nothing is stitched: there is no per-material bucket to
fill, the only index domains are a cluster's u8 local index over ≤ 48 vertices
and a u32 `vertexBase` over ~361k library vertices, and the heaviest LOD model in
the Commonwealth is 2,244 triangles — three orders below any ceiling. If a model
ever did exceed a cluster's addressing, **the writer refuses and names the
model**.

**Asserting this is not testing it.** The reconstruction gate must run on
**(−32,0) dim 32** and assert an *asymmetric* difference: the set of placements
with ≥ 1 triangle in `.lodo`+`.lodi` minus the set with ≥ 1 triangle in the
`.BTO` must be **non-empty (≥ 2,000 members, each verified 0-in-BTO)**, and the
reverse difference **empty**.

And the census gate must mean something: "instances written == census
placements" is vacuous when *census* is our own `placed` counter, which already
increments for a placement whose every shape was dropped. Split it: (i)
`instanceCount` equals an **independent ESM census**, exactly; (ii) every
instance whose base has a non-0xFFFF `rep` resolves to ≥ 1 cluster with ≥ 1
triangle, and the count of instances with neither geometry nor a `cardLayer`
is **0**.

---

## 9. Sample files

**None, and none are possible**: there is no writer. See
`scratchpad/handoff_fo4cs/README.md` §5 for what a first lane must produce.

---

## Source

`scratchpad/specs_20260906/spec_fo4cs_native.md` (118,557 bytes, copied out of
`%TEMP%` on 2026-09-09): §3.2 the instance record, §3.3 the library, §3.4 mesh
selection, §4 the material story, §8 the container, §8.1 the `.lodo` header,
§8.2 the `.lodi` header, §8.3 the D3D11 draw, §8.4 residency, §8.5 the four
conflicts, §8.6 the refusal split, §8.7 region bakes.
bungo's `.lodo`/`.lodi` naming ruling and the *Improved LOD* module ruling are
in `HANDOFF.md` (2026-09-09) and in
`E:\Projects\Fo4CommunityShaders\Codex\HANDOFF.md` (15:34 2026-09-09).

**Nothing on this page has been checked against a writer, because there is no
writer.** Every number is the spec's own, and the spec labels its own
provenance tags (`[par]`, `[pri]`, `[cei]`, `[nat]`, `[arith]`, `[con]`,
`[inv]`) — consult it before quoting any of them as measured.
