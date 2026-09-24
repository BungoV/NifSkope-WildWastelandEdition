# SUPERSEDED 2026-09-11 10:3x -- the lane finished.
#
# This file was written before the build, as insurance (CONSTITUTION 1b).
# Everything it lists as NOT done was done: one build (09:56:11) plus two
# counted relinks (10:04:40, 10:14:23), the whole gate chain green, the
# contract rewritten to v3, both pictures drawn, and the three documents
# written. It is kept as the record of that moment, not as a resume.
# The resume that matters now is scratchpad/native1b_20260911/HANDOFF_BLOCK.md
# and the report scratchpad/lane_native1b_report.md.

# NATIVE1b -- resume (written before the build, CONSTITUTION 1b)

## State at the moment this was written

**Nothing has been built.** `release/NifSkope.exe` is still NATIVE1a's
**2026-09-11 08:49:08, 20,989,440 B**. The rollback rung
`release/NifSkope.before_native1b.exe` is that exact copy (md5
`cf7fc96177afec212bb63a90c08c88c4`), written once; **never overwrite it**.
No `BUILDING` marker is up. Game down, no NifSkope running.

## What is on disk and finished

Code, all four translation units **syntax-checked RC=0** with the real flags
(`sx_native1b.sh`, delete it after the build):

| file | what changed |
|---|---|
| `src/lodofile.h` | `.lodo` **version 3**; `LODO_FLAG_LADDER` (bit 3); cluster flags bit 2 `LODO_CLUSTER_CONE_OPEN`; the mesh row's reserved word -> `clusterCountL0` + `levelCount`; **new 48-byte `LodoClusterLod`** (sphere, error, parentError, parent range, level, cone, sourceTriangles); header `offClusterLods` 0xC0, stride 0xC8, `levelMax` 0xCC, `ladderGroup` 0xCD; ladder constants; `lodoPackOct16`/`lodoUnpackOct16`; eleven new `LodoMeshStats` fields |
| `src/lodofile.cpp` | the oct16 pair; point-triangle distance + two-sided soup deviation; `lodoEmitCluster` (ONE emitter for every level: sphere, cone, size class); **the whole "pass 2" rewritten** (level 0 per material exactly as v2, then the ladder: weld, partition, lock the group border, simplify, measure, re-split, link); writer + reader for the new table and header words; `lodoDescribe` per-level rows |
| `src/lodifile.h` | `.lodi` **version 3**; `LodiOccluder` (40 B) + `LodiOccluderRange` (8 B); header 0x98/0xA0/0xA8/0xAC/0xAE; `LodiSrcInstance::hasOccluder/occCentre/occHalf`; five new `LodiWriteStats` fields |
| `src/lodifile.cpp` | the occluder selection (per cell, volume desc then index asc), the two new payloads, `indexCrc32` extended, the reader's occluder rules, `lodiDescribe` rows, and the FIXTURE updated (ladder on, the cube's cone that must open, one hand-derived occluder, v3 expect keys) |
| `src/nativeemit.cpp` | the occluder FITTER (watertight only, 16^3 voxel interior, largest box, one voxel shaved, 100-point probe) with named refusals; the ladder/occluder switches; two new census lines `native-ladder:` and `native-occluders:`; mesh report **version 2** with 13 new columns before `model` |
| `src/nativeemit.h` | `lodgenNativeBegin` gained `buildLadder` / `buildOccluders` (defaulted true) |
| `NifSkope.pro`, `src/nifcli.cpp` | through the REFUSING script `scratchpad/native1b_20260911/hookup.py` (7 anchors, each x1, CR 0 -> 0, **APPLIED**): `lib/meshoptimizer/src/partition.cpp` added, `--native-no-ladder` / `--native-no-occluders` |
| `tests/spells/lodgen_native_decode.py` | v3 throughout, through `patch_decode.py` (22 anchors x1, **APPLIED**), compiles |

Patch scripts kept: `scratchpad/native1b_20260911/{patch_pass2.py,hookup.py,patch_decode.py}`
and `pass2_new.txt`.

## What is NOT done

1. `tests/spells/lodgen_native_mutate.py` -- one mutation per NEW v3 field/rule.
2. `tests/spells/lodgen_native_fields.py` -- the v3 field gate.
3. `tests/spells/lodgen_native_cut.py` -- NEW: the reference selector (the cut,
   the partition proof, the three tolerances x three distances and the two
   floors) plus the sphere/cone containment gates and the occluder
   inside-the-mesh gate with its grown-box floor.
4. `tests/spells/lodgen_native.sh` -- the new legs.
5. **THE BUILD.** `qmake` is OWED because `NifSkope.pro` gained a source
   (`partition.cpp`). Then `make`. Then the gate chain.
6. `docs/LODGEN_NATIVE_LODO_LODI.md` -> v3 (`ww-contract-provenance`).
7. The two pictures, `images/ladder.png` and `images/occluders.png`.
8. `WW_CHANGES_ENTRY.md`, `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md`.

## The build, exactly

```
# 1. qmake FIRST -- the .pro gained lib/meshoptimizer/src/partition.cpp
# 2. game check + "whose NifSkope is that" IMMEDIATELY before the link
# 3. bungo's own window (no --port) is RENAMED ASIDE, never killed
bash tools/ww_build.sh src/lodofile.cpp src/lodifile.cpp src/nativeemit.cpp src/nifcli.cpp
```
Put `scratchpad/native1b_20260911/BUILDING` up before it and replace it with
`DONE` the moment the chain returns (MISTAKES.md 2026-09-11, lane WATER8).

## Baselines to beat or hold (measured on the rung, not quoted)

fixture decoder **56/0**, refusal set **24/0**, two writes identical,
`Synthetic.lodo` 28,903 B / `Synthetic.lodi` 16,424 B / `expect` 2,175 B;
the real pair `scratchpad/native1a_20260911/gate/native/Native/`
**5,692,388 B** and **126,512 B**; `lodgen_native.sh` 13/0, decoder on the real
pair 87/0, field gate 25/0, `lodgen_native_baseline.sh --check` 25/0,
`lodgen_terrain.sh` 26/0, `lodl_open.sh` 23/0, `ui_align.sh` 11/0,
`animws.sh` 72/0 + 1 skip, `water_ui.sh` 86/0.

## The one thing a resume must not get wrong

**`.lodo` and `.lodi` are now v3 and the readers refuse v2 BY NAME.** Any pair
left on disk from NATIVE1a (including `scratchpad/native1a_20260911/gate/`) is
a v2 pair and the new exe will refuse it -- that is the intended behaviour and
a gate leg, not a regression. Re-bake before comparing anything.
