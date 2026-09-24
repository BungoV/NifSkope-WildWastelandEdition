---
name: ww-hkx-animation
description: Work on Fallout 4 Havok ANIMATION files (.hkx clips, skeleton.hkx) in the NifSkope Wild Wasteland tree -- where the clips actually live (the animations BA2, not the unpacked corpus), pulling and unpacking a fixture, reading a packfile's section table without the 0x40 trap, reading a Havok class layout straight out of the exe's hkClass reflection, resolving a disassembly's rip-relative float constant, the whole-archive census, the fixture set and the gate commands of lane HKX1, and what the contract already settles so it is not re-derived. Use for HKX2 (playback/mapping), HKX3 (UI), any later animation lane, and any question of the form "what does FO4 store for bone/track/frame X".
---

# FO4 .hkx animation work in the WW tree

The contract is `docs/HKX_ANIMATION_FORMAT.md` (read it IN FULL before
touching `src/hkxanim.cpp`; CONSTITUTION rule 3). The reader is
`src/hkxanim.{h,cpp}` (`hkxAnimLoad(path)`: `.xml` = HKXPACK route,
anything else = packfile route, one `HkxAnimFile` either way). The
independent oracle is `tests/spells/hkxanim_decode.py`. Lane report:
`scratchpad/lane_hkx1_report.md`.

## 1. Where the files are (the corpus has none)

`E:\Tools\Fallout 4\DataUnpacked\Data\meshes\actors\character\` holds the
NIFs and `Animations\` holds only `Furniture` and `Weapon` NIF folders --
**zero `.hkx`**. Every animation and `skeleton.hkx` is inside
`X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4 - Animations.ba2`
(GNRL, 29,716 files, 15,320 `.hkx`). List and pull with the repo's own tools:

```bash
BA2="/x/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - Animations.ba2"
python scratchpad/hkx1_20260910/ba2list.py "$BA2" 'actors.character.animations.MT.Neutral.[^\\/]*[.]hkx$' | sort -n
python tools/ba2get.py "$BA2" "Meshes\Actors\Character\Animations\MT\Neutral\JogForward.hkx" clips/jog.hkx
```

Regex note: a Git-Bash single-quoted `'\\'` reaches Python as ONE backslash;
write `.` or `[\\/]` for the separator, never `\\`.

Fixtures already extracted and unpacked in `scratchpad/hkx1_20260910/clips/`:
`skeleton` (player, 95 bones + 18-bone ragdoll), `tpose_idle` (94 tracks, 2
frames, all static -- NOT the bind pose), `jog` (23 frames, splines, root
motion, annotations), `turn` (yaw motion), `twoblock` (311 frames, 2 blocks),
`q48` (THREECOMP48, 2 blocks), `lossless` (1st-person, refused). There is no
bind-pose clip in the archive (searched tpose/bindpose/apose/refpose).

## 2. HKXPACK (behaivor-graph skill has the general procedure)

```bash
H="/e/Tools/Fallout 4/HKXPACK/hkxpack-cli.jar"
java -jar "$H" unpack clip.hkx -o clip.xml       # ~2 s; the XML names members exactly as the reflection does
java -jar "$H" pack   clip.xml -o clip.hkx        # works on a hand-built animation XML too (gate (d) does it)
```

The XML prints `data` as decimal bytes, `referencePose` as
`(t)(q)(s)` vec4 triples, `parentIndices` unsigned (65535 = -1).

## 3. The packfile section table, without the 0x40 trap

Section headers start at `0x40 + u16@0x3e` (`predicateArraySizePlusPadding`):
0x50 in every animation file, 0x40 in a collision blob. `tools/hkparse.py`
and `src/gl/hknpdecode.cpp` hardcode 0x40 and REFUSE animation files; the
walkers in `census.py` / `hkxanim_decode.py` / `hkxanim.cpp` read the u16.
Copy from those.

## 4. A Havok class layout from the exe, in one read

```bash
cd scratchpad/hkx1_20260910 && python hkclass_reflect.py hkaSkeletonClass_Members "hkaSplineCompressedAnimation::Members" hkaAnimationBindingBlendHintEnumItems
```

Symbol names are `<Class>Class_Members`, `<Class>::Members` or
`<Class><Enum>EnumItems` in Todd's treat's symbol list; some `_Members` arrays
live in `.data` with a dynamic initializer and still read back (the const
bytes are there). Prints `+offset name type subtype` per member. This is
REFL provenance; the packfile offset equals the in-memory offset.

## 5. A rip-relative constant from a disassembly line

The Todd's treat tooling's disassembly prints `movss xmm0, dword ptr [rip + 0xNNN] ; rip->_real`
without the value. Resolve it: target RVA = instruction RVA + instruction
length (bytes column / 2) + displacement; read 4 bytes there with
its `PEImage(exe).read(rva, 4)` and `struct.unpack('<f')`. The three
constants of the quaternion unpackers were resolved this way (2047.0,
0.000345435663, 16383.0, 4.3161006e-05, 1/65535).

## 6. The census

```bash
python scratchpad/hkx1_20260910/census.py "$BA2" "" census_rows.tsv > census_all.txt   # 6 s, whole archive
python scratchpad/hkx1_20260910/census.py "$BA2" 'Character.Animations.MT' rows.tsv       # a pattern
```

Tallies quantization per track, masks, blocks, float tracks, blend hints,
motion classes, annotation counts, binding permutations, skeleton names;
`census_rows.tsv` has one row per clip (name, tracks, float tracks, frames,
blocks, maxFramesPerBlock, data bytes, motion class). `find_clips.py` lists
the lossless files and the THREECOMP48 character clips.

## 7. The gates (no NifSkope.exe needed)

```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWildWastelandEdition && bash scratchpad/hkx1_20260910/build_dump.sh'   # syntax pass + release/hkxanim_dump.exe
python tests/spells/hkxanim_gates.py      # (a) C++ vs Python both routes, (b) skeleton.hkx vs skeleton.nif, (c) T-pose vs bind pose, (f) header, (g) block overlap; 134 checks / 3 fixture failures
python tests/spells/hkxanim_synthetic.py  # (d) hand-built 90-degree clip through HKXPACK; 29/0
python tests/spells/hkxanim_mutate.py     # (e) 20 single-byte corruptions refused by name; 20/0
```

`hkxanim_dump.exe CLIP --out x.tsv` and `hkxanim_decode.py CLIP --out y.tsv`
write the same TSV (`frame track bone tx ty tz qx qy qz qw sx sy sz`, root
motion as track -1). The angle metric is `4*asin(|q1 -+ q2|/2)` (lane HKX5b 2026-09-10 proved
`2*asin` returns HALF the true angle -- known-answer controls at 0.5/5/45/120
deg; every angle lane HKX1 published is a half), never `acos(dot)` (no
resolution below 0.03 deg).

## 8. Settled, do not re-derive

* Quaternions are Havok (x,y,z,w) in the file; `HkxTransform.rotation` is
  NifSkope's `Quat` (w,x,y,z); the hkx quaternion maps to the NiNode rotation
  matrix DIRECTLY (gate (b): 2.7e-4, transposed 2.0).
* Bone names: skeleton.hkx vs skeleton.nif differ in CASE on Head, Spine1,
  Spine2, Weapon; 17 `Weapon*` bones have no body node; match
  case-insensitively and expect partial matches.
* FO4 uses THREECOMP40 and THREECOMP48 rotations and 16-bit scalars only;
  1st-person clips are `hkaLosslessCompressedAnimation` (856 files), refused.
* Blocks are 256 frames overlapping by one; frame F is block F/255, local
  F-255*block; the boundary frame is stored twice (2.7e-4 apart).
* Degree-1 splines everywhere seen: a frame is a control point; between
  frames the engine lerps components (nlerp on rotations).
* Root motion = `(xyz, yaw about up)` per frame, separate from track 0.
* A Havok packfile has no checksum: a flipped payload byte is a different
  valid pose, not a refusal.

## 9. The class set FO4 actually ships (lane HKXCLASS, 2026-09-10, measured)

Havok version, from `hkHavokCurrentClasses::VersionString` rva `0x02dbd038`:
**`hk_2014.1.0-r1`**. Do not re-derive any of the following.

`hkaAnimationAnimationTypeEnumItems` rva `0x02de49c0` holds exactly 7 items:
0 UNKNOWN, 1 INTERLEAVED, 2 MIRRORED, 3 SPLINE_COMPRESSED,
4 QUANTIZED_COMPRESSED, 5 PREDICTIVE_COMPRESSED, 6 REFERENCE_POSE.
**No delta, no wavelet** -- `DeltaCompressed` and `Wavelet` return 0 symbols in
the whole 1.10.155 symbol table; never emit those class names.
`HK_MIRRORED_ANIMATION` keeps an enum slot with **no class**.
`hkaLosslessCompressedAnimation` is the reverse: a real registered class with
**no** enum value (FO4's 1st-person clips carry it by class name).

Present, registered and fully implemented (member array rva / objectSize /
loader index / HKXPACK signature):

| class | Members rva | objectSize | registry idx | signature |
|---|---|---|---|---|
| hkaSplineCompressedAnimation | 0x02e46140 (13 members) | 0xb0 | 145 | 0x8c3b5f7e |
| hkaInterleavedUncompressedAnimation | 0x02e988e0 (2: `transforms` hkArray<hkQsTransform> +0x38, `floats` hkArray<hkReal> +0x48) | 0x58 | 136 | 0xa5eff3f2 |
| hkaLosslessCompressedAnimation | 0x02e98a70 (11) | 0xe0 | 137 | 0x278bffe8 |
| hkaPredictiveCompressedAnimation | 0x02e99030 (11) | 0xc0 | 140 | - |
| hkaQuantizedAnimation | 0x02e998d0 (3) | 0x58 | 141 | 0x3158d9ce |
| hkaReferencePoseAnimation | 0x02e99b30 (1) | 0x40 | 142 | 0x9e1c3a1c |

Base `hkaAnimation::Members` rva `0x038395e0` (in `.data`, raw-backed):
`type` +0x10, `duration` +0x14, `numberOfTransformTracks` +0x18,
`numberOfFloatTracks` +0x1c, `extractedMotion*` +0x20, `annotationTracks[]` +0x28.
`hkclass_reflect.py`'s member walker OVERRUNS this one by one row (its
name-plausibility break) -- the true count is 6, read from the class-object
initializer's `mov dword ptr [rsp+0x48], N`.

**The loader does NOT switch on `m_type` or on a signature.** It hashes the
class-NAME string: `hkTypeInfoRegistry::finishLoadedObject` rva `0x01504610`
does `hkCachedHashMap<hkStringMapOperations>::getWithDefault`, then the
per-class leaf (e.g. `finishLoadedObjecthkaInterleavedUncompressedAnimation`
rva `0x01e131b0`) writes the vtable pointer into the loaded object. The case
list is two null-terminated arrays built by the initializer at rva `0x02adfe80`:
`hkBuiltinTypeRegistry::StaticLinkedClasses` rva `0x02e40880` (908 entries,
name->hkClass*) and `...StaticLinkedTypeInfos` rva `0x02e3edf0` (849 entries,
name->vtable), indices matching one-for-one. Walk them with
`scratchpad/hkxclass_20260910/registry.py`.
After load, sampling is pure virtual dispatch: `hkaAnimationControl::sampleTracks`
rva `0x019ab940` ends `jmp qword ptr [rax + 0x20]` = vtable slot 4. Interleaved
and spline vftables (rva `0x02e98938` / `0x02e46528`) have identical 14-slot
layouts, every slot a class-specific body. A byte scan of all of `.text` for
`cmp dword ptr [reg+0x10], 1..6` found no type test in any animation code.

**Writing uncompressed is possible and was proven.** HKXPACK's `classxml/`
carries all six classes at the same offsets the exe declares, and its
signatures are the ones vanilla files store (jog.hkx's `__classnames__` has
`0x8c3b5f7e` for spline = HKXPACK's value). `hkxclass_20260910/interleaved.xml`
-> `hkxpack-cli.jar pack` -> `interleaved.hkx` (2,160 bytes, 3 sections) reads
back with `type=1`, `transforms[]` at +0x38 size 10, 48-byte hkQsTransform
stride, angles exact to 0/40/90 deg. NOT yet loaded by the game.

**Whole-archive class census** (`hkxclass_20260910/animclass_census.py`, both
the object list and the `__classnames__` table, which agree): 15,320 `.hkx`
scanned, 15,320 parsed, 0 refused. `hkaSplineCompressedAnimation` 13,514 files,
`hkaLosslessCompressedAnimation` 856, everything else **zero**; the remaining
950 files carry no animation object at all (skeletons, ragdolls, behaviours).

**Trap:** a linear capstone sweep of the 46 MB `.text` to find rip-relative
xrefs desynchronises and returns 0 hits. Use `callscan.py` (E8 callers) or
disassemble the one named function; see root `MISTAKES.md` 2026-09-10.

## 10. A third-party clip may carry an EMPTY binding (lane FIXTURE, 2026-09-10)

A converted clip can be a perfectly good FO4 spline animation and still be
refused by `hkxAnimLoad`, for one reason: its `hkaAnimationBinding` leaves
`transformTrackToBoneIndices` EMPTY. Measured on the Mixamo Collection's
`AnimPreviews\Running_To_Slide_And_Back_To_Running.hkx` (40,000 bytes):

* 95 transform tracks, 93 frames, 1 block, `frameDuration` 0.0166667 = **60 fps**
  (vanilla third-person clips are 30), THREECOMP40 rotations + 16-bit
  translation and scale on all 95 tracks, `numberOfFloatTracks` 0,
  `blendHint` NORMAL, `originalSkeletonName` "Root". The whole clip decodes:
  8,835 rows, the block walk ending exactly at the block's float offset;
* the binding's bytes at +0x20 are `count=0, capflags=0x80000000` with no local
  fixup — the array really is absent, it is not a parse failure. Vanilla
  `jog.hkx` has `count=95` and a payload at +0x28a0. That single difference is
  the whole refusal (`validate()`: "binding maps 0 tracks, the animation has 95");
* the file also lacks the `hkMemoryResourceContainer` object vanilla clips carry
  (5 objects vs 6). Cosmetic.

**The mapping is then IDENTITY, and that is measurable, not assumed.** In a
skinned rig every joint's local translation is fixed by the skeleton, so frame
0's per-track translation must equal `referencePose[i].translation` under the
right mapping. Result: **75 of 95 within 1e-3 at shift 0, and 18 of 95 at
shift +1, +2 or -1** — a control that fails. The 20 that differ at shift 0 are
all expected: `COM` (this clip travels 487 units in Y on the COM track), the 13
`Weapon*` / `Camera` / `CamTarget` nodes the animation places, `Spine1`, and
four finger tips at 1.5e-3 float noise.

**Root motion can be present and still be zero.** This clip carries a
`hkaDefaultAnimatedReferenceFrame` with 93 samples, up = (0,0,1), and **every
component 0.000000**. Read the max over all samples, never just first vs last:
a clip that loops would also show `|last - first| = 0`.

So a reader that wants third-party clips needs one rule, not a new format:
**an empty `transformTrackToBoneIndices` means the identity map**, accepted when
the bound skeleton has at least `numberOfTransformTracks` bones, and refused by
name when it does not. `scratchpad/fixture_20260910/clip_dump_identity.py`
does exactly that on the Python side without touching `src/hkxanim.cpp`;
`clip_raw.py` and `clip_tracks.py` beside it are the byte dump and the
identity test.

## 11. Settled by lane HKX2 (playback into the scene graph), 2026-09-10

The reader hands you `HkxAnimClip`. Putting it on a rig in NifSkope is these
five facts; none of them needs deriving again.

* **The pose goes into `Node::local`, written from `Node::transform()`
  immediately after `IControllable::transform()`** (`src/gl/glnode.cpp`). That
  is one step after the node's own controllers, so a clip WINS over a
  `NiTransformController` naming the same node; and it is before this node's
  collision body is the first thing in the frame to read a world transform, and
  before any child is walked. Do NOT do it in `Scene::transform` after the roots
  walk: that needs a second `transformCache.clear()`, which throws away the
  `bhkBodyTransKey` entries `Node::drawHvkConstraint` reads.
* **`Node::local`, `Node::parent`, `Node::children` and `Node::nodeId` are
  PROTECTED.** Everything that writes them is a `friend` at the top of
  `class Node` (`ControllerManager`, `TransformController`,
  `MultiTargetTransformController`, `KeyframeController`,
  `ProcLightningController`, and now `HkxPlayback`). Read a member's access from
  the LAST specifier above it.
* **A clip becomes an animations-list entry for free.** Put the name in
  `Scene::animGroups`, `{"start","end"}` in `Scene::animTags`, and
  `Scene::CycleLoop` in `Scene::animCycle`. `Scene::timeMin`/`timeMax`
  short-circuit on `animTags`, so play/pause/loop/reverse/speed/scrub/cycle and
  the Timeline dock's ruler all drive it with NO new transport code.
  `Scene::setSequence` is then the bind/unbind hook. The Animation Manager
  dock's own `seqBox` does NOT follow: it is built from `NiControllerSequence`
  BLOCKS (`QPersistentModelIndex`), and a loaded clip has no model index.
* **Two arithmetic traps in `src/data/niftypes.h`.** `Quat::normalize()` divides
  by the SQUARED magnitude, so it only normalises quaternions that are already
  unit -- write your own. `Quat::slerp` is Blow's approximation, not slerp; it
  does return `p` exactly at t=0. And a `Transform` carries ONE scale where a
  Havok transform carries three: take x and say you dropped y/z.
* **A frame time needs an epsilon.** `N * frameDuration` divided back by
  `frameDuration` is not `N` in float (5/30 comes back as 4.99999952), so a
  frame-exact read has to snap to the nearest frame within about 1e-4 of a
  frame or it silently interpolates 99.99997% of the next one.

And one correction to section 8: **an empty
`transformTrackToBoneIndices` is the IDENTITY map, not a missing one** -- so a
consumer sizes its "does this skeleton fit" test on `numberOfTransformTracks`,
never on the length of the binding vector.

### Bone names are not in a clip

A clip stores track -> bone INDEX against a skeleton it only NAMES, so any
playback needs an `hkaSkeleton` first. FO4's layout is
`<actor>/Animations/<group>/<clip>.hkx` with the skeleton at
`<actor>/CharacterAssets/skeleton.hkx`, so the search walks UP from the clip and
looks SIDEWAYS into `CharacterAssets` at each level, then does the same from the
open NIF's folder, then asks the game archives
(`Game::GameManager::get_file(blob, game, "meshes/.../skeleton.hkx")`, with
`Game::GameManager::get_game(nif)` for the mode). Match names
CASE-INSENSITIVELY and expect a partial match; on skeleton.nif the answer is
**78 matched, 17 unmatched, 4 matched only by case**, and those three numbers
are the gate.

### The in-app gate

`src/hkxplaybacktest.cpp` + `tests/spells/hkxanim_play.sh` (`WW_HKXANIM_TEST`).
It reads the pose back off `Node::localTrans()`, never off the playback's own
record, and every check has a floor beside it: the wrong frame must FAIL the
same comparison, invented bone names must match nothing, and the clip must
actually have moved the rig before "unload restored it" means anything.


## 12. Absolute paths, or the gates lie (lane BUILD7, 2026-09-10)

`scratchpad/hkx2_20260910/PENDING.md` quotes its resume with repo-relative
paths. Run verbatim, they produce two green-looking falsehoods, and neither is
a defect in the reader, the playback or the mapping:

| command as written | what happened | with an absolute Windows path |
|---|---|---|
| `SRC=fixtures/human_male_vanilla.nif bash tests/spells/hkxanim_play.sh` | `NIF:  (1 nodes)`, "0 bones matched (expected 78)", 8 failures of 27 | 27 checks, 0 failures, **78 / 17 / 4** |
| `CLIP=scratchpad/hkx1_20260910/clips/jog.hkx bash scratchpad/hkx2_20260910/shots.sh` | 1 distinct image of 4 -- gate (e)'s own refuter | **4 of 4 distinct** |

Git-Bash does not get MSYS2's argv/environment path conversion and
`_harness.sh`'s `winpath()` only rewrites `/e/...`. And the `WW_HKXANIM_CLIP`
hook in `src/nifskope_ui.cpp` keeps the loader's refusal in a local it only
tests for emptiness, so a clip that does not load is silent.

**One more framing fact, for any picture of a third-party clip.** A converted
clip may carry its travel on the COM TRACK rather than in the root-motion
channel -- the Mixamo fixture moves 487 units in +Y with root motion all zero --
so a FRONT view puts that travel along the view axis and a perspective camera
shrinks the figure to nothing by the last frame (5 KB of empty background, and
gate (e) still passes at 3 distinct of 4). Photograph such a clip
ORTHOGRAPHICALLY: `WW_RENDER_VIEW=5 WW_RENDER_CENTER=0,0,62 WW_RENDER_ORTHO=80`
frames every frame of both `jog.hkx` and the Mixamo clip at the same scale and
the same screen position, and the bind-pose tile then comes out BYTE-IDENTICAL
between two different clips' sheets -- which is the camera pin's own proof.

## 13. The generic packfile layer (lane HKXEDIT1, 2026-09-10)

Any .hkx -- clip, skeleton, ragdoll, behaviour graph -- is now a typed object
graph in C++ (`src/hkxfile.{h,cpp}`, `Hkx::File::read` / `write`) and in
Python (`tests/spells/hkxfile_oracle.py`), both driven by
`res/hkclasses_fo4.json`. Contract: `docs/HKX_PACKFILE_MODEL.md`. Do not
re-derive any of this.

* **The class database is the exe's, extracted, not typed.**
  `python tools/hkclassdb_extract.py` (1 s) rebuilds `res/hkclasses_fo4.json`
  from the 1.10.155 exe + symbol TSV: 908 registered classes (+35 variants /
  unregistered), every member's offset / type / subtype / flags / class /
  enum, every enum's items, and the class SIGNATURE computed from
  `hkClass::writeSignature` -- 908/908 equal to HKXPACK's. The hkClass
  OBJECTS are not on disk: the tool emulates each `dynamic initializer for
  'XClass''` with capstone; the member `enum*` slots are zero on disk and
  patched by `... 'XClass_Members''` initializers, emulated too. A class
  layout question is one JSON lookup now, never a fresh `hkclass_reflect.py`
  run; `hkclass_reflect.py`'s plausibility break over-reads by one row (s9).
* **Read / write / edit any file without the exe:** `release/hkxfile_gate.exe
  --db res/hkclasses_fo4.json dump|roundtrip|census|edit|get FILE` (built by
  `scratchpad/hkxedit1_20260910/build_gate.sh`, Qt6Core only, run from the
  MSYS2 shell), and `python tests/spells/hkxfile_oracle.py dump|roundtrip|
  layout|census`. `layout` prints the first chunks where the writer's rule
  and the file disagree -- it is how every layout rule was found; use it
  before touching the writer.
* **The round-trip gate is the whole archive**: `python tests/spells/hkxfile_gates.py`
  (109 checks, 39 s) -- 15,278 of 15,320 files byte-identical by both
  readers, 42 `hclClothSetupContainer` files refused (the exe lacks the
  class). The set lives in `scratchpad/hkxedit1_20260910/census_hkx/`
  (gitignored; `extract_census.py` regenerates it in 13 s). A writer change
  that is not 15,278/15,278 is a regression, whatever its author says.
* **Layout rules settled** (each cost a mismatch class): objects and array
  payloads 16-aligned before; a payload of STRUCTS is not padded after, one
  of POINTERS or plain values is, one of STRING POINTERS is not and its
  strings sit at even offsets with one 16-pad after the run; a direct string
  pads to 16 after itself; hkRelArray payloads live inside the object's
  chunk after the body, 16-aligned; global fixups follow the flush order
  (a pointer inside an earlier member's array precedes a later direct
  pointer member); objects are written depth-first from the root.
* **A shipped file's shell-facing path is a bash comment when it starts with
  `#`.** The gate binary addresses fields as `#2.numFrames`; quote every
  argument you pass through `bash -lc`, or the edit silently applies nothing
  and the "wrote" line reads as success (MISTAKES.md 2026-09-10 entry 3).
* **The Blocks tab:** `HkxModel : BaseModel` (`src/hkxmodel.{h,cpp}`), the
  KfmModel idiom; `animFile()` hands the document's bytes to
  `hkxAnimLoadPackfile` -- the workspace's reader on the saved bytes, no
  second decoder. Hook-up NOT applied at the time of writing
  (`scratchpad/hkxedit1_20260910/hookup.py`, `PENDING.md`).

## 14. The editable clip and the animation workspace (lane HKXEDIT2, 2026-09-10)

`src/hkxclipedit.{h,cpp}` is THE edit model (QtCore-only; the standalone gate
`release/hkxclipedit_gate.exe` proves it, `scratchpad/hkxedit2_20260910/
build_gate.sh`, 72 checks). `src/animworkspace.{h,cpp}` + `src/animdopesheet.
{h,cpp}` are the dock that replaces the Animation Manager. Do not re-derive:

* **The key model is one sentence.** Keys are SPARSE over the DENSE
  per-frame clip; a track's frames regenerate from its keys (verbatim at a
  key; between keys linear translation/scale + shortest-arc nlerp with an
  exact normalise -- the same law `HkxPlayback::sampleTrack` uses between
  frames; first/last key held outside). A loaded clip has a key at EVERY
  frame, so nothing is lost until `reduce()`. Regeneration touches only the
  edited track: the byte-identity of every other track is a gate, not a hope.
* **Deleting a key that replaced a frame's key does NOT restore the frame** --
  it makes the frame the interpolation of its neighbours (0.877 deg from the
  original on the Mixamo thigh at 46). Undo (a document snapshot per command)
  is the exact way back. Pre-register gates against the model, not the wish.
* **HKXPACK 0.1.6 prints EMPTY annotation text for every file
  `src/hkxwrite.cpp` emits** (its local-fixup ORDER differs from the shipped
  one; 16 bytes in the fixup tables of jog). HKX1's reader and HKXEDIT1's
  oracle read the names fine. Any file meant for HKXPACK -- or for a game
  flight where the names matter -- goes through `Hkx::File` read->write first
  (`WW_HKXCLIP_CANON` in the workspace's Save; `hkxfile_oracle.py roundtrip
  --out` by hand). Then HKXPACK shows the names.
* **The annotation vocabulary is generated, not typed:**
  `res/hkx_annotation_vocabulary.txt` (1,642 names over 43,843 annotations in
  15,278 shipped clips, `scratchpad/hkxedit2_20260910/annot_vocab.py` over
  HKXEDIT1's census set; FootLeft 5,464 / weaponFire 5,397 / FootRight 5,380).
  The `behaivor-graph` skill's event tables are the graph side of the same
  names. The Mixamo fixture itself carries 4 EMPTY annotations at t=0.
* **Retime arithmetic that keeps coincident frames bit-exact:** compute the
  old frame index in double as `i / newFps * oldFps` and snap within 1e-4;
  60->30 gives 47 frames all copied, 30->60 gives 93 with the even ones
  copied. Trim slices verbatim and keys the cut boundaries; root motion is
  sliced, not re-based.
* **Root-motion bake/unbake byte-identical** only by REMEMBERING the removed
  translations and the previous motion samples; `a0 + (a - a0)` is not `a`
  in float. Unbake restores the bytes when the track is untouched since,
  else adds the motion back arithmetically and says so.
* **Playback edits:** `HkxPlayback::replaceClip( name, clip, trackNames )`
  swaps a loaded clip's data (range re-registered, re-bound if active);
  `setHeldNode( blockNumber )` keeps one node out of the pose so the gizmo's
  block writes show in the viewport. `Node::id()` IS the block number.
* **The dock's gate is in-app** (`WW_ANIMWS_TEST`, `tests/spells/animws.sh`,
  gates a-j) and had NOT run when this section was written -- its numbers are
  predictions until `PENDING.md` is resumed.
