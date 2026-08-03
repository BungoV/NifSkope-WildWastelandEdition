# Bone & Weight Transfer — Implementation Plan

> **BACKLOG MOVED — 2026-07-21.** This file is **design detail / history only**.
> The single authoritative list of what is left to implement is
> **`TO_BE_IMPLEMENTED.md`**. Do not record open work here and do not trust any
> status claim below: this file's own claims have been wrong in both directions
> before, which is exactly why the backlog was consolidated into one place.


Transfer skinning (bone bindings **and** per-vertex weights) from a donor mesh onto a
target mesh inside NifSkope, in-place, with no DCC round-trip. Primary use case: rig a
piece of outfit/armor/headgear to a human skeleton by borrowing the skinning of a donor
body/head mesh.

Status: **Phase 3A release candidate.** The cross-file FO4 workflow for an already-skinned
target is implemented, integrated into the Rigging workspace, and covered by a native
end-to-end suite. Prerequisite research (FO4 skin format, faceBones semantics) remains
documented below.

### Current implemented scope

- FO4 BS version 130, one skinned donor shape to one already-skinned target shape.
- Cross-file donor selection, multi-shape donor choice, closest-face barycentric transfer,
  top-four normalization, donor hierarchy import, missing bone binding, target-derived bone
  bounds, and byte-exact `CustomizationRemapData` generation/update.
- One atomic user workflow with preflight geometry/bone/remap summary, relative snap-distance
  diagnostics, responsive cancellation, exact rollback, and one Undo entry.
- A dedicated Rigging workspace plus independently usable advanced inspection/repair spells.
- Read-only viewport inspection: a common-space cyan donor geometry overlay with
  fill/wireframe/opacity controls, plus a selected-bone target weight heatmap using a
  blue→cyan→yellow→red ramp.
- Repository-owned native integration coverage for direct spells and the real workspace
  button, including undo/redo, save/reload, cancellation, rejection, rollback, multi-shape
  selection, changed topology, poor-distance detection, and normalized output weights.

### Remaining or intentionally deferred scope

- Creating a complete new `BSSkin::Instance`/`BSSkin::BoneData` and packed skin vertex
  layout on an entirely unskinned target. Phase 3A deliberately requires an existing FO4
  skin so it can preserve the standard-skeleton skin in `CustomizationRemapData` before
  replacing weights. This is the largest remaining Phase 3 backend, not a hidden v1 claim.
- Weight painting is now implemented as a first-class viewport mode with continuous strokes,
  selection masking, Add/Subtract/Replace/Smooth operations, accumulation, normalized four-slot
  FO4 output, and one Undo entry per stroke. Mirroring, mapping-mode/max-influence controls, an
  independent persistent skeleton reference, pruning structurally bound but zero-result bones,
  classic NiSkin, and `CustomizationRemapNewBonesData` remain follow-on work.
- Native production verification covers female-head to beard-hairline, male-head, MaleBody,
  and a different-surface Combat Armor outfit pair. In-game FaceGen/morph and visual behavior
  are still required before calling the feature production-proven.

---

## 1. Why this, and why first

The originating question was "generate FO4 `CustomizationRemapData` for custom headgear."
Investigation (byte-level, validated against vanilla) established that:

- `CustomizationRemapData` is **a copy of the mesh's standard-skeleton skinning**
  (4×float16 weights + 4×uint8 bone indices per vertex), serialized into a
  `NiBinaryExtraData` blob. An encoder that regenerates it from a mesh's existing skin
  reproduces the vanilla blob **byte-for-byte** (verified on `BaseFemaleHead`).
- What actually makes headgear *conform* to different faces is **the mesh's own skin
  bound to the face sculpt bones** (`skin_bone_C_*`) — i.e. skinning transferred from the
  base head. `CustomizationRemapData` does **not** do the conforming; it is the
  "remap back to the animation skeleton" bookkeeping.

Conclusion: **weight/bone transfer is the primitive; RemapData generation is a thin
serialization layer on top of it** (the encoder is already written and proven). Building
transfer first solves the real problem (conforming outfits to a donor) and makes RemapData
authoring fall out almost for free. Building RemapData first would deliver the less-useful
half and still require transfer afterward.

## 2. Differentiator vs. Blender DataTransfer

Blender's DataTransfer modifier (Nearest Vertex / Nearest Face Interpolated / Projected
Face Interpolated, etc.) already does high-quality **weight** transfer — but it only moves
weights into **vertex groups that already exist**, because the armature is already present
and groups map by name. It also requires a `nif → blend → nif` round-trip that can mangle
NIF-specific data (vertex layout, tangents, extra data, BS-specific blocks).

This feature must additionally **establish the skinning itself** on a target that may have
none:

- create the bone binding (`BSSkin::Instance` + `BSSkin::BoneData`, or
  `NiSkinInstance` + `NiSkinData`) referencing the donor's bones,
- compute per-bone **inverse bind (skin-to-bone) transforms**,
- write weights+indices into the target's vertex buffer, set the `Skinned` vertex flag,
  fix stride / data size,
- (re)generate skin partitions,
- ensure the referenced **bone nodes exist** in the target's scene graph.

"Transfer the bones too" = all of the above, not just the weights. That, plus in-place /
no round-trip, is the reason to do it in NifSkope.

## 3. Scope

**v1 (this plan):**
- Single donor shape → single target shape.
- Mapping modes: `Nearest Vertex` (fast/simple) and `Nearest Face Interpolated`
  (closest point on donor triangle + barycentric) — mirroring Blender's naming.
- Max 4 influences per vertex (game standard); prune + renormalize.
- Support **FO4 `BSSkin`** first (motivating case). Structure the geometry core so the
  classic `NiSkinInstance`/`NiSkinData` path (Skyrim/FO3/Oblivion) can be added after.
- Bone-set handling: union of donor bones actually used after transfer; skip donor bones
  that end up with zero influence on the target.

**Explicitly out of v1:** multi-donor blending, heat/geodesic/RBF diffusion,
projected-along-normal mapping, partial/masked transfer, transferring non-weight vertex
data (normals/colors/UVs). Note them as future work.

## 4. Data model

### 4.1 Two skin representations (do not conflate)

| | Classic (Skyrim/FO3/Oblivion) | FO4 |
|---|---|---|
| Instance | `NiSkinInstance` / `BSDismemberSkinInstance` | `BSSkin::Instance` |
| Bone data | `NiSkinData` (per-bone transform + vertex weight lists) | `BSSkin::BoneData` (per-bone bound + transform) |
| Weights live in | `NiSkinData` bone weight lists (+ `NiSkinPartition`) | **vertex buffer** (`BSVertexData`, 4×half weight + 4×byte index at skin offset) |
| Partition | `NiSkinPartition` | `NiSkinPartition` (still present) |
| Bone list | `NiSkinInstance.Bones` (node refs) | `BSSkin::Instance.Bones` (node refs) |

The existing spells in `src/spells/skeleton.cpp` (`Fix Bip01`, `Make Skin Partition`,
`spSkinPartition`) operate on the **classic** `NiSkinInstance`/`NiSkinData` path. **FO4
`BSSkin` is not handled there** — the transfer feature must read/write it explicitly.

### 4.2 FO4 vertex skin layout (verified)

Per-vertex, at `skinOffset = ((vertexDesc >> 28) & 0xF) * 4` inside a `stride =
(vertexDesc & 0xF) * 4` byte vertex; `Skinned` flag = bit `0x40` of the vertex-flags field
(bits 44+ of the u64 desc):
- bytes 0–7: 4× `float16` bone weights (sum ≈ 1.0)
- bytes 8–11: 4× `uint8` bone indices (into `BSSkin::Instance.Bones`)

This is exactly the `CustomizationRemapData` record format — the RemapData blob is this
same data lifted out for the standard-skeleton binding.

## 5. Algorithm (geometry core)

Pure function, no NIF types, unit-testable:

```
input:  donorVerts[] (position, 4 weights, 4 boneGlobalIds)
        donorTris[]  (i0,i1,i2)
        targetVerts[] (position)
        mode
output: targetSkin[] (list of (boneGlobalId, weight), normalized, ≤4)
```

1. Transform donor and target verts into a **common space** (both to skeleton/world via
   their node transforms; see §6.3 — mismatched local spaces are the classic footgun).
2. Build a spatial accelerator over donor geometry. For head/body meshes (~1k–10k verts)
   a uniform grid or even brute force is acceptable in v1; leave a hook for a BVH.
3. Per target vertex:
   - `Nearest Vertex`: nearest donor vertex → copy its (bone,weight) set.
   - `Nearest Face Interpolated`: closest point on nearest donor **triangle** →
     barycentric coords `(u,v,w)` → blend the three donor verts' weight sets by bone id
     (accumulate into a map keyed by global bone id).
4. Normalize; keep **top-4** by weight; renormalize so they sum to 1.0.
5. Return per-target-vertex skin keyed by **global bone identity** (node ref / name), not
   donor-local index — the target's bone list is built afterward from the union actually
   used.

Reuse NifSkope math: `Vector3`, `Triangle`, `Transform`, `Matrix`/`Matrix4`, `BoundSphere`
(see `src/spells/skeleton.cpp`, `meshtools.cpp`, `tangentspace.cpp` for existing usage
patterns). Add only a closest-point-on-triangle helper.

## 6. NIF write plumbing (the fiddly part / real research)

### 6.1 Build the target bone list
- Collect the set of global bones used by `targetSkin`.
- For each, ensure a corresponding **bone node exists in the target file** (by name). If
  absent, create the `NiNode` (or reuse skeleton import) with the donor's node transform.
- Assign each a target-local index; remap `targetSkin` bone ids → local indices.

### 6.2 Create/populate the skin blocks
- **FO4:** insert `BSSkin::Instance` (skeleton root, bone refs, `Data` → `BSSkin::BoneData`),
  and `BSSkin::BoneData` (per-bone bound sphere + transform). Write weights/indices into
  the vertex buffer; set `Skinned` in the vertex desc; recompute stride, skin offset,
  `Data Size`. Rebuild the vertex array bytes.
- **Classic (later):** insert `NiSkinInstance`/`NiSkinData`, per-bone vertex weight lists.

### 6.3 Inverse bind (skin-to-bone) transforms
For each bone: `skinToBone = inverse(boneWorld) * meshWorld` (exact convention must match
what the engine/existing NifSkope skin code uses — cross-check against `Fix Bip01` and a
vanilla skinned mesh). **This is the highest-risk math**; validate by round-tripping a
vanilla mesh (strip skin, re-transfer from itself, compare bounds/positions in the render
view).

### 6.4 Partitions & bounds
- Run existing `spSkinPartition` logic to (re)generate `NiSkinPartition`.
- Update bone bounding spheres (`BSSkin::BoneData` bounds; cf. any existing
  update-bounds spell).

### 6.5 Bone pruning
Drop bones with no residual influence after top-4 pruning so the target doesn't carry a
bloated bone list. Keep a report of dropped/added bones for the UI.

### 6.6 Cross-file donor & skeleton reference (the PRIMARY workflow)
The donor is almost always in **another file** — `skeleton.nif` for bones, a separate
body/head nif for the donor mesh, your outfit as the target. Same-file donor is the rare
case. Two distinct reference inputs, which may or may not be the same file:

- **Donor mesh**: a *skinned* shape to borrow weights from (has geometry + skin).
- **Skeleton reference**: usually `skeleton.nif` — **bones only, no mesh** — the authority
  for bone rest transforms + hierarchy. A persistent "active skeleton" slot in the Rigging
  dock; every tool reuses it.

Why the skeleton matters beyond adding bones:
- **Common space.** Donor and target live in different files/roots — can't closest-point
  naively. The shared skeleton is the common frame: transform both meshes into
  bind/skeleton space via node transforms, match **by bone name** (proven to work in the
  RemapData analysis). This is what makes cross-file transfer well-defined.
- **Creating missing bones.** When transfer/paint needs a bone the target lacks, pull that
  `NiNode` (name + rest transform) from the skeleton and splice it in.

**Existing precedent to reuse — this is largely solved:** `spCollisionManager::importDonorCollision`
(`src/spells/collisiontools.cpp:1356`) already does cross-file import: `QFileDialog` seeded
at `nif->getFolder()` → standalone `NifModel donor; donor.loadFromFile(fileName)` →
`getBSVersion()` guard → enumerate donor blocks → serialize a donor branch
(`saveIndex` into a `QDataStream`/`QBuffer`) → splice into the target with a
`QMap<qint32,qint32>` block-number remap. Follow this verbatim for pulling bone nodes /
reading a donor shape.

**Architectural rule this forces (get right early):** the skin layer + transfer/compare
core must take **`(NifModel *, blockIndex)` pairs for both donor and target** — never
assume "the active document." Cross-file has to be designed in from the start or it's a
painful refactor. The bone-list and compare panels then accept a donor from any file for
free.

Guards (mirror the collision code): bsVersion/game match; handle node scale baked into
transforms; remember a "recent skeleton" (FO4 default:
`meshes/actors/character/characterassets/skeleton.nif`).

## 7. Validation strategy (de-risk before C++)

Follow the pattern that worked for the RemapData research: **prototype + validate against
vanilla ground truth in Python first**, then port the validated algorithm to C++.

- **Ground-truth test:** donor = `BaseFemaleHead_faceBones.nif` (has the sculpt-bone rig);
  target = `BigBeard01_Hairline_faceBones.nif` (shares the face/neck surface, has a *known
  vanilla* sculpt skinning). Transfer donor sculpt weights → hairline; compare against the
  hairline's actual vanilla vertex skin. Expect close (not necessarily byte-exact — Beth's
  exporter differs), quantify mean per-vertex weight error and bone-set agreement.
- **Self-transfer test:** strip a mesh's skin, transfer from an untouched copy of itself;
  expect near-identity. Catches space/transform bugs (§6.3).
- Python parser already exists in scratchpad (`nifparse.py`, `compare.py`, `encode_test.py`)
  and reads FO4 `BSSkin` + vertex skin. Extend it for the transfer prototype.

### 7.1 Phase-1 prototype results (DONE — scratchpad `nifshape.py` + `transfer.py`)
Closest-point-on-triangle + barycentric transfer, top-4 prune + renormalize, positions in
common (node) space. Both modes implemented (`vertex` = nearest donor vertex, `face` =
closest point on incident triangle).

- **Self-transfer (head→head): PASS, near-exact.** meanL1 = 0.0001, dominant-bone match
  100% (vertex) / 99.9% (face, one tie-break flip), snap distance 0. Validates the space
  handling, closest-point math, and barycentric interpolation end-to-end.
- **Cross-mesh (head→hairline): PASS, plausible.** ~83–84% dominant-bone match, meanL1
  ≈ 0.34 (i.e. ~83% of weight mass agrees), median snap 0.19u. Bone sets fully overlap
  (hair's 53 ⊂ head's 68, 0 hair-only dominant bones) and error is **uniform across snap
  distance**, so the residual is genuine divergence from Bethesda's independently
  hand-authored hairline skin (a complex jaw region, verts floating ~0.19u off the scalp),
  not a bug. `face` mode ≈ `vertex` here only because the target floats off-surface; on a
  target lying on the donor surface `face` wins (and self-transfer is exact).
- **Takeaway:** transfer alone reaches ~80–85% agreement with hand-authored vanilla — which
  is exactly why **manual weight paint is a required follow-up**, not optional polish.

### 7.2 Inverse-bind (skin-to-bone) write math — VALIDATED
Proven in Python (`invbind.py`, `invcheck.py`) against vanilla `BSSkin::BoneData` across
female head, male head, beard, and hairline:

- **Conventions pinned:** rotation matrices used **as-stored** (no transpose), `world =
  parent @ local`, local `M = [rot·scale | trans]`.
- **Formula:** `skinToBone_k = inverse(boneWorld_k) @ meshBindWorld`, where `boneWorld_k`
  accumulates the NiNode parent chain and `meshBindWorld` is a single per-mesh bind
  transform. Recovering `meshBindWorld = boneWorld_k @ stored_k` is **consistent across all
  bones** (deviation ~1e-5), and the formula reproduces the vanilla transforms to ~1e-5.
- **All head-attached FaceGen assets share** `meshBindWorld = T(0, -0.88, 120.84)` (the HEAD
  bone position) — they're authored at origin and bound at the head. For body outfits
  authored in-place, `meshBindWorld` ≈ identity → `skinToBone_k = inverse(boneWorld_k)`.
  The write path must source `meshBindWorld` per target (identity for in-place meshes; the
  attach-bone offset for origin-authored heads) — a documented input, not a blocker.
- **Round-trip:** regenerated BoneData transforms match vanilla float32 to 1.6e-5
  (180/884 floats already bit-identical); weight bytes re-pack exactly.

**Status: all risky skin math (weights + bone transforms + RemapData) is de-risked in
Python. Remaining work is NIF block assembly / partitions / C++ port / UI — mechanical
plumbing best done in the NifSkope context with build verification.** Bone bounding spheres
in BoneData are non-critical (culling); an approximate correct bound is fine — do not chase
byte-exact bounds (Bethesda's sphere algorithm differs).

### 7.3 C++ core ported and verified (DONE — `tools/rigging_prototype/`)
The pure math core is ported to C++ and checked against the Python ground truth using the
UCRT64 g++ (`C:\msys64\ucrt64\bin`, run inside its bash env — see prototype README):

- `skincore.cpp` — closest-point-on-triangle, 4x4 multiply + Gauss-Jordan inverse, inverse-
  bind. All tests PASS (closest-point to machine precision; inverse-bind to 1e-6).
- `skintransfer.cpp` — end-to-end transfer (nearest-vertex → incident-triangle refine →
  barycentric blend → top-4 normalize) on the real head→hairline case (`dump_case.py`):
  **reproduces the Python result exactly** — meanL1 0.00000, 100% dominant-match, 676 verts.

The validated Python + C++ prototype now lives in-repo at `tools/rigging_prototype/` (with
README) so the foundation survives. C++ portable structs map 1:1 to NifSkope
`Vector3`/`Matrix4` on integration. **The entire transfer/skin algorithm is implementation-
ready; only NifModel I/O + UI remain.**

### 7.4 Native NifSkope production-asset validation (DONE for head/hairline and non-head skin)

The repository runner now has a reusable real-asset mode that runs the actual atomic
NifSkope spell, validates the saved/reloaded FO4 skin, proves byte-exact Undo/Redo, and
optionally compares the result by bone name against a vertex-compatible vanilla reference.
It was run against the original extracted Fallout 4 assets without Blender or a DCC:

- `BaseFemaleHead_faceBones.nif` → `BigBeard01_Hairline.nif`, compared with Bethesda's
  `BigBeard01_Hairline_faceBones.nif`: **676 vertices, Caution, 8→69 bones, 8,112 remap
  bytes, mean L1 0.343375, max L1 1.88278, 83.58% dominant-bone agreement.** This matches
  the independently validated Python ground truth (~0.34 / ~83–84%) and confirms the
  expected divergence from Bethesda's hand-authored hairline rather than a write-path bug.
- `BaseMaleHead_faceBones.nif` → `BaseMaleHead.nif`, compared with the donor/reference:
  **1,696 vertices, Close, 10→69 bones, 20,352 remap bytes, mean L1 2.25823e-06, max L1
  0.000274658, 99.88% dominant-bone agreement.** This confirms near-identity transfer on
  a second production head and exercises a distinct male hierarchy/mesh.
- `MaleBody.nif` → `MaleBody.nif`, a standard-skeleton non-head self-surface case:
  **1,523 vertices, Close, 58→58 bones, 18,276 remap bytes, mean L1 1.70321e-07, max L1
  6.10352e-05, 100% dominant-bone agreement, and 0→0 validation problems.** This exposed
  and fixed a real ordering bug: face-sculpt transfer snapshots RemapData before replacing
  the standard skin, while standard-only transfer refreshes it after the final float16 write.
- `F_Torso_Mid.nif` → `F_Torso_Lite.nif`, a different-surface standard-skinned Combat
  Armor outfit pair compared with the original Lite weights: **32 target vertices, Caution,
  8→8 bones, 384 remap bytes, mean L1 0.0439682, max L1 0.116577, 100% dominant-bone
  agreement, and 0→0 validation problems.** This closes the compatible outfit-pair check.

All outputs pass `Validate FO4 Skin`, normalized-weight checks, save/reload, and exact
Undo/Redo. Generated results and logs live under `tests/rigging/out/` and are not fixtures.
Additional body candidates were deliberately rejected before mutation: Vault111 suit versus
`FemaleBody` and `FemaleBody` versus `FemaleGhoulBody` were `Poor` geometry/rest-pose matches;
`OldMaleBody` versus `MaleBody` was geometrically `Close` but had an incompatible `Chest_skin`
rest pose. These guard results are expected safety behavior, not transfer failures.

## 8. Phasing

**Progress:** Phases 1–2 are done and verified (§7). Phase 3A, the complete transfer for
already-skinned FO4 targets, and the primary Phase 4 workspace flow are implemented and
native-window tested. RemapData generation from the target's pre-transfer standard skin is
also implemented. The original broader plan remains below so deferred backends and UX work
do not disappear from scope.

1. **Research + Python prototype** — closest-point-on-triangle transfer; run both
   validation tests; lock the algorithm and the inverse-bind convention. ✅ DONE (§7.1)
2. **C++ geometry core** — port the pure transfer function; unit-testable, no NIF types.
   ✅ DONE + verified (§7.3)
3. **FO4 write path** — ✅ **Phase 3A:** existing-skin vertex-buffer write, donor-node
   creation/binding, transforms, bounds, validation, and atomic rollback. **Phase 3B:**
   constructing skin blocks/descriptor/partition data for an entirely unskinned target.
4. **UI** — ✅ cross-file donor/shape picker, Preview, preflight/report dialogs, progress,
   Rigging workspace, donor viewport overlay, and selected-bone weight heatmap.
   Mapping-mode/max-influence controls and manual brush painting remain deferred.
5. **Classic `NiSkinInstance` path** — second write backend behind the same core.
6. **RemapData follow-on** — ✅ byte-exact `CustomizationRemapData` action and atomic-flow
   integration. `CustomizationRemapNewBonesData` remains deferred (see open question below).

### 8.1 Release-candidate exit criteria

- [x] Release build compiles and links.
- [x] Direct four-step and atomic paths produce valid normalized FO4 weights.
- [x] Atomic Undo/Redo and save/reload preserve the expected blocks, bones, and remap bytes.
- [x] Cancellation, confirmation rejection, poor-geometry rejection, and injected inner-step
  failure are mutation-free; rollback is byte-exact.
- [x] Non-finite geometry and invalid triangle references are guarded before mapping;
  versioned fixtures are verified byte-identical after native GUI execution.
- [x] Real Rigging workspace selection and primary-button flow pass in a native window.
- [x] Run same-surface and genuinely different-surface vanilla head/hairline pairs and
  compare generated weights against Bethesda's vertex-compatible face-bone references.
- [x] Run at least one non-head standard-skinned mesh through native production QA.
- [x] Run a compatible standard-skinned headgear/outfit pair through native production QA.
- [ ] Verify saved output in Fallout 4, including FaceGen morphing/customization behavior.
- [x] Complete final source review, package the Release build, and archive test evidence.

## 9. UI/UX

The feature lives in a **new "Rigging" workspace**, alongside Default / Animation /
Materials / Collision. The workspace hosts a **Rigging manager dock** (donor/target
pickers, transfer options, run/report) and is the home for future rigging tools
(weight painting/mirroring, bone bounds, RemapData generation).

### 9.1 Workspace integration (grounded in current code)

The workspace selector is built in `src/nifskope_ui.cpp` (~L1719–1818). Adding "Rigging"
touches five spots, mirroring how Collision is wired:

1. **Manager dock factory** — new `QDockWidget * tlCreateRiggingManagerDock( NifModel *,
   QMainWindow *, GLView * )` in a new `src/spells/riggingtools.cpp` (pattern:
   `tlCreateCollisionManagerDock` @ `collisiontools.cpp:2356`,
   `tlCreateMatTexManagerDock` @ `meshtools.cpp:887`). Add the `.cpp` to `NifSkope.pro`.
2. **Instantiate** near L1725: `extern` decl + `QDockWidget * dRiggingMgr =
   tlCreateRiggingManagerDock( nif, this, ogl );` + `toggleViewAction()->setText(tr("Rigging Manager"))`.
3. **Manager set** — add `dRiggingMgr` to `workspaceManagers` (L1732, sets
   `workspaceRole="manager"` for mutual-exclusion) **and** to the `managers` list (L1792).
4. **`workspaceNames`** (L1784) — append `tr("Rigging")`.
5. **`activateWorkspace`** (L1793–1811) — the clamp is hardcoded `std::clamp(workspace,0,3)`
   and the target is a hardcoded ternary chain for 1/2/3. Extend clamp to `0,4`, add the
   `workspace==4 → managers.at(3)` branch, and pick a dock area (Rigging is a right-side
   inspector → `Qt::RightDockWidgetArea`). Persistence via `QSettings "UI/Workspace"`
   works unchanged.

Note the index coupling: `managers` order must match the `workspaceNames`/branch indices
(Animation=1→Timeline, Materials=2→Mat, Collision=3→Collision, **Rigging=4→Rigging**).
Consider refactoring the ternary into a lookup while here, but a minimal edit is fine for v1.

### 9.2 Rigging manager dock contents (v1)

- **Donor** picker (shape in this file; later: second file) and **Target** shape (or use
  current selection).
- Mapping mode (Nearest Vertex / Nearest Face Interpolated), max influences (1–4),
  "create missing bone nodes" and "prune unused donor bones" toggles.
- **Transfer** button → runs, then shows a summary (bones added/dropped, vertices
  transferred, mean influence count). Model the dialog/report style on
  `SkinPartitionDialog` in `skeleton.cpp`.
- Also reachable as a spell: right-click target shape → *Rigging ▸ Transfer Bones &
  Weights from…* (page "Rigging").
- Later additions to this workspace: *Generate CustomizationRemapData* (§1 encoder),
  skin-partition/bounds helpers, weight mirror.

### 9.3 Donor viewport overlay (v1 implemented)
The donor lives in a background `NifModel` (§6.6); GLView renders the *active* model's
`Scene`. Show the donor as a read-only overlay in the same viewport so the user can see
what they're transferring from and verify alignment.

- **v1 — ghost triangle-soup overlay. ✅** Extract donor triangles once on load, transform
  into the **target's common space** (§6.6, same transform as transfer), draw via the
  existing overlay path (`scene->drawTriangles(...)`, cf. collision preview
  `glview.cpp:~695–745`): flat, tinted, semi-transparent, non-pickable. Selection stays on
  the target — `GLView::raycastScene(pos, excludeBlock, onlyShapes)` already filters shapes.
  `Scene::update(model, …)` (`glview.cpp:387`) proves Scene takes the model as a param, so a
  donor scene/soup is not construction-bound to the active document.
- **v2 — second full `Scene`** (`new Scene; update(donorModel)`) with donor materials remains
  deferred. The target's live selected-bone colours no longer require it: those are supplied
  through a separate ephemeral per-corner heatmap buffer.
- **Key property:** the overlay uses the *same* common-space transform as the transfer, so
  **it doubles as the alignment preview** — if the ghost lines up with the outfit, closest-
  point transfer will match; if it floats/offsets, that's a skeleton/space mismatch caught
  before committing. Identity transform for same-skeleton assets authored at origin.
- Implemented controls: Load/Clear donor, filled surface, wireframe, and opacity. Hiding the
  Rigging dock removes the GL overlay without discarding its cache; showing restores it.
  Target changes, structural edits, atomic transfer, and Undo/Redo clear stale geometry.

### 9.4 Selected-bone target heatmap (implemented)

Selecting a bone in the Rigging tree and enabling **Show selected-bone weights** overlays
the target with Blender-style weight colours: blue 0, cyan 0.25, yellow 0.75, red 1. The
manager reports the number of affected vertices and maximum weight. Geometry and per-corner
colours live only in `GLView`; enabling, changing, hiding, or clearing the heatmap creates no
NIF write and no Undo entry. This is the visual foundation for the manual brush editor.

### 9.5 Manual target weight painting (implemented)

The Rigging Manager now turns that heatmap into a direct FO4 weight editor. Select a bone,
choose **Add**, **Subtract**, **Replace**, or topology-aware **Smooth**, set Weight,
Strength, and Radius, then click **Start Painting**. The viewport shows a cyan brush;
LMB drags collect target vertices with smooth radial falloff, the wheel resizes it, and
RMB/Esc exits. A target-only raycast prevents painting through empty screen space, while
the viewport X-ray option controls backface inclusion.

The viewport emits model-free stroke samples. The manager applies the collected samples
on release so one mouse drag becomes exactly one `NifSnapshotCommand`. Writes are
deterministic, capped at four `uint8` bone slots, renormalized, and packed through
`NifModel`; Smooth reads original one-ring neighbour weights so results do not depend on
vertex iteration order. Bone bounds are recalculated after every changed stroke. If a
standard-only skin already has `CustomizationRemapData`, the same Undo snapshot refreshes
its bytes from the final packed weights; face-sculpt skins deliberately retain the
pre-transfer standard-skeleton blob. Undo/Redo clears stale overlays and reacquires the
same target block in the manager.

The viewport mode selector exposes the workflow directly in this order: **Object Mode**,
**Edit Mode**, **Vertex Paint**, **Weight Paint**, and **Segment Paint**. Weight Paint is
connected to the Rigging Manager and is mutually exclusive with Edit Mode. Vertex Paint
and Segment Paint are intentionally present but disabled until those editors are built.

## 10. Open questions / risks

- **Inverse-bind convention (§6.3):** exact transform composition. Mitigate with the
  self-transfer test.
- **Multi-file donor:** RESOLVED — cross-file is the primary workflow; reuse the
  `importDonorCollision` pattern (standalone `NifModel::loadFromFile` + branch splice with
  block remap). Core API takes `(NifModel *, blockIndex)` pairs. See §6.6.
- **`CustomizationRemapNewBonesData` (RemapData follow-on):** per-bone struct only
  *partially* reverse-engineered — name + tail transform matrix confirmed, but the record
  contains **leaked 64-bit runtime pointers** (raw in-memory dump; e.g. stray `T69hm`
  bytes). To *write* it: enumerate referenced-but-absent bones, emit name + transform
  (available from the donor/target nodes), zero the pointer scratch, and **verify in-game**.
  Not needed for transfer itself; only for the RemapData feature.
- **Vertex-buffer rewrite for FO4:** changing `Skinned`/stride means rebuilding the packed
  vertex array — ensure other attributes (pos/uv/normal/tangent offsets) are preserved.
  Cross-check against `meshtools.cpp` vertex handling.
- **Value vs. Outfit Studio:** OS already does weight transfer well. Lean into the niche —
  in-place, no round-trip, and it unlocks FO4 faceBones/RemapData authoring in the same
  tool — rather than trying to match OS on transfer quality in v1.

## 11. Reference material

- Proven FO4 faceBones/RemapData findings + validated encoder:
  `reference_fo4_customization_remap.md` (auto-memory) and scratchpad
  `nifparse.py` / `compare.py` / `prove.py` / `encode_test.py`.
- FO4 NIF binary layout: `reference_fo4_nif_format.md`.
- Existing skin spells & partition logic: `src/spells/skeleton.cpp`.
- Mesh/vertex handling: `src/spells/meshtools.cpp`, `mesh.cpp`.
