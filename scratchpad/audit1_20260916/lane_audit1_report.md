# Lane AUDIT1 — the final-build audit of the lodgen pipeline

Written incrementally. Every number below was measured by this lane on disk unless the line says
QUOTED, in which case the source is named.
## Summary

1. **Exe.** Audited `a843fca6` (22,567,424 B, 12:45:27), rung as `release/NifSkope.before_audit1.exe`. Shipping now **`48f7f1ab`, 19:30:16**: seven edits, three files, every one a refusal or a message.
2. **The writer did not move.** Region (a) re-baked on the fixed exe: **55 of 55 byte-identical**, 0 differ, 0 missing, only the bake record excluded.
3. **Board.** All 27 lodgen gates re-run on the fixed exe: **24 green, 3 red**, and all three reds are pre-registered known ones (grass ground cover 7, the byte gate's missing rung 3, CARDWIDTH 1). **Five gates went red to green** -- the addendum's three standing reds plus two nobody had attributed -- and `lodgen_native`'s section 14, this lane's own four checks against the payload-bounds and aggregate defects, is green by name.
4. **Every red is attributed.** Three standing reds plus two nobody had claimed were all the GATE, decided by running the exe both ways, each fixed with its own refuter; the three that stay red are pre-registered known ones (the four grass-feature ground-cover checks, the byte gate is missing a rung exe nobody kept, and CARDWIDTH at 1.74 texels).
5. **Four CONFIRMED defects fixed:** two payload bounds tests that wrapped in `quint64`, the aggregate rules that stopped at version 4 while the default bake writes version 5, a valued switch that silently did nothing spelled last (`--incremental` full-baked at exit 0), and a warning that named an outcome that did not happen. A fifth, `aggregateStride 0` on a v5 file, was found by the fix's own verifier.
6. **Four handed on with reasons** (6.7): the unguarded road-triangle index, the texture failure that never reaches the exit code, the census counting freed bytes before the delete, the viewer's 16-bit bucket overflow.
7. **Bakes.** Three fixture regions through the FO4CS target with defaults: **45 / 50 / 73 s** at **2.00 / 2.13 / 2.68 GB** peak working set, and **meshes is the long pole every time** (39.0 / 39.5 / 59.7 s). The stock target on the same three regions is **byte-identical to the rung exe**, every file but the bake record, whose format changed under BAKEREC1. A null `--incremental` over my own fresh bake replays all 9 chunks, writes 2 files, and is **55 of 55 byte-identical** in 30 s against 45 s -- and still rebuilds the object library, in words.
8. **Decode.** 15 invariants, 76 files, 304 checks, 0 violations, 21 refuters of which 19 went red -- the two that did not are the exe's own aggregate rules, which is what F3 and F4 fix.
9. **Design gaps, as rows:** `cardCorpusHash` is zero on every bake including one baked against 23 real cards; `--dim` takes one integer and a second `--native` run replaces rather than merges, so no `.lodi` this CLI writes can carry two non-zero slots; under the shipped defaults the object library is rebuilt on every bake of any kind.
10. **bungo's row** (thick leaves): measured, not a defect -- the cause is the `--library near` DEFAULT, not the card bake. Section L.
11. **One picture is a refusal, and it is the viewer working.** The whole of downtown Boston -- 33,123 instances over a 517,534-cluster library -- is refused by the native view by name, `more than 9500000 vertices ... ask for a smaller WW_LODI_REGION or a coarser WW_LODI_LEVEL`, so region (c) is pictured through a sixteen-cell window, which is the way back the message names. Section 6.8.

## 0. The exe under audit, and the method

| what | value |
|---|---|
| `release/NifSkope.exe` | **2026-09-17 12:45:27**, **22,567,424 B**, sha1 `a843fca68c18c2740efddcb20e9fe715b7732a22` |
| measured by | `ls -la release/NifSkope.exe` + `sha1sum`, 2026-09-17 15:42:47 (`date` in the same command) |
| agrees with | the addendum's identity for PERF1's build — mtime, size and sha1 all three |
| rung for this lane | `release/NifSkope.before_audit1.exe` — written **only if this lane builds** (§6) |
| sources newer than the exe | none under `src/`, `tests/`, `tools/` except `__pycache__/*.pyc` (checked at §6) |
| game / NifSkope at lane start | none (`tasklist | grep -i -E "Fallout4|NifSkope"` → rc=1, 15:42) |

**Pre-flight on the gates themselves** (the `nifskope-ww-lodgen` rule: a SyntaxError in a
`<<'PYEOF'` block does not stop the script, it silently deletes checks). All 27
`tests/spells/lodgen_*.sh` compiled: **35 `PYEOF` blocks + 4 blocks under other markers
(`IDXEOF`, `LEDEOF`, `MSNEOF`, `STATEOF`) = 39 of 39 compile**, plus every
`tests/spells/lodgen_*.py`, `lodb_read.py`, `lodj_read.py`. Script:
`scratchpad/audit1_20260916/pycompile.py`. So a `RESULT FAIL` below is a real failing check and
never a dead block.

**Game check.** `scratchpad/audit1_20260916/run_gates.sh` runs `tasklist` as its own step before
EVERY gate and aborts the whole batch (exit 90/91, `abort.txt` written) if `Fallout4.exe` or any
`NifSkope.exe` is up at that moment. No gate in this report ran with the game up.

**Fixture regions, chosen by measurement, not by taste.** Three 12x12-cell (3x3-chunk) regions of
the Commonwealth, each aligned to the chunk grid:

| # | region | cells | why |
|---|---|---|---|
| a | **Sanctuary** | `-20 24 -9 35` | the region every native lane used (chunk (-20,24) at dim 4 is its near chunk) |
| b | **South-east coast** | `4 -28 15 -17` | **44.9 % of its overview texels sit below their cell's resolved water height**, over **5 distinct water heights** — measured off the shipped whole-Commonwealth `.lodl` |
| c | **Downtown Boston** | `0 -12 11 -1` | the dense urban region the lodgen skill names (33,123 placements, 280 occluder boxes); 30.3 % wet, 2 water heights |

The water region was picked by the script `scratchpad/audit1_20260916/pick_regions2.py`, which
reads the per-cell table and the coarse overview of
`E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl` (v2, 35,953,294 B, 2026-09-09)
and counts, per 12x12 region, the overview texels whose height is below their own cell's resolved
water height. The first instrument tried — the per-cell `has water` flag — is **vacuous on this
worldspace**: all 36,864 cells carry it, because the Commonwealth's `DNAM` default water plane
covers the whole grid. That is written down because it is exactly the "a check that cannot fail on
its input is not a check" trap, and it would have made any 12x12 region look equally watery.

## L. bungo's row, 2026-09-17: "thick leaves on LOD textures. Thicker than on the vanilla bakes"

Not fixed. No ledger entry claims it, and nothing in this lane fixes it, because
what makes our leaves thicker is a **default**, not a defect: since the native
library landed, a default FO4CS bake builds every object's rung 0 from the
**near model** and therefore from the **near leaf texture**, while vanilla's
object LOD draws the MNAM LOD model and its dedicated, thinned LOD texture.
Everything below is measured on this exe and on the shipped game files; the last
sub-section names the one-switch way back and what it would cost.

Every number is the **alpha-test coverage**: the fraction of a surface's texels
that pass the alpha test the consumer actually runs. A leaf is a cut-out, so
that fraction IS its thickness on screen. The readers are new and read only the
compressed bytes -- `scratchpad/audit1_20260916/leaf_alpha.py` (BC1
punch-through: a block stored `c0 <= c1` makes index 3 transparent, every other
index opaque, and a `c0 > c1` block is wholly opaque; BC3: the eight-bit ramp),
`leaf_chain.py` (the writer's own 2x2 box filter), `find_tile.py` (normalised
cross-correlation, to find a source's tile inside a baked atlas),
`leaf_pairs.py`, `leaf_table.py`, `dds_png.py`, `leaf_picture.py`.

### L.1 What the threshold is, and it is the one bungo already had fixed

`src/lodgen.cpp:2161` throws the source's own alpha threshold away and forces
**128** with a comment naming this same complaint ("near-tree materials test at
65-80, which at LOD distance passes far more canopy texels than vanilla's chunks
do ... bungo diagnosed the denser-canopy difference as exactly this cutoff"), and
`src/nativeemit.cpp:1337` carries that byte into the `.lodo` material row.
MEASURED on this lane's Sanctuary bake: every one of the twenty foliage material
rows reads `a=128`, and 137 of 769 distinct material strings are alpha-tested.
So that half is intact and is not the mechanism.

### L.2 The mechanism, proven by running the exe both ways

A default FO4CS bake of Sanctuary (region -20 24 .. -9 35, dim 4) against the
same bake with `--library mnam`:

| | `.lodo` bytes | triangles | materials | foliage materials named |
|---|---|---|---|---|
| default (`--library near`) | 225,399,755 | 7,572,082 | 787 | 8 `Materials\LOD\Trees\*` **and 12 near ones**: `Landscape\Trees\MapleAtlas01/02`, `MapleAtlas02_Tree`, `TreeElmAtlas`, `TreeSMarsh`, `TreesGlowingSea`, `BlastedForestBurntTreeAtlas`, `BlastedForestDestroyedTreeAtlas`, ... |
| `--library mnam` | 8,764,388 | 226,863 | 136 | 8 `Materials\LOD\Trees\*` and **no near one** |

The near rows come with the near models, measured off the cluster table: 2,990
triangles of `MapleAtlas02.BGSM` on `Landscape\Trees\TreeHero01.nif`, 1,915 of
`BlastedForestBurntTreeAtlas.BGSM` on
`Landscape\Trees\BlastedForestBurntTreeUpright01.nif`, 1,437 of
`TreeElmAtlas.BGSM` on `Landscape\Trees\TreeElmFree01.nif`, and so on -- full
near `.nif` files, not `LOD\` ones. The switch is `src/nativeemit.cpp:1207-1214`:
`--library near` (the default) is `[ near MODL, MNAM 0, MNAM 1, MNAM 2 ]` and
pushes vanilla's LOD slots one rung down.

That the placements themselves are vanilla's dim-4 LOD placements is measured
too: the `.lodi` header's `slotInstances` reads **[3526, 0, 0, 0]** -- every
placement came from MNAM slot 0, the slot vanilla draws at this ring. The
library is what changed under them, not the placement set.

### L.3 The numbers: the near texture against the LOD texture vanilla uses

Alpha-test coverage at 128, per mip, on six foliage pairs. "near" is the texture
a default bake's rung 0 names; "LOD" is the texture vanilla's object LOD (and our
own rungs 1-3) names. Both columns are the file's OWN stored mip chain.

| near texture (ours, rung 0) | mip 0 | mip 3 | mip 5 | LOD texture (vanilla) | mip 0 | mip 3 | mip 5 | mip-0 ratio |
|---|---|---|---|---|---|---|---|---|
| MapleAtlas01_d 4096x2048 | 0.7175 | 0.7704 | 0.8396 | MaplePostWarSet01LODlv2_d 256 | 0.1230 | 0.0977 | 0.0312 | **x5.83** |
| MapleAtlas02_d 4096x2048 | 0.3780 | 0.3989 | 0.4403 | MaplePostWarSet02LODlv2_d 256 | 0.1003 | 0.0732 | 0.0000 | **x3.77** |
| ElmAtlas_D 4096x2048 | 0.5525 | 0.5762 | 0.6117 | ElmSet02LODlv2_d 256 | 0.1528 | 0.1016 | 0.0625 | **x3.62** |
| ElmPreWarAtlas_d 4096x2048 | 0.6198 | 0.6339 | 0.6714 | ElmSet01LODlv2_d 256 | 0.1251 | 0.0791 | 0.0000 | **x4.95** |
| BlastedForestBurntTreeAtlas_d 4096x2048 | 0.5013 | 0.5006 | 0.4966 | BlastedForestSet01LODlv2_d 256 | 0.1946 | 0.1885 | 0.0938 | **x2.58** |
| BlastedForestDestroyedTreeAtlas_d 4096x2048 | 0.3966 | 0.3964 | 0.3920 | BlastedForestSet01LODlv2_d 256 | 0.1946 | 0.1885 | 0.0938 | **x2.04** |

Two readings, and the second is the one that shows on screen:

* at every mip the near texture passes **2.0 to 5.8 times** as many texels as the
  LOD texture the same tree has beside it;
* the two chains go OPPOSITE WAYS. The LOD texture thins with distance (Maple
  0.1230 -> 0.0312 by mip 5, gone by mip 6). The near texture **fattens** (Maple
  0.7175 -> 0.8396 at mip 5 and 0.8828 at mip 7 in its own chain, 0.9043 under
  ours). A leaf card whose texels each cover a big footprint stops being a
  cut-out and fills in **solid**. The branch-card region of `MapleAtlas01_d`
  alone (the 512x512 cell at 0,0) reads 0.4681 at mip 0 and 0.4912 at mip 4.

The picture is `images/leaf_sidebyside.png`: the LOD texture, vanilla's baked
tile, and the near texture at two mips, all thresholded at 128, with each panel's
coverage on it.

### L.4 What is NOT the mechanism (each refuted with its own numbers)

* **Our mip filter.** `lodgen.cpp:4690-4705` (arrays) and `:4549` (atlas) average
  alpha `(acc[3]+2)>>2`. Run against the source's own stored chain on the same
  files, ours tracks it: MapleAtlas01 mip 7 ours 0.9043 vs source 0.8828 (+2.4%),
  ElmAtlas mip 7 ours 0.5156 vs source 0.6484 (**-20%**, ours thinner). It is not
  a fattener, and a **default bake writes no atlas and no arrays at all** -- the
  Sanctuary FO4CS bake's nineteen files are `.lodo`, `.lodi`, `.lodj`, `.lodb`
  and the manifests, so this chain is not even in play unless `--arrays` or
  `--atlas` is spelled.
* **The colour dilation** (`lodgen.cpp:12269`). It writes RGB only --
  `sheet[i] = (sheet[i] & 0xFF000000U) | ...` keeps the alpha byte -- so it
  cannot move a silhouette.
* **Our atlas being the thick one.** Where the stock-target atlas does hold the
  same LOD tile, ours is the THIN one. Vanilla's `Commonwealth.Objects.DDS` holds
  these LOD textures 1:1 at 256x256 on a 256 grid (found by cross-correlation:
  MaplePostWarSet01LODlv2 at 2560,1024 ncc 0.819; ElmSet02LODlv2 at 2048,1024 ncc
  0.703; ElmSet01LODlv2 at 3840,768 ncc 0.613; MapleTrunksLOD at 2560,1280 ncc
  0.980; ElmTrunksLOD at 2304,1024 ncc 0.981; BlastedForestTrunksLOD at 3840,512
  ncc 0.984). Vanilla's BC1 re-encode FATTENS the tile against its own source --
  Maple 0.1230 -> 0.1258 at mip 0 and 0.0820 -> 0.2031 at mip 4 -- so ours reads
  **x0.98 of vanilla at mip 0 and x0.40 at mip 4** on that tile, x0.83 -> x0.50
  on ElmSet02, x0.93 -> x0.42 on ElmSet01. Trunk tiles are solid on both sides
  (1.0000 at every mip), which is the floor that says the comparison can move.
* **The impostor card coverage floor** (16/255, WW_CHANGES ~5685). Cards need
  `--impostors <tree>`, which a default bake does not pass, and this bake wrote
  none.

### L.5 The smallest fix, and why this lane did not make it

The way back already exists and is one switch: **`--library mnam`**, which builds
the ladder from vanilla's own MNAM slots. Measured above: it removes every near
foliage material, and it takes the Sanctuary `.lodo` from 225.4 MB to 8.8 MB.
Making it the DEFAULT is a default change and a design ruling, which this lane is
forbidden. It is bungo's call, and it is not free: the near model at rung 0 is
the whole point of the library (it is what puts real geometry where vanilla has
billboards), so `mnam` trades the thick canopy back for vanilla's silhouette
everywhere, not just on trees.

Three narrower options, in rising cost, for whoever takes the ruling:

1. **Per-family**: keep `near` for everything except bases whose material carries
   `LODO_MAT_TREE` (`src/lodofile.h:184`, already set -- measured `f=4` on all
   twenty foliage rows), which take their MNAM slot instead. Small, and gateable:
   after such a bake NO material row may be both TREE-flagged and named under
   `Landscape\`; the refuter is that the same assertion fails on today's bake,
   where twelve such rows exist.
2. **Texture substitution**: keep the near geometry but point a TREE material at
   the base's MNAM-0 LOD texture. Cheap on paper, wrong in fact -- the near
   model's UVs address the near atlas, and the LOD texture has a different
   layout. Refuted; do not do this.
3. **Coverage-preserving mips for the near leaf texture** once `--arrays` is in
   play (rescale each level's alpha so the pass fraction matches mip 0). It would
   hold Maple at 0.7175 instead of 0.9043 -- worth about a fifth of the fattening
   -- and WW_CHANGES ~5685 records the same law REFUSED on the impostor sheets
   (worst silhouette -9.1%, a solid rectangle fattened by a whole texel). It does
   nothing for a default bake, which writes no arrays.

**The gate this row is owed either way** (a guard on the half that IS fixed, and
it does not need a ruling): every TREE-flagged material row in a `.lodo` must
carry `alphaThreshold == 128`. It passes today on both bakes above; the refuter
is one doctored byte in the material table, which turns it red.

## 1. Every lodgen gate on the final exe

Every `tests/spells/lodgen_*.sh` in the tree was run once against
`release/NifSkope.exe` of 2026-09-17 12:45:27 (22,567,424 B, sha1
`a843fca68c18c2740efddcb20e9fe715b7732a22`), before any edit of mine to the
product. 27 gates, 5,369 s of exe time in total. Logs:
`scratchpad/audit1_20260916/gates1/*.log`, wall clocks from the runner's own
`DONE <name> rc=<n> secs=<n>` lines (`gates1_runner.log`); the five run one at
a time afterwards carry their own timings.

### 1.1 The board

Counts are taken from each gate's own summary line where it prints one, and
otherwise by counting its `ok` and `FAIL` lines; where the two disagree the
column says so rather than averaging them. `WHOSE` is the column the brief
asked for: who owns the red.

| gate | rc | s | checks / fails | ok lines | FAIL lines | WHOSE |
|---|---|---|---|---|---|---|
| `bakerec` | 0 | 243 | (no summary line) | 69 | 0 | -- |
| `btofree` | 1 | 123 | 21 / 3 | 18 | 4 | **STALE GATE**, standing red 2 of the addendum; adjudicated 1.3 |
| `byte_gate` | 1 | 1969 | (see note) | -- | 3 | **KNOWN**, named by two LANDED lines; 1.4 |
| `card_arrays` | 0 | 5 | (no summary line) | 35 | 0 | -- |
| `defaults` | 0 | 757 | 28 / 0 | 28 | 0 | -- |
| `farring` | 0 | 34 | (no summary line) | 21 | 0 | -- |
| `ground_cover` | 1 | 25 | 29 / 4 (inner block 21 / 3) | 43 | 7 | **KNOWN**, pre-existing grass-feature red; 1.4 |
| `identity` | 0 | 2 | (no summary line) | 8 | 0 | -- |
| `impostor_cards` | 0 | 3 | (no summary line) | 12 | 0 | -- |
| `incremental` | 0 | 174 | (no summary line) | 10 | 0 | -- |
| `ladder` | 0 | 125 | 32 / 0 | 40 | 0 | -- |
| `layout` | 0 | 865 | (no summary line) | 23 | 0 | -- |
| `merge` | 1 | 20 | (no summary line) | 9 | 1 | **STALE GATE**, unattributed until this lane; adjudicated 1.2, FIXED |
| `native` | 1 | 170 | 309 / 1 over 7 blocks | 123 | 2 | **STALE GATE**, standing red 1 of the addendum; adjudicated 1.3 |
| `native_baseline` | 0 | 11 | (no summary line) | 3 | 0 | -- |
| `octahedral` | 1 | 69 | (no summary line) | 108 | 3 | 2 **STALE GATE** (LAYOUT1 moved the card path), 1 **KNOWN** (CARDWIDTH, 1.74 texels); 1.3 / 1.4 |
| `panel_run` | 0 | 45 | 137 / 0 | 137 | 0 | -- |
| `perf` | 0 | 549 | (no summary line) | 11 | 0 | -- |
| `resources` | 0 | 3 | 4 / 0 | 4 | 0 | -- |
| `roads` | 1 | 21 | 11 / 1 | 10 | 1 | **STALE GATE**, unattributed until this lane; adjudicated 1.2, FIXED |
| `stage_times` | 1 | 34 | 16 / 1 | 15 | 2 | **STALE GATE**, standing red 3 of the addendum; adjudicated 1.3 |
| `terrain` | 0 | 13 | 26 / 0 | 26 | 0 | -- |
| `terrain_pbrm` | 0 | 34 | 14 / 0 | 14 | 0 | -- |
| `terrain_vt` | 0 | 56 | 58 / 0 | 112 | 0 | -- |
| `texture_arrays` | 0 | 7 | (no summary line) | 40 | 0 | -- |
| `tree_sway` | 0 | 8 | 4 / 0 | 4 | 0 | -- |
| `water_subdiv` | 0 | 4 | 7 / 0 | 7 | 0 | -- |

Nineteen gates PASS. Eight are red, and not one of the eight is a red of the
product introduced by this campaign: three are the standing reds the addendum
named, two are stale gates nobody had attributed, and three are pre-registered
known reds. The adjudications are below, each with the evidence that decides
gate against product.

Two bookkeeping notes on the table itself, so the numbers are not read as more
than they are. (1) `byte_gate` prints its own `137 checks, 0 failures` line --
that is the PANEL self-test it runs inside itself, not its own count; its
three failures are phase lines with a different prefix, which is why the
generic ok-line counter reads 0 for it. Its failures are listed by name in
1.4. (2) `stage_times` prints `16 checks, 1 failures` while carrying 15 `ok`
and 2 `FAIL` lines: the second FAIL is the suite's own trailing `FAIL` verdict
word, not a check. Both discrepancies are in the gates' output, not in the
counting.

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

### 1.3 The three standing reds the addendum names: all three are the GATE

The addendum asks each of these to be decided with evidence -- stale gate
against wrong product -- and the wrong side fixed, with a refuter. All three
came out the same way, and none of them is a product defect. In each case the
gate is asking a question that a landed ruling has already answered
differently, and in each case the product's own output says so.

**Standing red 3, `lodgen_stage_times.sh` -- 1 FAIL: `and the run wrote the
.lodo/.lodi pair --native asked for`.** The gate looked for
`<native>/Commonwealth.lodo` and `<native>/Commonwealth.lodi`, which is where
they were until lane LAYOUT1 (2026-09-16) moved every FO4CS file to
`<mod>/FO4CSLOD/<worldspace>/`. The bake writes them; the gate looks in the
parent. Decided by the bake's own tree: the run the gate had just made has both
files under `FO4CSLOD/Commonwealth/`, and its census names that root -- `layout
.../FO4CSLOD, N file(s), 0 outside`. The check above it in the same gate, which
reads the `stage times:` line, was green throughout, so the bake itself ran.

FIX (`tests/spells/lodgen_stage_times.sh`): a `pairdir()` helper resolves the
pair under the FO4CS layout with the flat path as a fallback, so the gate
follows the ruling instead of a hardcoded shape; a new check asserts the census
names ONE FO4CS root with zero files outside it, which is the layout contract
itself; and a refuter runs the same test against a tree whose `.lodo` has been
removed and requires it to read 0. **Before: 16 checks / 1 fail, 34 s. After:
18 checks / 0 fail, 39 s**, with `refuter: the same test reads 0 on a tree
whose .lodo is missing` green.

**Standing red 2, `lodgen_btofree.sh` -- 3 FAIL, legs (a), (b) and (c).** Two
different stale assumptions, both about files that landed AFTER the rung the
gate pins its bytes to.

Legs (a) and (b) compare a bake against `NifSkope.before_btofree1.exe` file by
file and failed with `0 differ, 0/1 only on one side` -- nothing differs; one
file exists on the new side only. The gate printed which:
`only in drop: nat/FO4CSLOD/Commonwealth/Commonwealth.4.-20.24.lodj`. That is
lane INCR1's per-chunk native cache, which the rung predates entirely. A file
that did not exist when the baseline was cut is not a byte change in an output.

Leg (c) compares the whole stock output and failed with
`DIFFERS: Commonwealth.lodb -> Commonwealth.lodb (716 vs 2161)`. The rung
writes the version 1 BINARY `LODB` container; this exe writes the version 2
PLAIN-TEXT record, so `cmp` can only ever say they differ. That is the bake
record's own format landing, not the stock path moving -- and the stock path is
exactly what this leg exists to measure. Measured on my own step-2 trees: the
stock record is 6,258 B of plain text with 52 `out` rows, 16 `switch` rows, 9
`chunk` rows and 4 `census` rows; nothing in it is a chunk file.

FIX (`tests/spells/lodgen_btofree.sh`): the three sweeps exclude those files BY
NAME and say so in the check text, and -- this is the part that keeps the
exclusion honest -- each exclusion is then ASSERTED on both sides. Leg (a) now
requires that this exe really did write the `.lodj` and that the rung wrote
none, so the exclusion is proved to cover a new output rather than to hide a
change; leg (b) requires the cache on both sides; leg (c) checks the record
separately, by its own format and size, instead of by `cmp`. Two refuters
append a single byte to a compared file and require the sweep to report it.
**Before: 21 checks / 3 fails, 123 s. After: 27 checks / 0 fails, 146 s.**

**Standing red 1, `lodgen_native.sh` section 5 -- 2 FAIL.** The section exists
to prove that the stock path does not move when `--native` is added. Its first
check does exactly that and was green: `25 stock files compared, 0 differ`.
Both failures are one claim about the bake RECORD --

```
FAIL the second ledger differs from the first ONLY in the command-line digest
     (5399992dfee2 -> 5b6569b33c87)
```

-- the first line printed by `lodgen_btofree_ledger.py` itself, the second the
suite's own verdict word for the same thing, which is why six `FAIL` lines
count as five failures in this gate.

THE PAIR THE CHECK ACTUALLY COMPARES is the stock bake against a
`--native --keep-bto` bake: check 4 spells `--keep-bto` so that there are chunks
on both sides. I reproduced it by hand on my own step-2 Sanctuary records and
got the gate's two digests exactly, `5399992dfee2 -> 5b6569b33c87`, so what
follows is measured on the gate's own question. Reduced to canonical rows, the
two records agree completely:

| | stock record | FO4CS record |
|---|---|---|
| `.DDS` sheets | 27 | 27 |
| `.BTR` terrain chunks | 9 | 9 |
| `.txt` manifests | 8 | 8 |
| `.BTO` object chunks | 8 | 8 (`--keep-bto` is spelled) |
| `.lodj` native cache | 0 | **9** |
| canonical rows shared | 52 | 52, none only on one side |
| **recorded digests that moved** | | **0** |

The `.lodj` rows are lane INCR1's per-chunk native cache, which the stock target
cannot have, and those are already checked-then-dropped by `--fo4cs-vs-stock`.
After that there is not one output file the two records disagree about. So the
red was never about an output at all.

WHAT IT WAS ABOUT. `keep` prints "differs ONLY in the command-line digest" but
runs a whole-document `==` over everything the record reader returns. On this
pair that document differs in ten places and not one is an output row:

| key | stock | FO4CS |
|---|---|---|
| `baked` | two clock readings, seconds apart | |
| `bytes` / `lineCount` | 6,258 / 90 | 10,984 / 113 |
| `target` | `stock` | `fo4cs` |
| `census` | 4 rows | 12 rows |
| `hashes` | 1 (`loadOrderHash`) | 5 (the four corpus digests too) |
| `endFiles` / `endBytes` | 52 / 11,512,374 | 11 / 226,572,422 |
| `switchTokens` | | `--native`, `--keep-bto` |
| `chunks` | the raw `outFiles` / `outDigests` lists | |

The last row is the one that proves this is the gate. Lane BAKEREC1 gave the
record reader `outFiles` and `outDigests` beside `out`; `canon_doc()` and
`strip_digests()` were written before that and only ever knew about `out`. So
every reduction this file exists for -- the `FO4CSLOD/` prefix off, the `../`
hops off, the digests held back so `explained()` can check them one at a time --
was being undone by the raw copies sitting untouched in the same dict, and the
comparison could not survive the LAYOUT1 move no matter what the bake did.

`baked` proves the rest of it in one line. A copy of one stock record with
nothing altered but its clock:

```
$ lodgen_btofree_ledger.py same self.lodb timeshift.lodb
  FAIL the two records name the same outputs, digest for digest
```

Two bakes never share a clock reading, so `keep` and `same` could not pass on
any real pair. They pass in the suite today only where they are skipped -- both
legs that would reach them hit the version-1 rung guard first -- which is why
this stayed invisible until an FO4CS pair had to be compared for real.

FIX (`tests/spells/lodgen_btofree_ledger.py`). Two small changes, both toward
the sentence the check already prints. `canon_doc()` drops the parallel
`outFiles`/`outDigests` lists and sorts the canonical rows, since the order two
bakes happen to write their outputs in is not a property of the bytes and a
sorted multiset still fails on a dropped or duplicated row. And a new `shape()`
sorts every field the reader returns into three groups named in the file:
`SHAPE_KEYS` -- worldspace, region, dim, load order, algorithm knobs, plugins,
resources, chunk rows -- which must always match; `RUN_KEYS`, the per-run
bookkeeping (the clock, the file's own size and line count, which exe, and the
command line, whose digest is the one thing these modes ask about directly);
and `TARGET_KEYS`, what the bake target decides, dropped only under
`--fo4cs-vs-stock`. Digests are untouched and still go one by one through
`explained()`.

The group lists cannot rot into a sweep: `shape()` returns every key that none
of the three names, and the caller goes red listing it. Measured refuter: a
record given a `somethingNew` field reports `unaccounted: ['somethingNew']`.

THE REFUTERS, all four run on the gate's own pair, each red:

| doctored | result |
|---|---|
| one `.BTR` digest in the FO4CS record, leading hex digit swapped 0<->1 | **FAIL** (the gate's own refuter) |
| one `.BTO` output row deleted (113 -> 112 lines) | **FAIL** |
| the stock record given one `.lodj` row | **FAIL**, naming `1 .lodj row(s)` |
| a record field the three groups do not name | **FAIL**, naming the field |

**Before: 31 checks / 5 fails. After: 31 checks / 4 fails, and those four are section 14.** The four remaining failures
are section 14, this lane's own new checks against C1 and C2, which stay red
until the source fix in section 6.

### 1.4 The reds that are already on the record, re-measured but not re-reported as news

The brief names four standing reds that are not this lane's to discover. All
four are exactly where they were, and each is quoted here from the line that
registered it so the board can be read without the history.

**`ground_cover` -- the grass-feature red.** HANDOFF's BTOFREE1 landing line
registers it by check name: *"lodgen_ground_cover 29/4 (C1 green on the frozen
file; C2 x3 / C6a / C9 / C16 = pre-existing grass-feature red, open)"*. My run
is that set name for name and nothing else: outer block 29 checks / 4 failures,
where the fourth is the roll-up line `C3..C17 the measurements on the files`
over an inner block of 21 / 3. The six distinct failing checks are `C2 this
ground has no cover (coverMax=69)`, `C2 all three grass-free sheets are
byte-identical with --cover`, `C2 a grass-free chunk stays DXT1 (fourCC DXT5)`,
`C6a the model and the bake agree on the composite`, `C9 the slope gate is
there and it bites`, `C16 the cover survives the mip chain`. Unmoved. Not news.

**The stock `.BTO` ~6 percent silent drop.** Registered in the same landing
line as *"stock .BTO ~6 percent drop (his call)"* -- a decision owed by bungo,
not a defect anyone has been told to fix. It did not surface as a failing check
in any of the 27 gates on this run: no gate asserts a chunk count against the
plugin's placements, which is why it is a standing item rather than a red line.
I have not added one; a gate for it would be a new assertion about what a bake
should contain, and that is a design question, not an audit finding.

**`byte_gate` phase (c), the panel-versus-CLI divergence.** Registered as
*"NEW UNOWNED byte_gate phase (c) panel-vs-CLI divergence on
Commonwealth.4.-20.24.DDS + Commonwealth.lodi at identical sizes, present on
the rung too (14/3 vs 15/2)"*. My run reproduces it exactly: `panel vs command
line: 15 identical, 2 differ, 0 missing`, the two files being
`Commonwealth.4.-20.24.DDS` (174,888 vs 174,888 bytes) and `Commonwealth.lodi`
(41,638 vs 41,638). Since it was registered as UNOWNED I measured WHERE the two
files diverge, which the registration does not say
(`scratchpad/audit1_20260916/bytegate_probe.py`):

| file | bytes | differing bytes | first difference | what sits there |
|---|---|---|---|---|
| `Commonwealth.4.-20.24.DDS` | 174,888 | 123,515 | 0x80 | the first byte of the DDS payload -- the whole compressed image, not a header field |
| `Commonwealth.lodi` | 41,638 | 1,376 | 0x0C | the header CRC, i.e. the header itself already differs |

So it is not a timestamp or a path string embedded in a header: the panel and
the command line are producing different terrain-sheet PIXELS for the same
chunk, and a `.lodi` that differs from its twelfth byte. That is a finding
about the SHAPE of a known red, not a new red, and it is the one thing I would
put at the top of a follow-up lane's brief. I did not chase it here: it
reproduces on the rung as well, so it is not this campaign's doing, and running
it down means driving the panel under a harness, which is a lane of its own.

**`octahedral` F1, the impostor cube's texel span.** Registered by lane
CARDWIDTH on 2026-09-10 as *"octahedral 110/1 (F1 cube 1.78 vs bar 1, unmoved
by the fix, left red)"*. My run reads `F1: every frame of the cube spans its
predicted texels within 1 AT THE READER THRESHOLD (worst 1.74)`. Still red,
0.04 texels better than when it was registered, and left alone: closing it is a
change to how a card frame is laid out, which is a design change and out of
scope by the brief's own line.

### 1.5 The gate that cannot run

One row on the board is a gate that could not do its job at all, and the brief
asks for it as a row with its reason.

| gate | phase | reason it cannot run | whose |
|---|---|---|---|
| `byte_gate` | (a) the panel against a rung | `FAIL: no exe at release/NifSkope.before_panel1.exe` -- the rung the phase pins its bytes to is not in the tree | **KNOWN**, LAYOUT1's landing line records the same missing rung; not restorable by me, and the brief forbids deleting or manufacturing rung exes |

Measured, not assumed: `release/` holds 11 `NifSkope.before_*.exe` rungs on
this tree and `before_panel1` is not one of them, and LAYOUT1's own landing
line reads *"(a) cannot run here (no NifSkope.before_panel1.exe ...)"*.

The other two of that gate's three failures are the phase-(c) divergence in
1.4. Phase (b) ran and passed. So `byte_gate` is red for one missing exe and
one known unowned divergence, and for nothing else.

### 1.6 Three checks added this lane that are RED on the audited exe

These are not board rows in 1.1 -- they did not exist when the board was run.
They are the before-proofs the brief requires for step 6: each one is a check
written against a defect found in the diff review (section 4), each is red on
the audited exe now, and each is the thing that must go green after the fix and
red again on a broken build. They are recorded here so the board can be read
against the same gate files afterwards.

| gate | new check | before, on this exe | the defect it pins (section 4) |
|---|---|---|---|
| `lodgen_incremental.sh` | arm **(g)** `--incremental` with no value must refuse, name the switch, and write nothing | **RED**: `rc=0, 15 file(s) written under the out-dir`; the run silently full-baked | C8: a switch that takes a value, spelled last with no value, parses to an empty string |
| `lodgen_defaults.sh` | phase **(f)** a `--land-guide` value that is not one of the six | **RED**: the warning says `; off stands` while the bake that is written is the DEFAULT one, byte for byte | C4: the warning text names a fallback the code does not take |
| `lodgen_native.sh` | section **14**, a floor plus four doctored cases through `--native-verify` | **RED**, 4 of 4: both wrap cases are refused by the WRONG rule (`pad byte ... is not zero`, not the bounds), and both version-5 aggregate cases are ACCEPTED | C1 the payload-bounds add that wraps, C2 the aggregate rules gated on version 4 in a file that is version 5 |

Both of the first two carry their own floor, so neither can pass vacuously.
Phase (f)'s floor is `--land-guide off is a different bake from the default`,
which is green -- the two bakes really are distinguishable, so the third check
is comparing against something that can move. Arm (g) asserts the file COUNT
under the out-dir, not just the exit code, so a refusal that still wrote half a
bake would fail it.

## 2. A fresh end-to-end bake on three fixture regions

Ten bakes, all on the audited exe, all started after a clean
`tasklist | grep -i -E "Fallout4|NifSkope"` (rc=1, nothing running), driven by
`scratchpad/audit1_20260916/bake_all.sh` -> `bake_one.sh`, which runs one bake at
a time and samples the process's working set with PowerShell
`Get-Process` every 0.4 s for the peak. The log of the whole run is
`bake_runner.log`, which ends `BAKES-COMPLETE 2026-09-17 16:58:20`; the tables
below are generated from it and from the trees on disk by `mk_section2.py`.

The three regions (each 3x3 dim-4 chunks, 12x12 cells, `--dim 4`, defaults
otherwise):

| fixture | `--terrain-region` | what it is |
|---|---|---|
| (a) `sanctuary` | `-20 24 -9 35` | Sanctuary Hills and the woods around it -- the brief's chunk (-20,24) is its first chunk |
| (b) `coast` | `4 -28 15 -17` | the WATER region: the WETTEST 12x12-cell region in the Commonwealth, 44.9 % of its overview texels submerged, scored by `pick_regions2.py` off the shipped `Commonwealth.lodl` overview before it was picked (Sanctuary reads 1.8 %, urban 30.3 %) |
| (c) `urban` | `0 -12 11 -1` | the URBAN region: downtown Boston, the densest placement count of the three (33,123 instances against 3,526), and wet too at 30.3 % |

Command shape, verbatim from `bake_one.sh` (the FO4CS target is `--native`):

```
release/NifSkope.exe -no-gui lodgen <Fallout4.esm> --worldspace 3C \
  --terrain-region <x0> <y0> <x1> <y1> --dim 4 \
  --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" \
  --out-dir <bake>/ --tex-dir <bake>/tex --native <bake>/
```

`--keep-bto` adds that flag; the STOCK target drops `--native` (and with it the
`FO4CSLOD` tree); `sanctuary_incr` adds `--incremental` over a copy of (a)'s own
finished output (section 2.4, where it did not do what I asked it to).

### 2.1 Wall clock, peak memory, and what landed

| bake | wall | peak working set (sampler) | samples | files | bytes |
|---|---|---|---|---|---|
| `sanctuary_fo4cs` | 45 s | 1998118912 B (2.00 GB) | 110 | 56 | 233955347 |
| `coast_fo4cs` | 50 s | 2134081536 B (2.13 GB) | 118 | 57 | 233465449 |
| `urban_fo4cs` | 73 s | 2678247424 B (2.68 GB) | 177 | 57 | 245548296 |
| `sanctuary_keepbto` | 50 s | 2016555008 B (2.02 GB) | 122 | 64 | 238095780 |
| `coast_keepbto` | 56 s | 2133086208 B (2.13 GB) | 134 | 66 | 236636028 |
| `urban_keepbto` | 80 s | 2663170048 B (2.66 GB) | 192 | 66 | 256856282 |
| `sanctuary_stock` | 7 s | 1532690432 B (1.53 GB) | 17 | 53 | 11518632 |
| `coast_stock` | 11 s | 2129215488 B (2.13 GB) | 25 | 55 | 10127763 |
| `urban_stock` | 22 s | 2641244160 B (2.64 GB) | 53 | 55 | 20542011 |
| `sanctuary_incr` | 44 s | 2005393408 B (2.01 GB) | 105 | 56 | 233955362 |

| bake | stage times (s) | long pole |
|---|---|---|
| `sanctuary_fo4cs` | landscape 0.0, meshes 39.0, textures 2.7, impostors 0.0 | meshes (39.0 s) |
| `coast_fo4cs` | landscape 0.0, meshes 39.5, textures 6.3, impostors 0.0 | meshes (39.5 s) |
| `urban_fo4cs` | landscape 0.0, meshes 59.7, textures 9.7, impostors 0.0 | meshes (59.7 s) |
| `sanctuary_keepbto` | landscape 0.0, meshes 45.2, textures 2.6, impostors 0.0 | meshes (45.2 s) |
| `coast_keepbto` | landscape 0.0, meshes 44.7, textures 4.9, impostors 0.0 | meshes (44.7 s) |
| `urban_keepbto` | landscape 0.0, meshes 67.4, textures 7.6, impostors 0.0 | meshes (67.4 s) |
| `sanctuary_stock` | landscape 0.0, meshes 1.2, textures 2.5, impostors 0.0 | textures (2.5 s) |
| `coast_stock` | landscape 0.0, meshes 2.3, textures 5.0, impostors 0.0 | textures (5.0 s) |
| `urban_stock` | landscape 0.0, meshes 10.2, textures 7.6, impostors 0.0 | meshes (10.2 s) |
| `sanctuary_incr` | landscape 0.0, meshes 37.5, textures 2.6, impostors 0.0 | meshes (37.5 s) |

**`sanctuary_fo4cs`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.86 GB (1998127104 bytes), bto built in scratch E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/sanctuary_fo4cs/lodgen_bto_scratch, 8 chunk(s), 8 dropped, 4140002 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/sanctuary_fo4cs/FO4CSLOD, 19 file(s), 0 outside
```

**`coast_fo4cs`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.99 GB (2138288128 bytes), bto built in scratch E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/coast_fo4cs/lodgen_bto_scratch, 9 chunk(s), 9 dropped, 3170062 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/coast_fo4cs/FO4CSLOD, 20 file(s), 0 outside
```

**`urban_fo4cs`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 2.50 GB (2681909248 bytes), bto built in scratch E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/urban_fo4cs/lodgen_bto_scratch, 9 chunk(s), 9 dropped, 11307488 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/urban_fo4cs/FO4CSLOD, 20 file(s), 0 outside
```

**`sanctuary_keepbto`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.88 GB (2016563200 bytes), bto built in the mod folder, 8 chunk(s), 0 dropped, 0 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/sanctuary_keepbto/FO4CSLOD, 11 file(s), 0 outside
```

**`coast_keepbto`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.99 GB (2138685440 bytes), bto built in the mod folder, 9 chunk(s), 0 dropped, 0 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/coast_keepbto/FO4CSLOD, 11 file(s), 0 outside
```

**`urban_keepbto`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 2.50 GB (2680946688 bytes), bto built in the mod folder, 9 chunk(s), 0 dropped, 0 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/urban_keepbto/FO4CSLOD, 11 file(s), 0 outside
```

**`sanctuary_stock`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.43 GB (1535737856 bytes), bto built in the mod folder, 8 chunk(s), 0 dropped, 0 bytes freed, layout n/a (no FO4CS-target file written)
```

**`coast_stock`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.99 GB (2137837568 bytes), bto built in the mod folder, 9 chunk(s), 0 dropped, 0 bytes freed, layout n/a (no FO4CS-target file written)
```

**`urban_stock`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 2.46 GB (2643378176 bytes), bto built in the mod folder, 9 chunk(s), 0 dropped, 0 bytes freed, layout n/a (no FO4CS-target file written)
```

**`sanctuary_incr`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.87 GB (2005401600 bytes), bto built in scratch E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/sanctuary_incr/lodgen_bto_scratch, 8 chunk(s), 8 dropped, 4140002 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/sanctuary_incr/FO4CSLOD, 19 file(s), 0 outside
```

### Every output file of the three default FO4CS bakes

`sanctuary_fo4cs` -- 56 files, 233955347 bytes; 20 under `FO4CSLOD/`:

| file (under `FO4CSLOD/Commonwealth/`) | bytes |
|---|---|
| `Commonwealth.4.-12.24.BTO.manifest.txt` | 43349 |
| `Commonwealth.4.-12.24.lodj` | 162652 |
| `Commonwealth.4.-12.28.BTO.manifest.txt` | 40868 |
| `Commonwealth.4.-12.28.lodj` | 152871 |
| `Commonwealth.4.-12.32.BTO.manifest.txt` | 5639 |
| `Commonwealth.4.-12.32.lodj` | 20744 |
| `Commonwealth.4.-16.24.BTO.manifest.txt` | 54408 |
| `Commonwealth.4.-16.24.lodj` | 204885 |
| `Commonwealth.4.-16.28.BTO.manifest.txt` | 36428 |
| `Commonwealth.4.-16.28.lodj` | 135858 |
| `Commonwealth.4.-16.32.BTO.manifest.txt` | 2806 |
| `Commonwealth.4.-16.32.lodj` | 10550 |
| `Commonwealth.4.-20.24.BTO.manifest.txt` | 53101 |
| `Commonwealth.4.-20.24.lodj` | 199713 |
| `Commonwealth.4.-20.28.BTO.manifest.txt` | 40385 |
| `Commonwealth.4.-20.28.lodj` | 150732 |
| `Commonwealth.4.-20.32.lodj` | 64 |
| `Commonwealth.lodb` | 10553 |
| `Commonwealth.lodi` | 134598 |
| `Commonwealth.lodo` | 225399755 |

| outside the layout (class) | files | bytes | distinct sizes |
|---|---|---|---|
| `Commonwealth.<x>.<y>.24.BTR` | 3 | 98427 | 28713, 32772, 36942 |
| `Commonwealth.<x>.<y>.28.BTR` | 3 | 91144 | 29271, 29391, 32482 |
| `Commonwealth.<x>.<y>.32.BTR` | 3 | 86337 | 28593, 28731, 29013 |
| `tex/Commonwealth.<x>.<y>.24.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.24_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.24_msn.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.28.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.28_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.28_msn.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.32.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.32_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.32_msn.DDS` | 3 | 1049040 | 349680 |

`coast_fo4cs` -- 57 files, 233465449 bytes; 21 under `FO4CSLOD/`:

| file (under `FO4CSLOD/Commonwealth/`) | bytes |
|---|---|
| `Commonwealth.4.12.-20.BTO.manifest.txt` | 19748 |
| `Commonwealth.4.12.-20.lodj` | 80863 |
| `Commonwealth.4.12.-24.BTO.manifest.txt` | 8116 |
| `Commonwealth.4.12.-24.lodj` | 28702 |
| `Commonwealth.4.12.-28.BTO.manifest.txt` | 4381 |
| `Commonwealth.4.12.-28.lodj` | 15361 |
| `Commonwealth.4.4.-20.BTO.manifest.txt` | 19397 |
| `Commonwealth.4.4.-20.lodj` | 71319 |
| `Commonwealth.4.4.-24.BTO.manifest.txt` | 93637 |
| `Commonwealth.4.4.-24.lodj` | 347346 |
| `Commonwealth.4.4.-28.BTO.manifest.txt` | 21241 |
| `Commonwealth.4.4.-28.lodj` | 79032 |
| `Commonwealth.4.8.-20.BTO.manifest.txt` | 15750 |
| `Commonwealth.4.8.-20.lodj` | 58363 |
| `Commonwealth.4.8.-24.BTO.manifest.txt` | 58741 |
| `Commonwealth.4.8.-24.lodj` | 229655 |
| `Commonwealth.4.8.-28.BTO.manifest.txt` | 16073 |
| `Commonwealth.4.8.-28.lodj` | 58769 |
| `Commonwealth.lodb` | 10551 |
| `Commonwealth.lodi` | 134375 |
| `Commonwealth.lodo` | 225399755 |

| outside the layout (class) | files | bytes | distinct sizes |
|---|---|---|---|
| `Commonwealth.<x>.<y>.-20.BTR` | 3 | 134476 | 43588, 44228, 46660 |
| `Commonwealth.<x>.<y>.-24.BTR` | 3 | 125834 | 32518, 46116, 47200 |
| `Commonwealth.<x>.<y>.-28.BTR` | 3 | 138860 | 41644, 46330, 50886 |
| `tex/Commonwealth.<x>.<y>.-20.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-20_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-20_msn.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.-24.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-24_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-24_msn.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.-28.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-28_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-28_msn.DDS` | 3 | 1049040 | 349680 |

`urban_fo4cs` -- 57 files, 245548296 bytes; 21 under `FO4CSLOD/`:

| file (under `FO4CSLOD/Commonwealth/`) | bytes |
|---|---|
| `Commonwealth.4.0.-12.BTO.manifest.txt` | 35740 |
| `Commonwealth.4.0.-12.lodj` | 129993 |
| `Commonwealth.4.0.-4.BTO.manifest.txt` | 260662 |
| `Commonwealth.4.0.-4.lodj` | 958700 |
| `Commonwealth.4.0.-8.BTO.manifest.txt` | 488627 |
| `Commonwealth.4.0.-8.lodj` | 1790922 |
| `Commonwealth.4.4.-12.BTO.manifest.txt` | 195075 |
| `Commonwealth.4.4.-12.lodj` | 717733 |
| `Commonwealth.4.4.-4.BTO.manifest.txt` | 836010 |
| `Commonwealth.4.4.-4.lodj` | 3191995 |
| `Commonwealth.4.4.-8.BTO.manifest.txt` | 550469 |
| `Commonwealth.4.4.-8.lodj` | 2112696 |
| `Commonwealth.4.8.-12.BTO.manifest.txt` | 134470 |
| `Commonwealth.4.8.-12.lodj` | 496141 |
| `Commonwealth.4.8.-4.BTO.manifest.txt` | 84186 |
| `Commonwealth.4.8.-4.lodj` | 303392 |
| `Commonwealth.4.8.-8.BTO.manifest.txt` | 21747 |
| `Commonwealth.4.8.-8.lodj` | 81471 |
| `Commonwealth.lodb` | 10445 |
| `Commonwealth.lodi` | 1126755 |
| `Commonwealth.lodo` | 225399755 |

| outside the layout (class) | files | bytes | distinct sizes |
|---|---|---|---|
| `Commonwealth.<x>.<y>.-12.BTR` | 3 | 109362 | 30140, 36086, 43136 |
| `Commonwealth.<x>.<y>.-4.BTR` | 3 | 110562 | 30666, 38262, 41634 |
| `Commonwealth.<x>.<y>.-8.BTR` | 3 | 106284 | 31554, 35028, 39702 |
| `tex/Commonwealth.<x>.<y>.-12.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-12_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-12_msn.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.-4.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-4_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-4_msn.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.-8.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-8_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-8_msn.DDS` | 3 | 1049040 | 349680 |


### 2.2 The null incremental on region (a), over this lane's own fresh bake

The addendum asks for a null `--incremental` over my own fresh bake on region
(a), byte identity confirmed, the census `native-library-build:` line read in
words, and the two wall clocks recorded.

It took two runs to get, and the first one is a confirmed bug rather than a
measurement. `bake_all.sh` spelled the flag LAST, `... --native <dir>
--incremental`, which C8 parses to an empty string; that run full-baked in 44 s
and said `native-library-build: rebuilt (not offered: this is not an
incremental bake)`. Nothing on the console distinguished it from a real
incremental run. The evidence is quoted in section 4.1 and it is mine: I read a
silent full bake as a null incremental, which is a MISTAKES entry.

Spelled properly -- `--incremental <the finished bake>`, over a byte-for-byte
copy of this lane's own `sanctuary_fo4cs` tree -- the run is
`bake/sanctuary_incr2`, log `bake/sanctuary_incr2.log`:

| | full bake | null incremental |
|---|---|---|
| wall clock | **45 s** | **30 s** |
| peak working set (the census's own number) | 1.86 GB | 1.88 GB |
| chunks baked | 9 | **0** (`chunk jobs 0, chunk workers 0`) |
| native cache | 9 written to `.lodj` | **9 replayed** (3,526 placements) |
| files written under the layout | 19 | **2** |
| output files byte-identical to the bake it replayed | | **55 of 55, 0 differ**, same file list |
| the bake record's `chunk`/`out` lines | | md5 `337ae4bf...` on **both** sides |

The record is compared by its content lines rather than by `cmp` because a
record records the RUN -- its own clock, its own switch paths -- so its bytes
must move. Its output rows must not, and they did not.

The census in its own words:

```
incremental: 0 of 9 chunks dirty (0 inputs moved, 0 not in the ledger,
             0 output lost, 0 by neighbour, 0 with no native chunk cache)
native cache: 0 chunk(s) written to .lodj, 9 replayed from cache
              (3526 placement(s)), 0 failure(s)
native-library-build: rebuilt (occluders are on and the per-model box is in
                      neither file)
```

**And that last line is a finding, not a formality.** Nothing was dirty and
every chunk was replayed, yet the object library was rebuilt from scratch: of
the 30 s, `models 18.0 s` and `ladder 8.5 s` are the rebuild, 26.5 s of a 28.2 s
mesh stage. The exe names the reason itself -- occluders are ON by default and
the per-model occluder box is stored in neither the `.lodo` nor the `.lodi`, so
there is nothing on disk to reload it from. This is the incremental twin of
PERF1 row C, and it is carried into section 5 as a design gap rather than a
defect: the library reuse that exists cannot be offered under the shipped
defaults, in a full bake OR in a null incremental, and the census says so in
words every time rather than quietly reusing nothing. It is measured, not read:
the two runs differ by 15 s, and the 15 s is the nine chunks, not the library.

### 2.3 The stock target against the rung exe: byte-identical on all three regions

The stock target is a hard byte gate, so all three regions were re-baked with
`release/NifSkope.before_perf1.exe` (22,534,144 B, 2026-09-17 08:55:33 -- the
build before the audited one) on the same command line without `--native`, and
compared file by file against this lane's own step-2 stock trees:

| region | rung wall | files | identical | differ | file-list mismatch |
|---|---|---|---|---|---|
| (a) sanctuary | 7 s | 53 | **52** | **0** | none |
| (b) coast | 11 s | 55 | **54** | **0** | none |
| (c) urban | 20 s | 55 | **54** | **0** | none |

The one file excluded from each row is the `.lodb` bake record, and it is
excluded for a stated reason rather than for convenience: the rung writes the
version 1 BINARY `LODB` container and this exe writes the version 2 plain-text
record (lane BAKEREC1), so `cmp` between them can only ever say they differ. The
same exclusion, with the same reason, is what `lodgen_btofree.sh` leg (c) makes
-- see section 1.3 -- and there it is asserted by format and size rather than
waved through.

Every other byte of the stock target is unchanged across the two builds, on a
wooded region, a region that is 44.9 % water and downtown Boston.

## 3. Independent decode of every output file

Nothing in this section reads a number the writer printed. Each file is opened
by a Python reader in `tests/spells/` that works from the format documents, and
each invariant is followed by a REFUTER -- the same reader run against a doctored
copy of the same real file, which must go red. A check with no refuter is not
reported as a check.

**Two readers were written for this audit, and the brief asks me to say which.**
`tests/spells/lodgen_lodm_check.py` is the permitted new one: there was a reader
for every other type in the family -- `.lodl`, `.lodt`, `.lodo`, `.lodi`,
`.lodb`, `.lodj` -- and none for `.lodm`. `scratchpad/audit1_20260916/`
`lodj_sweep.py` is audit scaffolding rather than a gate: it drives the existing
`tests/spells/lodj_read.py` across a whole tree and cross-checks it against the
`.lodi`, which no single-file reader could do. The refuter drivers
(`refute_native.sh`, `refute_rest.sh`, `mutate_*.py`) are scaffolding too. The
readers themselves are the tree's.

### 3.1 The invariants

| # | invariant | reader | files | checks | violations | refuter, and what it did |
|---|---|---|---|---|---|---|
| 1 | `.lodl` subsample: a coarse level IS the fine sample, so it cannot hold a mean | `lodgen_lodl_pyramid.py` A | 1 | 1 (295,936 samples on 295,936 distinct slots) | **0** | A-FLOOR, three children collapsed onto one slot: **not a bijection, red** |
| 2 | `.lodl` every sampled height inside its own cell's stored range | same, B | 1 | 1 (18,496 samples, worst 0.000 u) | **0** | B-FLOOR, the range shrunk to a tenth of its span: **15,720 outside, red** |
| 3 | `.lodl` water: the dry sentinel, no sentinel where there is water, every type in WATR | same, C1--C3 | 1 | 3 | **0** | C2/C3 have subjects (36,864 water cells, 15 interned types). **C1 does not** -- see 3.3 |
| 4 | `.lodt` tiles: an absent tile is 24 zero bytes, every payload the size the header implies, 4,096-aligned, non-overlapping, pad bytes zero, CRC recomputes | `lodgen_vt_check.py tiles` | 2 | 14 | **0** | R1, one byte flipped at offset 5,898,288 of 11,796,576: **red** |
| 5 | `.lodt` a tile's border is its NEIGHBOUR's content, not a clamp of its own edge | `lodgen_vt_check.py border` | 2 | 8 | **0** | the check's own second half: border texels equal to this tile's edge must stay under 75 % |
| 6 | `.lodt` georef: row 0 is north, and two tiles are not the same tile | `lodgen_vt_check.py georef` | 2 | 4 | **0** | built into the check |
| 7 | `.lodo`/`.lodi` pair: worldspace, corpus hashes, load order, base ids, draw keys | `lodgen_native_decode.py` | 4 pairs | 24 | **0** | `lodi-wrap` and `lodo-wrap` doctored onto a real pair: **both REFUSED**, "instances/vertices runs past the file" |
| 8 | `.lodo` version 3 is refused BY NAME, not by falling off a range | same | 1 real pair stamped back to v3 | 1 | **0** | the refusal is the check: *"version 3: a v3 base row spends crossPx16[0..1] on two screen-size steps"* |
| 9 | `.lodi` fields, every word lane, against the bake's own mesh report and manifests | `lodgen_native_fields.py` | 1 pair + report + 8 manifests | **52** | **0** | groups e1 and g1 REFUSE TO RUN without `--native-mesh-report` and report themselves as FAIL rather than skipping -- see 3.2 |
| 10 | `.lodo` ladder, cluster spheres, normal cones, and the cut is a partition at nine (tolerance, distance) pairs | `lodgen_native_cut.py` | 4 pairs | 64 | **0** | four floors inside each run: a sphere shrunk 10 % (**438.3 u overshoot, red**), a cone tightened past its own worst face, tolerance 0, a huge tolerance |
| 11 | `.lodm` cards: envelope, family, kind, textures on disk, emissive, gap/pad/mips recomputed, `projection ortho`, coverage ordering, library agreement | `lodgen_lodm_check.py` (**new**) | 23 | 24 | **0** | six mutations -- magic, version, declared size, family, `projection persp`, an odd gap -- **each refused** |
| 12 | FO4CSLOD layout: everything the FO4CS target writes is under `<mod>/FO4CSLOD/<ws>/` and nothing else is | direct | 4 trees | 4 | **0** | see 3.4 |
| 13 | `.lodb` record: its sections against the tree and the log, its five corpus hashes against the `.lodo` header, written last | `lodgen_bakerec_gate.py` `sections`/`hashes`/`written-last` | 1 | **33** | **0** | R2 one hex digit of `objectCorpusHash` (**red**), R3 one `out` row deleted (**red**) |
| 14 | INCR1 `.lodj`: every cache names its own chunk, its `end` trailer equals its sections, every lighting row names a placed object, and the placements summed over the chunks equal the `.lodi` instance count | `lodj_sweep.py` over `lodj_read.py` (**new driver**) | 36 | 20 | **0** | one placement row dropped from one cache: **J2, J3 and J4 all red** |
| 15 | INCR1 byte identity: a null incremental reproduces the bake it replayed | `cmp`, section 2.2 | 55 | 55 | **0** | the record's OWN bytes do move, and are compared by content rows instead |

**Totals: 15 invariants, 76 files, 304 checks, 0 violations, and 21 refuters of
which 19 went red.** The two that did not are rows 7's aggregate pair and they
are not a gap in this section -- they are C2, and they are named in 3.5.

**CORRECTION, written in step 6 rather than left standing.** Row 11 above is
narrower than it reads. The 23 files it measured are all of kind `card`, and
the writer emits six kinds. Pointed at every `.lodm` this lane baked, the same
reader reported three FAILs and all three were the READER, not the writer: a
kind list of three, and a texture table required of a `terrainVT` sidecar that
legitimately has none. Fixed with three new refuters, in section 6.6; after it
99 sidecars read at 100 checks and 0 failures and the 23-card library still
reads 24 / 0. **Row 11 covers the card family; it did not cover the format.**

### 3.2 Two field groups needed a switch, and said so rather than passing

`lodgen_native_fields.py` on the four step-2 trees reported 35--39 checks and
**two FAILs**, `e1 a mesh report was given` and `g1 a mesh report was given`.
That is not a product defect and it is not a stale gate: `--native-mesh-report
<file>` is an opt-in on the bake, my step-2 command lines are the DEFAULTS and
do not spell it, and the reader refuses to pretend it measured a group whose
input is absent. It is the right behaviour -- a skipped check that prints `ok`
is how a suite loses coverage silently.

So the bake was made: `bake/meshrep`, region (a), `--native <dir>
--native-mesh-report <dir>/mesh_report.txt`, 39 s, a 911,262-byte report and
eight manifests. With the report and all eight manifests on the command line the
same reader on the same pair goes from 35 checks / 2 fails to **52 checks / 0
fails**. Every word lane in the `.lodi` is measured against the mesh report the
bake wrote for it.

### 3.3 One water check has no subjects, and a check with no subjects is not a check

Row 3's C1 reads *"every cell WITHOUT water writes height 0 and type 0xFFFF (0
dry cell(s), 0 broke it)"*. Zero dry cells. On the Commonwealth every one of the
36,864 cells in the 192x192 grid carries a water height, so C1 passes over an
empty set on this worldspace and always will. C2 and C3 are the two that have
subjects here (36,864 water cells with no sentinel, 15 interned types all inside
the WATR table), and they carry the row.

I am reporting C1 as VACUOUS rather than as a pass. It is not a defect in the
reader -- a worldspace with dry cells would give it subjects -- but this audit
did not exercise it, and a later lane reading "3 water checks green" would
inherit a check that has never run. That is exactly the failure mode the tree's
own rule names: a check that cannot fail on its input is not a check.

### 3.4 The layout, and the chunk with no manifest

All four FO4CS trees put every file of the target under
`<mod>/FO4CSLOD/Commonwealth/` and nothing anywhere else except where the
command line pointed `--out-dir` and `--tex-dir`. Region (a):

```
FO4CSLOD/Commonwealth/   1 .lodo   1 .lodi   1 .lodb   9 .lodj   8 .BTO.manifest.txt
```

**Nine caches and eight manifests**, which reads like a lost file until the
chunk is named. It is `Commonwealth.4.-20.32`, and both the cache and the bake
log account for it: the `.lodj` says `placements 0`, and the log says
`0 px no land, 262144 px no base tex` and `placements=0 meshes=0 shapes=0`. An
empty chunk builds no `.BTO`, so the scratch pass has nothing to leave a
manifest for; the cache is still written so that an incremental bake knows the
chunk was considered and found empty rather than never visited. Coherent, and
worth writing down because the count looks wrong at a glance.

### 3.5 What the independent readers caught that the exe did not

Row 7's refuters are the sharpest result in this section, because the two
readers of the same contract disagree about the same real file.

| doctored case | the Python decoder | the exe's `--native-verify` |
|---|---|---|
| `lodi-wrap` -- the instance blob's offset set 4,096 short of 2^64 so `off + bytes` wraps | **REFUSED**, "instances runs past the file" | **accepted, rc 0** |
| `lodo-wrap` -- the same on the `.lodo` vertex blob | **REFUSED**, "vertices runs past the file" | **accepted, rc 0** |
| `agg-views` -- a version-5 file with one aggregate at `aggregateViews 1` | accepted | accepted, rc 0 |
| `agg-record` -- a version-5 file with one all-zero 48-byte aggregate record | accepted | accepted, rc 0 |

The first two are C1 measured from the other side: the Python reader compares
`bytes > fileBytes || off > fileBytes - bytes` and the C++ at
`src/lodifile.cpp` / `src/lodofile.cpp` adds first, so the sum wraps below
`fileBytes` and the file is waved through. Two readers, one contract, opposite
answers on bytes neither of them wrote.

The last two are C2, and the Python decoder misses them for a different reason
that is worth stating so nobody counts it as a second bug: the decoder has no
`aggregateViews` rule and no aggregate-record rule **at all**. Those two rules
exist only in the C++ (`src/lodifile.cpp:888` and `:1160`), gated on version 4.
The decoder's own version handling is already right -- its header parse is
`version >= 4` and its payload table is `version == 4 or (v5 and
aggregateCount)`, which is the in-file idiom the fix in section 6 copies. So
C2's refuter is the exe's, and it is `lodgen_native.sh` section 14's two
doctored files, which are red today by design and must turn green on the fixed
build.

### 3.6 One reader is fragile on a legal file, and it is not a defect I am fixing

`lodgen_vt_check.py border` and `georef` both crashed on the default `--vt`
pyramid with a bare `TypeError: 'NoneType' object is not subscriptable`, having
run **0 checks** and exited 1 -- which on a gate board reads exactly like two
failing checks. The cause is not the product: `--vt-height` is OFF by default
(`src/lodgen.h:1341`, `src/nifcli.cpp:7882`), a version-2 container is required
to carry only the colour, msn and mask sheets (`src/io/lodvfile.cpp:592`), and
`Lodv.heights()` returns `None` when no role-4 sheet is present. Both commands
then index it.

**This is not a stale gate.** The shipped gate that owns these commands,
`tests/spells/lodgen_terrain_vt.sh`, spells `--vt-height` on its own bakes
(lines 130 and 133) and even asserts at V23 that the flag is off by default. The
gate is correct and my invocation was the gap; I re-baked with `--vt-height`
(`bake/everything/vth`, 10 s) and rows 4--6 above are measured on that pyramid,
all green. I am recording the fragility as an observation rather than fixing it,
because the brief's licence is for confirmed bugs and for STALE gates, and this
reader is neither. It is worth one line in the changelog for whoever next points
it at a default bake.

## 4. Diff review of the campaign

`git status --porcelain` at 2026-09-17 16:01: **552 entries** (61 tracked modifications under
`src/ tests/ tools/ docs/`, the rest new untracked sources and scratchpads). `git diff --stat
720762a -- src/`: **35 files, 19,371 insertions, 1,131 deletions**, plus the untracked lodgen
sources that carry no diff at all (`nativeemit.cpp` 2,349 lines, `lodofile.cpp` 2,166,
`lodifile.cpp` 1,695, `lodinative.cpp` 761, `lodgenaggregate.cpp` 696, `lodbfile.cpp` 599,
`lodgenchunkpass.cpp` 571, `lodgenparallel.cpp` 368, `lodtsheets.cpp` 296, `lodgenlayout.cpp` 125).

**Method.** The review looked for the seven classes the brief names, not for style: a write past a
buffer, an uninitialised field in a written struct, an endianness/alignment assumption in a file
writer, an off-by-one at a grid/tile/cell edge, a refusal path that does not refuse, a `WW_*` or
panel switch with no way back, and a census word that reports intent rather than what was written.
Four read-only passes were made over the four bodies of code (the `.lodo/.lodi` writer pair; the
emitter, ladder, aggregates and sheets; the record, chunk pass, parallel and layout; and the two
big diffs `src/lodgen.cpp` and `src/nifcli.cpp`). **Every row below was then re-read at its line by
this lane before it was written down**, and the ones that could not be traced to a reachable input
are marked SUSPECT rather than promoted.

### 4.1 CONFIRMED

| # | file:line | class | what breaks | reproduction | fix size |
|---|---|---|---|---|---|
| C1 | `src/lodifile.cpp:959`, `src/lodofile.cpp:1803` | write/read past a buffer | `if ( t.off + t.bytes > h.fileBytes )` adds two `quint64` and **wraps**, so a payload offset near 2^64 passes the only bounds test the reader has and is then walked (`for ( i = prevEnd; i < t.off; i++ ) p[i]`, same lines +3) or `memcpy`-ed from | set a payload offset in a `.lodi`/`.lodo` to `0xFFFFFFFFFFFFF000` (4,096-aligned, so the alignment and order tests pass) with `bytes` chosen so the sum wraps below `fileBytes`, re-sign `headerCrc32`, open it with `--native-verify` | 1 line each: compare `t.off > h.fileBytes - t.bytes` after `t.bytes > h.fileBytes` |
| C2 | `src/lodifile.cpp:1160` | a refusal path that does not refuse | the whole aggregate payload gate (identity bit, cell order, the `coveredFirst` partition, index range, double cover) is behind `if ( h.version == LODI_VERSION_AGGREGATE )`, i.e. **version 4 only**, while line 941 already admits that a **version 5** file legally carries aggregates. Lines 888..909's three header refusals (`aggregateViews >= 2`, `aggSwitchPx > 0`, `aggBandRatio > 1`) are v4-only for the same reason | `--aggregate` on any bake: placement AO is on by default, so `lodifile.cpp:608` writes version 5 and every aggregate rule above stops being enforced | 2 lines: test `h.aggregateCount` (or `iAgg >= 0`) instead of the version word, in both places |
| C3 | `src/lodgen.cpp:7902` | read past a buffer | `const Vector3 & v = sh.pos[t[k]];` subscripts a `QVector` with a triangle index read straight out of a road `.nif`, unvalidated. The sibling pass guards the identical loop at `src/lodgen.cpp:8367` (`if ( int( t.v1() ) >= sh.pos.size() ... ) continue;`); the road pass does not. `sh.uv[t[0]]` (7950) and `sh.col[t[0]]` (7973) are guarded only by `isEmpty()`, not by size | any road model under `Landscape/Roads/` whose `Triangles` array names an index >= `Num Vertices`, or whose UV/colour arrays are shorter than its vertex array; roads are ON by default, so a plain region bake reaches it | 3 lines, copied verbatim from the sibling at 8367 |
| C4 | `src/nifcli.cpp:7610` | telemetry says what did not happen | a misspelt `--land-guide` value prints `... is not one of off|drag|aspect|aspecthex|slopewarp|flatwarp; off stands` and then sets **nothing**, so the DEFAULT (`flatwarp:1.0`, lane DEFAULTS1) stands and the sheets are flatwarp sheets while the message says off | `lodgen ... --land-guide flatwrap` beside `lodgen ... --land-guide off`: exit 0 both times, different sheets | 1 line, either the message or the call |
| C5 | `src/lodgenchunkpass.cpp:156` + `src/nifcli.cpp:4389,4875` | a refusal path that does not refuse | a texture-bake failure is stored in `out.texError`, printed to stderr by the driver, and increments **no** counter; the exit code is `failed ? 1 : 0` and `failed` never moved, so the bake exits 0 having not written the textures | point `--tex-data-root` at a tree with the landscape diffuse missing | 1 line (`failed++`), but see 4.3 |
| C6 | `src/lodgenchunkpass.cpp:300` | census reports intent | `r.freed += fi.size();` is summed **before** `if ( QFile::remove( p ) ) r.dropped++;` at 321, so `bto built in scratch ..., N chunk(s), M dropped, B bytes freed` can claim bytes that are still on disk | hold one scratch `.BTO` open or mark it read-only; the clause reads `8 chunk(s), 7 dropped, 8-chunks-worth bytes freed` | 2 lines: take the size, add it inside the successful branch |
| C7 | `src/lodinative.cpp:649` (and 159, 499) | narrowing with no clamp | `bk.tris.push_back( Triangle( quint16( vstart + t[0] ), ... ) )` where `vstart = bk.verts.size()` and the bucket at `src/lodinative.cpp:542` accumulates **every placement in the loaded region** for one (mesh, material). `Triangle` is three `quint16`, so past 65,536 bucket vertices every triangle indexes a wrapped vertex. The `MAX_SHAPE_VERTS` split at 195 runs in `emitBucket`, i.e. **after** the wrap, so it is not a guard | open a `.lodi` region in the native view with more than ~65,536 vertices' worth of one base+material (any sizeable tree population) | a bucket that rolls over to a new bucket when the next part would not fit: ~6 lines. **This is the viewer, not the bake** |
| C8 | `src/nifcli.cpp:7424` | a refusal path that does not refuse | the lodgen argument loop reads a switch value with `auto next = [&]() -> QString { return ( i + 1 < a.size() ) ? a.at( ++i ) : QString(); };` , which returns an EMPTY string when the value it wants is the last token, and **no caller checks**. Measured: 142 `next()` call sites in that loop. So a value-taking switch spelled last is silently read as the empty value, and `--incremental` with no directory becomes an empty previous-bake path, which the incremental driver reads as "no previous bake" and FULL-BAKES | `lodgen <esm> --worldspace 3C --terrain-region -20 24 -17 27 --dim 4 --data-root <d> --out-dir <o> --native <o> --incremental`: exit 0, 15 files written under the out-dir, not one word of diagnostic | ~4 lines: remember the switch whose value was missing, and refuse by name after the loop |

Found while running step 2, not in the diff: `--incremental` spelled last is how this lane first wrote the null-incremental bake, and the run full-baked without complaining. It is in this table rather than in section 2 because the defect is the argument loop, not the incremental driver -- the same shape reaches every switch in the table. It has a MISTAKES entry of its own (mine, for reading a silent full bake as a null incremental) and a gate written against it, `lodgen_incremental.sh` arm (g).

The bake record of that run is the evidence, in the product's own words. Its
last two `switch` rows, read back out of
`bake/sanctuary_incr/FO4CSLOD/Commonwealth/Commonwealth.lodb`, are

```
--native   E:/.../bake/sanctuary_incr
--incremental
```

-- the flag recorded with nothing after it, which is the empty value C8
describes reaching the incremental driver. The same run's census then says

```
native cache: 9 chunk(s) written to .lodj, 0 replayed from cache (0 placement(s))
native-library-build: rebuilt (not offered: this is not an incremental bake)
```

so the exe did not merely fail to reuse the previous bake: it never saw an
incremental bake at all. Wall clock 44 s against the full bake's 45 s, over a
tree that was a byte-for-byte copy of a finished bake of the same region. A run
that reused everything and a run that reused nothing are one second apart, which
is why this is worth a refusal rather than a warning -- there is no number on
the console that would have told me.

**C1 and C2 are no longer read off the source: both are now measured against the product own reader.**
C1 -- a `.lodi` whose instance blob offset is set to `0xFFFFFFFFFFFFF000`, so that `off + bytes` wraps to
80,528 against a `fileBytes` of 134,598, gets PAST the only bounds test the reader has. It is then caught
by accident three lines later, by the pad-zero walk, which reports `pad byte at 12288 before the instance
blob is not zero` -- the wrong rule, and only because the real data at that offset happens to be non-zero.
The viewer reads with `payloadCheck = false` (`src/lodinative.cpp:407`), so the pad walk does not run there
at all and the `memcpy` at `src/lodifile.cpp:1001` is reached with the wrapped offset.
C2 -- and this one is no longer constructed at all: **the product writes the file itself, on a
default command line, and then misreads it.** `--aggregate` refuses without a real card library
(`src/nifcli.cpp:3857`), and lane SHOWCASE1 left one on disk -- 23 octahedral tree cards under
`scratchpad/showcase1_20260912/cards`, `oct 8`, `tile 512`, every one of them `projection ortho`
(independently decoded, section 3). Region (a) baked against it,
`--native <out> --impostors <cards> --aggregate` and no other switch, lands in
`scratchpad/audit1_20260916/bake/aggreal` at rc 0, and the census is happy:

```
aggregate cards: 23 tree bases with a usable ortho card set; refused 0 with no set in
  .../showcase1_20260912/cards, 0 baked through a perspective camera, 0 with no octahedral grid
```

The `.lodi` it wrote, read byte by byte at the header offsets in `src/lodifile.cpp:48-50`:

| field | offset | value |
|---|---|---|
| version | 0x04 | **5** |
| `offAggregates` / `offCovered` | 0xB0 / 0xB8 | 131,072 / 139,264 |
| `aggregateCount` / `coveredCount` | 0xC0 / 0xC4 | **97** / **3,423** |
| `aggregateStride` / `aggregateViews` | 0xC8 / 0xCA | **48** / 8 |
| `aggregateSwitch` / `aggregateBand` | 0xCC / 0xD0 | 96.0 / 1.2 |
| `offPlacementAo` / `placementAoCount` | 0xE4 / 0xEC | 155,648 / 3,526 |

Ninety-seven aggregates and 3,423 covered instances, in a file whose version is 5 because placement
AO has been on by default since DEFAULTS1. Every aggregate RULE in the reader is gated on
`h.version == LODI_VERSION_AGGREGATE`, which is 4. So on this file -- a real bake, shipped defaults,
no doctoring -- not one of them runs.

The product says so itself. `--native-verify` over that pair returns **rc 0**, and among the fields
it prints back is

```
lodi aggregateStride 0
```

while the bytes at 0xC8 hold 48. The reporting site (`src/lodifile.cpp:1289`) shares the
version-4 gate with the rules, so the exe's own report of the file contradicts the exe's own
bytes, on a file the exe had just written. That is C2 end to end, with no constructed input
anywhere in the chain.

The two doctored version-5 files of `lodgen_native.sh` section 14 stay as the gate, because a gate
needs an input that is WRONG and a real bake's aggregates are right: `aggregateViews 1` --
**accepted**, rc 0; an all-zero record with HEIGHT clear, no identity bit and no extent --
**accepted**, rc 0. The version-4 reader refuses both by name. The fix in section 6 must therefore
do two things and is measured on both: turn those two red, and leave `bake/aggreal` verifying.

### 4.2 SUSPECT (a guard may exist that this lane could not rule out, or the input is not reachable
from a vanilla corpus)

| # | file:line | class | why it is not promoted |
|---|---|---|---|
| S1 | `src/lodifile.cpp:1047,1066` | read past a buffer | `T.cellRanges[c.cellRangeOffset + k]` is indexed before the `present != h.presentChunks` refusal at 1152 that would catch the lying header; needs a hand-built `.lodi` whose `presentChunks` disagrees with its chunk table |
| S2 | `src/lodifile.cpp:289` + `:400` + `:1121` | narrowing with no clamp | the writer accepts `scale` down to 0 (`r.scale < 0.0f` is the only floor), quantises with `lround( scale * 8192 )`, and the **reader refuses `scale == 0` by name**. So a placement with `XSCL` 0 or a SCOL product below 6.1e-5 makes this writer emit a file its own reader rejects. Not promoted because this lane has not yet found such a placement in the Commonwealth: the three bakes' `max scale` and the decode of step 3 are where that is measured |
| S3 | `src/lodofile.cpp:1394` | a refusal path that leaves a half-written structure | the cluster-count refusal fires after the clusters, ladder rows and vertices were already appended; harmless only because every caller abandons the whole library on a false return |
| S4 | `src/lodgenchunkpass.cpp:428..490` | a refusal path that does not refuse | at `--threads 1` the pass's own `ok` is never set false (the only `ok = false` at 548 is in the fan-out branch, after the one-thread `return ok` at 490). Contained today because the CLI's retire callback counts `!r.btrSaved` itself (`src/nifcli.cpp:4381`); the GUI driver does not |
| S5 | `src/lodgen.cpp:13639` vs `:7773`, `:8242` | off-by-one at a grid edge | the incremental input digest reaches one cell past the chunk while the road and object-height passes gather with `margin = 2`, so a ref in the second cell outside the baked region can change an edge chunk without moving its digest. The neighbour widening in `src/nifcli.cpp` covers every case where that ref lands inside another job's chunk; only the band outside the region escapes |
| S6 | `src/lodgen.cpp:7428` | a validation that does not validate | `lodgenMsnFromCache` checks only `w > 0 && h > 0`, never against the sheet's own resolution, so an `--msn-cache` entry of the wrong side length is written as that chunk's `_msn` |
| S7 | `src/lodgenlayout.cpp:72` (via `src/lodtfile.cpp:1811`) | census reports intent | `lodgenNoteLayoutFile( path )` is called before the open that can fail, so `layout <root>, N file(s), 0 outside` counts a file the bake then failed to write |
| S8 | `src/lodbfile.cpp:50` | a refusal path that does not refuse | `while ( !( buf = f.read( 1 << 20 ) ).isEmpty() )` cannot tell a mid-file read error from EOF, so a partial FNV-1a is returned as an answer and `--native-verify` could report EDITED on a plugin it failed to read |
| S9 | `src/lodtsheets.cpp:40`, `src/nativeemit.cpp:396,1235` | endianness | a `quint32[32]` DDS header blitted with `reinterpret_cast`, and `memcmp( &r.baseType, "SCOL", 4 )` on a `quint32` fourcc. Correct on x86 only, which is the only target this fork builds for; recorded, not promoted |

### 4.3 Deliberately NOT proposed as fixes

* **C5** (`texError` not counted) changes an **exit code**, so a bake that today exits 0 with a
  stderr line would start exiting 1. That is a ruling, not an audit fix: it is a row for bungo.
* **C4** has two one-line fixes that are not the same decision -- make the message true (`the
  default stands`, which is what the two sibling messages 20 lines above say) or make the
  behaviour true (call `lodgenSetLandGuideRule( LODGEN_LANDGUIDE_OFF )`). Only the first cannot
  move a single baked byte, so only the first is proposed here; the second is bungo's call.
* Nothing in class 6 (a switch with no way back) was found: every flipped default in the campaign
  has a named reverse -- `--identity`, `--terrain-identity`, `--land-hex 0 --land-warp 0
  --land-mip-bias 0 --land-guide off`, `--road-ground-paint 1`, `--no-roads`/`--roads-legacy`,
  `--keep-bto`, `--no-native-cache`, `--library mnam`, `--native-no-placement-ao`,
  `--native-no-occluders`, `--native-ladder-foliage`, `--no-simplify`.

## 5. Design gaps against the plan, the rulings and the census

Three verdicts only: **DONE** (verified by me on a file, with the number),
**PARKED-BY-RULING** (bungo has ruled and the words are quoted), **GAP**. A row
that the page calls DONE and whose field is zero on every bake is not DONE; it
is a field nobody has looked at, and two rows moved on exactly that test.

### 5.1 Plan §5, "What the generator still owes the runtime" -- all nineteen rows

| # | the row | verdict | my evidence |
|---|---|---|---|
| 1 | per-MNAM-slot instance totals in the `.lodi` header | **DONE as a field, GAP as an instrument** | the field is right: `slotInstances` sums to `instanceCount` on 4 of 4 pairs. But see 5.2 -- no file this CLI writes can have two non-zero slots |
| 2 | a per-base full-detail triangle count | **DONE** | `fullTriangles` non-zero on **2,974 of 2,974** bases, **1,120** distinct values, max **149,282**, identical on all four trees. The plan's own numbers, re-measured |
| 3 | a card count in the `.lodo` header | **DONE as a field, GAP as a value** | `cardCount` is present at 0xD0 and reads **0** on every bake including `bake/aggreal`; 0 of 2,974 bases carry a `cardLayer`. The field exists and nothing fills it -- which is row 11, not a second defect |
| 4 | the aggregate outermost-band cards, and the forested-cell count | **GAP** | owned by CARDS-AGG. `bake/aggreal` shows the aggregate PAYLOAD path works on a real card library (97 aggregates, 3,423 covered) -- what is missing is the band-3 bake, not the writer |
| 5 | a watertight bit in the `.lodo` mesh row | **DONE** | **654 of 5,567** meshes carry `LODO_MESH_WATERTIGHT`, strictly between 0 and the mesh count, on all four trees. The plan's number, re-measured |
| 6 | the decoder refuses the downtown-Boston pair, so R3's occluder gate has no fixture | **DONE -- this row is CLOSED, and closing it is a result of this audit** | region (c) is downtown Boston and its pair carries **280 occluders**; region (b) carries **61**. The independent decoder reads both at **6 checks / 0 failures**, and `lodgen_native_cut.py` adds 17 more each. R3's occluder gate now has two fixtures on disk |
| 7 | `.BTO` chunk files still written under the FO4CS target | **DONE** | BTOFREE1. Measured in section 2: the default FO4CS bake leaves no `.BTO`, `--keep-bto` is byte-identical to the rung, the stock target is untouched |
| 8 | the resource stack cannot see a `.pbrm` or a `.lodm` | **GAP** | not exercised by this audit; no `--resource` bake was made. Reported unmeasured rather than assumed |
| 9 | far-terrain sheets DXT1/8 mips against vanilla's DXT5/10 | **GAP** | TERRAINFMT1. Not re-measured here |
| 10 | splat grading vs vanilla, mean difference 19.96/255 | **GAP** | SPLAT1. Not re-measured here |
| 11 | no bake writes a card layer; `cardCorpusHash` is 0 | **GAP, and sharper than written** | `cardCorpusHash` reads `0000000000000000` on **all four** pairs **including the one baked against 23 real ortho cards**. In `src/` the only assignment outside the synthetic fixture is the copy `src/lodofile.cpp:1571`, so the value defaults (`src/lodofile.h:326`) and nothing ever sets it. `--impostors` + `--aggregate` feeds the aggregate payload without stamping the card corpus |
| 12 | the height sheet is opt-in, and the CLI table does not list the flag | **half DONE, half PARKED-BY-RULING** | the CLI half is stale: `--vt-height` **is** in the VT CLI table (`docs/LODGEN_TERRAIN_VT.md:2328`, inside `## 5. The CLI` at 2311), with its byte cost. The bake half is ruling (e), still open. I measured the cost myself: rows 4--6 of section 3 needed a `--vt-height` pyramid and the default one has three sheets, not four |
| 13 | the asymmetric-drop proof on the (-32,0) dim-32 chunk | **GAP** | not run by this lane either; a dim-32 chunk bake is outside the three fixture regions |
| 14 | no `.lodt` container and no VT `.lodm` has ever been written to disk | **DONE -- CLOSED by this audit** | `bake/everything/vt` holds `Commonwealth.VT.2.lodt`, `Commonwealth.VT.4.lodt` and `Commonwealth.VT.lodm`, and `bake/everything/vth` the same with a height sheet. All six containers decode at 0 violations (section 3 rows 4--6). **R2 has a sample** |
| 15 | no v2 manifest and no texture-array output exists on disk | **half DONE** | the manifest half is closed: every chunk sidecar begins `# lodgen manifest 2 ws Commonwealth dim 4 chunk ...`, 8 of them per region tree, kept by the BTOFREE1 teardown. The texture-array half was not separately measured |
| 16 | the shipped FO4CS `.lodl` parser pins `kVersion = 1u` | **GAP** | an FO4CS-side row; this lane does not touch that tree (standing order: FO4CS readers come last) |
| 17 | three census words this page needs | **GAP** | a census-page lane |
| 18 | the module still composes the OLD paths | **generator half DONE** | section 3.4: all four trees put every FO4CS output under `<mod>/FO4CSLOD/Commonwealth/` and nothing elsewhere. The reader half is FO4CS's |
| 19 | a stale pair cannot name the plugin that went stale | **DONE** | BAKEREC1. `lodgen_bakerec_gate.py` reads the record against the tree at **33 checks / 0 failures**, and section 3 row 13 gives it two refuters that both go red |

### 5.2 The one row that moved from DONE to half-DONE, and the two bakes that moved it

Census §6.3 row 1 says of `slotInstances`, in its own words:

> `tests/spells/lodgen_native_fields.py` §j4 checks both that the sum holds and
> that the four MOVE -- a slot distribution that is all in one bin would pass a
> sum check and mean nothing.

The sum holds. The four do not move, and they cannot:

| bake | command | `slotInstances` |
|---|---|---|
| `bake/sanctuary_fo4cs` | region (a), `--dim 4` | **[3526, 0, 0, 0]** |
| `bake/sanctuary_dim8` | the same region, `--dim 8` | **[0, 3219, 0, 0]** |
| `bake/merge2` | `--dim 8` run into a finished `--dim 4` tree | **[0, 3219, 0, 0]** |

The slot index IS the bake dim (`src/nativeemit.cpp:1711` takes the placement's
own MNAM slot; `src/lodifile.cpp:415` tallies it). `--dim` takes ONE integer and
defaults to 4 (`src/nifcli.cpp:7476`, `:7276`); there is no `--dims`. And the
third row is the one that settles it: a second `--native` run into the same tree
**replaces** the library rather than merging into it -- 3,526 instances became
3,219, not 6,745. So **no `.lodi` this CLI can write has two non-zero slots.**

The gate passes anyway, because `j4b`'s predicate is `any(slots) and
len(set(slots)) > 1` and `set([3526, 0, 0, 0])` has two members. It is satisfied
by one non-zero slot and three zeros -- precisely the distribution the census
page names as the thing it is there to catch.

**I am not tightening it, and the reason is a measurement rather than caution:**
`[3526, 0, 0, 0]` is a CORRECT file. A predicate demanding four non-zero slots
would refuse every legitimate single-dim bake, which is every bake this audit
made and every fixture in the gates. The instrument census §6.3 promises needs a
FIXTURE that mixes dims in one library, and nothing in the CLI can produce one
today. That is the row, and it belongs to the lane that owns the multi-ring
bake, not to a one-line predicate change here.

### 5.3 Plan §6, the open rulings -- what this audit's evidence says about each

Only the rulings this lane measured something against are listed; the rest are
untouched and stay open.

| ruling | status | what I measured |
|---|---|---|
| **(e)** "Must the full Commonwealth bake run with `--vt-height`?" | **PARKED-BY-RULING, still open** | the cost is real and now measured on our own files: the default pyramid carries three sheets (colour role 1, msn role 2, mask role 5) and `heights()` returns nothing; with the flag it carries four. Section 3.6 |
| **(k)** `.BTO` under the FO4CS target -- **RULED 2026-09-16** | **PARKED-BY-RULING, and implemented** | section 2's default/`--keep-bto`/stock legs all behave as the ruling describes |
| **(j)** where the ground cover lives -- the mask sheet's alpha (shipped) or the colour sheet's | **PARKED-BY-RULING** | confirmed on disk rather than assumed: exactly ONE sheet declares a `dxgiFormatCover` different from its `dxgiFormat`, and it is the **mask** (role 5, `dxgi 71` / `dxgiCover 77`). The shipped arm, not `--vt-cover-in-color` |
| **(l)** the CLI's `--candidates` default is `missing` while the panel's is `trees` | **GAP** | lane SHOWCASE1's library on disk records `candidates trees`, so the two halves still disagree by their own written record |

### 5.4 Plan §8.2--8.4 -- all three are PARKED-BY-RULING and none is this lane's

| | ruling, quoted | this lane |
|---|---|---|
| **8.2** per-asset LOD mesh generation | bungo: *"I think it should be done on individual asset level, but if a vanilla model features a lod mesh, it wins over ours."* then *"Except tree impostors"* | **PARKED-BY-RULING**, "after the lodgen queue". Nothing in this audit touches it |
| **8.3** preview an octahedral impostor as the game will react | bungo: *"Is there a way for me to preview a generated octahedral impostor for a tree in nifskope? I want to see it reacts as it does in game."* -- the page's own answer is "Today: NO" | **PARKED-BY-RULING (ASKED)**. The 23 ortho cards this audit decoded are the input such a preview would take |
| **8.4** quadtree seams | bungo, shown the three-panel diagram: *"Stitching looks good"* | **PARKED-BY-RULING**, hybrid LOD rung 3 |

### 5.5 The one-line audit of PERF1 row C, confirmed by test

PERF1 row C says a DEFAULT full bake can never reuse a library. **Confirmed, and
the exe says so itself in two different sentences, neither of which is a
warning.** Both are read out of my own bakes' censuses:

```
full bake, region (a):          native-library-build: rebuilt (not offered: this is not an incremental bake)
null incremental, region (a):   native-library-build: rebuilt (occluders are on and the per-model box is in
                                                      neither file)
```

So the reuse is unreachable from BOTH ends. A full bake is never offered it. A
null incremental with **0 of 9 chunks dirty and all 9 replayed** is offered it
and refused, because occluders are ON by default and the per-model occluder box
is stored in neither the `.lodo` nor the `.lodi` -- there is nothing on disk to
reload. That rebuild is `models 18.0 s` + `ladder 8.5 s` = **26.5 s of a 28.2 s
mesh stage**, on a run whose chunks cost nothing.

The one line: **under the shipped defaults the library is rebuilt from scratch
on every bake of any kind, and the only thing an incremental bake saves is the
chunks.** Measured, 45 s against 30 s on region (a), and the 15 s is the nine
chunks. This is a design gap and not a defect -- the census states the reason in
words every time rather than quietly reusing nothing -- and the way to close it
is a place to store the per-model occluder box, which is a format decision and
therefore not mine.


## 6. The confirmed bugs fixed, and the exe they are in

Step 6's rule was the narrow one: a one-to-few-line fix for a CONFIRMED defect,
each with a gate that goes red before and green after, and never a design
change, a default change or a format bump. **Seven edits went in, in three
files, all of them refusals or messages; not one of them can move a baked
byte, and section 6.4 measures that claim rather than asserting it.**

### 6.1 The rung, the marker, the two builds

| | bytes | mtime | sha1 |
|---|---|---|---|
| the audited exe, rung as `release/NifSkope.before_audit1.exe` | 22,567,424 | copy taken 2026-09-17 19:27 | `a843fca68c18c2740efddcb20e9fe715b7732a22` |
| after F1--F6 (intermediate, superseded) | 22,567,424 | 2026-09-17 ~19:10 | `a03bf5cbb72bdf6ba4ea38e9ecf445dec0cbaaf7` |
| **the exe this report ends on, `release/NifSkope.exe`** | **22,567,424** | **2026-09-17 19:30:16** | **`48f7f1ab0e563bfe24aeb2003dc8b72cc41a3fd1`** |

The rung copy carries the audited exe's own bytes -- its sha1 is the one the
addendum names, so the rung is the exe this lane was handed and not a copy of
something I built. Every other `release/NifSkope.before_*.exe`,
`NifSkope.archlock1_rung.exe` and `NifSkope_inuse_2000.exe` is untouched --
counted on disk at the end of the lane, **14 files** (12 `before_*`, the
archlock rung and the in-use copy), all still present. `tasklist` was clear of
`Fallout4.exe` before each build; the `BUILDING` marker was taken before the
first build and cleared after the second. Both builds reported `make rc=0` with
zero `error:` lines.

Sources after the seven edits: `src/lodifile.cpp` 93,389 B, `src/lodofile.cpp`
98,267 B, `src/nifcli.cpp` 405,280 B, all three still CR 0.

### 6.2 The seven edits

| id | defect (section 4) | site | the edit | red before | green after |
|---|---|---|---|---|---|
| **F1** | C1 | `src/lodifile.cpp:973` | `if ( t.off + t.bytes > h.fileBytes )` becomes `if ( t.bytes > h.fileBytes \|\| t.off > h.fileBytes - t.bytes )` | `lodgen_native_doctor.py lodi-wrap` on a real pair: `--native-verify` **accepted it, rc 0** | the same file is **REFUSED by name** |
| **F2** | C1 | `src/lodofile.cpp:1807` | the same one-line rewrite in the `.lodo` table reader | `lodo-wrap`: **accepted, rc 0** | **REFUSED** |
| **F3** | C2 | `src/lodifile.cpp:896`, `:949` | `if ( h.version == LODI_VERSION_AGGREGATE )` becomes `\|\| ( v5 && h.aggregateCount )` -- the three aggregate HEADER refusals reach version 5 | `agg-views` (aggregateViews forced to 1): **accepted, rc 0** | **REFUSED** |
| **F4** | C2 | `src/lodifile.cpp:1182` | `if ( h.version == LODI_VERSION_AGGREGATE )` becomes `if ( h.aggregateCount )` -- the whole aggregate PAYLOAD gate reaches version 5 | `agg-record` (a cell-order/index violation): **accepted, rc 0** | **REFUSED** |
| **F5** | C4 | `src/nifcli.cpp:7607`, `:7629` | the `--land-guide` warning says `the default stands` instead of `off stands` | the exe printed `off stands` and then let the DEFAULT `flatwarp:1.0` stand | the message names what actually happens; no baked byte moves |
| **F6** | C8 | `src/nifcli.cpp:7430`, `:7436`, `:8051` | the lodgen arg loop remembers the switch whose value was missing; after the loop, `error: <switch> needs a value`, `return 2` | `--incremental` spelled last: **rc 0, 15 files baked, no diagnostic** | **rc 2 in 0 s, 0 files written**, `error: --incremental needs a value` |
| **F7** | found by this lane's own verifier, not by the diff | `src/lodifile.cpp:1317` | the `aggregateStride` REPORTING ternary reaches version 5 | `--native-verify` on `bake/aggreal` (97 aggregates, stride 48 in the bytes) printed `aggregateStride 0` beside `aggregateCount 97` | prints **48**, while the three no-aggregate v5 bakes still print 0 |

**F3 came within one token of refusing every default bake.** My first reading of
`src/lodifile.cpp` had `h.aggregateCount` being read only inside the v4-only
block, which would have made the new guard dead. Reading `:843-880` showed the
v5 block reads all eight aggregate words at `:868-875`, so the count is
populated -- and that is exactly why the guard is `( v5 && h.aggregateCount )`
and not `v5` alone: the v4 rules include a `aggregateCount == 0` refusal, and a
default bake is version 5 with `aggregateCount 0`. Without the `&&` clause F3
would have refused every shipped default bake. The refuter for that is block A
of section 6.3, which is the only reason I know it.

**F7 is a fix that only existed because of F1--F6.** It is not in the diff
review: nothing in the campaign's diff is wrong on its face at `:1317`. It
surfaced when block A of my verifier printed the `aggregateStride` each
legitimate file reports, to prove the new refusals had not started refusing
real files -- and `bake/aggreal` reported 0 for a word its own header holds at
48. The ternary could not simply be dropped: `aggregateStride` defaults to
`LODI_AGGREGATE_STRIDE` (48) in `src/lodifile.h:377`, not to 0, so an
unguarded print would make a version-3 file report 48 out of thin air. The word
is now printed exactly when the reader read it, which is v4 or v5.

**The exe on disk carries the edits, checked in its own bytes rather than in
the build log.** Three strings, counted in both exes with `grep -a -c`:

| string | audited exe | fixed exe |
|---|---|---|
| `off stands` | 1 | **0** |
| `the default stands` | 1 (the sibling message that already said it) | **2** |
| `needs a value` | 0 | **1** |

A build log that says `rc=0` says the compiler was happy, not that the binary
beside it is the one that was compiled. This is the cheapest instrument that
answers the second question.


### 6.3 Both directions, on the fixed exe

A fix that makes bad files refuse is half a fix; the other half is that the
good files still pass. `scratchpad/audit1_20260916/step6_verify.sh` asks both
questions of every fix, and its block A is the one that found F7. Block A
verifies five real pairs this lane baked (three default regions, the aggregate
bake, the mesh-report bake) and prints the `aggregateStride` each one reports;
block B doctors a copy of a real pair four ways with the tree's own
`tests/spells/lodgen_native_doctor.py` and requires each to be refused; block C
spells `--incremental` with no value and requires a refusal before any work;
block D spells the same switch properly and requires the incremental path to
still be taken. Verbatim, on `release/NifSkope.exe` sha1 `48f7f1ab`:

```
```
=== exe: release/NifSkope.exe
    22567424 B  Sep 17 19:30

=== A. THE LEGITIMATE FILES MUST STILL VERIFY (a fix that refuses everything is not a fix)
    sanctuary_fo4cs  rc=0  aggregateStride reported: 0
    coast_fo4cs      rc=0  aggregateStride reported: 0
    urban_fo4cs      rc=0  aggregateStride reported: 0
    aggreal          rc=0  aggregateStride reported: 48
    meshrep          rc=0  aggregateStride reported: 0

=== B. THE DOCTORED FILES MUST NOW BE REFUSED (C1 and C2)
    lodi-wrap    REFUSED  (want "instance blob runs past the file")
        native REFUSED a.lodi: instance blob runs past the file (18446744073709547520 + 84624 > 134598)
    lodo-wrap    REFUSED  (want "vertex blob runs past the file")
        native REFUSED a.lodo: vertex blob runs past the file (18446744073709547520 + 166453248 > 225399
    agg-views    REFUSED  (want "aggregateViews")
        native REFUSED a.lodi: aggregateViews 1; a card needs at least two azimuths to blend between
    agg-record   REFUSED  (want "HEIGHT is clear")
        native REFUSED a.lodi: aggregate 0 (cell 0, 0): HEIGHT is clear; every aggregate sheet carries h

=== C. C8: a valued switch spelled without its value must refuse before any work
    rc=2 in 0s, 0 file(s) written
    error: --incremental needs a value

=== D. and the SAME switch spelled properly must still be taken
    rc=0 in 31s
        incremental: 0 of 9 chunks dirty (0 inputs moved, 0 not in the ledger, 0 output lost, 0 by neighbour, 0 with no native chunk cache)
        native cache: 0 chunk(s) written to .lodj, 9 replayed from cache (3526 placement(s)), 0 failure(s), 0 arrival(s) lit by more than one chunk
        native-library-build: rebuilt (occluders are on and the per-model box is in neither file)

STEP6 0 problem(s)
```
```

Read across it: the four doctored files that the audited exe accepted at rc 0
are refused by name; the five legitimate pairs still verify at rc 0; `aggreal`
reports the stride its bytes hold (48) while the three default v5 bakes, which
carry no aggregates, still report 0; a valued switch with no value costs 0 s
and writes 0 files instead of full-baking for 44 s at exit 0; and the same
switch spelled properly still replays every chunk from the cache.


### 6.4 The writer is untouched, measured rather than argued

Every one of the seven edits is in a reader, a refusal or a message, so the
claim to test is that the BYTES a bake writes did not move. Region (a),
Sanctuary chunk (-20,24), was re-baked on the fixed exe with the same command
line as step 2's fresh bake and compared file by file against it:

```
rc=0 42s
identical 55, differ 0, missing 0, .lodb excluded 1
```

55 of 55 byte-identical, nothing differing and nothing missing. The `.lodb` is
excluded by hand and is the only exclusion: it is the bake RECORD, and it
records the run's own wall clock and exe stamp, so two runs of the same bake are
supposed to differ there. (The first attempt at this comparison reported 27
missing files; that was my command line missing `--tex-dir`, not the exe.)



### 6.5 Step 1's board, re-run on the fixed exe

Step 6's own rule: after any build, step 1's table and step 3's invariants are
run again. This board is the same 27 gates, the same runner
(`scratchpad/audit1_20260916/run_gates.sh`, which does its own `tasklist` game
check before every gate), on `release/NifSkope.exe` sha1 `48f7f1ab`, with
nothing else running on the machine.

The board is read by the same two instruments section 1.1 used -- each gate's
own `N checks, M failures` summary where it prints one, and the `ok` / `FAIL`
line counts either way -- and the BEFORE column is parsed out of section 1.1's
own table by `scratchpad/audit1_20260916/mk_board_after.py` rather than retyped,
so a row cannot move by transcription.

| gate | rc before / after | s before / after | checks/fails after | ok / FAIL lines before | ok / FAIL lines after | moved |
|---|---|---|---|---|---|---|
| `bakerec` | 0 / 0 | 243 / 301 | 0 block(s): 0/0 | 69 / 0 | 69 / 0 | same |
| `btofree` | 1 / 0 | 123 / 141 | 1 block(s): 27/0 | 18 / 4 | 27 / 0 | **MOVED** |
| `byte_gate` | 1 / 1 | 1969 / 1866 | 1 block(s): 137/0 | -- / 3 | 0 / 0 | **MOVED** |
| `card_arrays` | 0 / 0 | 5 / 5 | 0 block(s): 0/0 | 35 / 0 | 35 / 0 | same |
| `defaults` | 0 / 0 | 757 / 951 | 1 block(s): 31/0 | 28 / 0 | 31 / 0 | same |
| `farring` | 0 / 0 | 34 / 29 | 0 block(s): 0/0 | 21 / 0 | 21 / 0 | same |
| `ground_cover` | 1 / 1 | 25 / 24 | 2 block(s): 50/7 | 43 / 7 | 43 / 7 | same |
| `identity` | 0 / 0 | 2 / 2 | 0 block(s): 0/0 | 8 / 0 | 8 / 0 | same |
| `impostor_cards` | 0 / 0 | 3 / 3 | 0 block(s): 0/0 | 12 / 0 | 12 / 0 | same |
| `incremental` | 0 / 0 | 174 / 162 | 0 block(s): 0/0 | 10 / 0 | 11 / 0 | same |
| `ladder` | 0 / 0 | 125 / 122 | 4 block(s): 32/0 | 40 / 0 | 40 / 0 | same |
| `layout` | 0 / 0 | 865 / 852 | 0 block(s): 0/0 | 23 / 0 | 23 / 0 | same |
| `merge` | 1 / 0 | 20 / 19 | 0 block(s): 0/0 | 9 / 1 | 12 / 0 | **MOVED** |
| `native` | 1 / 0 | 170 / 167 | 7 block(s): 315/0 | 123 / 2 | 133 / 0 | **MOVED** |
| `native_baseline` | 0 / 0 | 11 / 11 | 0 block(s): 0/0 | 3 / 0 | 3 / 0 | same |
| `octahedral` | 1 / 1 | 69 / 67 | 0 block(s): 0/0 | 108 / 3 | 110 / 1 | **MOVED** |
| `panel_run` | 0 / 0 | 45 / 42 | 1 block(s): 137/0 | 137 / 0 | 137 / 0 | same |
| `perf` | 0 / 0 | 549 / 597 | 0 block(s): 0/0 | 11 / 0 | 11 / 0 | same |
| `resources` | 0 / 0 | 3 / 3 | 1 block(s): 4/0 | 4 / 0 | 4 / 0 | same |
| `roads` | 1 / 0 | 21 / 20 | 1 block(s): 13/0 | 10 / 1 | 13 / 0 | **MOVED** |
| `stage_times` | 1 / 0 | 34 / 38 | 1 block(s): 18/0 | 15 / 2 | 18 / 0 | **MOVED** |
| `terrain` | 0 / 0 | 13 / 12 | 1 block(s): 26/0 | 26 / 0 | 26 / 0 | same |
| `terrain_pbrm` | 0 / 0 | 34 / 8 | 1 block(s): 14/0 | 14 / 0 | 14 / 0 | same |
| `terrain_vt` | 0 / 0 | 56 / 53 | 2 block(s): 58/0 | 112 / 0 | 112 / 0 | same |
| `texture_arrays` | 0 / 0 | 7 / 7 | 0 block(s): 0/0 | 40 / 0 | 40 / 0 | same |
| `tree_sway` | 0 / 0 | 8 / 8 | 1 block(s): 4/0 | 4 / 0 | 4 / 0 | same |
| `water_subdiv` | 0 / 0 | 4 / 4 | 1 block(s): 7/0 | 7 / 0 | 7 / 0 | same |

27 gate(s) in the after board, 7 of them moved
  btofree          rc 1 -> 0, FAIL lines 4 -> 0
  byte_gate        rc 1 -> 1, FAIL lines 3 -> 0
  merge            rc 1 -> 0, FAIL lines 1 -> 0
  native           rc 1 -> 0, FAIL lines 2 -> 0
  octahedral       rc 1 -> 1, FAIL lines 3 -> 1
  roads            rc 1 -> 0, FAIL lines 1 -> 0
  stage_times      rc 1 -> 0, FAIL lines 2 -> 0

**24 of 27 gates are green and every one of the three reds is a red that was
pre-registered before this lane built anything.**

| red | FAILs | whose |
|---|---|---|
| `lodgen_ground_cover` | 7 | **KNOWN**, the four grass-feature ground-cover failures the brief names, unchanged: 43 ok / 7 FAIL before and after, same lines |
| `lodgen_byte_gate` | 3 | **KNOWN**, named by two LANDED lines. Identical failure set to the before run: the same missing `release/NifSkope.before_panel1.exe` rung, the same two `DIFFERS` rows (`Commonwealth.4.-20.24.DDS`, `Commonwealth.lodi`) at the same byte sizes, the same `byte gate failures: 3` |
| `lodgen_octahedral` | 1 | **KNOWN**, CARDWIDTH: `F1 ... worst 1.74` texels. This is the ONE of its three before-FAILs that was not a stale gate -- the other two were LAYOUT1 moving the card path, fixed in section 1.3, and they are green now |

**Seven rows moved, and every one of them moved in the direction the fix
predicted.** Five went from red to green, one lost two of its three FAILs, and
one moved only in the instrument:

| gate | before | after | why |
|---|---|---|---|
| `btofree` | rc 1, 4 FAIL | **rc 0, 27 checks / 0** | **standing red 2 of the addendum**, adjudicated STALE in 1.3: the sweep predated INCR1's `.lodj` and BAKEREC1's `.lodb` |
| `native` | rc 1, 2 FAIL, 309 checks | **rc 0, 315 checks / 0** | **standing red 1**, adjudicated STALE in 1.3 (`--native-mesh-report` is opt-in). Its section 14 -- this lane's own four new checks against C1 and C2, red by design until the source fix -- is now `ok (lodi-wrap)`, `ok (lodo-wrap)`, `ok (agg-views)`, `ok (agg-record)`, each refused BY NAME |
| `stage_times` | rc 1, 2 FAIL | **rc 0, 18 checks / 0** | **standing red 3**, adjudicated STALE in 1.3: the `.lodo`/`.lodi` pair moved under `FO4CSLOD/` |
| `merge` | rc 1, 1 FAIL | **rc 0, 12 ok / 0** | STALE, unattributed until this lane: object identity is OFF by default since DEFAULTS1 |
| `roads` | rc 1, 1 FAIL | **rc 0, 13 checks / 0** | STALE, unattributed until this lane: the bake record was swept in with the outputs |
| `octahedral` | rc 1, 3 FAIL | rc 1, **1 FAIL** | two of the three were LAYOUT1's moved card path, fixed; the third is CARDWIDTH, above |
| `byte_gate` | rc 1, 3 FAIL | rc 1, "0 FAIL" | **the instrument, not the gate.** This gate spells its failure `FAIL: no exe at ...` with a colon, and the counter's pattern wants whitespace after the word, so it reads 0 where the gate itself says `byte gate failures: 3`. Counted by hand the two runs are identical, file for file and byte for byte. Reported rather than silently corrected, because a counter that can read a red as a green is worth knowing about |

Seconds are not reproducible run to run and are not evidence here: `bakerec`
243 -> 301, `defaults` 757 -> 951, `terrain_pbrm` 34 -> 8. Nothing else was
running on the machine in either pass; the runner does its own `tasklist` check
before every gate and would have aborted the batch otherwise.

`incremental` shows 10 ok before and 11 after with no change in rc, and that is
this lane's own doing rather than drift: its leg **(g)**, the red-before gate for
F6, was added after step 1's board was taken. It reads
`(g) it refuses (rc=2), names the flag, and writes nothing` on this exe.


### 6.6 Step 3's invariants, re-run on the fixed exe's output

The invariants are re-run against `bake/sanctuary_after`, which the fixed exe
wrote, and against the three fixture trees and the aggregate bake. The readers
are the same independent Python ones section 3 used, so a number that moved
would be a number the fix moved.

| # | invariant (section 3's row) | reader | before (audited exe) | after (fixed exe) | moved |
|---|---|---|---|---|---|
| 1--3 | `.lodl` subsample, cell range, water | `lodgen_lodl_pyramid.py` | 7 checks / 0 | **7 / 0** | no |
| 4 | `.lodt` tiles | `lodgen_vt_check.py tiles` | 7 / 0 per container | **7 / 0** per container | no |
| 5 | `.lodt` borders are the neighbour's content | same, `border` | 4 / 0 per container | **4 / 0** per container | no |
| 6 | `.lodt` georef | same, `georef` | 2 / 0 per container | **2 / 0** per container | no |
| 7--8 | `.lodo`/`.lodi` pair, and the v3 refusal | `lodgen_native_decode.py` | 6 / 0 per tree, 5 trees | **6 / 0** per tree, 5 trees | no |
| 9 | `.lodi` fields, every word lane | `lodgen_native_fields.py` | 35 / 2 and 39 / 2 | **35 / 2** and **39 / 2** | no (the 2 are `e1`/`g1`, 3.2) |
| 10 | `.lodo` ladder, spheres, cones, the cut partition | `lodgen_native_cut.py` | 15 / 0 and 17 / 0 | **15 / 0** and **17 / 0** | no |
| 11 | `.lodm` cards | `lodgen_lodm_check.py` | 24 / 0 on 23 cards | **24 / 0** on 23 cards, and **100 / 0** on all 99 sidecars | the READER widened -- see below |
| 12 | FO4CSLOD layout | direct | everything under `FO4CSLOD/<ws>/` | **20 files** under `FO4CSLOD/Commonwealth/`, nothing outside `--out-dir` / `--tex-dir` | no |
| 13 | `.lodb` record: sections, hashes, written last | `lodgen_bakerec_gate.py` | 33 / 0 | **24 + 8 + 1 = 33 / 0** | no |
| 14 | INCR1 `.lodj` caches against the `.lodi` | `lodj_sweep.py` | 5 / 0, `cache 3526, .lodi 3526` | **5 / 0**, `cache 3526, .lodi 3526` | no |
| 15 | INCR1 byte identity | `cmp` | 55 of 55 | **55 of 55** (6.4) | no |

**Nothing in the decoders moved.** That is the result the section is for: seven
edits in readers, refusals and messages, and every independent reader in the
tree reads the same numbers off the same files it read before the build.


**The fixed exe's own bake decodes check for check like the audited exe's.**
`decode_all.sh` was run over `bake/sanctuary_after` (region (a), written by
`48f7f1ab`) and over the four trees section 3 measured. The two sanctuary logs
were then compared line by line on their `ok`/`FAIL`/`N checks` lines:

```
diff <(grep -E "^  ok|^  FAIL|^[0-9]+ checks" bake/decode_sanctuary_fo4cs.log) \
     <(grep -E "^  ok|^  FAIL|^[0-9]+ checks" bake/decode_sanctuary_after.log)
CHECK-FOR-CHECK IDENTICAL
```

| tree | native_decode | native_fields | native_cut |
|---|---|---|---|
| `sanctuary_after` (the fixed exe's bake) | 6 / 0 | 35 / 2 | 15 / 0 |
| `sanctuary_fo4cs` | 6 / 0 | 35 / 2 | 15 / 0 |
| `coast_fo4cs` | 6 / 0 | 39 / 2 | 17 / 0 |
| `urban_fo4cs` | 6 / 0 | 39 / 2 | 17 / 0 |
| `aggreal` | 6 / 0 | 35 / 2 | 15 / 0 |

The 2 in every `native_fields` column is the documented pair `e1`/`g1`, which
refuse to pretend they measured a group whose input (`--native-mesh-report`) was
not asked for; section 3.2 has it, and `bake/meshrep` is the tree where those
two go green.

**The `.lodj` sweep, on the fixed exe's own cache:** `5 checks, 0 failures`,
with J4 reading `cache 3526, .lodi 3526` -- the count that was my own FAIL
earlier in this lane, from a remembered offset.

**The refuters, all of them re-run:** `refute_rest.sh` `0 refuter(s) that did
NOT catch their mutation` (the `.lodt` byte flip, the `.lodb` corpus hash, a
deleted `out` row); `refute_native.sh` catches lodi-wrap and lodo-wrap in the
Python decoder and still reports agg-views and agg-record as NOT CAUGHT there,
which is correct and is the point: those two rules live in the exe, and section
6.3 block B is where the exe now refuses them.

**One reader of my own turned out to be incomplete, and step 6 is where it
showed.** `tests/spells/lodgen_lodm_check.py` -- the one reader this lane was
permitted to add -- was written against the card and array families and then
pointed, for the first time, at every `.lodm` on disk. It reported 3 FAILs, and
all three were the READER:

* `kind 'aggregate' is not one of source/card/array`. Five kinds are written,
  measured by grepping every `root.insert( "kind" )` in `src/`: `card`
  (`lodgen.cpp:3071`), `array` (`:5091`), `terrainVT` (`:11804`), `cardArray`
  (`:13479`), `aggregate` (`lodgenaggregate.cpp:643`), plus `source` as
  `io/lodmfile.cpp:56`'s default. The list now names all six; an unknown word is
  still refused, because that is how a typo in the writer would look.
* `no textures object`, twice, against the two VT sidecars. A `terrainVT`
  sidecar legitimately carries a `terrain` object and no texture table -- the
  pyramid's sheets are in the `.lodt` container beside it. The requirement is
  per kind now, and `terrainVT` must carry a `terrain` object with `cellUnits`,
  `content` and `border`, so the exemption is a rule and not a hole.

Three new refuters were added with it, and all three go red: the `terrain`
object removed, `terrain.cellUnits` removed, and a textures table added to a
terrainVT sidecar. After: **99 sidecars, 100 checks, 0 failures** (97 aggregate
+ 2 terrainVT), the 23-card library still **24 checks / 0 failures**, an array
sidecar 2 / 0, and every refuter red on every family -- 6 of 6 on a card, 7 of 7
on a terrainVT.

**That is the reader, not the writer.** Every one of those 99 files was written
by the exe and none of them changed; what changed is a reader that had only ever
been pointed at two of the six families it claims to read. A reader whose
coverage is narrower than its verdict is exactly the trap this lane wrote down
twice already.

**The landscape and VT trees, re-decoded after the build:** `everything/lodl`
`lodl_pyramid 7 / 0`; `everything/vth` both containers `vt_tiles 7 / 0`,
`vt_border 4 / 0`, `vt_georef 2 / 0` each. Identical to section 3, reader for
reader.

### 6.7 The confirmed defects this lane did NOT fix, and why

Four of the eight CONFIRMED findings in section 4 are not in the exe. Each is a
row for the director, with the reason it is a ruling rather than an audit fix:

| id | why not fixed here | what it needs |
|---|---|---|
| **C3** `src/lodgen.cpp:7902` | the patch is three lines copied verbatim from the sibling guard at `:8367`, but the input that reaches it is a **malformed road `.nif`** and no file in any shipped corpus is malformed. I could not build a gate that goes deterministically red BEFORE the fix -- an out-of-range `QVector` subscript in a release build is undefined, not reliably a crash -- and this lane's rule is that a fix without a red-before gate does not go in | a hostile-input fixture and a second build. The patch text is already written, in section 4.1 |
| **C5** `src/lodgenchunkpass.cpp:156` | the one-line fix (`failed++`) changes an **exit code**: a bake that today exits 0 with a stderr line would start exiting 1, and every harness that reads that exit code changes verdict with it | bungo's ruling, not an audit fix |
| **C6** `src/lodgenchunkpass.cpp:300` | a two-line fix, but it changes what the **census clause** reports (`B bytes freed`), and the census is what `lodgen_btofree.sh` leg (d) reads. Changing a number a gate asserts on, in the same lane that re-runs the gate, buries the evidence | a row for whoever owns the census wording; the mechanism and the repro are in section 4.1 |
| **C7** `src/lodinative.cpp:649` | this is the **viewer**, not the bake. It needs a bucket that rolls over rather than a bounds test, roughly six lines, and the region that proves it is a large tree population in the native view -- a picture, not a gate | a rung of the native-view lane |

**C3 is wider than the one line section 4 found, and this lane measured how
wide.** `src/lodgen.cpp` has ten loops over a `Triangle` array. Six of them
subscript an array with the triangle's own index -- `buried[t.v1()]` (`:3935`),
`remap[t.v1()]` (`:4030`), `s.pos[t.v1()]` (`:4118`), `pos[t.v1()]` (`:4129`),
`sh.pos[t[k]]` (`:7902`, the site section 4 names) and `verts[t.v1()]`
(`:13133`) -- and exactly ONE loop in the whole file validates the index first,
the sibling at `:8367`. The arrays come off the model unchecked: `:2149` is
`s.tris = src.getArray<Triangle>( iTris );` and nothing between there and the
subscripts compares an index against `pos.size()`.

That changes the shape of the fix, which is why it is a row and not a patch
here: the cheap correct place is the LOAD, one test beside `:2149`, not six
guards. A load-time filter is also the one that can move a baked byte -- if any
shipped `.nif` carries such a triangle today, dropping it changes that model's
LOD -- so it needs a corpus measurement first, and a measurement I did not take
is not one I will patch around.

**One SUSPECT closed by measurement while step 6 ran.** S2 said the writer
accepts a placement scale down to 0 while the reader refuses `scale == 0` by
name, and that whether the Commonwealth contains such a placement was where it
would be decided. It does not, in any region this lane baked. The quantised
word (`lround( scale * 8192 )`) read straight out of the four `.lodi` files
with the independent decoder:

| tree | placements | min | max |
|---|---|---|---|
| `sanctuary_fo4cs` | 3,526 | 2048 (0.25) | 16056 (1.96) |
| `coast_fo4cs` | 3,303 | 3277 (0.40) | 19661 (2.40) |
| `urban_fo4cs` | 33,123 | 2130 (0.26) | 16384 (2.00) |
| `aggreal` | 3,526 | 2048 (0.25) | 16056 (1.96) |

43,478 placements, not one zero and nothing within four orders of magnitude of
the 6.1e-5 quantum. S2 stays SUSPECT rather than CONFIRMED, and it stays there
on a number rather than on a hunch: the path exists in the code and no input in
this worldspace reaches it.


### 6.8 Pictures

Four, all rendered by this lane from files on disk and none of them a
screenshot of a window someone arranged by hand. The three native views are
`release/NifSkope.exe`'s own native renderer opening the `.lodi` of each
fixture region, pinned by arithmetic -- the camera centre and ortho half-width
come from the region's cell footprint (4,096 units a cell), never from a
remembered screen coordinate -- by
`scratchpad/audit1_20260916/make_images.sh`. The `.lodt` sheet is decoded in
Python by the tree's own independent reader
(`tests/spells/lodgen_vt_check.py`, `Lodv` + `decode_bc1`) and laid out at the
tile grid the container's header declares, by
`scratchpad/audit1_20260916/make_lodt_sheet.py`, so a container that decoded to
garbage would show as garbage rather than as the renderer's idea of the file.

| picture | what it is | measured |
|---|---|---|
| `images/native_sanctuary.png` | region (a) Sanctuary, the fixed exe's native view opening `bake/sanctuary_fo4cs`'s own `.lodi` | 1400x1091, 913,663 B, **18.32 % of pixels are not the background**, 35,814 distinct colours |
| `images/native_coast.png` | region (b) the south-east coast, same, on `bake/coast_fo4cs` | 1400x1091, 591,407 B, **14.23 % covered**, 50,942 distinct colours |
| `images/native_urban.png` | region (c) downtown Boston, same, on `bake/urban_fo4cs`, windowed to cells `4,-8..7,-5` | 1400x1091, 1,359,588 B, **43.54 % covered**, 88,060 distinct colours |
| `images/lodt_colour_level4.png` | the `.lodt` sheet render the brief asks for: the colour role (role 1) of `everything/vt/Commonwealth.VT.4.lodt`, mip 0, decoded from BC1 by `lodgen_vt_check.py`'s own `Lodv` reader and tiled by `scratchpad/audit1_20260916/make_lodt_sheet.py` | 3x3 tiles at 272 px, 366,314 B |
| `images/lodt_colour_level2.png` | the same container's mip 1, as a second sheet | 6x6 tiles at 136 px, 366,716 B |

The camera is arithmetic, not a remembered screen coordinate: the centre and the
ortho half-width come from each region's own cell footprint at 4,096 units a
cell, so a picture cannot be framed to flatter the bake. The coverage fraction
is there so a black frame cannot pass as a render -- and it caught one.

**The whole-region urban view is REFUSED, by name, and that is the viewer
working.** The first urban render came back at 7,066 bytes and 0.0000 covered.
The reason is in the app's own log, not in a guess:

```
[Warning] lodi objects: "this region needs more than 9500000 vertices;
          ask for a smaller WW_LODI_REGION or a coarser WW_LODI_LEVEL"
```

`src/lodinative.cpp:36` holds `MAX_TOTAL_VERTS = 9500000` and `:237` is the
refusal that names both ways back. Downtown Boston is **33,123 instances in 12
of 20 chunks against a 2,974-base, 5,567-mesh, 517,534-cluster library** -- the
exe prints that line itself on load -- where Sanctuary is 3,526 and the coast
3,303. So the picture above is the same file through `WW_LODI_REGION=4,-8,7,-5`,
a sixteen-cell window, which is the way back the message names. A refusal with
a limit, a reason and a named escape is what this should look like; the row
worth carrying forward is that the limit is reached by a real Commonwealth
region at the shipped defaults, not by a synthetic one.

## 7. Changelog text for the director, and the MISTAKES entries this lane wrote

### 7.1 Changelog text

Ready to splice into `WW_CHANGES.md` -- this lane did not touch that file, nor
`HANDOFF.md`, and did not commit or stash anything. Every number in it is
measured in the sections above.

```markdown
## The lodgen pipeline audited before the bake (2026-09-17, AUDIT1)

A final-build audit of the FO4CS bake: every lodgen gate run on the shipped exe, three fresh
end-to-end bakes on named regions, every output file decoded by a reader that is not the writer,
the whole campaign's diff read for seven defect classes, and the plan's own claims checked against
the bytes. Nothing in it is a design change: seven small edits went in, all of them refusals or
messages, and the writer is unmoved.

**Seven fixes, three files.**

- Two payload-table bounds tests added `off + bytes` in `quint64` and **wrapped**, so a `.lodi` or
  `.lodo` with a payload offset near 2^64 passed the only bounds test the reader has and was then
  walked. Now compared as `bytes > fileBytes || off > fileBytes - bytes`
  (`src/lodifile.cpp`, `src/lodofile.cpp`, one line each). Measured: a doctored pair that
  `--native-verify` **accepted at rc 0** is now refused by name.
- The aggregate rules -- the three header refusals and the whole payload gate (identity bit, cell
  order, the covered partition, index range, double cover) -- were behind `version == 4` while a
  **version 5** file legally carries aggregates, and placement AO is on by default, so version 5 is
  what a default `--aggregate` bake writes. They now test `aggregateCount`
  (`src/lodifile.cpp`, three sites). Two doctored aggregate files that verified clean now refuse.
- `--native-verify` reported `aggregateStride 0` for a version-5 file whose header holds 48, the
  lone reporting site left on the version-4 ternary. It now prints the word exactly when the reader
  read it -- v4 or v5 -- and not unguarded, because the field defaults to 48 rather than to 0 and an
  unguarded print would make a version-3 file report a stride it never read.
- A valued lodgen switch spelled **last** read the empty string and nobody checked: `--incremental`
  with no directory full-baked for 44 s at exit 0 with no diagnostic, and 141 other call sites had
  the same hole. The argument loop now remembers the switch whose value was missing and refuses by
  name: `error: --incremental needs a value`, exit 2, before any work. Measured: rc 2 in 0 s,
  0 files written, where the same command line used to write 15.
- A misspelt `--land-guide` value printed `off stands` and then let the shipped default
  (`flatwarp:1.0`) stand. The message now says `the default stands`, which is what its two siblings
  twenty lines above already said. Making the BEHAVIOUR match instead is a ruling, not an audit fix.

**Five gates were stale, and a stale gate is a defect of the test suite.** All three standing reds
the brief named, plus two nobody had attributed, were the gate asking a question a landed ruling had
already answered differently -- object identity off by default (`lodgen_merge`), the bake record
being swept in with the outputs (`lodgen_roads`), INCR1's `.lodj` and BAKEREC1's `.lodb` arriving
after the sweep was written (`lodgen_btofree`), the `.lodo/.lodi` pair moving under `FO4CSLOD/`
(`lodgen_stage_times`), and `--native-mesh-report` being opt-in (`lodgen_native`). Each was decided
by running the exe both ways, each fix carries a new refuter, and each gate is red again on a
doctored input.

**The board after the build: 27 gates, 24 green, 3 red**, against 19 green and 8 red before it. The
three that stay red are the ones that were red before this campaign and are named in the ledger: the
four grass-feature ground-cover checks (7 FAILs), the byte gate's missing `NifSkope.before_panel1.exe`
rung (3, an identical failure set to the run before the build), and CARDWIDTH's 1.74 texels (1). No
gate moved from green to red, and the writer's bytes did not move: region (a) re-baked on the fixed
exe is 55 of 55 byte-identical to the same region baked on the audited one.

**Rows for bungo, nothing done:** `cardCorpusHash` is written as zero on every bake including one
baked against 23 real cards -- nothing ever sets it; `--dim` takes one integer and a second
`--native` run replaces rather than merges, so no `.lodi` this CLI writes can carry two non-zero
slots; under the shipped defaults the object library is rebuilt on every bake of any kind, because
occluders are on and the per-model occluder box is stored in neither file; and the four CONFIRMED
defects this audit did not fix, each with its reason, are in report section 6.7.
```

### 7.2 MISTAKES entries written this lane

Ten, at the head of the repo-root `MISTAKES.md`, newest first, each written at
the moment it was recognised rather than collected at the end. They are listed
here so the director can see the shape of them without reading the ledger:

| when | the mistake | the rule it produced |
|---|---|---|
| 21:2x | a harness that threw away the message it needed: the urban native view was redirected to `/dev/null`, so an empty frame cost a second render before the exe's own refusal was read | a harness may discard the app's output only once it has captured it to a file; an empty frame is a question, not a measurement |
| 20:0x | a reader whose verdict was wider than its coverage: the `.lodm` row went into section 3 as the format's row after 23 card files, and the reader knew three of six kinds | a reader's verdict covers the inputs it was GIVEN; enumerate a format's families by grepping the writer, not by remembering |
| 19:0x | the addendum's build PATH pasted into Git Bash, where `/ucrt64/bin` does not exist | a PATH quoted for one shell is not a PATH for another; check the first directory exists before blaming the toolchain |
| 18:3x | `instanceCount` read at a remembered 0x80 instead of the decoder's 0x58, and the resulting mismatch reported as a product FAIL | when an independent number disagrees with the product, suspect the offset before the product; the tree's own decoder is the offset of record |
| 18:2x | a refuter aimed at a field its reader never reads, so it proved nothing while reading green | a refuter must mutate a byte the reader actually consumes, and it must be SEEN to go red |
| 18:1x | backslashes through a heredoc, twice in one session, against a rule already written down -- and a third time later the same evening | no text carrying a backslash or an apostrophe goes through a heredoc; a patch script asserts its inserted text is backslash-free |
| 17:5x | a bare `TypeError` traceback read as two failing checks | a traceback is a crashed reader, not a red check; read the exception before the count |
| 17:3x | CRLF spliced into an LF-only report | line endings are measured with Python byte counts, before and after every splice |
| 17:0x | the null incremental run with `--incremental` spelled LAST, so it full-baked for 44 s while I recorded it as incremental | when a run is supposed to take a different path, the proof is the line where the exe NAMES the path, not the clock |
| 16:27 | a patch script that truncated its target before it failed its own anchor check | validate every anchor before opening the target for writing, and write through a temp file plus `os.replace` |

Two of the ten turned into product or suite fixes rather than staying
confessions: 17:0x is F6, and 20:0x is the `.lodm` reader fix in 6.6.
