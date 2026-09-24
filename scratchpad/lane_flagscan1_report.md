# Lane FLAGSCAN1 -- is there a RECORD FLAG that says "bake me into the far-terrain texture"?

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, working tree.
**Read-only Python.** Nothing under `src/ res/ tests/ tools/ docs/` or
`NifSkope.pro` was touched, no build was run, no exe was launched, nothing was
committed. Lane directory `scratchpad/flagscan1_20260911/`.

Question (bungo, 2026-09-11 16:4x): how does Fallout 4 decide that an object is
baked into the far-terrain COLOUR sheet -- ROADS1 measured that on
`Commonwealth.4.-20.20.DDS` only the `Landscape\Roads` / `Landscape\Sidewalks`
family clears its displaced floor -- and does the same rule cover object types
we do not bake yet?

**Short answer, stated before the tables: there is no such flag.** Not one of
the 32 STAT header bits is even a majority on roads, let alone ~100 percent, and
the one bit with any presence at all (bit 15 `Has Distant LOD`) runs the WRONG
WAY: it is *rarer* on roads (17.7 percent) than on everything else (28.0
percent), and the road bases that carry it are the highway overpasses and
bridges -- the road pieces vanilla draws as LOD MESHES rather than baking.
Of the 71 road models ROADS1 actually projected into chunk (-20,20), **70 have
a record-flags word of exactly `0x00000000`** and the 71st carries only
`NavMesh - Ground`. The bake set is the UNFLAGGED set. ROADS1's folder
convention stands, and it stands alone.

---

## 0. Method

### 0.1 Instruments

| file | what it is |
|---|---|
| `scratchpad/flagscan1_20260911/stat_flag_census.py` | the whole-worldspace walk. Derived from ROADS1's `esm_refs.py` (same GRUP walk, same field reader from `tests/spells/lodgen_cover_model.py`), widened from a 13x15-cell window to **every exterior cell of worldspace 0x3C**, and extended to carry the 32-bit record header flags, MNAM presence, the REFR header-flag histogram per base, and the CELL `DATA` bit 12. |
| `scratchpad/flagscan1_20260911/flagtable.py` | section 1's table. Carries ROADS1's road test verbatim: component equality on the model path -- separators normalised, lowercased, a leading `meshes` component dropped, first component `landscape`, second `roads` or `sidewalks`. Never a substring (`MISTAKES.md`'s "sTREEt" lesson; `SetDressing\RailRoad\WaxCandle02Off.nif` is the live counter-example and it is rejected). |
| `scratchpad/flagscan1_20260911/control.py` | the known-answer controls, run before any verdict. |
| `scratchpad/flagscan1_20260911/detail.py` | sections 2 and 4. |
| `scratchpad/flagscan1_20260911/tile2.py` | section 3's second-tile score, on ROADS1's `rasterlib.py` + `placements.py` + the displaced-mask floor. |

Corpus: `X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm`
(330,776,576 B). Meshes and vanilla sheets from the unpacked corpus
`E:\Tools\Fallout 4\DataUnpacked\Data` (memory: FO4 corpus, never a mod folder).

### 0.2 What the walk saw

```
cells 38970 (lod-only 0)  refr 1244528 (worldspace 701769)
scol placed 33201 -> parts 217915
placed bases with a record: 14195   (of which STAT: 10940)
```

(38,970 is every exterior cell in the file; the Commonwealth's own share is
**36,865**, counted separately in `celldata.txt`. The census line counts cells
across all worldspaces because the counter is not gated on `world` -- a
reporting looseness, not a data one: every placement number below IS gated on
worldspace 0x3C. Noted in section 6.)

Placements are counted two ways and both are reported, because ROADS1 measured
that most Sanctuary road pieces arrive as SCOL parts rather than as direct
REFRs: **direct** = a `REFR` whose `NAME` is that base; **viaSCOL** = reached by
expanding a placed `SCOL` one level through its `ONAM`/`DATA` parts.

### 0.3 The controls (`control.py`), run FIRST

**C1 -- the flag reader is validated against a second, independent field.**
Header bit 15 `Has Distant LOD` lives in the 24-byte record header; the MNAM
"Distant LOD" subrecord lives in the record body. They are read by different
code paths, so if the header word were misaligned the agreement would collapse
to chance.

```
both 3005   bit15 only 5   MNAM only 0   neither 7930
agreement 99.95%          (a misaligned header word reads ~50%)
```

99.95 percent, over 10,940 placed STAT bases. The flag reader is sound. (The 5
strays carry the flag with no MNAM -- Bethesda's own authoring noise, not a
parse error: MNAM-without-the-flag, which a parse error would also produce, is
zero.)

**C2 -- the census's road set contains ROADS1's window set.** Re-deriving the
road models from ROADS1's own `sanctuary_refs.json` with ROADS1's own test:
**119 window road models, 119 present in this census, 0 missing.** (119, not 71:
119 counts `landscape/roads` + `landscape/sidewalks` over the whole 13x15-cell
window; 71 is the `landscape/roads`-only set ROADS1 projected into chunk
(-20,20). Both sets are used below and both are named where they are used.)

**C3 -- is there a candidate at all?** Printed before anything else is
interpreted, so the absence is a measurement and not a claim:

```
bit 15 Has Distant LOD    road  17.66% (83/470)   non-road  27.96%
bit 30 NavMesh - Ground   road   0.21% ( 1/470)   non-road   0.05%
-> NO bit is ~100% on roads.
```

Those are the only two bits set on any road base at all.

---

## 1. The flag table

`flagtable.py census.json`, whole Commonwealth. **10,940 placed STAT bases: 470
road, 10,470 non-road.** Placements: road 16,988 direct + 2,556 viaSCOL;
non-road 567,292 direct + 215,359 viaSCOL.

### 1.1 By BASE (each distinct STAT counted once)

| bit | name | road % | non-road % | non-road bases | lift |
|---|---|---|---|---|---|
| 2 | Heading Marker | 0.0 | 0.1 | 8 | 0 |
| 4 | Non Occluder | 0.0 | 6.7 | 698 | 0 |
| 7 | Add-On LOD Object | 0.0 | 0.0 | 1 | 0 |
| 9 | Hidden From Local Map | 0.0 | 2.5 | 261 | 0 |
| 10 | Headtrack Marker | 0.0 | 0.0 | 1 | 0 |
| 11 | Used as Platform | 0.0 | 0.0 | 5 | 0 |
| 13 | Pack-In Use Only | 0.0 | 0.0 | 2 | 0 |
| **15** | **Has Distant LOD** | **17.7** | **28.0** | **2,927** | **0.6x** |
| 23 | Is Marker | 0.0 | 3.4 | 358 | 0 |
| 25 | Obstacle | 0.0 | 0.0 | 1 | 0 |
| 26 | NavMesh - Filter | 0.0 | 0.3 | 29 | 0 |
| 27 | NavMesh - Bounding Box | 0.0 | 2.7 | 284 | 0 |
| 30 | NavMesh - Ground | 0.2 | 0.0 | 5 | 4.5x |

Bits absent from the table are set on **no placed STAT base in the
Commonwealth at all** -- including four of bungo's candidates:

* **bit 6 `Has Tree LOD`** -- 0 bases. xEdit's comment "Used in Fallout 4 ?" is
  answered: no.
* **bit 17 `Uses HD LOD Texture`** -- 0 bases.
* **bit 19 `Has Currents`** -- 0 bases.
* **bit 28 `Show In World Map (Sky Cell Only)`** -- 0 bases.

### 1.2 By PLACEMENT (direct + viaSCOL; road 19,544, non-road 782,651)

| bit | name | road % | non-road % |
|---|---|---|---|
| 2 | Heading Marker | 0.0 | 0.6 |
| 4 | Non Occluder | 0.0 | 18.3 |
| 9 | Hidden From Local Map | 0.0 | 7.4 |
| **15** | **Has Distant LOD** | **3.0** | **21.0** |
| 23 | Is Marker | 0.0 | 1.0 |
| 25 | Obstacle | 0.0 | 0.1 |
| 26 | NavMesh - Filter | 0.0 | 2.0 |
| 27 | NavMesh - Bounding Box | 0.0 | 10.7 |
| 30 | NavMesh - Ground | 0.3 | 0.3 |

Weighted by placements the anti-correlation is far stronger: **3.0 percent of
road placements carry `Has Distant LOD` against 21.0 percent of everything
else.** The flag's direction is the opposite of the hypothesis.

### 1.3 MNAM

| set | MNAM present |
|---|---|
| road | 83 / 470 (17.7%) |
| non-road | 2,922 / 10,470 (27.9%) |

Same shape, same direction (and C1's cross-check).

### 1.4 The REFR header flags, over direct placements

| bit | name (ACTI/STAT/SCOL/TREE base) | road % | non-road % |
|---|---|---|---|
| 4 | (unnamed in the defs) | 2.55 | 0.09 |
| 8 | LOD Respects Enable State | 0.00 | 0.00 |
| 10 | Persistent | 0.04 | 1.46 |
| 11 | Initially Disabled | 0.00 | 0.06 |
| **15** | **Visible When Distant** | **2.57** | **12.12** |
| 16 | Is Full LOD | 0.00 | 0.07 |

`Visible When Distant` is 2.57 percent on road placements against 12.12 percent
elsewhere -- again backwards, and again nowhere near a rule. Bit 4 is the only
bit in the whole scan with a large lift towards roads (28x) and it reaches 2.55
percent, which is 433 placements out of 16,988; it is not a rule either, and it
is not even named in the xEdit definitions for this base class.

### 1.5 The CELL flag

`CELL DATA` bit 12 "Distant LOD only": **0 of 36,865 exterior Commonwealth
cells.** And that zero is a measurement, not a missing field:

```
exterior CELLs in worldspace 0x3C: 36865 ; with no DATA field: 0
DATA field sizes: {2: 36865}            (itU16, wbDefinitionsFO4.pas:5966)
   0x0002   36656   bits [1]            Has Water
   0x0042     202   bits [1, 6]         Has Water + Hand Changed
   0x000A       7   bits [1, 3]         Has Water + No LOD Water
cells with bit 12 set: 0
```

Every cell has the field, every field is the two bytes the definitions say, and
three distinct values occur -- so the reader works and the flag is simply never
used out here. It is not the mechanism.

---

## 2. Everything that carries the best candidate bit

There is no candidate of the shape the question assumes, so "the best
candidate" here means the only bit with material presence on roads: **bit 15
`Has Distant LOD`**, and it is reported because its membership list is the
finding, not because it is the rule.

### 2.1 The ROAD bases that carry it are the ones vanilla does NOT bake

All 83 of them, by placement count, are highway overpasses, bridges and two
raised park sidewalks -- road pieces that stand ABOVE the terrain:

| EDID | model | placements | MNAM level 0 |
|---|---|---|---|
| HWOnRampCol01 | `Landscape\Roads\HighwayOverpass\HWOnRampCol01.nif` | 60 | `...\HWOnRampCol01_LOD_0.nif` |
| HWSingleChunkLarge02 | `Landscape\Roads\HighwayOverpass\...` | 34 | `..._LOD_0.nif` |
| HWDoubleChunkLargeTop03 | `Landscape\Roads\HighwayOverpass\...` | 22 | `..._LOD_0.nif` |
| SWFullParkCurve01_**HasLOD** | `Landscape\Sidewalks\Park\SWFullParkCurve01.nif` | 18 | `..._LOD_0.nif` |
| RoadStrBridgeEnd01 | `Landscape\Roads\Bridge\RoadStrBridgeEnd01.nif` | 10 | `..._LOD_0.nif` |
| ... (79 more, same three folders) | | | |

The EDID `SWFullParkCurve01_HasLOD` is Bethesda naming the distinction itself:
this sidewalk piece is a duplicate of `SWFullParkCurve01` made specifically to
carry a LOD mesh.

The road bases that do NOT carry it are the flat road surface, and they are the
ones with the placements:

| EDID | model | placements |
|---|---|---|
| SWEndCap01b | `Landscape\Sidewalks\SWEndCap01b.nif` | 962 |
| SWEndCap01a | `Landscape\Sidewalks\SWEndCap01a.nif` | 954 |
| RoadChunk04 | `Landscape\Roads\RoadAChunk04.nif` | 696 |
| RuralRoadEnd02 | `Landscape\Roads\Country\RoadASkirtEnd02.nif` | 586 |
| RoadChunk02 | `Landscape\Roads\RoadAChunk02.nif` | 585 |
| ... | | |

**The 71 road models ROADS1 actually projected into chunk (-20,20): 70 of 71
have a record-flags word of exactly `0x00000000`.** The only one with anything
set is `Road1Way01`, `0x40000000` = `NavMesh - Ground`. The set vanilla bakes
is the set with no flags.

### 2.2 The NON-road bases that carry bit 15

2,927 bases, 164,512 placements. By model folder, top by placements:

| folder | bases | placements |
|---|---|---|
| architecture/buildings | 1,417 | 67,831 |
| landscape/trees | 36 | 64,700 |
| interiors/industrial | 288 | 8,167 |
| architecture/warehouse | 167 | 4,206 |
| architecture/shacks | 74 | 3,825 |
| landscape/rocks | 22 | 3,450 |
| architecture/parkinggarage | 33 | 2,142 |
| architecture/quarry | 122 | 1,410 |
| landscape/pier | 29 | 1,187 |
| architecture/unique | 103 | 1,122 |
| setdressing/signage | 43 | 1,071 |
| setdressing/greebs | 25 | 1,047 |
| landscape/retainingwall | 54 | 849 |
| architecture/residential | 26 | 675 |
| lod/neighborhoods | 310 | 310 |
| (26 further folders) | | |

This is not a list of "other things vanilla bakes into the terrain texture". It
is the object-LOD roster: the things that get a `_LOD_0.nif` MESH. ROADS1
already measured three of these families against vanilla's sheet in chunk
(-20,20) and none of them cleared its floor (trees 0.529 against a floor
reaching 0.533; rocks 0.448; architecture 0.621 against a floor reaching 0.614,
inside the floor's own spread). Section 3 scores the flagged family as a family
on a second tile.

The full list is `scratchpad/flagscan1_20260911/detail.txt`.

---

## 3. The second-tile score

### 3.0 The pipeline's known-answer control

Before scoring anything new, `tile2.py` was run on ROADS1's own tile (-20,20)
with ROADS1's own reference dump, and it must reproduce ROADS1's published road
numbers or the re-implementation is not trusted:

| quantity | ROADS1 reported | `tile2.py` here |
|---|---|---|
| road texels in chunk (-20,20) | 23,321 | **23,321** |
| road AUC(bright) | 0.716 | **0.716** |
| road displaced floor (bright) | 0.470 .. 0.601 | **0.470 .. 0.601** |
| road AUC(grey) | 0.678 | **0.678** |
| road displaced floor (grey) | 0.448 .. 0.513 | **0.448 .. 0.513** |
| ceiling (mask scored by itself) | 1.000 | **1.000** |

Identical to three decimals on every figure. The projection, the AUC and the
floor are the same instrument ROADS1 used.

A SECOND floor is added, as `ww-control-calibration` part 4 requires of a floor
built from the subject's own data: the mask's own amplitude spectrum with
**random phase**, thresholded back to the mask's own area. It is reported beside
the displaced floor everywhere below, and where the two disagree the
conservative one is quoted.

### 3.1 The flagged family on the decisive tile, (-20,20)

| family | texels | AUC(bright) | displaced floor | phase twin | AUC(grey) | displaced floor | phase twin |
|---|---|---|---|---|---|---|---|
| **road** | 23,321 | **0.716** | 0.470 .. 0.601 | 0.472 .. 0.505 | **0.678** | 0.448 .. 0.513 | 0.486 .. 0.546 |
| flagged non-road (bit 15) | 60,839 | 0.536 | 0.432 .. **0.533** | 0.474 .. 0.499 | 0.504 | 0.477 .. **0.563** | 0.485 .. 0.536 |
| unflagged non-road | 101,914 | 0.585 | 0.494 .. 0.526 | 0.485 .. 0.503 | 0.500 | 0.489 .. 0.498 | 0.500 .. 0.518 |

The flagged family reads 0.536 against a displaced floor that reaches 0.533 --
inside its own floor's spread -- and 0.504 against a grey floor that reaches
0.563, i.e. **below** it. It does not clear.

### 3.2 The second tile, chosen for the flagged family

`pickchunk.py` bins every Commonwealth placement into dim-4 chunks. Chunk
**(-24,16)** carries **751 flagged non-road placements and 0 road placements** --
the brief's condition, with the road count at exactly zero rather than merely
small.

```
flagged_nonroad      meshes  1514  triangles   404661  texels  91155 (34.77%)
unflagged_nonroad    meshes  4820  triangles  1694463  texels  93236 (35.57%)
road                 meshes     0  triangles        0  texels      0
```

91,155 projected texels, eighteen times the brief's 5,000 floor.

| family | texels | AUC(bright) | displaced floor | phase twin | AUC(grey) | displaced floor | phase twin |
|---|---|---|---|---|---|---|---|
| flagged non-road | 91,155 | 0.651 | 0.476 .. 0.588 | 0.518 .. 0.550 | **0.474** | 0.467 .. **0.498** | 0.493 .. 0.501 |
| unflagged non-road | 93,236 | 0.598 | 0.471 .. 0.553 | 0.506 .. 0.525 | 0.471 | 0.479 .. 0.499 | 0.492 .. 0.497 |
| road | 0 | absent from this chunk | | | | | |

Ceiling (a mask scored by itself) = 1.000 on both tiles.

**Verdict on the flag: it does not clear.** Brightness alone clears the
displaced floor (0.651 against 0.588); the grey score, which is the one that
separated the road family from everything else in ROADS1 (0.678 against a 0.513
floor), reads **0.474 against a floor reaching 0.498 -- below its own floor**.
And the discriminator that matters is the paired one on the same tile: the
family the flag EXCLUDES scores 0.598 bright / 0.471 grey against the flagged
family's 0.651 / 0.474. The flag moves the brightness score by 0.053 and the
grey score by 0.003, where the real rule moves grey from 0.513 to 0.678.

The brightness signal both families share is explained without a bake: large
objects are placed where the terrain is painted differently (streets, yards,
cleared ground), so any footprint of large placed objects correlates with the
sheet's brightness. The grey score is what tells asphalt from ground, and
neither family has it.

### 3.3 The measurement that DID find a rule: elevated road pieces are not baked

Inside the road family, bit 15 is never set on a flat road piece and only ever
set on a RAISED one:

| road sub-folder | bases | of which bit 15 |
|---|---|---|
| highwayoverpass | 74 | 54 |
| park (sidewalks) | 35 | 11 |
| river | 15 | 11 |
| bridge | 8 | 6 |
| plaza (sidewalks) | 1 | 1 |
| city | 69 | **0** |
| alley | 25 | **0** |
| country | 22 | **0** |
| dirt | 19 | **0** |
| sanctuary | 18 | **0** |
| glowingsea | 7 | **0** |
| raised | 6 | **0** |
| lexingtongreen | 4 | **0** |
| root-level `RoadAChunk*` / `SW*` curb models | ~180 | **0** |

**100 percent specific, 73 percent sensitive**: 60 of the 82
`HighwayOverpass`/`Bridge` bases carry it, and of the 388 other road bases the
only 23 that carry it are the raised `park` / `plaza` / `river` pieces. So the
flag does not mean "bake me"; it means "I am not lying on the ground".

`hw_tile.py` then tests whether vanilla bakes those raised pieces, twice.

**(a) A chunk with elevated road and no ground road at all, (-20,-12)** -- 26
elevated placements, 0 ground placements:

| family | texels | AUC(bright) | displaced floor | AUC(grey) | displaced floor |
|---|---|---|---|---|---|
| road elevated | 38,453 | 0.739 | 0.389 .. 0.657 | **0.471** | 0.469 .. **0.559** |
| non-road | 129,175 | 0.571 | 0.463 .. 0.545 | 0.472 | 0.465 .. 0.530 |

**(b) The paired test, both families on ONE sheet, chunk (-8,8)** -- 58 elevated
and 202 ground road placements, same tile, same floors, same pipeline:

| family | texels | AUC(bright) | displaced floor | phase twin | AUC(grey) | displaced floor | phase twin |
|---|---|---|---|---|---|---|---|
| **road ground** | 27,988 | 0.629 | 0.475 .. 0.618 | 0.494 .. 0.530 | **0.716** | 0.385 .. 0.629 | 0.477 .. 0.524 |
| road elevated | 72,264 | 0.547 | 0.442 .. **0.554** | 0.518 .. 0.560 | **0.419** | 0.410 .. **0.509** | 0.467 .. 0.524 |
| non-road | 145,001 | 0.506 | 0.482 .. 0.534 | 0.501 .. 0.512 | 0.539 | 0.443 .. 0.510 | 0.498 .. 0.526 |

On one sheet: the flat road surface clears its grey floor by 0.087 (0.716
against 0.629) -- ROADS1's Sanctuary result reproduced in a different
neighbourhood 21 cells away -- while the elevated family, with **2.6x more
projected texels**, fails BOTH scores, its grey score sitting *below* its own
floor (0.419 against 0.410..0.509; more saturated than the background, which is
the opposite of asphalt).

**Vanilla does not bake the raised highway and bridge decks into the terrain
colour sheet. It gives them LOD meshes instead** -- which is exactly what their
`MNAM` says: `HWOnRampCol01_LOD_0.nif` and 59 like it.

`src/lodgen.cpp`'s `lodgenIsRoadModel()` accepts first component `landscape`,
second `roads` or `sidewalks`, so as shipped it also takes
`Landscape\Roads\HighwayOverpass\*` and `Landscape\Roads\Bridge\*`, and the
max-z projection will paint a deck's footprint onto the ground beneath it -- a
dark ribbon under every raised highway where vanilla's sheet is clean. 994
placements over 82 bases in the Commonwealth. **This is a lead with numbers, not
a landed finding: nobody has looked at OUR bake of chunk (-8,8) or (-20,-12).**
Its refuter is cheap and named: bake (-8,8) with the shipped rule and score our
own sheet's elevated-road footprint the same way; if our grey AUC comes out near
0.42 like vanilla's, the deck never reached the sheet and there is nothing to
fix.

---

## 4. The tree flags

`lodgenIsTreeModel()` (`src/lodgen.cpp:2031`, read only) re-typed into
`detail.py`: the path contains `\trees\` or `/trees/` case-insensitively, or the
file name starts with `tree`. Over the Commonwealth's placed STAT bases:

| set | bases | placements | bit 6 `Has Tree LOD` | bit 15 `Has Distant LOD` |
|---|---|---|---|---|
| the heuristic's tree set | 139 | 164,005 | **0.0% (0/139)** | 25.9% (36/139) |
| everything else | 10,801 | 638,190 | **0.0% (0/10,801)** | 27.5% (2,974/10,801) |

* **Bit 6 is dead.** Not one placed STAT in the Commonwealth sets it. xEdit's own
  comment on that bit reads "Used in Fallout 4 ?"; the answer is no. It can
  neither replace nor check the heuristic.
* **Bit 15 carries no tree signal either**: 25.9 percent on the tree set against
  27.5 percent off it. And it is inconsistent *within one tree family* --
  `TreeMapleForest3` (12,681 placements) carries it, `TreeMapleForest4` (14,254
  placements, the most-placed tree in the game) does not. 103 of the 139 tree
  bases, carrying 99,305 placements, have no distant-LOD mesh at all.
* **`placed TREE-signature bases: 0`.** The generator's `memcmp( &base.type,
  "TREE", 4 )` clause never fires in the Commonwealth; every Fallout 4 tree out
  here is a STAT. The heuristic is doing all the work, alone.
* What the heuristic actually selects: 133 bases under `landscape/trees`, 2
  under `landscape/plants`, and **4 false positives** produced by the
  `startsWith("tree")` clause on the file name --
  `SetDressing\TreeSwing_RopePile01.nif`, `TreeNoose01_Branch.nif`,
  `TreeSwing_Grounded01.nif`, `TreeSwing03_NoSwing.nif`: a rope pile, a noose
  branch and two swings.

**Conclusion for section 4: a flag would not be better, because there is no
flag.** The heuristic is the only instrument available; the cheap improvement is
not a flag but tightening the `startsWith("tree")` clause, which buys back four
set-dressing props.

---

## 5. Verdict

**There is no record flag that says "bake me into the far terrain".** Over all
10,940 STAT bases placed in the Commonwealth, not one of the 32 header bits is
even a majority on the road family, let alone the ~100 percent a rule would
need: the only two bits set on any road base at all are `Has Distant LOD` (17.7
percent of road bases, 3.0 percent of road placements) and `NavMesh - Ground`
(one base), and `Has Distant LOD` runs backwards -- 28.0 percent of non-road
bases and 21.0 percent of non-road placements carry it. Four of the candidate
bits -- 6 `Has Tree LOD`, 17 `Uses HD LOD Texture`, 19 `Has Currents`, 28 `Show
In World Map` -- are set on **zero** placed STATs in the whole worldspace, and
`CELL DATA` bit 12 "Distant LOD only" is set on **zero** of 36,865 exterior
cells (with the field present on every one of them and three other values
occurring, so that zero is measured, not missing). At the REFR level `Visible
When Distant` reads 2.57 percent on road placements against 12.12 percent
elsewhere -- backwards again. The decisive picture is the bake set itself: of the
71 road models ROADS1 projected into chunk (-20,20), **70 have a record-flags
word of exactly `0x00000000`** and the 71st carries only `NavMesh - Ground`;
vanilla's baked set is its *unflagged* set, so the mechanism is an absence, and
the folder convention ROADS1 shipped stands unchallenged. The best candidate bit
was carried through the brief's step 3 anyway and refused on both tiles: on
ROADS1's own tile the flagged non-road family reads 0.536 bright against a
displaced floor reaching 0.533 and 0.504 grey against a floor reaching 0.563; on
chunk (-24,16), chosen for it (91,155 projected texels, 0 road placements), it
reads 0.651 bright against a 0.588 floor but **0.474 grey against a 0.498
floor**, while the family the flag EXCLUDES scores 0.598/0.471 on the same sheet
-- the flag moves the grey score by 0.003, where the real rule moves it from
0.513 to 0.678. The pipeline that says so reproduced ROADS1's whole road row to
three decimals before it was pointed anywhere new. The one place a flag does
carry information is inside the road family and it says the opposite of the
hypothesis: bit 15 is set on 60 of 82 `HighwayOverpass`/`Bridge` bases and on
**none** of the 388 flat road bases, and a paired test on one sheet (chunk
(-8,8), 58 elevated and 202 ground placements) has the flat road surface
clearing its grey floor 0.716 against 0.629 while the elevated family, with 2.6x
more texels, reads 0.419 against a floor reaching 0.509 -- **vanilla gives raised
decks LOD meshes and does not bake them**, and `lodgenIsRoadModel()` currently
takes them, which is a lead worth one bake and one score.

So: nothing new becomes bakeable on the strength of a flag. If more families are
ever to be baked it will be on measured evidence per family, family by family,
the way ROADS1 did it -- not on a bit.

---

## 6. Mistakes

1. **The cell counter was not gated on worldspace.** `stat_flag_census.py`
   counts every exterior CELL in the file (38,970), not the Commonwealth's
   (36,865), because the `stats['cells']` increment sits outside the `world ==
   WORLD` test while every placement counter sits inside it. Caught by the
   separate `celldata.txt` pass -- written to prove the bit-12 zero was not a
   missing field -- which disagreed by 2,105. No number in any table depends on
   it (every placement, base and flag count IS worldspace-gated), but the
   report's section 0.2 quoted the loose figure and section 1.5 originally
   repeated it; both corrected in place, with the correction left visible rather
   than silently overwritten.
2. **C2's expected count would have read as a failure against the brief.** The
   brief and ROADS1 both say "71 road models"; the control compares against 119
   and matches 119 of 119. 71 is the `landscape/roads`-only set ROADS1 projected
   into chunk (-20,20); 119 is `landscape/roads` + `landscape/sidewalks` over
   ROADS1's whole 13x15-cell window. Both are correct for their own question,
   and the control now names which set it uses at each use --
   `ww-spec-gate-audit`'s rule that the expectation belongs to the rule under
   test, restate the arithmetic, and say that you restated it.
3. **Recorded because it is the failure that would have faked this answer:** a
   misaligned 32-bit record header word would have produced exactly the result
   this lane reports -- "no flag correlates with anything" -- for entirely the
   wrong reason. It was checked before any table was believed (header bit 15
   against the MNAM subrecord, two different code paths, 99.95 percent agreement
   over 10,940 bases, with the 5 disagreements all in the direction authoring
   noise takes and none in the direction a parse error takes).

No entry is owed to the tree's `MISTAKES.md`: nothing here is a defect in the
tree, and this lane may not write outside its own directory. Item 1 is a defect
in this lane's own scratchpad script, corrected above.

---

## 7. Finished-work skill review

Skills loaded at the start of the lane, as the brief directed:
`ww-spec-gate-audit`, `ww-control-calibration`.

**`ww-spec-gate-audit` -- used, and it shaped the lane.** The brief handed me a
gate of exactly the shape the skill warns about: "a bit that is ~100 percent on
roads and rare elsewhere is THE candidate". There is no prior lane's script to
audit here, so the move that applied was the skill's NATIVE1b section -- *audit
the gate you write for yourself, and start with the DOMAIN*. Two domains were
named out loud before any predicate was written: "the road family as ROADS1's
folder test selects it" (470 bases) and "the set vanilla actually bakes", which
is only ever observed through a projected footprint on one tile (71 models).
They are not the same set, and an aggregate over 470 could have hidden a flag
that was 100 percent on the baked subset and rare across the folder. That is why
section 2.1 prints the 71 models' flag words individually rather than only the
percentage; it did not hide one, and now that is shown rather than assumed. The
skill's *print the table, not the count* is why section 1 prints all thirteen
occurring bits including the twelve that are zero on roads, and why section 3.3
prints the per-sub-folder split that turned "bit 15 is anti-correlated with
roads" into the usable "bit 15 means raised". And the skill's closing rule --
*keep the part that was real* -- is section 3.3 itself: a lane that ended "no
flag, done" would have thrown away the only actionable finding the scan
produced.

**`ww-control-calibration` -- used, and its part 4 changed what I quote.** Part 1
(run the metric on inputs whose answer you already know) is section 3.0: the
re-implemented pipeline reproduces ROADS1's six published road figures to three
decimals before it is pointed at anything new; had it not, nothing below it
would be worth reading. Part 4 (a paired floor built from the subject's own data
needs an independent twin) is why every row carries a phase-randomised twin
beside the displaced floor -- same amplitude spectrum, random phase, thresholded
back to the mask's own area -- and it mattered on chunk (-24,16), where the twin
sits at 0.518..0.550 rather than at 0.5, so the flagged family's bright margin is
quoted against the conservative floor. Part 5 (the ceiling from the same data
with the property removed) is the 1.000 self-score printed on both tiles. The
skill's 2026-09-11 addition -- *a floor with a FIXED step cannot fire on a
degenerate subject* -- was checked against this lane's floor and is satisfied by
construction: the displaced floor is built from the subject's OWN mask, so a
subject with no texels yields no floor at all rather than a clean zero, and
section 3.2's `road | 0 | absent from this chunk` is that refusal firing rather
than a green gate hiding an empty subject.

**What neither skill covered.** Both assume the thing under test exists and the
question is whether it is measured honestly. Here the hypothesised object is
ABSENT, and what the lane needed instead was a third control neither skill
names: **the paired same-sheet test** (section 3.3b). Scoring two families
against each other on ONE tile, with one set of floors, is what turned an
ambiguous single-family pair of results -- elevated road clears bright on
(-20,-12) but fails grey -- into a clean statement, because every confound the
two families share (the sheet's grading, that tile's paint, the block codec)
cancels between them. `family_auc.py` already had every ingredient; what was
missing was the written rule that when a family's own floor gives a marginal
answer, the comparison you actually want is family-against-family on one sheet
rather than family-against-its-own-floor on two. If a skill is wanted out of this
lane, that is the sentence in it.

---

## Files

All under `scratchpad/flagscan1_20260911/`. Read-only against the game and the
tree; nothing outside this directory was written, nothing was built, nothing was
committed.

| file | what |
|---|---|
| `stat_flag_census.py` / `census.json` | the whole-worldspace walk and its output |
| `flagtable.py` / `flagtable.txt` | section 1 |
| `control.py` / `control.txt` | the controls |
| `detail.py` / `detail.txt` | sections 2 and 4 |
| `overpass.txt`, `specificity.txt` | the road-family flag split |
| `pickchunk.py` / `pickchunk.txt`, `pick_hw.txt` | tile selection |
| `tile2.py`, `tile_m20_20.{txt,npz}`, `tile_m24_16.{txt,npz}` | sections 3.0-3.2 |
| `hw_tile.py`, `hw_m20_m12.{txt,npz}`, `hw_m8_8.{txt,npz}` | section 3.3 |
| `celldata.txt` | the CELL bit-12 proof |
| `hw_refs.json`, `hw2_refs.json` | reference dumps for the two extra tiles |
