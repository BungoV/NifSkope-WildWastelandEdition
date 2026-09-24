<!-- Lane SKELFIX, 2026-09-10. TEXT ONLY for WW_CHANGES.md; the director splices
     it (CONSTITUTION rule 8). The file is MIXED and the 2026-09 entries at the
     top are LF-only: paste as LF and assert CR 19,020 is unchanged.
     STATUS BLOCK AT THE END IS A PREDICTION -- replace it with the measured
     numbers when the build lands. -->

## 2026-09-10 -- Overlays > Show Skeleton draws the ARMATURE, not every parent-child pair (lane SKELFIX)

The director, on `scratchpad/skeloverlay_20260910/on_frame46.png`: long segments
fan from the character to a point far above and left of it. BUILD9's gate table
said `17 checks, 0 failures, PASS` on that same frame -- correctly, because no
check measured how LONG a drawn bone was or WHERE it went.

**The cause, measured, not named.** The overlay drew a body between every parent
and child in its list. At frame 46 of the Mixamo clip the character's travel is
carried on the COM track (487 units in Y; the clip's root-motion channel is all
zeros), while `Root`, `Camera`, `Camera Control`, `CamTarget`, `CamTargetParent`
and the eight `AnimObject*` nodes sit at the world origin, and `CharacterBumper`
and `EyeLeftDummy001` sit at their bind spots under the file root. Each was
being joined to a relative that had moved:

| child | class | tracked | parent | segment |
|---|---|---|---|---|
| COM | not-a-bone | yes | Root | **300.5** |
| CamTarget | not-a-bone | yes | CamTargetParent | **289.6** |
| Camera | not-a-bone | yes | Root | **288.1** |
| CharacterBumper, EyeLeftDummy001, Root, Camera Control, AnimObjectA/B, CamTargetParent | not-a-bone | mixed | -- | endpoint 275-300 units from COM |

For scale: the longest bind-pose segment between two nodes the Skeleton Manager
calls bones is **22.40** units, and the character is **38.3** units tall at that
frame. Full table: `scratchpad/skelfix_20260910/segments_frame46.tsv`.

**The rule that shipped.** A bone body is drawn only between two ARMATURE nodes,
where the armature is *every node a skin lists (the Skeleton Manager's Bones
filter), closed upwards through the parent chain, then cut at the deepest node
that still has all of those bones beneath it*. On the fixture that root is
`COM`; 111 of the 130 nodes are armature and the 19 that are not are exactly the
camera, weapon, anim-object, bumper, eye-dummy and above-root nodes -- excluded
because no skin bone sits beneath them, never because of their names. A node
outside the armature is still listed and still gets its joint marker, so the
overlay's census still equals the dock's and gates (a) and (b) are untouched.

**The Bones filter ALONE was measured and refused.** FO4 body meshes weight the
`*_skin` helper bones, so `Pelvis`, `COM`, `LLeg_Thigh`, `LLeg_Calf`,
`LArm_UpperArm` and the whole limb chain are not in the Bones filter: 33
deforming bones have a parent the dock calls "not a bone". Taking the filter
alone draws 60 of 129 segments and takes the spine, both legs and both forearms
apart; adding a "the child has a track, or its whole parent chain does" test
takes it to 40. That track test is also the wrong instrument: an untracked node
cannot lag behind a tracked parent (with no track it keeps its bind local and
inherits its parent's world transform), and the nodes that stay behind are the
TRACKED ones the clip parks at the origin.

| rule at frame 46 | segments | longest | worst endpoint from COM |
|---|---|---|---|
| what shipped in BUILD9 | 129 | 300.5 | 300.5 |
| Bones filter both ends | 60 | 22.4 | 73.0 |
| Bones filter + track test | 40 | 22.4 | 73.0 |
| **armature closure (shipped)** | **110** | **31.9** | **73.0** |
| the gate's limits | -- | <= 33.6 | <= 76.6 |

**Also.** `setSkeletonOverlay(true)` now builds the list on the way in as well as
lazily, so `GLView::skeletonOverlayRule()` -- the sentence the Overlays entry
puts in its tooltip, naming the armature's root and how many nodes are
marker-only -- exists the moment the entry is ticked. A file with no skin at all
falls back to drawing every node as a bone and SAYS SO in that sentence
(CONSTITUTION rule 10). The census gained `skipped`, the count of bodies the
rule refused.

**New gates** in `tests/spells/skeleton_overlay.sh` / `src/skeloverlaytest.cpp`,
each with a floor that is the OLD rule on the same readback: (f) no drawn
segment longer than 1.5x the longest bind-pose bone-to-bone segment, (g) every
segment endpoint inside the bones' own bounding box, (h) `skipped` is written
and moves, (i) the rule is stated in words and the armature has members and
non-members. And (j), in `tests/spells/skeleton_overlay_mask.py`: every pixel
the overlay changed in `on_frame46.png` lies inside the character's bounding box
as measured from `off_frame46.png` -- the same frame with the overlay off, so
the overlay cannot enlarge the box it is judged against. Its floor (j') is the
BUILD9 picture itself, kept at
`scratchpad/skelfix_20260910/before_on_frame46.png`.

### STATUS: BUILD PENDING -- predictions, not results

`scratchpad/build10_20260910/` carried `BUILDING_WATER5` and no `DONE` at
16:2x, so nothing was built and `release/NifSkope.exe` (15:52:46) was not
touched. What IS proven: `src/glview.cpp` and `src/skeloverlaytest.cpp` both
pass `g++ -fsyntax-only` with `Makefile.Release`'s own flags, RC=0, with only
the five warnings lane SKELOVERLAY already recorded as pre-existing; the shell
and Python gates pass `bash -n` and `py_compile`; `skeleton_overlay_mask.py`
was exercised end to end on the bind-pose pair (`off.png` vs `on.png`,
5 checks, 0 failures). Line endings: `src/glview.cpp` CR 23,284 -> 23,423 with
dCR = dLF = +139; every other file CR 0.

The numbers the gate is predicted to print, from an offline model whose controls
reproduce the built application exactly (`measure.py`: dock All 130 / Bones 93 /
Deforming 93 / Unused 0 and the clip's 78 matched / 17 unmatched / 4 case-folded
all reproduced): segments **110**, stubs **62**, skipped **38**, armature
**111** / marker-only **19**, (f) longest **31.94** vs limit **33.60**, (f')
old rule **300.5**, (g) **0** outside, (g') old rule **19** outside worst
**241.3**. Resume: `scratchpad/skelfix_20260910/PENDING.md`.

**Not measured:** whether the fixed picture reads well to bungo. The re-render
needs the build; the interim is a diagram of the two rules' drawn segments at
one scale, `scratchpad/skelfix_20260910/rule_before_after.png`, which is NOT a
render and does not discharge CONSTITUTION rule 5.
