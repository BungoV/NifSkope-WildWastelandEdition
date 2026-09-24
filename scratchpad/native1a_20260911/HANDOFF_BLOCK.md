- NATIVE1a block, spliced (lane text verbatim):

**NATIVE1a LANDED AND IS GATED, EXE FREE.** `release/NifSkope.exe`
**2026-09-11 08:49:08**, **20,989,440 bytes**, sha256 `140bb8d879cb2cf3...` (SKEL2's was
07:58:59, 20,927,488). ONE build (08:43:28) plus **TWO counted relinks** (08:47:59, the two
defects the gate found; 08:49:08, a measurement bug of my own). Markers:
`scratchpad/native1a_20260911/DONE` in, `BUILDING` gone. Rung
`release/NifSkope.before_native1a.exe` = the 07:58:59 bytes exactly, md5-verified, written
once. Report `scratchpad/lane_native1a_report.md`; entry text
`scratchpad/native1a_20260911/WW_CHANGES_ENTRY.md`; **five MISTAKES entries NOT appended by
the lane** -- `scratchpad/native1a_20260911/MISTAKES_ENTRIES.md`. **Nothing committed.**

## THE PREMISE OF THE BRIEF WAS STALE, AND THAT IS THE FIRST THING TO KNOW

The brief's item 1 -- apply NATIVE0b's owed hook-up -- was **already done**. Lane BUILD6
applied it, ran qmake, built and gated it on **2026-09-10 03:57**, and said so only in a
section at the END of `scratchpad/lane_native0_report.md`. The contract page's STATUS block
at the TOP still said "NOT YET BUILT INTO THE EXE", and so did the brief. Measured, not
inferred: every A1-A4 / B1-B6 anchor present exactly once, `Makefile.Release` naming the
three sources, their objects on disk older than the 07:58:59 exe -- and the exe ran
`--native-fixture` and a real region bake before a line of this lane's code was written. The
STATUS line is now current and this lane owns keeping it so.

## What bungo gets

**The first real worldspace pair exists.** Nine-chunk Sanctuary, dim 4, 35 s headless:
`Commonwealth.lodo` **5,692,388 B** (2,970 bases, 2,982 meshes, 10,634 clusters, 142,138
triangles, 273,695 vertices) and `Commonwealth.lodi` **126,512 B** for **3,526 placements --
the stock manifests' count exactly, 0 dropped**. 35.9 bytes a placement, which reaches the
spec's planned 5.1 MiB at the spec's own placement count.

**His five performance rulings of 2026-09-11 are in the bytes**, each with a test that shows
the field moves and a floor that shows the test can fail: the staleness hash ("Add it"), the
instance-to-REFR join, the bound-radius rule, instances pre-sorted by mesh then material, GPU
cache order, the identity that the far shadows key on, and the shadow-caster silhouette rule.

**The stock path did not move.** 25 of 25 output files byte-identical with `--native` off,
AND 25 of 25 against the lane-0 baseline written by the 2026-09-10 03:57:46 exe.

## Gates (all on the 08:49:08 exe, sequential, one instance)

| gate | numbers | baseline |
|---|---|---|
| `lodgen_native.sh` (NEW, nine legs) | **13 checks, 0 failures, PASS** | newly registered |
| -- the decoder on the fixture | **56 / 0** | 46 / 0 (+10 v2 expect keys) |
| -- the refusal set | **24 / 0**, each refused BY NAME | 20 (+9 v2, 1 corrected) |
| -- stock path with `--native` off | **25 files, 0 differ** | -- |
| -- the decoder on the real pair | **87 / 0** | 72 / 9 on the rung (both 9 were the decoder's own bars) |
| -- the v2 field gate | **25 / 0** | newly registered |
| -- the staleness floor | refused, naming the file and the field | newly registered |
| `lodgen_native_baseline.sh --check` | **25 in, 25 baked, 0 differ, PASS** | 25 / 0 |
| `lodgen_terrain.sh` | **26 / 0** | 26 / 0 |
| `lodgen_identity.sh` / `lodgen_merge.sh` / `lodl_write.sh` | **PASS** | PASS |
| `lodl_open.sh` | **23 / 0** | 23 / 0 |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 |
| `animws.sh` | **72 / 0, 1 skip, PASS** | 72 / 0, 1 skip |
| `water_ui.sh` | **86 / 0, PASS** | 86 / 0 |

Three counts moved and each is named: the fixture decoder 46 -> 56 (ten v2 expect keys); the
mutation set 20 -> 24 (nine v2 mutations, and the `indexCrc32` case replaced because
`headerCrc32` covers the stored value and was answering instead); `water_ui.sh` 82 -> 86 with
the four `SHOT=` arguments, the same arming rule SKEL2 recorded.

## Three real defects the gate found, all fixed

1. **4 of 3,526 instances sat up to 0.055 u outside their own chunk's cull box** -- the
   writer computed `maxBoundRadius` from the unquantised scale and the consumer cannot.
2. **meshopt's cache optimiser read WORSE than Bethesda's own order on 21 of 2,982 meshes.**
   The writer now keeps whichever order is better, per shape.
3. **My own:** fixing (2) dropped the "after" accumulators, so the census printed
   `acmr 1.8600 -> 0.0000` and the gate called that an improvement. Caught by reading the
   number, not the verdict.

And two of my own instruments were wrong and were fixed as instruments: the decoder's
manifest position bar was measuring the manifest's six-significant-digit printf (step 1.0 at
six-digit coordinates -- 3,356 of 3,526 y values), and the cache-order floor encoded an
expectation about Bethesda's corpus instead of testing the metric.

## FOR BUNGO -- two calls, both written into the contract as deviations

1. **The mesh/material sort sits inside the CELL, not inside the chunk.** Mesh-major inside a
   chunk would leave one cell's instances in up to sixteen runs, which the 8-byte cell-range
   row cannot describe, and that blob is what the near-field suppression reads. A cell is
   4,096 units, so the grouping is still contiguous per (cell, mesh, material).
2. **The placed REFR form id stayed in the 8-byte cold record**, at the instance's own index,
   instead of growing the hot record from 24 to 32 bytes. The join is already one array read
   with no search; growing it is +33% on the one buffer the per-frame cull dispatch reads
   (~1.2 MiB on a full Commonwealth) to duplicate a number already there.

Either is a v3 format break if he wants it the other way round, and the readers refuse the
old stride or the old order by name.

## Owed / red

* **The (-32,0) dim-32 asymmetric drop proof was NOT run** -- it needs its own
  `--slot-fallback --native` bake. The census gate it backs is exact on this region.
* **The LOD panel has no `.lodo`/`.lodi` row at all**, and prints no stage times for any
  output today, so **the four stage times are NOT delivered**; the row is lane LODUI1's.
* Nothing in FO4CS reads either file; there is no reconstruction path, so parity is coverage
  numbers plus a point-set picture (`images/coverage.png`: 3,519 mutual pairs of 3,526 =
  99.80%, worst 0.1374 u once the manifest's print step is budgeted, 0 over the format's
  bound).
* `WW_CHANGES.md`, `HANDOFF.md` and `MISTAKES.md` NOT edited by the lane -- text is in
  `scratchpad/native1a_20260911/`.

## What NATIVE1b inherits

The cluster hierarchy is v3 and the room is named in the contract's new section 12:
`.lodo` header **0xC0..0xFF (64 reserved bytes)** for the cluster-error table's offsets,
`LodoCluster.flags` **bits 2-15** for the normal cone, `LodoBase.crossPx16[4]` already in the
row and written as 0, `.lodi` header **0x98..0xFF (104 reserved bytes)** for the occluder
table. The library stores full-detail geometry -- nothing is decimated -- so the ladder bungo
asked to run "from full detail down" has somewhere to start. **The instance record's growth
slot is GONE** (v2 took 0x16 for `drawKey`): a per-instance tint or light index now means a
32-byte stride and a reader that refuses 24 by name.

**RESTART: yes** -- any window bungo has open predates 08:49:08.

Skills amended in the REPO tree, for the director to mirror to the live one:
`nifskope-ww-lodgen` (the relative-path trap; the manifest-printf trap) and
`ww-standalone-writer-gate` (name the rule that must answer).
