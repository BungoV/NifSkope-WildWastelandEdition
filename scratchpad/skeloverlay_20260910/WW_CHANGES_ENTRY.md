<!-- Lane SKELOVERLAY, 2026-09-10. TEXT ONLY -- the director splices this into
     WW_CHANGES.md. Never sed -i that file: it is MIXED and stays so; assert its
     CR count is unchanged after the splice (19,020 as of 2026-09-09; the
     2026-09 entries at the top are LF-only, so this block goes in LF).
     The bracketed numbers are filled in by whoever builds it -- they are
     pre-registered gates, not results. -->

### Overlays > Show Skeleton -- the rig, over the model, through the mesh

bungo, 2026-09-10, verbatim: *"Add to the overlays: View skeleton, shows you the
bones, basically the same view as in the skeleton manager"*.

A new checkable entry in the viewport's **Overlays** dropdown, beside Show Nodes
and Do Skinning. Default off, persisted as `GLView/Display/ShowSkeleton` like
the other entries in that menu. It draws the whole rig with the depth test off
so it reads through the mesh, at a fixed pixel weight, following whatever pose
the frame is in -- so a loaded `.hkx` clip animates the armature.

**"Basically the same view as in the skeleton manager" is implemented as a
shared cause, not as a resemblance.** The overlay's bone list is
`skeletonAnalyse()`'s -- the same call the Skeleton Manager dock builds its tree
from and the same call the `skeleton` CLI prints -- and its three colours are
that analysis's own three classes: the palette's `text` for a bone a vertex is
weighted to, `accent` for a bone a skin lists that nothing uses, `textMuted` for
a node no skin references. The dock's Deforming and Unused filter counts are
therefore this overlay's colour counts, and the harness holds them against each
other rather than against the analysis they both come from.

It is deliberately NOT the Skeleton Manager's own `skeletonView` flag reached a
second way: that flag is driven by the DOCK'S VISIBILITY, so an Overlays tick
writing it would be switched back off the next time the dock was shown or
hidden.

**Bone names ride on the existing Show Nodes entry** rather than a row of their
own -- it is already the toggle that labels what the viewport draws over the
model, and these are the same node names. Blender's armature overlay has a
separate Names checkbox; the divergence is deliberate and is one of five listed
in `scratchpad/lane_skeloverlay_report.md` §2 (the others: one tick instead of
Blender's six, class colours instead of bone-group colours, one octahedral body
per parent->child pair plus a capped stub for a leaf because a NIF bone has no
authored tail, and the overlay being read-only because Pose Mode and the
Skeleton Manager already own picking).

**Two small pieces of shared code came out of it.**
`GLView::characteristicBoneSize()` and `GLView::boneTailIn()` are
`refreshPoseBoneSize()` and `poseBoneTail()`'s own laws, lifted so the Pose Mode
armature and the Overlays armature cannot drift apart. `Scene::findNode()` is a
look-up that does NOT create a Node: `Scene::getNode()` constructs one for any
block it is handed, and this overlay is offered every `NiAVObject` block in the
file, so on a file with a block the graph does not reach it would have grown
`nodes`, moved `Scene::bounds()` and changed the very picture it is only
supposed to draw on top of. Blocks with no scene node are COUNTED
(`missingNodes`) rather than hidden.

**Verification.** `tests/spells/skeleton_overlay.sh` on
`fixtures/human_male_vanilla.nif` with
`fixtures/Running_To_Slide_And_Back_To_Running.hkx` (93 frames at 60 fps) at
frame 46 -- the frame bungo already has as `mixamo_fhalf` in
`scratchpad/build7_20260910/frames_mixamo.png`. The gates, each with a floor:

* (a) the overlay's joint count is the **dock's own All-filter row count**, read
  off the dock's tree at run time; floor (a'): with the overlay off its census
  is all zeros;
* (b) the three colour counts are the dock's **Bones / Deforming / Unused**
  filter counts, and the muted ones are All minus Bones;
* (c) a render with the overlay on differs from one with it off ONLY inside a
  mask rasterised from the segments the overlay REPORTS having drawn; floors
  (c'): some pixels must differ at all, and the mask must cover under 80% of the
  frame, or it would pass (c) for free;
* (d) at frame 46 every drawn joint is within 1e-3 units of the ANIMATED
  `Node::worldTrans()`; floor (d'): the same drawn positions against the bind
  pose must disagree on at least one bone;
* (e) toggling the overlay off restores the off-render byte for byte.

Result: [N] checks, [M] failures, exe [HH:MM:SS]. Dock/overlay numbers measured
on the fixture: [nodes] node(s), [bones] bone(s), [D] deforming, [U] unused.
Pictures: `scratchpad/skeloverlay_20260910/off.png`, `on.png`, `on_frame46.png`
(one pinned orthographic camera through the render hook) and the harness's own
`gates/gate_mask.png`, which paints the mask dark grey and every changed pixel
orange.

**Not measured:** whether the armature is legible on a dense facial rig (no
instrument for that; the picture is the only one and bungo is the judge), and
whether the pose-armature factoring is behaviour-identical -- `WW_SKELETON_TEST`
and `WW_POSEDRAW_TEST` are what would say so.
