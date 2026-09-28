---
name: ww-lodi-drawn-mesh
description: Pick the mesh a .lodi placement actually DRAWS when an offline Python gate or refuter places .lodo geometry (poke tests, coverage, occluder checks, identity checks). The file does not store the placement's MNAM slot, so "the base's first rep" measures the wrong mesh on about 4 percent of bases. Covers the three exact sources (occluder row meshId, one-mesh bases, the emitter's group dump) and the one tie you may ignore. Use before trusting any failure an offline gate reports against placed .lodo triangles.
---

# The mesh a .lodi placement draws

Repo `E:\Projects\NifskopeWildWastelandEdition`. Worked example: lane IDENT1, 2026-09-28,
`tests/spells/lodi_occluder_building.py` (`drawn_mesh()`).

## The trap
The emitter draws `lib.bases[r.baseId].rep[r.mnamSlot]` (`src/nativeemit.cpp`). The `.lodi` instance
record does NOT carry `mnamSlot`; only the header's `slotInstances[4]` totals exist. A gate that takes the
base's first non-empty `rep0..rep3` measures rep0, while a highway or building piece may draw its
`_LOD_1` mesh in rep1/rep2. On the whole Commonwealth, 203 of 5,165 bases name more than one distinct mesh.
IDENT1's poke gate read 23 failing boxes, worst 0.88. Against the drawn meshes: 5 failing, worst 0.16.

## The exact sources, in order
1. **The occluder row's `meshId`** is the carrier's drawn mesh. Use it for the carrier, always.
2. **One-mesh base.** When every non-empty rep slot names the same mesh, that mesh is drawn. This is the
   precedent in `lodi_v7_refuters.one_mesh`.
3. **The emitter's group dump** (`WW_LODI_GROUP_DUMP=<file>`). Each `P` line prints the placement's level-0
   triangle count and its placed world box (lo xyz, hi xyz). Keep the rep whose level-0 triangle count
   matches (count clusters with `level == 0` only, the way the emitter does). If more than one is left,
   keep the rep whose placed box matches within 2 u.
4. **A tie between meshes with the same placed triangles** (one NIF filed in two slots, different mesh ids)
   is no tie for geometry: take either. Compare the sorted, rounded placed vertices.
5. **Anything else: skip it and COUNT it** in the output (`membersSkippedRepSlotsDisagree`). Skipping a wall
   raises a poke; it never hides one. Say so in the report.

## Self-check (must be printed)
For every occluder row, run rule 3 on the carrier and compare the result with the row's `meshId`. IDENT1
got 507 of 511 matching; the other 4 were same-geometry ties (rule 4). A large miss count means the dump
and the file come from different bakes.

## Red controls
Keep the gate's floor (the same boxes grown 1.25x must fail; 510 of 511 did). Also run `--inflate 1.02`
(270 of 511 over) so you know the fix did not blunt the gate.
