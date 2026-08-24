# What the engine does with a ragdoll

Findings from the leaked 1.10.155 exe+PDB pair (`E:\Projects\Fo4CommunityShaders\Fo4PDB`,
queried with `tools/exere/f4pdb.py`). **Every RVA below is 1.10.155.** The live game
is 1.11.221; bodies match closely, addresses do not — re-find with `f4re.py` before
trusting an address against the running build.

Written after a rebuilt human ragdoll misbehaved in game while every offline
comparison of the file said it matched vanilla. The rule this exists to serve:
a byte diff answers "is our file the same", never "what does the engine do with it".

## Closed: nothing validates our data on load

`hknpRagdollData::checkConsistency` @0x18dbed0 is `ret 0` — a no-op in release.
There is no gate to fail, so "the engine rejected the ragdoll" is not a
hypothesis.

## Closed: the mass the engine reads is the mass we write

`hkbnpRagdollInterface::getOriginalMassOfBody` @0x1737450:

    r10 = [this+0x18]              the ragdoll data
    search [r10+0x20] (count [r10+0x28]) for the argument -> index
    r8  = [r10+0x10]               the physics system data
    bounds-check index against [r8+0x48]
    rax = [r8+0x40]                bodyCinfos, stride 0x60  (idx*3 << 5)
    rdx = [cinfo+0x0c]             the MOTION INDEX
    rax = [r8+0x30]                dyn_inertia, stride 0x70
    xmm1 = [inertia+0x04]          inverse mass

Exactly the fields we write, resolved *through* the motion index — so the index
being a permutation of vanilla's is harmless.

## Closed: the grab weight is a scene-graph SUM, and ours equals vanilla's

`PlayerCharacter::StartGrabObject` @0xeadaa0 → `TESHavokUtilities::GetSceneGraphMass`
@0x63b850 → traversal @0x63f280, which for every collision object in the graph:

  * skips it if `byte [target+0x108] & 1` is set;
  * else adds `bhkNPCollisionObject::GetMass`.

The total is compared against the game setting `fGrabMaxWeightWalking`.

`bhkNPCollisionObject::GetMass` @0x1d803d0 returns **0** when:

  * the system has no physics world (`bhkPhysicsSystem::getWorld` null), or
  * `[motion+0x28] != resolvedBodyId` — the body must be the FIRST attached to
    its motion, which is how Havok avoids counting a shared motion twice.

Otherwise it takes `hknpBody+0x68` (motion id), indexes the world's motion array
at `[world+0xe0]` with stride **128**, unpacks a **16-bit half-float** inverse
mass from `motion+0x20` (punpcklwd + shufps 0xff), and reciprocates it with a
Newton step, returning 0 for a zero inverse mass.

Measured on the human skeleton, vanilla vs our rebuild: 18 bodies with a motion,
18 distinct motions, **sum 93.5000 kg on both**. The grab path is identical, so it
cannot be the cause of a corpse that lifts differently.

## Closed: the file is equivalent to vanilla, at every level I can compare

Done after the mass and trigger fixes, on the human skeleton, vanilla vs our
decompile/recompile. Four independent layers:

**1. The NIF outside the blobs.** 166 blocks, and with every Ref/Ptr resolved to
its target's type and name so renumbering compares equal: **0 blocks differ**. All
129 node world transforms identical. All 19 `bhkNPCollisionObject` carry flags 128
(ANIM_TARGETED), as vanilla.

**2. The packfile container.** Header and section table byte-identical (0x00..0x100).
Section stride is **64**: tag[19] + 0xff pad + 7 ints + 16 bytes padding, first
record at 0x40, data at 0x100. `__data__` local fixups identical (10, same order),
virtual fixups identical (38, same order). Global fixups: 89 each, **16 differ** --
all in one array, discussed below.

**3. Havok's own deserializer** (`FileConvert.exe -x`, the real hk_2014 reader).
4,529 scalar leaves, **137 differ**, and every one is inert:
  * 32 are `materialId` / `motionPropertiesId`, indices into arrays whose 18
    entries are **all identical to each other** (checked: 1 distinct signature);
  * 74 are `hkVector4` w lanes -- padding. One dumped in full to be sure: a
    unit-Z basis vector whose w vanilla left as 1.0;
  * 31 are capsule polytope `planes`, which compare **equal as a set** -- the core
    box is rolled 180 degrees about its own axis and its cross-section is square,
    so the solid is identical and each file is internally consistent (plane k is
    defined from that file's own e1/e2, and the face/index table is expressed in
    the same local frame).
Separately, **all 197 pointer fields resolve to the same object** in both.

**4. The differing global fixups are the SHAPE LIST, and only its order.**
The root's array descriptors, read from the local fixups:

    +0x10 -> 144    materials      18 x 0x50
    +0x20 -> 1584   motionProps    18 x 0x40
    +0x30 -> 2736   dyn_inertia    18 x 0x70
    +0x40 -> 4752   bodyCinfos     18 x 0x60
    +0x50 -> 6480   constraints    17 x 0x18
    +0x60 -> 6896   SHAPE LIST     18 pointers   <-- the 16 differing fixups
    +0x80 -> 7040   boneToBodyMap  18 x 4

Ours lists the capsules in ascending file order; vanilla's is a permutation. It
does not matter, and that is measured rather than argued: **every body resolves to
the identical capsule**, radius and length, body for body, all 18. `bodyCinfos[i]
.shape` points at the same object in both.

Note for later: `hknpBodyCinfo +0x12` is **`materialId`** by Havok's own
reflection, and this codebase writes the body's shape-list SLOT there. The two
coincide in everything we produce (our shape list is the identity, so slot ==
body index) and in vanilla (its materials are all identical), so nothing is wrong
today -- but they are not the same concept, and a system with per-body materials
would expose it.

## Not the cause either: mod conflict

No other mod in `E:\Projects\Fallout 4 Mods\mods` provides
`Actors/Character/CharacterAssets/skeleton.nif`, loose or in any of the 12 BA2s,
and the game's own Data folder has no loose copy. Our mod is the only source, so
it is not overriding somebody else's skeleton.

## Where that leaves it

Our rebuilt human skeleton is equivalent to vanilla's in the NIF, in the packfile
container, in every scalar Havok reads, in every pointer, and in the two engine
paths traced above. Two real defects WERE found and fixed on the way (primitive
mass properties, the trigger material), and both were found by comparing against
vanilla -- but neither was found by comparing the ragdoll packfile, which is where
the effort went.

If the symptoms persist on build 8b1b312, the cause is not in this file, and the
next move is an A/B in one session rather than more analysis.

## Both skeletons, and the complete taxonomy of what differs

The same comparison run on the brahmin (39 bodies) as on the human (18):

                              human        brahmin
    scalar leaves compared     4529          10178
    differing                   137            270
      array indices              32             76
      hkVector4 w lanes          63            137
      capsule plane order        42             44
      negated quaternions         0             13
      anything else               0              0
    pointer fields              197            449
      differing targets            0              0

Every class is inert, and each was checked rather than asserted:

  * **array indices** -- `materialId` and `motionPropertiesId` index arrays whose
    entries are all identical to one another (1 distinct signature across 18);
  * **w lanes** -- `hkVector4` padding. Dumped one in full: a unit-Z basis vector
    whose w vanilla left as 1.0;
  * **capsule planes** -- equal as a SET; the core box is rolled 180 degrees about
    its own axis and its cross-section is square, so the solid is unchanged;
  * **negated quaternions** -- all seven are complete negations, dot = -1.000000
    to six places, which is the same rotation. The components that appear not to
    differ are zeros, where a sign flip is invisible.

The shape-list pointer array (root +0x60) is ordered differently, and **every body
still resolves to the identical capsule**, radius and length, body for body.

## Engine paths traced, and what each ruled out

    hknpRagdollData::checkConsistency        @0x18dbed0   ret 0 -- no load validation
    hkbnpRagdollInterface::getOriginalMassOfBody @0x1737450 mass via cinfo +0x0c ->
                                                           dyn_inertia 0x70, +0x04
    PlayerCharacter::StartGrabObject         @0xeadaa0    grab entry
    TESHavokUtilities::GetSceneGraphMass     @0x63b850    sums the scene graph
    BSVisit::TraverseScenegraphNPCollision   @0x63f280    skips [target+0x108]&1
    bhkNPCollisionObject::GetMass            @0x1d803d0   0 if no world, 0 if
                                                           [motion+0x28] != bodyId;
                                                           else 1/half-float at
                                                           motion+0x20
    BShkbAnimationGraph::RestoreOriginalMassInRagdollImpl @0x16277c0
    BShkbAnimationGraph::EaseInConstraintsImpl @0x1627960  loosens then restores the
                                                           ragdoll constraints over a
                                                           duration -- this is the
                                                           vanilla "settle" and the
                                                           reason a ragdoll is briefly
                                                           loose after death
    hknpEaseConstraintsAction::loosenConstraints @0x1884700
    hkpConstraintDataUtils::loosenConstraintLimits @0x17bfe20

Measured consequence: the grab weight is the SUM over the scene graph, and both
files give **18 bodies, 18 distinct motions, 93.5000 kg** on the human skeleton.
Identical, so the grab path cannot produce a different result.

`EaseInConstraintsImpl` is worth remembering: the engine deliberately loosens a
ragdoll's constraints on death and eases them back over a duration. A ragdoll that
looks loose for a moment after death and then firms up is vanilla behaviour, not a
defect -- and its input is constraint data that matches vanilla in every scalar.

## THE REFERENCE WAS WRONG: DataUnpacked is a different build of the game

Found last, after everything above said the file was equivalent to vanilla. The
one assumption nothing had tested was the meaning of "vanilla": every comparison
in this session, and in this project's collision work generally, used
`E:\Tools\Fallout 4\DataUnpacked`. That tree is **not what the installed game
loads.**

Measured by reading the installed archives directly (`ba2get.py`, GNRL format:
'BTDX', version, type, numFiles, nameTableOffset; then 36-byte records
nameHash/ext/dirHash/flags/offset/packed/unpacked/align; then a u16-length name
table in record order):

    NIFs sampled from Fallout4 - Meshes.ba2      600
    identical to the unpacked tree                 0
    different                                    600

Most differ by exactly 44 bytes -- the NIF header's export path string -- but not
all, and **collision blobs differ too**: 6 of 6 sampled, including plain statics.
`CeilingFan01` is 3904 bytes in the archive against 3920 unpacked, 462 bytes
differing, while decoding to the same 2 shapes and the same 54 preview triangles.
So it is a re-export, not corruption. The installed archives are **BA2 version 8**,
i.e. the next-gen update.

### Why it matters for ragdolls specifically, and not much elsewhere

The two builds order a ragdoll's BODIES differently:

    bone   the game        DataUnpacked
    0      COM             COM
    1      LLeg_Thigh      LLeg_Thigh
    2      RLeg_Thigh      SPINE1
    3      SPINE1          RLeg_Thigh
    5      RLeg_Calf       SPINE2
    8      RLeg_Foot       Chest
    10     LArm_UpperArm   RArm_UpperArm

Each file is internally consistent -- every bone's rest pose matches its own body
order, `boneToBodyMap` is the identity in both -- so both are valid ragdolls. The
brahmin is reordered the same way (62041 bytes in the archive, 62085 unpacked).

Our pipeline faithfully preserves whichever order it is given. Built from
DataUnpacked it produces DataUnpacked's order, and that was then installed over a
game whose animation and behaviour data -- which lives outside the NIF and which
we never touch -- expects the other order.

**Rebuilt from the game's own archive, our output reproduces the game's order
exactly**: bone for bone, parent for parent, node for node, with identical
body-to-node mapping, total mass (93.500 kg human, 121.000 kg brahmin), mean
density and trigger-body counts.

### What this means for the rest of the collision work

Every "vanilla says X" figure in this project was measured against DataUnpacked.
The corpus statistics are still self-consistent and the format conclusions drawn
from them are still sound -- both builds are the same FORMAT. But any claim of the
shape "our output is byte-identical to vanilla" is byte-identical to *that* build,
and any mod built from the corpus is built from *that* build's assets. For static
collision that has been harmless. For ragdolls it was not.
