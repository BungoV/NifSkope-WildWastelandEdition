# Lane BAKEPERF1 — the generator uses the whole machine

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, nothing committed.
Exe at launch `release/NifSkope.exe` **2026-09-11 13:24:12, 21,234,176 B** (LODUI1),
md5 `edd2a99ea9fcbeb21b18db05c8b0dbb3`. Rollback rung taken at 13:5x:
`release/NifSkope.before_bakeperf1.exe`, **same md5**, so the rung is the launch exe
byte for byte (gate P5's third clause, discharged before any edit).

Machine: 16 logical cores, 31 GB, RTX 5070 Ti.

Concurrent lane CENSUS1 owns `docs/LODGEN_CENSUS.md`,
`tests/spells/lodgen_census_check.py` and one pointer paragraph in
`docs/LODGEN_NATIVE_LODO_LODI.md`. This lane touched none of them.

---

## 0. Pre-registered gates

Registered **before** any source was edited. Written 2026-09-11 13:5x.

| id | gate | what makes it red |
|---|---|---|
| **P1** | the shared-state inventory (section 1) is written before any code, timestamped | a cache, static or global touched during a bake that section 1 does not name |
| **P2** | WHOLE-REGION BYTE IDENTITY, `--threads 1` vs `--threads N`, **every output file**, on BOTH test regions | one file differing by one byte; one file present on one side and absent on the other |
| **P3** | the four stage times before/after on both regions, as a table with the speed-up per stage; peak working set printed | a stage that got slower; a peak that extrapolates past 31 GB on the Commonwealth |
| **P4** | `--threads 1` on the NEW exe is byte-identical to the RUNG exe's run, every file, both regions | any difference — it means the rewrite changed arithmetic |
| **P5** | exe newer than every changed source; the six objects that include `lodgen.h` newer than their sources; `res/style.qss` == `release/style.qss`; rung == launch bytes | a stale object, a stale exe, a moved rung |
| **P6** | no NifSkope left running at the end; `Fallout4.exe` down at every build and every launch | either process alive |

Floors, so a gate cannot pass vacuously:

* **P2/P4 floor.** The comparison set must be **non-empty and named**: the run must
  produce at least the `.BTR`, `.BTO` and `.BTO.manifest.txt` for every populated
  chunk of the region, plus the terrain texture sheets, plus (native on) the
  `.lodo`/`.lodi` pair. A comparison over zero files passes trivially and is a
  red, not a pass. The file COUNT is printed beside the verdict.
* **P2 refuter.** Before believing the comparator, it is run against a
  **deliberately corrupted** copy of one output file and must report that file as
  differing. A comparator that cannot go red is not a gate.
* **P3 floor.** A stage time of exactly 0.0 s on both sides is reported as
  "did not run", never as a speed-up.

Ways back, exact at their off value:

* `--threads 1` on the CLI (this lane's; a PANEL ROW is **not** this lane's to add —
  it belongs to LODUI1's successor or a follow-up, and is listed as owed in §4).
* The rung exe `release/NifSkope.before_bakeperf1.exe`.

---

## 1. Shared state inventory

**Written 2026-09-11 13:5x, before one line of source was changed.** Every cache,
static, global and Qt object a bake touches, and how it is written.

### 1.1 The plugin reader — `EsmWorld` / `ESMFile`

| what | where | written how | verdict |
|---|---|---|---|
| `lodBaseCache`, `scolCache`, `ltexCache`, `ltexCoverCache`, `grasCache` | `src/esmdata.h:309-313`, all `mutable QHash` | **lazily, per lookup, during the chunk build**; the accessors hand out `const X &` **into the hash** (`lodBase()`, `scolParts()`, `ltexTextureSet()`, `ltexCover()`) | **RACE.** Two threads inserting rehash the hash and invalidate a reference another thread is still reading. A mutex around the insert does **not** fix it, because the reference outlives the lock. |
| `census` / `censusBuilt` | `esmdata.h:314-315` | once, lazily | race on first touch only |
| `persistentCache` / `persistentCacheBuilt` | `esmdata.h:319-320` | once, lazily, returns `const QVector &` | same reference-invalidation race |
| `tintFn` / `tintUser` | `esmdata.h:316-317` | set once by the caller | read-only after warm-up |
| `grasReads` | `esmdata.h:318` | **incremented per GRAS decode** | a census counter; racy increment would under-count |
| `ESMFile::zlibBuf`, `zlibBufRecord` | `lib/libfo76utils/src/esmfile.hpp:96,99` | **`ESMField(ESMFile&, …)` decompresses a compressed record into a SHARED scratch buffer and rewrites `r.fileData` to point at it** | **HARD RACE.** Two threads reading two compressed records at once corrupt each other's field data. This is the memo the lodgen skill already warns about ("fetch neighbours FIRST, the cell LAST"). |
| `ESMFile::recordBuf`, `formIDMap`, `pluginMap`, `esmFiles` | same header | built once by the constructor | read-only after `load()` |

**Conclusion: `EsmWorld` cannot be shared across workers at all.** Not with a
mutex — the API hands out interior references. The only byte-safe answer is
**one `EsmWorld` per worker**, each loaded from the same plugin string. The
parse is deterministic, so every worker's caches hold the same values; nothing
about the arithmetic changes.

### 1.2 The bake caches — `LodgenBakeCaches`

`src/lodgen.cpp:5182-5197`, created per run by `lodgenCreateBakeCaches()` and
passed down into `lodgenBakeTerrainTextures` and `lodgenBakeTerrainVt`.

| member | written how | verdict |
|---|---|---|
| `texCache` (`QHash<QString, DDSTexture16 *>`) | inserted on every texture miss | RACE; `lodgenCachedTexture()` returns a pointer INTO it |
| `texOrder`, `texBytes` (the LRU) | **rewritten on every hit and every miss**, and the evictor `delete`s textures | RACE, and a *use-after-free* race: one worker can evict a texture another is decoding from |
| `grassTint` | inserted per MODL on miss (`lodgenGrassTintResolve`) | RACE |
| `nifReads`, `texLoads` | incremented per load | census counters, racy increment |

**Conclusion: per-worker caches.** A mutex would serialise the decode, which is
most of what the texture stage costs, and the LRU's `delete` makes locking the
insert alone unsound. The texture budget is divided by the worker count so the
total stays where it was (see §2).

### 1.3 The model cache

* `lodgenBuildObjectChunk`'s `modelCache` (`src/lodgen.cpp:3411`) is a **local**,
  one per chunk — already unshared, so chunk fan-out cannot race it. (It is also
  why the same LOD model is re-read once per chunk; noted in §4 as a separate,
  byte-identical win this lane did not take.)
* `lodgenBakeTerrainTextures`'s `modelCache` (`src/lodgen.cpp:6118`) — same, local.
* **`lodgenNativeLoadModel`'s `static QHash<QString, QVector<LodSrcShape>> cache`
  (`src/lodgen.cpp:2213`) is a function-local static shared by the whole process**
  and returns a `const QVector &` into itself. RACE, same shape as §1.1.

### 1.4 The resource stack and the archive indices

| what | where | written how | verdict |
|---|---|---|---|
| `lodgenStack()` — `static QStringList` | `lodgen.cpp:1478` | set once by `lodgenSetResources()` before a run | read-only during a bake |
| `lodgenStackIndex()` — `static unique_ptr<BA2File>` + `builtFor` + `tried` | `lodgen.cpp:1553-1555` | **built on first use**, rebuilt when the stack changes | RACE on first use only; **warmed on the main thread before fan-out** |
| `lodgenMeshArchives()` — `static unique_ptr<BA2File>` + `tried` | `lodgen.cpp:1577-1578` | built on first use | same; warmed before fan-out |
| `BA2File::extractFile` / `findFile` | `lib/libfo76utils` | reads from the index built above; the output buffer is the caller's | read-only after the index exists, **provided the index is already built** |
| `lodgenTerrainRingSelfTestOnce()`'s `static bool done` | `lodgen.cpp:5660` | once | warmed before fan-out |

### 1.5 The FO4CS-native emitter

`src/nativeemit.cpp:42-80`. `State & st()` is a **process-wide singleton**:
`byKey`, `arrivals` (a `std::vector`, grown by `push_back`), `byObject`,
`arrivalsSeen`.

`lodgenNativeAddPlacement()` assigns each new `(ref, part)` **the next index in
`arrivals`** — so the accumulator's content depends on **arrival order**, which
under a fan-out is completion order. This is exactly the ordering leak the brief
names. Verdict: **RACE and ORDER-SENSITIVE**. Handled by journalling per worker
and replaying into the singleton in deterministic chunk order (§2.4).

### 1.6 Qt objects

| what | verdict |
|---|---|
| `NifModel` ctor/dtor → `Game::GameManager::getNIFResources()` / `removeNIFResourcePath()` and the process-wide `nifResourceMap` with its `refCnt` (`src/gamemanager.cpp:531-565`) | **RACE.** Every `NifModel` built or destroyed inserts into / erases from one `std::map`. Serialised under a dedicated mutex (construction is microseconds against a multi-second chunk). |
| `NifModel::updateSettings()` → `QSettings` per construction | `QSettings` is reentrant, but is under the same mutex anyway as a consequence |
| The panel's `preview()` splice into the workspace, `progress`, `map->markChunk` | main thread only, always; results retire to the main thread in queue order |
| `QFile` writes (`nif.saveToFile`, the manifest, the DDS writers) | per-call, no shared handle — safe off the main thread |

### 1.7 Order-sensitive outputs (must come from the queue, never from completion)

* `writtenBto` — the list the atlas, the texture arrays, the shape merge, the
  far-ring cut and the card arrays all consume **in list order**.
* the `.BTO.manifest.txt` index column (`R + G*256` per chunk) — per chunk, safe.
* the native `.lodo` table sort laws and the `.lodi` sort law — via §1.5.
* texture-array layer assignment and card-array packing — both run **after** the
  chunk queue, from `writtenBto`.
* the `done` counter printed as `[n] <name>` — cosmetic, but it is in the log a
  gate reads, so it is emitted in queue order too.

### 1.8 Things that are already safe

* The BC1/BC3/BC4 block loops (`lodgen.cpp:4296`, `4429`, `2665`, `7205`):
  each block reads `mip` read-only and writes a disjoint slice of `block`.
  **Embarrassingly parallel with no shared state at all.**
* `lodgenEncodeBC1Block`, `lodgenEncodeBC4Block`: pure functions.
* `lodgenTriangulateGrid`, `waterDesiredLevel`, `lodgenTerrainMsnPixel`, the
  `static const` direction tables: pure / read-only.

---

## 2. What went parallel, and how the order is kept

### 2.1 The one number, and the one way back

`src/lodgenparallel.{h,cpp}` (new). `lodgenThreadCount()` is the generator's
whole thread budget; `lodgenSetThreadCount(n)` sets it and the CLI's new
`--threads N` is the only caller that matters. **0 or absent = the machine
(`QThread::idealThreadCount()`, 16 here). 1 is the exact way back**: every
fan-out in this lane degenerates to the loop that was there before, on the
calling thread, in ascending order.

`lodgenParallelFor(n, f)` is the fan-out. It runs serial when the count is 1,
when n is 1, **or when the caller is already inside a generator worker** — a
chunk fan-out has the cores already and a nested one would only oversubscribe.

### 2.2 Chunks — `src/lodgenchunkpass.{h,cpp}` (new)

The panel (`lodgenmanager.cpp`, one chunk per event-loop tick) and the command
line (`nifcli.cpp`, `for (cy) for (cx)`) had the same loop written twice. It is
one function now, shared, and fanned over the workers.

Per worker, private for the length of the pass: **its own `EsmWorld`** and **its
own `LodgenBakeCaches`**. Section 1 says why neither can be shared even under a
mutex. A worker's `EsmWorld` costs about **0.46 s to load and ~336 MB measured**
(one `--list-impostor-candidates` run, which is load plus one chunk's walk), and
the texture budget is divided by the worker count so the total stays at 512 MB.

How order is kept:

| what | mechanism |
|---|---|
| results | workers write into `results[jobIndex]`; the driver **retires in job order** on its own thread and never from a completion callback |
| `writtenBto` (the atlas, texture arrays, merge, far-ring cut, card arrays all consume it in list order) | appended in `retire`, so job order |
| the `[n] <name>` lines | printed in `retire` |
| the panel's live preview and progress bar | spliced in `retire`; the window stays live because `retire` pumps the event loop, the way the pyramid pass already did |
| the FO4CS-native accumulator | see 2.3 |
| Cancel | workers poll the flag **before picking a job up**, so it still lands between chunks |
| `NifModel` ctor/dtor vs `GameManager::nifResourceMap` | one recursive mutex in `gamemanager.cpp`, three call sites, none hot |
| the lazy singletons (`lodgenStackIndex`, `lodgenMeshArchives`, `GameResources::init_archives`, the ring self-test) | `lodgenWarmSharedIndices()` builds all four on the calling thread **before** the fan-out |
| each job's ring | a job over "all rings" mixes 4/8/16/32, and the builders read `dim` off their own options — so every job takes its own copy of the option blocks |

No `NifModel` ever crosses a thread. A worker serialises its document through a
`QBuffer` (what `BaseModel::saveToFile` does before it opens a file) and hands
the **bytes** on; the panel's preview copy — the `.BTR` with the chunk's world
translation on its root — is written as a file by the worker, and the main
thread only opens it.

### 2.3 The ordering leak that had to be closed first

`lodgenNativeAddPlacement` gives each new `(ref, part)` **the next index in
`arrivals`**. Under a fan-out that index would depend on which worker finished
first, and the `.lodo`/`.lodi` pair would stop being a function of the input.

So a worker never speaks to the accumulator. It opens a **journal** on its own
thread (`lodgenNativeJournalBegin`, `src/nativeemit.cpp`), one per job; every
`AddPlacement` and `Lighting` call on that thread is recorded instead of
applied, with both kinds in **one sequence** — the order between them is exactly
what has to survive. The driver replays each job's journal at the moment that
job retires, so the accumulator receives the same calls, with the same
arguments, in the same sequence a serial run would have made. Byte identity of
the pair is therefore by construction, not by hope. At one thread no journal is
opened at all.

### 2.4 BC encoding, per block row

Four block loops, all the same shape — each block reads the mip read-only and
writes a disjoint slice of the output buffer, so there is no shared state at
all:

* `lodgenWriteDdsBC5` (the BC4 pair, `src/lodgen.cpp`)
* `lodgenWriteDds` (BC1/BC3, the file writer)
* `lodgenEncodeArrayLayer` (BC1/BC3, the texture-array layers)
* `lodgenVtEncodeBlocks` (the terrain pyramid's tiles)

The fourth is why the pyramid gets some parallelism even though its tile loop
stayed serial (2.6).

### 2.5 Writes off the critical path

`LodgenWriter` (in `lodgenparallel`): one writer thread, a **bounded** queue
(256 MB) so a 3,060-chunk bake cannot hold every chunk it has built and not yet
written, and `finish()` **drains** before the pass returns — a run that exits 0
with a file still unwritten is worse than a slow run, so nothing returns before
the drain and the failures list is reported by name. The writer uses `QSaveFile`
with the direct-write fallback, exactly as `BaseModel::saveToFile` does, so a
file that went through the queue lands the same way as one written inline.
At one thread there is no writer and the serial path writes inline.

### 2.6 What stayed serial, and why

* **The terrain-pyramid tile loop** (`lodgenBakeTerrainVt`). Same two blockers
  as the chunks — one `EsmWorld`, one `LodgenBakeCaches` — but the pass is a
  single call that owns its own staging, and giving it per-worker worlds is a
  second lane's worth of change. Its BC encoding does fan out (2.4).
* **The post-queue passes** — the object atlas, the texture arrays, the shape
  merge, the far-ring cut, the card arrays, and the `.lodo`/`.lodi` write. All
  of them read `writtenBto` **in list order** and several of them assign indices
  from that order; they are where the ordering leaks would be, and they are not
  where the time is.
* **`lodgenBuildObjectChunk`'s `modelCache`** is a local, one per chunk, so the
  same LOD model is re-read once per chunk. Hoisting it to the run would be a
  pure, byte-identical win and is **not** this lane's (it is a caching change,
  not a threading one); it is listed as owed in §4.

---

## 3. Build and gates

### 3.1 The build

ONE build at **13:59:25 → 14:02:15**, `QMAKE-RC=0` `BUILD-RC=0`, 131 translation
units (qmake was re-run because two new sources joined the project), 0 `error:`
lines, `release/NifSkope.exe` **14:02:15, 21,259,264 B**.

Consistency (gate **P5**): the exe is newer than all 13 changed files; every
object that includes a changed header is newer than that header —
`gamemanager.h` 9 TUs / 0 stale, `lodgen.h` 6 / 0, `nativeemit.h` 5 / 0,
`lodgenparallel.h` 5 / 0, `lodgenchunkpass.h` 3 / 0; `res/style.qss` and
`release/style.qss` compare equal; the rung `release/NifSkope.before_bakeperf1.exe`
still has the launch exe's md5. **P5 green.**

Then **counted relink 1** at 14:5x, for the two race fixes of §3.3. No other
link.

### 3.2 The comparator, shown red before it was believed

`scratchpad/bakeperf1_20260911/bake_diff.py` hashes every file under both trees
(the run's own log excluded), compares by relative path AND content, and prints
the file COUNT beside the verdict so a comparison over nothing reads as the red
it is.

| run | result |
|---|---|
| an untouched copy of the 9-chunk output (the control) | `RESULT PASS — 60 file(s), 24,975,886 bytes` |
| **one byte flipped** at offset 4096 of `Commonwealth.4.-12.24.BTO` | `RESULT FAIL — DIFFER Commonwealth.4.-12.24.BTO` |
| the same, plus **one output file deleted** | `RESULT FAIL — 1 only-in-A, 1 differ` |

So the gate can go red on a single byte and on a missing file. **P2's refuter is
discharged.**

### 3.3 THE FIRST MULTI-THREAD RUN CRASHED, AND WHAT IT WAS

`--threads 1` ran clean; `--threads 16` died after 2–4 seconds with 0–3 files
written. **Four runs out of four**, exit code `0xC0000374` —
`STATUS_HEAP_CORRUPTION`, three times, and `0xC0000005` once. Under `gdb` it did
not crash at all, twice, which is what a first-touch race looks like.

The cause was **not** in this lane's new code. `setupArrayPseudonyms()`
(`src/model/nifmodel.cpp:67`) fills three process-wide `QHash`es the first time
any `NifModel` is constructed, guarded only by
`if ( !arrayPseudonyms.isEmpty() ) return;`. In the application that first
construction has always been on the main thread. In a `-no-gui lodgen
--terrain-region` run **the first `NifModel` of the process is one a worker
builds** — and sixteen workers reached the empty hash within milliseconds of
each other and all sixteen filled it. Section 1 named `NifModel`'s resource-map
entry and missed this one: it is recorded in §5.

Two fixes, both in the relink:

1. a mutex at the top of `setupArrayPseudonyms()`, so the guard means what it
   says (one lock per `NifModel` construction, microseconds against a chunk);
2. `lodgenWarmSharedIndices()` now also builds and throws away **one
   `NifModel`** on the calling thread, so every "first time only" path inside a
   `NifModel` constructor — the pseudonym tables, the `QSettings` read, the
   resource-map entry — happens where nothing races.

**A separate, pre-existing defect found while auditing that file and NOT fixed
here** (it is a UI path, outside this lane): `src/model/nifmodel.cpp:1421`
binds `QHash<QString,QString> & pseudonymMap = arrayPseudonyms;` and then
assigns `pseudonymMap = multiArrayPseudonyms1;` — which copy-assigns into the
global rather than re-pointing, so the first multi-array row ever displayed
permanently replaces the application's singular-name table. Written up in §4.

### 3.4 THE CRASH SIGNATURE, and the dialogs that reached bungo's desktop

**Written at the director's instruction, 2026-09-11 14:2x, before the cause was
settled.** Six Windows "Application Error" dialogs appeared on his desktop in
three minutes while this lane was bisecting. That is a regression on its own
terms — a headless run has no business showing a window, and a modal error box
obeys neither the second-monitor rule nor never-SetForegroundWindow.

**What was done about it, immediately:** every launcher script in
`scratchpad/bakeperf1_20260911/` now dot-sources `no_crash_dialog.ps1`, which
calls `SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOOPENFILEERRORBOX)` in the launching shell —
Windows gives a child the error mode of the process that created it. That is
the stop-gap. **The real fix is in the exe**, at relink 3: `initModelLayer()`
(`src/nifcli.cpp`), the first thing the `-no-gui` path runs, sets the same
error mode for itself, so a headless crash never shows a dialog no matter who
launched it. A crash still fails the run and still sets the exit code; only the
window goes away.

**The signature:**

| | |
|---|---|
| exit code | `0xC0000374` `STATUS_HEAP_CORRUPTION` (once `0xC0000005`, before the first two fixes) |
| Event Log | id 1000, faulting module **ntdll.dll**, offset **0xff509**, the same offset on every run |
| stage | the **terrain texture bake** — all nine `.BTR` written, 1–2 texture sheets written, then the fault |
| thread count | **appears at 9 workers, absent at 1, 2 and 4** — three runs each |
| reproducibility | 5 runs out of 5 at 9 workers; **never once under `gdb`**, twice, which is what a timing-dependent fault looks like |
| `--no-roads` | **clean 3 of 3 at 9 workers** — dropping the road pass removes it |
| no `--tex-dir` at all | clean 3 of 3 at 9 workers |
| `--native` off | 2 of 3 still crashed — not the native journal |
| `--no-ao` | 3 of 3 crashed — not the AO bake |

**The candidate the numbers point at, and why.** Every worker owns its own
`EsmWorld` and its own `LodgenBakeCaches`, so nothing in the texture stage is
shared — except one thing this lane itself introduced: **the texture budget was
divided by the worker count**, `max(64 MB, 512 MB / workers)`. The clean/crash
boundary lands exactly where that division reaches its floor: 2 workers
256 MB clean, 4 workers 128 MB clean, 9 workers **64 MB** crash. A 64 MB cache
holds about three 2048-square landscape diffuses, so it evicts on nearly every
lookup — and eviction is the only code in that stage that `delete`s anything.
With `--no-roads` almost no texture resolves on this region ("262144 px no base
tex"), the cache stays nearly empty, and nothing is ever evicted: which is
exactly the variant that does not crash.

Relink 3 therefore gives **every worker the whole 512 MB budget** and the
numbers are re-taken below. If the fault survives that, the cause is a genuine
race rather than the budget, and the fallback stands: **`--threads 1` is
byte-identical to the rung on both regions (already proven, §3.5), so the
serial bake is unaffected and the parallel path ships opt-in and NOT PROVEN.**
A serial bake that works beats a parallel one that faults.

### 3.5 WHAT THE CRASH ACTUALLY IS, symbolised

The release exe is linked `-Wl,-s`, so `gdb` could only print `?? ()`. One
**diagnostic relink with the symbol table kept** (`make LFLAGS="-Wl,-subsystem,windows
-mthreads"`, no source change, 26,344,231 B, discarded afterwards) and the same
`gdb` run on the 25-chunk region at 16 threads caught it. The faulting thread:

```
#0-#4  NifItem::deleteChildItems()            <- SIGSEGV, four levels deep
#5     BaseModel::~BaseModel()
#6     (anonymous)::lodgenLoadModel(QString const&, QString const&, QHash<...>&)
#7     (anonymous)::LodgenRoadSet::addPlacement(...)
#8     (anonymous)::LodgenRoadSet::gather(...)
#9     lodgenBakeTerrainTextures(...)
#10    (anonymous)::runJob(...)
#11    (anonymous)::ChunkThread::run()
```

and **every other worker was in the same parser at the same moment** —
`BaseModel::getItemInternal`, `NifExpr::partition` under
`NifModel::updateArraySizeImpl`, `Transform::Transform(NifModel const*, ...)`,
`NifModel::get<HalfVector2>`. All nine of them under
`LodgenRoadSet::gather` → `lodgenLoadModel`.

**So the unsafe thing is the NifModel / NifItem / nif.xml layer, not this
lane's code and not the generator's caches.** It is why the road pass is the
trigger: the road gatherer parses a fresh NIF per placed road model, and it
does it far more often than anything else in the texture stage. `NifItem`'s
slab pool is already mutex-protected and its comment says "the XML checker
parses NifModels on worker threads" — so the layer is *believed* safe and is
not. Three hypotheses were tested and refuted on the way, each with numbers:
the array-pseudonym tables (fixed, moved the crash from 0–3 files in to 11),
the nested BC fan-out (fixed, no change), and the divided texture budget
(fixed, no change — 5 runs of 5 still faulted).

### 3.6 WHAT SHIPS, and what does not

Per the director's instruction of 14:2x and CONSTITUTION 7 (always leave a
zero-effort fallback):

* **`--chunk-threads N` is the chunk queue's own number and it DEFAULTS TO 1.**
  A bake therefore runs exactly as it did before this lane unless somebody asks
  for more, and it is byte-identical to the rung — proven, §3.7.
* **`--threads N` keeps its meaning** as the general fan-out budget (the BC
  encoders and anything added later) and still defaults to the machine. That
  fan-out is pure arithmetic over disjoint output and is not implicated.
* `lodgenLoadModel` now takes a mutex for the whole life of its temporary
  document, so NIF parsing is serialised even when chunks are not. Whether that
  makes `--chunk-threads 16` survivable is a measurement, below, not a claim.

### 3.7 A SECOND REGRESSION THE STAGE TABLE CAUGHT

With the chunk queue serial — the shipped default — the new exe was **slower
than the rung**, and only the stage table showed it:

| region | stage | rung exe | new exe, relink 4 |
|---|---|---|---|
| Sanctuary, 9 chunks | textures | **5.2 s** | **8.0 s** (+54%) |
| Sanctuary, 9 chunks | wall | 15.3 s | 18.8 s |
| Boston, 25 chunks | textures | **53.9 s** | **61.0 s** (+13%) |
| Boston, 25 chunks | wall | 101.9 s | 113.1 s |

The cause is this lane's own BC-encoder fan-out. `lodgenParallelFor` created a
fresh `QThread` per call and joined it; the BC writers call it once per MIP of
every sheet, so a 512-square sheet costs seventy-five thread creations, several
sheets a chunk, nine chunks — thousands of `CreateThread`s for block rows that
encode in less time than a thread takes to start. **A fan-out that is not
measured is not a speed-up.**

Relink 5 replaces the per-call threads with a **persistent pool** (expiry
disabled, so the threads survive between calls) and raises the size floor from
8 block rows to 32, so only the big mips fan out at all.

### 3.8 THE GATES, on the relink-4 exe

**P2 and P4, byte identity — GREEN, all four, both regions.** The comparator
was shown red on a flipped byte and on a missing file first (§3.2).

| gate | what | files | bytes | verdict |
|---|---|---|---|---|
| **P4** Sanctuary | rung exe vs new exe, shipped default | 60 | 24,975,886 | **PASS, byte-identical** |
| **P2** Sanctuary | serial chunk queue vs `--chunk-threads 16` | 60 | 24,975,886 | **PASS, byte-identical** |
| **P4** Boston | rung exe vs new exe, shipped default | 163 | 77,591,755 | **PASS, byte-identical** |
| **P2** Boston | serial chunk queue vs `--chunk-threads 16` | 163 | 77,591,755 | **PASS, byte-identical** |

So the rewrite changed no arithmetic, and **the fan-out does not change one
byte of output** — the deterministic retire order and the native journal do
what they were built to do. The reason it is off by default is safety and
speed, not correctness.

**Stability, five runs each, `--chunk-threads 16`, Sanctuary:**

| exe | result |
|---|---|
| relink 1 (pseudonym race fixed) | 5 of 5 `0xC0000374`, 11 files in |
| relink 3 (whole texture budget) | 5 of 5 `0xC0000374`, 9–11 files in |
| **relink 4 (NIF parsing serialised)** | **5 of 5 clean, 60 files, RC=0** |

**Stage times and peak working set** (all on the relink-4 exe; the rung column
is the 13:24:12 exe):

| region | run | landscape | meshes | textures | wall | peak WS |
|---|---|---|---|---|---|---|
| Sanctuary 9 chunks | rung | 0.0 | 8.3 | 5.2 | 15.3 s | 1,524 MB |
| | shipped default | 0.0 | 8.6 | **8.0** | 18.8 s | 1,754 MB |
| | `--chunk-threads 16` | 0.0 | 7.2 | **24.6** | 32.4 s | **5,775 MB** |
| Boston 25 chunks | rung | 0.0 | 44.0 | 53.9 | 101.9 s | 3,612 MB |
| | shipped default | 0.0 | 46.9 | **61.0** | 113.1 s | 3,752 MB |
| | `--chunk-threads 16` | 0.0 | 127.5 | 75.5 | 206.1 s | **17,992 MB** |

Three things to read off it:

1. **The chunk fan-out is a 1.8x SLOWDOWN**, both regions. With NIF parsing
   serialised, the part that costs is the part that cannot overlap, and nine or
   sixteen workers add cold caches and contention on top.
2. **The default got slower than the rung too** — that is this lane's own BC
   fan-out, §3.7, fixed at relink 5 and re-measured in §3.9.
3. **Memory is the other reason the fan-out is off.** 16 workers is
   **18.0 GB** on a 25-chunk region: each worker holds its own plugin reader and
   its own 512 MB texture cache. On 31 GB that is already two thirds of the
   machine before the Commonwealth's own working set, and the extrapolation is
   in §3.10.

### 3.9 THE GATES AGAIN, on the SHIPPING exe (relink 5, 14:48:52, 21,261,312 B)

Everything in §3.8 was re-run on the exe that ships. **Byte identity GREEN,
all four, both regions, same file counts and same byte totals** — 60 files /
24,975,886 B on Sanctuary and 163 files / 77,591,755 B on Boston, the rung
against the shipped default AND the serial queue against `--chunk-threads 16`.

Stage times, shipping exe:

| region | run | meshes | textures | wall | peak WS |
|---|---|---|---|---|---|
| Sanctuary 9 | rung | 8.3 | 5.2 | 15.3 s | 1,524 MB |
| | shipped default | 8.5 | 6.0 | 17.5 s | 1,755 MB |
| | `--chunk-threads 16` | 6.7 | 23.4 | 30.7 s | 5,775 MB |
| Boston 25 | rung | 44.0 | 53.9 | 101.9 s | 3,612 MB |
| | shipped default | 45.0 | 57.7 | 110.2 s | 3,753 MB |
| | `--chunk-threads 16` | 105.6 | 52.0 | 161.1 s | **21,919 MB** |

The persistent pool recovered most of §3.7's regression — Sanctuary textures
8.0 → **6.0 s**, Boston 61.0 → **57.7 s** — and the parallel path improved with
it (Boston 206 → 161 s). It is still 1.5–1.8x slower than one thread, and it
still costs **21.9 GB on 25 chunks**, which is 70 percent of this machine.

**Whether the shipped default is still slightly behind the rung is measured in
§3.9a and not eyeballed**: the rung's own two runs of the SAME region differed
by 4.5 s (15.3 s and 19.8 s), so a single pair of numbers cannot answer it.

### 3.9a IS THE DEFAULT SLOWER THAN THE RUNG? Measured, not eyeballed

The rung's own two runs of the same region differed by **4.5 s** (15.3 s and
19.8 s), so the single pairs in §3.8 and §3.9 could not answer it. Three runs
each, **alternating** rung/new so drift and thermal state fall on both sides
equally, `scratchpad/bakeperf1_20260911/repeat_timing.ps1`, Sanctuary 9 chunks,
shipped default on the new side:

| exe | wall, three runs | wall MEDIAN | textures, three runs | textures MEDIAN |
|---|---|---|---|---|
| rung (13:24:12) | 17,692 / 16,040 / 15,417 ms | **16,040 ms** | 6.1 / 5.6 / 5.3 s | **5.6 s** |
| shipping (14:48:52) | 17,227 / 16,130 / 16,074 ms | **16,130 ms** | 6.2 / 5.4 / 5.4 s | **5.4 s** |

**The answer is no.** The medians differ by **90 ms of 16 s — 0.6 percent —
against a within-exe spread of 2.3 s**, and the texture stage's median is
actually 0.2 s lower on the new exe. The shipped default is the rung's speed,
and the 8.0 s / 61.0 s figures of §3.7 were a real regression of the per-call
threads that relink 5 removed, not a property of the rewrite.

**So the honest summary of the performance work is: no regression, and no
speed-up.** The one number in the table that is not noise is the parallel
path's, and it is negative.

### 3.9b Gate (e) in its literal form

The brief's gate (e) asks for `--threads 1` specifically. The shipped default
already passes the stronger form — it has the general fan-out ON (the BC
encoders use all 16) and is byte-identical to the rung on both regions, §3.9 —
so `--threads 1` differs from it only by turning the block encoders off, which
cannot reach the arithmetic. Run anyway for the letter of it:

```
IDENTITY P4e r1  rung exe  vs  new exe --threads 1 --chunk-threads 1
  A: 60 file(s)   B: 60 file(s)   compared: 60
  RESULT PASS - 60 file(s), 24975886 bytes, byte-identical
```

**Gate (e) green.** (15,578 ms wall, which is the rung's own median to within
the noise of §3.9a.)

### 3.10 PEAK MEMORY, and the Commonwealth extrapolation

**Measured**, all on the relink-4 exe, `GetProcessMemoryInfo` peak working set,
sampled every 100 ms by the runner AND printed by the exe itself in the census
line:

| run | 9 chunks | 25 chunks |
|---|---|---|
| rung exe | 1,524 MB | 3,612 MB |
| shipped default (chunk queue serial) | 1,754 MB | 3,752 MB |
| `--chunk-threads 16` | 5,775 MB | **17,992 MB** |

**The extrapolation, and it is an extrapolation, not a measurement.** The
Commonwealth at dim 4 alone is 48x48 = 2,304 chunks against these 9 and 25, and
the four rings together are 3,060 (LODUI1's count). Two parts behave
differently:

* **What scales with the chunk count**: nothing, in memory terms. A chunk's own
  working set is freed when the chunk retires. Between 9 and 25 chunks the
  serial peak went 1,754 -> 3,752 MB, +2.0 GB for +16 chunks — but that is not
  per-chunk accumulation, it is the `writtenBto` list and the post-queue passes
  (the atlas, the arrays, the merge) holding more chunk files open at once. That
  part IS proportional to the region, and it is **the number to watch**: at 3,060
  chunks a straight-line fit off two points gives roughly **125 MB per chunk of
  post-queue state**, which is not credible as a straight line and must be
  measured on a third, larger region before anyone runs the Commonwealth.
  **STATED AS A REFUSAL: two points do not determine that curve.**
* **What scales with the WORKER count**: each worker holds its own plugin reader
  (~250 MB marginal, measured: a load-plus-one-chunk run peaks at 336 MB) and
  its own 512 MB texture cache. 16 workers is therefore **~12 GB of fixed
  overhead** before any chunk state, which is what the 18.0 GB at 25 chunks is.
  On 31 GB that is survivable on a small region and is not something to point at
  the whole Commonwealth.

**What this says for the full bake.** The shipped default (one chunk at a time)
peaked at 3.75 GB on 25 chunks and has no per-worker multiplier at all, so it is
the configuration to run the Commonwealth with — which is what it defaults to.
The honest statement for `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md` is
that **the memory question for the full bake is still open** and wants one
measured run on a ~100-chunk region before the estimate is trusted.

### 3.11 Which harnesses were run, and why those

The change reaches: the LOD generator's chunk builders and their two drivers
(`lodgenmanager.cpp`, `nifcli.cpp`), the DDS writers, the FO4CS-native emitter,
`gamemanager.h` (whose 9 translation units all rebuilt), and
`src/model/nifmodel.cpp` (whose header 81 translation units include). So: the
lodgen suites, the panel's own run harness, and — because `gamemanager.h` and
`nifmodel.cpp` touch everything — the two UI baselines the brief named.

`lodgen_stage_times.sh` matters twice over: it is the instrument this lane's
whole table is read off, and it is the gate that the four stage times are
written AND move.

All on the shipping exe (14:48:52), sequential, one NifSkope instance at a
time, run from Git-Bash so the spells' `python` is the one with numpy (the
lodgen skill's ROADS1 trap):

| harness | this lane | LODUI1's baseline |
|---|---|---|
| `lodgen_stage_times.sh` | **16 checks, 0 failures, PASS** | 16 / 0 |
| `lodgen_terrain.sh` | **26 checks, 0 failures, PASS** | 26 / 0 |
| `lodgen_native.sh` | **18 checks, 0 failures, PASS** | 18 / 0 |
| `lodgen_identity.sh` | **RESULT PASS** | PASS |
| `lodgen_merge.sh` | **RESULT PASS** | PASS |
| `lodgen_texture_arrays.sh` | **RESULT PASS** | PASS |
| `lodgen_card_arrays.sh` | **RESULT PASS** | PASS |
| `lodgen_terrain_vt.sh` | **41 checks, 1 failure** — `V9b the assembled and direct _msn sheets are byte-identical`, the SAME check, unmoved | 41 / 1 (V9b, red on the rung too) |
| `lod_generation.sh` | **116 checks, 0 failures, PASS** (floor 116) | 116 / 0 |
| `lodgen_panel_run.sh` | **125 checks, 0 failures, PASS** (floor 125) — the panel driven to completion twice, on the rewritten chunk loop | 125 / 0 |
| `ui_align.sh` | **11 checks, 0 failures, PASS** | 11 / 0 |
| `water_ui.sh` | **82 checks, 0 failures, 0 skips, PASS** (floor 72) | 37 / 0 at UI3; it has grown since |

**Twelve harnesses, eleven green, one unmoved red.** The only failure is
`lodgen_terrain_vt.sh`'s `V9b the assembled and direct _msn sheets are
byte-identical` — the SAME check, at the SAME count (41 / 1), that was red on
the rung exe before this lane touched anything. Nothing moved.

The panel one is the important one: `lodgen_panel_run.sh` drives the LOD
Generation panel to completion **twice** and it is 125 / 0 on the rewritten
chunk loop, which is what says the GUI half of the change works and not only the
command line.

### 3.12 The `threads` census word: WRITTEN, and it MOVES

Gate (c) of the brief, under the rule of 2026-09-04 21:33. One formatter
(`lodgenBakeCensusLine()`), used by the command line and by the panel's result
line, so the two cannot word it differently. Every field observed moving on real
runs:

```
bake census: threads 16, chunk threads  1, chunk jobs  9, chunk workers 1, peak working set: 1.71 GB (1840144384 bytes)
bake census: threads 16, chunk threads 16, chunk jobs  9, chunk workers 9, peak working set: 5.64 GB (6055882752 bytes)
bake census: threads 16, chunk threads  1, chunk jobs 25, chunk workers 1, peak working set: 3.67 GB (3935715328 bytes)
bake census: threads  8, chunk threads  8, chunk jobs  4, chunk workers 4, peak working set: 3.31 GB (3558850560 bytes)
```

* `threads` moves with `--threads` (16 / 8).
* `chunk threads` moves with `--chunk-threads` (1 / 16 / 8).
* `chunk jobs` moves with the region (9 / 25 / 4).
* `chunk workers` is what the queue could actually USE and is deliberately a
  different number — a 4-job queue cannot use 8 — so the two disagreeing is
  information, not a contradiction.
* the peak working set moves with all of them (1.71 / 5.64 / 3.67 / 3.31 GB) and
  is read from `GetProcessMemoryInfo`, cross-checked against an independent
  100 ms sampler in the runner script: 1,754.9 MB sampled vs 1.71 GB printed on
  the same run, which is the same number.
* a run that never entered the chunk pass reads `chunk workers 0` — a default
  that accuses its own plumbing rather than flattering it.

### 3.13 One small thing left behind

`LodgenPanel::preview( NifModel &, int, int, const QString & )`
(`src/lodgenmanager.cpp:2479`) now has **no caller**: the preview copy is
written by the worker as a file and the main thread only opens it. It is left in
place rather than deleted — it is the only remaining description of what a
preview splice used to do to the live document, and deleting a method in a file
another lane may be editing is not this lane's call. **Named here so it is not
found later and mistaken for a live path.**

### 3.14 P6: nothing left running, the game down

`Fallout4.exe` was checked before the build and before every relink and was
down every time (`Get-CimInstance Win32_Process` filtered on both names, not
`tasklist | grep`). No NifSkope window was open at any point except this lane's
own harnesses and its own `-no-gui` CLI runs, one at a time; bungo's exe was
never renamed aside because he had none open.

**At the end of the lane:** the same filter returns nothing — no `NifSkope.exe`,
no `Fallout4.exe`. `release/NifSkope_inuse_20560.exe` is still on disk; it is a
leftover from an EARLIER lane (the handoff names it as the pre-WATER6 rung),
not this one, and deleting another lane's rung is not this lane's call.

`scratchpad/bakeperf1_20260911/BUILDING` is gone and `DONE` is in. The scratch
directory was trimmed from 325 MB to under 1 MB: the six region bake trees and
the 21 MB symbol-bearing exe copy were deleted after their numbers were taken;
every log, verdict and script that a number in this report rests on is kept.

---

## 4. Owed / red / bungo's calls

### 4.1 THE ANSWER TO HIS QUESTION, in one paragraph

*"bake time, anything we can do to speed it up? use my system to its fullest
here?"* — **not on the chunk queue yet, and the reason is nameable.** A chunk
is mostly NIF parsing, and the NifModel / NifItem / nif.xml layer faults when
two threads parse two documents at once (§3.5, symbolised stack, five runs of
five). Serialising the parse makes the fan-out survive and then it is slower
than one thread, because the serialised part is the part that costs. Everything
needed to use the machine is now in place and switched off behind one number;
what is missing is a thread-safe parser.

### 4.2 RED

| # | what | state |
|---|---|---|
| **R1** | **The NIF parser is not thread-safe.** `lodgenLoadModel` holds a mutex for the whole life of its temporary document as a containment. A lane of its own should find what in `NifItem` / `NifData` / `NifExpr` / the nif.xml tables is shared — `NifItem`'s slab pool comments claim thread safety and the layer around it is not. Until then `--chunk-threads` stays at 1. | **the blocker** |
| **R2** | The chunk fan-out, with the parse serialised, is **slower** than one thread on both test regions. It is correct (byte-identical) but it is not a speed-up, and it ships off. | measured, §3.8 |
| **R3** | `src/model/nifmodel.cpp:1421` binds `QHash<QString,QString> & pseudonymMap = arrayPseudonyms;` and then assigns `pseudonymMap = multiArrayPseudonyms1;` — which **copy-assigns into the global** rather than re-pointing. The first multi-array row ever displayed replaces the application's singular-name table for the rest of the session. Pre-existing, UI path, deliberately not fixed here. | found, not fixed |
| **R4** | The terrain-pyramid tile loop is still serial (one `EsmWorld`, one cache set, same blockers). Its BC encoding does fan out. | by design, §2.6 |
| **R5** | No panel row for either thread number. The brief said the switch is this lane's and the row is not. | as briefed |

### 4.3 OWED, in order of what it would buy

1. **Hoist the model cache to the RUN.** `lodgenBuildObjectChunk`'s `modelCache`
   is a local — one per chunk — so every chunk re-parses every model it needs.
   Over the 9-chunk region that is nine times the parsing for anything shared.
   A run-level cache is **byte-identical** (the loader is a pure function of the
   path) and it speeds up the SERIAL path, which is the one that ships. It is a
   caching change, not a threading one, which is why this lane did not take it —
   and on these numbers it is almost certainly worth more than the fan-out was.
2. **Batched card bakes** (the brief's item 4) — **not done, and the numbers for
   the decision are in §4.4.**
3. A panel row for `--chunk-threads`, once R1 is fixed and it is worth having.
4. The pyramid tile loop, once R1 is fixed.

### 4.4 CARD BAKES: what was found, and what was not done

`tools/bake_impostor_cards.sh` launches **one whole NifSkope process per model**
and the hook waits a fixed **1200 ms** after `completeLoading` before it starts
(`src/nifskope_ui.cpp`, the `WW_IMPOSTOR_BAKE` hook). So every card pays a
process start plus 1.2 s of sleep before a pixel is drawn, and the driver's
comment puts the whole thing at about 3 s a card.

**Rendering K models per frame into offscreen targets was not attempted, and
should not be**: every extent the bake records — the card's half-width and
half-height, the per-frame offsets, the sidecar's front/side lines — is a world
measurement taken off VIEWPORT PIXELS through one units-per-pixel constant, and
lane CARDORTHO already found what happens when the camera under that arithmetic
changes without the arithmetic changing with it (every card baked before
2026-09-10 was drawn through a perspective frustum and measured as if it were
orthographic). Changing the render target is changing that camera.

**What is worth doing instead, and is a lane of its own:** teach the hook to
bake a LIST of models in ONE process, one after another, in the same window at
the same viewport — the picture is identical by construction because nothing
about the camera or the frame changes — and the driver loses its
one-launch-per-model loop. That removes (K−1) process starts and (K−1) × 1.2 s
of sleep from a K-model library. On a tree-only Sanctuary run of 19 candidates
that is roughly 19 × (startup + 1.2 s) saved. The gate is the existing one:
bake the same two models both ways and compare the PNGs byte for byte.

---

## 5. Mistakes

Full text for `MISTAKES.md` is in
`scratchpad/bakeperf1_20260911/MISTAKES_ENTRIES.md` — **three entries**, not
appended by this lane (the file is shared; the director splices):

1. **Six Windows crash dialogs reached bungo's desktop.** A lane deliberately
   provoking a crash must arm the error mode first, and the exe must arm it for
   itself. Fixed in the exe at relink 3 and in every launcher script.
2. **The shared-state inventory stopped at the boundary of `NifModel`** and the
   crash was inside it. An inventory must follow the CALL, not the file, and a
   comment claiming thread safety is a claim to test.
3. **Three fixes shipped on hypotheses, two of them wrong.** Get the stack
   first: one diagnostic relink with `LFLAGS="-Wl,-subsystem,windows -mthreads"`
   (no source change) answered in one run what three relinks had not.

A fourth, smaller one, recorded here rather than in the ledger because it cost
only minutes: **a backslash does not survive a shell heredoc**, which the lodgen
skill already says. It happened three times in this lane anyway — a
`QStringLiteral( "\n" )` became a real newline in the source, a `.pro`
continuation lost its backslash, and a PowerShell dot-source path lost its
separator. Every patch after that was written with the Write tool and built its
backslashes from `chr(92)`.

---

## 6. Finished-work skill review

**Skills loaded and used.** `nifskope-ww-lodgen` (the byte-identity gates ARE
this lane's gate; its CLI table, its absolute-path trap, its heredoc-backslash
trap — which still bit three times — and its region facts),
`nifskope-ww-build-verify` (the gated chain on make's own exit code, the
dependency read-back after a header change, the exe-newer sweep, the process
guard), `ww-spec-gate-audit` (read before the gates were believed: this lane's
gate number is self-referential — byte identity against its own off value —
so the one-sided-approximation test does not apply, but "print the table, not
the count" is why §3.8 lists files AND bytes AND the comparator's refuter),
`fo4cs-census-field` (the `bake census` line: written, moves with
`--threads`/`--chunk-threads`, no duplicate key, a default that accuses its own
plumbing — `chunk workers 0` when the pass never ran).

**Skills NAMED in the brief and not used, with the reason.**
`ww-anchored-hookup` — it covers a lane that may not touch the existing files
until a gate passes, and writes the hook-up as a refusing script instead. This
lane OWNS `src/` and `NifSkope.pro` outright, so there was no file to defer;
every edit was still made by a Write-tool patch script asserting
`count(anchor) == 1` and re-measuring CR/LF byte counts, which is the part of
that skill that applies. `nifskope-ww-render-shot` — the one-instance rule was
honoured (never two NifSkopes at once, and the process guard was run before
every build), but no render was taken: the card-bake batching it was named for
was refused with numbers instead (§4.4).

**Two skills written, both because the procedure was re-derived from first
principles here and will be needed again.** They are in the REPO tree
(`<repo>/.claude/skills/`); the director mirrors them to the live tree.

1. **`nifskope-ww-crash-diagnose`** — the whole of §3. Reading a Windows
   NTSTATUS exit code; the crash-dialog rule and why a wrapper is only half of
   it; the bisect-the-stage table with three runs a variant; **the one-minute
   diagnostic relink that puts the symbol table back** (`make
   LFLAGS="-Wl,-subsystem,windows -mthreads"`, no source change), which is the
   step that should have come second and came fifth; the gdb batch and why
   `thread apply all bt` matters more than the faulting thread; and the rule
   that a fix which moves a crash later is progress, not a fix. **This lane
   spent three relinks and about an hour on hypotheses because that page did
   not exist.**

2. **`ww-parallelise-a-stage`** — the inventory that must follow the CALL and
   not the file; the four recurring unsafe shapes in this tree (a cache that
   hands out interior references, a decompression scratch, an LRU whose evictor
   frees, a lazy first-use static); retire-by-index and journal-and-replay as
   the two ways to keep order; the byte-identity gate WITH its refuter and the
   rung comparison; wall time versus summed CPU time in a stage line; **a
   per-call thread is not a pool**; and the rule this lane learned the hard way,
   that a fan-out nobody measured is not a speed-up.

**Declined.** No skill for the region choices (Sanctuary 9 chunks, downtown
Boston 25) — `nifskope-ww-lodgen` already carries both and why.
