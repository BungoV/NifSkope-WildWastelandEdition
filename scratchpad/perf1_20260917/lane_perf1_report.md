# Lane PERF1 -- report

**Exe at launch**, read off disk by this lane (`ls -la --time-style=full-iso`):
`release/NifSkope.exe` **2026-09-17 07:33:21.117** (`.608617900 +0200`),
**22,534,144 B** -- INCR1's build, as the brief says. Lane opened 2026-09-17
08:40:25 (`date`, same turn). Game check `tasklist | grep -i -E
"Fallout4|NifSkope"` -> `rc=1`, nothing running, run again before every build
and every exe launch below.

Rung taken ONCE before the first build: `release/NifSkope.before_perf1.exe`
(section 9 carries its mtime and size, read back after the copy).

**State of this report, 2026-09-17 15:4x.** All thirteen sections are finished.
Every number in them is measured on disk by this lane, or labelled with whose it
is. Both sides of the nine neighbour gates are complete; section 8.3 records
that the "after" side was interrupted twice by the game coming up, and how it
was finished.

---

## 1. Measured FIRST, on the exe at launch, before any source change

Driver `scratchpad/perf1_20260917/s1_measure.sh` (one bake per invocation, the
ruled FO4CS pipeline: `--native <dir> --cover --roads --road-detail 1`, dim 4,
`--worldspace 3C`, plugin `X:/.../Fallout 4/Data/Fallout4.esm`, data root
`E:/Tools/Fallout 4/DataUnpacked/Data`). Comparer
`scratchpad/perf1_20260917/treecmp.py`, which uses the tree's ONE record reader
`tests/spells/lodb_read.py` for the `.lodb` and never a parser of its own.

Regions, named by cell range:

* **A = `sanctuary9`**, `--terrain-region -24 16 -13 27` dim 4 -- **9 chunks**,
  the region INCR1 used.
* **B = `boston16`**, `--terrain-region -24 16 -9 31` dim 4 -- **16 chunks**,
  the same origin widened to 4x4 chunks, chosen so the chunk count is the only
  thing that moves between A and B.

### 1.1 Wall clock, stage times, peak working set, census bound-by

`a` = first run after the region's out-dir was created (cold OS file cache for
that tree); `b` = the identical command repeated immediately after (warm).

| region | chunks | `--chunk-threads` | run | wall s | landscape s | meshes s | textures s | impostors s | peak working set | bound by |
|---|---|---|---|---|---|---|---|---|---|---|
| A sanctuary9 | 9 | 1 | a (cold) | 114.3 | 0.0 | 92.0 | 18.1 | 0.0 | 2.29 GB | asked |
| A sanctuary9 | 9 | 1 | b (warm) | **82.0** | 0.0 | 69.3 | 9.8 | 0.0 | 2.29 GB | asked |
| A sanctuary9 | 9 | 8 | a (cold) | 90.4 | 0.0 | 68.2 | 20.4 | 0.0 | 9.29 GB | asked |
| A sanctuary9 | 9 | 8 | b (warm) | **74.6** | 0.0 | 66.0 | 7.1 | 0.0 | 9.30 GB | asked |
| B boston16 | 16 | 1 | a (cold) | 92.2 | 0.0 | 72.7 | 15.4 | 0.0 | 2.55 GB | asked |
| B boston16 | 16 | 1 | b (warm) | **92.8** | 0.0 | 74.1 | 14.7 | 0.0 | 2.55 GB | asked |
| B boston16 | 16 | 8 | a (cold) | 92.2 | 0.0 | 76.4 | 13.9 | 0.0 | 10.75 GB | asked |
| B boston16 | 16 | 8 | b (warm) | **92.5** | 0.0 | 69.9 | 21.0 | 0.0 | 10.83 GB | asked |

Read straight off that table, before any source changed:

* **the chunk fan-out is not the prize.** Region A warm 82.0 s -> 74.6 s (-9 %)
  for 4x the memory (2.29 -> 9.30 GB); region B warm 92.8 s -> 92.5 s, no gain
  at all, for 4.2x the memory (2.55 -> 10.83 GB). RESUME3 measured the same
  shape at 16 workers and kept the default at 1 for speed and memory; this lane
  measured it again on its own exe and agrees. Section 12 carries the row.
* **seven more chunks cost 4 % of a bake.** A is 9 chunks, B is 16, same
  worldspace, and warm `meshes` goes 69.3 -> 74.1 s. Nearly everything a bake
  spends is spent whatever the region is, which is section 1.3's subject.

### 1.2 Row zero -- byte identity 1 vs 8 chunk threads, on THIS exe

`python treecmp.py work/s1/A_ct1_b work/s1/A_ct8_b --record-mask`

```
COMPARED 57 file(s); 57 identical; 0 difference(s)
VERDICT IDENTICAL
```

**Row zero is GREEN on region A**: every one of the 57 output files of a
9-chunk FO4CS bake is byte-identical between `--chunk-threads 1` and
`--chunk-threads 8`.

What `--record-mask` masks in the `.lodb`, and why each is run identity rather
than output (the mask is written out in the comparer's own docstring):

* the five volatile fields the record already declares (`baked`, the resource
  path/size/mtime, the plugin path, the `stage times:` census line, the
  `peak working set:` clause) -- masked by the tree's own reader, not by me;
* the VALUE line of the `--threads` / `--chunk-threads` switches, and the
  `threads N, chunk threads N bound by W` + `chunk workers N` clauses of the
  `bake census:` line. Those record WHAT WAS ASKED. A comparison that refused
  to mask them could not compare a 1-thread arm with an 8-thread arm at all.

Everything else in the record still compares: every chunk input digest, every
output digest, the resource kinds and their order, the plugin stack, the switch
digest.

First run of the comparer was red on two entries and both were mine, not the
bake's: `bake.log` (the shell's own transcript, now skipped by name) and the
`.lodb`'s out-dir paths, because the two arms baked into two different
directories -- BAKEREC1's own lesson (MISTAKES.md 2026-09-17 01:1x, "leg (h)
first baked into two DIFFERENT out-dirs ... so it was measuring the folder
name"). The step-7 gate bakes both arms into the SAME out-dir and moves the
first tree aside, so it needs no path substitution at all.

### 1.3 The split INSIDE `lodgenNativeWrite` -- the table that decides steps 2 to 4

Seven wall-clock reads were put at the stage boundaries of `lodgenNativeWrite`
(`src/nativeemit.cpp`) and read back by `lodgenNativeLibrarySplit()`. They were
measured BEFORE a line of parallel code was written, and they ship (step 6),
because a second scratch build would have measured a different exe.

Region A, warm, `--chunk-threads 1`, on build 1 (the split only, no fan-out).
Two runs agreeing to 0.1 s; wall 81.2 s and 81.1 s:

```
stage times: landscape 0.0 s, meshes 68.9 s, textures 9.3 s, impostors 0.0 s
  (library: census 0.1 s, models 24.2 s, ladder 36.4 s, bases 0.0 s,
   lodo write 2.1 s, instances 0.0 s, lodi write 0.0 s)
```

| stage inside the library build | seconds | share of the 81.2 s bake |
|---|---|---|
| census (`nativeObjectCensus`, the whole worldspace) | 0.1 | 0.1 % |
| **models** (one NIF parse + vertex walk per model) | **24.2** | **30 %** |
| **ladder** (weld, cluster, simplify, occluder fit, per mesh) | **36.4** | **45 %** |
| bases (the base table and the draw rank) | 0.0 | 0 % |
| `.lodo` write | 2.1 | 3 % |
| instances + `.lodi` write | 0.0 | 0 % |
| **library total** | **62.8** | **77 %** |
| the chunk pass and the post-passes (rest of `meshes` + `textures`) | 18.4 | 23 % |

**What this table decided.**

1. The prize is `models` + `ladder` = 60.6 s of an 81.2 s bake: step 2, the
   object pass, is where the lane spends itself.
2. **There is no texture stage inside the library build at all** -- it measures
   0.0 s, because `lodgenNativeWrite` writes no texture. The 9.3 s of `textures`
   belongs to the chunk pass, which already fans out per file above the BC
   encoders. Step 3 is therefore a measurement and a statement, not a rewrite;
   section 3 carries it.
3. The library build is REGION-INDEPENDENT, and the table says so twice over:
   `nativeObjectCensus` walks `world.cellBounds()`, not the asked region, and
   nine chunks vs sixteen moved `meshes` by 3.4 s (69.3 -> 74.1 warm). Seven
   more chunks are 4 % of a bake. That is why step 4's chunk fan-out cannot be
   the prize and why step 5's reuse of the `.lodo` can be.

---

## 2. Step 2 -- the object pass in parallel

### 2.1 What was changed, and where the order is kept

Two fan-outs, because the second needs a table the first completes.

**Fan-out 1, the models** (`src/nativeemit.cpp`, the loop that was
`for ( auto it = models.begin(); ... )`): per model, the loader, the radius, the
alpha/emit flags, the sway channel and the model's own material rows, all of
which touch one `Model` and nothing else. Retired in job order (ascending QMap
order, the order the serial loop had): the `modelHash` FNV **chain** -- which is
`modelCorpusHash` in the `.lodo` header and cannot be folded from per-model
digests without becoming a different number -- the FIRST-WINS `materialRows` /
`materialString` tables, and the counters.

**Fan-out 2, the ladder**: `lodoAppendMesh` appends into six shared tables and
its ids are positions in them, so a worker cannot call it. The worker calls the
new `lodoStageMesh` (`src/lodofile.cpp`), which runs the identical code against
a PRIVATE library carrying the same fixed head (flags, the two ladder knobs, the
finished material table, the worldspace EDID); the retire calls the new
`lodoMergeStagedMesh`, which rebases the staged rows -- cluster range, vertex
base, parent range, mesh id, the model string -- into the real library in
ascending job order. `lodoAppendMesh` itself is untouched. Staging runs in
batches of 256 so a staged library is never held longer than its batch.

**The shared state that had to be fixed first.** `lodgenNativeLoadModel`
(`src/lodgen.cpp`) held `static QHash<QString, QVector<LodSrcShape>> cache;` and
`lodgenLoadModel` returns a REFERENCE INTO that hash -- the exact shape
`ww-parallelise-a-stage` refuses, because the reference outlives any lock and a
rehash under another worker's reference is a use-after-free, not a lost counter.
It is now `thread_local`. It costs nothing: `lodgenNativeWrite` visits each
folded model path exactly once, so the cache never hit across models even when
it was shared. The chunk pass never had the problem -- `lodgenBuildObjectChunk`
has always declared its `modelCache` as a local, one per chunk.

Everything else the workers read (`models` by pointer, never
`QMap::operator[]`, which would insert; the loader's data root; the material
table; the library head, copied into `proto` before the fan-out) is fixed before
the fan-out and written by nobody during it.

### 2.2 Measured, region A, `--chunk-threads 1`, the same command each time

| `--threads` | model workers | ladder workers | models s | ladder s | meshes s | wall s |
|---|---|---|---|---|---|---|
| exe at launch (serial) | -- | -- | 24.2 | 36.4 | 68.9 | 82.0 |
| 1 (**the way back**) | 1 | 1 | 24.1 | 36.3 | 69.3 | 81.6 |
| 2 | 2 | 2 | 18.8 | 19.7 | 47.2 | 60.0 |
| 4 | 4 | 4 | 19.1 | 12.6 | 40.5 | 52.9 |
| 6 | 6 | 6 | 24.0 | 10.4 | 43.3 | 56.3 |
| 8 | 8 | 8 | 27.1 | 9.2 | 45.6 | 58.5 |
| 16 (uncapped, build 2) | 16 | 16 | 33.9 | 8.3 | 54.1 | 66.9 |
| **0 = machine (shipped), capped** | **4** | **16** | **19.0** | **8.4** | **36.5** | **49.1** |

`--threads 1` reproduces the exe at launch to 0.1 s in every stage. That is the
way back measured, not asserted.

**The models stage does not scale, and past four workers it is slower than the
serial loop it replaced.** 24.2 s at one worker, 18.8 at two, 19.1 at four,
24.0 at six, 27.1 at eight, 33.9 at sixteen. A NIF parse is millions of small
allocations against one process heap; the shape of the curve is measured, the
mechanism behind it is NOT proven and this lane does not claim it. The ladder
stage over the same models in the same bakes went 36.3, 19.7, 12.6, 10.4, 9.2,
8.3 -- so one worker count for both stages has to throw one of them away.

So `lodgenParallelFor` gained an optional `cap` (`src/lodgenparallel.h`): a
CEILING for one fan-out, never a floor and never a way past `--threads`
(`min( lodgenThreadCount(), cap )`). The model fan-out passes 4; the ladder
fan-out passes nothing and takes the machine. Section 12 carries the row that
asks bungo whether that 4 should be a switch, since it is measured on ONE
machine -- his: 16 logical cores, 31 GB.

**Region A, warm, shipped defaults: 82.0 s -> 49.1 s, 40 % off the bake**, and
the library build inside it 62.8 s -> 30.2 s. Peak working set 2.29 GB against
2.29 GB: the fan-out holds one model per worker and one batch of staged meshes,
not a second library.

**A disturbance, recorded rather than hidden.** Two runs in the middle of this
sweep (`A3_t4_a`, `A3_t4_b`) came back at 165 s and 226 s, with `models` at
111.7 s and `textures` at 122.4 s -- three to twelve times the numbers either
side of them. A stray `find.exe / -name TESHitEvent.h` from a 07:17 session
(not this lane's) was walking the whole filesystem throughout. The arm was
re-run (`A3_t4_c`: 56.3 s, models 18.9, ladder 12.6, within 0.3 s of the same
arm before the disturbance) and the pair is discarded as an outlier, named here
so nobody finds them in `s1_wall.txt` and thinks they are data.

### 2.3 Identity -- every file, whole output tree

`treecmp.py` over both trees, SHA-256 per file, the `.lodb` through the tree's
own reader. No difference is tolerated anywhere; the record's run-identity
clauses are masked as section 1.2 sets out, and `--build-mask` (the record
header's last field, the generator exe's byte size) is passed ONLY where the two
arms are two different exes, never for a 1-vs-N arm.

| arm | comparison | result |
|---|---|---|
| a | new exe `--threads 1` vs new exe `--threads 0` (same exe) | **57 file(s); 57 identical; 0 difference(s) -- IDENTICAL** |
| b | exe at launch (serial) vs new exe `--threads 1` | **57 identical; 0 differences -- IDENTICAL** |
| c | exe at launch (serial) vs new exe `--threads 0` (16 ladder workers) | **57 identical; 0 differences -- IDENTICAL** |

Row c is the one that matters: every byte of the `.lodo`, the `.lodi`, all 20
`FO4CSLOD` files, every `.BTO` and every `.dds` of a nine-chunk FO4CS bake is
what the serial pass produced, with the library built by sixteen threads.

---

## 3. Step 3 -- the texture pass in parallel

The brief asks for a per-file fan-out above the already-parallel BC block
encoders. Measured on the ruled FO4CS pipeline, that fan-out would buy nothing,
and here is every number that says so.

**3.1 There is no texture pass inside the library build.** Section 1.3's split
measures it directly: the library build's texture time is not small, it is
absent -- `lodgenNativeWrite` writes no texture at all, and its seven stages sum
to 62.8 s of census, models, ladder, bases and three file writes. The 9.3 s of
`textures` in that bake belongs to the chunk pass.

**3.2 The BC encoders are already fanned out, and they are not the cost.** The
tree has four `lodgenParallelFor` call sites in the texture writers
(`src/lodgen.cpp`: `lodgenWriteDdsBC5`, `lodgenWriteDds` x2,
`lodgenWriteDdsArray`), each over BC block ROWS. Above them,
`lodgenWriteChunkSheets` writes the colour sheet and then the `_msn` sheet, and
`lodgenBakeTerrainTextures` writes `_data.DDS` after both: two to three files
per chunk, one after another.

That per-file loop is what step 3 would fan out. It cannot help, because the
whole stage does not move with threads at all. Region A, nine chunks, the same
command, only `--threads` changing (section 2.2's sweep, `textures` column):

| `--threads` | 1 | 2 | 4 | 6 | 8 | 16 |
|---|---|---|---|---|---|---|
| textures s | 9.4 | 9.7 | 9.5 | 9.6 | 10.1 | 9.4 |

Sixteen threads and one thread encode the same sheets in the same 9.4 s. A
per-file fan-out above encoders that are themselves already parallel would also
NEST -- `lodgenParallelFor` turns an inner fan-out serial inside a worker, by
design (`tlsInWorker`, lane BAKEPERF1's heap fault) -- so two files each encoded
on one thread would replace two files each encoded on sixteen. There is no
version of this that is faster than what the numbers above already show.

**3.3 The per-file dimension that DOES pay is the chunk fan-out**, which
already exists and is step 4: `--chunk-threads 8` took region A's textures from
9.8 s to 7.1 s. It is the same per-file parallelism, one level up, and it costs
four times the memory (section 4).

**3.4 What is actually serial in that 9.4 s, and why this lane did not take
it.** The cost is the per-texel composite in `lodgenBakeTerrainTextures`: a
`RES x RES` loop per chunk that samples the land diffuse per texel through
`lodgenCachedTexture`. That cache is an **LRU with eviction** -- it mutates on
every HIT (`texOrder.removeOne` / `append`) and `delete`s a victim texture
whenever `texBytes` passes `texBudget`. A row fan-out over that loop would have
readers holding pointers to textures another row's miss is free to delete, and
the only ways out are a per-worker cache (sixteen cold texture caches: exactly
the memory RESUME3 measured at 5.80 GB on this region) or pinning the chunk's
whole texture working set for the duration of the loop, which is a MEMORY POLICY
this lane has no standing to move. It is written up as a divergence row in
section 12 and it is the honest next prize after this lane, worth about 9 s of a
49 s bake.


## 4. Step 4 -- the chunk fan-out, measured honestly

**The default does NOT move.** `--chunk-threads` stays 1. What follows is the
measurement that says what moving it would buy and what it costs, so bungo can
rule; the divergence row is section 12, row A.

### 4.1 The soak

`scratchpad/perf1_20260917/s4_soak.sh` bakes ONE reference tree at
`--chunk-threads 1 --threads 1` -- the whole way back, both fan-outs off -- and
then bakes N consecutive trees at `--chunk-threads 8 --threads 0` into the same
place, comparing every file of the output tree against the reference after each
run with `treecmp.py` (no `--build-mask`: same exe on both sides, so every byte
of the record is compared too, header included). A non-zero exit, a missing file
or one differing byte stops the soak and names the run.

Region A, the 9-chunk Sanctuary region `--terrain-region -24 16 -13 27 --dim 4`,
exe of 2026-09-17 09:27:17 (the step-2/3 build):

| | value |
|---|---|
| reference, `--chunk-threads 1 --threads 1` | 118 418 ms |
| runs at `--chunk-threads 8 --threads 0` | 20 |
| faults (non-zero exit, missing file, differing byte) | **0** |
| files compared per run | 57, all 57 identical, every run |
| wall clock, 20 runs | min 41 041 ms, median 41 565 ms, max 45 307 ms |

Raw: `scratchpad/perf1_20260917/s4_sanctuary9.txt`, one line per run.

### 4.2 What the chunk fan-out is actually worth

The soak's reference is the fully serial bake, so its 118.4 s is not the number
to price `--chunk-threads` against -- section 2 already banked the object pass.
Against the SHIPPED default of this lane's build (`--threads 0`,
`--chunk-threads 1`, 49.1 s warm on region A, section 2.2), eight chunk threads
buy region A 49.1 s -> 41.6 s, **-15 %**. Section 1.1 measured the same switch on
the exe at launch, where the library was serial and dominated everything, at
82.0 s -> 74.6 s, -9 %. The fan-out got RELATIVELY better because the library
stopped hiding it, and it is still the small half of the bake: the chunk loop is
about 19 s of a 49 s bake and eight threads take about 7 s off it.

Region B (16 chunks) is in section 4.3, and it is worth MORE there: 59.4 s ->
46.9 s, **-21 %**.

### 4.3 Region B, and the cost side said plainly

The same soak on region B, the 16-chunk region `--terrain-region -24 16 -9 31
--dim 4`, on the exe this lane ships. Raw:
`scratchpad/perf1_20260917/s4_boston16.txt`.

| | region A (9 chunks) | region B (16 chunks) |
|---|---|---|
| reference, `--chunk-threads 1 --threads 1` | 118 418 ms | 109 855 ms |
| runs at `--chunk-threads 8 --threads 0` | 20 | 20 |
| faults (non-zero exit, missing file, differing byte) | **0** | **0** |
| files compared per run | 57, all identical, every run | **99, all identical, every run** |
| wall clock, min / median / max | 41 041 / 41 565 / 45 307 ms | 45 501 / **46 855** / 79 020 ms |
| this lane's shipped default (`--threads 0 --chunk-threads 1`) | 49.3 s | 59.4 s |
| what 8 chunk threads buy against it | **-16 %** | **-21 %** |
| **peak working set, `--chunk-threads` 1 -> 8** | **2.29 GB -> 9.32 GB** | **2.55 GB -> 10.48 GB** |

Region B's `max` of 79 020 ms is one run of twenty and is named rather than
averaged away; it is 1.7x the median while every other run sits within 4 % of
it, which is the signature of something else on the machine, not of the bake.
The median is the number to read.

**The cost side.**

- **Peak working set is the whole argument.** Measured on this lane's exe, in
  the soak itself: region A 2.29 GB at one chunk thread and **9.32 GB** at
  eight; region B 2.55 GB and **10.48 GB**. That is about **four times the
  memory for about a fifth of the wall clock**, and a full Commonwealth bake is
  far more than sixteen chunks on a 31 GB machine. That is the reason the
  default is 1 and the reason this lane does not move it.
  **Correction:** an earlier draft of this section said 5.80 GB and cited
  section 1.1 for it. 5.80 GB is RESUME3's number for this region, quoted in
  section 3 and never measured by this lane; section 1.1's own table says 9.29
  and 9.30 GB, and the soak above says 9.32. The figure that reached
  `lodgen --help` was corrected in the same pass, and MISTAKES.md carries it.
- **Determinism is not in doubt.** Forty consecutive runs across the two
  regions, 0 faults, every one of 57 and 99 files byte-identical to the
  1-thread bake, every run.
- **The prize is elsewhere.** 16-21 % from the chunk fan-out against 40 % from
  the object pass (section 2) and, under `--incremental`, the whole library
  (section 5). Chunk parallelism is the last fifth, not the first.


## 5. Step 5 -- the library is not rebuilt for nothing (`--incremental` only)

### 5.1 The mechanism

`lodgenNativeWrite` now starts by deciding whether it has to build the library
at all. Stages 2 and 3 (every model loaded, every mesh laddered) and the `.lodo`
write are wrapped in `if ( !libraryReused ) { ... }`; everything after them --
the draw rank, the instances, the `.lodi`, every census clause -- runs from the
same names either way, filled from the file instead of from the work.

The decision is a chain of refusals, first one wins, and the census prints the
one that decided (`src/nativeemit.cpp`):

| # | test | the census says |
|---|---|---|
| 0 | the driver did not offer | `rebuilt (not offered: this is not an incremental bake)` |
| 1 | `s.occluders` | `rebuilt (occluders are on and the per-model box is in neither file)` |
| 2 | `world.loadOrderHash()` vs the record's hex | `rebuilt (the load order moved)` |
| 3 | `world.vhgtCorpusHash()` vs the record's hex | `rebuilt (the plugin corpus moved)` |
| 4 | the object census hash vs the record's hex | `rebuilt (the object census moved)` |
| 5 | no previous `.lodo` beside the new one | `rebuilt (no previous .lodo beside the one this bake would write)` |
| 6 | `lodoRead( …, payloadCheck = true )` refuses | `rebuilt (the previous .lodo did not read back: <the reader's own words>)` |
| 7 | the read header disagrees with the record | `rebuilt (the previous .lodo's own header disagrees with the record)` |
| — | all seven pass | `reused (previous .lodo kept; base census, load order, plugin corpus and object corpus unmoved, payload checked)` |

The switch digest is NOT in the chain because it cannot be: `src/nifcli.cpp`
refuses the whole incremental run when the digest moves, so a write that reaches
the decision already has it equal. The offer is carried by
`lodgenNativeOfferLibraryReuse( const NativeReuseOffer & )`
(`src/nativeemit.h`), armed from the driver beside the `lodgenNativeWrite` call
and CONSUMED by that write -- one arming buys one write, so a second write in
the same process rebuilds rather than inheriting a stale yes.

No new file. No format change. No default moved: a bake without `--incremental`
never arms the offer and takes test 0.

### 5.2 A defect the work found: `lodoRead` dropped `loadOrderHash`

The first reuse arm wrote a `.lodi` that was 12 bytes different from the full
bake's: header `0x90` (`H_LOADORDER`) was ZERO, and `0x0C`, the header CRC over
it, followed. `lodoRead` restored `pluginCorpusHash`, `objectCorpusHash`,
`modelCorpusHash` and `cardCorpusHash` into the library and had never restored
`loadOrderHash`, which v2 added at header `0xB8` (`src/lodofile.cpp`). Nothing
had noticed because, until this lane, nothing read a library back and then WROTE
from it. One line, with the refuter beside it: revert it and the reuse arm goes
red on the `.lodi`, 12 bytes apart. Entered in `MISTAKES.md`.

### 5.3 The proof -- INCR1's own two arms

`scratchpad/perf1_20260917/s5_reuse.sh`, region `-24 16 -17 23` dim 4 (INCR1's
own 4-chunk region), exe of 2026-09-17 10:17:34. Each arm: a full FO4CS bake,
then a NULL incremental (nothing moved), then a MIXED one (one `.lodj` deleted:
1 chunk rebaked, 3 replayed).

| arm | run | wall | `native-library-build:` | `.lodo`/`.lodi` vs the full bake |
|---|---|---|---|---|
| occluders OFF | full | 39 320 ms | `rebuilt (not offered…)` | — |
| | null | **2 530 ms** | **`reused`** | **byte-identical** |
| | mixed (1 rebaked, 3 replayed) | **5 284 ms** | **`reused`** | **byte-identical** |
| occluders ON (shipped) | full | 39 035 ms | `rebuilt (not offered…)` | — |
| | null | 31 141 ms | `rebuilt (occluders are on…)` | byte-identical |
| | mixed | 5 322 ms | `rebuilt (occluders are on…)` | byte-identical |

The library's own split, same arm, occluders off:

| stage | full bake | reused |
|---|---|---|
| census | 0.1 s | 0.1 s |
| models | 18.9 s | **1.5 s** (the payload-checked read of the 225 MB `.lodo`, booked here) |
| ladder | 8.3 s | 0.0 s |
| lodo write | 2.6 s | 0.0 s |
| **whole bake** | **39.3 s** | **2.5 s** |

Raw: `scratchpad/perf1_20260917/s5_reuse.txt`, logs under
`scratchpad/perf1_20260917/work/s5/`.

### 5.4 The refuter, and the hole it exposes

The brief's refuter is *touch one byte of a plugin between two incremental runs;
the library must be REBUILT and the census must say why.* The real
`Fallout4.esm` is never touched: the arm bakes off the plugin STACK
`Fallout4.esm,WWPerf1.esp`, where the second plugin is written by
`scratchpad/perf1_20260917/mkesp.py` -- one `TES4` record, one master, no other
records, so it contributes nothing to the object census or the VHGT corpus and
the only hash it can move is `loadOrderHash`.

| run | the plugin | measured `native-library-build:` |
|---|---|---|
| untouched | author `ww`, 84 bytes | `reused` |
| **R1** author `ww` -> `www` | 85 bytes, **size moved** | **`rebuilt (the load order moved)`** |
| **R2** author `www` -> `xxx` | 85 bytes, **bytes moved, size equal** | `reused` |

R1 is the refuter and it fires.

**R2 is the design working, not a hole, and it is worth being precise about
why.** `loadOrderHash` folds each plugin's lower-cased NAME and byte SIZE and
nothing else, so it cannot see a same-size edit -- but it is not the only test.
`vhgtCorpusHash` folds the VHGT bytes of every `LAND` and the object census hash
folds exactly what the object walk reads, so an in-place terrain edit or an
in-place placement edit IS caught, at any size. An author string is neither: it
cannot change a single byte of the object library, so keeping the library is the
correct answer. The three hashes together cover the plugin corpus the `.lodo` is
actually a function of.

**What they do NOT cover is the MESH corpus, and that is measured, not
supposed.** The reused run's own census reads `models 0 loaded 0 failed`: no
model file is opened at all, so a `.nif` edited on disk with no plugin change
cannot be seen by anything in the chain. The `.lodo` header carries a
`modelCorpusHash` and the record stores it, but both sides of that comparison
are the SAME previous number -- computing today's requires loading every model,
which is the 19 s stage the reuse exists to skip. Section 12 row C asks for the
cheap stamp (path, size, mtime over the census's model list) that would close
it. Until then the honest statement is: `--incremental` library reuse sees the
plugins and does not see the meshes.

### 5.5 What it is worth, and to whom

On the ruled pipeline -- occluders ON -- reuse never fires today. The measured
prize is therefore conditional, and the condition is section 12 row B (one
`.lodo` v5 field for the per-model occluder box). With occluders off it is
39.3 s -> 2.5 s on a 4-chunk region, and it scales with the library, not with
the region: the library is a whole-worldspace object, so the bigger the region
the smaller the share this saves. It is the `--incremental` workflow's prize, not
the full bake's.


## 6. Step 6 -- the stage-time split ships

### 6.1 What the line says now

`lodgenNativeLibrarySplit()` (`src/nativeemit.h`, `src/nativeemit.cpp`) returns

```
library: census A s, models B s, ladder C s, bases D s, lodo write E s, instances F s, lodi write G s, model workers H, ladder workers I
```

and it is APPENDED to the `stage times:` census line rather than given a line of
its own. A stock bake -- one with no `--native` pair -- gets the empty string and
keeps the `stage times:` line it always had, to the byte.

The two worker counts are measured, not asked: `nativeNoteWorker()` inserts
`QThread::currentThreadId()` into a set under a mutex once per job, the set is
snapshotted and cleared between the two fan-outs, and `lodgenThreadCount()` never
enters the number. That is what makes gate leg (b) able to fail: a run that
permitted 16 threads but ran one reports 1, and the gate calls it VACUOUS rather
than passing it.

Under library reuse the `models` cell holds the payload-checked read of the
previous `.lodo` (1.5 s for 225 MB, section 5.3) and `ladder` reads a true 0.0 s,
because no mesh was staged. That is documented at the assignment.

### 6.2 The count of volatile fields stays at FIVE

`ww-volatile-field-law` step 4. The split rides INSIDE an existing volatile
field, so nothing new became volatile. Verified on disk, both normalisers, this
lane, this build:

| normaliser | file | what it does with the line |
|---|---|---|
| `lodbNormalise()` | `src/lodbfile.cpp:183` | `if ( l.startsWith( "census\\tstage times:" ) )` -> writes `census\\tstage times: <volatile>` |
| `normalise()` | `tests/spells/lodb_read.py:244` | `elif f[0] == 'census' and f[1].startswith('stage times:')` -> keeps `census\\tstage times: <volatile>` |

Both replace the WHOLE line, so the split is masked by construction: there is no
spelling of the split that could leak into a normalised record. `lodb_read.py`'s
own `VOLATILE_FIELDS` count is untouched. Gate leg (e) asserts both halves --
the split is PRESENT in the census and ABSENT from the normalised record.

### 6.3 The registry

Section 5's line got its OWN keyword, `native-library-build:`, and this is the
correction of a claim an earlier draft of this section made.

It first went out as `native-library:`, and this section said it needed no
registry change because that keyword was already in `g_censusKeywords[]` at
`src/lodbfile.cpp:81`. It was -- carrying a completely different sentence:
where level 0 came from, the MNAM fallback count and placement AO. Counted on a
real record, every other census keyword appears **exactly once** and
`native-library` was now the only doubled one, so a reader grepping the keyword
would have matched two unrelated lines:

```
$ awk -F'\t' '$1=="census"{split($2,a,":"); print a[1]}' Commonwealth.lodb | sort | uniq -c | sort -rn
      2 native-library
      1 stage times
      1 native-occluders
      ... every other keyword: 1
```

So the line is now `native-library-build:`, registered in all THREE lists the
contract keeps:

| list | file |
|---|---|
| the emitter's registry | `src/lodbfile.cpp`, `g_censusKeywords[]` |
| the gate's copy of it, whose completeness floor compares the two | `tests/spells/lodgen_bakerec_gate.py`, `CENSUS_KEYWORDS` |
| the human table | `docs/LODGEN_CENSUS.md` |

It is still no new FIELD and no format bump -- a census line is what the record
already carries by keyword -- so it reaches the record through
`ww-census-contract` as before. Section 12 row D names it for bungo anyway,
because it is an addition to what every `--native` bake writes. MISTAKES.md
carries the original error.

### 6.4 No panel row

The brief's condition was *no panel row unless one already exists*. None exists
for the stage times, and none was added.

---

## 7. Step 7 -- the gate

`tests/spells/lodgen_perf.sh` (new), with `tests/spells/lodgen_treecmp.py` (new)
as its comparer. **There is no wall-clock floor in it.** A wall clock is a
weather report: a machine under load would make the spell lie, and a spell that
lies is worse than no spell. What is gated is everything that could go wrong
while going fast.

### 7.1 The five floors

| leg | what it asserts | how it could fail |
|---|---|---|
| **(a) IDENTITY** | the whole output tree, every file, `--threads 1 --chunk-threads 1` against `--threads 0 --chunk-threads 8`, on BOTH regions | one differing byte anywhere |
| **(b) NON-VACUITY** | the census's MEASURED worker counts (distinct thread ids that actually ran a job) are both >= 2 | a run that permitted 16 threads and ran 1 -- reported VACUOUS, not passed |
| **(c) THE WAY BACK** | `--threads 1 --chunk-threads 1` on this exe reproduces the RUNG exe's tree, and the one census line the rung exe never wrote is named and asserted separately | the serial path drifting |
| **(d) REUSE** | occluders off + unmoved corpus -> `reused` and a byte-identical pair on the null AND mixed arms; occluders on -> `rebuilt` with the reason | a stale library, or a reuse that is silent |
| **(e) THE SPLIT** | the `stage times:` line carries the library split AND the normalised record masks the line whole | a sixth volatile field |

### 7.2 The result, on the exe this lane ships (2026-09-17 12:45:27)

`scratchpad/perf1_20260917/logs/gate_perf_final3.log`, read back in full. It was
run AGAIN, whole, on the 12:45 link, because that link corrected the
`--chunk-threads` help text and a gate result must describe the binary that is
actually on disk (the 10:54 run, `logs/gate_perf_final2.log`, is kept and reads
the same, leg for leg):

```
(a) region A: 57 file(s), every byte identical                                ok
(a) region B: 99 file(s), every byte identical                                ok
(b) measured: model workers 4, ladder workers 16 -- 4 and 16 actually ran     ok
(c) the way back reproduces the rung exe: 57 identical; 0 differences         ok
(c) the one new census line reads: native-library-build: rebuilt (not
    offered: this is not an incremental bake)                                 ok
(d) noocc: the census says reused, as it must                                 ok
(d) noocc: null and mixed (3 replayed from cache) both write the
    byte-identical pair                                                       ok
(d) occ: the census says rebuilt, as it must                                  ok
(d) occ: null and mixed (3 replayed from cache) both write the
    byte-identical pair                                                       ok
(e) the stage-time line carries the library's split                           ok
(e) the record masks the whole line, split included                           ok

lodgen_perf: PASS
```

**11 ok, 0 RED, 0 skip**, on `release/NifSkope.exe` 22 567 424 bytes, sha1
`a843fca68c18c2740efddcb20e9fe715b7732a22`, as the gate's own header prints it.
Leg (d) also re-proves section 5 on the SHIPPED exe -- section 5's own tables
were measured on the 10:17:34 build, before the census keyword was renamed, and
this is the same behaviour measured again on the exe that ships.

### 7.3 What the gate got wrong first, and what that cost

Written down because a gate's own history is part of what it is worth.

Leg (c) was **red by construction on its first two runs**, both times mine:

1. The new exe writes one census line the rung exe never wrote. The leg had no
   way to name that concession, so the record could not match. Fixed by
   teaching the comparer `--drop-record-line PREFIX`: it removes the line from
   BOTH sides, **prints every line it drops**, and -- because a dropped line is
   not compared at all -- the leg then asserts that line's content separately,
   in the same leg. The concession is therefore read, never assumed.
2. The helper the new assertion used (`libline`) was defined halfway down the
   file, which is to say not defined for the half above it. `command not
   found`, and the assertion compared the empty string. Moved up beside the
   other helpers.

Between them they also produced a wrong diagnosis that got as far as being
written into MISTAKES.md before it was checked and corrected. All of it is in
MISTAKES.md entry 4, and the rule it earned is: *a leg that has never been seen
green is not a gate yet, and when it goes red, read the leg's own command before
theorising about the product.*

### 7.4 The three refuters

Each had to make the spell go red, and each did.

| refuter | what it breaks | what the gate must do | measured |
|---|---|---|---|
| **identity** | one worker's retire order flipped in a scratch build (`scratchpad/perf1_20260917/s7_refute_order.py break`) -- the ladder retires back-to-front inside each batch, nothing else touched | leg (a) RED | section 7.5 |
| **vacuity** | `PAR_THREADS=1 bash tests/spells/lodgen_perf.sh` -- the parallel arm asks for one thread | leg (b) **VACUOUS**, not PASS | section 7.6 |
| **reuse** | one byte of a plugin touched between two incremental runs | the library REBUILT, and the census says why | section 5.4: `rebuilt (the load order moved)` |


### 7.5 The identity refuter, and what it taught about leg (a)

`scratchpad/perf1_20260917/s7_refute_order.py` edits ONE thing in
`src/nativeemit.cpp`: the ladder's retire loop runs back to front inside each
batch. The same meshes, the same staging, the same everything -- merged in the
wrong order. Two scratch builds were made, run, and reverted.

**`break` -- flipped ALWAYS, including on the `--threads 1` arm.** Scratch exe
sha1 `f0443f5193b648fe54182ff9bf364fd7f2ee48dc`, log
`scratchpad/perf1_20260917/logs/refute_identity.log`:

```
ok      (a) region A: 57 file(s), every byte identical
ok      (a) region B: 99 file(s), every byte identical
RED     (c) --threads 1 no longer reproduces the rung exe:
            COMPARED 57 file(s); 55 identical; 2 difference(s) VERDICT DIFFER
RED     (d) noocc: the census says 'native-library-build: rebuilt (the previous
            .lodo did not read back: Commonwealth.lodo: mesh table is not sorted
            by model path at row 1)', wanted 'reused'
lodgen_perf: 2 FAIL
```

**Leg (a) stayed GREEN, and that is a real limit of leg (a), written down rather
than hidden.** Leg (a) compares two arms of the SAME exe. A defect that
reorders BOTH arms equally -- which an unconditional flip is -- is invisible to
it. What caught it was leg (c), which compares against the rung exe (not
reordered), and leg (d), whose payload-checked read named the damage in as many
words: *the mesh table is not sorted by model path at row 1*. Three legs, and
the one that fired was not the one I expected.

**`break2` -- flipped only when `lodgenThreadCount() > 1`.** That is the shape a
real threading defect has: the serial path is fine, the parallel path is not.
Scratch exe sha1 `f1421cc0d50de6e6e43cbf15159e15d41d944510`, log
`scratchpad/perf1_20260917/logs/refute_identity2.log` (both identity arms on
region A, to spend less machine time on a build that was going to be thrown
away):

```
RED     (a) region A: COMPARED 57 file(s); 55 identical; 2 difference(s) VERDICT DIFFER
```

**Leg (a) goes red, as the brief requires.** The run was stopped after leg (a)
had answered, the source was restored, and the exe rebuilt.

### 7.6 The vacuity refuter

`PAR_THREADS=1 PAR_CHUNKS=1 bash tests/spells/lodgen_perf.sh`, on the shipped
exe, log `scratchpad/perf1_20260917/logs/refute_vacuity.log`:

```
ok      (a) region A: 57 file(s), every byte identical
ok      (a) region B: 99 file(s), every byte identical
VACUOUS (b) the arm ran SERIAL (1 model, 1 ladder worker(s));
            nothing parallel was tested
lodgen_perf: 1 FAIL
```

Leg (a) passes, as it must -- serial against serial IS identical -- and that is
exactly the trap: a gate with only an identity floor would have called that run
a PASS and proved nothing at all. Leg (b) reads the MEASURED worker counts out
of the census, sees 1 and 1, and refuses: **VACUOUS, and a FAIL, not a pass.**

---
## 8. The neighbours, before and after

Run one at a time by `scratchpad/perf1_20260917/s7_neighbours.sh`, never two at
once, each with its own log under `scratchpad/perf1_20260917/logs/neighbours/`
and a tab-separated summary in `s7_before.txt` / `s7_after.txt`. "Before" is the
rung this lane was cut from, `release/NifSkope.before_perf1.exe` (22 534 144
bytes, 08:55) -- which is INCR1's exe, and that matters below. "After" is
`release/NifSkope.exe` as it ships (22 567 424 bytes, 12:45:27).

The counts in the table are the gates' OWN words, read back out of the logs by
`scratchpad/perf1_20260917/s8_tally.py`, not the runner's grep tally: a grep for
`FAIL` matches the word `COMPARED`, and a gate that prints its verdict twice
would be counted twice.

### 8.1 The table

| gate | before | after | whose the red is |
|---|---|---|---|
| `lod_generation.sh` | **PASS** 128 checks, 0 failures | **PASS** 128 checks, 0 failures | -- |
| `lodgen_defaults.sh` | **PASS** 28 checks, 0 failures | **PASS** 28 checks, 0 failures | -- |
| `lodgen_native.sh` | **FAIL** 2 red, section 5 | **FAIL** 2 red, section 5, the same two lines and the same two digests | **not mine -- red on the RUNG too**, 8.2 |
| `lodgen_bakerec.sh` | **PASS** 69 ok, 0 red | **PASS** 69 ok, 0 red | -- |
| `lodgen_layout.sh` | **PASS** 23 ok, 0 red | **PASS** 23 ok, 0 red | -- |
| `lodgen_btofree.sh` | **FAIL** 21 checks, 3 failures, legs (a)(b)(c) | **FAIL** 21 checks, 3 failures, the same three legs and the same counts | **not mine -- red on the RUNG too**, 8.2 |
| `lodgen_incremental.sh` | **PASS** 10 ok, `FAILURES: 0` | **PASS** 10 ok, `FAILURES: 0` | -- |
| `lodgen_stage_times.sh` | **FAIL** 16 checks, 1 failure | **FAIL** 16 checks, 1 failure, the same leg | **not mine -- red on the RUNG too**, LAYOUT1's one root, 8.2 |
| `lodgen_identity.sh` | **PASS** 8 ok, 0 red | **PASS** 8 ok, 0 red | -- |

**Nine gates, both sides, and the two sides are the same gate for gate, check
for check, red for red.** Six PASS on both sides; three FAIL on both sides, with
the identical legs, the identical counts and -- in `lodgen_native.sh` -- the
identical pair of digests `5399992dfee2 -> 5b6569b33c87`. **This lane made no
red.** It also cleared none, which section 8.2 says plainly rather than leaving
the reader to infer it from a table of unchanged numbers.

The check counts are the gates' OWN tallies, read back by
`scratchpad/perf1_20260917/s8_tally.py -v`, which also prints every red line on
both sides so that "the same red" is a quotation and not a count.

### 8.2 Every red, named, on the side this lane was cut from

The brief asks whether MY rung -- `before_perf1`, which is INCR1's exe --
already shows the reds the previous lanes handed on. **It does, all of them.**
That is the answer, measured, not assumed -- and the "after" side reproduces
each one line for line, which is why the table's right-hand column says "the
same":

**`lodgen_native.sh`, section 5, 2 FAIL** (`logs/neighbours/before_lodgen_native.log`
lines 75 and 77):

```
  ok   the stock bake is byte-identical with and without --native
  FAIL the second ledger differs from the first ONLY in the command-line
       digest (5399992dfee2 -> 5b6569b33c87)
  ok   and that digest DID move, so an --incremental run cannot reuse the
       other bake
  FAIL and the two ledgers differ ONLY in the command-line digest, every
       recorded chunk digest alike
```

The gate wants the stock `.lodb` and the `--native` `.lodb` to differ in exactly
one field. They differ in more, because the record is BAKEREC1's version 2 and
carries the exe, the corpus hashes, a line per plugin and the census -- and a
`--native` bake's census is not a stock bake's. Both legs are red on the rung.
Owner: **BAKEREC1** (the v2 record) meeting **BTOFREE1**'s gate.

**`lodgen_btofree.sh`, 21 checks, 3 failures** (`before_lodgen_btofree.log`):

```
    only in drop: nat/FO4CSLOD/Commonwealth/Commonwealth.4.-20.24.lodj
  FAIL (a) every other file is byte-identical to the rung's
           (0 differ, 0/1 only on one side)
  FAIL (b) every output file is byte-identical to the rung's
           (0 differ, 0/1 only on one side)
    DIFFERS: Commonwealth.lodb -> Commonwealth.lodb (716 vs 2161)
  FAIL (c) the whole stock output is byte-identical to the rung's (1 differ)
```

Legs (a) and (b): **0 differ**. Nothing that exists on both sides disagrees by a
byte; what is red is one EXTRA file, `Commonwealth.4.-20.24.lodj`, INCR1's
per-chunk cache, which that gate's own rung predates. Leg (c): the `.lodb` is
716 bytes on the gate's rung and 2 161 on ours -- BAKEREC1's version 2 record
against version 1. Owners: **INCR1** (a, b) and **BAKEREC1** (c). All three are
red on `before_perf1`, so this lane did not make them, and this lane did not fix
them either: they are a gate whose stored rung is older than two landed lanes.

**`lodgen_stage_times.sh`, 1 failure** (`before_lodgen_stage_times.log` line 10):

```
  FAIL and the run wrote the .lodo/.lodi pair --native asked for
```

The leg is `tests/spells/lodgen_stage_times.sh:80`:

```sh
want "$( [ -s "$W/region/Commonwealth.lodo" ] && [ -s "$W/region/Commonwealth.lodi" ] && echo 1 || echo 0 )"
```

It looks for the pair FLAT in the `--native` folder. **LAYOUT1** moved it, on
2026-09-16, to `FO4CSLOD/<ws>/` -- the one root this lane's whole report is
measured under -- so the pair is written, at
`<native>/FO4CSLOD/Commonwealth/Commonwealth.lodo`, and the gate looks in the
old place. The four stage numbers the gate exists to check all pass on both
sides. Owner: **LAYOUT1**. This is also the cheapest red in the set to clear
(one path in one gate), and it is named here rather than fixed because a lane
does not edit a neighbour's gate to make its own table look better.

### 8.3 How the "after" side was run, and the two halts in it

The "before" side ran in one sitting, all nine. The "after" side did not, and
the reason is in the logs rather than glossed over.

**Twice** -- at 13:59 (Fallout4.exe pid 44844) and again at 14:57 (pid 26328),
both times between `lodgen_defaults` and `lodgen_native` -- **bungo started
Fallout 4**. The runner checks `tasklist` before EVERY gate, not only at the
start, so it stopped itself both times, mid-side, with nothing of this lane
running:

```
lod_generation      rc=0  ok=129  14s    PASS
lodgen_defaults     rc=0  ok=28   859s   RESULT PASS
STOPPED: Fallout4.exe came up before lodgen_native
```

No bake of this lane was running when the game came up either time, and none was
started after; the rule in the brief is absolute -- game up, no build and no exe
launch.

**The second halt changed how the side is run, and that is worth saying rather
than hiding.** Both halts landed in the same place because the side was being
re-run from the top, and `lodgen_defaults` alone costs 14-17 minutes: twice
spent, twice for a result already known (28 checks, 0 failures, both times). So
the runner now takes a gate list and APPENDS, and the seven that had never run
on the new exe were queued CHEAPEST FIRST by the "before" side's own timings --
8 s, 75 s, 180 s, 169 s, 167 s, 269 s, 872 s as they actually came out -- so
that the next interruption would cost the least. They then ran without
interruption, 15:08 to 15:40.

**Each gate was run whole, one at a time, on one exe** -- the 12:45:27 link,
whose size the runner prints in its own header on every line of `s7_after.txt`.
What was assembled across sittings is the SIDE, not any gate. The two gates that
ran twice (`lod_generation`, `lodgen_defaults`) gave the same counts both times,
which is the only evidence available that stitching a side did not change one,
and it is weaker than a single pass: it is stated, not claimed away.

---
## 9. The build

| | |
|---|---|
| exe | `E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe` |
| mtime | **2026-09-17T12:45:27** (read with `ls --time-style=full-iso`, in the same turn) |
| size | **22 567 424 bytes** |
| sha1 | **a843fca68c18c2740efddcb20e9fe715b7732a22** |
| the binary before it | 22 566 912 bytes, sha1 `14736e9dc96bd99c9ed1f1042ae08625d035ec56`, kept at `scratchpad/perf1_20260917/NifSkope.final_backup.exe`. The 12:45 link differs from it in the two `--help` blocks ONLY (the `--chunk-threads` memory correction, MISTAKES.md entry 7); every gate result in sections 7 and 8 was re-run on the 12:45 binary so that no number in this report describes a binary that is not the one on disk |
| rung | **`release/NifSkope.before_perf1.exe`** -- cut ONCE, before this lane's first build; 22 534 144 bytes. Never deleted, and neither was any other `release/NifSkope.before_*.exe` nor `release/NifSkope.archlock1_rung.exe` |
| built with | MSYS2 UCRT64, `export PATH=/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`, `mingw32-make -f Makefile.Release -j8` |
| game check | `tasklist \| grep -i -E "Fallout4\|NifSkope"` run as its OWN command before every build and every exe launch in this lane; `rc=1` (nothing up) every time |

Sources changed by this lane:

| file | what |
|---|---|
| `src/nativeemit.cpp` | the two fan-outs and their in-order retire; the stage timers; the reuse decision chain and the `native-library-build:` line |
| `src/nativeemit.h` | `lodgenNativeLibrarySplit()`, `NativeReuseOffer`, `lodgenNativeOfferLibraryReuse()` |
| `src/lodofile.cpp` | `lodoStageMesh` / `lodoMergeStagedMesh`; **the `loadOrderHash` restore defect in `lodoRead`** |
| `src/lodofile.h` | the two staging entry points and their contract |
| `src/lodgenparallel.h` | `lodgenParallelFor`'s optional `cap` |
| `src/lodgen.cpp` | `lodgenNativeLoadModel`'s model cache made `thread_local` |
| `src/lodbfile.cpp` | `native-library-build:` registered in `g_censusKeywords[]` |
| `src/nifcli.cpp` | the reuse offer armed under `--incremental`; three `lodgen --help` blocks |
| `tests/spells/lodgen_perf.sh` | NEW -- this lane's gate |
| `tests/spells/lodgen_treecmp.py` | NEW -- the whole-tree comparer the gate uses |
| `tests/spells/lodgen_bakerec_gate.py` | `native-library-build:` registered in the gate's copy of the keyword list |
| `docs/…` | section 10 |
| `MISTAKES.md` | seven entries, all mine, written when each was recognised |

Not touched, by rule: `WW_CHANGES.md`, `HANDOFF.md`, and the git index --
nothing was committed and nothing was stashed.

---

## 10. Documentation touched

One line each, with where the claim comes from.

| file | what was added | provenance |
|---|---|---|
| `docs/LODGEN_NATIVE_LODO_LODI.md` | new **§8.2**, "Keeping the library instead of rebuilding it": the four hashes that must be unmoved, the two structural refusals (occluders, the mesh corpus), and the `lodoRead` `loadOrderHash` defect with its refuter | measured in this lane, report §5.1-§5.4 |
| `docs/LODGEN_BAKE_RECORD.md` | **§7** continued, "What PERF1 did with that last bullet" -- the record needed no format change, it now says which happened, and what its hashes still cannot answer | INCR1's own §7 bullet; measured in report §5.3 |
| `docs/LODGEN_LEDGER_FORMAT.md` | **§5** gains the `native-library-build:` paragraph: both spellings of the line, the eight refusal reasons in the order they are tested, and that only `--incremental` is offered the choice | the emitter, `src/nativeemit.cpp`; the strings are quoted from the code |
| `docs/LODGEN_CENSUS.md` | one new row in the census-line table for `native-library-build:` | the keyword registry, `src/lodbfile.cpp` |
| `lodgen --help`, `--incremental` | the library-reuse sentence and that occluders ON always rebuilds | report §5.3 |
| `lodgen --help`, `--threads` | now names the object library's two fan-outs (model load, cap 4; mesh ladder, uncapped) and carries **82.0 s -> 49.3 s** on a 9-chunk FO4CS region and **91.5 s -> 59.4 s** on a 16-chunk one | report §2.2 and §13, measured on this machine, on the exe that ships |
| `lodgen --help`, `--chunk-threads` | the stale "2.3x slower, 25 GB against 3.8" is replaced by this lane's own measurement, on both regions: **2.29 -> 9.32 GB for 49.3 s -> 41.6 s** (9 chunks) and **2.55 -> 10.48 GB for 59.4 s -> 46.9 s** (16 chunks), 20 of 20 consecutive runs a region, 0 faults, every file byte-identical (57 and 99); the older 16-thread/25-chunk 25 GB reading is kept as the shape of the ceiling | report §4.1-§4.3; the superseded text was measured before the library was parallel and would now mislead. **A first version of this block said 5.80 GB, which is RESUME3's number, not mine** -- corrected before the lane stood, MISTAKES.md entry 7 |

---
## 11. Changelog text for the director to splice

This lane never edits `WW_CHANGES.md` or `HANDOFF.md`. The text below is
offered for splicing, and every number in it is measured in this report.

### For `WW_CHANGES.md` -- one paragraph

> **LOD generation -- the FO4CS bake uses the machine (lane PERF1,
> 2026-09-17).** The object library that dominates an FO4CS bake now builds in
> parallel: the model load and the mesh ladder each fan out through
> `lodgenParallelFor` and retire strictly in job order, so the `.lodo` and the
> `.lodi` come out byte for byte what the serial pass produced. Measured at
> the shipped defaults, on the exe that ships: a 9-chunk Sanctuary region
> **82.0 s -> 49.3 s (-40 %)** and a 16-chunk Boston one **91.5 s -> 59.4 s
> (-35 %)**, at unmoved peak working set (2.29 and 2.55 GB, both sides);
> `--threads 1` reproduces the old exe to a tenth of a second in every stage. Under `--incremental` the
> library can now also be KEPT rather than rebuilt, when the base census, the
> load order, the plugin corpus and the object corpus are all unmoved and the
> previous `.lodo` reads back whole -- **39.3 s -> 2.5 s** on the null arm, with
> the pair still byte-identical to a full bake. The record says which happened,
> every run, in the new `native-library-build:` census line, and the
> `stage times:` line now carries the library's own seven-stage split so the
> next lane does not have to instrument anything. `--chunk-threads` stays at 1:
> eight chunk threads are clean (40 of 40 runs across two regions, 0 faults,
> every one of 57 and 99 files identical) and worth a further 16-21 %, but cost
> **9.32 GB of peak working set against 2.29, and 10.48 against 2.55** -- about
> four times the memory for about a fifth of the clock -- and that is bungo's
> call, not a lane's. New gate `tests/spells/lodgen_perf.sh`; one real
> defect fixed on the way (`lodoRead` never restored `loadOrderHash`, which
> nothing had noticed because nobody had ever read a library back and then
> written from it).

### For `HANDOFF.md` -- a LANDED block

> **HANDOFF LANDED -- lane PERF1, 2026-09-17, the parallel object pass and the
> kept library.**
>
> * **Shipped.** Both fan-outs inside `lodgenNativeWrite` (model load, capped at
>   4 workers; mesh ladder, uncapped), retired in job order. Library reuse under
>   `--incremental` behind a chain of eight refusals. The seven-stage library
>   split appended to the `stage times:` census line. The new census keyword
>   `native-library-build:`, registered in all three lists.
> * **Measured.** At the shipped defaults, on the shipped exe: region A
>   (9 chunks) 82.0 s -> 49.3 s, region B (16 chunks) 91.5 s -> 59.4 s; the mesh
>   stage inside region A 69.5 s -> 36.8 s; peak working set unmoved (2.29 and
>   2.55 GB on both sides). Incremental null arm 39.3 s -> 2.5 s.
> * **Proven.** `tests/spells/lodgen_perf.sh`, five legs, no wall-clock floor:
>   identity on two regions (57 and 99 files, every byte), non-vacuity from the
>   MEASURED worker counts, the way back against the rung exe, reuse on two
>   arms with the census asserted, and the split present-and-masked. Three
>   refuters run and red as required.
> * **Not done, and asked for instead.** Four divergence rows in the lane
>   report's section 12: the `--chunk-threads` default; whether the model
>   fan-out's ceiling of 4 should be a switch; a `modelCorpusStamp` for the one
>   input the reuse chain cannot see (a `.nif` edited on disk with no plugin
>   change); and the new census keyword itself.
> * **Where.** Report `scratchpad/perf1_20260917/lane_perf1_report.md`; rung
>   `release/NifSkope.before_perf1.exe`; pictures in
>   `scratchpad/perf1_20260917/images/`.

---
## 12. Divergence rows for bungo

Written before the lane stands, not after. **Nothing in this section is done.**
Each row is a thing this lane measured and then did NOT do, because doing it
would move a default, add a switch or add a field, and those are his.

### Row A -- `--chunk-threads` stays 1, and here is what 8 is worth

| | region A (9 chunks) | region B (16 chunks) |
|---|---|---|
| shipped default (`--chunk-threads 1 --threads 0`) | 49.3 s | 59.4 s |
| `--chunk-threads 8 --threads 0`, median of 20 | **41.6 s (-16 %)** | **46.9 s (-21 %)** |
| **peak working set, 1 -> 8** | **2.29 GB -> 9.32 GB** | **2.55 GB -> 10.48 GB** |
| faults in 20 consecutive runs | **0** | **0** |
| files byte-identical to the 1-thread bake, every run | **57 of 57** | **99 of 99** |

The default is NOT moved. The fan-out is correct and it is clean; what it costs
is memory -- **about four times it, for about a fifth of the wall clock** -- and
the machine it would cost it on is his (31 GB, and a full Commonwealth bake is
far more than 16 chunks). An earlier lane measured 25 GB at 16 threads on 25
chunks, which is the shape of the ceiling.

**The question:** do you want `--chunk-threads` to default to a small number
above 1 -- 2 or 4 -- on the ruled FO4CS pipeline? Note that the numbers above
are the ends of the range, not the offer: 2 or 4 threads would buy less than
16-21 % and cost less than 4x the memory, and this lane did not measure the
middle of that curve because measuring it would invite moving the default. A
yes is one line in `src/nifcli.cpp`, one line of `--help`, and a soak at
whatever number you name.

### Row B -- the model fan-out's ceiling of 4 is a constant, not a switch

Section 2.2 measured the model stage at 24.2 s (1 worker), 18.8 (2), 19.1 (4),
24.0 (6), 27.1 (8), 33.9 (16) -- it stops scaling at two and is SLOWER than the
serial loop past four. The ladder stage over the same bakes went 36.3, 19.7,
12.6, 10.4, 9.2, 8.3 and wants every core. So the model fan-out is capped at 4
and the ladder fan-out takes `--threads`.

That 4 is measured on ONE machine: yours, 16 logical cores, 31 GB, and the curve
is a heap-contention curve, so a machine with a different allocator or a
different core count could sit somewhere else.

**The question:** should the cap become a switch (`--native-model-threads N`,
0 = the machine) so a different machine can find its own number, or stay a
constant with the measurement written beside it? This lane kept the constant,
because a switch is a default and defaults are yours.

### Row C -- the kept library cannot see a `.nif` edited on disk

Section 5's chain refuses to reuse the library on four inputs: the base census,
the load order, the plugin corpus and the object census. All four come out of
the PLUGINS. A mesh edited on disk with no plugin change moves none of them, and
the reused run opens no model at all (`models 0 loaded` in its own census), so
it cannot notice either. The result would be a `.lodo` built from yesterday's
`.nif`.

Today this is fenced rather than solved: reuse only fires under `--incremental`,
occluders ON always rebuilds (so the ruled pipeline rebuilds), and the census
line says `reused` in as many words so it is never silent.

**The question:** may this lane's successor add a `modelCorpusStamp` to the bake
record -- path, byte size and mtime over exactly the model list the object
census already walks -- and refuse reuse when it moves? It is a NEW FIELD in the
record (a sixth volatile thing it is not; a stamp is deterministic), so it needs
your word. The honest alternative, hashing every model's bytes, costs the stage
it is trying to skip.

### Row D -- one new census keyword, `native-library-build:`

The record gained one line: `native-library-build: reused (...)` or
`rebuilt (<the one test that refused>)`. No format bump, no new file, no new
FIELD -- it is a census line, which the record already carries by keyword, and
it is registered in all three lists the contract keeps (`src/lodbfile.cpp`,
`tests/spells/lodgen_bakerec_gate.py`, `docs/LODGEN_CENSUS.md`).

It is named here anyway because it is an addition to what every `--native` bake
writes, and because it began life hung off the existing `native-library:`
keyword, which already carries the level-0 sentence -- that would have made one
keyword mean two things and is written up in MISTAKES.md.

**The question:** none, unless you would rather the line were not there at all.

---
## 13. The pictures

All three are in `scratchpad/perf1_20260917/images/`, drawn by
`scratchpad/perf1_20260917/s8_pics.py` from files on disk. No matplotlib is
installed on this machine; the charts are drawn with PIL, and every bar carries
its number so the picture cannot say anything the tables do not.

| picture | what it is | drawn from |
|---|---|---|
| `stage_times_region_A.png` | the four `stage times:` stages and the wall clock, before and after, region A (9 chunks) | `s8_bars.txt`, two bakes at the SHIPPED defaults, one per exe |
| `stage_times_region_B.png` | the same, region B (16 chunks) | `s8_bars.txt` |
| `library_cells_1_vs_8.png` | the same library cells from the 1-thread bake and the 8-thread bake, and their measured pixel difference | the two `.lodi`/`.lodo` pairs gate leg (a) had just proven byte-identical |

**What the cell picture is, exactly.** It is not a render of the game and it is
not a screenshot. It is the `.lodi` INSTANCE TABLE drawn as it is stored: the
sixteen cells with the most instances, one tile each, every instance a disc at
its own world `x,y` (normalised inside the tile by that cell's own extent --
the stored `px,py` are quantised over the CHUNK, so a cell's instances would
huddle in a quarter of the tile), with the radius from the base's `boundRadius`
times the instance's scale and the colour from `drawKey`. `drawKey` is the
field the two fan-outs could most plausibly have reordered, which is why it is
the colour and not a decoration.

The third panel is `numpy.abs(A - B)` over the two rendered panels, and the
caption prints the percentage of pixels that differ. **It reads 0.000 %.** If it
ever reads anything else, the lane is not done -- and it would be saying the
same thing gate leg (a) says with 57 and 99 SHA-256 digests, in a form a person
can look at.
