# CHANGE_NEEDED — what another lane owns, from lane HKX4b (2026-09-10)

These are BEHAVIOUR changes in files this lane may not touch (one lane per
file, CONSTITUTION 1). They are not hook-ups and a resume must not apply them:
each names the mechanism, what would prove it, and who owns it.

---

## C1 — the Animation workspace registers the clip provider (owner: lane HKX3)

**Mechanism.** The glTF menu entry has no way of its own to know which `.hkx`
the user has loaded; that list is the Animation workspace's. `src/gltfexportnif.h`
declares the seam:

```cpp
typedef bool ( *GltfExportClipProvider )( HkxAnimClip & clip, QStringList & boneNames );
void gltfExportSetClipProvider( GltfExportClipProvider fn );
```

HKX3 calls `gltfExportSetClipProvider()` once, with a function that fills the
clip currently selected in the animations list and the bone names of the
skeleton it was bound to, and returns false when nothing is selected.

**Until it does**, `gltfExportClipProvider()` is null and
`src/lib/importex/gltfanim.cpp` exports the character with **no animation**
and says so in its own message box:

> NO ANIMATION was written: no clip is loaded in the Animation workspace.

That is deliberate: a menu entry that quietly writes a file without the clip
is worse than one that refuses to pretend.

**The gate.** Load a clip in the workspace, export, and run
`tests/spells/gltf_readback.py` on the result with `--clip` naming the same
`.hkx`. Gate R4 reproduces every bone's TRS every frame; if the provider hands
over the wrong clip, or the wrong skeleton's names, R4 goes red by name.

**What must NOT be done instead:** copying the export code into the workspace,
or giving `gltfexportnif.cpp` a dependency on the workspace's headers. The
seam is a function pointer for exactly that reason — `src/gltfexport.cpp` and
`src/gltfexportnif.cpp` stay linkable with no GUI, which is what lets the
gates run without building NifSkope.

---

## C2 — `fixtures/human_male_vanilla.nif`: `LLeg_Toe1` is 2.865 units out
(owner: lane FIXTURE, or whoever next rebuilds the fixture)

**Measurement.** For every skin bone, `global(bone) · storedBoneTransform`
must be one and the same rigid transform (contract section 6). On Bethesda's
own `meshes/actors/character/characterassets/MaleBody.nif` all 58 bones agree
to **0.00098 units**. On the assembled fixture the same shape's bones agree to
0.0016 **except** `LLeg_Toe1` at **2.8646 units** (then `RLeg_Toe1` 0.107,
`RLeg_Foot` 0.052, `LLeg_Foot` 0.050); the other eight shapes are at 0.0016 or
better.

**Cause, named as a candidate, not a fact.** The stored bone transforms were
copied from the donor NIFs unchanged, so the difference is in the NODE pose:
`skeleton.nif` places `LLeg_Toe1` where `MaleBody.nif`'s bind pose does not.
Both files are Bethesda's. The discriminator is a direct comparison of the two
files' global transform for that one node — one line of
`tests/spells/gltf_nifread.py` plus the chain walk in
`tests/spells/gltf_readback.py`.

**Consequence if left.** In Blender the fixture's left toe is displaced by
2.9 units (4 cm). Everything else is right. `tests/spells/gltf_gates.sh`
registers it as 2 expected failures, so it cannot rot into an unnoticed pass;
whoever fixes the fixture must also drop the "2 registered" in that script back
to 0.

---

## C3 — `.dds` textures do not resolve in Blender (owner: nobody yet; bungo's call)

**Mechanism.** glTF's specification allows only PNG and JPEG images. FO4 ships
`.dds`, and this exporter writes the game-relative path as an external `uri`,
so Blender prints `Cannot read ...` once per material and imports the model
with the material present and the image empty.

**The three ways out, none taken:**

1. do nothing — the user points Blender at the textures himself. What ships;
2. convert the referenced `.dds` to `.png` beside the `.gltf` on export. The
   tree already decodes DDS (`src/ddstxt16.hpp`, used by the upstream
   exporter), so this is a small addition, but it makes the export write files
   the user did not ask for;
3. embed them in the `.bin` as the upstream exporter does — which is that
   exporter's whole point, and is why it still exists.

**Not a defect and not to be "fixed" quietly.** It is listed in section 9 of
`docs/GLTF_INTERCHANGE.md` as a named loss, and `tests/spells/gltf_check.py`
reports it as a WARNING with the reason, never as a failure.
