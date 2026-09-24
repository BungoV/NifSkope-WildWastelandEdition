## 2026-09-16 -- lodgen: the object library is built from the near model; foliage, the silhouette floor and a per-placement AO byte (lane NATIVE1c)

Four changes to the FO4CS-native far field, `.lodo` to **v4** and `.lodi` to
**v5**. Every one has a switch that turns it off, and the four switches together
bake what the previous exe baked:

```
--library mnam --native-ladder-foliage --native-silhouette 0 --native-no-placement-ao
```

### Level 0 now comes from each base's near `MODL`

The library's level 0 was the base's first `MNAM` LOD slot -- already Bethesda's
LOD mesh, a mean of **47.7 triangles for a whole building** -- so the cluster
ladder's first step was a simplification of a simplification. It is now the
base's own near model, and the four `MNAM` slots move one rung down the `rep`
array. No format change; `--library mnam` is the exact way back.

What it buys, both arms of the same exe on the same 9-chunk region, measured off
the `.lodo` bytes by `tests/spells/lodgen_ladder_select.py` at the spec's own
one-pixel tolerance:

| | `--library mnam` | `--library near` (default) |
|---|---|---|
| median level-1 deviation | 65.8751 units, **3.8028 %** of the model diagonal | 3.5537 units, **0.1521 %** |
| first step reaches 1 px at | **90,316 units** | **4,872 units** |
| level-1 clusters | 5,676 | 152,577 |

The 3.8028 reproduces `docs/FO4CS_IMPROVED_LOD_PLAN.md` §6 (a)'s own 3.80 from
the file, which is the control that says the two arms are the thing that row was
about. **The first step now lands 18.5 times closer.** §6 (a) is marked ruled.

What it costs, stated plainly: `Commonwealth.lodo` goes **9,657,316 B ->
225,399,755 B (23.3x)**, 252,268 -> 7,572,082 library triangles, and the bake of
that region goes 13 s -> 83 s. The `.lodi` is unaffected by the switch.

### Foliage is never laddered

A cluster whose material is alpha-tested -- a tree or a bush card -- is refused a
group and stays a root, because a simplifier scores a leaf quad by two triangles
that mean nothing when the shape is in the cutout. bungo over lane NATIVE1b's
`ladder.png`: *"Hm, that tree LOD becomes a stump there"*. Refusals are counted
by name (`refFoliage`) in the census: **2,849 clusters** on the region.
`--native-ladder-foliage` refuses 0 and puts them back -- level-1 clusters
152,577 -> 154,201, gated both ways.

### A silhouette floor, per LEVEL, with the floor calibrated not asserted

Every level the ladder keeps must hold at least **0.70** of level 0's outline
from the worst of 8 horizon views at 96 px, and the comparison is the whole CUT
a viewer sees at that level, not the level's own clusters. Below the floor the
whole level is rolled back. On the region it refused **5,683 levels across 3,266
meshes**, and the worst fraction any kept level holds is exactly 0.7000.

The 0.70 is calibrated, by an independent rasteriser in
`tests/spells/lodgen_silhouette.py` that reads the `.lodo` bytes rather than
asking the writer: a real kept cut reads **0.7288** at its worst over 75 levels,
and a random-vertex-drop twin matched on triangle count reads **0.2614** at
worst and 0.4943 median and FAILS on 18 of 21 meshes. The floor sits in the gap.
`--native-silhouette <0..1>` moves it; `0` is the exact v3 ladder.

### A per-placement AO byte, so a card-drawn LOD is not always fully lit

bungo, 2026-09-11: *"is vertex AO baked into impostors too on top of the texture
AO they hold?"* The answer was no. The instance record's `ao` is a mean over that
placement's own lit chunk-mesh vertices, and a card-drawn placement has none, so
it was written **255, fully lit** -- a tree under a bridge read the same as a
tree in a field. `.lodi` v5 adds a parallel `u8` blob, one byte an instance, from
one ray cast straight up at bake against the assembled chunk **and** the
heightfield, exactly as the chunk's own vertices get theirs. Measured on the
region: 3,526 of 3,526 written, **min 38, median 254, mean 236.4, 9 distinct
values**. `0xFF` means NOT MEASURED and is not AO 255.

### Four header words that were owed

`docs/LODGEN_CENSUS.md` §6.3 items 1, 2, 3 and 5, all four written and all four
checked to MOVE, not merely to exist:

* `.lodi` `slotInstances[4]` at 0xD4 -- instances per `MNAM` rung; the reader
  refuses a file whose four totals do not sum to `instanceCount`, by name.
* `.lodo` `fullTriangles`, spending `crossPx16[0..1]` -- 2,974 of 2,974 bases
  non-zero, 1,120 distinct values; the reader RECOUNTS it and refuses a mismatch.
* `.lodo` `cardCount` at 0xD0 -- `cardCount > baseCount` refused by name.
* `.lodo` `LODO_MESH_WATERTIGHT` in the mesh row's free flags, from zero boundary
  edges -- 654 of 5,567 meshes; the gate requires the count to be strictly
  between 0 and the mesh count, because all-set and all-clear are both the bit
  not working.

### The version words, and what an old file now gets told

`.lodo` goes to **4 unconditionally** and **refuses version 3 by name**, because
four bytes v3 wrote as zeros are now READ as a triangle count and zero triangles
is not "absent", it is a lie that reads as a number. `.lodi` goes to **5
conditionally** -- only when the AO blob is present -- because every header word
it adds is pad the old writer already zeroed. Both refusals are gated.

**This means every `.lodo` on disk from before today is refused by name and has
to be re-baked.** That is the intended cost of the unconditional bump and the
refusal says so in words, naming the four bytes.

### New gate `tests/spells/lodgen_ladder.sh` (22 checks, 0 failures)

Bakes four arms of one region -- everything on, the foliage refuter, the whole
way back, and the same region on the exe this lane started from -- and checks
the foliage refusal moves, the silhouette floor holds with its twin red, the
first step is selectable, the AO byte and the four words are written and move,
and the way back is exact. Two helpers ship with it:
`tests/spells/lodgen_ladder_select.py` (the selection law applied to the `.lodo`
bytes) and `tests/spells/lodgen_lodi_wayback.py` (names every differing byte of
two `.lodi` files and refuses any that is not inside a derived word).

### Nothing else moved

`lodgen_native.sh` reads **120 checks, 0 failures, 2 skips** (it read 108/0/2 on
the previous exe; the 12 are the new field checks).
`lodgen_native_baseline.sh --check` reads **25 files, 25 baked, 0 differ** against
the baseline frozen on the 2026-09-10 exe. `lodgen_defaults.sh` reads **28
checks, 0 failures**: no default changed, and `--road-detail 1` is in every bake
on every arm of every gate here.
