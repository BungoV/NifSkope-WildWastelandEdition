# `human_male_vanilla.nif` — the vanilla Fallout 4 human male, one file, one rig

Assembled 2026-09-10 by lane FIXTURE for the HKX animation-preview work
(HANDOFF.md, "HKX ANIMATION PREVIEW"). Nothing here is authored: every block
comes verbatim out of Bethesda's shipped meshes, merged onto the player
skeleton so the whole model shares one bone tree and can be posed as one rig.

A second copy lives at `E:\Projects\Fallout 4 Mods\HumanMaleVanilla\`
(bungo, 2026-09-10: *"Also, save the human model for later use for me"*).

| | |
|---|---|
| file | `human_male_vanilla.nif`, 338,563 bytes, md5 `8ab3d0dd5ea57efc8cddad579bef6cf8` |
| format | NIF 20.2.0.7, user 12, BSVER 130 (Fallout 4) |
| blocks | 215 — 130 `NiNode`, 9 `BSSubIndexTriShape`, 9 `BSSkin::Instance`, 9 `BSSkin::BoneData`, 8 `BSLightingShaderProperty`, 1 `BSEffectShaderProperty`, 8 `BSShaderTextureSet`, 3 `NiAlphaProperty`, 19 `bhkNPCollisionObject`, 1 `bhkPhysicsSystem`, 1 `bhkRagdollSystem`, 4 × `NiTransformController`/`Interpolator`/`Data`, `BSBoneLODExtraData`, `BSBound`, `BSConnectPoint::Parents`, `BSXFlags`, `BSEyeCenterExtraData` |
| root | one — the `NiNode` named `skeleton.nif` (skeleton.nif's own root; every donor root was dropped and its children re-parented) |
| pose | the vanilla bind pose. Every `BSSkin::BoneData` block is **byte-identical** to its donor's |

## Corpus

`E:\Tools\Fallout 4\DataUnpacked\Data` — the unpacked FO4 corpus, never a mod
folder. All source paths below are relative to
`…\Data\meshes\actors\character\characterassets\`.

## The rig

`skeleton.nif` (the player skeleton, 129 `NiNode`s) is the merge TARGET, so the
fixture carries its complete node set — bones, `*_skin` helper nodes,
`CamTargetParent`, `CharacterBumper`, the ragdoll/collision blocks, the bone-LOD
extra data and the connect points. The merged fixture's node set is
**skeleton.nif's set exactly, plus one node**: `EyeLeftDummy001`, which
`MaleEyes.nif` brings with it as its eye-centre marker.

Against `skeleton.hkx` (the animation skeleton, 95 bones), matching
case-insensitively: **78 of 95 bones have a node here**; the 17 that do not are
`WeaponBolt`, `WeaponExtra1-3`, `WeaponIKTargetL/R(+Mirror)`, `WeaponMagazine`,
`WeaponMagazineChild1-5`, `WeaponOptics1-2`, `WeaponTrigger` — weapon nodes that
live on the weapon NIF, not on a body. Four more names differ only in CASE
between the two files (`Head`/`HEAD`, `Spine1`/`SPINE1`, `Spine2`/`SPINE2`,
`Weapon`/`WEAPON`), which is why bone matching must be case-insensitive.

## Parts included (9 shapes)

| shape | source file | why |
|---|---|---|
| `BaseMaleBody:0` | `MaleBody.nif` | the body |
| `BaseMaleHands3rd:0` | `MaleHands.nif` | the hands (the body mesh ends at the wrists) |
| `BaseMaleHead:0` | `BaseMaleHead.nif` | the face / front of the head |
| `MaleHeadRear:0` | `FaceParts\maleheadRear.nif` | the rear of the head |
| `MaleEyes:0` | `FaceParts\MaleEyes.nif` | the eyeballs |
| `MaleEyesAO:0` | `FaceParts\MaleEyesAO.nif` | the eye ambient-occlusion layer (`BSEffectShaderProperty`) |
| `MaleEyesWet:0` | `FaceParts\MaleEyesWet.nif` | the eye wet/gloss layer |
| `MouthHuman:0` | `FaceParts\MaleMouth.nif` | mouth interior, teeth and tongue |
| `MaleBrows` | `FaceParts\MaleBrows.nif` | eyebrows |

## Parts LEFT OUT, and why

| left out | why |
|---|---|
| every `*_faceBones.nif` (`BaseMaleHead_faceBones`, `MaleMouth_faceBones`, `MaleEyes_faceBones`, `maleheadRear_faceBones`, …) and `skeleton_faceBones.nif` | these are the **chargen sculpt rig**, not the animation rig: they are skinned to a different skeleton (~60 `skin_bone_*` face bones that `skeleton.nif` does not have) and they carry the `CustomizationRemapData` / `CustomizationRemapNewBonesData` binary blobs. The `nif` skill's face-customization reference gives the rule for a mesh that needs no chargen support: **omit entirely**. Merging them would also have broken the "node set == skeleton.nif's set" property |
| `MaleNeckGore.nif` | the decapitation stump — only shown when the head comes off |
| `1stPersonMaleBody.nif`, `1stPersonMaleHands.nif`, `1stPersonReverseArm.nif` | the first-person arms rig, a different model |
| `OldMaleBody/Head/Hands.nif`, `MaleGhoul*`, `Child*`, `SynthGen2Head1.nif` | other races / ages, not the vanilla human male |
| `FaceParts\MaleEyesLeft.nif`, `MaleEyesGlossLeft.nif`, `MaleEyesAOLeft.nif`, `MaleEyesWetLeft.nif`, `EyeLashesLeft.nif` | the chargen per-eye (heterochromia) variants; the paired `MaleEyes*` meshes above already cover both eyes |
| male eyelashes | there is no adult-male lash mesh in the corpus — only `FemaleLashes.nif`, `EyeLashesLeft.nif` and the child/ghoul ones |
| hair (`Hair\`), beards (`Beards\`), body tattoos (`BodyTattoos\`) | chargen choices, not part of the base body |
| `FaceParts\MaleEyesShade.nif` | despite the name this is not a face part but one of Bethesda's own scratch preview assemblies (9 shapes, 63 nodes, body + head + `hair01`). Useful as a sanity reference for what a combined male preview looks like; not a source here |
| `.tri` morph files, `FaceGenData\` | chargen morph targets and per-character FaceGen head data — the face here is the untinted, unsculpted base head |

## Materials and skin tint

The face keeps **BaseMaleHead's own default material**,
`Materials\Actors\Character\BaseHumanMale\basehumanskinHead.bgsm`, exactly as
shipped — no tint was applied, no FaceGen head data and no chargen `.tri` morph
was merged, so the head is the neutral base male head the engine would tint at
run time from the actor record. Every donor's `BSLightingShaderProperty`,
`BSEffectShaderProperty` and `BSShaderTextureSet` block came across unchanged
(same count, same sizes). The materials referenced, all present in the corpus:

```
Materials\actors\Character\BaseHumanMale\basehumanskin.bgsm          (body, head rear)
Materials\actors\Character\BaseHumanMale\basehumanmaleskinhands.bgsm (hands)
Materials\Actors\Character\BaseHumanMale\basehumanskinHead.bgsm      (head)
Materials\Actors\Character\HumanCommon\Eyes.BGSM                     (eyes)
Materials\Actors\Character\HumanCommon\EyeAO.BGEM                    (eye AO)
Materials\Actors\Character\HumanCommon\EyeWet.BGSM                   (eye wet)
Materials\Actors\Character\HumanCommon\Mouth.BGSM                    (mouth)
Materials\template\SkinTemplate_Wet.bgsm                             (wet-material slot on the four skin shapes)
Textures\Actors\Character\HumanCommon\MaleBrows01_d.dds + FemaleBrows_n.dds  (brows, inline texture set, no BGSM)
```

`nifskope-cli check` reports "Material file is missing … not found in archives"
for these. That is NifSkope's archive configuration, not this file: the same
message fires on the untouched vanilla `MaleBody.nif` and `MaleEyes.nif`. Every
one of the paths above resolves under `…\DataUnpacked\Data`.

## Regenerating it

One command. The output path must be **absolute** — a relative `-o` resolves
against the exe's folder and the save fails.

```bash
cd E:/Projects/NifskopeWildWastelandEdition
D="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/character/characterassets"
./release/nifskope-cli.cmd merge "$D/skeleton.nif" \
    --add "$D/MaleBody.nif" \
    --add "$D/MaleHands.nif" \
    --add "$D/BaseMaleHead.nif" \
    --add "$D/FaceParts/maleheadRear.nif" \
    --add "$D/FaceParts/MaleEyes.nif" \
    --add "$D/FaceParts/MaleEyesAO.nif" \
    --add "$D/FaceParts/MaleEyesWet.nif" \
    --add "$D/FaceParts/MaleMouth.nif" \
    --add "$D/FaceParts/MaleBrows.nif" \
    -o "E:/Projects/NifskopeWildWastelandEdition/fixtures/human_male_vanilla.nif"
```

It prints `total: 9 shape(s) added, 122 node(s) shared with the target` and
`no duplicate bone names introduced`. Only one node is genuinely new
(`EyeLeftDummy001`); every other donor bone was de-duplicated onto the
skeleton's, which is what makes the whole model one poseable rig.

## What was measured (gates, all with a control that fails)

| gate | result |
|---|---|
| block-count arithmetic: `skeleton.nif` 166 + 5+5+5+5+7+5+6+5+6 | 215, and the file reads back as 215 |
| skin partitions: `segments` on the fixture vs the nine donors, block indices stripped | **byte-identical**, 68 lines each |
| `BSSkin::BoneData` blocks (pure float bind-pose data, no refs) | md5 multiset identical, 9/9; a `FemaleBody.nif` block is not in the set (floor) |
| every skin's `Bones[]` array, IN ORDER, donor vs fixture | identical on all 9; `FemaleBody.nif`'s list is not (floor) |
| every `Bones[]` entry points at a real `NiNode`; the declared block size is consumed exactly by the layout | 9/9 skins, 0 bad refs, 0 bytes left over |
| node set vs `skeleton.nif` | 0 missing, 1 added (`EyeLeftDummy001`), 0 duplicate names |
| `nifskope-cli check` | 34 findings, every class of which also fires on the untouched vanilla donors (missing-material archive lookup, collision on visible geometry, `.ssf` sidecar naming) |

PENDING, because Fallout4.exe came up at 05:32 and the exe may not be launched:
the two render-hook pictures (bind pose, front and side) into this folder. The
resume is `scratchpad/fixture_20260910/PENDING.md`.

## The animation clip beside it

`Running_To_Slide_And_Back_To_Running.hkx` (40,000 bytes, md5
`c62b7f45a30132d30fa730fab70347c8`), copied verbatim from
`C:\Users\bungo\Downloads\Animations - Mixamo Collection\Data\Meshes\Actors\Character\animations\AnimPreviews\`.
See `scratchpad/lane_fixture_report.md` §2 for the decode: it IS a supported
FO4 spline clip (THREECOMP40 + 16-bit, 95 tracks, 93 frames, 60 fps), but its
`hkaAnimationBinding` carries an EMPTY `transformTrackToBoneIndices`, which
`hkxAnimLoad` currently refuses.
