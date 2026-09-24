# NIF Bashing — Practical Recipes

> ## Ported-file banner — read before running anything here
>
> Every recipe below is written against the `modkit nif` CLI and its **stateful session model**
> (`open_nif` → `inspect`/`modify`/`add_block` → `save_nif` → `close`), with a `batch([...])`
> wrapper to avoid round-trips. **None of that exists on this machine.**
>
> - `modkit nif` — **NOT AVAILABLE ON THIS MACHINE.**
> - **There are no sessions.** Our tool, `nifskope-cli`, is stateless: file in, `-o` file out.
>   Wherever a recipe says `sid`, there is no `sid`; pass the file path and an output path.
> - **There is no `batch`.** It is unnecessary here and does not exist. Chain commands, using
>   each one's `-o` output as the next one's input.
> - The **command mapping table is in `../SKILL.md`** — read it once, then the recipes below
>   translate mechanically:
>   `inspect(sid, N)` → `nifskope-cli dump F -b N` ·
>   `inspect(sid, -1)` → `nifskope-cli list F` ·
>   `modify(sid, N, {field: v})` → `nifskope-cli set F -b N -f "field" -v v -o OUT` ·
>   `new_nif(path)` → `nifskope-cli new -o OUT` ·
>   `copy_blocks` → `nifskope-cli merge F --add OTHER -o OUT` *(whole-file splice with node
>   de-duplication, NOT a single-block copy)*.
> - **`add_block`, `remove_blocks`, `generate_collision` and `remove_collision` have no headless
>   equivalent** — NOT AVAILABLE ON THIS MACHINE as CLI commands. Block creation/removal and
>   collision *generation* live in the NifSkope GUI (the mesh-shape collision builder shipped
>   2026-08-23 and its output works in game), or in a spell driven by `nifskope-cli cast`.
>   `nifskope-cli collision F [--extract|--roundtrip|--constraints|--skeleton|--bodies]`
>   inspects and re-encodes existing collision but does not create it.
>
> **What survives unchanged and is the reason to keep this file:** the *structural* knowledge —
> the weapon / effect-mesh / particle NIF layouts, BSXFlags values, the texture-array and
> shader-material fields, the Havok scale factor of **69.99125**, the back-reference /
> circular-dependency trap on copies, and the tips at the end. Those are facts about the
> format, not about a tool.

Common NIF manipulation tasks. The upstream text below uses the `modkit nif` session model —
translate per the banner.

## Inspecting a NIF

### Get an overview
```
open_nif(path)     → session_id, block_count, root info, block type summary
inspect(sid, -1)   → full scene graph hierarchy tree
```

### Inspect multiple blocks (use batch)
```
batch([
    {"tool": "inspect", "args": {"session_id": sid, "block_id": 0}},
    {"tool": "inspect", "args": {"session_id": sid, "block_id": 5}},
    {"tool": "inspect", "args": {"session_id": sid, "block_id": 12}},
])
```

### Find specific block types
Use `inspect(sid, -1)` to get the hierarchy, then batch-inspect the block IDs you need. The hierarchy shows block IDs, types, and names.

### Read vertex/triangle data
```
inspect(sid, <BSTriShape_block_id>)
```
Returns all fields including Vertex Data (array of vertex structs with position, UV, normals, tangents, colors, bone weights depending on Vertex Desc flags) and Triangles.

---

## Adding Nodes

### Add a bone/attachment point
```
open_nif(path)
add_block(sid, "NiNode", fields={"Name": "WeaponMagazine"}, attach_to=0)
save_nif(sid, output_path)
```

The `attach_to` parameter automatically adds the new block as a child of the specified parent block (updates Children array and Num Children).

### Add extra data to root (batch multiple adds)
```
batch([
    {"tool": "add_block", "args": {"session_id": sid, "type_name": "BSXFlags",
        "fields": {"Name": "BSX", "Integer Data": 202}, "attach_to": 0}},
    {"tool": "add_block", "args": {"session_id": sid, "type_name": "BSBehaviorGraphExtraData",
        "fields": {"Name": "BGED", "Behaviour Graph File": "UniqueBehaviors\\MyWeaponFX\\MyWeaponFX.hkx",
                   "Controls Base Skeleton": 0}, "attach_to": 0}},
    {"tool": "add_block", "args": {"session_id": sid, "type_name": "NiStringExtraData",
        "fields": {"Name": "Prn", "String Data": "PSYCHO"}, "attach_to": 0}},
])
```

---

## Modifying Existing Blocks

### Change field values (batch multiple modifies)
```
batch([
    {"tool": "modify", "args": {"session_id": sid, "block_id": 0, "fields": {"Name": "NewName"}}},
    {"tool": "modify", "args": {"session_id": sid, "block_id": 0, "fields": {"Scale": 1.5}}},
    {"tool": "modify", "args": {"session_id": sid, "block_id": 3, "fields": {"Flags": 14}}},
])
```

### Change texture paths
Find the BSShaderTextureSet block, then modify its texture array:
```
inspect(sid, <texture_set_block_id>)
# Textures is a list of 9 paths (diffuse, normal, specular, etc.)
modify(sid, texture_set_id, {"Textures": [
    "textures\\weapons\\mymod\\diffuse.dds",
    "textures\\weapons\\mymod\\normal_n.dds",
    "", "", "", "", "", "", ""
]})
```

### Change shader material path
```
modify(sid, <shader_block_id>, {"Name": "materials\\weapons\\mymod\\myshader.BGSM"})
```

---

## Copying Between NIFs

### Copy a subtree from one NIF to another
```
src_sid = open_nif("source.nif")["session_id"]
tgt_sid = open_nif("target.nif")["session_id"]

# Copy block 5 (and all its dependencies) from source to target
# attach_to=0 adds it as child of target's root
copy_blocks(src_sid, [5], tgt_sid, attach_to=0)

save_nif(tgt_sid, "output.nif")
```

The `copy_blocks` tool automatically:
- Resolves the full dependency tree (all Ref/Ptr links)
- Deep-copies all dependent blocks
- Remaps all Ref/Ptr indices to the new block IDs
- Returns a mapping of source → target block IDs

### Copy multiple independent blocks
```
copy_blocks(src_sid, [5, 10, 15], tgt_sid, attach_to=0)
```

---

## Removing Blocks

### Remove blocks and remap references
```
remove_blocks(sid, [5, 6, 7])
```

All Ref/Ptr indices across the entire NIF are remapped. Blocks that referenced removed blocks get -1 (null ref).

---

## Creating a New NIF from Scratch

### Empty weapon NIF
```
new_nif("output.nif", game="FO4")
```

Creates a NIF with correct FO4 header (version 20.2.0.7, BS version 130) and a BSFadeNode root.

### Build up a weapon receiver
```
result = new_nif("my_weapon.nif")
sid = result["session_id"]

# Add BSXFlags
add_block(sid, "BSXFlags", {"Name": "BSX", "Integer Data": 202}, attach_to=0)

# Add NiStringExtraData for weapon slot
add_block(sid, "NiStringExtraData", {"Name": "Prn", "String Data": "PSYCHO"}, attach_to=0)

# Add connect points (P-Barrel, P-Scope, etc.)
# BSConnectPoint::Parents fields depend on the schema

# Add child nodes for weapon parts
add_block(sid, "NiNode", {"Name": "WeaponBolt"}, attach_to=0)
add_block(sid, "NiNode", {"Name": "WeaponMagazine"}, attach_to=0)

save_nif(sid, "my_weapon.nif")
```

---

## Common Patterns

### Weapon NIF structure
```
BSFadeNode "WEAPON" (root)
├── BSXFlags "BSX" (value 202)
├── NiStringExtraData "Prn" (weapon slot)
├── BSConnectPoint::Parents (P-Barrel, P-Scope, etc.)
├── BSConnectPoint::Children
├── NiNode "WeaponBolt"
├── NiNode "WeaponMagazine"
├── NiNode "WeaponTrigger"
├── BSTriShape "ReceiverMesh:0"
│   ├── BSLightingShaderProperty → BSShaderTextureSet
│   └── NiAlphaProperty
└── (additional mesh shapes)
```

### Effect mesh structure
```
BSFadeNode "root" (root)
├── BSXFlags "BSX" (value 1 = Animated)
├── BSBehaviorGraphExtraData "BGED" → behavior.hkx
├── BSTriShape "EffectMesh:0"
│   ├── BSEffectShaderProperty → texture
│   └── NiAlphaProperty
├── NiControllerManager
│   ├── NiControllerSequence "partA"
│   └── NiControllerSequence "partB"
└── NiDefaultAVObjectPalette
```

### NIF with particles
```
BSFadeNode "root" (root)
├── BSXFlags "BSX" (value 1)
├── NiNode "emitter_parent"
│   └── NiNode "emitterRef"
│       └── NiNode "gravityRef"
├── NiParticleSystem "Smoke"
│   ├── NiPSysData
│   ├── BSEffectShaderProperty
│   ├── NiAlphaProperty
│   └── [modifiers...]
├── BSTriShape "smoke_billboard"
│   ├── BSEffectShaderProperty
│   └── NiAlphaProperty
└── NiControllerManager
    └── NiControllerSequence "Smoking"
```

---

## Generating Collision

### Add collision to a mesh (one call)
```
generate_collision(sid, node_block_id=0, shape_type="convex_hull", layer="STATIC")
```

Creates the full hierarchy: `bhkCollisionObject` → `bhkRigidBody` → shape, wired to the parent node. Auto-discovers BSTriShape children for vertex source.

### Shape types
- `"convex_hull"` — scipy ConvexHull from all mesh vertices (best for most objects)
- `"box"` — AABB box (bhkTransformShape + bhkBoxShape)
- `"list"` — compound: one convex hull per mesh child, wrapped in bhkListShape

### Collision layers
`"STATIC"` (1), `"ANIMSTATIC"` (2), `"CLUTTER"` (4), `"WEAPON"` (5), `"PROJECTILE"` (6), `"TERRAIN"` (13)

### Full example — weapon with collision
```
sid = open_nif("weapon.nif")["session_id"]
generate_collision(sid, node_block_id=0, shape_type="convex_hull", layer="WEAPON")
save_nif(sid, "weapon.nif")
```

### Replace vs merge existing collision
```
# Replace (default): removes old collision, creates new
generate_collision(sid, 0, shape_type="box", replace=True)

# Merge: wraps old + new shapes in bhkListShape
generate_collision(sid, 0, shape_type="convex_hull", replace=False)
```

### Remove collision
```
remove_collision(sid, node_block_id=0)
```
Removes the entire collision subtree and clears the Collision Object reference.

### Collision hierarchy produced
```
Parent Node (existing)
  └── bhkCollisionObject (Flags=0x81)
        ├── Target → Parent Node (back-ref)
        └── Body → bhkRigidBody
              ├── Shape → bhkConvexVerticesShape / bhkBoxShape / bhkListShape
              ├── Havok Filter → {Layer:FO4 = value}
              └── Mass/Friction/Restitution
```

### Important: Havok scale
FO4 uses a Havok scale factor of **69.99125** (not 7.0). All collision vertices are automatically scaled by `1/69.99125` when generating shapes.

---

## Known Writer Limitations

### save_nif and complex blocks
The NIF writer preserves unparsed binary data (via `_remainder`) for blocks with complex internal formats like NiPSysData and bhkPhysicsSystem. Roundtrip saves are **byte-identical** for these blocks.

**Note:** `generate_collision` creates legacy Havok collision (bhkCollisionObject → bhkRigidBody → shape) rather than bhkNPCollisionObject → bhkPhysicsSystem. Both formats work in FO4. The legacy format uses discrete blocks that are easier to inspect and modify.

### copy_blocks and circular dependencies
`copy_blocks` follows all Ref/Ptr links to build the dependency tree. Some fields are "back references" (Target on controllers → controlled node, Manager on sequences → controller manager) that point upward in the scene graph. Following these creates circular dependencies and corrupts the NIF.

**Fixed:** `collect_dependency_tree` now excludes back-reference fields (Target, Manager, Scene, palette entries). If you still see issues, use `add_block` to create blocks manually instead of copying.

## Tips

- **Always inspect before modifying** — understand the block structure first
- **Block IDs change after remove_blocks** — re-inspect after removals
- **String table is rebuilt on save** — no need to manage it manually
- **Vertex data is stored per-vertex** as dicts with conditional fields based on Vertex Desc flags
- **Connect points** use quaternion rotation — inspect existing NIFs for correct values
- **BSXFlags = 202** for standard weapons (Havok + Complex + Dynamic + Articulated)
- **BSXFlags = 1** for animated effect meshes (Animated only)
- **bhkPhysicsSystem** blocks are preserved byte-perfectly on save (binary blob round-trips intact)
