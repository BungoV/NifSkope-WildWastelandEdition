# Lane SKELFIX -- the skeleton overlay's stray segments

The director, on `scratchpad/skeloverlay_20260910/on_frame46.png` (BUILD9,
2026-09-10): *long segments fan from the character to a point far above and
left*. BUILD9's own gate table said `17 checks, 0 failures, PASS` on that frame.

Sections: 1 the instrument, 2 the measurement, 3 the fix and why the brief's own
rule was refused, 4 the files, 5 the gates, 6 the pictures, 7 build state,
8 mistakes, 9 the finished-work skill review.

## 1. The instrument: an offline model with the application as its control

Nothing could be built (BUILD10 held the slot) and no picture could be
re-rendered, so the measurement was made offline, from the same three sources
the application reads and from nothing else:

* the NIF's NiNode hierarchy and bind locals -- `scratchpad/skelfix_20260910/nifnodes.py`,
  a size-checked parser (every block's parse must end exactly at the header's
  own size, so a wrong field width refuses instead of producing plausible
  numbers). It settled one format fact on the way: **FO4 (BS 130) `NiNode` has
  no Effects array** -- block 0 is name + 4 extras + controller + flags + TRS +
  collision + 12 children = 140 bytes exactly, and a `Num Effects` u32 overruns;
* the skins' `BSSkin::Instance` Bones arrays -> the Skeleton Manager's classes;
* the clip's decoded pose at frame 46 (`tests/spells/hkxanim_decode.py`), mapped
  to nodes by the rule `HkxPlayback::bind()` itself uses: case-insensitive,
  first node wins, one track per node.

**The model is not trusted on its own word.** It reproduces, exactly, seven
numbers the built application had already measured:

| control | model | the exe |
|---|---|---|
| dock All / Bones / Deforming / Unused | 130 / 93 / 93 / 0 | 130 / 93 / 93 / 0 |
| the clip's matched / unmatched / case-folded | 78 / 17 / 4 | 78 / 17 / 4 |

`measure.py` refuses to print its table if any of the seven misses.

## 2. What those segments are

At frame 46 the character has travelled: **COM is at (-13.6, 299.2, 24.9)** and
the character is **38.3 units tall**. The clip carries the travel on the COM
TRACK (487 units in Y over the clip); its root-motion channel is all zeros.
Meanwhile a set of nodes stays where the file or the clip puts them, at the
world origin. The overlay drew a body between every parent and child in its
list, so each of those was tied to a relative that had moved.

**Every segment whose endpoint is more than 2x the character's height from COM
(7 of the 129 drawn):**

| child | class | has a track | parent | p. track | length | \|child-COM\| | child world pos |
|---|---|---|---|---|---|---|---|
| CharacterBumper | not-a-bone | no | skeleton.nif | no | 25.3 | 275.3 | (-0.6, 25.3, 0.0) |
| EyeLeftDummy001 | not-a-bone | no | skeleton.nif | no | 7.1 | 294.1 | (-2.1, 6.1, 2.8) |
| Root | not-a-bone | yes | skeleton.nif | no | 0.0 | 300.5 | (0, 0, 0) |
| Camera Control | not-a-bone | yes | Root | yes | 0.0 | 300.5 | (0, 0, 0) |
| AnimObjectA | not-a-bone | yes | Root | yes | 0.0 | 300.5 | (0, 0, 0) |
| AnimObjectB | not-a-bone | yes | Root | yes | 0.0 | 300.5 | (0, 0, 0) |
| CamTargetParent | not-a-bone | no | Root | yes | 0.0 | 300.5 | (0, 0, 0) |

**Every segment longer than 1.5x the longest bind-pose bone (22.40 -> 33.60):**

| child | class | has a track | parent | length |
|---|---|---|---|---|
| COM | not-a-bone | yes | Root | **300.5** |
| CamTarget | not-a-bone | yes | CamTargetParent | **289.6** |
| Camera | not-a-bone | yes | Root | **288.1** |

Both tables are one file: `scratchpad/skelfix_20260910/segments_frame46.tsv`
(129 rows: child, class, tracked, parent, parent class, parent tracked, length,
distance from COM, world position, bind length, kept by the new rule).

The eight remaining `AnimObject*` nodes, `PipboyBone`, `WEAPON` and `WeaponLeft`
are in the same class and at the same kind of position; they simply are not the
child of a *moved* parent, so they show as markers rather than long lines.

**The brief's expected suspects, checked one by one.** Camera / CamTarget /
Camera Control: confirmed, and they carry the two longest segments after COM.
AnimObject*: confirmed as origin-parked, drawn as zero-length bodies. Nodes at
the scene origin while the character slid 487 units: confirmed, that is the
mechanism. `EyeLeftDummy001`: confirmed. **The 17 unmatched `Weapon*` bones are
NOT among them** -- they are bones of the animation skeleton with no node in this
NIF at all, so the overlay never had a node to draw them at; the two weapon
NODES this NIF does have (`WEAPON`, `WeaponLeft`) are both tracked and both ride
their tracked parents correctly.

## 3. The fix, and why the brief's own rule was refused

### What the brief asked for, and what it does to this file

> the overlay draws a segment only when BOTH ends are bones the Skeleton Manager
> classes as bones (its Bones filter), and, when a clip is playing, only when
> the child has a track OR its parent chain is entirely tracked

Measured on the fixture, that rule **cuts the skeleton apart**. FO4 body meshes
are weighted to the `*_skin` helper bones, so the animation chain itself is not
in the Bones filter: `Pelvis`, `COM`, `LLeg_Thigh`, `LLeg_Calf`, `LArm_UpperArm`,
`LArm_UpperTwist1/2`, `LArm_ForeArm1..3` and their right-hand twins are all
"not a bone" to the dock. **33 deforming bones have a not-a-bone parent** --
`LLeg_Foot`, `SPINE1`, `LArm_Hand`, every `*_skin` bone.

The second clause is the wrong instrument, and this is measurable rather than an
opinion: **an untracked node cannot lag behind a tracked parent.** With no track
it keeps its bind local, so its world transform is its parent's -- it rides along
by construction. The nodes that "stay behind" are TRACKED ones the clip parks at
the origin (`Root`, `Camera`, `CamTarget`, `Camera Control`, `AnimObject*`),
which the clause keeps. What it removes is 48 untracked `*_skin` bones that were
drawing correctly.

| rule at frame 46 | segments of 129 | longest | worst endpoint from COM | screen-plane longest |
|---|---|---|---|---|
| what shipped in BUILD9 | 129 | **300.5** | **300.5** | 63.0 |
| Bones filter, both ends | 60 | 22.4 | 73.0 | 22.4 |
| Bones filter + the track clause | 40 | 22.4 | 73.0 | 22.4 |
| **armature closure (shipped)** | **110** | **31.9** | **73.0** | 31.6 |
| the pre-registered limits | -- | <= 33.60 | <= 76.6 | -- |

### The rule that shipped

A bone body is drawn only between two ARMATURE nodes, where the armature is

> every node a skin lists (the Skeleton Manager's Bones filter -- Deforming and
> Unused), **closed upwards** through the parent chain, and then **cut** at the
> deepest node that still has every one of those bones at or beneath it.

* the upward closure is what keeps the rig intact through the `*_skin` weighting;
* the cut at the common root is what removes `Root` and the file root above it,
  and with them the 300.5-unit `Root -> COM` segment.

On the fixture the common root is **COM**; **111 of 130** nodes are armature and
the 19 that are not are exactly `AnimObjectA/B/L1-3/R1-3`, `CamTarget`,
`CamTargetParent`, `Camera`, `Camera Control`, `CharacterBumper`,
`EyeLeftDummy001`, `PipboyBone`, `Root`, `WEAPON`, `WeaponLeft` and
`skeleton.nif`. **This is a rule about the file, not a list of names**: those 19
are outside because no skin bone sits beneath them.

A node outside the armature is still listed and still gets its joint marker in
pass 3, so the overlay's census still equals the dock's -- gates (a) and (b) are
untouched by design, which is the reason the harness's 17/0 is expected to hold.

Stubs follow the same test: a bone with no ARMATURE child gets its stub, and the
stub aims at the mean of its DRAWN children only (`boneTailIn` is now handed the
armature list), so a bone whose only child is a camera node does not point at it.

**FALLBACK, named** (CONSTITUTION rule 10): a file with no skin at all -- an
exported `skeleton.nif` -- gives the closure nothing to close over. Every node
keeps its body, and `skeletonOverlayRule()` says that is the arm that served.

### Where the rule is stated

`GLView::skeletonOverlayRule()` returns the sentence for the file that is open,
naming the armature's root and how many nodes are marker-only.
`setSkeletonOverlay(true)` now builds the list on the way IN as well as lazily,
so the sentence exists at the instant the Overlays entry is ticked (the rebuild
was previously deferred to the next draw). The tooltip edit itself is in
`src/nifskope_ui.cpp`, which this lane does not own -- see section 4.

## 4. The files

| file | what | line endings |
|---|---|---|
| `src/glview.cpp` | the armature computation in `refreshSkeletonOverlay()`, the gate in passes 1 and 2 of `drawSkeletonOverlay()`, the eager rebuild in `setSkeletonOverlay()` | CRLF; CR 23,284 -> **23,423**, LF 23,355 -> **23,494**, dCR = dLF = **+139** |
| `src/glview.h` | census field `skipped`; `skeletonOverlayInArmature()`, `skeletonOverlayRule()`; three private members | LF, CR 0 |
| `src/skeloverlaytest.cpp` | gates (f) (f') (g) (g') (h) (i); `skipped` added to the (a') floor | LF, CR 0 |
| `tests/spells/skeleton_overlay.sh` | a 4th render (`off_frame46.png`), gate (j) and its floor (j') | LF, CR 0 |
| `tests/spells/skeleton_overlay_mask.py` | NEW: gate (j) itself | LF, CR 0 |
| `MISTAKES.md` | three sections, appended append-only | LF, CR 0 |

Applied with binary splices (`scratchpad/skelfix_20260910/patch.py`,
`patch2.py`), every anchor asserted to match exactly once, CR/LF counted with
Python before and after. Marker `lane SKELFIX`: 5 in `glview.cpp`, 3 in
`glview.h`.

**One correction to the brief's file list.** It names `src/gl/glscene.cpp` as
"the overlay". The overlay is `src/glview.cpp`
(`refreshSkeletonOverlay` / `drawSkeletonOverlay`, ~line 2193); `glscene.cpp`
carries only lane SKELOVERLAY's `Scene::findNode()` look-up, which needed no
change and was not touched.

**The one file this lane does not own.** `src/nifskope_ui.cpp` (FILESTAB and
HKX3 wrote into it this session) needs one edit so the Overlays entry's tooltip
states the rule and gains the measured sentence when ticked. It is a refusing
script, **checked, not applied**: `scratchpad/skelfix_20260910/hookup.py`
(`--check`: anchor x1, +905 bytes, CR 0 -> 0, marker x2). Nothing here needs it
to compile or to pass a gate; skipping it loses only the tooltip.

## 5. The gates

Pre-registered in the brief, and each new one carries the OLD rule beside it as
its floor, on the same readback.

| gate | what it holds | against what |
|---|---|---|
| (a) (b) (c) (c') (d) (d') (e) | unchanged from lane SKELOVERLAY | the Skeleton Manager dock, a pixel readback, the animated nodes |
| (a') | extended: `skipped` is 0 while the overlay is off | a field never written cannot pass |
| (f) | no drawn segment longer than **1.5x the longest BIND-pose segment between two nodes the dock calls bones** | the bind pose and the dock's classes -- neither is derived from the rule under test |
| (f') | FLOOR: every parent -> child pair, same readback, must FAIL that limit | 300.5 > 33.60 |
| (g) | every drawn segment endpoint inside the bones' own bounding box at that frame + 5% of its diagonal | the frame's own geometry |
| (g') | FLOOR: the old rule must put endpoints outside it | 19 outside, worst 241.3 |
| (h) | `skipped` is WRITTEN and MOVES; `segments + stubs` equals the segment list the mask is built from; the joint count is untouched | the three rules of 2026-09-04 21:33 |
| (i) | the rule is stated in words; the armature has both members and non-members | 111 / 19 |
| (j) | **in the picture**: every pixel the overlay changed in `on_frame46.png` is inside the character's bounding box measured from `off_frame46.png` | a render, not a count |
| (j') | FLOOR: the BUILD9 picture must FAIL (j) | `before_on_frame46.png`, kept in the tree for this |

Gate (j) is `tests/spells/skeleton_overlay_mask.py`. The box comes from the
frame with the overlay OFF so the overlay cannot enlarge the box it is judged
against; it also floors on "the box is not the whole frame" and "some pixels
changed at all". It was exercised end to end on the bind-pose pair that already
exists (`off.png` vs `on.png`): **5 checks, 0 failures**, 42,623 pixels changed,
0 stray, box 63.0% of the frame.

Every other row above is a PREDICTION, not a result: nothing was built.

## 6. The pictures

**Owed and NOT delivered: the re-rendered `on_frame46.png`.** It needs the
build.

Delivered as the interim, and clearly not a substitute:
`scratchpad/skelfix_20260910/rule_before_after.png` -- the two rules' drawn
segments at frame 46, both panels in the same front-orthographic projection at
one scale, magenta for a segment over the 33.6 limit, the world origin marked.
Left: 129 segments, longest 300.5, 3 over the limit, three magenta lines
converging on the origin. Right: 110 segments, longest 31.9, none over the
limit, and the rig visibly intact. It is a DIAGRAM of measured positions, not a
render through the hook, so it does not discharge CONSTITUTION rule 5.

`scratchpad/skelfix_20260910/before_on_frame46.png` is BUILD9's picture, copied
into this lane's folder because it is gate (j')'s only floor.

## 7. Build state: BUILD PENDING

The brief allowed ONE check of the handshake:

| checked once, 2026-09-10 ~16:2x | result |
|---|---|
| `scratchpad/build10_20260910/DONE` | **absent** |
| `scratchpad/build10_20260910/` | `BUILDING_WATER5` (16:20), `DONE_WATER4` -- BUILD10 is mid-chain |
| `tasklist \| grep -i -E "Fallout4\|NifSkope"` | no match, `rc=1` |
| `release/NifSkope.exe` | 15:52:46, BUILD9's, untouched |

Nothing was built, no `BUILDING` marker was created, the exe was not touched.
Resume: `scratchpad/skelfix_20260910/PENDING.md` (five steps: the hook-up, the
two-header object sweep, the build, the gate with its predicted numbers, the
neighbours, the four documents).

**What IS proven now.** `g++ -fsyntax-only` with `Makefile.Release`'s own flags:

```
== src/glview.cpp          RC=0
== src/skeloverlaytest.cpp RC=0
```

`src/glview.cpp` emits the same five warnings lane SKELOVERLAY recorded as
pre-existing (a `QImage::mirror` deprecation, three unused locals in `gizmoEnd`,
a dangling reference in `bevelSelection`); none is in this lane's code.
`bash -n` on the spell and `py_compile` on the mask script both clean. A syntax
pass proves the files compile and **nothing about the link or about behaviour**.

Nothing is committed (CONSTITUTION rule 8).

## 8. Mistakes

Written into `MISTAKES.md` at the root, three sections, verbatim there:

1. **The brief's own rule would have cut the skeleton in half** -- the Bones
   filter alone draws 60 of 129 segments (40 with the track clause) because FO4
   body meshes weight the `*_skin` helpers, so 33 deforming bones have a
   not-a-bone parent. Implementing it would have passed the gates by drawing
   less skeleton. The rule: a pre-registered MECHANISM is a hypothesis about the
   data until the data is read; the pre-registered OUTCOME gates survived and
   were used unchanged.
2. **A picture was shipped as proof that no count could check** -- BUILD9's 17/0
   was sound and blind: no check measured the length of a body or where it went,
   and the mask gate could not catch it because the mask is rasterised from the
   segments the overlay reports. When the deliverable is a picture, one gate has
   to read the picture.
3. **`WW_RENDER_SIZE` did not reach the picture** -- the spell exports
   `1000x1000` and all three delivered images are `1437x941`. Not this lane's
   code and not diagnosed; recorded because two of BUILD9's percentages are
   fractions of a frame whose size was not the one asked for. Discriminator:
   one render at 400x400.

## 9. Finished-work skill review (CONSTITUTION rule 1a)

**Loaded and used:** `ww-hkx-animation` (decisive -- the clip fixtures, the
identity binding, `hkxanim_decode.py`'s API, the 78/17/4 mapping, and the fact
that this clip carries its travel on the COM track rather than in root motion,
which is the whole mechanism of the defect; without it this lane would have
re-derived a packfile walker), `nifskope-ww-build-verify` (the syntax pass with
the real flags, and the `sx_<lane>.sh` naming amendment SKELOVERLAY added --
`sx_tmp.sh` was again present in the tree from another lane),
`ww-anchored-hookup` (the refusing script and the marker-string test rather than
the anchor), `nifskope-ww-resume-pending` (the shape of `PENDING.md`, the
two-header object sweep, "a PENDING's numbers are predictions", and the rule
that a kept before-picture is deleted only after the gate that replaced it
passed -- which is why `before_on_frame46.png` is in the tree).

**Not loaded, with the reason:** `nifskope-ww-render-shot` -- no render could be
taken (BUILD PENDING) and the spell's camera pin was already written by lane
SKELOVERLAY; the resume runs it unchanged. `ww-control-calibration` -- the
control here is not a signal floor but the application's own seven published
numbers, which is a stronger control than a synthetic one.
`nifskope-ww-panel-style` -- no widget.

**The skill that should have existed, and now does:**
`E:\Projects\Claude\.claude\skills\ww-offline-scene-model\SKILL.md` --
*model the viewport's own arithmetic offline when the exe cannot be run, and
prove the model against the application's published numbers before believing
it.* This lane had to work out from scratch: that FO4 `NiNode` has no Effects
array and that a block-size assertion is what finds that in one run; that
`Transform operator*` is `r1*r2`, `t1 + r1*t2*s1`, `s1*s2` and `Matrix::fromQuat`
takes NifSkope's (w,x,y,z) while Havok stores (x,y,z,w); that `Scene::getNodes()`
order decides which of two same-named nodes wins a case-insensitive bind; and,
above all, the discipline of refusing to print the table unless the model
reproduces the numbers the built application already measured. That is a
procedure with traps, it cost most of the lane's time, and the next lane that
cannot get a build slot will need it. **Written to the LIVE tree only** -- the
repo copy `<repo>/.claude/skills/` is the director's to sync (CONSTITUTION 1a,
the two trees drift).

**Declined, named:** the "binary splice into a CRLF source" procedure is now in
`nifskope-ww-build-verify` and `ww-anchored-hookup` between them; a third home
would drift. Not written.

## Build (BUILD11)

The hook-up applied exactly as this lane predicted: `hookup.py --check` printed
`anchor x1 (CR 0, LF 31680) / would grow by 905 bytes; CR 0 -> 0; marker x2`,
and `--apply` gave `src/nifskope_ui.cpp` 1,484,604 -> 1,485,509 (+905), CR 0,
`grep -c "lane SKELFIX"` = 2. Nothing else needed re-applying.

**The armature rule holds, and every prediction in `PENDING.md` step 3 was
met or beaten:**

| line | predicted | measured |
|---|---|---|
| overlay ON census | segments 110, stubs 62, skipped 38 | **110 / 62 / 38** |
| (f) longest BIND bone-to-bone | 22.40, limit 33.60 | **22.403 over 60 pairs, limit 33.6044** |
| (f) longest drawn at frame 46 | 31.94 | **31.9429** (172 segments) |
| (f') FLOOR, old rule | 300.5 | **300.51 over 129 segments -> red, as it must** |
| (g) endpoints outside the bone box | 0 | **0**, worst overshoot **0.932** (predicted 0.000) |
| (g') FLOOR, old rule | 19 outside, worst 241.3 | **19, worst 241.302** |
| (h) skipped | 38 on, 0 off | **38 on, census all zeros off** |
| (i) armature / marker-only | 111 / 19 | **111 / 19** |
| (d) drawn joint vs animated node | within 1e-3 | **worst 0 units** |
| (d') FLOOR | must disagree with the bind pose | **122 joints moved, largest 367.6** |
| (a) (b) | the dock's 130 / 93 / 93 / 0 | **130 / 93 / 93 / 0, muted 37 = 130 - 93** |

**THE PICTURE THE DIRECTOR IS OWED IS DELIVERED.**
`scratchpad/skeloverlay_20260910/on_frame46.png`, **1500x1000**, the human
fixture at frame 46 (t=0.766667, 60 fps) of
`fixtures/Running_To_Slide_And_Back_To_Running.hkx` with the overlay on. The
long segments are gone: the skeleton is drawn entirely on the body -- spine,
both arms out to individual finger bones, both legs, the skull -- in the
crouched mid-slide pose, and nothing runs off to the world origin. Beside it,
unchanged and NOT deleted because it is gate (j')'s only floor:
`scratchpad/skelfix_20260910/before_on_frame46.png`, the BUILD9 picture in
which the defect was seen.

**Two things the pictures cost, both measured:**

1. **Gate (j) is RED, and every stray pixel is a joint marker.** 53 of them,
   all inside two round grey marks about 7 px across at (486,485) and
   (463,503), colour (134,139,145) -- the muted "not a bone" colour, not the
   bone white. They are the joint markers of two of the 19 marker-only nodes,
   which this lane's shipped rule KEEPS on purpose ("The other 19 node(s) --
   camera, anim-object, weapon, attach and root nodes ... get a joint marker
   and no bone"). Gate (j) as pre-registered asks that every pixel the overlay
   drew be on the character. The rule and the gate disagree and only bungo can
   say which gives: leave the markers and relax (j) to segments, or stop
   drawing a marker for a node that projects off the character. The gate's own
   floor is honest -- (j') confirms the BUILD9 picture still fails (j).
2. **Gates (c) and (e) are not stable, and this is not a regression claim.**
   Four identical runs of the spell on this one exe:

   | run | (c) pixels outside the mask | (e) pixels differing after toggling off | in-app total |
   |---|---|---|---|
   | 1 | 10 | 17 | 27 checks, 2 failures |
   | 2 | **0** | **0** | 27 checks, **0 failures, PASS** |
   | 3 | 4 | 37 | 27 checks, 2 failures |
   | 4 | **0** | 2 | 27 checks, 1 failure |

   Both checks demand EXACT equality on a `grabFramebuffer()` result that is
   not bit-stable between repaints, and the counts are 0-37 pixels out of
   ~800,000. What is NOT settled: whether part of that is a real one-frame lag
   in the toggle rather than GL jitter. The discriminator is a run that
   settles the frame before each grab, or a tolerance stated in the check.

**The WW_RENDER_SIZE law, measured on this exe** (this lane logged its pictures
coming out 1437x941 from a request of 1000x1000):

| requested | measured |
|---|---|
| 1000x1000 | **1293x941** |
| 1500x1059 | **1500x1000** |
| 1800x800 | **1800x741** |

Height is exactly `requested - 59` in all three; width is
`max( requested, 1293 )`. The hook does `skope->resize( rw, rh )`
(`src/nifskope_ui.cpp` ~21636) and a QMainWindow will not go below its own
minimumSizeHint, so a width under that floor is silently raised to it. **The
floor is build-dependent** -- it was 1437 on the 15:52:46 exe this lane
measured and is 1293 on the 17:08:39 one -- so it is measured per build, never
remembered. The pictures above were re-rendered at 1500x1059.

### The one table of clocks

| artefact | time |
|---|---|
| `release/NifSkope.exe` (this build) | 2026-09-10 **17:08:39**, 20,693,504 B |
| the exe this replaced (lane BUILD10) | 16:45:53, 20,007,936 B |
| `release/style.qss` (copied at link time) | 17:08:39, equal to `res/style.qss` |
| `release/hkclasses_fo4.json` | 17:08:39, 1,511,662 B |
| `release/hkx_annotation_vocabulary.txt` | 17:08:39, 55,299 B |
| bungo's own window, opened mid-lane | started 17:05:44, pid 700, no `--port` |

qmake ran before make; `DEPCHECK missing=0` over eleven objects; the three WW
defines are each once in `Makefile.Release` and on every compile line; the
exe-newer sweep is 1 stale of 114 paths and the exception is
`src/watermark.cpp`, lane WATER7's live edit in this shared tree, which is NOT
in this exe. `WW_HKXANIM_UI`, `WW_HKXCLIP_CANON` and `WW_ANIMWS_HKXMODEL` are
new or newly-read flags, so the objects of every translation unit that reads
one were deleted before make (the BUILD9 DEFINES trap: make compares mtimes,
not flags).

**Skipped harnesses, named with the reason** (`nifskope-ww-resume-pending` s5):
`loaded_nifs.sh`, `top_bar.sh` and `ui_align.sh` -- this build renamed no
user-visible string and moved no bar, so the sibling-label class of red
(BUILD9 s10) cannot have been introduced; `lodgen_*`, `water_*`, `lodl/lodt`
and the impostor gates -- nothing in these three lanes reaches the generator,
the water tool or the terrain readers. `WW_POSEDRAW_TEST` was left alone: it
was already failing at "clicking a bone did not make it the active object" on
both fixtures BEFORE lane SKELFIX, and SKELOVERLAY's report says one run on the
70-bone facial rig is what settles it.
