# NIF Scene Graph & Block Types

NIF (NetImmerse File) is Bethesda's 3D mesh format. A NIF is a flat array of **blocks** that form a scene graph via parent-child Ref/Ptr indices. Each block has a type (e.g. `NiNode`, `BSTriShape`) and an integer ID (its index in the array).

## Scene Graph Example (FO4 Weapon)

```
BSFadeNode "WEAPON" (root, block 0)
├── NiNode "WeaponBolt"
│   └── BSTriShape "BoltMesh:0"
│       ├── BSLightingShaderProperty → BSShaderTextureSet
│       └── NiAlphaProperty
├── NiNode "WeaponMagazine"
│   └── BSTriShape "MagMesh:0"
├── NiControllerManager            ← animation orchestrator
│   ├── NiControllerSequence "FireGun"
│   └── NiControllerSequence "Idle"
├── BSXFlags "BSX"                 ← engine flags
├── BSConnectPoint::Parents        ← weapon attachment points
└── BSConnectPoint::Children
```

## Node Types

| Block Type | Purpose |
|------------|---------|
| `NiNode` | Generic node — bones, reference points, grouping |
| `BSFadeNode` | Root node for FO4 meshes (inherits NiNode, adds LOD fade) |
| `NiBone` | Skeleton bone (functionally same as NiNode) |
| `BSFaceGenNiNode` | Face geometry node |
| `BSLeafAnimNode` | Tree/leaf animation node |
| `NiBillboardNode` | Always-facing-camera node |
| `OrderedRenderingNode` | Forces child shapes to render in order (back-to-front compositing) |

Every node has: name, transform (translation/rotation/scale), flags, and Ref links to children, controllers, and extra data.

## Mesh Shapes

| Block Type | Purpose |
|------------|---------|
| `BSTriShape` | Standard FO4 triangle mesh (vertices, triangles, UVs, normals) |
| `BSDynamicTriShape` | Dynamic/deformable geometry |
| `BSSubIndexTriShape` | Mesh with segment/material data |

BSTriShape contains vertex data inline (controlled by `Vertex Desc` bitfield and `Data Size`). The vertex format flags determine which attributes are present (position, UV, normals, tangents, colors, bone weights).

## Shaders

| Block Type | Purpose | Use Case |
|------------|---------|----------|
| `BSLightingShaderProperty` | Standard PBR material | Solid objects — weapons, armor, architecture. References `.bgsm` material file |
| `BSEffectShaderProperty` | Effect/glow material | Particles, glow, animated effects, nixie tubes. References `.bgem` material file |
| `BSShaderTextureSet` | Texture path container | Referenced by BSLightingShaderProperty (9 slots: diffuse, normal, specular, etc.) |
| `NiAlphaProperty` | Alpha blending/testing | Controls transparency mode |

**BSEffectShaderProperty** is the key shader for visual effects. Supports:
- Emissive color and multiplier (glow intensity)
- UV offset animation (scrolling textures, progress bars)
- Soft-particle rendering
- Animated via BSEffectShaderPropertyFloatController / ColorController

### BSEffectShaderProperty Controlled Variables

Float variables animated by `BSEffectShaderPropertyFloatController`:

| Variable ID | Name | Use |
|-------------|------|-----|
| 0 | AlphaTransparency | Overall opacity (0=opaque, 1=transparent) |
| 1 | EmissiveMultiple | Glow intensity multiplier |
| 11 | SourceTexVOffset | UV V-offset (progress bar fill, texture scroll) |
| 12 | SourceTexUOffset | UV U-offset |
| 22 | Refraction strength | Distortion amount |

## Extra Data Blocks

| Block Type | Purpose |
|------------|---------|
| `BSXFlags` | Engine behavior flags (see BSXFlags section below) |
| `BSBehaviorGraphExtraData` | Path to behavior graph `.hkx` file |
| `NiStringExtraData` | Named string (e.g. "Prn" for weapon slot) |
| `NiIntegerExtraData` | Named integer |
| `BSBound` | Engine bounding box |
| `BSInvMarker` | Inventory preview rotation/zoom |
| `NiTextKeyExtraData` | Animation time markers |
| `BSFurnitureMarkerNode` | Furniture sit/sleep positions |

## Collision Blocks

| Block Type | Purpose |
|------------|---------|
| `bhkCollisionObject` | Standard Havok collision |
| `bhkNPCollisionObject` | FO4 native physics collision |
| `bhkPhysicsSystem` | FO4 collision geometry data |
| `bhkRigidBody` | Rigid body properties (mass, friction) |
| `bhkBoxShape` / `bhkCapsuleShape` / `bhkSphereShape` | Primitive collision shapes |
| `bhkConvexVerticesShape` | Convex hull collision |
| `bhkListShape` | Compound collision (multiple children) |

## BSXFlags Reference

The `BSXFlags` block on the root node tells the engine how to handle the NIF.

| Flag | Value | Meaning |
|------|-------|---------|
| Animated | 1 | Has animations |
| Havok | 2 | Has collision |
| Ragdoll | 4 | Ragdoll enabled |
| Complex | 8 | Complex geometry |
| Addon | 16 | Addon node |
| Editor marker | 32 | Editor marker |
| Dynamic | 64 | Dynamic object |
| Articulated | 128 | Articulated (weapon) |
| Need transform update | 256 | Force transform update |
| External emit | 512 | External particle emit |

Common weapon value: **202** = Havok(2) + Complex(8) + Dynamic(64) + Articulated(128)

## Connect Points (Weapon Attachments)

### BSConnectPoint::Parents

Stored on the root node. Each connect point has:
- `parent` — parent bone name (e.g. "WEAPON")
- `name` — connect point name (e.g. "P-Barrel", "P-Scope")
- `translation` — offset from parent bone
- `rotation` — quaternion (x, y, z, w)
- `scale` — uniform scale

Standard weapon connect point names:
```
P-Barrel      — barrel attachment
P-Scope       — scope/optics
P-Grip        — grip attachment
P-Mag         — magazine
P-Muzzle      — muzzle device
P-Stock       — stock attachment
P-Casing      — casing ejection point
P-ProjectileNode — projectile spawn
P-FX          — animated effect mesh attachment
```

**P-FX pattern:** Attaches animated effect meshes that use inherently looping textures (flickering lightning, glow). No NiControllerManager needed — the visual animation comes from the texture.

### BSConnectPoint::Children

Child connect points are names that child meshes (mod pieces) use to attach TO parent connect points. A barrel mod mesh would have `C-Barrel` matching the parent's `P-Barrel`.
