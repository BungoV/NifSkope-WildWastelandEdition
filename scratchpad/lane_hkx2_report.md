# Lane HKX2 -- Havok animation PLAYBACK + MAPPING (2026-09-10)

Repo `E:\Projects\NifskopeWildWastelandEdition`, main, **nothing committed**.
bungo's ruling, verbatim: *"in animation workspace, add an option to load a hkx
file with animation, then they get added to the animations list, and if there's
rigged geometry with nodes / bone names that match, they play"*. HKX1 was the
reader and the spline decompressor; this lane puts the decoded pose into the
scene graph and binds it to the open NIF's bones. HKX3 owns the rest of the UI.

**STATUS: BUILD PENDING.** `Fallout4.exe` was up (pid 41056) and there was no
`scratchpad/hkx2_20260910/GO`, so nothing was built, no exe was launched, no
gate was run and no picture was taken. Every new and changed file passes
`g++ -fsyntax-only` with the real `Makefile.Release` flags, `ALL-RC=0`, zero new
warnings (`scratchpad/hkx2_20260910/syntax.sh`). Resume:
`scratchpad/hkx2_20260910/PENDING.md`.

Skills invoked: `ww-hkx-animation`, `nif` (the animation/controller path was read
directly in `src/gl/` -- see section 8), `nifskope-ww-panel-style`,
`nifskope-ww-render-shot`, `nifskope-ww-build-verify`,
`nifskope-ww-resume-pending`, `ww-control-calibration`.

## 1. The shape of it, in one paragraph

A loaded clip becomes a NAME in `Scene::animGroups` with its start and end in
`Scene::animTags` (`hkxplayback.cpp:782`, `registerInScene`). `Scene::timeMin`
and `timeMax` already answer out of `animTags`, so play, pause, loop, reverse,
speed, scrub, "cycle through sequences" and the Timeline dock's ruler drive a
Havok clip with **no new transport code at all**. Selecting one goes through
`Scene::setSequence` (`glscene.cpp:395`), which binds it; selecting any of the
NIF's own sequences unbinds it and restores the pose the rig was in. Per frame
`Node::transform()` (`glnode.cpp:517`) calls `applyLocal()`, which writes
`Node::local` -- the same member `TransformController::updateTime` writes
(`controllers.cpp:381`).

## 2. MAPPING, as built

* **`HkxPlayback::mapNames`** (`hkxplayback.cpp:187`) holds a list of bone names
  against every NAMED node in the scene. Two tables: an exact one and a
  lower-cased one, because *matched* and *matched only after folding case* are
  different answers and the summary reports both. First name wins on a
  collision, so the result is a function of the file and not of hash order.
* **`HkxMapping::summary`** (`hkxplayback.cpp:149`) is bungo's summary line and
  covers all three of his cases in one sentence: how many play; the unmatched
  ones **named**, not counted ("17 have no node in this NIF: WeaponBolt,
  WeaponExtra1, ..."); and zero matches as a refusal ("... does not play: none
  of its 95 bones names a node in this NIF (133 named nodes). Open the rigged
  NIF this animation belongs to.").
* **Where the names come from.** A clip carries track -> bone INDEX, never a
  name (`HkxAnimClip::trackToBone`), so `resolveNames` (`hkxplayback.cpp:379`)
  has four arms and states in words which one served, per CONSTITUTION 10:
  (1) a skeleton already loaded this session, including the file's own; (2)
  `skeleton.hkx` beside the clip or in a `CharacterAssets` folder up to eight
  levels above it (`hkxFindSkeletonOnDisk`, `:267`) -- FO4 puts clips in
  `<actor>/Animations/<group>/` and the skeleton in `<actor>/CharacterAssets/`,
  so the walk goes up and looks sideways; (3) the same walk from the OPEN NIF's
  folder; (4) the game archives through `GameManager::get_file`
  (`hkxFindSkeletonInArchives`, `:290`), which is zero-authoring -- the user's
  own BA2s. The FLOOR is a refusal naming the skeleton it wanted, and the clip
  is kept rather than binding tracks to nodes by position.
* **Recomputed per NIF.** `bind()` (`:540`) is called from `setActive` every
  time, and `Scene::make` calls `onSceneRebuilt` (`glscene.cpp:307`), which
  re-registers every clip and re-binds the active one against the nodes that
  exist NOW. `Scene::clear` calls `onSceneCleared` (`:190`), which drops every
  binding before the nodes are deleted.
* **Root motion** is a switch of its own, default OFF (`setRootMotion`, `:692`;
  the row is `nifskope_ui.cpp:26927`). On, it is composed OUTSIDE the root
  bone's own transform (`out = m * out`, `:768`) on the node named by the
  animation skeleton's root bone, with a fallback to the bound node that has no
  bound ancestor; the summary names which node it chose.

## 3. PLAYBACK, as built

* **`sampleTrack`** (`hkxplayback.cpp:94`) is `static` and takes no node: it is
  the decoder's own answer, and it is the instrument gate (a) reads. At a
  decoded frame it returns the stored transform **verbatim** -- no
  interpolation, no renormalisation -- so a frame-exact comparison has no slack
  of its own. Between frames: linear on translation and scale, shortest-arc
  nlerp with an exact normalise on rotation (`hkxNlerp`, `:72`), which is what a
  degree-1 spline does and what the engine does between control points.
* **The frame epsilon** (`:34`, `hkxFrameAt` `:38`). `5 * (1/30)` divided back
  by `1/30` is 4.99999952, not 5. Without the snap, asking for frame 5 would
  blend in 99.99997% of frame 6 and gate (a) would fail by the whole distance
  between two frames. 1e-4 of a frame is 3.3 microseconds at 30 fps.
* **Where the pose is applied, and why exactly there** (`glnode.cpp:517`).
  Immediately after `IControllable::transform()` -- so a clip WINS over a
  `NiTransformController` that names the same node instead of losing to it --
  and before this node's collision body is the first thing in the frame to ask
  for a world transform, and before any child is walked. Putting it in
  `Scene::transform` after the roots walk would instead have needed a second
  `transformCache.clear()`, which would have thrown away the `bhkBodyTransKey`
  entries `Node::drawHvkConstraint` reads (`glnode.cpp:1116`).
* **Not gated on `Scene::animate`**, deliberately. The pose at the current time
  is a state, not a simulation step: gating it would FREEZE the last pose when
  "Animations" is unticked rather than restoring the rig, which is worse than
  leaving it posed. "(no sequence)" is how you get the bind pose back.
* **ADDITIVE clips** (230 + 29 in the archive, per HKX1's census) are composed
  on the pre-pose transform rather than replacing it (`:757`), and the summary
  says so.
* **Scale.** A NifSkope `Transform` carries ONE scale, a Havok transform three.
  `x.scale[0]` is taken and y/z are dropped (`:751`); HKX1 measured every FO4
  clip as uniform, so this loses the difference, never the pose. Stated rather
  than silently averaged.
* **Unloading is exact** (`restore`, `:632`). The pre-pose `Transform` of every
  node the clip touches is kept BY VALUE at bind time and assigned back --
  the same bit patterns, not a re-read of the NIF through
  `Transform(nif, index)`, which could not promise identity.

## 4. The gates, pre-registered, WRITTEN AND UNRUN

`src/hkxplaybacktest.cpp` (its own translation unit, so 400 lines of gates cost
`nifskope_ui.cpp` three), driven by `tests/spells/hkxanim_play.sh`. Fixtures:
`skeleton.nif` (the player rig) and `jog.hkx` + `skeleton.hkx` from HKX1's
fixture set, and `35CourtSign01.nif` -- a road sign -- as the no-match NIF.

| gate | what it asks | floor beside it |
|---|---|---|
| (a) | at frames 0, N/2, N-1 every matched NiNode's `localTrans()` equals `sampleTrack` for that frame: translation and scale <= 1e-4, rotation <= 0.01 deg | the SAME test against frame 0 while the scene stands at the last frame must go RED |
| (b) | after unload, every `Transform` in the scene is byte-identical to the pre-load snapshot (memcmp, all 13 floats, every node not just the bound ones) | while the clip is active, > 0 nodes must differ |
| (c) | the mapping on skeleton.nif is 78 matched / 17 unmatched / 4 case-folded, and the summary NAMES the unmatched | 95 invented names -> 0 matched, 95 unmatched |
| (d) | on a NIF with no matching bones: the clip still LOADS, `setActive` returns false, the sentence says "does not play", and 0 nodes changed | the load itself must succeed, or the refusal is a load failure wearing a refusal's words |
| (e) | four renders, same pinned camera: bind pose, frame 0, N/2, N-1 (`scratchpad/hkx2_20260910/shots.sh`) | >= 3 of the 4 md5s distinct -- four identical files mean the pose never reached the rig |
| (f) | `tests/spells/hkxanim_play.sh` reads each of the above by name out of `release/ww_hkxanim_test.log` and exits non-zero unless the log says PASS | -- |

The rotation metric is HKX1's `2*asin(|q1 -+ q2|/2)` (`hkxplaybacktest.cpp:125`),
never `2*acos(|dot|)`, which has no resolution below 0.03 degrees and so cannot
hold a 0.01-degree gate.

Gate (a) reads `Node::localTrans()`, the scene's own member, not the playback's
record of what it wrote: telemetry echoes truth, never intent.

**The 78 / 17 / 4 in gate (c) is lane HKX1's measurement, not this lane's.** It
was pre-registered in the brief before this code was written and this lane has
not been able to run it.

## 5. What was NOT measured

Everything. No build, no harness, no picture. In particular these are
UNVALIDATED claims of mechanism, each with what would refute it:

* That the pose reaches the scene at all -- refuted by gate (e) producing four
  identical PNGs, or gate (a floor) passing when it should fail.
* That `Node::transform()` is early enough for skinning -- refuted by a render
  where the bones move and the mesh does not.
* That the four skeleton-finding arms actually fire on bungo's tree. Arm 4
  (`GameManager::get_file`) has never been called with a `.hkx` path in this
  program; the format is game-agnostic but the resolution is not tested.
* That `Quat::slerp` is avoided for a reason -- `Quat::normalize()`
  (`niftypes.h:868`) divides by the SQUARED magnitude, so it only normalises
  quaternions that are already unit. That is a live defect in shared code; this
  lane worked around it (`hkxNlerp`) rather than touching another lane's file,
  and it is reported here rather than fixed.

## 6. What HKX3 still needs

1. **The Animation Manager dock's own sequence list** (`src/ui/widgets/timeline.cpp`,
   `seqBox`, `:1523`). It is populated from `NiControllerSequence` BLOCKS
   (`QPersistentModelIndex`), so a loaded clip -- which has no model index --
   cannot appear there without a second list beside `sequences`. The render
   toolbar's Animation panel picks the clip up for free because it reads
   `Scene::animGroups`; the dock does not.
2. **An unload row.** `HkxPlayback::unload(name)` exists and is gate (b)'s
   subject, but nothing in the UI calls it. A per-entry remove, or a "Clear
   loaded animations" button.
3. **Where the Load button lives.** It went into the render toolbar's Animation
   panel at grid row 8 (`nifskope_ui.cpp:26890`) because that panel is where the
   Sequence, Loop, Speed and Scrub rows already are. If bungo means the
   Animation Manager DOCK by "animation workspace", the same three widgets move
   there and this block is deleted.
4. **The panel style pass.** `nifskope-ww-panel-style` wants every control
   through `wwHeading` / `wwMatchFieldStyle` / `wwGuardWheel` and every rule
   counted in a self-test with a floor. The three widgets added here follow the
   neighbours' `boxQss` and the one-field-per-row grid, but there is no
   self-test count for them.
5. **The summary label is the only feedback.** A clip that refuses says so in
   the label under the button; it does not mark the list entry.

## 7. Mistakes (also in MISTAKES.md)

1. A Bash heredoc halved the backslashes in the `NifSkope.pro` patch script --
   the exact trap HKX1 recorded the same day and that
   `nifskope-ww-build-verify` and `nifskope-ww-resume-pending` both name. Three
   earlier heredoc edits in this lane worked because they carried no backslash,
   which is what lulled it. The rule is now stated as: **any patch script whose
   anchor contains a backslash goes through the Write tool.**
2. `src/hkxplayback.cpp` was written assuming `Node::local` and `Node::parent`
   are public, from reading the member block under a `public:` and missing the
   `protected:` between. They are protected, and every class that writes them is
   a `friend` at the top of `class Node`. Six compile errors; `HkxPlayback` was
   added to that friend list rather than the members made public. Caught by the
   syntax pass in 40 seconds, which is what it is for.

## 8. Finished-work skill review

**Loaded and used in earnest:** `ww-hkx-animation` (the settled facts -- Havok
(x,y,z,w) vs NifSkope (w,x,y,z), the direct quaternion-to-matrix mapping with no
transpose, the case-differing names, degree-1 = lerp between frames, root motion
separate from track 0 -- saved this lane every one of those derivations);
`nifskope-ww-build-verify` (the syntax pass with the real flags, and the heredoc
trap it warns about, which was ignored once); `nifskope-ww-resume-pending` (the
PENDING format, qmake-before-make, the dependency read-back);
`nifskope-ww-render-shot` (the invisible second-monitor rules that
`shots.sh` obeys).

**Declined, with the reason:** `ww-control-calibration` -- there is no
"ours vs vanilla's" scalar here; the oracle is the decoder itself, and the floor
is a deliberately wrong frame. `nifskope-ww-panel-style` was read but its
self-test counts belong to HKX3, which owns the rows.

**Amendment owed to `ww-hkx-animation`** (repo tree
`.claude/skills/ww-hkx-animation/SKILL.md`; the director mirrors it to the live
tree). Section 8 "Settled, do not re-derive" should gain, because this lane
worked all of it out from the source:

* **The playback path is `Node::local`, written from `Node::transform()` right
  after `IControllable::transform()`.** Not `Scene::transform`, which would need
  a second `transformCache.clear()` and would drop the `bhkBodyTransKey`
  entries the constraint drawing reads.
* **A clip is an animations-list entry for free**: put the name in
  `Scene::animGroups`, `{start,end}` in `Scene::animTags`, `CycleLoop` in
  `Scene::animCycle`. `Scene::timeMin/timeMax` short-circuit on `animTags`, so
  the whole transport follows with no changes.
* **`Node::local`, `Node::parent`, `Node::children` and `Node::nodeId` are
  PROTECTED**; writers are declared `friend` at the top of `class Node`.
* **`Quat::normalize()` divides by the squared magnitude** and must not be used;
  `Quat::slerp` is Blow's approximation and returns `p` exactly at t=0.
* **A frame time needs an epsilon**: `N * frameDuration / frameDuration` is not
  `N` in float, and without a 1e-4-of-a-frame snap a frame-exact read is an
  interpolation between two frames.

**A skill that should exist and does not:** *"add a WW_*_TEST harness"* -- the
in-app harness shape (the `completeLoading` + `singleShot(1500)` pattern, the
`check`/`fails` counter, `release/ww_<name>_test.log`, PASS/FAIL/done, clearing
the undo stack before quitting, the `_harness.sh` window rules, the sub-49152
port, and the "put it in its own translation unit when `nifskope_ui.cpp` is
contended" trick this lane used) was reconstructed by reading
`WW_CYCLETYPE_TEST` and copying it. That is a procedure done in most lanes in
this repo and it costs a full read of a 31,000-line file every time. It is
**not written here** because writing it properly needs a survey of the thirty
existing harnesses to say which parts are the convention and which are one
lane's habit, and this lane ended BUILD PENDING with an unrun gate -- proposing
a convention from one example would be exactly the "typed from memory" failure
rule 1a warns about. Recorded as owed, with the two files to read first
(`src/nifskope_ui.cpp` WW_CYCLETYPE_TEST at `:7721`, `tests/spells/_harness.sh`).

## 9. Files

New: `src/hkxplayback.h`, `src/hkxplayback.cpp`, `src/hkxplaybacktest.cpp`,
`tests/spells/hkxanim_play.sh`, `scratchpad/hkx2_20260910/`
(`syntax.sh`, `shots.sh`, `PENDING.md`, `WW_CHANGES_ENTRY.md`).
Changed (`git diff -U0`, no deletions anywhere): `NifSkope.pro` +3,
`src/gl/glscene.h` +10, `src/gl/glscene.cpp` +22, `src/gl/glnode.h` +3,
`src/gl/glnode.cpp` +15, `src/nifskope_ui.cpp` +87 in four spots (include;
`wwHkxAnimHarness( skope )` at `:7720`; `WW_HKXANIM_CLIP` in the render hook at
`:21666`; the Load / summary / Root motion rows at `:26878`), `MISTAKES.md` +2
entries. Every file LF-only, CR=0, measured with Python byte counts.


## Build (BUILD7)

Built and gated by lane BUILD7, 2026-09-10, on `main`, **nothing committed**.
`Fallout4.exe` and `NifSkope.exe` were both down at the build and before every
one of the 15 exe launches (`rc=1` each time).

**The build.** `qmake NifSkope.pro` RC=0, then `make -j2` RC=0 (a full rebuild:
qmake regenerating the Makefile invalidated every object). The dependency
read-back the resume asks for, per object rather than by `grep -A`:
`hkxplayback.h` is named by `glnode.o`, `glscene.o`, `nifskope_ui.o`,
`hkxplayback.o` and `hkxplaybacktest.o` -- all five -- and `hkxanim.h` by those
five plus `hkxanim.o`, `gltfimport.o` and `hkxwrite.o`. `release/NifSkope.exe`
is newer than all **70** changed files under `src/ res/ tools/ tests/`.
`res/style.qss` and `release/style.qss` compare equal.

**Lane HKX5's hook-up was applied in this build** (it is a refusing script and
had to be): `scratchpad/build7_20260910/apply_hookup.py` imports HKX5's own
`EDITS` table rather than retyping the anchors, and put
`src/gltfimport.{h,cpp}` and `src/hkxwrite.{h,cpp}` into `NifSkope.pro` --
18,779 -> 18,859 bytes, LF 717 -> 721, CR 0 unchanged. Lane HKX4b's
`src/gltfexport.*` was NOT touched and is NOT in the `.pro`.

### The gates

| gate | expected | measured | verdict |
|---|---|---|---|
| `hkxanim_gates.py` | 134 checks / 3 fixture failures | 134 / 3 | **PASS** |
| `hkxanim_synthetic.py` | 29 / 0 | 29 / 0 | **PASS** |
| `hkxanim_mutate.py` | 20 / 0 | 20 / 0 | **PASS** |
| (g) identity rule, C++ | 8,835 rows, `bone == track` on all | 8,835 rows, 0 mismatches; no refusal | **PASS** |
| (g) `identity_floor.py` | 6 / 6 | 6 / 6 | **PASS** |
| (a)(b)(c)(d)(f) `hkxanim_play.sh`, `skeleton.nif` | PASS | 27 checks, 0 failures | **PASS** |
| (a)(b)(c)(d)(f) `hkxanim_play.sh`, `human_male_vanilla.nif` | 78 / 17 / 4 | 27 checks, 0 failures, 78 / 17 / 4 | **PASS** |
| (e) pictures, `jog` | 4 PNGs, >= 3 distinct | 4 of 4 | **PASS** |
| (e) pictures, Mixamo | 4 PNGs, >= 3 distinct | 3 of 4 | **PASS, with a caveat below** |

The three failures inside `hkxanim_gates.py` are the pre-registered fixture
ones, named: the 17 `Weapon*` bones with no node in `skeleton.nif`; the
`weapon` bone's reference-pose translation at 0.00148 against a 1e-3 bar; and
the furniture `Tpose` clip not being the bind pose on 79 bones.

Numbers worth keeping from `hkxanim_play.sh` (they are the ones the resume's
refuters turn on): (a) worst translation **0**, rotation **7.64e-06 deg**,
scale **0** over 234 comparisons at frames 0, 11 and 22, with `scene t=` equal
to the asked-for time at every frame; (a floor) the same test against the wrong
frame goes red at 5.85e-05 / **0.218 deg**; (b) **0 of 129** nodes differ after
unload with (b floor) **78** differing while posed; (d) the road sign loads,
`setActive` returns false, the sentence contains "does not play", **0 of 2**
nodes change.

### (e)'s caveat, and the frames bungo asked for

Gate (e) passes on the Mixamo clip at 3 distinct of 4, but the two that match
are **5,052 bytes of empty background**. Cause, measured: that clip's COM track
travels **487 units in +Y** (frame 0 y=-0.7, frame 92 y=487.0) while its
root-motion channel is all zero, and gate (e)'s pinned camera is the 35-degree
PERSPECTIVE one at `dist=260` -- so by t=0.767 the figure is 750+ units from
the eye. Not a defect in the playback; a defect in that framing for that clip.

The frames for bungo were therefore rendered on an **orthographic** front
camera, where the travel runs along the view axis and changes nothing:
`scratchpad/build7_20260910/frames.sh`, the pin read back at every grab as
`arm=center/ortho/view view=5 lookat=0,0,62 halfW=80 halfH=70.6854 persp=0
vp=1065x941 upp=0.150235`. The same pin served all 13 pictures, and the two
sheets' bind-pose tiles are byte-identical (md5
`580245fba8bad849de9c5f0a7e9f44bb`) -- that identity is the pin's own proof.
6 of 6 tiles differ within each sheet.

* `scratchpad/build7_20260910/frames_jog.png` -- bind pose + frames 0, 6, 11,
  17, 22 of `jog.hkx`.
* `scratchpad/build7_20260910/frames_mixamo.png` -- bind pose + frames 0, 23,
  46, 69, 92 of the Mixamo clip, plus a **side view at frame 20** (t=0.333334),
  the clip's lowest COM (z=14.89) and so its mid-slide.

### Mistakes found while building (both in the root `MISTAKES.md`)

1. **The resume's own `SRC` and `CLIP` are relative paths, and both produce a
   green-looking falsehood.** `SRC=fixtures/human_male_vanilla.nif` opened a
   scene of one unnamed node -- "0 bones matched (expected 78)", 8 failures of
   27 -- and `CLIP=scratchpad/.../jog.hkx` produced four identical pictures,
   which is gate (e)'s refuter for "the pose never reached the rig". Git-Bash
   gets no MSYS2 path conversion and `winpath()` only rewrites the `/e/...`
   form. With absolute Windows paths: 27/0 and 4 of 4.
2. **The `WW_HKXANIM_CLIP` hook discards the loader's refusal string**
   (`src/nifskope_ui.cpp`: `const QString hkxErr = ...` is only ever tested for
   emptiness). That is why (1) cost a whole render round. Reported, NOT fixed --
   `nifskope-ww-resume-pending` rule 6.

### What was NOT measured

* **Fallout 4 has never loaded any of this.** No flight was run; the game was
  deliberately down for the whole lane.
* The round trip bungo actually asked for -- export an animation, import it
  back, hold the two against each other 1:1 -- is the NEXT step and waits on his
  word about the frames.
* Lane HKX5's own 23 gates were not re-run: they run on
  `release/hkxwrite_dump.exe`, which linking the same two translation units into
  `NifSkope.exe` does not change.
* No harness outside the animation set was run. The build is a full rebuild, so
  every other feature's harness is a candidate; none of them reaches this
  change, and none was run.

### Mtimes, in one table

| artefact | mtime | bytes |
|---|---|---|
| `release/NifSkope.exe` | 2026-09-10 14:04:34 | 19,197,952 |
| `release/style.qss` | 2026-09-10 14:04:35 | 11,097 (compares equal to `res/style.qss`) |
| `release/hkxanim_dump.exe` | 2026-09-10 14:05:13 | 526,009 |
| `release/hkxwrite_dump.exe` | 2026-09-10 13:41:42 | 795,378 (lane HKX5's, not rebuilt) |
| `fixtures/human_male_vanilla.nif` | 2026-09-10 05:29:52 | 338,563 |
| `scratchpad/build7_20260910/frames_jog.png` | 2026-09-10 14:14:09 | 602,781 |
| `scratchpad/build7_20260910/frames_mixamo.png` | 2026-09-10 14:14:09 | 626,212 |

### Skill review (CONSTITUTION 1a)

**Loaded and used:** `nifskope-ww-resume-pending` (the read order, qmake before
make, the per-object dependency walk, the whole-working-set staleness sweep, and
rule 6 -- measure the cause, do not land the fix -- which is why the
`WW_HKXANIM_CLIP` swallow was reported instead of patched);
`nifskope-ww-build-verify` (make's own exit code as the gate, the stylesheet
compare, the "successful build is not a consistent one" object check);
`nifskope-ww-render-shot` (the pin, `upp`, the absolute-path rule, and the
orthographic arm that solved the Mixamo framing); `ww-hkx-animation` (the gate
commands and section 11's playback facts); `ww-anchored-hookup` in spirit,
through HKX5's own script.

**The skill that should have existed and did not:** none new was written. The
two procedures this lane could have re-derived -- composing a labelled contact
sheet from renders, and applying another lane's refusing hook-up -- are already
covered by `ww-texel-picture`'s caption rules and `ww-anchored-hookup`
respectively, and both were followed rather than reinvented.

**Amendments owed and made:** `nifskope-ww-render-shot` and `ww-hkx-animation`
gain the absolute-INPUT-path rule (they carried it only for `WW_*` OUTPUT
paths), with the measured symptom -- a scene of one unnamed node, and four
identical pictures -- so the next lane recognises it in one line instead of a
render round.
