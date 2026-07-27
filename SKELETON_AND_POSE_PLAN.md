# Skeleton Manager & Pose Manager — Plan

> **Design detail.** The backlog lives in **`TO_BE_IMPLEMENTED.md`** (§1 and §2);
> this file is the design for those two entries. Drafted 2026-07-21, nothing
> built yet.

Both workspaces are already reserved as **disabled** menu entries in
`nifskope_ui.cpp` (~L4747), below the six implemented ones (Timeline, Material,
Collision, Rigging, Vertex Paint, UV). No dock exists for either — deliberately,
so persisted workspace indexes do not shift when they land.

Their tooltip contracts, which this plan is bound by:

- **Skeleton Manager** — *"skeleton hierarchy, rest-pose, bone transform, and validation workspace."*
- **Pose Manager** — *"character posing, prop staging, reusable pose, and load-screen composition workspace."*

---

## Design rule: follow Blender

Where a decision is unclear, the answer is **whatever Blender does**. The fork
is already Blender-familiar in the viewport (G/R/S, Tab, W specials, Ctrl+R,
box/circle select, 3D cursor, redo panels), so a feature invented from first
principles will feel foreign beside it. Deliberate divergences are fine but must
be stated with the reason.

Blender's model, NifSkope's skin: the visual language stays NifSkope's own flat
`Name | Value` tree (Shader Flags dialog aesthetic) — never web-app chrome.

### The mapping

| Blender | NIF / FO4 | State in this fork |
|---|---|---|
| Armature object | no such object — bones are plain `NiNode`s | — |
| Armature **Edit Mode** (edit bone rest transforms, parenting) | bone `NiNode` translation/rotation/scale + hierarchy | **Skeleton Manager** |
| **Rest Position / Pose Position** toggle | — | **already shipped**: `Scene::restPoseBlock` + `Node::restWorldTrans()` |
| Armature **Pose Mode** (transform bones, rest data untouched) | transforms applied on top of rest | **Pose Manager** |
| **Action** (keyframed pose data) | `NiControllerSequence` → ControlledBlocks → interpolators → `NiTransformData` | shipped (timeline, `anim-setup`) |
| **Pose Library** (poses as Action assets, apply/blend) | a sequence holding **one key per bone** | **Pose Manager** — see §B.1 |
| **Vertex Groups** + Armature modifier | per-vertex bone indices/weights in `BSVertexData`, `BSSkin::Instance` | shipped (Rigging Manager, weight paint) |
| Bone display (octahedral/stick) | nothing — nodes draw as dots/axes | **Skeleton Manager**, §A.5 |
| Bone Collections / layers | — | filtering only (§A.4) |
| X-Mirror, `.L`/`.R` naming | same convention in FO4 rigs | partly shipped (`riggingFlipBoneName`, weight-paint Mirror) |
| Bone constraints, IK, B-Bones | no runtime equivalent | **non-goal** |

---

# Part A — Skeleton Manager

Blender counterpart: **Armature Edit Mode + the Bone properties tab + the
Outliner's armature view.**

## A.1 Scope

1. **Hierarchy** — a real skeleton tree: bones, their parenting, which shapes
   each influences. Blender's Outliner filtered to an armature.
2. **Bone transform** — edit a bone's rest transform (translate/rotate/scale),
   Blender's Edit Mode. **This is the dangerous one — see A.3.**
3. **Rest pose** — expose the existing rest/pose toggle here, where it belongs,
   instead of only as an edit-mode side effect.
4. **Validation** — the checks that catch a broken rig before the game does.
5. **Persistent skeleton reference** — the "active `skeleton.nif`" slot that
   `BONE_WEIGHT_TRANSFER_PLAN.md` §6.6 already argues for; every tool reuses it.

**Non-goals:** bone constraints, IK, B-bones, envelopes, bone roll (NIF bones
have no head/tail so roll is meaningless), retargeting.

## A.2 What already exists — do not rebuild

- `Scene::restPoseBlock` + `Node::restWorldTrans()` — rest-pose display, shipped.
- `riggingBoneNames()`, `riggingSkinInstance()`, `riggingFlipBoneName()` in
  `src/spells/riggingtools.cpp`.
- Rigging Manager: donor/receiver bone trees, bind bones, transfer weights,
  weight paint, Mirror (X).
- Cross-file import precedent: `importDonorCollision` (`collisiontools.cpp:1356`)
  — file dialog → standalone `NifModel` → serialize a branch → splice with a
  block-number remap. **Use this verbatim** for pulling bones from `skeleton.nif`.
- The validated inverse-bind math in `BONE_WEIGHT_TRANSFER_PLAN.md`
  (`skinToBone = inv(boneWorld) @ meshBindWorld`, ~1e-5 vs vanilla).

## A.3 The rebind hazard — the central correctness problem

In Blender, moving a bone in Edit Mode silently keeps the mesh bound, because
the armature modifier recomputes deformation from the current rest pose.

**NIF has no such indirection.** `BSSkin::BoneData` stores a baked inverse-bind
transform per bone. Change a bone `NiNode`'s transform and every vertex weighted
to it deforms wrongly — the mesh tears or explodes.

So **any bone transform edit must recompute the affected bones' inverse-bind
transforms in the same undo step**, using the already-validated formula. Two
modes, both worth having, and the UI must say which is active:

- **Rebind (default)** — mesh stays put; the bone moves and the bind updates.
  This is Blender Edit Mode's felt behaviour.
- **Deform** — mesh follows the bone. This is Pose Mode's behaviour, and it
  belongs in the Pose Manager, not here.

Bone bounds in `BoneData` must be recalculated too (`Mesh/Update Bounds`
already does this — see the note in `CLI.md` about it writing a *zero* block
sphere for skinned shapes by design).

## A.4 Layout

Flat, NifSkope's own language. Docked left/right like the other managers.

```
Skeleton reference   <none>                    [ Load... ]   ← persistent, §A.6
Skeleton root        Root  (block 0)

[ search................ ]  [ All | Bones | Deforming | Unused ]

Bone                          Shapes   Verts   Weight
  Root
    Chest                        1      412    38.2      ← deforming
      LArm_UpperTwist            1       96     8.1
      RArm_UpperTwist            0        0     0.00     ← unused, prunable
    Camera                       0        0     0.00     ← not a bone, grey

Rest pose  [x]                                          ← Scene::restPoseBlock

Bone transform    (LArm_UpperTwist)
  Translation   X ....  Y ....  Z ....
  Rotation      X ....  Y ....  Z ....      (Euler, mEuler exists)
  Scale         ....
  On edit:  (•) Rebind — keep the mesh in place    ( ) Deform
  [ Recompute bind ]  [ Update bone bounds ]

Validation                                    [ Re-check ]
  2 bones carry no weight — RArm_UpperTwist, LLeg_Tail
  bone 'Neck' is in the skin but absent from the node tree
  duplicate bone name 'Chest' (blocks 12, 41)
  [ Prune unused bones ]
```

Rows are selectable and two-way sync with the block list and viewport, like
every other manager. Findings are clickable and select the offending block.

## A.5 Bone display in the viewport (Blender parity)

Currently bones draw as node dots. Blender draws **octahedral** bones between
head and tail, which is most of why an armature is readable.

NIF bones have no tail, so derive one: a bone's shape runs from its own origin
to its child's origin (average of children when several; a short stub along the
local axis for leaves). Blender's Stick and Wire display modes map cleanly too.
Worth doing — it turns an unreadable dot cloud into a skeleton. Deferred to
phase 3 because it touches the render path.

## A.6 Persistent skeleton reference

A dock slot holding an active `skeleton.nif`, remembered in `QSettings` and
shared by every rigging tool. Supplies:

- bone rest transforms + hierarchy as the authority,
- missing bones to splice in (name + transform) when transfer or paint needs one,
- the common frame that makes cross-file weight transfer well-defined.

This is `BONE_WEIGHT_TRANSFER_PLAN.md` §6.6's proposal; building it here retires
the loose "independent persistent skeleton reference" backlog item.

## A.7 Phasing

1. **Read-only** — hierarchy tree, bone/deforming/unused classification,
   influence and weight counts, selection sync, rest-pose toggle. No writes, so
   nothing can break. Ships useful on its own.
2. **Validation + prune** — the checks, then `Prune unused bones` (removes the
   bone from `Instance`/`BoneData` and remaps every vertex's bone indices —
   the Join bug in reverse; the index-remap lesson from `WW_CHANGES 2026-07-19b`
   applies exactly).
3. **Bone transform + rebind** — the risky part. Gauntlet before shipping:
   must-not-move check for a parent bone and a rotated non-parent bone, as the
   Create Skin gauntlet already does.
4. **Skeleton reference slot**, then **viewport bone display**.

## A.8 CLI surface

Everything above is model-layer work, so it belongs in batch mode too
(`src/nifcli.cpp`):

```
skeleton <file>                       hierarchy, deforming/unused, weights
skeleton <file> --validate            the checks, exit 1 if any fire
skeleton <file> --prune-unused -o OUT
```

`--validate` returning non-zero makes it usable as a pre-export gate, which is
the real payoff.

---

# Part B — Pose Manager

> **STATUS 2026-07-22 — SHIPPED, bar two deferred items.** Pose Mode already
> worked (07-22b); the pose library + blend shipped in the shared API (07-22c);
> the **dock** with a bone list, save/apply/delete and a blend slider shipped
> 07-22d (`src/posetools.cpp`, Workspaces ▸ Pose), verified by
> `WW_POSEDOCK_TEST`. **Still open: prop staging (B.3.3) and mirror/paste-flipped
> (B.5.3)** — see §B.6 for the prop-staging question. Load-screen composition
> resolved to merge-into-one-file (07-22a).

Blender counterpart: **Pose Mode + the Pose Library + the Action Editor.**

## B.1 The key decision: a pose *is* a one-key sequence

In Blender a pose is an **Action** — keyframes on pose bones — and the Pose
Library stores poses as Actions. The NIF equivalent of an Action is a
`NiControllerSequence`.

> **A pose is a `NiControllerSequence` carrying exactly one key per controlled
> bone at t=0.**

Everything follows from that, and almost all of it already exists:

- **Creating** a pose = the `Setup Controllers` path, already parameterised and
  CLI-driven (`AnimSetup::setupControllers`, `anim-setup`).
- **Storing** poses = ordinary sequences under `NiControllerManager` — visible
  in the timeline, editable there, saved in the file with no new format.
- **Applying** a pose = evaluate the sequence at t=0. `Controller::interpolate`
  already does this; the timeline already scrubs it.
- **Blending** two poses = interpolate between their keys — Blender's pose
  blending, and trivial on one-key data.
- **A pose library across files** = a `.nif`/`.kf` of sequences, imported with
  the `importDonorCollision` cross-file recipe.

**Invent no pose format.** A bespoke JSON/extra-data pose store would duplicate
the sequence machinery, be invisible to the timeline, and not survive export.

## B.2 Pose Mode vs Edit Mode (why this is a separate workspace)

Blender's split is exactly the one that matters here:

- **Skeleton Manager = Edit Mode** — changes the *rest* skeleton; bind data must
  be recomputed (§A.3).
- **Pose Manager = Pose Mode** — transforms bones *on top of* rest; rest data and
  bind transforms are never touched. The mesh is meant to deform.

Practically, Pose Mode writes to interpolator keys (or to a live pose buffer that
is then keyed), never to bone `NiNode` rest transforms. That single rule keeps
posing incapable of corrupting a rig, which is the main safety argument for
building it after A.

## B.3 Scope

> **VERIFIED 2026-07-22 — item 1 below is ALREADY DONE.** `WW_POSE_TEST` proved
> that selecting a bone node and transforming it poses the skinned mesh live:
> skinned bounds moved 6.71, 7938 pixels changed, restoring the bone returned the
> delta to exactly 0, and **both** merged armour pieces followed the shared bone.
> `updateBoneTransforms()` reads `bone->localTrans( skeletonRoot )` — the live
> node transform — so the posing engine needs no work. See
> `WW_CHANGES 2026-07-22b`. **What remains is the library (item 2) and viewport
> convenience** (picking bones in the viewport rather than the block list).

1. ~~**Pose Mode**~~ — **already worked** (live skinning), and 2026-07-22f added
   a real viewport Pose Mode on top: skeleton drawn with parenting, click-to-
   select bones, hover highlight, bone-name labels, filter (all/deforming/face
   sculpt), reset-to-rest (all + split rot/loc/scale), and X-axis mirror. In the
   mode dropdown + Pose workspace. See `WW_CHANGES 2026-07-22f`.
2. **Pose library** — named poses (sequences) listed, applied, blended, renamed,
   deleted; apply to all bones or the selected subset.
3. **Prop staging** — attach an external NIF under a bone (Blender: parenting an
   object to a bone). Cross-file import precedent exists.
4. **Load-screen composition** — see the open question in §B.6.

**Non-goals:** IK, constraints, physics, NLA-style layering.

## B.4 Layout

```
Pose      T-Pose ▾                 [ Apply ]  [ New from current ]  [ Delete ]
Blend     0% ──────●────── 100%                       ← Blender pose blending

[ search................ ]  [ Selected bones only ]

Bone                         Δ Translation      Δ Rotation
  Chest                      0, 0, 0            0, 0, 0
  LArm_UpperTwist            0, 0, 0            0, -12.5, 0     ← posed
  ...

[ Clear transforms ]  [ Clear selected ]  [ Copy pose ]  [ Paste mirrored ]

Props                                                    [ Attach NIF... ]
  Weapon_Rifle       →  WeaponBone
```

`Paste mirrored` reuses `riggingFlipBoneName` + the weight-paint Mirror logic —
Blender's Paste Pose Flipped.

## B.5 Phasing

1. ~~**Pose library over existing sequences**~~ — **SHIPPED 2026-07-22c** as
   `AnimSetup::poseBoneNodes/savePose/applyPose` + CLI `pose --list/--save/--apply`.
   Save→move→apply restores a bone exactly. **Blend is still open**, as is the
   dock UI; the model-layer half is done and shared, so a Pose Manager dock is
   now mostly presentation over an existing API.
2. **Pose Mode transforms** — bone selection + G/R/S writing to keys, live
   deform. Needs the viewport work; reuses the existing modal transform.
3. **Mirror / copy / paste-flipped.**
4. **Prop attach.**
5. **Load-screen composition**, once §B.6 is answered.

## B.6 Open questions

1. ~~**"Load-screen composition"**~~ — **ANSWERED 2026-07-22 by the user.** The
   workflow is: open an armour piece → merge the rest of the set and a
   `skeleton.nif` into it → pose that skeleton with the armour following → that
   posed file *is* the load screen. So composition is **merge-into-one-file**,
   not a multi-file scene system — far smaller than feared.

   **The merge step shipped the same day** (`WW_CHANGES 2026-07-22a`,
   `src/nifmerge.cpp`, CLI `merge`), including the de-duplication that makes the
   set share one skeleton — without which posing is impossible. Loading a bare
   `skeleton.nif` is the same command, since a skeleton is just `NiNode`s.

   What remains for this workflow is only Part B's posing itself.
2. **"Prop staging"** — attach props into the *saved file* (splice the prop's
   branch under a bone), or preview-only for screenshots? The first is a real
   edit, the second is a viewport feature.
3. **Pose library scope** — in-file sequences only, or a cross-file library on
   disk? Blender's asset browser is cross-file; matching that means a library
   folder plus import, which is phase-4 sized.

---

## Shared guardrails — project landmines that apply to both

Learned the hard way; violating any of these has already cost a debugging
session in this repo.

- **Bulk writes go under `setState(BaseModel::Processing)` + one
  `dataChanged`.** Per-leaf signals are quadratic with live views — a 10× freeze,
  measured. Applies to weight/index remaps when pruning bones.
- **`holdUpdates` does NOT suppress per-leaf `dataChanged`; only state does.**
- **Any load-into-existing-model path must self-suppress signals** —
  `loadIndex()` does not set Loading state, so splicing bones from a
  `skeleton.nif` must wrap insert+load in `setState(Loading)`/`restoreState`.
- **A freshly `updateArraySize`'d `BSVertexData` row leaves its `#ARG#`
  conditional arrays (`Bone Weights`/`Bone Indices`) zero-length** until a
  deferred cascade. Size them before copying, and **test weight VALUES, not just
  index ranges** — the index check passed while every weight was zero.
- **Bone index remapping is mandatory** when the bone list changes. Verbatim
  vertex copies left indices pointing into an old list — that was the Join
  corruption (`WW_CHANGES 2026-07-19b`).
- **Do not touch Vertex Desc.** Nothing here needs it; that is the Create Skin
  corruption zone.
- **In-place undo where possible** (`TlShapeStateCommand`); snapshot undo
  reloads the whole model and visibly flashes. Ops that add/remove *blocks*
  (splicing a bone, attaching a prop) stay snapshot by design.
- **Every phase ships with a headless harness**, following
  `WW_*_TEST` + an independent Python verifier. For A.3 specifically, reuse the
  Create Skin gauntlet's must-not-move check.

## Build order

**A.1 → A.2 → B.1 → A.3 → A.6 → B.2 → the rest.**

Read-only skeleton first (useful, unbreakable), then validation and pruning,
then the pose library — which is nearly free given the sequence machinery — and
only then bone transforms with rebind, the one genuinely risky piece. B.1 lands
before A.3 because it delivers user-visible value at near-zero risk while the
dangerous work waits for its gauntlet.
