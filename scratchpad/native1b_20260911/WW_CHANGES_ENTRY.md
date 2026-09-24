### `.lodo` + `.lodi` v3 — the cluster ladder, the bounds and cones, the occluder boxes (lane NATIVE1b, 2026-09-11)

bungo's performance list of 2026-09-11 08:0x, items **1** and **2**
(*"continuous detail, no pop at ring borders, triangles spent only where they
show"* and cluster bounds + normal cones for GPU culling) and the second round's
**"2 sounds good"** (*"a few boxes per cell for buildings and hills, baked from
the meshes"*) are in the bytes. `release/NifSkope.exe` **2026-09-11 10:14:23**,
**21,101,056 B** — one build plus two counted relinks.

**The ladder.** Every library mesh now carries a cluster hierarchy from FULL
detail down: level 0 is exactly what v2 emitted, and above it each material's
clusters are grouped four at a time (`meshopt_partitionClusters`, new to the
build: `lib/meshoptimizer/src/partition.cpp`), simplified together with every
border vertex LOCKED so a replaced group cannot crack against a neighbour, and
re-split under the same 16-triangle / 48-vertex caps. A new 48-byte parallel row
per cluster (`.lodo` header 0xC0) carries the bounding sphere, the normal cone,
`geometricError` measured against FULL detail, the parent link and the level.
Nine-chunk Sanctuary: **levels 0..7, 20,678 clusters (was 10,634), 1,900 of
2,982 meshes laddered**, 4,715 roots covering 142,138 full-detail triangles
exactly. `.lodo` **5,692,388 → 9,657,316 B** (+69.6 percent);
`--native-no-ladder` writes 6,204,388 B and is the exact way back.

**Bounds and cones.** Sphere and octahedral cone per cluster, and
`LodoCluster.flags` bit 2 = `CONE_OPEN` where the normals span more than a
hemisphere (14,604 of 20,678 clusters — a worldspace of trees and cut-out
fences). A real defect the gate found: both described the geometry that walked
IN rather than the quantised geometry written OUT, so the sphere missed its own
stored vertices by up to **0.058 u** and the cone excluded its own faces by
**0.002033**. Both are **0.000000** now; the writer computes them from the
dequantised positions and the contract states that as a format rule.

**Occluders.** Up to four oriented boxes a cell, each fitted inside a WATERTIGHT
LOD mesh on a 16³ voxel grid, shaved by a whole voxel on every side and probed
at a hundred interior points before it is written; two new `.lodi` tables at
header 0x98 and 0xA0. Downtown Boston (cells 0,−12..11,−1): **280 boxes, 87 of
147 populated cells**, every box holding all 100 points and every one leaking
when grown. **The Sanctuary region writes ZERO and that is correct** — it draws
41 distinct LOD meshes and none is watertight — so the box gates run on a second
small region rather than being loosened until Sanctuary passed.

**The selection law**, in the contract at 4.4 and in a reference selector:
`screenErrorPx = geometricError × scale × projectionScale / distance`, drawn
when its own error is under the tolerance and its parent's is over. Nine cases
(0.5 / 1 / 4 px × 2,000 / 8,000 / 32,000 u) are each a PARTITION of the surface;
triangles fall 124,205 → 104,090.

**THE FINDING, and it is bungo's call.** The ladder is correct and barely
selectable: the median level-1 cluster deviates by **3.80 percent of its model's
diagonal**, which reaches one screen pixel only past **52,100 units**, so at a
one-pixel tolerance the first step is not selected anywhere in the Commonwealth.
The cause is that "full detail" in this file is already Bethesda's LOD mesh — a
mean of 47.7 triangles for a whole building. Building the library from each
base's near `MODL` instead would serve his 10:3x near-field ruling and needs no
format change at all.

**The far-shadow rule at the ladder's own level** (his *"a far shadow cast by a
LOD tower behind me will cover the area I'm at"*): a simplified group whose
boundary-edge count exceeds that of the surface it replaces is REFUSED and its
clusters stay roots — **57 groups on this region**, two of which had taken a
watertight building to four and eight boundary edges.

Gates (`tests/spells/lodgen_native.sh`, now thirteen legs, **18 checks 0
failures PASS**): fixture decoder **69/0** (was 56/0), refusal set **44/0** (was
24/0, one mutation per new field refused by name), stock path **25 files 0
differ**, decoder on the real pair **87/0**, field gate **37/0 + 1 named skip**,
the new geometry gate **17/0** on the fixture and **15/0 + 1 skip** on the real
pair, both ways back, and the occluder region. New driver
`tests/spells/lodgen_native_cut.py`. Contract rewritten to v3 under
`ww-contract-provenance` (96 anchor rows, idempotent). Pictures
`scratchpad/native1b_20260911/images/{ladder,occluders}.png`.
