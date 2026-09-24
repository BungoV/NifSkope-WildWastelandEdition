# Lane NIFPARSE1 — the NIF model loader made thread-safe

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, nothing committed.
Exe at launch `release/NifSkope.exe` **2026-09-11 14:48:52, 21,261,312 B** (BAKEPERF1).

**CODE-ONLY at start.** Lane CARDS-AGG holds the build slot and `src/lodgen.cpp`,
`src/nativeemit.*`, `src/lodofile.*`, `src/lodifile.*`, the card/impostor code and
`NifSkope.pro`. No build, no relink — not even the diagnostic one — until
`scratchpad/cards_agg_20260911/DONE` exists and no `scratchpad/*/BUILDING` is up.

Machine: 16 logical cores, 31 GB, RTX 5070 Ti.

---

## 0. Pre-registered gates

Registered **before** any source was edited. Written 2026-09-11 16:0x, from the
brief's gate list; nothing here was invented after a number came in.

| id | gate | what makes it red |
|---|---|---|
| **N1** | the fault is named from a **symbolised** stack, on **three** runs, and the three agree (or the report says they do not), BEFORE any fix; the shared-state inventory is a table by CALL path, written first | a fix proposed with no stack; an object touched during a worker parse that the table does not name; three stacks that disagree and are reported as agreeing |
| **N2** | `--chunk-threads 16`, **20 consecutive runs** on EACH region (9-chunk Sanctuary, 25-chunk Boston): no fault, exit 0 every run; **byte identity** of every output file vs the serial run on both regions | one non-zero exit in twenty (the report says at which run); one file differing by one byte; one file present on one side only |
| **N3** | stage-time table serial vs 16 threads, **medians of 3**, speed-up per stage; peak working set at 16 threads on 25 chunks; ONE ~100-chunk run as the third memory point | a stage that got slower and is not reported as a red; a peak that puts the Commonwealth past 31 GB with no stated rule |
| **N4** | serial path byte-identical to the rung on both regions; the editor harnesses at their baselines; the 38k-vert torso load time within **5 percent** of before | any editor harness count that moved and is not explained by name; torso load >5 percent slower |
| **N5** | exe newer than every changed file; **every TU including a changed model header** rebuilt (the model headers reach ~81 TUs); drivers rebuilt; the rung equals CARDS-AGG's exe byte for byte | one stale object; a rung that is not the exe this lane started from |
| **N6** | no NifSkope left running; `Fallout4.exe` down at every build and every launch; **no crash dialog ever** — the error mode stays armed in the exe and in every launcher | either process alive at the end; one Windows "Application Error" box |

Floors, so no gate can pass vacuously:

* **N2 floor.** The comparison set must be **non-empty and named**: at least the
  `.BTR`, `.BTO` and `.BTO.manifest.txt` for every populated chunk, the terrain
  sheets, and the `.lodo`/`.lodi` pair. BAKEPERF1's counts are the floor —
  **60 files / 24,975,886 B** on Sanctuary and **163 files / 77,591,755 B** on
  Boston. Fewer files than that is a red, not a pass.
* **N2 refuter.** BAKEPERF1's comparator
  (`scratchpad/bakeperf1_20260911/bake_diff.py`) is run FIRST against a
  deliberately corrupted copy and against a copy with one file deleted, and must
  report both. Shown red before any verdict is believed.
* **N1 floor.** The stack must name a frame inside the model layer. A stack of
  `?? ()` is not a stack — that is what the diagnostic relink is for.
* **N3 floor.** A stage time of 0.0 s on both sides reads "did not run", never
  as a speed-up. Timing verdicts are medians of three alternating runs, because
  BAKEPERF1 measured a 4.5 s within-exe spread on a 16 s region.
* **N4 floor.** The torso measurement is taken on the SAME model both times and
  the before number is re-measured on the rung exe in the same session, not
  quoted from an older report.

Ways back, exact at their off value:

* `--chunk-threads 1` — the serial queue, the loop that has always run.
* `--threads 1` — the general fan-out off as well.
* The rung exe (taken from whatever CARDS-AGG leaves), `release/NifSkope.before_nifparse1.exe`.

---

## 1. The fault, named

### 1.1 Written BEFORE any fix, and what is still owed

The inventory below (1.2) was written **2026-09-11 16:2x, before one line of
`src/model`, `src/xml` or `src/data` was changed**, from reading the call paths
a worker takes. Gate N1's other half -- **the symbolised stack, three runs** --
needs the diagnostic relink, and a relink is a build: lane CARDS-AGG holds the
build slot. Until it is taken, 1.2 is an inventory and 1.4 is a list of
CANDIDATES with the discriminator that separates them, not a cause. Nothing
here is stated as the cause (CONSTITUTION 4, rule 2 of 2026-09-04 21:33).

### 1.2 The inventory, BY CALL PATH

BAKEPERF1's recorded mistake was that its inventory "stopped at the boundary of
`NifModel`". This one starts there and follows the CALL: every row below is
reached from `lodgenLoadModel` -> `NifModel` ctor / `load()` / `get<>()` /
`~NifModel`, and nothing is listed because of which file it lives in.

Legend: **static** = one instance for the process; **per-model** = one per
`NifModel`; **pool** = the slab allocator.

| object | kind | where | touched by (call path) | written during a worker parse? | protected by | verdict |
|---|---|---|---|---|---|---|
| `NifItemPool` (free list, chunk vector, slot size) | pool | `src/data/nifitem.cpp:56-120` | `new NifItem` / `delete NifItem` -- every item, millions per mesh | **yes, constantly** | `std::mutex`, taken in BOTH `allocate` and `deallocate` | **safe.** The comment claiming it is safe is, for once, true |
| `arrayPseudonyms`, `multiArrayPseudonyms1/2` | static | `src/model/nifmodel.cpp:66-68` | `NifModel` ctor -> `setupArrayPseudonyms()` | once, first ctor | `static QMutex pseudonymGuard` (BAKEPERF1) | safe on the write; the READERS are the editor path -- see R3 |
| `NifModel::compounds / fixedCompounds / blocks / blockHashes / supportedVersions` | static | `src/xml/nifxml.cpp:50-54` | `insertType`, `insertNiBlock`, `inherits`, `isFixedCompound`, `clear()`, `load()` -- read on every field of every block | **no** -- written only by `loadXML()`, which `initModelLayer()` (`src/nifcli.cpp:153`) runs before any model exists | nothing on the read side; `XMLlock` is taken on the WRITE side only and **no reader anywhere takes a read lock** | safe **given** that ordering, and only given it |
| `blockHashes[hash]` | static | `src/model/nifmodel.cpp:2199` | `NifModel::load`, file version `0x14030102` only | the non-const `QMap::operator[]`, i.e. a `detach()` on process-wide state | nothing | **latent**, and not the Fallout 4 path (that version is not FO4). Made a const read anyway |
| `NifValue::typeMap / enumMap / typeTxt / aliasMap` | static | `src/data/nifvalue.cpp:44-47` | `NifValue::type()` from `insertType` (`:1261`) and `updateArraySizeImpl` (`:689`) | **`NifValue::type()` calls `initialize()` -- which CLEARS all four -- whenever `typeMap` is empty** (`nifvalue.cpp:142`) | nothing | **latent.** It cannot fire after `loadXML()`; a lazy destructive re-init reachable from the parse is still a live edge |
| `NifValue::typeDescription`'s `txtCache` | static | `src/data/nifvalue.cpp:159` | `data(..., TypeCol)` -- editor only | yes, on first tooltip | nothing | editor path; a bake never reaches it |
| `OPT_PER_LINE` | static | `src/data/nifvalue.cpp:50` | `enumOptionName()`, reads `QSettings` once | yes, first flags render | nothing | reachable from a value's string form; an `int` write, benign, named |
| `NifSharedData` behind every `NifData` | static-owned | `src/data/nifitem.h:273`, `:309` | every `NifItem` copy-constructs the XML template's `NifData`, so **~1.5M reference bumps per big mesh land on a handful of objects owned by `NifModel::blocks`** | the refcount only; `QSharedData::ref` is a `QAtomicInt`, and no template is ever mutated in place (every setter detaches first) | Qt's atomics | **safe.** Contended, not racy |
| `NifItem::rowIdx / conditionStatus / vercondStatus` | per-model | `src/data/nifitem.h:812-816` | `row()`, `evalCondition`, `invalidateDependentConditions` | yes | per item, one model per thread | safe |
| the row-0 condition cache (`getConditionCacheItem`) | per-model | `src/model/nifmodel.cpp:2763` | `evalConditionImpl` / `evalVersionImpl` on fixed compounds (`BSVertexData`) | writes row 0's `conditionStatus` on behalf of rows 1..n | inside ONE model | **per-model, not static** -- the brief's question answered: it is not shared between threads |
| `NifIStream` / `NifOStream` and their `QDataStream` | per-model | `src/io/nifstream.{h,cpp}` | `load()` | per stream object, constructed per call | n/a | safe -- **there is not one file-scope static in that file** |
| `Transform::Transform( const NifModel *, const QModelIndex & )` | pure | `src/data/niftypes.cpp:701` | `lodgenWorldTransform` | touches only the passed model | n/a | safe |
| `semanticStrings` | static const | decl `src/data/niftypes.h:2251`, def `src/gl/glmesh.cpp:1076` | renderer only | no | n/a | not on this path |
| `BaseModel::messages`, `msgMode` | per-model | `src/model/basemodel.h:609,620` | `logMessage` / `reportError` on a malformed NIF | yes | per model; the ctor sets `MSG_TEST` | safe -- **and `MSG_TEST` is the only thing keeping the next row off a worker** |
| `Message::messageBoxes`, and the `QMessageBox` widgets it builds | static | `src/message.cpp:160`; boxes at `:30, :51, :197` | `Message::append` / `message` / `warning` | **yes, and it CONSTRUCTS QWidgets on the calling thread**, parenting them to a main-thread widget | **nothing** | **UNSAFE.** Entered from `nifmodel.cpp:414` (the ctor's unsupported-startup-version warning, **not** gated by `msgMode`), `gamemanager.cpp:204/247/317`, and `qWarning()` at `gamemanager.cpp:306` through the installed handler |
| `GameManager::nifResourceMap` | static | `src/gamemanager.cpp:78` | ctor `:285`, `clear()` `:449`, `load()` `:2157`, dtor `:295` | yes, per model | `QRecursiveMutex` at 3 of its 5 touch sites | **safe on this path.** The two unlocked iterations are `close_materials` (`:267`) and `close_resources` (`:654`); a bake calls neither |
| `GameManager::archives[game]` and its `BA2File * ba2File` | static | `src/gamemanager.cpp:77`, `:174-209`, `:254-262` | `find_file` / `get_file` -> **lazy `init_archives()`**, and the self-healing retry at `:310-316`, which calls `close_archives()` -> `delete ba2File` | **yes, lazily -- and the retry FREES it** | **nothing** | **UNSAFE.** `lodgenWarmSharedIndices()` pre-builds it, which covers first use; it does **not** cover the retry's `delete`, and `findFile` hands out interior `std::string_view`s that dangle the moment it runs |
| `lodgenStackIndex()` / `lodgenMeshArchives()` statics | static | `src/lodgen.cpp:1557-1582` | `lodgenReadAsset` | `index.reset()` if the resource stack changes mid-run | nothing; pre-warmed | latent; the stack does not change mid-bake |
| `QSettings` (Windows registry) | Qt global | `src/model/nifmodel.cpp:300` (**every ctor**) and `:2135` (**every load**) | ctor, `load()` | Qt's own cache | Qt's internal locks; a distinct object per thread | reentrant per Qt's contract -- listed because nobody had counted it: it is a registry hit per model |
| `Q_DECLARE_METATYPE( NifValue )` registration | Qt global | `src/data/nifvalue.h:437` | any `QVariant` of a value | pre-registered on the main thread, `src/nifcli.cpp:151` | Qt | safe, **because** the CLI pre-registers it |
| `QAbstractItemModel` signals (`sigProgress`, `beginInsertRows`) | per-model | `nifmodel.cpp:2163/2176`, `basemodel.cpp:102` | `load()`, `updateArraySizeImpl` | emitted with **nothing connected** in a bake | n/a | safe: there is not one `connect()` in `nifmodel.cpp` or `basemodel.cpp`, and `lodgenLoadModel` builds models with no views |
| `SpellBook` registries (`_spells`, `_hash`, ...) | static | `src/spellbook.cpp:50-80` | -- | filled by static initialisers before `main` | n/a | **not reached**: the only spell call from the model is `SpellBook::instant` inside `data()`, an editor path |
| `NifExpr::partition`'s `reInt / reUInt / reFloat / reVersion` | static | `src/xml/nifexpr.cpp:212-215` | **every array resize**, via `addConditionParentPrefix` -> `NifData` ctor -> `NifExpr` | Qt compiles the pattern lazily inside the shared instance | Qt's own `QRegularExpressionPrivate` compile mutex | believed safe -- made `thread_local` anyway, because "believed safe" is exactly what the slab pool's comment said |

### 1.3 What this table says that the last one did not

Two rows are new, and **neither is in the model layer**:

1. **`Message::append` builds `QMessageBox` WIDGETS on whatever thread calls
   it** and appends them to an unguarded static vector. A worker reaching it
   constructs a QWidget off the main thread and splices it into a main-thread
   widget's child list. The parse's OWN errors do not go there -- `msgMode` is
   `MSG_TEST` on a default-constructed model, so they land in the model's own
   list -- but six paths bypass `msgMode` entirely, including `qWarning()` on
   **every missing texture** inside `GameResources::get_file`.
2. **`GameResources::init_archives()` / `close_archives()` free and rebuild a
   shared `BA2File`** with no lock at all. The warm-up covers first use; it does
   not cover the retry at `gamemanager.cpp:310-316`, which `delete`s that
   `BA2File` while other workers are inside `findFile()` / `extractFile()`.

Both sit in the **road pass's** reach -- which is the pass BAKEPERF1 bisected
the crash to (`--no-roads` was clean 3 of 3 at nine workers, and that result was
read at the time as "the road pass parses more NIFs", which it also does).

### 1.4 The candidates, and the experiment that separates them

The stack BAKEPERF1 obtained names a PLACE -- `NifItem::deleteChildItems()`
under `BaseModel::~BaseModel()` -- and for `STATUS_HEAP_CORRUPTION` the place is
where the damage was **detected**, not where it was **done**; the crash-diagnose
skill says so in as many words. So that stack alone cannot choose between:

| # | candidate | what it predicts |
|---|---|---|
| **C1** | the model layer itself (parser, item tree, XML tables) | a fault with the parser ALONE on N threads, nothing else running |
| **C2** | the resource layer -- `init_archives` / `close_archives` on the shared `BA2File` | no fault with the parser alone; a fault only once archive lookups are in the mix |
| **C3** | `Message` / `qWarning` building QWidgets on workers | no fault with the parser alone; a fault only once a warning fires |

**THE DISCRIMINATOR, and it is written:** `src/nifparsestress.{h,cpp}` (new,
this lane). It reads the NIFs once on the calling thread, then builds, loads,
walks and destroys `NifModel`s from those bytes on N threads with **no
`EsmWorld`, no texture cache, no archive lookup, no road gatherer and no file
I/O inside the threaded region at all**. C1 predicts it faults; C2 and C3
predict it does not.

It is not only a crash detector. Every worker digests what it READ BACK out of
the document -- block names, types, array sizes and every value's string form,
in a fixed walk order -- and every digest must equal the single-threaded
reference digest for the same file, so silent corruption that happens not to
fault still fails. Its floors:

* a fixture that did not load is a named failure, not a skip;
* a walk that digested fewer than 32 items is a named failure (a truncated walk
  would make every later comparison pass over nothing);
* the digest is taken TWICE on one thread first -- if it is not reproducible
  serially, asking sixteen threads to reproduce it measures nothing;
* the load count must equal `threads x reps x files`;
* `--stress-sabotage digest` flips one byte for one worker and MUST come back
  red; if it does not, the digest is hashing a constant and the gate is
  worthless. `--stress-sabotage share` puts every worker on ONE shared
  `NifModel` -- the thing the layer is not allowed to survive -- and is the
  floor under "N threads clean" meaning anything.

It compiles today, before any hook-up: `g++ -fsyntax-only` **RC=0** through
MSYS2 UCRT64 with the project's own include set.

The hook-up that puts it into the build and onto the command line is written as
a **refusing script and NOT applied** -- `NifSkope.pro` belongs to CARDS-AGG
while that lane is alive and `src/nifcli.cpp` is shared
(`scratchpad/nifparse1_20260911/hookup.py`, per `ww-anchored-hookup`):

```
NifSkope.pro           after   count=1     (HEADERS)
NifSkope.pro           after   count=1     (SOURCES)
src/nifcli.cpp         after   count=1     (the include)
src/nifcli.cpp         after   count=1     (the --stress-* options)
src/nifcli.cpp         after   count=1     (the locals)
src/nifcli.cpp         replace count=1     (the dispatch branch)
src/nifcli.cpp         after   count=1     (the usage lines)
NifSkope.pro           CR before=0 after=0
src/nifcli.cpp         CR before=0 after=0
RESULT CHECK OK - 7 anchors, all matched once, nothing written
```

**One mistake the check itself caught, recorded in section 5:** the dispatch
edit was first written as an `after` on the retired-`lodt` branch's opening
brace, which would have left that branch empty and moved its error message onto
an `else if ( false )`. It is a `replace` that repeats its own anchor now.

### 1.5 Two candidates narrowed by reading, before any run

Reading the resource path all the way through moved two of the three
candidates, and both movements are evidence rather than opinion, so they are
recorded here with the lines that settle them.

**Where a bake's file reads actually go** (`lodgenReadAsset`, `src/lodgen.cpp:1631`):

* a **`.nif`** is read from `lodgenMeshArchives()` -- a `BA2File *` behind a
  function-local static -- with `findFile()` + `extractFile()`, both `const`.
  `BA2File` has no `mutable` member and `ba2file.cpp` never touches
  `FileBuffer::filePos` on the extract path; it works from pointers and offsets
  into disjoint caller buffers. **Concurrent extraction is not the fault**, and
  this path never enters `GameManager` at all.
* a **`.dds`, `.bgsm` or `.pbrm`** falls through to
  `Game::GameManager::get_file( out, Game::FALLOUT_4, ... )`
  (`src/lodgen.cpp:1662`) -- which IS the path with the lazy `init_archives()`,
  the `close_archives()` retry, and the `qWarning()` on every miss.

So the texture stage and the road pass -- the two variants BAKEPERF1 bisected
the crash to -- are exactly the two that enter `GameManager`, and the mesh
reads are not. That is a real narrowing and it does not depend on a stack.

**C3 is refuted for a headless run, and stays live for the PANEL.**
`qInstallMessageHandler( NifSkope::MessageOutput )` sits inside
`if ( auto a = qobject_cast<QApplication *>( app.data() ) )`
(`src/main.cpp:161`), and `-no-gui` builds a plain `QCoreApplication`
(`src/main.cpp:76-77`), so that cast fails and the widget-building handler is
**never installed** in a `-no-gui lodgen` run; `nifskopeCliMain` installs its own
stderr-only handler instead (`src/nifcli.cpp:5622`). The remaining direct
`Message::append` / `Message::warning` calls would try to construct a QWidget
with no `QGuiApplication`, which Qt turns into a named fatal on stderr, not a
silent corruption -- and no such line is in BAKEPERF1's logs.

But the same code under the **LOD Generation panel** has `MessageOutput`
installed and a real `QApplication`, so a chunk worker that misses one texture
builds a `QMessageBox` on a worker thread and appends it to the unguarded
`messageBoxes`. `lodgen_panel_run.sh` is the harness that would meet it. The fix
(F1) therefore stays, and its justification is the panel, not the command line.

**What is left for the headless fault:** C1 (the model layer) and C2 (the
`close_archives()` retry freeing the shared `BA2File` mid-read). The stress
harness separates those two and nothing else has to.

---

## 2. The fix

**NOT APPLIED.** Gate N1 is not discharged, and BAKEPERF1's third recorded
mistake was "three fixes shipped on hypotheses, two of them wrong". Everything
below is written, anchored against the tree and `--check` green; it goes in
after the stack and the stress run say which candidate is the cause.

`scratchpad/nifparse1_20260911/fixes.py`, **25 edits over eight files**, all
anchors matched once, CR count unchanged on every file:

| id | file | what, and why it is in the "make the shared thing safe" tier rather than the "per-thread copy" one |
|---|---|---|
| **F1** | `src/message.cpp` (5 edits) | `Message::append` / `message` refuse to build a widget off the GUI thread and write the message to stderr instead. One four-line guard at each of the four entry points, so every public helper (`warning`, `critical`, `info`) is covered by construction. **On the main thread nothing changes at all** — that is the way back, and it is exact |
| **F2** | `src/gamemanager.{h,cpp}` (7 edits) | one **recursive `QReadWriteLock`** over every `GameResources`' archive state. `init_archives` / `close_archives` / `close_materials` take the WRITE lock; `find_file` / `get_file` take the READ lock across `findFile` AND `extractFile` AND the interior `string_view`s between them, and drop it before the retry re-enters through `close_archives`. Read/write rather than a mutex **on purpose**: `extractFile` is where the texture stage's time is, and a plain mutex would cost the bake exactly what the fan-out buys. `close_materials` also takes `nifResourceMutex()` — it walks `nifResourceMap`, and it is one of the two places BAKEPERF1's lock missed |
| **F3** | `src/data/nifvalue.cpp` (2 edits) | `NifValue::type()`'s lazy `initialize()` — which **clears all four static tables** — behind `std::call_once`. It cannot fire after `loadXML()`; a destructive lazy re-init reachable from the parse path is not left resting on call order |
| **F4** | `src/model/nifmodel.cpp` | `blockHashes[hash]` -> `blockHashes.value(hash)`: the non-const `QMap::operator[]` detaches a process-wide table from inside a per-file load |
| **F5** | `src/model/nifmodel.cpp` | **R3, carried from BAKEPERF1 and left unfixed there.** A pointer where a reference was: `QHash & m = arrayPseudonyms;` then `m = multiArrayPseudonyms1;` copy-assigns INTO the global, so the first multi-array row ever displayed replaces the application's singular-name table for the session. Editor path, gated by the editor harnesses, not by the bake |
| **F6** | `src/xml/nifexpr.cpp` (4 edits) | the four shared `QRegularExpression`s on the array-resize path become `thread_local`. Qt guards its own lazy compile, so this is belt and braces — taken because "believed safe" is exactly what the slab pool's comment said |
| **F7** | `src/lodgenparallel.{h,cpp}` (3 edits) | `lodgenChunkThreadMemoryCap()` and `lodgenChunkThreadBoundBy()`, declaration AND definition together so neither can land alone. `--chunk-threads 0` stops meaning "the core count" and means the smaller of the cores and what free memory holds; the census says which bound decided it. The per-worker world figure is BAKEPERF1s measurement (250 MB marginal), not a round number. See section 4 |

**What it costs the serial path** is the open question and it is measured, not
asserted: F2's read lock is the only addition on a hot path (`get_file` per
texture and per material). It is uncontended at `--chunk-threads 1`, which is
the shipped default, and the measurement that decides whether it ships is the
stage table against the rung, **medians of three, alternating** — BAKEPERF1
measured a 4.5 s within-exe spread on a 16 s region, so a single pair of numbers
cannot answer it. A read lock that costs the serial bake 5 percent is a refusal
with numbers, per the brief.

**One fix is NOT in this script and must go through a second `--check` hook-up:**
removing `lodgenLoadModel`'s parse mutex (`src/lodgen.cpp:2067`). That file is
CARDS-AGG's.

---

## 3. Build and gates

**NONE OF IT RAN.** The build slot was held by lane CARDS-AGG for the whole of
this lane's window; a diagnostic relink is a link and therefore a build, so even
gate N1's instrument could not be taken. Polled every 60 s; the marker
`scratchpad/cards_agg_20260911/DONE` never appeared and the exe on disk is still
BAKEPERF1's **14:48:52, 21,261,312 B**.

| gate | state |
|---|---|
| **N1** the fault named from a symbolised stack, 3 runs | **NOT DISCHARGED.** The inventory half is done (section 1.2); the stack half needs the relink. Three candidates and the experiment that separates them are in 1.4 |
| **N2** 20 consecutive 16-thread runs per region + identity | not run. Script ready: `loop20.ps1`, NTSTATUS decoded per run, first run's tree kept as the identity subject |
| **N3** stage table, peak working set, the 100-chunk point | not run. `--terrain-region 0 -12 39 27` is the 10x10 = 100-chunk region |
| **N4** serial byte-identity, editor harnesses, torso load time | not run |
| **N5** exe newer than every changed file, ~81 TUs rebuilt | not applicable — no build |
| **N6** nothing left running, game down, no crash dialog | **GREEN.** `Fallout4.exe` was down at every check; this lane started no NifSkope instance at all and provoked no crash, so no dialog could reach his desktop |

What IS discharged, and was checked rather than claimed:

* `g++ -fsyntax-only` on `src/nifparsestress.cpp`: **RC=0**, MSYS2 UCRT64, the
  project's own include and define set out of `Makefile.Release`.
* `hookup.py --check`: **7 anchors, all matched once**, `NifSkope.pro` CR 0
  before and after, `src/nifcli.cpp` CR 0 before and after, nothing written.
* `fixes.py --check`: **25 edits, all anchors matched once**, CR unchanged on
  all eight files, nothing written.

---

## 4. Owed / red / bungo's calls

### 4.1 The one thing to say to him

The bake is **not** faster yet and this lane did not make it faster. What it did
was refuse to build on an unmeasured cause: the previous lane's finding that
"the NIF parser is not thread-safe" rests on a single stack taken inside a run
where five subsystems are live at once, and following the calls turned up two
genuinely unsafe things **outside** the parser — a shared archive index that one
worker can free while another reads it, and a message path that builds a window
from a worker thread. The experiment that tells the two apart is written and
needs one build.

### 4.2 RED

| # | what | state |
|---|---|---|
| **R1** | **The fault is NOT named.** Gate N1 is undischarged; no fix has been applied on the strength of a guess | the blocker |
| **R2** | `GameResources::init_archives` / `close_archives` free a shared `BA2File` with no lock (`gamemanager.cpp:174-209`, `:254-262`, retry at `:310-316`) | found, fix written, not applied |
| **R3** | `Message::append` / `message` build QWidgets on any thread (`message.cpp:30/51/197`) and append to an unguarded static (`:160`). Refuted for `-no-gui`; **live for the LOD panel** | found, fix written, not applied |
| **R4** | `src/model/nifmodel.cpp:1431` copy-assigns into the global pseudonym table (BAKEPERF1's R3) | fix written, not applied |
| **R5** | Whether a safe parser buys any SPEED-UP is unmeasured. BAKEPERF1: 1.5-1.8x slower with the parse serialised, 21.9 GB at 16 workers on 25 chunks | open |
| **R6** | `close_resources` (`gamemanager.cpp:654`) still iterates `nifResourceMap` unlocked. Not on the bake path; not fixed here | named, not fixed |

### 4.3 bungo's calls, when the numbers exist

1. **The `--chunk-threads` default cannot be "the machine".** Each worker owns
   its own plugin reader (~250 MB marginal, BAKEPERF1's measurement) and its own
   texture cache (512 MB), so sixteen workers is ~12 GB of fixed overhead before
   one chunk of state. The rule this lane proposes, to be set from the 100-chunk
   run: `workers = min( cores, (freePhysical x 0.6) / (worldBytes + texBudget) )`,
   with the census line printing the cap AND which bound decided it, so a run
   held back by memory says so in words instead of looking slow.
2. **`--chunk-threads 1` stays the exact way back** whatever the gate says, and
   the serial path stays byte-identical to the rung.

---

## 5. Mistakes

Full text in `scratchpad/nifparse1_20260911/MISTAKES_ENTRIES.md` — **four
entries, not appended by this lane** (the file is shared; the director splices):

1. **A hook-up anchor that would have emptied the branch it landed in.** An
   `after` on a line ending in `{` inserts INSIDE that scope. The `--check` was
   green: "matches exactly once" proves an anchor is unique, not correct.
2. **Indentation typed from a screen instead of measured from the bytes.** Four
   tabs where the file has five; the anchor counted 0 and, because the script
   writes all-or-nothing, refused twenty-two correct edits with it.
3. **"The NIF parser is not thread-safe" was a conclusion, not a measurement** —
   and the rule that catches it (follow the CALL, not the file) is one the
   previous lane had itself just written into a skill.

---

## 6. Finished-work skill review

**Loaded and used.** `nifskope-ww-crash-diagnose` (its section 3 relink recipe
is `relink_sym.sh`; its "the other threads say what it collided with" is why
`gdb3.sh` runs `thread apply all bt`; its NTSTATUS table is why `loop20.ps1`
decodes the exit code per run) — **and its opening rule, that the stack names
where the damage was DETECTED, is the whole of section 1.4**.
`ww-parallelise-a-stage` (the inventory that follows the CALL and not the file —
which is the rule that found F1 and F2; the byte-identity gate with its refuter;
the four recurring unsafe shapes, of which the shared `BA2File` is number four,
"a lazy build-once static", and `close_archives` is number three, "an LRU whose
evictor frees"). `ww-anchored-hookup` (both scripts; **amended, below**).
`ww-test-harness-add` (the floor that fires: `--stress-sabotage digest` is run
FIRST and its red is the precondition for believing the green).
`fo4cs-census-field` (F7's `bound by` word is designed written-and-moves:
"cores" / "memory" / "asked", and a run that never entered the pass must not be
able to read like one that did). `nifskope-ww-build-verify` and
`nifskope-ww-lodgen` (read; their build halves could not be exercised).
`nifskope-ww-resume-pending` (the PENDING resume shape).

**Amended, and the director must mirror it to the live tree.**
`.claude/skills/ww-anchored-hookup/SKILL.md` gained **section 5, "Matches once
is not goes in the right place"**: never anchor an `after` on a line ending in
`{`; measure a multi-line anchor's indentation with Python byte counts rather
than typing it (the Read tool's line-number prefix makes the first level
uncountable by eye); and after `--check` goes green, read the resulting shape
before believing the count. Both rules cost this lane a pass, and the second is
the line-ending rule extended to leading whitespace.

**Wished for, and written as a recommendation rather than a file** because it
wants the numbers this lane could not take: a skill for **"separate the stage
from the pipeline"** — the discipline of section 1.4, where a fault inside a
five-subsystem run is attributed by building a run that contains ONE of them.
`ww-parallelise-a-stage` says to inventory shared state and
`nifskope-ww-crash-diagnose` says to bisect by option; neither says to build a
standalone driver whose green REFUTES the accusation. If the stress harness
earns its keep at the gate, that page should be written from it.

**Declined.** No skill for the region choices or the bake runner —
`nifskope-ww-lodgen` carries both, and BAKEPERF1's `bake_run.ps1` /
`bake_diff.py` / `repeat_timing.ps1` were reused unchanged rather than retyped,
which is the point of keeping a lane's scripts in the repo.

---

## Build (RESUME3) -- 2026-09-11, appended, this lane's own text untouched

Lane RESUME3 built and gated everything section 3 above lists as "NONE OF IT
RAN". Exe at launch and rung: `release/NifSkope.before_resume3.exe`
2026-09-11 16:20:02, 21,419,520 B, md5 `3ebf175826feee6c6545cf0873635acc`
(CARDS-AGG's). Shipping exe: `release/NifSkope.exe` **19:08:42, 21,435,904 B,
md5 `847236ecb8f83d64b25b8a2ceeec91d3`**. One build (16:53:23) plus five counted
relinks. Full detail in `scratchpad/lane_resume3_report.md`.

### The gates

| gate | verdict |
|---|---|
| **N1** the fault named from a symbolised stack, 3 runs | **DISCHARGED, four stacks.** The fault is `cliMessageHandler` (`src/nifcli.cpp`) writing through `err()`, a function-local `static QTextStream` with no lock, from every chunk worker at once via `qWarning()` in `GameResources::get_file`. Two of four faults are inside that function; the other two are an innocent `QList<Vector3>` / `QList<Color4>` reallocation in `lodgenLoadModel` that reached the corrupted heap first. All four share the bottom half `ChunkThread::run -> runJob -> lodgenBakeTerrainTextures -> LodgenRoadSet::gather -> addPlacement -> lodgenLoadModel` |
| **N1's other half, the stress harness** | `parse_stress.sh` PASS 5/5 on first run; both sabotage floors seen red FIRST (`digest` reports one mismatch by name; `share` segfaults). **20 consecutive runs at 16 threads x 8 reps x 4 fixtures: 10,240 loads, 0 mismatches, 0 faults.** **C1 IS REFUTED.** |
| **N2** 20 consecutive 16-thread runs per region + identity | **GREEN both.** Sanctuary 20/20 clean, 60 files every run; Boston 20/20 clean, 163 files every run. Byte identity serial vs 16 threads: 60 files / 24,975,886 B and 163 files / 77,591,755 B, both PASS, comparator shown red first on a flipped byte and on a deleted file |
| **N3** stage table, peak working set | **TAKEN, and the fan-out is still slower.** Medians of 3, alternating: Sanctuary 17,947 ms serial vs 40,484 ms at 16 (2.26x), peak 1,755 vs 5,800 MB; Boston 128,118 vs 156,043 ms (1.22x), peak 3,756 vs **25,136 MB**. The texture stage is where it goes backwards (5.8 -> 31.9 s; 66.1 -> 89.4 s). The 100-chunk point was NOT taken |
| **N4** serial byte-identity, editor harnesses, torso load time | **GREEN.** Rung exe vs fixes exe, both serial: byte-identical on both regions. All five editor harness logs byte-identical to the rung's, including `duplicateElements returned in 37 ms` on both. The torso instrument (15 ms of parse inside a 280 ms process) is too weak for a 5 % question, so the cost was measured with `parsestress --stress-threads 1`: 160 loads, pre-fix median 3,422 ms vs fixed 3,375 ms, **-1.4 %**, inside the pre-fix side's own 113 ms spread |
| **N5** exe newer than every changed file, ~81 TUs rebuilt | **GREEN.** Whole-working-set sweep 0 STALE; `obj_stale.py` walked all 246 object dependency blocks against the 95 changed files under `src/ res/ lib/` and found **0 stale objects and 0 objects newer than the exe** |
| **N6** nothing left running, game down, no crash dialog | **GREEN.** `Fallout4.exe` absent at every check; no NifSkope left running; every bake launched through `bake_run.ps1`, which dot-sources `no_crash_dialog.ps1`, and the exe sets the same error mode itself. **No Application Error box reached his desktop**, including on the 3-of-5 deliberate faults |

### What was applied, and what was left behind

19 of the 25 edits in `fixes.py`, selected BY FILE through
`scratchpad/resume3_20260911/fixes_subset.py` (which imports this lane's `EDITS`
table rather than editing this lane's script):

* **applied** -- F1 `src/message.cpp` (5), F2 `src/gamemanager.{h,cpp}` (7),
  F4+F5 `src/model/nifmodel.cpp` (2), F7 `src/lodgenparallel.{h,cpp}` (3);
* **left behind, deliberately, with the number** -- F3 `src/data/nifvalue.cpp`
  (2) and F6 `src/xml/nifexpr.cpp` (4). The experiment put 10,240 loads on 16
  threads through both and neither moved. They are latent, they are named, and
  the edits are still in `fixes.py`. This lane's own rule -- "never the whole
  list on a guess" -- was followed;
* **added, because the verdict named it** -- `cliMessageHandler` writes with
  `std::fputs` to `stderr` under its own mutex. The CRT locks the `FILE *`; on
  one thread the bytes and their order are exactly what `err()` produced;
* **removed** -- BAKEPERF1's process-wide parse mutex in `lodgenLoadModel`
  (`src/lodgen.cpp`), through a refusing script, with the measurement written
  into the comment that replaced it.

### Two corrections to this report's own text

1. **Section 1.5's "C3 is refuted for a headless run" was half right.** No
   `QMessageBox` is ever built in `-no-gui` -- that part holds exactly as
   written. But the handler `nifskopeCliMain` installs INSTEAD is the fault. The
   rule is in `MISTAKES.md`.
2. **`gdb3.sh` was pointed at the wrong region and at the wrong heap.** Boston
   at 16 chunk threads ran CLEAN with the lock off while Sanctuary faulted 3 of
   5; and `gdb` on Windows hands the inferior the DEBUG heap unless
   `_NO_DEBUG_HEAP=1` is set in its environment, which is why "it never crashes
   under gdb" was recorded twice as a property of the bug. With that one line
   the same run faults in three seconds. Both rules are now in
   `nifskope-ww-crash-diagnose`.

### The `--chunk-threads` default, by the rule this lane proposed

`--chunk-threads 0` now means
`min(cores, floor(0.60 x availPhys / (250 MB + max(64 MB, texBudget))))`, and
the bake census says which bound decided it. **The default stays 1**, and the
reason is no longer safety: 2.26x slower on 9 chunks, 25 GB on 25. The word was
proved to MOVE -- `default` with no switch, `asked` at `--chunk-threads 4`,
`cores` at `--chunk-threads 0`. `memory` is arithmetically correct and
UNEXERCISED (22,388 MB were free; the crossover is 20,320 MB), and it is named
as unexercised. Two defects in the field were found and fixed on the way: nothing
read it at all, and then it could not tell the shipped default from a typed 1.
