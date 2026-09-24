# PLANSYNC1 -- audit of NATIVE cites in docs/FO4CS_IMPROVED_LOD_PLAN.md

Read-only audit, 2026-09-23. Sources as read (md5):

- docs/FO4CS_IMPROVED_LOD_PLAN.md 1,605 lines, d6ff2f4c41a781e1596809de8a781d14
- docs/LODGEN_NATIVE_LODO_LODI.md 2,276 lines, 91af5fa944d3de32e43d68333266b018
- src/lodofile.h 584 lines, 9f39e5928bc85654d982a122acfb5187 (LODO_VERSION = 4)
- src/lodifile.h 798 lines, f3817d364c03f649e077a497dcc5b702 (LODI_VERSION = 3 base constant; conditional 4/5/6/7/9)

Bake labels used below: **A** = MNAM + ladder, 2026-09-11 (the 9,657,316 B pair); **B** = near-library
(`--library near`) default of 2026-09-16; **C** = today's authored-only default
(`--native-no-ladder --library mnam`, 2026-09-17 ruling).

## (5) Plan section 9 source-line cites -- re-derived

| plan cite | current line | unique anchor quote | verdict |
|---|---|---|---|
| `src/lodifile.h:556` (offGroup 0x100, groupCount 0x108) | 556 (`offGroup`), 557 (`groupCount`) | `quint64 offGroup = 0;                   //!< v7, 0x100: the group table, one u16 an instance` (1 hit) ; `quint32 groupCount = 0;                 //!< v7, 0x108` | OK (still 556) |
| `src/lodifile.h:490` (cold record refFormId) | 490 | `struct LodiCold` then `quint32 refFormId;` (`refFormId;` 1 hit in file) | OK |
| `src/lodifile.h:412` (comment) | 412 | `` `refFormId` is the PLACED REFR's form ID in the load-order-mapped ID space `` (1 hit) | OK |

## Authored-default (bake C) numbers, read from bytes on disk

Header words decoded read-only (python struct read, no exe run). Every C `.lodo` found is 6,204,388 B:
**v4, flags 0x5 (LADDER clear), 2,970 bases, 2,982 meshes, 10,634 clusters, 273,695 vertices,
136 materials, levelMax 0, ladderGroup 0, cardCount 0, loadOrderHash 0xa056a596e2bb16e7**.
Counted from the tables of horizonout_20260919/scrap/nat: level-0 triangles **142,138**; CONE_OPEN
**6,982 of 10,634**; WATERTIGHT **365 of 2,982**; fullTriangles non-zero on **2,970 of 2,970**, max **2,706**.
No ladder -> every cluster is a root (10,634 roots covering 142,138).
Agrees with WW_CHANGES.md:712 "Authored LOD models only" and NATIVE s1 (`--native-no-ladder` = 6,204,388 B).
Urban C `.lodi`: 33,123 instances, 12 present of 20 dense, occluderCount **280**, slotInstances
(33,123, 0, 0, 0), 1,061 groups (v9 `--scrappable` file). Chunk 4.4.-12: 2,449 instances, groups
**167 (proximity 64 u, default)** vs **588 (`--identity-join legacy`)**, 40 occluders.
**No authored-default Sanctuary pair found**, so the Sanctuary `.lodi` gates (3,526 / 10 of 12 / size) have no C fixture on disk.

## Main table

| plan line | claim | current truth | source anchor | verdict |
|---|---|---|---|---|
| 183-184 | R0 reads both headers + index CRCs (NATIVE 3, 4) | s3 `.lodo` 256 B; s4 `.lodi` 256 B, **512 B on v7/v8/v9**; headerCrc window = version's header size | NATIVE s4 row 0x0C; lodifile.h `lodiHeaderBytes` | OK (R0-IMPACT) |
| 191 | hard: version "1 and 2 refused BY NAME" | `.lodo`: 1, 2 **and 3** by name, v4 only. `.lodi`: 1, 2 by name; accepts 3,4,5,6,7,8 (read-only),9 | lodofile.cpp:1750/1753/1764; lodifile.cpp:972/976/989 | **WRONG** |
| 191-197 | hard key list | plan = NATIVE s5 of 09-11; s5 now adds groupStride, group density, groupCount sum, sky/AO slice length, v3..6 carrying v7 words; readers enforce many more (part 4) | NATIVE s5 line 1541 | STALE (incomplete) |
| 198-200 | soft = five hashes | s5 unchanged; but a mismatch BETWEEN the two files (plugin/object/loadOrderHash, lodoIdentity) is a hard pairing refusal | NATIVE s4 row 0x90; nativeemit.cpp:2977-2989 | OK (R0-IMPACT) |
| 211 | `bStaleRefuses` promotes s5 soft class | s5 soft class exists | NATIVE s5 | OK |
| 230-231 | `.lodo` 9,657,316 B, 2,970, 2,982, 20,678 clusters, levelMax 7, 4,715 roots / 142,138 | bake A. **C: 6,204,388 B, 2,970, 2,982, 10,634, levelMax 0, 10,634 roots / 142,138.** NATIVE s6 line 1562 says "9,710,564 bytes" (page disagrees with itself) | NATIVE s1; bytes on disk | STALE-NUMBER (A) |
| 232 | `.lodi` 128,256 B, 3,526, 10 of 12 | 128,256 B = v3 file of 09-11; default now writes **v7** (512-B header + PAO + vertex AO + groups + sky), Sanctuary size unmeasured. 3,526 and 10/12 are instance facts, not library-dependent, not re-measured | NATIVE s1, s6, s4 row 0x04 | STALE-NUMBER (size); rest OK |
| 233 | cites NATIVE 1, 3.5.3, 6 | exist | -- | OK |
| 234-235 | loadOrderHash 0xa056a596e2bb16e7 (NATIVE 8.1) | stated; confirmed in C bytes | NATIVE s8.1 | OK |
| 236 | v2 refused by name (NATIVE 0) | true; add v3 `.lodo` by name (Dev 14); v8 `.lodi` accepted read-only | NATIVE s0, s11 Dev 14, s4.11 | STALE (incomplete) |
| 238-240 | occluders 0 Sanctuary / 280 downtown (NATIVE 4.5.3) | stated; 280 confirmed on C; 0 = A, unmeasured on C | NATIVE s4.5.3 | OK |
| 264-266 | cold refFormId (4.1); chunk maxBoundRadius (4) | present | NATIVE s4.1 | OK |
| 270-272 | chunkIndex 636 KiB (7 step 3) | present | NATIVE s7 | OK |
| 273-277 | boundRadius x scale, maxBoundRadius, 8,224-unit tree (7 step 4) | present | NATIVE s7 | OK |
| 278-279 | 12/24/48 size classes (7 step 6) | present | NATIVE s7 | OK |
| 289-291 | cell-range row 8 B (4, 2.1) | present | NATIVE s4, s2.1 | OK |
| 301-314 | "NATIVE 4.1c says TWO identities"; far shadows keyed on the **instance index** | s4.1c now **THREE** words; "the caster identity is the GROUP (s4.9)"; the instance index is not the shadow key (director 2026-09-18). Plan s9 already says group | NATIVE s4.1c lines 764-785 | **WRONG** |
| 308-314 | 7 of 3,526; 1.54x (4.1c) | stated | NATIVE s4.1c | OK |
| 321 | `bSuppressEngineFarObjects` inert (7 step 9) | present | NATIVE s7 | OK |
| 349-351 | 3,526, 0 dropped (9) | present | NATIVE s9 | OK |
| 353-354 | 8-14 + 1-2 buckets vs 343-1,795 (7 step 7) | present | NATIVE s7 | OK |
| 382-385 | 172x spread, 47.8..8,224.1, median 1,086.4 (4.4) | present | NATIVE s4.4 line 920 | OK |
| 386-388 | no VB/IB (7); drawKey rank (2.1) | present; rank check nativeemit.cpp:3074 | NATIVE s2.1 | OK |
| 516-519 | ladder row 48 B (3.2); occluder table (4.5.1) | present | lodofile.h static_assert | OK |
| 523-538 | cut law, FLT_MAX at root (4.4) | present; on C every cluster is a root, law selects level 0 everywhere | NATIVE s4.4 | OK (degenerate on C) |
| 540-542 | projectionScale 1371.0 (4.4) | present | NATIVE s4.4 | OK |
| 544-548 | cone test (3.6); 14,604 of 20,678 open | rule present; number A. **C: 6,982 of 10,634** | NATIVE s3.6 | STALE-NUMBER (A) |
| 550-552 | occluder fit (4.5.1) | present | NATIVE s4.5.1 | OK |
| 559-563 | radius rule, scale 0 refused (4.1b) | present | NATIVE s4.1b | OK |
| 586-588 | shadow view coarser selection (4.4) | present; NATIVE line 912 still says the pass keys on the identity index (page contradicts s4.1c) | NATIVE s4.4 | OK (see part 6) |
| 599, 607-608 | 1 px tolerance; bOccluderCull; bConeCull | present | NATIVE s4.4, 4.5, 3.6 | OK |
| 620-623 | `no_ladder` fallback "on a `--native-no-ladder` bake" | no-ladder IS the default (`bool lgNativeLadder = false`); levelMax 0 is the normal state | nifcli.cpp:7426 | **WRONG** (framing) |
| 640-647 | 4.4.1 triangle table | A only. On C level 0 at every tolerance; derived (not measured) expectation 124,205 at every distance | NATIVE s4.4.1 | STALE-NUMBER (A) |
| 649-651 | two floors (4.4.1) | present; on C both floors are the same set, gate cannot separate them | NATIVE s4.4.1 | OK (vacuous on C) |
| 659-661 | per-level caps 10,634/5,676/2,781/1,029/414/123/19/2 (3.5.3) | A. C: 10,634 at level 0, none above | NATIVE s3.5.3 | STALE-NUMBER (A) |
| 671-682 | 3.80 %, 52,100 u...; "bungo's call, open in s6" | numbers A. Call taken 09-16 (near), **reversed 2026-09-17**: authored LODs only, MNAM library, one level a mesh | WW_CHANGES.md:712 | **WRONG** (status) |
| 691-692 | refuter reads levelMax | on C levelMax 0 by design | -- | STALE |
| 836-856 | ladder + instance tables, refFormId (4.1a), cell ranges (2.1) | present | -- | OK |
| 861-868 | ladder has NEAR levels (3.5); "R5c not chartered before that call" | default C has no ladder and no near levels; both opt-in (`--native-ladder`, `--library near`); call taken then reversed | nifcli.cpp:7426, 7456 | **WRONG** (status) |
| 885-887 | SCOL expanded (NATIVE 2.1 / lodgen.cpp ~3389) | NATIVE s2.1 does not describe SCOL expansion; expansion at lodgen.cpp:3484 `if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {` | lodgen.cpp:3484 | SECTION-MOVED |
| 1018-1021 | 3,526 (9); 10/12 (6); 0/280, 87 of 147 (4.5.3) | present | NATIVE | OK |
| 1022 | per-level table (3.5.3) | A; C = 10,634 / 0 | NATIVE s3.5.3 | STALE-NUMBER (A) |
| 1023 | CONE_OPEN 14,604 of 20,678 | A; C = 6,982 of 10,634 | bytes | STALE-NUMBER (A) |
| 1024-1025 | buckets (7 step 7) | present | NATIVE s7 | OK |
| 1026 | 124,205/124,121/115,583 (4.4.1) | A; C expected 124,205 all three (derived, unmeasured) | NATIVE s4.4.1 | STALE-NUMBER (A) |
| 1029 | ~67.8 MiB resident `.lodo` (7) | NATIVE s7 = 63.8 MiB (v2) + 4.0 MiB ladder growth; 67.8 is A. C growth 512,000 B -> ~64.3 MiB | NATIVE s7 | STALE-NUMBER (A) |
| 1050 | row 1 slotInstances 0xD4..0xE3, v5, sum refused | present; lodifile.cpp:1176. C urban: all 33,123 in slot 0 | NATIVE s4 row 0xD4 | OK |
| 1051 | row 2 fullTriangles; 2,974 of 2,974, max 149,282 | field + recount OK (lodofile.cpp:2131). Numbers are B. **C: 2,970 of 2,970, max 2,706** | lodofile.h:309 | STALE-NUMBER (B) |
| 1052 | row 3 cardCount 0xD0, > baseCount refused | present (lodofile.cpp:1836); bounded only, never recounted | NATIVE s3 row 0xD0 | OK (R0-IMPACT) |
| 1054 | row 5 WATERTIGHT = 4; 654 of 5,567; 2,617 of 2,982 refused | bit OK (lodofile.h:189). 654/5,567 = B; **C = 365 of 2,982**; 2,617 = A/C. Reader checks unknown flag bits only | lodofile.cpp mesh-flag check | STALE-NUMBER (B) |
| 1055 | row 6 decoder refuses downtown at instance 3359 | appears closed: `lodiCellAgrees` + `LODI_CELL_QUANT_TOL` (lodifile.h:712-728) and the decoder carries a QUANT_TOL; NATIVE page does not record it; unverified by a run | lodifile.h:712 | STALE (likely closed) |
| 1062 | row 13 asymmetric-drop proof owed | NATIVE s9 still "owed" | NATIVE s9 | OK |
| 1064 | row 15 | not a NATIVE cite | -- | not audited |
| 1068 | row 19 BAKEREC1 `.lodb` | NATIVE s8.1 | NATIVE s8.1 | OK |
| 1077-1079 | (a) "RULED 2026-09-16 ... `--library near` is the default" | superseded 2026-09-17: default MNAM library + no ladder; near and ladder opt-in | nifcli.cpp:7426 `bool lgNativeLadder = false`, :7456 `bool lgLibraryNear = false`; WW_CHANGES.md:712 | **WRONG** |
| 1087, 1108 | (a) cites 3.5.4, 3.5.7 | exist; s3.5.7 itself still says near is the default | NATIVE s3.5.7 | OK (NATIVE stale) |
| 1094-1100 | (a) A vs B table | still true of those two bakes; does not describe the default | -- | STALE (historical) |
| 1141 | (g) cell sort (2.1, Dev 6) | present | NATIVE s11 Dev 6 | OK |
| 1147-1148 | (h) refFormId cold, +33 %, 1.2 MiB (4.1a, Dev 7) | present | NATIVE s11 Dev 7 | OK |
| 1150-1154 | (i) weld, 117,722 over 263,876 (Dev 9) | A only; moot on default (no ladder) | NATIVE s11 Dev 9 | STALE (moot on C) |
| 1246 | `.lodo` version **3**, refuse 1 and 2 | **4**; refuse 1, 2, **3** by name | lodofile.h:78 `constexpr quint32 LODO_VERSION = 4;` | **WRONG** |
| 1247 | `.lodi` version **3**, refuse 1 and 2 | writes 3/4/5/6/7/9 conditionally; **default = 7 (512-B header)**; v8 retired, reader-only; `--scrappable` -> 9; refuse 1, 2 by name | lodifile.h version constants; NATIVE s4.11 | **WRONG** |
| 1455-1461 | 50-58 % vs ~9 %; readers still open v8 | present | NATIVE s4.11 | OK |
| 1470-1471 | offGroup 0x100, groupCount 0x108, :556 | match | lodifile.h:556-557 | OK |
| 1472-1475 | "s4.9 ... proximity join (`--identity-join`) ... within 64 units" | true in code (default proximity, gap 64 u, trees never join), but NATIVE s4.9 documents only the legacy 16 u architecture-path rule; `--identity-join` appears in NATIVE only at s4.11 line 1462 | nifcli.cpp:7471-7472 `float lgIdentityJoinGap = 64.0f;` | **WRONG** cite (SECTION-MOVED to source) |
| 1484-1486 | scrappable = v9 bit 6 (s4.12) | present; OFF by default, default bake v7 bit 6 clear | NATIVE s4.12 | OK |
| 1517-1520 | :490, :412, s4.1a | unchanged | part 5 | OK |
| 1584-1588 | s4.9, s4.12, s4.1a, no decimation | consistent | -- | OK |

Counts: OK 43, WRONG 8, STALE-NUMBER 12, STALE (status/incomplete/moot) 8, SECTION-MOVED 1 (+1 cite counted in WRONG), not audited 1.

## (4) Complete hard/soft refusal list (what the FO4CS R0 loader must implement)

SOFT (load, log, stale=1 census row, keep rendering) -- live-data mismatch only:
pluginCorpusHash, objectCorpusHash, modelCorpusHash, cardCorpusHash, loadOrderHash vs the running game
(NATIVE s5; nativeemit.cpp:3023/3033/3037).

HARD (refuse, never hide the engine LOD tree). Anchors lodofile.cpp `lodoRead` from 1727, lodifile.cpp `lodiRead` from 949.

Pairing between the two files (nativeemit.cpp:2977-2989; NATIVE s4 row 0x90): worldspace differs; plugin/object corpus hash differ; loadOrderHash differs; lodoIdentity mismatch (unless NOLIB).

`.lodo`:
- magic; v1 (1750), v2 (1753), v3 (1764, named: crossPx16 -> fullTriangles, cardCount, WATERTIGHT); any version but 4
- headerCrc32; flags bit0 clear; reserved flag bits; EDID not NUL-terminated
- clusterMaxTris != 16; vertexStride != 16; reserved 0xAC; clusterLodStride != 48; levelMax > 15
- LADDER flag vs ladderGroup/levelMax inconsistent (1828/1831); cardCount > baseCount (1836); reserved 0xCE-0xCF, 0xD4-0xFF; fileBytes
- payload 4096 alignment, order, bounds, zero pad; string-blob rules; indexCrc32
- mesh: reserved byte; levelCount 0; clusterCountL0 bad or recount; levelCount recount; sort; flags beyond ALPHA/SWAY/WATERTIGHT; maxClustersPerMesh
- cluster: vertexCount 1..48; triangleCount 1..16; ranges; reserved flags; size class; local index >= vertexCount; unused slots != 0xFF; outside its mesh; sort (mesh, material, level); ladder reserved fields; radius 0; level > levelMax; L0 error != 0; L0 sourceTriangles != triangleCount; level>0 with error 0; parent fields inconsistent; geometricError > parentError; parent range/mesh/material/level+1/parentError mismatch; CONE_OPEN carrying a cone; cone cosine outside (0,1]; levelMax vs deepest level
- material: reserved byte; layer outside 2048..0xFFFE; family; string offset; sort
- base: sort by formId; rep past meshCount; no mesh and no card; boundRadius 0; string offset; reserved flags; fullTriangles recount (2131); no mesh but fullTriangles != 0 (2134)

`.lodi`:
- magic; v1 (972), v2 (976) by name; version not in 3..9 (989); short header for version
- headerCrc32; ROW_ORDER_NORTH_UP clear; reserved flag bits; lodoIdentity 0 without NOLIB; NOLIB with lodoIdentity != 0
- EDID; chunkCells != 4; instanceStride != 24; occluderStride != 40; maxOccludersPerCell 0
- v5: placementAoStride != 1 (1083); no PAO blob (1085); placementAoCount != instanceCount (1171); slotInstances sum != instanceCount (1176)
- v6: no vertex-AO blob or vertexAoBytes < 4(n+1) (1092)
- v7/v9: neither group nor sky (1106); groupStride != 2 (1111); group present with groupCount 0 (1114); no group but count/stride set (1117); sky too small (1122); no sky but vertexSkyBytes != 0 (1125)
- v8: no 0x11C stream (1140); stream size (1143); horizonAzimuths 0 (1146); horizonReach not positive finite (1149)
- cross-version words: v7/v9 carrying v8 words (1157); v3-v6 carrying v7 words at 0x100/0x110 (1166); v3/v4 carrying PAO words at 0xE4 (1194)
- aggregates: stride != 48; v4 with count 0; views < 2; aggSwitchPx <= 0; aggBandRatio <= 1
- reserved bytes by version: from 0xB0 (v3), 0xD4 (v4), 0xF1 (v5; to 0xF3 on v6+); 0x11C-0x1FF (v7/v9); 0x130-0x1FF (v8)
- fileBytes; extent inverted; extent > 65,536; chunkCount vs extent; presentChunks > chunkCount; payload alignment/order/bounds/pad; indexCrc32 (1339)
- vertex-AO offsets: monotone, first 0, last = bytes - 4(n+1) (1378/1383)
- groups: id >= chunk placements (1407); ids not dense (1414); groupCount != sum (1419)
- sky offsets (1431/1436); sky slice != AO slice (1448); horizon offsets and slice = AO x azimuths (1470/1475/1487)
- chunk: reserved word; absent chunk not zero; order; range; cellRangeOffset; z range; chunk crc32; cell ranges must partition the chunk and sum
- occluder: empty range not zero; over cap; order; past count; reserved word/flags; bit0 clear; centre not finite; half extent <= 0; instanceIndex past count; instance outside its cell (1585)
- instance: flags beyond 0x7F; bit 6 below v9 (1604); scale 0 (1614); `lodiCellAgrees` failure, with the ambiguity band (1625); order (cell, drawKey, ref, part) (1634)
- totals: presentChunks, chunks cover every instance, maxInstancesPerChunk
- aggregate rows: reserved flags; HEIGHT clear; identity != 0x80000000|i; views mismatch; half/depthSpan/boundRadius <= 0; centre not finite; order; covers nothing; coveredFirst not running sum; bounds; covered list not ascending; instance covered twice; covered instance in no chunk or outside the aggregate's cell; total covered mismatch
- native-verify: drawKey not the base's rank (nativeemit.cpp:3074)

Known gap: cardCount is only bounded, never recounted -- the loader should recount it.

## (6) NATIVE page's own stale spots

1. s3.2 base row still prints `u16 crossPx16[4]`; v4 is `u32 fullTriangles` + `u16 crossPx16[2]` (lodofile.h:309-311).
2. s3.5.7 says near MODL is the default; superseded 2026-09-17 (WW_CHANGES.md:712). The page never records the authored-only ruling.
3. s4.1 flags say bits 6-15 reserved; bit 6 = scrappable on v9 (s4.12, lodifile.h).
4. s4.4 line 912: far-shadow pass "keys on the identity index", contradicts s4.1c (GROUP).
5. s4.4: crossPx16 "still written as 0" -- field reinterpreted in v4.
6. s4.6.1 cites "Deviation 6" for conditional lodi; that is Deviation 12.
7. s4.9 documents only the legacy 16 u architecture-path rule as shipped; the proximity-64 default and `--identity-join` are missing (only s4.11 line 1462 mentions legacy).
8. s6 line 1562 "9,710,564 bytes" vs 9,657,316 elsewhere.
9. s12: `.lodi` 0xF1-0xFF "15 reserved bytes" -- v6+ uses 0xF1-0xF3; v7+ header is 512 B.
10. Source anchor near line 2064 cites `src/nifcli.cpp:5796` for `--native-no-ladder`; now 8059 (`--native-ladder` 8060, `--identity-join` 8096).
11. s1/s3.5.3/s3.6/s7 measured numbers are all bake A; no C re-measure on the page apart from the s1 6,204,388 B line.
12. s5 hard list lacks: v3 `.lodo`, v5/v6/v8 rules, reserved-by-version, cardCount/fullTriangles, mesh flags, bit 6 below v9, aggregates, LADDER/ladderGroup/levelMax, cell ambiguity band, pairing class; s4 row 0x90 calls between-file loadOrderHash a pairing refusal while s5 lists loadOrderHash only as soft.
13. Plan s5 row 6 fix (`lodiCellAgrees`, `LODI_CELL_QUANT_TOL`) not recorded.

## Files on disk (newest default-bake pairs; paths under scratchpad/)

| folder | time | `.lodo` | `.lodi` | note |
|---|---|---|---|---|
| horizonout_20260919/scrap/nat | 2026-09-19 07:27/07:28 | 6,204,388 B md5 89407121f640c1d5161a94bd53b9ede1 | 2,449,912 B v9 (`--scrappable`), urban, 33,123 inst, 12/20, 280 occ, 1,061 groups | newest; C library, but v9 not the default version |
| horizonout_20260919/join/prox/nat | 07:25 | -- | 243,420 B v7, chunk 4.4.-12, 2,449 inst, 167 groups | true default settings (single chunk) |
| horizonout_20260919/join/legacy/nat | 07:25 | -- | 243,420 B v7, 588 groups | `--identity-join legacy` |
| viewfix_20260917/urban_ao/nat | 2026-09-18 05:18/05:19 | 6,204,388 B | 1,753,592 B v6 | pre-v7 |
| viewfix_20260917/urban_authored | 2026-09-17 23:09 | 6,204,388 B md5 10af42aaf62427ac4b3d53b59460c6b5 | 1,126,755 B v5 | first C bake |

Not default (contrast): horizonout bytecheck 132,690,554 B / 3,109,505 B (near/ladder, 09-19 06:3x); viewfix urban 223,498,874 B (near, 09-17 22:0x).
No full-urban v7 default pair and no Sanctuary C pair was found (targeted folders only; whole-scratchpad find timed out).

### Addendum: whole-scratchpad find (finished late)

Newest default pair on disk: **cellview2b_20260919/lodibake/nat** (2026-09-19 18:15): `.lodo` 6,204,388 B, `.lodi` **130,666 B, v7**.
Also defaults1_20260912/gate/e_new/nat (2026-09-19 08:32): `.lodo` 6,204,388 B, `.lodi` 129,756 B, v7.
Both `.lodi` sizes are close to A's 128,256 B v3 file, so they are likely the small (Sanctuary-sized) region,
but region, instance count and chunk counts were NOT decoded here. This is the best candidate C fixture for R0 gate 1.
