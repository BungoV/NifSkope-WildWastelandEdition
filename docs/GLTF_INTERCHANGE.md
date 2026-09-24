# glTF 2.0 interchange — the mapping contract

**What this page is.** The exact correspondence between a Fallout 4 NIF plus a
Havok `.hkx` clip and the `.gltf` + `.bin` pair this tree writes: the axes, the
unit, the node and bone order, the skin, the animation channels, and — named
one by one — everything that is lost on the way. It is the page
`src/gltfexport.cpp`, `src/gltfexportnif.cpp` and the three gate scripts all
implement; where they disagree with it, they are wrong.

**bungo's ruling, 2026-09-10 ~06:5x, verbatim: *"gltf sounds good"*.** FBX was
discussed and set aside: it has no public specification, Blender imports only
the binary form, and reading it would mean vendoring Assimp.

**Not related to the flow-map ruling.** The DirectX normal-map convention
bungo ruled for the water flow PNG on 2026-09-10 ~05:4x (`R = +X`, `G = +Y`
toward the image bottom) governs a TEXTURE. This page governs GEOMETRY and
ANIMATION and shares nothing with it; a reader who applies one to the other
gets a mirrored character.

**Status: BUILT AND MEASURED IN THE APPLICATION, 2026-09-10 14:37:53, lane
BUILD8.** `scratchpad/hkx4_20260910/hookup.py` is applied: `src/gltfexport.cpp`,
`src/gltfexportnif.cpp` and `src/lib/importex/gltfanim.cpp` are in
`NifSkope.pro`, the menu entry
`File > Export > .glTF (skeleton, skin, animation)` exists, and
`NifSkope -no-gui gltf` is in the CLI's own help. The CLI reproduces the
standalone driver's numbers exactly -- 139 nodes, 9 shapes (9 skinned), 6,443
vertices, 11,375 triangles, 78 of 95 tracks matched -- and the gates run on the
CLI's output unchanged: `tests/spells/gltf_check.py` 6,411 checks / 0 failures,
`tests/spells/gltf_readback.py` 3,234 checks / 2 pre-registered fixture
failures (the `LLeg_Toe1` pair of section 6).

**The round trip through this page and `docs/GLTF_IMPORT.md` is closed and
measured** (lane BUILD8): `.hkx -> .gltf -> .hkx`, compared bone for bone and
frame for frame against an independent decode of the original, worst
**8.0e-06 NIF units / 3.41e-05 degrees** on the vanilla `JogForward` (1,794
bone-frames) and **4.4e-05 units / 5.21e-06 degrees** on the 60 fps Mixamo clip
(7,254 bone-frames), against bars of 1e-4 units and 0.01 degrees. The 391 and
1,581 rows that do not survive are the 17 `Weapon*` tracks section 8 already
names, times the frame count.

---

## 1. The two exporters, and which one to use

| | `src/lib/importex/gltf.cpp` (upstream) | `src/gltfexport.cpp` + `src/gltfexportnif.cpp` (this page) |
|---|---|---|
| reaches | a static scene, through the GL `Scene` | the `NifModel`, no Scene, no GL |
| animation | never — its `animations` array is not populated | one clip, per-bone TRS channels at the clip's own rate |
| textures | decoded and embedded as PNG in the `.bin` | referenced by their game-relative `.dds` path, not written |
| materials | the full FO4/FO76/Starfield sets, alpha, LOD, `MSFT_lod` | one `pbrMetallicRoughness` per shape, base colour only |
| games | Skyrim SE, FO4, FO76, Starfield | FO4 (BS version 130) |
| gateable without building NifSkope | no | yes — that is why it exists |

They agree on the two conventions a user can see — the unit and the up-axis
rotation, sections 2 and 3 — so a mesh exported by either lands in the same
place in Blender. Use the upstream one for a static mesh with its textures;
use this one when the clip has to come with it.

## 2. Units

**1 NIF unit = 0.9144 / 64 = 0.0142875 metres exactly.** 64 units is one yard.
The constant is not this lane's invention: `src/lib/importex/gltf.cpp` has used
`0.9144 / 64.0` since upstream, and this writer takes the same value so the two
exports superimpose.

The measurement that makes it concrete on the fixture: the human male's head
bone `HEAD` stands at 116.6 NIF units above the skeleton root, and the whole
body mesh spans 120.25 units from crown to sole — **1.718 m**, a plausible
adult male. At the alternative constant 1/64 it would be 1.879 m, and at 1/100
it would be 1.20 m; 0.9144/64 is the only one of the three that gives a human.

Every position, every node translation, every animation translation and the
translation column of every inverse-bind matrix is multiplied by that constant.
**Rotations, scales and texture coordinates are not scaled.**

## 3. Axes

FO4 is **Z-up, right-handed**; glTF 2.0 is **Y-up, right-handed, metres**. The
conversion is not baked into the vertices — it is one node:

* glTF node **0** is a synthetic root named by `asset.extras.upAxisNode`
  (default `NifSkope_Y_up`) carrying the single quaternion
  `[-0.707106781, 0, 0, 0.707106781]`, a rotation of **−90° about X**;
* every NIF root hangs beneath it;
* everything below is in NIF axes and only scaled.

So NIF `(x, y, z)` appears in the world as glTF `(x, z, −y)`: NIF `+Z` (up)
becomes glTF `+Y` (up), and NIF `+Y` (the character's forward) becomes glTF
`−Z`, which is the direction Blender's viewport calls "forward". Undoing the
export is therefore: divide by the unit, then read the node tree; nothing else.

Keeping it in a node rather than in the data is what lets
`tests/spells/gltf_readback.py` compare the exported numbers to the NIF's own
directly, with no basis change in the comparison, and it is what lets a user
delete one node in Blender and get the raw game axes back.

## 4. Nodes

* Every `NiNode` (and every block inheriting it) under the exported root
  becomes one glTF node, in the file's own child order, with its **local**
  `translation` (scaled), `rotation` and `scale`.
* The NIF stores rotation as a `Matrix33`, **row-major**, applied as
  `v' = M v` (nif.xml: *"Stored in row-major format"*). It is converted to a
  unit quaternion by Shepperd's method — the branch with the largest pivot, so
  no square root of a small difference — and written glTF-style as
  `(x, y, z, w)`. NifSkope's own `Quat` is `(w, x, y, z)`; the swap happens in
  the writer and nowhere else.
* NIF scale is one float; glTF wants three, and the writer emits `[s, s, s]`.
  **A non-uniform scale cannot be represented by a NIF node and is therefore
  never produced.**
* Node names are the NIF's, unchanged, including FO4's mixed case. Matching a
  clip's bone names against them is **case-insensitive, first name wins**,
  because a player skeleton carries `Head` and `HEAD`, `Spine1` and `SPINE1`.
* A skinned mesh's node is written **without** a transform: the glTF spec says
  a skinned mesh node's transform is ignored, and FO4's skinned shapes carry
  the identity there anyway (measured: all nine shapes of the fixture and
  Bethesda's own `MaleBody.nif`).

## 5. Meshes

One glTF mesh per `BSTriShape`-family shape, always **exactly one primitive**,
mode 4 (triangles).

| glTF attribute | NIF source | conversion |
|---|---|---|
| `POSITION` | `Vertex Data / Vertex` (half or full precision) | × unit scale; `min`/`max` written, as the spec requires |
| `NORMAL` | `Vertex Data / Normal`, a `ByteVector3` | re-normalised to unit length; a byte normal arrives up to 0.004 off |
| `TEXCOORD_0` | `Vertex Data / UV`, a `HalfTexCoord` | copied unchanged; FO4's V axis already points down, as glTF wants |
| `JOINTS_0` | `Vertex Data / Bone Indices`, 4 bytes | copied, as `UNSIGNED_SHORT` |
| `WEIGHTS_0` | `Vertex Data / Bone Weights`, 4 halves | copied, as `FLOAT` |
| `indices` | `Triangles` | `UNSIGNED_SHORT` at ≤ 65,535 vertices, else `UNSIGNED_INT` |

**Four influences, always.** FO4 stores exactly four bone weights per vertex
and this writer emits exactly four. Half-float weights do not sum to one
exactly: on the fixture the row sums run **0.999695 … 1.000397**, and the gate
tolerance is 1e-3. They are written as they are stored — renormalising them
would make the export disagree with the game.

**How the partitions merge.** An FO4 body shape is a `BSSubIndexTriShape`: its
`Segment` array carries a dismemberment table (`Start Index`, `Num Primitives`,
`Parent Array Index`, and sub-segments) that FO4 uses to hide body parts. Those
are **draw ranges over the one vertex and index buffer**, not separate
geometry, so all of them belong to a single glTF primitive and no merging
decision has to be made. The table is not thrown away: it is written verbatim
into the primitive's `extras.mergedPartitions`, one sentence per segment.

Its arithmetic is checked, and the check found the rule: a segment's
sub-segments **re-describe that segment's own range, they do not extend it**.
Counting both levels on `BaseMaleBody:0` gave 4,351 triangles against 2,698
real ones; the export refuses unless the top-level segments' `Num Primitives`
sum to the shape's triangle count and each segment's sub-segments sum back to
that segment.

## 6. Skin

`BSSkin::Instance` gives the bone node list; `BSSkin::BoneData / Bone List`
gives one `BSSkinBoneTrans` per bone — `NiBound` (16 B), `Matrix33` (36 B),
`Vector3` translation (12 B), `float` scale (4 B).

**That stored transform IS the inverse bind matrix**, and it is written as the
glTF `inverseBindMatrices` accessor with only its translation scaled. It is not
a guess: upstream's `createInverseBoneMatrices()` writes `b.trans.toMatrix4()`
into the same accessor, and it is confirmed by measurement (below). Layout: the
NIF's row-major 3×3 times the bone scale goes into the upper 3×3 of a
COLUMN-major 4×4 (element `[c][r]` = `R[r][c] · s`), translation in elements
12–14, last row `0 0 0 1`.

**Joint order is the NIF's bone order, unchanged**, so `JOINTS_0` indexes the
skin's `joints` array directly with no remapping. `skins[i]` belongs to
`meshes[i]`; the writer refuses a scene where some meshes are skinned and
others are not, because that identity would no longer hold.

**A skin bone with no node under the exported root** is added as a child of the
scene root at its bind position, so the mesh still skins, and it is named in
the report and in the caller's message. It is never dropped silently.

### The measurement that fixes the convention

For every joint `k`, `J_k = global(joint_k) · inverseBind_k`. In Bethesda's own
`meshes/actors/character/characterassets/MaleBody.nif`, all 58 of them come out
as **one and the same rigid transform**, spread 0.00098 units:

> `J = translate(−0.0002, −0.8818, +120.8437)` NIF units.

Not the identity — and that is the fact a reader has to know. **FO4 body meshes
store their vertices with the origin at the top of the head**: `BaseMaleBody:0`
spans `z = −120.25 … −5.688`, and the skin is what stands the model on its
feet. A transposed inverse-bind, a shifted joint list, a dropped up-axis
rotation or a mis-composed node chain each break the constancy of `J`, which is
gate **R5** of `tests/spells/gltf_readback.py`.

The same measurement is the honest test of a NIF's own consistency. On
`fixtures/human_male_vanilla.nif` — the body parts grafted onto `skeleton.nif`
— every part agrees to 0.0016 units **except** `LLeg_Toe1`, which sits **2.865
units** away: `skeleton.nif` poses that bone differently from the pose
`MaleBody.nif`'s skin was authored against. The character will show that one
toe displaced in Blender. It is a property of the two shipped Bethesda files,
not of the export, and the vanilla donor exports clean.

## 7. Materials and textures

One material per shape, `pbrMetallicRoughness` with `metallicFactor` 0 and
`roughnessFactor` 1, `doubleSided` true. The diffuse is the first entry of
`BSShaderTextureSet / Textures` for a `BSLightingShaderProperty`, or
`Source Texture` for a `BSEffectShaderProperty` — both are read after the
leading `Shader Type` uint that `NiObjectNET` carries **only** for
`BSLightingShaderProperty` (nif.xml: `onlyT="BSLightingShaderProperty"`,
vercond `#BS_GTE_SKY# #AND# #NI_BS_LTE_FO4#`). Miss it and the whole block
reads four bytes out of step.

The path is written as an external `images[].uri`, normalised to
`textures/<...>` with forward slashes and percent-escaped; the raw NIF string
is preserved in the material's `extras.nifTexturePath`. A shape with no
diffuse gets `baseColorFactor [0.8, 0.8, 0.8, 1]` instead.

**The texture is NOT written and will not resolve.** glTF's specification
allows only PNG and JPEG images, and FO4 ships `.dds`; Blender reports
`Cannot read ...` for each and imports the model with the material present and
the image empty. This is deliberate — embedding the textures is the upstream
exporter's job — and it is the one loss a user sees immediately.

## 8. Animation

One glTF animation per exported clip.

* **Channels.** Per matched bone, up to three: `translation` (VEC3, scaled),
  `rotation` (VEC4 quaternion, `(x, y, z, w)`, re-normalised) and `scale`
  (VEC3, unscaled). Every channel of an animation shares ONE input accessor.
* **Times.** `input[i] = i · frameDuration`, in seconds, straight from the
  clip's own header. Vanilla third-person clips are 30 fps
  (`frameDuration` 0.0333); the Mixamo fixture is 60. The rate is not
  resampled, and the frame count and duration are repeated in
  `extras.frames` / `extras.frameDuration` / `extras.framesPerSecond`.
  **The two defaults do not compose** (lane BUILD8): this exporter writes
  whatever rate the clip has, and `docs/GLTF_IMPORT.md`'s importer defaults to
  30 fps for everything, so a DEFAULT round trip of the 60 fps fixture comes
  back at 30 -- 47 frames instead of 93. Pass `--source-rate` (or `--fps 60`)
  on the import to keep the grid; with it the 93 frames return one for one.
* **Interpolation is LINEAR, and that is exact.** FO4's spline-compressed
  clips are **degree 1** everywhere the whole-archive census saw
  (15,320 files): a frame IS a control point and the engine lerps — nlerp on
  rotations — between them. A cubic interpolation would be a different curve
  from the game's.
* **Bone matching** is by name, case-insensitively, through the clip's
  `transformTrackToBoneIndices` and the `hkaSkeleton`'s bone names. **Every
  unmatched track is listed by name** in `extras.unmatchedTracks` and in the
  caller's message. On the fixture, 78 of 95 tracks match; the 17 that do not
  are the `Weapon*` set, which `skeleton.hkx` carries and the body NIF does
  not.
* **An empty `transformTrackToBoneIndices` is the IDENTITY map**, not a
  missing one — measured on the Mixamo clip by lane FIXTURE and implemented in
  `src/hkxanim.cpp`. Without that rule a converted third-party clip is refused
  outright.

### Root motion

`hkaDefaultAnimatedReferenceFrame` gives one `(x, y, z, yaw-about-up)` per
frame, separate from track 0. It is **left out by default** and applied only
behind a flag (`--root-motion` on the CLI, `applyRootMotion` in the struct):

* **off** — the clip plays in place, and `extras.rootMotion` says
  `"present in the clip, omitted on request"` so nothing is silently dropped.
  This is the zero-effort way back required of any visible behaviour
  (CONSTITUTION 7);
* **on** — for the root bone only: `translation += rootMotionTranslation[f]`
  (then scaled) and `rotation = yaw(f) · rotation`, with the yaw taken about
  the clip's own `up` vector. No other node is touched.

**WHICH node that is, and why it matters on the way back** (lane BUILD8,
2026-09-10). The travel goes onto the node of the clip's ROOT BONE -- `Root` on
the player skeleton -- and NOT onto the glTF's scene root, which is the NIF's
own root `NiNode` (`skeleton.nif` on the fixture) and drives no bone at all.
The importer's default is the scene root, so a re-import must be told the node
by name:
`NifSkope -no-gui gltf-import ... --root-motion --root-node Root`.
Without it the import refuses, in words -- *"root motion was asked for from
'skeleton.nif', which has no track (it reached no bone)"* -- which is correct
behaviour on both sides, but neither page said it until it was measured. With
it, the 165.354 units of `JogForward`'s travel come back to **1.5e-05 units and
0.0 degrees of yaw**.

A clip can carry root motion and have it be **all zero** — the Mixamo fixture
does, and its 487-unit travel is on the `COM` track instead. Read the maximum
over all samples, never first-versus-last, which a looping clip also reports
as zero.

## 9. What is lost, in one list

| lost | why | recoverable? |
|---|---|---|
| the texture bytes | glTF allows only PNG/JPEG images; FO4 ships `.dds` | yes — the path is in the uri and in `extras.nifTexturePath` |
| normal, specular, glow maps and every shader flag | this exporter writes base colour only | no; use the upstream `.glTF` export for a static mesh |
| tangents and bitangents | glTF regenerates them from UVs | no, and no consumer wants the stored ones |
| vertex colours, the second UV set, eye data | not carried | no |
| collision (`bhkNPCollisionObject`, ragdoll), connect points, `BSBound`, `BSXFlags`, extra data | no glTF equivalent | no |
| `NiTransformController` / `NiTransformData` — the NIF's OWN animation | only the `.hkx` clip is exported | no; a NIF-controller export is a later lane |
| float tracks and annotations of the clip | no glTF equivalent | no |
| the dismemberment table as structure | it is a draw range, not geometry | yes — written verbatim into `extras.mergedPartitions` |
| unmatched animation tracks | their bone has no node in this file | yes — named in `extras.unmatchedTracks` |
| a node's `Flags`, `Collision Object` and controllers | not carried | no |
| exact weight sums | FO4's half-float weights sum to 1 ± 4e-4 | they are exported as stored, so nothing is lost |

## 10. The file, in order

`asset` (with `extras.metresPerUnit`, `extras.sourceUpAxis` = `"Z"`,
`extras.upAxisNode`) · `scene` · `scenes` · `nodes` · `meshes` · `materials` ·
`images` / `textures` / `samplers` (only when a diffuse exists) · `skins` (only
when a mesh is skinned) · `animations` (only when a clip was given) ·
`accessors` · `bufferViews` · `buffers`.

Every array that would be empty is **omitted**, because an empty array is a
spec violation (`EMPTY_ENTITY`); a node-only export writes no `accessors`,
no `bufferViews`, no `buffers` and no `.bin` at all.

One buffer, one `.bin` beside the `.gltf` with the same base name. Every
`bufferView` is 4-byte aligned and tightly packed — one view per accessor, no
`byteStride`, no interleaving, no sparse accessors.

Numbers are written with 9 significant digits, which round-trips an IEEE-754
binary32 exactly.

## 11. How to reproduce the fixtures

```bash
# build the standalone driver (MSYS2 UCRT64, no NifSkope.exe needed)
bash scratchpad/hkx4_20260910/build_dump.sh

# the vanilla character + a vanilla 30 fps clip
release/gltfexport_dump.exe --skeleton fixtures/human_male_vanilla.nif \
    --mesh fixtures/human_male_vanilla.nif \
    --clip scratchpad/hkx1_20260910/clips/jog.hkx \
    --bones scratchpad/hkx1_20260910/clips/skeleton.hkx \
    --name JogForward --out scratchpad/hkx4_20260910/out/human_male_jog.gltf

# the Mixamo 60 fps clip, root motion applied
release/gltfexport_dump.exe --skeleton fixtures/human_male_vanilla.nif \
    --mesh fixtures/human_male_vanilla.nif \
    --clip fixtures/Running_To_Slide_And_Back_To_Running.hkx \
    --bones scratchpad/hkx1_20260910/clips/skeleton.hkx \
    --name RunningToSlide --root-motion \
    --out scratchpad/hkx4_20260910/out/human_male_mixamo.gltf

# the gates
bash tests/spells/gltf_gates.sh
```

After the hook-up in `scratchpad/hkx4_20260910/hookup.py` is applied and
NifSkope is built, the same export is
`NifSkope -no-gui gltf <file.nif> -o out.gltf --clip C.hkx --bones S.hkx`
and `File > Export > .glTF (skeleton, skin, animation)`.

---

## Provenance

Written by lane HKX4b, 2026-09-10, against these files. Every claim above is
addressed by an anchor rather than by a line number alone; the numbers were
re-derived from the anchors in one scripted pass
(`scratchpad/hkx4_20260910/anchors.py`) after the last edit.

| file | sha256 (16) | bytes | lines | CR |
|---|---|---|---|---|
| `src/gltfexport.h` | `77e2b2f7e2010ca5` | 5,434 | 128 | 0 |
| `src/gltfexport.cpp` | `cbd5ce5e0e569a34` | 29,592 | 804 | 0 |
| `src/gltfexportnif.h` | `518d8f981a663d32` | 4,695 | 104 | 0 |
| `src/gltfexportnif.cpp` | `15cd1ae6b0d2e737` | 17,971 | 482 | 0 |
| `src/lib/importex/gltfanim.cpp` | `0f180bf891dc3e97` | 3,777 | 89 | 0 |
| `tests/gltfexport_dump.cpp` | `1e1c6297bb10b5cf` | 30,677 | 801 | 0 |
| `tests/spells/gltf_check.py` | `4129c7760dcb4d61` | 15,108 | 297 | 0 |
| `tests/spells/gltf_readback.py` | `a15945aa73d1e532` | 25,395 | 514 | 0 |
| `tests/spells/gltf_nifread.py` | `35d5b4dc17e16f53` | 14,367 | 325 | 0 |
| `tests/spells/gltf_sabotage.py` | `18de140a4cc32b98` | 6,455 | 155 | 0 |
| `tests/spells/gltf_blender_check.py` | `71813ba79be547ac` | 5,409 | 137 | 0 |

| claim | cite | anchor |
|---|---|---|
| 1 unit = 0.9144/64 m, this writer | `src/gltfexport.h:114` | `float unitScale = 0.9144f / 64.0f;` |
| 1 unit = 0.9144/64 m, upstream | `src/lib/importex/gltf.cpp:112` | `return Vector3( float( double( v[0] ) * ( 0.9144 / 64.0 ) )` |
| the −90° X rotation on the synthetic root | `src/gltfexport.cpp:544` | `// the up-axis root: rotate Z-up (NIF) into Y-up (glTF) = -90 deg about X` |
| positions scaled, rotations not | `src/gltfexport.cpp:399` | `f[v * 3 + 0] = mesh.positions[v][0] * U;` |
| the inverse-bind translation is the only part scaled | `src/gltfexport.cpp:438` | `ibm[j2 * 16 + 12] *= U;` |
| a skinned mesh node is written with no transform | `src/gltfexport.cpp:566` | `// a skinned mesh node's transform is ignored by the spec, and the` |
| NifSkope `Quat` is (w,x,y,z), glTF (x,y,z,w) | `src/gltfexport.cpp:85` | `Q4 fromQuat( const Quat & q )` |
| normals re-normalised on export | `src/gltfexport.cpp:407` | `// a NIF byte normal can arrive slightly off unit; glTF wants unit` |
| UNSIGNED_SHORT indices below 65,536 | `src/gltfexport.cpp:444` | `if ( nv <= 65535 ) {` |
| one input accessor per animation | `src/gltfexport.cpp:454` | `// animations: one shared time accessor per animation, then the channels` |
| LINEAR interpolation | `src/gltfexport.cpp:701` | `+ ",\"interpolation\":\"LINEAR\",\"output\":"` |
| root motion composed onto the root node only | `src/gltfexport.cpp:496` | `q = qmul( qaxis( an.rootMotionUp, double( an.rootMotionYaw[i] ) ), q );` |
| the rootMotion sentence in extras | `src/gltfexport.cpp:717` | `: ( an.rootMotionTranslation.isEmpty() ? "none in the clip" : "present in the clip, omitted on request" ) ) + "\"";` |
| skin index == mesh index, or refuse | `src/gltfexport.cpp:673` | `// skin index == mesh index is only legal when every mesh is skinned` |
| empty arrays omitted | `src/gltfexport.cpp:594` | `// meshes + materials + textures. An EMPTY glTF array is a spec violation` |
| a node-only export writes no buffer | `src/gltfexport.cpp:733` | `if ( buf.accessors.isEmpty() ) {` |
| 9 significant digits | `src/gltfexport.cpp:34` | `return QByteArray::number( v, 'g', 9 );` |
| four influences per vertex | `src/gltfexport.h:45` | `//! FO4 stores exactly four influences per vertex.` |
| the raw NIF texture path kept in extras | `src/gltfexport.h:69` | `QString diffuseSourcePath;` |
| the `Shader Type` uint only on BSLightingShaderProperty | `src/gltfexportnif.cpp:88` | `if ( nif->blockInherits( iShader, "BSEffectShaderProperty" ) )` |
| the texture path normalised to `textures/` | `src/gltfexportnif.cpp:70` | `QString normaliseTexture( const QString & raw )` |
| the inverse-bind layout, column-major | `src/gltfexportnif.cpp:53` | `void toInverseBind( const Transform & t, QVector<float> & out )` |
| sub-segments re-describe their segment | `src/gltfexportnif.cpp:101` | `QStringList segmentsOf( const NifModel * nif, const QModelIndex & iShape, int numTris, QString & error )` |
| a skin bone outside the subtree is added and named | `src/gltfexportnif.cpp:338` | `// A skin bone outside the exported subtree. It becomes a` |
| case-insensitive bone match, first name wins | `src/gltfexportnif.cpp:211` | `if ( !nodeByName.contains( key ) )` |
| the seam to the Animation workspace | `src/gltfexportnif.h:100` | `typedef bool ( *GltfExportClipProvider )( HkxAnimClip & clip, QStringList & boneNames );` |
| the menu says so when no clip is loaded | `src/lib/importex/gltfanim.cpp:82` | `msg += tr( "\n\nNO ANIMATION was written: no clip is loaded in the Animation workspace. "` |
| gate R5, the bind pose is one rigid transform | `tests/spells/gltf_readback.py:367` | `# ---- R5: the bind pose is one rigid transform -------------------` |
| the segment arithmetic, driver side | `tests/gltfexport_dump.cpp:443` | `// A segment's sub-segments RE-DESCRIBE that segment's own` |
| the NiObjectNET prefix, driver side | `tests/gltfexport_dump.cpp:281` | `// The FO4 (BS version 130) shader-property prefix, from nif.xml:` |
| the unsequenced-read fix | `tests/gltfexport_dump.cpp:227` | `// Three reads in one argument list are UNSEQUENCED: g++ evaluates` |

Facts taken from other pages rather than re-derived here:
`docs/HKX_ANIMATION_FORMAT.md` (the clip container, the spline decompressor,
degree-1 splines, root motion, the 15,320-file census) and
`scratchpad/lane_hkx1_report.md` / `scratchpad/lane_fixture_report.md` (the
fixture set, the bone-name case differences, the Mixamo clip's empty binding).
`release/nif.xml` is the authority for every NIF field order quoted above.
