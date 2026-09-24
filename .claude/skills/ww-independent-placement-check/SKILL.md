---
name: ww-independent-placement-check
description: Gate a pipeline that PLACES Fallout 4 geometry in the world -- a cell view, a LOD chunk bake, a precombine, an impostor grid -- by recomputing every placement's world bounding box from the plugin and the NIF in an independent reader. Covers the five reader defects that every one of them disguises as "the rotation convention is wrong", why a box can never be rotated, and the three ways the checker's idea of the scene ends up narrower than the code's. Use before believing any disagreement between such a checker and the code it checks.
---

# The independent placement check, and why it lies to you first

Repo `E:\Projects\NifskopeWildWastelandEdition`. Worked example:
`tests/spells/cell_open_check.py` against `src/cellview.cpp`, lane CELLVIEW1,
2026-09-19. The same shape applies to `lodgen`'s chunk placements, to a
precombine, and to anything else that turns a REFR into world-space triangles.

## The gate

The placing code writes down what it placed -- form id, base, record type,
part index, position, the euler it was GIVEN, scale, and the placement's world
AABB. (Welded geometry cannot be asked how many references it drew, so the
builder has to say so: `WW_CELL_DUMP`.) A second reader, sharing no code, walks
the same plugin and the same NIFs and recomputes all of it.

Three checks, and the third is the only one that depends on the ROTATION and
the SCALE rather than merely on the parse:

1. **The set, both ways round.** Invented is never forgiven. Missing is
   forgiven only for a reason the checker VERIFIES.
2. **The position** of every non-collection reference against the REFR's own
   DATA field, to 0.05 units.
3. **The world box**, recomputed from the model's own vertices.

Plus `--red`: recompute (3) under the WRONG euler convention. The gate must
then report that the boxes MOVED and name references. **Run the red control
only on a green checker** -- on a broken one it "passes" for the wrong reason,
every time.

## The five defects, in the order they will bite

Every one of them presents as *"N of M world boxes disagree with the engine
euler convention, and only the rotated ones"*. That sentence is a diagnosis and
it is almost always wrong. Before touching any rotation code, read
`Matrix::fromEuler` out of `src/data/niftypes.cpp:215` and compare it element
for element with yours; it will match, and then work down this list.

1. **FO4 BSTriShape positions are HALF floats.** `<eee`, not `<fff`, unless
   vertexDesc bit 54 (`VF_FULL_PRECISION = 1 << 54`) is set; stride is
   `(vdesc & 0xF) * 4`. Read wrong they come back as 1e14, not as slightly
   wrong numbers. **The MAGNITUDE is the tell**: a disagreement of
   3,986,846,973,645 units is not a convention, it is a reader.
2. **The shape's own local transform.** Translation, rotation, scale sit
   between the block header and the geometry. Cost of skipping: 16-170 units.
3. **The NiNode chain above the shape.** The loader composes it
   (`lodgenWorldTransform`, `src/lodgen.cpp:1444`); a checker that reads only
   the shape is off by the chain. Compose exactly as
   `Transform::operator*` does -- `t.translation = t1.translation + t1.rotation
   * t2.translation * t1.scale` -- and apply as `rotation * v * scale +
   translation`.
4. **A BOX CANNOT BE ROTATED.** Rotating the eight corners of a model-space
   AABB gives the AABB *of the rotated AABB*: a strict superset, exactly equal
   when the rotation is zero and tens of units too large the moment it is not.
   That is the whole "only the rotated ones fail" pattern. Cache the model's
   model-space POINTS (an `array('f')` is cheap enough -- 1578 downtown
   placements re-transform in about 3 s) and take min/max after transforming
   them, the way the placing code does.
5. **`tname.endswith('Node')` is not "is a node".** `BSFurnitureMarkerNode` is
   an `NiExtraData`. Parsed as a node it read a furniture heading of pi as a
   scale of -0.0, claimed the root block as its child, and collapsed all 921
   vertices of one chair onto a point. Use an explicit list --
   `NiNode, BSFadeNode, BSOrderedNode, BSMultiBoundNode, NiBillboardNode,
   BSLeafAnimNode, BSValueNode, BSTreeNode, NiSwitchNode, BSBlastNode,
   BSDebrisNode, NiSortAdjustNode, BSRangeNode, BSMasterParticleSystem` -- plus
   the guard that a child block index always exceeds its parent's.

## Then the checker turns out to be narrower than the code

Once the boxes match, the SET check starts reporting. Each of these was the
checker, and each had to be measured rather than argued:

* **Base record types.** `src/esmdata.cpp` reads `MODL` out of ANY base record,
  so a placement pipeline draws MISC pool balls and a TERM terminal, not just
  the ten types a brief listed. A budget census may legitimately be narrower;
  the CHECK must not be. Pass the wider type set in rather than editing the
  census's own pre-registered numbers.
* **The worldspace PERSISTENT cell.** It is the CELL whose parent GRUP is the
  world-children group (type 1) itself, not a block or subblock
  (`src/esmdata.cpp:141`). Its references have no grid of their own and are
  filed by POSITION into `[x0*4096, (x1+1)*4096)`. Miss it and every one of
  them is "invented".
* **Markers, and models with no drawable geometry.** Two different reasons and
  both must be verified, not assumed: the marker rule
  (`src/cellview.cpp:125-132`, mirror it exactly), and models such as
  `StaticCollectionPivotDummy` or `InvisibleGeneric01` whose only shapes sit
  under an `EditorMarker` node the loader drops -- they parse to nothing. "The
  model is in the data tree" is a different question from "the scene had
  anything to draw".

## Collections (SCOL) rotate twice

A part's world rotation is `rm * pm`: the reference's euler and the part's own
euler inside the collection (`src/cellview.cpp:622`), and its scale is
`refScale * partScale`. A dump row can only carry one euler triple and carries
the reference's, so the checker must read the part eulers back out of the
SCOL's ONAM/DATA (7 floats per placement: pos, rot, scale) and compose. Match
the part index to the plugin BY MODEL NAME and say so out loud when it does not
line up, rather than trusting two walks to number parts identically.

Two more things a collection's dump row legitimately carries: the PART's base
form and the PART's record type (a `TreeCluster05` part is a `STAT`). Comparing
those against the collection's own is how a named-reference row reports
`WRONG SCOL ... base 0x0004A075, expected 0x0003581A` while the expansion is
working perfectly. Check a collection by its ROW COUNT instead, and pre-register
that count -- noting that the part INDEX counts every part while only the ones
with geometry produce a row.

## The rule

**When an independent checker disagrees with the code it is checking, the
checker is the prime suspect**, and the SHAPE of the disagreement is evidence
about the reader before it is evidence about the maths. Write that reasoning
into the checker's own comments beside each fix, because the next person to see
"only the rotated ones fail" will reach for the rotation code too.
