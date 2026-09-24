---
name: ww-offline-scene-model
description: Answer "what exactly is the viewport drawing, and where" for a NifSkope Wild Wasteland scene WITHOUT running release/NifSkope.exe -- when the build slot is held, the game is up, or the lane is on account B. Builds the viewport's own arithmetic in Python from the NIF and the clip, and then REFUSES to report anything until the model reproduces numbers the built application has already published. Use whenever a lane must measure node positions, bone classes, segment lengths, parent chains or an animated pose and cannot get a build.
---

# NifSkope WW: model the scene offline, with the application as the control

Repo `E:\Projects\NifskopeWildWastelandEdition`. A lane that ends BUILD PENDING
still has to MEASURE (CONSTITUTION rule 4: no cause without a measurement). This
is how, when the exe is out of reach. Written from lane SKELFIX (2026-09-10),
which located a 300-unit drawing defect, sized three candidate fixes and
pre-registered their gate limits without compiling anything.

## 1. The control comes FIRST, and it is the application's own published numbers

An offline model that agrees with itself is worth nothing. Before the model
prints a single row it must reproduce figures the built application has ALREADY
measured and written down -- a harness log, a `## Build` section of a lane
report, the HANDOFF top block. Lane SKELFIX used seven:

```
dock All 130 / Bones 93 / Deforming 93 / Unused 0     (BUILD9's gate table)
clip matched 78 / unmatched 17 / case-folded 4        (the mapping summary)
```

Structure the script so a miss is a REFUSAL, not a warning:

```python
print("CONTROLS (the built exe's own measured numbers)")
for name, got, want in ctl:
    ok = ok and (got == want); print(...)
if not ok:
    return 2          # the table below is not evidence
```

If no published number exists for the area, the model is not ready to be
believed: find one, or produce one from a gate that already ran.

## 2. Parse with a SIZE ASSERTION, never a hopeful struct

Every block parse ends exactly at the header's own size, or it raises. This is
the only thing that separates "I read the format" from "I read plausible
floats", and it settles format questions in one run:

```python
if p != end:
    raise Refusal("block %d %s: parse ended at +%d, size %d" % (i, t, p - o, sizes[i]))
```

Facts settled this way, do not re-derive:

* **FO4 (BSVersion 130) `NiNode` has NO Effects array.** Block 0 of
  `fixtures/human_male_vanilla.nif` is Name + NumExtraData(4) + 4 refs +
  Controller + Flags(u32) + Translation(3f) + Rotation(9f) + Scale + Collision +
  NumChildren + 12 child refs = 140 bytes exactly. Adding `Num Effects` overruns
  by 4.
* `NiAVObject` Flags is a **u32** on this version, not u16.
* `BSSkin::Instance` = SkeletonRoot(ref) + Data(ref) + NumBones(u32) + refs +
  NumScales(u32) + 12 bytes each.
* The NIF header's export strings are **byte**-length-prefixed (`nifhdr.py`
  already gets this right); block-type names are u32-prefixed.

Start from `scratchpad/fixture_20260910/nifhdr.py` (header, types, sizes, string
table) and `scratchpad/skelfix_20260910/nifnodes.py` (hierarchy, bind locals,
skin bone lists) rather than writing a third one.

## 3. The arithmetic the viewport actually uses

Copy these; guessing them produces a model that is wrong by a transpose and
still looks like a skeleton.

* `Transform operator*` (`src/data/niftypes.cpp`):
  `r = r1*r2`, `t = t1 + r1*t2*s1`, `s = s1*s2`. **The parent's scale multiplies
  the child's translation.**
* `Matrix::fromQuat` takes NifSkope's `Quat` = **(w,x,y,z)**; a Havok clip stores
  **(x,y,z,w)**. The hkx quaternion maps to the NiNode rotation matrix directly
  (no transpose -- measured 2.7e-4 by lane HKX1, transposed 2.0).
* A `Transform` carries ONE scale where a Havok transform carries three: take x
  and SAY you dropped y/z, which is what `HkxPlayback::applyLocal` does.
* Parent comes from the **Children arrays**, not from any back-link:
  `skeletonAnalyse()` builds it that way and a node can be a skin bone while
  sitting anywhere in the tree.
* **`Scene::getNodes()` order is the roots' depth-first walk, children in
  order**, and it decides a tie: `HkxPlayback::bind()` matches bone names
  case-insensitively with FIRST NODE WINS, and FO4 body rigs really do carry two
  nodes named `Camera`.

## 4. The animated pose without the engine

`tests/spells/hkxanim_decode.py` is the independent oracle; import it.

```python
import hkxanim_decode as D
anim = D.parse_hkx(CLIP)["animations"][0]
stride = anim["maxFramesPerBlock"] - 1
blk = min(FRAME // stride, anim["numBlocks"] - 1)
pose = D.decode_frame_in_block(anim, blk, float(FRAME - blk * stride))   # [(t, q, s, len)] per track
```

Then: a node with a track takes the clip's local; a node without one keeps its
BIND local; world transforms come from one recursive walk. Two consequences that
are worth stating in any report, because they refute the obvious hypotheses:

* **an untracked node can never lag behind a tracked parent** -- with no track it
  keeps its bind local and inherits its parent's world transform;
* a clip may carry the character's travel on a TRACK (Mixamo puts it on `COM`,
  487 units) with the root-motion channel all zeros, so "does it move" must be
  read off the tracks, not off `rootMotion`.

Bone NAMES are not in a clip: load an `hkaSkeleton`
(`scratchpad/hkx1_20260910/clips/skeleton.hkx`, 95 bones) -- see
`ww-hkx-animation`.

## 5. Measure in the PICTURE'S plane as well as in world units

A world-space length is not what the director saw. For the pinned front
orthographic camera the spells use (`WW_RENDER_VIEW=5`), the screen plane is
**(x, z)** and the whole y axis is depth: lane SKELFIX's worst world segment
(300.5 units, `Root -> COM`) is only 28 px-equivalent on screen, while the two
that dominate the picture are `Camera` and `CamTarget` at 63.0 and 62.2. Report
both columns, or the table explains a different defect from the one in the
image.

## 6. What the model may and may not conclude

* It MAY size candidate rules, produce the numbers a gate is pre-registered
  against, and refuse a proposed mechanism with a count.
* It may NOT stand in for a render (CONSTITUTION rule 5) or for a harness. A
  diagram drawn from the model is labelled a DIAGRAM in the file, in the report
  and in the picture itself, and the owed render stays owed.
* Keep the script in the repo under `scratchpad/<lane>_<date>/` with its output
  as a TSV, so the resuming lane compares the built application's numbers
  against the predictions row by row rather than re-deriving them.
