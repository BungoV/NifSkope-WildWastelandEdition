## 2026-09-11 — the FO4CS-native far field is real: `.lodo` + `.lodi` v2 (lane NATIVE1a)

`release/NifSkope.exe` **08:49:08, 20,989,440 B** (one build 08:43:28 plus two counted
relinks). The emitter was already hooked into the build by lane BUILD6 on 2026-09-10; what
this lane did is write the FIRST REAL WORLDSPACE PAIR, gate it, and put bungo's five
performance rulings of 2026-09-11 into the bytes.

**The first real pair.** The nine-chunk Sanctuary region (cells −20..−9 × 24..35, dim 4,
AO and identity on), headless, 35 s:

* `Commonwealth.lodo` **5,692,388 B** — 2,970 bases of 2,974 in the census (4 have no
  loadable model), 2,982 meshes, 10,634 clusters, 142,138 triangles, 273,695 vertices,
  136 materials;
* `Commonwealth.lodi` **126,512 B** — **3,526 instances**, which is the stock manifests'
  placement count EXACTLY, 0 dropped, 0 unlit, 0 without a stock identity;
  **35.9 B a placement**;
* the stock `.BTO` set beside it is **byte-identical with `--native` off**: 25 of 25 files
  in the new spell, and 25 of 25 against `tests/baselines/stock_baseline.sha256`, which was
  written by the 2026-09-10 03:57:46 exe.

**Format v2, and v1 is refused by name** (a v1 file wrote zero into the two words v2 uses,
so a v2 reader would read "draw rank 0, identity 0" for every placement):

* **`loadOrderHash`**, u64, `.lodo` 0xB8 and `.lodi` 0x90 — FNV-1a 64 over each plugin's
  lower-cased base file name and its byte size, in load order. `--native-verify-corpus` (new)
  re-reads the plugin and refuses a stale pair naming the file and which of the three hashes
  moved. Measured here: `0xa056a596e2bb16e7`.
* **ONE sort law**: chunk, cell, **`drawKey`**, ref, part. `drawKey` (the record's declared
  v2 growth slot at 0x16) is the rank of the base's (primary mesh, first material) pair, so
  instances are grouped by mesh then material — bungo's item 5. The CELL stays outermost
  because the 8-byte cell-range row states ONE run per cell; stated as a deviation, his to
  overrule. 41 distinct ranks, and 787 adjacent pairs inside one cell are ordered by the rank
  where the ref alone would have ordered them the other way.
* **the stock identity survives**: the cold record's second word carries the `.bto` colour id
  (the manifest's `index`), which is what the far-shadow pass keys on. 696 distinct over the
  region, and it equals the manifest's index on all 3,526 rows.
* **the placed REFR formID** is at the instance's own index in the cold record, load-order
  mapped; the 24-byte hot record was NOT grown to 32 to duplicate it. Stated as a deviation.
* **the bound radius rule** is `base.boundRadius × scale`; a stored `scale` of 0 is now a
  refusal, and `maxBoundRadius` is computed from the QUANTISED scale — which fixed a real
  defect: 4 of 3,526 instances sat up to 0.055 u outside their own chunk's cull box.
* **GPU cache order** on every library mesh (`meshopt_optimizeVertexCache` +
  `meshopt_optimizeVertexFetchRemap`, header flag bit 2), **keeping whichever of the two
  orders reads better per shape** — meshopt read worse than Bethesda's own order on 21 of
  2,982 meshes. Measured gain, and it is small: ACMR 1.8600 → 1.8585, library vertices
  273,969 → 273,695, `.lodo` 5,696,484 → 5,692,388 B.
* **the shadow-caster silhouette rule**: the emitter counts boundary edges of the source and
  of the emitted mesh, welded by quantised position, and REFUSES a mesh that opens its
  silhouette. 2,982 meshes, 0 opened, 0 closed, worst delta 0; the floor (a hole-punched
  mesh, 430 → 433 edges) is shown red.

**New gate** `tests/spells/lodgen_native.sh`, nine legs, **13 checks, 0 failures**: the
known-answer fixture (decoder **56/0**, was 46/0), two-write byte identity, the refusal set
(**24/0**, one mutation per row rule and one per new field, CRCs re-signed so the rule and not
the checksum answers), the real bake, the stock-path identity, the decoder's ESM and manifest
legs on the real pair (**87/0**), `--native-verify --native-verify-corpus`, the v2 field gate
(**25/0**, one subsection a field with its floor), and the staleness floor. New drivers
`tests/spells/lodgen_native_mutate.py` and `lodgen_native_fields.py`.

**The v1 fixture never exercised its own order rule** (one instance a chunk). v2 puts three
instances in ONE cell, and the one with the smallest ref carries the higher rank, so the
fixture now discriminates the old law from the new one.

Suites re-run, all at baseline: `lodgen_terrain.sh` 26/0, `lodgen_identity.sh` PASS,
`lodgen_merge.sh` PASS, `lodl_write.sh` PASS, `lodl_open.sh` 23/0, `ui_align.sh` 11/0,
`animws.sh` 72/0 (1 skip), `water_ui.sh` 86/0.

Coverage instead of a render — no path turns a pair back into a renderable mesh yet:
**3,519 mutual nearest-neighbour pairs of 3,526 = 99.80%**, worst distance 0.1374 u once the
manifest's own six-significant-digit print step is budgeted (0 over the format's 0.1768 u XY
bound), picture `scratchpad/native1a_20260911/images/coverage.png`.

`docs/LODGEN_NATIVE_LODO_LODI.md` rewritten as the v2 contract, every claim traced to a live
anchor by `scratchpad/native1a_20260911/anchors.py` (60 rows, each found exactly once), with
the reserved room for lane NATIVE1b named. Its STATUS line no longer says "not yet built into
the exe" — it had been wrong since 2026-09-10 03:57.
