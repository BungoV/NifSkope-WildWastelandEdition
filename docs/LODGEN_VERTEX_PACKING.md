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

Identity profile, `OBJ_VERTEX_DESC_COLORS` = `0x3B00000650406`, **24-byte
stride**. Without `--no-identity` the profile is `0x1B00000650405`, 20 bytes,
and carries none of this.

| slot | width | holds | status |
|---|---|---|---|
| Vertex Colors **R + G** | 16 bits | per-chunk object index, `index = R + G*256` | **SHIPPED** |
| Vertex Colors **B** | 8 bits | baked ambient occlusion | **SHIPPED** |
| Vertex Colors **A** | 8 bits | tree sway weight, 0 trunk base → 1 branch tip | **DECIDED** |
| UV2.x | ~11 bits | — | **FREE** |
| UV2.y | ~11 bits | — | **FREE** |
| Eye Data | 32 bits | — | **FREE** (terrain uses it for geomorph; objects could) |

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

Choosing A over UV2 also keeps the stride at 24 bytes. The 24-byte desc still
owes the stock-engine tolerance gate; widening it to 28 before that gate is run
would stack an unmeasured risk on an unmeasured risk.

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
| Vertex Colors **A** | 8 bits | written 1.0 | **FREE** |
| UV2.x (`VF_UV_2`) | ~11 bits | sky visibility, before byte quantisation | **SHIPPED** |
| UV2.y | ~11 bits | second-strongest material class, for two-material blending | **SHIPPED** |
| Eye Data (`VF_EYEDATA`) | 32 bits | geomorph weight: WORLD-unit height delta to the parent ring's surface | **SHIPPED** (`--geomorph`, dim < 32; dim 32 has no parent and stores 0) |

Terrain and objects deliberately do **not** share a layout: terrain has no
object index, objects have no material class.

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
