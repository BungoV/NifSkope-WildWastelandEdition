# Lane WATER1 — water bodies, flow and the `.lodl` v3 spec

Read-only lane. Nothing in `src/` was touched, nothing was built, nothing was
committed. Every number below was measured with Python, from two MASTERS:

| input | path | note |
|---|---|---|
| `.lodl` v2 | `E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl` | 35,953,294 bytes, mtime 2026-09-09 17:27 |
| `Fallout4.esm` | `X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm` | **the brief's path `E:\Tools\Fallout 4\DataUnpacked\Data\Fallout4.esm` does not exist** (see §4) |
| xEdit FO4 defs | downloaded to the session scratchpad | for the `WATR DNAM` field order only |

Scripts, all under `scratchpad/water_20260909/`:
`lodl_bulk.py` (bulk decode, built ON TOP of the authority decoder, not a second
reader), `esm_water.py` (ESM walk, built on lane ESPWRITE's validated
`fo4esm.py`), `ccl.py` (connected components + its own known-answer selftest),
`water_model.py` (the body model), `control_synth.py` (the pre-registered
control), `census_water.py`, `analyse_bodies.py`, `flow_feasibility.py`,
`final_census.py`, output `final_census.txt`, `census_ruleD.json`.

---

## 0. The controls, run BEFORE any real number was believed

Pre-registered in the brief: *a synthetic worldspace with a known lake, a
stepped river and a sea must classify correctly before the real numbers are
believed.*

**`ccl.py` selftest** — five hand-computed answers (two diagonally-touching
blobs → 2, a U → 1, a key-split bar → 3, the same bar unkeyed → 1, empty → 0):
**PASS**.

**`control_synth.py`** — a 24×24-cell synthetic worldspace at the real sample
rate (768×768 texels) holding a sea on the west edge at the default height, a
16-step river with its own water type, a square lake in a basin, and a 4×4
puddle. Expected before the run: 4 bodies, classes sea / river / lake / lake,
the river monotone along its own axis.

```
MEASURED  4 bodies, 18 surfaces
  body  1  type default     area  73728  surfaces  1  edge True   class sea    r +0.00 aniso 0.98
  body  3  type lake-type   area  16384  surfaces  1  edge False  class lake   r +0.00 aniso 0.00
  body  2  type river-type  area  10240  surfaces 16  edge False  class river  r -1.00 aniso 1.00
  body  4  type puddle-type area     16  surfaces  1  edge False  class lake   r +0.00 aniso 0.00
REFUTER   type-blind segmentation -> 3 bodies (expect 3: sea+river fused)
control PASS
```

The REFUTER is the part that earns the design: segmenting on connectivity alone
fuses the river into the sea, because a tidal river reach sits at exactly the
sea's height and touches it. Water TYPE is what separates them. That is the
measurement behind "an ID for them".

**Bulk decoder against the authority decoder** — `lodl_bulk.bulk_height_words`
scatters 37,748,736 level-0 samples in 1.7 s; 400 random samples were compared
against `tests/spells/lodl_open_authority.py`'s own `plane_word`: **0
mismatches**, and the scatter asserts that every sample was written exactly
once.

---

## 1. CENSUS — the Commonwealth's water, measured

### 1.1 What the file and the master say before any segmentation

| fact | number |
|---|---|
| `.lodl` version / cells | v2, −96..95 × −96..95 = **36,864 cells**, 32 samples per cell edge, **1 texel = 128 world units** |
| worldspace default water | height **450.0**, type **`00000018 ExtOceanWater`** (WRLD `DNAM`/`NAM3`/`NAM4`, and the v2 header at 0x98/0x9C agrees) |
| default land height | 0.0 (`WRLD DNAM`) — so the whole filler ring outside the built map lies under the default sea |
| cells carrying `Has Water` | **36,864 of 36,864 (100.00%)** — every exterior cell; the flag discriminates nothing here |
| cells inheriting the default water TYPE | **36,357 (98.62%)** |
| cells inheriting the default water HEIGHT | **36,319 (98.52%)** |
| distinct water heights in the file | **51** |
| WATR forms interned in the `.lodl` table | **15** (plus the never-interned default = 16 in play) |
| `No LOD Water` cells (`CELL DATA` bit 3) | 7 |
| wet texels (terrain below the cell's water plane) | **21,754,958 of 37,748,736 = 57.6%** |
| MNAM usable cell extent | NW (−33, 25), SE (28, −36) — the built map is 62×62 cells inside a 192×192 rectangle |

### 1.2 The body rule, and the two things the design assumed that are NOT true

The brief's model was: bodies from connectivity, *steps in height = river*,
*single height = lake*. Measured against the file:

* **803 of 804 bodies carry exactly ONE water height.** FO4's water is a set of
  FLAT plateaus. There is no height gradient *inside* a river to read a
  direction from — the stepped river exists in the synthetic control and
  essentially nowhere in the Commonwealth.
* **Plateaus almost never touch.** Segmenting on type gave 804 bodies of which
  only 6 had ANY neighbouring body at a different height: a step between two
  water bodies is a gap of dry land, not an adjacency. So "flow from
  connectivity" needs *proximity*, not adjacency.
* **A river is chopped by the cells nobody painted.** 534 of the 804 bodies
  inherit the default type; a reach whose `XCWT` was never set splits its river
  in two.
* **The 128-unit sample grid pinches rivers shut.** The Charles came out as two
  big bodies 1.0 texel apart.

Four candidate rules, measured on the same grid:

| rule | body = | bodies |
|---|---|---|
| A | connected + equal water TYPE | 804 |
| B | connected + equal water HEIGHT | 781 |
| C | connected + equal (height, type), then an inheriting component merged into the same-height painted component it touches | 805 → **792** (2 merges had >1 candidate) |
| **D** | C + bridge two same-height components whose shores are within **2 texels (256 units)** when their types match or one inherits | **590** (215 merges accepted, **3 refused** because both sides were painted with different types) |

Rule D is what the spec proposes. The refusals matter: at a 2-texel gap,
`ExtMarshDarkWater`/`ExtMarshScumWater` and
`ExtLakeQuannapowittWater`/`ExtRiverNFoothillsWaterSW` come within 1.0–1.4
texels of each other and must NOT become one body, or two differently coloured
waters merge into one tint.

### 1.3 The census table (rule D)

| | |
|---|---|
| bodies | **590** |
| area ≥ 4 texels | 302 |
| area ≥ 16 texels | 206 |
| area ≥ 64 texels | **93** |
| area ≥ 256 texels | 48 |
| area ≥ 1024 texels | 19 |
| area ≥ 65536 texels | 1 (the sea) |
| classes, all bodies | lake 458, river 131, sea 1 |
| classes, area ≥ 64 | lake 52, river 40, sea 1 |
| bodies assembled from more than one component | 102 |
| **stepped bodies (more than one water height)** | **1 of 590** |
| the sea's share of wet area | 21,585,117 of 21,754,958 = **99.2%** |
| the smallest 288 bodies | under 4 texels each — single-sample dips below the sea plane, sampling noise, not water bodies |

Per WATR form (rule D), with the count of separate bodies each form serves —
**this is the number that answers "different colours for different bodies"**:

| WATR form | bodies | texels |
|---|---|---|
| ExtOceanWater (the default) | 405 | 21,594,553 |
| ExtMarshDarkWater | 40 | 44,040 |
| ExtMarshScumWater | 53 | 26,410 |
| ExtRiverCharlesUpper | 13 | 25,214 |
| ExtLakeForestWater | 7 | 23,662 |
| ExtLakeWater | 16 | 12,960 |
| ExtCreekSanctuaryWater | 3 | 10,666 |
| ExtRiverNFoothillsWaterSE | 13 | 8,426 |
| ExtLakeQuannapowittWater | 1 | 3,696 |
| ExtMurkyWater | 17 | 1,427 |
| ExtLakeIrradiatedWater | 3 | 1,400 |
| ExtGlowingSeaWater01 | 12 | 1,203 |
| ExtRiverNFoothillsWaterSW | 1 | 677 |
| ExtPuddleWater | 5 | 373 |
| ExtCreekSanctuaryWaterE | 1 | 251 |

**16 water forms serve 590 bodies.** `ExtLakeWater` alone paints 16 separate
lakes with one colour and one velocity; `ExtOceanWater` paints 405. A per-body
ID with its own colour override is the only way to give two lakes two colours,
and that is the measured case for it.

The twenty biggest bodies (id, form, class, texels, plane height, how many
components rule D assembled it from, elongation, depth, flow rule, cell range):

```
  id   edid                       class       area   height parts  elong  aniso   depth flow    cells
  1    ExtOceanWater              sea     21585117    450.0     1    1.8   0.05   907.9 zero    (-96..95, -96..95)
  136  ExtMarshDarkWater          river      29305   1300.0     2    9.5   0.90   295.0 drain   (-9..2, -38..-22)
  233  ExtRiverCharlesUpper       river      25112   1300.0     9   13.2   0.98   312.5 drain   (-16..-6, -21..-4)
  282  ExtLakeForestWater         lake       15427   1800.0     3    4.2   0.90   499.6 stroke  (-22..-17, -16..-8)
  177  ExtMarshScumWater          river      14592    450.0     9    6.0   0.75    33.5 drain   (-3..5, -26..-17)
  434  ExtCreekSanctuaryWater     river      10664   7250.0     3    6.8   0.91   246.3 drain   (-23..-15, 17..24)
  149  ExtMarshDarkWater          river       7586    800.0     3    5.4   0.91    22.9 drain   (-3..2, -29..-23)
  419  ExtLakeForestWater         river       7466   5490.0     1    3.0   0.68   582.9 drain   (-24..-20, 11..14)
  411  ExtLakeWater               lake        6147    750.0     3    4.0   0.91   264.7 stroke  (-5..-3, 9..13)
  454  ExtRiverNFoothillsWaterSE  river       4657   2400.0     2   17.8   0.95   139.3 drain   (-4..4, 26..28)
  162  ExtMarshScumWater          river       4248    600.0     5    6.0   0.85    61.7 drain   (6..10, -27..-24)
  447  ExtLakeQuannapowittWater   lake        3696   1250.0     1    2.6   0.76   489.4 stroke  (0..2, 21..24)
  193  ExtMarshScumWater          lake        2771    450.0     4    4.0   0.76    27.0 stroke  (10..13, -24..-22)
  305  ExtOceanWater              lake        2735    450.0     3    1.5   0.33   214.6 zero    (9..11, -14..-12)
  428  ExtLakeWater               lake        2277    750.0     1    3.6   0.89   272.6 stroke  (1..2, 16..18)
```

(the full 590-row table is `scratchpad/water_20260909/census_ruleD.json`; the
printed report is `final_census.txt`.)

### 1.4 Where the forms are

Every painted form is local, which is why a per-body ID is cheap: the largest
`ExtRiverCharlesUpper` body spans cells (−16..−6, −21..−4) and the largest
`ExtCreekSanctuaryWater` body cells (−23..−15, 17..24). Only `ExtOceanWater`
is global, and 404 of its 405 bodies are inland pools that merely inherit it.

---

## 2. FLOW FEASIBILITY — what can be computed, and who needs a human stroke

Four candidate sources of a direction, measured per body:

| source | what it gives | measured |
|---|---|---|
| height gradient inside a body | direction + sign | **nothing**: 589 of 590 bodies are one flat plane |
| principal axis of the body | an unsigned AXIS | usable (anisotropy ≥ 0.5) on **68 of the 93 bodies ≥ 64 texels** |
| a lower body within 64 texels ("drain") | the SIGN along that axis | 27 bodies; **22 of those have the outlet along their own axis (\|cos\| ≥ 0.7)**, 5 off-axis |
| the terrain BED under the water | sign, when the bed falls one way | only **2** bodies pass \|r\| ≥ 0.7 and a drop ≥ 64 units: FO4's river beds are as flat as their surfaces |
| vanilla's `WATR NAM0` Linear Velocity | a direction per water TYPE | present and non-zero on every form measured (ExtOceanWater (−0.46, −0.12), ExtRiverCharlesUpper (0.10, 0.42), ExtLakeWater (0.47, 0.17), ExtMarshDarkWater (0.25, −0.05), ExtGlowingSeaWater01 (0.21, −0.05)) — but it is per FORM, so all 16 `ExtLakeWater` lakes flow the same way. **This is exactly the defect the per-body table fixes, and it is also the fallback floor: a body with no stroke and no drain inherits its form's NAM0.** |

Verdicts (`final_census.txt`, rule `flow_rule`):

| verdict | all 590 | area ≥ 64 (93) |
|---|---|---|
| `zero` — lake or sea, no outlet, round | 309 | 12 |
| `drain` — axis + a lower body to point at | 27 | 26 |
| `bed` — axis + a bed that falls one way | 2 | 1 |
| `stroke` — **a human must mark it** | **252** | **54** |

**58% of the bodies that matter need a stroke.** The automatic rule succeeds on
the big rivers and fails on the elongated standing water.

Named, the ones that COMPUTE (drain, outlet on-axis):

| body | form | texels | plane | outlet | drop to | cos |
|---|---|---|---|---|---|---|
| 233 | **ExtRiverCharlesUpper** — the Charles | 25,112 | 1300 | 4.1 texels away | 450 (the sea) | 1.00 |
| 136 | ExtMarshDarkWater | 29,305 | 1300 | 54.6 | 800 | 0.93 |
| 177 | ExtMarshScumWater | 14,592 | 450 | 22.0 | 0 | 0.81 |
| 434 | ExtCreekSanctuaryWater | 10,664 | 7250 | 16.6 | 7000 | 0.98 |
| 149 | ExtMarshDarkWater | 7,586 | 800 | 11.0 | 600 | 0.96 |
| 419 | ExtLakeForestWater | 7,466 | 5490 | 55.1 | 3600 | 0.96 |
| 454 | ExtRiverNFoothillsWaterSE | 4,657 | 2400 | 22.1 | 1250 | 0.89 |
| 162 | ExtMarshScumWater | 4,248 | 600 | 2.0 | 525 | 0.98 |

Five `drain` bodies have their outlet OFF the axis (|cos| 0.22–0.62: bodies 161,
253, 439, 203 and one more) — for those the sign is not trustworthy and the
panel must ask.

Named, the biggest that CANNOT be computed and need a stroke (all ≥ 64 texels;
54 in total, the full list is in `final_census.txt`):

| body | form | texels | plane | cells | why |
|---|---|---|---|---|---|
| 282 | ExtLakeForestWater | 15,427 | 1800 | (−22..−17, −16..−8) | long (aniso 0.90) but no lower body within 64 texels and a flat bed (r −0.12) |
| 411 | ExtLakeWater | 6,147 | 750 | (−5..−3, 9..13) | aniso 0.91, bed r +0.13 |
| 447 | **ExtLakeQuannapowittWater** | 3,696 | 1250 | (0..2, 21..24) | a named lake: axis known, sign unknown |
| 193 | ExtMarshScumWater | 2,771 | 450 | (10..13, −24..−22) | tidal marsh at sea level, no step anywhere |
| 428 | ExtLakeWater | 2,277 | 750 | (1..2, 16..18) | |
| 242 | ExtLakeIrradiatedWater | 722 | 0 | (2..4, −21..−20) | |
| 449 | ExtRiverNFoothillsWaterSW | 677 | 1250 | (2..3, 23..25) | a river form whose reach has no visible outlet |
| 429 | ExtCreekSanctuaryWaterE | 251 | 7000 | (−22..−21, 16..16) | aniso 0.98 — a creek reach, pure sign problem |

**There is no "the Mystic" in this data.** The Commonwealth's `XCWT` vocabulary
names only the Charles (`ExtRiverCharlesUpper`), the North Foothills river
(SE/SW), Sanctuary's creek (and its east reach), Lake Quannapowitt, and generic
lake / marsh / murky / puddle / glowing-sea forms. Anything else the player
would name is inside `ExtOceanWater`'s 405 inheriting bodies and has no name at
all in the master — which is a second argument for a body table with a
user-editable name field.


---

## 3. THE SPEC

**`scratchpad/specs_20260909/spec_water.md`** — 29 KB, LF-only, written under
`ww-contract-provenance` (banner **SPEC / NOT YET WRITTEN**, because nothing in
`src/` implements it; a provenance footer covering only the claims it makes
about the EXISTING v1/v2 format, with the source hashes re-checked end to end
after the last edit and every line number re-derived from its anchor —
4 rows moved, 10 unchanged, 0 anchors missing).

Sections:

| § | what |
|---|---|
| 1 | the six measured facts the design turns on |
| 2 | **the body rule (rule D)**, stated once for writer, panel and harness |
| 3 | **`.lodl` version 3**: the appended header fields `0xA0..0xF8` (`LODL_HEADER_V3 = 0xF8`, `0x00..0x9F` byte-for-byte unchanged), four new section-flag bits, the 48-byte body-table record, ONE tiled zlib plane store serving three planes (body ID / flow / shore) with uniform tiles costing 16 bytes a cell, the uint16 flow word (dir 8 / speed 4 / confidence 4), the shore plane, **the stroke store in WORLD coordinates as the SOURCE the planes are derived from**, the version bump and **both refusals** with the two things that must change in `lodtfile.cpp` for the old refusal to stay honest |
| 4 | the classification, flow and propagation algorithms, each with the census number beside it |
| 5 | **the NifSkope marking tool**: the water plane as the canvas, five tools mapped one-for-one against Blender's grease pencil with the divergences named, the panel rows under `nifskope-ww-panel-style`, and `WW_WATER_TEST` — the isolation gate (a stroke on body 233 changes 0 texels outside it, ≥ 60% inside it) **with its floor and its refuter run first** |
| 6 | the FO4CS reader's checklist, eight steps, each with the arm that serves it named |
| 7 | **the lane plan**: WATER2 (writer) with gates G1–G9, WATER3 (panel) with P1–P6, all pre-registered here |

Design decisions worth bungo's eye, all of them measured rather than assumed:

* **the body ID is keyed on (height, type) and then merged**, not on connectivity
  alone — the synthetic control proves connectivity alone fuses a tidal river
  into the sea;
* **the bridge gap is 2 texels and it is a parameter**: at 1 the Charles stays in
  two pieces, at 8 distinct marshes fuse;
* **depth stays derived** (`body height − terrain height`), shore distance is
  baked — the same reasoning the existing format document gives for dropping the
  old shore channel;
* **flow has four floors**, and the body record's `flow source` byte says which
  one served, so a fallback is never silent (CONSTITUTION rule 10).

## 4. MISTAKES

Four entries written to `MISTAKES.md` at the repo root, newest first:

1. **rewrote a script the skill told it to copy** — `ww-contract-provenance`
   names `scratchpad/rename_20260909/p14_anchors.py` by path; I wrote my own
   anchor pass instead. A skill's ARTEFACTS are the part that must not be
   retyped.
2. **summed a bit value** — `(fl & 2).sum()` reported 73,728 land cells in a
   36,864-cell worldspace. Caught only because the number exceeded its
   population; at `fl & 1` the same bug is invisible. Rule: print the population
   beside every count, and `.astype(bool)` before summing a mask.
3. **printed a body CLASS from a shape gate alone** — the first census classed
   804 bodies with an elongation ratio before the drainage relation had been
   measured at all, and called lakes rivers. Rule: measure the discriminator
   before segmenting on it.
4. **the brief's `Fallout4.esm` path does not exist** — `E:\Tools\Fallout
   4\DataUnpacked\Data\` holds no plugin; the master is on `X:`, as
   `docs/F4FX_PROVENANCE.md`, `scratchpad/mountains_20260907/esmland.py` and the
   `nifskope-ww-lodgen` skill all already say. Rule: briefs take corpus paths
   from the skill that owns the area, and a lane `ls`-es every input path first.

## 5. SKILL REVIEW (CONSTITUTION rule 1a, the finished-work review)

**Loaded and used:** `ww-control-calibration` (its step 1 is why the synthetic
worldspace and the CCL selftest ran before any real number, and its "refuter
with a number" is why the type-blind segmentation was measured rather than
asserted); `ww-contract-provenance` (the banner, the hash table, the anchor
column, the pass at the end, the version constant re-read last); 
`nifskope-ww-panel-style` (§5.3 of the spec is its rows and its self-test
counts, not my own invention); `nifskope-ww-lodgen` (the corpus paths, the
MISTAKES.md location, the heredoc traps, the CLI shape the new switches must
fit).

**Written this session:** `ww-lodl-offline-census`
(`E:\Projects\Claude\.claude\skills\ww-lodl-offline-census\SKILL.md`, copied
into the repo tree at `.claude/skills/ww-lodl-offline-census/SKILL.md` — both
trees, per the drift rule). It covers exactly what this lane re-derived from
first principles and what the WATER2/WATER3 lanes and every future worldspace
census will need again: the two readers already in the tree and the rule not to
write a third, the bulk pyramid scatter with its exactly-once gate and its
cross-check against the authority decoder, the fact that **there is no scipy on
this machine** and the three routines that stand in for it (run-length
components with a key, chamfer distance, their known-answer selftests), the ESM
side (`WRLD DNAM/NAM3/NAM4/MNAM`, `CELL DATA` u16 and the three `XCLW`
sentinels, `WATR DNAM` 201 bytes with its field offsets, `NAM0` as vanilla's
only flow), the real corpus path, and the caching rule (cache the walk, never
the derived arrays).

**Wished for and NOT written, with the reason:** a skill for "turn a numpy
label grid into a picture for bungo" — declined, because
`nifskope-ww-render-shot` and `nifskope-ww-vanilla-compare` own pictures in this
tree and a matplotlib-free PNG dump is three lines, not a procedure. If the
WATER3 lane needs a body-ID map rendered through the hook, that is the render
skill's territory and it is already written.

---

## Housekeeping

`scratchpad/water_20260909/` is 2.3 MB: the nine scripts, the ESM pickle, the
JSON tables and `final_census.txt`. The five intermediate `.npy` grids (566 MB
of labels, masks and water-height planes) were DELETED — `.npy` is not in
`.gitignore`'s scratchpad exclusions, and they regenerate in seconds:
`analyse_bodies.py` writes `labC.npy` + `ruleC_map.json`, `final_census.py`
writes `body_id_plane.npy`. Run order to reproduce everything from the two
masters:

```
python esm_water.py 3C        # ~20 s, writes esm_water_0000003C.pkl
python ccl.py                 # the labeller's five known answers
python control_synth.py       # the pre-registered control -- must PASS first
python census_water.py        # rules A/B, the first census
python analyse_bodies.py      # rules A/B/C and the plateau graph
python flow_feasibility.py    # the four flow candidates, per body
python final_census.py        # rule D, the tables in this report
```

Nothing was committed. `src/` was not touched (the `M` entries in `git status`
belong to other lanes). `MISTAKES.md` gained four entries; the new skill was
written to BOTH skill trees.
