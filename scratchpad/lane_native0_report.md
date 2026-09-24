# Lane NATIVE0 / NATIVE0b report — the `.lodo` + `.lodi` writers, the audit, lane 0

Written incrementally (CONSTITUTION 1, 1b). Repo `E:\Projects\NifskopeWildWastelandEdition`,
main, **nothing committed**. NATIVE0 died 03:1x on the account's session rate limit;
NATIVE0b (this file, from 03:2x) inspected what it left, kept all of it, and ran the gates
that need no exe. Neither lane edited `src/lodgen.cpp`, `src/nifcli.cpp`, `src/nifskope*`,
`src/glview*`, `src/btdterrain*`, `src/lodtfile*`.

Sources as read by NATIVE0 (~02:1x): `src/lodgen.cpp` `4ccfc1d0feb09c04` 8,673 lines,
`src/nifcli.cpp` `67667c8fd974d154` 5,971. As read by NATIVE0b (03:3x): `src/lodgen.cpp`
`69de2e977898c8cf` 8,848 lines (lane CLAMP2b editing), `src/nifcli.cpp` `af2a729cb574d6d9`
5,990. The six writer files and the decoder are stamped in the contract page's footer.

## 0. What NATIVE0 left, and the verdict on each file

The relaunch brief said "three .cpp files, no headers, likely partial". Measured (`ls -la`):
**all six** `src/lodofile.{h,cpp}` `src/lodifile.{h,cpp}` `src/nativeemit.{h,cpp}` exist
(02:19–02:33), plus `tests/spells/lodgen_native_decode.py` and `audit.py` + `audit_out.txt`;
all LF-only (0 CR). Nothing was partial. Read in full, every file was KEPT UNCHANGED:

| file | lines | what it is |
|---|---|---|
| `src/lodofile.h` | 262 | constants, the five packed rows with `static_assert`s (16/56/16/16/32), header + library structs, the packing API |
| `src/lodofile.cpp` | 776 | oct 12:12 and roll-tangent packing, u16 quantiser, FNV-1a, the cluster partitioner (whichever cap binds first), writer (4,096-aligned, pad zeroed by hand, headerCrc32 over 0x10..0xFF, indexCrc32 over the seven payloads), reader with every row rule and sort law, `lodoDescribe` |
| `src/lodifile.h` | 240 | the 24-byte record, chunk/cell/cold rows with asserts, the two u16 ceilings as constants |
| `src/lodifile.cpp` | 862 | smallest-three 2+3×15 (Matrix::toQuat's arithmetic), the two u16 refusals and the 65,536-chunk cap computed over the WHOLE set before a byte is written, north-up chunk/cell order, per-chunk CRCs, writer/reader, and the synthetic fixture with its `expect` lines |
| `src/nativeemit.h/.cpp` | 89 / 552 | the accumulator hooked by four calls; full-census base table (formId ascending); objectCorpusHash over exactly what the walk reads; one load per model; materials sorted per the contract; finest-ring lighting average; PARTIAL rule; census line; `--native-verify` |
| `tests/spells/lodgen_native_decode.py` | 661 | independent decoder (struct + zlib only): every refusal, the `--expect` leg, the `--esm` leg via `tools/lod_emission_probe.py`'s walker, the `--manifest` leg |

Only mtimes moved: my `touch` (see Mistakes) put 03:29 on the three headers.

## 1. Audit — the spec's numbers against the tree today

Script `scratchpad/native0_20260910/audit.py`, output `audit_out.txt` (re-run 03:30; the sample
set now has L4 too). Sample = `scratchpad/handoff_fo4cs/samples/L*/` region (0,0)..(3,3),
today's emitter (merge, far-ring simplify, cards, arrays, `--slot-fallback` at 16/32).

| spec number | verdict | measurement |
|---|---|---|
| 682 B a placement [par] → 423.6 MiB for 481 `.BTO` | **MOVED** | today's emitter: dim 4 539.9 B, dim 8 566.5, dim 16 637.8, dim 32 798.4; four rings 42,863,767 B / 60,948 rows = **703.3 B**. The 538.3 MiB stays [arith] on that anchor until lane 0's full bake |
| 538.3 → 60.2 MiB (29.8 without cards) | **UNVERIFIABLE here** | the native side (7.02 + 5.1 MiB) is [arith]; no worldspace `.lodo`/`.lodi` has been written. The writer's census line prints both file sizes and B/placement, so the first real bake measures it |
| 8–14 draws | **UNVERIFIABLE** | consumer-side; no FO4CS reader exists. The bucket key it rests on (size class × family × class × set × alpha) is what `cluster.flags` bits 0–1 and the material row carry as built |
| the 6.17% silent drop at (−32,0) dim 32 / 2.07% at (0,−32) | **CONFIRMED in kind, MOVED in place** | `continue; // bucket full` is at `lodgen.cpp:3466` today (spec `:3144`). dim-32 sample (0,0): **87 of 30,941 rows (0.28%) have no geometry**, first missing index 26,249 — order-dependent as the spec says; dim 16: 0 of 17,709; dim 4: 0 of 3,812. The spec's own chunks were not re-baked (no exe) |
| instance record 24 B: 6+6+2+2+1+1+1+1+2+2 | **CONFIRMED** | arithmetic closes; `static_assert`s pin 24 / 32 / 8 / 8 (`lodifile.h`) and 16 / 56 / 16 / 16 / 32 (`lodofile.h`) |
| position step 0.250 u | **CONFIRMED** | 16384 / 65535 = 0.25000 |
| scale u16/8192, step 1.221e-4, ceiling 7.99988 | **CONFIRMED** | 1/8192 = 1.2207e-4; corpus reach in the samples 0.17 … 3.51 (spec [pri] 0.010 … 4.970); refusal at > 65535/8192, computed before the write |
| `baseId` u16, corpus 3,400 | **CONFIRMED (bound)** | 1,541 distinct bases across the four samples; the writer counts the full census at run time and refuses above 65,535 |
| quaternion 2+3×15 worst 0.0146° [nat] | **MOVED (better)** | 10⁵ random rotations, seed 20260910, [−1/√2, 1/√2] → 15 bits: **worst 0.0073°, mean 0.0026°**; 3×10 bits worst 0.218° (spec 0.471°). The spec's figure is a bound |
| oct 12:12 worst 0.0591° / mean 0.0209° | **CONFIRMED** | 0.0582° / 0.0209°; 8:8 0.945° / 0.336° |
| `seed` u8 = the position hash; consumer applies yaw + mirror from it | **REFUSED as written** | `treeHash % 360` needs 9 bits, the mirror 1; a u8 holds 8. AS BUILT: the quaternion carries the DRAWN rotation (ESM × yaw), `flags` bit0 the mirror, `seed = treeHash & 0xFF`; the decoder recomputes the yaw from the ESM's float position (§3, contract page §4.3, Deviations 2) |
| instance sort: chunk, then cell index | **UNDERSPECIFIED → defined** | north-up row-major inside the chunk, `cell = (3 − ly)·4 + lx` |
| mesh row 48 B | **REFUSED as written** | no model path in the row → the sort law and the reconstruction gate uncheckable; AS BUILT 56 B (+26.8 KiB worldwide) |
| SCOL share "67% of the measured chunk" | **MOVED** | 19.9% SCOL parts over the sample region (12,130 of 60,948) |
| spec line anchors `:3144 :2955 :2981 :2915 :3027 :3033 :3481 :2881 :4237` | **ALL MOVED** | → 3466, 3279, 3314, 3238 (+3578 skirt twin), 3349, 3357, 3803, 3203, 4590; `lodgen.cpp` 8,286 → 8,848 lines |

## 2. Lane 0 — the baseline harness

`tests/spells/lodgen_native_baseline.sh` (new, 193 lines, LF, 3 Python blocks compile, `bash -n`
ok). `--write` bakes the spec's region set with the NAMED exe and writes
`tests/baselines/stock_baseline.sha256` with a header (exe sha256 + mtime, `git describe`,
the profile, bake seconds) and one `sha256  name` line per output file, sorted; `--check`
re-bakes into a scratch dir and prints every CHANGED / MISSING / NEW file, exit 1 on any;
`--selftest` proves the comparator can fail without a bake. Region set: (−20,24) d4,
(−24,24) d8, (−32,16) d16 `--slot-fallback`, (−32,0) d32 (the bucket-cap chunk), and the
region (−20,24)..(−19,25) d4 with `--arrays --atlas` for the array/atlas/manifest writers;
`.BTR` deliberately not hashed (terrain has its own lanes). AO on unless `AO=0`; the profile
is in the header and a check under another profile is refused.

**Run:** `--selftest` on a two-row fake baseline (`scratchpad/native0_20260910/selftest/`):
`CHANGED chunks/Commonwealth.4.-20.24.bto … 1 differ` and the baseline against itself
0 differ — **2/2, RESULT PASS**.
**PENDING:** the `--write` bake. At my ONE check (03:2x) `tasklist` printed rc=1 but
`scratchpad/water3_20260910/DONE` was absent, so lane BUILD5b's gates held the exe by the
brief's rule; I did not poll (the exe was indeed rebuilt at 03:38:56 by another lane while
this ran). `tests/baselines/PENDING.txt` marks it. Also owed (director): the writer-mutation
half of the gate (flip one stock vertex-writer constant, rebuild, `--check` names the file),
and the wall-clock of the (−32,0) dim-32 chunk with AO.

## 3. The writers and readers as built — byte tables and refusals

The byte tables are the contract page (`docs/LODGEN_NATIVE_LODO_LODI.md`, now AS BUILT with a
52-row anchor-derived provenance footer). In one place each:

* `.lodo` header 256 B: magic `LODO` (0x4F444F4C LE), version 1, flags (bit0 vertex v1 must be
  set, bit1 PARTIAL), headerCrc32 over 0x10..0xFF, four u64 corpus hashes, 32-byte edid,
  five counts + maxClustersPerMesh, clusterMaxTris 16, vertexStride 16, stringBytes, seven
  u64 offsets, indexCrc32, reserved 0xAC, fileBytes, reserved 0xB8..0xFF.
  Rows: vertex 16 B (u16 pos[3] into the mesh AABB, u16 uv[2] into the UV rect, 3-byte oct
  12:12, roll tangent, sway, selfAO); **mesh 56 B** (10 floats, clusterFirst, clusterCount,
  flags, modelStringOffset, reserved); cluster 16 B; material 16 B (`layer` < 2048 or 0xFFFF
  unassigned); base 32 B. Local-index blob 48 B/cluster, 0xFF past triangleCount.
* `.lodi` header 256 B: magic `LODI` (0x49444F4C), flags bit0 NORTH_UP (clear = refusal), bit1
  PARTIAL, bit2 NOLIB, headerCrc32, two u64 hashes, lodoIdentity (FNV-1a 64 over headerCrc32,
  modelCorpusHash, objectCorpusHash), edid, i16 extent ×4, chunkCells 4, instanceStride 24,
  chunkCount (≤ 65,536), instanceCount, presentChunks, maxInstancesPerChunk, indexCrc32 over
  chunk table + cell ranges, four offsets, fileBytes, reserved 0x90..0xFF.
  Rows: instance 24 B (pos[3] u16 into the chunk box, rot[3] = 2-bit selector + 3×15 LSB-first,
  scale×8192, baseId, ao, sky, ground, seed, flags bits 0–5, reserved); chunk 32 B; cell
  range 8 B (16 per present chunk); cold 8 B (refFormId, i16 scolPart, flags 0).
* Refusals by name (writer): scale > 7.99988 → names the ref, part and base; baseId > 65,535
  → names the base and ref; chunk table > 65,536 → names the extreme chunk and ref; reserved
  instance flag bits; a non-finite position; edid ≥ 32 bytes; a zero identity without NOLIB
  or NOLIB with an identity; the census naming > 65,536 LOD-bearing bases; a shape with an
  index past its vertices; > 65,535 meshes / clusters per mesh / materials.
  Readers: magic (naming the OTHER family member it recognises: LODI, LDTX, LODT, LODM, DDS,
  LODV), version, both CRCs, reserved header bytes by offset, alignment and table order,
  fileBytes, every row rule (ranges, reserved words, sort laws, size class vs triangleCount,
  local index < vertexCount and 0xFF past the count, no mesh and no card, boundRadius > 0,
  cells partition the chunk in order, (cell, ref, part) order, presentChunks /
  instanceCount / maxInstancesPerChunk agree with the table).

**Deviations from the spec (contract page §10):** mesh 56 B; the drawn rotation + seed low byte;
the cell numbering; `layer` 0xFFFF; and v1 writes only what the stock ring bakes (no cards,
no crossPx16, selfAO 255, no array layers; a base with no loadable slot model is left out and
its instances are COUNTED in the census line, not written mesh-less).

## 4. The hook-up patches — NOT applied

`scratchpad/native0_20260910/HOOKUP_CHANGE_NEEDED.md`: four sites in `src/lodgen.cpp`
(include; a 50-line static loader `lodgenNativeLoadModel` flattening `LodSrcShape` through
`lodgenLoadModel` + one declaration line in `lodgen.h`; one `NativePlacement` per placement
before `const bool swaying =` (line 3371) carrying `xf.rotation` AFTER the tree yaw; one
`lodgenNativeLighting` per vertex after `const float ao = scene.ambientOcclusion(` (3630),
reading the identity index from `bucket.col[v]` before AO overwrites B), and six in
`src/nifcli.cpp` (include, three option variables, `--native <dir>` / `--native-verify` /
`--native-fixture`, usage text, `cmdLodgen` signature + two early commands + arming after
`QDir().mkpath( outDir );` (3392) + writing before `if ( arrays && !writtenBto.isEmpty() )`
(3510), the call). Every site is an anchor with `count == 1`; line numbers are for orientation.
`NifSkope.pro` IS edited (+3 headers after `src/lodtfile.h`, +3 sources after
`src/lodtfile.cpp`; 706 → 712 lines, 0 CR): **qmake is owed** before the next build
(`nifskope-ww-resume-pending` step 3). Section C of that file lists the five runs the first
build makes, in order.

## 5. Gates run, and pending

| gate | result | how |
|---|---|---|
| `g++ -fsyntax-only`, real `Makefile.Release` flags, 3 TUs | **RC=0 ×3** (only the known `qchar.h` sfinae noise) | `scratchpad/native0_20260910/sx_tmp.sh` under MSYS2 UCRT64 |
| standalone link of the writers (+ `src/io/lodvfile.cpp`) against Qt6Core | **LINK-RC=0**, `fixture_tool.exe` 377,338 B | `fixture_main.cpp`; no `release/` or `GeneratedFiles/` touched |
| the synthetic pair written | `Synthetic.lodo` 28,903 B, `.lodi` 16,408 B, `.expect.txt` 1,901 B (03:31:06) | `fixture_tool.exe scratchpad/native0_20260910/fixture` |
| C++ readers on the pair, every check on | accepted; `identity matches` | `--verify` |
| **known-answer control: the independent Python decoder vs answers written before the run** | **46 checks, 0 failures, RESULT PASS** | `lodgen_native_decode.py … --expect` |
| determinism | two writes **byte-identical** (`cmp` both files) | `fixture2/` |
| refusals, tier 1 (plain flips, 9) | all refused: 4 `.lodo` (headerCrc32 ×2, indexCrc32 ×2), 5 `.lodi` (NORTH_UP clear by name; headerCrc32 ×2; chunk crc32 ×2) — both readers | `--mutate`, `mut/` |
| refusals, tier 2 (CRCs re-signed so the ROW RULE answers, 11) | all refused BY NAME in both readers: base sort law ("row 1, 0x00010002 after 0x00010003"), cluster reserved flag, material family 2, header reserved byte 0xB9 / 0x96, `boundRadius is -216 (never 0)`, instance reserved word (×2), NOLIB with an identity, lodoIdentity pairing (decoder `FAIL pair:`; the tool prints MISMATCH). One control was mis-aimed (byte +2 of the float, not the sign byte) and redone at +3 | `resign.py`, `mut2/` |
| `(cell, ref, part)` order rule | **checked by both readers, UNEXERCISED** — one instance per chunk in the fixture; a ref flip is not an order violation. A two-instance chunk fixture is owed | `k.lodi` |
| lane-0 comparator self-test | **2/2 PASS** on a fake baseline | `--selftest`, `BASE=` override |
| **lane-0 baseline bake** | **PENDING** (exe held by BUILD5b's gates at my one check; `tests/baselines/PENDING.txt`) | `--write` |
| the ESM leg (`--esm`) and the manifest leg on a real chunk | **PENDING** — need the hook-up + a build | HOOKUP §C steps 2–3 |
| the (−32,0) asymmetric drop proof | **PENDING** — same | HOOKUP §C step 5 |
| build / qmake / harness on the built exe | **BUILD PENDING** (the exe and `lodgen.cpp` were other lanes') | `nifskope-ww-resume-pending` |

Mtimes in one table: headers 03:29 (touched), `.cpp` 02:2x–02:33, decoder 02:33,
`fixture_tool.exe` 03:31, fixture files 03:31:06, `NifSkope.pro` 03:4x, contract page 03:5x,
`release/NifSkope.exe` 03:38:56 (another lane's build, not this lane's).

## 6. Mistakes (spliced into `MISTAKES.md`, three entries, +62 lines)

1. Mine: the brief's "no headers" typed into the plan without an `ls`; three replacement
   headers drafted; the Write tool's read-first guard refused them; `touch` moved mtimes.
2. Lane SPEC's, found by NATIVE0's audit: `seed` sized at 8 bits for a 9-bit yaw (+1 mirror);
   unrecoverable from a quantised position; resolved as Deviation 2.
3. Lane SPEC's, found by NATIVE0: the 48-byte mesh row could not name its model, so its own
   sort law and the reconstruction gate were uncheckable; resolved as Deviation 1.
Also noted, not a ledger entry: one refusal control aimed at the wrong byte of a float
(caught by reading the refusal text, redone).

## 7. Finished-work skill review

Loaded: `nifskope-ww-lodgen`, `nif`, `ww-contract-provenance` (the footer is derived by
script, 52 rows, per its step 3), `ww-spec-gate-audit` (the audit table and the two REFUSED
gates with numbers are its form), `ww-control-calibration` (known answer first, refuters as
scripts), `nifskope-ww-build-verify` (the syntax pass with real flags; `${PIPESTATUS[0]}`),
`nifskope-ww-resume-pending` (the PENDING resume shape; qmake-before-make owed).
Wished for: a procedure for gating a container writer WITHOUT the exe — I re-derived the
standalone-link + fixture + independent-decoder + re-signed-mutation sequence from first
principles, and it cost two mis-aimed controls. **Written:** `ww-standalone-writer-gate`
(`<repo>/.claude/skills/ww-standalone-writer-gate/SKILL.md`; the director mirrors it to the
live tree). Declined: a skill for "add TUs to NifSkope.pro" — a 20-line anchored script
(`pro_add.py`) that will not recur often enough; `nifskope-ww-resume-pending` already says
qmake is owed after it.

## Deliverables (all under the repo)

`src/lodofile.{h,cpp}` `src/lodifile.{h,cpp}` `src/nativeemit.{h,cpp}` (NATIVE0, kept),
`tests/spells/lodgen_native_decode.py` (NATIVE0, kept), `tests/spells/lodgen_native_baseline.sh`
(new), `tests/baselines/PENDING.txt`, `NifSkope.pro` (+6), `docs/LODGEN_NATIVE_LODO_LODI.md`
(AS BUILT), `scratchpad/handoff_fo4cs/README.md` (rows + §5 + §6.1), `MISTAKES.md` (+3),
`scratchpad/native0_20260910/{audit.py,audit_out.txt,fixture_main.cpp,fixture_tool.exe,
fixture/,fixture2/,mut/,mut2/,resign.py,sx_tmp.sh,pro_add.py,docs_patch.py,mistakes_add.py,
MISTAKES_ENTRIES.md,WW_CHANGES_ENTRY.md,HOOKUP_CHANGE_NEEDED.md,selftest/}`,
`.claude/skills/ww-standalone-writer-gate/SKILL.md`. `WW_CHANGES.md` NOT edited (other lanes
alive in it; the entry text is `WW_CHANGES_ENTRY.md` for the director to splice, mixed-CR
file, `never sed -i`).

## PENDING RESUME

1. Wait for `scratchpad/water3_20260910/DONE` and `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` → rc=1.
2. `bash tests/spells/lodgen_native_baseline.sh --write` (record bake seconds; then `--selftest`, then `--check` → 0 differ).
3. When lane CLAMP2b releases `src/lodgen.cpp`: apply `HOOKUP_CHANGE_NEEDED.md` A1–A4, B1–B6 exactly (assert count==1 per anchor, 0 CR), `qmake NifSkope.pro`, `bash tools/ww_build.sh src/lodofile.cpp src/lodifile.cpp src/nativeemit.cpp src/lodgen.cpp src/nifcli.cpp`.
4. HOOKUP §C steps 1–5 in order; the decoder's `--esm`/`--manifest` legs are the gates; then `lodgen_native_baseline.sh --check` must still print 0 differ (the stock bake untouched by the hook-up).
5. Director: mirror `ww-standalone-writer-gate` to `E:\Projects\Claude\.claude\skills`; splice `WW_CHANGES_ENTRY.md`.

## Build (BUILD6)

Hook-up applied, built and gated 2026-09-10 03:56-04:02 by lane BUILD6; nothing
committed. Logs under `scratchpad/build6_20260910/logs/`, summaries
`build2_summary.txt`, `native_summary.txt`, `chain3_summary.txt`.

**The hook-up.** `scratchpad/build6_20260910/apply_hookup.py` takes the note's
own fenced blocks by index and applies A1-A4, the two `lodgen.h` lines, and
B1-B6 exactly; every anchor counted 1 of 1 (the A2 anchor was taken as the
`rung 3` banner itself, the note's "`} // namespace` + one blank line" text has
two blank lines in the file). Byte counts, 0 CR before and after: `lodgen.cpp`
388,314 -> 391,673 (LF 8,850 -> 8,924, +74), `nifcli.cpp` 261,671 -> 263,848
(LF 5,990 -> 6,030, +40), `lodgen.h` 30,436 -> 30,578 (+3 LF). Nothing outside
the note was changed. `qmake NifSkope.pro` rc 0, `make -j2` rc 0 (game check
rc=1), 03:56:58-03:57:48; recompiled `nifcli.o`, `lodgen.o`, `nativeemit.o`
(+ `main.o`, `nifskope_ui.o`, `lodgenmanager.o` from the regenerated Makefile);
`lodofile.o` / `lodifile.o` were already 03:38 (BUILD5b's qmake had the `.pro`
lines). No new warning in the touched lines (the four `lodgen.cpp` warnings are
the pre-existing ones). `Makefile.Release` read back by owning object:
`nativeemit.h` under `nifcli.o` (line 3933), `lodgen.o` (4231),
`nativeemit.o` (4299); `lodifile.h` under `nifcli.o` (3935), `lodofile.o`,
`lodifile.o`, `nativeemit.o`. Exe **03:57:46**, 18,526,720 B, sha256
`664e0de4483792b5...`; newer than every changed file under `src/ res/ tools/
tests/ NifSkope.pro`; no object older than a header it includes.

**Two corrections to the note, found by running it.** (1) `lodgen
--native-fixture <dir>` and `lodgen --native-verify a b` are refused by the
CLI with `error: 'lodgen' needs a <file>` (`nifcli.cpp:5613`); both work with
the ESM as the positional, which the early return never opens. (2) The
decoder's manifest leg holds X/Y to 0.125 u, but the manifest prints
`%g`-style 6 significant digits, so a coordinate >= 10,000 u carries a print
step of 0.1 -- see the gate table.

| gate (HOOKUP §C) | reading | verdict |
|---|---|---|
| C1 fixture through the exe | `Synthetic.lodo` 28,903 B, `.lodi` 16,408 B, `.expect.txt` 1,901 B; **byte-identical to the standalone tool's** (`cmp` both); decoder `--expect` **46 checks, 0 failures, RESULT PASS**; `--native-verify` accepted, `pair identity ok` | PASS |
| C2 the (0,0) 4x4-cell region, dim 4, `--no-ao`, `--native` | `Commonwealth.lodo` **5,696,484 B** (bases 2,970 of 2,974 in the census, 4 without a loadable model = the four `LOD\Architecture\Warehouse\WrhsLeanTo*_LOD.nif`; 2,982 meshes, 10,634 clusters, 142,138 triangles, 273,969 vertices, 136 materials); `Commonwealth.lodi` **136,992 B** (3,812 instances from 3,812 arrivals over 701,131 census refs, 0 dropped, 3,812 unlit, 1 chunk, max scale 1.7334); census line `1530.3 B a placement`; beside it `Commonwealth.4.0.0.BTO` 2,058,014 B + manifest 305,056 B | written |
| the ring set, same region, dim 8 / 16 / 32 (`--no-ao`) | `.lodo` 5,696,484 B at every dim (the base library is worldspace-wide); `.lodi` 279,624 B (8,329 instances, 4 chunks, `717.5 B a placement`) / 33,064 B (549, 13 of 16 chunks, `10436.3`) / 16,392 B (1 instance, `5712876.0`); the `.BTO` beside each 4,762,238 / 606,382 / 5,078 B | written |
| per-placement bytes vs the audit's 703.3 | the emitter's figure is (`.lodo` + `.lodi`) / instances of THAT bake, so it is the fixed 5.7 MB library divided by however many instances the region had: 1,530.3 at dim 4, **717.5 at dim 8**, 10,436 at dim 16. Over the four bakes together, 6,162,556 B / 12,691 instances = 485.6 B. The `.lodi` alone is 35.9 B an instance at dim 4 (24 B hot + cold + chunk headers). The audit's 703.3 is not reproduced by any one of these and the audit's own denominator is not recorded here; the number is reported, not matched | measured |
| C3 `--native-verify` on all four pairs | accepted, `pair identity ok`, every check on (logs `c3_verify_d*.log`) | PASS |
| C3 decoder, ESM leg (`--esm`, `--chunk 0 0 <dim>`) | all four dims: every present ref has its base form; **X/Y within 0.125 u (worst 0.1254)**; **rotation within 0.02 deg incl. the tree yaw (worst 0.0048)**; dim 4: 47,260 plain refs in the ESM, 3,690 in the table | PASS |
| C3 decoder, manifest leg | every row in the table (3,812 / 8,329 / 549 / 1; 0 not in the table), base forms match, scale within 1/16384 on all four; **X/Y within 0.125 u FAILS at dim 4 / 8 / 16**: 288 / 1,118 / 92 rows, worst 0.1711 / 0.1740 / 0.1686; dim 32 passes (**12 checks, 1 failure** x3; 12/0 at dim 32) | **FAIL as pinned; cause measured below** |
| the manifest miss, measured (`manifest_precision.py`) | 296 / 1,145 / 92 coordinates exceed 0.125; **275 / 1,109 / 91 of them are coordinates >= 10,000 u, where the manifest's 6-significant-digit print has a step of 0.1**; with half that step budgeted on top of the 0.125 quantisation bound, **0 coordinates exceed at every dim** (worst residual 0.0000). The writer's positions are inside the `.lodi` quantisation bound (the ESM leg says so at 0.1254); the decoder's manifest bar is tighter than the manifest's own print. NOT re-pinned by this lane -- the director decides whether the bar budgets the print or the manifest prints more digits | verdict |
| C4 / LANE 0 `lodgen_native_baseline.sh --write` | **RESULT PASS**, `tests/baselines/stock_baseline.sha256` written (31 lines: 6 header + **25 files**: 4 chunks, their manifests, the region's `.BTO`s, `Objects/` arrays + atlas), header `exe 664e0de4... 2026-09-10T03:57:46`, `git v0.3.3-68-g720762a-dirty`, `profile ao=1 identity=1 arrays=1 atlas=1 slot-fallback=dim16 exclude=BTR`, **`bake-seconds 8`**; `PENDING.txt` removed | PASS |
| `--selftest` | a one-digit flip names exactly `chunks/Commonwealth.16.-32.16.bto`, 1 differ; the baseline against itself 0 differ; **0 failures, RESULT PASS** | PASS |
| `--check` (once) | **25 files in the baseline, 25 baked, 0 differ**; profile matches; same exe sha both sides; wall 8 s | PASS |
| C5 the (-32,0) dim-32 `--slot-fallback --native` asymmetric-drop proof | **not run** -- not in BUILD6's brief; the baseline bakes that chunk stock (without `--native`) and hashes it | skipped, named |

**The wall time, stated exactly.** The lane-0 harness bakes a REGION SET, not
a worldspace: chunks (-20,24) d4, (-24,24) d8, (-32,16) d16 `--slot-fallback`,
(-32,0) d32 (the bucket-cap chunk, 42,560 placements), and the 2x2-cell region
(-20,24)..(-19,25) at d4 with `--arrays --atlas`, AO on, identity on. That set
took **8 s wall** on this exe (`--write` 04:00:07-04:00:15; `--check` the same
8 s). It is the first measured stock-bake number for this tree and it goes into
`scratchpad/handoff_fo4cs/README.md` as what it is. **A full Commonwealth bake
was NOT timed by this lane** -- the brief read the harness as a whole-worldspace
bake and it is not one; bungo's GUI bake will be the first full-worldspace
number, and no figure is invented for it here.

**CLAMP2's gates 1 and 2 re-run on this exe** (the terrain code is the same
`lodgen.cpp` bytes, relinked): `ringcontrol.sh` 13/0 PASS; `lodgen_terrain_vt.sh`
35 checks, 1 failure -- the same V9b `msn differs: Commonwealth.4.-20.28` (see
`lane_clamp2_report.md` "## Build (BUILD6)"); the hook-up moved nothing there.

Mtimes in one table: `lodgen.cpp` / `nifcli.cpp` / `lodgen.h` 03:56:5x (the
patch), `Makefile.Release` 03:56:5x, objects 03:57:15-03:57:44, exe 03:57:46,
native bakes 03:59:20-04:00:07, baseline 04:00:07-04:00:24, chain3 04:01:31-
04:02:13. Owed after this lane: the manifest-leg bar (director's call); C5; the
writer-mutation half of lane 0 (flip a stock vertex-writer constant, rebuild,
`--check` must name the file -- needs a build the director schedules); the
`(cell, ref, part)` two-instance fixture; a consumer in FO4CS; and
`scratchpad/handoff_fo4cs/README.md`'s note on the audit's 703.3 denominator.
