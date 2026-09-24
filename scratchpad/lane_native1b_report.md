# Lane NATIVE1b report -- the GPU-driven data: the cluster ladder, bounds and cones, occluder boxes (`.lodo`/`.lodi` v3)

Repo `E:\Projects\NifskopeWildWastelandEdition`, `main`, **nothing committed** (CONSTITUTION 8).
Written incrementally (CONSTITUTION 1, 1b).

Launch state, measured, before a line was read:

| thing | reading |
|---|---|
| `release/NifSkope.exe` at launch | 2026-09-11 **08:49:08**, **20,989,440 B**, md5 `cf7fc96177afec212bb63a90c08c88c4` (NATIVE1a) |
| rollback rung `release/NifSkope.before_native1b.exe` | written ONCE 09:18:34, **same md5**, same size -- never overwritten |
| other lanes alive | `ls scratchpad/*/BUILDING` -> **nothing** (rc=2). No other lane is alive. |
| `tasklist \| grep -i -E "Fallout4\|NifSkope"` | **game down, no NifSkope running** |

---

## 0. Pre-registered gates

Registered BEFORE any code was written or any number measured on a changed tree
(CONSTITUTION 1). Baselines are the rung exe's own readings, in section 4 and 5.

| gate | what it asserts | predicted / baseline |
|---|---|---|
| **B1 ladder** | every library mesh has at least one level above 0 unless its level-0 triangle count is <= 16 **or the simplifier could not remove a single triangle from any of its groups** (see the audit below; each class counted AND its table printed); every cluster's `geometricError` <= its `parentError` (monotone), and a doctored child with a larger error than its parent is REFUSED BY NAME | baseline: no ladder exists (v2 has one level) |
| **B2 bounds / cones** | every triangle of every cluster lies inside that cluster's stored sphere (+1e-3 slack), and every face normal lies inside its stored cone -- or the cone is flagged OPEN. FLOORS, both shown red: a sphere shrunk by 10 percent and a cone narrowed by 5 degrees | newly registered |
| **B3 occluders** | for every written box, 100 pseudo-random points inside it are inside the source mesh (ray parity); FLOOR: the same box grown by 10 percent is shown red. Census prints boxes per cell and the fraction of cells with none | newly registered |
| **B4 the reference cut** | at 3 tolerances (0.5, 1, 4 px) x 3 distances the cut is a PARTITION of the surface: every level-0 cluster has exactly one ancestor-or-self in the cut; triangle counts printed per case. FLOORS: tolerance 0 selects exactly the level-0 clusters, a huge tolerance selects exactly the root set | newly registered |
| **B5 standalone gate** | `ww-standalone-writer-gate` before and after: fixture decoder, two-write identity, one mutation per NEW field/rule refused BY NAME | before: fixture decoder **56 / 0**, refusal set **24 / 0** (NATIVE1a) |
| **B6 stock path** | the region bake with `--native` off is byte-identical over every output file | 25 files, 0 differ |
| **B7 build hygiene** | exe newer than every changed file; every gate DRIVER rebuilt and `-nt`-checked; rung == the 08:49:08 bytes | n/a |
| **B8 process** | no NifSkope left running; game down at every launch | n/a |

Suite baselines to hold (NATIVE1a's readings): `lodgen_native.sh` 13/0,
decoder on the real pair 87/0, the field gate 25/0,
`lodgen_native_baseline.sh --check` 25/0, `lodgen_terrain.sh` 26/0,
`lodl_open.sh` 23/0, `ui_align.sh` 11/0, `animws.sh` 72/0 + 1 skip,
`water_ui.sh` 86/0.


### The gate-number audit (`ww-spec-gate-audit`), run before a line was written

Three numbers this lane is told to reproduce were **re-measured on the rung exe**
rather than read out of NATIVE1a's report:

| number as briefed | re-measured here | verdict |
|---|---|---|
| fixture decoder 56 / 0 | **56 checks, 0 failures, PASS** | holds |
| refusal set 24 / 0 | **24 checks, 0 failures, PASS** | holds |
| `.lodo` 5,692,388 B, `.lodi` 126,512 B | the kept pair under `scratchpad/native1a_20260911/gate/native/Native/` is exactly those two sizes | holds |

Logs `scratchpad/native1b_20260911/logs/before_{fixture,decode,mutate}.log`;
the fixture itself is `scratchpad/native1b_20260911/gate/before/fx/`
(`Synthetic.lodo` 28,903 B, `Synthetic.lodi` 16,424 B, `Synthetic.expect.txt` 2,175 B),
and a second write of it is byte-identical.

**One gate as briefed is an over-claim, and it is corrected above rather than
reproduced.** The brief's B1 reads *"every mesh has >= 1 level above 0 unless it
is already <= 16 tris"*. The refuter is a mesh of, say, twenty triangles that is
ten disconnected quads: an edge collapse cannot join two components, so no
simplifier can remove a triangle from it without deleting a whole component --
which is not a simplification, it is a hole, and bungo's far-shadow constraint
("a far shadow cast by a LOD tower behind me will cover the area I'm at")
forbids it outright. The honest form of the gate keeps the claim and adds the
refusal class BY NAME with its own count and table, so a mesh that cannot ladder
says why instead of being silently absent. The measured split is in section 1.

---

## 1. The ladder

### The grouping rule, stated

Level 0 is **exactly what NATIVE1a emitted**: the same per-shape greedy walk,
the same 16-triangle / 48-vertex caps, the same flush between shapes, the same
attributes straight off the source shape. Nothing about the full-detail
geometry changed. Above it, per **(mesh, material)** and never across a
material -- two materials are two textures and a collapse across them drags one
texture's geometry onto the other's:

1. **weld** one entry per distinct QUANTISED library position over the
   material's shapes (an edge collapse cannot cross a split vertex);
2. **group** with `meshopt_partitionClusters`, target **4 clusters** a group;
3. **lock** every welded vertex a triangle outside the group still uses -- this
   is the whole reason a ladder is built on groups and not on clusters, because
   a consumer that replaced this group and kept its neighbour would otherwise
   see a crack along the shared edge;
4. **simplify** to half the triangles (`meshopt_simplifyWithAttributes`);
5. **measure** (below), **refuse** or **form**;
6. **re-split** under the same caps at `level + 1`, and link.

`lib/meshoptimizer/src/partition.cpp` had never been in `NifSkope.pro`; it is
now, which is why `qmake` ran before `make`.

### The error definition, and which rule measured each one

`geometricError` is the deviation from **FULL DETAIL**, never from the parent --
that is what lets a consumer compare ONE stored number against ONE tolerance
without walking the chain. Two rules, and the file counts how many rows each
served:

| rule | when | measured |
|---|---|---|
| **exact** -- the two-sided vertex-sampled Hausdorff distance between the group's simplified triangles and the full-detail triangles under it | the subtree covers <= 512 full-detail triangles | **6,973 groups** |
| **chain-bounded** -- `child error + this step's deviation`, an upper bound by the triangle inequality | above that | **4 groups** |

Then `E = max(E, max child error)`, and a group is **not formed unless
`E > max child error`**. Every chain's errors therefore strictly increase, which
is what makes "tolerance 0 selects level 0 and nothing else" true rather than
hopeful -- and the gate shows both floors live.

### Level statistics on the real pair (nine-chunk Sanctuary, the whole library)

| level | clusters | triangles | mean `geometricError`, world units at scale 1 |
|---|---|---|---|
| 0 | 10,634 | 142,138 | 0.0000 |
| 1 | 5,676 | 68,448 | 165.62 |
| 2 | 2,781 | 29,699 | 497.16 |
| 3 | 1,029 | 8,750 | 726.31 |
| 4 | 414 | 2,503 | 921.86 |
| 5 | 123 | 626 | 1,192.52 |
| 6 | 19 | 98 | 1,527.20 |
| 7 | 2 | 6 | 3,328.47 |

**1,900 of 2,982 meshes have a level above 0.** Of the 1,082 that do not,
**1,065 are 16 triangles or fewer** (one cluster, nothing to group) and the
other 17 named their refusal. **4,715 root clusters cover 142,138 full-detail
triangles**, which is the library's level-0 triangle count exactly: the ladder
is a partition of its own surface, checked per mesh by `--native-verify` and by
the reference selector at every tolerance.

**The four refusals, each counted by its own name:**

| refusal | groups |
|---|---|
| the group holds 4 triangles or fewer | 2,476 |
| the simplifier removed no triangle | 34 |
| the error would not grow | 742 |
| **the simplified group had MORE boundary edges than the surface it replaced** | **57** |
| formed | 6,235 |

### B1, as pre-registered

**Green.** Every mesh above 16 triangles either laddered or named its refusal:
1,900 laddered, 1,065 are <= 16 triangles, 17 could not be cut and say why
(`lodgen_native_fields.py` h8). Monotonicity is a REFUSAL and it was exercised:
mutation `v3 lodo a child deviates more than its parent` is refused naming
`monotone`, and the field gate's h4b shows the test is not vacuous --
**15,963 of 15,963** non-root rows have a strictly larger parent.

### THE FINDING, and it is bungo's call

The ladder is correct and it is **barely selectable**:

* the median level-1 cluster deviates by **3.80 percent of its model's own
  diagonal** -- 38 units on a 1,000-unit building, which reaches one screen
  pixel only past **52,100 units**;
* over the 1,900 laddered meshes the deepest level's error is a median
  **26.9 percent** of the model diagonal, p90 **52.8 percent**;
* the worst are objects that are a handful of triangles already:
  `HitExtStrutureMassFusion02_LOD.nif` is 54 triangles for a 7,093-unit
  building, and one halving costs 98 percent of its diagonal.

**The cause is not the ladder. "Full detail" in this file is already Bethesda's
LOD mesh** -- a mean of 47.7 triangles for a whole building, with almost nothing
left to remove. The ladder would have room, and his 10:3x ruling ("the cluster
hierarchy must include the NEAR levels too") would be served, if the library
were built from each base's own near `MODL` instead of its `MNAM` slots. That is
a bigger library and **his decision**; nothing here blocks it, because the
format, the writer and every gate are indifferent to what level 0 was built from.

---

## 2. Bounds, cones, occluders

### Where they live

| what | encoding | where |
|---|---|---|
| bounding sphere | `f32 centre[3]` + `f32 radius`, mesh-local, never 0 | the new **48-byte `LodoClusterLod`** row, one per cluster, at `.lodo` header **0xC0**; the stride is at 0xC8, `levelMax` at 0xCC, `ladderGroup` at 0xCD, and the pad now starts at 0xCE |
| normal cone | `u16[2]` octahedral 16:16 axis + `f32 coneCos`, or **`LODO_CLUSTER_CONE_OPEN`** = `LodoCluster.flags` bit 2 with axis (0,0) and cosine -1 | the same row, plus the bit the v2 contract's section 12 had reserved |
| the ladder | `geometricError`, `parentError`, `parentFirst`, `parentCount`, `level`, `sourceTriangles` | the same row |
| occluder boxes | **40-byte `LodiOccluder`** (world centre, half extents, the instance's rotation in the same smallest-three codec, flags, instanceIndex, meshId) + an 8-byte per-cell range blob parallel to the cell ranges | `.lodi` header **0x98** and **0xA0**, count at 0xA8, stride 0xAC, per-cell cap 0xAE; the pad starts at 0xB0 |

A parallel table, not a wider cluster row: the 16-byte cluster row is what the
index-fetch path reads per DRAWN cluster while these fields are read per
CANDIDATE cluster by the cull, and the v2 row had four bytes free against the
thirty-two this needs.

### A REAL DEFECT the gate found, and it cost the first of two relinks

**The sphere and the cone described the geometry that walked IN, not the
geometry that was written OUT.** The library keeps positions as u16 into the
mesh AABB, so a consumer's triangle is up to half a quantum away on every axis.
Measured on the region, before the fix:

| reading | before | after |
|---|---|---|
| worst sphere overshoot, over 4,136 sampled clusters | **0.057992 u** | **0.000000 u** |
| worst cone cosine deficit, over 1,201 coned clusters | **0.002033** | **0.000000** |

The fix is a format-level rule, now in the contract: **a v3 writer computes the
sphere and the cone from the DEQUANTISED positions**, so the description and the
data are the same numbers. The cone's cosine is additionally measured against
the axis *as the reader decodes it*, so the axis's own quantisation is already
inside the stored value.

### B2, as pre-registered: both floors red in the same run

| check | reading |
|---|---|
| every triangle inside its cluster sphere (+1e-3) | **worst overshoot 0.000000 u** over 4,136 clusters |
| FLOOR, a sphere shrunk by 10 percent | **RED**, worst overshoot **502.56 u** |
| every face normal inside its cone, or the cluster is CONE_OPEN | **worst cosine deficit 0.000000** over 1,201 coned clusters (2,935 open in the sample; **14,604 of 20,678** in the file) |
| FLOOR, a cone tightened past its own worst face | **RED**, deficit 0.001000 |

**The cone floor as the brief wrote it could not fire and was corrected.**
"Narrow by 5 degrees" does nothing to a one-triangle cluster, whose cone is a
point: the first run read a deficit of exactly 0.000000 and would have passed
for the wrong reason. The floor now tightens the cosine a thousandth past the
cluster's OWN worst face, which excludes that face at every cone width.

### Occluders: the fit, and its named refusals

A box is only an occluder because it sits inside a particular object, so every
constant is shaped by one rule: **a box that sticks out hides things wrongly,
which is worse than no occluder.**

| step | refused, over the 2,982 library meshes |
|---|---|
| watertight only (`boundarySrc == 0`; ray parity means nothing on an open mesh) | 2,617 |
| AABB diagonal >= 256 u at scale 1 | 100 |
| a largest all-interior box on a 16^3 voxel grid, one ray a (y, z) row | 123 (no interior voxel) |
| a whole voxel shaved off every side, then >= 2 percent of the AABB volume | 43 (too thin) |
| **100 points inside the box are inside the MESH** -- the writer's own gate | 0 |
| | **99 fitted** |

Per cell the largest four by world volume survive, ties broken on the instance
index (not decoration: it is what keeps two writes byte-identical).

### B3, and why it needed a second region

**The nine-chunk Sanctuary region writes ZERO boxes, and that is correct.** It
draws **41 distinct LOD meshes and not one of them is watertight**; the library's
365 watertight meshes are elsewhere in the Commonwealth. Rather than loosen a
rule until this region passed, the gate bakes a **second small region** -- cells
(0,-12)..(11,-1), downtown Boston -- and asks the box questions there:

| reading | number |
|---|---|
| boxes written | **280** |
| offered by instances | 599 |
| dropped by the four-a-cell cap | 319 |
| cells with at least one | **87 of 147 populated (59.2 percent)** |
| every box holds all 100 interior points | **280 of 280** |
| FLOOR, a grown box leaks | **280 of 280**, worst growth factor needed **1.5** |

On Sanctuary both box groups are a **NAMED SKIP**, never a pass -- in the field
gate (`i1..i4`) and in the geometry gate (`D`).

**The box floor as the brief wrote it could not fire either.** "Grown by 10
percent" stays inside a big object, because the fitter already shaves a whole
voxel off every side; the first run read `0 of 1 grown boxes leak` on the
fixture. The floor now grows the box until it leaks (1.1, 1.25, 1.5, 2.0) and
reports the factor, failing only if doubling it stays inside.

---

## 3. The selection law and the reference cut

Written into the contract at 4.4, and implemented as a reference selector in
`tests/spells/lodgen_native_cut.py`, which shares no code with the writers:

```
screenErrorPx = geometricError x scale x projectionScale / distance

draw the cluster when   screenErrorPx <= tolerance
                 AND    parentError x scale x projectionScale / distance > tolerance
```

`parentError` is `FLT_MAX` at a root, so a chain whose every error is under the
tolerance terminates at its root rather than selecting nothing. `projectionScale`
is the contract's REFERENCE constant `960/tan(35 deg) = 1371.0` and a consumer
recomputes it from the live projection. The default global tolerance is **1 px**
(bungo 10:4x). **The shadow view is a second, coarser selection of the same
data** with its own tolerance; nothing in the file is per-view.

### The six-plus-three cases, on the real pair (3,526 instances, camera at the cloud's centre)

| tolerance | 2,000 u | 8,000 u | 32,000 u |
|---|---|---|---|
| 0.5 px | 124,205 | 124,181 | 121,898 |
| 1 px | 124,205 | 124,121 | 115,583 |
| 4 px | 124,121 | 115,583 | 104,090 |

(triangles; clusters were 10,429 / 10,429 / 10,405, 10,429 / 10,429 / 10,116,
10,429 / 10,116 / 9,246.)

**Every one of the nine is a PARTITION**: walking up from every level-0 cluster
of every drawn mesh, exactly one group of the chain is in the cut -- checked per
mesh, per instance, at every case. Level 0 alone is 142,138 triangles over the
library, of which these instances reach 124,205, so **the cut saves between 0
and 16 percent on this region** -- which is section 1's finding expressed as
triangles rather than as a percentage of a diagonal.

**Both floors, in the same run:** tolerance 0 selects exactly the level-0
clusters, and a tolerance of 1e12 selects exactly the roots.

---

## 4. Standalone gate before and after

`ww-standalone-writer-gate`, through the built exe (the writers are in it), on
the rung exe first and the new one after:

| leg | before (rung, 08:49:08) | after (10:14:23) |
|---|---|---|
| the fixture decoder against answers written BEFORE the bytes | **56 checks, 0 failures** | **69 checks, 0 failures** |
| two writes byte-identical | identical | identical |
| the refusal set, one mutation per row rule and per new field, every CRC re-signed so the RULE answers | **24 checks, 0 failures** | **44 checks, 0 failures** |
| `Synthetic.lodo` / `.lodi` / `.expect.txt` | 28,903 / 16,424 / 2,175 B | **32,999 / 24,960 / 2,720 B** |

**The thirteen new expect keys** (56 -> 69): the level-0 counts split out from
the whole-file ones (`lodo.level0.clusterCount`, `.vertices`, `.triangles`), the
ladder's prediction and its partition (`lodo.levelMaxAtLeast`,
`lodo.rootSourceTriangles`), the mesh-relative cluster keys that survive the
ladder's reindexing (`lodo.mesh1.l0cluster0/1.*`), **the cone that must open**
(`lodo.mesh0.l0cluster0.coneOpen` = 1 for the closed cube, 0 for the flat strip),
the two hand-derived spheres, and the six occluder keys.

**The twenty new mutations** (24 -> 44), each refused BY NAME: `version 2` in
both files, `clusterLodStride`, `levelMax`, `ladderGroup`, the ladder row's
reserved field, a level-0 cluster given an error, `sourceTriangles`, a zero
sphere radius, `clusterCountL0`, `levelCount`, CONE_OPEN cleared on a cluster
with no cone, CONE_OPEN set with an axis stored, **a child that deviates more
than its parent** (refused naming `monotone`), `occluderStride`,
`maxOccludersPerCell`, the occluder's reserved word, its flag bit 0, a
non-positive half extent, and a box naming an instance outside its own cell.
The two v2 reserved-byte cases moved with the header room (0xC0 -> 0xCE,
0x98 -> 0xB0) and the unknown-flag case moved off bit 3, which is now LADDER.

**What the fixture no longer claims, and why.** Its v2 known answers included
`lodo.clusterCount 3`, `vertexCount 8+18+6` and `triangles 32`. With the ladder
on in the same fixture all three describe a SIMPLIFIER'S output, which no hand
derives; keeping them would have meant either turning the ladder off in the
known-answer control or copying the writer's own numbers into the expectations.
The level-0 counts stay hand-derived, the ladder is checked by invariants that
hold whatever the simplifier does, and one number is stated as a PREDICTION made
before the run (`lodo.levelMaxAtLeast 1`). **A fixture may predict; it may never
copy.**

---

## 5. Build and gates

### Mtimes and identity, in one table

| artefact | time | size / hash |
|---|---|---|
| rung `release/NifSkope.before_native1b.exe` | written once **09:18:34** | 20,989,440 B, md5 `cf7fc96177afec212bb63a90c08c88c4` -- **the 08:49:08 launch bytes exactly** |
| `qmake` (the `.pro` gained `lib/meshoptimizer/src/partition.cpp`) | **09:55:36**, `QMAKE-RC=0`, `Makefile.Release` names it 7 times | -- |
| **the build** (one) | **09:56:11** | 21,100,544 B, `BUILD-RC=0` |
| counted relink 1 -- the sphere and the cone computed from the DEQUANTISED positions | **10:04:40** | 21,101,056 B |
| counted relink 2 -- the ladder's own silhouette refusal | **10:14:23** | 21,101,056 B, sha256 `ab97d12c511aa5...` |
| sources newer than the exe, over `src` and `NifSkope.pro` | **NONE** | -- |
| objects vs the headers they include | **0 STALE** over `lodofile.h`, `lodifile.h`, `nativeemit.h` | -- |
| `res/style.qss` vs `release/style.qss` | in step (the chain's own `cmp`) | -- |
| the new switches actually in the exe | `--native-no-ladder`, `--native-no-occluders`, `native-ladder:`, `native-occluders:` all present (UTF-16 for the `QLatin1String` compare, ASCII for the usage text) | -- |

**Gate DRIVERS.** Every driver this lane runs is a Python script read at run
time (`lodgen_native_decode.py`, `_mutate.py`, `_fields.py`, the new `_cut.py`),
not a compiled binary; `grep -n "release/[a-z_]*\.exe"` over them names only
`release/NifSkope.exe`, which the exe-newer rule already covers. No standalone
`release/*.exe` driver is used. The two pictures are drawn by
`scratchpad/native1b_20260911/pictures.py`, also read at run time.

### The gate table

| gate | this lane | baseline | note |
|---|---|---|---|
| `tests/spells/lodgen_native.sh` (now **thirteen** legs) | **18 checks, 0 failures, PASS** | 13 / 0 | +5 legs, named below |
| -- leg 1, the decoder on the fixture | **69 / 0** | **56 / 0** | +13 v3 expect keys (section 4) |
| -- leg 2, two writes byte-identical | identical | identical | |
| -- leg 3, the refusal set | **44 / 0**, each refused BY NAME | **24 / 0** | +20 v3 mutations (section 4) |
| -- leg 4, the real region bake | written, 9,657,316 + 128,256 B | 5,692,388 + 126,512 B | the ladder's price, section 1 |
| -- leg 5, the stock path with `--native` off | **25 files, 0 differ** | 25 / 0 | |
| -- leg 6, the decoder on the real pair | **87 / 0** | 87 / 0 | unchanged, as predicted |
| -- leg 7, `--native-verify --native-verify-corpus` | **rc=0** | rc=0 | now also states the ladder is a partition per mesh |
| -- leg 8, the field gate | **37 / 0, 1 skip** | 25 / 0 | +12: section h (the ladder, 11 checks) and section i (the boxes, skipped on this region) |
| -- leg 9, the staleness floor | refused, naming the file and the field | same | |
| -- **leg 10 (NEW), the geometry gate on the fixture** | **17 / 0, 0 skips** | newly registered | spheres, cones, the cut, the box, every floor red |
| -- **leg 11 (NEW), the geometry gate on the real pair** | **15 / 0, 1 skip** | newly registered | the box group is a NAMED SKIP here |
| -- **leg 12 (NEW), the two exact ways back** | `--native-no-ladder` writes one level, flag clear, `levelMax` 0; `--native-no-occluders` writes 0 boxes | newly registered | |
| -- **leg 13 (NEW), the occluders on a second small region** | **280 boxes, 280/280 inside, 280/280 leak when grown** | newly registered | Sanctuary has no watertight mesh |
| `lodgen_native_baseline.sh --check` | **25 in the baseline, 25 baked, 0 differ, PASS** | 25 / 0 | against the 2026-09-10 03:57:46 exe |
| `lodgen_terrain.sh` | **26 / 0, PASS** | 26 / 0 | |
| `lodgen_identity.sh` | **PASS** | PASS | |
| `lodgen_merge.sh` | **PASS** | PASS | |
| `lodl_write.sh` | **PASS** | PASS | |
| `lodl_open.sh` | **23 / 0** | 23 / 0 | |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 | |
| `animws.sh` | **72 / 0, 1 skip, PASS** | 72 / 0, 1 skip | |
| `water_ui.sh` | **82 / 0, 0 skips, PASS** | 86 / 0 | **not a drop**: 86 is the count WITH the four `SHOT=` / `TABSHOT=` / `STRIPSHOT=` / `LODSHOT=` picture arguments, which arm four extra checks; 82 is the same suite asked without them, exactly as NATIVE1a recorded |

**Every count that moved, by name.** Five, and no more:

1. `lodgen_native.sh` **13 -> 18**: legs 10, 11, 12 and 13, plus leg 13's second
   check. Nothing was removed.
2. the fixture decoder **56 -> 69**: the thirteen v3 expect keys.
3. the mutation set **24 -> 44**: twenty v3 mutations; two v2 reserved-byte
   cases moved offset with the header room and the unknown-flag case moved off
   bit 3 (now LADDER). No case was deleted.
4. the field gate **25 -> 37 + 1 skip**: section h's eleven ladder checks and
   section i's four box checks, the latter a NAMED SKIP on a region with no box.
5. `water_ui.sh` **86 -> 82**, and it is an ARGUMENT and not a regression: the
   four picture arguments this lane had no use for arm four extra checks, which
   NATIVE1a's report already states. The three UI suites were run because the
   brief pre-registered them, not because this change reaches them -- it touched
   no QWidget, no `.qss` and no in-application harness.

**Nothing else moved**, and the suites this change cannot reach (hkx, gltf,
collision, block, the water suites, the impostor and card suites, the UI suites,
`skeleton_overlay.sh` which BUILD11 measured as flaky) were not run, which is why.

### The stock path

`--native` off, the same nine-chunk region: **25 of 25 output files
byte-identical**, and 25 of 25 against `tests/baselines/stock_baseline.sha256`,
written by the **2026-09-10 03:57:46** exe. The emitter still touches nothing
the stock bake writes.

---

## 6. Pictures

Both are drawn from the DECODER'S OWN geometry -- the `.lodo`'s dequantised
cluster vertices and the `.lodi`'s decoded instance transforms -- never from a
number the writer printed and never from a screenshot. There is still **no
reconstruction path** in this tree that turns a pair into a mesh the render hook
can photograph, and this lane did not build one: the pair is a GPU-driven format
with no index buffer, and the selector that would drive such a path is the
reference cut, which lives in Python. Script
`scratchpad/native1b_20260911/pictures.py` (Pillow; matplotlib is not installed
on this machine).

**`scratchpad/native1b_20260911/images/ladder.png`** (1680x536). Four panels,
same camera and same scale, of ONE library mesh at levels 0, 1, 2 and 3 --
`TreeMapleForest02_LOD_1.nif`, chosen from the data as the mesh the region
places most often that has at least four levels (415 placements, 2,014-unit
diagonal). Each panel is every triangle of every cluster at that level, drawn as
a wireframe front elevation, with its triangle count and its stored
`geometricError` burned in underneath. **What it shows:** 57 -> 28 -> 10 -> 3
triangles as the error goes 0 -> 236.3 -> 527.7 -> 804.1 units, and the crown
visibly dissolving -- which is section 1's finding as a picture rather than as a
percentage.

**`scratchpad/native1b_20260911/images/occluders.png`** (980x1062). The downtown
occluder region from above, cropped to the instances and padded one cell: every
one of the 33,123 `.lodi` placements is a blue dot, the light grid is the
4,096-unit cell the boxes are binned into, and every one of the 280 written
boxes is drawn as its footprint in orange with a ring so a 300-unit box is
findable at this scale. **What it shows:** the Boston street grid in dots and
the boxes sitting on the building blocks, 87 of 147 populated cells carrying at
least one, with the four-a-cell cap visible as clumps where a block has more
large closed shells than it may keep.

---

## 7. Owed / red / bungo's calls

### bungo's calls -- the new one first

1. **THE LADDER IS BARELY SELECTABLE, AND THE FIX IS HIS TO TAKE.** The median
   level-1 cluster deviates by **3.80 percent of its model's own diagonal**,
   which reaches one screen pixel only past **52,100 units**, so at the default
   one-pixel tolerance the ladder's first step is not selected anywhere in the
   Commonwealth. The cause is that **"full detail" in this file is already
   Bethesda's LOD mesh** -- a mean of 47.7 triangles for a whole building.
   Building the library from each base's near `MODL` instead of its `MNAM` slots
   would give the ladder real room and would serve his 10:3x ruling
   (*"the cluster hierarchy must include the NEAR levels too"*) properly. **It
   needs NO format change** -- the format, the writer and every gate are
   indifferent to what level 0 was built from -- but it is a much bigger library
   and a bake-time cost, so it is a decision and not a lane's initiative.
2. **The ladder simplifies on a POSITION WELD** (contract Deviation 9), so
   levels 1 and up carry the first contributor's UVs where two vertices shared a
   quantised position. Measured: **117,722 welds merged vertices whose UVs
   differ by more than 1/256 of a UV unit**, over 103,534 welded positions and
   263,876 source vertices. Level 0 never uses the weld, so the near view is
   untouched and the artefact is a texture shift on coarse levels only. The
   alternative -- welding by (position, UV), which respects every seam and
   ladders far fewer meshes -- is one bake to measure and is his call.
3. **CARRIED FORWARD FROM NATIVE1a, UNCHANGED AND STILL HIS** (contract
   Deviations 6 and 7): the mesh/material sort sits inside the CELL rather than
   mesh-major across the chunk, because mesh-major would break the 8-byte
   cell-range row the near-field suppression reads; and the placed REFR form id
   stays in the 8-byte COLD record at the instance's own index rather than
   growing the hot 24-byte record to 32 (+33 percent on the one buffer the
   per-frame cull dispatch reads). v3 did NOT take the opportunity to change
   either; both are still open.

### Red / owed

| item | state |
|---|---|
| **the aggregate ring-3 impostors** (his 08:3x *"1 sounds good"*) | **NOT this lane** -- CARDS-AGG. `cardLayer` and `cardCorpusHash` are in the rows and still 0 |
| the asymmetric drop proof on (-32,0) dim 32 | **STILL NOT RUN**, as at NATIVE1a: it needs its own `--slot-fallback --native` bake of that chunk |
| a consumer in FO4CS | nothing reads either file yet; there is still no reconstruction path, so the pictures are the decoder's own geometry rather than a render |
| the LOD Generation panel | **no `.lodo`/`.lodi` row exists** and the panel prints no stage times for any output; lane LODUI1's, with the five-outputs-under-FO4CS ruling. THE FOUR STAGE TIMES ARE STILL NOT DELIVERED |
| `boundaryCoarsest` in the mesh report | a REPORTING column, not a gate: it is not comparable to level 0's count wherever the ladder is partial (contract Deviation 11). 18 of 1,900 laddered meshes show a rise and every one of them has a refused group |
| `WW_CHANGES.md`, `HANDOFF.md`, `MISTAKES.md` | **NOT edited by this lane.** Text is in `scratchpad/native1b_20260911/{WW_CHANGES_ENTRY,HANDOFF_BLOCK,MISTAKES_ENTRIES}.md` |
| commits | **none.** Nothing committed (CONSTITUTION 8) |

### Restart

**YES.** Any window bungo has open predates **2026-09-11 10:14:23**. No window
held the exe at any of the three links (`exe not held by a window` in all three
chain logs), so nothing of his was renamed aside and nothing was killed.

---

## 8. Mistakes

Four, written the moment each was recognised; the full text is
`scratchpad/native1b_20260911/MISTAKES_ENTRIES.md` for the director to splice.

1. **A gate's own path line ran BOTH halves of a `||`.** `a && b || c && d` is
   `(((a && b) || c) && d)`, so `pwd -W || pwd` ran `pwd` as well and `$WA` came
   back as two lines. Nine checks failed as if the writer were broken; the whole
   diagnosis is the `\n` inside the path in the traceback. The suite had only
   ever been run WITHOUT `OUT=`, so a day-old defect surfaced on the first run
   with a new argument.
2. **A hand-derived fixture answer that no hand can derive.** With the ladder on,
   `lodo.clusterCount`, `vertexCount` and `triangles` describe a simplifier's
   output. Keeping them would have meant turning the ladder off in the
   known-answer control or copying the writer's numbers into the expectations.
   Split into level-0 counts (hand-derived), invariants (true whatever the
   simplifier does) and one stated PREDICTION.
3. **A gate I wrote myself compared a FRAGMENT against a WHOLE and called the
   difference a hole.** The coarsest level's boundary count against level 0's
   looked like a silhouette rule; a ladder is partial wherever a group refuses,
   so the coarsest level is a fragment covering as little as 6.6 percent of its
   own mesh. One real defect inside it (a watertight building taken to four and
   eight boundary edges) became a per-STEP refusal in the writer that fires 57
   times; the rest was the instrument.
4. **A backslash through a Bash heredoc, exactly the trap `nifskope-ww-lodgen`
   names**, twice: once in a patch anchor and once in the lane's own
   `anchors.py`. The rule was already written down and already read.

---

## 9. Finished-work skill review

**Loaded and used.** `nifskope-ww-lodgen` (the CLI, the absolute-path rule, the
byte-identity gates, the heredoc trap -- which was right twice and ignored
twice); `nifskope-ww-build-verify` (`tools/ww_build.sh`, make's own exit code,
the process guard immediately before the link, the header-staleness sweep, the
syntax pass with the real flags, and the rule that the exe-newer test covers
every driver a gate runs); `ww-standalone-writer-gate` (the shape of legs 1-3,
and its own "name the rule that must answer" section, which is why all 44
mutations quote the substring their refusal must carry);
`ww-spec-gate-audit` (which is why the first thing this lane did was re-measure
the three numbers the brief told it to reproduce, and why gate B1 was corrected
BEFORE any code was written rather than reproduced);
`ww-contract-provenance` (hashes first, 96 anchors re-derived by script, the run
repeated for idempotence, version constants re-read last, hashes re-diffed);
`ww-anchored-hookup` (the `.pro` and `nifcli.cpp` edits through a refusing
script, 7 anchors each exactly once, CR 0 -> 0);
`fo4cs-census-field` (every new word WRITTEN and MOVES, every refusal named --
`refSmall`, `refNoCut`, `refFlat`, `refSilhouette`, and the six occluder
refusals -- and a default that accuses its own plumbing rather than printing 0);
`ww-control-calibration` (the floor/ceiling pairing, and the reason both of this
lane's first floors were rebuilt).

**Not loaded, and why.** `nifskope-ww-render-shot` -- there is still no
reconstruction path from a `.lodo`/`.lodi` pair to renderable geometry, so there
is nothing for the render hook to photograph; the pictures are the decoder's own
geometry, which the brief allows and section 6 says plainly.
`nifskope-ww-panel-style`, `ww-test-harness-add` -- this lane touched no QWidget
and added no in-application harness; its gates are CLI and Python.
`nifskope-ww-resume-pending` -- the lane never ended BUILD PENDING.

**Amended, in the REPO tree, for the director to mirror to the live tree**
(`E:\Projects\Claude\.claude\skills`), all CR 0:

1. **`.claude/skills/ww-spec-gate-audit/SKILL.md`** (7,345 -> 10,041 B) -- a new
   section, *"Audit the gates you write for YOURSELF, and start with the
   DOMAIN"*. The skill was written for a number another lane hands you; this
   lane's worst gate defect was one it invented itself an hour earlier, and the
   question that would have caught it before a line was written is "are these
   two quantities over the same domain?". With the three moves: name both
   domains out loud, print the rows AND their domains when a check fails on real
   data, and split the audit's outcome so the enforceable half becomes a refusal
   in the writer that must be seen to FIRE.
2. **`.claude/skills/ww-control-calibration/SKILL.md`** (4,515 -> 6,279 B) -- a
   new section, *"A floor with a FIXED step cannot fire on a degenerate
   subject"*. Two of this lane's floors were built by perturbing the subject by
   a fixed amount and both read a clean zero on subjects the amount could not
   move (a zero-width cone, a box with room to grow). The repair is to perturb
   RELATIVE to the subject's own measurement, and to report the perturbation
   that finally bit as a number in its own right. With the test to apply before
   the run: name a subject for which the perturbation changes nothing.
3. **`.claude/skills/nifskope-ww-lodgen/SKILL.md`** (27,775 -> 30,210 B) -- three
   measured facts: the `a && b || c && d` shell trap in this repo's own spell and
   its grouped fix, plus "a gate run with a new argument is a NEW gate"; **the
   Sanctuary region has NO watertight LOD mesh** (41 drawn, 0 watertight,
   against 365 in the library) so every solid-volume rule is vacuous there and
   needs the second region, cells 0 -12 11 -1; and the corpus number anything
   planning to simplify these meshes should size itself against -- **the mean
   Commonwealth LOD mesh is 47.7 triangles**.

**Wished for and named rather than left silent.** A skill for *"grow a binary
format by a PARALLEL table"* -- the pattern this lane ran four times (header
offset + stride + a count + a cross-checked summary word in the row it parallels
+ the table joining `indexCrc32` + the reader's per-row rules + one mutation
each) and that CARDS-AGG and TERRAIN-R will both run again. NATIVE1a declined a
"grow a format by one FIELD" skill because it was covered by
`ww-standalone-writer-gate` plus `fo4cs-census-field`; a whole parallel TABLE is
a different shape, and the part neither existing skill covers is the CRC and
table-order arithmetic that makes the old version's file unreadable by the new
reader even without the version word. **It is a page rather than a paragraph, so
whether it gets its own file is the director's call.**

**Declined.** A skill for the occluder voxel fit: it is one function, it is
documented in the contract at 4.5.1 with its refusals, and nothing else in this
tree fits a box inside a mesh.
