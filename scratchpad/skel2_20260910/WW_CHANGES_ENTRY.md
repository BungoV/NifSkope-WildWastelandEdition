# Entry for WW_CHANGES.md -- lane SKEL2 (the director splices; this lane does not edit that file)

## 2026-09-11 -- One skeleton renderer, Blender's octahedral bone in blue, and the overlay mirroring the Skeleton Manager

bungo, over a screenshot of `human_male_vanilla.nif` with Overlays > Show
Skeleton on beside the Skeleton Manager: *"for that bone view toggle, shouldn't
it mirror the skeleton manager view?"*, *"shouldn't we improve both views (that
will now be shared)?"*, *"what about the bone shape? Shouldn't it be something
like in Blender?"* and, over a picture of Blender 4.5.3's default armature bone,
*"Just keep the color of the bones blue"*.

`release/NifSkope.exe` **2026-09-11 07:58:59, 20,927,488 B** (one build at
07:55:14 plus one counted relink). Rung `release/NifSkope.before_skel2.exe` =
the 07:06:04 bytes. **A restart is needed.**

### One renderer

Pose Mode and the Overlays armature were two routines drawing the same rig with
two colour laws, two shape rules and two sets of widths. There is now one --
`GLView::drawArmature( QVector<ArmatureBone>, ArmatureStyle )` -- fed by each
caller with a bone list plus per-bone state (selected / active / hovered /
pinned / marker-only / stub / depth fade). Everything Pose Mode had is kept:
relationship lines, the depth ramp, pins, hover, the weight overlay, mirror and
the pose import/export. A source gate in `tests/spells/skeleton_overlay.sh`
counts the bone-drawing calls inside each of the two functions (0) and the
`drawArmature(` calls (>= 1), so they cannot drift apart again.

One palette function answers for the viewport AND the dock's rows:
`skeletonKindColor( kind, state )` in `src/skeletontools.cpp`, and the gate
counts its callers.

### The bone

Blender's solid octahedron: eight flat-lit facets, the square ring at 0.10 of
the length (Blender's own value), a ball at the head and a smaller one at the
tail. Back faces are dropped on the CPU because the armature draws with the
depth test off; the light is fixed in camera space so a bone does not change
shade as the user orbits; the whole armature's facets go through one
`drawTriangles` call with per-vertex colours.

**Colour: the palette's blue** `toggle` **#4772b3** -- Blender's own option blue
and the only blue in `skinVars[]`. Unselected is that blue at 0.78 brightness,
selected is the blue, active is 42% toward white, hovered 24%. A node no skin
references keeps its muted grey.

**Overlays > Bone Display** (rows only, no text): Octahedral (default), Stick,
Wire, then X-ray and Names. **Wire is the exact way back** -- the 12-line
wireframe with the collar at the shipped 0.15 -- and it is gated as one:
`tests/spells/skeleton_overlay_coverage.py` compares the SET OF PIXELS the
overlay covers on the rung and on this build. **only-rung 2, only-new 0** out of
34,904 covered pixels, against a bar of 64. B-Bone and Envelope are REFUSED:
both need per-bone authored data (segment count, handle bones, envelope radii)
that a NIF `NiNode` does not carry.

### The mirror

`skeletonListedBlocks( report, chip, search, isolated )` is the ONE place the
chip's predicates, the search text and the dock's Isolate set are applied; the
dock's tree and the overlay's list are both built from its answer. The dock
pushes its chip and search box to the viewport at the end of every refresh.
Gate (k) reads the two sets back **by name** under all four chips and under a
search, with a floor (a chip the dock is not showing must make them disagree).
A body reaches to the nearest LISTED armature ancestor, so a chip cannot cut the
rig into disconnected pieces; under All that ancestor is the parent and the
picture is unchanged.

Selection is two-way: a click on a bone selects it and makes its row current and
visible; selecting a row lights the bone; double-clicking a row frames it
(`frameSelected()` now answers for bone NODES, which are not shapes and used to
fall through to Center); hover works in both directions. Names are on the
hovered and selected bones only by default -- his word for all 130 at once was
clutter -- with Overlays > Bone Display > Names for all of them.

### The Skeleton Manager's list

The Bone column no longer elides to nothing. Indentation 12 px, the three
numeric columns fixed at 46 / 46 / 58 on the right, and the name column sized
from the rows themselves, so a narrow dock scrolls instead of swallowing the
name. Measured at a forced 400 px on the human rig: **95 rows at depth 6 or
deeper, 0 empty, 0 elided**, the tightest (`RArm_Finger13`, depth 14) with 59 px
to spare. Under the shipped law the same row had **-71 px** and 50 of those rows
were elided. `WW_SKELETON_LEGACY_COLUMNS=1` is the exact way back and is what
the before/after picture is made of.

Rows are coloured by kind in the same colours the viewport uses, a search keeps
the ancestors of every match visible, and a deselected row gets its kind colour
back -- before this it kept the selection's orange for the rest of the session.

### The two grey dots, named

BUILD11 shipped `on_frame46.png` with 53 pixels of overlay ink above the
character's back and nobody could say what they were. They are **`Camera`
(block 148)** and **`CamTarget` (block 159)**, both marker-only nodes, named
from a per-frame dump the overlay itself writes (`WW_SKELOVERLAY_DUMP`) matched
against the stray-pixel clusters, 0.9 and 0.8 px from their centres. Under the
**Bones** chip they are not listed and the frame has **zero** stray pixels,
which is where gate (j) is now registered; under All they are drawn on purpose
and the old gate is dropped with the reason.

### The flaky gates

(c) and (e) demanded EXACT framebuffer equality on a `grabFramebuffer()` result
that is not bit-stable, and had been red on a correct overlay for two builds.
Five identical runs on the rung measured (c) at 16 / 13 / 14 / 9 / 1 and (e) at
12 / 22 / 19 / 15 / 21; with BUILD11's four runs the worst value on record is 37
out of ~1.2 million pixels. The bar is **64 px**, fixed, written into the check's
own sentence, and it carries its own floor -- the same bar must still refuse the
overlay's real ON-vs-OFF difference.

### Numbers

`skeleton_overlay.sh` **rc=0**: in-app **46 checks, 0 failures** (was 27 / 2),
(j) under Bones 5 / 0 with 0 stray, S2 coverage 4 / 0, the dots 2 clusters 0
unnamed, S1 6 / 6. Neighbours all at their baselines: `animws` 72 / 0 1 skip,
`water_ui` 86 / 0, `ui_align` 11 / 0, `hkxanim_ui` 48 / 1, `loaded_nifs`
166 / 3, `top_bar` 43 / 5, `files_tab` 28 / 2. `WW_SKELETON_TEST` PASS
(130 / 93 / 93 / 0). `WW_POSEDRAW_TEST` and `WW_POSEEXTRAS_TEST` are RED with
the SAME one failure each they had on the rung, measured before this lane wrote
a line; neither is repaired here and neither is claimed.

Pictures: `scratchpad/skel2_20260910/images/` --
`bones_stick_vs_octa.png`, `cmp_bones_before_after.png`, `bones_bones_chip.png`,
`bones_all_chip.png`, `bones_xray.png`, `manager_after.png`,
`cmp_manager_names.png`.
