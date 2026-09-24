### 1.2 The two reds nobody had attributed: both are the GATE, and both are fixed

**`lodgen_merge` — `FAIL every A line names an existing block and its layer
matches the vertices (2 per-vertex shapes)`.** The gate deletes its work
directory on exit, so the evidence was reproduced into
`scratchpad/audit1_20260916/mergedbg/` with the same script and `W` pinned.
What the shapes actually carry (`merge_probe.py`, reading the chunk's vertex
data, not the gate's counters):

| merged block | vertices | triangles | segments | UV 2 channel | layers stored | the A line says |
|---|---|---|---|---|---|---|
| 2 | 201 | 109 | 16 | **absent** | -- | layer 0 |
| 9 | 18,481 | 17,061 | 16 | **absent** | -- | layer -1 |
| 15 | 13,919 | 7,069 | 16 | **absent** | -- | layer -1 |
| 22 | 759 | 497 | 16 | **absent** | -- | layer 8 |
| 28 | 1,598 | 739 | 16 | **absent** | -- | layer 9 |

Not one of the five shapes has a UV 2 channel, so the gate compares every
layer claim against an EMPTY set and all five fail -- `badLayer` 5, `badA` 0.
The cause is a ruled DEFAULT, not a writer defect: since DEFAULTS1
(2026-09-12) object identity is OFF unless `--identity` is spelled, and a
default chunk's vertex descriptor has no UV 2. The product says so in its own
code -- `src/lodgen.cpp:5149`, `shapesWithoutUv2++;  // a profile without the
extra channels: the manifest still says` -- and it keeps writing the A line so
the layer is still on record. The gate's two bakes never spelled `--identity`.

REFUTED by running the exe the other way: the same region, the same switches
plus `--identity` (`mergedbg/wid`), same five blocks, same triangle counts,
and now every shape carries UV 2 -- block 2 `{0}`, block 9 `{1,2,3,5,7}`,
block 15 `{4,6}`, block 22 `{8}`, block 28 `{9}` -- and all five A lines match
exactly. The product is right; the gate is stale.

FIX (`tests/spells/lodgen_merge.sh`): both bakes now spell `--identity`, since
the layer-in-UV2 contract is what check 2 exists to measure; a new check
asserts the channel is present (`every merged shape carries the UV 2 layer
channel`), so the gate goes red rather than vacuous if the channel ever stops
being written; and a refuter doctors every claim (a per-vertex shape told it is
single-layer, a single-layer one told it is per-vertex) and requires all of
them to be refused. **Before: RESULT FAIL, 9 ok / 1 fail, 20 s. After: RESULT
PASS, 12 ok / 0 fail, 19 s, with `the layer comparison can fail (5 of 5
doctored claims refused)`.**

**`lodgen_roads` — `FAIL R1 two --no-roads runs are byte-identical (1 of 10
differ)`.** Re-run with `OUT=` to keep the tree
(`scratchpad/audit1_20260916/roadsdbg`). The one differing file is
`obj/Commonwealth.lodb`, the BAKEREC1 record, and `diff` puts the whole
difference in four lines that are a record of the RUN, not of the output:

| line | run 1 | run 2 |
|---|---|---|
| `baked` | `2026-09-17T15:11:25Z` | `2026-09-17T15:11:28Z` |
| `switch` (x3) | `.../roadsdbg/roadOff/{obj,mod,tex}` | `.../roadsdbg/roadOff2/{obj,mod,tex}` |
| `census` | peak working set 1591427072 B, layout `.../roadOff/mod/FO4CSLOD` | 1591590912 B, `.../roadOff2/mod/FO4CSLOD` |

Every other line is identical, including the `switches` digest (it excludes
path-valued switches) and all four content hashes -- `chunk -20 20 4
0f0440cd...`, and the BTR, BTO and manifest `out` hashes. The other nine files
(BTR, BTO, manifest, three terrain sheets, two `.lodt` and the VT `.lodm`) are
byte-identical. **The bake is deterministic; R1 was sweeping the bake record in
with the outputs.** The gate predates the record: HANDOFF's ROADS2/ROADS3
baselines read `lodgen_roads.sh` **11/0**, and the `.lodb` is the tenth file
that BAKEREC1 added afterwards.

FIX (`tests/spells/lodgen_roads.sh`): the byte loop skips `*.lodb` and says so
in the check text; the record is compared on its content instead -- every
`chunk` and `out` hash line must match; and a floor proves that comparison can
move, by requiring a DIFFERENT region's record to name other hashes. The first
floor tried was `--roads` against `--no-roads`, and it FAILED: the record names
only the chunk files (BTR, BTO, manifest) and roads paint the terrain sheets,
so the road-bearing region hashes identically with the switch either way. That
is measured, not assumed, and it is why the floor is a different region.
**Before: 11 checks / 1 fail, 21 s. After: 13 checks / 0 fail, 19 s.**
