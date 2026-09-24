# Lane NATIVE1a report -- the `.lodo` + `.lodi` become real, and tonight's per-instance data

Repo `E:\Projects\NifskopeWildWastelandEdition`, `main`, **nothing committed** (CONSTITUTION 8).
Written incrementally (CONSTITUTION 1, 1b).

Launch state, measured:

| thing | reading |
|---|---|
| `release/NifSkope.exe` at launch | 2026-09-11 07:58:59, 20,927,488 B, md5 `95fc4155b437d277ff77d076dbfd0c69` (SKEL2) |
| rollback rung `release/NifSkope.before_native1a.exe` | written ONCE 08:1x, **same md5**, same size |
| other lanes alive | `ls scratchpad/*/BUILDING` -> **nothing** (rc=2). No other lane is alive. |
| `tasklist \| grep -i -E "Fallout4\|NifSkope"` | **rc=1** -- game down, no NifSkope running |

---

## 0. Pre-registered gates

Registered BEFORE any measurement on the changed tree (CONSTITUTION 1).

| gate | what it asserts | predicted / baseline |
|---|---|---|
| **N1** | the NATIVE0b hook-up is in the tree at re-counted anchors; `Makefile.Release` names the three native sources; their objects build | see the audit in section 1 -- **the premise was stale** |
| **N2** | `ww-standalone-writer-gate`: synthetic pair, independent decoder, two-write identity, mutations refused by name -- green BEFORE and AFTER the format change, and the mutation set gains one mutation per new field | before: **46 checks, 0 failures** |
| **N3** | the first real region pair decodes; instance count == the stock manifests' placement count; the order rule is exercised; `--native-verify` rc=0 | before: 3,526 == 3,526, rc=0 |
| **N4** | every v2 field is WRITTEN and MOVES, one test per field (`fo4cs-census-field`) | n/a |
| **N5** | the stock path is byte-identical with `--native` off, rung exe vs new exe, over every output file | 0 differ over 26 files |
| **N6** | identity unique per placement and equal to the stock manifest's index | n/a |
| **N7** | silhouette (shadow-caster) gate per library mesh, with its floor shown red | n/a |
| **N8** | exe newer than every changed file; every gate DRIVER rebuilt and `-nt`-checked; rung == the 07:58:59 bytes | n/a |
| **N9** | no NifSkope left running; game down at every launch | n/a |

Baselines measured on the RUNG exe (07:58:59), before a line was changed -- see sections 2 and 3.

---

## 1. The hook-up -- it was already applied, and the brief's premise is stale

`ww-spec-gate-audit` step 1: check the claim before building to it.

The brief and `docs/LODGEN_NATIVE_LODO_LODI.md`'s STATUS line both say the hook-up is
"a patch ... not applied" and the emitter is "NOT YET BUILT INTO THE EXE". **Both are wrong as of
2026-09-10 03:57.** `scratchpad/lane_native0_report.md` carries a section at its END, `## Build
(BUILD6)`, written by a later lane, which applied `HOOKUP_CHANGE_NEEDED.md` A1-A4 and B1-B6, ran
`qmake`, built, and gated it. The contract page's STATUS block at the TOP was never updated to
match -- a contract page whose header contradicts a report section four files away.

Measured on disk at 08:1x, not inferred:

| site | anchor | count | where |
|---|---|---|---|
| A1 | `#include "nativeemit.h"` | 1 | `src/lodgen.cpp:10` |
| A2 | `bool lodgenNativeLoadModel( void * user, ...)` | 1 | `src/lodgen.cpp:2046` |
| A3 | `if ( lodgenNativeActive() ) {` + `lodgenNativeAddPlacement( np );` | 1 | `src/lodgen.cpp:3416`, `:3438` |
| A4 | `lodgenNativeLighting( chunkX, chunkY, dim,` | 1 | `src/lodgen.cpp:3702` |
| A2b | the declaration | 1 | `src/lodgen.h:311` |
| B1 | `#include "nativeemit.h"` | 1 | `src/nifcli.cpp:26` |
| B2/B5 | `cmdLodgen(..., nativeDir, nativeVerifyLodo, nativeVerifyLodi, nativeFixture )` | 1 | `src/nifcli.cpp:2470` |
| B5a | `lodNativeFixtureWrite` early command | 1 | `:2492` |
| B5b | `lodgenNativeVerify` early command | 1 | `:2498` |
| B5c | `lodgenNativeBegin(` arming | 1 | `:3414` |
| B5d | `lodgenNativeWrite(` / `lodgenNativeEnd()` | 1 | `:3534`, `:3536`, `:3540` |
| B3 | `--native` / `--native-verify` / `--native-fixture` parse | 1 each | `:5756`-`:5758` |
| B4 | usage text | 1 | `:5322`-`:5328` |

`NifSkope.pro` names `src/lodofile.h|.cpp`, `src/lodifile.h|.cpp`, `src/nativeemit.h|.cpp`
(lines 288-290, 468-470). `Makefile.Release` (2026-09-10 20:59) mentions them 29 times.
Objects on disk: `GeneratedFiles/.obj/lodofile.o` 03:38, `lodifile.o` 03:38, `nativeemit.o` 03:57,
`lodgen.o` 03:57, `nifcli.o` 2026-09-11 07:54 -- all older than the 07:58:59 exe, so **the exe
at launch already carries the emitter**. Proved by running it, not by the mtimes: see section 2.

All files LF-only, 0 CR, before and after this lane (Python byte counts).

**No hook-up was applied by this lane.** The owed work was already done; what was owed and is
done HERE is the contract page's STATUS line (section 4/5).

---

## 2. Standalone gate, BEFORE the format change (baseline, rung exe)

`ww-standalone-writer-gate` run through the built exe rather than the standalone link, because
the exe already carries the writers.

| step | command | reading |
|---|---|---|
| synthetic pair | `lodgen <esm> --native-fixture <abs dir>` | `Synthetic.lodo` **28,903 B**, `Synthetic.lodi` **16,408 B**, `Synthetic.expect.txt` 1,901 B -- byte-for-byte the sizes NATIVE0b's standalone tool wrote |
| independent decoder | `python tests/spells/lodgen_native_decode.py ... --expect ...` | **46 checks, 0 failures, RESULT PASS** |
| `--native-verify` | `lodgen <esm> --native-verify <lodo> <lodi>` | accepted, `pair identity ok`, `instancesWithGeometry 3`, `instancesWithNeitherGeometryNorCard 0`, **rc=0** |

Logs: `scratchpad/native1a_20260911/logs/fixture_v1.log`.

**A CLI trap, measured and new:** `release/NifSkope.exe` resolves a RELATIVE output path against
`release/`, not against the shell's cwd. The first fixture run wrote
`release/scratchpad/native1a_20260911/fixture_v1/` and the directory the command named stayed
empty. Every path handed to the CLI in this lane is absolute. (Recorded in
`MISTAKES_ENTRIES.md`.)

---

## 3. The first real pair (baseline, rung exe, v1 format)

The 9-chunk Sanctuary region, dim 4, AO and identity on (the defaults), headless, into this
lane's own out-dir. **Never** `Data\Terrain`, never the Commonwealth.

```
lodgen <Fallout4.esm> --worldspace 3C --terrain-region -20 24 -9 35 --dim 4 \
       --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" \
       --out-dir <out>/rung_native --native <out>/rung_native/Native
```

The region is snapped outward to chunk alignment by the CLI, giving chunks
(-20,-16,-12) x (24,28,32) = **9 stock chunks**; 8 of them carry objects, (-20,32) is empty.

| reading | number |
|---|---|
| wall clock | **34.9 s** |
| `Commonwealth.lodo` | **5,696,484 B** (the library is worldspace-wide, not region-wide) |
| `Commonwealth.lodi` | **126,512 B** |
| bases | 2,970 written of 2,974 in the census, 4 without a loadable model (the four `WrhsLeanTo*_LOD.nif`) |
| meshes / clusters / triangles / vertices / materials | 2,982 / 10,634 / 142,138 / 273,969 / 136 |
| instances | **3,526** from 3,526 arrivals over 701,131 census refs, **0 dropped**, **0 unlit** |
| chunks | 10 present of 12 dense, max 693 a chunk, max scale 1.9600, max baseId 2,019, PARTIAL |
| bytes a placement | 1,651.4 (library + table over this region's instances) |
| stock `.BTO` + manifests beside it | 8 `.BTO` (6,149,810 B), 8 manifests (276,791 B), 9 `.BTR` |

**Instance count vs the stock manifests: 3,526 == 3,526, exactly.** Counted independently by
summing the placement rows of all eight `*.BTO.manifest.txt` (skipping the `I`/`A`/`M`/`C`
meta lines); every one of the 3,526 `(ref, part)` keys is distinct. The discriminator the brief
names -- `dropped for a base outside the table` -- reads **0**, so there is nothing to explain.

Against the spec's planned whole-Commonwealth 7.0 / 5.1 MiB: the `.lodo` is already
**5.43 MiB at 9 chunks** because the library is built from the FULL worldspace census in every
bake (that is what makes `baseId` worldspace-stable), so it will not grow much with region size;
the `.lodi` is **123.5 KiB for 3,526 placements = 35.9 B a placement**, which extrapolates to
**~24 MiB** over the Commonwealth's ~700k census refs if every one were LOD-bearing, and to
**5.2 MiB** at the spec's 150k far placements -- the spec's 5.1 MiB is reproduced at its own
placement count.

### The pair decoded (independent Python decoder, rung exe's output)

`72 checks, 9 failures` -- **two failure classes, both measured, neither in the data:**

1. **`chunk (-20,32): at least one ref present` (1 of 9).** That chunk has no LOD-bearing
   placement at all; the stock path wrote no `.BTO` for it either. The decoder's own floor
   ("a chunk you asked about must have something in it") fires on a legitimately empty chunk.
   Fixed by not asking about a chunk the stock bake refused.

2. **`manifest: X/Y within 0.125 u` (8 of 8 manifests, worst 0.6230).** This is the decoder's
   bar, not the writer's. The `.lodi` quantisation bound is 0.25/2 = 0.125 u and the ESM leg
   confirms the writer is inside it (**worst 0.1251 over all nine chunks**). The manifest prints
   coordinates with six significant digits, so its own print step depends on the magnitude.
   Measured over all 3,526 rows of this region:

   | column | print step | rows |
   |---|---|---|
   | y | **1.0** (>= 100,000 u, six digits eat the fraction) | 3,356 |
   | y | 0.1 | 170 |
   | x | 0.1 | 3,141 |
   | x | **1.0** | 385 |

   0.5 (half a print step) + 0.125 (quantisation) = **0.625**; the worst observed is **0.6230**.
   Lane BUILD6 saw the same class at worst 0.1740 on region (0,0), where no coordinate reached
   100,000 and the step was only 0.1. **The instrument is wrong, not the file.** Fixed in
   section 4 by budgeting the manifest's own print step, with a floor that a deliberately
   shifted position is still caught.

The nine ESM-leg chunks all pass: every present ref has its base form, **X/Y worst 0.1251 u**,
**rotation worst 0.0046 deg** including the tree yaw.

`--native-verify` on the real pair: accepted, rc=0, `pair identity ok`, 3,526 instances with geometry, 0 with neither geometry nor a card.

Logs: `scratchpad/native1a_20260911/logs/bake_rung_native.log`,
`logs/decode_rung_real.log`.

---

## 4. The v2 fields -- one subsection a field, each with the test that shows it moves

Every field below is checked by `tests/spells/lodgen_native_fields.py` on the REAL region pair
(25 checks, 0 failures) and by `tests/spells/lodgen_native_mutate.py` on the synthetic pair
(24 mutations, each refused BY NAME with its CRCs re-signed so the row rule and not the checksum
answers). `fo4cs-census-field`'s rule is WRITTEN **and** MOVES, with a floor.

### a. Corpus hash / staleness -- `loadOrderHash`, u64, `.lodo` 0xB8 and `.lodi` 0x90

The object records were already hashed (`objectCorpusHash`, header 0x18, both files). What was
missing is the LOAD ORDER. The law, stated exactly: **FNV-1a 64 over, for each plugin of the list
`EsmWorld::load` was given, IN THAT ORDER, the lower-cased BASE FILE NAME's UTF-8 bytes then its
byte size as a little-endian u64.** Nothing else -- not the directory, not the mtime -- so moving a
mod folder does not fire it while adding, removing, reordering or editing a plugin does. A reader
takes it from the header alone, offset stated, without parsing a table.

`--native-verify-corpus` (new) re-reads the plugin and recomputes **all three** hashes -- object,
plugin, load order -- and refuses a stale pair naming the file and which hash moved.

| test | reading |
|---|---|
| WRITTEN | `0xa056a596e2bb16e7` from `Fallout4.esm` alone, in both headers |
| MOVES | the independent decoder reproduces the law from the same plugin list and matches |
| refused, by name | mutating it in the `.lodo` only, or the `.lodi` only, is refused naming `loadOrderHash` (2 mutations) |
| FLOOR | leg 9 of the spell doctors a written pair -- flips `objectCorpusHash`, re-derives `lodoIdentity` so the pairing rule cannot answer first, re-signs both header CRCs -- and `--native-verify-corpus` refuses it naming the file and the field |

The first attempt at that floor FAILED and taught the lane something: flipping `objectCorpusHash`
also moves `lodoIdentity`, so the PAIRING rule answered before the staleness rule and the floor
proved the wrong thing. The doctor now re-derives the identity.

### b. Placed REFR formID per instance -- it is already there, and the record did NOT grow

`cold[i].refFormId` sits at the instance's own index, so the join is one array read with no search,
and it is in the load-order-mapped ID space (`src/esmdata.cpp:379`, `ref.formID = r->formID` -- the
same space `ESMFile::mapFormID` puts every other id in).

**The 24-byte hot record was NOT grown to 32.** Growing it is +33% on the one buffer the per-frame
cull dispatch reads -- 28 KiB on this region, about 1.2 MiB on a full Commonwealth at the spec's
placement count -- to duplicate a number already present at the same index in a blob the draw path
never touches. The brief allowed "state whether the rule suffices"; this is the statement, and it
is **bungo's to overrule**: if he wants the formID in the hot record anyway it is a v3 stride
change and the reader refuses 24 by name. Written into the contract as Deviation 7.

| test | reading |
|---|---|
| WRITTEN | every instance names a REFR (0 zeros over 3,526) |
| MOVES / unique | the `(ref, part)` key is unique, 0 duplicates |
| against the master | **the key set equals the stock manifests' set exactly: 3,526 of 3,526, nothing only in the table, nothing only in the manifests** |

### c. Per-instance bound radius -- the rule suffices, and it needed two fixes to be true

`base.boundRadius x scale` satisfies the ruling: one multiply, more accurate than a 1-unit u16, zero
bytes an instance. Stated in the contract as the rule, with two enforcements added:

* **a `scale` of 0 is a REFUSAL**, naming the instance and its ref (a zero radius fails the
  screen-size test at every distance and the object is never drawn);
* **`maxBoundRadius` is computed from the QUANTISED scale.** This is a REAL DEFECT the gate found:
  the v1 writer used the unquantised scale, and **4 of 3,526 instances then had
  `base.boundRadius x (scale/8192)` sitting up to 0.055 u outside their own chunk's expanded box**
  -- a per-chunk cull could clip an object the consumer expected to keep.

| test | reading |
|---|---|
| c1 | no record has scale 0 (0 of 3,526) |
| c2 | no base has boundRadius 0 (0 of 2,970) -- the reader refuses it |
| c3 | every product is inside its chunk's `maxBoundRadius`: **0 over, worst radius 4,270.8 u**. Before the fix: **4 over** |
| refused, by name | a mutated scale of 0 is refused naming `scale is 0` |

### d. The ONE sort law -- chunk, cell, `drawKey`, ref, part

`drawKey` is a u16 in the record's declared v2 growth slot (0x16, `reserved` in v1): the rank of the
base's (primary mesh id, that mesh's first material id) pair among every base in the `.lodo`.

**Why the cell stays outermost**, reconciled with the existing rule rather than replacing it: the
cell-range blob states exactly ONE `(first, count)` run per cell and the reader checks that a
chunk's sixteen cells partition its instances in order. A mesh-major order inside a chunk leaves one
cell's instances in up to sixteen disjoint runs, which the 8-byte row cannot describe. A cell is
4,096 units -- the granularity the blob exists for -- so a consumer still walks one contiguous run
per (cell, mesh, material). **The reverse (mesh outermost, cell ranges dropped) is a v3 break and
bungo's call.** Contract Deviation 6.

Storing a number that is a pure function of `baseId` is deliberate: it is what lets the `.lodi`
reader check the sort **without opening the `.lodo`**, and `--native-verify` and the decoder then
check the rank itself against the library, so the redundancy is checked in both directions.

| test | reading |
|---|---|
| WRITTEN + correct | `drawKey` equals the base's rank on all 3,526 (checked by `--native-verify`, by the decoder, and by the field gate) |
| MOVES | **41 distinct draw ranks** over the region |
| the key is load-bearing | **787 adjacent pairs inside one cell are ordered by the rank where the ref alone would have ordered them the other way** |
| the fixture EXERCISES it | v1 had one instance a chunk and the order rule never fired (the v1 contract said so). v2 puts **three instances in ONE cell** of chunk (0,0), and the one with the SMALLEST ref carries the HIGHER rank -- so ref order alone puts it first and the law puts it last |
| refused, by name | two mutations: a rank out of order inside a cell, and a rank that is not the base's -- both refused naming `drawKey` |

### e. GPU cache order on every library mesh

`meshopt_optimizeVertexCache` on each shape's triangles, then `meshopt_optimizeVertexFetchRemap` on
its vertices, applied BEFORE the cluster walk. `lib/meshoptimizer/src/vfetchoptimizer.cpp` and
`indexanalyzer.cpp` were added to `NifSkope.pro` (the tree had `vcacheoptimizer.cpp` but neither the
fetch remap nor the analysers). Header flag bit 2 (`CACHE_ORDER`) says the library was written that
way; a library without it is source order and still valid.

**What it bought, and it is small -- reported as what it is, not as a win:**

| number | before | after |
|---|---|---|
| ACMR, 16-entry post-transform cache, triangle-weighted over 2,982 meshes | **1.8600** | **1.8585** |
| overfetch | 1.0097 | 1.0097 |
| library vertices in the region's `.lodo` | 273,969 | **273,695** |
| `.lodo` bytes | 5,696,484 | **5,692,388** |
| meshes the pass changed at all | -- | **45 of 2,982** |

Bethesda's shipped LOD meshes are already close to cache-optimal. The pass costs nothing, can no
longer hurt, and buys about **0.08% of vertex-shader invocations and 4,096 bytes** here.

**A REAL DEFECT the gate found:** meshopt's cache optimiser is a heuristic and read **WORSE** than
Bethesda's own order on **21 of 2,982 meshes**. Both orders are already measured, so the writer now
**keeps whichever is better, per shape**, and "no mesh reads worse" is true by construction.

| test | reading |
|---|---|
| e1 | the corpus ACMR improves, 1.8600 -> 1.8585 |
| e2 | no single mesh reads worse (0; before the fix, 3 by the report's rounding and 21 by the writer's own float) |
| e3 MOVES | the pass changed 45 of 2,982 meshes |
| e4 FLOOR | **a control on the METRIC**: the largest mesh's triangles put in a deliberately shuffled order score **2.990** against the **1.633** we emit |
| e5 | overfetch does not get worse |

### f. Identity survives -- and what is unique, over what

`cold[i].identity` (v2, the cold record's former reserved word) is the **stock bake's** identity
index for that placement: the manifest's `index` column, R + G*256 of the `.bto` vertex colour --
exactly what bungo said the far-shadow pass keys on.

**The honest law, measured, not assumed:** identity is unique inside the **STOCK CHUNK** that drew
the placement, not inside the `.lodi`'s 16,384-unit bin. **7 of 3,526 collide inside a `.lodi`
chunk**, because a placement whose position sits across the chunk line from the chunk that baked it
lands in the neighbour's bin and meets that chunk's own index. The format's own per-placement
identity is the **instance INDEX**, a u32, unique across the file by construction -- which is what
satisfies "unique per placement"; `cold.identity` is the join to the `.bto`.

| test | reading |
|---|---|
| WRITTEN | 0 instances without a stock identity (the census line counts them by name) |
| MOVES | **696 distinct identities** over the region |
| unique | 0 duplicates inside every stock chunk (over the eight manifests); 7 collisions inside a `.lodi` bin, named and explained |
| equals the master | **identity equals the stock manifest's index on all 3,526 rows** |
| refused, by name | a mutated identity is caught by the known-answer expect leg (`identities`) |

### g. The shadow-caster silhouette constraint

The invariant is the count of **boundary edges** -- edges used by exactly one triangle -- over the
topology **welded by the quantised library position**, so the source's shape split and the emitted
mesh's cluster split are comparable. The emitter computes both per mesh and **refuses**, naming the
model and both numbers, if the emitted count exceeds the source's.

| test | reading |
|---|---|
| per mesh | **2,982 meshes, 365 watertight, 0 opened, 0 closed, worst delta 0** -- the emit is a permutation, never a decimation |
| the counts are real | 2,617 meshes have a non-zero boundary, so the check cannot pass on all-zeros |
| FLOOR, shown RED | removing one triangle from the largest mesh (`AirportTower01_LOD.nif`) takes it from **430 to 433** boundary edges, which the same rule rejects |
| printed per mesh | `--native-mesh-report <file>`, one line a mesh: `meshId triangles srcVerts emitVerts acmrBefore acmrAfter atvrBefore atvrAfter boundarySrc boundaryEmitted model` |

---

## 5. Build and gates

### Mtimes, in one table

| artefact | time | size |
|---|---|---|
| rung `release/NifSkope.before_native1a.exe` | 2026-09-11 **07:58:59** | 20,927,488 B, md5 `95fc4155...` -- the launch bytes exactly |
| build (one) | **08:43:28** | 20,986,880 B |
| counted relink 1 (the two code fixes the gate found) | **08:47:59** | 20,989,952 B |
| counted relink 2 (my own measurement bug: the ACMR "after" accumulators) | **08:49:08** | 20,989,440 B, sha256 `140bb8d8...` |
| `Makefile.Release` (qmake, the `.pro` gained two sources) | 08:41 | -- |
| every changed source under `src res tests tools NifSkope.pro` newer than the exe | **none** | -- |
| objects vs the headers they include | **0 STALE** over `lodofile.h lodifile.h nativeemit.h esmdata.h lodgen.h` | -- |
| `res/style.qss` vs `release/style.qss` | in step | -- |

`qmake` ran BEFORE `make` because `NifSkope.pro` gained two sources
(`lib/meshoptimizer/src/vfetchoptimizer.cpp`, `indexanalyzer.cpp`). Dependency read-back, parsed out
of `Makefile.Release`: `src/lodofile.h -> lodgen, lodifile, lodofile, nativeemit, nifcli`;
`src/lodifile.h -> lodifile, lodofile, nativeemit, nifcli`;
`src/nativeemit.h -> lodgen, nativeemit, nifcli`;
`src/esmdata.h -> esmdata, lodgen, lodgenmanager, lodtfile, nativeemit, nifcli, nifskope_ui`;
`src/lodgen.h -> lodgen, lodgenmanager, main, nativeemit, nifcli, nifskope_ui`. Every list complete.

**Gate DRIVERS.** The three this lane runs are Python scripts read at run time
(`lodgen_native_decode.py`, `lodgen_native_mutate.py`, `lodgen_native_fields.py`), not compiled
binaries, so the `-nt` rule has nothing to catch; `grep -n "release/[a-z_]*\.exe"` over them names
only `release/NifSkope.exe`, which the rule already covers. No standalone `release/*.exe` driver is
used by this lane.

### The gate table

| gate | this lane | baseline | note |
|---|---|---|---|
| `tests/spells/lodgen_native.sh` (NEW) | **13 checks, 0 failures, PASS** | newly registered | nine legs |
| -- leg 1, the decoder on the fixture | **56 checks, 0 failures** | **46 / 0** | +10: the ten v2 expect keys |
| -- leg 2, two writes byte-identical | **identical** | identical | |
| -- leg 3, the refusal set | **24 checks, 0 failures** | 20 (NATIVE0b) | +9 v2 mutations, 1 v1 case corrected |
| -- leg 4, the real region bake | written | -- | |
| -- leg 5, the stock path with `--native` off | **25 files, 0 differ** | -- | |
| -- leg 6, the decoder on the real pair | **87 checks, 0 failures** | 72 / **9** on the rung | the 9 were the decoder's own bars, both fixed below |
| -- leg 7, `--native-verify --native-verify-corpus` | **rc=0** | rc=0 (no corpus leg existed) | |
| -- leg 8, the v2 field gate | **25 checks, 0 failures** | newly registered | one subsection a field |
| -- leg 9, the staleness floor | **refused by name** | newly registered | |
| `tests/spells/lodgen_native_baseline.sh --check` | **25 in the baseline, 25 baked, 0 differ, PASS** | 25 / 0 differ | the baseline was written by the **2026-09-10 03:57:46** exe; the strongest stock-path result |
| `lodgen_terrain.sh` | **26 checks, 0 failures** | 26 / 0 | `esmdata.cpp` changed, so this is run |
| `lodgen_identity.sh` | **PASS** | PASS | the manifest identity path |
| `lodgen_merge.sh` | **PASS** | PASS | |
| `lodl_write.sh` | **PASS** | PASS | `esmdata.cpp` reach |
| `lodl_open.sh` | **23 checks, 0 failures** | 23 / 0 | |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 | |
| `animws.sh` | **72 / 0, 1 skip, PASS** | 72 / 0, 1 skip | |
| `water_ui.sh` | **86 / 0, PASS** | 86 / 0 | 82 without the four `SHOT=` / `TABSHOT=` / `STRIPSHOT=` / `LODSHOT=` arguments -- the same arming rule SKEL2 recorded |

**Every count that moved, by name.** Three, and no more:

1. `lodgen_native_decode.py` on the fixture, **46 -> 56**: the ten v2 expect keys.
2. the mutation set, **20 -> 24**: nine v2 mutations added, and the v1 `lodo indexCrc32 flip` case
   REPLACED -- flipping the stored `indexCrc32` is caught by `headerCrc32` first, because that CRC
   covers 0x10..0x100 and the stored value is inside it. The case now corrupts a vertex byte and
   re-signs only the header, so `indexCrc32` is the rule that answers.
3. `water_ui.sh` **82 -> 86** with the four shot arguments. Not a regression.

**Nothing else moved.** The suites this change cannot reach (hkx, gltf, collision, block, the water
solve/flow/mark/window suites, the impostor and card suites, `skeleton_overlay.sh` which BUILD11
measured as flaky) were not run, and that is why.

### What the gate found, and what was changed because of it

Three real defects, none cosmetic, all found by a floor rather than by reading:

1. **the bound radius against the quantised scale** (4c) -- 4 instances outside their own chunk's
   cull box. Fixed in `lodiWrite`; relink 1.
2. **meshopt reading worse than Bethesda's order on 21 meshes** (4e). Fixed by keeping the better of
   the two orders per shape; relink 1.
3. **my own measurement bug**: relink 1's revert dropped the ACMR/overfetch "after" accumulators, so
   the census printed `acmr 1.8600 -> 0.0000` and the gate called that an improvement. Found by
   reading the number rather than the verdict. Fixed; relink 2.

And two instrument errors in my own gates, fixed as instrument errors rather than worked around:

4. **the decoder's manifest bar measured the manifest's printf.** The `.lodi` quantisation bound is
   0.125 u, but the manifest prints six significant digits, so a coordinate at or above 100,000
   comes back as a bare integer and carries a print step of **1.0**. Measured over this region:
   **3,356 of 3,526 y values and 385 x values print at step 1.0**; 0.5 + 0.125 = 0.625 and the worst
   observed miss was **0.6230**. Lane BUILD6 saw the 0.1 form of this on region (0,0) and left the
   call to the director. The decoder now budgets half the manifest's own print step, computed from
   the printed token, and still catches a deliberately shifted position.
5. **the cache-order floor was an expectation about Bethesda's corpus**, not a test of the metric
   (4e). Replaced with a shuffled-order control.

---

## 6. Pictures and coverage numbers

**There is no reconstruction path in this tree** that turns a `.lodo` + `.lodi` back into a mesh the
render hook can photograph: the pair is a GPU-driven format with no index buffer and no NIF writer
behind it, and building one is a consumer's or lane NATIVE1b's work. So, as the brief allows, the
parity is given as coverage numbers and a picture of the two point sets --
`scratchpad/native1a_20260911/coverage.py`, `images/coverage.png` (1672x598).

**Described before it is cited:** three panels at the same scale over the nine-chunk region, the
16,384-unit chunk grid burned in. LEFT, blue: the 3,526 placements the stock `.BTO` manifests list.
MIDDLE, amber: the 3,526 instance positions decoded from the `.lodi`. RIGHT: the stock set drawn
dim, with every placement that has no mutual nearest-neighbour partner marked in red -- **seven of
them, and they are the only red in the panel.** The blue and amber panels are the same picture: the
same clumps along the river, the same empty quarry, the same scatter in the north-east corner.

The numbers behind it, matched **purely on XY distance**, with no use of the `(ref, part)` key:

| reading | number |
|---|---|
| stock manifest placements | 3,526 |
| `.lodi` instances | 3,526 |
| mutual nearest-neighbour pairs | **3,519 = 99.80%** |
| raw distance against the manifest as printed | median 0.2385 u, p99 0.5771 u, worst 0.6295 u |
| with the manifest's own print step budgeted | **median 0.0100 u, p99 0.1138 u, worst 0.1374 u** |
| over the format's own XY bound of 0.1768 u (0.125 per axis) | **0 of 3,519** |
| only in the stock manifests / only in the `.lodi` | 7 / 7 |

The seven are not missing objects. Position-only matching cannot separate two placements that sit
closer to each other than the manifest's print error, so it pairs them the wrong way round; the
key-based check in the same run shows **3,526 of 3,526 exact**, with the base form, the scale and
the identity all matching.

Also on disk, from the GUI harness rather than this lane's own work: `images/dock.png`,
`images/strip.png`, `images/lod.png` -- the `water_ui.sh` in-application grabs at 08:53, which show
the LOD Generation panel is unchanged by this lane.

---

## 7. Owed, red, bungo's calls, and what NATIVE1b inherits

### Red / owed

| item | state |
|---|---|
| the asymmetric drop proof on (-32,0) dim 32 | **NOT RUN.** It needs a `--slot-fallback --native` bake of that one chunk and was not in this lane's nine-chunk budget. The census gate it backs is exact on this region (3,526 == 3,526, 0 dropped) |
| a consumer in FO4CS | nothing reads either file yet |
| the LOD Generation panel | **no `.lodo`/`.lodi` row exists** (`grep` over `src/lodgenmanager.cpp` and `src/nifskope_ui.cpp` finds none), and the panel prints no stage times for any output today. The CLI is the gate; the panel row is **lane LODUI1's**, with the five-outputs-under-FO4CS ruling. THE FOUR STAGE TIMES ARE NOT DELIVERED and cannot be until that row exists |
| `WW_CHANGES.md`, `HANDOFF.md`, `MISTAKES.md` | **NOT edited by this lane.** Text is in `scratchpad/native1a_20260911/{WW_CHANGES_ENTRY,HANDOFF_BLOCK,MISTAKES_ENTRIES}.md` for the director to splice |
| commits | **none.** 15 source/test/doc files changed, uncommitted |

### bungo's calls, both written into the contract as deviations

1. **The mesh/material sort is inside the CELL, not inside the chunk** (Deviation 6). The
   alternative costs the cell-range blob, which the near-field suppression reads.
2. **The placed REFR formID stayed in the 8-byte cold record** rather than growing the hot record to
   32 bytes (Deviation 7). The join is already one array read at the same index; growing it is +33%
   on the buffer the per-frame cull reads.

Either is a v3 format break if he wants it the other way, and the readers refuse the old stride or
the old order by name.

### What NATIVE1b inherits, with the reserved room named

| what 1b needs | the room, stated |
|---|---|
| per-cluster geometric error, level id, parent link | `.lodo` header **0xC0..0xFF = 64 reserved bytes**, enough for eight u64 table offsets; a parallel cluster-error table hangs off one |
| cluster normal cone | `LodoCluster.flags` **bits 2-15** are free (14 bits), plus that same parallel table |
| the ladder's screen-size steps per base | `LodoBase.crossPx16[4]` is in the row and **written as 0 today** |
| occluder boxes per cell | `.lodi` header **0x98..0xFF = 104 reserved bytes** |
| the NEAR levels too (bungo 10:3x: the ladder runs from FULL detail down) | the library already stores full-detail geometry -- nothing is decimated -- so the ladder has somewhere to start |
| a per-instance tint or light index | **gone**: v2 took the record's 0x16 word for `drawKey`. A v3 field there means a 32-byte stride and the reader must refuse 24 by name |

1b also inherits the three gate drivers and the spell, which take a `.lodo`/`.lodi` pair and keep
working as the format grows, and the `--native-mesh-report` file, which is the natural place for
per-cluster numbers.

---

## 8. Mistakes

Five, written the moment each was recognised; the full text is
`scratchpad/native1a_20260911/MISTAKES_ENTRIES.md` for the director to splice.

1. **A relative output path handed to the CLI.** `release/NifSkope.exe` resolves it against
   `release/`, not the shell's cwd; the first fixture run wrote into the repo's `release/` tree and
   the named directory stayed empty. Every CLI path in this lane is absolute now.
2. **Two heredoc backslash losses**, exactly the trap `nifskope-ww-lodgen` names. `"...model\n"`
   written through a `<<'PYEOF'` heredoc arrived as a real newline inside a C string literal and the
   translation unit would not compile. Caught by the syntax pass, not by the build. Every patch
   after that was a script file written with the Write tool, building backslashes from `chr(92)`.
3. **A patch script that asserts mid-way and writes at the end loses the earlier edits.** Two of my
   multi-replacement scripts aborted on the last assertion and I assumed the earlier replacements
   had landed; they had not. Re-ran them; no damage, but it cost two rounds.
4. **I dropped a measurement while fixing the thing it measures.** The keep-the-better-order fix
   replaced the block that also accumulated the ACMR "after" totals, so the census printed
   `acmr 1.8600 -> 0.0000` and my own gate called that an improvement. A green gate is not a result;
   read the NUMBER.
5. **A floor that encoded an expectation about the data, not a test of the instrument.** My first
   cache-order floor demanded that 5% of meshes move and failed at 2.2% -- it was measuring
   Bethesda's meshes. Replaced with a shuffled-order control on the metric itself.

---

## 9. Finished-work skill review

**Loaded and used:** `nifskope-ww-lodgen` (the CLI, the byte-identity gates, the editing traps --
its heredoc warning was right twice), `nifskope-ww-build-verify` (the gated chain,
`tools/ww_build.sh`, the process guard immediately before the link, the header-staleness sweep, the
syntax pass with the real flags), `ww-standalone-writer-gate` (the shape of legs 1-3: known-answer
fixture, independent decoder, two-write identity, re-signed mutations refused by name),
`ww-spec-gate-audit` (which is why the first thing this lane did was check the brief's premise --
and it was stale), `ww-contract-provenance` (hashes first, anchors re-derived by script, version
constants last, source hashes re-diffed end to end afterwards: 14 of 14 unchanged),
`fo4cs-census-field` (written AND moves, with a floor, for all seven v2 fields),
`nifskope-ww-resume-pending` (qmake before make, the dependency read-back per object).

**Not loaded, and why:** `ww-anchored-hookup` -- its subject is a cross-file hook-up a lane must not
apply, and this lane applied none (it was already in the tree) and owned every file it touched, with
no other lane alive. `nifskope-ww-render-shot` -- there is no renderable artefact here.
`ww-lodl-offline-census` -- the census this lane needed is the object one, which the emitter already
computes.

**Amended, in the REPO tree, for the director to mirror to the live tree:**

1. `.claude/skills/nifskope-ww-lodgen/SKILL.md` -- two new items. **"The CLI resolves a relative
   path against `release/`"** (symptom: a named output directory that stays empty while the bytes
   land under `release/`; rule: every path handed to `-no-gui` is absolute). And **"A Bethesda
   manifest's own printf can be coarser than our tolerance"**: the manifest prints six significant
   digits, so a coordinate at or above 100,000 has a print step of 1.0 and any bar tighter than
   0.5 u measures the printf. Compute the step from the printed token, budget half of it, and prove
   the bar still catches a deliberate shift.
2. `.claude/skills/ww-standalone-writer-gate/SKILL.md` -- a new section, **"Name the rule that must
   answer, and check the refusal names it"**, with this lane's two traps: a mutation to a field
   INSIDE the header CRC's own range is answered by the CRC and not by the rule (re-sign only the
   header and corrupt a payload instead), and a mutation that also moves a DERIVED field (here
   `lodoIdentity`) is answered by the pairing rule first (re-derive the dependent field so the rule
   under test is the one that fires).

**Wished for and named rather than left silent:** a skill for comparing our generated output against
a Bethesda sidecar whose own print precision is coarser than our tolerance. Two lanes have now spent
a round on it (BUILD6 at print step 0.1, this lane at step 1.0) and it will recur for `.lodm`, the
card sidecars and the terrain pyramid. It is a paragraph rather than a page, so it went into the
`nifskope-ww-lodgen` amendment above; **whether it deserves its own file is the director's call.**

**Declined:** a skill for "grow a binary format by one field" -- the procedure was entirely
`ww-standalone-writer-gate` plus `fo4cs-census-field` and neither needed anything new.
