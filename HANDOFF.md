# Handoff — NifSkope, Wild Wasteland Edition

**Read this first.** It is the short document: where things are, how to build,
what will bite you. [WW_CHANGES.md](WW_CHANGES.md) is the detailed history,
[WW_FEATURES.md](WW_FEATURES.md) is what the fork adds,
[docs/TO_BE_IMPLEMENTED.md](docs/TO_BE_IMPLEMENTED.md) is the single backlog, and
[docs/MISTAKES.md](docs/MISTAKES.md) is what went wrong and what stops it
happening again.

**Working directory:** `E:\Projects\NifskopeWildWastelandEdition`
(GitHub: [BungoV/NifSkope-WildWastelandEdition](https://github.com/BungoV/NifSkope-WildWastelandEdition),
branch `main`, `origin` is the fork — never push upstream.)

Updated **2026-08-22**. Edition **0.3.2**. Build green. The current work is a
sweep of the **compiled-collision backlog**, and as of 2026-08-21 it is being
tested IN THE GAME, which changed what the work is.

## The live test, and what it found (2026-08-22)

`E:\Projects\Fallout 4 Mods\mods\WW Concord Collision Test` is 114 meshes — every
reference in ConcordMuseum01 the game must open a NIF for — rebuilt by our own
writer. It rebuilds in five minutes with three parallel workers
(`rebuild.sh` + `MANIFEST=` per worker), which is the point: an earlier
1,619-mesh build took an hour, and an hour is the wrong unit of work when one
mistake repeats it.

**STATE 2026-08-22: IT WORKS IN THE GAME.** bungo confirmed a door running our
recompiled collision -- the blob differs from vanilla's, so it is genuinely ours
-- loads, opens and closes. That is the whole round trip proven in the engine on
an ANIMATED object: decompile, recompile, load, activate. The Museum set's solids
are proven too (110 of 114 identical to vanilla within 0.46 mm,
`tools/collision_ab.py`), every file has exactly one physics system, and all 22
keyframed records carry the inertia sentinel.

## NEXT SESSION STARTS HERE: multi-body systems, and body order with it (2026-08-23)

**Item 3c is CLOSED IN GAME.** bungo re-tested the wall terminal after the unit
fix and its collision is where it belongs, which validates three commits at once:
the mesh SHAPE builder (`c873b7b`), mixed compounds (`00dbc65`), and the
Havok-translation fix (`e5ab899`). Nothing in the Museum set is unverified any
more.

**What is left, and it is one piece of work rather than two:**

  1. **The ragdoll's own skeleton, which is the last piece.** Multi-body compile
     landed 2026-08-23 (WW_CHANGES 2026-08-23h): `Havok/Compile All Collision`
     captures every joint, compiles every body and merges them into ONE
     `bhkPhysicsSystem` with the joints rebound -- 155 of 155 corpus files, 1202
     joints in and 1202 out, and the Brahmin skeleton comes back as 41 bodies and
     38 joints with every capsule still a capsule.

     What is left for a ragdoll specifically:

       * `hkaSkeleton`, and it needs NO carrier -- measured 2026-08-23, see
         WW_CHANGES 2026-08-23i and `collision <file> --skeleton`. The reference
         pose derives from the BODIES (their `cinfo +0x30` / `+0x40`, already
         carried as `bhkRigidBody`'s Center and Rotation): **850 of 850 bones,
         worst 3.1e-06 m**. NOT from the node transforms, which the handoff used
         to guess -- those miss on 16 files and miss hard, every bone of the
         standing turret and the Vertibird's wings by 2.03 m, because those NIFs
         are authored in a display pose. The rest is rules, all measured with no
         exceptions: bone tree = the joint graph (bones == joints + 1, 75/75),
         `lockTranslation` = parent >= 0 (925/925), reference scale = 1.0 on the
         root and 0.99999994 elsewhere (as BIT PATTERNS -- both print "1.0000"),
         pose scale w = 1.0f on the root and 0 elsewhere. Only the pose
         translation w lane is not derivable, and `--roundtrip` already treats it
         as inert.
       * the ROOT CLASS. Compile All writes `hknpPhysicsSystemData`; a ragdoll
         needs `hknpRagdollData`, which the encoder already writes
         (`hknpEncodeRagdoll`) and which needs the skeleton above.
       * a skeleton NIF holds TWO packfiles -- the ragdoll and a character bumper
         -- where Compile All writes one. Every other corpus file has exactly
         one, so this is a ragdoll-only exception rather than a rule to relax.

**The Museum set now stands at:** shape classes 114/114 vs vanilla, stored solids
113/114 (one known face-decomposition difference), compounds 37/37, body rest
state 0 differing, shape header words 1 differing, body order 46/46 on vanilla's
own rule. Everything that remains is documented and deliberate.

**Done and CONFIRMED IN GAME since the railings closed:**

  * **The mesh SHAPE builder is out** (`c873b7b`, WW_CHANGES 2026-08-23a).
    `hknpEncodeMeshShapeObjects` emits the four objects of a compressed mesh as
    self-contained pack objects; `hknpEncodeCompressedMesh` is the system half
    plus a splice. Byte-identical output on 114 of 114, twice.
  * **`encodeShapeObject` can BUILD a mesh**, not only copy one back -- the
    branch that unblocks everything below.
  * **Mixed compounds, item 3c, closed** (`00dbc65`, WW_CHANGES 2026-08-23b).
    Shape classes match vanilla on 114 of 114; stored solids 110 -> 113;
    compounds 34 -> 37 of 37.
  * **A mesh leaf under a transform was 18 game units out** (`e5ab899`,
    WW_CHANGES 2026-08-23c). Older than mixed compounds and true of every such
    mesh; flattening hid it. Ten green checks passed while it was wrong, because
    all of them measured what the collision IS and none measured WHERE.

**What the engine reads to choose an impact sound** (1.10.155 RVAs; the full
derivation is WW_CHANGES 2026-08-22j):

    FOCollisionListener::OnContactImpulseEvent          @0x630c60
        matA = bhkUtilFunctions::GetMaterialForShape(bodyA->m_shape, keyA)
        matB = ... bodyB ...
        BGSImpactManager::ProcessEvent({matA, matB, contactPos, velocity})
    bhkUtilFunctions::GetMaterialForShape              @0x1d8c300
        key == 0xffffffff -> shape+0x18; !(shape+0x10 & 4) -> 0; else the leaf
    BGSImpactManager::ProcessEvent                     @0xd19e00
        BOTH materials must resolve -- either null and BOTH surfaces go silent

**We never reach it.** The event is named after the flag that raises it.
`hknpBody::m_flags` bit 7 is `RAISE_CONTACT_IMPULSE_EVENTS`, named by the
engine's own debug printer `NVFlex::printHknpBodyInfo` @0x27afa4, and it arrives
from `hknpBodyCinfo::flags` at cinfo +0x18: `hknpBody::initialize` @0x14daaf0
copies that word through untouched apart from the low four bits.

Vanilla sets it on **1,408 of 1,408 dynamic bodies** and on none of the 12,456
static or keyframed ones (13,889 bodies, 11,820 files). Our Museum rebuild sets
it on **0 of 170**. The writer already writes the field and the decoder already
reads it; what is missing is the middle -- Decompile has no `bhkRigidBody` field
to put it in, so Compile starts from a default-constructed `HknpBodyPhys`, whose
`cinfoFlags` is 0.

**How it is written now:** `RAISE_CONTACT_IMPULSE_EVENTS` is DERIVED from
`in.dynamic`; `USER_FLAG_0` and `RAISE_TRIGGER_EVENTS` are CARRIED on
`bhkRigidBody`'s "Body Flags" bits 1 and 2, because neither follows from anything
we model. Those four values are all the corpus uses, and the rule reproduces all
13,889. `tools/hkbodyflags.py` checks it and is a gate in
`tools/rebuild_collision.sh`.

**Ruled out for good, and why the earlier reading was wrong:** the shape header
at +0x18 is only the FALLBACK for a composite shape, which is why hand-patching
it changed nothing. And the "per-body material words `000000ff` / `000100ff`,
whose high u16 is just the body's own index" are two named fields, `qualityId`
(u8 at +0x10) and `materialId` (u16 at +0x12); ours and vanilla's agree on both,
so the permuted body order was never a difference there.

**A method note worth keeping:** the exe carries Havok's own `hkClass` reflection
tables -- `<Class>Class_Members`, const arrays naming every field and its offset.
`hknpBodyCinfo`, `hknpBody`, `hknpMotionCinfo` and `hknpPhysicsSystemData` all
came out in a single query. Read those before deriving a layout by hand.

Both items that used to be here are accounted for above: mixed compounds CLOSED
on 2026-08-23, and the enabling change they shared -- the compressed-mesh SHAPE
builder pulled out of `hknpEncodeCompressedMesh` -- done with it. What is left is
multi-body systems and body order, both listed at the top.

**Four crashes and failures, none of which any check we owned could see:**

1. A compound's BVH shipped with **no pointer to it** — the local fixup at
   `+0x10 -> +0x40` was never emitted. `hknpDynamicCompoundShape::updateAabb`
   read null.
2. Compound AABBs were unioned from the children's **vertex lists**, and a
   capsule has none. Bounds stopped short, and an all-capsule body bounded
   nothing, was refused, and fell through to the triangle path.
3. Every compound wrote the placeholder `0x01000001` at +0x10. **Bit 0 of that
   word is the engine's "I am convex" flag**, so a compound was handed to
   `hknpScaledConvexShapeBase::calcAabb` on the first scaled reference.
4. Doors would not open. There is a **KEYFRAMED** body state between static and
   dynamic -- inertia record and a motion INDEX, no dyn_motion record, 170 of
   1,200 vanilla files and every one of them something the game moves -- and
   Compile refused it, in a guard that named the case. A static body cannot be
   driven by an animation however right its collision is. The body's own
   position (its hinge) and orientation were being dropped too.

**The rule those three teach:** our own reader and writer agreeing is ONE
measurement, not two. `--roundtrip` was byte-exact through all three. Use the
two external authorities — **Elric says what the tool WRITES**
(`tools/elric_pair.sh`), **the PDB says what the engine READS**
(`Fo4PDB` + `f4pdb.py` in the FO4CS repo; `asConvexShape` is four instructions
and settles a question a corpus histogram cannot).

Tools that now do this: `tools/hkcompound.py` (follows the pointer or fails;
`--flags` decodes the header word; `--aabb` prints the bound; `--damage`
reproduces the crash so the check is proved able to fail),
`tools/hkcompound_sweep.py` (holds a whole rebuild against a vanilla tree), and
`tools/fo4_crash_triage.sh` (reads the newest Addictol log and names the mesh).

(Elric IS installed, at `X:\Programs\Steam\steamapps\common\Fallout 4 1946160\Tools\Elric`
— an earlier note in these files said otherwise, from a search that covered only
C: and E:. `Fallout4.esm` is a DIFFERENT folder: `...\common\Fallout 4\Data`.)

- **Multi-material collision, both ways** — a body made of parts keeps every
  part's material through Compile, and Decompile splits a mesh back into one
  shape per material instead of throwing the rest away, so the round trip is
  closed. The CMSD run table that carries them is decoded (below).
- **Friction and restitution round** into their stored word instead of
  truncating. 0.4 is the Fallout 4 default restitution, so every body we compiled
  was one ULP low.
- **triangleIsInterior** is measured, not guessed: fully-edge-shared is a
  necessary condition, exactly, on 3,037 of 3,037 set bits. Still zero, and zero
  is the safe direction.
- **A convex source compiles to a convex shape** — box, hull, sphere, capsule —
  instead of a triangle mesh, and several of them in one body compile to a
  COMPOUND, whose BVH is decoded (86 of 86 vanilla compounds fit it).
- **Create Collision adds beside** rather than deleting the shape that is there.

Under that sits the rest of **compiled collision**, below, including the Elric
campaign that decoded both hkcd trees and lifted the 128-triangle cap.

### A convex source compiles to a convex shape (2026-08-20)

Compile had one output, `hknpCompressedMeshShape`, so a box went in and a
triangle soup came out. The class follows the SOURCE — 228 static systems in the
corpus are polytope-only — so this is not the dynamic-only concern the backlog
filed. What a future session needs:

- A polytope's mass properties describe the hull GROWN by its convex radius.
  Volume is the Minkowski sum (within 2% on 271 of 299), inertia is that solid's
  approximated by growing the hull's bounding box by r (within 15% on 255 of
  268), and the stored tensor is 1.5× the physical one.
- The major-axis frame at massProperties+0x20 is still undecoded, and does not
  need to be: 76.8% of vanilla carries `00 80 00 80 00 80 30 f5`, and a
  synthesized shape takes that.
- **Compounds are written too, since 2026-08-21.** `hknpDynamicCompoundShapeData`
  is `0x60 + 2n × 32`: 2n-1 depth-first BVH nodes and one zero record, a node
  being `float3 min | u32 0x3f000000|(parent+1) | float3 max | u16 leftChild+1 or
  0 | u16 rightChild+1 or the instance index`. Left children are implicit (always
  the next record), so only the right link carries information. 86 of 86 vanilla
  compounds fit it. Note `hknpStaticCompoundShape` is a class Elric never writes —
  all 71 corpus compounds are dynamic, 45 of them in bodies that do not simulate —
  so its type hash is not in our table and does not need to be.

### Compile keeps every material (2026-08-20)

Compile used to write ONE material per packfile — whichever the first leaf shape
held — so a body assembled from parts came out uniform. Two structures carry the
rest, and both are decoded now against the vanilla corpus (2,490 meshes, 3,898
sections, 9,536 run records):

- **hknpBSMaterialProperties' entry stride is 0x18**, not the 0x20 a
  single-entry table cannot be distinguished from. Object = 0x20 header +
  0x18 × n; each entry is a 1 at +0x10 and the CRC at +0x14.
- **The CMSD run table at +0xa0** is 4-byte records
  `[u8 material][u8 0][u8 firstPrimitive][u8 count]`, and the start is
  SECTION-RELATIVE: every section's runs begin at primitive 0 and cover its own
  primitive count exactly.
- **Section +0x54 = `(firstRun << 8) | runCount`**, the same packing as +0x50's
  primitives. The literal `1` that used to sit there pointed EVERY section at
  run 0 — a real bug in every multi-section packfile we had written, not just an
  omission.
- Identical run blocks are shared between sections; first-use dedup reproduces
  2,472 of 2,490 vanilla tables byte for byte.

What a future session needs to know:

- `tools/hkmatrun.py` checks those invariants by parsing the packfile itself, so
  it fails on a writer bug NifSkope's own decoder would agree with. `--damage`
  makes a copy with the old layout, which is how the harness proves the check
  can fail.
- `tests/spells/collision_materials.sh` is the guard (11 checks). The one that
  matters is the SWAP: exchange the two source shapes' materials and the run
  order has to exchange with them. Counting table entries cannot see a writer
  that assigns materials by position.
- **Still flattened, and it is the bigger half now**: a single compiled mesh
  holding several materials decompiles to ONE editable shape, so vanilla →
  decompile → compile still loses them. 13.5% of SetDressing meshes are
  multi-material. See item 0b in docs/TO_BE_IMPLEMENTED.md.

### Compiled collision no longer writes half the format (2026-08-16)

Collision compiled by NifSkope crashed Fallout 4. The compile path
(`hknpEncodeCompressedMesh`, which is NOT the assembler that reproduces 810 of
822 stock systems) was leaving out the traversal tree each section carries, both
of the shape's hkBitFields, three CMSD members, and the material table's own
pointer — and it wrote a 0x40-byte dynamic-inertia record where the stride is
0x70. All of it is measured against a 41-shape vanilla corpus; the table of
every field is in [WW_CHANGES.md](WW_CHANGES.md) under 2026-08-16.

What a future session needs to know:

- **The mesh tree's codec** (decoded here, confirmed on all 63 corpus sections):
  byte 3 bit 0 set = internal, right child at `self + (byte3 & 0xfe)`, left child
  next; clear = leaf, primitive `byte3 >> 1`, section-relative. `2n-1` nodes.
  The leaf index is a byte, which is why **a section holds at most 128
  primitives** — this partitioned at 255, so half of a full section was
  unreachable.
- **The section tree at CMSD +0x10 is decoded too (2026-08-17, via Elric
  reference pairs), so multi-section works to 511 sections** — no triangle cap
  short of the 65,535-vertex refusal. FIVE bytes a node: 3 bound bytes + u16;
  `data & 0x80` internal with the high byte the left subtree's leaf count,
  else leaf with the high byte the SECTION INDEX. Both trees' bound nibbles
  are `floor(sqrt(inset/parentSpan) * 15)`, hierarchical against the parent's
  DEQUANTIZED box — sqrt, not linear, which is what the 08-16 "89% clipping"
  measurement was actually seeing.
- **The Elric harness**: copy `Settings\PCMeshes.esf`, embed absolute
  ConvertTarget/OutputDirectory, set CloseWhenFinished, put fixtures under a
  path containing `Meshes\`, launch `Elrich.exe <esf>` on the desktop
  (unsandboxed) — runs and exits with zero clicks. Fixtures = decompiled
  vanilla meshes perturbed with `nifskope-cli set`; Elric is deterministic, so
  one changed vertex diffs to a handful of annotated bytes. It STRIPS
  already-compiled collision (vanilla's too), so it is a pair machine, not a
  load oracle.
- **The guard that says the writer agrees with itself** is
  `nifskope-cli collision <nif> --roundtrip`: a fresh compile must come back
  `byte-exact 1 / 1`. It did not before this, and that is what found the
  class-name ordering and the fixup ordering.
- Open, none blocking (details in
  [docs/TO_BE_IMPLEMENTED.md](docs/TO_BE_IMPLEMENTED.md)): triangleIsInterior
  semantics (zero is safe), quad pairing (would double section capacity),
  convex dynamic sources as polytopes, friction f16 round-vs-truncate.
- **Not validated in game.** Every claim here is against vanilla files and
  Elric pairs. Whether Fallout 4 accepts the result is the test that counts
  and it is bungo's.

The work before this is the **overnight cleanup wave** (below), on top of the
scene-composition pipeline and SAM import from 2026-08-10.

### The overnight wave (2026-08-11, commits 21b04c2…7b37226)

Run under an overseer-verifier model with parallel agents; every item
user-directed:

- **Per-glyph tooltips** on the Loaded-NIFs strip + Block List Vis column,
  state-aware, same rects as the clicks (`21b04c2`).
- **SAM pose EXPORT** (`0caf20b`): exact inverse of the import (toEuler ==
  SAM's MatrixToEulerYPR incl. both gimbal branches), six decimals, structural
  bone-set rule (96 keys covering the corpus's 89); round trip is one check of
  five in sam_pose_import.sh phase 3.
- **Merge polish** (`0caf20b`): flash placement is extent-aware (bounds, not
  pivots — bare minigun lights at P-FlashShort, not 56 units of air); a C-less
  donor that publishes points is a gun, not a flash; base-on-bone Receiver
  note silenced. OMOD assembler DROPPED per user; follow-specific-row resolved
  by design (single active skeleton).
- **Refraction actually works** (`b9dfcb0`): sequences bind data-less
  NiBlend*Interpolators (never serialized) — strength was frozen at 0.0
  forever; data-less blends now resolve to the controller's authored
  interpolator. Shader: normal.xy × strength × vertex alpha, 5% viewport
  height cap. refraction.sh (21 checks) walks the controller's own ramp
  against closed-form values; 6-of-7 A/B corpus byte-identical.
  render_regression baselines remain stale (pre-existing) — re-baseline
  pending.
- **The flat-list heap overflow** fixed (`2b0635d`, see the corrected section
  below) and **poselib** measuring the real invariant.
- **winpath consolidated** into _harness.sh, pure bash (`7b37226`); eight
  scripts flagged (pre-existing) for placing GUI windows without
  WW_WINDOW_AT.
- **Process rules now standing**: one NifSkope instance at a time
  (.harness.lock mkdir-mutex around every launch), all harness windows on the
  second monitor, never `git stash` in a shared tree (an agent's stash swept
  three others' work mid-run; recovered, stash preserved then retired), and
  suites whose fixtures build under /tmp cannot be verified from a sandboxed
  Git-Bash tool shell — the native exe cannot write C:\msys64\tmp there; use
  a PowerShell-launched login shell.

### The scene-composition pipeline (2026-08-10, commits f6b0285…8aed8d8)

One user-specified workflow, shipped in four verified increments:
**load → mark → snap → pose → flatten.**

- **Weapon mark + pose-follow mark** on Loaded-NIF rows: always-visible
  one-click toggles (five-slot glyph strip; the gesture suite caught a shipped
  slide-off select/drag bug on the eye/disc and fixed it for all four).
- **Weapon parts snap by BSConnectPoint data** (`nifmerge.cpp`): donor
  `C-<slot>` (any-of, case-insensitive incl. vanilla's `C-Reciever`) vs the
  assembly's published `P-<slot>` points, full transform composed via a
  per-part wrapper NiNode (10mm P-Mag cant 26.52° asserted). Muzzle flashes
  declare NO connect points and take the END of the barrel chain
  (P-ProjectileNode > P-Muzzle > P-Flash* > P-Barrel > bone+note). No
  whitelists anywhere — validation is purely node require/provide, per the
  user's explicit design. Reference DB from the OMOD/connect-point research is
  archived in the session scratchpad (`weapon_combos.json` + `.md`).
- **Pose-follow** renders a marked document against the loaded skeleton's
  bones by name via `Scene::skeletonOverride`, gated per document —
  non-destructive (follower file asserted byte-identical while its rendered
  vertex tracks the posed bone at constant grip distance).
- **Flatten** (`flattenWorkspaceToDocument(bakePose)`): live scene → one NIF,
  pose baked (exact, disk-round-tripped) or Pose-Manager-captured rest; no
  false choice when no rest exists. Flatten results are excluded as future
  flatten sources (self-feeding bug caught by the suite).
- Suites: `flatten.sh` 28/28, `weapon_mark.sh` 77/77, `loaded_nifs.sh`
  112/112, plus the art-object/particle gate the user mandated
  (`artobject_attach` 14/14, `carries_everything` 24/24, `live_effects`
  15/15) — effect NIFs merge 1:1 and render pixel-identical when unmarked.
- **Increment 5 + icons (3c8b243…8362c27):** the skull became a strip toggle
  (STRICTLY single-active — one pointer of state shared with merge targeting
  and pose-follow resolution), a divider separates role marks from display
  toggles, and the glyphs are skull / user-drawn pistol (embedded verbatim as
  an alpha mask in `weaponMarkPixmap` — re-embed from a PNG via binary
  splice, never redraw over it) / symmetric dog-bone.
  `release/ww_icon_sheet.png` (from `renderMarkIconSheet`, regenerated by
  `loaded_nifs.sh`) is the approval artifact for glyph changes; pixel-only
  changes need only that one run.
- Open cosmetic: a gun merged after Frame.nif reports "nothing publishes
  P-Receiver" (true, harmless) — `offersConnectPoints` is the knob. Bare
  minigun flash takes P-FlashFar (farthest-wins reading); a C-less second
  gun would chain-end. All three stated in merge summaries when they occur.

### This session (2026-08-11c) — Block List visibility

**H / Alt+H work in the Block List, and the Summary column is now the eye and
the see-through disc.** Details in [WW_CHANGES.md](WW_CHANGES.md); what a future
session needs to know:

- **There is ONE hidden set and it is `Scene::hiddenNodes`** (block numbers,
  session-only, never written to the NIF, subtree by `Node::isHidden`'s
  parent-chain walk). The viewport's H, the Block List's H, both context menus
  and the row's eye all reach it through `GLView::hideSelected` /
  `setBlockHidden`. Do not add a second one.
- **`hideSelected()` reads `objSelection` now**, so it is multi-selection aware
  on both surfaces. The old "Hide This" label described the old behaviour
  honestly and is gone.
- **Per-block see-through is `Scene::ghostNodes` + `Node::isGhosted()`, rendered
  by the X-ray blend in `BSShape::drawShape`.** The Loaded-NIFs per-document
  see-through cannot be reused: a ghosted document there is a flat triangle
  soup, not a Scene. Like the global X-ray this covers `BSShape` only, which is
  every shape in a Fallout 4 file.
- **The column REUSED slot 11** (`WwSummaryCol` → `WwVisCol`) rather than adding
  a thirteenth. That is deliberate and load-bearing: which columns the Block List
  hides is exactly what sends `QHeaderView`'s running total negative. If you ever
  do need to change the set, it goes through
  `wwReleaseBlockListColumns`/`wwApplyBlockListColumns` and each mode's own blob.
- **The eye and the disc are drawn once**, in `src/ui/wwglyphs.h`. Three callers:
  the Loaded-NIFs row strip, the Block List column, and `renderMarkIconSheet`.
  The sheet used to redraw them by hand and had already drifted — do not put a
  fourth copy anywhere.
- **A row gets toggles iff the scene can resolve it to a drawable of its own**:
  top item, real block number, inherits `NiAVObject`. It does not apply the
  key's promote-to-owning-shape rule, or one object's eye would appear on two
  rows.
- `block_visibility.sh` **74/74** across both modes. It replaced `WW_SUMMARY_TEST`.

### The session before (2026-08-10)

**The Pose Manager imports Screen Archer Menu poses (`4e36bbb`).** SAM `.json`
poses (a Discord request, 80 real PA pose files as corpus) are **absolute**
local transforms — not rest-relative deltas like OS poses — with rotation
`Rx(yaw)·Ry(pitch)·Rz(roll)` in degrees, which is element-for-element
`Matrix::fromEuler`. The convention was proven against the PA skeleton rest
pose (576 candidates eliminated, median matrix error 7.7e-08) and cross-checked
against SAM's own source (`SAF/conversions.cpp` — beware its
`MatrixFromDegree`/`…Transposed` path, which is the skeleton-adjust route and
yields the inverse rotation). `AnimSetup::applySamPose` parses everything
before touching the model, applies in one `nifSnapshotOp`, blends from the
CURRENT transform toward the pose (slerp), and uses `Transform::writeBack` so
quat-rotation nodes and Scale (which SAM carries, `0.0` = hide trick) work.
Merge re-apply dispatches `.json`/`.xml` on the active pose. Values are JSON
strings; missing bones are non-fatal (5 of 80 real files omit 17 armour-piece
bones). `sam_pose_import.sh` is green incl. a hand-coded expected matrix
(5.96e-08) and a real-corpus check (89/89 bones, Back_Armor rot diff 8.7e-08).
No SAM *export* — if added, write 6-decimal angles (SAM's own `%.02f` costs
~0.005°). Format research archived at the session scratchpad's
`sam_convention.md`.

**The first visual pass shipped a crumple and called it a pose (`a4e848a`
fixed it).** Frame.nif — like every skinned mesh — carries a FLAT copy of the
bone names off its root; SAM values are parent-space, so posing it directly is
garbage, and the phase-2 pixel-delta check passed it because a crumple moves
pixels beautifully. The importer now refuses flat targets (0 of 55 parented)
with an explanation naming the skeleton-merge workflow, sharing the rig
merge's `hasWorkspaceBoneHierarchy` test (now `AnimSetup::hasBoneHierarchy`).
The photographed path is the supported one: skeleton primary + frame
rig-merged, then posed — verified by composing WORLD transforms down the
parent chain independently in the harness (depths 2/9/12, worst 9.8e-6; a
chain-ignoring control misses by 113+ units). `sam_pose_import.sh` PASS ×3,
`workspace_skeleton_target.sh` 34/34. Lesson recorded twice this session:
**a pixel delta proves motion, never correctness** — and the Edit tool
flattens MIXED-ending files (`nifskope.cpp`, `WW_CHANGES.md`); binary splice
is the only safe route there.

### Window-state diagnostic cleanup safety

`window_state_roundtrip.sh` had an unsafe failure path: its `cmd /c` cleanup
discarded the exit status from deleting/importing the NifSkope registry tree and
then removed the backup unconditionally. A failed run left the real profile with
the test-only values `New Document Cube=0` and `Suppress Save Confirmation=1`.
Both user values were restored to their safe defaults (`1` and `0`), and the
harness now checks direct `reg.exe` process exit codes and retains/reports the
backup on any restore failure. Do not reintroduce cleanup that can erase the
only snapshot without proving the import succeeded. (`New Document Cube` no
longer exists — the starter cube was removed 2026-08-11b and the preference with
it. The incident stays written down because the restore logic is what it bought.)

### This session

One structural UI pass, closed end to end: the left editor is now one permanent
three-mode column instead of four tabified docks.

**X-01 refraction no longer becomes a giant dark silhouette.** The stock VFX
animates strength to 1.0; the preview's old 0.12 viewport-relative multiplier
turned its normal map into roughly 200-pixel jumps. Distortion is still driven
by that normal map, now capped at eight screen pixels at every resolution.
Multiple Loaded NIFs also collect all opaque geometry before one shared
transparent/refraction pass, so a refractive document can copy geometry behind
it from another Loaded NIF. Single-NIF render captures are unchanged except for
the intended refraction case; the X-01 `autoLoop` peak was captured on/off, the
release build is green, and `workspace_skeleton_target.sh` remains **34/34**.

**Explorer-to-NifSkope `.nif` drops now work across the whole window.** The
application-level route recognizes the native OpenGL container as well as the
specialist tree views, then offers an explicit adaptive workspace action, new
window(s), or Cancel. Adaptive replaces only the clean, untitled, sole starter;
otherwise it preserves the primary and adds every file to Loaded NIFs. A
multi-file starter drop opens the first file and enrolls the rest. The choice
menu opens only after the native drag loop has released the pointer. Its compact
form has no heading separator or file icon and labels the starter action simply
**Open Here**. A real edit of the starter is part of the replacement guard
harness — it renames block 0 and requires the document to stop being eligible,
which is one of the reasons the starter still has a root node now that the cube
is gone (2026-08-11b). The release build is green; `external_nif_drop.sh` is
**17/17** and `loaded_nifs.sh` remains **95/95**. No physical pointer-driving
test was run.

**The skull marker is optional for Loaded-NIF merges.** Clothing and props still
merge normally with no skeleton selected, using the clicked row as target; a
marked skeleton elsewhere in Loaded NIFs does not interfere. If the marked model
is included in the selection, it is intentional rig-merge input and becomes the
target automatically. NifSkope first requires a real NiNode hierarchy below the
file root, so flat bone-reference nodes in a frame or clothing NIF are refused
with a useful explanation rather than mistaken for a skeleton. The real-corpus
`workspace_skeleton_target.sh` passes **34/34** across all four cases, and
`loaded_nifs.sh` remains **95/95**.

**Loaded NIFs → Make Primary / Edit no longer reloads the application window.**
The old route created a second hidden `NifSkope`, restored its complete UI, then
showed it and hid the current window. It now shares the established in-place
swap route with the direct edit gesture: the live row bytes replace the content
of the existing primary `NifModel`, rebuilding only the scene. The same main
window, viewport, dock, page stack, Loaded model, active mode and splitter sizes
survive. Skeleton and face-donor marks follow the promoted mesh. The release
build is green and `loaded_nifs.sh` is **95/95**, including exact object-identity,
window-count, geometry and role-remapping assertions over the real Make Primary
entry point.

The compressed-width migration gap is closed. Schema 1 had already saved
`LeftColumnDock` at Qt’s incidental ~260 px content hint, so the old “new dock”
400 px initialization no longer ran. Schema 2 now requests **400 px once**,
after state and geometry replay, then permanently returns width ownership to the
user. The real schema-1 profile measured **400 px** on launch and retained the
existing **164 → 432 px** fold/unfold range; the capture was inspected and
`loaded_nifs.sh` remains **93/93**.

The top **Blocks · Header · NIFs** selector is now a full-width, equal-third
segmented control in selection blue. It shares its skin-backed geometry with
Collision Creation / Simulation: only the two outside ends are rounded, and
each square internal join has one border rather than two beveled corners.
Verified in the release build with `loaded_nifs.sh` **93/93** and
`collision_panel.sh` **41/41**; both generated captures were inspected.

- **Blocks** shows Block List above Block Details.
- **Header** gives the Header the full column.
- **NIFs** shows NIF Browser above Loaded NIFs.
- The buttons are ordered **Blocks · Header · NIFs**. Tab positions carry an
  explicit stable-mode ID, so moving NIFs from the second to the third button
  does not reinterpret an existing saved NIF mode as Header.
- The selector is the first row at the top. Switching it changes only a
  `QStackedWidget` page: the views, models, selection, searches, splitter sizes
  and unsaved Loaded NIFs stay alive in place.
- The old four dock shells are consumed before `restoreUi()`, then deleted. The
  new `LeftColumnDock` is the only core dock Qt ever restores, so no live widget
  is reparented after it enters the saved dock graph.
- Saved-window state is version `0x074`. Existing `0x073` layouts get one
  compatibility replay for unrelated docks/toolbars; mode and both inner
  splitters then persist explicitly under `UI/LeftColumn`.
- The column still folds from **164 to 432 px** in the harness while preserving
  the viewport's 50 px minimum.
- **Inactive pages cannot paint through.** The legacy visibility reset now runs
  before the content widgets enter `QStackedWidget`; doing it afterwards had
  overridden Header's hidden state and exposed its Type column as a vertical
  strip of characters along the viewport divider. The mode harness now requires
  exactly one page to be visible and the screenshots were checked again.

- **The selector and both Block panels were compacted and clarified.** Blocks,
  Header and NIFs use a flat orange-underlined selector with no shortcuts. Block
  List has one toolbar plus an advanced Filters dropdown, accurate
  Block/Name/Vis columns, navigable breadcrumbs, cached totals and a clear
  no-results state. Block Details has one search/pin/overflow row and explicit
  no-selection, no-match and no-pins states.
- **Header is now a standalone file inspector.** It shows source identity and
  NIF/User/Bethesda versions, recursively searches Name/Value/Type without
  replacing its model/root, exposes copy-summary/path actions, and retains the
  useful Type column while folding with the unified dock.

- **Eye and transparency clicks no longer select the row.** The Loaded-NIF view
  owns the full press/release gesture over those glyphs, so no orange/blue
  selection flash appears and no drag begins accidentally.
- **Loaded NIFs has its own search field and real vertical scrolling.** Filtering
  hides source rows without proxy-remapping their drag/action identity. A
  40-row harness probe proves the scrollbar gets a non-zero range.
- **Loaded NIFs reports its live membership.** Its header says total or
  shown/total, carries a glyph legend, and row tooltips add source/unsaved state.
  Empty and no-match states are passive paint. Browser Refresh and filtering are
  now required to preserve the exact Loaded model pointers and persistent rows.
  The 4 px vertical splitter stays non-collapsible and keeps both panes reachable.
- **The row menu is grouped by intent.** File actions, rigging, workspace
  display, tools/revert and removal are separated; skeleton and face-donor use
  the same skull/face icons as the row; duplicate Close/Remove wording is gone.
- **The nearly immovable left divider was a hard 400×240 dock minimum.** Those
  List/Tree dock floors are gone. The tested browser column now travels from
  164 to 432 px while preserving the viewport's 50 px minimum. Collision,
  Rigging and Vertex Paint expose horizontal scrollbars when folded; UV no
  longer adds a redundant 340 px dock floor. Genuine content floors (UV render
  view and timeline graph/lane) remain.
- **Verified:** staged release build green; `loaded_nifs.sh` **91/91**;
  `WW_DOCKS_TEST` **13/13**; `archive_browse_survives_load.sh` **4/4**; and the
  final populated/no-match screenshots were visually checked. The earlier
  `collision_panel.sh` **39/39** and two-cycle window-state pass are unchanged.

### Open

- **`WW_POSELIB_TEST` fails at its final Delete step, and it is PRE-EXISTING.**
  `findPose()` returns null after the Apply click's refresh
  (`nifskope_ui.cpp` ~3410). Verified unrelated to the SAM import: the log is
  byte-identical with all of that commit's `src/` changes stashed. Diagnose
  the dock refresh vs. the harness lookup before trusting this harness again.
- **`block_drag_live.ps1` has not been run since the multi-parent payload
  change.** It seizes the mouse; ask first, every time.
- The NIF Browser harness covers the real view gates, exact captured payloads,
  both save/load routes and rendered geometry, but no pointer-seizing live mouse
  script was run.
- An auxiliary re-run of `window_state_roundtrip.sh` did not reach its restore
  assertion: both the prior canonical binary and the staged binary stayed open
  after cycle 1's `CloseMainWindow()`. The script restored the registry profile
  each time. This did not reproduce in the dock or Loaded-NIF harnesses and is
  not attributed to the left-panel change; diagnose the close harness separately
  before claiming a fresh two-cycle pass.
- The flat-list **hang** below is still open and still harness-only.
- Everything else in this file's later sections is carried forward and untouched.

Two headlines:

- **The Block List is a direct-manipulation panel now** — drag to re-parent,
  reorder and un-parent; paste follows the pointer *in every window*; a click on
  blank space selects nothing. Details below.
- **The flat list mode is fixed and kept.** It was worse than filed — *no* row in
  it could be clicked, dropped on or right-clicked, not just newly inserted ones
  — and the cause was `QHeaderView`'s cached total going negative, not anything
  about the model. Hierarchy mode was never affected.
- **CLOSED 2026-08-11 (`2b0635d`): the "flat-list hang" was never a hang — it
  was a heap overflow, and this section's lore was wrong on both counts.**
  `QTreeView::expandAll()` emits `expanded()` from inside
  `QTreeViewPrivate::layout()`; `NifTreeView::scrollExpand` answered it
  synchronously with `scrollTo()`, whose `doItemsLayout()` cleared and re-laid
  `viewItems` under the outer layout's feet — which then wrote past the
  reallocated buffer. The process was already DEAD while the script waited out
  its 63-second deadline (that is why it read as a hang), and the Application
  log DOES carry APPCRASH records (`0xc0000005` then `0xc000041d`) — the "no
  APPCRASH" claim above was simply wrong. Under gdb's debug heap the ~50%
  becomes 4-in-4 with "Heap block modified past requested size". The scroll is
  now posted (QueuedConnection, QPersistentModelIndex), coalesced per burst,
  cancelled by explicit `scrollTo()`. 12/12 green with list mode back in
  `block_rename.sh`'s gate; `collision_drop.sh`'s "stall" was this same crash
  taking the process down mid-suite (10/10 now). Lesson for the next
  mystery: **a "hang" whose process cannot be attached to may be a corpse —
  check the process is alive before reaching for deadlock theories.**

### The Block List, as it now behaves

| gesture | result |
|---|---|
| drop **on** a `NiNode` | re-parent into it, **preserving world position** |
| **Shift** + drop | re-parent keeping the LOCAL transform |
| **Ctrl** + drop | link — adds the child link and keeps the old parent |
| drop in the **gap** between two rows | reorder to that position in the parent's `Children` |
| drop in the **blank space**, or the gap beside a top-level row | **out** — loses every parent, becomes a root of its own |
| hover a node that would accept the block | it unfolds after ~650ms, and folds back when the drag ends |
| pointer near the top/bottom edge | the list auto-scrolls |
| **Ctrl+V** over a row / over blank space | pastes into that row / with no parent |
| click blank space | selects nothing at all |

A row that **cannot take children is all gap** — there is nothing to drop inside
a mesh, so its whole height reorders. Only a `NiNode` keeps the
third/middle/third split.

`wwReparentBlocks` in `blocks.cpp` is the one primitive, shared with the
Collision Manager's Set Parent. `release/ww_drag.log` records the most recent
drag with no flag to set (`WW_DRAG_LOG=off`, or a path, overrides).

### Where to pick up

*(From the 2026-08-07 block-list sessions. Merging collision shapes in the
Collision Manager, listed here as unstarted, shipped in `WW_CHANGES.md` 08-07zb.)*

The four things that handoff listed are all closed — three fixed, one measured
and deliberately not done. What is left of them:

1. **Drag-and-drop has no coverage in flat list mode.** Nothing structural is in
   the way now that the view answers `indexAt` there; `block_dragdrop.sh` seeds
   no list mode, so it needs the registry dance `block_list_modes.sh` uses. Its
   code branches on the model and is believed correct, and nothing has driven it.
2. **Every structural edit serialises the whole file twice.** `nifSnapshotOp`
   saves the NIF before the operation and again after, for one undo step — 88 ms
   on a 512-block file, 160 ms on 2012, and it does not track what the operation
   touches. It is the largest cost in a drag by a wide margin and it is shared by
   everything, so it wants its own decision. In the backlog.
3. **The two list modes have drifted.** Flat list has no reorder, no drag-out and
   no auto-expand — deliberate, since it is file order rather than anyone's
   children. The mode is being kept; this is a question of how far to take it,
   not whether.

The live drag script is **cleared**: seven scenarios, six verified green in one
run and the seventh read out of that run's own log. Two things it taught, both
worth carrying:

- **A refused target never receives a drop event.** The handler answers with
  `Qt::IgnoreAction` and Qt withholds the `QDropEvent` entirely, so "no DROP
  reached the list" is the correct outcome for a refusal, not a failure.
- **`payload [N]` in the drag log is the block COUNT.** The identity of what was
  picked up is in the `=== drag start … first N ===` header. Reading the count as
  a block number had the script convicting the program in two whole runs.

It also could not fail at all until this session — `Write-Output` inside a
function whose caller wrapped it in `if (-not (…))` put the message *into* the
condition, and a two-element array is truthy however the verdict came out.

### Open, and honest about it

- **The UV Editor fold assertion is not functional coverage.** Its current
  `minimumWidth() < 340` check changes with polish/layout timing. A direct
  `resizeDocks(..., 280)` probe after showing the dock produced **795 px**: the
  wide header rows still impose a real effective floor despite the old explicit
  340 px dock minimum being gone. This was discovered while verifying the left
  editor’s independent 400 px migration and was deliberately not folded into
  that one-change fix.

- **Preset save/rename/remove still has no harness.** The "+" goes through a
  modal `QInputDialog::getText`, so it is not drivable the way the existing
  harnesses drive widgets; covering it means exposing the storage helpers behind
  test-only entry points first. Re-parenting, the other half of this note, is
  covered now — the operation moved into `wwReparentBlocks` and
  `block_dragdrop.sh` drives it.
- **Re-parenting has two transform rules now, on purpose.** The block list's
  plain drop preserves world position; the Collision Manager's **Set Parent**
  keeps the LOCAL transform, which is right for attaching collision to a bone
  and is why it was not changed.
- **`window_state_roundtrip.sh` runs outside the restricted sandbox.** It needs
  `Add-Type` temporary writes under `C:\msys64\tmp`; with that permission it is
  green for two maximised save/restore cycles on the second monitor. A sandbox
  permission failure before NifSkope launches is environmental, not a product
  failure.
- **The title bar reports a stale build.** `NIFSKOPE_REVISION` is baked when
  qmake runs, not when make does, so the About box and title can name an older
  commit than the binary. Cosmetic; fix by regenerating on link the way
  `README.md` already is.
- Parked from the same conversation, by choice: refit-shape-to-mesh (ranked
  highest of these), copy/paste physics between bodies, save-preset-from-an-
  existing-body, change shape type in place, mirror across X, and settling
  whether several selected meshes should make one `bhkListShape` body instead of
  several.

---

## State

Edition **0.3**, on upstream NifSkope 2.0.dev11 (fo76utils `develop` @
`f2587869`).

### What the last session changed

Fourteen commits, `873e02f`…`a901281`. All committed, harnessed and pushed.

The feature is in the table above. What is worth carrying forward is *how* it
went, because the shape of it will repeat:

- **The first version shipped dead, with 26 green checks.** Nothing in this
  codebase set `Qt::ItemIsDragEnabled`, so `QAbstractItemView` never entered
  `DraggingState` and `startDrag()` was never called. The harness drove the drop
  handlers directly — correct, since no synthetic event can enter a native drag
  loop — which put the one broken step outside everything it measured.
- **Four more fixes were made by reading code, none of them right**, while the
  harness climbed to 44 green. What actually found it was
  `tests/spells/block_drag_live.ps1`, driving the physical mouse: one run, one
  `DragEnter`, then silence. Ignoring a drag event ends the drag over the widget,
  and a drag begins on the row being dragged, whose neighbouring gaps refuse as
  no-ops. Dead before it began.
- **Rename was already built** (`d5765c4`) and filed in the backlog as not
  started, because nothing measured it. It had one real gap — proxy-only, so flat
  list mode did nothing at all.
- **Three checks were written that passed for the wrong reason** and had to be
  rewritten: a paste test casting an invalid index, a hover test whose helper
  re-expanded the row before hovering, and a Block Details test calling
  `NifTreeView::isRowHidden`, which shadows Qt's with a different meaning. Two of
  them wasted a build each; the third wasted two.

Everything above is in [WW_CHANGES.md](WW_CHANGES.md) under 2026-08-07 a–n.

### The rule that came out of it

**Ask what your harness enters below, and cover that separately.** Every bug in
this session lived above the point where the tests started: the drag start, the
native event loop, the paint during a modal drag. 82 checks below that line and
zero above it read as thorough and was not.

### Open, not started

- **Scale Inertia Tensor** exists in no UI. The 3ds Max exporter offers it.
- **Phantom / Shape Phantom** are present but greyed: they need
  `bhkSPCollisionObject` + `bhkSimpleShapePhantom`, which nothing here writes.
- **Gravity Factor, Rolling Friction Multiplier, Time Factor, Collision
  Response** are real `bhkRigidBodyCInfo` fields exposed nowhere.
- With **Replace off**, a new shape joins an existing body's `bhkListShape` —
  and the body settings in the create popup then do not apply to it. Decide what
  should happen.

Everything in [WW_FEATURES.md](WW_FEATURES.md) is built, committed, and covered
by at least one harness. Nothing is half-landed. Two things are deliberately
parked and say so in the UI:

- **PBR rendering** — implemented, mode and toggle greyed out.
- **The four mapped features** — growing-op segment attribution, `NiPointLight`,
  `NiPSysColliderManager`, `NiPSysBombModifier`. Researched and written up in
  [docs/FOUR_FEATURES_PLAN.md](docs/FOUR_FEATURES_PLAN.md), not started, on the
  user's instruction.

One thing on the list is unfinished-by-request: the **Shading menu** is longer
than it should be. Simplification was raised and then deferred — do not touch it
without asking.

## Build

Windows, MSYS2 UCRT64. Release:

```bash
C:/msys64/usr/bin/bash.exe -c 'export PATH=/ucrt64/bin:/usr/bin:/e/Tools/GIT/mingw64/bin:$PATH; cd /e/Projects/NifskopeWildWastelandEdition && make -f Makefile.Release -j2'
```

Output is `release/NifSkope.exe`. That is always the correct binary; do not test
against anything else.

**`git` has to be on that PATH** — the link rule runs `git rev-parse --short HEAD`
to bake the build rev into the title bar, and MSYS2 does not ship git. Without
`/e/Tools/GIT/mingw64/bin` the whole build compiles and then dies at the last
step with `git: command not found` / `Error 127`, leaving the OLD exe in place —
which then quietly passes or fails harnesses as if it were the new one. Set the
PATH with `export` before `cd`, too: a `PATH=... make ... | tail` pipeline puts
the pipe stages outside the assignment, and `tail` is not on the default path
either.

**`-j2`, not `-j8`.** This machine has hard-shut-down under sustained all-core
load — Kernel-Power 41 with no bugcheck, which reads as a power or thermal
margin problem rather than a software fault.

**The generated `Makefile*` files hold absolute paths.** They are gitignored, so
a fresh clone is fine, but if the repository folder is ever moved or renamed,
re-run qmake before building or the old path comes back at you as a
file-not-found from the middle of a link.

**After changing the include graph or adding data members to a widely-included
class, re-run qmake before trusting an incremental build:**

```bash
qmake6 -o Makefile NifSkope.pro
```

A stale dependency list once linked an old-layout `.o` and hard-crashed at
startup (`0xC0000005` inside `QHash::findNode`). More generally: **rule out a
stale incremental build before bisecting any access violation or heap
corruption.** `make clean` first. Identical source has crashed 6/6 incremental
and 0/12 clean.

**This bit again on 2026-08-07** and cost an hour: three widely-included headers
gained members, a dozen incremental builds followed, and a harness started dying
half way through with "Free Heap block modified after it was freed". The code
was correct. Clean build, 25 of 25. Do the clean build *first*, not after
reading the diff four times.

**`make clean` breaks the next build**, so know this before you run it: qmake
writes the `icon_res.o` rule with an absolute target path and lists the object
with a relative one, so make stops at `No rule to make target
'GeneratedFiles/.obj/icon_res.o'`. Build it once by hand and carry on:

```bash
windres -i res/icon.rc -o GeneratedFiles/.obj/icon_res.o --include-dir=./res
```

**Set a writable temp when building from a sandboxed shell.** `export
TMPDIR=/tmp TMP=/tmp TEMP=/tmp` — otherwise g++ tries `C:\Windows` for its
intermediates and fails with "Cannot create temporary file". And piping make to
`grep`/`tail` hides its exit status: use `set -o pipefail` or check
`${PIPESTATUS[0]}`, or a failed build reads as a successful one.

Relink can also fail with the exe locked. Kill the straggler and relink; it is
not a code error.

### Version numbers — there are two, on purpose

| | where | what it is |
|---|---|---|
| `WW_VER` | `NifSkope.pro` | this fork's edition number (`0.3`) — title bar, About box |
| `VER` | `build/VERSION` | upstream lineage (`2.0.dev11`) |

`applicationName` stays `"NifSkope 2.0"` because it is the **QSettings key**;
renaming it would strand every existing user's settings. The edition name lives
on `applicationDisplayName`. `NifSkope::migrateSettings` compares against
`NIFSKOPE_VERSION`, so that one has to keep tracking upstream.

To cut 0.3: change `WW_VER` in `NifSkope.pro`. Nothing else.

### README.md is generated

`README.md` is produced at link time by `QMAKE_PRE_LINK` from
`build/README.md.in`, substituting `@VERSION@` and `@WWVERSION@`. **Edit the
`.in` file.** Editing `README.md` directly works until the next link, then
silently reverts.

## Layout

```
src/                    application
  glview.cpp            the 3D viewport — modes, modeling ops, selection, gizmos
  nifskope_ui.cpp       docks, toolbars, menus, workspaces, the WW_* harnesses
  gl/hknpdecode.cpp     compiled Havok collision, read
  gl/hknpencode.cpp     compiled Havok collision, written back
  uvtools.cpp           UV editing workspace
  unfucktools.cpp       Issue Manager
  nifcli.cpp            headless CLI
  wwskin.h              the palette — colours come from here, never literals
  ui/widgets/
    wwnumberfield.*     the one number field
    timeline*.cpp       animation timeline
    physicspanel.*      ragdoll / physics sim
lib/                    vendored deps (qhull, gli, meshoptimizer, libfo76utils)
tests/                  harness wrappers, one per feature area
tools/                  byte-level verifiers, corpus scripts, render regression
docs/                   plans, audits, research, CLI reference
```

No submodules. Everything is vendored, so `git clone` is complete.

Note: source comments cite plan docs by bare filename (`MODELING_TOOLS_PLAN.md`,
`TO_BE_IMPLEMENTED.md`, …). Those files now live under `docs/`. The names are
still unique — grep finds them.

## Testing

Harnesses are environment-gated code paths inside the real binary. Set the flag,
the app drives itself and writes `release/ww_*.log`, and a shell wrapper asserts
on it.

```bash
tests/spells/top_bar.sh
tests/spells/collision_undo.sh
tests/spells/scrub_uniform.sh
```

The collision and menu work added six:

| harness | covers |
|---|---|
| `collision_panel.sh` | the two-button split, both popups, the disabled-shape tooltip, and that every moved control still writes its key |
| `collision_per_shape.sh` | N selected shapes → N bodies, each on its own node, source meshes consumed |
| `quick_favourites.sh` | pinning, the Q menu, Space/Q scoping, the search menu reaching menu actions |
| `spell_search.sh` | the palette: dismissal, positioning, and not listing its own row |
| `nav_keys.sh` | rotate/zoom rebinding, and that letting go stops the camera |
| `collision_compiled_edit.sh` | editing a compiled body in place |
| `window_state_roundtrip.sh` | open, close maximised, open again — the startup crash |

And the block-list session added three:

| harness | covers |
|---|---|
| `block_dragdrop.sh` | 87 checks: that the drag starts at all, the three modifiers, reorder by the gap, drag-out, every refusal, multi-select as one payload and its ordering, the highlight and the painted insertion line, the drag card, auto-expand and its fold-back, paste following the pointer *in a second window*, blank-click deselect, one undo step. `WW_BLOCKDND_BENCH=<n>` also times a move on a file that size |
| `block_rename.sh` | 25 in hierarchy (list mode is out of the gate, see below): F2 and double-click, that nothing else opens on top, no sideways scroll, Escape, the column asymmetry, the txt icon, and that the name reaches the palette |
| `collision_drop.sh` | 7 checks: a mesh dragged onto the Collision Manager gets collision, at the shape type the panel is showing, one body per mesh — and check 2 asks whether the dock accepts drops at all, which is the only thing the harness steps over |
| `block_list_modes.sh` | 8 per mode: that the header's total matches the sections it totals, that every row resolves back to itself through `indexAt`, that a block inserted now is addressable, and that all of it survives switching modes |
| `block_drag_live.ps1` | **the only thing above the native-drag boundary** — drives the physical mouse across 7 drags: into a shut node, into a row its own auto-unfold revealed, into a second root, a root made a child, out to blank space, a mesh row's all-gap reorder, and a refused cycle. See the warning below. |
| `block_visibility.sh` | 37 per mode: H / Alt+H over the list with a framebuffer delta either way, multi-select H, the subtree and inherited-from-an-ancestor rules, the eye and disc clicks asserted to land on **the same state the key produced**, the press/slide-off/release contract, non-drawable rows exposing nothing, see-through against BOTH the solid and the hidden frame, the column shape in both modes, and that F2 rename still types an 'h'. Seeds `List Mode` like the two above |

All three build their fixture from the CLI cube fixture (`-no-gui new --cube`),
so they need no game corpus at all. **`--cube` is not optional here**: as of
2026-08-11b the program's new document is empty (header plus one root NiNode),
and `new` on its own writes that. `new --cube` writes the old four-block cube
scene, byte-identical to what `new` produced before, which is why none of these
suites needed an assertion changed. Add Primitive cannot stand in for it — it
clones an existing BSTriShape, so it refuses to make the first shape in a
document. `block_rename.sh` and `block_list_modes.sh` seed
`List Mode` into the registry before launch and put it back on exit, because the
mode is read during window construction — the app has to *start* in the mode
under test, and both of them assert that it did rather than assuming.

**Never `ignore()` a drag event you mean to keep receiving.** Ignoring a
DragEnter or DragMove ends the drag over that widget — not one further event
arrives — so the first position the pointer happens to be at decides the whole
gesture. A drag begins on the row being dragged, whose neighbouring gaps refuse
as no-ops, so the block list's drag was dead before it began and stayed that way
through four wrong fixes. Accept the event and put the verdict in the drop
ACTION: `Qt::IgnoreAction` gives the no-drop cursor while the stream stays alive.

**A window that follows the cursor during a drag must be
`Qt::WindowTransparentForInput`.** Otherwise it takes part in hit-testing, and
the moment it passes under the pointer the view underneath gets a `DragLeave` and
stops receiving `DragMove` — so the follower freezes, the drop feedback stops
updating, and the stale caption it is left showing gets read as the program's
verdict on wherever the cursor is now. That arrived as three separate bug
reports: a stuck label, a line that never appeared, and legal drops "refused".

**Anything a drag draws must `repaint()`, not `update()`.** `QDrag::exec()` runs a
native modal loop; a posted update is coalesced and can sit in the queue until
the drag ends, so the paint lands after it stops being useful — indistinguishable
from never painting at all, and reported that way twice. No harness can catch it
either: a harness delivers drag events directly and is never inside the loop that
swallows the paint.

**A drag event cannot be delivered with `QApplication::sendEvent`.**
`QApplication::notify` routes drag and drop through the drag manager, so a
synthetic one reaches neither the widget's `event()` nor any event filter —
measured at zero, to the view and to the viewport both. That is why the drop
handlers are `NifTreeView` overrides and why `wwDeliverDragEvent` exists: a
harness needs an entry point that begins where Qt's routing ends. The override
count it reports is the check that the overrides ran, rather than the hook being
poked directly.

**There is a live-drag test, and it is the only thing above that boundary.**
`tests/spells/block_drag_live.ps1` drives the physical mouse at the block list on
the second monitor and reads `release/ww_drag.log` — which the program writes on
every drag, with no flag to set (`WW_DRAG_LOG=off` disables it, or a path
overrides it). It found in one run what four code-reading fixes missed while the
harness sat at 44 green.

**It SEIZES THE POINTER, so it is run by hand, never fired off.** Placement on
the second monitor keeps a *window* out of the way; the mouse is not per-monitor,
and clicks land wherever the cursor is dragged. It was once run mid-task and
disturbed the user's live session. Ask before every run, and if it has already
answered the question, do not re-run it to confirm.

**And that entry point is exactly how the drag shipped broken.** Driving the drop
handlers covers everything below `startDrag()` — and `startDrag()` was never
called, because `QAbstractItemView` will not enter `DraggingState` unless the
MODEL reports `Qt::ItemIsDragEnabled`. 26 checks green, feature dead. **When a
harness has to enter below the top of a mechanism, name what it stepped over and
cover that separately**: the flags on both models, `dragEnabled()` on the view,
and a real press-and-move. Swap the drag hook for a counting one first — the
production hook ends in `QDrag::exec()`, a modal loop that never returns with
nobody at the mouse.

**Two useful capture levers, both added while chasing things reading was not
finding.** `WW_CAMERA_LOG=<file>` appends every camera reorientation with its
rotation and view state — that is what finally located the startup view being
overwritten, after three carefully-read suspects each turned out innocent. And
`WW_RENDER_VIEW` now accepts a **negative** value, meaning "leave the camera
exactly as startup left it", which is the only way to photograph the startup
view: every other value overrides the thing under test. Its old upper bound was
`ViewWalk`, so asking for `ViewUser` was silently rewritten to Front and
produced a capture that looked like a passing test of a view it had never used.

`window_state_roundtrip.sh` is the one harness that **cannot** source
`_harness.sh`. `saveUi()` deliberately bails out on any `WW_*` variable so
harness layouts never overwrite the user's, and `WW_WINDOW_AT` is a `WW_*`
variable — so the flag that places the window off the primary monitor also
disables the write path the test exists to exercise. It seeds `UI/Window
Geometry` onto the second monitor instead, asserts the window landed there
before doing anything else, and restores the settings key on exit. If you write
another test that needs real settings written, it has the same problem.

`tests/spells/_harness.sh` holds shared setup. 60 flags exist; `grep -rhoE
'WW_[A-Z_]+_TEST' src/ | sort -u` lists them.

Three rules that were learned the hard way:

1. **Run only the harnesses whose code path the change reaches**, and say why
   each is in the list. A blanket run opens a dozen windows and tells you
   nothing about most of them.
2. **A harness must force the state it measures**, never inherit persisted
   `QSettings`. A green suite went red with no code change because
   `GLView/Enable Animations` was left `false` in the registry.
3. **Measure with an invariant that fails on broken code, and prove it fails.**
   A proxy number that merely agrees with correctness is not evidence. Where
   possible the check is committed *before* the fix, so its failure is recorded
   against the unfixed binary.

Three ways that rule got broken in one session, all worth knowing:

- **A threshold the wrong answer also clears.** "More than 20 materials" passed
  for two builds on Oblivion's 32 where Fallout 4 has 157. Name things that
  exist only in the right answer.
- **A check satisfied by the bug itself.** "The material can be typed" asked
  whether the field was editable — and the leftover `setEditable(true)` *was*
  the bug. Open the thing and look inside it.
- **A check that never ran.** Two green assertions about zooming, on a camera
  that had not moved: the pump spun `processEvents` for microseconds while
  `advanceGears` steps on real elapsed time, and the measurement read
  `cameraDistance()` when zoom changes `Zoom`. **Prove the control moves before
  asserting that a change moved it.**

And: **a check that fails intermittently is worse than no check**, because it
teaches you to re-run rather than to look. Poll for a condition with a deadline;
never sample once after a fixed delay.

`grabFramebuffer` does **not** repaint — pump `ogl->update()` +
`processEvents()` twice or you diff a stale frame.

## Landmines

**It will not start?** This was the long-running one, and it is fixed as of
`5983a97` — but the lesson it taught is wrong, so read the correction.

Clearing `UI/Window State` under `HKCU\Software\NifTools\NifSkope 2.0\UI` did
cure it, every time, which is why three sessions treated the saved blob as
corrupt. It never was. `restoreUi()` restored geometry before state, and
replaying a layout saved while **maximised** into a window that
`restoreGeometry()` had just flagged maximised, but that had not been shown yet,
faulted `0xC0000005` in `QLayout::addChildWidget`. Clearing the key worked
because an empty blob makes `restoreState()` a no-op — it removed the symptom,
not the cause. State is restored before geometry now.

**The real lesson: "clearing X fixes it" does not mean X was bad.** Hold one
variable at a time instead. The same 2090-byte blob crashes under a maximised
geometry and restores perfectly under a normal one; swapping only the geometry,
with the blob byte-identical, flips the outcome. That single comparison is what
broke it open after two sessions of bisecting the wrong thing.

Still true and still worth keeping: **a crash that survives reverting the change
that appears to cause it is not caused by that change** — check persisted state
before bisecting further.

**`QHeaderView` keeps its total by adding and subtracting, and a model change
desyncs it.** `length` is the sum of the sections, maintained by deltas rather
than re-derived. Hiding a section subtracts its width and remembers it; changing
the model gives every remembered width back **without adding it to `length`**.
Hide them again and each width comes off twice. The Block List hides 9 of the
NifModel's 12 columns, so after one load `length` was NEGATIVE — and
`visualIndexAt` returns -1 for anything past it, so `QTreeView::indexAt` had no
column and returned no index for any point in the view. Nothing in the flat list
could be clicked, dropped on or right-clicked, and it read as "the rows are
there but dead".

Two rules follow, and `block_list_modes.sh` guards both: **release the columns
before changing a view's model and apply them after**
(`wwReleaseBlockListColumns` / `wwApplyBlockListColumns`), and **a saved header
blob belongs to a model shape** — restoring one saved against the 3-column proxy
onto the 12-column NifModel desyncs the total the same way, so each mode keeps
its own.

**`NifTreeView::isRowHidden( int, const QModelIndex & )` is not
`QTreeView::isRowHidden`.** It shadows it with a different meaning: it ignores the
row number and answers for the item behind the index you passed as the *parent*.
Asking it whether a row is hidden returns something unrelated — under an invalid
root it is always false — and a check built on it measures nothing. Use
`visualRect().isEmpty()`, which is usually the real question anyway.

**"Selected" in the Block List is two things.** Qt's selection and current index,
and `NifModel::selHighlight` — mirrored from the 3D view's object selection, and
the one the row's COLOUR comes from. Clearing one leaves the other showing.

**An invalid root index means "show the whole model" to a QTreeView**, not "show
nothing". Block Details listed every block in the file the first time nothing was
selected.

**CRLF.** Four sources and one doc are CRLF: `src/glview.cpp`,
`src/gl/controllers.cpp`, `src/nifskope.cpp`, `src/spells/havok.cpp`,
`WW_CHANGES.md`. Python text-mode I/O flattens them into a 33,000-line junk
diff — **and so does `sed -i`**, which caught this twice in one session. Use an
editor that preserves them, or binary I/O, and check `git diff --numstat` before
committing.

**A disabled widget shows no tooltip.** It receives no mouse events, so Qt never
delivers it a `ToolTip` event and `setToolTip` on it displays nothing —
silently, in exactly the case where the explanation is needed. Put the text on
an enabled wrapper (`createShapeHost` in `collisiontools.cpp` is the pattern).

**`populatePhysicsEnums()` returns early before the physics editor exists.** It
guards on the editor's own combos, so calling it during construction to fill
create-side lists does nothing, leaving `currentData()` invalid and writing 0 —
which is `MO_SYS_INVALID`, `OL_UNIDENTIFIED` and friends. Fill those after the
editor is built, and again on `modelReset`.

**Collision tables are Fallout 4's, unconditionally.** `materialEnumType()` and
`layerEnumType()` used to walk the BS version down into Oblivion, and a BS
version of 0 — which is what you get before a file is open and from this
program's own new document — reached the bottom rung. Do not reintroduce a
ladder.

**`spRemoveBranch` reads the block-list selection.** Calling it to remove one
block removes the whole published multi-selection. `castCollisionOverSelection`
takes the selection down for the duration of a run for this reason.

**Scene node ids do not follow a renumber.** `Node::nodeId` is assigned when the
Scene builds it, so gathering geometry after an insert reads through a stale id.
Gather before mutating, or ask the `Node` for its own id.

**Colours.** Never introduce a colour literal. `wwSkinColor("name")` from
`src/wwskin.h`, or add a token there. The same discipline applies to the number
field: `wwMakeScrubField` / `WwNumberField`, never a sixth private copy of the
scrub gesture — that is exactly how the five that got deleted came to exist.

**Menus.** `QMenu` paints the icon and the check indicator in the *same* column,
so an icon-bearing checkable item silently loses its checkmark. Checked rows are
styled by fill (blue background, orange text) instead.

**Event filters run before the target's own handler.** Insetting a spin box's
line edit on the *host's* resize gets undone; the correction has to hook the
editor's own `Resize`.

**`QToolButton::sizeHint()` under-reports** on stylesheet-styled buttons, because
QSS padding is not folded in. Take the max of the hint and font-metrics + padding
or the label elides.

## Conventions

- Solo repo: commit straight to `main`. No branches, no PRs.
- **Update [WW_CHANGES.md](WW_CHANGES.md) with every change batch, unprompted.**
  Newest entry at the top, dated, with the measurement that backs it.
- When a feature's design is uncertain, look up Blender's equivalent and follow
  it. State deliberate divergences.
- Do GUI verification via the harnesses rather than handing over a checklist.
  In-game validation belongs to the user.
- Fallout 4 only. Fallout 76, Skyrim and Starfield paths are inherited, not
  maintained, and not to be worked on.
