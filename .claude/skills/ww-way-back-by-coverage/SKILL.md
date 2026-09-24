---
name: ww-way-back-by-coverage
description: Prove the EXACT WAY BACK for a viewport change when bungo has also ordered the look to change, so a byte-for-byte framebuffer comparison can never pass -- keep the rung's renders before the link, compare the SET OF PIXELS THE FEATURE COVERS rather than their colours, and carry the three floors that stop the comparison passing for the wrong reason. Also the shape of merging two drawing routines into one without losing either view's behaviour. Use whenever CONSTITUTION rule 7 demands "a way back that is exact at its off value" for something that DRAWS, and whenever a lane is about to pre-register "the new picture equals the old picture".
---

# NifSkope WW: the way back, proved by coverage

Repo `E:\Projects\NifskopeWildWastelandEdition`. Written from lane SKEL2
(2026-09-11), which merged the Pose Mode and Overlays armatures into one
renderer, changed the bone shape to Blender's solid octahedron and the bone
colour to the palette's blue -- three visible changes at once, all ordered by
bungo, under a rule that says every one of them needs an exact way back.

## 1. The trap: a pre-registered gate that cannot pass

The brief said *"Stick mode == the 07:06:04 framebuffer for the same bones and
camera"*. Two facts made that impossible before a line was written, and both
were readable in the source:

* **the colour ruling.** *"Just keep the color of the bones blue"* changes every
  one of the feature's pixels. No display mode can reproduce the old frame;
* **the shape that shipped was not the shape the brief named.** The overlay drew
  a 12-line WIREFRAME OCTAHEDRON, not segments, so "Stick" (Blender's plain
  line) is a third shape, not the old one.

**Read what the code actually draws before pre-registering a picture
comparison against it.** A gate that cannot pass is not a gate; it costs a build
to find out, and it is then re-registered under pressure, which is exactly when
a weaker gate slips in unnoticed.

When you must re-register, do it **in the report, before the first build**, with
the old text quoted, the reason it cannot hold, and the replacement's own floors
written out. Lane SKEL2's section 0.4 is the shape.

## 2. The replacement: coverage, not colour

The colour ruling licensed the COLOURS to move. It did not license the bones to
move, or get thicker, or multiply. So compare the set of pixels the feature
covers:

```
coverage = { p : ON(p) != OFF(p) }        # at one pinned camera
```

measured twice -- once on the rung, once on the new build in its way-back
mode -- and compared as SETS:

```
only-rung  = |coverage_rung  \ coverage_new|
only-new   = |coverage_new   \ coverage_rung|
```

Both must be under the jitter bar. `tests/spells/skeleton_overlay_coverage.py`
is the implementation; it takes four PNGs (rung off, rung on, new off, new on).

Lane SKEL2's numbers: rung 34,904 px covered, new 34,902, shared 34,902,
**only-rung 2, only-new 0** against a bar of 64 -- and the two ON renders
differed in 35,682 px, which is the colour change and nothing else.

### The three floors, and none of them is optional

1. **both coverages are non-empty** (`> 1000 px`). Two features that draw
   nothing cover the same nothing;
2. **the two ON renders DIFFER by more than the bar.** They are different
   colours; a zero here means the new picture IS the old picture, i.e. the
   binary was never rebuilt or the switch never reached the render;
3. **the NEW default mode, measured the same way, must FAIL.** If Octahedral
   passes the coverage test against the old wireframe, the comparison is not
   measuring shape at all. Run it and assert the failure.

### It catches what the other gates cannot

Lane SKEL2's first build passed 46 in-application checks including two written
specifically to catch a bone in the wrong place -- longest segment 31.94 of a
33.60 limit, zero endpoints outside the character's bounding box -- and the
coverage gate still went red at `only-new 782`. A refactor had widened the
armature-membership rule by nine nodes; every extra bone was short and ON the
character, so nothing else could see it. **One relink, and the census line in
the same log (`19 -> 10` marker-only) named the cause.**

## 3. Take the rung's renders BEFORE the link

They cannot be taken afterwards: the link overwrites `release/NifSkope.exe`, and
a `EXE=` override of the kept rung copy is an extra variable in the one
comparison that must have none. The order is:

```bash
cp -p release/NifSkope.exe release/NifSkope.before_<lane>.exe   # once, guarded
bash scratchpad/<lane>/rung_shots.sh          # off/on at the PINNED camera
... write the code, build ...
bash tests/spells/<the spell>.sh              # renders the new pair, compares
```

* the camera is PINNED (`WW_RENDER_CENTER` + `WW_RENDER_ORTHO`), never the
  auto-fit, or the two halves are two framings;
* **read the render size back** and refuse if the two pairs differ. The
  `WW_RENDER_SIZE` width floor moves with the build -- 1437, then 1293, then
  1524 across three builds of one week -- so the sizes are checked, never
  assumed;
* the rung's renders live under `scratchpad/<lane>/rung/` in the repo, not in
  `%TEMP%` (CONSTITUTION 8).

## 4. Merging two drawing routines into one

The other half of lane SKEL2, and the reason the way back was needed at all.

* **One routine, fed a list plus a style.** `drawArmature( QVector<Bone>,
  Style )`: the per-bone struct carries the STATE each caller knows (selected,
  active, hovered, pinned, marker-only, depth fade) and the style carries only
  what the two callers genuinely differ in. Nothing below the routine knows
  which view asked.
* **The per-view behaviour stays in the caller.** Relationship lines, pins, the
  weight overlay, the hover name: those are not drawing a bone, they are that
  view's own work, and moving them down is how a merge starts losing features.
* **KEEP THE DRAW ORDER.** An overlay drawn blended with the depth test off is
  order-dependent wherever two elements cross. The old code drew every body,
  then every stub, then every joint dot; the merged routine has to keep those
  three passes in that order or the way-back mode differs by hundreds of pixels
  at the crossings. Carry a `stub` flag on the bone for exactly this.
* **One element per drawn THING, not per shape.** A leaf bone appears twice (a
  body from its parent AND its own stub). Drawing the joint dot from both
  entries blends it twice and it is brighter than every other dot in the frame.
  Give the struct a `ball` flag and emit one ball per node.
* **The way back is a MODE, and it keeps the old constants.** Blender's own
  vocabulary had the name for the shipped shape (`Wire`) and for the new one
  (`Octahedral`); the old collar position (0.15 of the length) stayed on `Wire`
  while the new shape took Blender's 0.10. A way back that "looks the same" is
  not one.
* **One palette function for both views.** When the dock's rows and the
  viewport's bones are meant to agree, they call the same
  `<thing>KindColor( kind, state )` and a source gate counts its callers, so a
  third one cannot appear quietly.

## 5. The source gate that a picture cannot replace

Two renderers that happen to agree today are still two renderers. Gate it in the
spell, in awk, over the function's own body:

```bash
n=$(awk -v f="void GLView::$fn" '
    index($0, f) == 1 {on=1}
    on && /^\}/ {on=0}
    on && (/scene->drawLine\(/ || /drawOctahedralBone\(/ || /scene->drawPoints\(/) {c++}
    END {print c+0}' src/glview.cpp)
```

`index($0,f)==1` rather than `$0 ~ f`, so a mention in a comment does not arm
the scan; `/^\}/` is the closing brace at column 0, which a tab-indented body
cannot contain. Assert the count is 0 and that `drawArmature(` appears at least
once in the same span.
