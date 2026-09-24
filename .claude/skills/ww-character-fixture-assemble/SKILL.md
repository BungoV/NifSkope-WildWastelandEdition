---
name: ww-character-fixture-assemble
description: Build ONE poseable NIF out of Bethesda's many shipped character parts — body, hands, head, head rear, eyes, mouth, brows, armour pieces — merged onto a skeleton so the whole model shares one bone tree, and prove the result kept every skin partition, every bone list and the bind pose. Use whenever a lane needs a whole character (or a whole armour set) as one file — an animation-preview fixture, a pose test, a render subject — and whenever "assemble the vanilla <race><sex>" is the ask. It also carries the part-selection rules (what is a face part, what is the chargen sculpt rig, what is a scratch file) that cost a corpus read to work out once.
---

# Assembling a whole FO4 character as one NIF (WW tree)

Written from lane FIXTURE, 2026-09-10, which built
`fixtures/human_male_vanilla.nif` (the vanilla human male: body, hands, face,
head rear, eyes + AO + wet, mouth, brows on `skeleton.nif`). Read `nif` first
for the CLI itself; this is the character-specific procedure and its gates.

## 1. The corpus and the parts

`E:\Tools\Fallout 4\DataUnpacked\Data\meshes\actors\character\characterassets\`
— never a mod folder. Face parts are one level down in `FaceParts\`.

The vanilla adult human MALE, complete and visible:

| shape | file |
|---|---|
| body | `MaleBody.nif` |
| hands | `MaleHands.nif` — the body mesh **ends at the wrists**, this is not optional |
| face | `BaseMaleHead.nif` |
| rear of the head | `FaceParts\maleheadRear.nif` |
| eyes | `FaceParts\MaleEyes.nif` |
| eye AO layer | `FaceParts\MaleEyesAO.nif` (`BSEffectShaderProperty`) |
| eye wet layer | `FaceParts\MaleEyesWet.nif` |
| mouth, teeth, tongue | `FaceParts\MaleMouth.nif` (shape `MouthHuman:0`) |
| eyebrows | `FaceParts\MaleBrows.nif` — no BGSM, textures inline in the `BSShaderTextureSet` |

Female is the same list with `Female*` / `FemaleheadRear.nif` /
`FemaleEyes*` / `FemaleMouth.nif` / `FemaleEyeBrows.nif`, plus
`FemaleLashes.nif` (there is **no adult-male lash mesh** in the corpus).

**What is NOT a part, and why it looks like one:**

* **every `*_faceBones.nif` and `skeleton_faceBones.nif`** are the CHARGEN
  SCULPT RIG, not the animation rig: a different skeleton with ~60
  `skin_bone_*` face bones, carrying `CustomizationRemapData` /
  `CustomizationRemapNewBonesData`. The `nif` skill's
  `references/face-customization-remap.md` gives the rule for a mesh needing no
  chargen support — **omit entirely**. Merging one also destroys the
  "node set == skeleton.nif's set" gate below;
* **`FaceParts\MaleEyesShade.nif`** is not a face part at all. It is one of
  Bethesda's own scratch preview assemblies — 9 shapes, 63 nodes, body + hands +
  head + `MaleEyesGloss` + `EyeLashes` + `hair01` — and it is the single best
  sanity reference for what a combined preview is supposed to contain. Read it,
  do not merge it;
* **`*Left.nif`** (`MaleEyesLeft`, `MaleEyesGlossLeft`, `EyeLashesLeft`, …) are
  the chargen per-eye heterochromia variants; the unsuffixed mesh already covers
  both eyes;
* `MaleNeckGore.nif` is the decapitation stump; `1stPerson*` is the first-person
  arms rig; `Old*`, `*Ghoul*`, `Child*`, `SynthGen2Head1` are other races/ages;
  `Hair\`, `Beards\`, `BodyTattoos\` are chargen choices.

## 2. The command

`merge` de-duplicates `NiNode`s BY NAME, so merging onto the skeleton gives one
shared rig and re-points every skin's `Bones[]` array for free. The SKELETON is
the merge target, not a donor: that is what makes the fixture carry the whole
node set and one root.

```bash
cd /e/Projects/NifskopeWildWastelandEdition
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?     # must be rc=1
D="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/character/characterassets"
./release/nifskope-cli.cmd merge "$D/skeleton.nif" \
    --add "$D/MaleBody.nif" --add "$D/MaleHands.nif" \
    --add "$D/BaseMaleHead.nif" --add "$D/FaceParts/maleheadRear.nif" \
    --add "$D/FaceParts/MaleEyes.nif" --add "$D/FaceParts/MaleEyesAO.nif" \
    --add "$D/FaceParts/MaleEyesWet.nif" --add "$D/FaceParts/MaleMouth.nif" \
    --add "$D/FaceParts/MaleBrows.nif" \
    -o "E:/ABSOLUTE/PATH/human_male_vanilla.nif"
```

**`-o` MUST BE ABSOLUTE.** A relative `-o` resolves against the EXE's folder;
every merge line reports success and the last line is
`error: failed to save …`. Same rule as `WW_RENDER_SHOT` (MISTAKES.md
2026-09-10).

Read the summary, it is a gate in itself: `N node(s) added` must be ~0 (any
larger number means the donors do not share the skeleton's bone names),
`no duplicate bone names introduced` must be printed, and the block count must
be `skeleton blocks + sum of the per-donor "+N block(s)"`.

Expected for the male list above: `9 shape(s) added, 122 node(s) shared`,
1 node added (`EyeLeftDummy001`, `MaleEyes.nif`'s eye-centre marker),
166 + 49 = **215 blocks**.

## 3. The gates, each with a control that fails

Written as `scratchpad/fixture_20260910/{nifhdr,skin_check,bones_compare}.py`;
none of them needs NifSkope, so they still run while the game is up.

| gate | how | control |
|---|---|---|
| block-count arithmetic | `nifskope-cli info` vs skeleton + merge summary | — |
| **skin partitions intact** | `nifskope-cli segments` on the fixture and on all donors, `sed -E 's/^\[[0-9]+\] //'` both, `diff` | must be byte-identical; 68 lines each for the male list |
| **bind pose intact** | md5 of every `BSSkin::BoneData` block (pure floats — no refs, no string indices, so byte-identity is meaningful) | a `FemaleBody.nif` BoneData md5 must NOT be in the set |
| **bone lists intact IN ORDER** | each skin's `Bones[]` names, donor vs fixture, as a LIST not a set — vertex weights are indices into it | `FemaleBody.nif`'s list must not compare equal |
| skin layout sane | every `Bones[]` entry points at a real `NiNode`; the declared block size is consumed exactly by `12 + 4*nBones + 4 + 4*nScales` | a wrong layout leaves bytes over and fails |
| node set | fixture vs `skeleton.nif`, case-insensitively: 0 missing, the added ones NAMED, 0 duplicates | — |
| `nifskope-cli check` | run it on the fixture AND on two untouched donors | every finding class must also fire on vanilla, or it is ours |

A name-SET match is not an order match, and only the order match proves the
skinning still works. Lane FIXTURE nearly reported the set alone.

## 4. Reading a FO4 NIF header without the exe

`scratchpad/fixture_20260910/nifhdr.py` — block types, per-block type index and
sizes, the string table, and the Name of every `Ni*Node` / `*TriShape`. Two
traps in that header:

* Author / Process Script / Export Script / Max Filepath are **ExportStrings**:
  a single BYTE length that counts the trailing NUL. The block-type names two
  fields later are **uint32**-prefixed SizedStrings. Getting this wrong reads a
  640 MB length out of a 77 KB file (MISTAKES.md 2026-09-10);
* FO4 shapes name their material as a **string-table entry** on the
  `BSLightingShaderProperty` (`Materials\...\x.bgsm`), so it shows up in
  `--strings`. A shape with an EMPTY material name keeps its texture paths
  INLINE in the `BSShaderTextureSet` instead (`MaleBrows.nif` does) — grep the
  raw bytes for `.dds` to find those.

`BSSkin::Instance` body: `i32 skeletonRoot, i32 dataRef, u32 nBones,
i32 bones[nBones], u32 nScales, f32 scales[nScales]`.
`BSSkin::BoneData`: `u32 nBones`, then per bone 17 floats (bounding sphere 4,
rotation 3x3, translation 3, scale 1) = 68 bytes.

## 5. Materials: the "missing" that is not missing

`nifskope-cli check` says `Material file is missing … not found in archives`
for every vanilla BGSM. That is the app's archive configuration, not the
fixture — **run `check` on an untouched donor and watch the same line appear**
before reporting it. Resolve the paths yourself against
`…\DataUnpacked\Data\` to say whether they really exist.

Skin tint: a merged head carries its donor's material
(`basehumanskinHead.bgsm`) and nothing else. The tint is applied by the engine
at run time from the actor record; a fixture that merges no FaceGen head data
and no chargen `.tri` is the neutral, unsculpted base head, and that is what to
say in the README rather than "default tint".

## 6. What ships beside the file

A `README.md` next to every copy: the part table with source paths, the parts
LEFT OUT with reasons, the skeleton, the material list with its resolution
check, the exact regeneration command, and the gate table with its numbers.
The point is that the file regenerates from the README alone.
