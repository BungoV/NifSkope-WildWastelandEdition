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

Identity profile. `--no-identity` gives `0x1B00000650405`, 20 bytes, carrying
none of this. With identity: `0x3B00000650406`, 24 bytes. With identity AND the
extra channels (the default, `objectChannels`): **32 bytes**, UV2 and Eye Data
added. Never hardcode any of it — see the offsets note below.

| slot | width | holds | status |
|---|---|---|---|
| Vertex Colors **R + G** | 16 bits | per-chunk object index, `index = R + G*256` | **SHIPPED** |
| Vertex Colors **B** | 8 bits | baked ambient occlusion | **SHIPPED** |
| Vertex Colors **A** | 8 bits | tree sway weight, 0 trunk base → 1 branch tip | **DECIDED** |
| UV2.x | ~11 bits | sky visibility: fraction of the upper hemisphere that reaches open sky | **SHIPPED** |
| UV2.y | ~11 bits | — | **FREE** |
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

`LAND_VERTEX_DESC` = `0x300000000303` plus flags, extended only when the CS
profile is on.

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

### Water carries nothing TODAY, and that is a choice, not a law

LOD water is its own `BSMultiBoundNode 'WATER'` under a `BSEffectShaderProperty`,
with `WATER_VERTEX_DESC` = `0x100000000002` — **position only, 8 bytes**. It is
also one quad per wet cell: the far-ring shape for a whole dim-4 chunk measured
**8 vertices, 4 triangles**, corners 4096 units apart. Even widened, that mesh
cannot describe a shoreline, and foam wants metres.

That is why shore proximity lives on the TERRAIN vertices today, at 128-unit
sample spacing, following the true contour because the contour comes from
terrain height rather than from the per-cell water plane.

**But the water mesh is ours to generate, and 4 vertices a cell is only what
nothing has needed yet.** Subdivide it — adaptively, dense near shores where the
gradients are and coarse in open water, with the decimation the terrain pass
already has — and real per-vertex water data becomes possible. Ranked by value:

  * **depth** (water height minus terrain height): absorption and colour,
    transparency, where waves shoal and break, where foam forms. The single
    thing that makes water read as water. Computable from data the water pass
    already holds — the per-cell height and the per-sample terrain grid it
    already tests exposure against.
  * **distance to land**, at the water's own resolution rather than inferred
    from the far side. The same BFS the terrain shore channel uses.
  * **flow direction and speed** for rivers, off the steepest-descent machinery
    the wetness channel is already built on.
  * **direction to shore**, so waves travel toward land and break parallel to
    it — the gradient of the distance field, so it arrives with it.
  * **fetch**: how far open water runs upwind, which sets wave amplitude and
    separates open ocean from a sheltered inlet.

Sizing: uniform 128-unit sampling would be ~1089 vertices per CELL against the
Land shape's 1068 for a whole chunk, so adaptive is not an optimisation, it is
the only version that fits.

The one genuine unknown is which slots are inert. Water is a
`BSEffectShaderProperty`, and effect shaders certainly sample **UV0** for their
base texture, so that slot is not free the way it is on Land. UV2 and Eye Data
are probably free and "probably" is not the standard this file holds itself to
— settle it with the PDB or a live test before writing anything there.

---

## The manifest — `<chunk>.bto.manifest.txt`

Per-object constants, keyed by the same index the vertex channels carry.

    <index> <formID hex8> <recordType> <x> <y> <z> <scale>          SHIPPED
    I <baseform hex8> <model> <count> ...                           SHIPPED  (instance groups)

**Missing, and needed before the A channel can be interpreted generally:**

  * **class** (tree / building / rock / misc) — the charter's rule is that the
    class decides what A *means*. Without it, A is only interpretable because
    today it is only ever written for trees.
  * **bound radius** — the charter's screen-size fade input.

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
identity hashed, identity raw, AO, or the class parameter. `WW_LOD_CHANNEL=<1..4>`
drives the same modes headlessly.
