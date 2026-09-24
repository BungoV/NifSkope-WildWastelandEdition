# Lane CARDS-AGG — aggregate ring-3 impostors, one card set per forested cell

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, nothing committed.
Exe at launch `release/NifSkope.exe` **2026-09-11 14:48:52, 21,261,312 B**,
md5 `b9b8f55472514b1e1c2bb7a5de780737` (BAKEPERF1). Rollback rung taken ONCE at
**15:08**: `release/NifSkope.before_cards_agg.exe`, md5
`b9b8f55472514b1e1c2bb7a5de780737` — equal to the launch exe byte for byte.
Game down and zero NifSkope processes at 15:08 (`tasklist` rc=1).

---

## 0. Pre-registered gates

Written **2026-09-11 15:09**, before any census number was read and before any
line of code was written. A gate invented after the numbers are in is not a gate
(CONSTITUTION 1).

| id | gate | what fails it | the floor / control on the other side |
|---|---|---|---|
| **A1** | the census is printed BEFORE the code | section 1 of this report carries a timestamp later than section 2's | the timestamps are read from the filesystem, not typed |
| **A2** | count identity per cell | for any aggregated cell, `trees photographed into the aggregate != trees removed from ring 3's instance list` | a cell deliberately under the forest threshold must keep every per-tree instance, and the same counter must read 0 removed there |
| **A3** | calibrated picture gate | the aggregate card's silhouette coverage at ring-3 distance differs from the per-tree cards' by more than the stated tolerance | **floor** = the aggregate of a DIFFERENT (wrong) cell scored by the same metric, which must land far outside the tolerance; **ceiling** = the individual render scored against itself, which must land at ~0 |
| **A4** | height moves, identity law stated | the aggregate sheet's height channel (normal B) is constant, or absent, or the same for a tall forest and a short one; the identity law is not written into the contract | a synthetic pair of cells with a stated height difference must move the measured number in the stated direction, and a flat card must read flat |
| **A5** | `--aggregate` off is byte-identical | any output file of a region bake differs from the same bake on the rung with the switch off | the comparator is shown RED first on one flipped byte and one missing file |
| **A6** | standalone gate for the new layout / kind | the new `.lodi` table or the new `.lodm` kind is accepted when malformed, or a refusal does not NAME the rule | every mutation's refusal string must contain the named substring; a re-signed CRC so the ROW rule is what refuses, not the CRC |
| **A7** | exe newer than every changed file | any changed source under `src`/`tests`/`tools`/`res` or `NifSkope.pro` is newer than `release/NifSkope.exe`; the rung differs from the launch bytes | the sweep lists every changed file, not a sample |
| **A8** | no NifSkope left running; game down at every launch | a NifSkope process survives a step, or `Fallout4.exe` is up at a build or a bake | `tasklist` before and after every launch, logged |

Two further rules this lane holds itself to, pre-registered here:

* **The stock path is never touched.** Aggregation is CS-exclusive; `--aggregate`
  is a module switch whose OFF value is the exact way back (CONSTITUTION 7, 10).
* **A refusal with numbers and pictures is a valid deliverable.** If the
  aggregate cannot be made to match the per-tree render inside the tolerance,
  this lane reports that with the measurement and the images rather than
  shipping a card that lies.

---

## 1. Census — printed 2026-09-11 15:11 ... 15:21, BEFORE any code

**Gate A1's evidence, from the filesystem and not typed:** every census
artefact under `scratchpad/cards_agg_20260911/logs/` carries an mtime between
**15:11:33** and **15:21:23**; the first source file of the aggregate module is
stamped after this section was written and its mtime is quoted in section 2.
`census_main.cpp` and `census_cost.py` are the census's own instruments, not
the feature.

### 1.0 How the census was taken, and what checks it

It was taken **without a build**. `census.exe` links `src/esmdata.cpp` and the
vendored ESM container against Qt6Core alone (`ww-standalone-writer-gate`
step 2), so it costs no NifSkope build slot and holds no exe. It reads
`Fallout4.esm` only - no bake, no chunk, no card.

The tree rule is the shipped one: the TREE record type, or `lodgenIsTreeModel`'s
three path tests, copied verbatim from `src/lodgen.cpp:2031` with the line
quoted beside the copy. **The copy is checked against the shipped exe's own
answer**, not trusted:

| check | result |
|---|---|
| census tree bases on the region vs the exe's `--list-impostor-candidates --candidates trees` | **23 of 23, the same form ids, 0 differences** |
| the same comparison against the exe's `--candidates missing` list (the FLOOR - the comparison must be able to fail) | **89 differing lines**, 106 bases vs 23 |
| census distinct tree bases over the whole Commonwealth vs the exe's whole-worldspace tree candidate list | **36 and 36** |

Logs: `logs/census_region.log`, `logs/census_commonwealth.log`,
`logs/candidates_trees_region.txt`, `logs/candidates_missing_region.txt`,
`logs/candidates_trees_cw.txt`.

### 1.1 The population, so every count can be checked against its own total

| | 9-chunk Sanctuary region (cells -20,24 to -9,35) | whole Commonwealth |
|---|---|---|
| cells in the rectangle | 144 | 36,864 |
| REFRs read (enabled, not deleted, with a base) | 6,041 | 668,806 |
| SCOL parts expanded | 7,743 | 217,809 |
| placements whose base has a LOD model | 3,523 | 162,080 |
| **of those, TREE placements** | **3,443** | **64,662** |
| tree placements whose base has NO LOD at all (dropped, not aggregatable) | 4,336 | 99,274 |
| distinct tree bases | 23 | 36 |
| cells holding at least one placement | 102 | 4,455 |
| **cells holding at least one tree** | **102** | **3,685** |

Two of those rows deserve a sentence rather than a number.

* **99,274 tree placements in the Commonwealth have no LOD model at all** - more
  than the 64,662 that do. They are the small growth the record type also calls
  TREE. They vanish at every ring today and the aggregate cannot change that: a
  card can only be made of something that has a far representation. Naming them
  stops the aggregate's count being read as "the forest".
* **97.7 percent of the trees in the Sanctuary region arrive through SCOL
  expansion**, not as loose REFRs (7,743 parts expanded against 6,041 REFRs
  read). A census that skipped the static collections would have called
  Sanctuary bare.

### 1.2 Forested cells, by the threshold N

A cell is *forested at N* when it holds **N or more tree placements with a LOD
base**. bungo's brief proposes N = 8; the table is the whole sweep, so the
threshold is a knob he can set from evidence rather than a constant baked into
the writer.

**Whole Commonwealth:**

| N | forested cells | trees in them | share of all trees |
|---|---|---|---|
| 1 | 3,685 | 64,662 | 100.0 % |
| 2 | 3,513 | 64,490 | 99.7 % |
| 4 | 3,202 | 63,711 | 98.5 % |
| 6 | 2,900 | 62,349 | 96.4 % |
| **8** | **2,631** | **60,605** | **93.7 %** |
| 12 | 2,172 | 56,285 | 87.0 % |
| 16 | 1,734 | 50,366 | 77.9 % |
| 24 | 1,011 | 36,426 | 56.3 % |
| 32 | 575 | 24,537 | 37.9 % |
| 48 | 139 | 7,880 | 12.2 % |
| 64 | 25 | 1,800 | 2.8 % |
| 128 | 0 | 0 | 0.0 % |

**9-chunk Sanctuary region:** 102 cells / 3,443 trees at N = 1; **97 / 3,423 at
N = 8**; 57 / 2,546 at N = 32; 2 / 151 at N = 64.

**No cell in the Commonwealth holds 128 trees.** The densest holds **92**.

### 1.3 Trees per cell - the histogram

Over the 3,685 Commonwealth cells that hold at least one tree:

| trees in the cell | cells |
|---|---|
| 1 | 172 |
| 2 - 3 | 311 |
| 4 - 7 | 571 |
| 8 - 15 | 897 |
| 16 - 31 | 1,159 |
| 32 - 63 | 550 |
| 64 - 127 | 25 |

min 1, **p50 14**, p90 37, p99 61, **max 92**, mean 17.5. The Sanctuary region
is denser than the worldspace: min 2, p50 35, p90 53, p99 65, max 86, mean 33.8
- 102 of its 144 cells hold trees.

**The number this table decides:** the typical forested cell is a dozen to three
dozen trees, not hundreds. An aggregate of a 14-tree cell removes 13 quads from
the far band and costs a whole card set, which is why the cost table below is
the other half of the census.

### 1.4 The cell's own silhouette, measured

The frame law needs the silhouette a cell presents at the horizon. Both
quantities are measured, not assumed:

* **land relief inside the cell** - the range of the cell's own 33 x 33 `VHGT`
  grid: min 0, **p50 1,048**, p90 2,096, max 7,200 units (Commonwealth);
  264 / 1,152 / 1,744 / 2,536 on the Sanctuary region.
* **the biggest tree standing in the cell** - column 2 of the shipped exe's
  candidate listing, the larger of a model's horizontal radius and its half
  height: min 346, p50 916, p90 1,183, **max 1,326** units
  (`TreeElmFree01.nif`).

From them, per cell,

```
width  W = 4096*sqrt(2) + 2*maxTreeExtent   the cell seen CORNER-ON, plus the
                                            trees that overhang both edges
height H = relief + 2*maxTreeExtent         the terrain's own range plus a whole
                                            tree
```

and the existing frame law (`docs/LODGEN_CARD_SHEETS.md` 3.2) does the rest
unchanged: long side = the tile rung, short side = the smallest multiple of 16
whose inner rect is not narrower than the silhouette.

**The relief is why the aggregate frame is not a thin strip.** A median cell's
terrain moves 1,048 units under trees whose whole height is about 1,830, so the
canopy band is roughly 2,880 units tall against 8,440 wide - an aspect near
1:2.9, and the short side lands on 48 or 64 at a 128-px tile, not on 16.

### 1.5 What the views cost, and the trade between the two ways to pick them

Horizon views only, per bungo's ruling. Two ways to take them:

| | **8 azimuths x 1 elevation band** (what this lane proposes) | the hemi-octahedral grid's outer ring |
|---|---|---|
| frames per cell | **8** | 4N-4 = **28** at N = 8 |
| azimuth step | 45 degrees, uniform | 12.8 degrees mean but **not uniform**: z = 1 - max(abs u, abs v), so the whole grid BOUNDARY is the horizon, and the boundary is walked in equal steps of x+y rather than of angle, which bunches frames toward the diagonals |
| phase | the four face-on and the four corner-on directions are sampled **exactly** | the corners are exact horizon, the rest land wherever the mapping puts them |
| sheet | 3.5x smaller | 3.5x bigger for the same tile |

The phase argument decides it. A cell is a **square** footprint: its projected
width swings from 4,096 units face-on to 5,793 corner-on - **41 percent, with a
period of 90 degrees**. Eight azimuths at 0/45/90/... sample both extremes
exactly, so the two hardest views are photographed rather than interpolated.
The cost of the coarse step is that a reader blending two neighbouring frames is
up to 22.5 degrees from either; that is the number the picture gate A3 has to
answer for, and it is registered as such.

### 1.6 The sheet cost - the number bungo asked for

Three BC3 sheets a set (colour + coverage, normal + height + sway, mask), the
existing gap/mip law, **no emissive sheet** (a forest emits nothing; the `.lodm`
says `emissiveScale` 0 and a reader binds black). Script `census_cost.py`, logs
`logs/cost_cw_8views.log`, `logs/cost_region_8views.log`.

**Whole Commonwealth, 8 horizon views:**

| tile | N=1 (3,685 cells) | N=8 (2,631) | N=16 (1,734) | N=32 (575) | N=48 (139) | per cell |
|---|---|---|---|---|---|---|
| **64 px** | 190.1 MB | **135.6 MB** | 89.3 MB | 29.5 MB | 6.8 MB | ~53 KB |
| **128 px** | 793.3 MB | **571.9 MB** | 378.9 MB | 127.3 MB | 29.9 MB | ~223 KB |
| **256 px** | 3,071.4 MB | **2,216.3 MB** | 1,470.2 MB | 493.8 MB | 115.0 MB | ~860 KB |

9-chunk Sanctuary region at N = 8: **4.8 MB** at tile 64, **21.3 MB** at tile
128.

**And the comparison that makes those numbers mean something.** The per-tree
card library for the SAME trees is measured on disk, not estimated:
`scratchpad/cardwidth_20260910/cards` holds 18 tree sets at tile 128 in
**14,801,664 B over its three sheets - 0.78 MB a tree type**. All 36 tree bases
of the Commonwealth therefore cost about **28 MB, once, for the whole
worldspace**.

So at tile 128 and N = 8 the aggregate asks for **572 MB against 28 MB, twenty
times the texture budget of the thing it replaces**, and buys the removal of
**57,974 quads** from the far band. At tile 64 and N = 8 it is **135.6 MB, 4.8
times**, for the same 57,974 quads. At tile 64 and N = 32 it is **29.5 MB, about
the same as the whole per-tree library**, for 23,962 quads.

**That asymmetry is the whole finding of this census.** A per-tree card is paid
ONCE PER TREE TYPE; an aggregate is paid ONCE PER CELL. The aggregate wins on
draw submission and loses on memory, and the crossover is set by N and by the
tile. The lane therefore ships **tile 64 and N = 8 as the defaults**, makes both
a switch, and section 5 puts the choice to bungo with this table.

### 1.7 The cross-fade band (brief item 4)

Ring 3 is a runtime distance band, and no bake can state one: the `.lodi` has no
rings at all - it is one instance table with a 4-cell chunk directory, and
selection is by projected size (`docs/LODGEN_NATIVE_LODO_LODI.md` 4.4). So the
band is stated the way bungo's own screen-size spec of 10:4x states everything
else - **as a projected size with a 20 percent hysteresis** - and the file
carries the threshold, not a distance.

The rule this lane writes into the contract:

```
the aggregate is selected when the CELL's projected width falls to
aggSwitchPx (default 96 px, three quarters of a 128-px frame), and the
per-tree cards cross-fade out over aggSwitchPx .. 1.2 * aggSwitchPx.
```

At the contract's own reference projection (projectionScale = 960/tan(35 deg) =
1371.0, `docs/LODGEN_NATIVE_LODO_LODI.md` 4.4) a 4,096-unit cell is 96 px wide
at **58,500 units** and 115 px wide at **48,800 units**, so on that reference
screen the band is **48,800 to 58,500 units, about 2.4 cells wide**, and the
aggregate takes over at about **14 cells out**. Those three numbers are the
reference reading of the rule and NOT the rule: a consumer recomputes them from
its live projection, exactly as it does for the cluster cut.

### 1.8 The identity law (bungo's 08:4x ruling)

**One identity per aggregate**, not the dominant tree's.

The identity index is what the far-shadow pass keys on so a caster never shadows
itself (bungo, 08:4x). Once a cell's trees are drawn as ONE card they ARE one
caster: giving the aggregate its dominant tree's identity would make it share an
identity with that same tree's own per-tree instances, which are still drawn at
the nearer rings, and the shadow pass would then exclude the wrong pixels. So
each aggregate carries its own index, taken from the same u32 per-placement
space as 4.1c and **disjoint from it by construction**: the aggregate's identity
is `0x80000000 | aggregateIndex`, so an aggregate identity can never collide
with an instance identity and a consumer can tell which it holds from the top
bit alone.

**The sheet carries no identity channel**, and that is deliberate rather than an
omission: inside one aggregate the identity is constant by the law above, so a
per-texel channel would spend a channel to store one number. The `.lodm` says
`identity: "per-aggregate"` in words so a reader cannot mistake the absence for
a missing feature.

---

## 2. The aggregate bake and the instance table

First source file of the feature on disk: `src/lodgenaggregate.h`, written
**after** section 1 was finished (section 1's artefacts are all stamped
15:11:33 .. 15:21:23; `census_main.cpp` and `census_cost.py` are the census's
own instruments). Section written 2026-09-11 16:19.

**Gate A1, from the filesystem:** `src/lodgenaggregate.h` is stamped
**15:39:28**, `src/lodgenaggregate.cpp` **16:03:41** (its last edit), and every
census artefact under `logs/` is stamped **15:11:33 .. 15:21:23**. The census
was printed before the feature existed.

### 2.1 Cards or meshes, and which was used

**The cell's own trees' CARDS**, which is bungo's own wording: *"places their
cards with the same rotation and mirror the repetition breaking would give
them"*. The rotation is the DRAWN one (the ESM rotation times the repetition
breaker's `treeHash % 360` yaw) and the mirror is `(treeHash >> 8) & 1`, taken
from the same `NativePlacement` the chunk builder already fills in, so the bake,
the repetition breaker and the aggregate cannot drift apart.

Compositing cards does compound the card's own frame quantisation, and that is
named rather than hidden: an aggregate can only show what the cards show, so it
inherits their 12.8-degree horizon step at OCT 8 and their coverage floor. That
is precisely why the picture gate's reference is **the same cards composited
finely** and not the meshes — the aggregate is measured against the thing it
actually replaces.

### 2.2 The photograph is an orthographic COMPOSITE, not a viewport render

This is a **deviation from the brief**, taken deliberately, with three numbers
behind it. The brief says the aggregate bake is a render through the two-pass
matte hook; it is not.

1. **The render route cannot bake the worldspace.** The hook photographs one
   model per process and sleeps 1,200 ms a card. 2,631 forested cells at the
   default threshold, 8 views each, is **21,048 photographs — over seven hours
   of sleep alone**, against an object stage that measures 4.1 s on the
   nine-chunk region. bungo is going to bake the whole Commonwealth himself
   (his 10:0x ruling); an impostor stage that runs overnight is not a
   deliverable.
2. **There is nothing left to photograph.** Since lane CARDORTHO (2026-09-10)
   every card sheet is orthographic and metric — `card.projection ortho`, and
   `half`, `center`, `frameOffset` are world lengths that describe the sheet
   beside them. An orthographic composite of orthographic sheets is a
   **resample**, exact up to the resample; a re-render through a viewport adds a
   second round of pixel quantisation on top of the first.
3. **It is deterministic and holds nothing.** Two runs of one region give
   byte-identical sheets — which is what gate A5 measures against — and the pass
   needs no GL context, no window and no instance slot. The measured aggregate
   stage on the 9-chunk region is **seconds inside a 17-second bake**.

The render hook is still used for what it is for: the pictures.

### 2.3 What the compositor does

Per forested cell, per azimuth:

* the frame of each tree's card whose own direction best matches
  `R^T · eye(v)` is chosen, the mirror flips it and the sign of its right
  offset;
* the quad's place in the view is `centre + scale·(R·card.center)` plus that
  frame's own `frameOffset` along the view's right and up;
* **pass 1 is ANALYTIC** — the silhouette box of a view is the union of those
  rectangles, computed without drawing anything, which is what gives `half`,
  the frame's short side and `frameOffset` exactly;
* **pass 2 splats** every source texel into the aggregate frame with the weight
  `sampleArea / texelArea`, trees composited **back to front** by depth;
* the height channel is re-encoded through the aggregate's own `depthSpan`
  (`3 × max(boundRadius, 1024)`, the card law), and the sway weight is carried
  from the tree it came from rather than re-derived.

### 2.4 The instance table: the `.lodi` at version 4

bungo's words are *"the ring 3 instance list then holds one placement per cell
instead of one per tree"*. **There is no ring-3 instance list in this format** —
the `.lodi` is ONE instance table with a 4-cell chunk directory and selection is
by projected size (contract §4.4). So the aggregate cannot remove instances; it
**suppresses** them past its own threshold, and the file says exactly which:

* a **48-byte aggregate row** per forested cell — centre, half extents,
  depthSpan, bound radius, the cell, the view count, flags, the identity, and
  the range it owns in the covered blob;
* a **u32 covered-instance blob**, ascending inside each aggregate, naming the
  instances that aggregate stands for;
* header words at 0xB0..0xD3 — the two offsets, the two counts, the stride, the
  azimuths, `aggSwitchPx` and `aggBandRatio`.

**The version is conditional, and that is a deviation stated rather than taken
quietly.** No aggregate means version 3, byte for byte the file this writer
wrote before this lane; with aggregates it is version 4. The reader accepts both
and refuses 1 and 2 by name. The condition exists because aggregation is a
module and CONSTITUTION 10 makes its off value the exact way back — an
unconditional bump would have made `--aggregate` off a different file from the
bake before it, and there would be nothing for gate A5 to measure.

### 2.5 The switches

| switch | default | what it does |
|---|---|---|
| `--aggregate` / `--no-aggregate` | **OFF** | the module's master switch; off is byte-identical |
| `--aggregate-min N` | 8 | a cell is FORESTED at N tree placements |
| `--aggregate-tile N` | 64 | the long side of an aggregate frame, in texels |
| `--aggregate-views N` | 8 | azimuths at the horizon |

`--aggregate` without `--impostors` is **refused in words**: an aggregate is
made of the cards it replaces and cannot be invented. A card library with no
usable ortho set for any of the region's tree bases is refused the same way,
with the count of what was refused and why.

### 2.6 What is NOT in this lane

* **No panel row.** `--aggregate` is CLI-only. The LOD Generation panel has no
  aggregate control, and adding one is bungo's call (section 5).
* **The stock path is untouched.** No `.BTO`, no manifest, no atlas and no
  texture array changed; gate A5 measures it over every output file.

---

## 3. Build and gates

Written 2026-09-11 16:37.

**ONE build plus ONE counted relink.**

| | when | exe | bytes |
|---|---|---|---|
| launch / rollback rung | 14:48:52 | `release/NifSkope.before_cards_agg.exe` md5 `b9b8f55472514b1e1c2bb7a5de780737` | 21,261,312 |
| the build (qmake re-run: two NEW sources) | 15:49:39 | | 21,419,520 |
| **relink 1** — the area-weight defect gate A3's own ceiling found | **16:20:02** | `release/NifSkope.exe` md5 `3ebf175826feee6c6545cf0873635acc` | **21,419,520** |

`qmake` had to be re-run: `src/lodgenaggregate.{h,cpp}` are new and
`src/lodgen.h`, `src/nativeemit.h` and `src/nifcli.cpp` now include the new
header, which the frozen dependency list did not name.

**Consistency, on the 16:20:02 exe:** no changed file under `src`, `res`,
`tools`, `tests` or `NifSkope.pro` is newer than the exe (swept over every
changed path, not a sample); `res/style.qss` and `release/style.qss` are
byte-identical. Game down and zero NifSkope processes at every launch and at the
end.

### 3.1 The pre-registered gates

| gate | result | the controls under it |
|---|---|---|
| **A1** census before code | **PASS** | census artefacts 15:11:33..15:21:23; the first feature source 15:39:28. Read from the filesystem |
| **A2** count identity per cell | **PASS, 10 checks / 0** | 97 rows: the `.lodi` row's `coveredCount`, the `.lodm`'s `trees` and the covered blob's own length agree on every cell and in total (3,414 / 3,414 / 3,414); no instance covered twice (3,414 distinct of 3,414); **FLOOR: 135 cells under the threshold, and not one has an aggregate** |
| **A3** calibrated picture gate | **PASS, 8 checks / 0** | see 3.2 |
| **A4** height moves, identity law | **PASS** (inside A2's run) | span non-zero on 24 of 24 sheets read (124..156 of 255), it VARIES between cells (sd 8.89, so not a constant), a bigger cluster reads a deeper span (correlation **0.773**), and the FLOOR — a constant plane — reads 0 through the same arithmetic. The identity law is in the contract and gated by the standalone mutation set |
| **A5** `--aggregate` off byte-identical | **PASS** | **27 files in both trees, 0 differ, 0 only in either**, the new exe against the rung on the 9-chunk region. The comparator was shown RED first, twice: one flipped byte and one missing file |
| **A6** standalone layout / kind gate | **PASS, 18 checks / 0** + the fixture's own **21 / 0** | 13 mutations, each re-signed so the ROW RULE answers and not the CRC, each required to NAME its rule; two writes byte-identical |
| **A7** exe newer than every changed file | **PASS** | 0 stale over every changed path; the rung equals the launch bytes |
| **A8** nothing left running, game down | **PASS** | `tasklist` 0 at every step |

### 3.2 Gate A3 in full — the calibration, and what it cost

The measure is the silhouette **MASS**: the integral of coverage over the card,
in world units squared. It is threshold-free and resolution-free, which a
thresholded mask is not — a canopy at 64 texels is mostly PARTIAL coverage, so a
hard alpha test measures the test and not the bake (`ww-silhouette-compare` §3).
The first run of this gate used IoU at 0.5 and read a **ceiling of 0.507**,
which is what said the metric was wrong rather than the bake.

Reference: the same 65-tree cluster of the same cards composited at
`--aggregate-tile 1024` — **6.3 units a texel** against 100.4 for the shipped
tile-64 card, and against ~23 units a texel inside a tree's own 48-texel card
frame, so it resolves the cards better than the cards resolve themselves.

Over **128 cell-views** (16 cells x 8 azimuths):

| arm | mass error vs the reference | mean per-texel coverage error |
|---|---|---|
| known answer (reference vs itself) | **0.0000** | 0.0000 |
| known answer 2 (box filter IN THE FRAME) | **0.00013** mean, 0.00126 max | — |
| **CEILING** (that box filter read on the common grid) | 0.0138 mean, 0.0473 max | 0.0459 |
| **SUBJECT** (the shipped tile-64 aggregate) | **0.0525 mean, 0.1160 worst** | **0.0475** |
| **FLOOR** (a DIFFERENT cell, same view, same grid) | **0.3142 mean, 1.4971 max** | 0.1031 |

The floor is **6x** the subject and the subject is **at** its own ceiling per
texel (0.0475 against 0.0459). The ceiling's own decomposition is printed with
it: the box filter costs 0.00013 and the nearest-neighbour resample onto the
common grid costs the rest, which is a property of the comparison and not of
either sheet.

**THE DEFECT THIS GATE FOUND, and it was not small.** On the first build the
subject read a mass error of **0.9449** against that same ceiling of 0.0138 —
the aggregate carried 94 percent more coverage than the cluster it stood for.
The cause: each tree's layer was normalised by the NUMBER of samples that landed
in a texel, so a tree covering a tenth of a coarse texel composited as if it
covered all of it. The weight is now `sampleArea / texelArea`. That is the
counted relink, and the ceiling arm is what caught it: a subject far below its
own ceiling is a defect, and the ceiling is the only thing that can say so.

### 3.3 The baseline chain, every count against BAKEPERF1's

| harness | this exe | baseline | note |
|---|---|---|---|
| `lodgen_native.sh` | **18 / 0 PASS** | 18 / 0 | the pair still decodes, 280 occluder boxes, every floor fires |
| `lod_generation.sh` | **116 / 0 PASS** | 116 / 0 | |
| `lodgen_panel_run.sh` | **125 / 0 PASS** | 125 / 0 | the panel driven to completion twice |
| `lodgen_stage_times.sh` | **16 / 0 PASS** | 16 / 0 | |
| `lodgen_terrain.sh` | **26 / 0 PASS** | 26 / 0 | |
| `lodgen_roads.sh` | **11 / 0 PASS** | 11 / 0 | |
| `lodgen_terrain_vt.sh` | **41 / 1** | 41 / 1 | the same `V9b` red at the same count, red on the rung too |
| `lodgen_octahedral.sh` | **FAIL, 1 check: `F1 … worst 1.88`** | — | **pre-existing: the SAME check with the SAME number, 1.88, on the rung.** Control log `logs/gate_lodgen_octahedral_RUNG.log` |
| `lodgen_card_arrays` / `lodgen_identity` / `lodgen_merge` / `lodgen_texture_arrays` / `lodgen_impostor_cards` | **PASS** | PASS | |
| `ui_align.sh` | **11 / 0 PASS** | 11 / 0 | |
| `water_ui.sh` | **82 / 0 PASS** | 82 / 0 | |

**Not one count moved.** The one FAIL and the one red are both reproduced on the
rung exe, and the octahedral one is named with its control run because it is a
card-bake gate and this lane touches cards.

### 3.4 What the aggregate costs, measured on the 9-chunk Sanctuary region

| | |
|---|---|
| cells holding trees (the emitter saw) | 105 |
| forested at the default 8 | **97** |
| aggregates written | **97** |
| trees photographed | **3,414** |
| trees refused, base with no card set in the library | 9 |
| `.lodi` version | 3 -> **4** |
| `.lodi` bytes | 128,256 -> **152,920** (+24,664 = 97 rows x 48 + 3,414 x 4 + two 4,096-aligned payloads) |
| the sheets | **388 files, 8.4 MB** at tile 64 |
| the bake | 17.3 s total with aggregation on, against the same bake's own object stage |

The **ESM census taken before any of this existed** reads 97 forested cells
holding 3,423 trees on the same region: **3,414 photographed + 9 refused =
3,423**, from two instruments that share no code.

---

## 4. Pictures

All three are described before they are cited, and every number in a caption is
the number this report quotes.

### 4.1 `scratchpad/cards_agg_20260911/images/agg_cell_sheet.png` (2,264 x 692)

One cell's whole aggregate card set at the texel level (`ww-texel-picture`).
The cell is chosen BY THE METRIC — the most trees on one sheet — not by
eye: **(-18, 26), 65 trees**. Top row: the eight horizon frames' COVERAGE, the
colour sheet's alpha decoded through the coverage contract, magnified 4x,
nearest neighbour, a green-to-yellow ramp over a checkerboard so an empty texel
is visibly empty, the 2-texel margin marked red on all four sides of every
frame. Bottom row: the same eight frames' HEIGHT (the normal sheet's blue),
drawn only where the sheet is covered.

What it shows: the sheet is **512 x 48 texels** — 8 azimuths of a 64 x 48 frame
— for 65 trees; `half` 3,213.8 x 2,410.4 units; `depthSpan` 10,780;
`identity per-aggregate`; `projection ortho`; coverage 16 / 128 / 160; mean
coverage over the whole sheet **0.062**, covered texels **4,387 of 24,576**. The
silhouette turns with the azimuth — the face-on views are narrower than the
corner-on ones, which is the 41 percent swing the 8-azimuth choice exists to
sample — and the height channel is visibly PER TREE, which is what gate A4
measures.

### 4.2 `.../images/cmp_individual_vs_aggregate.png` (960 x 1,570)

Gate A3's calibrated pair, same cell, view 0, all five arms on ONE world grid:
the **REFERENCE** (the same 65 trees composited at tile 1024, 6.3 units a
texel — individual trunks and crowns are legible), the **CEILING**, the
**SUBJECT** (the shipped tile-64 card, 100.4 units a texel), the **FLOOR** (a
different cell, obviously a different forest), and the **DIFFERENCE**
`|subject − reference|`. The measured mass error is burned into each caption:
0.0000 / 0.0035 / **0.0485** / 0.1635.

### 4.3 `.../images/agg_region_map.png` (1,000 x 886)

The region from above, read back from `Commonwealth.lodi` with the INDEPENDENT
decoder: every cell that holds trees, which of them carry an aggregate row, and
how many trees each one removes from the far band. Green = an aggregate with its
own `coveredCount`; grey = trees but under the threshold; white = no tree; blue
hatching = outside the baked region (the `.lodi`'s chunk extent reaches past it
because the chunk builder hands the emitter placements from the neighbouring
cells — which is also why the bake says 105 cells hold trees where the census,
clipped to the rectangle, says 102).

### 4.4 THE PICTURE THAT WAS REFUSED, and why

The brief asks for a render-hook far view of the region with per-tree cards
beside aggregate cards. **It cannot be taken in this tree, and inventing it
would be worse than not having it.** An aggregate is DATA: nothing in NifSkope
draws a `.lodi` aggregate row, because the reconstruction path is FO4CS's and
does not exist yet — the same state lane NATIVE1b reported for the cluster
ladder ("Nothing in FO4CS reads either file, so the pictures are the decoder's
own geometry"). A render of "the aggregate cards" would be a render of geometry
this lane built for the photograph and nothing else reads. 4.2 is the honest
version of that comparison: the same cluster, the same view, the thing we ship
against the thing it replaces, with the floor and the ceiling beside it.

---

## 5. Owed / red / bungo's calls

### 5.1 THE ONE THING FOR BUNGO TO DECIDE: what the aggregate costs

The census's own table, and it is the whole argument:

* a **per-tree card is paid once per TREE TYPE**. All 36 tree bases of the
  Commonwealth cost about **28 MB**, once, measured on the library on disk
  (0.78 MB a tree type over its three sheets at tile 128);
* an **aggregate is paid once per CELL**. At the shipped defaults — tile 64,
  threshold 8 — the whole Commonwealth is **2,631 cells and 135.6 MB**, and it
  removes **57,974 quads** from the far band.

| tile | N = 8 (2,631 cells) | N = 16 (1,734) | N = 32 (575) | N = 48 (139) |
|---|---|---|---|---|
| 64 px | **135.6 MB** | 89.3 MB | 29.5 MB | 6.8 MB |
| 128 px | 571.9 MB | 378.9 MB | 127.3 MB | 29.9 MB |
| 256 px | 2,216.3 MB | 1,470.2 MB | 493.8 MB | 115.0 MB |

So the trade is **draw submission against memory**, and the crossover is the
threshold and the tile. tile 64 / N 8 costs 4.8x the whole per-tree library;
tile 64 / N 32 costs about the same as it (29.5 MB) and still removes 23,962
quads. **His call: the two numbers, or leave the defaults.** Both are switches
(`--aggregate-min`, `--aggregate-tile`) and neither needs a rebuild to change.

### 5.2 The two deviations, both stated in the contract

1. **The photograph is an orthographic COMPOSITE, not a viewport render**
   (report §2.2, contract §10.4). The render route is 21,048 photographs and
   over seven hours of sleep for the Commonwealth; the composite is seconds. If
   bungo wants the render anyway it is a different lane and a different bake
   time.
2. **The `.lodi` version word is CONDITIONAL** — 3 without aggregates, 4 with
   (Deviation 12). Every earlier bump in this format was unconditional. The
   condition is what makes `--aggregate` off byte-identical, which is the
   module rule. **If he would rather the version always moved it is one line,
   and every v3 baseline is re-pinned.**

### 5.3 Red, and owed

* **`lodgen_octahedral.sh` F1 is RED and is NOT this lane's** — the same check
  at the same number (1.88) on the rung exe, control log kept. It is the card
  bake's own reader-threshold check and it wants a lane.
* **`lodgen_terrain_vt.sh` 41/1 (V9b)** — the carried red, unchanged.
* **A relative `--impostors` path silently finds no card set.** The run with
  `--impostors scratchpad/…/cards` refused all 23 tree bases; the same run with
  the absolute path took 19. This is the relative-path trap
  `nifskope-ww-lodgen` already records, and the refusal named the directory so
  it was one line to see — but the CLI should resolve the path itself, and it
  does not. One line, not taken here because it was not in the brief and would
  have cost a second relink.
* **No panel row.** `--aggregate` is CLI-only. The LOD Generation panel has no
  aggregate control and no cost line for it.
* **The four Sanctuary tree bases with no card set** (`000a7206`, `000d9ca7`,
  `000d9ca8`, `00121551`) — 9 placements in the region — stay per-tree. The
  library on disk holds 20 of the worldspace's 36 tree types; a full
  Commonwealth bake wants a full card library first.
* **Nothing in FO4CS reads any of this.** The aggregate rows, the covered blob,
  `aggSwitchPx` and the sheets are all owed to the runtime, which is the FO4CS
  session's list. The cross-fade itself is theirs (his "Dithered cross fade is
  good", 08:3x).
* **The view-basis handedness is argued, not proved.** The composite places the
  cards and rasterises them in ONE basis, so an error there cancels; what would
  NOT cancel is a mirrored sample of each tree's own card frame. The control
  that would settle it is a ONE-TREE cell whose aggregate must reproduce that
  tree's own card frame, and it is **not run** — the picture gate's floor and
  ceiling are consistent with a correct basis (a mirrored one would have shown
  as a subject far below its ceiling), but that is evidence, not proof.

### 5.4 Not committed

Nothing committed (CONSTITUTION 8). Changed by this lane: `src/lodifile.h`,
`src/lodifile.cpp`, `src/nativeemit.h`, `src/nativeemit.cpp`, `src/lodgen.h`,
`src/lodgen.cpp`, `src/nifcli.cpp`, `NifSkope.pro` (CR count 0 before and
after), the NEW `src/lodgenaggregate.h` and `src/lodgenaggregate.cpp`, and the
three contract pages `docs/LODGEN_LODM_FORMAT.md`,
`docs/LODGEN_CARD_SHEETS.md`, `docs/LODGEN_NATIVE_LODO_LODI.md`. Everything else
is under `scratchpad/cards_agg_20260911/`.

---

## 6. Mistakes

Text for `MISTAKES.md`; the lane did not append them itself (the director
splices). Also in `scratchpad/cards_agg_20260911/MISTAKES_ENTRIES.md`.

1. **2026-09-11, lane CARDS-AGG — the composite normalised by sample COUNT and
   not by AREA, and the aggregate carried 94 percent too much coverage.** Each
   tree's layer divided its accumulated coverage by the number of samples that
   landed in a target texel, so a tree covering a tenth of a coarse texel was
   composited as if it covered all of it. Measured mass error 0.9449 against a
   ceiling of 0.0138. Found by gate A3's CEILING arm, which is the only arm that
   could have found it: the subject alone looked plausible, the floor was
   comfortably worse, and only "the subject is far below what a 64-texel frame
   could do" said anything was wrong. **The rule: a resample weights by the
   share of the TARGET's area a sample stands for, and every downsampling gate
   carries a ceiling, not just a floor.**

2. **2026-09-11, lane CARDS-AGG — the first picture gate used IoU at a hard
   alpha test and its own CEILING read 0.507.** A canopy at 64 texels is mostly
   partial coverage, so thresholding measured the threshold. `ww-silhouette-compare`
   §3 says exactly this and the lane re-derived it the expensive way. **The
   rule: when the subject is a partially covered sheet, measure a
   threshold-free quantity — the coverage MASS — and keep the thresholded number
   only as a labelled aside.**

3. **2026-09-11, lane CARDS-AGG — the BC4 alpha decoder in the gate interpolated
   with `(7-i)*a0`, which produces values above 255.** numpy said so with an
   overflow warning that was nearly ignored because "the picture looked right".
   The correct eight-value mode is `(6-i)*a0 + (i+1)*a1` over 7. **The rule: an
   out-of-range warning from a decoder is a decoder defect, not noise; a
   known-answer block (a0 = a1 = 255 must decode to 255 everywhere) costs two
   lines.**

4. **2026-09-11, lane CARDS-AGG — a mutation case's expected refusal substring
   was guessed and the reader named a DIFFERENT, correct rule.** Growing row 0's
   `coveredCount` was labelled "coveredFirst moved"; what actually answered was
   "covers instance 4, which stands in cell (2, 1)" — the cell rule, and the
   right one. `ww-standalone-writer-gate` warns about exactly this ("the
   mutation label must match the offset arithmetic, and the reader's refusal
   text is the proof, not the label"). Fixed by making the label say which rule
   answers AND adding a separate case that exercises `coveredFirst` alone.

5. **2026-09-11, lane CARDS-AGG — a relative `--impostors` path found zero card
   sets and the lane spent a round on it.** The trap is already in
   `nifskope-ww-lodgen`; the lane read the skill but did not apply it to the
   FIRST bake command it typed. **The rule: every path handed to the lodgen CLI
   is absolute, without exception, and a skill's trap list is a checklist for
   the command line, not background reading.**

---

## 7. Finished-work skill review

**Skills loaded and used:** `ww-standalone-writer-gate` (the census tool AND the
v4 fixture were both built this way — it is the reason this lane cost no build
slot to measure anything), `ww-control-calibration` (the floor/ceiling/known-
answer set of gate A3, and its instruction to decompose a control stage by
stage, which is what separated the box filter from the resample),
`ww-silhouette-compare` (the threshold-versus-downsample rule, which the lane
first ignored and then paid for), `ww-texel-picture` (the sheet picture's four
drawing rules — the checkerboard and the colour ramp are the difference between
a legible canopy and a grey wash), `nifskope-ww-build-verify` (make's own exit
code, the exe-newer sweep, the stylesheet check), `nifskope-ww-lodgen` (the
CLI's shape and its relative-path trap), `ww-contract-provenance` (hash first,
anchors beside every line number, version constants last),
`fo4cs-census-field` / `ww-census-contract` (every new census word WRITTEN and
MOVING with a refusal of its own), `ww-lodl-offline-census` (a census does not
need a build — the whole of section 1), `ww-anchored-hookup` (new code in new
files; the existing files took only the few lines that join them).

**Skills that should have existed, and what they would have saved:**

1. **`ww-downsample-gate`** — the procedure for "our coarse artefact must match
   a fine one": compare a threshold-free quantity (mass, not a thresholded
   mask); build the CEILING by box-filtering the reference to the subject's own
   pitch; decompose the ceiling into the filter and the resample; require the
   subject to be AT the ceiling and not merely inside a tolerance. Two of this
   lane's five mistakes are in that paragraph, and the next lane that
   downsamples anything — the terrain pyramid, a card mip, an atlas — will
   re-derive it. **Written this session**: `.claude/skills/ww-downsample-gate/SKILL.md` (repo tree; the director mirrors it to the live tree).

2. **`ww-module-off-is-identical`** — the shape of a feature whose off value
   must be byte-identical: the conditional version word, the payloads written
   last and only when non-empty, the CRC that folds zero bytes, the comparator
   shown red on a flipped byte and a missing file before it is believed, and
   the deviation paragraph that has to go in the contract. This lane, ROADS1 and
   BAKEPERF1 have now each built it from scratch. **Written this session**:
   `.claude/skills/ww-module-off-is-identical/SKILL.md` (repo tree; the director
   mirrors it).

**Declined, with the reason:** a skill for the aggregate composite itself. It is
one feature in one file and the next lane will extend it rather than rebuild it;
the contract section (`docs/LODGEN_CARD_SHEETS.md` §10) is the right home for
its law.
