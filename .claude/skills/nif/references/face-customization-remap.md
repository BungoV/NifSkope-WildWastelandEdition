# Face Customization Remap Data — NiBinaryExtraData for FO4 Faces

Fallout 4's character creation (chargen) face sculpting system uses two `NiBinaryExtraData` blocks embedded in `_faceBones.nif` meshes. These are **not documented** in any public wiki, NIF XML schema, or modding community resource — this is original reverse-engineering.

## Overview

Face meshes in FO4 have **two separate skinning systems**:
1. **BSSkin weights** — standard bone weights that drive animation (blinking, talking, expressions)
2. **CustomizationRemap weights** — a parallel set of bone weights that drive chargen sculpt sliders

The chargen system reads the CustomizationRemap data to know how much each face bone's slider should move each vertex. Without these blocks, the face mesh renders fine but **chargen sliders have no effect** — the face is static/unsculptable.

## File Location

```
Meshes/Actors/Character/CharacterAssets/BaseMaleHead_faceBones.nif
```

The two NiBinaryExtraData blocks are children of the `BSSubIndexTriShape` geometry node (e.g., "BaseMaleHead_faceBones:0"):
- **"CustomizationRemapData"** — per-vertex sculpt weights (large)
- **"CustomizationRemapNewBonesData"** — extra bone definitions (small)

## CustomizationRemapData — Binary Format

**Size:** `vertex_count * 12` bytes (e.g., 1696 vertices × 12 = 20,352 bytes for BaseMaleHead).

Each 12-byte record maps one vertex to up to 4 face bones:

```
Offset  Size  Type     Description
──────  ────  ───────  ─────────────────────────
0       2     float16  Weight 0 (half-precision)
2       2     float16  Weight 1
4       2     float16  Weight 2
6       2     float16  Weight 3
8       1     uint8    Bone Index 0
9       1     uint8    Bone Index 1
10      1     uint8    Bone Index 2
11      1     uint8    Bone Index 3
```

**Format notes:**
- Identical layout to standard NIF vertex bone weights (4× half-float + 4× uint8)
- Weights are normalized (sum to ~1.0)
- Bone indices reference the `BSSkin::BoneData` bone list (same indices as animation skinning)
- Little-endian byte order for float16 values

### Default Entry (no sculpt effect)

```
Bytes: [0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00]
       ╰─ 1.0 ──╯  ╰─ 0.0 ──╯  ╰─ 0.0 ──╯  ╰─ 0.0 ──╯  ╰ bone indices ──╯
```

- Weight 1.0 on bone index 2 (HEAD), all others zero
- The **vast majority** of vertices use this default — they follow HEAD rigidly
- These vertices are unaffected by any chargen slider

### Active Entry (sculpt-affected vertex)

```
Bytes: [0xB1, 0x37, 0x85, 0x35, 0xFF, 0x2F, 0x53, 0x2A, 0x02, 0x05, 0x07, 0x03]
       ╰─ 0.492 ─╯  ╰─ 0.345 ─╯  ╰─ 0.125 ─╯  ╰─ 0.049 ─╯   2    5    7    3
```

- Blends across bones 2, 5, 7, 3 with weights summing to ~1.0
- These vertices deform when their associated chargen sliders are adjusted

### Common Bone Indices

Indices reference the BSSkin bone list order. Key mappings for BaseMaleHead:

| Index | Bone | Role |
|-------|------|------|
| 0 | Root (NiNode) | — |
| 1 | Chest_skin | Chest area |
| 2 | HEAD | Default/identity (no sculpt) |
| 3 | Head_skin | Head periphery |
| 5 | Neck1_skin | Upper neck |
| 7 | Neck_skin | Lower neck |
| 68 | Adam's Apple / extended bone | Extra bone beyond standard set |

Indices 3-68 map to various `skin_bone_*` face bones (cheek, jaw, eyebrow, nose, ear, temple, etc.). The exact mapping depends on the bone order in `BSSkin::Instance`.

## CustomizationRemapNewBonesData — Binary Format

**Size:** 208 bytes for BaseMaleHead (varies by bone count).

Defines bones added to the chargen system beyond the standard skeleton. Structure:

```
Offset   Size    Type          Description
───────  ──────  ────────────  ────────────────────────────────
0        varies  null-term str Bone name (e.g., "Neck")
varies   varies  mixed         Engine metadata (counts, offsets, flags)
N-64     16      4×float32     Bounding sphere (center XYZ + radius)
N-48     48      3×4 float32   Rotation matrix (3 rows × 4 columns)
N-4      4       float32       Always 1.0 (w component)
```

### Transform Matrix (last 64 bytes)

The final 64 bytes contain a bounding region (4 floats) followed by a 4×4 affine transform matrix:

```
Bounding:  [2.867, 4.740, 0.000, 5.129]   ← center XYZ + radius

Matrix:
  [ ~0.000,  ~0.000, -1.000,  0.000 ]     ← X axis
  [ 0.377,   0.926,  ~0.000,  0.000 ]     ← Y axis (sin/cos ~22°)
  [ 0.926,  -0.377,  ~0.000,  0.000 ]     ← Z axis
  [ 8.228,  ~0.000,  ~0.000,  1.000 ]     ← Translation + w
```

This defines the bind-pose transform for the "Neck" bone in the chargen system. The rotation portion is orthonormal (valid rotation matrix). The middle bytes contain runtime engine metadata that is largely opaque.

## Related Text Files

Four `FacialBoneRegionUIRemapping` files in `CharacterAssets/` define how chargen UI sliders map to bone axes:

```
HumanRaceFacialBoneRegionUIRemappingMale.txt
HumanRaceFacialBoneRegionUIRemappingFemale.txt
PowerArmorRaceFacialBoneRegionUIRemappingMale.txt
PowerArmorRaceFacialBoneRegionUIRemappingFemale.txt
```

Format: `Region <boneID> <PosX> <PosY> <PosZ> <RotX> <RotY> <RotZ> <TextureValue> [BanterEventType]`

These map slider X/Y/Z to actual bone translation/rotation axes (e.g., `-Z`, `Y`, `-X`), controlling which direction a slider moves each face bone.

## Creating New Face Meshes

### Strategy by scenario

| Scenario | CustomizationRemapData | CustomizationRemapNewBonesData |
|----------|----------------------|-------------------------------|
| Same vertex layout as vanilla | Copy verbatim | Copy verbatim |
| Modified mesh, same vertex order | Copy verbatim (should work) | Copy verbatim |
| Re-topologized mesh | Must recompute per-vertex weights | Copy verbatim (skeleton-tied) |
| No chargen support needed | Omit entirely | Omit entirely |

### Recomputing weights for new meshes

For a fully custom face mesh, generate CustomizationRemapData by:

1. For each vertex in the new mesh:
   a. Find the nearest face bones (from the BSSkin bone list)
   b. Compute distance-based weights (inverse distance, heat diffusion, or similar)
   c. Select top 4 bones, normalize weights to sum to 1.0
   d. Pack as 4× float16 (little-endian) + 4× uint8 bone indices
2. Vertices far from any face bone region → default entry (weight 1.0 on HEAD)
3. Write the full array as a NiBinaryExtraData named "CustomizationRemapData"

### Copying from vanilla (recommended approach)

```python
# Pseudocode for copying remap data from vanilla to a new NIF
vanilla = open_nif("BaseMaleHead_faceBones.nif")
custom = open_nif("MyCustomHead_faceBones.nif")

# Find the NiBinaryExtraData blocks in vanilla
remap_data = get_block_by_name(vanilla, "CustomizationRemapData")
new_bones = get_block_by_name(vanilla, "CustomizationRemapNewBonesData")

# Add as children of the BSSubIndexTriShape in the custom NIF
add_binary_extra_data(custom, shape_block, "CustomizationRemapData", remap_data.bytes)
add_binary_extra_data(custom, shape_block, "CustomizationRemapNewBonesData", new_bones.bytes)
```

### Important constraints

- Vertex count MUST match the record count in CustomizationRemapData (records = size / 12)
- Bone indices MUST match the BSSkin::Instance bone order in the target NIF
- The face mesh MUST use `BSSubIndexTriShape` (not `BSTriShape`) for face parts
- Shader type MUST be set to Face (shader type 4) on the `BSLightingShaderProperty`
- The mesh needs the full set of `skin_bone_*` nodes in its skeleton hierarchy

## Python Decode Example

```python
import struct

def decode_customization_remap(data: bytes) -> list[dict]:
    """Decode CustomizationRemapData binary blob into per-vertex records."""
    records = []
    for i in range(0, len(data), 12):
        chunk = data[i:i+12]
        # 4 half-float weights (little-endian)
        w0, w1, w2, w3 = struct.unpack('<eeee', chunk[0:8])
        # 4 uint8 bone indices
        b0, b1, b2, b3 = struct.unpack('BBBB', chunk[8:12])
        records.append({
            'weights': (w0, w1, w2, w3),
            'bones': (b0, b1, b2, b3),
        })
    return records

def encode_customization_remap(records: list[dict]) -> bytes:
    """Encode per-vertex records back to CustomizationRemapData binary blob."""
    data = bytearray()
    for r in records:
        w = r['weights']
        b = r['bones']
        data += struct.pack('<eeee', *w)
        data += struct.pack('BBBB', *b)
    return bytes(data)
```
