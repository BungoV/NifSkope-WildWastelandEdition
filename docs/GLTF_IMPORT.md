# glTF 2.0 ANIMATION import contract

**Status: BUILT AND MEASURED IN THE APPLICATION, 2026-09-10 14:37:53, lane
BUILD8.** `src/gltfimport.{h,cpp}` implements exactly this page, is linked into
`release/NifSkope.exe`, and is reachable as
`NifSkope -no-gui gltf-import <in.gltf> -o OUT.hkx`.

**THE TWO PAGES HAVE NOW BEEN READ SIDE BY SIDE** (lane HKX5b's owed item,
discharged by lane BUILD8, 2026-09-10). Every source both pages cite was
re-hashed first and every one is unchanged. They AGREE on the unit, the up-axis
node and its quaternion, the quaternion component order, scale being
dimensionless, LINEAR being exact for FO4's degree-1 splines, and the
root-motion formula. They DISAGREED on two things, both now written into both
pages: **which node carries the root motion** (section 5) and **the frame rate
the two defaults produce** (section 4).

**The measured round trip, end to end through the built exe:** `.hkx -> .gltf ->
.hkx`, compared against an independent decode of the original, bone for bone and
frame for frame -- worst **8.0e-06 units / 3.41e-05 degrees** on the vanilla
`JogForward` (1,794 bone-frames) and **4.4e-05 units / 5.21e-06 degrees** on the
60 fps Mixamo clip (7,254 bone-frames), against bars of 1e-4 and 0.01 degrees.
Through **Blender 4.5** and back (its own importer and exporter, factory
settings) the same clip lands at **1.006e-04 units / 4.68e-04 degrees** with
Blender's scene rate set to the clip's 30 fps, and at **0.29 units / 4.83
degrees** at Blender's factory 24 fps -- that difference is Blender re-timing
the action, not this page.

It is the inverse of the EXPORT contract,
`docs/GLTF_INTERCHANGE.md` (lane HKX4, `src/gltfexport.{h,cpp}`); where that
page and this one disagree, the disagreement is a defect in one of them and
round trip 2 (gate (b)) catches it.

bungo's ruling, 2026-09-10 (HANDOFF ~06:5x), verbatim: *"gltf sounds good"* —
glTF 2.0 is the animation interchange format.

Authorities: **SPEC** (the glTF 2.0 specification), **EXPORT** (read out of
`src/gltfexport.cpp`, with the line's own text quoted), **GATE** (a number
this lane measured, gate letter given).

---

## 1. What is imported

One `animations[i]` of one `.gltf` (JSON + external `.bin`, or an embedded
`data:` base64 buffer) or `.glb`, turned into one `HkxAnimClip`
(`src/hkxanim.h`) — the same struct lane HKX1's reader produces, so everything
downstream (playback, the writer) takes it without knowing where it came from.

Meshes, materials, skins, cameras, lights, morph-target weights and every
extension are **not read**. A channel whose `path` is not `translation`,
`rotation` or `scale` is named in the report (`unmatchedNodes`, "channels not
carried: …") and skipped, never silently dropped.

## 2. The axis and the units — the two arms

`src/gltfexport.cpp` does **not** rotate bone data. It writes NIF-space TRS on
every node and hangs the whole hierarchy under one synthetic root:

```
// the up-axis root: rotate Z-up (NIF) into Y-up (glTF) = -90 deg about X
j += "{\"name\":" + jstr( scene.upAxisNodeName )
    + ",\"rotation\":[" + num( -s ) + ",0,0," + num( s ) + "]"      // s = sqrt(0.5)
```
[EXPORT, `src/gltfexport.cpp`, the `// nodes` block]

and multiplies every translation, node and channel alike, by
`U = scene.unitScale` = `0.9144f / 64.0f` = 0.0142875 metres per NIF unit
exactly (64 units = one yard) [EXPORT, `GltfExportScene::unitScale`].

So the import has **two arms, and it names which one served**:

* **The node is consumed.** A scene root named `NifSkope_Y_up`, *or* any
  childed, mesh-less scene root with zero translation, unit scale and a
  rotation within 1e-5 of (-√½, 0, 0, √½), is taken to BE the axis conversion:
  it is dropped and its children become the NIF roots. Nothing else changes.
* **The rotation is undone.** A glTF with no such root (Blender's own export)
  gets +90° about X composed into each scene root's own local transform —
  translation rotated, rotation pre-multiplied, scale untouched — and into the
  sampled channels of those roots.

The two are the same arithmetic: for a root child `C` under a node whose
rotation is `Ru`, the glTF world transform is `Ru · Tc`, and `Ru⁻¹ · Ru · Tc`
is `Tc`. Dropping a node whose rotation is exactly `Ru` and applying `Ru⁻¹` to
its children are therefore identical, which is why one importer can serve both
files. `convertUpAxis = false` is the way back and keeps the glTF's own Y-up
frame (CONSTITUTION rule 7).

Refusals: two scene roots that both look like the up-axis node ("which one
converts the axis cannot be guessed"); an up-axis node that carries animation
channels ("consuming it would drop them").

Translations, node and channel alike, are divided by `unitScale`. Scale is
dimensionless and is not touched. **Measured cost of the metre round trip
(gate (b)): max 8.0e-06 NIF units over 1,794 bone-frames** — float32 relative
error on values up to ~70 units.

## 3. Node → bone: three arms, each finished before the next begins

The target skeleton is `GltfImportOptions::skeletonBoneNames`, in bone order.
When it is empty the glTF's own node names become the bone names and the
mapping is the identity — what a round trip through our own exporter wants.

1. **exact** name match;
2. **case-insensitive** — `skeleton.hkx` and `skeleton.nif` disagree in case on
   `Head`, `Spine1`, `Spine2` and `Weapon` (lane HKX1, gate (b) of that lane;
   confirmed here: `SPINE1 → Spine1`, `SPINE2 → Spine2`, `HEAD → Head`,
   `WEAPON → Weapon`);
3. **partial** — the node name contains the bone name or the bone name contains
   the node name, case-insensitively, and **exactly one free bone** matches.
   Two or more is not a match: the node goes to `ambiguous` and is named.

**Each arm runs to completion over every node before the next starts, and an
ANIMATED node outranks a still one for the same bone.** Doing it per node
instead lets a partial match claim a bone that a later node matches exactly:
on `skeleton.nif`, `CamTargetParent` contains `CamTarget`, node order puts it
first, and the real `CamTarget` node lost its bone. This was measured in round
trip 2 on 2026-09-10 and is in the root `MISTAKES.md`.

A bone is driven by at most one node; a second claimant is named
("`X` (bone 'B' already driven by 'Y')"). Every unmatched node and every
unmatched bone is listed by name in `GltfImportReport`. Zero matches refuses.

`includeStaticTracks` (default true) gives a matched node with no channel a
track constant at its bind TRS, because a shipped FO4 clip carries a track for
every bone.

## 4. Resampling

glTF stores key times; a Havok clip stores a uniform frame grid. So:

* the span is `[min first key, max last key]` over every sampler the animation
  actually uses;
* at `targetFps` (default 30 — FO4's third-person rate) the clip gets
  `round(span * fps) + 1` frames, never fewer than 2 (Havok stores a one-frame
  pose as two), sampled at `t0 + i/fps`. `durationDelta` reports
  `|(N-1)/fps - span|`, the cost of landing the last frame on the grid;
* **the default is 30 fps whatever the file says**, while
  `docs/GLTF_INTERCHANGE.md`'s exporter writes the clip's OWN rate, so a
  default round trip of a 60 fps clip comes back at 30 -- 47 frames instead of
  93 (lane BUILD8). `--source-rate` on the CLI keeps the grid, and with it the
  93 frames return one for one;
* `preserveSourceRate` keeps the source's own rate instead, but **only when
  every used sampler shares one uniform grid** (identical key times to 1e-6,
  identical steps to 1e-5 relative). When it does not, the option is honoured
  as far as it can be and the report says so in words rather than pretending.
* over 65535 frames refuses by name.

`HkxAnimClip::duration` is always written as `(numFrames-1) * frameDuration`,
which is the law the writer and the engine both gate on.

Interpolation, per the SPEC:

| glTF | rule |
|---|---|
| `LINEAR` | component lerp for translation and scale; **slerp on the shortest arc** for rotation |
| `STEP` | hold the left key |
| `CUBICSPLINE` | the spec's Hermite, `p = h00·v(k) + h10·td·b(k) + h01·v(k+1) + h11·td·a(k+1)`, with `td = t(k+1)-t(k)`, `a` the in-tangent and `b` the out-tangent; rotations normalised afterwards |

Before the first key and after the last, the key's own value is held.
`BEZIER` or any other word refuses by name.

**Measured (gate (c)):** a hand-written 3-bone glTF carrying one channel of
each interpolation — LINEAR rotation, STEP translation, CUBICSPLINE scale —
imports at 30 fps to the **hand-computed** values with worst |ΔT| **0.0**,
worst angle **4.38e-06 degrees**, worst |Δscale| **3.93e-08**.
**Measured (resampling):** the 60 fps Mixamo fixture resampled to 30 fps gives
47 frames, and every one of them is **bit-identical** to the corresponding
60 fps source frame (max |ΔT| 0.0, max angle 0.0 degrees) — the grid points
that coincide are not merely close, they are exact.

## 5. Root motion, behind a flag

`extractRootMotion` is **false** by default: the root node's travel stays on
its own track and the clip has no `hkaDefaultAnimatedReferenceFrame`, so it
plays where it is authored.

**true** lifts it: the root node is `rootNodeName`, or the single scene root
once the up-axis node is consumed (two roots and no name refuses). For every
frame `f`, against the transform at `rootMotionReferenceFrame` (default 0):

> **NAME THE NODE when the file came from our own exporter** (lane BUILD8,
> 2026-09-10). `src/gltfexport.cpp` composes the travel onto the ROOT BONE's
> node, `Root`; the single scene root of such a file is the NIF's own root
> `NiNode` (`skeleton.nif`), which drives no bone. So the default arm refuses,
> correctly and by name -- *"root motion was asked for from 'skeleton.nif',
> which has no track (it reached no bone)"* -- and the round trip needs
> `--root-node Root`. With it, 165.354 units of travel return to **1.5e-05
> units, 0.0 degrees of yaw**.

```
rootMotion[f].translation = T(f) - T(ref)
rootMotion[f].yaw         = 2 * atan2( (Q(f) · Q(ref)⁻¹).xyz · up, (…).w )
track(root, f)            = the reference transform      // the track plays in place
```

`up` is `rootMotionUp`, (0,0,1) in NIF space. This is the exact inverse of the
exporter's `applyRootMotion` arm (`p = p_track + rootTranslation`,
`q = yaw(f) · q_track`) **when the underlying root track is constant**, which
is true of every FO4 clip seen (track 0 "Root" is identity on all of them).
When it is not constant, whatever the root track did becomes root motion, and
that is a stated loss, not a silent one.

**Measured:** `jog` exported with root motion applied and re-imported with the
flag recovers the samples to **max |ΔT| 1.5e-05 units, max |Δyaw| 0.0 degrees**.

**Known loss:** the exporter's default omits root motion from the channels and
records only a *sentence* in the animation's `extras` ("present in the clip,
omitted on request"), not the numbers. A default export therefore cannot carry
root motion back, and the importer does not pretend otherwise — the clip simply
has none. Export with `--root-motion` to round-trip it.

## 6. What the importer refuses, by name

`asset.version` that is not 2.x; a `.glb` that is not version 2, whose length
disagrees with its header, whose chunk runs past the file, or that has no JSON
chunk; JSON that does not parse (the parser's own message and offset); a
buffer with no `uri` and no GLB chunk, a non-base64 `data:` uri, a missing
external file, a buffer shorter than its `byteLength`; a **sparse** accessor;
an accessor with an unknown `componentType` or `type`, a non-positive `count`,
a `bufferView` that does not exist, a `byteStride` smaller than its element, a
view outside its buffer, or an element run past the view's length; a non-finite
component; a node with a child index out of range, a node that is its own
child, a node with two parents, a hierarchy with a cycle; a `matrix` that is
not 16 numbers, has a zero-length axis, **mirrors** (negative determinant) or
**shears** (axes not orthogonal to 1e-4); a scene naming a node that does not
exist; no nodes; no root; no animation; an animation index out of range; an
animation with no channels or driving nothing; a channel naming a node or
sampler that does not exist; an unknown interpolation; a sampler input that is
not SCALAR, an output whose element count does not match its interpolation, a
key time that does not increase, a sampler whose component count does not fit
its path; a target skeleton not one of whose bones any node reaches; a frame
count over 65535; a root-motion request naming a node that is not there, that
has no track, or with an ambiguous root; a non-positive unit scale or target
rate.

**Measured (gate (d)): 13 corruptions of a valid glTF, 13 refused with a
sentence naming the field and the value.**

## 7. Provenance

| source | sha256 (16) | bytes | role |
|---|---|---|---|
| `src/gltfimport.h` | `0c0da7182b3e965e` | 7,289 | this contract's interface |
| `src/gltfimport.cpp` | `a2cec5a5f738ef57` | 43,805 | this contract implemented |
| `src/gltfexport.h` | `77e2b2f7e2010ca5` | 5,434 | the export side; `unitScale` and `upAxisNodeName` quoted in section 2 |
| `src/gltfexport.cpp` | live, lane HKX4b | — | the up-axis block and the channel writer quoted in section 2; **re-read before quoting again, that lane is still writing it** |
| `src/hkxanim.h` | `bbdaa21fdfb89a56` | 5,009 | `HkxAnimClip`, the output type |
| `tests/hkxwrite_dump.cpp` | `38b312a2e6cf6a9e` | 7,774 | the standalone driver the gates run |
| `tests/spells/hkxwrite_gates.py` | `4ca1dc6bd7d8dac2` | 10,199 | gates (a)-(f), 23/23 |
| `scratchpad/hkx5_20260910/make_3bone.py` | `877a0fe8b42d28d2` | 8,605 | gate (c): the hand-written file AND the closed-form expectation |
| `scratchpad/hkx5_20260910/mutate.py` | `ba188050bf73eaaa` | 10,686 | gate (d): the corruptions and the floor |
| `scratchpad/hkx5_20260910/tsvcmp.py` | see the report | — | the round-trip metric, `4·asin(|q1∓q2|/2)` (the skill's `2·asin` reads half the angle — root MISTAKES.md, 2026-09-10) |
| glTF 2.0 specification | — | — | SPEC: accessors, animation samplers, the three interpolations |
