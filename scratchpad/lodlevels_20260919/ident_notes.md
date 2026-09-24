# Are houses/buildings identified as ONE THING in Fallout 4's plugin data?

IDENT lane, 2026-09-19. Read-only. Scripts: `ident_scan.py` (one ESM walk),
`ident_measure.py` (the mechanism tally), `ident_group.py` (SCOL + "is a layer a
building"), `ident_compare.py` (our bake vs the ESM). Intermediates went to the
session scratchpad, never into this folder.

---

## PLAIN ANSWER

**PARTLY — and there is exactly one mechanism, `XLYR`.**

* There is **no** record whose meaning is "this is one building". No SCOL wraps a
  downtown building, no enable-parent binds one, no multibound, no pack-in trace.
* What there **is**: 86–88% of downtown architecture REFRs carry an **`XLYR`
  pointer to a `LAYR` record** — the Creation Kit's layer, an *authoring*
  construct. It is not a building marker by definition, but Bethesda's artists
  used it as one about half the time.
* Measured: of the 225 layers with ≥8 architecture REFRs in the wider downtown,
  **102 are named per-building** (`Theater49_Bld01`, `BackBay_TTowerBld01`,
  `GenAtomicsBuilding`, `Financial26_Bld01`…) and for those the **median share of
  the layer's REFRs sitting in ONE touching-bounds blob is 0.970**. For those
  layers, XLYR *is* a per-building identity.
* The other 123 are **districts** (`SouthBostonBlock14`, dominant-blob share
  0.707, median 8.1 blobs) or **workflow buckets** (`Theater_Clutter`: 602 REFRs
  in 352 blobs, dominant share 0.148). For those, XLYR is not a building.
* So: a layer is a *usable but unreliable* building id. It never has to be
  checked for geometric sanity by the engine, and roughly half of them are not
  buildings.

---

## Method

### The architecture rule (stated, and its count)

> A REFR is **architecture** when its base's near `MODL` path has a *directory*
> component that is exactly `architecture` or `buildings`, or a directory
> component that **ends in** `kit`. Case-insensitive, `\` and `/` both separate.

| region | cells | all REFRs | architecture | share |
|---|---|---|---|---|
| **PRIMARY** (chunk 4.4.-12) | x 4..7, y -12..-9 | 9 650 | **4 018** | 41.6% |
| **WIDE** | x 0..11, y -16..-5 | 77 863 | **32 895** | 42.2% |

Base types selected, PRIMARY: STAT 4 014, DOOR 4. WIDE: STAT 32 779, MSTT 56,
DOOR 36, CONT 20, ACTI 4.

What the rule discards in PRIMARY, by top folder: `setdressing` 2 776,
`landscape` 1 061, `interiors` 497, (empty MODL) 489, `props` 215, **`scol` 203**,
`vehicles` 153, `markers` 87.

> **Known artefact of the rule, stated plainly:** a SCOL's own `MODL` is an
> auto-generated `SCOL\…` path, so the rule scores SCOL at **0%** by construction.
> SCOL is therefore also measured separately by *base type* in §(a).

REFR source: `esm.pkl` (other lane's, read-only) for position/base/MODL;
`ident_scan.py` walks Fallout4.esm itself for the subrecords esm.pkl does not
keep. Both use the same filter — Commonwealth (0x3C), not DELETED (0x20), not
INITIALLY_DISABLED (0x800) — so they join on refForm.

### Subrecord definitions relied on (quoted from `wbDefinitionsFO4.pas`)

| sig | line | definition |
|---|---|---|
| `XLYR` | 3246 | `wbFormIDCk(XLYR, 'Layer', [LAYR])` |
| `XRFG` | 3246+ | `wbFormIDCk(XRFG, 'Reference Group', [RFGP])` |
| `XESP` | 4330 | `wbStruct(XESP, 'Enable Parent', [ FormID(Reference), u8 Flags, wbUnused(3) ])` |
| `XLKR` | 3227 | `wbStructSK(XLKR, [0], 'Linked Reference', [ FormID 'Keyword/Ref', FormID 'Ref' ]).SetOptionalFrom(1)` |
| `XLRT` | 3259 | `wbArray(XLRT, 'Location Ref Type', wbFormIDCk('Ref', [LCRT, NULL]))` |
| `XMBR` | 3257 | `wbFormIDCk(XMBR, 'MultiBound Reference', [REFR])` |
| `XPRD`/`XPPA` | 4538–4539 | `wbFloat(XPRD,'Idle Time')` / `wbEmpty(XPPA,'Patrol Script Marker')` — **patrol**, not pack-in |
| `LAYR` | 12400 | `wbRecord(LAYR,'Layer',[ wbEDID, wbFormIDCk(PNAM,'Parent',[LAYR]) ])` |
| `SCOL` | 12627 | `wbRecord(SCOL,'Static Collection', …, wbRArrayS('Parts', wbStaticPart))` |
| `wbStaticPart` | 12621 | `wbRStructSK([0],'Part',[ wbFormIDCk(ONAM,'Static',[…]), wbStaticPartPlacements ])` |
| `PKIN` | 12571 | `wbRecord(PKIN,'Pack-In', …, [ wbEDID, wbObjectBounds, wbFLTR, wbFormIDCk(CNAM,'Cell',[CELL]), wbInteger(VNAM,'Version') ])` |
| `RFGP` | 12582 | `wbRecord(RFGP,'Reference Group',[ wbEDID, wbString(NNAM,'Name'), wbFormIDCk(RNAM,'Reference'), wbFormIDCk(PNAM,'Pack-In',[PKIN]) ])` |

**One field is NOT from the definition and is flagged as derived:**
`wbStaticPartPlacements` is *not defined anywhere in wbDefinitionsFO4.pas*
(`grep Placements` returns only the single use at line 12624). I used the
Skyrim/FO4 SCOL placement stride **28 bytes** (pos 3f + rot 3f + scale f) and
**verified it against the whole corpus: all 15 878 SCOL `DATA` subrecords in the
shipped ESM divide exactly by 28, none left over.** Part *count* below therefore
means placement count, not `ONAM` count.

---

## SUMMARY TABLE

Share is of that region's **architecture** REFR count (PRIMARY 4 018, WIDE 32 895).
"Group" = the set of REFRs sharing one value of the mechanism. Diagonal is the
world-bound diagonal of a group, in world units (1 unit ≈ 1.43 cm).

| mechanism | region | REFRs carrying it | share | median group size | median group diagonal |
|---|---|---:|---:|---:|---:|
| **XLYR layer** | PRIMARY | **3 528** | **87.80%** | 80 | 2 823 u (40.3 m) |
| **XLYR layer** | WIDE | **28 021** | **85.18%** | 64 | 2 954 u (42.2 m) |
| XRFG reference group | PRIMARY | 452 | 11.25% | 2.5 | 463 u |
| XRFG reference group | WIDE | 2 526 | 7.68% | 4 | 768 u |
| XLKR linked ref | PRIMARY | 91 | 2.26% | 45.5 | 2 155 u |
| XLKR linked ref | WIDE | 12 061 | 36.67% | 91 | 2 811 u |
| **SCOL base** (by the MODL rule) | both | **0** | **0.00%** | — | — |
| SCOL base (by base TYPE, of *all* REFRs) | PRIMARY | 203 | 2.10% of all | 1 instance | 437 u OBND (6.2 m) |
| SCOL base (by base TYPE, of *all* REFRs) | WIDE | 1 471 | 1.89% of all | 1 instance | 467 u OBND (6.7 m) |
| **XESP enable parent** | PRIMARY | **0** | **0.00%** | — | — |
| XESP enable parent | WIDE | 13 | 0.04% | 1 | 1 245 u |
| **XLRT loc-ref-type** | PRIMARY | **0** | **0.00%** | — | — |
| XLRT loc-ref-type | WIDE | 1 | 0.00% | 1 | — |
| **XMBR multibound** | both | **0** | **0.00%** | — | — |
| **PKIN trace on a REFR** | both | **0** | **0.00%** | — | — |
| XPRD / XPPA on architecture | both | 0 | 0.00% | — | — |
| **nothing at all** (residual) | PRIMARY | **266** | **6.62%** | — | — |
| nothing at all (residual) | WIDE | 4 155 | 12.63% | — | — |
| *…"XLYR only, or nothing"* | PRIMARY | 3 491 | 86.88% | — | — |

Whole-plugin record counts (all worldspaces): LAYR **3 832**, SCOL **2 617**,
PKIN **872**, RFGP **6 116**.

---

## (a) SCOL — a wall cluster, never a building

0.00% under the architecture rule (artefact, see above). By base type, **203 of
9 650 PRIMARY REFRs (2.10%)** and **1 471 of 77 863 WIDE (1.89%)** are SCOL
instances.

* median part **placements** per instance: **5** (mean 7.7 PRIMARY / 6.3 WIDE)
* median **OBND diagonal of an instance: 437 u = 6.2 m** (WIDE 467 u = 6.7 m)
* SCOL instances whose parts include *any* architecture mesh: **32 of 203**
  (PRIMARY), **158 of 1 471** (WIDE)

A 6-metre, 5-part object is a prop cluster, not a building. Five named examples,
the most-placed downtown:

| formID | EDID | placements | arch parts | OBND diag | x placed |
|---|---|---:|---:|---:|---:|
| 0005094C | `RWResRailing1Way01Post` | 2 | 0 | 149 u (2.1 m) | 29 |
| 000B40E6 | `RockClusterL01` | 12 | 0 | 437 u (6.2 m) | 12 |
| 00092A1C | `Spotlight90deg_Off` | 3 | 0 | 74 u (1.1 m) | 12 |
| 0005E20C | `TreeClusterDead02` | 5 | 0 | 1 201 u (17.2 m) | 9 |
| 00152B18 | `WharfFloor` | 2 | 0 | 363 u (5.2 m) | 8 |

(WIDE's top five are the same shape: `RockCoastCluster01Wet` x188 / 5 parts,
`RWStraight01_SG` x132 / 3 parts, `TreeCluster04` x40, `TreeCluster09` x33.)

**Verdict: a SCOL is a small prop/rock/rail cluster. Not a building, not even a
wall cluster of a building — none of the top five contains an architecture mesh.**

---

## (b) XLYR — the only real mechanism, and what a layer actually is

**PRIMARY: 3 528 of 4 018 = 87.80%, in 31 distinct layers.**
**WIDE: 28 021 of 32 895 = 85.18%, in 268 distinct layers.**

### Twenty most-used layers, PRIMARY (chunk 4.4.-12)

| formID | editor ID | REFRs | diag (u) | parent |
|---|---|---:|---:|---|
| 0016CA8D | AndrewStation | 664 | 3 527 | — |
| 0016CBC6 | SouthBostonBlock14 | 358 | 5 506 | 001357A1 |
| 00170533 | SouthBostonBlock19 | 307 | 2 800 | 001357A1 |
| 0016D3D1 | SouthBostonBlock35 | 303 | 5 004 | 001357A1 |
| 0016CBBB | SouthBostonBlock03 | 223 | 3 522 | 001357A1 |
| 0016CBC5 | SouthBostonBlock09 | 215 | 3 421 | 001357A1 |
| 0016CBC2 | SouthBostonBlock11 | 137 | 3 293 | 001357A1 |
| 0016CBC0 | SouthBostonBlock07 | 137 | 2 845 | 001357A1 |
| 0016CBC7 | SouthBostonBlock15 | 130 | 3 809 | 001357A1 |
| 0016CA8E | SouthBostonBlock02 | 103 | 4 730 | 001357A1 |
| 00074404 | DN135_GwinnettExt | 103 | 5 452 | — |
| 0016CBBD | SouthBostonBlock33 | 97 | 4 493 | 001357A1 |
| 0013AC8B | Theater47_FreewayS02 | 96 | 2 008 | 00085472 |
| 0016CBBF | SouthBostonBlock05 | 89 | 3 066 | 001357A1 |
| 0020A55C | SouthBostonBlock34 | 85 | 3 610 | 001357A1 |
| 0016CBBC | SouthBostonBlock04 | 80 | 3 878 | 001357A1 |
| 001D4B5C | SouthBostonBlock16 | 65 | 3 226 | 001357A1 |
| 0023EDEA | Truck | 64 | 819 | — |
| 0016CBBE | SouthBostonBlock06 | 64 | 1 881 | 001357A1 |
| 001B1F35 | Theater47_Bld01 | 56 | 1 863 | 0013AC8B |

### Twenty most-used layers, WIDE

| formID | editor ID | REFRs | diag (u) | parent |
|---|---|---:|---:|---|
| 0002F189 | BackBay_TTowerBld01 | 706 | 8 304 | 000197B2 |
| 000B71BF | Theater001_Building01 | 669 | 5 119 | 0023B6F7 |
| 0016CA8D | AndrewStation | 664 | 3 527 | — |
| 0017632B | Theater06_Bldg01 | 653 | 12 351 | 00068E9D |
| 0012D4D5 | Theater_Clutter | 602 | 24 922 | 00085472 |
| 001C44D2 | Theater25_MedCenter_Bld02 | 533 | 10 469 | 001AE210 |
| 001B1F3F | Theater49_Bld01 | 472 | 3 324 | 000C4B0A |
| 0023FAB5 | WaterFront_Building040_Exterior | 427 | 6 959 | 000C4B0C |
| 001EEED7 | WaterFront_Block05 | 378 | 4 462 | 00071EE2 |
| 000A529F | Financial26_Bld01 | 371 | 5 837 | 00071ECB |
| 000D0FEC | Waterfront_Buildings | 366 | 11 740 | 00071EE2 |
| 00132E0A | Theater23 | 361 | 3 834 | 00085472 |
| 001B23A3 | BackBay_OldJHC | 361 | 4 107 | 000D526F |
| 0016CBC6 | SouthBostonBlock14 | 358 | 5 506 | 001357A1 |
| 001B1F3B | Theater28_Bld01 | 357 | 8 689 | 0009C403 |
| 0019894F | GenAtomicsBuilding | 347 | 4 617 | — |
| 00170533 | SouthBostonBlock19 | 346 | 3 379 | 001357A1 |
| 00132E0C | Theater_Building016 | 341 | 3 930 | 00085472 |
| 000CFB90 | Theater27_Bld01 | 334 | 4 978 | 001C44CC |
| 001B23A9 | BackBay22_Bld01 | 312 | 4 191 | 0014C30A |

`LAYR` is a **tree** (`PNAM` parent): 3 832 layers, depth histogram
`{0: 2 470, 1: 791, 2: 418, 3: 140, 4: 13}`, 2 470 roots, 3 659 leaves.
`001357A1` is the South Boston root; `00085472` the Theater district root.

### The judgement, measured

For every layer with ≥8 architecture REFRs in WIDE I ran **connected components
over the REFRs' rotated world bounds** (union-find over overlapping AABBs, no
padding) and recorded how many disjoint blobs the layer holds and what share of
its REFRs the largest blob holds.

| layer class (by editor ID) | layers | placements | **median dominant-blob share** | mean blobs |
|---|---:|---:|---:|---:|
| `…Bld01` / `…Building01` / `…Bldg01` | **102** | 14 732 | **0.970** | 7.1 |
| `…Block NN` | 53 | 6 595 | 0.707 | 8.1 |
| other | 64 | 5 458 | 0.605 | 11.5 |
| `Clutter` / `Debris` / `Roads` / `Pipes` | 6 | 1 087 | **0.148** | 102.7 |
| **all 225** | 225 | 27 872 | **0.895** | — |

48.9% of layers have a dominant-blob share ≥ 0.90.

PRIMARY's 31 layers: median 4 blobs each, only 5 of 31 (16.1%) are a single
touching blob. WIDE's 268: median 4 blobs, 62 (23.1%) single-blob.

**Judgement: a layer is one building *when the artist named it after one*. The
naming convention predicts it — `Bld/Building`-named layers are 97% one
connected blob. `Block`-named layers are city blocks (median 8 blobs, 40–80 m
diagonals). `Theater_Clutter` is a pure workflow bucket: 602 REFRs, 352 blobs,
249 m diagonal. The plugin does not distinguish these three uses — only the
editor ID string does.**

---

## (c) XESP enable parent — absent

**PRIMARY: 0 of 4 018 = 0.00%.** WIDE: **13 of 32 895 = 0.04%**, in 5 parent
groups, size histogram `{1: 3, 5: 2}`, median group size 1, median diagonal of a
≥2 group 1 245 u.

Nothing to build on. Enable-parents in FO4 are quest/scripted swaps, not
structural identity.

---

## (d) PKIN pack-in origin — **nothing survives**

* **872 `PKIN` records do exist** in the shipped Fallout4.esm. They are small
  prefabs — `Buoy_Bell_Red01`, `Buoy_Bell_Green01`, `SubGatePackIn` — and each
  carries only `CNAM` → the *interior CELL* that holds the template refs.
* **No REFR subrecord points at a PKIN.** The xEdit REFR definition has no
  PKIN-typed field at all. The only indirect route is `XRFG` → `RFGP` → `PNAM`
  (PKIN), and **all 6 116 RFGP records in the shipped plugin have PNAM = 0** —
  zero with a pack-in pointer.
* `XPRD` / `XPPA` are patrol-package subrecords (definition lines 4538–4539) and
  appear on **0** architecture REFRs. They are not pack-in marks.

**Plainly: placing a pack-in in the Creation Kit copies its references out flat.
The shipped plugin keeps no trace of which pack-in a placed REFR came from.**

### `XRFG` / `RFGP` — the near-miss

This is the one mechanism I did *not* expect and it deserves naming: `RFGP`
("Reference Group") is the CK's selection group, and REFRs point at it via
`XRFG`.

**PRIMARY: 452 of 4 018 = 11.25% in 22 groups, median group size 2.5, median
diagonal 463 u.** WIDE: **2 526 of 32 895 = 7.68% in 161 groups, median size 4,
median diagonal 768 u.**

Largest downtown groups: `00175241` n=88 diag 3 174; `00174EE3` n=86 diag 1 670;
`00174EE8` n=68 diag 1 668; `00174F08` name="Group" n=67 diag 1 914;
`001D64C2` name="CornerBrew" n=29 diag 1 096; `0022C836` name="CornerBldg" n=68
diag 1 695. Most have an **empty EDID and an empty or generic NNAM** ("Group",
"DebrisConcGroup"). A handful are genuinely one building ("CornerBrew",
"CornerBldg", ~24 m diagonals). At 7–11% coverage with median group size 2.5–4,
it cannot carry a per-building identity for the region.

---

## (e) Linked-ref grouping and multibound

* **`XLKR`** — PRIMARY **91 of 4 018 = 2.26%** in 2 groups (sizes 56, 35), median
  diagonal 2 155 u. WIDE **12 061 of 32 895 = 36.67%** in 91 groups, median group
  size 91, median diagonal 2 811 u, largest groups 669 / 650 / 623 / 427 / 405.
  Groups of 600+ spanning tens of metres are location/workshop hook-ups, not
  buildings — and the mechanism is near-absent (2.26%) in the primary chunk, so
  it is not even consistently present.
* **`XLRT`** — PRIMARY **0 = 0.00%**. WIDE **1 = 0.00%**. Absent.
* **`XMBR` multibound** — **0 of 4 018 and 0 of 32 895 = 0.00% in both regions.**
  No architecture REFR downtown is in a multibound. (Multibounds in FO4 are an
  interior occlusion tool; no `BSMultiBoundNode`-bearing MSTT is referenced by
  any architecture REFR here.)

---

## (f) Residual — carrying nothing

PRIMARY: **266 of 4 018 = 6.62%** carry no SCOL base and none of
XLYR/XESP/XLKR/XLRT/XMBR/XRFG.
WIDE: **4 155 of 32 895 = 12.63%.**

And the number that matters for a per-object identity: **86.88% of PRIMARY
architecture REFRs carry "XLYR only, or nothing"** — i.e. for nearly nine in ten
downtown wall pieces the layer pointer is the *entire* identity information the
plugin holds.

---

## OUR BAKE vs THE BEST ESM-NATIVE GROUPING

Source: `scratchpad/horizon2_20260918/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi`,
read with `tests/spells/lodgen_native_decode.py::read_lodi`.

* file: **v8**, `instanceCount` 2 449, `chunkCount` 3, `groupCount` **588**
* group table `T['group']` — one u16 per placement, **dense per chunk**, so the
  global group key is `(chunk index, group id)`
* `T['cold'][i]['refFormId']` joins to the plugin; `T['cold'][i]['scolPart']`
  marks SCOL explosion — **266 placements are SCOL parts**, so 2 449 placements
  come from only **2 258 distinct refForms**
* matches the bake log exactly: 588 groups, largest **205**, **468 singletons**

ESM side = **XLYR** (the only mechanism above 12%). **A placement with no XLYR is
its own singleton** — "no layer" is not an identity and must not be allowed to
collapse all 316 unlayered placements into one class. 30 layers cover the other
2 133 placements.

### Metric: pair counting (Rand-style) over the same placement set

| | ALL placements (n=2 449, 2 997 576 pairs) | ARCHITECTURE only (n=1 759, 1 546 161 pairs) |
|---|---:|---:|
| **a** co-grouped by BOTH | **63 207** | **62 105** |
| **b** co-grouped by OURS only (we merge what the ESM separates) | **5 815** | **5 218** |
| **c** co-grouped by THEIRS only (we split what the ESM joins) | **81 616** | **29 800** |
| **d** separated by both | 2 846 938 | 1 449 038 |
| **Rand agreement** (a+d)/pairs | **0.9708** | **0.9774** |
| **Jaccard** on co-grouped pairs a/(a+b+c) | **0.4196** | **0.6394** |

Rand is flattering here because both partitions are sparse (d dominates). The
honest number is the **Jaccard over co-grouped pairs: 0.42 over all placements,
0.64 over architecture only.**

### The structural relation: our partition is a REFINEMENT of theirs

Of our **588 groups**, **581 (98.8%)** lie entirely inside a single layer value,
and only **3 groups join two different real layers**. The 7 groups that span more
than one layer value have sizes 205, 64, 58, 30, 19, 12, 11 — mostly a real layer
plus unlayered strays.

**c ≫ b (29 800 vs 5 218) says the same thing: we almost never merge across the
ESM's lines; we cut inside them.** Our touching-bounds rule finds *structures*;
the layer finds *what the artist selected together*.

### Five named buildings where they disagree

| layer | formID | ESM placements | our groups | our biggest | foreign placements in it | direction |
|---|---|---:|---:|---:|---:|---|
| `DN135_GwinnettExt` | 00074404 | 260 | **205** | 50 | 0 | **we split what the ESM joins** |
| `AndrewStation` | 0016CA8D | 230 | 7 | 204 | 1 | **both** — we split it *and* one of our groups reaches outside |
| `SouthBostonBlock19` | 00170533 | 174 | 5 | 168 | 0 | **we split what the ESM joins** |
| `SouthBostonBlock14` | 0016CBC6 | 152 | 10 | 73 | 0 | **we split what the ESM joins** |
| `SouthBostonBlock03` | 0016CBBB | 145 | 10 | 71 | 0 | **we split what the ESM joins** |

* **DN135_GwinnettExt** (Gwinnett brewery exterior, 6 434 u ≈ 91.9 m diagonal) —
  commonest bases `IndSmPipe2Way01` ×29, `IndSmPipe1Way01` ×14,
  `DecoMainA1x1Wall01Half01` ×11. We cut it into **205 groups**: the pipework
  does not touch, so touching-bounds shatters it. The ESM keeps it as one layer.
  This single layer contributes most of `c`.
* **AndrewStation** (3 316 u ≈ 47.4 m) — `DecoMainA1x1Wall01Half01Full` ×51,
  `DecoMainA1x1Wall01Half01` ×28, `PGarageInFloor1x2Str01` ×26. We hold 204 of
  230 in one group and shed 26 into six more; one placement in our big group
  belongs to a different layer.
* **SouthBostonBlock19** (2 835 u ≈ 40.5 m) — `DecoMainA1x1WinC01` ×42. We hold
  168 of 174 in one group; the remaining 6 fall out as detached trim.
* **SouthBostonBlock14** (5 429 u ≈ 77.6 m) and **SouthBostonBlock03**
  (3 695 u ≈ 52.8 m) — these are **blocks, not buildings**. Our 10 groups each
  are plausibly the *individual houses*, i.e. here **our split is more correct
  than the ESM's join**, and the "disagreement" is the ESM being coarser.

Also worth naming: `SouthBostonBlock34` (110 placements → **95** of our groups)
and `SouthBostonBlock16` (45 → 33) shatter almost completely — those layers are
fence/rubble collections whose pieces genuinely do not touch.
`Theater47_Bld01` is the clean case: 56 placements → **1** of our groups.

---

## What this means for per-object far shadows

1. There is **no** authoritative "one building" record to read. Anything built on
   XLYR inherits the artists' naming discipline, which is ~50% reliable.
2. XLYR is nevertheless **real signal and nearly free**: 86–88% coverage, and our
   own grouping is already a *refinement* of it (only 3 of 588 groups cross a
   layer line). Using the layer as a **merge ceiling** — never join two groups in
   different layers, and optionally join our fragments that share one
   `Bld/Building`-named layer — would repair the `DN135_GwinnettExt` class of
   shatter without inventing merges the ESM contradicts.
3. SCOL, XESP, XLRT, XMBR and PKIN are **0%** here. They are not options.
