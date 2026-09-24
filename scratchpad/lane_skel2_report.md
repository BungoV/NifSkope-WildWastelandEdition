# Lane SKEL2 -- one skeleton renderer, Blender's octahedral bone in blue, and the overlay mirroring the Skeleton Manager

Written incrementally. Section 0 is the pre-registration: every number below it
that the lane later quotes as a RESULT was predicted or measured on the rung
BEFORE a line of this lane's code was written.

* Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, working tree.
* Rung `release/NifSkope.before_skel2.exe` -- taken once at 2026-09-11, a byte
  copy of `release/NifSkope.exe` **2026-09-11 07:06:04, 20,867,584 B** (UI6).
* No other lane was alive at launch: `ls scratchpad/*/BUILDING` printed nothing
  (`rc=2`, no match). `tasklist | grep -i -E "Fallout4|NifSkope"` -> `rc=1`, so
  the game was down and bungo had no window open.

---

## 0. Pre-registered gates

### 0.1 Baselines measured ON THE RUNG, before any code

`scratchpad/skel2_20260910/baseline.sh`, console
`scratchpad/skel2_20260910/logs/baseline_console.txt`, per-run logs
`scratchpad/skel2_20260910/logs/base_*.log`. One instance at a time, second
monitor, a fresh port per run.

| harness | on the rung (07:06:04) | note |
|---|---|---|
| `WW_SKELOVERLAY_TEST` (in-app half of `skeleton_overlay.sh`) | **27 checks, 2 failures**, on all five runs | the two failures are gates (c) and (e) themselves |
| `WW_POSEDRAW_TEST` | **RED**, one failure: *clicking a bone did not make it the active object* | already red before this lane; the probe bone is block 0 and the click selects block 5 |
| `WW_POSEEXTRAS_TEST` | **RED**, one failure: *weight overlay found no influenced vertices* | already red before this lane; it inspects bone 0, the root, which drives no vertex |
| `WW_SKELETON_TEST` | **PASS** -- All 130, Bones 93, Deforming 93, Unused 0, 9 skinned shapes | the Skeleton Manager's own numbers on the fixture |

Both pose harnesses are RED ON THE RUNG. Neither red is this lane's, and
neither is claimed as this lane's.

### 0.2 The (c)/(e) instability, measured (brief item 6)

Five identical runs of the overlay gate on the rung, same exe, same fixture,
same frame. Both checks demand EXACT framebuffer equality.

| run | (c) pixels outside the overlay's own mask | (e) pixels differing after toggling the overlay back off |
|---|---|---|
| 1 | 16 | 12 |
| 2 | 13 | 22 |
| 3 | 14 | 19 |
| 4 | 9 | 15 |
| 5 | 1 | 21 |
| **max** | **16** | **22** |

BUILD11 measured the same two checks at 10 / 0 / 4 / 0 and 17 / 0 / 37 / 2 over
four runs of the 17:08:39 exe. Over the nine runs now on record the largest
value either check has ever shown is **37**.

**Pre-registered tolerance: 64 pixels**, for both checks, out of a
1293x941 = 1,216,713-pixel frame (0.0053%). It clears the measured worst case
by 27 px and is a fixed number written into the check's own sentence, not a
percentage that moves with the frame size. Floors registered with it: the
tolerance must still refuse a real difference -- the overlay's own ON frame
differs from the OFF frame by tens of thousands of pixels, and that comparison
is asserted to FAIL the same 64-px bar in the same run.

### 0.3 The gates this lane registers, with the numbers predicted before the code

| gate | what it asserts | predicted / floor |
|---|---|---|
| S1 | `drawPoseSkeleton()` and `drawSkeletonOverlay()` both delegate to ONE routine, `GLView::drawArmature()`; neither draws a bone itself | `grep -c` of the primitive calls inside the two functions = 0; `drawArmature(` called from both |
| S1b | the two views agree on colour BY CONSTRUCTION: one palette function answers for both | `skeletonKindColor()` called from `glview.cpp` and `skeletontools.cpp`, from nowhere else |
| S1c | the Pose harnesses stay at their rung baselines | `WW_POSEDRAW_TEST` red with the SAME one failure; `WW_POSEEXTRAS_TEST` red with the SAME one failure |
| S2 | `Wire` display == the rung's framebuffer for the same bones and camera | <= 64 px different (0.2); floor: `Octahedral` differs by > 2000 px |
| S3 | under each chip the overlay's drawn node set == the Skeleton Manager's listed set | All 130, Bones 93, Deforming 93, Unused 0; floor: a deliberately wrong set is shown red |
| S4 | selection is two-way | a synthetic click at a bone's screen position selects that block; selecting the block makes the row current; double-click moves the camera |
| S5 | rows at depth 6+ carry a non-empty, un-elided name at the default dock width | 0 empty names; floor: the same rows measured under the rung's own column arithmetic are empty |
| S6 | the two grey dots are NAMED, and the Bones chip puts no pixel off the character | 2 names printed; 0 stray pixels under the Bones chip |
| S7 | the (c)/(e) tolerance is measured from five runs and printed | the table in 0.2 |
| S8 | the exe is newer than every changed file, and every object of a changed header is newer than the header | 0 stale |
| S9 | nothing of this lane left running; bungo's window untouched | `rc=1` |

### 0.4 Two gates the brief asked for that were REFUSED, before any code

**S2 as written cannot hold, and the reason is bungo's own later ruling.** The
brief asks `Stick` to reproduce the 07:06:04 framebuffer byte for byte. He also
ruled that the bones are to be blue. The overlay that shipped draws them in
`text`, `accent` and `textMuted`; every overlay pixel therefore changes colour,
whatever the shape does, and an exact framebuffer match is impossible by
construction. Two further facts the brief did not have: the shipped overlay did
not draw plain segments at all, it drew a 12-line WIREFRAME OCTAHEDRON, so
`Stick` (Blender's plain line) is a different shape from what shipped; and
Blender's own name for the shipped shape is `Wire`.

So: three display modes, not two -- **Octahedral** (new, solid), **Stick**
(Blender's line) and **Wire** (the shipped shape, at the shipped collar
position) -- and S2 is re-registered, before the build, as the thing his colour
ruling did NOT license to move:

> **S2** In `Wire`, the SET OF PIXELS the overlay covers is the set the shipped
> overlay covered, within the same 64-px bar. Coverage is measured as
> "pixels where ON differs from OFF at the same pinned camera", on the rung and
> on the new build, and the two sets are compared.
> FLOORS: both coverages non-empty; the two ON renders must DIFFER (they are
> different colours, so a zero there would mean nothing was rebuilt);
> and `Octahedral` measured the same way must FAIL, or the comparison is not
> measuring shape at all.

The rung's four renders for it were taken BEFORE the build
(`scratchpad/skel2_20260910/rung/`, `rung_shots.sh`), because the link
overwrites the exe.

**The brief's gate (j) under All is dropped, and the reason is measured.** Under
the All chip the marker-only nodes are drawn ON PURPOSE (lane SKELFIX's shipped
rule: a node no skin sits beneath still gets its joint marker), and two of them
project above the character's back at frame 46. On the rung that is **56 stray
pixels in two clusters, at (573,488) and (544,507) in a 1524x941 render** -- the
same two dots BUILD11 measured as 53 px at (486,485) and (463,503) in its own
frame size. A gate that asks for zero there is asking the rule to be broken. It
is re-registered under the **Bones** chip, where the marker-only nodes are not
listed at all, and the two dots are NAMED instead.

Note on the render size: `WW_RENDER_SIZE`'s width floor is build-dependent and
it has MOVED again. It was 1437 on 2026-09-10 15:52:46, 1293 on 17:08:39, and on
this rung a request of 1000x1000 comes back **1524x941**. It is measured per
build, never remembered -- which is why the dots are named from the overlay's
own per-frame dump rather than from a remembered coordinate.

---

## 1. The two renderers today (the state this lane found)

| | `GLView::drawPoseSkeleton()` (glview.cpp ~2060) | `GLView::drawSkeletonOverlay()` (glview.cpp ~2375) |
|---|---|---|
| who calls it | Pose Mode AND the Skeleton Manager dock (`skeletonView`) | the Overlays menu's Show Skeleton tick |
| the bone list | `poseBones`, built in `refreshPoseBones()` from the skinned shapes' `Bones` arrays plus their ancestors | `skelOverlayBones`, built in `refreshSkeletonOverlay()` from `skeletonAnalyse()` -- the Skeleton Manager's own analysis |
| the shape | `drawOctahedralBone()` (a 12-line wireframe) in the dock's view, a plain `scene->drawLine()` in Pose Mode | `drawOctahedralBone()` always |
| the colour | hard-coded floats: `#FF9D00` active, `1.0/0.85/0.4` hover, pale grey pinned, `0.55/0.72/1.0` otherwise, all multiplied by a depth ramp | the palette, by the dock's class: `text` deforming, `accent` unused, `textMuted` not-a-bone; no ramp |
| widths | 1.8 px, 2.8 px when active; point 9 / 7 / 4..6 | a fixed 1.6 px; point 5 |
| extras | relationship lines, pins, hover names, the weight-influence overlay, mirror, Outfit Studio / SAM pose I-O | a joint marker for every listed node, and the SKELFIX armature rule |

**What Pose Mode needs kept**, and is kept: relationship lines, the depth ramp,
pins, the hover highlight, the weight overlay, the mirror and the pose I-O. All
of those are per-view state and stayed in `drawPoseSkeleton()`; what moved out
is the drawing.

Two facts from this table decide what follows:

1. **the overlay did not draw "segments" before this lane** -- it drew the
   12-line WIREFRAME octahedron. The brief's `Stick`, described as "the exact
   way back to today's segment drawing", is a different shape from what shipped,
   so a third mode had to exist for the way back to be exact. Blender has the
   name for it: **Wire**;
2. **no blue exists in the overlay today.** Its three colours are `text`,
   `accent` and `textMuted`; the only blue anywhere near this code is Pose
   Mode's hard-coded `0.55, 0.72, 1.0`, which is not from the palette at all.
   "the skin palette's BLUE the overlay uses today" does not exist, so the blue
   was taken from the palette by its own description: `toggle`, **#4772b3**,
   which `skinVars[]` documents as *"Blender's wcol_option inner-selected
   blue"* and which is the same value in the light and the dark column.

## 2. The shared renderer and the octahedron

### 2.1 One routine

`GLView::drawArmature( const QVector<ArmatureBone> &, const ArmatureStyle & )`
is now the only code in the tree that draws a bone. Each caller fills a list of
`ArmatureBone` -- block, head, tail, kind, and the per-bone state (selected /
active / hovered / pinned / marker-only / stub / depth fade) -- and a small
`ArmatureStyle` -- display mode, X-ray, line width, point size, whether the
depth ramp applies. Nothing below the renderer knows which view asked.

The passes, in the order they run: the solid facets (ONE draw call for the whole
armature), then every BODY edge, then every STUB edge, then one joint ball per
node, then the tail balls. The body-before-stub order is not cosmetic -- the
armature is blended with the depth test off, so the order decides which line
wins a crossing pixel, and `Wire` has to reproduce the picture that shipped.

### 2.2 The octahedron

Built to bungo's reference picture (Blender 4.5.3, a default bone in Object
Mode): a square ring one tenth of the length from the head, tapering to a point
at the tail, a ball at the head and a smaller ball at the tail, solid-shaded so
the four facets read.

* eight facets: four from the head ball to the ring, four from the ring to the
  tail point;
* **the ring sits at 0.10 of the length**, Blender's own value and what his
  picture shows. The shipped wireframe put it at 0.15; `Wire` keeps 0.15,
  because `Wire` is the way back;
* **back faces are dropped on the CPU**, by the sign of the facet normal in
  camera space. The armature draws with the depth test off, so the GPU cannot
  resolve which facet is in front and the far side of a bone would blend over
  the near side. Four facets survive, which is what Blender shows;
* the normal is pointed outward by the octahedron's OWN AXIS, never by the
  winding of the triangle. A winding taken backwards here does not look wrong,
  it silently drops every facet and the bone draws as a wireframe;
* the light is **fixed in camera space** (0.35, 0.45, 0.82 normalised), shade =
  0.42 + 0.58 x Lambert. A world-fixed light would make a bone change shade as
  the user orbits, which reads as the bone changing colour rather than the
  camera moving;
* the whole armature's facets go through ONE `Scene::drawTriangles()` call with
  per-vertex colours (`selection.prog` takes attribute 1 as the vertex colour
  when `vertexColorOverride` is zero, which is what passing a colour array does).

### 2.3 The colours, and where each number comes from

`skeletonKindColor( kind, state )` in `src/skeletontools.cpp` is the only
function that answers, and both the viewport and the dock's rows call it.

| kind | base | why |
|---|---|---|
| deforming bone | `toggle` **#4772b3** | bungo: *"Just keep the color of the bones blue"*; the palette's only blue |
| a bone a skin lists that no vertex uses | `accent` **#f0a54a** | kept from what shipped -- the dock's attention colour, whose tooltip already says "listed as a bone ... but no vertex is weighted to it". **A divergence from his words, named:** it is a bone and it is not blue. On the human fixture there are **zero** of them, so it is not what he will see; it is his to change |
| not a bone | `textMuted` **#aeb3ba** | unchanged |

| state | arithmetic | a deforming bone |
|---|---|---|
| unselected | base x 0.78 | #365989 |
| selected | base | #4772b3 |
| active | base, then 42% toward white | #8CA9D2 |
| hovered | base, then 24% toward white | #6B8DC2 |

The facets take their own Lambert shade on top (0.42..1.00), so a solid bone
reads as four faces of one colour rather than as four colours.

### 2.4 Divergences from Blender, each with its reason

1. **The head and tail balls are screen-space dots, not spheres.** Blender draws
   two 3-D spheres that grow as you zoom in. Ours are GL points at a fixed pixel
   size, which is what the joint markers already were and what keeps a 130-node
   rig pickable at any zoom. Consequence: very close in, the ball does not grow
   with the bone.
2. **B-Bone and Envelope are REFUSED.** Both need per-bone authored data -- a
   B-Bone a segment count and two handle bones, an Envelope a head radius, a
   tail radius and a distance -- and a NIF `NiNode` carries none of the five.
   Inventing them would be hand-authored data coupled to one rig, which the
   zero-authoring rule forbids. The Bone Display submenu has three rows, not
   five.
3. **Bones are blue, not Blender's grey.** bungo's ruling, and the one place
   this fork deliberately leaves the reference.
4. **No bone groups, no custom bone shapes, no bone axes.** Blender colours
   bones by bone group and can swap a bone for any object as its shape. A NIF
   has neither, so the colour carries the only classification the file has,
   which is the Skeleton Manager's three classes.
5. **Names are not a fourth checkbox in a data tab.** Blender puts Names, Axes,
   Shapes, Group Colors and Relationship Lines in the Armature data tab. Here
   Names is a row in the same Overlays > Bone Display submenu, and the Overlays
   menu's existing Show Nodes still turns every name on as well.
6. **One X-ray, not two.** Blender has both an armature "In Front" property and
   a viewport X-Ray mode. There is one row here, and it governs both views.
7. **A body reaches to the nearest LISTED ancestor, not only to the parent.**
   Blender has no equivalent, because it has no chip filter over an armature.
   Under the Bones chip an FO4 body rig lists the 93 `*_skin` helpers and not
   their parents, so joining only listed parents would draw 93 disconnected
   bones -- the very disconnection lane SKELFIX measured and refused. Under All
   the nearest listed ancestor IS the parent, so the picture that shipped is
   unchanged.

## 3. The mirror and the manager

### 3.1 One list, asked for twice

`skeletonListedBlocks( report, chip, search, isolated )` in
`src/skeletontools.cpp` is the only place the chip's predicates, the search text
and the dock's Isolate set are applied. The dock's tree is built from its
answer; `GLView::refreshSkeletonOverlay()` asks it with the same three
arguments. That makes bungo's *"shouldn't it mirror the skeleton manager view?"*
true by construction rather than by two files happening to agree, and it is what
gate (k) reads back -- **by name**, because two sets of 93 that are not the same
93 would pass a count comparison.

The dock pushes its chip and search text to the viewport at the end of every
`refresh()`, through `GLView::setSkeletonOverlayFilter()`. It is a no-op when
neither has moved.

**Search keeps ancestors.** A match's whole parent chain stays listed, the way
the Block List already behaves; without it, searching `Finger` in a 130-node rig
gave thirty rows at the top level with no sign of which hand they were on. A
chip on its own does NOT add ancestors -- the dock promotes a filtered row to
the top level instead, which is what keeps its Bones count equal to the
analysis's bone count and keeps `WW_SKELETON_TEST` green.

### 3.2 The bone column

The defect, measured before it was touched: column 0 was `Stretch` and the three
numeric columns `ResizeToContents`, so the name got whatever was left -- and a
`QTreeWidget` spends `indentation() * depth` of that on the branch before a
glyph is drawn. The human rig's arm chain is nine deep; at Qt's default 20 px
that is 180 px of a 400 px dock, and the rows under `LArm_UpperArm` read
`LAr...` and then nothing.

The fix is Blender's Outliner: indentation 12 px, the three numeric columns
**fixed** at 46 / 46 / 58 px on the right, and the name column sized in
`refresh()` to `max( what the deepest row needs, what the dock leaves )`. When
the dock is narrower than the widest row it is the dock that scrolls, never the
name that disappears.

**The trade, stated, and visible in `manager_after.png`:** at 400 px the rows
need 469 px (319 for the widest name + 150 for the three numbers), so a
horizontal scrollbar appears and the Weight column sits off the right edge until
the dock is widened or scrolled. That is the trade chosen -- a number you can
scroll to, rather than a name that is not there -- and it is the opposite of
what shipped, where the numbers were always visible and the name was not.

`WW_SKELETON_LEGACY_COLUMNS=1` is the exact way back AND gate (m)'s floor: the
same build, the shipped column law, one variable. `manager_before.png` and
`manager_after.png` are the two runs.

### 3.3 Rows coloured by kind, and the selection bug that came with it

Rows now take `skeletonKindColor( kind, 0 )` -- the same call the bone in the
viewport makes. Doing that exposed a defect nobody had noticed:
`paintSelection()` only cleared the BACKGROUND when a row was deselected, never
the foreground, so a row that had once been selected kept the selection's orange
text for the rest of the session and stopped saying what kind of node it was.
The kind is stashed on the row (`Qt::UserRole + 1`) and put back.

### 3.4 Selection, both ways

* **viewport -> dock**: a click on the Overlays armature picks the nearest drawn
  bone within 12 px, selects that block and emits `clicked()`, which is the
  application's own selection path; the dock's `currentNifIndexChanged` handler
  then makes the row current, expands its ancestors and scrolls to it. Before
  this lane that handler only rebuilt the tree and left it pointing at nothing.
  A click that hits no bone falls through, so ticking the overlay costs a user
  nothing they had before;
* **dock -> viewport**: selecting a row also calls `objectSelectClick()`, so the
  bone lights up in the viewport instead of only being the current block;
* **double-click a row** goes through `GLView::frameSelected()`. That needed a
  fix of its own: `frameSelected()` only ever grew its box over `scene->shapes`,
  and a bone is a node, not a shape, so it fell through to `center()` and framed
  the whole model. It now answers for bone nodes too, padded by the
  characteristic bone size;
* **hover** is two-way as well: the row under the pointer lights the bone, the
  bone under the pointer lights the row, and both ends read
  `skeletonKindColor( kind, 3 )`.

### 3.5 Names

Default: the hovered bone and the selected bones only. bungo's screenshot had
all 130 painted at once and his word for it was clutter. Overlays > Bone Display
> Names turns every name on, and the Overlays menu's existing Show Nodes still
does too, so the picture that shipped is one tick away.

## 4. Build and gates

### 4.1 The clocks

| artefact | time | size |
|---|---|---|
| `release/NifSkope.before_skel2.exe` (the rung) | 2026-09-11 **07:06:04** | 20,867,584 B |
| the build | 2026-09-11 **07:55:14** | 20,926,976 B |
| the ONE counted relink | 2026-09-11 **07:58:59** | **20,927,488 B** |
| `release/style.qss` | 07:55:15, byte-identical to `res/style.qss` | 11,097 B |

**ONE build and ONE relink.** The relink is the armature-membership fix in
section 7's first mistake; it changed `src/glview.cpp` only and nothing else was
rebuilt between the two. `tasklist | grep -i -E "Fallout4|NifSkope"` printed
`rc=1` immediately before the build AND immediately before the relink, so
neither the game nor a window of bungo's was up, no exe had to be renamed aside,
and nothing of his was touched at any point in this lane.

Consistency, not just success:

* **122 of 122** changed paths under `src res tools tests` are older than the exe;
* `src/glview.h` changed: **38 of 38** objects of the translation units that
  include it are newer than the header;
* `src/skeletontools.h` changed: **9 of 9**;
* `res/style.qss` and `release/style.qss` are byte-identical.

### 4.2 The gate table

All on the 07:58:59 exe, sequential, one instance, every window on the second
monitor. Logs under `scratchpad/skel2_20260910/logs/`.

| gate | this build | baseline on the rung | moved? |
|---|---|---|---|
| `skeleton_overlay.sh`, in-app (`WW_SKELOVERLAY_TEST`) | **46 checks, 0 failures, PASS** | 27 / **2** | **+19 checks, both failures gone** -- the 19 are lane SKEL2's (k)(k')(l)(l')(m)(m')(n)(o)(p) plus the tolerance's own floor; the two that were red are (c) and (e), now measured against the 64-px bar |
| `skeleton_overlay.sh` as a whole | **rc=0** | rc=1 | the picture gates, the coverage gate and the dots are all inside it |
| (j) under the **Bones** chip | 5 / 0 PASS, **0 stray pixels** | -- | newly registered |
| (j) under All (informational, no longer a gate) | 55 stray in 2 clusters | 56 on the rung | unchanged behaviour, by design |
| (j') FLOOR, the BUILD9 picture | fails (j), so (j) can fail | same | -- |
| S2 coverage, `Wire` vs the rung | **only-rung 2, only-new 0**, bar 64, PASS | -- | the way back is exact in geometry |
| S2 control, `Octahedral` vs the rung | FAILS the same comparison, as it must | -- | -- |
| S6 the dots | **2 clusters, 0 unnamed** | -- | `Camera` (148) and `CamTarget` (159) |
| S1 one renderer (source) | 6 / 6 ok | -- | -- |
| `WW_POSEDRAW_TEST` | RED, **the same one failure**: *clicking a bone did not make it the active object* | RED, same failure | not this lane's; pixels changed 970 -> 1088 (the solid shape draws more ink) |
| `WW_POSEEXTRAS_TEST` | RED, **the same one failure**: *weight overlay found no influenced vertices* | RED, same failure | not this lane's |
| `WW_SKELETON_TEST` | PASS -- All 130, Bones 93, Deforming 93, Unused 0 | same | -- |
| `animws.sh` | 72 / 0, 1 skip, PASS | 72 / 0, 1 skip | -- |
| `hkxanim_ui.sh` | 48 / **1** -- the same unfirable wheel floor | 48 / 1 | -- |
| `water_ui.sh` | **86 / 0, PASS** with the shot paths set | 86 / 0 | -- |
| `ui_align.sh` | 11 / 0, PASS | 11 / 0 | -- |
| `loaded_nifs.sh` | 166 / **3** -- the same three UI6 named | 166 / 3 | -- |
| `top_bar.sh` | 43 / **5** -- the same five `Panels lists the ... dock` | 43 / 5 | -- |
| `files_tab.sh` | 28 / **2** -- the same two | 28 / 2 | -- |

**The one count that moved, named by check.** `water_ui.sh` read **82 / 0** the
first time it was run here, against UI6's 86 / 0. The four missing checks are
`(shot) the top-strip grab was written`, `(shot) the LOD-tab grab of the LOD
panel was written`, `(shot) the zoomed strip crop was written` and
`(shot floor) ...and it is not an empty strip`. All four are armed only when the
spell is given `SHOT=` / `TABSHOT=` / `STRIPSHOT=` / `LODSHOT=`, which the first
run did not pass. Re-run with them: **86 / 0, PASS**, the same suite UI6 ran.
Nothing regressed; the first run simply asked four fewer questions.

**Skipped, with the reason:** every suite this change cannot reach -- the LOD
generator, the terrain and `.lodl` readers, the impostor and card gates, the
water solve / flow / weights / marking gates, the collision, block and glTF
gates, and `hkxfile_gates.py`. This lane touched the viewport's armature, the
Skeleton Manager dock, one Overlays submenu and two harness files; none of those
suites reads any of them. `poselib.sh` and `sam_pose_import.sh` were not run:
they drive the pose LIBRARY and the SAM importer, which this lane did not touch,
and both need the power-armour Frame fixture that is not in `fixtures/`.

### 4.3 The census fields, written AND moving

| field | off | All chip | Bones chip |
|---|---|---|---|
| `nodes` | 0 | 130 | 93 |
| `skipped` (marker-only) | 0 | 19 | 0 |
| `filtered` (rows the chip removed) | 0 | **0** | **37** |

`filtered` is new this lane and gate (n) holds both halves: zero on All, above
zero on Bones. `skipped` kept its name and CHANGED ITS MEANING -- it used to
count refused BODIES (38) and now counts marker-only NODES (19). Same idea,
cleaner definition, and it is stated here rather than left for the next reader
of the log to trip over.

## 5. Pictures

All in `scratchpad/skel2_20260910/images/`, composed by `compose.py` from
renders and in-application grabs the spell took; nothing was re-rendered to make
them and nothing is a desktop capture.

* **`bones_stick_vs_octa.png`** (960x964) -- the same legs, the same pinned
  orthographic camera, Stick above and Octahedral below at 1.6x. The top half
  shows one thin blue line per bone with a dot at each joint; the bottom half
  shows the solid octahedra, wide at the knee and tapering to a point at the
  ankle, with the four facets taking different light so the bone's direction and
  twist read at a glance.
* **`cmp_bones_before_after.png`** (960x964) -- the same crop, the 07:06:04
  build above and this one below. Above, the shipped near-white wireframe
  octahedra with the collar at 15% of the length; below, the same rig as solid
  blue octahedra with the collar at Blender's 10%.
* **`bones_bones_chip.png`** (840x595) -- frame 46 under the Skeleton Manager's
  Bones chip: 93 rows, every one of them blue, and **zero pixels off the
  character** (gate (j), 5 checks 0 failures). The long reach-through bodies
  across the torso are the honest consequence of the mirror and are flagged in
  section 6.
* **`bones_all_chip.png`** (840x595) -- the same frame under All: 130 rows, and
  the two muted grey dots above the back that BUILD11 could not name. They are
  `Camera` (block 148) and `CamTarget` (block 159).
* **`bones_xray.png`** (1524x504) -- the whole figure twice at half scale. Left,
  X-ray on (the default): the rig reads through the mesh. Right, X-ray off: the
  closed body mesh hides every bone inside it, so the figure is bare -- which is
  correct and worth knowing before ticking it.
* **`manager_after.png`** (400 px wide) -- an in-application grab of the
  Skeleton Manager dock at a FORCED 400 px, scrolled to the deepest arm rows.
* **`cmp_manager_names.png`** (1600x474) -- the same dock, the same build, the
  same rows, at 2x: left under the shipped column law
  (`WW_SKELETON_LEGACY_COLUMNS=1`), right under the new one. On the left the
  nine-deep rows show `RA...` or nothing at all beside their three numbers; on
  the right every one of them reads its whole name, in the kind colour.

Gate evidence images, not deliverables:
`scratchpad/skeloverlay_20260910/gates/` -- `gate_frame46_bones_mask.png`,
`gate_frame46_mask.png`, `dots_frame46.png`, `coverage_wire.png`,
`coverage_octa.png`.

## 6. Owed / red / bungo's calls

1. **The Bones chip draws long reach-through bodies** (`bones_bones_chip.png`).
   An FO4 body mesh weights the `*_skin` helpers, so under the Bones chip the
   dock lists 93 helpers and NONE of the chain nodes between them; each listed
   bone therefore reaches to its nearest listed ancestor, which can be across
   the torso. It is a faithful mirror and it passes every gate (0 pixels off the
   character), but it does not look like a skeleton. Three ways out, his call:
   (a) leave it; (b) draw each listed bone as its own short stub under a chip,
   which is closer to what Blender shows when bones are hidden; (c) let a chip
   keep the connecting nodes as muted context, the way a SEARCH already keeps
   ancestors -- which would break the "the overlay lists exactly the dock's
   rows" gate as written.
2. **17 of the 172 drawn bodies are muted grey, not blue**, and they are the
   animation chain: `Pelvis`, `LArm_UpperArm`, `LArm_ForeArm1..3`,
   `LArm_UpperTwist1/2`, `LLeg_Thigh`, `LLeg_Calf` and their right-hand twins.
   The Skeleton Manager classes them "not a bone" because no skin lists them,
   which is true of this file and is what the mirror says. 155 bodies and 93 of
   the 130 joint balls are blue. If he wants the whole armature blue, the change
   is one line -- the armature's non-bone nodes take kind 0 instead of kind 2 --
   and it would make the dock's own colour coding say something different from
   its own Bones filter.
3. **An unused bone is amber, not blue.** Named in 2.3 as a divergence from his
   words. Zero of them on the human fixture.
4. **`WW_POSEDRAW_TEST` and `WW_POSEEXTRAS_TEST` are RED and were red on the
   rung**, each with one named failure, measured before this lane wrote a line.
   Neither is repaired here and neither is claimed. `WW_POSEDRAW_TEST`'s is a
   real defect in the application (a click at bone 0's screen position selects
   block 5); `WW_POSEEXTRAS_TEST`'s is a defect in the harness (it inspects bone
   0, the root, which drives no vertex).
5. **`loaded_nifs.sh` 166 / 3, `top_bar.sh` 43 / 5, `files_tab.sh` 28 / 2,
   `hkxanim_ui.sh` 48 / 1** -- all unchanged from UI6's baselines and all owed to
   other lanes.
6. **Nothing is committed** (CONSTITUTION 8). This lane changed
   `src/glview.{h,cpp}`, `src/skeletontools.{h,cpp}`, `src/skeloverlaytest.cpp`,
   `src/nifskope_ui.cpp`, `tests/spells/skeleton_overlay.sh`, and added
   `tests/spells/skeleton_overlay_coverage.py` and
   `tests/spells/skeleton_overlay_dots.py`.
7. **NOT MEASURED, and it is the one thing here a gate does not touch: nobody
   CLICKS the new menu rows.** Every gate drives the armature through the API
   (`setArmatureDisplay` / `setArmatureXray` / `setArmatureNames` /
   `setSkeletonOverlayFilter`) or through the render hook's environment
   switches. The Overlays > Bone Display submenu itself is proved only to the
   extent that its strings are in the binary -- `Bone Display` x1,
   `GLView/ArmatureDisplay` x1, `BoneDisplay%1` x1, read back out of
   `release/NifSkope.exe` with the UTF-16 and Latin-1 searches -- and that the
   code compiles. A harness that opens the dropdown and clicks the three rows is
   owed; the discriminator for a defect there would be the setting sticking
   across a restart while the viewport does not change, or the reverse.
8. **His open window needs a restart.** Whatever he opens next must be launched
   after **07:58:59**.

## 7. Mistakes

Three entries, written into `scratchpad/skel2_20260910/MISTAKES_ENTRIES.md` for
the director to splice into `MISTAKES.md` (this lane does not edit that file):

1. **a refactor widened a shipped RULE, and only the gate's own number said so**
   -- the armature-membership test became "is the common root an ancestor",
   which took in `PipboyBone`, `WEAPON`, `WeaponLeft` and the `AnimObject*`
   nodes: armature 111 -> 120, marker-only 19 -> 10, bodies+stubs 172 -> 190,
   782 more pixels covered. Forty-six in-application checks were green,
   including the two written to catch a bone in the wrong place. The coverage
   gate caught it; one relink fixed it; the numbers are back at 111 / 19 / 172;
2. **a pre-registered gate that could not pass** -- S2 as written asked for a
   byte-identical framebuffer under a ruling that changes every overlay pixel's
   colour, against a shape the brief misdescribed. Re-registered in section 0.4
   BEFORE the build, with its own floors;
3. **`WW_RENDER_SIZE`'s width floor moved again** (1437 -> 1293 -> **1524**) and
   a remembered screen coordinate was nearly used to name the two dots. They are
   named from the overlay's own per-frame dump instead.

## 8. Finished-work skill review (CONSTITUTION rule 1a)

**Loaded and used.** `nifskope-ww-build-verify` -- the gated chain, the
`&&`-chained process check immediately before each link, the syntax pass with
`Makefile.Release`'s own flags under `sx_SKEL2.sh` (which caught nothing this
time but cost 90 seconds), the header-staleness sweep for `glview.h` and
`skeletontools.h`, the exe-newer sweep over the whole `git status` set.
`ww-test-harness-add` -- the arming and timing shape, the "read the widgets, not
the private members" rule (which is why `sizeHintForColumn` had to become
`header()->sectionSizeHint`, caught by `-fsyntax-only` and not by a build), the
floor-that-can-fire rule, and 5a's warning that a check's TEXT is an interface,
which is why the spell's own greps were re-read after (c) and (e) were reworded.
`nifskope-ww-render-shot` -- the pinned orthographic camera, `env` for a
conditional variable, reading the size back with PIL, and the build-dependent
width floor. `ww-anchored-hookup` -- `src/nifskope_ui.cpp` went in through a
refusing script with its anchor counts and CR assertion printed.
`nifskope-ww-panel-style` -- the dock's colours through `wwSkinColor` only, no
blurbs in the new submenu, and the self-test counting each rule with a floor.

**Not loaded, with the reason.** `ww-retire-a-surface` -- nothing was retired;
the old drawing path is still reachable, as the `Wire` display mode, which is
the opposite of retiring it. `ww-silhouette-compare` -- offered for the bone
shape gate, but the shape's proof is a coverage comparison against the previous
build plus two pictures, and a silhouette metric would have added a number
without adding a refuter.

**Written.** `<repo>/.claude/skills/ww-way-back-by-coverage/SKILL.md` -- how to
prove an exact way back when bungo has ALSO ordered the look to change: read
what the code actually draws before pre-registering a picture comparison, take
the rung's renders before the link, compare the SET OF PIXELS the feature covers
rather than their colours, carry the three floors (both coverages non-empty, the
two ON renders must differ, the new default mode must FAIL the same test), and
the shape of merging two drawing routines into one without losing either view's
behaviour (per-caller state, keep the draw order, one element per drawn thing,
the way back as a MODE that keeps the old constants, one palette function with a
source gate counting its callers). This lane spent a build learning the last
part and the next renderer merge will need all of it.

**Amended, and they need mirroring to `E:\Projects\Claude\.claude\skills`:**

* `<repo>/.claude/skills/ww-test-harness-add/SKILL.md` -- new sections **5c** (a
  framebuffer check needs a MEASURED tolerance: five runs on the rung, a fixed
  count above the worst, the number written into the check's own sentence, and
  the tolerance's own floor that the same bar still refuses a real difference --
  plus the rule that a check red for two builds on correct code is a defect in
  the check) and **5d** (force every state the gate measures, including the ones
  the lane itself has just made persistent in QSettings);
* `<repo>/.claude/skills/nifskope-ww-render-shot/SKILL.md` -- the
  `WW_RENDER_SIZE` table gains the exe each row was measured on and the new
  **1524x941** row, and a corollary: a screen COORDINATE is never carried
  between builds either, because an orthographic camera's units-per-pixel moves
  with the width; the honest way to name a thing in a picture is to have the
  code that drew it say where it put it.

**Declined, named.** A skill for "add a row to the Overlays menu" -- the Overlays
dropdown is built in one place, the pattern is four lines, and
`nifskope-ww-panel-style` already carries the rules it has to obey. It will not
recur often enough to earn a page.
