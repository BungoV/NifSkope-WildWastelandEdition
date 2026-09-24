## 2026-09-10 - the FO4CS-native far field has a writer: `.lodo` + `.lodi`, read back independently, not yet built into the exe

`src/lodofile.{h,cpp}` (new: the object library), `src/lodifile.{h,cpp}` (new: the
instance table + the synthetic fixture), `src/nativeemit.{h,cpp}` (new: the emitter and
`--native-verify`), `tests/spells/lodgen_native_decode.py` (new: the independent
decoder), `tests/spells/lodgen_native_baseline.sh` (new: lane 0), `NifSkope.pro` (+6
lines), `docs/LODGEN_NATIVE_LODO_LODI.md` (SPEC -> AS BUILT, deviations, an
anchor-derived provenance footer of 52 rows), `scratchpad/handoff_fo4cs/README.md`
(the two rows, section 5, section 6 item 1), `MISTAKES.md` (three entries),
`scratchpad/native0_20260910/` (the audit, the standalone fixture tool, the refusal
controls, `HOOKUP_CHANGE_NEEDED.md`), `scratchpad/lane_native0_report.md`. Lanes
NATIVE0 (killed 03:1x by the rate limit) and NATIVE0b.

**What exists.** `lodoWrite`/`lodoRead` for the library (five packed rows with their
strides pinned by `static_assert`: vertex 16 B, mesh **56 B**, cluster 16, material 16,
base 32; oct 12:12 normals, a roll-angle tangent, a 16-triangle / 48-vertex cluster
partitioner where whichever cap binds first closes the cluster), `lodiWrite`/`lodiRead`
for the instances (24-byte records, smallest-three 2 + 3 x 15 rotation, the two u16
refusals computed over the whole set BEFORE a byte is written and naming the ref or the
base, the 65,536-chunk cap naming the extreme chunk, north-up chunk and cell order,
per-chunk CRCs), and `lodgenNativeWrite`, which builds the base table from the FULL
worldspace census (formId ascending, so `baseId` is worldspace-stable in a one-chunk
bake), hashes exactly what the object walk reads into `objectCorpusHash`, loads each
model once, and averages the finest ring's AO / sky / ground into the record.

**What was measured.** Standalone link of the three writer TUs against Qt6Core
(`scratchpad/native0_20260910/fixture_tool.exe`, no NifSkope object touched): the
synthetic 3-instance / 2-mesh worldspace written by hand (`fixture/Synthetic.lodo`
28,903 B, `.lodi` 16,408 B); the Python decoder, which shares no code with the writers,
passed **46 checks, 0 failures** against answers written before the run (positions to
0.125 u, rotation to 0.02 deg including a 123-degree tree yaw, every count, every sort,
the pairing identity); two writes **byte-identical**; **20 single-byte mutations
refused**, 9 by the CRCs and **11 with the CRCs re-signed so the row rule itself
answered by name** (base sort law, cluster reserved flag, material family, reserved
header bytes in both files, `boundRadius` -216, instance reserved word / reserved flag,
NOLIB with an identity, `ROW_ORDER_NORTH_UP` clear, the pairing identity). All three
TUs pass `g++ -fsyntax-only` with the flags of `Makefile.Release`. The lane-0 harness's
comparator names a one-digit flip (self-test 2/2).

**Two deviations from the spec, ratified on the contract page with the measurement:**
the mesh row is 56 B because the spec's 48 carried no model path and its own sort law
was uncheckable; the instance rotation is the DRAWN one (ESM x tree yaw) with `seed` =
the hash's low byte, because `treeHash % 360` needs 9 bits and the u8 holds 8
(`audit_out.txt` section 3) -- the decoder's ESM leg recomputes the yaw from the
plugin's float position, so the cross-check the spec feared losing still runs.

**The audit** (`audit.py`): the spec's 682 B a placement is 703.3 on today's emitter
over the (0,0) sample set; the silent bucket-cap drop is confirmed in kind (87 of 30,941
rows without geometry on the (0,0) dim-32 sample, first missing index 26,249) but the
spec's 6.17% chunk (-32,0) was not re-baked; the quaternion's worst error is 0.0073 deg
(the spec's 0.0146 was a bound); every `lodgen.cpp` line anchor in the spec has moved
(8,286 -> 8,848 lines); 538.3 -> 60.2 MiB and 8-14 draws stay [arith] until lane 0's
bake and a consumer exist.

**NOT done, and why.** No build: lane BUILD5b held the exe and `src/lodgen.cpp` was
lane CLAMP2b's. The one hook-up (four sites in lodgen.cpp, six in nifcli.cpp) is an
exact patch in `HOOKUP_CHANGE_NEEDED.md`, not applied; qmake is owed after it (three
new sources). Lane 0's `stock_baseline.sha256` is **PENDING** (`tests/baselines/
PENDING.txt`): the bake needs the exe, and the writer-mutation half of its gate needs a
rebuild. No real worldspace `.lodo`/`.lodi` exists; nothing in FO4CS reads one. The
`(cell, ref, part)` order rule is checked by both readers but unexercised by the
one-instance-per-chunk fixture.
