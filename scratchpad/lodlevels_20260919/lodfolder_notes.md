# Are the meshes in Meshes\LOD the only LOD meshes?  (LODFOLDER, 2026-09-19)

**VERDICT: NO intermediate geometry exists.  Bethesda authored exactly two tiers --
the full near model, and the LOD tree.  There is nothing in between.**

Evidence, measured over every MNAM slot of every LOD-bearing record (STAT, SCOL,
MSTT, FURN, DOOR, ACTI, LIGH, TREE, FLOR, CONT, ALCH, MISC) in Fallout4.esm and all
six DLC masters -- 36,998 LOD-capable bases, 3,911 of them actually carrying LOD:

  1. **0 of 3,766** distinct MNAM mesh paths point anywhere outside an LOD folder.
     Not one LOD slot in the whole shipped game names a mesh from Architecture\,
     Props\, SetDressing\ or any other near-model folder.  The 403 paths that are
     not literally under `Meshes\LOD\` are all under `Meshes\DLC03\LOD\` or
     `Meshes\DLC04\LOD\` -- the DLCs' own LOD trees.
  2. The jump is a **cliff, not a ramp**: median triangles(LOD slot 0) /
     triangles(near MODL) = **0.038**, i.e. the very first LOD already throws away
     ~96% of the geometry (~26x simpler) in one step.
  3. Multiple LOD *levels* do exist, but only INSIDE the LOD tree and only for
     **199 of 3,911** bases (5%).  bungo is right that trees have them: 121 of
     those 199 are trees/landscape.  Architecture almost never does -- only 9
     building bases have different meshes across their slots; 2,512 repeat one
     mesh in every filled slot.

So: the meshes he is looking at in `Meshes\LOD\Architecture` ARE the only LOD
meshes for those kits.  The finer `_lod_0 / _lod_1 / _lod_2` ladder he may have
seen belongs mostly to trees, and it lives in the same LOD folder -- it is not a
separate, higher-complexity intermediate tier sitting between the LOD and the
full model.

Sources: `X:\Programs\Steam\steamapps\common\Fallout 4\Data\*.esm` (7 masters),
meshes from `E:\Tools\Fallout 4\DataUnpacked\Data\Meshes\`.  Scripts:
`lodfolder_collect.py`, `lodfolder_q1q3.py`, `lodfolder_q2.py`, `lodfolder_assemble.py`.
Triangle counts via `tests/spells/gltf_nifread.py`.  Read-only: nothing outside this
folder was written, no build, no NifSkope.

---

### Q1 -- where do MNAM (LOD) mesh paths point?

LOD-bearing bases (>=1 filled MNAM slot): 3911 of 36998 LOD-capable bases
filled-slot-count distribution: 1 slot(s): 772 bases, 2 slot(s): 2656 bases, 3 slot(s): 432 bases, 4 slot(s): 51 bases

| where | distinct MNAM mesh paths |
|---|---|
| under Meshes\LOD\ | **3363** |
| somewhere else | **403** |
| total distinct | 3766 |

"Elsewhere" paths by top-level folder under Meshes\:

| top-level folder | distinct paths |
|---|---|
| dlc03 | 216 |
| dlc04 | 187 |

Refinement -- every "elsewhere" path is a DLC LOD folder, not a non-LOD folder:

| | distinct paths |
|---|---|
| "elsewhere" paths that still have a \LOD\ component (e.g. DLC04\LOD\Architecture\...) | **403 of 403** |
| MNAM paths with NO LOD folder component ANYWHERE | **0 of 3766** |

Ten named "elsewhere" examples:

| MNAM path | slot | base editor ID | formID | plugin |
|---|---|---|---|---|
| DLC04\LOD\Landscape\SinkHole\DLC04_Sinkhole_LG01_LOD_0.nif | 0 | DLC04_SinkHole_LG01_HighTechRubble | 0101C955 | DLCNukaWorld.esm | 
| DLC04\LOD\Landscape\SinkHole\DLC04_SinkHole_LG02_LOD_0.nif | 0 | DLC04_SinkHole_LG02_HighTechRubblePile | 0101C933 | DLCNukaWorld.esm | 
| DLC04\LOD\Landscape\SinkHole\DLC04_SinkHole_LG03_LOD_0.nif | 0 | DLC04_SinkHole_LG03_HighTechRubblePile | 0101C930 | DLCNukaWorld.esm | 
| DLC04\LOD\Architecture\KiddieKingdom\DLC04_KK_Building02_lod.nif | 0 | DLC04Workshop_KK_GingerBreadHouse04 | 0105039C | DLCNukaWorld.esm | 
| DLC04\LOD\Landscape\SinkHole\DLC04_SinkHole_MD01_LOD_0.nif | 0 | DLC04_SinkHole_MD01_HighTechRubble | 0101C961 | DLCNukaWorld.esm | 
| DLC04\LOD\Architecture\BorderWall\DLC04NukaBorderMainWallStraight01_LOD.nif | 0 | DLC04NukaBorderMainWallStraight01Door | 0103E3AA | DLCNukaWorld.esm | 
| DLC04\LOD\Architecture\BorderWall\DLC04NukaBorderMainWallStraight04_LOD.nif | 0 | DLC04NukaBorderMainWallStraight04a | 01044349 | DLCNukaWorld.esm | 
| DLC04\LOD\Landscape\SinkHole\DLC04_SinkHole_SM01_LOD_0.nif | 0 | DLC04_SinkHole_SM01_HighTechRubble | 0101C95A | DLCNukaWorld.esm | 
| DLC04\LOD\Landscape\SinkHole\DLC04_SinkHole_SM02_LOD_0.nif | 0 | DLC04_SinkHole_SM02_HillGrass&Dirt | 0100D38D | DLCNukaWorld.esm | 
| DLC04\LOD\Architecture\BorderWall\DLC04NukaBorderMainWallCap01_LOD.nif | 0 | DLC04NukaBorderMainWallCap01 | 01020464 | DLCNukaWorld.esm | 

MNAM slot whose path EQUALS that same base's own near MODL path:

count = **400** slot(s) across 317 distinct bases

Of those 317 bases, **317** have a near MODL that is ITSELF a file in an LOD folder --
i.e. they are LOD-only filler statics (city-block shells such as Ticonderoga_Bld01LOD)
placed in the world, not full models being reused as their own LOD.  Only **0**
base(s) point an MNAM slot at a genuine full-detail near model.

| base editor ID | formID | plugin | slot | path (= MODL) |
|---|---|---|---|---|
| Ticonderoga_Bld01LOD | 00249A85 | Fallout4.esm | 2 | LOD\Neighborhoods\Cambridge\Ticonderoga_Bld01LOD.nif |
| Ticonderoga_Bld01LOD | 00249A85 | Fallout4.esm | 3 | LOD\Neighborhoods\Cambridge\Ticonderoga_Bld01LOD.nif |
| CITAddMetalWall01_LOD_0 | 00249674 | Fallout4.esm | 0 | LOD\Unique\CIT\CITAddMetalWall01_LOD_0.nif |
| CITAddMetalWall01_LOD_0 | 00249674 | Fallout4.esm | 1 | LOD\Unique\CIT\CITAddMetalWall01_LOD_0.nif |
| CITAddMetalWall02_LOD_0 | 00249673 | Fallout4.esm | 0 | LOD\Unique\CIT\CITAddMetalWall02_LOD_0.nif |
| CITAddMetalWall02_LOD_0 | 00249673 | Fallout4.esm | 1 | LOD\Unique\CIT\CITAddMetalWall02_LOD_0.nif |
| CITAddMetalWallDoor01_LOD_0 | 00249672 | Fallout4.esm | 0 | LOD\Unique\CIT\CITAddMetalWallDoor01_LOD_0.nif |
| CITAddMetalWallDoor01_LOD_0 | 00249672 | Fallout4.esm | 1 | LOD\Unique\CIT\CITAddMetalWallDoor01_LOD_0.nif |
| WaterFront_Bld40LOD | 00240496 | Fallout4.esm | 2 | LOD\Neighborhoods\WaterFront\WaterFront_Bld40LOD.nif |
| WaterFront_Bld40LOD | 00240496 | Fallout4.esm | 3 | LOD\Neighborhoods\WaterFront\WaterFront_Bld40LOD.nif |


---

### Q2 -- do the four MNAM slots hold DIFFERENT meshes?

| pattern | bases |
|---|---|
| only ONE filled slot | **772** |
| >=2 filled slots, the SAME mesh repeated in every filled slot | **2940** |
| >=2 filled slots, at least two DIFFERENT meshes | **199** |
| total LOD-bearing bases | 3911 |

#### by category (classified from the model path)

| category | multi-mesh across slots | same mesh repeated | single filled slot | total |
|---|---|---|---|---|
| trees/landscape | **121** | 123 | 95 | 339 |
| architecture/buildings | **9** | 2512 | 150 | 2671 |
| vehicles | **0** | 11 | 0 | 11 |
| other | **69** | 294 | 527 | 890 |
| **all** | **199** | 2940 | 772 | 3911 |

#### five TREE / landscape multi-level examples (triangles read from the .nif)

| base editor ID | formID | plugin | slot 0 | slot 1 | slot 2 | slot 3 |
|---|---|---|---|---|---|---|
| TreeElmForestPreWar01Gr | 0006164B | Fallout4.esm | TreeElmFrstPW01Gr_LOD_0.nif<br>(234 tris) | TreeElmFrstPW01Gr_LOD_1.nif<br>(95 tris) | TreeElmFrstPW01Gr_LOD_2.nif<br>(4 tris) | TreeElmFrstPW01Gr_LOD_2.nif<br>(4 tris) |
| TreeElmForestPreWar01Yw | 0006164A | Fallout4.esm | TreeElmFrstPW01Yw_LOD_0.nif<br>(234 tris) | TreeElmFrstPW01Yw_LOD_1.nif<br>(95 tris) | TreeElmFrstPW01Yw_LOD_2.nif<br>(4 tris) | TreeElmFrstPW01Yw_LOD_2.nif<br>(4 tris) |
| TreeElmForestPreWar02Gr | 0006164D | Fallout4.esm | TreeElmFrstPW02Gr_LOD_0.nif<br>(168 tris) | TreeElmFrstPW02Gr_LOD_1.nif<br>(81 tris) | TreeElmFrstPW02Gr_LOD_2.nif<br>(4 tris) | TreeElmFrstPW02Gr_LOD_2.nif<br>(4 tris) |
| TreeElmForestPreWar02Yw | 0006164C | Fallout4.esm | TreeElmFrstPW02Yw_LOD_0.nif<br>(168 tris) | TreeElmFrstPW02Yw_LOD_1.nif<br>(81 tris) | TreeElmFrstPW02Yw_LOD_2.nif<br>(4 tris) | TreeElmFrstPW02Yw_LOD_2.nif<br>(4 tris) |
| TreeElmFreePreWar01Gr | 0006164F | Fallout4.esm | TreeElmFreePW01Gr_LOD_0.nif<br>(302 tris) | TreeElmFreePW01Gr_LOD_1.nif<br>(133 tris) | TreeElmFreePW01Gr_LOD_2.nif<br>(4 tris) | TreeElmFreePW01Gr_LOD_2.nif<br>(4 tris) |

#### five ARCHITECTURE / building multi-level examples

| base editor ID | formID | plugin | slot 0 | slot 1 | slot 2 | slot 3 |
|---|---|---|---|---|---|---|
| Constitution01 | 000556EF | Fallout4.esm | Constitution01_LOD_0.nif<br>(2210 tris) | Constitution01_LOD_1.nif<br>(496 tris) | Constitution01_LOD_1.nif<br>(496 tris) | - |
| Lighthouse01 | 00151E35 | Fallout4.esm | Lighthouse01_LOD_0.nif<br>(334 tris) | Lighthouse01_LOD_1.nif<br>(106 tris) | Lighthouse01_LOD_1.nif<br>(106 tris) | - |
| Satellite01 | 00125B03 | Fallout4.esm | Satellite01_LOD.nif<br>(302 tris) | Satellite01_LOD1.nif<br>(50 tris) | Satellite01_LOD1.nif<br>(50 tris) | - |
| SatelliteDish01 | 00125B05 | Fallout4.esm | SatelliteDish01_LOD.nif<br>(458 tris) | SatelliteDish01_LOD1.nif<br>(166 tris) | SatelliteDish01_LOD1.nif<br>(166 tris) | - |
| SatelliteDish02 | 00213FC4 | Fallout4.esm | SatelliteDish01_LOD.nif<br>(458 tris) | SatelliteDish01_LOD1.nif<br>(166 tris) | SatelliteDish01_LOD1.nif<br>(166 tris) | - |

#### ten mixed multi-level examples (most filled slots first)

| base editor ID | formID | plugin | slot 0 | slot 1 | slot 2 | slot 3 |
|---|---|---|---|---|---|---|
| DLC04RideFerrisWheel01 | 01007EA9 | DLCNukaWorld.esm | DLC04RideFerrisWheel01_LOD_0.nif<br>(not unpacked tris) | DLC04RideFerrisWheel01_LOD_1.nif<br>(not unpacked tris) | DLC04RideFerrisWheel01_LOD_2.nif<br>(not unpacked tris) | DLC04RideFerrisWheel01_LOD_2.nif<br>(not unpacked tris) |
| TreeElmForestPreWar01Gr | 0006164B | Fallout4.esm | TreeElmFrstPW01Gr_LOD_0.nif<br>(234 tris) | TreeElmFrstPW01Gr_LOD_1.nif<br>(95 tris) | TreeElmFrstPW01Gr_LOD_2.nif<br>(4 tris) | TreeElmFrstPW01Gr_LOD_2.nif<br>(4 tris) |
| TreeElmForestPreWar01Yw | 0006164A | Fallout4.esm | TreeElmFrstPW01Yw_LOD_0.nif<br>(234 tris) | TreeElmFrstPW01Yw_LOD_1.nif<br>(95 tris) | TreeElmFrstPW01Yw_LOD_2.nif<br>(4 tris) | TreeElmFrstPW01Yw_LOD_2.nif<br>(4 tris) |
| TreeElmForestPreWar02Gr | 0006164D | Fallout4.esm | TreeElmFrstPW02Gr_LOD_0.nif<br>(168 tris) | TreeElmFrstPW02Gr_LOD_1.nif<br>(81 tris) | TreeElmFrstPW02Gr_LOD_2.nif<br>(4 tris) | TreeElmFrstPW02Gr_LOD_2.nif<br>(4 tris) |
| TreeElmForestPreWar02Yw | 0006164C | Fallout4.esm | TreeElmFrstPW02Yw_LOD_0.nif<br>(168 tris) | TreeElmFrstPW02Yw_LOD_1.nif<br>(81 tris) | TreeElmFrstPW02Yw_LOD_2.nif<br>(4 tris) | TreeElmFrstPW02Yw_LOD_2.nif<br>(4 tris) |
| TreeElmFreePreWar01Gr | 0006164F | Fallout4.esm | TreeElmFreePW01Gr_LOD_0.nif<br>(302 tris) | TreeElmFreePW01Gr_LOD_1.nif<br>(133 tris) | TreeElmFreePW01Gr_LOD_2.nif<br>(4 tris) | TreeElmFreePW01Gr_LOD_2.nif<br>(4 tris) |
| TreeElmFreePreWar01Yw | 0006164E | Fallout4.esm | TreeElmFreePW01Yw_LOD_0.nif<br>(302 tris) | TreeElmFreePW01Yw_LOD_1.nif<br>(133 tris) | TreeElmFreePW01Yw_LOD_2.nif<br>(4 tris) | TreeElmFreePW01Yw_LOD_2.nif<br>(4 tris) |
| TreeMaplePreWar01Gr | 0006163A | Fallout4.esm | TreeMaplePW01Gr_LOD_0.nif<br>(111 tris) | TreeMaplePW01Gr_LOD_1.nif<br>(66 tris) | TreeMaplePW01Gr_LOD_2.nif<br>(4 tris) | TreeMaplePW01Gr_LOD_2.nif<br>(4 tris) |
| TreeMaplePreWar01Or | 00061639 | Fallout4.esm | TreeMaplePW01Or_LOD_0.nif<br>(111 tris) | TreeMaplePW01Or_LOD_1.nif<br>(66 tris) | TreeMaplePW01Or_LOD_2.nif<br>(4 tris) | TreeMaplePW01Or_LOD_2.nif<br>(4 tris) |
| TreeMaplePreWar01Rd | 00052037 | Fallout4.esm | TreeMaplePW01Rd_LOD_0.nif<br>(111 tris) | TreeMaplePW01Rd_LOD_1.nif<br>(66 tris) | TreeMaplePW01Rd_LOD_2.nif<br>(4 tris) | TreeMaplePW01Rd_LOD_2.nif<br>(4 tris) |

#### triangle ratio distribution slot N : slot 0

The set that answers the question is the FIRST block -- the 199 bases whose slots
actually name different meshes.  The last block adds the 2940 repeat bases to show
how much of the whole population is just the same file in every slot.

| set | ratio | n | median | mean | p10 | p90 | exactly 1.000 | < 0.99 |
|---|---|---|---|---|---|---|---|---|
| MULTI-mesh, all | slot1/slot0 | 185 | 0.333 | 0.383 | 0.116 | 1.000 | 21 (11%) | 163 (88%) |
| MULTI-mesh, all | slot2/slot0 | 132 | 0.286 | 0.366 | 0.025 | 1.000 | 21 (16%) | 111 (84%) |
| MULTI-mesh, all | slot3/slot0 | 18 | 0.024 | 0.024 | 0.013 | 0.036 | 0 (0%) | 18 (100%) |
| MULTI, trees/landscape | slot1/slot0 | 113 | 0.339 | 0.321 | 0.091 | 0.500 | 1 (1%) | 111 (98%) |
| MULTI, trees/landscape | slot2/slot0 | 68 | 0.202 | 0.246 | 0.022 | 0.444 | 1 (1%) | 67 (99%) |
| MULTI, trees/landscape | slot3/slot0 | 18 | 0.024 | 0.024 | 0.013 | 0.036 | 0 (0%) | 18 (100%) |
| MULTI, architecture | slot1/slot0 | 6 | 0.340 | 0.303 | 0.166 | 0.362 | 0 (0%) | 6 (100%) |
| MULTI, architecture | slot2/slot0 | 5 | 0.317 | 0.286 | 0.166 | 0.362 | 0 (0%) | 5 (100%) |
| MULTI, architecture | slot3/slot0 | 0 | - | - | - | - | - | - |
| MULTI, vehicles+other | slot1/slot0 | 66 | 0.309 | 0.497 | 0.231 | 1.000 | 20 (30%) | 46 (70%) |
| MULTI, vehicles+other | slot2/slot0 | 59 | 0.333 | 0.511 | 0.231 | 1.000 | 20 (34%) | 39 (66%) |
| MULTI, vehicles+other | slot3/slot0 | 0 | - | - | - | - | - | - |
| MULTI + REPEAT (whole population) | slot1/slot0 | 2663 | 1.000 | 0.957 | 1.000 | 1.000 | 2499 (94%) | 163 (6%) |
| MULTI + REPEAT (whole population) | slot2/slot0 | 374 | 1.000 | 0.776 | 0.179 | 1.000 | 263 (70%) | 111 (30%) |
| MULTI + REPEAT (whole population) | slot3/slot0 | 31 | 0.036 | 0.433 | 0.017 | 1.000 | 13 (42%) | 18 (58%) |

slot-0 triangle counts over the multi-mesh bases: n=185  median 99  mean 191  max 2210

#### near model (MODL) vs its own LOD slot 0

n=3122  median slot0/near = **0.038**  mean 0.680  p10 0.008  p90 0.251

So the very first LOD slot is already ~26x simpler than the near model: the drop
from full geometry to LOD happens in ONE step, not through an intermediate.

(6 further bases have slot 0 == their own MODL path, ratio 1.000 by definition.)

NIF read tally: 6061 parsed, 769 named a file not present in the unpacked corpus, 0 failed to parse (counted, not silently dropped).

CAVEAT on the missing 769: E:\Tools\Fallout 4\DataUnpacked\Data holds the BASE GAME
BA2s only -- there is no Meshes\DLC03 or Meshes\DLC04 there.  Every DLC LOD mesh
therefore reads as "not unpacked"; the DLC records are still counted in every
slot-pattern and category table above, only their triangle counts are absent.

---

### Q3 -- orphan .nif files under Meshes\LOD\

Denominator note: E:\Tools\Fallout 4\DataUnpacked\Data is the UNPACKED shipped BA2s,
so every .nif the game ships in that tree is present -- this is a fair denominator,
not a mod folder sample.

| | count |
|---|---|
| .nif files on disk under Meshes\LOD\ | **3997** |
| of those, referenced by >=1 MNAM slot | 3362 |
| **orphans** (referenced by NO MNAM slot in any of the 7 masters) | **635** (15.9%) |
| MNAM LOD paths naming a file NOT on disk | 1 |

Twenty orphan examples:

  - lod\architecture\airport\airportterminalend01_lod.nif
  - lod\architecture\airport\airportterminalmid01_lod.nif
  - lod\architecture\airport\airportterminalroofmid01_lod.nif
  - lod\architecture\buildings\decocombined\cambridge\cambridgebuilding01_lod.nif
  - lod\architecture\buildings\decocombined\cambridge\cambridgebuilding02_lod.nif
  - lod\architecture\buildings\decocombined\cambridge\cambridgebuilding03_lod.nif
  - lod\architecture\buildings\decocombined\cambridge\cambridgebuilding04_lod.nif
  - lod\architecture\buildings\hightech\hitexta45walltrim01_lod.nif
  - lod\architecture\buildings\hightech\hitextabase01_lod.nif
  - lod\architecture\buildings\hightech\hitextabase1501_lod.nif
  - lod\architecture\buildings\hightech\hitextabase1502_lod.nif
  - lod\architecture\buildings\hightech\hitextabase1503b_lod.nif
  - lod\architecture\buildings\hightech\hitextabase4502_lod.nif
  - lod\architecture\buildings\hightech\hitextabase45llong01_lod.nif
  - lod\architecture\buildings\hightech\hitextabase45llong02_lod.nif
  - lod\architecture\buildings\hightech\hitextabasecornerin01_lod.nif
  - lod\architecture\buildings\hightech\hitextabasestair45long01_lod.nif
  - lod\architecture\buildings\hightech\hitextacolumntall02dmg01_lod.nif
  - lod\architecture\buildings\hightech\hitextacorneracaptopdmg01.nif
  - lod\architecture\buildings\hightech\hitextacorneratalldmgpanels01_lod.nif
