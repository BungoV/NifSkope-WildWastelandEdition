# Lane SKELOVERLAY — Overlays > Show Skeleton

bungo, 2026-09-10 ~15:0x, verbatim: *"Add to the overlays: View skeleton, shows
you the bones, basically the same view as in the skeleton manager"*.

Report written incrementally. Sections: 1 what was found before writing a line,
2 the design and its divergences from Blender, 3 the files, 4 the hook-up,
5 the harness and its gates, 6 the pictures, 7 build state, 8 mistakes,
9 the finished-work skill review.

## 1. What already existed (read before writing anything)

The tree already draws an armature, and it is NOT reachable from the Overlays
menu:

* `GLView::drawPoseSkeleton()` (`src/glview.cpp:2039`) draws octahedral bones,
  joint dots and dashed parent-relationship lines with the depth test off. It
  runs for Pose Mode and for the **Skeleton Manager dock** (`skeletonView`,
  set by the dock's visibility through `GLView::setSkeletonView`).
* Its bone list is `poseBones`, built by `GLView::refreshPoseBones()`
  (`src/glview.cpp:1475`) from the skinned shapes' `Bones` arrays, with a
  separate "every non-geometry NiAVObject" arm for skeleton view.
* The Skeleton Manager dock (`src/skeletontools.cpp:318`) builds its tree from
  `skeletonAnalyse()` (`src/skeletontools.h`), a MODEL-LAYER analysis shared
  with the CLI. Its three classes are `verts > 0` (deforming), `inSkin &&
  verts == 0` (unused) and `!inSkin` (not a bone), and its footer prints
  `N node(s) shown · M bone(s), D deforming, U unused · S skinned shape(s)`
  where `M = D + U`.
* So `poseBones` and the dock's tree are built by TWO DIFFERENT rules and can
  disagree. Anything that must "agree with the Skeleton Manager" has to be
  built from `skeletonAnalyse()`, not from `poseBones`.
* The Overlays dropdown is `src/nifskope_ui.cpp:25457`; the locally-created
  entries (`Show Origins`, `Billboards Face Camera`, ...) persist through a
  `persist( action, "GLView/Display/<Key>" )` helper at the end of that block.
* A loaded .hkx clip poses the rig in `Node::transform()`
  (`src/gl/glnode.cpp:516`), before any world transform is read — so anything
  that reads `Node::worldTrans()` per frame follows the animation for free.
* `WW_SKELETON_TEST` (`src/nifskope_ui.cpp:3059`) already proves the dock's
  tree against `skeletonAnalyse()`, and records the LINE-PATH WARM-UP trap:
  streaming line geometry draws nothing until a pick render has run, so a grab
  without `indexAt()` first shows joint dots and no bones.

## 2. The design, and the divergences from Blender

**One tick, in the Overlays dropdown, beside Show Nodes and Do Skinning.**
Checkable, default OFF, persisted as `GLView/Display/ShowSkeleton` through the
same `persist()` helper the other locally-created Overlays entries use.

**Its bones come from `skeletonAnalyse()`** -- the Skeleton Manager dock's own
analysis, and the same call the `skeleton` CLI prints -- not from `poseBones`.
That is the whole of "basically the same view as in the skeleton manager": the
list, the classes and the counts are one number with three faces (the dock's
tree, the dock's filter buttons, this overlay), and there is no second rule that
could drift.

**Its three colours are the dock's three.** `text` for a deforming bone,
`accent` (the palette's orange, the dock's attention colour) for a bone a skin
lists that no vertex uses, `textMuted` for a node no skin references -- read
from `wwSkinColor()`, the same call the dock makes, so a palette change moves
both.

**Depth test off** (`glDisable(GL_DEPTH_TEST)`, depth mask off): the armature
reads through the mesh. **Fixed pixel width** 1.6 px * device pixel ratio for
the bones and a 5 px joint marker, neither scaled by depth nor by zoom.

**It follows the pose because it reads `Node::worldTrans()` every frame.** A
loaded .hkx clip writes `Node::local` in `Node::transform()` before anything
reads a world transform, so no new plumbing was needed for the animated case --
which is also why gate (d) is a readback and not a claim.

DIVERGENCES from Blender, stated (CONSTITUTION rule 10):

| Blender | here | why |
|---|---|---|
| Armature "In Front" + five more checkboxes (Names, Axes, Shapes, Group Colors, Relationship Lines) | ONE tick | bungo asked for one entry; the END-menu rule is rows only when a setting is really added |
| a separate **Names** checkbox | names ride on the Overlays menu's existing **Show Nodes** | it is already the toggle that labels what the viewport draws over the model, and these are the same node names |
| bones coloured by **bone group** (an authored property) | coloured by the Skeleton Manager's three classes | a NIF has no bone groups; the classes are the only classification the file actually carries |
| a bone is head -> tail as authored | one octahedral body per **parent -> child** pair, plus a capped stub for a bone with no drawn child | a NIF bone has no tail; this is `poseBoneTail()`'s existing law, and one segment per pair is what the ruling asks for |
| the armature is selectable in the viewport | read-only | Pose Mode and the Skeleton Manager already own picking; an overlay that stole clicks would fight them |

NOT `skeletonView` reached a second way, deliberately: that flag belongs to the
Skeleton Manager dock and is driven by the dock's VISIBILITY, so an Overlays
tick writing it would be switched back off the next time the dock was shown or
hidden.

## 3. The files

| file | what | line endings |
|---|---|---|
| `src/glview.h` | the API, the census struct, the private state | LF, unchanged (CR 0) |
| `src/glview.cpp` | `setSkeletonOverlay` / `refreshSkeletonOverlay` / `drawSkeletonOverlay` / `paintSkeletonOverlayNames`, plus `characteristicBoneSize()` and `boneTailIn()` factored out of the pose armature so both use one law | CRLF; +284 lines, dCR +284 = dLF +284 |
| `src/gl/glscene.h/.cpp` | `Scene::findNode()`, a look-up that does NOT create a Node | LF, unchanged |
| `src/skeloverlaytest.cpp` | NEW: the whole harness, so this lane's footprint in `nifskope_ui.cpp` is one line | LF |
| `tests/spells/skeleton_overlay.sh` | NEW: the gates and the three pictures | LF |

`Scene::findNode()` exists because `Scene::getNode()` CONSTRUCTS a Node for any
block handed to it. The overlay is offered every `NiAVObject` block in the file,
so on a file with a block the scene graph does not reach, `getNode` would have
grown `nodes`, moved `Scene::bounds()`, and changed the picture the overlay is
only supposed to draw on top of -- while the code called itself read-only.

## 4. The hook-up, unapplied

`scratchpad/skeloverlay_20260910/hookup.py`, a refusing script
(`ww-anchored-hookup`). Five edits, **checked, not applied** -- lanes FILESTAB
and HKX3 are writing into `nifskope_ui.cpp` and lane BUILD8 owns `NifSkope.pro`:

```
P1  NifSkope.pro          anchor x1   src/skeloverlaytest.cpp joins the build
U1  src/nifskope_ui.cpp   anchor x1   the "Show Skeleton" Overlays entry
U2  src/nifskope_ui.cpp   anchor x1   persist( ..., "GLView/Display/ShowSkeleton" )
U3  src/nifskope_ui.cpp   anchor x1   WW_SKELETON_OVERLAY=1 for a headless render
U4  src/nifskope_ui.cpp   anchor x1   the harness's one line
NifSkope.pro would grow by 27 bytes; src/nifskope_ui.cpp by 1901 bytes
```

Every inserted block carries the string `lane SKELOVERLAY`, so a resume decides
"applied or not" by grepping for that and not from the anchor still matching --
which it does either way.

Nothing this lane wrote NEEDS the hook-up to compile: `src/skeloverlaytest.cpp`
and `src/glview.cpp` call only public API that exists today, and all three
changed translation units pass `-fsyntax-only` now (section 7).

## 5. The gates, pre-registered

`tests/spells/skeleton_overlay.sh` on `fixtures/human_male_vanilla.nif`, with
`fixtures/Running_To_Slide_And_Back_To_Running.hkx` (93 frames at 60 fps) at
frame 46 -- the frame bungo already has as `mixamo_fhalf` in
`scratchpad/build7_20260910/frames_mixamo.png`, so the armature can be held
against a pose he has already looked at.

| gate | what it holds | against what |
|---|---|---|
| (a) | joint markers drawn + blocks with no scene node = the **dock's All-filter row count**; and the missing count is 0 | the Skeleton Manager dock's own tree, NOT `skeletonAnalyse()` -- the overlay is built from that call, so comparing to it would be our own output judging our own output |
| (a') | FLOOR: with the overlay OFF the census is all zeros | a census that is never written cannot pass by accident |
| (b) | bone-coloured = the **Bones** filter, deforming-coloured = **Deforming**, unused-coloured = **Unused**, muted = All - Bones | the same dock, its four filter buttons clicked |
| (c) | every pixel that changes when the overlay goes on lies inside a mask rasterised from the segments the overlay REPORTS having drawn plus its joints | a readback (`skeletonOverlaySegments()`), not a re-derivation of where the lines ought to be |
| (c') | FLOOR x2: some pixels must differ at all, and the mask must cover < 80% of the frame | a mask that covered everything would pass (c) for free |
| (d) | at frame 46 every drawn joint is within 1e-3 units of the **animated** `Node::worldTrans()` | read off the scene graph at that moment |
| (d') | FLOOR: those same drawn positions against the BIND pose must disagree on at least one bone by > 1e-3 | otherwise "it follows the animation" is vacuous |
| (e) | toggling the overlay off restores the off-render BYTE for byte | `wwDiffPixels(imgOff, imgOff2) == 0` |

Written but not yet RUN: the exe on disk does not contain this code. Every row
above is a pre-registration, not a result (section 7).

The harness also writes its own evidence into
`scratchpad/skeloverlay_20260910/gates/`: `gate_off.png`, `gate_on.png`,
`gate_on_frame46.png` and `gate_mask.png` -- the last one paints the mask dark
grey and every differing pixel orange, so "only the overlay's pixels" can be
LOOKED at rather than believed.

**The line-path warm-up is in the harness and is not optional.** Streaming line
geometry draws nothing until a pick render has run (the open 07-17 defect), so
without `indexAt()` first the bones would be missing from every grab and only
the joint dots would survive -- which would quietly turn gate (c) into a test of
three hundred points. `WW_SKELETON_TEST` carries the same note for the same
reason.

## 6. The pictures

`tests/spells/skeleton_overlay.sh` takes them **through the render hook**
(`WW_RENDER_SHOT`), not from the harness's own grab, so they carry the camera
census: one pinned orthographic camera, `WW_RENDER_VIEW=5` (ViewFront),
`WW_RENDER_CENTER=0,0,62`, `WW_RENDER_ORTHO=80`, `WW_RENDER_SIZE=1000x1000`,
`WW_RENDER_CLEAN=1` -- the same pin lane BUILD7 used for the frame sheets.

* `scratchpad/skeloverlay_20260910/off.png` -- overlay off, bind pose
* `scratchpad/skeloverlay_20260910/on.png` -- overlay on, bind pose
* `scratchpad/skeloverlay_20260910/on_frame46.png` -- overlay on, the clip at
  frame 46

`WW_SKELETON_OVERLAY=1` is what arms the overlay for a headless capture (edit U3
of the hook-up). It is a viewport toggle rather than a `Scene::option`, so
`WW_RENDER_CLEAN` cannot reach it and a picture of the armature would otherwise
depend on whatever the user last ticked -- which is not reproducible.

Three identical md5s at the end of the script would mean the overlay never
reached the picture; the script fails on fewer than three distinct images.

**NOT TAKEN YET** -- see section 7.

## 7. Build state: BUILD PENDING

The brief allowed ONE check of the build handshake and it came back not-clear:

| checked once, 2026-09-10 ~15:1x | result |
|---|---|
| `scratchpad/build8_20260910/DONE` | **absent** |
| `scratchpad/filestab_20260910/BUILDING`, `scratchpad/hkx3_20260910/BUILDING` | absent |
| any `BUILDING` marker under `scratchpad/` | none |
| `tasklist \| grep -i -E "Fallout4\|NifSkope"` | no match, `rc=1` |
| `release/NifSkope.exe` | 2026-09-10 14:37:53 (BUILD8's) |
| `scratchpad/build8_20260910/` newest file | 14:52:47 -- BUILD8 is still writing |

The slot was free but BUILD8 had not signalled DONE, so **nothing was built, no
`BUILDING` marker was created, and the exe was not touched.** The lane ends
BUILD PENDING; the resume is
`scratchpad/skeloverlay_20260910/PENDING.md` (six steps, in order: hook-up,
qmake-before-make, the two-header object sweep, the gates and pictures, the two
neighbouring harnesses the factoring reaches, the four documents).

**What IS proven right now:** all three changed translation units compile.
`-fsyntax-only` with the real `Makefile.Release` flags:

```
== src/gl/glscene.cpp        RC=0
== src/skeloverlaytest.cpp   RC=0
== src/glview.cpp            RC=0
```

`src/glview.cpp` emits five warnings, all of them pre-existing and none in this
lane's code (`QImage::mirror` deprecation at 5127, three unused locals in
`gizmoEnd`, a dangling reference in `bevelSelection`, an unused `skipped` in a
lambda). It caught one real error on the first pass, which is the whole reason
the pass exists: `NifSkope::ogl` is PRIVATE, and the harness had copied
`skope->ogl` from `WW_SKELETON_TEST`, which is inside the class. It uses
`skope->getGLView()`.

`-fsyntax-only` proves the files compile and **nothing about the link, about moc
for a new `Q_OBJECT`, or about behaviour.**

Line endings, measured with Python byte counts, never grep:

| file | before | after |
|---|---|---|
| `src/glview.cpp` | CR 23,000 / LF 23,071 | CR 23,284 / LF 23,355 -- dCR +284 = dLF +284, its CRLF run intact |
| `src/glview.h` | CR 0 | CR 0 |
| `src/gl/glscene.h`, `src/gl/glscene.cpp` | CR 0 | CR 0 |
| `src/skeloverlaytest.cpp` (new) | -- | CR 0 |
| `tests/spells/skeleton_overlay.sh` (new) | -- | CR 0 |

Nothing is committed (CONSTITUTION rule 8; his "Not yet" for this session's work
stands until he says otherwise).

## 8. Mistakes

1. **`NifSkope::ogl` copied as if it were public.** The harness was written by
   following `WW_SKELETON_TEST`, which reads `skope->ogl` -- legally, because it
   is code INSIDE `NifSkope`. Lifting the same line into a separate translation
   unit does not lift the access. Found by the `-fsyntax-only` pass, which is
   exactly the failure class that pass is for. Rule that prevents it: when
   copying a pattern out of a class's own file into a new one, the first thing
   checked is whether every member it touches is public.
2. **`Scene::getNode()` used at first for a read-only overlay.** It CONSTRUCTS a
   Node for any block handed to it, and this overlay is handed every
   `NiAVObject` block in the file. On a file with a block the scene graph does
   not reach, the "read-only" overlay would have grown `nodes` and moved
   `Scene::bounds()`. Caught by reading `getNode`'s body before trusting its
   name; fixed by adding `Scene::findNode()`. Rule: an accessor named `get` is
   not evidence that it only gets.
3. **`sx_tmp.sh` at the repo root was already there.** The build-verify skill
   names that exact filename for the throwaway syntax-check script, and another
   live lane was using it. Writing to it would have destroyed another lane's
   working file mid-run. Used `sx_skeloverlay.sh` instead and deleted it after.
   **This is a defect in the `nifskope-ww-build-verify` skill**, not a one-off:
   the skill hands every lane the same filename in a tree where lanes run
   concurrently. Amendment written in section 9.

## 9. Finished-work skill review (CONSTITUTION rule 1a)

**Loaded and used:** `nif` (the fork's CLI and corpora -- of limited use here,
this was viewer work), `nifskope-ww-render-shot` (the headless rules, the
absolute-path trap, the camera pin, the `env` prefix trap that costs whole runs
at rc 127 -- all three are in `tests/spells/skeleton_overlay.sh`),
`ww-anchored-hookup` (the refusing script, the marker string rather than the
anchor as the applied/not test), `nifskope-ww-build-verify` (the syntax pass and
its exact flags, which caught the private-member error),
`nifskope-ww-resume-pending` (the shape of `PENDING.md`, the qmake-before-make
rule and the two-header object sweep).

**Not loaded, with the reason:** `nifskope-ww-panel-style` -- no QWidget panel
or dock was written; the lane's whole UI is one `QAction` in an existing menu,
and the brief itself said "rows only if a setting is added".

**The skill that should have existed, and now does not yet:** an amendment, not
a new skill. `nifskope-ww-build-verify`'s "When you CANNOT build" section tells
every lane to write its throwaway syntax-check script to **`sx_tmp.sh` at the
repo root**. In a tree where three or four lanes run concurrently -- which is
the normal state of this repo -- that is a shared mutable filename, and this
lane found the file already present and owned by someone else. One line of the
skill needs to change:

> Write them once into a throwaway script IN THE REPO named for YOUR LANE
> (`sx_<lane>.sh`, never `sx_tmp.sh` -- other lanes are in this tree and that
> name collides), and delete it afterwards.

Recommended for the live tree `E:\Projects\Claude\.claude\skills\nifskope-ww-build-verify\SKILL.md`
and for `<repo>\.claude\skills\` if that copy carries the same sentence. Not
applied by this lane: amending a skill another live lane may be reading in the
same minute is the director's call.

**A second, smaller one, declined:** the "overlay entry in the Overlays menu"
procedure (the `ds` list, the `wwStyleCheckedRows` ordering, the greyscale-icon
loop, the `persist()` block) is now written down twice -- here and in
`aOrigins`' own comments. It is not yet a procedure with traps that cost a
build, and one more instance of it would still be cheaper than a skill. Named
here so the next lane that adds an Overlays entry can decide differently.


## Build (BUILD9)

Built and gated 2026-09-10 by lane BUILD9, after FILESTAB and HKX3.

`hookup.py --check`: 5 of 5 anchors match once, no text already present.
`--apply`: `NifSkope.pro` +27 bytes and `src/nifskope_ui.cpp` +1,901, exactly as
predicted, CR 0 -> 0 on both, `grep -c "lane SKELOVERLAY"` = 4 as predicted.
qmake RC=0, make RC=0, `src/skeloverlaytest.cpp` in `Makefile.Release` at three
places. Two-header object sweep over `src/glview.h` and `src/gl/glscene.h`: no
stale object. Exe newer than every changed path; sheet in step.

### The gate table (final exe 15:52:46)

| gate | measured | verdict |
|---|---|---|
| (a) | dock All 130 / Bones 93 / Deforming 93 / Unused 0; overlay census nodes 130, segments 129, stubs 78, missing 0, draws 3 | pass |
| (a') | overlay OFF census all zeros | floor pass |
| (b) | bone-coloured 93 = Bones 93; deforming 93 = Deforming 93; unused 0 = Unused 0; muted 37 = All 130 - Bones 93 | pass |
| (c) | 14,390 pixels changed (2.412%); **0 outside the mask** the overlay reports having drawn | pass |
| (c') | some pixels differ, and the mask covers 12.23% of the frame, not all of it | floor pass x2 |
| (d) | at frame 46, worst joint-to-animated-node distance **0 units** | pass |
| (d') | the same joints against the BIND pose disagree on 122, largest 367.6 units | floor pass |
| (e) | toggling off restores the off-render byte for byte: 0 pixels differ | pass |

**17 checks, 0 failures, PASS.**

### The pictures

Through the render hook, one pinned orthographic camera (`WW_RENDER_VIEW=5`,
centre 0,0,62, ortho 80, 1000x1000, clean), three distinct md5s:
`scratchpad/skeloverlay_20260910/off.png` (bind pose, overlay off),
`on.png` (bind pose, overlay on) and `on_frame46.png` (the clip at frame 46).
The gate's own evidence is under `gates/`, including `gate_mask.png` with the
mask painted dark grey and every changed pixel orange -- every orange pixel is
inside the grey. All three were regenerated on the FINAL exe, after the
alignment change, so what was delivered is what ships.

### The pose-armature factoring: NOT settled, and here is exactly what is known

`WW_SKELETON_TEST` PASSES (129 bones drawn on the vanilla `skeleton.nif`, footer
and tree agree). `WW_POSEDRAW_TEST` FAILS at "clicking a bone did not make it
the active object", and it fails on BOTH fixtures available here --
`fixtures/human_male_vanilla.nif` (130 bones drawn) and the vanilla
`skeleton.nif` (129) -- with `poseBoneAt` at a bone's own drawn screen position
resolving block 0 in each, so the probe the click is compared against is 0 while
the click legitimately selects a real block (152 and 155).

The factoring is arithmetically identical by diff:
`characteristicBoneSize()` is `refreshPoseBoneSize()`'s body with the bone list
parameterised and -1 standing in for the early return (the caller keeps its
previous size, which is what the early return did), and `boneTailIn()` is
`poseBoneTail()`'s body with the list and the cap parameterised. So it is not a
candidate for this failure. WW_CHANGES.md records this harness last green on a
FACIAL rig of 70 bones with an L/R pair -- neither fixture here is that rig, and
both skip the mirror check for want of a pair. **Reported, not cured**
(`nifskope-ww-resume-pending` rule 6: the resuming lane's product is a verdict).
What would settle it: the facial rig the 2026-09 entry used, run once.

### Not measured

Whether the armature is legible on a dense facial rig. No instrument measures
that; the picture is the only one and bungo is the judge.

### The clocks, in one table

| artefact | time |
|---|---|
| `release/NifSkope.exe` (final) | 2026-09-10 15:52:46 |
| `release/style.qss` | 15:52:46, `cmp` equal to `res/style.qss` |
| FILESTAB hook-up applied | 15:17 |
| FILESTAB first gated exe | 15:20:34, re-gated on 15:25:05 and 15:52:46 |
| HKX3 hook-up applied | 15:29 |
| HKX3 gated exe | 15:32:06, re-gated on 15:35:05 and 15:52:46 |
| SKELOVERLAY hook-up applied | 15:40 |
| SKELOVERLAY gated exe | 15:36:50, re-gated on 15:52:46 |
| alignment before-measurement | 15:43:14 exe |
| alignment after-measurement | 15:52:46 exe |

Exe-newer sweep over every path `git status --porcelain -- src res tools tests`
reports: 0 stale at each gate run.
