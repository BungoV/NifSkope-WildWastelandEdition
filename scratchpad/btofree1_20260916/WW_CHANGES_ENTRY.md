## 2026-09-16 -- lodgen: the FO4CS bake stops leaving `.BTO` chunks in the mod folder

Under the FO4 Community Shaders target the mod folder now receives our own types
and nothing else: `.lodl .lodt .lodo .lodi .lodm`, the texture and card arrays,
the heightmap DDS and the `<chunk>.BTO.manifest.txt` sidecars. The `.BTO` chunk
files are gone from it.

They are still **built**, because five passes read a chunk back after it is
written -- the texture arrays, the atlas, `lodgenMergeChunkShapes`,
`lodgenSimplifyFarRings` and the card arrays, each of which opens the manifest
sidecar beside the file it is reading. They are just not built where the game
looks:

```
<mod folder>/lodgen_bto_scratch/Commonwealth.4.-20.24.BTO
                               /Commonwealth.4.-20.24.BTO.manifest.txt
```

Every read-back runs there, with the sidecar beside the chunk exactly as before.
The teardown then moves each **manifest** into `meshes/terrain/<worldspace>/` and
removes the chunks and the folder. The folder sits inside the mod folder rather
than in `%TEMP%` so that an interrupted bake leaves its scaffolding where it can
be seen, and a run that finds one left by a dead bake removes it first.

One function does the teardown for both front ends -- `lodgenDropBtoScratch()` in
`src/lodgenchunkpass.cpp` -- because `tests/spells/lodgen_byte_gate.sh` phase (c)
compares what the panel and the command line leave on disk, file by file.

### The way back

| | |
|---|---|
| command line | `--keep-bto` |
| panel | **Object modules -> Keep legacy .BTO chunks** (default **OFF**, offered under the FO4CS target only) |

Either one leaves the chunks in the mod folder **byte for byte** as a bake from
before today left them. The manifest sidecar is kept either way.

### The stock engine target does not change

`--keep-bto` is consulted only when `--native <dir>` is given, and the panel
creates a scratch folder only when the native row is on, so a stock bake never
reaches the branch. The whole stock output is byte-identical to the previous
exe's, measured on its own bake rather than inferred.

### The census says which happened

`bake census:` gains a clause, and it is a measurement -- the count and the bytes
are read off the files themselves, not predicted from the job list:

```
bto built in scratch <dir>, 1 chunk(s), 1 dropped, 860743 bytes freed   <- the default
bto built in the mod folder, 1 chunk(s), 0 dropped, 0 bytes freed       <- --keep-bto
```

and the command line prints its own line beside it, for the default only:

```
bto scratch: 1 chunk(s) built in <dir>, 1 removed, 1 manifest sidecar(s) kept, 860743 bytes freed
```

Those are the real numbers off the gate's own one-chunk bake of Commonwealth
(-20,24) at dim 4; a bigger bake prints bigger ones the same way.

The `.lodb` ledger records **outputs**, so with a scratch it no longer digests the
chunk -- digesting a file that is about to be deleted is the defect that once
forced full rebakes. It records the manifest at its final path instead.

### The object-coverage check in `native_open.sh` was a wrong check, and now it is not

Check (c) rendered the `.lodi` scene and the chunk's own `.BTO` from one camera
and demanded `IoU >= 0.95`. It has printed 0.8179 on every exe that ever ran it:
it never passed, so 0.95 was never a measurement of these two pictures. It could
not be. The scene places whole library models; the chunk carries a mesh that the
bake merged, far-ring cut and clipped to the chunk box. Their outlines differ by
construction.

Measured at chunk (-20,24) dim 4:

| library | IoU | covered | area vs the `.BTO` |
|---|---|---|---|
| `mnam` | 0.8179 | 0.9874 | 1.195 |
| `near` (today's default) | 0.6190 | 0.9604 | 1.512 |

and 100.0 % of the scene's excess pixels sit within 16 px of a pixel the two
share, with the largest connected blob of excess at 143 px and none over 200 px:
one silhouette drawn a hair wider than the other, everywhere.

The check now asks the two questions it always wanted -- does the scene draw
**everything** the chunk draws (floor 0.90), and does it do that without simply
drawing the world (area at most 2.00 x the chunk) -- with three refuters that
fire: the chunk mirrored in Y scores 0.1800, a solid frame scores a perfect
1.0000 coverage and is caught by the area bar at 5.5119, and a different chunk's
`.BTO` scores 0.3750. `IOU` is still printed; nothing is gated on it.

### Gates

* `tests/spells/lodgen_btofree.sh` -- new. (a) the default FO4CS bake leaves zero
  `.BTO` and no scratch folder, and every other file is byte-identical to the
  previous exe's, with the previous exe's own chunk count as the refuter; (b)
  `--keep-bto` reproduces the whole tree; (c) the stock target reproduces the
  whole tree; (d) the census clause is written and it moves.
* `tests/spells/native_open.sh` -- **17 checks** (was 14), 0 failures. Its fixture
  was a version-3 `.lodo` the current exe refuses by name and has been re-baked.
* `tests/spells/lodgen_native.sh` check 4 now spells `--keep-bto`, so check 5 can
  go on comparing the stock chunk files with and without `--native`, and a new
  check reads the chunks the way back left behind.
* `tests/spells/lodgen_byte_gate.sh` phase (c) asks that **both** front ends
  dropped the chunk and that neither left a scratch folder.
* `WW_LODGEN_TEST` gains the new row in `wwNewRows[]` and three checks: the row is
  offered under FO4CS and ships off, hidden under the stock engine, and the
  sentence beside Generate says the chunks are built in a scratch folder and
  dropped.

### What the numbers came out at

Built 2026-09-16 17:11:17, `release/NifSkope.exe` 22,356,992 bytes. Every count
below was read out of that run's own log.

| gate | result |
|---|---|
| `tests/spells/lodgen_btofree.sh` | **23 checks, 0 failures** (new) |
| `tests/spells/native_open.sh` | **17 checks, 0 failures, 2 skipped** (was 14 checks and never green) |
| `tests/spells/lodgen_native.sh` | **125 checks, 0 failures, 2 skips** |
| `tests/spells/lodgen_ladder.sh` | **22 checks, 0 failures, 0 skips** |
| `tests/spells/lodgen_native_baseline.sh --check` | **25 files, 25 baked, 0 differ** |
| `tests/spells/lodgen_defaults.sh` | **28 checks, 0 failures** |
| `tests/spells/lod_generation.sh` (the panel self-test) | **124 checks, 0 failures** |
| `tests/spells/lodgen_byte_gate.sh` (b), the per-row sweep | **128 checks, 0 failures** (floor 125) |
| `tools/lodgen_native_decode.py` | **6 pairs, 36 checks, 0 failures** |

`lodgen_native.sh` check 5 compares the stock output with and without `--native`.
It now compares **25 stock files, 0 differ** and hands the `.lodb` ledger to a new
field-by-field comparator, `tests/spells/lodgen_btofree_ledger.py`, which asks that
the two ledgers differ **only** in the command-line digest and that every recorded
chunk digest is alike. A byte compare of the ledger could not stay honest once one
of the two runs spells `--keep-bto`, because the switch is deliberately inside the
digest -- it decides what is on disk -- and loosening the check to skip the file
would have skipped the chunk digests with it.

### Two differences phase (c) reports that this change did not cause

`lodgen_byte_gate.sh` phase (c) compares the panel's tree with the command line's.
It reads **15 identical, 2 differ**: `Commonwealth.4.-20.24.DDS` (174,888 bytes on
both sides, payload different from byte 129 on) and `Commonwealth.lodi` (41,638
bytes on both sides). The previous exe, run through the same gate, reports the
**same two files** plus the `.BTO` this change removes -- 14 identical, 3 differ --
and the panel's own tree is byte-identical across the two exes on all fourteen
surviving files. The pair is a front-end divergence that predates this work and is
left where it is, named rather than absorbed.

Closes `docs/FO4CS_IMPROVED_LOD_PLAN.md` §5 row 7 and rules §6 (k).
