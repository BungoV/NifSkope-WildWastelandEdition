# Lane SKEL2 -- entries for MISTAKES.md (append-only; the director splices)

## 2026-09-11, lane SKEL2 -- a refactor widened a shipped RULE, and only the gate's own number said so

**What was done.** `refreshSkeletonOverlay()` was restructured so the overlay
could list the Skeleton Manager's chip and search. In the rewrite, the test for
"is this node part of the armature" became "is the common root an ancestor of
it", applied to every drawn node.

**What was true instead.** Lane SKELFIX's shipped rule is narrower: the armature
is every node a skin lists CLOSED UPWARDS through the parent chain, then cut at
the common root -- so a node that merely hangs BENEATH a bone without being one
is outside it. The rewrite took in `PipboyBone`, `WEAPON`, `WeaponLeft` and the
`AnimObject*` nodes under the character and gave each of them a bone body.
Measured: armature **111 -> 120**, marker-only **19 -> 10**, bodies+stubs
**172 -> 190**, and **782 more pixels covered** in the `Wire` display.

**How it was found.** Not by reading the code back. Gate S2 -- "the way back
covers the same pixels the shipped overlay covered" -- went red at
`only-new 782, bar 64`, and the census line in the same log had moved from
`19` to `10` marker-only nodes. Every other gate in the suite was green,
including the two that exist to catch a bone in the wrong place ((f) longest
31.94 of a 33.60 limit, (g) 0 endpoints outside the character's box), because
the extra bodies were all ON the character and all short.

**The rule that prevents it.** When a refactor moves a shipped RULE into new
code, the rule's own published numbers are a gate, not a footnote: SKELFIX
printed `111 / 19` in its report and in the tooltip sentence, and those two
numbers should have been pre-registered as "unchanged" before the first build.
A gate that compares the new picture to the OLD BUILD'S picture is what caught
it, which is the case for keeping a rung's renders before the link even when
nobody expects the picture to move.

## 2026-09-11, lane SKEL2 -- a pre-registered gate that could not pass, and why the brief could not have known

**What was asked.** The brief pre-registered "S2: Stick mode == the 07:06:04
framebuffer for the same bones and camera within tolerance" -- an exact way back.

**What was true instead.** Two facts, both readable in the source before any
code was written. (1) The overlay did NOT draw plain segments: it drew a 12-line
WIREFRAME OCTAHEDRON, so `Stick` -- Blender's plain head-to-tail line -- is a
different shape from what shipped and can never reproduce it. (2) bungo's own
colour ruling in the same session ("Just keep the color of the bones blue")
changes every overlay pixel's colour, so no display mode can reproduce that
framebuffer.

**What was done instead**, before the code and written into the report's
pre-registration: a THIRD display mode named `Wire` (which is also Blender's own
name for that shape) carries the way back, and S2 was re-registered as the thing
the colour ruling did not license to move -- the SET OF PIXELS the overlay
covers, measured on the rung and on the new build and compared. It is a weaker
statement than byte equality and it is stated as one; it is also the gate that
caught the entry above.

**The rule.** A gate that cannot pass is not a gate, and finding that out costs
a build. Read what the code actually draws before pre-registering a picture
comparison against it, and re-register in the REPORT, before the first build,
when a pre-registered gate turns out to contradict a later ruling.

## 2026-09-11, lane SKEL2 -- WW_RENDER_SIZE's width floor moved again, and a remembered coordinate was nearly used

**What was done.** The brief names the two grey dots by their coordinates in
BUILD11's frame -- `(486,485)` and `(463,503)` in a 1293x941 run -- and the
obvious route was to project the nodes and look for those two points.

**What was true instead.** The `WW_RENDER_SIZE` width floor is
build-dependent. It was 1437 on the 15:52:46 exe of 2026-09-10, 1293 on
17:08:39, and on this lane's rung a request of 1000x1000 comes back
**1524x941**. The ortho camera's units-per-pixel moves with the width, so the
same node lands at a different coordinate in every build -- and the two dots
are at `(574,491)` and `(546,509)` here.

**How it was avoided.** The dots were named from the overlay's OWN per-frame
dump (`WW_SKELOVERLAY_DUMP`, written inside `drawSkeletonOverlay()` with the
viewport size in its header) matched against the stray-pixel clusters measured
from the same pair of renders, with a 12-px refusal so a node that merely
happens to be nearest is not named. They are `Camera` (block 148) and
`CamTarget` (block 159), matched 0.9 and 0.8 px from the cluster centres.

**The rule.** A screen coordinate is never carried between builds. The skill
`nifskope-ww-render-shot` already says the floor is measured per build; the
corollary this lane adds is that anything DERIVED from it -- a pixel position in
a picture -- is measured per build too, and the honest way to name something in
a picture is to have the code that drew it say where it put it.
