## 2026-09-10 — glTF 2.0 export: a clip and the character, in one file Blender opens (lane HKX4b)

bungo's ruling of 2026-09-10, verbatim — *"gltf sounds good"* — settled the
animation interchange format. This is the export half: a Fallout 4 character
(node tree, skinned meshes, skin) plus one Havok `.hkx` clip, written as a
`.gltf` + `.bin` pair that Blender opens natively with a working armature and
a playing action. The import half is lane HKX5.

**The contract is `docs/GLTF_INTERCHANGE.md`** — axes, units, bone order, the
channels, and every loss named one by one, with 31 provenance anchors
re-derived from the sources in one scripted pass.

**Why a second exporter.** `src/lib/importex/gltf.cpp` stays the exporter for a
static scene: it embeds textures as PNG, carries the full FO4/FO76/Starfield
material sets, LODs and `MSFT_lod`. It cannot write an animation — its
`animations` array is never populated — and it reaches the model only through
the GL `Scene`, so nothing it does can be gated without building the whole
application. The new writer is deliberately the other shape: a neutral struct
in, two files out, QtCore only, which is what let this entire lane be measured
with `release/NifSkope.exe` untouched. The two agree on the conventions a user
can see — 1 unit = 0.9144/64 m and the Z-up → Y-up rotation on a synthetic root
node — so a mesh from either lands in the same place in Blender.

**What ships.** `src/gltfexport.{h,cpp}` (the writer), `src/gltfexportnif.{h,cpp}`
(`NifModel` → the struct, and `HkxAnimClip` → a glTF animation),
`src/lib/importex/gltfanim.cpp` (the menu entry), `tests/gltfexport_dump.cpp`
(the standalone driver, with its own container-level NIF reader so the gate has
an independent C++ producer), and six gate scripts under `tests/spells/`:
`gltf_check.py`, `gltf_sabotage.py`, `gltf_nifread.py`, `gltf_readback.py`,
`gltf_blender_check.py` and `gltf_gates.sh`.

**The gates**, `bash tests/spells/gltf_gates.sh`, 2026-09-10 13:55 —
**0 gate(s) not as registered**:

* structural validator **6,411 checks / 0 failures** on the jog export, the
  same on the Mixamo export, 688 / 0 on Bethesda's own `MaleBody.nif`; its
  floor is 12 deliberately broken copies, **12 of 12 refused**;
* the independent read-back **3,234 checks**, worst animation error **4.20e-6
  units** on translation and **2.34e-6 deg** on rotation over 234 channels ×
  23 frames, scale exact (asked for: 1e-4 and 0.01 deg); its floor is 7
  sabotages, every one red, each hitting the gates it should;
* mesh: 9 shapes, **6,443 vertices, 11,375 triangles**, every index identical
  to the NIF's, weight rows 0.999695 … 1.000397 against 1e-3;
* **Blender 4.5, headless**, its own Khronos importer, expectations read out of
  the file rather than typed in: armature `COM` **110 bones**, **9 of 9 meshes**
  matched by name with the file's counts, action `JogForward` 0.000..17.600
  frames (23 at 30 fps), 790 fcurves. The 60 fps Mixamo clip: 0.000..36.800.

**Two things the gates settled that nobody had written down.**

*The inverse-bind convention.* The textbook invariant — skinning a bind-pose
mesh returns the mesh — fails by 1.727 m on every FO4 body shape, and the gate
was what was wrong. `global(bone) × storedBoneTransform` on Bethesda's own
`MaleBody.nif` is one rigid transform for all 58 bones,
`translate(-0.0002, -0.8818, +120.8437)` units, spread 0.00098: **FO4 body
meshes store their vertices with the origin at the top of the head**
(`BaseMaleBody:0` spans z = -120.25 … -5.688) and the skin stands the model on
its feet. So `BSSkinBoneTrans` IS the inverse bind matrix, written straight
through with only its translation scaled — now with a number behind it.

*How the dismemberment segments merge.* They are draw ranges over the one
vertex and index buffer, so every segment belongs to a single glTF primitive
and no merge decision exists. The arithmetic check found the rule: a segment's
**sub-segments re-describe its own range, they do not extend it** — counting
both levels on `BaseMaleBody:0` gave 4,351 triangles against 2,698 real ones.
The table goes verbatim into `extras.mergedPartitions`.

**A fixture truth, failing as pre-registered.** Gate R5 requires every joint
matrix to agree. On `fixtures/human_male_vanilla.nif` eight of nine shapes
agree to 0.0016 units and the body's **`LLeg_Toe1` is 2.8646 units out**:
`skeleton.nif` poses that one bone differently from the pose `MaleBody.nif`'s
skin was authored against. Both files are Bethesda's; the vanilla donor exports
clean (1,246 checks / 0). The runner registers it as 2 expected failures so it
cannot rot into a pass.

**Four defects were found in code that had already produced a file Blender
opened happily**, which is the whole argument for the read-back gate: three
unsequenced reads in one argument list that reversed every vector the driver
read (caught by gate R2, 231 failures); a Python gate reader that had never
been executed and crashed on its first shader block; a texture-set lookup that
was right by accident and silently missed every `BSEffectShaderProperty`; and
two empty-array spec violations on paths the fixtures do not take. All five
entries are in `MISTAKES.md`.

**Named losses**, all in section 9 of the contract: the texture bytes (glTF
allows only PNG/JPEG, FO4 ships `.dds` — the path survives in the uri and in
`extras.nifTexturePath`), the non-diffuse maps and shader flags, tangents,
vertex colours and UV2, collision and extra data, the NIF's own
`NiTransformController` animation, and the clip's float tracks and annotations.
Root motion is **left out by default** — the clip plays in place and
`extras.rootMotion` says the travel was omitted on request — and applied only
behind `--root-motion`.

**Status: BUILD PENDING.** `release/NifSkope.exe` (03:57:46) contains none of
this. Everything above was measured through the standalone
`release/gltfexport_dump.exe` (13:36:20, newer than every source it links);
the menu entry, the `gltf` CLI command and the three `NifSkope.pro` lines are
eleven inserts in `scratchpad/hkx4_20260910/hookup.py`, **not applied** —
`--check` reports 11 of 11 anchors matching exactly once. Syntax with the real
`Makefile.Release` flags is `SYNTAX-RC=0` on every new source and on the CLI
function inside the hook-up script. Resume:
`scratchpad/hkx4_20260910/PENDING.md`; what other lanes own:
`CHANGE_NEEDED.md` beside it. Skill `ww-interchange-readback` written.
