# IDENT1 deliverable text (director splice)

## HANDOFF text
IDENT1 (2026-09-27/28): landed on branch ident1-20260927, not merged. What it adds:
- Object groups ("identity") now come from a contact join: pieces whose placed LOD triangles come within 32 u
  join, and no group may grow wider than 4,096 u.
- One occluder box per building, checked against the building's own triangles.
- Knobs: contact tolerance 32 u, group cap 4,096 u (GroupKnobs, src/nativeemit.cpp). Both numbers come from a
  whole-Commonwealth sweep; the reasons are in docs section 4.9.
- Off bake (`--identity-join proximity --occluder-fit piece`) matches the 2026-09-27 rung file for file. The only
  differences are the .key files and one .lodb plugin line, from an esp that changed on disk after the rung.

Gates on the after bake:
- Every placement has a group (0 bad roots), and no joined group is over the cap.
- Hub towers: east 1 group, west 4 groups. Trinity: 1 group.
- Street coverage went from 0.029 to 0.560.
- FAIL, Diamond City: 15 groups. No cap that stops the welding keeps the stadium whole.
- FAIL, row houses: 1 group, not the 3 or more wanted. The terraces share walls, so any contact rule joins them.
- FAIL, poke gate: 5 of 511 boxes are more than 1% outside their building. 3 of those 5 are a lattice plane lying
  on a joint between two wall pieces.

Pictures: scratchpad/ident1_20260927/pics/ (ident_before/after, occ_top_before/after, boxes_before/after).

Owed:
- (a) File-wide ids need a u32 group word, which is a format bump.
- (b) Diamond City and the row houses need a landmark/precombine join rule. Distance alone cannot do it.
- (c) Hill boxes need a format bump.
- (d) The 5 boxes that poke out.
- (e) The .lodo "unknown header flag bit" mutation in lodgen_native.sh leg 3 is refused without naming it. This
  predates the lane; no IDENT1 commit touches src/lodofile.*.

## WW_CHANGES text
Object groups and occluder boxes (lane IDENT1, 2026-09-28):
- Native emitter: a new default identity rule, the contact join (`--identity-join contact`):
  - SCOL parts first.
  - Then every non-tree drawn piece joins pieces whose placed level-0 triangles come within 32 u, nearest pair
    first.
  - A join that would make the group wider than 4,096 u is refused.
  - `--identity-join proximity` gives back the 2026-09-27 files byte for byte.
- Native emitter: one occluder box per building (`--occluder-fit building`, default). The box is fitted in the
  building's yaw frame, then probed against its triangles on a 9x9x9 lattice. It shrinks half a voxel a face while
  more than 0.5% is out, or the building gets no box. `--occluder-fit piece` is the way back.
- Whole Commonwealth: 21,140 groups and 511 boxes. Street coverage went from 0.029 to 0.560.
- Measuring surface: WW_LODI_CONTACT_TOL, WW_LODI_GROUP_CAP, WW_LODI_GROUP_DUMP, WW_LODI_OCC_RAYS.
- Viewer: the `identityraw` channel is renamed `placement-lowbyte`. The captions now say what each channel draws.
- tests/spells/lodi_occluder_building.py: a new gate. Every building box must be at most 1% outside its building,
  and the same boxes grown 1.25x must leave it.

## MISTAKES text
- 2026-09-28 IDENT1: the building poke gate measured the wrong mesh. It took a base's first non-empty rep slot, but
  a placement draws rep[mnamSlot], and the .lodi does not store that slot.
  - Effect: highway `_LOD_1` boxes poked 0.87 against rep0 and 0.000 against the drawn mesh. The gate reported 23
    failures where 5 were real.
  - Fix: use the row's meshId for the carrier. For other members, use the rep whose level-0 triangle count and
    placed box match the emitter's group dump.
  - Rule: a refuter that places geometry must name the exact mesh drawn, or count what it skipped.
- 2026-09-28 IDENT1: the first before-picture label said "2 u proximity join". The proximity join's gap is 64 u
  (src/nifcli.cpp lgIdentityJoinGap); 2 u was the contact join's old tolerance. Relabelled.
  - Rule: read a default from the code before printing it.
- 2026-09-28 IDENT1: the WW_LODI_BOXES wire-box shots show nothing. A box that fits its building sits inside the
  walls: before vs after differ in 247 pixels.
  - The top-down offline pictures (occ_topdown.py) are the box pictures.
