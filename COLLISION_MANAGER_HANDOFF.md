# Collision Manager — Implementation Handoff

Self-contained brief for whoever (human or AI) builds the Collision Manager
in this NifSkope fork. Everything here was validated during the 2026-07
reverse-engineering sessions; nothing below is guesswork unless marked.

**Read alongside:** `TO_BE_IMPLEMENTED.md` section "Collision Manager —
CONCEPT (2026-07-09)" (the agreed feature spec) and
`docs/collision_manager_mockup.html` (interactive UI mockup — open in a
browser; click rows, switch Colour-by, untick Solid).

---

## 1. Project conventions (do these or the user will notice)

- **Update `WW_CHANGES.md` with every change batch, unprompted.** Newest
  entries first. This is a standing instruction from the user (bungo).
- Backlog lives in `TO_BE_IMPLEMENTED.md`.
- Build: MSYS2 UCRT64 **from PowerShell** (never from Git Bash — inherited
  MSYS env vars break qmake6):
  ```powershell
  $env:MSYSTEM='UCRT64'; $env:CHERE_INVOKING='1'
  C:\msys64\usr\bin\bash.exe -lc "cd /e/Projects/ClaudeNifskope && qmake6 NifSkope.pro && make -j8"
  ```
  Output: `release/NifSkope.exe`. qmake6 only needed after .pro changes.
- **Avast quirk**: freshly built unsigned exes may start SUSPENDED
  (CyberCapture cloud scan) — looks like a hang, 1 thread "Initialized".
  It resolves or the user has an exception for the project folder.
- **Git clone quirk**: Avast MITMs TLS. Recipe that works with verification
  kept ON: export the Avast root cert (CurrentUser\Root, thumbprint
  F02ED056F2A05EE71E5BFD4513643B529C44F2A6) to PEM, concatenate onto
  `E:/Tools/GIT/mingw64/etc/ssl/certs/ca-bundle.crt`, then
  `git -c http.sslBackend=openssl -c http.sslCAInfo=<combined> clone ...`
- The user tests builds in-game and in the app; ship builds, ask for
  verification, never claim untested things work.

## 2. What already exists (don't rebuild these)

| Piece | Where | State |
|---|---|---|
| Packfile decoder | `src/gl/hknpdecode.{h,cpp}` | Done. Shapes + per-body physics (`HknpBodyPhys`: layer/friction/restitution/COM/hasMotion; system: dynamic/mass/density/inertia/damping) |
| Decode spells | `src/spells/havok.cpp` (`spDecodeCompiledCollision`, `spDecodeAllCompiledCollision`) | Done. One bhkRigidBody+bhkCollisionObject **per body on its own node**; writes all decoded physics + validated authoring enums |
| Compiled-collision preview | `src/gl/glnode.cpp` (search `hknpDecodeCached`) | Done, but **wireframe only, amber** — the Manager work replaces this with solid display |
| Legacy collision preview | `src/gl/glnode.cpp` `drawHvkShape` | Upstream; layer-coloured wireframe |
| Mesh→convex spell | `src/spells/havok.cpp` `spCreateCVS` "Create Convex Shapes" | Upstream, works: BSTriShape/NiNode → bhkBoxShape (auto box detect) / bhkConvexVerticesShape, CoACD decomposition, bhkListShape/RigidBody/CollisionObject, settings dialog |
| Mesh-chain builder | `spDecodeCompiledCollision::buildShape` | Builds bhkNiTriStripsShape + NiTriStripsData chains and primitive blocks — reuse for BSTriShape→mesh collision |
| Vendored libs | `lib/` + `src/lib/qhull.cpp` | qhull (hulls), miniball (bounding sphere), CoACD (convex decomposition), meshoptimizer (decimation, see `simplify.cpp`) |
| Manager dock pattern | `src/spells/meshtools.cpp` `tlCreateMatTexManagerDock` + hookup in `nifskope_ui.cpp` (~line 1673) | Copy this pattern for `tlCreateCollisionManagerDock` in a new `src/spells/collisiontools.cpp` |
| Timeline dock pattern | `src/ui/widgets/timeline.cpp`, hookup `nifskope_ui.cpp` ~580 | Alternative pattern (QDockWidget, bottom area) |
| Python RE tools | `tools/hkparse.py` (packfile walker), `tools/hkdump.py` (extract blobs from nif), `tools/batch_validate.py` (corpus checks), `tools/test_hkdecode.cpp/.exe` (standalone decoder harness) | Extend for encoder validation |
| Material CRC hash | `hashFunctionCRC32` in `lib/libfo76utils/src/common.hpp`, usage example `src/lib/importex/obj.cpp` ~712 | CRC32 poly EDB88320, **init 0, no final XOR, lowercased** |

## 3. Validated format knowledge (the crown jewels)

FO4 `bhkPhysicsSystem` "Binary Data" = Havok 2014.1 binary packfile
(magic 57E0E057 10C0C010, v11, 64-bit ptrs, LE). Full walker in
`tools/hkparse.py` and `src/gl/hknpdecode.cpp`. Scale: Havok × **69.99125**
= game units.

**THE placement rule (hard-won, user-verified in the viewport):** each
`bhkNPCollisionObject` names its body via its **Body ID** field; that body
is placed by **that node's transform**. The hknpBodyCinfo position/rotation
is only Elric's rest pose — on vanilla stair helpers the two differ and the
node transform is authoritative. Never compose the cinfo transform into
geometry. (Independently confirmed by PyNifly's importer.)

`hknpPhysicsSystemData` (PSD) hkArray slots (each 16 bytes: u64 ptr via
local fixup, u32 size, u32 capacity|0x80000000):

| PSD offset | Array | Stride | Contents (validated offsets) |
|---|---|---|---|
| +0x10 | body_props | **0x50/body** | friction trunc-f16 @+0x12 (duplicate @+0x14), restitution @+0x16. Defaults 0.5/0.4 |
| +0x20 | dyn_motion | 0x40, count 1 | present ONLY on dynamic systems: gravity @+0x08 (1.0), maxLinVel @+0x10 (104.375), maxAngVel @+0x14 (31.57), linDamp @+0x18 (0.1), angDamp @+0x1C (0.05) |
| +0x30 | dyn_inertia | 0x40, count 1 | dynamic only: **inverseMass f32 @+0x04** (mass 10 → 0.1 exact), density @+0x08 (mass/volume), inertia diagonal @+0x20/24/28 |
| +0x40 | BodyCInfo | 0x60/body | shape ptr @+0x00 (global fixup), **motion idx @+0x0C (0x7fffffff = static body)**, qualityId byte @+0x18 (0xFF default), body index @+0x1A, **collision layer u32 @+0x1C**, position @+0x30, quaternion xyzw @+0x40 |
| +0x60 | ShapeEntry | 0x10 | shape ptr per body |

trunc-f16 = upper 16 bits of the float32 (`u16 << 16`, reinterpret).

Shapes (decoder handles all): hknpConvexPolytopeShape (convexRadius +0x14;
hkRelArrays: +0x30 verts hkVector4 w=0x3F000000|idx, +0x40 planes, +0x44
faces u16 first/u8 count/u8 flags, +0x48 u8 indices; sphere = 1 unique vert
+ radius; capsule = 2 unique verts), hknpCompressedMeshShape (+0x18 material
CRC; CMSD via global fixup at CMS+0x60), hknpCompressedMeshShapeData
(11-11-10 packed verts per section, shared verts u64 21-21-22 against the
object AABB, +0x70 shidx u16 map), hknpDynamicCompoundShape (0x80-byte
instances: 3×vec4 rotation rows, translation +0x30, scale +0x40, child ptr
+0x50 global fixup).

**Raw Max-exporter authoring values** (write these when creating legacy
bodies; from `C:\Users\bungo\Documents\3dsMax\export\meshes` dumps):

| Field | Dynamic prop | Static |
|---|---|---|
| Motion System | 3 | 5 |
| Quality Type | 4 (MOVING) | 0 |
| Solver Deactivation | 2 | 1 |
| Deactivator Type | 1 | 1 |
| Mass | authored (e.g. 10) | 0 |
| Common | fric 0.5, rest 0.4, linDamp 0.1, angDamp 0.05, maxLinVel 104.375, maxAngVel 31.5703, PenDepth 0.15, TimeFactor 1.0, CollisionResponse 1, ContactDelay 0xFFFF, NumShapeKeys 3 | same |

Field names: `bhkRigidBodyCInfo2014` struct in `release/nif.xml` (~line
3436), reached via `nif->getIndex(iBody, "Rigid Body Info")`. Layer via the
"Havok Filter" child compound ("Layer" field, `Fallout4Layer` enum, 57
entries 0–56). Materials: `Fallout4HavokMaterial` enum, 157 entries; value
= CRC32 (see §2 hash) of the name — legacy `Material_Metal`/`Material_Wood`
hash from short names "metal"/"wood"; custom names are legal, just hash
them.

## 4. PyNifly reference (github.com/BadDogSkyrim/PyNifly)

Independent RE + Blender plugin. **The packer is the prize**:
`io_scene_nifly/pyn/bhk_autopack.py` writes complete, game-accepted
packfiles (FixupBuilder, classnames, PSD, convex polytope, compressed mesh
single-section, sphere). Port it to C++ as `src/gl/hknpencode.{h,cpp}`.
Their format doc: `docs/fo4_havok_packfile_format.md`.

**Known PyNifly errors (do NOT copy):**
- body_props stride 0x110 → real stride is **0x50** (proven by signature
  scan on 6-body files)
- `collisionResponse @ body_props+0x10A` → garbage, reads past the entry
- their doc's body_props offsets are shifted by 0x10 vs their own code
- they miss the sphere's embedded center vertex, don't decode capsules,
  don't handle materials

**Worth stealing besides the packer:** u16 quad indices for compressed-mesh
sections whose local vert count > 255 (our decoder assumes u8 — real edge
case); their `num_vertices` derivation from firstVertex deltas.

## 5. Test assets & validation protocol

- **Controlled Elric pairs** (authoring ground truth):
  `C:\Users\bungo\Documents\3dsMax\export\meshes\*.nif` (raw Max exports)
  vs `...\export\processed\*.nif` (Elric-compiled). Includes PropCollision
  (mass 10, layer 10, dynamic), StairHelper{,Offset,Rotated} (6 bodies),
  Multi* variants, MetalAndWoodCollision.
- **Vanilla corpus**: ~300 architecture NIFs used by
  `tools/batch_validate.py` (see its header for how it loads them).
- **FileConvert.exe** (Creation Kit, in the user's Downloads/Examples):
  dumps pure-Havok packfiles to XML — use as an **oracle** for field values
  and as a **structural linter** for encoder output (it rejects malformed
  packfiles; it cannot read Bethesda's hknpBSMaterialProperties — that's
  expected, not a bug).

**Encoder acceptance gates (run all before claiming it works):**
1. `decode(encode(legacy_raw)) == decode(elric_processed)` field-by-field
   on every controlled pair the encoder claims to support.
2. Corpus round-trip: `decode(encode(decode(vanilla))) == decode(vanilla)`.
3. FileConvert accepts the emitted packfile.
4. Only then: in-game smoke test by the user (walk on it, shoot it).

## 6. Build plan (agreed phasing)

- **P1 — encoder core**: hknpencode (convex polytope + box + sphere, static
  bodies, multi-body with Body ID wiring) + "Compile Collision" spell +
  validation harness. This unlocks author-in-NifSkope end to end.
- **P2 — manager dock**: browser panel (one row per body), toolbar
  (Colour-by / Solid / X-ray / Collision only), physics panel, wire
  existing spells. Solid two-pass display replaces the wireframe preview
  (shared helper for compiled + legacy paths). Bottom-bar selection readout
  (main window currently has NO QStatusBar — add one).
- **P3 — create/convert**: capsule PCA fit, mesh-chain button,
  Collision→BSTriShape and back (proxy naming `COL_<node>` +
  NiStringExtraData marker), viewport G/R/S on collision (see
  TO_BE_IMPLEMENTED for the write-target table per shape kind).
- **P4 — heavy formats**: compressed-mesh encoder, compounds/instances,
  dynamic bodies (inertia tensor from geometry — Elric computes it too;
  the pairs give expected values: BoxProp cube → equal diagonal 48.9881),
  hknpBSMaterialProperties, multi-section meshes, u16 quad indices.

UI spec details (colour modes, wire-encodes-type palette, material families
bucketing, 57 collision types, custom material CRC field, presets) are all
in the TO_BE_IMPLEMENTED concept section — follow it as written; the
mockup HTML demonstrates every interaction.

## 7. Gotchas that cost us time (learn from our scars)

- **Trust controlled pairs over any doc** — including PyNifly's and this
  one. One well-chosen before/after pair settles what reasoning can't.
- The cinfo transform vs node transform confusion caused weeks of "offset
  stair helper" reports. If placement looks wrong, re-read §3's rule first.
- A shared bhkPhysicsSystem referenced by several nodes is the NORM
  (platform + ramps). Never draw or decode a system once per referencing
  node — that was the "duplicated collision" bug.
- `nif->set` on nested fields fails silently if the parent index is wrong —
  chain explicitly (`getIndex(iBody,"Rigid Body Info")` →
  `getIndex(iInfo,"Havok Filter")`) and follow obj.cpp's idiom
  (`set<quint32>` for enums).
- Enum rows with failing verconds exist in nif.xml three times (e.g.
  "Rigid Body Info" 550/2010/2014) — `getIndex` resolves the active one;
  this is the established pattern, don't fight it.
- Qt classifier/AV may block runs of fresh exes; a "hung" NifSkope right
  after build is usually Avast (§1).
