# Lane HKX4 / HKX4b -- glTF 2.0 EXPORT of a clip + the character (2026-09-10)

Repo `E:\Projects\NifskopeWildWastelandEdition`, main, **nothing committed**
(the lane's brief: "commit NOTHING").

bungo's ruling, verbatim (HANDOFF ~06:5x): *"gltf sounds good"* -- the animation
interchange format is glTF 2.0. HKX4 = export a loaded clip + the character
(skeleton, skinned mesh) as glTF so Blender opens it natively; HKX5 = the
import side.

**Lane history.** HKX4 (first run) was killed by the session limit at ~07:4x
and left `src/gltfexport.{h,cpp}`, `tests/gltfexport_dump.cpp`,
`tests/spells/gltf_check.py` and two scratchpad scripts on disk with **no
report**. The HANDOFF says it "died at first compile"; that is not what the
disk said -- `release/gltfexport_dump.exe` (775,202 B, 05:41:32) and a written
`out/human_male_jog.gltf` were already there, so it died *after* its first
successful build and export. HKX4b (this lane) resumed from those files.

Skills invoked: `nifskope-ww-resume-pending`, `ww-standalone-writer-gate`,
`ww-hkx-animation`, `ww-anchored-hookup`, `ww-contract-provenance`, `nif`
(through the tree's own `release/nif.xml` rather than the skill's summary).

**Deliverable, met:** `scratchpad/hkx4_20260910/out/human_male_jog.gltf` +
`.bin` -- the vanilla human male with skeleton, nine skinned meshes and a
vanilla 30 fps clip, opened natively by Blender 4.5.

---

## 1. What HKX4 left, and what HKX4b did with it

| file | HKX4 | HKX4b verdict |
|---|---|---|
| `src/gltfexport.h` | 127 lines, the neutral input struct + two entry points | **KEPT**; one field added (`diffuseSourcePath`) |
| `src/gltfexport.cpp` | 781 lines, the whole writer | **KEPT**; three defects fixed (section 3) |
| `tests/gltfexport_dump.cpp` | 688 lines, standalone driver with its own NIF reader | **KEPT**; one real bug fixed and three readers added (section 3) |
| `tests/spells/gltf_check.py` | 297 lines, structural validator | **KEPT unchanged** -- it is sound; it was given a FLOOR instead |
| `scratchpad/hkx4_20260910/nifread.py` | independent Python NIF reader | **NEVER RAN, and crashed on its first shader property.** Rewritten as `tests/spells/gltf_nifread.py` |
| `scratchpad/hkx4_20260910/build_dump.sh` | syntax pass + standalone link | **KEPT as written**, unmodified |
| `scratchpad/hkx4_20260910/sx.sh` | a half-edited copy of HKX1's script with an unterminated string | **REWRITTEN** as a general syntax-check driver |
| `scratchpad/hkx4_20260910/hookup.py` | referenced by a code comment, **absent** | **WRITTEN** |
| `docs/GLTF_INTERCHANGE.md` | referenced by four comments, **absent** | **WRITTEN** |
| `tests/spells/gltf_readback.py` | **absent** | **WRITTEN** |
| the report | **absent** | this file |

New in HKX4b beyond the brief: `src/gltfexportnif.{h,cpp}` (the NifModel-side
builder, so the hook-up has something to call), `src/lib/importex/gltfanim.cpp`
(the menu half), `tests/spells/gltf_sabotage.py` (the floor under the
validator), `tests/spells/gltf_blender_check.py` and `tests/spells/gltf_gates.sh`.

## 2. What ships

| file | what |
|---|---|
| `src/gltfexport.h` / `.cpp` | the writer. QtCore only -- no NifModel, no Scene, no GL, no tiny_gltf. A neutral struct in, `.gltf` + `.bin` out. That is what lets a standalone binary link it and the gates run with no NifSkope.exe |
| `src/gltfexportnif.h` / `.cpp` | `NifModel` -> that struct, and `HkxAnimClip` -> a glTF animation. The only part that knows what a NIF block is |
| `src/lib/importex/gltfanim.cpp` | the menu entry `File > Export > .glTF (skeleton, skin, animation)`; picks the name, calls the above, reports in words |
| `tests/gltfexport_dump.cpp` | the standalone driver `release/gltfexport_dump.exe`, with its OWN container-level NIF reader so the gate has a C++ producer independent of the model path |
| `tests/spells/gltf_check.py` | structural glTF validator (HKX4's) |
| `tests/spells/gltf_sabotage.py` | 12 broken copies, the floor under it |
| `tests/spells/gltf_nifread.py` | independent Python NIF reader |
| `tests/spells/gltf_readback.py` | the independent read-back, gates R1-R6, with 7 sabotages |
| `tests/spells/gltf_blender_check.py` | Blender's own importer, headless |
| `tests/spells/gltf_gates.sh` | the whole set in one run |
| `docs/GLTF_INTERCHANGE.md` | the contract, 31 provenance anchors |
| `scratchpad/hkx4_20260910/hookup.py` | the 11 lines four shared files need. **NOT APPLIED** |

## 3. The four defects found in HKX4's code, each by a gate

**(a) The vectors came out backwards.** `tests/gltfexport_dump.cpp` read three
floats inside one argument list:

```cpp
nd.t = Vector3( br.f32(), br.f32(), br.f32() );
```

The three calls are **unsequenced**: g++ evaluates the argument list right to
left, so the first float read landed in `z` and the vector was reversed. Three
sites had it (node translation, shape translation, full-precision vertices).
It was found by gate R2, which reported every bone's `x` and `z` swapped
against the Python reader -- 231 failures. Fixed with named locals. **Nothing
in the shipping writer had the pattern**; it was test code, but it is exactly
the class of bug a gate exists for, and no amount of reading found it first.

**(b) `nifread.py` had never been executed.** Its shader-property walk missed
the leading `Shader Type` uint that `NiObjectNET` carries **only** for
`BSLightingShaderProperty` (nif.xml, `onlyT="BSLightingShaderProperty"`), so
it read `Num Extra Data List` four bytes out of step and asked `struct` for a
17-gigabyte buffer on the very first shader block of the fixture. Rewritten,
self-contained, as `tests/spells/gltf_nifread.py`.

**(c) The driver guessed the texture set.** It scanned the shader block for a
u32 that happened to index a `BSShaderTextureSet` and took word 10 "if no
other word does". That is right by accident (word 10 IS `Texture Set` when the
extra-data list is empty) and wrong the moment a shape carries extra data.
Replaced by the field-by-field walk, which also picked up the one shape the
heuristic missed entirely: `MaleEyesAO:0` carries a `BSEffectShaderProperty`,
whose diffuse is a `Source Texture` string and not a texture set at all
(`textures/Shared/Black01_d.dds`).

**(d) Two glTF spec violations in the writer, on paths the fixtures do not
take.** An animation-only or node-only export wrote `"meshes":[]`,
`"materials":[]`, `"accessors":[]` -- an empty array is `EMPTY_ENTITY` and
illegal. Both are now omitted, and a node-only export writes no `.bin` at all.
Also removed: a `num()` whose two branches returned the same value.

## 4. The gates, as pre-registered in the brief

`bash tests/spells/gltf_gates.sh`, last run 2026-09-10 **13:55** ->
**`0 gate(s) not as registered`**.

| gate | what | result |
|---|---|---|
| **(a) validator** | the Khronos validator is not installable here (no node, no npx, no pip); `tests/spells/gltf_check.py` stands in: buffer length, view bounds and 4-byte alignment, accessor counts and byteLengths, `min`/`max` **recomputed from the bytes**, node reachability and single parenthood, mesh attribute counts, index range, skin joints, weight sums, sampler input/output counts, non-decreasing times, unit quaternions, every material/texture/image index | **PASS** -- 6,411 checks / **0 failures** on the jog export, 6,411 / 0 on the Mixamo export, 688 / 0 on the vanilla donor. Warnings only: the 9 `.dds` uris (section 7) |
| **(a-floor)** | `gltf_sabotage.py` writes 12 broken copies -- byte length, view overrun, misalignment, accessor count, a `min` a metre off, a dropped `min`, an out-of-range joint, a second parent, an unreachable subtree, a non-unit quaternion, a sampler mismatch, a bogus channel path | **PASS** -- **12 of 12 refused**. The validator has now been seen failing |
| **(b) independent read-back** | `gltf_readback.py`: R1 the up-axis root, R2 every bone's bind TRS, R3 every mesh (counts, indices, positions, UVs, normals, joints, weights, the skin's joint order and every inverse-bind matrix), R4 every channel every frame, R5 the bind pose, R6 root motion. Sources: `gltf_nifread.py` (NIF) and HKX1's `hkxanim_decode.py` (clip). **No code shared with the writer** | **PASS** -- 3,234 checks, **2 failures, both pre-registered** (see below). Worst animation error **4.20e-6 units** on translation and **2.34e-6 deg** on rotation, over 234 channels x 23 frames; scale exact. Tolerances asked for: 1e-4 / 0.01 deg |
| **(b-floor)** | 7 sabotages applied after reading and before checking | **PASS** -- every one red, and each hits the gate it should: `ibm-transpose` R3+R5, `joint-shift` R3+R5, `quat-conjugate` R4, `frame-shift` R4, `unit-scale` R2+R3+R4, `up-axis` R1+R5, `node-translate` R2+R5 |
| **(c) mesh** | vertex and index counts and weight sums equal the NIF's | **PASS** -- 9 shapes, 6,443 vertices, 11,375 triangles, every index identical, worst position error 0 units, worst weight error 0, weight-row sums 0.999695 .. 1.000397 against a 1e-3 tolerance |
| **(d) Blender** | Blender **4.5** (`E:\Tools\3D\Blender 4.5\blender.exe`), `--background --factory-startup`, its own Khronos importer, expectations read out of the file itself rather than typed in | **PASS** -- jog: armature `COM` **110 bones**, **9 of 9 meshes** matched by name with the file's own counts, action `JogForward` frames **0.000..17.600** (0.7333 s at Blender's 24 fps scene rate = 23 frames at 30 fps), **790 fcurves**; Mixamo: same, action **0.000..36.800** (1.533 s = 93 frames at 60 fps); vanilla donor: 1 of 1, 58 bones |

**The two failures are the FIXTURE, and they were registered before the run.**
Gate R5 requires every joint matrix `global(joint) x inverseBind` to be one and
the same rigid transform. On Bethesda's own `MaleBody.nif` all 58 bones agree
to **0.00098 units**. On `fixtures/human_male_vanilla.nif` eight of the nine
shapes agree to 0.0016 or better, and the body's `LLeg_Toe1` is **2.8646
units** out: `skeleton.nif` poses that one bone differently from the pose
`MaleBody.nif`'s skin was authored against. Both files are Bethesda's. The
vanilla donor is the clean control and passes 1,246 / 0. Recorded as C2 in
`CHANGE_NEEDED.md`.

## 5. The two things the gates settled that nobody had written down

**The inverse-bind convention, and what it is NOT.** The first version of gate
R5 asked for the textbook invariant -- skinning a mesh in its bind pose returns
the mesh -- and failed by **1.727 m** on every shape. It was the gate that was
wrong. Measuring `global(bone) x storedBoneTransform` on Bethesda's own
`MaleBody.nif` gives one rigid transform for all 58 bones,

> `translate(-0.0002, -0.8818, +120.8437)` NIF units, spread 0.00098,

because **FO4 body meshes store their vertices with the origin at the top of
the head**: `BaseMaleBody:0` spans `z = -120.25 .. -5.688` and the skin is what
stands the model on its feet. The rotation part of every joint matrix is the
Z-up -> Y-up rotation to 9.2e-6. So the stored `BSSkinBoneTrans` IS the inverse
bind matrix, written straight through with only its translation scaled -- which
is also what upstream's `createInverseBoneMatrices()` does, and now there is a
number behind it instead of a reading.

**How FO4's dismemberment segments merge.** They are draw ranges over the one
vertex and index buffer, so all of them belong to one glTF primitive and no
merge decision exists. The arithmetic check found the rule the first walk got
wrong: a segment's **sub-segments re-describe that segment's own range, they do
not extend it**. Counting both levels on `BaseMaleBody:0` gave 4,351 triangles
against 2,698 real ones. The export now refuses unless the top-level segments
sum to the triangle count and each segment's sub-segments sum back to it, and
the table goes verbatim into `extras.mergedPartitions` (8 rows for the body).

## 6. The fixtures

| export | source | clip | size |
|---|---|---|---|
| `scratchpad/hkx4_20260910/out/human_male_jog.gltf` + `.bin` | `fixtures/human_male_vanilla.nif` | `MT\Neutral\JogForward.hkx`, 23 frames at 30 fps, root motion present and NOT applied (165.354 units of travel, recorded in extras) | 94,601 + 508,720 B |
| `out/human_male_mixamo.gltf` + `.bin` | the same | `fixtures/Running_To_Slide_And_Back_To_Running.hkx`, **93 frames at 60 fps**, `--root-motion` | 94,837 + 727,400 B |
| `out/vanilla_malebody.gltf` + `.bin` | Bethesda's `characterassets/MaleBody.nif` | none | 13,688 + 105,188 B |

Both character exports: 139 nodes, 9 meshes, 6,443 vertices, 11,375 triangles,
**78 of 95 tracks matched**; the 17 that do not are the `Weapon*` set that
`skeleton.hkx` carries and the body NIF does not, and every one is named in
`extras.unmatchedTracks`.

**The Mixamo clip only exports because HKX2b's identity-map rule had landed**
in `src/hkxanim.cpp` at 13:22 (its `transformTrackToBoneIndices` is empty, and
that means identity, not missing). This lane read that file and changed
nothing in it. Its root motion is present and **all zero**: the 487 units of
travel are on the `COM` track, so `--root-motion` changes nothing on it -- as
lane FIXTURE measured.

## 7. Named losses (the contract's section 9)

The texture bytes (glTF allows only PNG/JPEG, FO4 ships `.dds` -- Blender
prints `Cannot read ...` nine times and imports the model with empty images);
normal/specular/glow maps and every shader flag; tangents and bitangents;
vertex colours, UV2 and eye data; collision, connect points, `BSBound`,
`BSXFlags`, extra data; the NIF's OWN `NiTransformController` animation; the
clip's float tracks and annotations. Recoverable from the file itself: the
texture path (in the uri and in `extras.nifTexturePath`), the segment table
(`extras.mergedPartitions`) and the unmatched tracks
(`extras.unmatchedTracks`).

## 8. Mtimes, in one table (CONSTITUTION 4)

| file | mtime | bytes |
|---|---|---|
| `src/gltfexport.h` | 13:28:34 | 5,434 |
| `src/gltfexport.cpp` | 13:28:39 | 29,592 |
| `tests/gltfexport_dump.cpp` | 13:35:57 | 30,677 |
| `release/gltfexport_dump.exe` | **13:36:20** | 796,805 |
| `src/gltfexportnif.h` | 13:48:55 | 4,695 |
| `src/gltfexportnif.cpp` | 13:49:01 | 17,971 |
| `src/lib/importex/gltfanim.cpp` | 13:49:22 | 3,777 |
| `tests/spells/gltf_readback.py` | 13:41:06 | 25,395 |
| `tests/spells/gltf_gates.sh` | 13:55:11 | 6,010 |
| `out/human_male_jog.gltf` | 13:55:16 | 94,601 |
| `docs/GLTF_INTERCHANGE.md` | 13:55:39 | 23,180 |
| `src/hkxanim.cpp` (lane HKX2b's, read only) | 13:22:48 | 39,694 |
| `release/NifSkope.exe` | **03:57:46** | 18,526,720 |

`gltfexport_dump.exe` is newer than every source it links. `NifSkope.exe` is
**not** built with any of this -- that is what BUILD PENDING means here.

## 9. BUILD PENDING, and what is owed

`Fallout4.exe` and `NifSkope.exe` were both down for the whole lane
(`tasklist` checked before every build). Nothing in `release/` was touched
except `gltfexport_dump.exe`, and **NifSkope.exe was never built and never
launched** -- the brief reserves the build to the director.

Compile evidence: `SYNTAX-RC=0` with the real `Makefile.Release` flags on
`src/gltfexport.cpp`, `src/gltfexportnif.cpp`,
`src/lib/importex/gltfanim.cpp`, `tests/gltfexport_dump.cpp`, and on the CLI
function that lives inside `hookup.py` (extracted to
`scratchpad/hkx4_20260910/probe_cli.cpp` and compiled against the real
headers, so the hook-up's largest insert is not untested text).

`python scratchpad/hkx4_20260910/hookup.py` (the default `--check`, which
writes nothing): **11 of 11 anchors match exactly once, 0 insertions already
present**, `NifSkope.pro` 18,779 B / CR 0, `src/lib/importex/importex.cpp`
7,044 B / CR 0, `src/nifcli.cpp` 263,848 B / CR 0.

**Cross-lane, and it decides the build order.** While this lane ran, HKX5b
(glTF import) added `src/gltfimport.{h,cpp}` and `src/hkxwrite.{h,cpp}` to
`NifSkope.pro` directly; `src/gltfimport.h` includes `src/gltfexport.h` and
`src/gltfimport.cpp:688` hard-codes this writer's up-axis quaternion. But
`src/gltfexport.cpp` is NOT in the `.pro` — only `gltfimport` is — so **the
next full build must apply this lane's hook-up first or it will not link.**
The three `.pro` anchors still match exactly once after HKX5b's additions
(re-checked at 14:0x; the file grew 18,779 → 18,859 bytes under this lane).

The resume is `scratchpad/hkx4_20260910/PENDING.md` (P1 apply, P2 qmake before
make, P3 read the dependencies back per object, P4 exe-newer sweep, P5 the CLI
must reproduce the driver's numbers exactly, P6 the menu by hand, P7 the four
documents). What another lane owns is
`scratchpad/hkx4_20260910/CHANGE_NEEDED.md` (C1 HKX3 registers the clip
provider, C2 the fixture's `LLeg_Toe1`, C3 the `.dds` question for bungo).

All new files are LF-only, measured with Python byte counts, CR 0 on every one.

## 10. Mistakes

Written into the root `MISTAKES.md` as they were found. In short:

1. **A gate script that was never run** (`nifread.py`), shipped by HKX4 as if
   it were evidence. It crashed on its first real input.
2. **Three unsequenced reads in one argument list**, which silently reversed
   every vector the driver read. Caught by gate R2, not by reading.
3. **A pre-registered invariant that was wrong about the data.** Gate R5's
   first form assumed FO4 meshes are stored in skeleton space. The right
   response was to measure Bethesda's own file and restate the gate, not to
   loosen the tolerance until it passed.
4. **A field walk that was right by accident** (the texture-set heuristic).
5. **The HANDOFF's account of HKX4's death was wrong** -- "died at first
   compile" against an exe and an export on disk from 05:41. Checking the
   disk before believing the ledger is the rule that catches it.

## 11. Skill review (CONSTITUTION 1a, the finished-work review)

**Loaded and used:** `nifskope-ww-resume-pending` (the read order, the
qmake-before-make rule and the exe-newer sweep are P2-P4 of the resume);
`ww-standalone-writer-gate` (the whole shape of this lane -- syntax with the
real flags, a standalone link, an independent decoder, refusal controls in two
tiers); `ww-hkx-animation` (the fixture set, the `2*asin` angle metric, the
identity-map rule, and the settled facts that were NOT re-derived);
`ww-anchored-hookup` (`hookup.py` is its reference shape, and its warning about
heredocs halving backslashes is the reason the script was written with the
write tool -- a heredoc had already silently eaten `\\` out of an anchor
earlier in this lane and reported 0 matches); `ww-contract-provenance` (the
five steps; the anchor pass reports 31 rows, 0 missing, 0 ambiguous, and a
second run reports 0 moved).

**Declined, with the reason:** `ww-control-calibration` -- its subject is a
floor and ceiling for a *signal* measurement against a shipped texture. The
control this lane needed was categorical, not spectral: Bethesda's own
`MaleBody.nif` exported through the same path, green where the assembled
fixture is red. `ww-character-fixture-assemble` -- the fixture already existed
and this lane only read it.

**The skill that should have existed, and is now written:**
`E:\Projects\Claude\.claude\skills\ww-interchange-readback\SKILL.md` --
"the independent read-back gate for an interchange writer". Everything in this
lane that cost the most was a procedure with no page: choosing an invariant
that is a property of the *data* rather than of the textbook (R5 cost two
rewrites and a wrong 1.727 m verdict); building the sabotage set BEFORE
believing the green run; using the vendor's own shipped file as the control so
a red gate names the fixture instead of the code; and deriving the Blender
expectation from the file rather than typing it in. It will be done again for
HKX5 (glTF import) and for the `.hkx` writer, which is why it is a page and not
a paragraph in this report.


## Build (BUILD8)

Built by lane BUILD8, 2026-09-10, on `main`, **nothing committed**.
bungo's order, verbatim: *"now export and import of gltf"*.

### The exe, and the mtimes in ONE table

| artefact | mtime | bytes |
|---|---|---|
| `release/NifSkope.exe` | 2026-09-10 **14:37:53** | 19,382,272 |
| `release/style.qss` | 14:37:53 | 11,097 (compares equal to `res/style.qss`) |
| `release/hkxanim_dump.exe` | 14:51:01 | 532,675 |
| `release/hkxwrite_dump.exe` | 14:46:32 | 802,556 |
| `release/gltfexport_dump.exe` | 14:46:46 | 802,959 |
| `src/hkxanim.cpp` (BUILD8's interleaved read arm) | 14:42 | 43,891 |
| `src/nifcli.cpp` (the three CLI commands) | 14:36 | 275,399 |
| `scratchpad/build8_20260910/frames_mixamo_original.png` | 14:5x | 1,370x1,420 |
| `scratchpad/build8_20260910/frames_mixamo_roundtrip.png` | 14:5x | 1,370x1,420 |

`qmake` + `make -j2`, both RC=0. The exe is newer than all **72** changed files
under `src/ res/ tools/ tests/ NifSkope.pro`. `Makefile.Release`, read back per
object by name, gives `gltfexport.h` to `gltfexport.o`, `gltfexportnif.o`,
`gltfanim.o` and `nifcli.o`; `gltfimport.h` and `hkxwrite.h` to their own
objects and to `nifcli.o`.

**The first link FAILED** because bungo opened `release/NifSkope.exe` at
14:26:18, mid-lane. The running copy was renamed aside (never killed) as
`release/NifSkope_inuse_44632.exe` and the link re-run. It is in `MISTAKES.md`:
a process check that only echoes its answer is not a gate.

### The CLI, and what had to be written to have one

`scratchpad/hkx4_20260910/hookup.py --apply` -- 11 of 11 anchors, exactly once
each, CR 0 unchanged on all three files. Then TWO hook-ups this lane wrote,
because **lane HKX5b's importer and writer had no route at all**: they were in
`NifSkope.pro` and linked, and nothing called them.

```
gltf        <file.nif>  -o OUT.gltf [--clip C.hkx [--bones S.hkx]] [--root-motion]
gltf-import <in.gltf>   -o OUT.hkx  [--bones S.hkx] [--fps N] [--source-rate]
                                    [--root-motion --root-node Root] [--route a|b] [--tsv T]
hkx-tsv     <in.hkx>    -o OUT.tsv
```

All three are in `NifSkope -no-gui --help`. Scripts:
`scratchpad/build8_20260910/hookup_import.py`, `hookup_rootnode.py`.

### THE ROUND TRIP -- every leg through `release/NifSkope.exe`

Bars pre-registered by lane HKX5b: **1e-4 units, 0.01 degrees**, per bone per
frame, the angle being the corrected `4*asin(|q1-+q2|/2)`. The original is
decoded by `tests/spells/hkxanim_decode.py` and the written file by
`scratchpad/hkx5_20260910/interleaved_decode.py` -- two independent Python
decoders, neither of them our C++.

| leg | rows | max abs dT (units) | max angle (deg) |
|---|---|---|---|
| `JogForward`: the IMPORTED clip vs the original decode | 1,794 | **8.0e-06** | **3.41e-05** |
| `JogForward`: written, decoded again | 1,794 | 8.0e-06 | 3.41e-05 |
| `JogForward` `--root-motion --root-node Root` | 1,794 | 8.0e-06 | 3.41e-05 (root motion 1.5e-05 / 0.0 deg over 165.354 units) |
| Mixamo 60 fps `--source-rate`: the imported clip | 7,254 | **4.4e-05** | **5.21e-06** |
| Mixamo: written, decoded again | 7,254 | 4.4e-05 | 5.21e-06 |
| the WRITER alone (imported clip vs its own decode) | 1,794 / 7,254 | 0.0 | 0.0 |
| the exe's own reader vs the independent decoder, on the written file | 1,794 / 7,254 | 0.0 | 0.0 |
| the exe's own reader vs the oracle, on the ORIGINAL spline clip | 2,185 / 8,835 | 3.6e-06 / 1.5e-05 | 4.70e-06 / 4.83e-06 |

Scale exact on every row. The 391 / 1,581 rows that do not survive are the 17
`Weapon*` tracks with no node in the body NIF, each named by the exporter.
Mesh and skin through the exporter equal the NIF's: 9 shapes, 6,443 vertices,
11,375 triangles, every index identical.

### THE PICTURE

The five frames of lane BUILD7's sheet plus the bind pose, rendered twice from
ONE pinned orthographic camera (`WW_RENDER_VIEW=5 CENTER=0,0,62 ORTHO=80`,
`upp=0.150235`, read back from `release/ww_camera_pin.log` at every grab) --
once from the original clip, once from the clip that went `hkx -> glTF -> hkx`.

| tile | Mixamo: differing pixels of 1,352,217 | worst step | jog |
|---|---|---|---|
| bind (no clip on either side) | **0** | 0 | **0** |
| frame 0 | 11 | 1 | 0 |
| 1/4 | 1 | 1 | 12 (step 1) |
| 1/2 | **0** | 0 | 33 (step 4) |
| 3/4 | 8 | 1 | 170 (step 16) |
| last | 22 | 1 | 52 (step 2) |

**The noise floor is ZERO**: the same clip rendered twice is byte-identical on
all six tiles, so every count above is the round trip's float error surfacing as
sub-pixel antialiasing on a silhouette. Worst case anywhere: 170 pixels of
1,352,217 -- 0.013% -- at 16 of 255 levels.

Sheets: `scratchpad/build8_20260910/frames_{mixamo,jog}_{original,roundtrip}.png`;
amplified difference pictures `images/diff_*.png`.

### Blender 4.5, the third-party leg

`--background --factory-startup`, our `.gltf` in, Blender's own `.gltf` out,
ours in again (`scratchpad/build8_20260910/blender_roundtrip.py`).

* Blender KEEPS our node names, including the synthetic `NifSkope_Y_up`, so our
  importer's first arm serves its file too.
* armature `COM`, 110 bones, **107 of them with a non-zero roll** -- a Blender
  concept glTF has no field for.
* scene unit METRIC, scale 1.0, metres. Nothing changed there.
* **the frame rate is what it changes.** At the factory 24 fps the clip comes
  back re-timed: 18 keys on a 1/24 grid, our importer resamples to 22 frames,
  and the error is **0.29 units / 4.83 degrees**. With the scene rate set to the
  clip's own 30 fps: 23 frames and **1.006e-04 units / 4.68e-04 degrees** --
  21x inside the angle bar and **0.6% OVER** the translation bar. Both are
  Blender's resampling, not ours.

### Gates

| gate | result |
|---|---|
| `bash tests/spells/gltf_gates.sh` | **0 gate(s) not as registered** (G2 0/0/0, G2f 12/12 refused, G3 2/2/0 pre-registered, G3f 7/7 red, G4 Blender 9/9, 9/9, 1/1) |
| `python tests/spells/hkxwrite_gates.py` | **23/23** |
| `python tests/spells/hkxanim_gates.py` | **134 checks / 3 failures**, all three pre-registered (the 17 `Weapon*` bones, the 0.00148 `weapon` pose translation, the furniture T-pose clip) -- on `hkxanim_dump.exe` REBUILT at 14:51:01 |
| `python tests/spells/hkxanim_mutate.py` | 20 / 0 |
| `python tests/spells/hkxanim_synthetic.py` | 29 / 0 |
| `bash tests/spells/hkxanim_play.sh` | 27 / 0 |
| `bash tests/spells/render_shot.sh` | 82 / 0 |
| FLOOR, the new interleaved read arm | one payload float moved by 1.0: the exe's reader reports **1.000e+00 at (frame 1, track 1) and 0 everywhere else** |
| FLOOR, the picture | the same clip rendered twice: 0 differing pixels on all six tiles |

### THE CROSS-LANE CHANGE, named

`src/hkxanim.cpp` is lane HKX2b's file and this lane changed it: lane HKX1's
reader dispatches on the class name and refused every
`hkaInterleavedUncompressedAnimation` -- the only class `src/hkxwrite.cpp` can
write -- so **NifSkope could not open the file its own glTF import had just
written**, and the picture proof (which goes through the render hook, which goes
through this reader) was impossible. The arm reads the `hkArray<hkQsTransform>`
at +0x38, 48 bytes an element, frame-major, with the frame count derived as
`transforms.size / numberOfTransformTracks` (`docs/HKX_WRITE_FORMAT.md` 3.1,
from the engine's own `transformTrack`, rva `0x01fa1ac0`). Additive: every other
class is refused with the identical sentence and the spline path is unchanged.
The pre-change file is kept at
`scratchpad/build8_20260910/hkxanim.cpp.pre-build8`. It is in `MISTAKES.md`.

### The two contract pages, read side by side at last

Lane HKX5b's owed item is discharged. Every source both pages cite was re-hashed
first and every one is unchanged (`src/gltfexport.h` `77e2b2f7e2010ca5`,
`src/gltfexport.cpp` `cbd5ce5e0e569a34`, `src/gltfimport.h` `0c0da7182b3e965e`,
`src/gltfimport.cpp` `a2cec5a5f738ef57`). They AGREE on the unit, the up-axis
node and its quaternion, the quaternion component order, dimensionless scale,
LINEAR being exact for FO4's degree-1 splines, and the root-motion formula.
They DISAGREED on two things, now written into both:

1. **which node carries the root motion** -- the export composes it onto the
   ROOT BONE's node (`Root`), the import defaulted to the single scene root
   (`skeleton.nif`), which drives no bone. The import refused by name until
   `--root-node` existed;
2. **the frame rate** -- the export writes the clip's own, the import defaults
   to 30 for everything, so a default round trip of the 60 fps fixture returns
   47 frames instead of 93.

### What is NOT measured

* **Fallout 4 has still never loaded a file written by `src/hkxwrite.cpp`.** The
  two flight files in `scratchpad/hkx5_20260910/flight/` are unchanged and
  unflown.
* The menu entry `File > Export > .glTF (skeleton, skin, animation)` was not
  exercised by hand (PENDING.md's P6): the exe was kept free of a GUI session.
  **There is no menu route to the IMPORT at all** -- CLI only.
* `tests/spells/hkxanim_decode.py`, `tests/spells/hkxanim_gates.py` and
  `docs/HKX_ANIMATION_FORMAT.md` section 4.6 still carry the `2*asin` angle
  metric, so lane HKX1's published angle figures are still halves. Untouched
  again, for the same reason BUILD7 gave: changing the metric moves the gate
  numbers this build had to reproduce.
* Nothing is committed.
