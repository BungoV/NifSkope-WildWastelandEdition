# NifSkope — WW Edition: To Be Implemented

**This is THE backlog.** One file, everything that is left. Consolidated
2026-07-21 from what used to be seven scattered plan documents, each of which
independently claimed to know what was open — and several of which were wrong.

**Last full verification against the code: 2026-07-27.**

The 07-27 sweep found **five** stale claims and **six** items that were missing
from this file entirely. Same failure mode as the 07-21 sweep: two of the five
were features NifSkope has *always* had (stock behaviour filed as new work),
one was reversed by a later user decision and never un-filed, and one had grown
by 14 units while the estimate stayed frozen. Details in the two lists at the
bottom.

---

## How to use this file

1. **Verify against the code before building anything listed here.** These docs
   have been wrong in *both* directions. On 2026-07-21 a single review found
   **nine** stale claims: six Blender-batch items marked open that were fully
   implemented (Mirror editing, Checker deselect, Repeat Last, F9, Rip, Split),
   rest-pose display marked open when it had shipped, a "model landmine" that
   was an untested conjecture and proved false, and `TIMELINE_PLAN.md`'s 43
   unticked checkboxes which were **all** implemented.
2. **The code is the only complete inventory.** Docs are also wrong by
   *omission*: the Skeleton Manager and Pose Manager workspaces — the two
   largest un-started features in the project — existed only as disabled UI
   entries and a line in `CURRENT_STATUS.md`, and were missing from the backlog
   entirely. When asking "what's left", grep for `setEnabled( false )`
   placeholders and greyed menu items as well as reading markdown.
3. **Fast checks**: the `viewport.*` shortcut registry at the top of
   `glview.cpp` settles any viewport feature in seconds. For everything else,
   grep the feature's identifiers.
4. **Check whether the "missing" feature is stock NifSkope.** Two 07-27 stale
   claims (string-index display, nif.xml tooltips) were upstream behaviour that
   had simply never been looked for. Grep `nifmodel.cpp`'s `data()` role
   switches before filing display work.
5. **Grep the code, not just one file.** The 07-27 sweep initially "confirmed"
   the NifItem slab pool as unbuilt because the grep covered `nifmodel.cpp` and
   `nifitem.h` but not `nifitem.cpp`. It had shipped.
6. **A file on disk is not necessarily compiled.** `src/collisiontools.cpp` is a
   committed orphan that no longer builds (see §14). Diff the `.pro`'s exact
   paths against `find src -name '*.cpp'` — matching on basename alone hides it.

**Related files** — none of them carry backlog any more:

| file | role |
|---|---|
| `WW_CHANGES.md` | change log / history. Update it with every batch. |
| `CURRENT_STATUS.md` | session handoff: build rules, working agreement, gotchas |
| `BLOCK_DETAILS_OVERHAUL_PLAN.md` | design detail + binding visual rules |
| `MODELING_TOOLS_PLAN.md`, `UV_EDITOR_PLAN.md` | design detail |
| `TIMELINE_PLAN.md` | historical v1/v2 checklist (all ticked) |
| `PERFORMANCE_PLAN.md` | tiered analysis + measurements, incl. deferral rationale |
| `BONE_WEIGHT_TRANSFER_PLAN.md` | validated skin math, transfer design |
| `COLLISION_MANAGER_HANDOFF.md` | validated packfile offsets, test assets, gotchas |
| `BLOCK_LIST_DETAILS_OVERHAUL.md` | superseded v1 design notes (list side only) |

---

## Summary — everything open, by size

| # | area | size | state |
|---|---|---|---|
| 0 | **Renderer: PBR draws empty frames** | medium | **parked behind one constant** — hottest open bug |
| 1 | **Skeleton Manager** workspace | large | **phase 1 SHIPPED 07-27j** (read-only); phases 2-4 open |
| 2 | **Pose Manager** workspace | large | **SHIPPED 07-22d..l**; prop staging + 4 refinements deferred |
| 3 | Performance 15c + 16 (flattened storage + off-thread parse) | large | deferred as one joint project, prereqs listed |
| 4 | Collision Manager P4 | medium | partly **blocked** (compound/instance) |
| 5 | Block Details: remaining typed editors | small | **2 of 4 were already done**; 2 remain, ready to build |
| 6 | Block Details: array table (P4) | medium | not started |
| 7 | Block Details: whole-file search, recent values/revert, hex viewer (P5) | medium | not started |
| 8 | Block List: summary column, status badges | small | not started |
| 9 | Rigging leftovers | medium | partly absorbed by #1 |
| 10 | UV editor: cross-mesh operators | medium | active-mesh-only is current design |
| 11 | Animation: 3 remaining gaps | small | the other 43 items are done |
| 12 | Rendering: spec/gloss **SHIPPED**, PBR **parked**, SSS future | large | see §0 and §12 |
| 13 | CLI follow-ups (see §13) | small–medium | base CLI shipped 07-21e; harness port is 22 files, not 8 |
| 14 | Repo hygiene: orphaned source, 54 uncommitted files | small | new 07-27 |
| 15 | Viewport: Separate By Material / By Loose Parts | small | new 07-27 — disabled placeholders |

---

## 0. Renderer — PBR draws empty frames (HOTTEST)

Everything for PBR shipped over 07-27b..e and is then **switched off** by
`static constexpr bool pbrmFeatureEnabled = false;` at `glproperty.cpp:996`.
The gate is in code, not just the UI, so a stale QSettings value cannot switch
it on behind a greyed-out menu — verified by leaving `Settings/Render/PBRM Mode`
set to PBR in the registry and confirming a full frame still rendered.

`BSLightingShaderProperty` shapes draw **nothing** in PBR mode. RenderDoc
confirmed program selection, texture binding and draw submission all work; blend
is disabled with a full write mask; effect-shader shapes still render; the grid
is not occluded. **Eliminated:** per-draw GL state, cubemap completeness,
uniform-block binding, unconditional base-alpha-as-opacity.

**Next diagnostic — now actually possible (07-27q/r).** Every prior finding here
came from captures that contained only Qt's composite blit, so "RenderDoc
confirmed X" in the paragraph above is worth re-checking rather than trusting.
The in-app capture path lands real frames now:

```
WW_RDC_FRAMES=4 WW_RDC_OUT=%TEMP%\pbr\cap NifSkope.exe <file.nif>
rdc open %TEMP%\pbr\cap_paint002_capture.rdc && rdc draws && rdc cbuffer <eid> --stage vs
```

Start by reading the **uniform block contents at the failing draw**, because that
is exactly what the 07-17 line defect turned out to be: `viewportDimensions`
arriving as zeros while the CPU-side struct read correct. A probe on the app side
cannot see that. `rdc cbuffer`, `rdc rt <eid>` (colour target after a specific
draw) and `rdc snapshot <eid>` (full state + shader source) are the tools that
settled it.

Note `rdc debug vertex` shows `0xCCCCCCCC` inputs on the *working* legacy draw
too, so that output is an artifact and cannot be read as evidence.

To resume: flip the constant and re-enable the two greyed menu entries. Full
findings in `WW_CHANGES.md 2026-07-27e`.

### Refraction guard — FIXED 07-27g, one bounded follow-up left

The "fixture guards nothing" symptom turned out to be a **real renderer bug**:
`hasRefraction` was only ever assigned in `updateParams`'s no-material branch, so
refraction could not engage on any FO4 mesh backed by a BGSM/BGEM — nearly all of
them — and had been dead since the feature shipped on 07-06. Fixed, and the
fixture rebuilt as an A/B pair against `glass_visor` (same mesh, flag off). Full
account, including why the material is OR'd with the NIF flag rather than
replacing it (0 of 6899 vanilla FO4 materials set `bRefraction`), in
`RENDERER_MATCH_PLAN.md §0`.

**Still open, small:** the fixture proves refraction *engages*, not that it
*distorts* — a refracting shape over a featureless background is invisible rather
than warped, so "refracting" and "shape culled" look identical. Needs geometry
placed behind the refracting shape. A plain `merge` is not enough (the pieces
land at unrelated scales and never overlap); it needs deliberate transform
placement.

---

## 1. Skeleton Manager workspace — PHASE 1 SHIPPED 2026-07-27j

**Shipped (phase 1, read-only):** `src/skeletontools.{h,cpp}` +
`Workspaces ▸ Skeleton`. Hierarchy tree, deforming/unused/not-a-bone
classification, per-bone shapes/verts/weight, filters, search, selection sync,
rest-pose toggle, and the two free validation findings (dangling skin bones,
duplicate names). CLI `skeleton <file> [--validate]`, exit 1 on findings.
Verified by `WW_SKELETON_TEST` plus an independent weight cross-check (summed
weight 1688.98 vs 1689 vertices). Writes nothing.

The workspace is appended LAST in the managers list on purpose: the persisted
`UI/Workspace` index is positional, so inserting earlier would reopen a
different workspace for every existing user.

**Still open here:**
- **Phase 2 — validation + prune.** The remaining checks, then
  `Prune unused bones`, which must remap every vertex's bone indices (the Join
  bug in reverse; `WW_CHANGES 2026-07-19b` applies exactly). Also add
  `--prune-unused` to the CLI. Note the *Unused* filter still has no positive
  test case: a 40-mesh sweep of vanilla FO4 armour found zero unused bones, so
  phase 2 needs a constructed fixture.
- **Phase 3 — bone transform + rebind.** The genuinely risky part (§A.3):
  every transform edit must recompute the affected inverse-bind transforms in
  the same undo step, or the mesh tears. Ships only behind the Create Skin
  gauntlet's must-not-move check.
- **Phase 4 — persistent skeleton reference slot**, then octahedral viewport
  bone display.

**Design: `SKELETON_AND_POSE_PLAN.md`** (2026-07-21) — Blender-grounded
(Armature Edit Mode), with the rebind hazard, phasing and CLI surface worked out.

Tooltip contract: *"skeleton hierarchy, rest-pose, bone transform, and
validation workspace."*

**This absorbs several items previously filed as loose rigging tasks** — an
independent persistent skeleton reference is this workspace's prerequisite, not
a separate feature, and zero-weight bone pruning belongs here too.

Building blocks that already exist:
- `Scene::restPoseBlock` + `Node::restWorldTrans()` — rest-pose display, shipped
  (`glnode.cpp:337/353`, five writers in `glview.cpp`).
- Rigging Manager: bone list, donor/target bone compare, transfer, weight paint.
- `BONE_WEIGHT_TRANSFER_PLAN.md` — validated inverse-bind and transfer math.
- FO4 `CustomizationRemapData` decode (see the reference memory / plan doc).

## 2. Pose Manager workspace — SHIPPED 2026-07-22d..l (1 feature + 4 refinements open)

Built: posing engine already worked (live skinning); pose library + blend in
`AnimSetup` (`savePose`/`applyPose`/`readPose`); the dock (`src/posetools.cpp`,
Workspaces ▸ Pose) with a bone list that drives selection, save/apply/delete and
a blend slider. Verified by `WW_POSEDOCK_TEST` + a screenshot. CLI `pose` mirrors
it. Load-screen composition resolved to `merge` (07-22a).

Much more shipped after 07-22d than this section used to admit: 07-22e..l added
bone-by-bone posing, viewport skeleton + click-to-pose, Outfit Studio pose XML,
load-skeleton-from-archive, a folder-based pose library, and the 07-22i batch
(weights overlay, hover name, multi-select, pin, non-destructive posing,
proportional editing, mirror axis).

**Still open — one feature:**
- **Prop staging** (attach an external NIF under a bone) — 0 hits in
  `posetools.cpp`. Blocked on a question below.

**Deferred refinements** (all from 07-22i, all bounded, none blocking):
- **Pose-mode bone proportional editing** — the edit-mode version shipped; the
  object-gizmo path is parent-space and more intricate. The falloff infra is
  already shared, so this is a contained follow-up.
- **Topology-based mirror pairing** — position pairing works for symmetric
  meshes, which is every real case so far.
- **Weight overlay uses bind positions** (shape world transform × vertex) —
  exact for an unposed mesh, lags the deformed surface once bones are posed.
- **Non-destructive posing is a mode-boundary guarantee**, not a display
  overlay: bone nodes *are* touched while in Pose Mode and restored on exit. A
  never-touch-the-nodes version is a larger rearchitecture.

**Mirror / paste-flipped poses is DONE** — `PoseMirrorButton` +
`ogl->poseMirrorBone()` (`posetools.cpp:192`, `:491`). It was filed as open here
while `poseMirrorBone` already existed; 07-22i says so explicitly.

**Design: `SKELETON_AND_POSE_PLAN.md`** (2026-07-21) — Blender-grounded (Pose
Mode + Pose Library). Key decision recorded there: **a pose IS a
`NiControllerSequence` carrying one key per bone at t=0** — the NIF equivalent
of a Blender Action — so the library reuses the sequence machinery `anim-setup`
and the timeline already provide instead of inventing a pose format.

Tooltip contract: *"character posing, prop staging, reusable pose, and
load-screen composition workspace."*

(The stale line "Nothing built" lived here until 07-27, four shipping days after
the dock landed. The plan's build order — pose library B.1 before bone
transforms A.3 — was followed and is now history rather than guidance.)

**One question still blocks prop staging** (plan §B.6): does it edit the saved
file, or is it preview-only? The other two questions in that section are answered
and should not be re-asked — load-screen composition resolved to `merge` (07-22a)
and the pose library is a folder of files, i.e. cross-file (07-22j).

## 3. Performance — 15c + 16, deferred as ONE joint project

Authoritative analysis, measurements and rationale: `PERFORMANCE_PLAN.md`.
Tier 1, Tier 2 and Tier 3 batch 1 (NifItem slab pool) have shipped.

- **15c — flattened packed storage** for `BSVertexData` rows, and
- **16 — off-thread raw-buffer parse.**

Deferred together on evidence: measurement shows item *construction* dominates
load, and an off-thread parse only pays once flattened storage makes the raw
buffer the model's storage. Prereqs recorded in the plan: full gauntlet per
step, a `QPersistentModelIndex`-over-virtual-rows design, and the `#ARG#`
condition-cache contract either preserved exactly or replaced wholesale.

**Deliberately not doing** (rationale in the plan, do not re-open casually):
- T2.13 draw sorting — needs a draw-loop restructure plus per-shape uniform
  caching (Tier-3 scale), and a bare `glUseProgram` current-check is exactly the
  cache-desync landmine behind the old startup-grid bug.
- T2.14 particle frustum culling — the sim must keep running for correct resume,
  draw-only culling has minimal payoff, and a wrong-space plane test silently
  blanks VFX with no headless test to catch it.

## 4. Collision Manager — P4

P1–P3 have landed; the multi-section compressed-mesh encoder shipped 07-18.
Precisely (re-checked 07-27, the old wording here was ambiguous): a section still
holds ≤255 verts/tris — that is the format — and the encoder now partitions a
mesh into up to **4096** such sections by spatial slab, with per-section domains,
and single-section output stays byte-identical to the validated writer.

- **Compound / instance encoding — BLOCKED.** Needs reference pairs to validate
  against. `COLLISION_MANAGER_HANDOFF.md` is explicit: **do not build blind.**
- Per-triangle face-material painting — open (0 hits in `collisiontools.cpp`).
- `hknpBSMaterialProperties` beyond the single-material table — open.
- The shipped multi-section encoder still needs an **in-game walk test**.

### 4a. Ragdoll (bone collision) — DECODE FIRST, then the encoder — NEW 07-27o

Viewing and editing per-bone collision **works** as of 07-27o: 32 of the 35
vanilla FO4 actor ragdolls attribute their capsules per bone, Decompile yields one
`bhkRigidBody` + `bhkCapsuleShape` per bone, and the Collision Manager lists them
with a Bone column. What is missing is the way back.

**This is not an encoder task.** A `bhkRagdollSystem` packfile carries five classes
NifSkope does not decode, so there is nothing to round-trip an encoder against —
`decode(encode(decode(x))) == decode(x)` is the only usable test. Do these in
order; each is independently verifiable against a vanilla file:

1. ~~**`hknpRagdollData`** — the packfile root.~~ **DONE 07-27p.** It derives from
   `hknpPhysicsSystemData`, so matching the class name was the whole fix; all 35
   vanilla ragdolls now decode their bodies. Its remaining unread arrays are
   `+0x50` (38 entries on 39 bones — the constraint bindings, see 3) and `+0x80`
   (39 entries, unidentified).
2. ~~**Per-body mass / inertia / damping.**~~ **DONE 07-27p.** Now on
   `HknpBodyPhys`. Strides: `dyn_motion` `0x40`, `dyn_inertia` `0x70`, both measured
   twice (brahmin ragdoll and a 15-body Halloween banner). Indexed by the **motion
   index** at `cinfo +0x0c`, not the body index — that distinction is load-bearing,
   see the 07-27p entry. The arrays carry their own counts at `+0x28` / `+0x38`.
3. ~~**`hknpSphereShape` synthesis.**~~ **DONE 07-27ad.** The plane/face/index
   relArrays at `+0x40`/`+0x44`/`+0x48` only exist on a polytope; on a sphere the
   vertex payload starts at `+0x30 + 0x10`, which *is* `+0x40`, so those fields
   were vertex bytes read as counts (Deathclaw's `+0x44` read 32544 faces) and the
   shared sanity check returned before the sphere branch could run. Vertices are
   read first now, polytope arrays only after the primitives return. **All 35
   vanilla ragdolls decode with zero undecoded shape classes**, including
   Deathclaw 28/28, `Robot/SkeletonRef` 48/48 and `skeletonSentryBodyPart` 24/24.
4. ~~**The joints**~~ **DONE — graph 07-27ae, frames 07-27af, limits 07-28a.**
   The **binding graph** is decoded and on
   `HknpSystem::constraints`: root `+0x50`, 0x18 an entry, constraint pointer at
   `+0x00` (global fixup), child body `+0x08`, parent body `+0x0c`, 8 bytes pad.
   Verified on the brahmin against the skeleton itself — all 38 name a bone and its
   nearest *embodied* ancestor (bodiless intermediates like `LNeckHub` skipped) —
   and all 35 vanilla ragdolls decode, 757 joints of which 237 are hinges.

   **Joint FRAMES also done 07-27af**: `hkTransform` at constraint `+0x30`
   (parent side) and `+0x70` (child side), three basis rows then the pivot, on
   `HknpConstraint::rotA/rotB/pivotA/pivotB`. 755 of 757 joints decode frames; the
   2 that do not are `hknpBreakableConstraintData`, a wrapper with its own layout
   that is deliberately skipped rather than misread.

   **The angular LIMITS also done 07-28a**, via a validated atom type→size table
   (`decodeConstraintAtoms`; sizes and evidence in WW_CHANGES 07-28a). All 757
   constraint objects walk to their exact end, each class with exactly one atom
   sequence. `HknpConstraint` now carries `twist`/`cone`/`plane`/`hinge`,
   `friction`, `motorEnabled` and `breakable`; **757 of 757 joints decode limits**,
   the last two by following the `hknpBreakableConstraintData` wrapper (the
   Vertibird's doors) to the `hkp*` data inside. The predicted values all came
   back exactly: brahmin tail-base twist ±5°, cone 40°, knee hinge -15°..40°.

   Two vanilla hinges store min/max transposed (`+5.00/-5.00`, `+0.10/-0.10`);
   that is the file's own data and is reported, not silently corrected.
5. ~~**`hkaSkeleton`**~~ **DONE 07-28b.** A root object beside `hknpRagdollData`,
   one per ragdoll: `hkArray`s at `+0x18` parentIndices (`hkInt16`), `+0x28` bones
   (16 bytes: a name pointer, always null here, then `lockTranslation`) and `+0x38`
   referencePose (`hkQsTransform`, 48 bytes). The four arrays after those are empty
   in every vanilla ragdoll. On `HknpSystem::bones`; `collision` prints the rest pose.

   **Bone index == body index**: all 757 constraint bindings agree with
   `parentIndices`, and bones == joints + 1 on all 35. Rotations are unit
   quaternions and scales exactly `(1,1,1)` on all 792 bones.

   **Bone names are NOT in the packfile** — every name pointer is null and unfixed.
   They must come from the NIF node list, as the body→node mapping already does.
6. Only then the encoder. The array machinery in `hknpEncodeCompressedMesh` is
   directly reusable — the ragdoll root uses the same `hkArray` layout at the same
   offsets. Validate by round-trip on all 35 ragdolls.

   **`hknpRagdollData`'s full root layout** (measured 07-28c; every member, not
   just the ones the decoder reads). All arrays are `hkArray`: payload pointer
   patched by a local fixup, count at +8, capacity|0x80000000 at +12.

   | offset | member | stride | count |
   |---|---|---|---|
   | `+0x00` | (empty array) | — | 0 |
   | `+0x10` | body_props | `0x50` | bodies |
   | `+0x20` | dyn_motion | `0x40` | bodies |
   | `+0x30` | dyn_inertia | `0x70` | bodies |
   | `+0x40` | bodyCinfos | `0x60` | bodies |
   | `+0x50` | constraint bindings | `0x18` | bones − 1 |
   | `+0x60` | shape entries | `8` | bodies |
   | `+0x78` | **pointer to `hkaSkeleton`** (global fixup) | — | — |
   | `+0x80` | bone → body index | `4` | **bones** |

   Two traps in there. The `+0x80` array is counted in **bones, not bodies**, and
   the two differ — `Robot/SkeletonRef` has 48 collision bodies but 11 ragdoll
   bones, `skeletonSentryBodyPart` 24 and 9. Reading it at the body count runs off
   the end into garbage. Its contents are the identity map in **all 35** vanilla
   ragdolls, so an encoder emits identity of length `bones`. Shape entries at
   `+0x60` are 8 bytes of nothing but a global fixup to the shape.

   **Shape encoding is a fixed template** (measured 07-28c across all 801 vanilla
   capsules and spheres). A `hknpCapsuleShape` is not a primitive: it is a full
   convex polytope *plus* the two exact end points, and every vanilla one has the
   same topology — **8 verts, 8 planes, 6 faces, 24 indices, 432 bytes**, with the
   index table and the face table **byte-identical across all 778**:

   ```
   faces   00 00 04 04  04 00 04 04  08 00 04 04  0c 00 04 04  10 00 04 04  14 00 04 04
   indices 07 06 02 03  03 02 00 01  07 05 04 06  01 00 04 05  01 05 07 03  02 06 04 00
   ```

   So both tables are constants an encoder embeds verbatim. What varies is only
   the geometry, and it is a **square-section box around the capsule axis** — all
   8 hull verts are equidistant from the A–B segment in all 778, so the corners
   are `±margin` perpendicular to the axis at each end. `convexRadius` (`+0x14`)
   is the capsule radius minus that margin; margin runs 0.000125 to 0.0186 with
   70 distinct values, so it is per-capsule and must be carried, not defaulted.

   Layout: `+0x14` convexRadius, `+0x18` a constant `0.08693` (shared with
   spheres), `+0x30` vertex relArray, `+0x40`/`+0x44`/`+0x48` plane / face / index
   relArrays, `+0x50` and `+0x60` the exact end points (`hkVector4`, w = 1), then
   verts from `+0x70`. Each hull vert's w encodes its own index in the low
   mantissa bits of 0.5.

   `hknpSphereShape` is far simpler: **128 bytes, 4 identical verts** (SIMD
   padding) from `+0x40`, radius in `convexRadius`, no plane/face/index arrays at
   all — which is exactly why reading those three fields on a sphere returns
   vertex bytes, the bug fixed in 07-27ad.

   **Class name hashes** for the encoder's `classNames` table, read out of the
   vanilla files and identical across all 35: `hkaSkeleton` `0xfec1cedb`,
   `hknpRagdollData` `0xdc8f20ab`, `hknpCapsuleShape` `0x60a75f4c`,
   `hknpSphereShape` `0x741e9012`, `hknpConvexPolytopeShape` `0x3ce9b3e3`,
   `hkpRagdollConstraintData` `0xb77d2036`, `hkpLimitedHingeConstraintData`
   `0x51ea603a`, `hkpPositionConstraintMotor` `0x143dd400`,
   `hknpShapeMassProperties` `0xe9191728`, `hknpBreakableConstraintData`
   `0xc40485c7`, `hknpDynamicCompoundShape` `0x4620d11c` / `Data` `0xf33dc3cc`.
   `hkRefCountedProperties` `0x7c574867` already matches what `hknpencode.cpp`
   emits today.

   **The decode is 99.5% of packfile bytes by class** (audited 07-28b). What is
   left, and what the encoder must do about it:

   - `hkpPositionConstraintMotor` — 35 objects, **1 distinct byte pattern** across
     every vanilla ragdoll, shared by all of a ragdoll's joints (98 pointers to the
     one object on the brahmin). A Bethesda preset: type 1 (POSITION), minForce
     -1e6, maxForce 100, tau 0.8, damping 1.0, recovery 5.0 / 0.2. **Emit verbatim.**
   - `hkRefCountedProperties` — 53 objects, **1 distinct byte pattern**. Also emit
     verbatim; there is nothing per-shape in it to author.
   - `hknpShapeMassProperties` — 53 objects but **43 distinct**, so this one is real
     per-shape data (`hkCompressedMassProperties`: centre of mass, inertia and
     major-axis space as `hkHalf` vectors, then mass and volume). It must be decoded
     or recomputed, not copied.

   All three appear exactly 53 times, matching `hknpConvexPolytopeShape` one for
   one — capsule and sphere mass properties are analytic, so only polytopes carry
   precomputed ones. A ragdoll of capsules alone needs none of this.

   Watch the constraint frames: `HknpConstraint::rotA/pivotA` is the **child**
   side and `rotB/pivotB` the **parent** side (measured, see WW_CHANGES 07-28b) —
   they were documented the wrong way round until then, and swapping them on write
   would produce a ragdoll that looks decodable and behaves wrongly.

Working tools for this: `NifSkope -no-gui collision <file>` prints the bindings and
the decode result; `--extract -b N -o F.bin` writes the raw packfile for
`tools/hkparse.py`, which already lists class names, hashes and the object table.

Until 5 lands, Decompile and Compile both warn (default-Cancel) that the trip is
one-way — do not remove those guards.

Full feature spec is preserved in the appendix at the bottom of this file;
validated packfile offsets and test assets live in
`COLLISION_MANAGER_HANDOFF.md`.

### 4b. Cloth collision — DECODED 07-28e, not surfaced — NEW

Reverse-engineered but **no code written**: nothing in the app reads this yet. It
would slot into the existing `collision` CLI, since `--extract` already pulls the
blob out verbatim.

`BSClothExtraData` holds a Havok **Cloth (`hcl`)** packfile — the same container
and version as collision (`hk_2014.1.0-r1`) but with a **0x50 file header where
`bhkPhysicsSystem` writes 0x40**, so section headers are not at a fixed offset.
Find them by searching for `__classnames__` rather than assuming, or the parse
desyncs. 24 classes; the collision-relevant ones are `hclCollidable`,
`hclCapsuleShape`, `hclTaperedCapsuleShape` and `hkaSkeleton`.

**`hclCollidable`** (176 bytes) is an `hkTransform` at `+0x20` (three basis rows,
translation at `+0x50`), a float `0.01` at `+0x84`, a shape pointer at `+0x88`
(global fixup) and an inline name following the convention
`Collidable_<Bone>NNN`. Sets are small — the PrewarDress collides against five:
both thighs, both calves and the spine, with L/R positions mirrored to `±6.62`.

**`hclCapsuleShape`** (96 bytes): A `+0x20`, B `+0x30`, unit axis `+0x40`,
**radius** `+0x50` — radius, *not* length (a torso one is 1.7 long by 9.9 radius).

**`hclTaperedCapsuleShape`** (176 bytes) is a cone frustum with no ragdoll
equivalent. Only four values are authored — A `+0x20`, B `+0x30`, and
`[radiusA, radiusB, length, apexDistance]` at `+0x90`. Everything else is
precomputed SIMD scratch and reproduces exactly from those: cone apex position
`+0x40` = `A - (rA*L/(rB-rA)) * axis`, unit axis `+0x50`, `|B-A|` broadcast
`+0x60`, apex distance broadcast `+0x70`, `-sin(taper)` `+0x80`, and
`[cos, ?, sin, sin^2]` at `+0xa0`.

Checked over 40 cloth meshes / 43 tapered capsules: length, apex distance, apex
position, cos and sin² each reproduce **43 of 43**. One shape's taper sign
disagrees, most likely `rB < rA` (a reversed taper) which the `-sin` reading does
not cover — resolve that before writing an encoder.

**Two traps.** Cloth is in **game units** (the spine collidable sits at z=68.91,
about hip height) while ragdoll collision is in **Havok metres** (0.7455); confuse
them and everything is ~70x wrong. And unlike ragdolls, the cloth `hkaSkeleton`
**keeps its bone names** — all 277 of them — so cloth data is self-describing
where ragdoll data is not.

### 4c. Collision simulation in the viewport — NEW 07-28f, agreed with bungo

The goal, in bungo's words: preview ragdolls and props colliding properly; click a
bone, hold it and drag to move the ragdoll around by that bone; props the same but
without bones; and fling objects at static collision to test it.

**Backend: our own solver, decided 07-28.** Bullet 3.25 is in MSYS2 and was
offered, but `btConeTwistConstraint` is an *approximation* of Havok's twist+cone.
Since 07-28a decoded Havok's exact limits, a purpose-built solver matches in-game
behaviour more closely than Bullet would, and keeps the repo dependency-free in
line with the vendor-under-`lib/` convention.

Method: **XPBD** (extended position-based dynamics, Müller et al., substepped)
rather than sequential impulses. It is markedly more stable for stiff ragdoll
joints — blow-up and jitter are the usual failure modes of the alternative — and
dragging falls out naturally, since moving a body and letting the solver resolve
is exactly what the formulation does. Constraint compliance also maps cleanly onto
the `tau` factor already decoded on every limit.

Phases, each testable before the next:

**STATUS 07-28f: phase 1 DONE — the solver is correct and can be built on.**
`src/physics/ragdollsim.{h,cpp}` and `NifSkope -no-gui simulate <file>`.
**37 of 37** vanilla actor ragdolls settle, none diverge, worst ball-socket
separation across the corpus **0.4 mm**. Joint error converges as h² (brahmin:
8.5e-4 / 6.1e-5 / 6e-6 at 8 / 32 / 96 substeps) and total energy is now
substep-independent, which is the property that was missing. The five faults
found and fixed are written up in WW_CHANGES 07-28f; the ones that change how the
data must be *read* are:

- **Bodies sit on the centre of mass, not the bone origin.** The tensor is about
  the centre of mass and the bone origin is ~0.13 m away, so using the bone origin
  understates the inertia there by m*d² (~27x on the brahmin thigh). The file has
  no centre-of-mass field; the shape centroid is the right substitute and the
  decoded tensor corroborates it.
- **cinfo +0x30 is the body POSITION** — renamed from `com`. It equals the bone
  origin accumulated from `hkaSkeleton`'s reference pose on all 39 brahmin bodies,
  which is also a free cross-check of the skeleton decode.
- **`hkRotation` stores COLUMNS.** Reading the three decoded vectors as rows
  transposes every joint frame; it left 22 of 38 joints violating their own limits
  at rest, with cone angles to 169°. The conjugate fixes it.
- **The plane limit is the angle out of the parent's plane**, not the a0-to-b0
  angle about b2 — that older reading is not a well defined signed angle at all,
  since neither vector is perpendicular to b2.
- **Four Gauss-Seidel sweeps per substep, plus mass splitting.** A ragdoll's
  inverse inertia reaches the hundreds, so one sweep diverges on any body carrying
  several joints. Both numbers are measured, not guessed.

Still true and still worth acting on: `dyn_inertia +0x20` holds **inverse
inertia, not inertia**, so **`HknpBodyPhys::inertia` is misnamed** and
`hknpencode.cpp`'s `dynamicInertia()` writes true inertia into that slot — latent,
invisible so far only because it writes static bodies.

`simulate --selftest` runs eight synthetic rigs whose energy is conserved
analytically; keep it green. It is what made the above findable, by reproducing
the ragdoll blow-up in 6 bodies (`forkh`) instead of 39. Note the trap it exposed:
initial conditions must be a state the rig can actually occupy — a sideways shove
with no angular velocity is not, and the solver's projection of it looks exactly
like a substep-independent leak in the solver itself.

Known-imperfect, deliberately not chased: the light-inertia `fork` rig drifts to
about +10% over 10 s instead of converging to zero. Bounded, no effect on any real
ragdoll, and 16 sweeps do not improve it — most likely the float32 floor in
`v = dx/h` at small h.

1. **Solver core, headless.** `src/physics/`: bodies (mass, diagonal inertia,
   pose, velocities), substepped XPBD integrator, ball-socket joints, and the
   angular limits from `HknpConstraint` (twist / cone / plane / limited hinge).
   Built straight from a decoded `HknpSystem` — no new file parsing. A `simulate`
   CLI command runs it with **no GUI** and reports energy, joint drift and
   penetration, so stability is measured rather than eyeballed. A ragdoll pinned
   at the root must settle, not explode.
2. **Collision.** DONE 07-28g. Closest-point-between-segments covers all three
   capsule/sphere pairings exactly; ground plane; self-collision filtered by
   Havok's filter groups and by a rest-pose overlap test. `--drop`, `--ground`,
   `--no-self`. Corpus: 37/37 settle, 0 diverge, worst penetration 0.4 mm, worst
   joint separation 6.9 mm.
   **07-28i corrected the 07-28g/h diagnosis.** Most of the "hot" machines were
   FALLING THROUGH THE FLOOR, not unstable: collision handled only capsules and
   spheres, every machine is a polytope, and free fall looks like rising energy.
   Compounding it, a body carries several shapes (Liberty Prime: 14 shapes / 12
   bodies) and the build kept only the last. Both fixed. Measure settling by
   SPEED, not energy — energy scales with mass.
   **Open after 07-28i: 4 of 37 genuinely wrong** — eyebot (200 m/s), workshop
   turret (66), Robot/SkeletonRef (24), radstag (12). 27 of 37 are at rest below
   1 m/s and 6 more are gently rocking. Body-body collision is still exact only
   for single capsules/spheres; a compound is a point set with no faces, so
   polytope-vs-polytope self-collision is NOT handled and is the most likely next
   cause. Ground contact is exact for both.
   (superseded) **Open after 07-28h: 6 of 37 settle hot** (bounded, intact, too energetic) —
   eyebot, Liberty Prime, a sentry, three turrets. All machines. Settled so far:
   the rest-pose joint error was constraints shipping with an UNSET parent
   transform (identity rotation, zero pivot, confirmed in the raw bytes); those
   are now derived from the rest pose as Havok's setInBodySpace would, taking calm
   from 28 to 31 of 37. Body placement is triple-validated and is NOT the problem:
   cinfo position equals the accumulated skeleton origin exactly and cinfo
   orientation matches within 0.1 degrees.
   **The residual is angular** — rebasing fixes pivots, not frames, and frames
   drive the limits (eyebot: 1.8 with `--no-limits`, 7,300 with
   `--only-limit hinge`). Next test, and it is a DATA question not a solver one:
   are these ragdolls authored in a different bind pose from the one the skeleton
   stores? Turret joint 2 has a properly authored pivotB of
   (0.7009, -0.1592, 0.2011) that still misses the rest pose by 0.22 m, which is
   what that would look like.
   Convex polytopes still want a proper centroid: the vertex mean is used, which
   is not the true hull centroid for an unevenly tessellated hull.
3. **Viewport.** Step in the render loop, draw the simulated pose, play/pause/reset.
4. **Interaction.** Ray-pick a body under the cursor, drag it with a mouse spring;
   fling spawned bodies at static collision.
5. **Static mesh collision.** Capsule vs `hknpCompressedMeshShape` triangles via a
   BVH over the already-decoded mesh.

Simulation doubles as the encoder's acceptance test: a ragdoll that is compiled
(4a item 6) and then falls over correctly is far stronger evidence than a byte
diff, because it exercises masses, inertia, joint frames and limits together.

**Note the units trap.** Ragdoll collision is in Havok metres, cloth in game units
(~70x apart, see 4b) — the solver works in one space and must convert at the edges.

### 4d. Compile every collision type — NEW 07-28f, agreed with bungo

Goal: write back every collision type, not just compressed meshes. Cloth is
explicitly deferred. Today `hknpEncodeCompressedMesh` is the *only* encoder and
the only caller is `collisiontools.cpp`, so everything else is a one-way trip.

Ordered by dependency:

1. **`hknpShapeMassProperties` — the blocker.** 43 distinct patterns in 53
   objects, so it is real per-shape data (`hkCompressedMassProperties`: centre of
   mass, inertia and major-axis space as `hkHalf` vectors, then mass and volume).
   Convex polytopes cannot be written without it. Decode this first.
2. **Capsule and sphere** — templates already fully specified in 4a: fixed 432 and
   128 byte objects, constant face/index tables, geometry a box hull around the
   axis.
3. **Convex polytope** — needs hull generation, and **qhull is already vendored**
   under `lib/qhull`, so the machinery is in the repo.
4. **Compounds** (static and dynamic) — instance arrays, decode side already works.
5. **Ragdoll** — root layout, `hkaSkeleton` and constraint atom chains, all
   specified in 4a. Watch the child/parent frame order and the `+0x80` bone-count
   trap.
6. **Round-trip validation** over the corpus, plus the simulation test above.

Coverage is only measured for **actor skeletons** (39 files). Architecture,
SetDressing, SCOL and Landscape are unmeasured, and that is where compressed
meshes, hulls and compounds actually live — run the corpus stride scan before
claiming any of this is complete.

## 5. Block Details — remaining typed editors (READY)

Small, independent, display-layer only, no corruption risk. **Two of the four
items filed here were already implemented** — see the note below; re-verified
07-27.

- **Colour swatch + picker** — genuinely open. `ColorEdit`
  (`valueedit.cpp:710`) is four r/g/b/a spin boxes with no swatch and no picker;
  `ColorWheel` exists but is used only by `settingspane.cpp`.
- **Texture path browse + missing-file marking** — genuinely open, zero hits.
  Resolve against the configured-resources VFS (the NIF Browser's BA2 index is
  cached, so this is cheap); missing file = red text, thumbnail tooltip via
  `TexCache`.

**Already done, do not rebuild** (both are upstream NifSkope behaviour that had
never been checked for):

- ~~String-index derived display~~ — `nifmodel.cpp:1385` resolves
  `tStringIndex` against the header string table and returns `"<string> [<idx>]"`,
  with distinct `<invalid string index>` / `<header strings not found>` messages.
  That *is* "the resolved string beside the raw index".
- ~~nif.xml tooltips on field labels~~ — `nifmodel.cpp:1512`, `ToolTipRole` on
  `NameCol`, builds `<p><b>name</b></p><p>description</p>` from
  `NifItem::text()`, which is the nif.xml description field
  (`src/data/nifitem.h:111`, `:583`). Block-type rows also get an ancestor list.

Design detail and the **binding visual rules** (flat `Name | Value` language,
hover-revealed row controls, coloured text not badges): see
`BLOCK_DETAILS_OVERHAUL_PLAN.md`.

## 6. Block Details — array table (P4)

"Open as table" on fixed-compound arrays (Vertex Data, Triangles, bone
weights): virtualized, viewport `pickedElems` sync, per-column stats including
NaN count, CSV export. Designed as the future display layer for the Tier-3
flattened storage (#3), so the two are worth sequencing together.

## 7. Block Details — whole-file search and revert (P5)

- Whole-file value search: the existing filter box gains a This Block / Whole
  File scope toggle; results list `block · field · value` rows that jump on
  click. Deferred/coalesced walk, skips big arrays unless the query is numeric.
- Recent values per field + "revert to loaded value" (capture original on first
  edit).
- Hex viewer.

## 8. Block List — remaining items

Shipped already: search, type chips, category icons, breadcrumb/footer,
Links-to/Referenced-by peek, foldable header.

Still open: a real per-type **summary column**, and **status badges**.
(**Drag-to-reparent was considered and rejected** — Set/Clear Parent cover it
safely. Do not build it.)

## 9. Rigging — leftovers

Verified 2026-07-21 against `src/spells/riggingtools.cpp`.

- **Classic NiSkin backend** (`NiSkinData`/`NiSkinPartition` path for
  Skyrim LE/SSE) — essentially unstubbed.
- **`CustomizationRemapNewBonesData`** — open; carries an unresolved
  leaked-pointer question.
- **Shader skinned-flag handling** for newly created skins — open.
- **In-game FaceGen validation** — a manual production check, not code.
- *Persistent skeleton reference* and *zero-weight bone pruning* — see #1; they
  belong to the Skeleton Manager.

## 10. UV editor — cross-mesh operators

Multi-mesh editing shipped (per-shape selection sets, cross-shape picking,
per-shape undo). Operators (merge / mirror / unwrap / pack), pins and hide
remain **active-mesh-only by design**; extending them across meshes is scope
expansion, not a gap.

Also deferred from the UV work: 3D-viewport seam marking (Ctrl+E) and Follow
Active Quads. **Proportional editing was explicitly declined — keep it that way.**

## 11. Animation / Timeline — 3 remaining gaps

**`TIMELINE_PLAN.md`'s 43 unticked boxes are all implemented** (verified by code
sweep 2026-07-21: key inspector, drag+snap, tangent handles, CSV round-trip,
lint, rubber-band multi-select, mute/lock, frames@fps, normalize, summary row,
preview-range band, easing presets, settings persistence). That file is history,
not backlog.

Genuinely open:
- **NiPSys (particle) controllers** are excluded from the Setup Controllers spell.
- **Rename sync** is lint's guided fix, not an automatic on-rename hook.
- **XYZ ↔ quaternion rotation conversion** is deliberately excluded from
  interpolation-type switching.

## 12. Rendering / engine

Rewritten 07-27 — this section said "future/disabled" while most of it shipped.
Plan and risk register: `RENDERER_MATCH_PLAN.md`.

**SHIPPED:**
- **Regression guard** — `WW_RENDER_SHOT` + `tools/render_regression/capture.ps1`,
  7 baselines, camera / scene clock / `showRefraction` / `showParticles` all
  pinned (an unpinned harness silently guards nothing — and the window size has
  to be pinned too, `WW_RENDER_SIZE`, or every compare returns "size mismatch").
- **FO4 spec/gloss on the editor's BRDF** (07-27a) — GGX + Smith + Schlick,
  `_s.R`→F0 at the dielectric 0.04, specular no longer gated on `hasSpecularMap`,
  energy conservation `(1-F)`. Channel assignment (`g`=gloss, `s`=spec) already
  matched the measured calibration.
- **`.pbrm` reader** (07-27b) — `src/io/pbrmfile.{h,cpp}`, Minimal Standard
  slice, fail-closed, CLI `pbrm` / `pbrm-resolve`.
- **Material resolution + auto-replace toggle** (07-27c), **`pbrm_default`
  program and the live mode** (07-27d), **three lighting modes** (07-27e).

**PARKED:** PBR mode and the auto-replace toggle — see §0. This is the whole
reason §12 is still open.

**Still future:** subsurface scattering. Genuinely not started.

**Unverified, needs a real asset:**
- `texAspect` resolution — `getTextureInfo( shaderProp->fileName( 0 ) )` may not
  match the cache key. Unproven; needs an 8:1 texture.
- A shape actually rendering *as* PBR — needs a `.pbrm` folder added under
  Settings ▸ Resources.

**Lightning leftovers** (07-26d shipped the controller-accurate rewrite):
mutation cadence is hardcoded at 1/24 s and amplitude decay at 0.5. Both are
guesses that looked right on screen; neither has been compared side-by-side with
the effect in game. Three *other* interpretations were disproven by rendering
and reverted — subdivisions-as-recursion-depth, Length-as-branch-length, and the
`Animate Arc Offset` re-reading. Do not re-try those without new evidence.

## 13. CLI follow-ups

Headless batch mode shipped 2026-07-21e (`src/nifcli.cpp`, docs in `CLI.md`):
`spells / info / list / dump / get / set / cast`, verified end to end. Open
follow-ups, roughly in value order:

- ~~**Parameterise Setup Controllers.**~~ **DONE 2026-07-21f** — logic moved to
  `AnimSetup::setupControllers()` (`src/spells/animationsetup.h`), dialog and
  CLI are both callers, exposed as `anim-setup`. Verified end to end; the
  generated graph passes `Animation/Fix Invalid AV Object Refs` unchanged.
  **GUI verification of the dialog is pending.** The same split is available
  for the other dialog-driven animation spells if they are ever wanted
  headless: Remove From Animation, Duplicate Sequence, Scale Sequence Times,
  Bake B-Spline To Keys.
- **Keyframe I/O from the CLI** — write key arrays from CSV/JSON. The data path
  is plain model arrays and the timeline's CSV round-trip already proves it.
  Pairs naturally with the item above and with the existing Animation lint
  spell, which can verify the result in the same run.
- **Port the `WW_*_TEST` harnesses onto it.** **There are 22, not eight** — the
  count in this file was frozen at 07-21 while 14 more were added (the whole
  `WW_POSE*` family, `WW_MERGEARCH`, `WW_OSPOSE`, `WW_CREATESKIN`, `WW_RENDER*`,
  `WW_PERF`, `WW_UI_SHOT*`). They hand-roll load → act → verify → quit in
  `nifskope_ui.cpp`; most of that is now CLI calls plus a script. Still the
  single biggest available reduction in that file's bulk — and now a
  medium-sized job rather than a small one. Note the GL-dependent ones
  (`WW_RENDER*`, `WW_POSEDRAW`, `WW_UI_SHOT*`) cannot move: `-no-gui` has no
  context, and `-platform offscreen` crashes on context creation (exit 139).
- **JSON output** (`--json` on `info`/`list`/`dump`/`get`) for machine
  consumption. Only worth doing once something actually consumes it.
- **MCP server** — deferred on purpose. A CLI is already drivable from an agent
  shell, which is most of the value. MCP earns its keep only for driving a
  *live* NifSkope, which means extending `IPCsocket::execCommand`
  (`main.cpp:235`, currently a five-line `if` understanding one command) into a
  real protocol. Decide after living with the CLI.

One CLI gotcha worth keeping here: a **running** NifSkope swallows filename
arguments over the UDP IPC handoff and exits 0, so a probe run silently opens the
file in the live session and your harness never fires. Always pass
`--port <unused>` when scripting.

## 14. Repo hygiene — NEW 07-27

- **`src/collisiontools.cpp` is a committed orphan.** It is *not* in
  `NifSkope.pro`; the live file is `src/spells/collisiontools.cpp` (2391 lines,
  Jul 26) and the orphan is the pre-move copy (2372 lines, Jul 11, last touched
  by `d5765c4`). Editing the wrong one compiles clean and changes nothing —
  exactly the kind of silent trap that costs an afternoon. Delete it, or move it
  under a `attic/` path that is obviously not built. It is the **only** such
  orphan: an exact-path diff of the `.pro` against `find src -name '*.cpp'`
  turned up nothing else.
- **~54 files uncommitted** across 07-25..07-27 (skin, lightning, spec/gloss,
  the whole PBRM stack, the regression harness). Committing has never been
  authorised in-session; the working agreement in `CURRENT_STATUS.md` still
  applies.

## 15. Viewport — Separate By Material / By Loose Parts — NEW 07-27

`glview.cpp:7241`: the Separate menu offers **Selection** (works), **By
Material** and **By Loose Parts**, the latter two `setEnabled( false )` with the
comment *"not implemented yet (Blender parity later)"*. Never filed here. Small,
self-contained, and the existing `separateSelection()` plus the FO4
skin/segment-aware path from 07-19e is most of the machinery.

Not to be confused with `glview.cpp:15032` **Clear Parent Inverse**, which is
disabled *permanently and correctly* — NIF scene objects store no Blender-style
parent-inverse matrix. That one is in the declined list, not the backlog.

---

## Verified SHIPPED — do not rebuild

Each of these was listed as open in some plan doc and is not. Confirmed against
the code on 2026-07-21, extended 2026-07-27.

| feature | evidence |
|---|---|
| Mirror editing (X) | `GLView::mirrorEditing`, `mirrorPairCache`, context-menu toggle |
| Checker deselect | `GLView::checkerDeselect` + redo panel, W ▸ Specials |
| Repeat Last (Shift+R) | `GLView::repeatLastOperator`, `viewport.repeat_last` |
| F9 panel at cursor | `viewport.panel_to_cursor` |
| Rip (V) / Split (Y) | `glview.cpp` "Split (Y) / Rip (V)", in-place undo |
| Bevel (Ctrl+B) | `GLView::bevelSelection` — needs **GUI testing**, not code |
| Rest-pose display | `Node::viewTrans`/`worldTrans` → `restWorldTrans()` |
| Euler rotation editing | `valueedit.cpp` `mEuler` |
| Tris to Quads (Alt+J) | `GLView::trisToQuads`, works from any select mode |
| Entire timeline v2 checklist | see #11 |
| Stock **Vertex Flags** spell is correct | tested + disproven landmine, `WW_CHANGES 2026-07-21a` |
| Pose mirror / paste-flipped | `PoseMirrorButton`, `ogl->poseMirrorBone()` — `posetools.cpp:192`, `:491` |
| String-index derived display | `nifmodel.cpp:1385` → `"<string> [<idx>]"` — **stock** |
| nif.xml tooltips on field labels | `nifmodel.cpp:1512` `ToolTipRole`/`NameCol` ← `NifItem::text()` — **stock** |
| Proportional editing (O / Shift+O) | `glview.cpp:184`, 8 Blender falloff curves, self-test at `:6327` — **the decline was reversed** |
| NifItem slab pool (perf T3 batch 1) | `nifitem.h:300` + `nifitem.cpp` — the 07-27 sweep first mis-read this as unbuilt |
| Multi-section collision encoder | `hknpencode.cpp:207` — spatial-slab partitioning, cap is now **4096 sections** |

## Explicitly declined — do not implement

- ~~**Proportional editing** (Blender O)~~ — **this decline was reversed.** The
  user later asked for it and it shipped in 07-22i (all 8 Blender falloff
  curves). The stale entry survived here for five days. Left visible on purpose:
  a decline is a snapshot of one conversation, not a permanent law, and this file
  has to be re-read against the change log rather than trusted.
- **Drag-to-reparent** in the Block List — Set/Clear Parent cover it safely.
- **Curated sections / header card** (Block Details P1) — the user chose "keep
  the existing tree, improve it in place" instead.
- **Loop Cut "Even" and "Correct UVs"** — deliberately not carried.
- Draw sorting (T2.13) and particle frustum culling (T2.14) — see #3.
- **Clear Parent Inverse** (`glview.cpp:15032`) — not a gap. NIF scene objects
  store no Blender-style parent-inverse matrix, so the action is permanently
  disabled with a tooltip saying so.
- **Clean-room rewrite for legal independence** — asked and answered 2026-07-26:
  clean-room requires never having read the original, which is already false.
  WW Edition stays an acknowledged BSD-3 derivative. Do not re-open.

## Awaiting verification (not implementation work)

**The working agreement changed on 07-22 and again on 07-26; the old line here
("the agent does not run GUI sessions") is wrong.** Current rules:

- The **agent** verifies GUI behaviour itself, via the `WW_*_TEST` harnesses and
  the CLI — not by handing over a checklist.
- **No interactive GUI launches while the user is working** (07-26). Screenshot
  harnesses are fine; `SetForegroundWindow` never is — it is refused from a
  background process anyway, and one attempt captured the user's Blender window
  instead of NifSkope. Always `--port <unused>`.
- The **user** owns in-game validation. That is the one thing no harness covers.

**Outstanding, no automated coverage:**
- **Bevel** — highest risk, still has no harness (`bevelSelection` is reachable
  only from the key dispatcher and the context menu). Test on a copy.
- The 07-20d..o batches (flag dialog, Block Details batch 1 + diff/Reference
  column, viewport batch, Loop Cut v1→v3), 07-21b pinned fields, performance
  batches 1–3.

**Needs the game, not a harness:**
- Multi-section compiled collision — an in-game walk test.
- Lightning cadence + decay (§12).
- FaceGen output from the rigging tools.

---

## 2026-07-27 verification record

What was actually checked, so the next sweep can trust or re-test it rather than
guess. **Confirmed still open** (grep evidence, all zero-hit unless noted):
Skeleton Manager (`plannedWorkspaces`), prop staging, colour swatch/picker
(`ColorEdit` is spin boxes), texture browse, array table, hex viewer, whole-file
search, Block List summary column + badges, classic `NiSkinData` backend (a lone
`TBD` comment at `riggingtools.cpp:302`), `CustomizationRemapNewBonesData`,
shader skinned-flag, NiPSys controllers (`controllerOptions` covers
BSEffectShader / BSLightingShader / NiAlphaProperty / NiLight / NiAVObject only),
rename-sync-is-a-guided-fix (`isNameMismatch` in `runLint`, `timelineedit.cpp:1563`),
XYZ↔quaternion conversion, perf 15c flattened storage, per-triangle face
materials, `--json`, keyframe CLI I/O.

**Not re-verified** — carried forward on the 07-21 sweep's word: the individual
07-20d..o GUI batches, the `TIMELINE_PLAN.md` 43-item sweep, the Blender-batch
SHIPPED table rows above, and the `PERFORMANCE_PLAN.md` measurements behind the
15c/16 deferral. If any of those matter to a decision, re-check before relying
on them.

---
---

# APPENDIX — Collision Manager feature spec

Preserved verbatim; this is the only copy. Technical handoff (validated
packfile offsets, PyNifly corrections, test assets, phasing, gotchas) is in
`COLLISION_MANAGER_HANDOFF.md`; the interactive UI mockup is
`docs/collision_manager_mockup.html`.

## Collision Manager — CONCEPT (2026-07-09, supersedes "Collision workflow" 07-08)

Third manager dock after Timeline and Mat/Tex Manager. One place to see,
create, edit, and compile collision. Factory `tlCreateCollisionManagerDock`
in a new `src/spells/collisiontools.cpp` (same pattern as
`tlCreateMatTexManagerDock` in meshtools.cpp).

**Implementation status (2026-07-10): functional workflow landed.** The
dock, live compiled/editable browser, row selection sync, Decode/Decode All,
Create Collision, viewport/block-list right-click entry points, Decimate via
the live preview, guided linter, amber budget warnings, donor import,
mass-from-material, Collision -> BSTriShape, snapshot undo and a validated
single-section compressed-mesh compiler are working.
Compiled collision has translucent solid + type-wire display with persisted
Solid/X-ray/Colour-by settings. Multi-section/compound encoding and
per-triangle face-material painting remain in P4.

### Existing building blocks (all in-repo, all working)
- `hknpdecode` — full packfile decoder incl. per-body physics
  (layer/friction/restitution/mass/COM), Body ID binding validated
- Decode / Decode All Compiled Collision spells (legacy-chain output with
  authored physics values)
- `spCreateCVS` "Create Convex Shapes" — BSTriShape/NiNode -> bhkBoxShape
  (auto box detect) / bhkConvexVerticesShape (+ bhkTransformShape,
  bhkListShape, bhkRigidBody, bhkCollisionObject), with **CoACD** convex
  decomposition and its own settings dialog
- vendored libs: **qhull** (hulls), **miniball** (optimal bounding sphere),
  **CoACD** (approx convex decomposition), **meshoptimizer** (decimation,
  used by simplify.cpp)
- decode spell's `buildShape` already builds the bhkNiTriStripsShape mesh
  chain and primitive shapes
- PyNifly's `bhk_autopack.py` — working, game-proven packfile WRITER
  (reference for the encoder; our validated corrections: body_props stride
  0x50, layer at cinfo+0x1C, dyn_motion/dyn_inertia layout)

### Panel A — Collision browser (top)
Tree of every node with collision, one row per bhk(NP)CollisionObject:
node name | state (COMPILED packfile / EDITABLE legacy / mixed) | shape
summary (Box, Hull(24v), Mesh(1.2k tris), Compound(5)) | layer | material |
static/dynamic | mass. Selecting a row selects the block and highlights it
in the viewport (amber preview already draws compiled collision). Buttons:
Decode, Decode All, **Compile** (encoder), Compile All, Delete, Copy To
(pick target node).

### Panel B — Create collision (from selected BSTriShape / NiNode)
Target type: Box (min OBB via PCA + wrapped bhkTransformShape) | Sphere
(miniball) | Capsule (PCA axis + radius, the missing primitive fit) |
Convex Hull (qhull, existing path) | Convex Decomposition (CoACD, existing
path) | Triangle Mesh (buildShape's bhkNiTriStripsShape chain).
Preset: Static / Prop(dynamic) / Custom — writes the validated authoring
values (dynamic: MotionSystem 3, Quality 4, SolverDeact 2; static: 5/0/1;
PenDepth 0.15 etc.). "Replace existing" checkbox.
- **Collision type** dropdown (UI name for the Havok Filter layer): all 57
  Fallout4Layer entries from nif.xml (STATIC 1 ... CHARBUMPER 56).
- **Material** is a SEARCH FIELD (added 2026-07-09): an editable combo —
  type a full or partial name and the 157-entry Fallout4HavokMaterial list
  filters live (QCompleter, ContainsMatching, case-insensitive). A name
  that matches nothing becomes a **custom material**: FO4 material values
  are CRC32 hashes of the name — poly EDB88320, **init 0, no final xor,
  lowercased** (verified: crc("materialwoodcrate")=341181474,
  crc("metal")=104858580 — legacy Material_Metal/_Wood hash from the short
  names "metal"/"wood"). Show the computed CRC next to the field; unknown
  CRCs elsewhere display as Custom(0x...). Custom names are remembered
  (QSettings list) and appear in future searches. Same hashFunctionCRC32
  the obj importer already uses.
- **User presets** (added 2026-07-09): a 💾 button saves the current create
  setup — collision type, material, and all physics values — as a named
  preset next to the built-in Static/Prop entries. Persist via QSettings
  ("CollisionManager/Presets/<name>"), delete via right-click on the entry.
- Buttons are the round-trip pair: **BSTriShape → Collision** (primary
  create action) and **Collision → BSTriShape** side by side.
- **Decimate Collision** button (added 2026-07-10): Blender-style geometry
  reduction for the selected collision mesh or collision-source BSTriShape,
  using the vendored meshoptimizer path already used by `simplify.cpp`.
  The redo panel exposes Ratio and Target Triangles, Preserve Boundaries,
  and a live before -> after vertex/triangle count. Apply it before creating
  Triangle Mesh / Convex Hull / Convex Decomposition collision, or to an
  editable collision proxy; compiled collision prompts **Decode to edit**.
  Preserve node transforms, material/layer, and rigid-body physics, rebuild
  bounds/topology, reject a degenerate result, and wrap the operation in a
  `NifSnapshotCommand` so it is fully undoable.

### Panel C — Physics properties (3ds Max parity, from 07-08 agreement)
Bound to the selected rigid body: Mass, Friction, Restitution, damping,
max velocities, gravity factor, layer, material, motion/quality/solver/
deactivator, COM override. Live edit of the legacy blocks; greyed out for
COMPILED rows with a "Decode to edit" hint (or auto decode-edit-recompile
later).

### Workflow accelerators (agreed 2026-07-10)
- **Right-click entry points**: block list BSTriShape/NiNode -> Havok ->
  Create Collision opens Panel B's logic with the active/last-used preset;
  the viewport object-mode menu gets Create Collision plus contextual Decode
  Collision / Compile Collision when the clicked object has collision.
- **Collision linter**: Check Collision uses the timeline-lint guided-fix
  pattern. Flag visible geometry with no collision, mixed compiled/editable
  state, suspicious hull vertex counts, near-box hulls that should be boxes,
  non-uniform primitive scale, STAIRHELPER bodies without a slope, and
  dangling bhkNPCollisionObjects. All listed checks are implemented; safe
  fixes cover dangling objects and layer-zero inference in one snapshot.
- **Import collision from donor NIF**: choose a file and collision node, copy
  its collision to the active node with transforms adjusted. Compiled donor
  collision may be decoded immediately for editing.
- **Mass from material**: density presets per material family times computed
  shape volume. Show the derived density and mass; allow manual override.
- **Snapshot undo for Compile/Decode**: wrap both structural operations in
  `NifSnapshotCommand` so the entire round trip is one safe undo step.
- **Collision budget**: manager footer totals collision vertices/triangles and
  packfile bytes, with amber thresholds for meshes worth decimating. Raw totals
  plus thresholds and per-row warnings are implemented.
- **Per-triangle mesh materials (P4)**: face-material painting backed by
  hknpBSMaterialProperties once compressed-mesh encoding supports its material
  list.

### Collision <-> BSTriShape round trip
- **Collision -> BSTriShape**: decoded (or legacy) shape geometry to a
  BSTriShape under the same node, so the full mesh toolset (edit mode,
  separate/join, transforms, mirror) can edit it. Spell + manager button.
- **BSTriShape -> collision**: Panel B on the edited mesh, "replace
  existing" — closes the loop decode -> mesh-edit -> rebuild -> compile.

### Viewport display (agreed 2026-07-09): solid, not wireframe
Collision draws as **translucent solid + wire overlay** (Havok Visual
Debugger style), unmistakable next to lit/textured BSTriShapes:
- Fill pass: tris at ~30% alpha, unlit, depth test ON / depth write OFF
  (overlaps blend), polygon offset vs the render mesh, backfaces first and
  darker (volume cue without lighting)
- Wire pass on top at full alpha (current wireframe becomes the overlay)
- Toggles: **X-ray** (fill ignores depth - see through walls),
  **Collision only** (hide render geometry); persisted display options
- Primitives get triangulated proxies (icosphere/capsule) for the fill;
  the synthesized analytic rings stay as their wire pass
- **Colour by** dropdown (one hue channel, so it's a mode):
  **Material** (default: bucket Fallout4HavokMaterial enum names by
  substring into ~12 families - wood browns, metal blue-grey, stone greys,
  glass pale cyan, dirt/gravel ochre, cloth purple, flesh pink...; tooltip
  shows exact material name) | Layer | State (compiled/editable) | Type
- **Wire colour always encodes shape type** regardless of mode: white box,
  yellow hull, cyan sphere/capsule/cylinder, magenta mesh, orange compound
  instance. Selection highlight overrides, as now.
- Manager browser rows show fill+wire swatches = the palette legend
- Compiled and legacy paths route through ONE shared two-pass helper
  (also unifies today's amber-vs-layer-colour inconsistency)
- **Material labels in the viewport** (added 2026-07-09): toolbar mode
  Off / **Selected (default)** / All — a small colored dot + material name
  (halo text, QPainter overlay like the snap indicator) anchored at each
  shell's top center. Declutter rules so it never gets busy: skip labels
  for shapes whose projected screen radius < ~40 px, skip when labels
  would overlap (keep the nearer shape), cap ~20 labels, and "All" only
  applies while Show Collision is on. CRCs reverse-looked-up to names,
  unknown -> Custom(0x...).

### Selection info in the bottom bar (added 2026-07-09)
The main window has no QStatusBar in use — add one. Selecting collision
anywhere (viewport shell click, manager row, block list) shows a one-line
readout: `Crate01 — Box · STATIC(1) · MaterialWoodCrate · COMPILED · 0 kg ·
fric 0.50 rest 0.40`. Material CRCs reverse-looked-up to names (unknown ->
Custom(0x...)). Clicking the readout focuses the manager row. Driven by the
same selection signal the manager uses.

### Transformable collision (added 2026-07-09)
Collision must move/rotate/scale in the viewport exactly like BSTriShapes
(the existing object-mode G/R/S tools). Where the transform writes, by case:
- **Whole body** (shell/row selected): write the OWNING NODE's transform —
  that IS the body placement (Body ID binding). Synergy: Set Origin
  (Shift+Ctrl+Alt+C) already works on plain NiNodes, so it repositions
  collision pivots too.
- **Sub-shape** in a bhkListShape: write (create if absent) its
  bhkConvexTransformShape / bhkTransformShape wrapper Matrix4 — Havok units
  (game translation / 69.99125).
- **Primitives**: sphere center via wrapper translation; capsule axis via
  First/Second Point; uniform scale -> Radius. Non-uniform scale on
  primitives is refused (Havok can't represent it).
- **Mesh shapes**: keep transform in the wrapper while dragging; offer
  "bake into vertices" (NiTriStripsData, game units) on apply.
- **Compiled systems are read-only**: G/R/S on a compiled shell prompts
  "Decode to edit" (one click, then the gizmo continues).

### Encoder: `hknpencode` — compile packfiles WITHOUT Elric
C++ port of PyNifly's packer with our corrections. `src/gl/hknpencode.{h,cpp}`
(paired with hknpdecode) + "Compile Collision" spell:
legacy bhkCollisionObject tree(s) on a node set -> one bhkPhysicsSystem +
per-node bhkNPCollisionObject with Body ID (the binding we validated).
**Status 2026-07-10:** the native writer, manager Compile action, structural
replacement, snapshot undo, and in-process encode -> decode validation are
implemented for one compressed-mesh section (<=255 verts/tris), including
dynamic/static physics, layer, material, density and AABB inertia. Remaining:
multi-section meshes, shared multi-body systems/compounds, primitive-specialized
writers, per-triangle material painting, FileConvert and in-game validation.
1. FixupBuilder (local/global/virtual tables, 0xFF terminators) + classnames
   section writer + 0x40 header / 3 section headers — layouts fully known
2. PSD writer: body_props (0x50/body: friction/restitution trunc-f16 at
   +0x12/+0x14dup/+0x16), BodyCInfo (0x60: shape ptr fixup, layer +0x1C,
   qualityId 0xFF, body index +0x1A, motion idx +0x0C, pos +0x30, quat
   +0x40), ShapeEntry; dyn_motion + dyn_inertia when dynamic (inverseMass,
   density = mass/volume, inertia diagonal — compute from shape like Elric)
3. Shape writers: hknpConvexPolytopeShape (verts w=0x3F000000|idx, planes,
   faces, fvi — inverse of our decoder), hknpSphereShape; compressed mesh
   single-section (<=255 verts/quads) first, multi-section later;
   hknpDynamicCompoundShape for lists/instances (phase 2);
   hknpBSMaterialProperties for mesh materials (phase 2)
4. **Validation, no game needed**: (a) round-trip decode(encode(legacy)) ==
   decode(elric_output) field-by-field on the controlled pairs
   (Documents/3dsMax/export); (b) corpus: encode(decode(vanilla)) ->
   decode == original decode over the 300-file architecture corpus
   (extend tools/batch_validate.py); (c) FileConvert.exe must accept our
   packfiles (it rejects malformed ones — free structural linter)
5. In-game smoke test last: known prop with our compiled collision

### Phasing
- **P1 encoder core**: convex/box/sphere static single-body + spell +
  validation harness (unlocks the full author-in-NifSkope loop; everything
  else is UI around it)
- **P2 manager dock**: browser panel + wire existing spells + physics panel
- **P3 create/convert**: capsule fit, mesh chain button, collision<->
  BSTriShape round trip
- **P4 heavy formats**: compressed-mesh encoder, compounds/instances,
  dynamic props with computed inertia, materials, multi-section meshes

