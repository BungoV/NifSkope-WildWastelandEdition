# LODGEN vertex packing — the contract Community Shaders reads

**CS is the consumer.** Everything below is written by `lodgen` into the
`.bto`/`.btr` and read back by the FO4CS LOD module. This file is the interface;
change it on both sides or not at all.

**The hard constraint is that stock Fallout 4 must render these files
correctly with the CS module OFF.** That is the charter's zero-effort fallback,
and it is why every slot below is either a field the vanilla shaders do not
sample, or a field they sample only behind a flag we leave clear. A packing that
is merely "unused by us" is not good enough — it has to be *inert to the stock
engine*.

Status: **SHIPPED** = written today and verified. **DECIDED** = agreed, not yet
written. **FREE** = unused, reserved.

---

## Objects — `.bto`

Identity profile. `--no-identity` gives **`0x0001B00000430205`**, 20 bytes,
carrying none of this. With identity: **`0x0003B00005430206`**, 24 bytes. With
identity AND the extra channels (the default, `objectChannels`):
**`0x0013F07006543208`**, **32 bytes**, UV2 and Eye Data added. Never hardcode
any of it — see the offsets note below.

> **Two of those three constants were wrong on this page until 2026-09-09**, and
> the same wrong hex is still in the source comments beside the constants it
> annotates (`src/lodgen.cpp:1358-1359` say `0x1B00000650405` and
> `0x3B00000650406`; the *integer values* are right, only the hex comments lie —
> `scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md` item 1). A reader who
> trusted the old numbers would have put UV at +16, the normal at +20 and no
> colour channel at all. Everything here is decoded from the writer's own
> integer constants through `BSVertexDesc::ResetAttributeOffsets` — see
> **The descriptors, decoded** at the end of this file.

| slot | width | holds | status |
|---|---|---|---|
| Vertex Colors **R + G** | 16 bits | per-chunk object index, `index = R + G*256` | **SHIPPED** |
| Vertex Colors **B** | 8 bits | baked ambient occlusion | **SHIPPED** |
| Vertex Colors **A** | 8 bits | tree sway weight, 0 trunk base → 1 branch tip | **SHIPPED** |
| UV2.x | ~11 bits | sky visibility: fraction of the upper hemisphere that reaches open sky | **SHIPPED** |
| UV2.y | ~11 bits | texture-array layer index, an integer; the shape's array is the `A` line for its block in the manifest | **SHIPPED** |
| Eye Data | 32 bits | ground-contact blend: 1 at the terrain surface, 0 by 256 world units above it | **SHIPPED** |

With the extra channels the stride is **32 bytes**, not 24 — built from the
flags by `ResetAttributeOffsets`, never hardcoded. **Colours move from +20 to
+24** when UV2 is present, because UV2 is laid out ahead of them. Read every
offset from the descriptor: `GetAttributeOffset(VA_COLOR) = (desc >> 22) & 0x3C`.
A reader that remembers +20 samples normal and tangent bytes as an object id and
reports cross-welded triangles that do not exist — which is exactly what the
terrain harness did until it was fixed.

Sky visibility is NOT ambient occlusion under another name: AO is
cosine-weighted about the surface normal and answers "how enclosed is this
point", sky visibility is normal-independent and answers "can weather and
skylight land here". A vertical wall face has low AO and high sky visibility; a
gully floor has the reverse. Both are quantised by their ray counts — nine
directions, so nine steps.

### The index is exact, not a hash

`R + G*256`, assigned in placement order, unique within the chunk. Verified on
Sanctuary (-20,24) dim 4: **678 distinct indices, 678 manifest rows, range
0–677, none missing either way, and zero indices split into more than one
spatially separate blob**. Decode it as an integer; do not fuzzy-match colours.

### Texture arrays

FO76 hangs texture arrays off its instanced LOD node; ours are files beside
the chunks, and the stock engine never reads them. `lodgenBuildTextureArrays`
(the `--arrays` flag, the panel's *Texture arrays* row) gathers every diffuse
the chunks reference — tiling or not, which an atlas cannot — groups them by
size, and writes one DX10 BC3 array per size class with real mips
(`<ws>.LodgenArrays.<W>x<H>_d.DDS`) and the normals as a second array (`_n`,
alpha = smoothness, resampled to the diffuse's size). The layer goes into
**UV2.y** of every vertex of the shape, and the chunk's manifest gets
`A <shape block> <layer> <array .lodm path>` per shape, so a chunk stays
self-contained after the atlas pass repoints its diffuse (the arrays run
BEFORE the atlas). A sidecar `<ws>.LodgenArrays.txt` (**version 5**:
`family class layer lodm color normal mask emissive source emissiveScale`) lists
every layer with its source paths — **the contract is
`docs/LODGEN_TEXTURE_ARRAYS.md`; this paragraph said version 4 until
2026-09-09**. Measured on Sanctuary (-20,24)..(-19,25): one class, 256×256,
eight layers, both arrays exactly header + 8 × mip chain, every vertex of all
14 shapes carrying its layer (`tests/spells/lodgen_texture_arrays.sh`).

### What the far-ring simplifier owes this table

`lodgenSimplifyFarRings` decimates rings 2 and 3 after the merge, and every
slot above has to survive it. Two of them are INDICES and one is a layer, so
the pass groups triangles by (identity index, UV2.y layer) and simplifies each
group alone: no collapse crosses an object or a texture layer, and because
meshoptimizer creates no vertices the survivors are a SUBSET of the originals
— the identity index, the AO byte, the sway byte, the sky fraction and the
ground blend all arrive as they were written, never blended. The quantities
also ride as weighted attributes so the metric prefers to keep them. Every
group is asked for at least two triangles and restored whole if the simplifier
returns nothing, so **the set of identity indices in a chunk is invariant under
the pass** — a manifest row can never point at an object that is no longer
there. Alpha-tested shapes and impostor cards are not cut at all.

That invariant is the reason a consumer may key on the manifest across rings:
`(ref, part)` still resolves at ring 2 and ring 3 after the cut.

### Impostor cards and texture arrays: the LOD texture spec

The channel contract for the mesh LOD arrays and the octahedral impostor sheets
is `docs/LODGEN_IMPOSTOR_SPEC.md`: three BC3 textures and a BC1 emissive in
one of two FAMILIES,
described by a `.lodm` (our LOD material) beside every set — legacy,
vanilla-sourced: `_d` (diffuse, coverage), `_n` (normal X, Y, height, sway),
`_gsaos` (gloss, specular, AO, subsurface mask), `_g` (emissive); pbr, from a
source `.lodm`:
`_bc`, `_n`, `_rmaos` (roughness, metallic, AO, subsurface mask), `_e`
(emissive). What follows
is the history of how the card packing got there; the spec page is the
contract.

### Impostor cards, octahedral (superseded packing, kept for the record)

A base whose MNAM slot for a ring is empty stands on an impostor when a card
library is given. Two crossed quads with the front|side sheet stay in the mesh
for the stock engine. Beside them, when the bake made them (`OCT=N` on
`tools/bake_impostor_cards.sh`, `WW_IMPOSTOR_OCT`), three sheets per base,
`<id>_oct.DDS`, `_oct_n.DDS`, `_oct_ds.DDS`, an N × N grid of views over the
upper hemisphere under the hemi-octahedral mapping, frames on the grid's
VERTICES (`u = i/(N−1)`, `v = j/(N−1)` mapped to [-1,1]²; `x = (u+v)/2`,
`y = (u-v)/2`, `z = 1−|x|−|y|`, normalised), so the corners are exact horizon
directions, the centre frame the exact top, and a direction always falls inside
a triangle of three frames — the (N−1)² triangle mesh between frame centres is
the blending rule:

| sheet | RGB | A |
|---|---|---|
| `_oct.DDS` (BC3) | albedo, UNLIT (lighting off, texturing on), un-premultiplied | coverage from the two-pass matte |
| `_oct_n.DDS` (BC3) | geometric normal in the VIEW's space (measured: opposite views differ in red by 42, top and horizon agree in blue), half-packed, shader channel 8 | ambient term from the depth tile |
| `_oct_ds.DDS` (BC5) | R: window depth, channel 9; `units = (R/255 − 0.5) × depthspan`, 0.5 is the card plane | G: sway weight, `h²·(0.35+0.65·r)` from the pixel's height and radius |

Frames are fitted, not sphere-sized: a first pass photographs every view at
the bound-sphere fit and takes the widest and tallest silhouette extent from
the centre over all of them; the second pass bakes at that fit, and frames
are RECTANGULAR in the silhouette's aspect (the longer side is the tile size,
the other follows, in BC blocks), one size for every view. The sidecar line
is `oct N tileW tileH halfW halfH cx cy cz depthspan`. The manifest gets
`C <index> <cx> <cy> <cz> <halfW> <halfH> <N> <depthspan> <sheet base>` after
the row of every placement on such a card; a consumer
picks the three views nearest its direction, blends them with the depth for
parallax, writes depth for the true silhouette, and warps by the sway weight.
Gate: `tests/spells/lodgen_octahedral.sh` (a real bake, N=4). Measured on
`TreeMapleForest02_LOD_1` at N=4, 64 px: frame 590 × 856 units in 44 × 64
pixels (the sphere fit was 1085 × 1085 in 64 × 64), 16 distinct views,
205–408 covered pixels a frame, best view 14.5% (5.3% at the sphere fit; a
maple is mostly gaps between leaf cards at that size), normals in view space
(opposite views differ in red by 53), 98 distinct AO and 105 distinct depth
values in one frame, sway 130 at the crown against 2 at the base, 863 card
placements in the far chunk (-32,16).

### Identity across rings and bakes: the key is the reference

The index is per chunk and per ring. The **stable key** of an object is the
placed *reference*, carried in the manifest row's `ref` column, with `part`
naming a SCOL part's ordinal inside its reference (`-1` for a plain ref). Not
the base — every copy of a maple shares one — and not the index. Measured on
Sanctuary (-20,24): **471 of the chunk's 678 objects are SCOL parts**, so a
bare reference key would have collapsed most of the chunk; the dim-8 chunk
that contains it shares **406 objects by (ref, part), all 406 with the same
base and position**, and two bakes of one chunk are byte-identical, file and
manifest (`tests/spells/lodgen_identity.sh`).

Row: `index base type x y z scale class height ref part`. The first line is
`# lodgen manifest 2 ws <edid> dim <d> chunk <x> <y> columns …`; `I` lines are
instance groups (base, model, count, member indices); `A` lines are texture
array layers (shape block, layer, array). A consumer pairing rings builds an
index-to-key table per loaded chunk and matches on `(ref, part)`.

### How the sway weight is computed

`h² × (0.35 + 0.65·r)`, in the placement's **own local space** so a leaning or
rotated tree still reads 0 at its own base rather than at whatever happens to be
its lowest world point. Height is squared because a trunk is a cantilever —
deflection grows faster than linearly with height — and the radial term
separates a branch tip from the trunk at the same height.

**Normalised across ALL of a placement's shapes, never per shape.** A tree is
two shapes split by texture (`ElmTrunksLOD_d`, `ElmBranchesLOD_d`), and
normalising each on its own would make a branch card's own base read 0 and put a
step change at the join. Measured on a Sanctuary-area chunk, that shows up
directly:

| shape | alpha > 0 | bottom quartile → top |
|---|---|---|
| `ElmTrunksLOD_d` | 86% | 4.6 → 72.6 |
| `ElmBranchesLOD_d` | **100%** | 42.8 → 97.3 |
| `MapleTrunksLOD_d` | 88% | 6.2 → 96.2 |
| `MapleBranchesLOD_d` | **100%** | 78.4 → 148.4 |
| `BlastedForestTrunksLOD_d` | 85% | 6.1 → 129.2 |
| Shack / Siding / Warehouse | **0%** | — |

Trunks are ~86% non-zero because they contain the ground contact that reads 0;
branches are wholly non-zero because they sit above it and inherit the trunk's
scale. That contrast is the guard in
`tests/spells/lodgen_tree_sway.sh`, along with the rise-with-height check that a
constant or inverted channel would fail.

Anything that is not a tree carries an **explicit zero**, so a consumer applying
the channel blindly to a shack does nothing rather than something wrong.

### Why sway is in A and not UV2

Vertex alpha *does* multiply into the FO4 alpha test —
`a = C.a * baseMap.a * alpha` in `fo4_default.frag` — and the branch cards are
the alpha-TESTED geometry (`ElmBranchesLOD`, `MapleBranchesLOD`, threshold 128).
That looks fatal, and is not: the engine only takes vertex alpha when
**`SLSF1_Vertex_Alpha` (flags1 bit 3)** is set, and it is **clear in vanilla LOD
and in ours** — measured `0x80400001` on every BSLightingShaderProperty in both
`Commonwealth.4.-20.24.BTO` and our output. So the byte is inert to stock FO4
and free for us.

> **The condition, and it is load-bearing:** this holds only while that flag
> stays clear. Anything that sets `SLSF1_Vertex_Alpha` on a LOD shape turns the
> sway weight into a discard mask and eats the branch cards from the inside out.
> If a generator option ever needs that flag, sway moves to UV2.x that day.

A costs no stride at all, which is why sway went there even though the profile
has since widened to 32 for sky visibility and ground blend. That widening makes
the **stock-engine tolerance gate** — still unrun — a bigger measurement than
it was, not a different one: the question was always whether stock FO4 tolerates
a fatter object desc, and the answer covers 32 as it would have covered 24.
Until it is run, every channel here is unproven in the engine, however well
measured in the file.

### Movement type is NOT in the mesh

A tree's movement type (leaf vs branch, and its parameters) is **per material**,
and the LOD buckets are already split per source texture —
`ElmBranchesLOD_d` and `ElmTrunksLOD_d` are separate shapes with separate
texture sets. So the type rides a `.pbrm` beside the LOD texture and costs no
vertex data; the mesh carries only the scalar amplitude, and the phase comes
from `hash(index)` at draw time.

> **`--atlas` breaks this.** It repoints six of this chunk's eight buckets onto
> one sheet, so five tree textures collapse to a single path and the per-material
> distinction is gone. Measured. Use `--atlas` only where per-material tree
> motion is not wanted, or move to FO76-style texture arrays, where each source
> texture keeps its own layer.

---

## Terrain — `.btr`

`LAND_VERTEX_DESC` = **`0x0000300000000203`** — position + UV, **12 bytes** —
plus flags, extended only when the CS profile is on. The full-profile value is
**`0x0012705004003206`**, 24 bytes.

> This page said `0x300000000303` until 2026-09-09, and so does the source
> comment at `src/lodgen.cpp:58`. The *integer* is right (52,776,558,133,763);
> the hex is not, and the wrong digit is the UV offset — `0x…303` puts UV at
> +12 where the writer puts it at +8, on a 12-byte vertex.

| slot | width | holds | status |
|---|---|---|---|
| Vertex Colors **R** | 8 bits | dominant LTEX material class | **SHIPPED** |
| Vertex Colors **G** | 8 bits | flow-accumulation wetness | **SHIPPED** |
| Vertex Colors **B** | 8 bits | heightfield ambient occlusion | **SHIPPED** |
| Vertex Colors **A** | 8 bits | shore proximity: 1 at/below the water plane, falling with height above it (512 units) and distance from water (32 samples ≈ 4096 units) | **SHIPPED** — terrain is now FULL |
| UV2.x (`VF_UV_2`) | ~11 bits | sky visibility, before byte quantisation | **SHIPPED** |
| UV2.y | ~11 bits | second-strongest material class, for two-material blending | **SHIPPED** |
| Eye Data (`VF_EYEDATA`) | 32 bits | geomorph weight: WORLD-unit height delta to the parent ring's surface | **SHIPPED** (`--geomorph`, dim < 32; dim 32 has no parent and stores 0) |

Terrain and objects deliberately do **not** share a layout: terrain has no
object index, objects have no material class.

**Terrain has no free slots left.** Anything further needs either a wider desc
or a per-chunk texture.

### The per-chunk data map — `<chunk>_data.DDS`

**The copy that gets shaded.** 512² per chunk, alongside the diffuse and
`_msn` bakes. **BC1 with alpha unused when the chunk has no ground cover,
BC3 with alpha = cover and a `'WWCV'` stamp when it has** (see below):

| channel | holds | distinct values, harbour chunk |
|---|---|---|
| R | ambient occlusion | 114 |
| G | flow-accumulation wetness | 125 |
| B | shore proximity | 135 |
| A | ground cover, or nothing at all: BC1 = no cover, DXT5 + the `'WWCV'` stamp = the alpha is cover | 0 or up to 255 |

**Why it exists.** Every one of these is computed on the 129²-per-chunk
heightfield and was then stored on the DECIMATED Land mesh — about 1180
vertices for a whole dim-4 chunk, some 480 world units apart. That is a 3.75×
downsample of the source, and what reaches the screen is triangle interpolation
between those vertices, not occlusion. At 512² the same data gets ~15× the
linear detail of the vertex path.

AO is recomputed at TEXTURE resolution, marched in world space against bilinear
heightfield samples, so it is genuinely smooth rather than the 129² lattice
upsampled. Wetness and shore are bilinear upsamples, because flow accumulation
is inherently grid-based and recomputing it per texel would be a different
algorithm, not a finer one.

**BC1 until something needs the slot; BC3 for the chunks that do.** Two
paragraphs of this file used to contradict each other and the code — one said
BC3 at ~350 KB, one said BC1 at 175 KB, and the file on disk settled it at
174,888 bytes of DXT1. The truth now has a switch in it: a chunk with no
cover is BC1 at **174,888 bytes**, byte for byte what this generator has
always written, and a chunk with cover is BC3 at **349,648**. When it is
BC3 the alpha is also the most precise slot in the file — two 8-bit
endpoints and a 3-bit index per 4×4 block, where the colour channels get
5:6:5.

**There is no alpha channel, and that is a result rather than an omission.**
Four candidates were tried and measured; each failed one of two tests — is it
**derivable** from what we already ship, and does it have **operands at its own
resolution**?

| candidate | verdict |
|---|---|
| sky visibility | it IS AO. `lodgenTerrainChannels` writes `skyVis[i] = vis` and `ao[i] = vis * 255` from one horizon measure — **measured r = 0.969** against R. The AO/sky distinction is real for OBJECTS, whose surfaces face every way; on a heightfield every normal points up and it collapses. |
| slope | independent (r = −0.65) but **recoverable** as `acos(n.z)` from `_msn`. Its only edge is precision below ~15°, which BC1 normals quantise to flat. |
| water depth | not derivable, but the **water mesh already carries it** per vertex on a mesh that refines toward the shoreline, and what land wants at the waterline is shore proximity — which is channel B. |
| material blend weight | not derivable, and it fixes a real gap (`matClass`/`matClass2` say which two materials meet, never in what proportion). But the **diffuse already composites the layers**, so colour needs nothing; and the class ids it would weight are per-VERTEX, so a 512² weight has no operands at its own resolution. |

A **fifth** candidate was tried in 2026-09-06 and is the first to pass both
tests: **ground cover**, the `LTEX → GNAM → GRAS` chain composited against the
cell's splat paint and gated on slope (`docs/LODGEN_TERRAIN_VT.md` §2).

| candidate | verdict |
|---|---|
| ground cover | **not derivable from anything shipped** — it needs LTEX `GNAM` and the GRAS `DATA` block, records nothing in this tree had ever read, and it is orthogonal to R by construction (R comes from the heightfield, cover from the splat). **Every operand exists at 512²**: `D(ltex)` is a per-FORM scalar and the opacity it multiplies is the 17×17 grid the diffuse loop already samples bilinearly per texel. It CONSUMES slope rather than storing it — the gate reads `acos(n.z)` from the normal computed in the same iteration — and it is a smooth scalar, so it tolerates the BC interpolation that would have made a class ID wrong. Gated by `tests/spells/lodgen_ground_cover.sh`: `\|r\|` against R, G, B and against the `_msn` Z must each be **< 0.5**, and a least-squares fit of cover on the slope angle alone must leave **> 50%** of the variance unexplained. |

So the map is **BC1 at 174,888 bytes a chunk where there is no grass, and BC3
at 349,648 where there is**. The blend weight is still
computed in `lodgenTerrainChannels` behind an optional out-parameter — it costs
nothing and belongs per-VERTEX beside the classes it weights, if a consumer
wants it.

**The provenance stamp, and why alpha alone is not a switch.** A DXT5
`_data.DDS` from any other tool — xLODGen, a mod, or a build of this repo made
from the stale "A = SLOPE" paragraph that used to sit above the code — would
otherwise read as "alpha is cover", and a constant-255 alpha would decode as
FULL cover on every texel: grass on rubble and on the ocean floor, and
unrepairable afterwards, because the bytes would contain nothing to repair.
So the writer stamps the DDS header's `dwReserved1` (file offsets 32..75,
zero in every DDS this tree has ever written): `hdr[8] = 'WWCV'` and
`hdr[9] = (coverLawVersion << 24) | round(COVER_FULL)`, with
`coverLawVersion = 1`. **Reader rule:** a DXT5 `_data.DDS` whose `hdr[8]` is
not `'WWCV'`, or whose decoded alpha is constant 255, carries **no cover** and
must be treated as DXT1-equivalent.

**Aspect** (`atan2(n.y, n.x)`) and **curvature** (divergence of the normals) were
considered and rejected on the same derivability test.

**What is deliberately NOT in it:**

  * **material class (R) and second class (UV2.y)** — these are discrete
    enumerations, and BC interpolates within each 4×4 block. That is correct
    for a smooth scalar and silently wrong for an id: it would synthesise class
    numbers that do not exist, at every block boundary. They stay per-vertex,
    where they are exact.
  * **geomorph weight** — a delta to a specific parent MESH, not a property of
    the ground, so it has no meaning at a texel.
  * **water depth** — see below: unlike terrain, the water mesh refines toward
    the data, so its vertices are not the bottleneck.

The vertex channels all still ship. They are free, they cost no extra file, and
they remain right for coarse shading; the map is for anything that is actually
looked at.

### Water — the mesh is subdivided; the channels are next

`WATER_VERTEX_DESC` = `0x100000000002` — **position only, 8 bytes** — and the
water shape is a `BSSubIndexTriShape`, so it runs the same `BSVertexDesc`
machinery as Land. **Nothing about the format restricts it.** Priced from
`ResetAttributeOffsets`:

| add | cost | buys |
|---|---|---|
| `VF_COLORS` | +4 B | 4 x 8-bit channels |
| `VF_UV_2` | +4 B | 2 x 16-bit halfs |
| `VF_EYEDATA` | +4 B | 1 x float |

**7 channels for +12 bytes**, 8 -> 20 a vertex. (`VF_UV` is another 4, but UV0
is the base-texture slot an effect shader samples.)

The limit was never capacity, it was **resolution**: vanilla emits one quad per
wet cell, four corners 4096 units apart, unwelded. Four corner values per cell
cannot describe a shoreline no matter how many channels ride on them.

**That is now fixed.** `--water-subdiv <levels>` (default **3**) runs a quadtree
over each wet cell, refining on the depth range of the terrain samples the leaf
covers: the waterline crosses it -> refine to the limit; merely shallow -> one
level less; open water -> not at all. Measured on the harbour chunk (0,0), which
has 12 wet cells:

| level | verts | tris | duplicated positions | T-junctions |
|---|---|---|---|---|
| 0 | 48 | 24 | 27 | 0 — vanilla, byte-identical output |
| 3 | 513 | — | **0** | **0** |
| 4 | 1304 | — | **0** | **0** |

**The mesh is welded and T-junction-free, and that is a requirement, not a
polish item.** Water is flat, so a hanging node cannot crack the *geometry* —
which is exactly why the problem is easy to miss. But a per-vertex channel
breaks on both of these:

  * **duplicated positions**: two vertices at one point can hold two different
    values, and the shared edge shows a hard seam. Welding by exact integer
    block key (every corner and every inserted split lands on a block
    coordinate by construction) makes that unrepresentable.
  * **T-junctions**: the coarse side interpolates linearly between its two edge
    endpoints while the fine side passes through a midpoint vertex. They agree
    only if that midpoint holds the average of its neighbours — but it holds the
    sampled field, and the field is non-linear exactly where we chose to refine.
    The seam would land on the shoreline, the one place it would be seen.

Two implementation traps, both of which shipped silently before being measured:

  * **Probing only an edge's midpoint is not enough** (63 survivors at subdiv 3).
    It assumes the neighbouring side changes exactly halfway, true only when
    that side is uniformly one level finer. The generator now WALKS each edge and
    inserts a vertex wherever the neighbouring *leaf* changes, which is correct
    for any configuration and does not depend on the 2:1 restriction holding.
  * **A stitched leaf must fan from its CENTRE, not a corner** (6 survivors).
    With a corner pivot, a midpoint adjacent to that pivot is collinear with it:
    the first triangle has zero area and the full-length edge survives with the
    midpoint sitting on it — restoring the exact T-junction being stitched.
    Leaves with no hanging nodes still use the plain two-triangle split, so
    subdiv 0 stays byte-identical.

Per-cell hiding survives: each cell's leaves are emitted consecutively, so the
cell still owns a contiguous triangle run and its segment still names it.

**`--water-subdiv 0` reproduces vanilla byte-for-byte** — verified by md5 against
the pre-change output — so the zero-effort fallback is one flag away.

Ranked by value, now that there are vertices to hold them:

  * **depth** (water height minus terrain height): absorption and colour,
    transparency, where waves shoal and break, where foam forms. The single
    thing that makes water read as water, and the refinement pass already
    computes the terrain range it needs.
  * **distance to land**, at the water's own resolution. The same BFS the
    terrain shore channel uses.
  * **flow direction and speed** for rivers, off the steepest-descent machinery
    the wetness channel is already built on.
  * **direction to shore**, so waves travel toward land and break parallel to
    it — the gradient of the distance field, so it arrives with it.
  * **fetch**: how far open water runs upwind, which sets wave amplitude and
    separates open ocean from a sheltered inlet.

Shore proximity still lives on the TERRAIN vertices as well (128-unit sampling),
and that stays useful: it follows the true contour because it comes from terrain
height rather than the per-cell water plane.

**Why water did NOT get the texture treatment.** Terrain needed it because
decimation strips resolution exactly where it matters — the simplifier
minimises height error, so flat ground goes first, and flat ground is where
shorelines are. The water quadtree does the opposite: it refines toward the
waterline, which is where depth varies fastest. Its finest quads are 512 units
at subdiv 3, 256 at 4 and **128 at 5 — the heightfield's own sample spacing**,
at which point a texture could add nothing. Depth stays per-vertex.

The vertices also have to exist regardless: a texture can hold data, but only
geometry can be displaced, so any future wave motion needs them anyway.

**Two things are still unsettled, and neither is a format question.** Which
slots the engine's water path actually reads — it substitutes its own water
rendering rather than using the `BSEffectShaderProperty` in the file, so this
needs Todd's treat or a live test. And the water TYPE: `XCWT` names a `WATR` record
per cell (16 distinct ones across exterior Commonwealth cells, over a default of
`ExtOceanWater`), and LODGEN currently reads only `XCLW`, the height — so a
shader cannot tell the Glowing Sea from the harbour.

---

## The manifest — `<chunk>.bto.manifest.txt`

Per-object constants, keyed by the same index the vertex channels carry.
**The contract is `docs/LODGEN_MANIFEST_FORMAT.md`.** The row is now eleven
fields, at manifest version 2:

    # lodgen manifest 2 ws <edid> dim <d> chunk <x> <y> columns index base type x y z scale class height ref part
    <index> <base hex8> <type> <x> <y> <z> <scale> <class> <height> <ref hex8> <part>
    C <index> <cx> <cy> <cz> <halfW> <halfH> <N> <depthSpan> <lodm> [<arrayLodm> <layer>]
    I <baseform hex8> <model> <count> <id0>,<id1>,…
    A <shape block> <layer> <lodm>
    M <shape block> <material>

> **This section listed `class` and `bound radius` as MISSING until 2026-09-09.**
> Both ship: `class` is field 7 (`tree` / `rock` / `building` / `misc`, a labelled
> heuristic) and the bound radius is field 8 — misleadingly named `height` in the
> header line, which is a shipped contract and therefore kept
> (`scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md` item 3).

Shape keys and names are matched case-insensitively everywhere, because the
engine's own `BSFixedString` pool is.

---

## Not usable, and why

| field | why not |
|---|---|
| skinning fields | `SKINNED` without a skin instance is undefined in-engine |
| the 3 stored bitangent components | breaks the tangent basis for both the stock engine and our own renderer; CS-exclusive variant only |
| a wider desc, generally | every added field widens the stride and joins the same stock-engine tolerance checklist, which is **still unrun** |

---

## Seeing it

`nifskope-cli lodgen ... --ao-grey` writes AO to R=G=B for inspection (a debug
view — the identity channel it produces is not decodable), and the World LOD
Generator's **Preview channel** box draws any one channel flat in the viewport:
identity hashed, identity raw, AO, the class parameter, or the three terrain channels (material class, wetness, water depth). `WW_LOD_CHANNEL=<1..7>`
drives the same modes headlessly.

---

## The descriptors, decoded

Every value below is the writer's own 64-bit `BSVertexDesc`, either a literal
constant or the result of `SetFlag(...)` + `ResetAttributeOffsets(130)` on one.
**Read every offset from the descriptor at run time**; this table exists so a
consumer can check its decoder, not so anyone can hardcode a number.

The bit layout, from `src/data/niftypes.h`:

```
bits  0..3   vertex size / 4            GetVertexSize() = (desc & 0xF) * 4
bits  4..43  ten 4-bit attribute slots  GetAttributeOffset(a) = (desc >> (4a+2)) & 0x3C
bits 44..63  vertex flags               VF_VERTEX 1, VF_UV 2, VF_UV_2 4, VF_NORMAL 8,
                                        VF_TANGENT 0x10, VF_COLORS 0x20, VF_SKINNED 0x40,
                                        VF_LANDDATA 0x80, VF_EYEDATA 0x100, VF_FULLPREC 0x400
```

`ResetAttributeOffsets(130)` lays attributes out in `VA_` order at these widths:
position 2 words (4 with `VF_FULLPREC`), UV 1, UV2 1, normal 1, tangent 1,
colour 1, skinning 3, eye data 1 — each word being 4 bytes.

### Objects — `.bto` (`lodgenBuildObjectChunk`)

| profile | descriptor | stride | flags | pos | UV | UV2 | normal | tangent | colour | eye |
|---|---|---|---|---|---|---|---|---|---|---|
| `--no-identity` | `0x0001B00000430205` | 20 | `0x01B` | 0 | 8 | — | 12 | 16 | — | — |
| identity, `--no-object-channels` | `0x0003B00005430206` | 24 | `0x03B` | 0 | 8 | — | 12 | 16 | 20 | — |
| identity + `objectChannels` (**the default**) | `0x0013F07006543208` | 32 | `0x13F` | 0 | 8 | 12 | 16 | 20 | 24 | 28 |

**Colours move from +20 to +24** when UV2 is present, because UV2 is laid out
ahead of them. A reader that remembers +20 samples normal and tangent bytes as
an object id and reports cross-welded triangles that do not exist — which is
exactly what the terrain harness did until it was fixed.

### Terrain — `.btr` (`lodgenBuildTerrainChunk`)

| profile | descriptor | stride | flags | pos | UV | UV2 | colour | eye |
|---|---|---|---|---|---|---|---|---|
| base `LAND_VERTEX_DESC` | `0x0000300000000203` | 12 | `0x003` | 0 | 8 | — | — | — |
| `--geomorph` only | `0x0010303000000204` | 16 | `0x103` | 0 | 8 | — | — | 12 |
| `--terrain-identity` only | `0x0002700004003205` | 20 | `0x027` | 0 | 8 | 12 | 16 | — |
| both (**the CS profile**) | `0x0012705004003206` | 24 | `0x127` | 0 | 8 | 12 | 16 | 20 |

Terrain carries **no normal and no tangent** at any profile: the surface normal
comes from the `_msn` sheet.

### Water (`WATER_VERTEX_DESC`)

| profile | descriptor | stride | flags | pos | eye |
|---|---|---|---|---|---|
| base | `0x0000100000000002` | 8 | `0x001` | 0 | — |
| with `VF_EYEDATA` | `0x0010102000000003` | 12 | `0x101` | 0 | 8 |

Position only, 8 bytes, on a `BSSubIndexTriShape`. Priced from
`ResetAttributeOffsets`: `VF_COLORS` +4 B, `VF_UV_2` +4 B, `VF_EYEDATA` +4 B —
**7 channels for +12 bytes**, 8 → 20 a vertex.

### The `.lodl` / `.btd` viewer scene (`src/btdterrain.cpp`)

Not a generator output — this is what NifSkope builds when it MESHES a `.lodl`
or a `.btd` for viewing. It is full precision (`VF_FULLPREC`), which none of the
generator profiles are.

| view | descriptor | stride | flags | pos | UV | normal | tangent | colour |
|---|---|---|---|---|---|---|---|---|
| Height (no vertex colour) | `0x0041B00000650407` | 28 | `0x41B` | 0 | 16 | 20 | 24 | — |
| any plane view (+`VF_COLORS`) | `0x0043B00007650408` | 32 | `0x43B` | 0 | 16 | 20 | 24 | 28 |

The Height view writes no vertex-colour channel at all, which is why a `.lodl`
Height scene and a `.btd` Height scene are the same bytes.

---

### Provenance for this section

`src/lodgen.cpp` sha256 `c05fd079655ac03e` (8,924 lines),
`src/btdterrain.cpp` `35c2adc319852901` (1,484 lines), `src/data/niftypes.h` `28ad54471e9c5044`
(2,255 lines), re-derived 2026-09-10 (lane DOCS2).

**Re-derived 2026-09-10 by lane DOCS2** (`ww-contract-provenance` step 3, script
`scratchpad/docs2_20260910/anchors.py`): every line number below was found again
from its own anchor text against the sources stamped above, never shifted by a
delta. 9 of 11 rows moved; the `OBJ_VERTEX_DESC` anchor was a prefix of
`OBJ_VERTEX_DESC_COLORS` on the next line and was lengthened, and the
accessor row's range end was put back on `ResetAttributeOffsets`'s own
closing brace rather than carried by the same delta as its start.

| claim | line | anchor |
|---|---|---|
| `LAND_VERTEX_DESC = 52776558133763` | `lodgen.cpp:58` | `constexpr std::uint64_t LAND_VERTEX_DESC` |
| `WATER_VERTEX_DESC = 17592186044418` | `lodgen.cpp:59` | `constexpr std::uint64_t WATER_VERTEX_DESC` |
| `OBJ_VERTEX_DESC = 474989027590661` | `lodgen.cpp:1358` | `constexpr std::uint64_t OBJ_VERTEX_DESC = 474989027590661ULL;` |
| `OBJ_VERTEX_DESC_COLORS = 1037939064898054` | `lodgen.cpp:1359` | `constexpr std::uint64_t OBJ_VERTEX_DESC_COLORS` |
| terrain flags added and reset at stream 130 | `lodgen.cpp:907-920` | `BSVertexDesc landDesc( LAND_VERTEX_DESC );` … `landDesc.ResetAttributeOffsets( 130 );` |
| terrain stride is 12 without either arm | `lodgen.cpp:908` | `quint32 landStride = 12;` |
| object flags added and reset at stream 130 | `lodgen.cpp:3728-3737` | `BSVertexDesc objDesc( opts.identity ? OBJ_VERTEX_DESC_COLORS : OBJ_VERTEX_DESC );` |
| water `VF_EYEDATA` variant | `lodgen.cpp:1263-1267` | `BSVertexDesc wdesc( WATER_VERTEX_DESC );` |
| viewer descriptor and its `+VF_COLORS` variant | `btdterrain.cpp:192-195` | `BSVertexDesc desc( 0x0041B00000650407ULL );` |
| the bit layout and the offset accessors | `niftypes.h:1907-1979` | `GetVertexSize`, `GetAttributeOffset`, `ResetAttributeOffsets` |
| attribute widths at stream 130 | `niftypes.h:1941-1976` | `uint attributeSizes[VA_COUNT] = {};` |
