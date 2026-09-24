# Lane FIXTURE — the vanilla human male + the Mixamo clip (2026-09-10)

Repo `E:\Projects\NifskopeWildWastelandEdition`, main, **nothing committed**.
bungo's words, verbatim: *"Vanilla human male, whole model (body, rear of the
head, face) with this: C:\Users\bungo\Downloads\Animations - Mixamo
Collection\Data\Meshes\Actors\Character\animations\AnimPreviews\Running_To_Slide_And_Back_To_Running.hkx.
Also, save the human model for later use for me"*.

Skills invoked: `nif`, `ww-hkx-animation`, `nifskope-ww-render-shot`,
`nifskope-ww-resume-pending`, plus the `nif` skill's
`references/face-customization-remap.md`.

Delivered:

| what | where |
|---|---|
| the model | `E:\Projects\NifskopeWildWastelandEdition\fixtures\human_male_vanilla.nif` |
| the copy bungo asked to keep | `E:\Projects\Fallout 4 Mods\HumanMaleVanilla\human_male_vanilla.nif` |
| README beside each | `fixtures\README.md`, `…\HumanMaleVanilla\README.md` (identical, 9,675 bytes, LF-only) |
| the clip | `fixtures\Running_To_Slide_And_Back_To_Running.hkx` |
| the clip decoded | `scratchpad\fixture_20260910\Running_To_Slide_And_Back_To_Running.tsv` (8,928 rows) |
| PENDING (the two pictures) | `scratchpad\fixture_20260910\PENDING.md` |

Both NIF copies md5 `8ab3d0dd5ea57efc8cddad579bef6cf8`, 338,563 bytes.
The clip is a verbatim copy, md5 `c62b7f45a30132d30fa730fab70347c8`, 40,000 bytes.

## 1. The model

`nifskope-cli merge` with **`skeleton.nif` as the merge TARGET** and the nine
part meshes as donors. `merge` de-duplicates `NiNode`s by name, so every donor
bone maps onto the skeleton's own node and every skin's `Bones[]` array is
re-pointed at the shared bones — the whole model is one poseable rig with one
root. The exact command is in `fixtures/README.md` ("Regenerating it") and
reproduces the file.

Merge summary, read back: `total: 9 shape(s) added, 122 node(s) shared with the
target`, `no duplicate bone names introduced`, one node genuinely added.

### Parts INCLUDED (9 shapes)

Corpus `E:\Tools\Fallout 4\DataUnpacked\Data\meshes\actors\character\characterassets\`.

| shape | source | why |
|---|---|---|
| — (rig) | `skeleton.nif` | the player skeleton, 129 `NiNode`s; merge target |
| `BaseMaleBody:0` | `MaleBody.nif` | the body |
| `BaseMaleHands3rd:0` | `MaleHands.nif` | the hands — the body mesh ends at the wrists |
| `BaseMaleHead:0` | `BaseMaleHead.nif` | the face / front of the head |
| `MaleHeadRear:0` | `FaceParts\maleheadRear.nif` | the rear of the head (bungo named it) |
| `MaleEyes:0` | `FaceParts\MaleEyes.nif` | the eyeballs |
| `MaleEyesAO:0` | `FaceParts\MaleEyesAO.nif` | eye ambient-occlusion layer (`BSEffectShaderProperty`) |
| `MaleEyesWet:0` | `FaceParts\MaleEyesWet.nif` | eye wet/gloss layer |
| `MouthHuman:0` | `FaceParts\MaleMouth.nif` | mouth interior, teeth, tongue |
| `MaleBrows` | `FaceParts\MaleBrows.nif` | eyebrows; no BGSM, textures inline in its `BSShaderTextureSet` |

### Parts EXCLUDED

| left out | why |
|---|---|
| every `*_faceBones.nif` + `skeleton_faceBones.nif` | the chargen SCULPT rig, not the animation rig: a different skeleton (~60 `skin_bone_*` face bones absent from `skeleton.nif`) carrying `CustomizationRemapData` / `CustomizationRemapNewBonesData`. The `nif` skill's face-customization reference gives the rule for a mesh needing no chargen support — **omit entirely**. Merging one would also have broken the node-set gate below |
| `MaleNeckGore.nif` | the decapitation stump |
| `1stPersonMaleBody/Hands.nif`, `1stPersonReverseArm.nif` | the first-person arms rig |
| `OldMale*`, `MaleGhoul*`, `Child*`, `SynthGen2Head1.nif` | other races / ages |
| `MaleEyesLeft`, `MaleEyesGlossLeft`, `MaleEyesAOLeft`, `MaleEyesWetLeft`, `EyeLashesLeft` | chargen per-eye (heterochromia) variants; the unsuffixed meshes already cover both eyes |
| male eyelashes | none exists — the corpus has `FemaleLashes.nif`, `EyeLashesLeft.nif` and the child/ghoul ones only |
| `Hair\`, `Beards\`, `BodyTattoos\` | chargen choices, not the base body |
| `FaceParts\MaleEyesShade.nif` | **not a face part** despite the name: one of Bethesda's own scratch preview assemblies (9 shapes, 63 nodes, body + hands + head + `MaleEyesGloss` + `EyeLashes` + `hair01`). Read as a sanity reference for what a combined preview contains; not merged |
| `.tri` morphs, `FaceGenData\` | chargen morph targets and per-character FaceGen head data |

### The rig, and the two HKX1 truths reproduced

The fixture's node set is **`skeleton.nif`'s set exactly (0 missing) plus one
node**, `EyeLeftDummy001`, which `MaleEyes.nif` brings as its eye-centre marker.
No duplicate names.

Against `skeleton.hkx` (95 bones), case-insensitively: **78 of 95 matched**.
The 17 unmatched are exactly HKX1's list — `WeaponBolt`, `WeaponExtra1-3`,
`WeaponIKTargetL/R(+Mirror)`, `WeaponMagazine`, `WeaponMagazineChild1-5`,
`WeaponOptics1-2`, `WeaponTrigger` — and the four case-only differences are
exactly HKX1's — `Head`/`HEAD`, `Spine1`/`SPINE1`, `Spine2`/`SPINE2`,
`Weapon`/`WEAPON`. Both independently re-measured here by
`scratchpad/fixture_20260910/bones_compare.py`, not copied from the report.

### Material and skin tint

The face keeps **BaseMaleHead's own default material**,
`Materials\Actors\Character\BaseHumanMale\basehumanskinHead.bgsm`, unchanged.
No tint was applied, no FaceGen head data or chargen `.tri` was merged: the head
is the neutral, unsculpted base male head that the engine tints at run time from
the actor record. Every donor's shader and texture-set block came across with
the same count and the same sizes. All eight BGSM/BGEM paths plus the two brow
`.dds` resolve under `…\DataUnpacked\Data` (listed in the README).

`nifskope-cli check` says `Material file is missing … not found in archives` for
them — that is NifSkope's archive configuration, proven by running `check` on
the untouched vanilla `MaleBody.nif` and `MaleEyes.nif`, where the same line
fires.

## 2. The clip

`fixtures\Running_To_Slide_And_Back_To_Running.hkx`, decoded with HKX1's
standalone tools only (`release/hkxanim_dump.exe` and
`tests/spells/hkxanim_decode.py`; NifSkope.exe was never used for this).

| | |
|---|---|
| container | Havok 2014.1.0-r1 binary packfile, little-endian; section headers at `0x40 + u16@0x3e` = 0x50 |
| objects | 5: `hkRootLevelContainer`, `hkaAnimationContainer`, `hkaSplineCompressedAnimation`, `hkaDefaultAnimatedReferenceFrame`, `hkaAnimationBinding`. Vanilla clips carry a 6th, `hkMemoryResourceContainer` — cosmetic |
| type | `HK_SPLINE_COMPRESSED_ANIMATION` — **a supported format** |
| frames | **93** |
| duration | **1.533333 s**; `frameDuration` 0.0166667 = **60 fps** (vanilla third-person clips are 30) |
| blocks | 1, `maxFramesPerBlock` 256, `maskAndQuantizationSize` 380 = 4 × 95, data 32,432 bytes |
| transform tracks | **95** |
| float tracks | 0 |
| quantization | **THREECOMP40 rotation + 16-bit translation + 16-bit scale on all 95 tracks** — inside FO4's own census (THREECOMP40/48 + 16-bit, nothing else) |
| blend hint | NORMAL (a pose, not an additive delta) |
| `originalSkeletonName` | `"Root"` — the FO4 player skeleton |
| root motion | **present as an object, and identically zero.** 93 samples, up = (0,0,1), max abs component over every sample and every axis **0.000000**. The clip's travel lives on the `COM` track instead: y goes 0 → **486.98** units, x −13.64…2.47, z 14.89…64.26 |
| decode | the whole clip decodes: 8,835 track rows + 93 root rows, the block walk ending exactly at the block's float offset |

### Which tracks match the skeleton's bones

The file carries **no track→bone map**: `transformTrackToBoneIndices` is empty
(bytes at binding +0x20: `count=0`, `capflags=0x80000000`, no local fixup — the
array is genuinely absent, not a parse failure; vanilla `jog.hkx` has
`count=95` with a payload). So are `floatTrackToFloatSlotIndices` and
`partitionIndices`.

The mapping is therefore **identity, and that was measured, not assumed**: in a
skinned rig every joint's local translation is fixed by the skeleton, so frame
0's per-track translation must equal `referencePose[i].translation`.

| mapping | tracks whose frame-0 translation matches within 1e-3 |
|---|---|
| **track i → bone i** | **75 / 95** |
| track i → bone i+1 | 18 / 95 |
| track i → bone i+2 | 18 / 95 |
| track i → bone i−1 | 18 / 95 |

The shifted rows are the control, and they fail. The 20 tracks that differ under
the identity map are all ones that are *supposed* to: `COM` (6.59, the clip
travels), the animation-placed nodes `WeaponLeft` `Weapon` `WeaponBolt`
`WeaponExtra1` `WeaponMagazine` `WeaponMagazineChild1-5` `WeaponOptics1`
`WeaponTrigger` `Camera` `CamTarget`, `Spine1` (0.40), and four finger tips at
0.0015 (float noise).

Against **this fixture's own nodes**, case-insensitively:

* **78 of the 95 tracks have a node in `human_male_vanilla.nif`** and will play;
* **17 do not**, and they are the same 17 `Weapon*` bones — they belong on a
  weapon NIF. This is bungo's "partial match = play the matched bones, list the
  unmatched", with the list.

### Is it supported? Yes — but HKX1's reader refuses it today

Format, quantization, block layout, knots and root motion are all inside the
contract. The refusal is one line of validation:

```
REFUSED: binding maps 0 tracks, the animation has 95
```

from `hkxanim_decode.py::validate` and, identically, from
`release/hkxanim_dump.exe`. **What the reader would need: one rule — an empty
`transformTrackToBoneIndices` means the identity map**, accepted when the bound
skeleton has at least `numberOfTransformTracks` bones and refused by name when
it does not. Nothing else about the file needs new code. That is a change to
`src/hkxanim.cpp` + `tests/spells/hkxanim_decode.py`, which are **HKX2's files,
not this lane's** (one lane per file), so it was not made. The Python-side proof
that the rule is sufficient is
`scratchpad/fixture_20260910/clip_dump_identity.py`, which decodes the whole
clip under that rule and writes the standard TSV.

## 3. Gates

Pre-registered in the brief: *the fixture loads back headless with the block
count and every skin partition intact; bone name set == skeleton.nif's set minus
the stated exclusions; render-hook pictures front and side.*

| gate | result |
|---|---|
| block-count arithmetic: 166 (`skeleton.nif`) + 5+5+5+5+7+5+6+5+6 | **215**, and `nifskope-cli info` reads back 215 |
| block tally | 130 `NiNode`, 9 `BSSubIndexTriShape`, 9 `BSSkin::Instance`, 9 `BSSkin::BoneData`, 8 `BSLightingShaderProperty`, 1 `BSEffectShaderProperty`, 8 `BSShaderTextureSet`, 3 `NiAlphaProperty`, 19 `bhkNPCollisionObject`, ragdoll + physics + bone-LOD + bound + connect points + `BSXFlags` + `BSEyeCenterExtraData` |
| **skin partitions intact** — `segments` on the fixture vs all nine donors, block indices stripped | **byte-identical, 68 lines each**. Every segment, subsegment, triangle range, UI index and bone name survived |
| **bind pose intact** — md5 of every `BSSkin::BoneData` block (pure floats: no refs, no string indices) | **9/9 identical to the donors**. FLOOR: `FemaleBody.nif`'s BoneData md5 is not in the set |
| **bone lists intact IN ORDER** (vertex weights are indices into `Bones[]`) | **identical on all 9 skins**. FLOOR: `FemaleBody.nif`'s list is not equal |
| skin layout sane | 9/9 skins: every `Bones[]` entry resolves to a real `NiNode`, 0 bad refs, and `12 + 4·nBones + 4 + 4·nScales` consumes the declared block size **exactly** (a wrong layout leaves bytes over) |
| all skeleton roots re-pointed | 9/9 skins now name the shared root `skeleton.nif`, not their donor roots |
| node set vs `skeleton.nif` | **0 missing, 1 added (`EyeLeftDummy001`), 0 duplicate names** |
| hkx bones matched by a fixture node | **78 / 95**; the 17 unmatched named above |
| `nifskope-cli check` | 34 findings; **every class also fires on the untouched vanilla donors** (missing-material archive lookup, "visible geometry has no collision", `.ssf` sidecar naming — `FacePssf` appears on vanilla `MaleEyes.nif` too). Nothing new was introduced |
| **render-hook pictures, front and side** | **PENDING — not taken.** `Fallout4.exe` came up at 05:32:15 (`tasklist` rc flipped to 0 mid-lane) and the exe may not be launched. Resume: `scratchpad/fixture_20260910/PENDING.md`. No build is needed; the exe (03:57) already served the merge and the read-backs |

Tools written for the gates, none of which needs NifSkope, so they still run
while the game is up: `scratchpad/fixture_20260910/nifhdr.py` (FO4 NIF header +
node names), `skin_check.py`, `bones_compare.py`, `clip_raw.py`,
`clip_tracks.py`, `clip_inspect.py`, `clip_dump_identity.py`.

## 4. Mistakes (also appended to root `MISTAKES.md`)

1. **`nifskope-cli merge -o` with a RELATIVE path fails the save.** The first
   merge reported every step successful and ended `error: failed to save
   scratchpad/fixture_20260910/trial1.nif`; no file appeared. A relative path
   resolves against the EXE's folder — the same rule already recorded for
   `WW_RENDER_SHOT` / `WW_IMPOSTOR_BAKE` / `SHOT=` / `WW_WATER_MARK_SHOT`. It is
   one rule about the process, and `-o` is its fifth site.
2. **A Havok packfile section table typed from a hexdump instead of taken from
   our own tree.** `clip_raw.py` was first written with a 0x30 stride and the
   fixups at +0x10; the real stride is 0x40 with seven offsets at +20, and that
   was already working in `tests/spells/hkxanim_decode.py::_packfile`, in a file
   this lane was already importing. CONSTITUTION 4, third rule of
   2026-09-04 21:33.
3. **The NIF header's export strings read as uint32-prefixed.** They are
   ExportStrings — one BYTE length including the NUL — while the block-type
   names two fields later are uint32. `nifhdr.py` read a 671,342,919-byte length
   out of a 77 KB file before a hexdump settled it.
4. **A bone-name SET match was nearly reported as proof the skinning survived.**
   It is not: vertex weights are indices into `Bones[]`, so only an ORDERED
   comparison proves it. The ordered check was added, with a `FemaleBody.nif`
   floor that fails.

## 5. Finished-work skill review

**Loaded and used in earnest.** `nif` — the `merge` semantics (de-duplication by
name is the whole reason this is one command rather than a scripted splice), the
"corpus, never a mod folder" rule, and `references/face-customization-remap.md`,
which is what settled the `_faceBones` question in one read instead of an
experiment. `ww-hkx-animation` — the 0x3e padding, the fixture set, the gate
commands, and section 8's settled facts, which meant nothing about the clip
format had to be re-derived. `nifskope-ww-resume-pending` — the shape of the
PENDING file and the rule that a lane which cannot run the exe delivers a
paste-able resume. `nifskope-ww-render-shot` — read in full before writing the
PENDING resume, so the render command in it already carries the absolute-path,
size-clamp, `rc=124` and one-instance traps rather than discovering them later.

**Written this session.**
`.claude/skills/ww-character-fixture-assemble/SKILL.md` (repo tree — the
director should mirror it to the live tree). Assembling a whole FO4 character
from Bethesda's shipped parts was worked out here from first principles and will
recur immediately (the female, the ghoul, an armour set, a weapon + body pair
for the 17 `Weapon*` tracks). It carries the part list, the four categories of
thing that LOOKS like a part and is not, the merge-onto-the-skeleton command,
the six gates with their floors, the two NIF-header traps, and the "missing
material that is not missing" control.

**Amended.** `.claude/skills/ww-hkx-animation/SKILL.md` gained section 10, the
empty-binding finding: what a third-party clip can carry, the identity test with
its shifted control, "root motion present and zero", and the one rule a reader
needs. Appended only — another lane is writing into that file.

**Declined.** No skill for "decode one clip and report it": `ww-hkx-animation`
already covers it, and the listing gained `fo4-measure-animation-clip` mid-lane
which covers the pose-measurement half. Nothing here was re-derived that those
two do not hold.
